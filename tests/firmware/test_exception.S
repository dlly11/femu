/**
 * test_exception.S - Test exception handling
 *
 * Tests: SVC, exception entry/return, stack frame
 *
 * Memory layout at 0x20000000:
 *   [0x00] = SVC handler executed (expect 1)
 *   [0x04] = SVC number extracted (expect 42)
 *   [0x08] = Return value from SVC (expect 0x1234)
 *   [0x0C] = R0 from stack frame
 *   [0x10] = R1 from stack frame
 *   [0x14] = R2 from stack frame
 *   [0x18] = R3 from stack frame
 *   [0x1C] = R12 from stack frame
 *   [0x20] = LR from stack frame
 *   [0x24] = Return PC from stack frame
 *   [0x28] = xPSR from stack frame
 *   [0x2C] = Second SVC executed (expect 1)
 *   [0x30] = Second SVC number (expect 7)
 *   [0x34] = EXC_RETURN value (expect 0xFFFFFFF9 or similar)
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
    .word   _fault + 1          /* MemManage */
    .word   _fault + 1          /* BusFault */
    .word   _fault + 1          /* UsageFault */
    .word   0                   /* Reserved */
    .word   0                   /* Reserved */
    .word   0                   /* Reserved */
    .word   0                   /* Reserved */
    .word   _svc_handler + 1    /* SVCall */
    .word   _fault + 1          /* Debug Monitor */
    .word   0                   /* Reserved */
    .word   _fault + 1          /* PendSV */
    .word   _fault + 1          /* SysTick */

    .section .text
    .thumb_func
    .global _reset
_reset:
    /* Setup base pointer for results */
    mov     r7, #0x20000000

    /* Clear result area */
    movs    r0, #0
    movs    r1, #0
    movs    r2, #15
clear_loop:
    str     r0, [r7, r1]
    adds    r1, r1, #4
    subs    r2, r2, #1
    bne     clear_loop

    /* Setup known register values before SVC */
    movs    r0, #0x10           /* Will be in stack frame */
    movs    r1, #0x11
    movs    r2, #0x12
    movs    r3, #0x13
    mov     r12, #0x1C

    /* Test 1: Trigger SVC #42 */
    svc     #42

    /* After return from SVC, r0 should have return value */
    str     r0, [r7, #0x08]     /* expect 0x1234 (set by handler) */

    /* Test 2: Trigger another SVC #7 */
    svc     #7

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x38]

    /* Done */
    bkpt    #0

    /* ============================================ */
    /* SVC Handler                                  */
    /* ============================================ */
    .thumb_func
    .global _svc_handler
_svc_handler:
    /* Save base pointer */
    mov     r7, #0x20000000

    /* On exception entry, hardware pushes:
     * [SP+0x00] = R0
     * [SP+0x04] = R1
     * [SP+0x08] = R2
     * [SP+0x0C] = R3
     * [SP+0x10] = R12
     * [SP+0x14] = LR (return address before SVC)
     * [SP+0x18] = Return PC (address of instruction after SVC)
     * [SP+0x1C] = xPSR
     *
     * LR contains EXC_RETURN value
     */

    /* Get stack pointer used (MSP since we're in handler mode) */
    mrs     r0, msp

    /* Extract SVC number from SVC instruction
     * Return PC points to instruction after SVC
     * SVC instruction is 2 bytes before that
     * SVC number is in the low byte of the SVC instruction
     */
    ldr     r1, [r0, #0x18]     /* Get return PC */
    subs    r1, r1, #2          /* Point to SVC instruction */
    ldrb    r2, [r1]            /* Get SVC number (low byte) */

    /* Check if this is first or second SVC */
    cmp     r2, #42
    beq     first_svc

    /* Second SVC (number 7) */
    movs    r1, #1
    str     r1, [r7, #0x2C]     /* Second SVC executed */
    str     r2, [r7, #0x30]     /* SVC number = 7 */

    /* Store EXC_RETURN value */
    str     lr, [r7, #0x34]

    /* Return from exception */
    bx      lr

first_svc:
    /* Mark handler executed */
    movs    r1, #1
    str     r1, [r7, #0x00]

    /* Store SVC number */
    str     r2, [r7, #0x04]     /* expect 42 */

    /* Examine and store stack frame */
    ldr     r1, [r0, #0x00]     /* R0 from stack */
    str     r1, [r7, #0x0C]

    ldr     r1, [r0, #0x04]     /* R1 from stack */
    str     r1, [r7, #0x10]

    ldr     r1, [r0, #0x08]     /* R2 from stack */
    str     r1, [r7, #0x14]

    ldr     r1, [r0, #0x0C]     /* R3 from stack */
    str     r1, [r7, #0x18]

    ldr     r1, [r0, #0x10]     /* R12 from stack */
    str     r1, [r7, #0x1C]

    ldr     r1, [r0, #0x14]     /* LR from stack */
    str     r1, [r7, #0x20]

    ldr     r1, [r0, #0x18]     /* Return PC from stack */
    str     r1, [r7, #0x24]

    ldr     r1, [r0, #0x1C]     /* xPSR from stack */
    str     r1, [r7, #0x28]

    /* Set return value in stacked R0 */
    ldr     r1, =0x1234
    str     r1, [r0, #0x00]     /* Modify stacked R0 */

    /* Return from exception */
    bx      lr

    .thumb_func
_fault:
    b       _fault

    .end
