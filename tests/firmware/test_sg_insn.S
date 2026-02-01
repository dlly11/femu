/**
 * test_sg_insn.S - Test SG (Secure Gateway) instruction
 *
 * Tests: SG instruction execution
 *
 * The SG instruction is used for secure gateway entry points.
 * When executed in Secure state, SG clears bit 0 of LR.
 * This test verifies SG executes without faulting.
 *
 * Memory layout at 0x20000000:
 *   [0x00] = Initial LR value (with bit 0 set)
 *   [0x04] = LR value after SG (bit 0 cleared)
 *   [0x08] = Function call returned successfully
 *   [0x0C] = Return value from function
 *   [0x10] = 0xC0FFEE42 (success marker)
 */

    .syntax unified
    .cpu cortex-m33
    .thumb

    /* ============================================ */
    /* Secure vector table                         */
    /* ============================================ */
    .section .vectors, "a"
    .word   0x20004000          /* Initial SP (top of secure RAM) */
    .word   _reset + 1          /* Reset vector */
    .word   _fault + 1          /* NMI */
    .word   _fault + 1          /* HardFault */
    .word   _fault + 1          /* MemManage */
    .word   _fault + 1          /* BusFault */
    .word   _fault + 1          /* UsageFault */
    .word   _fault + 1          /* SecureFault */

    /* ============================================ */
    /* Secure code section                         */
    /* ============================================ */
    .section .text
    .thumb_func
    .global _reset
_reset:
    /* Setup base pointer for results */
    ldr     r7, =0x20000000

    /* Store initial state indicator (running in secure) */
    mov     r0, #1
    str     r0, [r7, #0x00]

    /* ============================================ */
    /* Test: Execute SG instruction                */
    /* SG clears bit 0 of LR in Secure state       */
    /* ============================================ */

    /* Set up LR with bit 0 set (simulating a call) */
    ldr     lr, =0xDEADBEEF

    /* Store LR before SG */
    str     lr, [r7, #0x00]

    /* Execute SG - this should clear bit 0 of LR */
    sg

    /* Store LR after SG */
    str     lr, [r7, #0x04]

    /* ============================================ */
    /* Test: Call function via NSC-style entry     */
    /* ============================================ */

    /* Call the function that contains SG */
    bl      sg_entry

    /* Mark that we returned */
    mov     r1, #1
    str     r1, [r7, #0x08]

    /* Store return value */
    str     r0, [r7, #0x0C]

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x10]

    /* Done */
    bkpt    #0

    .thumb_func
_fault:
    b       _fault

    /* ============================================ */
    /* Function with SG entry (simulates NSC)      */
    /* ============================================ */
    .align 2
    .thumb_func
sg_entry:
    /* SG at entry point */
    sg

    /* Do some work */
    ldr     r0, =0xDEADC0DE

    /* Return */
    bx      lr

    .end
