/* ARMv8-M TrustZone Register Definitions and Access Functions
 *
 * Provides access to Security Attribution Unit (SAU) registers
 * and TrustZone-related system registers.
 */

#ifndef ARMV8M_TRUSTZONE_H
#define ARMV8M_TRUSTZONE_H

#include <stdint.h>

/* ========== SAU Register Addresses ========== */

#define SAU_CTRL_ADDR   0xE000EDD0
#define SAU_TYPE_ADDR   0xE000EDD4
#define SAU_RNR_ADDR    0xE000EDD8
#define SAU_RBAR_ADDR   0xE000EDDC
#define SAU_RLAR_ADDR   0xE000EDE0
#define SAU_SFSR_ADDR   0xE000EDE4
#define SAU_SFAR_ADDR   0xE000EDE8

/* ========== SAU Register Pointers ========== */

#define SAU_CTRL    (*(volatile uint32_t *)SAU_CTRL_ADDR)
#define SAU_TYPE    (*(volatile uint32_t *)SAU_TYPE_ADDR)
#define SAU_RNR     (*(volatile uint32_t *)SAU_RNR_ADDR)
#define SAU_RBAR    (*(volatile uint32_t *)SAU_RBAR_ADDR)
#define SAU_RLAR    (*(volatile uint32_t *)SAU_RLAR_ADDR)
#define SAU_SFSR    (*(volatile uint32_t *)SAU_SFSR_ADDR)
#define SAU_SFAR    (*(volatile uint32_t *)SAU_SFAR_ADDR)

/* ========== SAU_CTRL Bit Fields ========== */

#define SAU_CTRL_ENABLE_Pos     0
#define SAU_CTRL_ENABLE_Msk     (1UL << SAU_CTRL_ENABLE_Pos)

#define SAU_CTRL_ALLNS_Pos      1
#define SAU_CTRL_ALLNS_Msk      (1UL << SAU_CTRL_ALLNS_Pos)

/* ========== SAU_TYPE Bit Fields ========== */

#define SAU_TYPE_SREGION_Pos    0
#define SAU_TYPE_SREGION_Msk    (0xFFUL << SAU_TYPE_SREGION_Pos)

/* ========== SAU_RBAR Bit Fields ========== */

/* Base address is bits [31:5], aligned to 32 bytes */
#define SAU_RBAR_BADDR_Pos      5
#define SAU_RBAR_BADDR_Msk      (0x7FFFFFFUL << SAU_RBAR_BADDR_Pos)

/* ========== SAU_RLAR Bit Fields ========== */

#define SAU_RLAR_ENABLE_Pos     0
#define SAU_RLAR_ENABLE_Msk     (1UL << SAU_RLAR_ENABLE_Pos)

#define SAU_RLAR_NSC_Pos        1
#define SAU_RLAR_NSC_Msk        (1UL << SAU_RLAR_NSC_Pos)

/* Limit address is bits [31:5] */
#define SAU_RLAR_LADDR_Pos      5
#define SAU_RLAR_LADDR_Msk      (0x7FFFFFFUL << SAU_RLAR_LADDR_Pos)

/* ========== SAU_SFSR Bit Fields ========== */

#define SAU_SFSR_INVEP_Pos      0   /* Invalid entry point */
#define SAU_SFSR_INVEP_Msk      (1UL << SAU_SFSR_INVEP_Pos)

#define SAU_SFSR_INVIS_Pos      1   /* Invalid integrity signature */
#define SAU_SFSR_INVIS_Msk      (1UL << SAU_SFSR_INVIS_Pos)

#define SAU_SFSR_INVER_Pos      2   /* Invalid exception return */
#define SAU_SFSR_INVER_Msk      (1UL << SAU_SFSR_INVER_Pos)

#define SAU_SFSR_APTS_Pos       3   /* Attribution unit protection fault, transparent */
#define SAU_SFSR_APTS_Msk       (1UL << SAU_SFSR_APTS_Pos)

#define SAU_SFSR_LSPERR_Pos     5   /* Lazy state preservation error */
#define SAU_SFSR_LSPERR_Msk     (1UL << SAU_SFSR_LSPERR_Pos)

#define SAU_SFSR_SFARVALID_Pos  6   /* SFAR has valid contents */
#define SAU_SFSR_SFARVALID_Msk  (1UL << SAU_SFSR_SFARVALID_Pos)

#define SAU_SFSR_LSERR_Pos      7   /* Lazy state error */
#define SAU_SFSR_LSERR_Msk      (1UL << SAU_SFSR_LSERR_Pos)

/* ========== TT Instruction Return Value ========== */

/* TT instruction returns security attributes */
#define TT_RESP_MREGION_Pos     0
#define TT_RESP_MREGION_Msk     (0xFFUL << TT_RESP_MREGION_Pos)

#define TT_RESP_SREGION_Pos     8
#define TT_RESP_SREGION_Msk     (0xFFUL << TT_RESP_SREGION_Pos)

#define TT_RESP_MRVALID_Pos     16
#define TT_RESP_MRVALID_Msk     (1UL << TT_RESP_MRVALID_Pos)

#define TT_RESP_SRVALID_Pos     17
#define TT_RESP_SRVALID_Msk     (1UL << TT_RESP_SRVALID_Pos)

#define TT_RESP_R_Pos           18
#define TT_RESP_R_Msk           (1UL << TT_RESP_R_Pos)

