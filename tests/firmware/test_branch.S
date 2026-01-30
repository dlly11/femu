/**
 * Branch operations test for FEMU emulator.
 *
 * Tests: B, BL, BX, BLX, BEQ, BNE, BGT, BLT, CBZ, CBNZ
 *
 * Results stored at 0x20000000:
 * [0x00] = 1    (unconditional branch worked)
 * [0x04] = 2    (BL worked, LR was set correctly)
 * [0x08] = 3    (BEQ worked)
 * [0x0C] = 4    (BNE worked)
 * [0x10] = 5    (BGT worked)
 * [0x14] = 6    (BLT worked)
 * [0x18] = 7    (CBZ worked)
 * [0x1C] = 8    (CBNZ worked)
 * [0x20] = 0xB4A2C4ED (done marker)
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
    ldr     r7, =0x20000000

    /* Test 1: Unconditional branch */
    b       test1_target
test1_fail:
    b       _fault
test1_target:
    movs    r0, #1
    str     r0, [r7, #0]

    /* Test 2: BL (branch with link) */
    bl      subroutine
    /* If we get here, BL and return worked */
    movs    r0, #2
    str     r0, [r7, #4]

    /* Test 3: BEQ (branch if equal) */
    movs    r0, #5
    cmp     r0, #5
    beq     test3_pass
    b       _fault
test3_pass:
    movs    r0, #3
    str     r0, [r7, #8]

    /* Test 4: BNE (branch if not equal) */
    movs    r0, #5
    cmp     r0, #6
    bne     test4_pass
    b       _fault
test4_pass:
    movs    r0, #4
    str     r0, [r7, #12]

    /* Test 5: BGT (branch if greater than, signed) */
    movs    r0, #10
    cmp     r0, #5
    bgt     test5_pass
    b       _fault
test5_pass:
    movs    r0, #5
    str     r0, [r7, #16]

    /* Test 6: BLT (branch if less than, signed) */
    movs    r0, #3
    cmp     r0, #7
    blt     test6_pass
    b       _fault
test6_pass:
    movs    r0, #6
    str     r0, [r7, #20]

    /* Test 7: CBZ (compare and branch if zero) */
    movs    r0, #0
    cbz     r0, test7_pass
    b       _fault
test7_pass:
    movs    r0, #7
    str     r0, [r7, #24]

    /* Test 8: CBNZ (compare and branch if not zero) */
    movs    r0, #1
    cbnz    r0, test8_pass
    b       _fault
test8_pass:
    movs    r0, #8
    str     r0, [r7, #28]

    /* Done marker */
    ldr     r0, =0xB4A2C4ED
    str     r0, [r7, #32]

    bkpt    #0

/* Subroutine for BL test - just returns */
    .thumb_func
subroutine:
    bx      lr

_fault:
    ldr     r0, =0xFFFFFFFF
    ldr     r7, =0x20000000
    str     r0, [r7, #32]
    bkpt    #1

    .end
