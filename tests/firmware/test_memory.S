/**
 * Memory operations test for FEMU emulator.
 *
 * Tests: LDR, STR, LDRB, STRB, LDRH, STRH, LDM, STM
 *
 * Results stored at 0x20000100:
 * [0x00] = 0x12345678       (word store/load)
 * [0x04] = 0x000000AB       (byte store/load)
 * [0x08] = 0x0000CDEF       (halfword store/load)
 * [0x0C] = 0x11111111       (STM result 1)
 * [0x10] = 0x22222222       (STM result 2)
 * [0x14] = 0x33333333       (STM result 3)
 * [0x18] = 0x11111111       (LDM verify 1)
 * [0x1C] = 0x22222222       (LDM verify 2)
 * [0x20] = 0x33333333       (LDM verify 3)
 * [0x24] = 0xDEADC0DE       (done marker)
 */

    .syntax unified
    .cpu cortex-m4
    .thumb

    .section .vectors, "a"
    .align 2
_vectors:
    .word   _stack_top
    .word   _reset + 1
    .word   _fault + 1
    .word   _fault + 1
    .space  44

    .section .text
    .align 2
    .thumb_func
    .global _reset
_reset:
    /* R7 = result pointer */
    ldr     r7, =0x20000100

    /* Test word store/load */
    ldr     r0, =0x12345678
    str     r0, [r7, #0]
    ldr     r1, [r7, #0]
    /* R1 should equal R0 */
    cmp     r0, r1
    bne     _fault

    /* Test byte store/load */
    movs    r0, #0xAB
    strb    r0, [r7, #4]
    ldrb    r1, [r7, #4]
    str     r1, [r7, #4]    /* Store full word for verification */

    /* Test halfword store/load */
    ldr     r0, =0xCDEF
    strh    r0, [r7, #8]
    ldrh    r1, [r7, #8]
    str     r1, [r7, #8]    /* Store full word for verification */

    /* Test STM (store multiple) */
    ldr     r0, =0x11111111
    ldr     r1, =0x22222222
    ldr     r2, =0x33333333
    add     r3, r7, #12     /* R3 = address for STM */
    stm     r3!, {r0, r1, r2}

    /* Test LDM (load multiple) */
    add     r4, r7, #12     /* R4 = address for LDM */
    ldm     r4!, {r0, r1, r2}

    /* Store LDM results for verification */
    str     r0, [r7, #24]
    str     r1, [r7, #28]
    str     r2, [r7, #32]

    /* Done marker */
    ldr     r0, =0xDEADC0DE
    str     r0, [r7, #36]

    bkpt    #0

_fault:
    /* Store fault indicator */
    ldr     r0, =0xFFFFFFFF
    ldr     r7, =0x20000100
    str     r0, [r7, #36]
    bkpt    #1

    .end
