# GDB Debugging

FEMU includes a GDB Remote Serial Protocol (RSP) server, allowing you to debug firmware using standard ARM GDB.

## Quick Start

### 1. Start FEMU with GDB Server

```bash
femu run firmware.elf --gdb-port 3333
```

The emulator will:

- Load the firmware
- Start a GDB server on port 3333
- Wait for a GDB connection

### 2. Connect GDB

In another terminal:

```bash
arm-none-eabi-gdb firmware.elf -ex "target remote :3333"
```

Or interactively:

```bash
arm-none-eabi-gdb firmware.elf
(gdb) target remote :3333
```

## GDB Commands

### Execution Control

```text
# Continue execution
(gdb) continue
(gdb) c

# Single step (one instruction)
(gdb) stepi
(gdb) si

# Step over function calls
(gdb) nexti
(gdb) ni

# Stop execution
(gdb) Ctrl+C

# Quit
(gdb) quit
```

### Breakpoints

```text
# Set breakpoint at function
(gdb) break main
(gdb) b main

# Set breakpoint at address
(gdb) break *0x08000100

# Set breakpoint at source line
(gdb) break main.c:42

# List breakpoints
(gdb) info breakpoints
(gdb) i b

# Delete breakpoint
(gdb) delete 1
(gdb) d 1

# Disable/enable breakpoint
(gdb) disable 1
(gdb) enable 1
```

### Watchpoints

```text
# Watch for writes to address
(gdb) watch *0x20000000

# Watch for reads
(gdb) rwatch *0x20000000

# Watch for read or write
(gdb) awatch *0x20000000

# Watch variable
(gdb) watch my_variable

# List watchpoints
(gdb) info watchpoints
```

### Registers

```text
# Show all registers
(gdb) info registers
(gdb) i r

# Show specific register
(gdb) print $r0
(gdb) p $pc

# Set register
(gdb) set $r0 = 42

# Show special registers
(gdb) info registers xpsr
(gdb) info registers msp psp
```

### Memory

```text
# Examine memory (hex words)
(gdb) x/4xw 0x20000000

# Examine memory (bytes)
(gdb) x/16xb 0x08000000

# Examine as instructions
(gdb) x/10i 0x08000000

# Examine as string
(gdb) x/s 0x08001000

# Write to memory
(gdb) set {int}0x20000000 = 0x12345678
```

### Disassembly

```text
# Disassemble current location
(gdb) disassemble

# Disassemble function
(gdb) disassemble main

# Disassemble at address
(gdb) disassemble 0x08000100,0x08000120
```

### Stack

```text
# Show backtrace
(gdb) backtrace
(gdb) bt

# Show stack frame
(gdb) frame

# Move up/down stack
(gdb) up
(gdb) down
```

## Debugging Tips

### Load Symbols

If GDB doesn't have symbols:

```text
(gdb) file firmware.elf
(gdb) target remote :3333
```

### Reset the Target

```text
(gdb) monitor reset
```

### View Source

Ensure firmware was compiled with `-g`:

```text
(gdb) list main
(gdb) list *0x08000100
```

### Conditional Breakpoints

```text
# Break when condition is true
(gdb) break main.c:50 if count > 10
```

### Commands on Breakpoint

```text
(gdb) break my_function
(gdb) commands 1
> print counter
> continue
> end
```

## Example Debug Session

```bash
# Terminal 1: Start emulator
$ femu run firmware.elf --gdb-port 3333
Starting GDB server on port 3333
Waiting for connection...
```

```text
# Terminal 2: Connect GDB
$ arm-none-eabi-gdb firmware.elf

(gdb) target remote :3333
Remote debugging using :3333
0x00000000 in ?? ()

(gdb) break main
Breakpoint 1 at 0x80001a4: file main.c, line 10.

(gdb) continue
Continuing.
Breakpoint 1, main () at main.c:10

(gdb) info registers
r0             0x0      0
r1             0x0      0
...
pc             0x80001a4  0x80001a4 <main>

(gdb) stepi
11          int x = 42;

(gdb) print x
$1 = 42

(gdb) continue
```

## Troubleshooting

### "Connection refused"

- Ensure FEMU is running with `--gdb-port`
- Check the port number matches
- Verify no firewall blocking

### "Remote 'g' packet reply is too long"

This can happen with architecture mismatches. Ensure you're using `arm-none-eabi-gdb`, not host `gdb`.

### Breakpoints don't hit

- Verify the address is correct: `info breakpoints`
- Ensure code at that address is executed
- Check symbol loading: `info files`

### Can't see source code

- Compile with `-g` or `-g3`
- Ensure source files are accessible
- Use `directory` command to add source paths:

```text
(gdb) directory /path/to/sources
```

### Watchpoint not triggering

- Hardware watchpoints have limited slots
- Try software watchpoint: `set can-use-hw-watchpoints 0`

## GDB Configuration

Create a `.gdbinit` file for convenience:

```text
# ~/.gdbinit or project .gdbinit
set confirm off
set pagination off

# ARM-specific
set arm fallback-mode thumb

# Connect to FEMU
define femu
    target remote :3333
end

# Reset and connect
define reset
    monitor reset
    continue
end
```

Then use:

```text
(gdb) femu
(gdb) reset
```
