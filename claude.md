# FEMU - Claude Code Instructions

FEMU (Fast EMUlator) - A lightweight, extensible CPU emulator designed for AI-assisted development. Currently supports ARMv8-M (Cortex-M33 class) with more architectures planned.

## Quick Start for AI Sessions

```bash
femu dev context <module>
```

This prints exactly which files to read for a given module.

## Development Environment

This project uses Nix for reproducible development environments.

```bash
# Enter development shell (required before any development)
nix develop

# Or with direnv (auto-activates when entering directory)
direnv allow
```

The shell automatically installs the `femu` CLI tool.

### Available Tools

| Tool                | Purpose                     |
| ------------------- | --------------------------- |
| `gcc`               | Native C/C++ compilation    |
| `clang`             | Alternative compiler        |
| `arm-none-eabi-gcc` | ARM Cortex-M cross-compiler |
| `cmake`             | Build system                |
| `python3`           | Python runtime              |
| `pytest`            | Python testing              |
| `gdb`               | Debugging                   |
| `valgrind`          | Memory analysis             |
| `cppcheck`          | C/C++ static analysis       |
| `clang-tidy`        | Clang static analyzer       |

### Environment Variables

These are set automatically when entering `nix develop`:

```bash
$ARM_CC          # arm-none-eabi-gcc (for cross-compilation)
$ARM_OBJCOPY     # arm-none-eabi-objcopy
$ARM_OBJDUMP     # arm-none-eabi-objdump
$CPPUTEST_HOME   # Path to CppUTest ($PWD/lib/cpputest)
```

### First-Time Setup

```bash
# 1. Enter Nix environment
nix develop

# 2. Build the project
femu build all

# 3. Verify tests pass
femu test all
```

## CLI Reference

### Build Commands

```bash
femu build configure                    # Configure CMake (Debug)
femu build configure --build-type Release  # Configure for Release
femu build configure --compiler=clang   # Use Clang instead of GCC
femu build configure --no-sanitizers    # Disable ASan/UBSan
femu build compile                      # Compile the project
femu build all                          # Configure and compile
femu build all --compiler=clang         # Build with Clang
femu build clean                        # Clean build directory
femu build analyze                      # Run all static analyzers
femu build analyze --tool=cppcheck      # Run specific analyzer
```

### Test Commands

```bash
femu test c                             # Run C tests (CppUTest)
femu test c --filter=decoder            # Run only decoder tests
femu test python                        # Run Python tests (pytest)
femu test all                           # Run all tests
```

### Development Commands

```bash
femu dev context <module>               # Show AI context files for module
femu dev status                         # Show status of all modules
femu dev list                           # List all available modules
femu dev validate <module>              # Validate module implementation
femu dev validate-all                   # Validate all implemented modules
```

### Run Commands (Future)

```bash
femu run firmware.elf --gdb-port 3333   # Run with GDB server
```

## Building with Different Compilers

The project supports both GCC and Clang:

```bash
# Via CLI option
femu build all --compiler=gcc
femu build all --compiler=clang

# Via environment variables
CC=clang CXX=clang++ femu build all

# Clean rebuild with different compiler
femu build clean && femu build all --compiler=clang
```

## Static Analysis

### Running Analysis

```bash
# Run all analyzers
femu build analyze

# Run specific analyzer
femu build analyze --tool=cppcheck
femu build analyze --tool=clang-tidy

# Via CMake directly
cmake --build build --target cppcheck
cmake --build build --target clang-tidy
cmake --build build --target analyze
```

### What Gets Checked

- **cppcheck**: General static analysis, undefined behavior, memory leaks
- **clang-tidy**: Modern C++ checks, readability, performance, bug-prone patterns

Configuration file: `.clang-tidy`

## Dynamic Analysis (Sanitizers)

Debug builds automatically enable:

- **AddressSanitizer (ASan)**: Buffer overflows, use-after-free, memory leaks
- **UndefinedBehaviorSanitizer (UBSan)**: Integer overflow, null pointer dereference

```bash
# Build with sanitizers (default for Debug)
femu build all

# Build without sanitizers
femu build all --no-sanitizers

# Release builds never have sanitizers
femu build all --build-type=Release
```

## Architecture Overview

- **Python control plane** - CLI, GDB server, configuration
- **C/C++ simulation core** - CPU interpreter, memory, NVIC
- **Multi-language peripherals** - Python (prototyping), C/Rust (performance)

## Project Structure

