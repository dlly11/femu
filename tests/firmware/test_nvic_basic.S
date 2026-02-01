/**
 * test_nvic_basic.S - Test NVIC register read/write
 *
 * Tests: NVIC_ISER, NVIC_ICER, NVIC_ISPR, NVIC_ICPR, NVIC_IPR
 *
 * Memory layout at 0x20000000:
 *   [0x00] = ISER0 after enabling IRQ 0
 *   [0x04] = ICER0 effect (ISER0 after clearing IRQ 0)
 *   [0x08] = ISPR0 after setting IRQ 0 pending
 *   [0x0C] = ICPR0 effect (ISPR0 after clearing pending)
 *   [0x10] = IPR0 after setting priority
 *   [0x14] = ISER0 after enabling multiple IRQs
 *   [0x18] = 0xC0FFEE42 (success marker)
 */

    .syntax unified
    .cpu cortex-m4
    .thumb

    .section .vectors, "a"
    .word   0x20008000          /* Initial SP */
    .word   _reset + 1          /* Reset vector */
    .word   _fault + 1          /* NMI */
    .word   _fault + 1          /* HardFault */

    .section .text
    .thumb_func
    .global _reset
_reset:
    /* Setup base pointer for results */
    ldr     r7, =0x20000000

    /* NVIC register addresses */
    .equ NVIC_ISER0, 0xE000E100  /* Set-enable register 0 */
    .equ NVIC_ICER0, 0xE000E180  /* Clear-enable register 0 */
    .equ NVIC_ISPR0, 0xE000E200  /* Set-pending register 0 */
    .equ NVIC_ICPR0, 0xE000E280  /* Clear-pending register 0 */
    .equ NVIC_IPR0,  0xE000E400  /* Priority register 0 */

    /* ============================================ */
    /* Test 1: Enable IRQ 0 using ISER */
    /* ============================================ */
    ldr     r0, =NVIC_ISER0
    mov     r1, #1              /* Bit 0 = IRQ 0 */
    str     r1, [r0]            /* Enable IRQ 0 */

    /* Read back to verify */
    ldr     r2, [r0]
    str     r2, [r7, #0x00]     /* expect 0x00000001 */

    /* ============================================ */
    /* Test 2: Disable IRQ 0 using ICER */
    /* ============================================ */
    ldr     r0, =NVIC_ICER0
    mov     r1, #1              /* Bit 0 = IRQ 0 */
    str     r1, [r0]            /* Disable IRQ 0 */

    /* Read ISER0 to verify IRQ is disabled */
    ldr     r0, =NVIC_ISER0
    ldr     r2, [r0]
    str     r2, [r7, #0x04]     /* expect 0x00000000 */

    /* ============================================ */
    /* Test 3: Set IRQ 0 pending using ISPR */
    /* ============================================ */
    ldr     r0, =NVIC_ISPR0
    mov     r1, #1              /* Bit 0 = IRQ 0 */
    str     r1, [r0]            /* Set IRQ 0 pending */

    /* Read back to verify */
    ldr     r2, [r0]
    str     r2, [r7, #0x08]     /* expect 0x00000001 */

    /* ============================================ */
    /* Test 4: Clear IRQ 0 pending using ICPR */
    /* ============================================ */
    ldr     r0, =NVIC_ICPR0
    mov     r1, #1              /* Bit 0 = IRQ 0 */
    str     r1, [r0]            /* Clear IRQ 0 pending */

    /* Read ISPR0 to verify pending is cleared */
    ldr     r0, =NVIC_ISPR0
    ldr     r2, [r0]
    str     r2, [r7, #0x0C]     /* expect 0x00000000 */

    /* ============================================ */
    /* Test 5: Set priority for IRQ 0 */
    /* ============================================ */
    ldr     r0, =NVIC_IPR0
    mov     r1, #0x40           /* Priority 0x40 for IRQ 0 (in byte 0) */
    str     r1, [r0]            /* Set priority */

    /* Read back to verify */
    ldr     r2, [r0]
    str     r2, [r7, #0x10]     /* expect 0x00000040 */

    /* ============================================ */
    /* Test 6: Enable multiple IRQs */
    /* ============================================ */
    ldr     r0, =NVIC_ISER0
    ldr     r1, =0x0000000F     /* Enable IRQs 0-3 */
    str     r1, [r0]

    /* Read back to verify */
    ldr     r2, [r0]
    str     r2, [r7, #0x14]     /* expect 0x0000000F */

    /* Clean up: disable all IRQs we enabled */
    ldr     r0, =NVIC_ICER0
    str     r1, [r0]

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x18]

    /* Done */
    bkpt    #0

    .thumb_func
_fault:
    b       _fault

    .end
