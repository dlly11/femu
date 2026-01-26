# FEMU Architecture

> **For AI Development**: This is the primary reference document. Read relevant sections before implementing any module.

## Quick Links
- [Part 5: Proposed Architecture](#part-5-proposed-architecture) - Overall system design
- [Part 8: Multi-Language Peripherals](#part-8-multi-language-peripheral-system) - C/Rust/Python peripheral interface
- [Part 9: AI Development Model](#part-9-ai-driven-development-model) - How to work on this codebase

---

## Overview

FEMU is a lightweight, extensible CPU emulator with:
- **Python control plane** - CLI, GDB server, configuration, scripting
- **C/C++ simulation core** - CPU interpreter, memory, peripherals
- **Multi-language peripherals** - Python (prototyping), C/Rust (performance)

The architecture is specifically designed for **AI-driven development** where each module can be implemented in isolation without requiring the full codebase context.

**Currently supported architectures:**

- ARMv8-M (Cortex-M33 class)

---

## Part 1: ARMv8-M Target Architecture (Mainline)

### Cortex-M33 Features We're Implementing

| Feature | Priority | Notes |
|---------|----------|-------|
| Thumb-2 ISA | P0 | Core instruction set |
| NVIC | P0 | Interrupt handling |
| SysTick | P0 | System timer |
| Exception handling | P0 | Entry/return sequences |
| MPU (PMSAv8) | P1 | Memory protection |
| TrustZone | P2 | Security extensions (optional) |
| FPU | P2 | Single-precision (optional) |
| DSP | P2 | SIMD instructions (optional) |

### Register Set

```
R0-R12      General purpose
R13 (SP)    Stack pointer (banked: MSP, PSP, plus secure variants)
R14 (LR)    Link register
R15 (PC)    Program counter
xPSR        Combined program status register
PRIMASK     Interrupt mask
FAULTMASK   Fault mask
BASEPRI     Base priority mask
CONTROL     Execution control
```

---

## Part 5: Proposed Architecture

### System Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                    Python Control Plane                          │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐  │
│  │   CLI /     │  │   GDB       │  │   Machine Builder       │  │
│  │   Monitor   │  │   Server    │  │   (load ELF, config)    │  │
│  └─────────────┘  └─────────────┘  └─────────────────────────┘  │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐  │
│  │  Peripheral │  │   Event     │  │   Debug/Trace           │  │
│  │  Manager    │  │   Loop      │  │   Infrastructure        │  │
│  └─────────────┘  └─────────────┘  └─────────────────────────┘  │
├─────────────────────────────────────────────────────────────────┤
│                     FFI Layer (cffi)                             │
├─────────────────────────────────────────────────────────────────┤
│                    C/C++ Simulation Core                         │
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
│  │    │  (C)   │  │ (Rust) │  │ (Py)   │  │            │    │   │
│  │    └────────┘  └────────┘  └────────┘  └────────────┘    │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### Module Boundaries

| Module | Language | Size Target | Dependencies |
|--------|----------|-------------|--------------|
| decoder | C | ~800 LOC | types only |
| executor | C | ~1200 LOC | decoder, memory |
| memory | C | ~500 LOC | types only |
| nvic | C | ~600 LOC | types only |
| mpu | C | ~500 LOC | types only |
| core (glue) | C | ~400 LOC | all above |
| emulator.py | Python | ~300 LOC | core (via cffi) |
| gdb_server.py | Python | ~400 LOC | emulator |
| cli.py | Python | ~200 LOC | emulator |

---

## Part 8: Multi-Language Peripheral System

### Unified Peripheral Interface (ABI)

All peripherals—regardless of implementation language—expose this C interface:

```c
// include/peripheral_interface.h

typedef struct {
    uint32_t (*read)(void *ctx, uint32_t offset, uint8_t size);
    void (*write)(void *ctx, uint32_t offset, uint32_t value, uint8_t size);
    void (*reset)(void *ctx);
    void (*tick)(void *ctx, uint64_t cycles);
    void (*destroy)(void *ctx);
    void (*set_irq_callback)(void *ctx, 
        void (*cb)(void *emu_ctx, int irq, int level), void *emu_ctx);
} PeripheralVTable;

typedef struct {
    const char *name;
    const char *type;
    void *context;
    PeripheralVTable vtable;
} Peripheral;
```

### Language-Specific Implementation

**C**: Implement vtable functions directly, export factory function.

**Rust**: Use `#[no_mangle] extern "C"` functions matching the vtable signature.

**Python**: Create wrapper class that cffi calls back into Python methods.

### Registration

```c
int emulator_add_peripheral(Emulator *emu, 
    uint32_t base, uint32_t size, Peripheral *periph);
```

---

## Part 9: AI-Driven Development Model

### Core Principle

> Each module must be implementable in a single AI session without requiring context from other module implementations.

### What AI Needs Per Module

```
┌────────────────────────────────────────┐
│         AI Context Window              │
├────────────────────────────────────────┤
│ 1. docs/ARCHITECTURE.md (this file)    │  ← Skim relevant parts
│ 2. include/<module>.h                  │  ← Full read (interface)
│ 3. src/core/<module>/README.md         │  ← Full read (impl notes)
│ 4. include/armv8m_types.h              │  ← Full read (shared types)
│ 5. Relevant test vectors               │  ← Understand expectations
├────────────────────────────────────────┤
│ OUTPUT: src/core/<module>/*.c          │
│ OUTPUT: src/core/<module>/tests/*.c    │
└────────────────────────────────────────┘
```

### What AI Does NOT Need

- Other module implementations (only headers)
- Python code (unless implementing Python module)
- Peripheral implementations (unless implementing peripheral)
- Build system details (CMake, etc.)

### AI Session Template

When starting work on a module, use this prompt structure:

```markdown
## Task: Implement [module_name] module

## Context Files:
- docs/ARCHITECTURE.md (Part 5 and Part 9)
- include/armv8m_[module].h
- src/core/[module]/README.md
- include/armv8m_types.h

## Requirements:
1. Implementation must match header exactly
2. All functions must handle errors
3. Code must compile with -Wall -Werror -pedantic
4. Add tests for each public function

## Output:
- src/core/[module]/[module].c
- src/core/[module]/tests/test_[module].c
```

### Module Dependency Graph

```
                    armv8m_types.h
                          │
        ┌─────────────────┼─────────────────┐
        │                 │                 │
        ▼                 ▼                 ▼
    decoder.h         memory.h          nvic.h
        │                 │                 │
        │                 │                 │
        ▼                 ▼                 │
    executor.h◄───────────┘                 │
        │                                   │
        └───────────────┬───────────────────┘
                        │
                        ▼
                    armv8m.h (main API)
```

### File Size Guidelines

Keep files under these limits to ensure AI can process them:

| File Type | Max Lines | Rationale |
|-----------|-----------|-----------|
| Header | 150 | Interface only |
| README | 200 | Focused guidance |
| Implementation | 600 | Split if larger |
| Test file | 400 | One test file per source |

---

## Appendix: Development Commands

```bash
# Build everything
mkdir build && cd build && cmake .. && make

# Build single module (for testing)
cd src/core/decoder && make

# Run module tests
cd src/core/decoder && make test

# Run integration tests
pytest tests/integration/

# Start emulator with GDB server
python -m armv8m.cli --gdb-port 3333 firmware.elf
```

---

## Appendix: References

- ARMv8-M Architecture Reference Manual
- "Definitive Guide to ARM Cortex-M23 and Cortex-M33" by Joseph Yiu
- QEMU source: https://github.com/qemu/qemu (target/arm/)
- Renode source: https://github.com/renode/renode
- Unicorn Engine: https://github.com/unicorn-engine/unicorn
