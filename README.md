# ARMv8-M Emulator

A lightweight ARMv8-M (Cortex-M33 class) emulator designed for AI-assisted development.

## Architecture

- **Python control plane** - CLI, GDB server, configuration
- **C/C++ simulation core** - CPU interpreter, memory, NVIC
- **Multi-language peripherals** - Python (rapid prototyping), C/Rust (performance)

## Quick Start

```bash
# Build
mkdir build && cd build
cmake ..
make

# Run with GDB server
python -m armv8m.cli --gdb-port 3333 firmware.elf

# Connect GDB
arm-none-eabi-gdb firmware.elf -ex "target remote :3333"
```

## Project Structure

```
armv8m-emulator/
├── docs/                    # Documentation
│   ├── ARCHITECTURE.md      # System design (READ THIS FIRST)
│   └── AI_DEVELOPMENT.md    # AI development guide
├── include/                 # Public C headers
├── src/core/                # C simulation core
│   ├── decoder/             # Instruction decoder
│   ├── executor/            # Instruction executor
│   ├── memory/              # Memory subsystem
│   ├── nvic/                # Interrupt controller
│   └── mpu/                 # Memory protection
├── peripherals/             # Peripheral implementations
│   ├── c/                   # C peripherals
│   ├── rust/                # Rust peripherals
│   └── python/              # Python peripherals
├── python/armv8m/           # Python package
└── tools/                   # Development tools
```

## For AI Developers

This project is designed for AI-assisted development. Each module can be implemented independently.

```bash
# See which files to read for a module
python tools/ai_session.py context decoder

# Check module status
python tools/ai_session.py status
```

See [docs/AI_DEVELOPMENT.md](docs/AI_DEVELOPMENT.md) for the full guide.

## Module Status

| Module | Status | Description |
|--------|--------|-------------|
| decoder | 📋 Ready | Instruction decoder |
| executor | ⏳ Pending | Instruction executor |
| memory | ⏳ Pending | Memory subsystem |
| nvic | ⏳ Pending | Interrupt controller |
| mpu | ⏳ Pending | Memory protection |

## License

MIT License - See LICENSE file
