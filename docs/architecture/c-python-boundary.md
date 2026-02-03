# C-Python Boundary

FEMU uses CFFI (C Foreign Function Interface) to bridge Python and C code. This document describes how the boundary works.

## Overview

```text
Python                          C
──────                          ─
Machine
  │
  ▼
ARMv8MEmulator  ──CFFI──►  armv8m_emulator.c
  │                             │
  ▼                             ▼
Peripheral      ◄──CFFI──  EmuPeripheral vtable
```

## CFFI Setup

The CFFI interface is defined in `python/femu/_emulator_cffi.py`:

```python
ffi = FFI()

# C declarations that Python needs to call
ffi.cdef("""
    typedef struct ARMv8MEmulator ARMv8MEmulator;

    ARMv8MEmulator* armv8m_emulator_create(void);
    void armv8m_emulator_destroy(ARMv8MEmulator* emu);
    int armv8m_emulator_step(ARMv8MEmulator* emu);
    int armv8m_emulator_run(ARMv8MEmulator* emu, uint64_t max_cycles);

    // ... more declarations
""")

# Build the C extension
ffi.set_source("_emulator_cffi", ...)
```

## Type Mappings

### Basic Types

| C Type      | Python Type | CFFI Access       |
| ----------- | ----------- | ----------------- |
| `uint32_t`  | `int`       | Direct            |
| `uint8_t*`  | `bytes`     | `ffi.buffer()`    |
| `char*`     | `str`       | `ffi.string()`    |
| `void*`     | Handle      | `ffi.new_handle()`|

### Struct Access

```python
# C struct
typedef struct {
    uint32_t r[16];
    uint32_t xpsr;
} ARMv8MCPUState;

# Python access
state = ffi.new("ARMv8MCPUState*")
state.r[0] = 42
xpsr = state.xpsr
```

### Arrays

```python
# Fixed-size array
code = ffi.new("uint8_t[4]", [0x00, 0xBF, 0x00, 0x00])

# Buffer from Python bytes
data = b"\x00\xBF\x00\x00"
buf = ffi.from_buffer(data)
```

## Calling C from Python

### Simple Function Call

```python
# C declaration
# int armv8m_emulator_step(ARMv8MEmulator* emu);

# Python call
from . import _emulator_cffi as cffi

lib = cffi.get_lib()
result = lib.armv8m_emulator_step(self._emu)
if result < 0:
    raise RuntimeError("Step failed")
```

### Memory Access

```python
# C declaration
# int armv8m_emulator_read_mem(ARMv8MEmulator*, uint32_t addr,
#                               uint32_t* value, uint8_t size);

# Python wrapper
def read_mem(self, addr: int, size: int = 4) -> int:
    ffi = cffi.get_ffi()
    lib = cffi.get_lib()

    value = ffi.new("uint32_t*")
    result = lib.armv8m_emulator_read_mem(self._emu, addr, value, size)

    if result != 0:
        raise MemoryError(f"Read failed at 0x{addr:08x}")

    return value[0]
```

### Bulk Data Transfer

```python
# Reading bytes
def read_bytes(self, addr: int, length: int) -> bytes:
    ffi = cffi.get_ffi()
    lib = cffi.get_lib()

    buf = ffi.new(f"uint8_t[{length}]")
    result = lib.armv8m_emulator_read_bytes(self._emu, addr, buf, length)

    if result != 0:
        raise MemoryError(f"Read failed")

    return bytes(ffi.buffer(buf))
```

## Calling Python from C

This is used for peripherals implemented in Python.

### Callback Definition

```python
# Define callback signature
ffi.cdef("""
    typedef uint32_t (*EmuPeriphReadFn)(void*, uint32_t, uint8_t);
    typedef void (*EmuPeriphWriteFn)(void*, uint32_t, uint32_t, uint8_t);
""")

# Create callback
@ffi.callback("uint32_t(void*, uint32_t, uint8_t)")
def _py_periph_read(ctx, offset, size):
    try:
        periph = ffi.from_handle(ctx)
        return periph.read(offset, size)
    except Exception as e:
        logger.error("Read error: %s", e)
        return 0
```

### Handle Management

Python objects passed to C must be kept alive:

