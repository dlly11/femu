# Python Emulator Layer

The Python emulator layer wraps the C core and provides a high-level API.

## Overview

```text
python/femu/
├── arch/
│   └── armv8m.py       # ARMv8MEmulator class
├── machine.py          # Machine class (high-level)
├── _emulator_cffi.py   # CFFI bindings
└── _cffi_types.py      # Type definitions
```

## ARMv8MEmulator

The main Python wrapper for the C emulator.

### Class Definition

```python
class ARMv8MEmulator:
    """Python wrapper for ARMv8-M emulator."""

    def __init__(self):
        self._emu = lib.armv8m_emulator_create()
        self._peripherals: list[PeripheralBase] = []

    def __del__(self):
        if self._emu:
            lib.armv8m_emulator_destroy(self._emu)
```

### Execution Control

```python
def step(self) -> EmuStatus:
    """Execute one instruction."""
    result = lib.armv8m_emulator_step(self._emu)
    return EmuStatus(result)

def run(self, max_cycles: int = 0) -> int:
    """Run until stopped or max_cycles reached."""
    return lib.armv8m_emulator_run(self._emu, max_cycles)

def stop(self) -> None:
    """Stop execution (from callback or another thread)."""
    lib.armv8m_emulator_stop(self._emu)

def reset(self) -> None:
    """Reset CPU to initial state."""
    lib.armv8m_emulator_reset(self._emu)
```

### Register Access

```python
@property
def pc(self) -> int:
    """Program counter."""
    return lib.armv8m_emulator_get_reg(self._emu, 15)

@pc.setter
def pc(self, value: int) -> None:
    lib.armv8m_emulator_set_reg(self._emu, 15, value)

def get_reg(self, reg: int) -> int:
    """Get general purpose register (0-15)."""
    return lib.armv8m_emulator_get_reg(self._emu, reg)

def set_reg(self, reg: int, value: int) -> None:
    """Set general purpose register."""
    lib.armv8m_emulator_set_reg(self._emu, reg, value)

def dump_regs(self) -> dict[str, int]:
    """Get all registers as dictionary."""
    return {
        "r0": self.get_reg(0),
        "r1": self.get_reg(1),
        # ...
        "pc": self.get_reg(15),
        "xpsr": self.get_xpsr(),
    }
```

### Memory Access

```python
def read_mem(self, addr: int, size: int = 4) -> int:
    """Read from memory."""
    value = ffi.new("uint32_t*")
    result = lib.armv8m_emulator_read_mem(self._emu, addr, value, size)
    if result != 0:
        raise MemoryError(f"Read failed at 0x{addr:08x}")
    return value[0]

def write_mem(self, addr: int, value: int, size: int = 4) -> None:
    """Write to memory."""
    result = lib.armv8m_emulator_write_mem(self._emu, addr, value, size)
    if result != 0:
        raise MemoryError(f"Write failed at 0x{addr:08x}")

def read_bytes(self, addr: int, length: int) -> bytes:
    """Read raw bytes from memory."""
    buf = ffi.new(f"uint8_t[{length}]")
    result = lib.armv8m_emulator_read_bytes(self._emu, addr, buf, length)
    if result != 0:
        raise MemoryError(f"Read failed at 0x{addr:08x}")
    return bytes(ffi.buffer(buf))
```

### Memory Regions

```python
def add_memory(self, base: int, size: int, mem_type: str,
               data: bytes | None = None) -> None:
    """Add a memory region."""
    type_enum = {
        "ram": lib.EMU_MEM_RAM,
        "flash": lib.EMU_MEM_FLASH,
    }[mem_type]

    if data:
        buf = ffi.new(f"uint8_t[{len(data)}]", data)
        result = lib.armv8m_emulator_add_memory(
            self._emu, base, size, type_enum, buf
        )
    else:
        result = lib.armv8m_emulator_add_memory(
            self._emu, base, size, type_enum, ffi.NULL
        )

    if result != 0:
        raise ValueError(f"Failed to add memory at 0x{base:08x}")
```

