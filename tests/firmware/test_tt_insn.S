/**
 * test_tt_insn.S - Test TT/TTA/TTT/TTAT against a programmed SAU
 *
 * The firmware runs in Secure state (reset enters Secure when TrustZone is
 * present). It programs three SAU regions via the SAU MMIO registers
 * (0xE000EDD0+), enables the SAU, and then queries the security attributes of
 * Secure, NSC, and Non-secure addresses with the Test Target instructions.
 *
 * SAU programming:
 *   region 0: 0x08010000-0x08010FFF  Non-Secure Callable (NSC)
 *   region 1: 0x08020000-0x0802FFFF  Non-secure flash
 *   region 2: 0x20004000-0x20007FFF  Non-secure RAM
 *   (everything else stays Secure by default)
 *
 * Results stored at 0x20000000 (Secure RAM):
 *   [0x00] TT   secure flash   0x08000000
 *   [0x04] TT   NSC region     0x08010000
 *   [0x08] TT   non-secure fl  0x08020000
 *   [0x0C] TT   secure RAM     0x20000000
 *   [0x10] TT   non-secure RAM 0x20004000
 *   [0x14] TTA  secure flash   (Non-secure view -> no access)
 *   [0x18] TTT  secure flash   (Secure privileged view)
 *   [0x1C] TTAT secure flash   (Non-secure privileged view -> no access)
 *   [0x20] 0xC0FFEE42 success marker
 *
 * TT result format (per ARMv8-M): bit16=S, bits[15:8]=SREGION, bit7=SRVALID,
 * bit6=NSC, bit5=NS, bit1=RW, bit0=R.
 *
 * Real TT encoding (Rn=r0, Rd=r1):
 *   hw1 = 0xE840, hw2 = 0xF000 | (Rd<<8) | (A<<7) | (T<<6)
 *   TT=0xF100  TTT=0xF140(T)  TTA=0xF180(A)  TTAT=0xF1C0(A,T)
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
    /* ---- Program the SAU via its MMIO registers ---- */
    /* r4 = SAU_RNR, r5 = SAU_RBAR, r6 = SAU_RLAR */
    ldr     r4, =0xE000EDD8     /* SAU_RNR  */
    ldr     r5, =0xE000EDDC     /* SAU_RBAR */
    ldr     r6, =0xE000EDE0     /* SAU_RLAR */

    /* region 0: NSC 0x08010000..0x08010FFF (limit masked | EN | NSC) */
    movs    r3, #0
    str     r3, [r4]
    ldr     r3, =0x08010000
    str     r3, [r5]
    ldr     r3, =0x08010FE3     /* (0x08010FFF & ~0x1F) | EN(1) | NSC(2) */
    str     r3, [r6]

    /* region 1: Non-secure flash 0x08020000..0x0802FFFF */
    movs    r3, #1
    str     r3, [r4]
    ldr     r3, =0x08020000
    str     r3, [r5]
    ldr     r3, =0x0802FFE1     /* (0x0802FFFF & ~0x1F) | EN */
    str     r3, [r6]

    /* region 2: Non-secure RAM 0x20004000..0x20007FFF */
    movs    r3, #2
    str     r3, [r4]
    ldr     r3, =0x20004000
    str     r3, [r5]
    ldr     r3, =0x20007FE1     /* (0x20007FFF & ~0x1F) | EN */
    str     r3, [r6]

    /* Enable the SAU */
    ldr     r2, =0xE000EDD0     /* SAU_CTRL */
    movs    r3, #1              /* ENABLE */
    str     r3, [r2]

    /* ---- Query security attributes with TT instructions ---- */
    ldr     r7, =0x20000000     /* result base */

    /* TT secure flash */
    ldr     r0, =0x08000000
    .short  0xE840, 0xF100      /* TT r1, [r0] */
    str     r1, [r7, #0x00]

    /* TT NSC region */
    ldr     r0, =0x08010000
    .short  0xE840, 0xF100
    str     r1, [r7, #0x04]

    /* TT non-secure flash */
    ldr     r0, =0x08020000
    .short  0xE840, 0xF100
    str     r1, [r7, #0x08]

    /* TT secure RAM */
    ldr     r0, =0x20000000
    .short  0xE840, 0xF100
    str     r1, [r7, #0x0C]

    /* TT non-secure RAM */
    ldr     r0, =0x20004000
    .short  0xE840, 0xF100
    str     r1, [r7, #0x10]

    /* TTA secure flash (Non-secure view) */
    ldr     r0, =0x08000000
    .short  0xE840, 0xF180      /* TTA r1, [r0] */
    str     r1, [r7, #0x14]

    /* TTT secure flash (Secure privileged view) */
    ldr     r0, =0x08000000
    .short  0xE840, 0xF140      /* TTT r1, [r0] */
    str     r1, [r7, #0x18]

    /* TTAT secure flash (Non-secure privileged view) */
    ldr     r0, =0x08000000
    .short  0xE840, 0xF1C0      /* TTAT r1, [r0] */
    str     r1, [r7, #0x1C]

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x20]

    bkpt    #0

    .thumb_func
_fault:
    b       _fault

    .end
