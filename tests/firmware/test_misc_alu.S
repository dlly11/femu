/**
 * test_misc_alu.S - Test miscellaneous ALU instructions
 *
 * Tests: BIC, ORN, ADC, SBC, RSB, carry flag operations
 *
 * Memory layout at 0x20000000:
 *   [0x00] = BIC result (expect 0xFFFF0000)
 *   [0x04] = BIC with shifted operand (expect result)
 *   [0x08] = ORN result (expect 0xFFFF00FF)
 *   [0x0C] = ADC without carry (expect sum)
 *   [0x10] = ADC with carry (expect sum + 1)
 *   [0x14] = SBC without borrow (expect diff)
 *   [0x18] = SBC with borrow (expect diff - 1)
 *   [0x1C] = RSB result (expect negated)
 *   [0x20] = RSB immediate (expect imm - reg)
 *   [0x24] = 64-bit add low (via ADC chain)
 *   [0x28] = 64-bit add high
 *   [0x2C] = 64-bit sub low (via SBC chain)
 *   [0x30] = 64-bit sub high
 *   [0x34] = BIC immediate (expect result)
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
    /* Test 1-2: BIC (Bit Clear)                   */
    /* BIC Rd, Rn, Rm: Rd = Rn AND NOT(Rm)        */
    /* ============================================ */

    /* Test 1: BIC basic */
    ldr     r0, =0xFFFFFFFF
    ldr     r1, =0x0000FFFF
    bic     r2, r0, r1          /* 0xFFFFFFFF & ~0x0000FFFF = 0xFFFF0000 */
    str     r2, [r7, #0x00]     /* expect 0xFFFF0000 */

    /* Test 2: BIC with shifted operand */
    ldr     r0, =0xFFFFFFFF
    movs    r1, #0xFF           /* 0x000000FF */
    bic     r2, r0, r1, lsl #8  /* 0xFFFFFFFF & ~0x0000FF00 = 0xFFFF00FF */
    str     r2, [r7, #0x04]     /* expect 0xFFFF00FF */

    /* ============================================ */
    /* Test 3: ORN (OR NOT)                        */
    /* ORN Rd, Rn, Rm: Rd = Rn OR NOT(Rm)         */
    /* ============================================ */

    /* Test 3: ORN */
    ldr     r0, =0x00000000
    ldr     r1, =0x0000FF00
    orn     r2, r0, r1          /* 0x00000000 | ~0x0000FF00 = 0xFFFF00FF */
    str     r2, [r7, #0x08]     /* expect 0xFFFF00FF */

    /* ============================================ */
    /* Test 4-5: ADC (Add with Carry)              */
    /* ============================================ */

    /* Test 4: ADC without carry (clear carry first) */
    movs    r0, #0              /* Clear all flags */
    msr     apsr_nzcvq, r0

    movs    r0, #100
    movs    r1, #50
    adc     r2, r0, r1          /* 100 + 50 + 0 = 150 */
    str     r2, [r7, #0x0C]     /* expect 150 */

    /* Test 5: ADC with carry (set carry first) */
    ldr     r0, =0xFFFFFFFF
    movs    r1, #1
    adds    r3, r0, r1          /* Sets carry flag */

    movs    r0, #100
    movs    r1, #50
    adc     r2, r0, r1          /* 100 + 50 + 1 = 151 */
    str     r2, [r7, #0x10]     /* expect 151 */

    /* ============================================ */
    /* Test 6-7: SBC (Subtract with Carry/Borrow)  */
    /* SBC: Rd = Rn - Rm - NOT(C)                  */
    /* When C=1: no borrow, When C=0: borrow       */
    /* ============================================ */

    /* Test 6: SBC without borrow (carry = 1) */
    /* Set carry by doing a subtraction that doesn't borrow */
    movs    r0, #100
    movs    r1, #50
    subs    r3, r0, r1          /* 100 - 50 = 50, sets C=1 (no borrow) */

    movs    r0, #100
    movs    r1, #30
    sbc     r2, r0, r1          /* 100 - 30 - 0 = 70 */
    str     r2, [r7, #0x14]     /* expect 70 */

    /* Test 7: SBC with borrow (carry = 0) */
    /* Clear carry by doing a subtraction that borrows */
    movs    r0, #50
    movs    r1, #100
    subs    r3, r0, r1          /* 50 - 100 = -50, sets C=0 (borrow) */

    movs    r0, #100
    movs    r1, #30
    sbc     r2, r0, r1          /* 100 - 30 - 1 = 69 */
    str     r2, [r7, #0x18]     /* expect 69 */

    /* ============================================ */
    /* Test 8-9: RSB (Reverse Subtract)            */
    /* RSB Rd, Rn, Rm: Rd = Rm - Rn                */
    /* RSB Rd, Rn, #imm: Rd = imm - Rn             */
    /* ============================================ */

    /* Test 8: RSB with register */
    movs    r0, #30
    movs    r1, #100
    rsb     r2, r0, r1          /* 100 - 30 = 70 */
    str     r2, [r7, #0x1C]     /* expect 70 */

    /* Test 9: RSB with immediate (negate) */
    movs    r0, #42
    rsb     r2, r0, #0          /* 0 - 42 = -42 */
    str     r2, [r7, #0x20]     /* expect -42 (0xFFFFFFD6) */

    /* ============================================ */
    /* Test 10-11: 64-bit addition using ADC       */
    /* Add: 0x00000001_FFFFFFFF + 0x00000000_00000002 */
    /* Result: 0x00000002_00000001                  */
    /* ============================================ */

    /* First operand: r0:r1 = 0x00000001:0xFFFFFFFF */
    ldr     r0, =0xFFFFFFFF     /* Low */
    movs    r1, #1              /* High */

    /* Second operand: r2:r3 = 0x00000000:0x00000002 */
    movs    r2, #2              /* Low */
    movs    r3, #0              /* High */

    /* Add low parts, capture carry */
    adds    r4, r0, r2          /* Low result, sets carry */
    adc     r5, r1, r3          /* High result + carry */

    str     r4, [r7, #0x24]     /* Low: expect 0x00000001 */
    str     r5, [r7, #0x28]     /* High: expect 0x00000002 */

    /* ============================================ */
    /* Test 12-13: 64-bit subtraction using SBC    */
    /* Sub: 0x00000002_00000001 - 0x00000000_00000002 */
    /* Result: 0x00000001_FFFFFFFF                  */
    /* ============================================ */

    /* First operand: r0:r1 = 0x00000002:0x00000001 */
    movs    r0, #1              /* Low */
    movs    r1, #2              /* High */

    /* Second operand: r2:r3 = 0x00000000:0x00000002 */
    movs    r2, #2              /* Low */
    movs    r3, #0              /* High */

    /* Subtract low parts, capture borrow */
    subs    r4, r0, r2          /* Low result, clears carry if borrow */
    sbc     r5, r1, r3          /* High result - borrow */

    str     r4, [r7, #0x2C]     /* Low: expect 0xFFFFFFFF */
    str     r5, [r7, #0x30]     /* High: expect 0x00000001 */

    /* ============================================ */
    /* Test 14: BIC with immediate                 */
    /* ============================================ */

    ldr     r0, =0xFFFFFFFF
    bic     r1, r0, #0xFF       /* Clear low byte */
    str     r1, [r7, #0x34]     /* expect 0xFFFFFF00 */

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x38]

    /* Done */
    bkpt    #0

    .thumb_func
_fault:
    b       _fault

    .end
