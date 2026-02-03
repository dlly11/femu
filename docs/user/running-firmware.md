# Running Firmware

This guide covers how to prepare and run ARM firmware on FEMU.

## Supported Architecture

FEMU currently supports:

- **ARMv8-M Mainline** (Cortex-M33 class)
- **Thumb-2 instruction set**
- Optional: FPU (single-precision), DSP extensions

## Firmware Requirements

### File Format

FEMU loads firmware in **ELF format**. The ELF file must contain:

- Valid ARM Thumb-2 code
- Proper section layout (.text, .data, .bss)
- Entry point information

### Memory Layout

Default memory regions (configurable via machine files):

| Region | Start        | Size    | Type |
| ------ | ------------ | ------- | ---- |
| Flash  | `0x00000000` | 512 KB  | ROM  |
| SRAM   | `0x20000000` | 256 KB  | RAM  |
| PPB    | `0xE0000000` | 1 MB    | MMIO |

### Vector Table

ARM Cortex-M firmware requires a vector table at the start of flash:

```c
// Typical vector table
__attribute__((section(".isr_vector")))
const uint32_t vector_table[] = {
    (uint32_t)&_estack,        // Initial SP
    (uint32_t)Reset_Handler,   // Reset handler
    (uint32_t)NMI_Handler,
    (uint32_t)HardFault_Handler,
    // ... more handlers
};
```

## Compiling Firmware

### Basic Compilation

```bash
arm-none-eabi-gcc \
    -mcpu=cortex-m33 \
    -mthumb \
    -mfloat-abi=soft \
    -nostartfiles \
    -T linker.ld \
    -o firmware.elf \
    main.c startup.c
```

### With FPU Support

```bash
arm-none-eabi-gcc \
    -mcpu=cortex-m33 \
    -mthumb \
    -mfpu=fpv5-sp-d16 \
    -mfloat-abi=hard \
    -nostartfiles \
    -T linker.ld \
    -o firmware.elf \
    main.c startup.c
```

### Recommended Flags

```bash
# Debug build (recommended for development)
arm-none-eabi-gcc \
    -mcpu=cortex-m33 \
    -mthumb \
    -g3 \
    -O0 \
    -Wall -Wextra \
    -nostartfiles \
    -T linker.ld \
    -o firmware.elf \
    *.c

# Release build
arm-none-eabi-gcc \
    -mcpu=cortex-m33 \
    -mthumb \
    -Os \
    -DNDEBUG \
    -nostartfiles \
    -T linker.ld \
    -o firmware.elf \
    *.c
```

### Example Linker Script

```c
/* linker.ld */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 512K
    SRAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 256K
}

ENTRY(Reset_Handler)

SECTIONS
{
    .text : {
        KEEP(*(.isr_vector))
        *(.text*)
        *(.rodata*)
    } > FLASH

    .data : {
        _sdata = .;
        *(.data*)
        _edata = .;
    } > SRAM AT > FLASH

    .bss : {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        _ebss = .;
    } > SRAM

    _estack = ORIGIN(SRAM) + LENGTH(SRAM);
}
```

## Running Firmware

### Basic Execution

```bash
femu run firmware.elf
```

The emulator will:

1. Load the ELF file
2. Initialize memory regions
3. Set up the vector table
4. Begin execution at the reset handler

### Verbose Output

```bash
# Show load information
femu run firmware.elf -v

# Show execution details
femu run firmware.elf -vv

# Full trace
femu run firmware.elf -vvv
```

### Limiting Execution

```bash
# Stop after 1 million cycles
femu run firmware.elf --max-cycles 1000000
```

### Tracing Specific Modules

```bash
# Trace instruction decoding
femu run firmware.elf --trace decoder

# Trace memory accesses
femu run firmware.elf --trace memory

# Multiple traces
femu run firmware.elf --trace decoder --trace executor
```

## Execution States

The emulator can stop in several states:

| State        | Description                           |
| ------------ | ------------------------------------- |
| `RUNNING`    | Currently executing                   |
| `HALTED`     | Hit breakpoint or WFI                 |
| `STOPPED`    | Max cycles reached                    |
| `FAULT`      | Exception occurred (HardFault, etc.)  |

## Working with Peripherals

FEMU includes basic peripheral support. Peripherals are mapped to MMIO regions.

### Built-in Peripherals

- **UART** - Serial output
- **GPIO** - General purpose I/O

See [Peripherals](peripherals.md) for usage details.

### Custom Peripherals

Peripherals can be defined in machine configuration files. See [Machine Configuration](machine-configuration.md).

## Example: Minimal Firmware

```c
// main.c
#include <stdint.h>

// Vector table
__attribute__((section(".isr_vector")))
const uint32_t vectors[] = {
    0x20040000,              // Initial SP (top of 256KB SRAM)
    (uint32_t)main,          // Reset handler
};

int main(void) {
    volatile uint32_t *gpio = (uint32_t *)0x40000000;

    // Toggle GPIO
    while (1) {
        *gpio = 1;
        for (volatile int i = 0; i < 100000; i++);
        *gpio = 0;
        for (volatile int i = 0; i < 100000; i++);
    }

    return 0;
}
```

Compile and run:

```bash
arm-none-eabi-gcc -mcpu=cortex-m33 -mthumb -nostartfiles \
    -Wl,--section-start=.isr_vector=0x00000000 \
    -o minimal.elf main.c

femu run minimal.elf -v
```

## Troubleshooting

### "Invalid ELF file"

- Ensure the file is a valid ARM ELF
- Check: `arm-none-eabi-readelf -h firmware.elf`

### "HardFault" on startup

- Verify vector table is at address 0x00000000
- Check initial stack pointer is valid
- Ensure reset handler address is correct (must be odd for Thumb)

### "Memory access fault"

- Check your firmware's memory accesses are within valid regions
- Use `-vv` to see which address caused the fault

### Firmware doesn't start

- Verify entry point: `arm-none-eabi-readelf -h firmware.elf | grep Entry`
- Check reset vector: `arm-none-eabi-objdump -d firmware.elf | head -20`
