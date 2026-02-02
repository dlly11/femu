/* ARMv8-M C Runtime Startup
 *
 * Provides:
 * - Vector table at 0x08000000
 * - Reset handler with .data copy and .bss zero init
 * - FPU enable (CPACR bits for CP10/CP11)
 * - Call to main()
 * - Weak default exception handlers
 */

    .syntax unified
    .cpu cortex-m4
    .fpu fpv4-sp-d16
    .thumb

/* Memory locations from linker script */
    .extern _stack_top
    .extern _data_start
    .extern _data_end
    .extern _data_load
    .extern _bss_start
    .extern _bss_end
    .extern main

/* Vector table */
    .section .vectors, "a"
    .align 2
    .globl _vectors
_vectors:
    .word _stack_top          /* 0x00: Initial SP */
    .word _reset_handler      /* 0x04: Reset */
    .word _nmi_handler        /* 0x08: NMI */
    .word _hardfault_handler  /* 0x0C: HardFault */
    .word _memmanage_handler  /* 0x10: MemManage */
    .word _busfault_handler   /* 0x14: BusFault */
    .word _usagefault_handler /* 0x18: UsageFault */
    .word 0                   /* 0x1C: Reserved */
    .word 0                   /* 0x20: Reserved */
    .word 0                   /* 0x24: Reserved */
    .word 0                   /* 0x28: Reserved */
    .word _svc_handler        /* 0x2C: SVCall */
    .word _debugmon_handler   /* 0x30: DebugMon */
    .word 0                   /* 0x34: Reserved */
    .word _pendsv_handler     /* 0x38: PendSV */
    .word _systick_handler    /* 0x3C: SysTick */

/* Reset handler */
    .section .text
    .thumb_func
    .globl _reset_handler
    .type _reset_handler, %function
_reset_handler:
    /* Enable FPU: set CP10 and CP11 to full access */
    ldr r0, =0xE000ED88       /* CPACR address */
    ldr r1, [r0]
    orr r1, r1, #(0xF << 20)  /* Set CP10 and CP11 bits */
    str r1, [r0]
    dsb                       /* Ensure write completes */
    isb                       /* Flush pipeline */

    /* Copy .data section from flash to RAM */
    ldr r0, =_data_start      /* Destination start */
    ldr r1, =_data_end        /* Destination end */
    ldr r2, =_data_load       /* Source (in flash) */

.copy_data:
    cmp r0, r1
    bge .copy_data_done
    ldr r3, [r2], #4
    str r3, [r0], #4
    b .copy_data

.copy_data_done:
    /* Zero .bss section */
    ldr r0, =_bss_start
    ldr r1, =_bss_end
    mov r2, #0

.zero_bss:
    cmp r0, r1
    bge .zero_bss_done
    str r2, [r0], #4
    b .zero_bss

.zero_bss_done:
    /* Call main() */
    bl main

    /* If main returns, halt */
.halt:
    bkpt #0
    b .halt

/* Default exception handlers (weak, can be overridden) */
    .thumb_func
    .weak _nmi_handler
    .type _nmi_handler, %function
_nmi_handler:
    bkpt #1
    b _nmi_handler

    .thumb_func
    .weak _hardfault_handler
    .type _hardfault_handler, %function
_hardfault_handler:
    bkpt #2
    b _hardfault_handler

    .thumb_func
    .weak _memmanage_handler
    .type _memmanage_handler, %function
_memmanage_handler:
    bkpt #3
    b _memmanage_handler

    .thumb_func
    .weak _busfault_handler
    .type _busfault_handler, %function
_busfault_handler:
    bkpt #4
    b _busfault_handler

    .thumb_func
    .weak _usagefault_handler
    .type _usagefault_handler, %function
_usagefault_handler:
    bkpt #5
    b _usagefault_handler

    .thumb_func
    .weak _svc_handler
    .type _svc_handler, %function
_svc_handler:
    bkpt #6
    b _svc_handler

    .thumb_func
    .weak _debugmon_handler
    .type _debugmon_handler, %function
_debugmon_handler:
    bkpt #7
    b _debugmon_handler

    .thumb_func
    .weak _pendsv_handler
    .type _pendsv_handler, %function
_pendsv_handler:
    bkpt #8
    b _pendsv_handler

    .thumb_func
    .weak _systick_handler
    .type _systick_handler, %function
_systick_handler:
    bkpt #9
    b _systick_handler

    .end
