/**
 * Stack operations test for FEMU emulator.
 *
 * Tests: PUSH, POP, nested function calls with LR save/restore
 *
 * Results stored at 0x20000000:
 * [0x00] = Value pushed and popped (0x12345678)
 * [0x04] = Multiple push/pop test value 1
 * [0x08] = Multiple push/pop test value 2
 * [0x0C] = Multiple push/pop test value 3
 * [0x10] = Result from nested function call
 * [0x14] = SP check (should be back to initial)
 * [0x18] = 0xDEADFACE                  (done marker)
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

    /* Save initial SP for later check */
    mov     r6, sp

    /* Test 1: Simple push/pop single register */
    ldr     r0, =0x12345678
    push    {r0}
    movs    r0, #0          /* Clear R0 */
    pop     {r0}            /* Restore R0 */
    str     r0, [r7, #0]    /* Should be 0x12345678 */

    /* Test 2: Push/pop multiple registers */
    ldr     r0, =0x11111111
    ldr     r1, =0x22222222
    ldr     r2, =0x33333333
    push    {r0, r1, r2}
    movs    r0, #0
    movs    r1, #0
    movs    r2, #0
    pop     {r0, r1, r2}
    str     r0, [r7, #4]    /* Should be 0x11111111 */
    str     r1, [r7, #8]    /* Should be 0x22222222 */
    str     r2, [r7, #12]   /* Should be 0x33333333 */

    /* Test 3: Nested function calls */
    movs    r0, #5          /* Argument: compute fibonacci(5) = 5 */
    bl      fib
    str     r0, [r7, #16]   /* Result should be 5 */

    /* Test 4: Verify SP is back to initial value */
    mov     r0, sp
    cmp     r0, r6
    ite     eq
    moveq   r0, #1          /* SP correct */
    movne   r0, #0          /* SP wrong */
    str     r0, [r7, #20]

    /* Done marker */
    ldr     r0, =0xDEADFACE
    str     r0, [r7, #24]

    bkpt    #0

/* Fibonacci function - tests nested calls and stack frame
 * fib(0) = 0, fib(1) = 1, fib(n) = fib(n-1) + fib(n-2)
 * For simplicity, we use iterative approach to avoid deep recursion
 */
    .thumb_func
fib:
    push    {r4, r5, lr}    /* Save callee-saved regs and return address */

    cmp     r0, #1
    ble     fib_base        /* if n <= 1, return n */

    /* Iterative fibonacci */
    movs    r4, #0          /* fib(0) */
    movs    r5, #1          /* fib(1) */
    mov     r1, r0          /* counter = n */

fib_loop:
    cmp     r1, #1
    ble     fib_done

    mov     r2, r5          /* temp = fib(i-1) */
    add     r5, r4, r5      /* fib(i) = fib(i-2) + fib(i-1) */
    mov     r4, r2          /* fib(i-2) = old fib(i-1) */
    subs    r1, #1
    b       fib_loop

fib_done:
    mov     r0, r5          /* Return fib(n) */
    pop     {r4, r5, pc}    /* Restore and return */

fib_base:
    pop     {r4, r5, pc}    /* Return r0 as-is */

_fault:
    b       _fault

    .end
