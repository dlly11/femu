# Peripheral Framework

The peripheral framework enables Python peripherals to interact with the C emulator core.

## Overview

```text
python/femu/
├── peripheral.py           # Base classes
├── peripheral_registry.py  # Registration system
└── peripherals/
    ├── __init__.py
    ├── uart.py            # SimpleUART
    └── gpio.py            # SimpleGPIO
```

## Architecture

```text
┌──────────────────────────────────────────────────────────────┐
│                     Python Layer                              │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐   │
│  │ SimpleUART  │  │ SimpleGPIO  │  │  Custom Peripheral  │   │
│  └──────┬──────┘  └──────┬──────┘  └──────────┬──────────┘   │
│         │                │                     │              │
│         └────────────────┼─────────────────────┘              │
│                          ▼                                    │
│                  ┌───────────────┐                            │
│                  │  Peripheral   │  (base class)              │
│                  └───────┬───────┘                            │
│                          │                                    │
│                          ▼                                    │
│                  ┌───────────────┐                            │
│                  │ CFFI Callbacks│                            │
│                  └───────┬───────┘                            │
├──────────────────────────┼───────────────────────────────────┤
│                          ▼                                    │
│                  ┌───────────────┐                            │
│                  │EmuPeripheral  │  (C struct)                │
│                  │   vtable      │                            │
│                  └───────────────┘                            │
│                     C Layer                                   │
└──────────────────────────────────────────────────────────────┘
```

## Peripheral Base Class

```python
class Peripheral(PeripheralBase):
    """Base class for Python peripherals."""

    def __init__(self, name: str, peripheral_type: str):
        self._name = name
        self._peripheral_type = peripheral_type

        # Create handle for C to reference this Python object
        self._handle = ffi.new_handle(self)

        # Allocate C struct
        self._c_periph = ffi.new("EmuPeripheral*")
        self._c_periph.name = ffi.new("char[]", name.encode())
        self._c_periph.type = ffi.new("char[]", peripheral_type.encode())
        self._c_periph.context = self._handle

        # Set up vtable with CFFI callbacks
        self._c_periph.vtable.read = _py_periph_read
        self._c_periph.vtable.write = _py_periph_write
        self._c_periph.vtable.reset = _py_periph_reset
        self._c_periph.vtable.tick = _py_periph_tick

    @abstractmethod
    def read(self, offset: int, size: int) -> int:
        """Read from peripheral register."""
        ...

    @abstractmethod
    def write(self, offset: int, value: int, size: int) -> None:
        """Write to peripheral register."""
        ...
```

## CFFI Callbacks

Module-level callbacks that route C calls to Python:

```python
@ffi.callback("uint32_t(void*, uint32_t, uint8_t)")
def _py_periph_read(ctx, offset, size):
    """Route C read to Python peripheral."""
    try:
        periph = ffi.from_handle(ctx)
        result = periph.read(offset, size)
        return result & 0xFFFFFFFF
    except Exception as e:
        logger.error("Read error: %s", e)
        return 0

@ffi.callback("void(void*, uint32_t, uint32_t, uint8_t)")
def _py_periph_write(ctx, offset, value, size):
    """Route C write to Python peripheral."""
    try:
        periph = ffi.from_handle(ctx)
        periph.write(offset, value, size)
    except Exception as e:
        logger.error("Write error: %s", e)
```

## Handle Management

Python objects must stay alive while C holds references:

```python
class Peripheral:
    # Class-level storage prevents GC
    _instances: dict[int, Peripheral] = {}
    _handles: dict[int, CData] = {}

    def __init__(self, ...):
        # Keep reference to self
        Peripheral._instances[id(self)] = self
        Peripheral._handles[id(self)] = self._handle

    def __del__(self):
        # Clean up when done
        Peripheral._instances.pop(id(self), None)
        Peripheral._handles.pop(id(self), None)
```

## IRQ Support

Peripherals can trigger interrupts:

