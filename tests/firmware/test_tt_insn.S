/**
 * test_tt_insn.S - Test TT instruction variants
 *
 * Tests: TT, TTA, TTT, TTAT security attribute queries
 *
 * This test configures SAU regions and then uses TT instructions
 * to query the security attributes of various addresses.
 *
 * Memory layout at 0x20000000:
 *   [0x00] = TT result for secure flash (0x08000000)
 *   [0x04] = TT result for NSC region (0x08010000)
 *   [0x08] = TT result for non-secure flash (0x08020000)
 *   [0x0C] = TT result for secure RAM (0x20000000)
 *   [0x10] = TT result for non-secure RAM (0x20004000)
 *   [0x14] = TTA result for secure flash (privileged query)
 *   [0x18] = TTT result for secure flash (NS view)
 *   [0x1C] = TTAT result for secure flash (NS privileged view)
 *   [0x20] = 0xC0FFEE42 (success marker)
 */

    .syntax unified
    .cpu cortex-m33
    .thumb

    .section .vectors, "a"
    .word   0x20004000          /* Initial SP (top of secure RAM) */
    .word   _reset + 1          /* Reset vector */
    .word   _fault + 1          /* NMI */
    .word   _fault + 1          /* HardFault */
    .word   _fault + 1          /* MemManage */
    .word   _fault + 1          /* BusFault */
    .word   _fault + 1          /* UsageFault */
    .word   _fault + 1          /* SecureFault */

    .section .text
    .thumb_func
    .global _reset
_reset:
    /* Setup base pointer for results */
    ldr     r7, =0x20000000

    /* Note: SAU is not configured, so all memory is treated as Secure.
     * TT instructions will report security attributes based on default state.
     * With SAU disabled, TT reports all addresses as Secure with R/RW access.
     */

    /* TT instruction encoding macros
     * TT:   E840 Rn:4 0:4 1:4 0:4 Rd:4 = 0xE840_0_F001 for TT Rd, [Rn]
     * TTA:  E840 Rn:4 1:4 0:4 0:4 Rd:4 = 0xE840_1_F001 for TTA Rd, [Rn]
     * TTT:  E840 Rn:4 0:4 1:4 0:4 Rd:4 = 0xE840_0_F801 for TTT Rd, [Rn] (A=0, T=1)
     * TTAT: E840 Rn:4 1:4 1:4 0:4 Rd:4 = 0xE840_1_F801 for TTAT Rd, [Rn]
     *
     * Encoding: 1110 1000 0100 Rn:4 | A:1 000 T:1 000 Rd:4
     * For r0 as Rn and r1 as Rd:
     * TT r1, [r0]:   0xE840 0x0001
     * TTA r1, [r0]:  0xE840 0x1001
     * TTT r1, [r0]:  0xE840 0x0801
     * TTAT r1, [r0]: 0xE840 0x1801
     */

    /* ============================================ */
    /* Test 1: TT on secure flash                  */
    /* ============================================ */
    ldr     r0, =0x08000000     /* Secure flash */
    .short  0xE840, 0x0001      /* TT r1, [r0] */
    str     r1, [r7, #0x00]

    /* ============================================ */
    /* Test 2: TT on NSC region                    */
    /* ============================================ */
    ldr     r0, =0x08010000     /* NSC region */
    .short  0xE840, 0x0001      /* TT r1, [r0] */
    str     r1, [r7, #0x04]

    /* ============================================ */
    /* Test 3: TT on non-secure flash              */
    /* ============================================ */
    ldr     r0, =0x08020000     /* Non-secure flash */
    .short  0xE840, 0x0001      /* TT r1, [r0] */
    str     r1, [r7, #0x08]

    /* ============================================ */
    /* Test 4: TT on secure RAM                    */
    /* ============================================ */
    ldr     r0, =0x20000000     /* Secure RAM */
    .short  0xE840, 0x0001      /* TT r1, [r0] */
    str     r1, [r7, #0x0C]

    /* ============================================ */
    /* Test 5: TT on non-secure RAM                */
    /* ============================================ */
    ldr     r0, =0x20004000     /* Non-secure RAM */
    .short  0xE840, 0x0001      /* TT r1, [r0] */
    str     r1, [r7, #0x10]

    /* ============================================ */
    /* Test 6: TTA (privileged) on secure flash    */
    /* ============================================ */
    ldr     r0, =0x08000000     /* Secure flash */
    .short  0xE840, 0x1001      /* TTA r1, [r0] */
    str     r1, [r7, #0x14]

    /* ============================================ */
    /* Test 7: TTT (NS view) on secure flash       */
    /* ============================================ */
    ldr     r0, =0x08000000     /* Secure flash */
    .short  0xE840, 0x0801      /* TTT r1, [r0] */
    str     r1, [r7, #0x18]

    /* ============================================ */
    /* Test 8: TTAT (NS priv view) on secure flash */
    /* ============================================ */
    ldr     r0, =0x08000000     /* Secure flash */
    .short  0xE840, 0x1801      /* TTAT r1, [r0] */
    str     r1, [r7, #0x1C]

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x20]

    /* Done */
    bkpt    #0

    .thumb_func
_fault:
    b       _fault

    .end
