/**
 * Arithmetic operations test for FEMU emulator.
 *
 * Tests: ADD, SUB, MUL, AND, ORR, EOR, LSL, LSR, ASR
 *
 * Results stored at 0x20000000:
 * [0x00] = 15 + 7 = 22          (ADD)
 * [0x04] = 100 - 37 = 63        (SUB)
 * [0x08] = 6 * 7 = 42           (MUL)
 * [0x0C] = 0xFF & 0x0F = 0x0F   (AND)
 * [0x10] = 0xF0 | 0x0F = 0xFF   (ORR)
 * [0x14] = 0xAA ^ 0xFF = 0x55   (EOR)
 * [0x18] = 1 << 4 = 16          (LSL)
 * [0x1C] = 256 >> 4 = 16        (LSR)
 * [0x20] = -8 >> 2 = -2         (ASR, sign extended)
 * [0x24] = 0xCAFEBABE           (done marker)
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

    /* ADD: 15 + 7 = 22 */
    movs    r0, #15
    movs    r1, #7
    adds    r0, r0, r1
    str     r0, [r7, #0]

    /* SUB: 100 - 37 = 63 */
    movs    r0, #100
    movs    r1, #37
    subs    r0, r0, r1
    str     r0, [r7, #4]

    /* MUL: 6 * 7 = 42 */
    movs    r0, #6
    movs    r1, #7
    muls    r0, r1, r0
    str     r0, [r7, #8]

    /* AND: 0xFF & 0x0F = 0x0F */
    movs    r0, #0xFF
    movs    r1, #0x0F
    ands    r0, r0, r1
    str     r0, [r7, #12]

    /* ORR: 0xF0 | 0x0F = 0xFF */
    movs    r0, #0xF0
    movs    r1, #0x0F
    orrs    r0, r0, r1
    str     r0, [r7, #16]

    /* EOR: 0xAA ^ 0xFF = 0x55 */
    movs    r0, #0xAA
    movs    r1, #0xFF
    eors    r0, r0, r1
    str     r0, [r7, #20]

    /* LSL: 1 << 4 = 16 */
    movs    r0, #1
    movs    r1, #4
    lsls    r0, r0, r1
    str     r0, [r7, #24]

    /* LSR: 256 >> 4 = 16 */
    mov     r0, #256
    movs    r1, #4
    lsrs    r0, r0, r1
    str     r0, [r7, #28]

    /* ASR: -8 >> 2 = -2 (0xFFFFFFF8 >> 2 = 0xFFFFFFFE) */
    mvn     r0, #7          /* R0 = ~7 = -8 = 0xFFFFFFF8 */
    movs    r1, #2
    asrs    r0, r0, r1
    str     r0, [r7, #32]

    /* Done marker */
    ldr     r0, =0xCAFEBABE
    str     r0, [r7, #36]

    bkpt    #0

_fault:
    b       _fault

    .end
