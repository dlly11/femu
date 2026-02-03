/**
 * @file armv8m_mpu.h
 * @brief Memory Protection Unit for ARMv8-M (PMSAv8)
 *
 * AI INSTRUCTIONS:
 * - This header defines the COMPLETE interface for the MPU module
 * - Implementation goes in src/core/mpu/
 * - Depends on: armv8m_types.h
 * - See src/core/mpu/README.md for implementation guidance
 */

#ifndef ARMV8M_MPU_H
#define ARMV8M_MPU_H

#include "arch/armv8m/armv8m_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * MPU Constants
 *============================================================================*/

#define MPU_MAX_REGIONS 16 /**< Maximum MPU regions */

/* MPU Register Offsets (from 0xE000ED90) */
#define MPU_TYPE 0x00    /**< MPU Type Register */
#define MPU_CTRL 0x04    /**< MPU Control Register */
#define MPU_RNR 0x08     /**< MPU Region Number Register */
#define MPU_RBAR 0x0C    /**< MPU Region Base Address Register */
#define MPU_RLAR 0x10    /**< MPU Region Limit Address Register */
#define MPU_RBAR_A1 0x14 /**< Alias 1 of RBAR */
#define MPU_RLAR_A1 0x18 /**< Alias 1 of RLAR */
#define MPU_RBAR_A2 0x1C /**< Alias 2 of RBAR */
#define MPU_RLAR_A2 0x20 /**< Alias 2 of RLAR */
#define MPU_RBAR_A3 0x24 /**< Alias 3 of RBAR */
#define MPU_RLAR_A3 0x28 /**< Alias 3 of RLAR */
#define MPU_MAIR0 0x30   /**< Memory Attribute Indirection Register 0 */
#define MPU_MAIR1 0x34   /**< Memory Attribute Indirection Register 1 */

/* MPU_CTRL bits */
#define MPU_CTRL_ENABLE (1U << 0)     /**< MPU enable */
#define MPU_CTRL_HFNMIENA (1U << 1)   /**< Enable for HardFault/NMI */
#define MPU_CTRL_PRIVDEFENA (1U << 2) /**< Enable privileged default map */

/* MPU_RBAR bits */
#define MPU_RBAR_XN (1U << 0) /**< Execute Never */
#define MPU_RBAR_AP_SHIFT 1   /**< Access Permission shift */
#define MPU_RBAR_AP_MASK 0x3  /**< Access Permission mask */
#define MPU_RBAR_SH_SHIFT 3   /**< Shareability shift */
#define MPU_RBAR_SH_MASK 0x3  /**< Shareability mask */
#define MPU_RBAR_BASE_MASK                                                     \
  0xFFFFFFE0 /**< Base address mask (32-byte aligned) */

/* MPU_RLAR bits */
#define MPU_RLAR_EN (1U << 0)          /**< Region enable */
#define MPU_RLAR_ATTRINDX_SHIFT 1      /**< Attribute index shift */
#define MPU_RLAR_ATTRINDX_MASK 0x7     /**< Attribute index mask */
#define MPU_RLAR_LIMIT_MASK 0xFFFFFFE0 /**< Limit address mask */

/* Access Permissions */
typedef enum {
  MPU_AP_RW_PRIV = 0, /**< Privileged R/W, Unprivileged none */
  MPU_AP_RW_ALL = 1,  /**< Privileged R/W, Unprivileged R/W */
  MPU_AP_RO_PRIV = 2, /**< Privileged RO, Unprivileged none */
  MPU_AP_RO_ALL = 3,  /**< Privileged RO, Unprivileged RO */
} MPUAccessPerm;

/* Shareability */
typedef enum {
  MPU_SH_NONE = 0,  /**< Non-shareable */
  MPU_SH_OUTER = 2, /**< Outer shareable */
  MPU_SH_INNER = 3, /**< Inner shareable */
} MPUShareability;

/*============================================================================
 * MPU Region
 *============================================================================*/

/**
 * MPU region configuration.
 */
typedef struct {
  uint32_t rbar; /**< Region Base Address Register value */
  uint32_t rlar; /**< Region Limit Address Register value */
} MPURegion;

/*============================================================================
 * MPU State
 *============================================================================*/

/**
 * MPU context.
 */
