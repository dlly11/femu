/**
 * test_sat_add.S - Test 32-bit saturating arithmetic instructions
 *
 * Tests: QADD, QSUB, QDADD, QDSUB
 *
 * These differ from SSAT/USAT in that they operate on signed 32-bit values
 * and saturate to the INT32_MIN/INT32_MAX range.
 *
 * Memory layout at 0x20000000:
 *   [0x00] = QADD no saturation
 *   [0x04] = QADD positive overflow -> INT32_MAX
 *   [0x08] = QADD negative overflow -> INT32_MIN
 *   [0x0C] = QSUB no saturation
 *   [0x10] = QSUB positive overflow -> INT32_MAX
 *   [0x14] = QSUB negative overflow -> INT32_MIN
 *   [0x18] = QDADD no saturation (2*Rm + Rn)
 *   [0x1C] = QDADD with doubling overflow
 *   [0x20] = QDSUB no saturation (Rn - 2*Rm)
 *   [0x24] = QDSUB overflow
 *   [0x28] = Q flag set test
 *   [0x2C] = 0xC0FFEE42 (success marker)
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

    /* Clear Q flag */
    mrs     r0, apsr
    bic     r0, r0, #0x08000000  /* Clear Q bit (bit 27) */
    msr     apsr_nzcvq, r0

    /* ============================================ */
    /* Test 1: QADD no saturation                  */
    /* QADD Rd, Rm, Rn: Rd = Rm + Rn (saturated)   */
    /* 100 + 50 = 150                              */
    /* ============================================ */
    movs    r0, #100
    movs    r1, #50
    qadd    r2, r0, r1
    str     r2, [r7, #0x00]     /* expect 150 */

    /* ============================================ */
    /* Test 2: QADD positive overflow              */
    /* INT32_MAX + 1 -> INT32_MAX (0x7FFFFFFF)     */
    /* ============================================ */
    ldr     r0, =0x7FFFFFFF     /* INT32_MAX */
    movs    r1, #1
    qadd    r2, r0, r1
    str     r2, [r7, #0x04]     /* expect 0x7FFFFFFF */

    /* ============================================ */
    /* Test 3: QADD negative overflow              */
    /* INT32_MIN + (-1) -> INT32_MIN (0x80000000)  */
    /* ============================================ */
    ldr     r0, =0x80000000     /* INT32_MIN */
    ldr     r1, =0xFFFFFFFF     /* -1 */
    qadd    r2, r0, r1
    str     r2, [r7, #0x08]     /* expect 0x80000000 */

    /* ============================================ */
    /* Test 4: QSUB no saturation                  */
    /* QSUB Rd, Rm, Rn: Rd = Rm - Rn (saturated)   */
    /* 100 - 30 = 70                               */
    /* ============================================ */
    movs    r0, #100
    movs    r1, #30
    qsub    r2, r0, r1
    str     r2, [r7, #0x0C]     /* expect 70 */

    /* ============================================ */
    /* Test 5: QSUB positive overflow              */
    /* INT32_MAX - (-1) -> INT32_MAX               */
    /* ============================================ */
    ldr     r0, =0x7FFFFFFF
    ldr     r1, =0xFFFFFFFF     /* -1 */
    qsub    r2, r0, r1
    str     r2, [r7, #0x10]     /* expect 0x7FFFFFFF */

    /* ============================================ */
    /* Test 6: QSUB negative overflow              */
    /* INT32_MIN - 1 -> INT32_MIN                  */
    /* ============================================ */
    ldr     r0, =0x80000000
    movs    r1, #1
    qsub    r2, r0, r1
    str     r2, [r7, #0x14]     /* expect 0x80000000 */

    /* ============================================ */
    /* Test 7: QDADD no saturation                 */
    /* QDADD Rd, Rm, Rn: Rd = Rm + 2*Rn (saturated)*/
    /* 10 + 2*20 = 50                              */
    /* ============================================ */
    movs    r0, #10
    movs    r1, #20
    qdadd   r2, r0, r1
    str     r2, [r7, #0x18]     /* expect 50 */

    /* ============================================ */
    /* Test 8: QDADD with doubling overflow        */
    /* 0 + 2*0x7FFFFFFF -> 0x7FFFFFFF (sat on 2*Rn) */
    /* ============================================ */
    movs    r0, #0
    ldr     r1, =0x7FFFFFFF
    qdadd   r2, r0, r1
    str     r2, [r7, #0x1C]     /* expect 0x7FFFFFFF */

    /* ============================================ */
    /* Test 9: QDSUB no saturation                 */
    /* QDSUB Rd, Rm, Rn: Rd = Rm - 2*Rn (saturated)*/
    /* 100 - 2*20 = 60                             */
    /* ============================================ */
    movs    r0, #100
    movs    r1, #20
    qdsub   r2, r0, r1
    str     r2, [r7, #0x20]     /* expect 60 */

    /* ============================================ */
    /* Test 10: QDSUB overflow                     */
    /* INT32_MIN - 2*1 -> INT32_MIN                */
    /* ============================================ */
    ldr     r0, =0x80000000
    movs    r1, #1
    qdsub   r2, r0, r1
    str     r2, [r7, #0x24]     /* expect 0x80000000 */

    /* ============================================ */
    /* Test 11: Q flag set on saturation           */
    /* Clear Q, do saturating op, check Q flag     */
    /* ============================================ */
    mrs     r0, apsr
    bic     r0, r0, #0x08000000  /* Clear Q bit */
    msr     apsr_nzcvq, r0

    ldr     r0, =0x7FFFFFFF
    movs    r1, #1
    qadd    r2, r0, r1          /* Will saturate, setting Q */

    movs    r0, #0
    mrs     r3, apsr
    tst     r3, #0x08000000     /* Test Q bit */
    it      ne
    movne   r0, #1
    str     r0, [r7, #0x28]     /* expect 1 (Q flag set) */

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x2C]

    /* Done */
    bkpt    #0

    .thumb_func
_fault:
    b       _fault

    .end
