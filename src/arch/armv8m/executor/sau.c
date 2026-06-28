/**
 * @file sau.c
 * @brief Memory-mapped register access for the ARMv8-M SAU.
 *
 * The SAU control registers live in the System Control Space at 0xE000EDD0.
 * The emulator wires these to the executor's SAUState via the memory
 * subsystem (see sau_mmio_read/write in armv8m_emulator.c). Secure-only access
 * is not enforced here; the model assumes the programming code runs in Secure
 * state (which reset establishes when TrustZone is present).
 */

#include "arch/armv8m/armv8m_executor.h"
#include "arch/armv8m/armv8m_types.h"

uint32_t armv8m_sau_read(const SAUState *sau, uint32_t offset, uint8_t size) {
  (void)size; /* SAU registers are word-sized */

  switch (offset) {
  case ARMV8M_SAU_REG_CTRL:
    return sau->ctrl;
  case ARMV8M_SAU_REG_TYPE:
    /* TYPE[15:8] = number of implemented regions (read-only). */
    return (uint32_t)ARMV8M_SAU_REGIONS_MAX << 8;
  case ARMV8M_SAU_REG_RNR:
    return sau->rnr;
  case ARMV8M_SAU_REG_RBAR:
    if (sau->rnr < ARMV8M_SAU_REGIONS_MAX) {
      return sau->regions[sau->rnr].rbar;
    }
    return 0;
  case ARMV8M_SAU_REG_RLAR:
    if (sau->rnr < ARMV8M_SAU_REGIONS_MAX) {
      return sau->regions[sau->rnr].rlar;
    }
    return 0;
  default:
    return 0;
  }
}

void armv8m_sau_write(SAUState *sau, uint32_t offset, uint32_t value,
                      uint8_t size) {
  (void)size; /* SAU registers are word-sized */

  switch (offset) {
  case ARMV8M_SAU_REG_CTRL:
    sau->ctrl = value & (ARMV8M_SAU_CTRL_ENABLE | ARMV8M_SAU_CTRL_ALLNS);
    break;
  case ARMV8M_SAU_REG_TYPE:
    /* Read-only. */
    break;
  case ARMV8M_SAU_REG_RNR:
    if (value < ARMV8M_SAU_REGIONS_MAX) {
      sau->rnr = value;
    }
    break;
  case ARMV8M_SAU_REG_RBAR:
    if (sau->rnr < ARMV8M_SAU_REGIONS_MAX) {
      sau->regions[sau->rnr].rbar = value;
    }
    break;
  case ARMV8M_SAU_REG_RLAR:
    if (sau->rnr < ARMV8M_SAU_REGIONS_MAX) {
      sau->regions[sau->rnr].rlar = value;
    }
    break;
  default:
    break;
  }
}
