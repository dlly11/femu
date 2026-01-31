/**
 * test_system.S - Test system instructions
 *
 * Tests: MRS, MSR, barrier instructions, hints
 *
 * Memory layout at 0x20000000:
 *   [0x00] = Initial PRIMASK (expect 0)
 *   [0x04] = PRIMASK after set (expect 1)
 *   [0x08] = PRIMASK after clear (expect 0)
 *   [0x0C] = Initial BASEPRI (expect 0)
 *   [0x10] = BASEPRI after set (expect 0x40)
 *   [0x14] = CONTROL register (expect 0 in handler mode)
 *   [0x18] = PSP value (expect valid address)
 *   [0x1C] = APSR flags after operation
 *   [0x20] = NOP executed (always 1)
 *   [0x24] = DMB executed (always 1)
 *   [0x28] = DSB executed (always 1)
 *   [0x2C] = ISB executed (always 1)
 *   [0x30] = 0xC0FFEE42 (success marker)
 */

    .syntax unified
    .cpu cortex-m4
    .thumb

    .section .vectors, "a"
    .word   0x20001000          /* Initial SP */
    .word   _reset + 1          /* Reset vector */
    .word   _fault + 1          /* NMI */
    .word   _fault + 1          /* HardFault */

    .section .text
    .thumb_func
    .global _reset
_reset:
    /* Setup base pointer for results */
    mov     r7, #0x20000000

    /* ============================================ */
    /* Test 1-3: PRIMASK (interrupt mask)          */
    /* ============================================ */

    /* Test 1: Read initial PRIMASK (should be 0) */
    mrs     r0, primask
    str     r0, [r7, #0x00]     /* expect 0 */

    /* Test 2: Set PRIMASK (disable interrupts) */
    movs    r0, #1
    msr     primask, r0
    mrs     r0, primask
    str     r0, [r7, #0x04]     /* expect 1 */

    /* Test 3: Clear PRIMASK (enable interrupts) */
    movs    r0, #0
    msr     primask, r0
    mrs     r0, primask
    str     r0, [r7, #0x08]     /* expect 0 */

    /* ============================================ */
    /* Test 4-5: BASEPRI (priority mask)           */
    /* ============================================ */

    /* Test 4: Read initial BASEPRI (should be 0) */
    mrs     r0, basepri
    str     r0, [r7, #0x0C]     /* expect 0 */

    /* Test 5: Set BASEPRI to mask priorities >= 0x40 */
    movs    r0, #0x40
    msr     basepri, r0
    mrs     r0, basepri
    str     r0, [r7, #0x10]     /* expect 0x40 */

    /* Clear BASEPRI for further tests */
    movs    r0, #0
    msr     basepri, r0

    /* ============================================ */
    /* Test 6: CONTROL register                    */
    /* ============================================ */

    /* Read CONTROL (in Thread mode, should reflect default) */
    mrs     r0, control
    str     r0, [r7, #0x14]     /* expect 0 in handler mode (MSP, privileged) */

    /* ============================================ */
    /* Test 7: PSP (Process Stack Pointer)         */
    /* ============================================ */

    /* Set up PSP to a known value */
    ldr     r0, =0x20000F00
    msr     psp, r0

    /* Read back PSP */
    mrs     r0, psp
    str     r0, [r7, #0x18]     /* expect 0x20000F00 */

    /* ============================================ */
    /* Test 8: APSR (Application PSR) flags        */
    /* ============================================ */

    /* Do an operation that sets flags, then read APSR */
    movs    r0, #0
    subs    r0, r0, #1          /* 0 - 1 = -1, sets N and C flags */
    mrs     r1, apsr
    str     r1, [r7, #0x1C]     /* NZCV flags in bits 31-28 */

    /* ============================================ */
    /* Test 9-12: Hint and barrier instructions    */
    /* ============================================ */

    /* Test 9: NOP */
    nop
    movs    r0, #1
    str     r0, [r7, #0x20]     /* expect 1 */

    /* Test 10: DMB (Data Memory Barrier) */
    dmb     sy                  /* Full system barrier */
    movs    r0, #1
    str     r0, [r7, #0x24]     /* expect 1 */

    /* Test 11: DSB (Data Synchronization Barrier) */
    dsb     sy                  /* Full system barrier */
    movs    r0, #1
    str     r0, [r7, #0x28]     /* expect 1 */

    /* Test 12: ISB (Instruction Synchronization Barrier) */
    isb     sy                  /* Full system barrier */
    movs    r0, #1
    str     r0, [r7, #0x2C]     /* expect 1 */

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x30]

    /* Done */
    bkpt    #0

    .thumb_func
_fault:
    b       _fault

    .end
