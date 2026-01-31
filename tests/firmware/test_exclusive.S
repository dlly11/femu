/**
 * test_exclusive.S - Test exclusive access instructions (atomics)
 *
 * Tests: LDREX, STREX, LDREXB, STREXB, LDREXH, STREXH, CLREX
 *
 * Memory layout at 0x20000000:
 *   [0x00] = LDREX result (expect initial value)
 *   [0x04] = STREX success (expect 0 for success)
 *   [0x08] = Value after STREX (expect new value)
 *   [0x0C] = STREX after CLREX (expect 1 for failure)
 *   [0x10] = LDREXB result (expect byte value)
 *   [0x14] = STREXB success (expect 0)
 *   [0x18] = LDREXH result (expect halfword value)
 *   [0x1C] = STREXH success (expect 0)
 *   [0x20] = Atomic increment result
 *   [0x24] = 0xC0FFEE42 (success marker)
 *
 * Test data at 0x20000100
 */

    .syntax unified
    .cpu cortex-m4
    .thumb

    .section .vectors, "a"
    .word   0x20001000          /* Initial SP */
    .word   _reset + 1          /* Reset vector */
    .word   _fault + 1          /* NMI */
    .word   _fault + 1          /* HardFault */

    .section .text
    .thumb_func
    .global _reset
_reset:
    /* Setup base pointer for results */
    mov     r7, #0x20000000

    /* Setup test data area */
    ldr     r6, =0x20000100

    /* Initialize test data */
    ldr     r0, =0xDEADBEEF
    str     r0, [r6, #0]        /* Word for LDREX/STREX */
    movs    r0, #0x42
    strb    r0, [r6, #4]        /* Byte for LDREXB/STREXB */
    mov     r0, #0x1234
    strh    r0, [r6, #8]        /* Halfword for LDREXH/STREXH */
    movs    r0, #10
    str     r0, [r6, #12]       /* Counter for atomic increment */

    /* Test 1: LDREX - Load exclusive word */
    ldrex   r0, [r6]
    str     r0, [r7, #0x00]     /* expect 0xDEADBEEF */

    /* Test 2: STREX - Store exclusive word (should succeed after LDREX) */
    ldr     r1, =0xCAFEBABE
    strex   r0, r1, [r6]        /* r0 = 0 if success, 1 if fail */
    str     r0, [r7, #0x04]     /* expect 0 (success) */

    /* Test 3: Verify value was written */
    ldr     r0, [r6]
    str     r0, [r7, #0x08]     /* expect 0xCAFEBABE */

    /* Test 4: CLREX clears exclusive monitor, next STREX should fail */
    ldrex   r0, [r6]            /* Establish exclusive access */
    clrex                       /* Clear exclusive monitor */
    ldr     r1, =0x11111111
    strex   r0, r1, [r6]        /* Should fail (r0 = 1) */
    str     r0, [r7, #0x0C]     /* expect 1 (failure) */

    /* Test 5: LDREXB - Load exclusive byte */
    add     r4, r6, #4          /* Point to byte location */
    ldrexb  r0, [r4]
    str     r0, [r7, #0x10]     /* expect 0x42 */

    /* Test 6: STREXB - Store exclusive byte */
    movs    r1, #0x55
    strexb  r0, r1, [r4]
    str     r0, [r7, #0x14]     /* expect 0 (success) */

    /* Test 7: LDREXH - Load exclusive halfword */
    add     r4, r6, #8          /* Point to halfword location */
    ldrexh  r0, [r4]
    str     r0, [r7, #0x18]     /* expect 0x1234 */

    /* Test 8: STREXH - Store exclusive halfword */
    mov     r1, #0x5678
    strexh  r0, r1, [r4]
    str     r0, [r7, #0x1C]     /* expect 0 (success) */

    /* Test 9: Atomic increment pattern
     * Implements: atomic { counter++ }
     */
    add     r4, r6, #12         /* Point to counter */
atomic_inc:
    ldrex   r0, [r4]            /* Load exclusive */
    adds    r0, r0, #1          /* Increment */
    strex   r1, r0, [r4]        /* Store exclusive */
    cmp     r1, #0              /* Check if succeeded */
    bne     atomic_inc          /* Retry if failed */
    /* Counter is now 11 */
    ldr     r0, [r4]
    str     r0, [r7, #0x20]     /* expect 11 */

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x24]

    /* Done */
    bkpt    #0

    .thumb_func
_fault:
    b       _fault

    .end
