/**
 * Simple test firmware for FEMU emulator verification.
 *
 * This program:
 * 1. Initializes R0 = 0
 * 2. Loops 10 times, incrementing R0 each time
 * 3. Stores final R0 value to RAM at 0x20000000
 * 4. Stores a "done" marker (0xDEADBEEF) at 0x20000004
 * 5. Executes BKPT to stop
 *
 * Expected results after execution:
 * - R0 = 10
 * - Memory[0x20000000] = 10
 * - Memory[0x20000004] = 0xDEADBEEF
 */

    .syntax unified
    .cpu cortex-m4
    .thumb

    .section .vectors, "a"
    .align 2
    .global _vectors
_vectors:
    .word   _stack_top          /* Initial SP */
    .word   _reset + 1          /* Reset vector (Thumb bit set) */
    .word   _nmi + 1            /* NMI */
    .word   _hardfault + 1      /* HardFault */
    /* Fill rest of vector table with default handler */
    .word   _default + 1        /* MemManage */
    .word   _default + 1        /* BusFault */
    .word   _default + 1        /* UsageFault */
    .word   0                   /* Reserved */
    .word   0                   /* Reserved */
    .word   0                   /* Reserved */
    .word   0                   /* Reserved */
    .word   _default + 1        /* SVCall */
    .word   _default + 1        /* Debug Monitor */
    .word   0                   /* Reserved */
    .word   _default + 1        /* PendSV */
    .word   _default + 1        /* SysTick */

    .section .text
    .align 2
    .thumb_func
    .global _reset
_reset:
    /* Initialize R0 = 0 (counter) */
    movs    r0, #0

    /* Initialize R1 = 10 (loop count) */
    movs    r1, #10

    /* Initialize R2 = RAM base address */
    ldr     r2, =0x20000000

loop:
    /* Increment counter */
    adds    r0, r0, #1

    /* Decrement loop count */
    subs    r1, r1, #1

    /* Loop if R1 != 0 */
    bne     loop

    /* Store final counter value to RAM */
    str     r0, [r2, #0]

    /* Store "done" marker */
    ldr     r3, =0xDEADBEEF
    str     r3, [r2, #4]

    /* Hit breakpoint to stop execution */
    bkpt    #0

    /* Should never reach here, but loop forever just in case */
halt:
    b       halt

    .thumb_func
_nmi:
    b       _nmi

    .thumb_func
_hardfault:
    b       _hardfault

    .thumb_func
_default:
    b       _default

    .end
