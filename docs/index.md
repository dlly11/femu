# FEMU Documentation

FEMU is a lightweight, extensible CPU emulator for ARM Cortex-M firmware development and testing.

```{toctree}
:maxdepth: 2
:caption: User Guide
:hidden:

user/index
```

```{toctree}
:maxdepth: 2
:caption: Developer Guide
:hidden:

developer/index
```

```{toctree}
:maxdepth: 2
:caption: Architecture
:hidden:

architecture/index
```

```{toctree}
:maxdepth: 2
:caption: API Reference
:hidden:

api/index
```

## Quick Start

```bash
# Install (using Nix)
nix develop

# Build
femu build all

# Run firmware
femu run firmware.elf

# With GDB debugging
femu run firmware.elf --gdb-port 3333
```

## Documentation Sections

### For Users

If you want to **use** FEMU to run and debug firmware:

- {doc}`user/getting-started` - Installation and first steps
- {doc}`user/running-firmware` - How to run ARM firmware
- {doc}`user/cli-reference` - Complete command-line reference
- {doc}`user/gdb-debugging` - Using GDB with FEMU
- {doc}`user/machine-configuration` - YAML configuration format
- {doc}`user/peripherals` - Built-in peripheral usage

### For Developers

If you want to **contribute** to FEMU:

- {doc}`developer/building` - Build system guide
- {doc}`developer/testing` - Testing guide
- {doc}`developer/contributing` - Code style and workflow
- {doc}`developer/ai-development` - AI-assisted development
- {doc}`developer/writing-peripherals` - Peripheral development

### Architecture

If you want to understand **how FEMU works**:

- {doc}`architecture/overview` - High-level system design
- {doc}`architecture/c-python-boundary` - CFFI interface details
- {doc}`architecture/c-core/index` - C core modules
- {doc}`architecture/python-layer/index` - Python layer

### API Reference

Auto-generated API documentation:

- {doc}`api/c/index` - C API reference
- {doc}`api/python/index` - Python API reference

## Features

- **ARMv8-M Emulation** - Cortex-M33 class support
- **Thumb-2 ISA** - Full Thumb-2 instruction set
- **GDB Debugging** - Remote debugging via GDB RSP
- **Python Peripherals** - Easy peripheral development
- **YAML Configuration** - Declarative machine definitions