### Peripherals

```python
def add_peripheral(self, periph: PeripheralBase,
                   base: int, size: int) -> None:
    """Add a peripheral at the given address."""
    result = lib.armv8m_emulator_add_peripheral(
        self._emu, base, size, periph.c_struct
    )
    if result != 0:
        raise ValueError(f"Failed to add peripheral at 0x{base:08x}")

    # Keep reference to prevent GC
    self._peripherals.append(periph)
```

## Machine Class

Higher-level wrapper that handles configuration and ELF loading.

```python
class Machine:
    """High-level machine abstraction."""

    def __init__(self, config: dict):
        self._config = config
        self._emu = ARMv8MEmulator()
        self._peripherals: dict[str, PeripheralBase] = {}

        self._setup_memory()
        self._setup_peripherals()

    @classmethod
    def from_yaml(cls, path: str) -> Machine:
        """Load machine from YAML configuration."""
        with open(path) as f:
            config = yaml.safe_load(f)
        return cls(config)

    @classmethod
    def from_dict(cls, config: dict) -> Machine:
        """Create machine from dictionary."""
        return cls(config)

    def load_elf(self, path: str) -> ElfInfo:
        """Load ELF firmware."""
        # Parse ELF and write sections to memory
        ...

    def run(self, max_cycles: int = 0) -> int:
        """Run the machine."""
        return self._emu.run(max_cycles)

    def get_peripheral(self, name: str) -> PeripheralBase:
        """Get peripheral by name."""
        return self._peripherals[name]

    def __getitem__(self, name: str) -> PeripheralBase:
        """Dict-style peripheral access."""
        return self._peripherals[name]
```

## ELF Loading

```python
def load_elf(self, path: str) -> ElfInfo:
    """Load ELF firmware into emulator memory."""
    from elftools.elf.elffile import ELFFile

    with open(path, "rb") as f:
        elf = ELFFile(f)

        for segment in elf.iter_segments():
            if segment["p_type"] == "PT_LOAD":
                addr = segment["p_paddr"]
                data = segment.data()
                self._emu.write_bytes(addr, data)

        entry = elf.header["e_entry"]
        self._emu.pc = entry

    return ElfInfo(entry=entry, ...)
```

## Status Codes

```python
class EmuStatus(enum.IntEnum):
    """Emulator status codes."""
    OK = 0
    HALTED = 1      # WFI or breakpoint
    STOPPED = 2     # Max cycles or stop() called
    FAULT = 3       # Exception occurred
    ERROR = -1      # Internal error
```

## Callbacks

For breakpoints and watchpoints:

```python
def set_breakpoint(self, addr: int) -> int:
    """Set a breakpoint at address."""
    return lib.armv8m_emulator_set_breakpoint(self._emu, addr)

def remove_breakpoint(self, bp_id: int) -> None:
    """Remove a breakpoint."""
    lib.armv8m_emulator_remove_breakpoint(self._emu, bp_id)

def set_watchpoint(self, addr: int, size: int,
                   watch_type: str) -> int:
    """Set a watchpoint."""
    type_map = {"write": 0, "read": 1, "access": 2}
    return lib.armv8m_emulator_set_watchpoint(
        self._emu, addr, size, type_map[watch_type]
    )
```

## Thread Safety

The emulator is not thread-safe. All operations should be on a single thread:

```python
# Good: Single-threaded
machine.run(1000)
value = machine.read_mem(0x20000000)

# Bad: Multi-threaded access
import threading
t1 = threading.Thread(target=machine.run, args=(1000,))
t2 = threading.Thread(target=lambda: machine.read_mem(0x20000000))
# Don't do this!
```

For GDB server, asyncio provides the event loop, but all emulator calls happen in the main thread.

## See Also

- [C-Python Boundary](../c-python-boundary.md) - CFFI details
- [Peripheral Framework](peripheral-framework.md) - Peripheral integration
