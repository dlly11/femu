/**
 * test_acquire_release.S - Test load-acquire/store-release instructions
 *
 * Tests: LDA, LDAB, LDAH, STL, STLB, STLH, LDAEX, STLEX
 *
 * These ARMv8-M instructions provide memory ordering guarantees for
 * multi-threaded synchronization.
 *
 * Memory layout at 0x20000000:
 *   [0x00] = LDA result (word)
 *   [0x04] = STL stored value (word)
 *   [0x08] = LDAB result (byte)
 *   [0x0C] = STLB stored value (byte)
 *   [0x10] = LDAH result (halfword)
 *   [0x14] = STLH stored value (halfword)
 *   [0x18] = LDAEX result
 *   [0x1C] = STLEX success (0=success)
 *   [0x20] = Value after STLEX
 *   [0x24] = LDAEXB result
 *   [0x28] = STLEXB success
 *   [0x2C] = LDAEXH result
 *   [0x30] = STLEXH success
 *   [0x34] = 0xC0FFEE42 (success marker)
 *
 * Test data area at 0x20000100:
 *   [0x100] = Word test value 0x12345678
 *   [0x104] = Byte test value 0xAB
 *   [0x108] = Halfword test value 0xCDEF
 *   [0x10C] = Exclusive test value 0xDEADBEEF
 */

    .syntax unified
    .cpu cortex-m33
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
    /* Setup base pointers */
    mov     r7, #0x20000000     /* Results area */

    /* Setup test data area at 0x20000100 */
    add     r6, r7, #0x100
    ldr     r0, =0x12345678
    str     r0, [r6, #0x00]     /* Word test value */
    movs    r0, #0xAB
    strb    r0, [r6, #0x04]     /* Byte test value */
    ldr     r0, =0x0000CDEF
    strh    r0, [r6, #0x08]     /* Halfword test value */
    ldr     r0, =0xDEADBEEF
    str     r0, [r6, #0x0C]     /* Exclusive test value */

    /* ============================================ */
    /* Test 1: LDA - Load-Acquire (word)           */
    /* ============================================ */
    add     r0, r6, #0x00       /* Address of word test value */
    lda     r1, [r0]
    str     r1, [r7, #0x00]     /* expect 0x12345678 */

    /* ============================================ */
    /* Test 2: STL - Store-Release (word)          */
    /* ============================================ */
    ldr     r1, =0x87654321
    add     r0, r7, #0x04
    stl     r1, [r0]            /* Store to results area */
    /* Verify by loading back */
    ldr     r2, [r7, #0x04]     /* expect 0x87654321 */

    /* ============================================ */
    /* Test 3: LDAB - Load-Acquire Byte            */
    /* ============================================ */
    add     r0, r6, #0x04       /* Address of byte test value */
    ldab    r1, [r0]
    str     r1, [r7, #0x08]     /* expect 0x000000AB */

    /* ============================================ */
    /* Test 4: STLB - Store-Release Byte           */
    /* ============================================ */
    movs    r1, #0xCD
    add     r0, r7, #0x0C
    stlb    r1, [r0]
    /* Load back as word to verify (should be 0x000000CD) */
    ldr     r2, [r7, #0x0C]
    /* Store just the byte we care about */
    ldrb    r2, [r7, #0x0C]
    str     r2, [r7, #0x0C]     /* expect 0x000000CD */

    /* ============================================ */
    /* Test 5: LDAH - Load-Acquire Halfword        */
    /* ============================================ */
    add     r0, r6, #0x08       /* Address of halfword test value */
    ldah    r1, [r0]
    str     r1, [r7, #0x10]     /* expect 0x0000CDEF */

    /* ============================================ */
    /* Test 6: STLH - Store-Release Halfword       */
    /* ============================================ */
    ldr     r1, =0x0000ABCD
    add     r0, r7, #0x14
    stlh    r1, [r0]
    /* Load back as halfword to verify */
    ldrh    r2, [r7, #0x14]
    str     r2, [r7, #0x14]     /* expect 0x0000ABCD */

    /* ============================================ */
    /* Test 7: LDAEX - Load-Acquire Exclusive      */
    /* ============================================ */
    add     r0, r6, #0x0C       /* Address of exclusive test value */
    ldaex   r1, [r0]
    str     r1, [r7, #0x18]     /* expect 0xDEADBEEF */

    /* ============================================ */
    /* Test 8: STLEX - Store-Release Exclusive     */
    /* Should succeed since LDAEX was just done    */
    /* ============================================ */
    ldr     r2, =0xCAFEBABE
    stlex   r3, r2, [r0]        /* r3 = 0 if success */
    str     r3, [r7, #0x1C]     /* expect 0 */
    /* Verify the stored value */
    ldr     r1, [r0]
    str     r1, [r7, #0x20]     /* expect 0xCAFEBABE */

    /* ============================================ */
    /* Test 9: LDAEXB - Load-Acquire Exclusive Byte */
    /* Setup a new byte test location              */
    /* ============================================ */
    movs    r1, #0x42
    add     r0, r6, #0x10
    strb    r1, [r0]            /* Store test byte */
    ldaexb  r2, [r0]
    str     r2, [r7, #0x24]     /* expect 0x00000042 */

    /* ============================================ */
    /* Test 10: STLEXB - Store-Release Excl Byte   */
    /* ============================================ */
    movs    r2, #0x55
    stlexb  r3, r2, [r0]
    str     r3, [r7, #0x28]     /* expect 0 (success) */

    /* ============================================ */
    /* Test 11: LDAEXH - Load-Acquire Excl Halfword */
    /* ============================================ */
    ldr     r1, =0x1234
    add     r0, r6, #0x14
    strh    r1, [r0]            /* Store test halfword */
    ldaexh  r2, [r0]
    str     r2, [r7, #0x2C]     /* expect 0x00001234 */

    /* ============================================ */
    /* Test 12: STLEXH - Store-Release Excl Halfword */
    /* ============================================ */
    ldr     r2, =0x5678
    stlexh  r3, r2, [r0]
    str     r3, [r7, #0x30]     /* expect 0 (success) */

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x34]

    /* Done */
    bkpt    #0

    .thumb_func
_fault:
    b       _fault

    .end
