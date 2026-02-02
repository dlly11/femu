# Machine Configuration Guide

Machines define complete emulated systems including CPU configuration, memory regions, and peripherals. Configuration can be done via YAML files or Python dictionaries.

## YAML Format

### Complete Example

```yaml
machine:
  name: STM32L5
  arch: armv8m

cpu:
  has_fpu: true
  has_dsp: true
  has_trustzone: false
  num_mpu_regions: 8
  num_irqs: 32

memory:
  - type: flash
    base: 0x08000000
    size: 512K
  - type: ram
    base: 0x20000000
    size: 256K

peripherals:
  - type: simple_uart
    name: USART1
    base: 0x40013800
    size: 0x400
    config:
      echo: true
```

### Sections

#### `machine` (required)

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | Machine name for identification |
| `arch` | string | Architecture: `armv8m`, `armv7m`, `cortexm33`, `cortexm4` |

#### `cpu` (optional)

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `has_fpu` | bool | false | Enable FPU (VFP) support |
| `has_dsp` | bool | false | Enable DSP extensions |
| `has_trustzone` | bool | false | Enable TrustZone (ARMv8-M only) |
| `num_mpu_regions` | int | 8 | Number of MPU regions |
| `num_irqs` | int | 32 | Number of external interrupts |

#### `memory` (required)

List of memory regions:

| Field | Type | Description |
|-------|------|-------------|
| `type` | string | `flash` or `ram` |
| `base` | int/string | Base address (hex string or integer) |
| `size` | int/string | Size with optional suffix (K, M, G) |

#### `peripherals` (optional)

List of peripheral instances:

| Field | Type | Description |
|-------|------|-------------|
| `type` | string | Peripheral type (registered name) |
| `name` | string | Instance name (e.g., "USART1") |
| `base` | int/string | Base address |
| `size` | int/string | Address space size |
| `config` | dict | Type-specific configuration (optional) |
| `plugin` | string | Explicit plugin path (optional) |

#### `plugins` (optional)

List of plugin paths to load:

```yaml
plugins:
  - ./plugins/my_plugin.so
  - /usr/lib/femu/vendor_peripherals.so

plugin_dirs:
  - ./plugins
```

### Address/Size Parsing

Addresses and sizes support:

- Integers: `0x20000000`, `536870912`
- Hex strings: `"0x20000000"`
- Size suffixes: `"512K"`, `"1M"`, `"2G"`

Examples:
```yaml
base: 0x08000000     # Hex integer
base: "0x08000000"   # Hex string
size: 512K           # 512 * 1024 bytes
size: 0x80000        # Hex integer (512K)
size: "1M"           # 1 * 1024 * 1024 bytes
```

---

## Python API

### Loading from YAML

```python
from femu import Machine

machine = Machine.from_yaml("boards/stm32l5.yaml")
machine.load_elf("firmware.elf")
cycles = machine.run(max_cycles=1000000)
```

### Creating from Dictionary

```python
from femu import Machine

machine = Machine.from_dict({
    "machine": {"name": "test", "arch": "armv8m"},
    "cpu": {"has_fpu": True, "has_dsp": True},
    "memory": [
        {"type": "flash", "base": 0x08000000, "size": "64K"},
        {"type": "ram", "base": 0x20000000, "size": "32K"},
    ],
    "peripherals": [
        {
            "type": "simple_uart",
            "name": "USART1",
            "base": 0x40013800,
            "size": 0x400,
            "config": {"echo": True}
        }
    ]
})
```

### Machine Properties

```python
machine.name           # Machine name
machine.arch           # Architecture string
machine.emu            # Underlying emulator instance
machine.peripherals    # List of peripheral objects
machine.peripheral_names  # List of peripheral names

# Access peripheral by name
uart = machine.get_peripheral("USART1")
uart = machine["USART1"]  # Dict-style access
```

### Execution Methods

```python
# Load firmware
elf_info = machine.load_elf("firmware.elf")

# Execution control
cycles = machine.run(max_cycles=1000000)  # Run with cycle limit
result = machine.step()                    # Single step
machine.reset()                            # Reset to initial state
machine.stop()                             # Request stop

# State access
pc = machine.pc
machine.pc = 0x08000100
sp = machine.sp
total_cycles = machine.cycles
state = machine.state  # EmulatorState enum
```

### Register Access

```python
# General purpose registers (r0-r15)
r0 = machine.get_reg(0)
machine.set_reg(0, 0x12345678)

# Dump all registers
regs = machine.dump_regs()
print(regs)  # {'r0': 0, 'r1': 0, ..., 'pc': 0x08000100, ...}
```

### Memory Access

```python
# Read/write words
value = machine.read_mem(0x20000000, size=4)
machine.write_mem(0x20000000, 0xDEADBEEF, size=4)

# Read/write bytes
data = machine.read_bytes(0x08000000, 256)
machine.write_bytes(0x20000000, b"\x00\x00\x00\x00")
```

---

## Built-in Peripheral Types

| Type | Description |
|------|-------------|
| `simple_uart` | Basic UART with TX/RX buffering |
| `simple_gpio` | GPIO port with input/output modes |

### simple_uart

```yaml
peripherals:
  - type: simple_uart
    name: USART1
    base: 0x40013800
    size: 0x400
    config:
      irq: 37      # IRQ number (-1 to disable)
      echo: true   # Print TX to console
```

Registers:
- `0x00`: DATA - Data register (read/write)
- `0x04`: STATUS - Status (RXNE, TXE bits)
- `0x08`: CTRL - Control (EN, TE, RE bits)

Python API:
```python
uart = machine["USART1"]
output = uart.get_output()      # Get transmitted string
uart.inject_input("hello\n")    # Inject receive data
uart.clear_output()             # Clear TX buffer
```

---

## Example Configurations

### Minimal Test Setup

```yaml
machine:
  name: minimal
  arch: armv8m

memory:
  - type: flash
    base: 0x00000000
    size: 64K
  - type: ram
    base: 0x20000000
    size: 16K
```

### STM32L5-like Configuration

```yaml
machine:
  name: STM32L5
  arch: armv8m

cpu:
  has_fpu: true
  has_dsp: true
  has_trustzone: true
  num_mpu_regions: 8
  num_irqs: 109

memory:
  - type: flash
    base: 0x08000000
    size: 512K
  - type: ram
    base: 0x20000000
    size: 256K
  - type: ram
    base: 0x10000000
    size: 64K

peripherals:
  - type: simple_uart
    name: USART1
    base: 0x40013800
    size: 0x400
    config:
      irq: 37
  - type: simple_uart
    name: LPUART1
    base: 0x40008000
    size: 0x400
    config:
      irq: 66
```

### With Plugin Peripherals

```yaml
machine:
  name: custom_board
  arch: armv8m

plugins:
  - ./vendor_peripherals.so

plugin_dirs:
  - ./peripherals

memory:
  - type: flash
    base: 0x08000000
    size: 256K

peripherals:
  - type: vendor_adc
    name: ADC1
    base: 0x50000000
    size: 0x100
    config:
      channels: 16
      resolution: 12
```

---

## Key Files Reference

| File | Description |
|------|-------------|
| `python/femu/machine.py` | Machine class implementation |
| `python/femu/peripheral_registry.py` | Peripheral type registration |
| `python/femu/peripherals/` | Built-in peripheral implementations |
