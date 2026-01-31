/**
 * test_table_branch.S - Test table branch instructions
 *
 * Tests: TBB (table branch byte), TBH (table branch halfword)
 *
 * Memory layout at 0x20000000:
 *   [0x00] = TBB case 0 result (expect 10)
 *   [0x04] = TBB case 1 result (expect 20)
 *   [0x08] = TBB case 2 result (expect 30)
 *   [0x0C] = TBH case 0 result (expect 100)
 *   [0x10] = TBH case 1 result (expect 200)
 *   [0x14] = TBH case 2 result (expect 300)
 *   [0x18] = TBB loop accumulator (expect 60 = 10+20+30)
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
    /* Test 1-3: TBB (Table Branch Byte)           */
    /* TBB [Rn, Rm] branches to PC + 2*table[Rm]   */
    /* ============================================ */

    /* Test TBB case 0 */
    movs    r0, #0              /* Index = 0 */
    tbb     [pc, r0]            /* Branch using table */
tbb_table1:
    .byte   (tbb_case0 - tbb_table1) / 2
    .byte   (tbb_case1 - tbb_table1) / 2
    .byte   (tbb_case2 - tbb_table1) / 2
    .byte   0                   /* Padding for alignment */
    .align  2

tbb_case0:
    movs    r0, #10
    str     r0, [r7, #0x00]
    b       tbb_test2

tbb_case1:
    movs    r0, #20
    str     r0, [r7, #0x04]
    b       tbb_test3

tbb_case2:
    movs    r0, #30
    str     r0, [r7, #0x08]
    b       tbh_tests

tbb_test2:
    /* Test TBB case 1 */
    movs    r0, #1              /* Index = 1 */
    tbb     [pc, r0]
tbb_table2:
    .byte   (tbb2_case0 - tbb_table2) / 2
    .byte   (tbb2_case1 - tbb_table2) / 2
    .byte   (tbb2_case2 - tbb_table2) / 2
    .byte   0
    .align  2

tbb2_case0:
    b       tbb_test3           /* Should not reach */
tbb2_case1:
    movs    r0, #20
    str     r0, [r7, #0x04]
    b       tbb_test3
tbb2_case2:
    b       tbb_test3           /* Should not reach */

tbb_test3:
    /* Test TBB case 2 */
    movs    r0, #2              /* Index = 2 */
    tbb     [pc, r0]
tbb_table3:
    .byte   (tbb3_case0 - tbb_table3) / 2
    .byte   (tbb3_case1 - tbb_table3) / 2
    .byte   (tbb3_case2 - tbb_table3) / 2
    .byte   0
    .align  2

tbb3_case0:
    b       tbh_tests           /* Should not reach */
tbb3_case1:
    b       tbh_tests           /* Should not reach */
tbb3_case2:
    movs    r0, #30
    str     r0, [r7, #0x08]
    b       tbh_tests

    /* ============================================ */
    /* Test 4-6: TBH (Table Branch Halfword)       */
    /* TBH [Rn, Rm, LSL #1] branches to PC + 2*table[Rm] */
    /* ============================================ */
tbh_tests:
    /* Test TBH case 0 */
    movs    r0, #0              /* Index = 0 */
    tbh     [pc, r0, lsl #1]    /* Branch using halfword table */
tbh_table1_base:  /* This label is at PC+4, the reference for offsets */
    .short  (tbh_case0 - tbh_table1_base) / 2
    .short  (tbh_case1 - tbh_table1_base) / 2
    .short  (tbh_case2 - tbh_table1_base) / 2
    .short  0                   /* Padding */
    .align  2

tbh_case0:
    mov     r0, #100
    str     r0, [r7, #0x0C]
    b       tbh_test2

tbh_case1:
    mov     r0, #200
    str     r0, [r7, #0x10]
    b       tbh_test3

tbh_case2:
    mov     r0, #300
    str     r0, [r7, #0x14]
    b       tbb_loop_test

tbh_test2:
    /* Test TBH case 1 */
    movs    r0, #1
    tbh     [pc, r0, lsl #1]
tbh_table2_base:  /* This label is at PC+4, the reference for offsets */
    .short  (tbh2_case0 - tbh_table2_base) / 2
    .short  (tbh2_case1 - tbh_table2_base) / 2
    .short  0
    .align  2

tbh2_case0:
    b       tbh_test3
tbh2_case1:
    mov     r0, #200
    str     r0, [r7, #0x10]
    b       tbh_test3

tbh_test3:
    /* Test TBH case 2 */
    movs    r0, #2
    tbh     [pc, r0, lsl #1]
tbh_table3_base:  /* This label is at PC+4, the reference for offsets */
    .short  (tbh3_case0 - tbh_table3_base) / 2
    .short  (tbh3_case1 - tbh_table3_base) / 2
    .short  (tbh3_case2 - tbh_table3_base) / 2
    .short  0
    .align  2

tbh3_case0:
    b       tbb_loop_test
tbh3_case1:
    b       tbb_loop_test
tbh3_case2:
    mov     r0, #300
    str     r0, [r7, #0x14]
    b       tbb_loop_test

    /* ============================================ */
    /* Test 7: TBB in a loop (switch-case style)   */
    /* Sum values from cases 0, 1, 2               */
    /* ============================================ */
tbb_loop_test:
    movs    r4, #0              /* Accumulator */
    movs    r5, #0              /* Loop counter / case index */

tbb_loop:
    mov     r0, r5
    tbb     [pc, r0]
tbb_loop_table:
    .byte   (loop_case0 - tbb_loop_table) / 2
    .byte   (loop_case1 - tbb_loop_table) / 2
    .byte   (loop_case2 - tbb_loop_table) / 2
    .byte   (loop_done - tbb_loop_table) / 2
    .align  2

loop_case0:
    adds    r4, r4, #10
    b       loop_next
loop_case1:
    adds    r4, r4, #20
    b       loop_next
loop_case2:
    adds    r4, r4, #30
    b       loop_next
loop_next:
    adds    r5, r5, #1
    cmp     r5, #3
    blt     tbb_loop

loop_done:
    str     r4, [r7, #0x18]     /* expect 60 = 10+20+30 */

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x1C]

    /* Done */
    bkpt    #0

    .thumb_func
_fault:
    b       _fault

    .end
