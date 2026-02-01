/**
 * test_mpu_basic.S - Test MPU region configuration and read
 *
 * Tests: MPU_TYPE, MPU_CTRL, MPU_RNR, MPU_RBAR, MPU_RASR
 *
 * Memory layout at 0x20000000:
 *   [0x00] = MPU_TYPE DREGION field (number of regions)
 *   [0x04] = MPU_RBAR readback after setting region 0
 *   [0x08] = MPU_RASR readback after setting region 0
 *   [0x0C] = MPU_CTRL value after enabling
 *   [0x10] = 0xC0FFEE42 (success marker)
 */

    .syntax unified
    .cpu cortex-m4
    .thumb

    .section .vectors, "a"
    .word   0x20008000          /* Initial SP */
    .word   _reset + 1          /* Reset vector */
    .word   _fault + 1          /* NMI */
    .word   _fault + 1          /* HardFault */
    .word   _fault + 1          /* MemManage */
    .word   _fault + 1          /* BusFault */
    .word   _fault + 1          /* UsageFault */

    .section .text
    .thumb_func
    .global _reset
_reset:
    /* Setup base pointer for results */
    ldr     r7, =0x20000000

    /* MPU register addresses */
    .equ MPU_TYPE, 0xE000ED90
    .equ MPU_CTRL, 0xE000ED94
    .equ MPU_RNR,  0xE000ED98
    .equ MPU_RBAR, 0xE000ED9C
    .equ MPU_RASR, 0xE000EDA0

    /* ============================================ */
    /* Test 1: Read MPU_TYPE to get number of regions */
    /* ============================================ */
    ldr     r0, =MPU_TYPE
    ldr     r1, [r0]            /* Read MPU_TYPE */
    lsr     r2, r1, #8          /* DREGION field is bits [15:8] */
    and     r2, r2, #0xFF       /* Mask to 8 bits */
    str     r2, [r7, #0x00]     /* Store region count */

    /* ============================================ */
    /* Test 2: Configure MPU region 0 */
    /* ============================================ */

    /* Select region 0 */
    ldr     r0, =MPU_RNR
    mov     r1, #0
    str     r1, [r0]            /* MPU_RNR = 0 */

    /* Set region base address to 0x20000000 (RAM) */
    ldr     r0, =MPU_RBAR
    ldr     r1, =0x20000000
    str     r1, [r0]            /* MPU_RBAR = RAM base */

    /* Read back RBAR to verify */
    ldr     r2, [r0]
    str     r2, [r7, #0x04]     /* Store RBAR readback */

    /* ============================================ */
    /* Test 3: Set region attributes */
    /* ============================================ */

    /* Configure RASR:
     * - SIZE = 14 (32KB, encoded as 14 in bits [5:1])
     * - ENABLE = 1 (bit 0)
     * - AP = 0b011 (full access, bits [26:24])
     * - TEX = 0, S = 1, C = 1, B = 1 (normal memory, bits [21:16])
     *
     * RASR = (0b011 << 24) | (0b000 << 19) | (1 << 18) | (1 << 17) | (1 << 16) | (14 << 1) | 1
     *      = 0x0307001D
     */
    ldr     r0, =MPU_RASR
    ldr     r1, =0x0307001D
    str     r1, [r0]            /* MPU_RASR = configured */

    /* Read back RASR to verify */
    ldr     r2, [r0]
    str     r2, [r7, #0x08]     /* Store RASR readback */

    /* ============================================ */
    /* Test 4: Enable MPU with PRIVDEFENA */
    /* ============================================ */

    /* MPU_CTRL:
     * - ENABLE = 1 (bit 0)
     * - HFNMIENA = 0 (bit 1) - MPU disabled during hard faults
     * - PRIVDEFENA = 1 (bit 2) - privileged default memory map
     *
     * CTRL = 0x05
     */
    ldr     r0, =MPU_CTRL
    mov     r1, #0x05           /* ENABLE | PRIVDEFENA */
    str     r1, [r0]

    /* Memory barrier after MPU configuration */
    dsb     sy
    isb     sy

    /* Read back CTRL to verify */
    ldr     r2, [r0]
    str     r2, [r7, #0x0C]     /* Store CTRL readback */

    /* ============================================ */
    /* Disable MPU before finishing */
    /* ============================================ */
    mov     r1, #0
    str     r1, [r0]            /* Disable MPU */
    dsb     sy
    isb     sy

    /* Success marker */
    ldr     r0, =0xC0FFEE42
    str     r0, [r7, #0x10]

    /* Done */
    bkpt    #0

    .thumb_func
_fault:
    b       _fault

    .end
