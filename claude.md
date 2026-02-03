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
| `doxygen`           | C documentation             |
| `sphinx-build`      | Python/unified documentation|

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

### Documentation Commands

```bash
femu docs build                         # Build documentation locally
femu docs build --clean                 # Clean build first
femu docs serve                         # Serve docs at localhost:8000
```

### Run Commands

```bash
femu run firmware.elf                   # Run firmware
femu run firmware.elf --gdb-port 3333   # Run with GDB server
femu run firmware.elf -v                # Verbose output
femu run firmware.elf --trace executor  # Trace specific module
```

## Architecture Overview

- **Python control plane** - CLI, GDB server, configuration
- **C simulation core** - CPU interpreter, memory, NVIC, MPU
- **Multi-language peripherals** - Python (prototyping), C/Rust (future)

The codebase uses an **architecture-agnostic design**:

- `include/emu/` - Generic interfaces (emu_types.h, emu_memory.h, etc.)
- `include/arch/armv8m/` - ARMv8-M specific headers
- `src/core/` - Shared implementations (memory)
- `src/arch/armv8m/` - ARMv8-M specific implementations

## Project Structure

```text
femu/
├── .clang-tidy               # clang-tidy configuration
├── CMakeLists.txt            # Root CMake build
├── Doxyfile                  # Doxygen configuration
├── flake.nix                 # Nix development environment
├── docs/
│   ├── ARCHITECTURE.md       # System design (READ FIRST)
│   ├── AI_DEVELOPMENT.md     # AI workflow guide
│   ├── PLUGINS.md            # Peripheral development
│   ├── MACHINES.md           # Machine configuration
│   └── DEBUGGING.md          # GDB debugging guide
├── include/
│   ├── emu/                  # Architecture-agnostic interfaces
│   │   ├── emu_types.h       # Core type definitions
│   │   ├── emu_memory.h      # Memory interface
│   │   ├── emu_peripheral.h  # Peripheral VTable
│   │   ├── emu_decoder.h     # Decoder interface
│   │   ├── emu_executor.h    # Executor interface
│   │   └── emu_emulator.h    # Emulator interface
│   └── arch/armv8m/          # ARMv8-M specific headers
│       ├── armv8m_types.h    # ARMv8-M types
│       ├── armv8m_decoder.h  # Decoder interface
│       ├── armv8m_executor.h # Executor interface
│       ├── armv8m_nvic.h     # NVIC interface
│       ├── armv8m_mpu.h      # MPU interface
│       └── armv8m_emulator.h # Main emulator API
├── src/
│   ├── emu/                  # Generic logging
│   │   └── emu_log.c
│   ├── core/                 # Architecture-agnostic core
│   │   ├── memory/           # Memory subsystem
│   │   └── emulator/         # Emulator base
│   └── arch/armv8m/          # ARMv8-M implementation
│       ├── decoder/          # Instruction decoder
│       │   ├── README.md
│       │   ├── decoder.c
│       │   ├── decode_thumb16.c
│       │   ├── decode_thumb32*.c
│       │   └── tests/
│       ├── executor/         # Instruction executor
│       │   ├── README.md
│       │   ├── executor.c
│       │   ├── exec_*.c
│       │   └── tests/
│       ├── nvic/             # Interrupt controller
│       │   ├── README.md
│       │   ├── nvic.c
│       │   └── tests/
│       ├── mpu/              # Memory protection
│       │   ├── README.md
│       │   ├── mpu.c
│       │   └── tests/
│       ├── armv8m_emulator.c # Main emulator
│       └── armv8m_cpu.c      # CPU state
├── lib/cpputest/             # CppUTest (submodule)
├── python/femu/              # Python package
│   ├── cli.py                # Main CLI
│   ├── emulator.py           # Python emulator wrapper
│   ├── gdb_server.py         # GDB RSP server
│   ├── peripheral.py         # Peripheral base class
│   ├── arch/                 # Architecture bindings
│   │   └── armv8m.py
│   ├── peripherals/          # Built-in peripherals
│   │   ├── uart.py
│   │   └── gpio.py
│   └── dev/                  # Development tools
│       ├── session.py        # AI session helpers
│       ├── validate.py       # Module validation
│       └── test.py           # Test runners
└── tests/                    # Integration tests
    └── firmware/             # ARM firmware tests
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
   - `include/arch/armv8m/armv8m_<module>.h` (interface)
   - `src/arch/armv8m/<module>/README.md` (guidance)
   - `include/arch/armv8m/armv8m_types.h` (shared types)
   - `include/emu/emu_types.h` (generic types)

3. **Implement the module**:

   - Create/edit `src/arch/armv8m/<module>/<module>.c`
   - Match the header interface exactly
   - Handle all errors with return codes

4. **Write tests**:

   - Edit `src/arch/armv8m/<module>/tests/test_<module>.cpp`
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

### What to Read Per Module

| Module   | Header File                              | README Location                      |
| -------- | ---------------------------------------- | ------------------------------------ |
| decoder  | `include/arch/armv8m/armv8m_decoder.h`   | `src/arch/armv8m/decoder/README.md`  |
| executor | `include/arch/armv8m/armv8m_executor.h`  | `src/arch/armv8m/executor/README.md` |
| memory   | `include/emu/emu_memory.h`               | `src/core/memory/README.md`          |
| nvic     | `include/arch/armv8m/armv8m_nvic.h`      | `src/arch/armv8m/nvic/README.md`     |
| mpu      | `include/arch/armv8m/armv8m_mpu.h`       | `src/arch/armv8m/mpu/README.md`      |
| emulator | `include/arch/armv8m/armv8m_emulator.h`  | `src/arch/armv8m/README.md`          |

### What NOT to Read

- Other module implementations (only their headers if needed)
- Python code (unless implementing Python integration)
- Build system details (CMakeLists.txt)

## Module Status

| Module   | Status   | Description                    |
| -------- | -------- | ------------------------------ |
| decoder  | Complete | Thumb-2 instruction decoder    |
| executor | Complete | Instruction execution engine   |
| memory   | Complete | Memory subsystem               |
| nvic     | Complete | Interrupt controller           |
| mpu      | Complete | Memory protection unit         |
| emulator | Complete | Main emulator glue layer       |

## Testing

### C Tests (CppUTest)

Tests use CppUTest framework. Test files are in `src/arch/armv8m/<module>/tests/`.

```cpp
#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

extern "C" {
#include "arch/armv8m/armv8m_decoder.h"
#include "arch/armv8m/armv8m_types.h"
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
femu test c --filter=executor

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
gdb ./build/src/arch/armv8m/decoder/test_decoder

# Run with Valgrind
valgrind --leak-check=full ./build/src/arch/armv8m/decoder/test_decoder

# Sanitizer output is automatic in Debug builds
```

## References

- ARMv8-M Architecture Reference Manual
- QEMU source: target/arm/
- Unicorn Engine
