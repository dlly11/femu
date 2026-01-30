/**
 * IT block (If-Then) conditional execution test for FEMU emulator.
 *
 * Tests IT, ITE, ITT, ITEE, ITTE patterns with various conditions
 *
 * Results stored at 0x20000000:
 * [0x00] = IT EQ test (condition true)
 * [0x04] = IT EQ test (condition false)
 * [0x08] = ITE NE test (then branch)
 * [0x0C] = ITE NE test (else branch)
 * [0x10] = ITT GT test (both executed)
 * [0x14] = ITT GT test second value
 * [0x18] = ITEE LT test pattern
 * [0x1C] = Complex IT with flags
 * [0x20] = 0xC0FFEE42                  (done marker)
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

    /* Test 1: IT EQ with condition true */
    /* Compare 5 == 5, then conditionally move 1 */
    movs    r0, #0          /* Default value */
    movs    r1, #5
    cmp     r1, #5          /* Sets Z flag (equal) */
    it      eq
    moveq   r0, #1          /* Should execute: R0 = 1 */
    str     r0, [r7, #0]

    /* Test 2: IT EQ with condition false */
    /* Compare 5 == 6, then conditionally move 1 */
    movs    r0, #0          /* Default value */
    movs    r1, #5
    cmp     r1, #6          /* Clears Z flag (not equal) */
    it      eq
    moveq   r0, #1          /* Should NOT execute: R0 stays 0 */
    str     r0, [r7, #4]

    /* Test 3: ITE NE - then branch taken */
    /* Compare 3 != 5, then set 10, else set 20 */
    movs    r0, #0
    movs    r1, #3
    cmp     r1, #5          /* 3 != 5, Z=0 */
    ite     ne
    movne   r0, #10         /* Should execute */
    moveq   r0, #20         /* Should NOT execute */
    str     r0, [r7, #8]    /* Result: 10 */

    /* Test 4: ITE NE - else branch taken */
    /* Compare 5 != 5, then set 10, else set 20 */
    movs    r0, #0
    movs    r1, #5
    cmp     r1, #5          /* 5 == 5, Z=1 */
    ite     ne
    movne   r0, #10         /* Should NOT execute */
    moveq   r0, #20         /* Should execute */
    str     r0, [r7, #12]   /* Result: 20 */

    /* Test 5: ITT GT - both instructions execute when GT */
    /* Compare 10 > 5 */
    movs    r0, #0
    movs    r1, #0
    movs    r2, #10
    cmp     r2, #5          /* 10 > 5, sets flags for GT */
    itt     gt
    movgt   r0, #1          /* Should execute */
    movgt   r1, #2          /* Should execute */
    str     r0, [r7, #16]   /* Result: 1 */
    str     r1, [r7, #20]   /* Result: 2 */

    /* Test 6: ITEE LT - complex pattern */
    /* Compare 3 < 10: LT is true */
    movs    r0, #0
    movs    r1, #3
    cmp     r1, #10         /* 3 < 10, LT true */
    itee    lt
    movlt   r0, #1          /* T: Should execute */
    movge   r0, #2          /* E: Should NOT execute */
    movge   r0, #3          /* E: Should NOT execute */
    str     r0, [r7, #24]   /* Result: 1 */

    /* Test 7: IT with arithmetic that sets flags */
    /* Use ADDS inside IT block */
    movs    r0, #0
    movs    r1, #100
    movs    r2, #50
    cmp     r1, r2          /* 100 > 50 */
    it      gt
    addgt   r0, r1, r2      /* Should execute: R0 = 150 */
    str     r0, [r7, #28]   /* Result: 150 */

    /* Done marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #32]

    bkpt    #0

_fault:
    b       _fault

    .end
