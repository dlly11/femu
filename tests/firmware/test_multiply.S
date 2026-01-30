/**
 * Extended multiply test for FEMU emulator.
 *
 * Tests: MLA, MLS, UMULL, SMULL, UMLAL, SMLAL, UDIV, SDIV
 *
 * Results stored at 0x20000000:
 * [0x00] = MLA: 3*4+5 = 17             (multiply accumulate)
 * [0x04] = MLS: 10 - 3*2 = 4           (multiply subtract)
 * [0x08] = UMULL low: 0x100000 * 0x100 (unsigned 64-bit multiply, low)
 * [0x0C] = UMULL high                  (unsigned 64-bit multiply, high)
 * [0x10] = SMULL low: -100 * 1000000   (signed 64-bit multiply, low)
 * [0x14] = SMULL high                  (signed 64-bit multiply, high)
 * [0x18] = UDIV: 100 / 7 = 14          (unsigned divide)
 * [0x1C] = SDIV: -100 / 7 = -14        (signed divide)
 * [0x20] = SDIV: 100 / -7 = -14        (signed divide negative divisor)
 * [0x24] = UMLAL low                   (unsigned multiply accumulate long)
 * [0x28] = UMLAL high
 * [0x2C] = 0xCAFED00D                  (done marker)
 */

    .syntax unified
    .cpu cortex-m4
    .thumb

    .section .vectors, "a"
    .align 2
_vectors:
    .word   _stack_top
    .word   _reset + 1
    .word   _fault + 1      /* NMI */
    .word   _fault + 1      /* HardFault */
    .space  44              /* Rest of vector table */

    .section .text
    .align 2
    .thumb_func
    .global _reset
_reset:
    /* R7 = result pointer */
    ldr     r7, =0x20000000

    /* MLA: Rd = Rn * Rm + Ra = 3 * 4 + 5 = 17 */
    movs    r0, #3
    movs    r1, #4
    movs    r2, #5
    mla     r3, r0, r1, r2
    str     r3, [r7, #0]

    /* MLS: Rd = Ra - Rn * Rm = 10 - 3 * 2 = 4 */
    movs    r0, #3
    movs    r1, #2
    movs    r2, #10
    mls     r3, r0, r1, r2
    str     r3, [r7, #4]

    /* UMULL: 64-bit result = 0x100000 * 0x100 = 0x10000000000 */
    mov     r0, #0x100000
    mov     r1, #0x100
    umull   r2, r3, r0, r1  /* R3:R2 = R0 * R1 */
    str     r2, [r7, #8]    /* Low word: 0x00000000 */
    str     r3, [r7, #12]   /* High word: 0x00000010 */

    /* SMULL: 64-bit signed result = -100 * 1000000 = -100000000 */
    mov     r0, #-100
    ldr     r1, =1000000
    smull   r2, r3, r0, r1  /* R3:R2 = R0 * R1 (signed) */
    str     r2, [r7, #16]   /* Low word */
    str     r3, [r7, #20]   /* High word */

    /* UDIV: 100 / 7 = 14 */
    movs    r0, #100
    movs    r1, #7
    udiv    r2, r0, r1
    str     r2, [r7, #24]

    /* SDIV: -100 / 7 = -14 (rounds toward zero) */
    mov     r0, #-100
    movs    r1, #7
    sdiv    r2, r0, r1
    str     r2, [r7, #28]

    /* SDIV: 100 / -7 = -14 */
    movs    r0, #100
    mov     r1, #-7
    sdiv    r2, r0, r1
    str     r2, [r7, #32]

    /* UMLAL: Accumulate 64-bit: R3:R2 += R0 * R1 */
    /* Start with 0x100 in R3:R2, add 0x1000 * 0x1000 = 0x1000000 */
    mov     r2, #0x100      /* Low accumulator */
    movs    r3, #0          /* High accumulator */
    mov     r0, #0x1000
    mov     r1, #0x1000
    umlal   r2, r3, r0, r1  /* R3:R2 += R0 * R1 */
    str     r2, [r7, #36]   /* Low: 0x1000100 */
    str     r3, [r7, #40]   /* High: 0 */

    /* Done marker */
    ldr     r0, =0xCAFED00D
    str     r0, [r7, #44]

    bkpt    #0

_fault:
    b       _fault

    .end
