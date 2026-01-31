/**
 * test_adr.S - Test PC-relative addressing instructions
 *
 * Tests: ADR, ADD with PC, SUB with PC
 *
 * Memory layout at 0x20000000:
 *   [0x00] = ADR result (expect address of data_label)
 *   [0x04] = Data loaded via ADR (expect 0xDEADBEEF)
 *   [0x08] = ADR forward reference (expect address)
 *   [0x0C] = ADR backward reference (expect address)
 *   [0x10] = ADD PC result (expect computed address)
 *   [0x14] = Data via ADD PC (expect 0xCAFEBABE)
 *   [0x18] = String length test (expect 13)
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

    /* Test 1: ADR - load address of nearby label */
    adr     r0, data_label
    str     r0, [r7, #0x00]     /* Store the address itself */

    /* Test 2: Load data via ADR-computed address */
    adr     r1, data_label
    ldr     r0, [r1]
    str     r0, [r7, #0x04]     /* expect 0xDEADBEEF */

    /* Test 3: ADR with forward reference */
    adr     r0, forward_data
    str     r0, [r7, #0x08]

    /* Test 4: ADR with backward reference */
    b       test4_continue
back_label:
    .word   0x12345678
    .align  2
test4_continue:
    adr     r0, back_label
    str     r0, [r7, #0x0C]

    /* Test 5: ADD with PC (manual PC-relative addressing) */
    /* ADD Rd, PC, #imm - computes address relative to current PC */
    add     r0, pc, #0          /* r0 = address of next instruction (approximately) */
    str     r0, [r7, #0x10]

    /* Test 6: Load data using PC-relative ADD */
    adr     r1, data_label2
    ldr     r0, [r1]
    str     r0, [r7, #0x14]     /* expect 0xCAFEBABE */

    /* Test 7: String length using ADR
     * Compute length of a string by subtracting addresses
     */
    adr     r0, test_string
    adr     r1, test_string_end
    subs    r0, r1, r0          /* Length = end - start */
    str     r0, [r7, #0x18]     /* expect 13 ("Hello, World!" without null) */

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x1C]

    /* Done */
    bkpt    #0

    /* Data section (in .text for PC-relative access) */
    .align  2
data_label:
    .word   0xDEADBEEF

data_label2:
    .word   0xCAFEBABE

forward_data:
    .word   0x11223344

test_string:
    .ascii  "Hello, World!"
test_string_end:
    .align  2

    .thumb_func
_fault:
    b       _fault

    .end
