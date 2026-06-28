# Getting Started

This guide will help you install FEMU and run your first emulation.

## Prerequisites

### Using Nix (Recommended)

FEMU uses [Nix](https://nixos.org/) for reproducible development environments. This is the recommended approach as it handles all dependencies automatically.

1. Install Nix:

   ```bash
   # Linux/macOS
   curl -L https://nixos.org/nix/install | sh
   ```

2. Enable flakes (if not already):

   ```bash
   mkdir -p ~/.config/nix
   echo "experimental-features = nix-command flakes" >> ~/.config/nix/nix.conf
   ```

3. Enter the development shell:

   ```bash
   cd femu
   nix develop
   ```

   Or use [direnv](https://direnv.net/) for automatic activation:

   ```bash
   direnv allow
   ```

### Without Nix

If you prefer not to use Nix, install these dependencies manually:

**Required:**

- GCC 10+ or Clang 12+ (C11 support)
- CMake 3.16+
- Python 3.10+
- ARM cross-compiler (`arm-none-eabi-gcc`)

**Python packages** (using [uv](https://docs.astral.sh/uv/)):

```bash
uv pip install cffi click rich pytest
```

**Optional (for static analysis):**

- cppcheck
- clang-tidy

**Install FEMU:**

```bash
uv pip install -e .
```

## Building FEMU

Once in the development environment:

```bash
# Build with default settings (Debug, GCC, sanitizers enabled)
femu build all

# Build with Clang
femu build all --compiler=clang

# Build Release (no sanitizers, optimized)
femu build all --build-type=Release
```

## Verifying the Installation

Run the test suite to verify everything works:

```bash
# Run all tests
femu test all

# Run only C tests
femu test c

# Run only Python tests
femu test python
```

## Running Your First Emulation

### 1. Prepare Firmware

FEMU loads ARM Cortex-M firmware in ELF format. Your firmware should:

- Target ARMv8-M (Cortex-M33)
- Use Thumb-2 instruction set
- Be compiled with `arm-none-eabi-gcc`

Example compilation:

```bash
arm-none-eabi-gcc -mcpu=cortex-m33 -mthumb -nostartfiles \
    -T linker.ld -o firmware.elf main.c startup.c
```

### 2. Run the Emulator

```bash
# Basic run
femu run firmware.elf

# With verbose output
femu run firmware.elf -v

# With GDB server for debugging
femu run firmware.elf --gdb-port 3333
```

### 3. Connect GDB (Optional)

If you started with `--gdb-port`:

```bash
arm-none-eabi-gdb firmware.elf -ex "target remote :3333"
```

## Next Steps

- [CLI Reference](cli-reference.md) - Full command documentation
- [Running Firmware](running-firmware.md) - Detailed firmware requirements
- [GDB Debugging](gdb-debugging.md) - Debug your firmware
- [Machine Configuration](machine-configuration.md) - Configure memory and peripherals

## Troubleshooting

### "Library not found" errors

Build the project first:

```bash
femu build all
```

### Sanitizer errors on startup

Try building without sanitizers:

```bash
femu build all --no-sanitizers
```

### ARM toolchain not found

Ensure `arm-none-eabi-gcc` is in your PATH. In Nix, this is automatic. Otherwise:

```bash
# Ubuntu/Debian
sudo apt install gcc-arm-none-eabi

# macOS (Homebrew)
brew install arm-none-eabi-gcc
```
