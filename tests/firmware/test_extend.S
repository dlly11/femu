/**
 * test_extend.S - Test sign/zero extension instructions
 *
 * Tests: SXTB, SXTH, UXTB, UXTH, SXTAB, SXTAH, UXTAB, UXTAH
 *        with rotation variants
 *
 * Memory layout at 0x20000000:
 *   [0x00] = SXTB positive (expect 0x0000007F)
 *   [0x04] = SXTB negative (expect 0xFFFFFF80)
 *   [0x08] = SXTH positive (expect 0x00007FFF)
 *   [0x0C] = SXTH negative (expect 0xFFFF8000)
 *   [0x10] = UXTB (expect 0x000000AB)
 *   [0x14] = UXTH (expect 0x0000ABCD)
 *   [0x18] = SXTB with ROR 8 (expect 0xFFFFFFCD)
 *   [0x1C] = UXTB with ROR 16 (expect 0x000000AB)
 *   [0x20] = SXTAB (expect 0x1000007F)
 *   [0x24] = SXTAH (expect 0x10007FFF)
 *   [0x28] = UXTAB (expect 0x100000FF)
 *   [0x2C] = UXTAH (expect 0x1000FFFF)
 *   [0x30] = SXTAB with ROR 8 (expect result)
 *   [0x34] = 0xC0FFEE42 (success marker)
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

    /* Test 1: SXTB positive byte (0x7F -> 0x0000007F) */
    movs    r1, #0x7F
    sxtb    r0, r1
    str     r0, [r7, #0x00]

    /* Test 2: SXTB negative byte (0x80 -> 0xFFFFFF80) */
    movs    r1, #0x80
    sxtb    r0, r1
    str     r0, [r7, #0x04]

    /* Test 3: SXTH positive halfword (0x7FFF -> 0x00007FFF) */
    mov     r1, #0x7FFF
    sxth    r0, r1
    str     r0, [r7, #0x08]

    /* Test 4: SXTH negative halfword (0x8000 -> 0xFFFF8000) */
    mov     r1, #0x8000
    sxth    r0, r1
    str     r0, [r7, #0x0C]

    /* Test 5: UXTB (0xABCD -> 0x000000CD) */
    ldr     r1, =0x12345678
    movs    r1, #0xAB
    uxtb    r0, r1
    str     r0, [r7, #0x10]

    /* Test 6: UXTH (0x1234ABCD -> 0x0000ABCD) */
    ldr     r1, =0x1234ABCD
    uxth    r0, r1
    str     r0, [r7, #0x14]

    /* Test 7: SXTB with ROR 8
     * Input: 0xAABBCCDD, ROR 8 -> 0xDDAABBCC, extract byte -> 0xCC (negative)
     * Result: 0xFFFFFFCC? Actually let's recalc:
     * ROR 8 of 0xAABBCCDD = 0xDDAABBCC
     * SXTB extracts low byte 0xCC = -52 -> 0xFFFFFFCC
     * But wait, SXTB Rd, Rm, ROR #n rotates Rm then extracts
     */
    ldr     r1, =0xAABBCCDD
    sxtb    r0, r1, ror #8     /* Rotate right 8, then sign-extend byte */
    str     r0, [r7, #0x18]    /* 0xDDAABBCC -> extract 0xCC -> 0xFFFFFFCC */

    /* Test 8: UXTB with ROR 16
     * Input: 0x12AB34CD, ROR 16 -> 0x34CD12AB, extract byte -> 0xAB
     */
    ldr     r1, =0x12AB34CD
    uxtb    r0, r1, ror #16
    str     r0, [r7, #0x1C]    /* expect 0x000000AB */

    /* Test 9: SXTAB - add sign-extended byte to base
     * SXTAB Rd, Rn, Rm: Rd = Rn + SignExtend(Rm[7:0])
     * 0x10000000 + SignExtend(0x7F) = 0x10000000 + 0x7F = 0x1000007F
     */
    ldr     r1, =0x10000000
    movs    r2, #0x7F
    sxtab   r0, r1, r2
    str     r0, [r7, #0x20]

    /* Test 10: SXTAH - add sign-extended halfword to base
     * 0x10000000 + SignExtend(0x7FFF) = 0x10007FFF
     */
    ldr     r1, =0x10000000
    mov     r2, #0x7FFF
    sxtah   r0, r1, r2
    str     r0, [r7, #0x24]

    /* Test 11: UXTAB - add zero-extended byte to base
     * 0x10000000 + ZeroExtend(0xFF) = 0x100000FF
     */
    ldr     r1, =0x10000000
    movs    r2, #0xFF
    uxtab   r0, r1, r2
    str     r0, [r7, #0x28]

    /* Test 12: UXTAH - add zero-extended halfword to base
     * 0x10000000 + ZeroExtend(0xFFFF) = 0x1000FFFF
     */
    ldr     r1, =0x10000000
    ldr     r2, =0xFFFF
    uxtah   r0, r1, r2
    str     r0, [r7, #0x2C]

    /* Test 13: SXTAB with rotation
     * Base: 0x10000000
     * Value: 0x11223380, ROR 8 -> 0x80112233, extract 0x33 (positive)
     * Result: 0x10000000 + 0x33 = 0x10000033
     */
    ldr     r1, =0x10000000
    ldr     r2, =0x11223380
    sxtab   r0, r1, r2, ror #8
    str     r0, [r7, #0x30]    /* expect 0x10000033 */

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x34]

    /* Done */
    bkpt    #0

    .thumb_func
_fault:
    b       _fault

    .end
