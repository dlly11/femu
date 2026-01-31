/**
 * test_fpu.S - Test FPU (VFP) instructions
 *
 * Tests: VLDR, VSTR, VMOV, VADD, VSUB, VMUL, VDIV, VCMP, VCVT
 *
 * Memory layout at 0x20000000:
 *   [0x00] = VLDR.F32 result (as int)
 *   [0x04] = VADD.F32 result (1.5 + 2.5 = 4.0)
 *   [0x08] = VSUB.F32 result (5.0 - 2.5 = 2.5)
 *   [0x0C] = VMUL.F32 result (2.0 * 3.0 = 6.0)
 *   [0x10] = VDIV.F32 result (8.0 / 2.0 = 4.0)
 *   [0x14] = VCMP result (FPSCR flags: Z=1 if equal)
 *   [0x18] = VCVT int to float result (42 -> 42.0)
 *   [0x1C] = VCVT float to int result (3.7 -> 3)
 *   [0x20] = VMOV ARM to FPU and back
 *   [0x24] = VABS result (|-3.5| = 3.5)
 *   [0x28] = VNEG result (-2.5 = -2.5)
 *   [0x2C] = VSQRT result (sqrt(16.0) = 4.0)
 *   [0x30] = VCMP less than (N flag)
 *   [0x34] = VCMP greater than (C flag)
 *   [0x38] = 0xC0FFEE42 (success marker)
 */

    .syntax unified
    .cpu cortex-m4
    .fpu fpv4-sp-d16
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
    /* Test 1: VLDR.F32 - Load single precision    */
    /* Load 1.5 (0x3FC00000) from memory           */
    /* ============================================ */
    adr     r0, float_1_5
    vldr    s0, [r0]
    vstr    s0, [r7, #0x00]     /* store back to verify */

    /* ============================================ */
    /* Test 2: VADD.F32 - Add single precision     */
    /* 1.5 + 2.5 = 4.0                             */
    /* ============================================ */
    adr     r0, float_1_5
    vldr    s0, [r0]
    adr     r0, float_2_5
    vldr    s1, [r0]
    vadd.f32 s2, s0, s1
    vstr    s2, [r7, #0x04]     /* expect 4.0 = 0x40800000 */

    /* ============================================ */
    /* Test 3: VSUB.F32 - Subtract single precision */
    /* 5.0 - 2.5 = 2.5                             */
    /* ============================================ */
    adr     r0, float_5_0
    vldr    s0, [r0]
    adr     r0, float_2_5
    vldr    s1, [r0]
    vsub.f32 s2, s0, s1
    vstr    s2, [r7, #0x08]     /* expect 2.5 = 0x40200000 */

    /* ============================================ */
    /* Test 4: VMUL.F32 - Multiply single precision */
    /* 2.0 * 3.0 = 6.0                             */
    /* ============================================ */
    adr     r0, float_2_0
    vldr    s0, [r0]
    adr     r0, float_3_0
    vldr    s1, [r0]
    vmul.f32 s2, s0, s1
    vstr    s2, [r7, #0x0C]     /* expect 6.0 = 0x40C00000 */

    /* ============================================ */
    /* Test 5: VDIV.F32 - Divide single precision  */
    /* 8.0 / 2.0 = 4.0                             */
    /* ============================================ */
    adr     r0, float_8_0
    vldr    s0, [r0]
    adr     r0, float_2_0
    vldr    s1, [r0]
    vdiv.f32 s2, s0, s1
    vstr    s2, [r7, #0x10]     /* expect 4.0 = 0x40800000 */

    /* ============================================ */
    /* Test 6: VCMP.F32 - Compare (equal)          */
    /* Compare 2.5 with 2.5 -> should set Z flag   */
    /* ============================================ */
    adr     r0, float_2_5
    vldr    s0, [r0]
    vldr    s1, [r0]
    vcmp.f32 s0, s1
    vmrs    apsr_nzcv, fpscr    /* transfer FPSCR flags to APSR */
    mov     r0, #0
    it      eq
    moveq   r0, #1              /* Set 1 if equal (Z=1) */
    str     r0, [r7, #0x14]     /* expect 1 */

    /* ============================================ */
    /* Test 7: VCVT int to float                   */
    /* Convert 42 (integer) to 42.0 (float)        */
    /* ============================================ */
    movs    r0, #42
    vmov    s0, r0              /* Move int to FPU register */
    vcvt.f32.s32 s1, s0        /* Convert signed int to float */
    vstr    s1, [r7, #0x18]     /* expect 42.0 = 0x42280000 */

    /* ============================================ */
    /* Test 8: VCVT float to int                   */
    /* Convert 3.7 to 3 (truncate toward zero)     */
    /* ============================================ */
    adr     r0, float_3_7
    vldr    s0, [r0]
    vcvt.s32.f32 s1, s0        /* Convert to signed int (truncate) */
    vmov    r0, s1              /* Move result to ARM register */
    str     r0, [r7, #0x1C]     /* expect 3 */

    /* ============================================ */
    /* Test 9: VMOV ARM to FPU and back            */
    /* Round-trip a value through FPU register     */
    /* ============================================ */
    ldr     r0, =0xDEADBEEF
    vmov    s0, r0              /* ARM to FPU */
    vmov    r1, s0              /* FPU to ARM */
    str     r1, [r7, #0x20]     /* expect 0xDEADBEEF */

    /* ============================================ */
    /* Test 10: VABS.F32 - Absolute value          */
    /* |−3.5| = 3.5                                */
    /* ============================================ */
    adr     r0, float_neg_3_5
    vldr    s0, [r0]
    vabs.f32 s1, s0
    vstr    s1, [r7, #0x24]     /* expect 3.5 = 0x40600000 */

    /* ============================================ */
    /* Test 11: VNEG.F32 - Negate                  */
    /* −(2.5) = −2.5                               */
    /* ============================================ */
    adr     r0, float_2_5
    vldr    s0, [r0]
    vneg.f32 s1, s0
    vstr    s1, [r7, #0x28]     /* expect -2.5 = 0xC0200000 */

    /* ============================================ */
    /* Test 12: VSQRT.F32 - Square root            */
    /* sqrt(16.0) = 4.0                            */
    /* ============================================ */
    adr     r0, float_16_0
    vldr    s0, [r0]
    vsqrt.f32 s1, s0
    vstr    s1, [r7, #0x2C]     /* expect 4.0 = 0x40800000 */

    /* ============================================ */
    /* Test 13: VCMP less than                     */
    /* 1.5 < 2.5 -> N flag set                     */
    /* ============================================ */
    adr     r0, float_1_5
    vldr    s0, [r0]
    adr     r0, float_2_5
    vldr    s1, [r0]
    vcmp.f32 s0, s1
    vmrs    apsr_nzcv, fpscr
    mov     r0, #0
    it      lt
    movlt   r0, #1              /* Set 1 if less than (N=1) */
    str     r0, [r7, #0x30]     /* expect 1 */

    /* ============================================ */
    /* Test 14: VCMP greater than                  */
    /* 5.0 > 2.5 -> C flag set                     */
    /* ============================================ */
    adr     r0, float_5_0
    vldr    s0, [r0]
    adr     r0, float_2_5
    vldr    s1, [r0]
    vcmp.f32 s0, s1
    vmrs    apsr_nzcv, fpscr
    mov     r0, #0
    it      gt
    movgt   r0, #1              /* Set 1 if greater than */
    str     r0, [r7, #0x34]     /* expect 1 */

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x38]

    /* Done */
    bkpt    #0

    .thumb_func
_fault:
    b       _fault

    /* ============================================ */
    /* Float constants (IEEE 754 single precision) */
    /* ============================================ */
    .align 4
float_1_5:
    .word   0x3FC00000          /* 1.5 */
float_2_0:
    .word   0x40000000          /* 2.0 */
float_2_5:
    .word   0x40200000          /* 2.5 */
float_3_0:
    .word   0x40400000          /* 3.0 */
float_3_7:
    .word   0x406CCCCD          /* 3.7 */
float_5_0:
    .word   0x40A00000          /* 5.0 */
float_8_0:
    .word   0x41000000          /* 8.0 */
float_16_0:
    .word   0x41800000          /* 16.0 */
float_neg_3_5:
    .word   0xC0600000          /* -3.5 */

    .end
