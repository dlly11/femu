# Architecture Documentation

This section documents the internal architecture of FEMU.

```{toctree}
:maxdepth: 2
:hidden:

overview
c-python-boundary
c-core/index
python-layer/index
```

## Overview

- [Architecture Overview](overview.md) - High-level system design
- [C-Python Boundary](c-python-boundary.md) - CFFI interface

## C Core

The C simulation core provides high-performance instruction execution.

- [C Core Index](c-core/index.md)
- [Decoder](c-core/decoder.md) - Thumb-2 instruction decoding
- [Executor](c-core/executor.md) - Instruction execution
- [Memory](c-core/memory.md) - Memory subsystem
- [NVIC](c-core/nvic.md) - Interrupt controller
- [MPU](c-core/mpu.md) - Memory protection

## Python Layer

The Python layer provides the user interface and peripheral framework.

- [Python Layer Index](python-layer/index.md)
- [Emulator](python-layer/emulator.md) - Python wrapper
- [GDB Server](python-layer/gdb-server.md) - Remote debugging
- [Peripheral Framework](python-layer/peripheral-framework.md) - Python peripherals

## Quick Reference

### Directory Layout

```text
include/
├── emu/                    # Generic interfaces
└── arch/armv8m/            # ARMv8-M headers

src/
├── emu/                    # Logging utilities
├── core/                   # Shared modules (memory)
└── arch/armv8m/            # ARMv8-M modules
    ├── decoder/
    ├── executor/
    ├── nvic/
    ├── mpu/
    └── emulator/

python/femu/
├── arch/                   # Architecture wrappers
├── gdb/                    # GDB server
├── peripherals/            # Built-in peripherals
└── ...
```

### Data Flow

```text
Python: Machine.run()
           │
           ▼
CFFI:  armv8m_emulator_run()
           │
           ▼
C:     Decode → Execute → Memory Access
           │         │
           │         ▼
           │    Peripheral (vtable)
           │         │
           ▼         ▼
       NVIC    Python Peripheral.read/write
```
