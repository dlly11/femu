/* Linker script for FEMU test firmware */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 32K
}

/* Stack at top of RAM */
_stack_top = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
    /* Vector table at start of flash */
    .vectors :
    {
        KEEP(*(.vectors))
    } > FLASH

    /* Code */
    .text :
    {
        *(.text*)
        *(.rodata*)
    } > FLASH

    /* Initialized data (copied from flash to RAM at startup) */
    .data :
    {
        _data_start = .;
        *(.data*)
        _data_end = .;
    } > RAM AT > FLASH

    /* Zero-initialized data */
    .bss :
    {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
    } > RAM
}
