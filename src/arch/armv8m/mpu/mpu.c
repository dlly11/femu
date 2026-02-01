/**
 * @file mpu.c
 * @brief Memory Protection Unit implementation for ARMv8-M (PMSAv8)
 */

#include "arch/armv8m/armv8m_mpu.h"
#include <string.h>

/*============================================================================
 * Internal Helpers
 *============================================================================*/

/**
 * Check if address is within a region.
 */
static bool addr_in_region(const MPURegion *r, uint32_t addr)
{
    uint32_t base = r->rbar & MPU_RBAR_BASE_MASK;
    uint32_t limit = r->rlar | 0x1F;  /* Limit is inclusive, round up to 32-byte boundary */
    return addr >= base && addr <= limit;
}

/**
 * Find matching region for an address (highest number wins).
 * Returns region index or -1 if no match.
 */
static int find_region(const MPU *mpu, uint32_t addr)
{
    for (int i = mpu->num_regions - 1; i >= 0; i--) {
        if ((mpu->regions[i].rlar & MPU_RLAR_EN) &&
            addr_in_region(&mpu->regions[i], addr)) {
            return i;
        }
    }
    return -1;
}

/**
 * Check access permission for a region.
 */
static bool check_permission(const MPURegion *r, bool is_write,
                             bool is_fetch, bool privileged)
{
    uint32_t ap = (r->rbar >> MPU_RBAR_AP_SHIFT) & MPU_RBAR_AP_MASK;
    bool xn = (r->rbar & MPU_RBAR_XN) != 0;

    /* Check XN for instruction fetch */
    if (is_fetch && xn) {
        return false;
    }

    /* Check read/write permission based on AP bits */
    switch (ap) {
        case MPU_AP_RW_PRIV:
            /* Privileged RW, unprivileged none */
            return privileged;
        case MPU_AP_RW_ALL:
            /* RW for all */
            return true;
        case MPU_AP_RO_PRIV:
            /* Privileged RO, unprivileged none */
            return privileged && !is_write;
        case MPU_AP_RO_ALL:
            /* RO for all */
            return !is_write;
        default:
            return false;
    }
}

/**
 * Check if address falls in the default memory map regions that allow access.
 * Used when PRIVDEFENA is set and no MPU region matches.
 */
static bool check_default_map(uint32_t addr, bool is_write, bool is_fetch)
{
    (void)is_write;  /* Default map allows RW to most regions */

    /*
     * Default memory map (simplified):
     * 0x00000000-0x1FFFFFFF: Code (XN for writes)
     * 0x20000000-0x3FFFFFFF: SRAM (XN)
     * 0x40000000-0x5FFFFFFF: Peripheral (XN)
     * 0x60000000-0x9FFFFFFF: External RAM (XN)
     * 0xA0000000-0xDFFFFFFF: External Device (XN)
     * 0xE0000000-0xE00FFFFF: PPB (XN)
     * 0xE0100000-0xFFFFFFFF: Vendor-specific (XN)
     */

    /* Code region allows instruction fetch */
    if (addr < 0x20000000) {
        return true;
    }

    /* Other regions: data access OK, instruction fetch not allowed (XN) */
    if (is_fetch) {
        return false;
    }

    return true;
}

/**
 * Set fault information.
 */
static void set_fault_info(MPUFaultInfo *fault, bool is_fetch, uint32_t addr)
{
    if (fault) {
        fault->type = is_fetch ? MPU_FAULT_IACCVIOL : MPU_FAULT_DACCVIOL;
        fault->addr = addr;
        fault->addr_valid = true;
    }
}

/**
 * Check if access is allowed for a single address using default map or region.
 */
static bool check_single_addr(const MPU *mpu, int region, uint32_t addr,
                              bool is_write, bool is_fetch, bool privileged)
{
    if (region < 0) {
        /* No region - check default map if enabled */
        if (mpu->privdefena && privileged) {
            return check_default_map(addr, is_write, is_fetch);
        }
        return false;
    }
    return check_permission(&mpu->regions[region], is_write, is_fetch, privileged);
}

/**
 * Handle access that spans two different regions.
 */
static bool check_spanning_access(const MPU *mpu, int region, int end_region,
                                  uint32_t addr, uint32_t end_addr,
                                  bool is_write, bool is_fetch, bool privileged)
{
    /* Check start address */
    if (!check_single_addr(mpu, region, addr, is_write, is_fetch, privileged)) {
        return false;
    }

    /* Check end address */
    if (!check_single_addr(mpu, end_region, end_addr, is_write, is_fetch, privileged)) {
        return false;
    }

    return true;
}

/*============================================================================
 * MPU API Implementation
 *============================================================================*/

void armv8m_mpu_init(MPU *mpu, int num_regions)
{
    memset(mpu, 0, sizeof(MPU));

    if (num_regions > MPU_MAX_REGIONS) {
        num_regions = MPU_MAX_REGIONS;
    }
    if (num_regions < 0) {
        num_regions = 0;
    }

    mpu->num_regions = num_regions;
    mpu->enabled = false;
    mpu->privdefena = false;
    mpu->hfnmiena = false;
    mpu->ctrl = 0;
    mpu->rnr = 0;
    mpu->mair0 = 0;
    mpu->mair1 = 0;
}

