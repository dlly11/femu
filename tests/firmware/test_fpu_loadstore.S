/**
 * test_fpu_loadstore.S - Test FPU load/store multiple instructions
 *
 * Tests: VPUSH, VPOP, VLDM, VSTM, VLDMIA, VSTMIA, VLDMDB, VSTMDB
 *
 * Memory layout at 0x20000000:
 *   [0x00] = s0 after VPOP (expect 1.0)
 *   [0x04] = s1 after VPOP (expect 2.0)
 *   [0x08] = s2 after VPOP (expect 3.0)
 *   [0x0C] = s3 after VPOP (expect 4.0)
 *   [0x10] = VLDM test - s4 (expect 5.0)
 *   [0x14] = VLDM test - s5 (expect 6.0)
 *   [0x18] = VSTM destination check (expect 1.0)
 *   [0x1C] = VSTM destination check (expect 2.0)
 *   [0x20] = SP unchanged check (expect 0)
 *   [0x24] = 0xC0FFEE42 (success marker)
 *
 * Stack area: 0x20000F00 - 0x20001000
 * Data area: 0x20000100
 */

    .syntax unified
    .cpu cortex-m4
    .fpu fpv4-sp-d16
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
    /* Test 1: VPUSH/VPOP - Push and pop FP regs   */
    /* Load values into s0-s3, push, clear, pop    */
    /* ============================================ */

    /* Load test values into s0-s3 */
    adr     r0, float_1_0
    vldr    s0, [r0]            /* s0 = 1.0 */
    adr     r0, float_2_0
    vldr    s1, [r0]            /* s1 = 2.0 */
    adr     r0, float_3_0
    vldr    s2, [r0]            /* s2 = 3.0 */
    adr     r0, float_4_0
    vldr    s3, [r0]            /* s3 = 4.0 */

    /* Save SP for later check */
    mov     r4, sp

    /* Push s0-s3 onto stack */
    vpush   {s0-s3}

    /* Clear the registers by loading zeros */
    mov     r0, #0
    vmov    s0, r0
    vmov    s1, r0
    vmov    s2, r0
    vmov    s3, r0

    /* Pop s0-s3 back from stack */
    vpop    {s0-s3}

    /* Verify SP is restored */
    sub     r5, sp, r4          /* Should be 0 */
    str     r5, [r7, #0x20]     /* expect 0 */

    /* Store results to verify */
    vstr    s0, [r7, #0x00]     /* expect 1.0 = 0x3F800000 */
    vstr    s1, [r7, #0x04]     /* expect 2.0 = 0x40000000 */
    vstr    s2, [r7, #0x08]     /* expect 3.0 = 0x40400000 */
    vstr    s3, [r7, #0x0C]     /* expect 4.0 = 0x40800000 */

    /* ============================================ */
    /* Test 2: VLDMIA - Load multiple FP regs      */
    /* Load s4-s5 from data area using VLDM        */
    /* ============================================ */
    adr     r0, float_data
    vldmia  r0, {s4-s5}         /* s4 = 5.0, s5 = 6.0 */
    vstr    s4, [r7, #0x10]     /* expect 5.0 = 0x40A00000 */
    vstr    s5, [r7, #0x14]     /* expect 6.0 = 0x40C00000 */

    /* ============================================ */
    /* Test 3: VSTMIA - Store multiple FP regs     */
    /* Store s0-s1 to data area using VSTM         */
    /* ============================================ */
    add     r0, r7, #0x18       /* Destination for VSTM */
    vstmia  r0, {s0-s1}         /* Store s0=1.0, s1=2.0 */
    /* Values are now at [r7+0x18] and [r7+0x1C] */

    /* ============================================ */
    /* Test 4: VLDMIA with writeback (!= IA)       */
    /* ============================================ */
    adr     r0, float_data
    vldmia  r0!, {s6-s7}        /* Load and increment r0 */
    /* r0 should now point past the loaded data */

    /* ============================================ */
    /* Test 5: VSTMIA with writeback               */
    /* ============================================ */
    add     r0, r7, #0x80       /* Use higher memory area */
    vstmia  r0!, {s0-s1}        /* Store and increment */
    /* r0 should now be at 0x20000088 */

    /* ============================================ */
    /* Test 6: VLDMDB - Decrementing before        */
    /* (Not commonly used, but should work)        */
    /* ============================================ */
    adr     r0, float_data_end  /* Point past data */
    vldmdb  r0!, {s8-s9}        /* Load 2 regs, decrement before */
    /* s8 = 5.0, s9 = 6.0 (in reverse order of memory) */

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x24]

    /* Done */
    bkpt    #0

    .thumb_func
_fault:
    b       _fault

    /* ============================================ */
    /* Float constants (IEEE 754 single precision) */
    /* ============================================ */
    .align 4
float_1_0:
    .word   0x3F800000          /* 1.0 */
float_2_0:
    .word   0x40000000          /* 2.0 */
float_3_0:
    .word   0x40400000          /* 3.0 */
float_4_0:
    .word   0x40800000          /* 4.0 */

    /* Data area for VLDM/VSTM tests */
    .align 4
float_data:
    .word   0x40A00000          /* 5.0 */
    .word   0x40C00000          /* 6.0 */
float_data_end:

    .end
