/**
 * test_shift_rotate.S - Test shift and rotate edge cases
 *
 * Tests: LSL, LSR, ASR, ROR, RRX with various shift amounts
 *        including edge cases (0, 1, 31, 32)
 *
 * Memory layout at 0x20000000:
 *   [0x00] = LSL by 0 (expect unchanged)
 *   [0x04] = LSL by 1 (expect doubled)
 *   [0x08] = LSL by 31 (expect 0x80000000 or 0)
 *   [0x0C] = LSR by 0 (expect unchanged)
 *   [0x10] = LSR by 1 (expect halved)
 *   [0x14] = LSR by 31 (expect 1 or 0)
 *   [0x18] = ASR by 0 (expect unchanged)
 *   [0x1C] = ASR by 1 positive (expect halved)
 *   [0x20] = ASR by 1 negative (expect sign-preserved)
 *   [0x24] = ASR by 31 negative (expect 0xFFFFFFFF)
 *   [0x28] = ROR by 0 (expect unchanged)
 *   [0x2C] = ROR by 8 (expect rotated)
 *   [0x30] = ROR by 16 (expect swapped halves)
 *   [0x34] = RRX (rotate right through carry)
 *   [0x38] = LSL sets carry flag test
 *   [0x3C] = Variable shift amount test
 *   [0x40] = 0xC0FFEE42 (success marker)
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

    /* Test 1: LSL by 0 (no change) */
    ldr     r1, =0x12345678
    lsl     r0, r1, #0
    str     r0, [r7, #0x00]     /* expect 0x12345678 */

    /* Test 2: LSL by 1 (multiply by 2) */
    movs    r1, #0x40           /* 64 */
    lsls    r0, r1, #1
    str     r0, [r7, #0x04]     /* expect 0x80 (128) */

    /* Test 3: LSL by 31 */
    movs    r1, #1
    lsl     r0, r1, #31
    str     r0, [r7, #0x08]     /* expect 0x80000000 */

    /* Test 4: LSR by 0 (no change) */
    ldr     r1, =0x12345678
    lsr     r0, r1, #0
    str     r0, [r7, #0x0C]     /* expect 0x12345678 */

    /* Test 5: LSR by 1 (divide by 2) */
    mov     r1, #0x80           /* 128 */
    lsrs    r0, r1, #1
    str     r0, [r7, #0x10]     /* expect 0x40 (64) */

    /* Test 6: LSR by 31 */
    ldr     r1, =0x80000000
    lsr     r0, r1, #31
    str     r0, [r7, #0x14]     /* expect 1 */

    /* Test 7: ASR by 0 (no change) */
    ldr     r1, =0x12345678
    asr     r0, r1, #0
    str     r0, [r7, #0x18]     /* expect 0x12345678 */

    /* Test 8: ASR by 1 positive number */
    mov     r1, #0x80           /* 128, positive */
    asrs    r0, r1, #1
    str     r0, [r7, #0x1C]     /* expect 0x40 (64) */

    /* Test 9: ASR by 1 negative number */
    ldr     r1, =0x80000000     /* -2147483648 */
    asr     r0, r1, #1
    str     r0, [r7, #0x20]     /* expect 0xC0000000 (sign extended) */

    /* Test 10: ASR by 31 negative number */
    ldr     r1, =0x80000000     /* Negative */
    asr     r0, r1, #31
    str     r0, [r7, #0x24]     /* expect 0xFFFFFFFF (all sign bits) */

    /* Test 11: ROR by 0 (no change - actually ROR #0 is RRX in some encodings) */
    ldr     r1, =0x12345678
    ror     r0, r1, #0
    str     r0, [r7, #0x28]     /* expect 0x12345678 or RRX result */

    /* Test 12: ROR by 8 */
    ldr     r1, =0x12345678
    ror     r0, r1, #8
    str     r0, [r7, #0x2C]     /* expect 0x78123456 */

    /* Test 13: ROR by 16 (swap halves) */
    ldr     r1, =0x12345678
    ror     r0, r1, #16
    str     r0, [r7, #0x30]     /* expect 0x56781234 */

    /* Test 14: RRX (rotate right extended through carry)
     * First set carry flag, then RRX
     */
    movs    r1, #1
    lsrs    r1, r1, #1          /* Shift out 1, sets carry */
    ldr     r1, =0x00000002     /* Value to rotate */
    rrx     r0, r1              /* Rotate right, carry -> MSB, LSB -> carry */
    str     r0, [r7, #0x34]     /* expect 0x80000001 (carry was 1, shifted in) */

    /* Test 15: LSL sets carry flag test */
    movs    r0, #0
    ldr     r1, =0x80000000
    lsls    r1, r1, #1          /* Shift out MSB=1, sets carry */
    it      cs
    movcs   r0, #1
    str     r0, [r7, #0x38]     /* expect 1 (carry set) */

    /* Test 16: Variable shift amount (register) */
    ldr     r1, =0x12345678
    movs    r2, #4              /* Shift amount in register */
    lsl     r0, r1, r2          /* LSL by r2 */
    str     r0, [r7, #0x3C]     /* expect 0x23456780 */

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x40]

    /* Done */
    bkpt    #0

    .thumb_func
_fault:
    b       _fault

    .end
