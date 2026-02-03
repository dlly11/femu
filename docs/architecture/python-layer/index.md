# Python Layer

The Python layer provides the high-level interface to FEMU, including the CLI, GDB server, and peripheral framework.

```{toctree}
:maxdepth: 1
:hidden:

emulator
gdb-server
peripheral-framework
```

## Module Overview

| Module                                        | Purpose                       |
| --------------------------------------------- | ----------------------------- |
| [Emulator](emulator.md)                       | Python emulator wrapper       |
| [GDB Server](gdb-server.md)                   | Remote debugging server       |
| [Peripheral Framework](peripheral-framework.md)| Python peripheral support    |

## Package Structure

```text
python/femu/
├── __init__.py             # Package exports
├── cli.py                  # Command-line interface
├── machine.py              # High-level Machine class
├── logging.py              # Logging configuration
│
├── arch/
│   └── armv8m.py           # ARMv8MEmulator class
│
├── gdb/
│   ├── server.py           # GDB server
│   ├── protocol.py         # RSP handling
│   └── commands.py         # Command handlers
│
├── peripheral.py           # Peripheral base classes
├── peripheral_registry.py  # Registration system
└── peripherals/
    ├── uart.py             # SimpleUART
    └── gpio.py             # SimpleGPIO
```

## Key Classes

### Machine

High-level API for loading and running firmware:

```python
from femu import Machine

machine = Machine.from_yaml("boards/stm32l5.yaml")
machine.load_elf("firmware.elf")
machine.run(max_cycles=1000000)
```

### ARMv8MEmulator

Low-level emulator wrapper:

```python
from femu.arch.armv8m import ARMv8MEmulator

emu = ARMv8MEmulator()
emu.add_memory(0x20000000, 0x10000, "ram")
emu.pc = 0x08000000
emu.step()
```

### Peripheral

Base class for Python peripherals:

```python
from femu import Peripheral, PeripheralRegistry

@PeripheralRegistry.register("my_device")
class MyDevice(Peripheral):
    def read(self, offset, size):
        ...
    def write(self, offset, value, size):
        ...
```

## Dependencies

```text
femu (Python)
├── cffi          # C FFI bindings
├── click         # CLI framework
├── pyyaml        # YAML parsing
├── pyelftools    # ELF loading
└── rich          # Terminal output
```

## Entry Points

- `femu` - Main CLI command (via `python -m femu.cli`)
- `python -m femu` - Alternative entry point
