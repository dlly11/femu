/**
 * test_fpu_mac.S - Test FPU multiply-accumulate instructions
 *
 * Tests: VMLA, VMLS, VNMLA, VNMLS, VNMUL, VFMA, VFMS, VFNMA, VFNMS
 *
 * Memory layout at 0x20000000:
 *   [0x00] = VMLA result (2.0*3.0 + 1.0 = 7.0)
 *   [0x04] = VMLS result (10.0 - 2.0*3.0 = 4.0)
 *   [0x08] = VNMLA result (-(1.0 + 2.0*3.0) = -7.0)
 *   [0x0C] = VNMLS result (-(1.0 - 2.0*3.0) = 5.0)
 *   [0x10] = VNMUL result (-(2.0*3.0) = -6.0)
 *   [0x14] = VFMA result (2.0*3.0 + 1.0 = 7.0)
 *   [0x18] = VFMS result (1.0 - 2.0*3.0 = -5.0)
 *   [0x1C] = VFNMA result (-(2.0*3.0 + 1.0) = -7.0)
 *   [0x20] = VFNMS result (-(2.0*3.0 - 1.0) = -5.0)
 *   [0x24] = VMLA chained (1.0 + 2.0*3.0 + 4.0*5.0 = 27.0)
 *   [0x28] = VMLA with negatives (-1.0 + 2.0*3.0 = 5.0)
 *   [0x2C] = 0xC0FFEE42 (success marker)
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
    /* Test 1: VMLA.F32 - Multiply accumulate      */
    /* Sd = Sd + (Sn * Sm)                         */
    /* 1.0 + (2.0 * 3.0) = 7.0                     */
    /* ============================================ */
    adr     r0, float_1_0
    vldr    s0, [r0]            /* s0 = 1.0 (accumulator) */
    adr     r0, float_2_0
    vldr    s1, [r0]            /* s1 = 2.0 */
    adr     r0, float_3_0
    vldr    s2, [r0]            /* s2 = 3.0 */
    vmla.f32 s0, s1, s2         /* s0 = s0 + s1*s2 = 1.0 + 6.0 = 7.0 */
    vstr    s0, [r7, #0x00]     /* expect 7.0 = 0x40E00000 */

    /* ============================================ */
    /* Test 2: VMLS.F32 - Multiply subtract        */
    /* Sd = Sd - (Sn * Sm)                         */
    /* 10.0 - (2.0 * 3.0) = 4.0                    */
    /* ============================================ */
    adr     r0, float_10_0
    vldr    s0, [r0]            /* s0 = 10.0 (accumulator) */
    adr     r0, float_2_0
    vldr    s1, [r0]            /* s1 = 2.0 */
    adr     r0, float_3_0
    vldr    s2, [r0]            /* s2 = 3.0 */
    vmls.f32 s0, s1, s2         /* s0 = s0 - s1*s2 = 10.0 - 6.0 = 4.0 */
    vstr    s0, [r7, #0x04]     /* expect 4.0 = 0x40800000 */

    /* ============================================ */
    /* Test 3: VNMLA.F32 - Negated multiply accum  */
    /* Sd = -(Sd + Sn * Sm)                        */
    /* -(1.0 + 2.0*3.0) = -7.0                     */
    /* ============================================ */
    adr     r0, float_1_0
    vldr    s0, [r0]            /* s0 = 1.0 */
    adr     r0, float_2_0
    vldr    s1, [r0]            /* s1 = 2.0 */
    adr     r0, float_3_0
    vldr    s2, [r0]            /* s2 = 3.0 */
    vnmla.f32 s0, s1, s2        /* s0 = -(s0 + s1*s2) = -(1.0 + 6.0) = -7.0 */
    vstr    s0, [r7, #0x08]     /* expect -7.0 = 0xC0E00000 */

    /* ============================================ */
    /* Test 4: VNMLS.F32 - Negated multiply sub    */
    /* Sd = -(Sd - Sn * Sm) = Sn*Sm - Sd           */
    /* 2.0*3.0 - 1.0 = 5.0                         */
    /* ============================================ */
    adr     r0, float_1_0
    vldr    s0, [r0]            /* s0 = 1.0 */
    adr     r0, float_2_0
    vldr    s1, [r0]            /* s1 = 2.0 */
    adr     r0, float_3_0
    vldr    s2, [r0]            /* s2 = 3.0 */
    vnmls.f32 s0, s1, s2        /* s0 = -(s0 - s1*s2) = -(1.0 - 6.0) = 5.0 */
    vstr    s0, [r7, #0x0C]     /* expect 5.0 = 0x40A00000 */

    /* ============================================ */
    /* Test 5: VNMUL.F32 - Negated multiply        */
    /* Sd = -(Sn * Sm)                             */
    /* -(2.0 * 3.0) = -6.0                         */
    /* ============================================ */
    adr     r0, float_2_0
    vldr    s1, [r0]            /* s1 = 2.0 */
    adr     r0, float_3_0
    vldr    s2, [r0]            /* s2 = 3.0 */
    vnmul.f32 s0, s1, s2        /* s0 = -(s1*s2) = -6.0 */
    vstr    s0, [r7, #0x10]     /* expect -6.0 = 0xC0C00000 */

    /* ============================================ */
    /* Test 6: VFMA.F32 - Fused multiply-add       */
    /* Sd = Sd + (Sn * Sm)                         */
    /* 1.0 + (2.0 * 3.0) = 7.0                     */
    /* ============================================ */
    adr     r0, float_1_0
    vldr    s0, [r0]            /* s0 = 1.0 */
    adr     r0, float_2_0
    vldr    s1, [r0]            /* s1 = 2.0 */
    adr     r0, float_3_0
    vldr    s2, [r0]            /* s2 = 3.0 */
    vfma.f32 s0, s1, s2         /* s0 = s0 + s1*s2 = 7.0 */
    vstr    s0, [r7, #0x14]     /* expect 7.0 = 0x40E00000 */

    /* ============================================ */
    /* Test 7: VFMS.F32 - Fused multiply-subtract  */
    /* Sd = Sd - (Sn * Sm)                         */
    /* 1.0 - (2.0 * 3.0) = -5.0                    */
    /* ============================================ */
    adr     r0, float_1_0
    vldr    s0, [r0]            /* s0 = 1.0 */
    adr     r0, float_2_0
    vldr    s1, [r0]            /* s1 = 2.0 */
    adr     r0, float_3_0
    vldr    s2, [r0]            /* s2 = 3.0 */
    vfms.f32 s0, s1, s2         /* s0 = s0 - s1*s2 = 1.0 - 6.0 = -5.0 */
    vstr    s0, [r7, #0x18]     /* expect -5.0 = 0xC0A00000 */

    /* ============================================ */
    /* Test 8: VFNMA.F32 - Fused neg multiply-add  */
    /* Sd = -(Sd + Sn * Sm)                        */
    /* -(1.0 + 2.0*3.0) = -7.0                     */
    /* ============================================ */
    adr     r0, float_1_0
    vldr    s0, [r0]            /* s0 = 1.0 */
    adr     r0, float_2_0
    vldr    s1, [r0]            /* s1 = 2.0 */
    adr     r0, float_3_0
    vldr    s2, [r0]            /* s2 = 3.0 */
    vfnma.f32 s0, s1, s2        /* s0 = -(s0 + s1*s2) = -7.0 */
    vstr    s0, [r7, #0x1C]     /* expect -7.0 = 0xC0E00000 */

    /* ============================================ */
    /* Test 9: VFNMS.F32 - Fused neg multiply-sub  */
    /* Sd = -(Sd - Sn * Sm) = Sn*Sm - Sd           */
    /* 2.0*3.0 - 1.0 = 5.0                         */
    /* ============================================ */
    adr     r0, float_1_0
    vldr    s0, [r0]            /* s0 = 1.0 */
    adr     r0, float_2_0
    vldr    s1, [r0]            /* s1 = 2.0 */
    adr     r0, float_3_0
    vldr    s2, [r0]            /* s2 = 3.0 */
    vfnms.f32 s0, s1, s2        /* s0 = -(s0 - s1*s2) = 5.0 */
    vstr    s0, [r7, #0x20]     /* expect 5.0 = 0x40A00000 */

    /* ============================================ */
    /* Test 10: Chained VMLA                       */
    /* 1.0 + 2.0*3.0 + 4.0*5.0 = 27.0              */
    /* ============================================ */
    adr     r0, float_1_0
    vldr    s0, [r0]            /* s0 = 1.0 */
    adr     r0, float_2_0
    vldr    s1, [r0]            /* s1 = 2.0 */
    adr     r0, float_3_0
    vldr    s2, [r0]            /* s2 = 3.0 */
    vmla.f32 s0, s1, s2         /* s0 = 1.0 + 6.0 = 7.0 */
    adr     r0, float_4_0
    vldr    s1, [r0]            /* s1 = 4.0 */
    adr     r0, float_5_0
    vldr    s2, [r0]            /* s2 = 5.0 */
    vmla.f32 s0, s1, s2         /* s0 = 7.0 + 20.0 = 27.0 */
    vstr    s0, [r7, #0x24]     /* expect 27.0 = 0x41D80000 */

    /* ============================================ */
    /* Test 11: VMLA with negative accumulator     */
    /* -1.0 + 2.0*3.0 = 5.0                        */
    /* ============================================ */
    adr     r0, float_neg_1_0
    vldr    s0, [r0]            /* s0 = -1.0 */
    adr     r0, float_2_0
    vldr    s1, [r0]            /* s1 = 2.0 */
    adr     r0, float_3_0
    vldr    s2, [r0]            /* s2 = 3.0 */
    vmla.f32 s0, s1, s2         /* s0 = -1.0 + 6.0 = 5.0 */
    vstr    s0, [r7, #0x28]     /* expect 5.0 = 0x40A00000 */

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x2C]

    /* Done */
    bkpt    #0

    .thumb_func
_fault:
    b       _fault

    /* ============================================ */
    /* Float constants (IEEE 754 single precision) */
    /* ============================================ */
    .align 4
float_1_0:
    .word   0x3F800000          /* 1.0 */
float_neg_1_0:
    .word   0xBF800000          /* -1.0 */
float_2_0:
    .word   0x40000000          /* 2.0 */
float_3_0:
    .word   0x40400000          /* 3.0 */
float_4_0:
    .word   0x40800000          /* 4.0 */
float_5_0:
    .word   0x40A00000          /* 5.0 */
float_10_0:
    .word   0x41200000          /* 10.0 */

    .end