void armv8m_mpu_reset(MPU *mpu)
{
    int num_regions = mpu->num_regions;
    armv8m_mpu_init(mpu, num_regions);
}

bool armv8m_mpu_check(const MPU *mpu, uint32_t addr, uint32_t size,
                      bool is_write, bool is_fetch, bool privileged,
                      bool in_hardfault_nmi, MPUFaultInfo *fault)
{
    /* Clear fault info */
    if (fault) {
        fault->type = MPU_FAULT_NONE;
        fault->addr = 0;
        fault->addr_valid = false;
    }

    /* If MPU is disabled, all accesses are permitted */
    if (!mpu->enabled) {
        return true;
    }

    /*
     * If in HardFault/NMI handler and HFNMIENA is not set,
     * MPU is bypassed - all accesses permitted.
     */
    if (in_hardfault_nmi && !mpu->hfnmiena) {
        return true;
    }

    /* Validate size - zero-size access is invalid */
    if (size == 0) {
        set_fault_info(fault, is_fetch, addr);
        return false;
    }

    /* Find region for start address */
    int region = find_region(mpu, addr);

    /* If access spans multiple bytes, also check end address */
    if (size > 1) {
        /* Check for address overflow */
        uint32_t end_addr;
        if (addr > UINT32_MAX - size + 1) {
            /* Overflow - access wraps address space, deny */
            set_fault_info(fault, is_fetch, addr);
            return false;
        }
        end_addr = addr + size - 1;
        int end_region = find_region(mpu, end_addr);

        if (region != end_region) {
            /* Access spans regions - check both */
            if (!check_spanning_access(mpu, region, end_region, addr, end_addr,
                                       is_write, is_fetch, privileged)) {
                set_fault_info(fault, is_fetch, addr);
                return false;
            }
            return true;
        }
    }

    /* Single region access */
    if (!check_single_addr(mpu, region, addr, is_write, is_fetch, privileged)) {
        set_fault_info(fault, is_fetch, addr);
        return false;
    }

    return true;
}

int armv8m_mpu_configure_region(MPU *mpu, int region,
                                 uint32_t base, uint32_t limit,
                                 MPUAccessPerm ap, bool xn,
                                 MPUShareability sh, int attr_idx,
                                 bool enable)
{
    /* Validate region number */
    if (region < 0 || region >= mpu->num_regions) {
        return ARMV8M_ERR_UNPREDICTABLE;
    }

    /* Validate attribute index */
    if (attr_idx < 0 || attr_idx > 7) {
        return ARMV8M_ERR_UNPREDICTABLE;
    }

    /* Build RBAR value */
    uint32_t rbar = (base & MPU_RBAR_BASE_MASK) |
                    ((uint32_t)sh << MPU_RBAR_SH_SHIFT) |
                    ((uint32_t)ap << MPU_RBAR_AP_SHIFT) |
                    (xn ? MPU_RBAR_XN : 0);

    /* Build RLAR value */
    uint32_t rlar = (limit & MPU_RLAR_LIMIT_MASK) |
                    ((uint32_t)attr_idx << MPU_RLAR_ATTRINDX_SHIFT) |
                    (enable ? MPU_RLAR_EN : 0);

    mpu->regions[region].rbar = rbar;
    mpu->regions[region].rlar = rlar;

    return ARMV8M_OK;
}

void armv8m_mpu_enable(MPU *mpu, bool enable, bool hfnmiena, bool privdefena)
{
    mpu->enabled = enable;
    mpu->hfnmiena = hfnmiena;
    mpu->privdefena = privdefena;

    mpu->ctrl = (enable ? MPU_CTRL_ENABLE : 0) |
                (hfnmiena ? MPU_CTRL_HFNMIENA : 0) |
                (privdefena ? MPU_CTRL_PRIVDEFENA : 0);
}

