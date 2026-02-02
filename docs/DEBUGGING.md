# Debugging Guide

This guide covers debugging techniques for FEMU, including debugging emulated firmware, the emulator itself, and using VS Code integration.

## Prerequisites

```bash
# Enter development environment
nix develop

# Build with debug symbols (default)
femu build all
```

## Debugging Emulated Firmware

Debug ARM firmware running inside FEMU using GDB.

### Starting the GDB Server

```bash
# Start emulator with GDB server on port 3333
femu run tests/firmware/test_basic.elf --gdb-port 3333
```

The emulator will wait for a GDB connection before executing.

### Connecting with GDB

```bash
# In another terminal
arm-none-eabi-gdb tests/firmware/test_basic.elf
(gdb) target remote :3333
(gdb) break main
(gdb) continue
```

### Common GDB Commands

| Command | Description |
|---------|-------------|
| `target remote :3333` | Connect to FEMU GDB server |
| `break main` | Set breakpoint at main |
| `break *0x08000100` | Set breakpoint at address |
| `continue` | Continue execution |
| `stepi` | Step one instruction |
| `nexti` | Step over one instruction |
| `info registers` | Show all registers |
| `x/10i $pc` | Disassemble 10 instructions at PC |
| `x/10xw 0x20000000` | Examine 10 words at address |
| `watch *0x20000000` | Set write watchpoint |
| `rwatch *0x20000000` | Set read watchpoint |
| `awatch *0x20000000` | Set access watchpoint |

### Using Watchpoints

Watchpoints halt execution when memory is accessed:

```bash
(gdb) target remote :3333
(gdb) watch *0x20000000      # Break on write
(gdb) rwatch *0x20000004     # Break on read
(gdb) awatch *0x20000008     # Break on read or write
(gdb) continue
```

When a watchpoint triggers, GDB shows the old and new values.

## Debugging the Emulator (C Code)

Debug the emulator's C implementation using GDB.

### Method 1: Debug Test Executables

```bash
# Build tests
femu build all

# Debug a specific test
gdb ./build/src/arch/armv8m/decoder/test_decoder
(gdb) break armv8m_decode
(gdb) run
```

### Method 2: Debug via Python

The emulator C library is loaded by Python. To debug:

```bash
gdb python
(gdb) set follow-fork-mode child
(gdb) break armv8m_emu_step
(gdb) run -m femu run tests/firmware/test_basic.elf -vvv
```

### Useful Breakpoints

| Breakpoint | Purpose |
|------------|---------|
| `armv8m_decode` | Instruction decoding |
| `armv8m_execute` | Instruction execution |
| `armv8m_emu_step` | Single step |
| `memory_read` | Memory reads |
| `memory_write` | Memory writes |
| `nvic_set_pending` | Interrupt pending |
| `nvic_exception_entry` | Exception entry |

### Memory Debugging with Valgrind

```bash
# Check for memory errors
valgrind --leak-check=full ./build/src/arch/armv8m/decoder/test_decoder

# Check for undefined behavior
valgrind --track-origins=yes ./build/src/arch/armv8m/executor/test_executor
```

### Address Sanitizer

Debug builds enable AddressSanitizer automatically:

```bash
# Build with sanitizers (default)
femu build all

# Run tests - ASan reports errors automatically
femu test c
```

To disable sanitizers:

```bash
femu build all --no-sanitizers
```

## Debugging the Emulator (Python Code)

### Using pdb

```bash
# Insert breakpoint in Python code
# Add this line: import pdb; pdb.set_trace()

# Run with verbose output
python -m femu run tests/firmware/test_basic.elf -vvv
```

### Using debugpy (VS Code)

1. Open VS Code
2. Select "Debug Emulator (Python)" configuration
3. Set breakpoints in Python files
4. Press F5

## VS Code Integration

### Setup

1. Open the FEMU project in VS Code
2. Install recommended extensions when prompted (or run `Extensions: Show Recommended Extensions`)
3. Enter the Nix development shell: `nix develop`

### Debug Configurations

