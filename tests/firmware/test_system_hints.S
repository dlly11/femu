/**
 * test_system_hints.S - Test system hint instructions
 *
 * Tests: NOP, YIELD, WFE, WFI, SEV, ISB, DSB, DMB
 *        CLREX (also tested in test_exclusive.S)
 *
 * These instructions are hints that may or may not have visible effects
 * in an emulator. We test that they execute without faulting and that
 * execution continues properly.
 *
 * Note: In a real system:
 *   - WFI/WFE would wait for interrupt/event
 *   - SEV would signal an event to other cores
 *   - ISB flushes pipeline
 *   - DSB/DMB are memory barriers
 *
 * In an emulator, these typically act as NOPs but must decode correctly.
 *
 * Memory layout at 0x20000000:
 *   [0x00] = Counter after NOP sequence (expect 3)
 *   [0x04] = Counter after YIELD (expect 4)
 *   [0x08] = Counter after SEV (expect 5)
 *   [0x0C] = Counter after ISB (expect 6)
 *   [0x10] = Counter after DSB (expect 7)
 *   [0x14] = Counter after DMB (expect 8)
 *   [0x18] = Counter after NOP.W (expect 9)
 *   [0x1C] = 0xC0FFEE42 (success marker)
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

    /* Use r6 as a counter to verify execution continues */
    movs    r6, #0

    /* ============================================ */
    /* Test 1: NOP - No operation (16-bit)         */
    /* Execute 3 NOPs and increment counter        */
    /* ============================================ */
    nop
    adds    r6, r6, #1          /* counter = 1 */
    nop
    adds    r6, r6, #1          /* counter = 2 */
    nop
    adds    r6, r6, #1          /* counter = 3 */
    str     r6, [r7, #0x00]     /* expect 3 */

    /* ============================================ */
    /* Test 2: YIELD - Hint to switch threads      */
    /* (Acts as NOP on single-threaded systems)    */
    /* ============================================ */
    yield
    adds    r6, r6, #1          /* counter = 4 */
    str     r6, [r7, #0x04]     /* expect 4 */

    /* ============================================ */
    /* Test 3: SEV - Send Event                    */
    /* (Signals event to other processors)         */
    /* ============================================ */
    sev
    adds    r6, r6, #1          /* counter = 5 */
    str     r6, [r7, #0x08]     /* expect 5 */

    /* ============================================ */
    /* Test 4: ISB - Instruction Synchronization   */
    /* Barrier (flushes pipeline)                  */
    /* ============================================ */
    isb     sy                  /* Full system ISB */
    adds    r6, r6, #1          /* counter = 6 */
    str     r6, [r7, #0x0C]     /* expect 6 */

    /* ============================================ */
    /* Test 5: DSB - Data Synchronization Barrier  */
    /* (Ensures all memory accesses complete)      */
    /* ============================================ */
    dsb     sy                  /* Full system DSB */
    adds    r6, r6, #1          /* counter = 7 */
    str     r6, [r7, #0x10]     /* expect 7 */

    /* ============================================ */
    /* Test 6: DMB - Data Memory Barrier           */
    /* (Ensures memory ordering)                   */
    /* ============================================ */
    dmb     sy                  /* Full system DMB */
    adds    r6, r6, #1          /* counter = 8 */
    str     r6, [r7, #0x14]     /* expect 8 */

    /* ============================================ */
    /* Test 7: NOP.W - Wide NOP (32-bit encoding)  */
    /* ============================================ */
    nop.w
    adds    r6, r6, #1          /* counter = 9 */
    str     r6, [r7, #0x18]     /* expect 9 */

    /* ============================================ */
    /* Note: WFI and WFE are not tested here       */
    /* because they would halt execution waiting   */
    /* for an interrupt or event. In an emulator   */
    /* without interrupt generation, this would    */
    /* hang forever.                               */
    /* ============================================ */

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x1C]

    /* Done */
    bkpt    #0

    .thumb_func
_fault:
    b       _fault

    .end
