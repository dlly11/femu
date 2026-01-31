/**
 * test_dsp_exchange.S - Test DSP exchange add/subtract instructions
 *
 * Tests: SASX, SSAX, UASX, USAX (exchange add/subtract)
 *        SHASX, SHSAX, UHASX, UHSAX (halving exchange)
 *        QASX, QSAX, UQASX, UQSAX (saturating exchange)
 *
 * These instructions add one halfword and subtract the other:
 * SASX: Rd[31:16] = Rn[31:16] + Rm[15:0], Rd[15:0] = Rn[15:0] - Rm[31:16]
 * SSAX: Rd[31:16] = Rn[31:16] - Rm[15:0], Rd[15:0] = Rn[15:0] + Rm[31:16]
 *
 * Memory layout at 0x20000000:
 *   [0x00] = SASX result
 *   [0x04] = SSAX result
 *   [0x08] = UASX result
 *   [0x0C] = USAX result
 *   [0x10] = SHASX result (halving)
 *   [0x14] = SHSAX result (halving)
 *   [0x18] = UHASX result (halving)
 *   [0x1C] = UHSAX result (halving)
 *   [0x20] = QASX result (saturating)
 *   [0x24] = QSAX result (saturating)
 *   [0x28] = UQASX result (saturating)
 *   [0x2C] = UQSAX result (saturating)
 *   [0x30] = SASX with negatives
 *   [0x34] = 0xC0FFEE42 (success marker)
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
    /* Test 1: SASX - Signed add-subtract exchange */
    /* Rd[31:16] = Rn[31:16] + Rm[15:0]            */
    /* Rd[15:0]  = Rn[15:0]  - Rm[31:16]           */
    /* r0 = 0x00200010 (hi=0x0020, lo=0x0010)      */
    /* r1 = 0x00050008 (hi=0x0005, lo=0x0008)      */
    /* Result hi = 0x0020 + 0x0008 = 0x0028        */
    /* Result lo = 0x0010 - 0x0005 = 0x000B        */
    /* Result = 0x0028000B                         */
    /* ============================================ */
    ldr     r0, =0x00200010
    ldr     r1, =0x00050008
    sasx    r2, r0, r1
    str     r2, [r7, #0x00]     /* expect 0x0028000B */

    /* ============================================ */
    /* Test 2: SSAX - Signed sub-add exchange      */
    /* Rd[31:16] = Rn[31:16] - Rm[15:0]            */
    /* Rd[15:0]  = Rn[15:0]  + Rm[31:16]           */
    /* r0 = 0x00200010 (hi=0x0020, lo=0x0010)      */
    /* r1 = 0x00050008 (hi=0x0005, lo=0x0008)      */
    /* Result hi = 0x0020 - 0x0008 = 0x0018        */
    /* Result lo = 0x0010 + 0x0005 = 0x0015        */
    /* Result = 0x00180015                         */
    /* ============================================ */
    ldr     r0, =0x00200010
    ldr     r1, =0x00050008
    ssax    r2, r0, r1
    str     r2, [r7, #0x04]     /* expect 0x00180015 */

    /* ============================================ */
    /* Test 3: UASX - Unsigned add-subtract exchange */
    /* Same operation as SASX but unsigned          */
    /* ============================================ */
    ldr     r0, =0x00200010
    ldr     r1, =0x00050008
    uasx    r2, r0, r1
    str     r2, [r7, #0x08]     /* expect 0x0028000B */

    /* ============================================ */
    /* Test 4: USAX - Unsigned sub-add exchange    */
    /* Same operation as SSAX but unsigned          */
    /* ============================================ */
    ldr     r0, =0x00200010
    ldr     r1, =0x00050008
    usax    r2, r0, r1
    str     r2, [r7, #0x0C]     /* expect 0x00180015 */

    /* ============================================ */
    /* Test 5: SHASX - Signed halving ASX          */
    /* Result divided by 2                          */
    /* r0 = 0x00400020 (hi=0x0040, lo=0x0020)      */
    /* r1 = 0x00100010 (hi=0x0010, lo=0x0010)      */
    /* hi = (0x0040 + 0x0010)/2 = 0x0028           */
    /* lo = (0x0020 - 0x0010)/2 = 0x0008           */
    /* Result = 0x00280008                         */
    /* ============================================ */
    ldr     r0, =0x00400020
    ldr     r1, =0x00100010
    shasx   r2, r0, r1
    str     r2, [r7, #0x10]     /* expect 0x00280008 */

    /* ============================================ */
    /* Test 6: SHSAX - Signed halving SAX          */
    /* hi = (0x0040 - 0x0010)/2 = 0x0018           */
    /* lo = (0x0020 + 0x0010)/2 = 0x0018           */
    /* Result = 0x00180018                         */
    /* ============================================ */
    ldr     r0, =0x00400020
    ldr     r1, =0x00100010
    shsax   r2, r0, r1
    str     r2, [r7, #0x14]     /* expect 0x00180018 */

    /* ============================================ */
    /* Test 7: UHASX - Unsigned halving ASX        */
    /* Same values as SHASX for comparison         */
    /* ============================================ */
    ldr     r0, =0x00400020
    ldr     r1, =0x00100010
    uhasx   r2, r0, r1
    str     r2, [r7, #0x18]     /* expect 0x00280008 */

    /* ============================================ */
    /* Test 8: UHSAX - Unsigned halving SAX        */
    /* Same values as SHSAX for comparison         */
    /* ============================================ */
    ldr     r0, =0x00400020
    ldr     r1, =0x00100010
    uhsax   r2, r0, r1
    str     r2, [r7, #0x1C]     /* expect 0x00180018 */

    /* ============================================ */
    /* Test 9: QASX - Saturating ASX               */
    /* Non-saturating case for baseline            */
    /* ============================================ */
    ldr     r0, =0x00200010
    ldr     r1, =0x00050008
    qasx    r2, r0, r1
    str     r2, [r7, #0x20]     /* expect 0x0028000B */

    /* ============================================ */
    /* Test 10: QSAX - Saturating SAX              */
    /* Non-saturating case for baseline            */
    /* ============================================ */
    ldr     r0, =0x00200010
    ldr     r1, =0x00050008
    qsax    r2, r0, r1
    str     r2, [r7, #0x24]     /* expect 0x00180015 */

    /* ============================================ */
    /* Test 11: UQASX - Unsigned saturating ASX    */
    /* Test underflow case: lo = 0x0010 - 0x0020   */
    /* Should saturate to 0x0000                   */
    /* hi = 0x0020 + 0x0010 = 0x0030               */
    /* ============================================ */
    ldr     r0, =0x00200010
    ldr     r1, =0x00200010     /* hi=0x0020, lo=0x0010 */
    uqasx   r2, r0, r1
    str     r2, [r7, #0x28]     /* expect 0x00300000 (lo saturated) */

    /* ============================================ */
    /* Test 12: UQSAX - Unsigned saturating SAX    */
    /* Test underflow case: hi = 0x0020 - 0x0030   */
    /* Should saturate to 0x0000                   */
    /* lo = 0x0010 + 0x0020 = 0x0030               */
    /* ============================================ */
    ldr     r0, =0x00200010
    ldr     r1, =0x00200030     /* hi=0x0020, lo=0x0030 */
    uqsax   r2, r0, r1
    str     r2, [r7, #0x2C]     /* expect 0x00000030 (hi saturated) */

    /* ============================================ */
    /* Test 13: SASX with negative result          */
    /* r0 = 0x00100020 (hi=0x0010, lo=0x0020)      */
    /* r1 = 0x00300010 (hi=0x0030, lo=0x0010)      */
    /* hi = 0x0010 + 0x0010 = 0x0020               */
    /* lo = 0x0020 - 0x0030 = 0xFFF0 (-16 signed)  */
    /* Result = 0x0020FFF0                         */
    /* ============================================ */
    ldr     r0, =0x00100020
    ldr     r1, =0x00300010
    sasx    r2, r0, r1
    str     r2, [r7, #0x30]     /* expect 0x0020FFF0 */

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x34]

    /* Done */
    bkpt    #0

    .thumb_func
_fault:
    b       _fault

    .end