typedef struct {
  /* Regions */
  MPURegion regions[MPU_MAX_REGIONS];
  int num_regions; /**< Number of implemented regions */

  /* Control */
  uint32_t ctrl; /**< MPU_CTRL register */
  uint32_t rnr;  /**< Currently selected region */

  /* Memory Attributes */
  uint32_t mair0; /**< MAIR0 */
  uint32_t mair1; /**< MAIR1 */

  /* Derived state */
  bool enabled;    /**< MPU is enabled */
  bool privdefena; /**< Privileged default memory map enabled */
  bool hfnmiena;   /**< MPU enabled during HardFault/NMI */
} MPU;

/*============================================================================
 * MPU Fault Info
 *============================================================================*/

typedef enum {
  MPU_FAULT_NONE = 0,
  MPU_FAULT_IACCVIOL,  /**< Instruction access violation */
  MPU_FAULT_DACCVIOL,  /**< Data access violation */
  MPU_FAULT_MUNSTKERR, /**< MemManage on unstacking */
  MPU_FAULT_MSTKERR,   /**< MemManage on stacking */
} MPUFaultType;

typedef struct {
  MPUFaultType type;
  uint32_t addr;   /**< Faulting address */
  bool addr_valid; /**< Address is valid */
} MPUFaultInfo;

/*============================================================================
 * MPU API
 *============================================================================*/

/**
 * Initialize MPU.
 *
 * @param mpu           MPU to initialize
 * @param num_regions   Number of regions to implement (0-16)
 */
void armv8m_mpu_init(MPU *mpu, int num_regions);

/**
 * Reset MPU to default state.
 *
 * @param mpu       MPU to reset
 */
void armv8m_mpu_reset(MPU *mpu);

/**
 * Check memory access against MPU.
 *
 * @param mpu               MPU
 * @param addr              Address to check
 * @param size              Access size in bytes
 * @param is_write          true for write, false for read
 * @param is_fetch          true for instruction fetch
 * @param privileged        true for privileged access
 * @param in_hardfault_nmi  true if executing in HardFault or NMI handler
 * @param fault             Filled with fault info if access denied (may be
 * NULL)
 * @return                  true if access permitted, false if denied
 */
bool armv8m_mpu_check(const MPU *mpu, uint32_t addr, uint32_t size,
                      bool is_write, bool is_fetch, bool privileged,
                      bool in_hardfault_nmi, MPUFaultInfo *fault);

/**
 * Configure an MPU region.
 *
 * @param mpu       MPU
 * @param region    Region number (0 to num_regions-1)
 * @param base      Base address (32-byte aligned)
 * @param limit     Limit address (inclusive, 32-byte aligned minus 1)
 * @param ap        Access permissions
 * @param xn        Execute Never flag
 * @param sh        Shareability
 * @param attr_idx  Attribute index (0-7)
 * @param enable    Enable the region
 * @return          ARMV8M_OK or error code
 */
int armv8m_mpu_configure_region(MPU *mpu, int region, uint32_t base,
                                uint32_t limit, MPUAccessPerm ap, bool xn,
                                MPUShareability sh, int attr_idx, bool enable);

/**
 * Enable/disable MPU.
 *
 * @param mpu           MPU
 * @param enable        Enable MPU
 * @param hfnmiena      Enable for HardFault/NMI
 * @param privdefena    Enable privileged default map
 */
void armv8m_mpu_enable(MPU *mpu, bool enable, bool hfnmiena, bool privdefena);

/**
 * Read MPU register (MMIO access).
 *
 * @param mpu       MPU
 * @param offset    Register offset from MPU base (0xE000ED90)
 * @param size      Access size
 * @return          Register value
 */
uint32_t armv8m_mpu_read(MPU *mpu, uint32_t offset, uint8_t size);

/**
 * Write MPU register (MMIO access).
 *
 * @param mpu       MPU
 * @param offset    Register offset from MPU base
 * @param value     Value to write
 * @param size      Access size
 */
void armv8m_mpu_write(MPU *mpu, uint32_t offset, uint32_t value, uint8_t size);

/**
 * Get memory attributes for an address.
 *
 * @param mpu       MPU
 * @param addr      Address
 * @param privileged Privileged access
 * @return          Memory attribute byte from MAIR
 */
uint8_t armv8m_mpu_get_attributes(const MPU *mpu, uint32_t addr,
                                  bool privileged);

#ifdef __cplusplus
}
#endif

#endif /* ARMV8M_MPU_H */
