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

    /* Test result area - FIRST in RAM to keep fixed addresses */
    .testresults (NOLOAD) :
    {
        _test_results_start = .;
        . += 0x100;                /* 256 bytes for test results at 0x20000000 */
        _test_results_end = .;
        _test_counters = .;        /* Pass/fail counters at 0x20000100 */
        . += 8;                    /* 2 x 4 bytes for counters */
        _test_counters_end = .;
    } > RAM

    /* Initialized data (copied from flash to RAM at startup) */
    .data :
    {
        _data_start = .;
        *(.data*)
        _data_end = .;
    } > RAM AT > FLASH

    /* Load address of .data section (for C startup to copy) */
    _data_load = LOADADDR(.data);

    /* Zero-initialized data */
    .bss :
    {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
    } > RAM
}
