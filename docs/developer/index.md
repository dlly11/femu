# Developer Guide

Welcome to the FEMU developer guide. This documentation covers building, testing, and contributing to FEMU.

```{toctree}
:maxdepth: 2
:hidden:

building
testing
debugging
contributing
ai-development
writing-peripherals
```

## Getting Started

- [Building](building.md) - Build system, compilers, and static analysis
- [Testing](testing.md) - Running and writing tests
- [Debugging](debugging.md) - Debugging the emulator (C/Python code, profiling)

## Contributing

- [Contributing](contributing.md) - Code style, git workflow, and review process
- [AI Development](ai-development.md) - AI-assisted development workflow

## Extending FEMU

- [Writing Peripherals](writing-peripherals.md) - Creating custom peripheral implementations

## Quick Reference

```bash
# Build
femu build all                    # Debug build with GCC
femu build all --compiler=clang   # Build with Clang
femu build all --build-type=Release  # Release build

# Test
femu test all                     # Run all tests
femu test c --filter=decoder      # Filter C tests
femu test python -k uart          # Filter Python tests

# Static Analysis
femu build analyze                # Run cppcheck and clang-tidy

# Development
femu dev context decoder          # Get context for a module
femu dev status                   # Check module status
femu dev validate decoder         # Validate implementation

# Documentation
femu docs build                   # Build documentation
femu docs serve                   # Serve locally
```

## Architecture Overview

```text
Python Layer (Machine, GDB Server, Peripherals)
                    ↓ CFFI
C Core (Emulator, Decoder, Executor, Memory, NVIC, MPU)
```

For detailed architecture documentation, see [Architecture Documentation](../architecture/index.md).
