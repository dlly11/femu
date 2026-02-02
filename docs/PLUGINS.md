# Peripheral Development Guide

FEMU supports three types of peripherals:

1. **Python Peripherals** - Pure Python, easiest to develop
2. **C Peripherals** - Compiled into the emulator library
3. **Plugin Peripherals** - Loaded dynamically from shared libraries

## Python Peripherals

Python peripherals are the simplest way to add device emulation. Subclass `Peripheral` and implement `read()` and `write()` methods.

### Basic Example

```python
from femu import Peripheral, PeripheralRegistry

@PeripheralRegistry.register("my_timer")
class MyTimer(Peripheral):
    def __init__(self, name: str = "timer", irq: int | None = None):
        super().__init__(name, "my_timer")
        self._irq = irq
        self._counter = 0
        self._reload = 0
        self._enabled = False

    def read(self, offset: int, size: int) -> int:
        if offset == 0x00:
            return self._counter
        elif offset == 0x04:
            return self._reload
        return 0

    def write(self, offset: int, value: int, size: int) -> None:
        if offset == 0x00:
            self._counter = value
        elif offset == 0x04:
            self._reload = value
        elif offset == 0x08:
            self._enabled = bool(value & 1)

    def reset(self) -> None:
        self._counter = 0
        self._reload = 0
        self._enabled = False

    def tick(self, cycles: int) -> None:
        if self._enabled and self._counter > 0:
            self._counter -= 1
            if self._counter == 0:
                self._counter = self._reload
                if self._irq is not None:
                    self.pulse_irq(self._irq)
```

### Required Methods

| Method | Description |
|--------|-------------|
| `read(offset, size)` | Read register at byte offset, return value |
| `write(offset, value, size)` | Write value to register at byte offset |

### Optional Methods

| Method | Description |
|--------|-------------|
| `reset()` | Called on emulator reset |
| `tick(cycles)` | Called after each instruction with cycle count |

### IRQ Methods

Peripherals can trigger interrupts:

```python
# Assert interrupt line (level-triggered)
self.assert_irq(irq_number)

# Deassert interrupt line
self.deassert_irq(irq_number)

# Pulse interrupt (assert then immediately deassert)
self.pulse_irq(irq_number)
```

### Registration

Use the decorator to register peripherals with the global registry:

```python
@PeripheralRegistry.register("type_name", "Description for listings")
class MyPeripheral(Peripheral):
    ...
```

Or register manually:

```python
PeripheralRegistry.register_class("type_name", MyPeripheral, "Description")
```

### Using in Machine Configuration

```yaml
peripherals:
  - type: my_timer
    name: TIM1
    base: 0x40010000
    size: 0x400
    config:
      irq: 25
```

---

## C Peripherals

C peripherals use the `EmuPeripheralVTable` structure defined in `include/emu/emu_peripheral.h`.

### VTable Structure

```c
typedef struct {
    uint32_t (*read)(void *ctx, uint32_t offset, uint8_t size);
    void (*write)(void *ctx, uint32_t offset, uint32_t value, uint8_t size);
    void (*reset)(void *ctx);
    void (*tick)(void *ctx, uint64_t cycles);
    void (*destroy)(void *ctx);
    void (*set_irq_callback)(void *ctx, EmuPeripheralIRQCallback cb, void *emu_ctx);
    void (*set_dma_callback)(void *ctx, EmuPeripheralDMACallback cb, void *emu_ctx);
    int (*debug_state)(void *ctx, char *buf, size_t buf_size);
} EmuPeripheralVTable;
```

All functions except `read` and `write` may be NULL if not needed.

### Peripheral Structure

```c
typedef struct {
    const char *name;           // Instance name (e.g., "USART1")
    const char *type;           // Type identifier (e.g., "stm32_uart")
    void *context;              // Opaque peripheral state
    EmuPeripheralVTable vtable; // Function pointers

    // Set by emulator during registration:
    uint64_t base_addr;
    uint64_t size;
    void *emu_ctx;
} EmuPeripheral;
```

### Factory Function Example

```c
#include "emu/emu_peripheral.h"

typedef struct {
    uint32_t data_reg;
    uint32_t ctrl_reg;
    EmuPeripheralIRQCallback irq_cb;
    void *irq_ctx;
} MyUartState;

static uint32_t my_uart_read(void *ctx, uint32_t offset, uint8_t size) {
    MyUartState *state = (MyUartState *)ctx;
    switch (offset) {
        case 0x00: return state->data_reg;
        case 0x04: return state->ctrl_reg;
        default: return 0;
    }
}

static void my_uart_write(void *ctx, uint32_t offset, uint32_t value, uint8_t size) {
    MyUartState *state = (MyUartState *)ctx;
    switch (offset) {
        case 0x00: state->data_reg = value; break;
        case 0x04: state->ctrl_reg = value; break;
    }
}

static void my_uart_reset(void *ctx) {
    MyUartState *state = (MyUartState *)ctx;
    state->data_reg = 0;
    state->ctrl_reg = 0;
}

static void my_uart_set_irq(void *ctx, EmuPeripheralIRQCallback cb, void *emu_ctx) {
    MyUartState *state = (MyUartState *)ctx;
    state->irq_cb = cb;
    state->irq_ctx = emu_ctx;
}

EmuPeripheral* my_uart_create(const char *name, const char *config_json) {
    EmuPeripheral *periph = malloc(sizeof(EmuPeripheral));
    MyUartState *state = malloc(sizeof(MyUartState));

    memset(state, 0, sizeof(*state));

    periph->name = strdup(name);
    periph->type = "my_uart";
    periph->context = state;
    periph->vtable = (EmuPeripheralVTable){
        .read = my_uart_read,
        .write = my_uart_write,
        .reset = my_uart_reset,
        .set_irq_callback = my_uart_set_irq,
    };

    return periph;
}
```

