/**
 * test_saturate.S - Test saturating arithmetic instructions
 *
 * Tests: SSAT, USAT, and Q flag behavior
 *
 * Memory layout at 0x20000000:
 *   [0x00] = SSAT no saturation (expect same value)
 *   [0x04] = SSAT positive overflow (expect max)
 *   [0x08] = SSAT negative overflow (expect min)
 *   [0x0C] = USAT no saturation (expect same value)
 *   [0x10] = USAT positive overflow (expect max)
 *   [0x14] = USAT negative to zero (expect 0)
 *   [0x18] = Q flag set test (expect 1)
 *   [0x1C] = SSAT with shift (expect result)
 *   [0x20] = USAT with shift (expect result)
 *   [0x24] = SSAT to 8 bits (expect -128 to 127 range)
 *   [0x28] = USAT to 8 bits (expect 0 to 255 range)
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

    /* Clear Q flag first */
    mrs     r0, apsr
    bic     r0, r0, #0x08000000  /* Clear Q bit (bit 27) */
    msr     apsr_nzcvq, r0

    /* Test 1: SSAT no saturation needed
     * SSAT Rd, #n, Rm saturates to signed n-bit range
     * SSAT to 16 bits: range -32768 to 32767
     * Input: 100 (within range)
     */
    movs    r1, #100
    ssat    r0, #16, r1
    str     r0, [r7, #0x00]     /* expect 100 */

    /* Test 2: SSAT positive overflow
     * SSAT to 8 bits: range -128 to 127
     * Input: 200 -> saturates to 127
     */
    mov     r1, #200
    ssat    r0, #8, r1
    str     r0, [r7, #0x04]     /* expect 127 */

    /* Test 3: SSAT negative overflow
     * Input: -200 -> saturates to -128
     */
    mov     r1, #200
    rsb     r1, r1, #0          /* r1 = -200 */
    ssat    r0, #8, r1
    str     r0, [r7, #0x08]     /* expect -128 (0xFFFFFF80) */

    /* Test 4: USAT no saturation needed
     * USAT to 8 bits: range 0 to 255
     * Input: 100 (within range)
     */
    movs    r1, #100
    usat    r0, #8, r1
    str     r0, [r7, #0x0C]     /* expect 100 */

    /* Test 5: USAT positive overflow
     * Input: 300 -> saturates to 255
     */
    mov     r1, #300
    usat    r0, #8, r1
    str     r0, [r7, #0x10]     /* expect 255 */

    /* Test 6: USAT negative to zero
     * Input: -100 -> saturates to 0
     */
    movs    r1, #100
    rsb     r1, r1, #0          /* r1 = -100 */
    usat    r0, #8, r1
    str     r0, [r7, #0x14]     /* expect 0 */

    /* Test 7: Q flag set on saturation
     * Clear Q flag, do saturating op, check Q flag
     */
    mrs     r0, apsr
    bic     r0, r0, #0x08000000  /* Clear Q bit */
    msr     apsr_nzcvq, r0

    mov     r1, #1000
    ssat    r2, #8, r1          /* Will saturate, setting Q */

    movs    r0, #0
    mrs     r3, apsr
    tst     r3, #0x08000000     /* Test Q bit */
    it      ne
    movne   r0, #1
    str     r0, [r7, #0x18]     /* expect 1 (Q flag set) */

    /* Test 8: SSAT with shift
     * SSAT Rd, #n, Rm, LSL #shift
     * Input: 16, shift left 4 = 256, saturate to 8-bit signed = 127
     */
    movs    r1, #16
    ssat    r0, #8, r1, lsl #4
    str     r0, [r7, #0x1C]     /* expect 127 */

    /* Test 9: USAT with shift
     * Input: 32, shift left 4 = 512, saturate to 8-bit unsigned = 255
     */
    movs    r1, #32
    usat    r0, #8, r1, lsl #4
    str     r0, [r7, #0x20]     /* expect 255 */

    /* Test 10: SSAT to 8 bits boundary test
     * Input: 127 (max positive) - should not saturate
     */
    movs    r1, #127
    ssat    r0, #8, r1
    str     r0, [r7, #0x24]     /* expect 127 */

    /* Test 11: USAT to 8 bits boundary test
     * Input: 255 (max) - should not saturate
     */
    mov     r1, #255
    usat    r0, #8, r1
    str     r0, [r7, #0x28]     /* expect 255 */

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x2C]

    /* Done */
    bkpt    #0

    .thumb_func
_fault:
    b       _fault

    .end
