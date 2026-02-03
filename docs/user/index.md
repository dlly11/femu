# User Guide

Welcome to the FEMU user guide. This documentation covers how to use FEMU to emulate and debug ARM Cortex-M firmware.

```{toctree}
:maxdepth: 2
:hidden:

getting-started
running-firmware
cli-reference
gdb-debugging
machine-configuration
peripherals
```

## Getting Started

- [Getting Started](getting-started.md) - Installation and first steps
- [Running Firmware](running-firmware.md) - How to run ARM firmware

## Reference

- [CLI Reference](cli-reference.md) - Complete command-line reference
- [Machine Configuration](machine-configuration.md) - YAML configuration format
- [Peripherals](peripherals.md) - Built-in peripheral usage

## Debugging

- [GDB Debugging](gdb-debugging.md) - Using GDB with FEMU

## Quick Links

```bash
# Build FEMU
femu build all

# Run firmware
femu run firmware.elf

# Run with GDB server
femu run firmware.elf --gdb-port 3333

# Run tests
femu test all
```
