/**
 * test_load_store_misc.S - Test load/store variations
 *
 * Tests: LDRD, STRD, LDRSH, LDRSB, pre/post-indexed, PC-relative LDR
 *
 * Memory layout at 0x20000000:
 *   [0x00] = LDRD low word (expect 0xDEADBEEF)
 *   [0x04] = LDRD high word (expect 0xCAFEBABE)
 *   [0x08] = STRD low word (expect 0x12345678)
 *   [0x0C] = STRD high word (expect 0x9ABCDEF0)
 *   [0x10] = LDRSH positive (expect 0x00007FFF)
 *   [0x14] = LDRSH negative (expect 0xFFFF8000)
 *   [0x18] = LDRSB positive (expect 0x0000007F)
 *   [0x1C] = LDRSB negative (expect 0xFFFFFF80)
 *   [0x20] = Pre-indexed load result (expect 0xAABBCCDD)
 *   [0x24] = Post-indexed load result (expect 0x11223344)
 *   [0x28] = Pre-indexed store verify
 *   [0x2C] = Post-indexed store verify
 *   [0x30] = PC-relative LDR (expect 0xC0FFEE42)
 *   [0x34] = LDRH result (expect 0x0000BEEF)
 *   [0x38] = STRH verify low halfword
 *   [0x3C] = 0xC0FFEE42 (success marker)
 *
 * Test data area at 0x20000100
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

    /* Setup test data area */
    ldr     r6, =0x20000100

    /* Initialize test data */
    ldr     r0, =0xDEADBEEF
    str     r0, [r6, #0]
    ldr     r0, =0xCAFEBABE
    str     r0, [r6, #4]
    ldr     r0, =0x11223344
    str     r0, [r6, #8]
    ldr     r0, =0xAABBCCDD
    str     r0, [r6, #12]
    /* Store halfwords for signed load tests */
    ldr     r0, =0x80007FFF     /* low: 0x7FFF (positive), high: 0x8000 (negative) */
    str     r0, [r6, #16]
    /* Store bytes for signed load tests */
    ldr     r0, =0x0000807F     /* byte 0: 0x7F (positive), byte 1: 0x80 (negative) */
    str     r0, [r6, #20]

    /* Test 1-2: LDRD - Load double word */
    ldrd    r0, r1, [r6, #0]
    str     r0, [r7, #0x00]     /* expect 0xDEADBEEF */
    str     r1, [r7, #0x04]     /* expect 0xCAFEBABE */

    /* Test 3-4: STRD - Store double word */
    ldr     r0, =0x12345678
    ldr     r1, =0x9ABCDEF0
    strd    r0, r1, [r7, #0x08]
    /* Verify by loading back */
    ldr     r2, [r7, #0x08]
    ldr     r3, [r7, #0x0C]
    /* r2 should be 0x12345678, r3 should be 0x9ABCDEF0 */

    /* Test 5: LDRSH positive halfword */
    ldrsh   r0, [r6, #16]       /* Load 0x7FFF, sign-extend to 0x00007FFF */
    str     r0, [r7, #0x10]

    /* Test 6: LDRSH negative halfword */
    ldrsh   r0, [r6, #18]       /* Load 0x8000, sign-extend to 0xFFFF8000 */
    str     r0, [r7, #0x14]

    /* Test 7: LDRSB positive byte */
    ldrsb   r0, [r6, #20]       /* Load 0x7F, sign-extend to 0x0000007F */
    str     r0, [r7, #0x18]

    /* Test 8: LDRSB negative byte */
    ldrsb   r0, [r6, #21]       /* Load 0x80, sign-extend to 0xFFFFFF80 */
    str     r0, [r7, #0x1C]

    /* Test 9: Pre-indexed load (LDR with writeback)
     * r4 = base + 12, then load from r4
     */
    mov     r4, r6
    ldr     r0, [r4, #12]!      /* r4 += 12, then load */
    str     r0, [r7, #0x20]     /* expect 0xAABBCCDD */

    /* Test 10: Post-indexed load
     * Load from r5, then r5 = r5 + 4
     */
    add     r5, r6, #8          /* Point to 0x11223344 */
    ldr     r0, [r5], #4        /* Load, then r5 += 4 */
    str     r0, [r7, #0x24]     /* expect 0x11223344 */

    /* Test 11: Pre-indexed store */
    ldr     r0, =0x55667788
    add     r4, r7, #0x24       /* Point before target */
    str     r0, [r4, #4]!       /* r4 += 4, then store at 0x28 */
    /* Verify: [0x28] should be 0x55667788 */

    /* Test 12: Post-indexed store */
    ldr     r0, =0x99AABBCC
    mov     r4, r7
    add     r4, #0x2C           /* Point to 0x2C */
    str     r0, [r4], #4        /* Store at 0x2C, then r4 += 4 */
    /* Verify: [0x2C] should be 0x99AABBCC */

    /* Test 13: PC-relative LDR (literal pool) */
    ldr     r0, =0xC0FFEE42     /* Assembler generates PC-relative load */
    str     r0, [r7, #0x30]

    /* Test 14: LDRH (unsigned halfword load) */
    ldr     r0, =0xDEADBEEF
    str     r0, [r6, #24]
    ldrh    r0, [r6, #24]       /* Load low halfword 0xBEEF */
    str     r0, [r7, #0x34]     /* expect 0x0000BEEF */

    /* Test 15: STRH (store halfword) */
    ldr     r0, =0x12345678
    strh    r0, [r7, #0x38]     /* Store 0x5678 */
    ldr     r1, [r7, #0x38]     /* Load back full word */
    /* Low halfword should be 0x5678 */

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x3C]

    /* Done */
    bkpt    #0

    .thumb_func
_fault:
    b       _fault

    .end
