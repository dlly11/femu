/**
 * Bit manipulation test for FEMU emulator.
 *
 * Tests: CLZ, RBIT, REV, REV16, REVSH, BFC, BFI, UBFX, SBFX
 *
 * Results stored at 0x20000000:
 * [0x00] = CLZ(0x00010000) = 15       (count leading zeros)
 * [0x04] = CLZ(0) = 32                (all zeros)
 * [0x08] = RBIT(0x80000001)           (reverse bits)
 * [0x0C] = REV(0x12345678)            (reverse bytes)
 * [0x10] = REV16(0x12345678)          (reverse bytes in halfwords)
 * [0x14] = REVSH(0x00000080)          (reverse signed halfword)
 * [0x18] = BFC result                 (bit field clear)
 * [0x1C] = BFI result                 (bit field insert)
 * [0x20] = UBFX result                (unsigned bit field extract)
 * [0x24] = SBFX result                (signed bit field extract)
 * [0x28] = 0xBEEFCAFE                 (done marker)
 */

    .syntax unified
    .cpu cortex-m4
    .thumb

    .section .vectors, "a"
    .align 2
_vectors:
    .word   _stack_top
    .word   _reset + 1
    .word   _fault + 1      /* NMI */
    .word   _fault + 1      /* HardFault */
    .space  44              /* Rest of vector table */

    .section .text
    .align 2
    .thumb_func
    .global _reset
_reset:
    /* R7 = result pointer */
    ldr     r7, =0x20000000

    /* CLZ: Count leading zeros of 0x00010000 = 15 */
    mov     r0, #0x00010000
    clz     r0, r0
    str     r0, [r7, #0]

    /* CLZ: Count leading zeros of 0 = 32 */
    movs    r0, #0
    clz     r0, r0
    str     r0, [r7, #4]

    /* RBIT: Reverse bits of 0x80000001 -> 0x80000001 */
    ldr     r0, =0x80000001
    rbit    r0, r0
    str     r0, [r7, #8]

    /* REV: Reverse bytes of 0x12345678 -> 0x78563412 */
    ldr     r0, =0x12345678
    rev     r0, r0
    str     r0, [r7, #12]

    /* REV16: Reverse bytes in each halfword: 0x12345678 -> 0x34127856 */
    ldr     r0, =0x12345678
    rev16   r0, r0
    str     r0, [r7, #16]

    /* REVSH: Reverse bytes of low halfword, sign extend */
    /* 0x00000080 -> 0x00008000 -> sign extend -> 0xFFFF8000 */
    movs    r0, #0x80
    revsh   r0, r0
    str     r0, [r7, #20]

    /* BFC: Bit field clear - clear bits 8-15 of 0xFFFFFFFF */
    mov     r0, #-1         /* 0xFFFFFFFF */
    bfc     r0, #8, #8      /* Clear 8 bits starting at bit 8 */
    str     r0, [r7, #24]   /* Result: 0xFFFF00FF */

    /* BFI: Bit field insert - insert 0xAB into bits 16-23 */
    mov     r0, #0          /* Destination */
    mov     r1, #0xAB       /* Source value */
    bfi     r0, r1, #16, #8 /* Insert 8 bits at position 16 */
    str     r0, [r7, #28]   /* Result: 0x00AB0000 */

    /* UBFX: Unsigned bit field extract */
    /* Extract bits 8-15 from 0x0000FF00 */
    mov     r0, #0xFF00
    ubfx    r0, r0, #8, #8  /* Extract 8 bits from position 8 */
    str     r0, [r7, #32]   /* Result: 0x000000FF */

    /* SBFX: Signed bit field extract */
    /* Extract bits 4-7 from 0x000000F0 (value 0xF = -1 as 4-bit signed) */
    movs    r0, #0xF0
    sbfx    r0, r0, #4, #4  /* Extract 4 bits from position 4, sign extend */
    str     r0, [r7, #36]   /* Result: 0xFFFFFFFF (-1) */

    /* Done marker */
    ldr     r0, =0xBEEFCAFE
    str     r0, [r7, #40]

    bkpt    #0

_fault:
    b       _fault

    .end
