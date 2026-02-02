/**
 * IRQ test firmware for peripheral interrupt testing.
 *
 * This tests that peripherals can trigger interrupts:
 * 1. Sets up NVIC to enable IRQ 0 (external interrupt 0)
 * 2. Writes to peripheral at 0x40000000 offset 0x00 to trigger IRQ
 * 3. ISR sets a flag at 0x20000004
 * 4. Main code waits for flag, then verifies
 *
 * Results stored at:
 * - 0x20000000: Pre-ISR marker (0x11111111)
 * - 0x20000004: ISR marker (0x22222222 if ISR ran)
 * - 0x20000008: Post-wait marker (0xC0FFEE42 = done)
 */

    .syntax unified
    .cpu cortex-m4
    .thumb

    .section .vectors, "a"
    .align 2
    .global _vectors
_vectors:
    .word   _stack_top          /* 0: Initial SP */
    .word   _reset + 1          /* 1: Reset */
    .word   _nmi + 1            /* 2: NMI */
    .word   _hardfault + 1      /* 3: HardFault */
    .word   _default + 1        /* 4: MemManage */
    .word   _default + 1        /* 5: BusFault */
    .word   _default + 1        /* 6: UsageFault */
    .word   0, 0, 0, 0          /* 7-10: Reserved */
    .word   _default + 1        /* 11: SVCall */
    .word   _default + 1        /* 12: Debug */
    .word   0                   /* 13: Reserved */
    .word   _default + 1        /* 14: PendSV */
    .word   _default + 1        /* 15: SysTick */
    /* External interrupts start at vector 16 */
    .word   _irq0_handler + 1   /* 16: IRQ0 */

    .section .text
    .align 2
    .thumb_func
    .global _reset
_reset:
    /* Set up result area pointer */
    ldr     r5, =0x20000000     /* RAM result area */

    /* Store pre-ISR marker */
    ldr     r0, =0x11111111
    str     r0, [r5, #0]

    /* Clear ISR marker (will be set by ISR) */
    mov     r0, #0
    str     r0, [r5, #4]

    /* Enable IRQ 0 in NVIC */
    /* NVIC_ISER[0] at 0xE000E100 - write bit 0 to enable IRQ0 */
    ldr     r0, =0xE000E100     /* NVIC_ISER0 */
    mov     r1, #1              /* Enable IRQ 0 */
    str     r1, [r0]

    /* Set priority for IRQ 0 (optional, default 0 is highest) */
    /* NVIC_IPR[0] at 0xE000E400 */

    /* Trigger interrupt by writing to peripheral */
    /* Peripheral at 0x40000000, write to offset 0x00 triggers IRQ */
    ldr     r4, =0x40000000     /* Peripheral base */
    mov     r0, #1              /* Trigger value */
    str     r0, [r4, #0]        /* Write triggers IRQ */

    /* Small delay to let ISR execute */
    /* The ISR should run between instructions due to interrupt */
    mov     r0, #100
delay_loop:
    subs    r0, r0, #1
    bne     delay_loop

    /* Check if ISR ran (flag at 0x20000004 should be 0x22222222) */
    ldr     r0, [r5, #4]
    ldr     r1, =0x22222222
    cmp     r0, r1
    bne     fail

    /* Success - store done marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r5, #8]
    bkpt    #0

fail:
    /* Failure - store fail marker */
    ldr     r0, =0xDEADDEAD
    str     r0, [r5, #8]
    bkpt    #0

halt:
    b       halt

    /* IRQ 0 handler - this runs when peripheral triggers interrupt */
    .thumb_func
    .global _irq0_handler
_irq0_handler:
    /* Set ISR marker */
    ldr     r0, =0x20000004
    ldr     r1, =0x22222222
    str     r1, [r0]

    /* Clear pending (deassert) by writing to peripheral */
    ldr     r0, =0x40000000
    mov     r1, #0
    str     r1, [r0]            /* Clear IRQ */

    bx      lr                  /* Return from interrupt */

    .thumb_func
_nmi:
    b       _nmi

    .thumb_func
_hardfault:
    /* Store hardfault marker */
    ldr     r0, =0x20000008
    ldr     r1, =0xBADBAD00
    str     r1, [r0]
    bkpt    #0
    b       _hardfault

    .thumb_func
_default:
    b       _default

    .end
