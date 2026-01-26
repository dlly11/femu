# ARMv8-M Emulator - Claude Code Instructions

This is a lightweight ARMv8-M (Cortex-M33 class) emulator designed for AI-assisted development.

## Quick Start for AI Sessions

```bash
python tools/ai_session.py context <module>
```

This prints exactly which files to read for a given module.

## Architecture Overview

- **Python control plane** - CLI, GDB server, configuration
- **C/C++ simulation core** - CPU interpreter, memory, NVIC
- **Multi-language peripherals** - Python (prototyping), C/Rust (performance)

## Project Structure

```text
├── docs/
│   ├── ARCHITECTURE.md      # System design (READ FIRST)
│   └── AI_DEVELOPMENT.md    # AI workflow guide
├── include/                 # Public C headers (interfaces)
│   ├── armv8m_types.h       # Shared types (~240 lines)
│   ├── armv8m_decoder.h     # Decoder interface
│   ├── armv8m_executor.h    # Executor interface
│   └── peripheral_interface.h
├── src/core/                # C simulation core (implementations)
│   ├── decoder/
│   ├── executor/
│   ├── memory/
│   ├── nvic/
│   └── mpu/
├── peripherals/             # Multi-language peripherals
│   ├── c/
│   ├── rust/
│   └── python/
└── tools/
    └── ai_session.py        # AI context helper
```

## Module Development

### Core Principle

Each module is implementable in a single AI session without requiring context from other module implementations. Only read headers, not other implementations.

### What to Read Per Module

1. `docs/ARCHITECTURE.md` (Parts 5 and 9)
2. `include/armv8m_<module>.h` (interface definition)
3. `src/core/<module>/README.md` (implementation guidance)
4. `include/armv8m_types.h` (shared types)

### What NOT to Read

- Other module implementations (only their headers)
- Python code (unless implementing Python module)
- Build system details

## Module Status

| Module | Status | Description |
| ------ | ------ | ----------- |
| decoder | Ready | Instruction decoder (Thumb to DecodedInsn) |
| executor | Pending | Instruction executor |
| memory | Pending | Memory subsystem |
| nvic | Pending | Interrupt controller |
| mpu | Pending | Memory protection |

## Code Style

```c
// snake_case for functions and variables
int armv8m_decode_instruction(const uint8_t *mem, DecodedInsn *insn);

// UPPER_CASE for constants and macros
#define INSN_TYPE_DATA_PROC 0x01

// PascalCase for type names
typedef struct DecodedInsn DecodedInsn;
```

## Implementation Rules

1. Match the header interface exactly
2. Handle all errors - return error codes, never crash
3. No undefined behavior - check bounds, null pointers
4. Compile clean with `-Wall -Werror -pedantic`
5. Test each public function

## File Size Limits

| File Type | Max Lines |
| --------- | --------- |
| Header | 150 |
| README | 200 |
| Implementation | 600 (split if larger) |
| Test file | 400 |

## Testing Pattern

```c
static void test_decode_mov_immediate(void) {
    uint8_t code[] = {0x2A, 0x20};  // MOVS R0, #42
    DecodedInsn insn;

    int result = armv8m_decode(code, 0x08000000, &insn);

    assert(result == 2);
    assert(insn.type == INSN_DATA_PROC_IMM);
    assert(insn.rd == 0);
    assert(insn.imm == 42);
}
```

## Common Pitfalls

- Thumb instructions are little-endian in memory
- PC is +4 ahead in ARM during execution
- IT block state must be tracked by caller, not decoder
- Update flags only when `set_flags` is true

## Build Commands

```bash
mkdir build && cd build && cmake .. && make
cd src/core/<module> && make test
```

## References

- ARMv8-M Architecture Reference Manual
- QEMU source: target/arm/
- Unicorn Engine