```text
├── .clang-tidy               # clang-tidy configuration
├── CMakeLists.txt            # Root CMake build
├── flake.nix                 # Nix development environment
├── docs/
│   ├── ARCHITECTURE.md       # System design (READ FIRST)
│   └── AI_DEVELOPMENT.md     # AI workflow guide
├── include/                  # Public C headers (interfaces)
│   ├── armv8m_types.h        # Shared types
│   ├── armv8m_decoder.h      # Decoder interface
│   ├── armv8m_executor.h     # Executor interface
│   ├── armv8m_memory.h       # Memory interface
│   ├── armv8m_nvic.h         # NVIC interface
│   ├── armv8m_mpu.h          # MPU interface
│   └── peripheral_interface.h
├── src/core/                 # C simulation core (implementations)
│   ├── decoder/
│   │   ├── README.md         # Implementation guide
│   │   ├── CMakeLists.txt    # Module build config
│   │   └── tests/            # CppUTest tests
│   ├── executor/
│   ├── memory/
│   ├── nvic/
│   └── mpu/
├── lib/cpputest/             # CppUTest testing framework (submodule)
├── python/femu/              # Python package (CLI, tools)
│   ├── __init__.py           # Package init
│   ├── cli.py                # Main CLI entry point
│   ├── build.py              # Build system integration
│   └── dev/                  # Development tools
│       ├── session.py        # AI session helpers
│       ├── validate.py       # Module validation
│       └── test.py           # Test runners
└── python/tests/             # Python tests
```

## Module Development

### Core Principle

Each module is implementable in a single AI session without requiring context from other module implementations. Only read headers, not other implementations.

### Module Development Workflow

1. **Get context files**:

   ```bash
   femu dev context <module>
   ```

2. **Read the required files** (in order):
   - `docs/ARCHITECTURE.md` (Parts 5 and 9)
   - `include/armv8m_<module>.h` (interface definition)
   - `src/core/<module>/README.md` (implementation guidance)
   - `include/armv8m_types.h` (shared types)

3. **Implement the module**:
   - Create `src/core/<module>/<module>.c`
   - Match the header interface exactly
   - Handle all errors with return codes

4. **Write tests**:
   - Edit `src/core/<module>/tests/test_<module>.cpp`
   - Test each public function
   - Include edge cases

5. **Build and test**:

   ```bash
   femu build all
   femu test c --filter=<module>
   ```

6. **Run static analysis**:

   ```bash
   femu build analyze
   ```

7. **Validate**:

   ```bash
   femu dev validate <module>
   ```

### What to Read Per Module

| Module   | Header File           | README Location              |
| -------- | --------------------- | ---------------------------- |
| decoder  | `armv8m_decoder.h`    | `src/core/decoder/README.md` |
| executor | `armv8m_executor.h`   | `src/core/executor/README.md`|
| memory   | `armv8m_memory.h`     | `src/core/memory/README.md`  |
| nvic     | `armv8m_nvic.h`       | `src/core/nvic/README.md`    |
| mpu      | `armv8m_mpu.h`        | `src/core/mpu/README.md`     |

### What NOT to Read

- Other module implementations (only their headers if needed)
- Python code (unless implementing Python integration)
- Build system details (CMakeLists.txt)

## Module Status

| Module   | Status  | Description                                |
| -------- | ------- | ------------------------------------------ |
| decoder  | Ready   | Instruction decoder (Thumb to DecodedInsn) |
| executor | Pending | Instruction executor                       |
| memory   | Pending | Memory subsystem                           |
| nvic     | Pending | Interrupt controller                       |
| mpu      | Pending | Memory protection                          |

## Testing

### C Tests (CppUTest)

Tests use CppUTest framework. Test files are in `src/core/<module>/tests/`.

```cpp
#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

extern "C" {
#include "armv8m_decoder.h"
#include "armv8m_types.h"
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
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
}

int main(int argc, char** argv)
{
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
```

### Running Tests

```bash
# All C tests
femu test c

# Specific module tests
femu test c --filter=decoder
femu test c --filter=memory

# Verbose output
femu test c -v

# Python tests
femu test python

# All tests
femu test all
```

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
4. Compile clean with `-Wall -Wextra -Werror -pedantic`
5. Pass static analysis (cppcheck, clang-tidy)
6. Test each public function with CppUTest
7. Sanitizers must report no issues

## File Size Limits

| File Type      | Max Lines             |
| -------------- | --------------------- |
| Header         | 150                   |
| README         | 200                   |
| Implementation | 600 (split if larger) |
| Test file      | 400                   |

## Common Pitfalls

- Thumb instructions are little-endian in memory
- PC is +4 ahead in ARM during execution
- IT block state must be tracked by caller, not decoder
- Update flags only when `set_flags` is true
- Always check for null pointers in public functions
- Use `uint32_t` for addresses, not `int`

## Debugging

```bash
# Build with debug symbols (default)
femu build all

# Run tests under GDB
gdb ./build/src/core/decoder/test_decoder

# Run with Valgrind
valgrind --leak-check=full ./build/src/core/decoder/test_decoder

# Sanitizer output is automatic in Debug builds
```

## References

- ARMv8-M Architecture Reference Manual
- QEMU source: target/arm/
- Unicorn Engine
