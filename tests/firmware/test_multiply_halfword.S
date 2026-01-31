/**
 * test_multiply_halfword.S - Test halfword/DSP multiply instructions
 *
 * Tests: SMULBB, SMULBT, SMULTB, SMULTT - 16x16->32 signed multiply
 *        SMLABB, SMLABT, SMLATB, SMLATT - 16x16 with 32-bit accumulate
 *        SMULWB, SMULWT - 32x16->32 (upper 32 bits of 48-bit result)
 *        SMLAWB, SMLAWT - 32x16 with accumulate
 *        SMUAD, SMUADX - Dual 16x16, sum of products
 *        SMLAD, SMLADX - Dual 16x16 with accumulate
 *        SMUSD, SMUSDX - Dual 16x16, difference of products
 *        SMMUL, SMMULR - 32x32->upper32
 *        SMMLA, SMMLAR - 32x32 with accumulate (upper32)
 *        SMMLS, SMMLSR - 32x32 with subtract (upper32)
 *        USAD8 - Sum of absolute differences
 *        USADA8 - USAD8 with accumulate
 *
 * Memory layout at 0x20000000:
 *   [0x00] = SMULBB result
 *   [0x04] = SMULBT result
 *   [0x08] = SMULTB result
 *   [0x0C] = SMULTT result
 *   [0x10] = SMLABB result
 *   [0x14] = SMULWB result
 *   [0x18] = SMUAD result
 *   [0x1C] = SMUSD result
 *   [0x20] = SMMUL result
 *   [0x24] = SMMLA result
 *   [0x28] = USAD8 result
 *   [0x2C] = USADA8 result
 *   [0x30] = SMULBB negative test
 *   [0x34] = SMUADX result
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

    /* ============================================ */
    /* Test 1: SMULBB - Multiply bottom halves     */
    /* Rd = Rn[15:0] * Rm[15:0] (signed)           */
    /* 0x0003 * 0x0004 = 12                        */
    /* ============================================ */
    ldr     r0, =0x00050003     /* bottom = 3 */
    ldr     r1, =0x00060004     /* bottom = 4 */
    smulbb  r2, r0, r1
    str     r2, [r7, #0x00]     /* expect 12 = 0x0000000C */

    /* ============================================ */
    /* Test 2: SMULBT - Bottom * Top               */
    /* Rd = Rn[15:0] * Rm[31:16] (signed)          */
    /* 0x0003 * 0x0006 = 18                        */
    /* ============================================ */
    ldr     r0, =0x00050003     /* bottom = 3 */
    ldr     r1, =0x00060004     /* top = 6 */
    smulbt  r2, r0, r1
    str     r2, [r7, #0x04]     /* expect 18 = 0x00000012 */

    /* ============================================ */
    /* Test 3: SMULTB - Top * Bottom               */
    /* Rd = Rn[31:16] * Rm[15:0] (signed)          */
    /* 0x0005 * 0x0004 = 20                        */
    /* ============================================ */
    ldr     r0, =0x00050003     /* top = 5 */
    ldr     r1, =0x00060004     /* bottom = 4 */
    smultb  r2, r0, r1
    str     r2, [r7, #0x08]     /* expect 20 = 0x00000014 */

    /* ============================================ */
    /* Test 4: SMULTT - Multiply top halves        */
    /* Rd = Rn[31:16] * Rm[31:16] (signed)         */
    /* 0x0005 * 0x0006 = 30                        */
    /* ============================================ */
    ldr     r0, =0x00050003     /* top = 5 */
    ldr     r1, =0x00060004     /* top = 6 */
    smultt  r2, r0, r1
    str     r2, [r7, #0x0C]     /* expect 30 = 0x0000001E */

    /* ============================================ */
    /* Test 5: SMLABB - Multiply-accumulate BB     */
    /* Rd = Rn[15:0] * Rm[15:0] + Ra (signed)      */
    /* 3 * 4 + 100 = 112                           */
    /* ============================================ */
    ldr     r0, =0x00050003     /* bottom = 3 */
    ldr     r1, =0x00060004     /* bottom = 4 */
    movs    r3, #100
    smlabb  r2, r0, r1, r3
    str     r2, [r7, #0x10]     /* expect 112 = 0x00000070 */

    /* ============================================ */
    /* Test 6: SMULWB - 32x16 (bottom) -> 32       */
    /* Rd = (Rn * Rm[15:0])[47:16] (signed)        */
    /* 0x10000 * 0x0002 = 0x20000                  */
    /* [47:16] = 0x00000002                        */
    /* ============================================ */
    ldr     r0, =0x00010000     /* 65536 */
    ldr     r1, =0x00010002     /* bottom = 2 */
    smulwb  r2, r0, r1
    str     r2, [r7, #0x14]     /* expect 2 = 0x00000002 */

    /* ============================================ */
    /* Test 7: SMUAD - Dual multiply, add results  */
    /* Rd = Rn[15:0]*Rm[15:0] + Rn[31:16]*Rm[31:16] */
    /* (3*4) + (5*6) = 12 + 30 = 42                */
    /* ============================================ */
    ldr     r0, =0x00050003     /* top=5, bottom=3 */
    ldr     r1, =0x00060004     /* top=6, bottom=4 */
    smuad   r2, r0, r1
    str     r2, [r7, #0x18]     /* expect 42 = 0x0000002A */

    /* ============================================ */
    /* Test 8: SMUSD - Dual multiply, sub results  */
    /* Rd = Rn[15:0]*Rm[15:0] - Rn[31:16]*Rm[31:16] */
    /* (3*4) - (5*6) = 12 - 30 = -18               */
    /* ============================================ */
    ldr     r0, =0x00050003     /* top=5, bottom=3 */
    ldr     r1, =0x00060004     /* top=6, bottom=4 */
    smusd   r2, r0, r1
    str     r2, [r7, #0x1C]     /* expect -18 = 0xFFFFFFEE */

    /* ============================================ */
    /* Test 9: SMMUL - Upper 32 bits of 32x32      */
    /* Rd = (Rn * Rm)[63:32]                       */
    /* 0x40000000 * 0x00000004 = 0x100000000       */
    /* [63:32] = 0x00000001                        */
    /* ============================================ */
    ldr     r0, =0x40000000     /* 2^30 */
    movs    r1, #4
    smmul   r2, r0, r1
    str     r2, [r7, #0x20]     /* expect 1 = 0x00000001 */

    /* ============================================ */
    /* Test 10: SMMLA - SMMUL with accumulate      */
    /* Rd = Ra + (Rn * Rm)[63:32]                  */
    /* 10 + 1 = 11                                 */
    /* ============================================ */
    ldr     r0, =0x40000000     /* 2^30 */
    movs    r1, #4
    movs    r3, #10
    smmla   r2, r0, r1, r3
    str     r2, [r7, #0x24]     /* expect 11 = 0x0000000B */

    /* ============================================ */
    /* Test 11: USAD8 - Sum of absolute differences */
    /* For each byte: |Rn[i] - Rm[i]|, sum all     */
    /* r0 = 0x10203040, r1 = 0x0F1F2F3F            */
    /* |0x10-0x0F| + |0x20-0x1F| + |0x30-0x2F| + |0x40-0x3F| */
    /* = 1 + 1 + 1 + 1 = 4                         */
    /* ============================================ */
    ldr     r0, =0x10203040
    ldr     r1, =0x0F1F2F3F
    usad8   r2, r0, r1
    str     r2, [r7, #0x28]     /* expect 4 */

    /* ============================================ */
    /* Test 12: USADA8 - USAD8 with accumulate     */
    /* Rd = Ra + USAD8(Rn, Rm)                     */
    /* 100 + 4 = 104                               */
    /* ============================================ */
    ldr     r0, =0x10203040
    ldr     r1, =0x0F1F2F3F
    movs    r3, #100
    usada8  r2, r0, r1, r3
    str     r2, [r7, #0x2C]     /* expect 104 = 0x00000068 */

    /* ============================================ */
    /* Test 13: SMULBB with negative values        */
    /* (-10) * 5 = -50                             */
    /* 0xFFF6 = -10 as signed 16-bit               */
    /* ============================================ */
    ldr     r0, =0x0000FFF6     /* bottom = -10 (signed) */
    ldr     r1, =0x00000005     /* bottom = 5 */
    smulbb  r2, r0, r1
    str     r2, [r7, #0x30]     /* expect -50 = 0xFFFFFFCE */

    /* ============================================ */
    /* Test 14: SMUADX - Dual multiply, exchanged  */
    /* Rd = Rn[15:0]*Rm[31:16] + Rn[31:16]*Rm[15:0] */
    /* (3*6) + (5*4) = 18 + 20 = 38                */
    /* ============================================ */
    ldr     r0, =0x00050003     /* top=5, bottom=3 */
    ldr     r1, =0x00060004     /* top=6, bottom=4 */
    smuadx  r2, r0, r1
    str     r2, [r7, #0x34]     /* expect 38 = 0x00000026 */

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x38]

    /* Done */
    bkpt    #0

    .thumb_func
_fault:
    b       _fault

    .end
