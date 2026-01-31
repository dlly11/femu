/**
 * test_move.S - Test move instructions
 *
 * Tests: MOV, MVN, MOVW, MOVT, MOV with shifted operands
 *
 * Memory layout at 0x20000000:
 *   [0x00] = MOV immediate small (expect 0x000000FF)
 *   [0x04] = MOV immediate large (expect 0x12340000)
 *   [0x08] = MVN immediate (expect 0xFFFFFF00)
 *   [0x0C] = MOV register (expect 0xDEADBEEF)
 *   [0x10] = MOVW 16-bit immediate (expect 0x0000ABCD)
 *   [0x14] = MOVT set top half (expect 0x12340000)
 *   [0x18] = MOVW + MOVT combo (expect 0x12345678)
 *   [0x1C] = MOV with LSL (expect shifted value)
 *   [0x20] = MOV with LSR (expect shifted value)
 *   [0x24] = MOV with ASR (expect shifted value)
 *   [0x28] = MOV with ROR (expect rotated value)
 *   [0x2C] = MVN register (expect ~0x0000FFFF)
 *   [0x30] = MOV sets flags test (expect 1 if negative)
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

    /* Test 1: MOV immediate (small, Thumb-16 encoding) */
    movs    r0, #0xFF
    str     r0, [r7, #0x00]     /* expect 0x000000FF */

    /* Test 2: MOV immediate (large, Thumb-32 modified immediate) */
    /* Note: 0x12340000 can't be encoded as modified immediate, use encodable value */
    mov.w   r0, #0x12000000     /* 0x12 << 24 is encodable */
    str     r0, [r7, #0x04]     /* expect 0x12000000 */

    /* Test 3: MVN immediate (move not) */
    mvn     r0, #0xFF           /* ~0xFF = 0xFFFFFF00 */
    str     r0, [r7, #0x08]     /* expect 0xFFFFFF00 */

    /* Test 4: MOV register */
    ldr     r1, =0xDEADBEEF
    mov     r0, r1
    str     r0, [r7, #0x0C]     /* expect 0xDEADBEEF */

    /* Test 5: MOVW - Move 16-bit immediate to bottom half */
    movw    r0, #0xABCD
    str     r0, [r7, #0x10]     /* expect 0x0000ABCD */

    /* Test 6: MOVT - Move 16-bit immediate to top half (clears bottom) */
    movs    r0, #0              /* Clear register first */
    movt    r0, #0x1234         /* Set top 16 bits */
    str     r0, [r7, #0x14]     /* expect 0x12340000 */

    /* Test 7: MOVW + MOVT combination for full 32-bit immediate */
    movw    r0, #0x5678         /* Low 16 bits */
    movt    r0, #0x1234         /* High 16 bits */
    str     r0, [r7, #0x18]     /* expect 0x12345678 */

    /* Test 8: MOV with LSL (logical shift left) */
    movs    r1, #0x0F           /* 0x0000000F */
    lsl     r0, r1, #4          /* 0x0F << 4 = 0xF0 */
    str     r0, [r7, #0x1C]     /* expect 0x000000F0 */

    /* Test 9: MOV with LSR (logical shift right) */
    mov     r1, #0xF0           /* 0x000000F0 */
    lsr     r0, r1, #4          /* 0xF0 >> 4 = 0x0F */
    str     r0, [r7, #0x20]     /* expect 0x0000000F */

    /* Test 10: MOV with ASR (arithmetic shift right, preserves sign) */
    ldr     r1, =0x80000000     /* Negative number */
    asr     r0, r1, #4          /* Arithmetic right shift */
    str     r0, [r7, #0x24]     /* expect 0xF8000000 (sign extended) */

    /* Test 11: MOV with ROR (rotate right) */
    ldr     r1, =0x12345678
    ror     r0, r1, #8          /* Rotate right by 8 */
    str     r0, [r7, #0x28]     /* expect 0x78123456 */

    /* Test 12: MVN register (bitwise NOT of register) */
    ldr     r1, =0x0000FFFF
    mvn     r0, r1              /* ~0x0000FFFF = 0xFFFF0000 */
    str     r0, [r7, #0x2C]     /* expect 0xFFFF0000 */

    /* Test 13: MOVS sets flags - test negative flag */
    movs    r0, #0
    ldr     r1, =0x80000000     /* Negative when viewed as signed */
    movs    r0, r1              /* This should set N flag... but MOVS r0, r1 doesn't exist for high regs */
    /* Use different approach: */
    movs    r0, #0
    mvn     r1, #0              /* r1 = 0xFFFFFFFF = -1 */
    adds    r0, r1, #0          /* r0 = r1, sets N flag */
    ite     mi
    movmi   r0, #1
    movpl   r0, #0
    str     r0, [r7, #0x30]     /* expect 1 */

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x34]

    /* Done */
    bkpt    #0

    .thumb_func
_fault:
    b       _fault

    .end
