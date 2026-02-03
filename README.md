# FEMU

FEMU (Fast EMUlator) - A lightweight, extensible CPU emulator designed for AI-assisted development.

**Currently supported architectures:**

- ARMv8-M (Cortex-M33 class)

## Architecture

- **Python control plane** - CLI, GDB server, configuration
- **C/C++ simulation core** - CPU interpreter, memory, peripherals
- **Multi-language peripherals** - Python (rapid prototyping), C/Rust (performance)

## Prerequisites

This project uses [Nix](https://nixos.org/) for reproducible development environments.

### NixOS / Nix Users

```bash
# Enter the development shell
nix develop

# Or use direnv for automatic shell activation
direnv allow
```

### Without Nix

You'll need:

- GCC or Clang (C11 support)
- ARM cross-compiler (`arm-none-eabi-gcc`)
- CMake 3.16+
- Python 3.10+ with click, rich, pytest
- cppcheck (optional, for static analysis)

Then install the package:

```bash
pip install -e .
```

## Quick Start

```bash
# Enter Nix development environment
nix develop

# Build the project
femu build all

# Run tests
femu test all

# Run with GDB server
femu run firmware.elf --gdb-port 3333

# Connect GDB
arm-none-eabi-gdb firmware.elf -ex "target remote :3333"
```

## CLI Reference

```bash
femu --help                       # Show all commands

# Build
femu build all                    # Configure and compile
femu build all --compiler=clang   # Build with Clang
femu build all --no-sanitizers    # Build without ASan/UBSan
femu build clean                  # Clean build directory
femu build analyze                # Run static analysis

# Test
femu test all                     # Run all tests
femu test c                       # Run C tests only
femu test c --filter=decoder      # Run specific module tests
femu test python                  # Run Python tests only

# Development
femu dev status                   # Show module status
femu dev context <module>         # Show AI context for module
femu dev validate <module>        # Validate implementation

# Documentation
femu docs build                   # Build documentation
femu docs serve                   # Serve docs locally
```

## Building with Different Compilers

```bash
# Using CLI option
femu build all --compiler=gcc
femu build all --compiler=clang

# Using environment variables
CC=clang CXX=clang++ femu build all
```

## Static Analysis

```bash
# Run cppcheck
femu build analyze
```

## Project Structure

```text
femu/
├── docs/                     # Documentation (Sphinx)
│   ├── user/                 # User guides
│   ├── developer/            # Developer guides
│   ├── architecture/         # Architecture documentation
│   └── api/                  # API reference
├── include/
│   ├── emu/                  # Architecture-agnostic interfaces
│   │   ├── emu_types.h       # Core type definitions
│   │   ├── emu_memory.h      # Memory interface
│   │   └── emu_peripheral.h  # Peripheral VTable
│   └── arch/armv8m/          # ARMv8-M specific headers
│       ├── armv8m_types.h    # ARMv8-M types
│       ├── armv8m_decoder.h  # Decoder interface
│       ├── armv8m_executor.h # Executor interface
│       ├── armv8m_nvic.h     # NVIC interface
│       ├── armv8m_mpu.h      # MPU interface
│       └── armv8m_emulator.h # Main emulator API
├── src/
│   ├── core/memory/          # Shared memory subsystem
│   └── arch/armv8m/          # ARMv8-M implementation
│       ├── decoder/          # Instruction decoder
│       ├── executor/         # Instruction executor
│       ├── nvic/             # Interrupt controller
│       └── mpu/              # Memory protection
├── lib/cpputest/             # CppUTest testing framework
├── python/femu/              # Python package
│   ├── cli.py                # Main CLI
│   ├── gdb_server.py         # GDB RSP server
│   └── peripherals/          # Python peripherals
└── tests/                    # Integration tests
```

## Development

### Available Tools

The Nix environment provides:

| Tool                | Purpose                        |
| ------------------- | ------------------------------ |
| `gcc`               | Native x86_64 compilation      |
| `clang`             | Alternative compiler           |
| `arm-none-eabi-gcc` | ARM Cortex-M cross-compilation |
| `cmake`             | Build system                   |
| `python3`           | Python runtime + CLI           |
| `pytest`            | Python testing                 |
| `cppcheck`          | Static analysis                |
| `gdb` + `valgrind`  | Debugging                      |
| `sphinx`            | Documentation generation       |

### Environment Variables

Set automatically in `nix develop`:

```bash
$ARM_CC          # arm-none-eabi-gcc
$CPPUTEST_HOME   # Path to CppUTest library
```

### Sanitizers

Debug builds automatically enable AddressSanitizer and UndefinedBehaviorSanitizer:

```bash
# Build with sanitizers (default)
femu build all

# Build without sanitizers
femu build all --no-sanitizers
```

## For AI Developers

This project is designed for AI-assisted development. Each module can be implemented independently.

```bash
# See which files to read for a module
femu dev context decoder

# Check module status
femu dev status

# Validate your implementation
femu dev validate decoder
```

See the [Developer Guide](docs/developer/ai-development.md) for the full AI development workflow, or read `claude.md` for Claude Code specific instructions.

## Module Status

| Module   | Status   | Description                 |
| -------- | -------- | --------------------------- |
| decoder  | Complete | Thumb-2 instruction decoder |
| executor | Complete | Instruction execution       |
| memory   | Complete | Memory subsystem            |
| nvic     | Complete | Interrupt controller        |
| mpu      | Complete | Memory protection unit      |
| emulator | Complete | Main emulator glue          |

## License

MIT License - See LICENSE file
