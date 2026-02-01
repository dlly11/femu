/* ARMv8-M MPU Register Definitions and Access Functions
 *
 * Provides access to Memory Protection Unit registers:
 * - MPU_TYPE:  Information about number of regions
 * - MPU_CTRL:  MPU enable and configuration
 * - MPU_RNR:   Region number selection
 * - MPU_RBAR:  Region base address
 * - MPU_RASR:  Region attribute and size
 */

#ifndef ARMV8M_MPU_H
#define ARMV8M_MPU_H

#include <stdint.h>

/* ========== MPU Register Addresses ========== */

#define MPU_TYPE_ADDR   0xE000ED90
#define MPU_CTRL_ADDR   0xE000ED94
#define MPU_RNR_ADDR    0xE000ED98
#define MPU_RBAR_ADDR   0xE000ED9C
#define MPU_RASR_ADDR   0xE000EDA0

/* ARMv8-M additional MPU registers */
#define MPU_RBAR_A1_ADDR  0xE000EDA4
#define MPU_RASR_A1_ADDR  0xE000EDA8
#define MPU_RBAR_A2_ADDR  0xE000EDAC
#define MPU_RASR_A2_ADDR  0xE000EDB0
#define MPU_RBAR_A3_ADDR  0xE000EDB4
#define MPU_RASR_A3_ADDR  0xE000EDB8

/* ========== MPU Register Pointers ========== */

#define MPU_TYPE    (*(volatile uint32_t *)MPU_TYPE_ADDR)
#define MPU_CTRL    (*(volatile uint32_t *)MPU_CTRL_ADDR)
#define MPU_RNR     (*(volatile uint32_t *)MPU_RNR_ADDR)
#define MPU_RBAR    (*(volatile uint32_t *)MPU_RBAR_ADDR)
#define MPU_RASR    (*(volatile uint32_t *)MPU_RASR_ADDR)

/* ========== MPU_TYPE Bit Fields ========== */

#define MPU_TYPE_SEPARATE_Pos   0
#define MPU_TYPE_SEPARATE_Msk   (1UL << MPU_TYPE_SEPARATE_Pos)

#define MPU_TYPE_DREGION_Pos    8
#define MPU_TYPE_DREGION_Msk    (0xFFUL << MPU_TYPE_DREGION_Pos)

#define MPU_TYPE_IREGION_Pos    16
#define MPU_TYPE_IREGION_Msk    (0xFFUL << MPU_TYPE_IREGION_Pos)

/* ========== MPU_CTRL Bit Fields ========== */

#define MPU_CTRL_ENABLE_Pos     0
#define MPU_CTRL_ENABLE_Msk     (1UL << MPU_CTRL_ENABLE_Pos)

#define MPU_CTRL_HFNMIENA_Pos   1
#define MPU_CTRL_HFNMIENA_Msk   (1UL << MPU_CTRL_HFNMIENA_Pos)

#define MPU_CTRL_PRIVDEFENA_Pos 2
#define MPU_CTRL_PRIVDEFENA_Msk (1UL << MPU_CTRL_PRIVDEFENA_Pos)

/* ========== MPU_RBAR Bit Fields ========== */

#define MPU_RBAR_REGION_Pos     0
#define MPU_RBAR_REGION_Msk     (0xFUL << MPU_RBAR_REGION_Pos)

#define MPU_RBAR_VALID_Pos      4
#define MPU_RBAR_VALID_Msk      (1UL << MPU_RBAR_VALID_Pos)

#define MPU_RBAR_ADDR_Pos       5
#define MPU_RBAR_ADDR_Msk       (0x7FFFFFFUL << MPU_RBAR_ADDR_Pos)

/* ========== MPU_RASR Bit Fields ========== */

#define MPU_RASR_ENABLE_Pos     0
#define MPU_RASR_ENABLE_Msk     (1UL << MPU_RASR_ENABLE_Pos)

#define MPU_RASR_SIZE_Pos       1
#define MPU_RASR_SIZE_Msk       (0x1FUL << MPU_RASR_SIZE_Pos)

#define MPU_RASR_SRD_Pos        8
#define MPU_RASR_SRD_Msk        (0xFFUL << MPU_RASR_SRD_Pos)

#define MPU_RASR_B_Pos          16
#define MPU_RASR_B_Msk          (1UL << MPU_RASR_B_Pos)

#define MPU_RASR_C_Pos          17
#define MPU_RASR_C_Msk          (1UL << MPU_RASR_C_Pos)

#define MPU_RASR_S_Pos          18
#define MPU_RASR_S_Msk          (1UL << MPU_RASR_S_Pos)

