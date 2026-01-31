/**
 * test_dsp.S - Test DSP parallel add/subtract instructions
 *
 * Tests: SADD16, SADD8, SSUB16, SSUB8, UADD16, UADD8, USUB16, USUB8
 *        SHADD16, SHADD8, UHADD16, UHADD8, SHSUB16, SHSUB8, UHSUB16, UHSUB8
 *        UQADD16, UQADD8, UQSUB16, UQSUB8, QADD16, QADD8, QSUB16, QSUB8
 *
 * Memory layout at 0x20000000:
 *   [0x00] = SADD16 result
 *   [0x04] = SADD8 result
 *   [0x08] = SSUB16 result
 *   [0x0C] = SSUB8 result
 *   [0x10] = UADD16 result
 *   [0x14] = UADD8 result
 *   [0x18] = USUB16 result
 *   [0x1C] = USUB8 result
 *   [0x20] = SHADD16 result (halving)
 *   [0x24] = SHADD8 result (halving)
 *   [0x28] = UHADD16 result (halving)
 *   [0x2C] = UHADD8 result (halving)
 *   [0x30] = QADD16 result (saturating)
 *   [0x34] = QADD8 result (saturating)
 *   [0x38] = QSUB16 result (saturating)
 *   [0x3C] = QSUB8 result (saturating)
 *   [0x40] = UQADD8 saturation test
 *   [0x44] = UQSUB8 saturation test
 *   [0x48] = SADD16 with negative
 *   [0x4C] = 0xC0FFEE42 (success marker)
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
    /* Test 1: SADD16 - Signed parallel halfword add */
    /* Adds halfwords: (0x0010 + 0x0020) and (0x0030 + 0x0040) */
    /* r0 = 0x00300010, r1 = 0x00400020 */
    /* Result: 0x00700030 */
    /* ============================================ */
    ldr     r0, =0x00300010     /* hi=0x0030, lo=0x0010 */
    ldr     r1, =0x00400020     /* hi=0x0040, lo=0x0020 */
    sadd16  r2, r0, r1
    str     r2, [r7, #0x00]     /* expect 0x00700030 */

    /* ============================================ */
    /* Test 2: SADD8 - Signed parallel byte add */
    /* Adds bytes: 0x10+0x01, 0x20+0x02, 0x30+0x03, 0x40+0x04 */
    /* r0 = 0x40302010, r1 = 0x04030201 */
    /* Result: 0x44332211 */
    /* ============================================ */
    ldr     r0, =0x40302010
    ldr     r1, =0x04030201
    sadd8   r2, r0, r1
    str     r2, [r7, #0x04]     /* expect 0x44332211 */

    /* ============================================ */
    /* Test 3: SSUB16 - Signed parallel halfword sub */
    /* Subs halfwords: (0x0050 - 0x0010) and (0x0040 - 0x0020) */
    /* r0 = 0x00400050, r1 = 0x00200010 */
    /* Result: 0x00200040 */
    /* ============================================ */
    ldr     r0, =0x00400050
    ldr     r1, =0x00200010
    ssub16  r2, r0, r1
    str     r2, [r7, #0x08]     /* expect 0x00200040 */

    /* ============================================ */
    /* Test 4: SSUB8 - Signed parallel byte sub */
    /* Subs bytes: 0x50-0x10, 0x40-0x10, 0x30-0x10, 0x20-0x10 */
    /* r0 = 0x20304050, r1 = 0x10101010 */
    /* Result: 0x10203040 */
    /* ============================================ */
    ldr     r0, =0x20304050
    ldr     r1, =0x10101010
    ssub8   r2, r0, r1
    str     r2, [r7, #0x0C]     /* expect 0x10203040 */

    /* ============================================ */
    /* Test 5: UADD16 - Unsigned parallel halfword add */
    /* Same operands as SADD16 for positive values */
    /* ============================================ */
    ldr     r0, =0x00300010
    ldr     r1, =0x00400020
    uadd16  r2, r0, r1
    str     r2, [r7, #0x10]     /* expect 0x00700030 */

    /* ============================================ */
    /* Test 6: UADD8 - Unsigned parallel byte add */
    /* Same operands as SADD8 */
    /* ============================================ */
    ldr     r0, =0x40302010
    ldr     r1, =0x04030201
    uadd8   r2, r0, r1
    str     r2, [r7, #0x14]     /* expect 0x44332211 */

    /* ============================================ */
    /* Test 7: USUB16 - Unsigned parallel halfword sub */
    /* ============================================ */
    ldr     r0, =0x00400050
    ldr     r1, =0x00200010
    usub16  r2, r0, r1
    str     r2, [r7, #0x18]     /* expect 0x00200040 */

    /* ============================================ */
    /* Test 8: USUB8 - Unsigned parallel byte sub */
    /* ============================================ */
    ldr     r0, =0x20304050
    ldr     r1, =0x10101010
    usub8   r2, r0, r1
    str     r2, [r7, #0x1C]     /* expect 0x10203040 */

    /* ============================================ */
    /* Test 9: SHADD16 - Signed halving halfword add */
    /* Result is (a+b)/2 for each halfword */
    /* (0x0020 + 0x0010)/2 = 0x0018, (0x0040 + 0x0030)/2 = 0x0038 */
    /* ============================================ */
    ldr     r0, =0x00400020     /* hi=0x0040, lo=0x0020 */
    ldr     r1, =0x00300010     /* hi=0x0030, lo=0x0010 */
    shadd16 r2, r0, r1
    str     r2, [r7, #0x20]     /* expect 0x00380018 */

    /* ============================================ */
    /* Test 10: SHADD8 - Signed halving byte add */
    /* Result is (a+b)/2 for each byte (SIGNED)   */
    /* (0x20+0x10)/2=0x18, (0x40+0x20)/2=0x30, (0x60+0x30)/2=0x48 */
    /* For MSB: 0x80=-128 signed, 0x40=64, (-128+64)/2=-32=0xE0 */
    /* ============================================ */
    ldr     r0, =0x80604020
    ldr     r1, =0x40302010
    shadd8  r2, r0, r1
    str     r2, [r7, #0x24]     /* expect 0xE0483018 (signed halving) */

    /* ============================================ */
    /* Test 11: UHADD16 - Unsigned halving halfword add */
    /* ============================================ */
    ldr     r0, =0x00400020
    ldr     r1, =0x00300010
    uhadd16 r2, r0, r1
    str     r2, [r7, #0x28]     /* expect 0x00380018 */

    /* ============================================ */
    /* Test 12: UHADD8 - Unsigned halving byte add */
    /* ============================================ */
    ldr     r0, =0x80604020
    ldr     r1, =0x40302010
    uhadd8  r2, r0, r1
    str     r2, [r7, #0x2C]     /* expect 0x60483018 */

    /* ============================================ */
    /* Test 13: QADD16 - Saturating parallel halfword add */
    /* Test with values that don't saturate */
    /* ============================================ */
    ldr     r0, =0x00100020
    ldr     r1, =0x00100010
    qadd16  r2, r0, r1
    str     r2, [r7, #0x30]     /* expect 0x00200030 */

    /* ============================================ */
    /* Test 14: QADD8 - Saturating parallel byte add */
    /* Test with values that don't saturate */
    /* ============================================ */
    ldr     r0, =0x10203040
    ldr     r1, =0x01020304
    qadd8   r2, r0, r1
    str     r2, [r7, #0x34]     /* expect 0x11223344 */

    /* ============================================ */
    /* Test 15: QSUB16 - Saturating parallel halfword sub */
    /* ============================================ */
    ldr     r0, =0x00500040
    ldr     r1, =0x00200010
    qsub16  r2, r0, r1
    str     r2, [r7, #0x38]     /* expect 0x00300030 */

    /* ============================================ */
    /* Test 16: QSUB8 - Saturating parallel byte sub */
    /* ============================================ */
    ldr     r0, =0x50403020
    ldr     r1, =0x10101010
    qsub8   r2, r0, r1
    str     r2, [r7, #0x3C]     /* expect 0x40302010 */

    /* ============================================ */
    /* Test 17: UQADD8 - Unsigned saturating add (overflow) */
    /* 0xFF + 0x10 should saturate to 0xFF */
    /* ============================================ */
    ldr     r0, =0x000000FF
    ldr     r1, =0x00000010
    uqadd8  r2, r0, r1
    str     r2, [r7, #0x40]     /* expect 0x000000FF (saturated) */

    /* ============================================ */
    /* Test 18: UQSUB8 - Unsigned saturating sub (underflow) */
    /* 0x10 - 0xFF should saturate to 0x00 */
    /* ============================================ */
    ldr     r0, =0x00000010
    ldr     r1, =0x000000FF
    uqsub8  r2, r0, r1
    str     r2, [r7, #0x44]     /* expect 0x00000000 (saturated) */

    /* ============================================ */
    /* Test 19: SADD16 with negative values */
    /* (0x0010 + 0xFFF0) = 0x0000 (16+(-16)=0) */
    /* (0x7FFF + 0x0001) = 0x8000 (overflow to negative) */
    /* ============================================ */
    ldr     r0, =0x7FFF0010     /* hi=0x7FFF, lo=0x0010 */
    ldr     r1, =0x0001FFF0     /* hi=0x0001, lo=0xFFF0 (-16) */
    sadd16  r2, r0, r1
    str     r2, [r7, #0x48]     /* expect 0x80000000 */

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x4C]

    /* Done */
    bkpt    #0

    .thumb_func
_fault:
    b       _fault

    .end
