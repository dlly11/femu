# Emulator Glue Layer

This module integrates all ARMv8-M emulator components into a unified API.

## Overview

The emulator glue layer provides:
- Unified initialization of all modules (executor, memory, NVIC, MPU)
- Automatic callback wiring between components
- High-level execution control (step, run, stop)
- Memory and register access APIs
- Breakpoint management
- Peripheral registration

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                         Emulator                             │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │
│  │  Executor   │←→│   Memory    │←→│  System Registers   │ │
│  │  (CPU core) │  │  (regions)  │  │  (NVIC/SCB/MPU)     │ │
│  └─────────────┘  └─────────────┘  └─────────────────────┘ │
│        ↑                ↑                    ↑              │
│        │                │                    │              │
│        └────────────────┼────────────────────┘              │
│                    Callbacks                                 │
│                                                              │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │
│  │    NVIC     │  │     MPU     │  │    Peripherals      │ │
│  │ (interrupts)│  │ (protection)│  │ (UART, GPIO, etc)   │ │
│  └─────────────┘  └─────────────┘  └─────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

## API Summary

### Lifecycle
- `armv8m_emu_default_config()` - Initialize config with defaults
- `armv8m_emu_init()` - Create emulator instance
- `armv8m_emu_destroy()` - Free resources
- `armv8m_emu_reset()` - Reset to initial state

### Memory Setup
- `armv8m_emu_add_flash()` - Add flash memory region
- `armv8m_emu_add_ram()` - Add RAM region
- `armv8m_emu_load()` - Load binary data

### Execution
- `armv8m_emu_step()` - Execute single instruction
- `armv8m_emu_run()` - Run until stop condition
- `armv8m_emu_stop()` - Request stop (thread-safe)

### State Access
- `armv8m_emu_get_reg()` / `set_reg()` - Register access
- `armv8m_emu_get_pc()` / `set_pc()` - Program counter
- `armv8m_emu_get_xpsr()` / `set_xpsr()` - Status register
- `armv8m_emu_read_mem()` / `write_mem()` - Memory access

### Breakpoints
- `armv8m_emu_add_breakpoint()` - Set breakpoint
- `armv8m_emu_remove_breakpoint()` - Clear breakpoint
- `armv8m_emu_clear_breakpoints()` - Clear all

## Usage Example (C)

```c
#include "armv8m_emulator.h"

int main() {
    Emulator emu;
    EmulatorConfig config;

    // Configure
    armv8m_emu_default_config(&config);
    config.has_fpu = true;
    config.num_irqs = 64;

    // Initialize
    armv8m_emu_init(&emu, &config);

    // Set up memory
    armv8m_emu_add_flash(&emu, 0x08000000, 0x80000);
    armv8m_emu_add_ram(&emu, 0x20000000, 0x20000);

    // Load firmware (vector table + code)
    armv8m_emu_load(&emu, 0x08000000, firmware_data, firmware_size);

    // Reset (reads SP/PC from vector table)
    armv8m_emu_reset(&emu);

    // Run
    int64_t cycles = armv8m_emu_run(&emu, 1000000);
    printf("Executed %ld cycles\n", cycles);

    // Cleanup
    armv8m_emu_destroy(&emu);
    return 0;
}
```

## Usage Example (Python)

```python
from femu.emulator import Emulator

# Create and load ELF
emu = Emulator()
elf = emu.load_elf("firmware.elf")

# Run
cycles = emu.run(max_cycles=1_000_000)
print(f"PC: 0x{emu.pc:08x}, cycles: {cycles}")

# Access registers
for name, value in emu.dump_regs().items():
    print(f"{name}: 0x{value:08x}")
```

## GDB Debugging

```bash
# Terminal 1: Start emulator with GDB server
femu run firmware.elf --gdb-port 3333

# Terminal 2: Connect GDB
arm-none-eabi-gdb firmware.elf -ex "target remote :3333"
```

## Building

The emulator is built as both static and shared libraries:

```bash
# Build everything
femu build all

# Libraries created:
# - libarmv8m_emulator.a (static, for C tests)
# - libarmv8m_emulator.so (shared, for Python CFFI)
```

## Testing

```bash
# C tests
femu test c --filter emulator

# Python tests (requires built library)
femu test python -k emulator
```