| Configuration | Purpose |
|---------------|---------|
| Debug Firmware (FEMU) | Debug ARM firmware with Cortex-Debug |
| Debug Emulator (C) | Debug emulator C code |
| Debug Emulator (Python) | Debug emulator Python code |
| Debug C Tests | Debug CppUTest tests |
| Debug Python Tests | Debug pytest tests |

### Debugging Firmware with Cortex-Debug

1. Start the GDB server task: `Terminal > Run Task > FEMU: Start GDB Server`
2. Select "Debug Firmware (FEMU)" configuration
3. Press F5

Cortex-Debug provides:
- Register view (R0-R15, PSR, etc.)
- Peripheral register view
- Memory view
- Disassembly view
- Call stack

### Debugging C Code

1. Build: `Terminal > Run Task > FEMU: Build Debug`
2. Select "Debug C Tests" configuration
3. Set breakpoints in C files
4. Press F5

### Debugging Python Code

1. Select "Debug Emulator (Python)" configuration
2. Set breakpoints in Python files
3. Press F5

## Logging and Tracing

### Verbosity Levels

```bash
femu run firmware.elf           # Errors only
femu run firmware.elf -v        # + Warnings
femu run firmware.elf -vv       # + Info
femu run firmware.elf -vvv      # + Debug (instruction trace)
```

### Instruction Tracing

With `-vvv`, each executed instruction is logged:

```
DEBUG: 0x08000100: MOVS R0, #42
DEBUG: 0x08000102: LDR R1, [R2, #0]
DEBUG: 0x08000104: ADD R0, R0, R1
```

### Memory Access Logging

Memory reads and writes are logged at debug level:

```
DEBUG: MEM READ:  0x20000000 = 0x12345678 (4 bytes)
DEBUG: MEM WRITE: 0x20000004 <- 0xDEADBEEF (4 bytes)
```

### Exception Logging

```
INFO: Exception entry: IRQ 16 (priority 0)
INFO: Exception return: 0xFFFFFFF9
```

## Common Debugging Scenarios

### Instruction Decoding Issues

```bash
# Run decoder tests with filter
femu test c --filter=decoder -v

# Debug specific instruction
gdb ./build/src/arch/armv8m/decoder/test_decoder
(gdb) break armv8m_decode
(gdb) run -n DecoderTest
```

### Memory Access Faults

```bash
# Enable verbose logging
femu run firmware.elf -vvv 2>&1 | grep -E "(FAULT|MEM)"

# Check MPU configuration
(gdb) print emu->mpu
```

### Interrupt Handling Issues

```bash
# Trace NVIC operations
femu run firmware.elf -vvv 2>&1 | grep -E "(NVIC|IRQ|Exception)"

# Debug NVIC
gdb ./build/src/arch/armv8m/nvic/test_nvic
```

### Stack Corruption

```bash
# Check stack pointer
(gdb) info registers sp
(gdb) x/20xw $sp

# Watch stack writes
(gdb) watch *($sp - 100)
(gdb) continue
```

### Infinite Loops

```bash
# Interrupt with Ctrl+C, then examine
(gdb) target remote :3333
(gdb) continue
^C
(gdb) info registers pc
(gdb) x/5i $pc
```

## Performance Profiling

### Using perf

```bash
# Profile emulator
perf record femu run firmware.elf
perf report

# Flame graph
perf script | stackcollapse-perf.pl | flamegraph.pl > flame.svg
```

### Instruction Counting

```bash
# Count instructions executed
femu run firmware.elf --max-instructions 1000000 --stats
```

## Troubleshooting

### GDB Connection Refused

```bash
# Check if server is running
netstat -tlnp | grep 3333

# Restart with explicit port
femu run firmware.elf --gdb-port 3333
```

### Breakpoints Not Working

- Ensure firmware is built with debug symbols (`-g`)
- Check address matches loaded location
- Verify GDB architecture: `(gdb) show architecture`

### Cortex-Debug Issues

- Check `arm-none-eabi-gdb` is in PATH
- Verify GDB server started successfully
- Check Output panel for errors

### Sanitizer False Positives

```bash
# Suppress specific errors
export ASAN_OPTIONS="detect_leaks=0"
export UBSAN_OPTIONS="print_stacktrace=1"
```
