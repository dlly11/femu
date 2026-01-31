/**
 * test_compare.S - Test comparison and test instructions
 *
 * Tests: CMP, CMN, TST, TEQ and all condition codes
 *
 * Memory layout at 0x20000000:
 *   [0x00] = CMP equal test (expect 1)
 *   [0x04] = CMP not equal test (expect 1)
 *   [0x08] = CMP unsigned higher (expect 1)
 *   [0x0C] = CMP unsigned lower/same (expect 1)
 *   [0x10] = CMP signed greater (expect 1)
 *   [0x14] = CMP signed less (expect 1)
 *   [0x18] = CMN test (expect 1)
 *   [0x1C] = TST zero result (expect 1)
 *   [0x20] = TST non-zero result (expect 1)
 *   [0x24] = TEQ equal test (expect 1)
 *   [0x28] = TEQ not equal test (expect 1)
 *   [0x2C] = Negative flag test (expect 1)
 *   [0x30] = Overflow flag test (expect 1)
 *   [0x34] = Carry flag test (expect 1)
 *   [0x38] = 0xC0FFEE42 (success marker)
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

    /* Test 1: CMP equal (EQ condition) */
    movs    r0, #0
    movs    r1, #10
    movs    r2, #10
    cmp     r1, r2
    it      eq
    moveq   r0, #1
    str     r0, [r7, #0x00]

    /* Test 2: CMP not equal (NE condition) */
    movs    r0, #0
    movs    r1, #10
    movs    r2, #20
    cmp     r1, r2
    it      ne
    movne   r0, #1
    str     r0, [r7, #0x04]

    /* Test 3: CMP unsigned higher (HI condition) - r1 > r2 unsigned */
    movs    r0, #0
    mov     r1, #0xFF          /* 255 */
    movs    r2, #10
    cmp     r1, r2
    it      hi
    movhi   r0, #1
    str     r0, [r7, #0x08]

    /* Test 4: CMP unsigned lower or same (LS condition) */
    movs    r0, #0
    movs    r1, #5
    movs    r2, #10
    cmp     r1, r2
    it      ls
    movls   r0, #1
    str     r0, [r7, #0x0C]

    /* Test 5: CMP signed greater (GT condition) */
    movs    r0, #0
    movs    r1, #10
    movs    r2, #5
    cmp     r1, r2
    it      gt
    movgt   r0, #1
    str     r0, [r7, #0x10]

    /* Test 6: CMP signed less (LT condition) - test with negative */
    movs    r0, #0
    mvn     r1, #0             /* r1 = -1 (0xFFFFFFFF) */
    movs    r2, #5
    cmp     r1, r2
    it      lt
    movlt   r0, #1
    str     r0, [r7, #0x14]

    /* Test 7: CMN (compare negative) - like CMP with negated second operand */
    /* CMN r1, r2 sets flags for r1 + r2 */
    movs    r0, #0
    movs    r1, #5
    mvn     r2, #4             /* r2 = -5 */
    cmn     r1, r2             /* 5 + (-5) = 0, sets Z flag */
    it      eq
    moveq   r0, #1
    str     r0, [r7, #0x18]

    /* Test 8: TST zero result (Z flag set) */
    movs    r0, #0
    movs    r1, #0x0F
    movs    r2, #0xF0
    tst     r1, r2             /* 0x0F & 0xF0 = 0 */
    it      eq
    moveq   r0, #1
    str     r0, [r7, #0x1C]

    /* Test 9: TST non-zero result (Z flag clear) */
    movs    r0, #0
    movs    r1, #0xFF
    movs    r2, #0x0F
    tst     r1, r2             /* 0xFF & 0x0F = 0x0F != 0 */
    it      ne
    movne   r0, #1
    str     r0, [r7, #0x20]

    /* Test 10: TEQ equal (XOR gives zero) */
    movs    r0, #0
    movs    r1, #42
    movs    r2, #42
    teq     r1, r2             /* 42 ^ 42 = 0 */
    it      eq
    moveq   r0, #1
    str     r0, [r7, #0x24]

    /* Test 11: TEQ not equal (XOR gives non-zero) */
    movs    r0, #0
    movs    r1, #42
    movs    r2, #24
    teq     r1, r2             /* 42 ^ 24 != 0 */
    it      ne
    movne   r0, #1
    str     r0, [r7, #0x28]

    /* Test 12: Negative flag (MI condition) */
    movs    r0, #0
    mvn     r1, #0             /* -1 */
    cmp     r1, #0             /* -1 < 0, sets N flag */
    it      mi
    movmi   r0, #1
    str     r0, [r7, #0x2C]

    /* Test 13: Overflow flag (VS condition) */
    /* 0x7FFFFFFF + 1 overflows to negative */
    movs    r0, #0
    ldr     r1, =0x7FFFFFFF
    movs    r2, #1
    adds    r3, r1, r2         /* Overflow! */
    it      vs
    movvs   r0, #1
    str     r0, [r7, #0x30]

    /* Test 14: Carry flag (CS condition) */
    /* 0xFFFFFFFF + 1 sets carry */
    movs    r0, #0
    mvn     r1, #0             /* 0xFFFFFFFF */
    movs    r2, #1
    adds    r3, r1, r2         /* Carry out */
    it      cs
    movcs   r0, #1
    str     r0, [r7, #0x34]

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x38]

    /* Done */
    bkpt    #0

    .thumb_func
_fault:
    b       _fault

    .end