```python
class Peripheral:
    def assert_irq(self, irq: int) -> None:
        """Assert interrupt line."""
        if self._c_irq_callback and self._c_emu_ctx:
            self._c_irq_callback(self._c_emu_ctx, irq, 1)

    def deassert_irq(self, irq: int) -> None:
        """Deassert interrupt line."""
        if self._c_irq_callback and self._c_emu_ctx:
            self._c_irq_callback(self._c_emu_ctx, irq, 0)

    def pulse_irq(self, irq: int) -> None:
        """Pulse interrupt (edge-triggered)."""
        self.assert_irq(irq)
        self.deassert_irq(irq)
```

The IRQ callback is set by the emulator during registration:

```python
@ffi.callback("void(void*, EmuPeriphIRQCallback, void*)")
def _py_periph_set_irq_callback(ctx, callback, emu_ctx):
    """Store IRQ callback from emulator."""
    periph = ffi.from_handle(ctx)
    periph._c_irq_callback = callback
    periph._c_emu_ctx = emu_ctx
```

## Registration System

### Using Decorator

```python
@PeripheralRegistry.register("my_timer", "Simple timer peripheral")
class MyTimer(Peripheral):
    def __init__(self, name: str = "timer", irq: int = -1):
        super().__init__(name, "my_timer")
        self._irq = irq
        ...
```

### Manual Registration

```python
PeripheralRegistry.register_python(
    "my_timer",
    MyTimer,
    "Simple timer peripheral"
)
```

### Creating Peripherals

```python
# From registry
timer = PeripheralRegistry.create("my_timer", "TIM1", irq=25)

# Direct instantiation
timer = MyTimer(name="TIM1", irq=25)
```

## C Struct Layout

The C side expects:

```c
typedef struct EmuPeripheralVTable {
    uint32_t (*read)(void *ctx, uint32_t offset, uint8_t size);
    void (*write)(void *ctx, uint32_t offset, uint32_t value, uint8_t size);
    void (*reset)(void *ctx);
    void (*tick)(void *ctx, uint64_t cycles);
    void (*destroy)(void *ctx);
    void (*set_irq_callback)(void *ctx, EmuPeriphIRQCallback cb, void *emu_ctx);
} EmuPeripheralVTable;

typedef struct EmuPeripheral {
    const char *name;
    const char *type;
    void *context;
    EmuPeripheralVTable vtable;
    uint64_t base_addr;
    uint64_t size;
} EmuPeripheral;
```

## Plugin Peripherals

For loading C peripherals from shared libraries:

```python
class PluginPeripheral(PeripheralBase):
    """Peripheral loaded from a plugin."""

    @classmethod
    def from_plugin(cls, plugin_path: str, type_name: str,
                    name: str, config: dict = None) -> PluginPeripheral:
        """Create peripheral from plugin."""
        types = cls.load_plugin(plugin_path)
        if type_name not in types:
            raise ValueError(f"Type {type_name} not in plugin")

        create_fn = types[type_name]["create"]
        c_periph = create_fn(
            ffi.new("char[]", name.encode()),
            ffi.new("char[]", json.dumps(config or {}).encode())
        )

        return cls(c_periph, ...)
```

## Best Practices

### Error Handling

Catch all exceptions in callbacks:

```python
@ffi.callback(...)
def _py_periph_read(ctx, offset, size):
    try:
        periph = ffi.from_handle(ctx)
        return periph.read(offset, size)
    except Exception as e:
        # Log but don't propagate - would crash C
        logger.error("Error: %s", e)
        return 0  # Safe default
```

### Memory Safety

Keep references to prevent premature GC:

```python
class ARMv8MEmulator:
    def add_peripheral(self, periph, base, size):
        result = lib.add_peripheral(self._emu, base, size, periph.c_struct)
        # Keep reference!
        self._peripherals.append(periph)
```

### Performance

Minimize work in callbacks:

```python
def read(self, offset, size):
    # Fast path: direct register access
    if offset < len(self._regs):
        return self._regs[offset // 4]
    return 0

# Avoid in hot path:
# - String formatting
# - Complex computations
# - I/O operations
```

## See Also

- [C-Python Boundary](../c-python-boundary.md) - CFFI details
- [Writing Peripherals](../../developer/writing-peripherals.md) - Tutorial
- [User Guide: Peripherals](../../user/peripherals.md) - Usage