```python
class Peripheral:
    def __init__(self, name: str, peripheral_type: str):
        # Create handle for C to reference this Python object
        self._handle = ffi.new_handle(self)

        # Keep handle alive (prevent GC)
        Peripheral._instances[id(self)] = self
        Peripheral._handles[id(self)] = self._handle

        # Set up C struct
        self._c_periph = ffi.new("EmuPeripheral*")
        self._c_periph.context = self._handle
        self._c_periph.vtable.read = _py_periph_read
        # ...
```

## Memory Management

### Python-Owned Objects

Objects created in Python and passed to C:

```python
# Python owns this - must keep reference
self._name_buf = ffi.new("char[]", name.encode("utf-8"))
self._c_periph.name = self._name_buf  # C gets pointer

# If _name_buf is garbage collected, C has dangling pointer!
```

### C-Owned Objects

Objects created by C, returned to Python:

```python
# C owns this - Python just gets pointer
emu = lib.armv8m_emulator_create()

# Must call destroy when done
def __del__(self):
    if self._emu:
        lib.armv8m_emulator_destroy(self._emu)
```

### Preventing Leaks

```python
class ARMv8MEmulator:
    def __init__(self):
        self._emu = lib.armv8m_emulator_create()
        self._peripherals = []  # Keep Python peripherals alive

    def __del__(self):
        # Destroy in reverse order
        self._peripherals.clear()
        if self._emu:
            lib.armv8m_emulator_destroy(self._emu)
```

## Error Handling

### C to Python

C functions return error codes, Python wraps them:

```python
def step(self) -> EmuStatus:
    result = lib.armv8m_emulator_step(self._emu)
    if result < 0:
        raise RuntimeError(f"Step failed with code {result}")
    return EmuStatus(result)
```

### Python to C (Callbacks)

Python exceptions in callbacks must be caught:

```python
@ffi.callback("uint32_t(void*, uint32_t, uint8_t)")
def _py_periph_read(ctx, offset, size):
    try:
        periph = ffi.from_handle(ctx)
        result = periph.read(offset, size)
        return result & 0xFFFFFFFF
    except Exception as e:
        # Log but don't propagate - would crash C
        logger.error("Read error: %s", e)
        return 0
```

## Thread Safety

FEMU is **not thread-safe**. All emulator operations must happen on a single thread.

For GDB server (which uses asyncio):

```python
# All emulator access happens in main thread
# asyncio.run() handles the event loop
```

## Performance Considerations

### Minimize Crossings

Each CFFI call has overhead. Batch operations when possible:

```python
# Bad: Many crossings
for addr in range(0x20000000, 0x20001000, 4):
    value = machine.read_mem(addr, 4)

# Good: Single crossing
data = machine.read_bytes(0x20000000, 0x1000)
```

### Avoid Callbacks in Hot Paths

Python callbacks are slow. For high-performance peripherals:

1. Implement in C
2. Or implement in Python with minimal logic

### Use Native Arrays

```python
# Slow: Python list
values = [machine.read_mem(addr + i*4, 4) for i in range(100)]

# Fast: Direct buffer
buf = ffi.new("uint32_t[100]")
lib.armv8m_emulator_read_words(emu, addr, buf, 100)
values = list(buf)
```

## Debugging CFFI Issues

### Type Mismatches

```python
# C expects uint32_t, Python passes negative
lib.some_function(-1)  # Wraps to large positive!

# Fix: Validate in Python
if value < 0:
    raise ValueError("Value must be non-negative")
```

### Dangling Pointers

Symptom: Crash or garbage data after GC runs

```python
# Force GC to reproduce
import gc
gc.collect()
machine.step()  # Crashes if peripheral was collected
```

### Buffer Overflows

```python
# C function writes beyond buffer
buf = ffi.new("uint8_t[10]")
lib.some_function(buf, 100)  # Overflow!

# Fix: Match sizes
length = 100
buf = ffi.new(f"uint8_t[{length}]")
lib.some_function(buf, length)
```

## See Also

- [CFFI Documentation](https://cffi.readthedocs.io/)
- [Overview](overview.md) - System architecture
- [Peripheral Framework](python-layer/peripheral-framework.md) - Peripheral callbacks