### IRQ Callbacks

To trigger interrupts from C peripherals:

```c
// In your peripheral code:
if (state->irq_cb) {
    state->irq_cb(state->irq_ctx, irq_number, 1);  // Assert
    state->irq_cb(state->irq_ctx, irq_number, 0);  // Deassert
}
```

---

## Plugin Peripherals

Plugins are shared libraries (`.so`, `.dll`, `.dylib`) that export peripheral factories via the `emu_plugin_init()` entry point.

### Plugin Header

Include `include/emu/emu_plugin.h` which defines:

```c
// Plugin metadata
typedef struct {
    int api_version;        // Must be EMU_PLUGIN_API_VERSION (currently 1)
    const char *name;       // Plugin name
    const char *version;    // Version string
    const char *author;     // Author name
    const char *description;
} EmuPluginInfo;

// Peripheral type descriptor
typedef struct {
    const char *type_name;  // Type name for YAML configs
    const char *description;
    EmuPeripheral* (*create)(const char *name, const char *config_json);
    void (*destroy)(EmuPeripheral *periph);
} EmuPeripheralType;
```

### Complete Plugin Example

```c
#include "emu/emu_plugin.h"
#include <stdlib.h>
#include <string.h>

// --- Peripheral Implementation ---

typedef struct {
    uint32_t value;
} SimpleRegState;

static uint32_t simple_read(void *ctx, uint32_t offset, uint8_t size) {
    SimpleRegState *state = ctx;
    return state->value;
}

static void simple_write(void *ctx, uint32_t offset, uint32_t value, uint8_t size) {
    SimpleRegState *state = ctx;
    state->value = value;
}

static EmuPeripheral* simple_create(const char *name, const char *config_json) {
    EmuPeripheral *p = calloc(1, sizeof(*p));
    SimpleRegState *s = calloc(1, sizeof(*s));

    p->name = strdup(name);
    p->type = "simple_reg";
    p->context = s;
    p->vtable.read = simple_read;
    p->vtable.write = simple_write;

    return p;
}

static void simple_destroy(EmuPeripheral *p) {
    free((void*)p->name);
    free(p->context);
    free(p);
}

// --- Plugin Entry Point ---

static EmuPeripheralType types[] = {
    {
        .type_name = "simple_reg",
        .description = "Simple register peripheral",
        .create = simple_create,
        .destroy = simple_destroy,
    },
    { NULL }  // Terminator (required)
};

static EmuPluginInfo info = {
    .api_version = EMU_PLUGIN_API_VERSION,
    .name = "Example Plugin",
    .version = "1.0.0",
    .author = "Your Name",
    .description = "Example peripheral plugin"
};

EMU_PLUGIN_EXPORT
EmuPeripheralType* emu_plugin_init(const EmuPluginInfo **info_out) {
    if (info_out) *info_out = &info;
    return types;
}
```

### Building Plugins

```bash
# Linux
gcc -shared -fPIC -o my_plugin.so my_plugin.c -I/path/to/femu/include

# macOS
clang -shared -fPIC -o my_plugin.dylib my_plugin.c -I/path/to/femu/include

# Windows (MSVC)
cl /LD my_plugin.c /I path\to\femu\include /Fe:my_plugin.dll
```

### Loading Plugins

From Python:

```python
from femu import PluginPeripheral

# Load and list available types
types = PluginPeripheral.load_plugin("./my_plugin.so")
print(types.keys())  # ['simple_reg']

# Create peripheral instance
periph = PluginPeripheral.from_plugin(
    "./my_plugin.so",
    "simple_reg",
    name="REG1",
    config={"initial_value": 0}
)
emu.add_peripheral(periph, 0x40000000, 0x100)
```

From YAML:

```yaml
plugins:
  - ./my_plugin.so

peripherals:
  - type: simple_reg
    name: REG1
    base: 0x40000000
    size: 0x100
```

Or specify plugin inline:

```yaml
peripherals:
  - type: simple_reg
    name: REG1
    base: 0x40000000
    size: 0x100
    plugin: ./my_plugin.so
```

---

## Key Files Reference

| File | Description |
|------|-------------|
| `include/emu/emu_peripheral.h` | C peripheral interface and vtable |
| `include/emu/emu_plugin.h` | Plugin API and entry point |
| `python/femu/peripheral.py` | Python peripheral base classes |
| `python/femu/peripheral_registry.py` | Peripheral registration system |
| `python/femu/peripherals/uart.py` | Example Python UART peripheral |