uint32_t armv8m_mpu_read(MPU *mpu, uint32_t offset, uint8_t size)
{
    (void)size;  /* All MPU registers are word-sized */

    switch (offset) {
        case MPU_TYPE:
            /* TYPE register:
             * [15:8] DREGION: Number of regions
             * [0] SEPARATE: 0 = unified MPU
             */
            return ((uint32_t)mpu->num_regions << 8);

        case MPU_CTRL:
            return mpu->ctrl;

        case MPU_RNR:
            return mpu->rnr;

        case MPU_RBAR:
            if (mpu->rnr < (uint32_t)mpu->num_regions) {
                return mpu->regions[mpu->rnr].rbar;
            }
            return 0;

        case MPU_RLAR:
            if (mpu->rnr < (uint32_t)mpu->num_regions) {
                return mpu->regions[mpu->rnr].rlar;
            }
            return 0;

        case MPU_RBAR_A1:
            if ((mpu->rnr + 1) < (uint32_t)mpu->num_regions) {
                return mpu->regions[mpu->rnr + 1].rbar;
            }
            return 0;

        case MPU_RLAR_A1:
            if ((mpu->rnr + 1) < (uint32_t)mpu->num_regions) {
                return mpu->regions[mpu->rnr + 1].rlar;
            }
            return 0;

        case MPU_RBAR_A2:
            if ((mpu->rnr + 2) < (uint32_t)mpu->num_regions) {
                return mpu->regions[mpu->rnr + 2].rbar;
            }
            return 0;

        case MPU_RLAR_A2:
            if ((mpu->rnr + 2) < (uint32_t)mpu->num_regions) {
                return mpu->regions[mpu->rnr + 2].rlar;
            }
            return 0;

        case MPU_RBAR_A3:
            if ((mpu->rnr + 3) < (uint32_t)mpu->num_regions) {
                return mpu->regions[mpu->rnr + 3].rbar;
            }
            return 0;

        case MPU_RLAR_A3:
            if ((mpu->rnr + 3) < (uint32_t)mpu->num_regions) {
                return mpu->regions[mpu->rnr + 3].rlar;
            }
            return 0;

        case MPU_MAIR0:
            return mpu->mair0;

        case MPU_MAIR1:
            return mpu->mair1;

        default:
            return 0;
    }
}

void armv8m_mpu_write(MPU *mpu, uint32_t offset, uint32_t value, uint8_t size)
{
    (void)size;  /* All MPU registers are word-sized */

    switch (offset) {
        case MPU_TYPE:
            /* TYPE register is read-only */
            break;

        case MPU_CTRL:
            mpu->ctrl = value & (MPU_CTRL_ENABLE | MPU_CTRL_HFNMIENA | MPU_CTRL_PRIVDEFENA);
            mpu->enabled = (value & MPU_CTRL_ENABLE) != 0;
            mpu->hfnmiena = (value & MPU_CTRL_HFNMIENA) != 0;
            mpu->privdefena = (value & MPU_CTRL_PRIVDEFENA) != 0;
            break;

        case MPU_RNR:
            if (value < (uint32_t)mpu->num_regions) {
                mpu->rnr = value;
            }
            break;

        case MPU_RBAR:
            if (mpu->rnr < (uint32_t)mpu->num_regions) {
                mpu->regions[mpu->rnr].rbar = value;
            }
            break;

        case MPU_RLAR:
            if (mpu->rnr < (uint32_t)mpu->num_regions) {
                mpu->regions[mpu->rnr].rlar = value;
            }
            break;

        case MPU_RBAR_A1:
            if ((mpu->rnr + 1) < (uint32_t)mpu->num_regions) {
                mpu->regions[mpu->rnr + 1].rbar = value;
            }
            break;

        case MPU_RLAR_A1:
            if ((mpu->rnr + 1) < (uint32_t)mpu->num_regions) {
                mpu->regions[mpu->rnr + 1].rlar = value;
            }
            break;

        case MPU_RBAR_A2:
            if ((mpu->rnr + 2) < (uint32_t)mpu->num_regions) {
                mpu->regions[mpu->rnr + 2].rbar = value;
            }
            break;

        case MPU_RLAR_A2:
            if ((mpu->rnr + 2) < (uint32_t)mpu->num_regions) {
                mpu->regions[mpu->rnr + 2].rlar = value;
            }
            break;

        case MPU_RBAR_A3:
            if ((mpu->rnr + 3) < (uint32_t)mpu->num_regions) {
                mpu->regions[mpu->rnr + 3].rbar = value;
            }
            break;

        case MPU_RLAR_A3:
            if ((mpu->rnr + 3) < (uint32_t)mpu->num_regions) {
                mpu->regions[mpu->rnr + 3].rlar = value;
            }
            break;

        case MPU_MAIR0:
            mpu->mair0 = value;
            break;

        case MPU_MAIR1:
            mpu->mair1 = value;
            break;

        default:
            break;
    }
}

uint8_t armv8m_mpu_get_attributes(const MPU *mpu, uint32_t addr, bool privileged)
{
    /* If MPU disabled, return default attributes */
    if (!mpu->enabled) {
        return 0;
    }

    int region = find_region(mpu, addr);

    if (region < 0) {
        /* No matching region */
        if (mpu->privdefena && privileged) {
            /* Use default attributes - return 0 for simplicity */
            return 0;
        }
        return 0;
    }

    /* Get attribute index from RLAR */
    uint32_t attr_idx = (mpu->regions[region].rlar >> MPU_RLAR_ATTRINDX_SHIFT) & MPU_RLAR_ATTRINDX_MASK;

    /* Get attribute byte from MAIR0 or MAIR1 */
    if (attr_idx < 4) {
        return (uint8_t)((mpu->mair0 >> (attr_idx * 8)) & 0xFF);
    }
    return (uint8_t)((mpu->mair1 >> ((attr_idx - 4) * 8)) & 0xFF);
}