#define TT_RESP_RW_Pos          19
#define TT_RESP_RW_Msk          (1UL << TT_RESP_RW_Pos)

#define TT_RESP_NSR_Pos         20
#define TT_RESP_NSR_Msk         (1UL << TT_RESP_NSR_Pos)

#define TT_RESP_NSRW_Pos        21
#define TT_RESP_NSRW_Msk        (1UL << TT_RESP_NSRW_Pos)

#define TT_RESP_S_Pos           22
#define TT_RESP_S_Msk           (1UL << TT_RESP_S_Pos)

#define TT_RESP_IRVALID_Pos     23
#define TT_RESP_IRVALID_Msk     (1UL << TT_RESP_IRVALID_Pos)

#define TT_RESP_IREGION_Pos     24
#define TT_RESP_IREGION_Msk     (0xFFUL << TT_RESP_IREGION_Pos)

/* ========== Non-Secure Callable Address Markers ========== */

/* FNC_RETURN magic value for returning from non-secure call */
#define FNC_RETURN      0xFEFFFFFF

/* Exception return values */
#define EXC_RETURN_NS   0xFFFFFFBC  /* Return to Non-Secure Thread mode using PSP */
#define EXC_RETURN_S    0xFFFFFFFD  /* Return to Secure Thread mode using PSP */

/* ========== Helper Functions ========== */

/* Get number of SAU regions */
static inline uint32_t sau_get_num_regions(void) {
    return SAU_TYPE & SAU_TYPE_SREGION_Msk;
}

/* Select SAU region for configuration */
static inline void sau_select_region(uint32_t region) {
    SAU_RNR = region;
}

/* Configure SAU region
 * base_addr: Starting address (must be 32-byte aligned)
 * limit_addr: Ending address (must be 32-byte aligned, inclusive)
 * nsc: 1 = Non-Secure Callable, 0 = Non-Secure
 */
static inline void sau_configure_region(uint32_t region, uint32_t base_addr,
                                        uint32_t limit_addr, uint32_t nsc) {
    SAU_RNR = region;
    SAU_RBAR = base_addr & ~0x1F;  /* Clear lower 5 bits */
    SAU_RLAR = (limit_addr & ~0x1F) |
               (nsc ? SAU_RLAR_NSC_Msk : 0) |
               SAU_RLAR_ENABLE_Msk;
}

/* Enable SAU */
static inline void sau_enable(void) {
    SAU_CTRL = SAU_CTRL_ENABLE_Msk;
    __asm__ volatile ("dsb" ::: "memory");
    __asm__ volatile ("isb" ::: "memory");
}

/* Disable SAU (all memory is Secure) */
static inline void sau_disable(void) {
    SAU_CTRL = 0;
    __asm__ volatile ("dsb" ::: "memory");
    __asm__ volatile ("isb" ::: "memory");
}

/* Set all memory as Non-Secure (ALLNS mode) */
static inline void sau_set_all_ns(void) {
    SAU_CTRL = SAU_CTRL_ALLNS_Msk | SAU_CTRL_ENABLE_Msk;
    __asm__ volatile ("dsb" ::: "memory");
    __asm__ volatile ("isb" ::: "memory");
}

/* TT instruction - query security attributes
 * Note: These use inline assembly to generate the actual TT instructions
 */

/* TT - Test Target (unprivileged) */
static inline uint32_t tt(uint32_t addr) {
    uint32_t result;
    __asm__ volatile ("tt %0, [%1]" : "=r" (result) : "r" (addr));
    return result;
}

/* TTA - Test Target Alternate (privileged) */
static inline uint32_t tta(uint32_t addr) {
    uint32_t result;
    __asm__ volatile ("tta %0, [%1]" : "=r" (result) : "r" (addr));
    return result;
}

/* TTT - Test Target Non-Secure (unprivileged) */
static inline uint32_t ttt(uint32_t addr) {
    uint32_t result;
    __asm__ volatile ("ttt %0, [%1]" : "=r" (result) : "r" (addr));
    return result;
}

/* TTAT - Test Target Alternate Non-Secure (privileged) */
static inline uint32_t ttat(uint32_t addr) {
    uint32_t result;
    __asm__ volatile ("ttat %0, [%1]" : "=r" (result) : "r" (addr));
    return result;
}

/* Clear security fault status */
static inline void sau_clear_faults(void) {
    SAU_SFSR = SAU_SFSR_INVEP_Msk | SAU_SFSR_INVIS_Msk |
               SAU_SFSR_INVER_Msk | SAU_SFSR_APTS_Msk |
               SAU_SFSR_LSPERR_Msk | SAU_SFSR_LSERR_Msk;
}

/* Get security fault address */
static inline uint32_t sau_get_fault_addr(void) {
    if (SAU_SFSR & SAU_SFSR_SFARVALID_Msk) {
        return SAU_SFAR;
    }
    return 0;
}

/* Check if address is in Secure region (using TT result) */
static inline int is_secure(uint32_t tt_result) {
    return (tt_result & TT_RESP_S_Msk) != 0;
}

/* Check if address is Non-Secure Callable (using TT result) */
static inline int is_nsc(uint32_t tt_result) {
    /* NSC regions have S=1 and are marked as NSC in SAU */
    return is_secure(tt_result) && (tt_result & TT_RESP_SRVALID_Msk);
}

#endif /* ARMV8M_TRUSTZONE_H */
