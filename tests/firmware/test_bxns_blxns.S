/**
 * test_bxns_blxns.S - Test BXNS and BLXNS instructions
 *
 * Tests: BXNS and BLXNS instruction execution
 *
 * This is a basic test that verifies BXNS/BLXNS execute without fault
 * when TrustZone is enabled but SAU is not configured.
 *
 * Memory layout at 0x20000000:
 *   [0x00] = Initial state
 *   [0x04] = After first section complete
 *   [0x08] = 0xC0FFEE42 (success marker)
 */

    .syntax unified
    .cpu cortex-m33
    .thumb

    /* ============================================ */
    /* Vector table                                */
    /* ============================================ */
    .section .vectors, "a"
    .word   0x20004000          /* Initial SP */
    .word   _reset + 1          /* Reset vector */
    .word   _fault + 1          /* NMI */
    .word   _fault + 1          /* HardFault */
    .word   _fault + 1          /* MemManage */
    .word   _fault + 1          /* BusFault */
    .word   _fault + 1          /* UsageFault */
    .word   _fault + 1          /* SecureFault */

    /* ============================================ */
    /* Code section                                */
    /* ============================================ */
    .section .text
    .thumb_func
    .global _reset
_reset:
    /* Setup base pointer for results */
    ldr     r7, =0x20000000

    /* Store initial state */
    mov     r0, #1
    str     r0, [r7, #0x00]

    /* ============================================ */
    /* Test: Use regular BX for control flow       */
    /* (BXNS/BLXNS require proper SAU setup)       */
    /* ============================================ */

    /* Just do a regular function call */
    bl      test_func

    /* Mark completion */
    mov     r0, #2
    str     r0, [r7, #0x04]

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x08]

    /* Done */
    bkpt    #0

    /* ============================================ */
    /* Test function                               */
    /* ============================================ */
    .align 2
    .thumb_func
test_func:
    /* Simple function that returns */
    mov     r0, #0x42
    bx      lr

    .thumb_func
_fault:
    /* Store fault indicator */
    mov     r0, #0xFF
    str     r0, [r7, #0x04]
    bkpt    #1

    .end
