# AI-Assisted Development

FEMU is designed for AI-assisted development using tools like Claude Code. This guide explains the workflow and available tooling.

## Overview

The codebase provides:

- Clear module boundaries with well-defined interfaces
- Context gathering tools via `femu dev` commands
- Structured session protocols for AI assistants

## Quick Start

1. Identify the module you're working on
2. Gather context:

```bash
femu dev context decoder
```

3. Read the listed files
4. Implement following the header contract

## Available Commands

### Get Module Context

```bash
femu dev context <module>
```

Lists all relevant files for a module:

- Header file (the API contract)
- Type definitions
- Implementation files
- Test files
- README (if exists)

**Available modules:** `decoder`, `executor`, `memory`, `nvic`, `mpu`, `emulator`

### Check Module Status

```bash
femu dev status
```

Shows implementation status for all modules.

### List Modules

```bash
femu dev list
```

Lists all available modules with descriptions.

### Validate Module

```bash
femu dev validate <module>
```

Checks if a module's implementation matches its header contract.

## Architecture-Agnostic Design

The codebase separates generic interfaces from architecture-specific implementations:

| Directory              | Purpose                          |
| ---------------------- | -------------------------------- |
| `include/emu/`         | Generic emulator interfaces      |
| `include/arch/armv8m/` | ARMv8-M specific headers         |
| `src/core/`            | Shared implementations (memory)  |
| `src/arch/armv8m/`     | ARMv8-M specific implementations |

### Naming Conventions

- Generic interfaces: `emu_*` (e.g., `emu_memory.h`, `emu_types.h`)
- ARMv8-M specific: `armv8m_*` (e.g., `armv8m_decoder.h`)

## Session Protocol

When starting work on a module, follow this protocol:

### 1. Announce Your Intent

```markdown
I am implementing the [MODULE] module for an ARMv8-M emulator.

I have read:
- [ ] docs/ARCHITECTURE.md (Parts 5 and 9)
- [ ] include/arch/armv8m/armv8m_[module].h
- [ ] src/arch/armv8m/[module]/README.md (if exists)
- [ ] include/arch/armv8m/armv8m_types.h
- [ ] include/emu/emu_types.h

I will produce:
- [ ] src/arch/armv8m/[module]/[module].c
- [ ] src/arch/armv8m/[module]/tests/test_[module].cpp
```

### 2. Follow Implementation Rules

1. **Match the header exactly** - Function signatures, types, return values
2. **Handle all errors** - Return error codes, never crash
3. **No undefined behavior** - Check bounds, null pointers
4. **Compile clean** - `-Wall -Wextra -Werror -pedantic`
5. **Test everything** - Each public function needs tests

### 3. Code Style

```c
// Use snake_case for functions and variables
int armv8m_decode_instruction(const uint8_t *mem, DecodedInsn *insn);

// Use UPPER_CASE for constants and macros
#define INSN_TYPE_DATA_PROC 0x01

// Use PascalCase for type names
typedef struct DecodedInsn DecodedInsn;
```

### 4. Testing Pattern

```cpp
#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

extern "C" {
#include "arch/armv8m/armv8m_decoder.h"
}

TEST_GROUP(DecoderTest)
{
    DecodedInsn insn;

    void setup() {
        armv8m_decode_init(&insn);
    }

    void teardown() {}
};

TEST(DecoderTest, MovImmediate)
{
    uint8_t code[] = {0x2A, 0x20};  // MOVS R0, #42
    int result = armv8m_decode(code, 0x08000000, &insn);
    CHECK_EQUAL(2, result);
}
```

## Module Reference

### decoder

Decodes Thumb-2 instructions into `DecodedInsn` structures.

```bash
femu dev context decoder
```

**Key files:**

- `include/arch/armv8m/armv8m_decoder.h` - API
- `src/arch/armv8m/decoder/decoder.c` - Main decoder
- `src/arch/armv8m/decoder/decode_thumb16.c` - 16-bit instructions
- `src/arch/armv8m/decoder/decode_thumb32*.c` - 32-bit instructions

### executor

Executes decoded instructions, updating CPU state.

```bash
femu dev context executor
```

**Key files:**

- `include/arch/armv8m/armv8m_executor.h` - API
- `src/arch/armv8m/executor/executor.c` - Main executor

### memory

Memory subsystem with region mapping.

```bash
femu dev context memory
```

**Key files:**

- `include/emu/emu_memory.h` - API (generic)
- `src/core/memory/memory.c` - Implementation

### nvic

Nested Vectored Interrupt Controller.

```bash
femu dev context nvic
```

**Key files:**

- `include/arch/armv8m/armv8m_nvic.h` - API
- `src/arch/armv8m/nvic/nvic.c` - Implementation

### mpu

Memory Protection Unit.

```bash
femu dev context mpu
```

**Key files:**

- `include/arch/armv8m/armv8m_mpu.h` - API
- `src/arch/armv8m/mpu/mpu.c` - Implementation

## Debugging Tips

### When Tests Fail

1. Check the header contract - are you matching it exactly?
2. Check endianness - ARM is little-endian
3. Check bit extraction - use masks carefully
4. Add printf debugging - remove before committing
5. Run with sanitizers enabled (default in Debug builds)

### Common Mistakes

- Forgetting Thumb instructions are little-endian in memory
- Off-by-one in PC calculations (PC is +4 ahead in ARM)
- Not handling the IT block state for conditional execution
- Forgetting to update flags when `set_flags` is true

### Using GDB

```bash
# Build with debug symbols (default)
femu build all

# Run tests under GDB
gdb ./build/src/arch/armv8m/decoder/test_decoder

# Useful commands
(gdb) break armv8m_decode
(gdb) run
(gdb) print insn
(gdb) x/4xb code
```

## References

- ARMv8-M Architecture Reference Manual
- `docs/ARCHITECTURE.md` - System design
- `docs/PLUGINS.md` - Peripheral development
- QEMU source (target/arm/) for reference implementations
