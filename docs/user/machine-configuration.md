# Machine Configuration

Machines define complete emulated systems including CPU configuration, memory regions, and peripherals. Configuration can be done via YAML files or Python dictionaries.

## Quick Start

Create a YAML file describing your machine:

```yaml
# boards/my_board.yaml
machine:
  name: my_board
  arch: armv8m

memory:
  - type: flash
    base: 0x00000000
    size: 512K
  - type: ram
    base: 0x20000000
    size: 256K
```

Load and run:

```python
from femu import Machine

machine = Machine.from_yaml("boards/my_board.yaml")
machine.load_elf("firmware.elf")
machine.run()
```

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

### Configuration Sections

#### machine (required)

| Field  | Type   | Description                              |
| ------ | ------ | ---------------------------------------- |
| `name` | string | Machine name for identification          |
| `arch` | string | Architecture: `armv8m`, `cortexm33`, etc |

#### cpu (optional)

| Field             | Type | Default | Description                    |
| ----------------- | ---- | ------- | ------------------------------ |
| `has_fpu`         | bool | false   | Enable FPU (VFP) support       |
| `has_dsp`         | bool | false   | Enable DSP extensions          |
| `has_trustzone`   | bool | false   | Enable TrustZone (ARMv8-M)     |
| `num_mpu_regions` | int  | 8       | Number of MPU regions          |
| `num_irqs`        | int  | 32      | Number of external interrupts  |

#### memory (required)

List of memory regions:

| Field  | Type       | Description                            |
| ------ | ---------- | -------------------------------------- |
| `type` | string     | `flash` or `ram`                       |
| `base` | int/string | Base address (hex string or integer)   |
| `size` | int/string | Size with optional suffix (K, M, G)    |

#### peripherals (optional)

List of peripheral instances:

| Field    | Type   | Description                          |
| -------- | ------ | ------------------------------------ |
| `type`   | string | Peripheral type (registered name)    |
| `name`   | string | Instance name (e.g., "USART1")       |
| `base`   | int    | Base address                         |
| `size`   | int    | Address space size                   |
| `config` | dict   | Type-specific configuration          |

### Address and Size Parsing

Addresses and sizes support multiple formats:

```yaml
# All equivalent
base: 0x08000000       # Hex integer
base: "0x08000000"     # Hex string
base: 134217728        # Decimal

# Size suffixes
size: 512K             # 512 * 1024 bytes
size: 1M               # 1 * 1024 * 1024 bytes
size: 0x80000          # Hex (512K)
```

## Built-in Peripherals

### UART (simple_uart)

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

**Registers:**

| Offset | Name   | Description                |
| ------ | ------ | -------------------------- |
| 0x00   | DATA   | Data register (read/write) |
| 0x04   | STATUS | Status (RXNE, TXE bits)    |
| 0x08   | CTRL   | Control (EN, TE, RE bits)  |

### GPIO (simple_gpio)

```yaml
peripherals:
  - type: simple_gpio
    name: GPIOA
    base: 0x48000000
    size: 0x400
```

## Python API

### Loading Machines

```python
from femu import Machine

# From YAML file
machine = Machine.from_yaml("boards/stm32l5.yaml")

# From dictionary
machine = Machine.from_dict({
    "machine": {"name": "test", "arch": "armv8m"},
    "memory": [
        {"type": "flash", "base": 0x00000000, "size": "64K"},
        {"type": "ram", "base": 0x20000000, "size": "32K"},
    ],
})
```

### Machine Properties

```python
machine.name              # Machine name
machine.arch              # Architecture string
machine.emu               # Underlying emulator instance
machine.peripherals       # List of peripheral objects
machine.peripheral_names  # List of peripheral names
```

### Execution Control

```python
# Load firmware
elf_info = machine.load_elf("firmware.elf")

# Run
cycles = machine.run(max_cycles=1000000)

# Single step
result = machine.step()

# Reset
machine.reset()

# Stop
machine.stop()
```

### Register Access

```python
# Program counter and stack pointer
pc = machine.pc
machine.pc = 0x08000100
sp = machine.sp

# General purpose registers
r0 = machine.get_reg(0)
machine.set_reg(0, 0x12345678)

# Dump all registers
regs = machine.dump_regs()
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

### Peripheral Access

```python
# Get peripheral by name
uart = machine.get_peripheral("USART1")
uart = machine["USART1"]  # Dict-style access

# UART-specific
output = uart.get_output()      # Get transmitted string
uart.inject_input("hello\n")    # Inject receive data
uart.clear_output()             # Clear TX buffer
```

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
      echo: true
  - type: simple_uart
    name: LPUART1
    base: 0x40008000
    size: 0x400
    config:
      irq: 66
```

## Next Steps

- [Peripherals](peripherals.md) - Using built-in peripherals
- [Running Firmware](running-firmware.md) - Firmware requirements
- See [Writing Peripherals](../developer/writing-peripherals.md) for creating custom peripherals
