/**
 * test_pack.S - Test pack halfword instructions
 *
 * Tests: PKHBT, PKHTB
 *
 * PKHBT: Pack bottom halfword from Rn, top halfword from Rm (optionally shifted)
 * PKHTB: Pack top halfword from Rn, bottom halfword from Rm (optionally shifted)
 *
 * Memory layout at 0x20000000:
 *   [0x00] = PKHBT basic
 *   [0x04] = PKHBT with LSL
 *   [0x08] = PKHTB basic
 *   [0x0C] = PKHTB with ASR
 *   [0x10] = PKHBT zero shift
 *   [0x14] = PKHBT LSL #16
 *   [0x18] = PKHTB ASR #16
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

    /* ============================================ */
    /* Test 1: PKHBT basic (no shift)              */
    /* PKHBT Rd, Rn, Rm: Rd[15:0] = Rn[15:0]       */
    /*                   Rd[31:16] = Rm[15:0]       */
    /* r0 = 0x1111AAAA, r1 = 0x2222BBBB            */
    /* Result: 0xBBBBAAAA (lo from r0, hi from lo of r1) */
    /* ============================================ */
    ldr     r0, =0x1111AAAA
    ldr     r1, =0x2222BBBB
    pkhbt   r2, r0, r1
    str     r2, [r7, #0x00]     /* expect 0xBBBBAAAA */

    /* ============================================ */
    /* Test 2: PKHBT with LSL #8                   */
    /* Rm is shifted left before extracting        */
    /* r1 shifted: 0x22BBBB00                      */
    /* Result: 0xBB00AAAA                          */
    /* ============================================ */
    ldr     r0, =0x1111AAAA
    ldr     r1, =0x2222BBBB
    pkhbt   r2, r0, r1, lsl #8
    str     r2, [r7, #0x04]     /* expect 0xBB00AAAA */

    /* ============================================ */
    /* Test 3: PKHTB basic (ASR #0 encoded as 32)  */
    /* PKHTB Rd, Rn, Rm: Rd[31:16] = Rn[31:16]     */
    /*                   Rd[15:0] = Rm[31:16]       */
    /* Note: PKHTB requires shift, ASR #0 = ASR #32 */
    /* For ASR #32, bottom half comes from sign bit */
    /* Use explicit ASR to get useful result        */
    /* ============================================ */
    ldr     r0, =0xAAAA1111
    ldr     r1, =0xBBBB2222
    pkhtb   r2, r0, r1, asr #16
    str     r2, [r7, #0x08]     /* expect 0xAAAABBBB (hi from r0, lo from hi of r1) */

    /* ============================================ */
    /* Test 4: PKHTB with ASR #8                   */
    /* r1 shifted: 0xFFBBBB22 (sign extended)      */
    /* No wait, 0xBBBB2222 >> 8 = 0x00BBBB22       */
    /* Result: Rd[31:16] = 0xAAAA, Rd[15:0] = 0xBB22 */
    /* ============================================ */
    ldr     r0, =0xAAAA1111
    ldr     r1, =0xBBBB2222
    pkhtb   r2, r0, r1, asr #8
    str     r2, [r7, #0x0C]     /* expect 0xAAAABB22 */

    /* ============================================ */
    /* Test 5: PKHBT with no shift (same as test 1)*/
    /* Verify encoding with explicit lsl #0        */
    /* ============================================ */
    ldr     r0, =0x12345678
    ldr     r1, =0xABCDEF01
    pkhbt   r2, r0, r1, lsl #0
    str     r2, [r7, #0x10]     /* expect 0xEF015678 */

    /* ============================================ */
    /* Test 6: PKHBT with LSL #16                  */
    /* r1 shifted: 0xBBBB0000                      */
    /* Result: 0x0000AAAA                          */
    /* Wait no - Rd[31:16] = (Rm << 16)[15:0] = 0x0000? */
    /* Actually Rm << 16 = 0xBBBB0000, bottom half = 0x0000 */
    /* Rd[31:16] = 0x0000 from shifted value        */
    /* ============================================ */
    ldr     r0, =0x1111AAAA
    ldr     r1, =0x2222BBBB
    pkhbt   r2, r0, r1, lsl #16
    str     r2, [r7, #0x14]     /* expect 0x0000AAAA */

    /* ============================================ */
    /* Test 7: PKHTB with ASR #16                  */
    /* r1 = 0xBBBB2222, ASR #16 = 0xFFFFBBBB (sign extended) */
    /* Wait, 0xBBBB is negative as signed halfword  */
    /* ASR #16: 0xBBBB2222 -> 0xFFFFBBBB            */
    /* Rd[31:16] = 0xAAAA, Rd[15:0] = 0xBBBB        */
    /* ============================================ */
    ldr     r0, =0xAAAA1111
    ldr     r1, =0xBBBB2222
    pkhtb   r2, r0, r1, asr #16
    str     r2, [r7, #0x18]     /* expect 0xAAAABBBB */

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x1C]

    /* Done */
    bkpt    #0

    .thumb_func
_fault:
    b       _fault

    .end