#define MPU_RASR_TEX_Pos        19
#define MPU_RASR_TEX_Msk        (0x7UL << MPU_RASR_TEX_Pos)

#define MPU_RASR_AP_Pos         24
#define MPU_RASR_AP_Msk         (0x7UL << MPU_RASR_AP_Pos)

#define MPU_RASR_XN_Pos         28
#define MPU_RASR_XN_Msk         (1UL << MPU_RASR_XN_Pos)

/* ========== MPU Access Permission Values ========== */

#define MPU_AP_NO_ACCESS        0x0  /* No access */
#define MPU_AP_PRIV_RW          0x1  /* Privileged RW only */
#define MPU_AP_PRIV_RW_UNPRIV_RO 0x2 /* Privileged RW, unprivileged RO */
#define MPU_AP_FULL_ACCESS      0x3  /* Full access */
#define MPU_AP_PRIV_RO          0x5  /* Privileged RO only */
#define MPU_AP_RO               0x6  /* Read-only (priv and unpriv) */

/* ========== MPU Size Values ========== */

/* SIZE field encodes 2^(SIZE+1) bytes */
#define MPU_SIZE_32B    4
#define MPU_SIZE_64B    5
#define MPU_SIZE_128B   6
#define MPU_SIZE_256B   7
#define MPU_SIZE_512B   8
#define MPU_SIZE_1KB    9
#define MPU_SIZE_2KB    10
#define MPU_SIZE_4KB    11
#define MPU_SIZE_8KB    12
#define MPU_SIZE_16KB   13
#define MPU_SIZE_32KB   14
#define MPU_SIZE_64KB   15
#define MPU_SIZE_128KB  16
#define MPU_SIZE_256KB  17
#define MPU_SIZE_512KB  18
#define MPU_SIZE_1MB    19
#define MPU_SIZE_2MB    20
#define MPU_SIZE_4MB    21
#define MPU_SIZE_8MB    22
#define MPU_SIZE_16MB   23
#define MPU_SIZE_32MB   24
#define MPU_SIZE_64MB   25
#define MPU_SIZE_128MB  26
#define MPU_SIZE_256MB  27
#define MPU_SIZE_512MB  28
#define MPU_SIZE_1GB    29
#define MPU_SIZE_2GB    30
#define MPU_SIZE_4GB    31

/* ========== Helper Functions ========== */

static inline uint32_t mpu_get_num_regions(void) {
    return (MPU_TYPE & MPU_TYPE_DREGION_Msk) >> MPU_TYPE_DREGION_Pos;
}

static inline void mpu_select_region(uint32_t region) {
    MPU_RNR = region;
}

static inline void mpu_set_region_base(uint32_t base_addr) {
    /* Base address must be aligned to region size */
    MPU_RBAR = base_addr & ~0x1F;
}

static inline void mpu_set_region_attr(uint32_t size, uint32_t ap, uint32_t xn,
                                       uint32_t tex, uint32_t s, uint32_t c, uint32_t b) {
    MPU_RASR = (xn << MPU_RASR_XN_Pos) |
               (ap << MPU_RASR_AP_Pos) |
               (tex << MPU_RASR_TEX_Pos) |
               (s << MPU_RASR_S_Pos) |
               (c << MPU_RASR_C_Pos) |
               (b << MPU_RASR_B_Pos) |
               (size << MPU_RASR_SIZE_Pos) |
               MPU_RASR_ENABLE_Msk;
}

static inline void mpu_enable(uint32_t privdefena, uint32_t hfnmiena) {
    MPU_CTRL = MPU_CTRL_ENABLE_Msk |
               (privdefena ? MPU_CTRL_PRIVDEFENA_Msk : 0) |
               (hfnmiena ? MPU_CTRL_HFNMIENA_Msk : 0);
    __asm__ volatile ("dsb" ::: "memory");
    __asm__ volatile ("isb" ::: "memory");
}

static inline void mpu_disable(void) {
    MPU_CTRL = 0;
    __asm__ volatile ("dsb" ::: "memory");
    __asm__ volatile ("isb" ::: "memory");
}

/* Configure a complete region in one call */
static inline void mpu_configure_region(uint32_t region, uint32_t base_addr,
                                        uint32_t size, uint32_t ap, uint32_t xn) {
    mpu_select_region(region);
    mpu_set_region_base(base_addr);
    /* Default: TEX=0, S=1, C=1, B=1 (normal memory, shareable, cacheable) */
    mpu_set_region_attr(size, ap, xn, 0, 1, 1, 1);
}

#endif /* ARMV8M_MPU_H */
