/* Linker script for FEMU TrustZone test firmware
 *
 * Memory Layout:
 *   SECURE:     0x08000000 (64K)  - Secure code and data
 *   NSC:        0x08010000 (4K)   - Non-Secure Callable region (SG entry points)
 *   NONSECURE:  0x08020000 (64K)  - Non-Secure code and data
 *   RAM_S:      0x20000000 (16K)  - Secure RAM
 *   RAM_NS:     0x20004000 (16K)  - Non-Secure RAM
 */

MEMORY
{
    SECURE    (rx)  : ORIGIN = 0x08000000, LENGTH = 64K
    NSC       (rx)  : ORIGIN = 0x08010000, LENGTH = 4K
    NONSECURE (rx)  : ORIGIN = 0x08020000, LENGTH = 64K
    RAM_S     (rwx) : ORIGIN = 0x20000000, LENGTH = 16K
    RAM_NS    (rwx) : ORIGIN = 0x20004000, LENGTH = 16K
}

/* Stack at top of Secure RAM */
_stack_top_s = ORIGIN(RAM_S) + LENGTH(RAM_S);

/* Stack for Non-Secure code */
_stack_top_ns = ORIGIN(RAM_NS) + LENGTH(RAM_NS);

SECTIONS
{
    /* Secure vector table at start of secure flash */
    .vectors_s :
    {
        KEEP(*(.vectors))
        KEEP(*(.vectors_s))
    } > SECURE

    /* Secure code */
    .text_s :
    {
        *(.text*)
        *(.text_s*)
        *(.rodata*)
        *(.rodata_s*)
    } > SECURE

    /* Non-Secure Callable region - contains SG instructions */
    .nsc :
    {
        . = ALIGN(32);
        _nsc_start = .;
        KEEP(*(.nsc*))
        KEEP(*(.veneer*))
        . = ALIGN(32);
        _nsc_end = .;
    } > NSC

    /* Non-Secure code */
    .text_ns :
    {
        _ns_start = .;
        *(.text_ns*)
        *(.rodata_ns*)
        _ns_end = .;
    } > NONSECURE

    /* Non-Secure vector table (for NS exceptions) */
    .vectors_ns :
    {
        KEEP(*(.vectors_ns))
    } > NONSECURE

    /* Secure test result area */
    .testresults_s (NOLOAD) :
    {
        _test_results_start = .;
        . += 0x100;                /* 256 bytes for test results at 0x20000000 */
        _test_results_end = .;
    } > RAM_S

    /* Secure data (initialized) */
    .data_s :
    {
        _data_s_start = .;
        *(.data*)
        *(.data_s*)
        _data_s_end = .;
    } > RAM_S AT > SECURE

    _data_s_load = LOADADDR(.data_s);

    /* Secure BSS */
    .bss_s :
    {
        _bss_s_start = .;
        *(.bss*)
        *(.bss_s*)
        *(COMMON)
        _bss_s_end = .;
    } > RAM_S

    /* Non-Secure test result area */
    .testresults_ns (NOLOAD) :
    {
        _test_results_ns_start = .;
        . += 0x100;
        _test_results_ns_end = .;
    } > RAM_NS

    /* Non-Secure data */
    .data_ns :
    {
        _data_ns_start = .;
        *(.data_ns*)
        _data_ns_end = .;
    } > RAM_NS AT > NONSECURE

    _data_ns_load = LOADADDR(.data_ns);

    /* Non-Secure BSS */
    .bss_ns :
    {
        _bss_ns_start = .;
        *(.bss_ns*)
        _bss_ns_end = .;
    } > RAM_NS
}

/* Symbols for region addresses (used by SAU configuration) */
_secure_flash_start = ORIGIN(SECURE);
_secure_flash_end = ORIGIN(SECURE) + LENGTH(SECURE);
_nsc_flash_start = ORIGIN(NSC);
_nsc_flash_end = ORIGIN(NSC) + LENGTH(NSC);
_nonsecure_flash_start = ORIGIN(NONSECURE);
_nonsecure_flash_end = ORIGIN(NONSECURE) + LENGTH(NONSECURE);
_secure_ram_start = ORIGIN(RAM_S);
_secure_ram_end = ORIGIN(RAM_S) + LENGTH(RAM_S);
_nonsecure_ram_start = ORIGIN(RAM_NS);
_nonsecure_ram_end = ORIGIN(RAM_NS) + LENGTH(RAM_NS);
