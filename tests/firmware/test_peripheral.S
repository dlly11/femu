/**
 * Peripheral MMIO test firmware.
 *
 * This program tests peripheral read/write through MMIO:
 * 1. Writes values to peripheral registers at 0x40000000
 * 2. Reads back from peripheral registers
 * 3. Stores results to RAM for verification
 *
 * Expected peripheral behavior:
 * - Offset 0x00: Read returns 0x12345678
 * - Offset 0x04: Read returns whatever was last written
 * - Offset 0x08: Write triggers "action" (tracked by peripheral)
 *
 * Results stored at:
 * - 0x20000000: Read from offset 0x00
 * - 0x20000004: Read from offset 0x04 after write
 * - 0x20000008: Done marker (0xC0FFEE42)
 */

    .syntax unified
    .cpu cortex-m4
    .thumb

    .section .vectors, "a"
    .align 2
    .global _vectors
_vectors:
    .word   _stack_top          /* Initial SP */
    .word   _reset + 1          /* Reset vector */
    .word   _nmi + 1
    .word   _hardfault + 1
    .word   _default + 1        /* MemManage */
    .word   _default + 1        /* BusFault */
    .word   _default + 1        /* UsageFault */
    .word   0, 0, 0, 0          /* Reserved */
    .word   _default + 1        /* SVCall */
    .word   _default + 1        /* Debug */
    .word   0                   /* Reserved */
    .word   _default + 1        /* PendSV */
    .word   _default + 1        /* SysTick */

    .section .text
    .align 2
    .thumb_func
    .global _reset
_reset:
    /* Set up pointers */
    ldr     r4, =0x40000000     /* Peripheral base */
    ldr     r5, =0x20000000     /* RAM result area */

    /* Test 1: Read from offset 0x00 (should return 0x12345678) */
    ldr     r0, [r4, #0]
    str     r0, [r5, #0]        /* Store result */

    /* Test 2: Write 0xAABBCCDD to offset 0x04, then read back */
    ldr     r1, =0xAABBCCDD
    str     r1, [r4, #4]
    ldr     r0, [r4, #4]
    str     r0, [r5, #4]        /* Store result */

    /* Test 3: Write to offset 0x08 (action trigger) */
    mov     r0, #0x42
    str     r0, [r4, #8]

    /* Store done marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r5, #8]

    /* Stop */
    bkpt    #0

halt:
    b       halt

    .thumb_func
_nmi:
    b       _nmi

    .thumb_func
_hardfault:
    b       _hardfault

    .thumb_func
_default:
    b       _default

    .end
