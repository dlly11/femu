# Architecture Overview

FEMU is a lightweight, extensible CPU emulator designed for firmware testing and debugging.

## Design Principles

1. **Layered Architecture** - Clear separation between Python control plane and C simulation core
2. **Module Isolation** - Each module can be developed and tested independently
3. **Architecture Agnostic** - Generic interfaces allow multiple CPU architectures
4. **AI-Friendly** - Designed for AI-assisted development with clear boundaries

## System Layers

```text
┌─────────────────────────────────────────────────────────────────┐
│                    Python Control Plane                          │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐  │
│  │   CLI       │  │   GDB       │  │   Machine Builder       │  │
│  │             │  │   Server    │  │   (ELF loader, config)  │  │
│  └─────────────┘  └─────────────┘  └─────────────────────────┘  │
│  ┌─────────────┐  ┌─────────────────────────────────────────┐   │
│  │  Peripheral │  │   Logging / Tracing                      │   │
│  │  Framework  │  │                                          │   │
│  └─────────────┘  └─────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────┤
│                     CFFI Layer                                   │
├─────────────────────────────────────────────────────────────────┤
│                    C Simulation Core                             │
├─────────────────────────────────────────────────────────────────┤
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────────┐ │
│  │ Decoder  │  │ Executor │  │  Memory  │  │      NVIC        │ │
│  │          │◄─┤          │◄─┤          │  │                  │ │
│  └──────────┘  └──────────┘  └──────────┘  └──────────────────┘ │
│       ▲                           │                    │        │
│       │         ┌─────────────────┘                    │        │
│       │         ▼                                      ▼        │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                  Peripheral Bus                           │   │
│  │    ┌────────┐  ┌────────┐  ┌────────┐  ┌────────────┐    │   │
│  │    │  UART  │  │  GPIO  │  │ Timer  │  │    ...     │    │   │
│  │    └────────┘  └────────┘  └────────┘  └────────────┘    │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

## Python Control Plane

The Python layer provides:

- **CLI**: Command-line interface (`femu` command)
- **GDB Server**: Remote debugging via GDB RSP
- **Machine Builder**: YAML/dict configuration, ELF loading
- **Peripheral Framework**: Python peripherals with C callbacks
- **Logging**: Unified logging across C and Python

Key files:

| Component         | Location                          |
| ----------------- | --------------------------------- |
| CLI               | `python/femu/cli.py`              |
| GDB Server        | `python/femu/gdb_server.py`       |
| Machine           | `python/femu/machine.py`          |
| Peripheral Base   | `python/femu/peripheral.py`       |
| ARMv8M Emulator   | `python/femu/arch/armv8m.py`      |

## C Simulation Core

The C layer provides the high-performance simulation:

### Core Modules (Shared)

| Module   | Purpose                           | Location                    |
| -------- | --------------------------------- | --------------------------- |
| Memory   | Memory regions and access         | `src/core/memory/`          |
| Emulator | Base emulator infrastructure      | `src/core/emulator/`        |
| Logging  | Unified logging utilities         | `src/emu/`                  |

### Architecture-Specific Modules (ARMv8-M)

| Module   | Purpose                           | Location                        |
| -------- | --------------------------------- | ------------------------------- |
| Decoder  | Thumb-2 instruction decoding      | `src/arch/armv8m/decoder/`      |
| Executor | Instruction execution             | `src/arch/armv8m/executor/`     |
| NVIC     | Interrupt controller              | `src/arch/armv8m/nvic/`         |
| MPU      | Memory protection unit            | `src/arch/armv8m/mpu/`          |
| Emulator | ARMv8-M emulator glue             | `src/arch/armv8m/emulator/`     |

## Header Organization

```text
include/
├── emu/                          # Generic interfaces
│   ├── emu_types.h               # Common types (EmuStatus, etc.)
│   ├── emu_memory.h              # Memory interface
│   ├── emu_decoder.h             # Decoder interface
│   ├── emu_executor.h            # Executor interface
│   ├── emu_peripheral.h          # Peripheral interface
│   ├── emu_plugin.h              # Plugin API
│   └── emu_log.h                 # Logging
│
└── arch/armv8m/                  # ARMv8-M specific
    ├── armv8m_types.h            # ARM types (registers, etc.)
    ├── armv8m_decoder.h          # Decoder API
    ├── armv8m_executor.h         # Executor API
    ├── armv8m_nvic.h             # NVIC API
    ├── armv8m_mpu.h              # MPU API
    └── armv8m_emulator.h         # Emulator API
```

## Data Flow

### Instruction Execution

```text
1. Python calls emu->step() or emu->run()
2. Emulator fetches instruction bytes from Memory
3. Decoder parses bytes into DecodedInsn structure
4. Executor executes instruction, updating CPU state
5. If memory access: Memory module handles read/write
6. If peripheral access: Peripheral vtable called
7. If interrupt: NVIC handles priority and dispatch
8. Return to Python with result
```

### Peripheral Access

```text
1. Executor performs memory access to peripheral address
2. Memory module identifies address as peripheral region
3. Memory calls peripheral vtable read/write
4. For Python peripherals:
   a. C calls CFFI callback
   b. CFFI routes to Python Peripheral.read/write
   c. Python returns value through CFFI
5. Value returned to Executor
```

## Key Design Decisions

### Why C for Core?

- Performance: Instruction interpretation is CPU-intensive
- Determinism: Predictable timing behavior
- Portability: Compiles on all platforms

### Why Python for Control Plane?

- Flexibility: Easy scripting and configuration
- Rich ecosystem: Testing, debugging, visualization
- Rapid prototyping: Peripherals in Python first

### Why CFFI?

- Low overhead compared to ctypes
- Compile-time type checking
- Better error messages
- Supports callbacks efficiently

### Why Module Isolation?

- AI-friendly: Each module fits in context window
- Testability: Unit test modules independently
- Maintainability: Clear interfaces prevent coupling

## Supported Architectures

Currently:

- **ARMv8-M Mainline** (Cortex-M33 class)
  - Thumb-2 instruction set
  - Optional FPU, DSP, TrustZone

Future (design allows):

- ARMv7-M (Cortex-M3/M4)
- RISC-V (RV32I/RV32IM)

## See Also

- [C-Python Boundary](c-python-boundary.md) - CFFI interface details
- [C Core Modules](c-core/index.md) - Individual module documentation
- [Python Layer](python-layer/index.md) - Python module documentation
