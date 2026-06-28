/**
 * @file nvic.c
 * @brief Nested Vectored Interrupt Controller implementation for ARMv8-M
 */

#include "arch/armv8m/armv8m_nvic.h"
#include "emu/emu_log.h"
#include <string.h>

/*============================================================================
 * Internal Constants
 *============================================================================*/

/* Fixed exception priorities */
#define PRIORITY_RESET (-3)
#define PRIORITY_NMI (-2)
#define PRIORITY_HARDFAULT (-1)

/* Priority mask based on implemented bits (3 bits = 0xE0) */
#define PRIORITY_MASK 0xE0u

/* AIRCR key for writes */
#define AIRCR_VECTKEY 0x05FAu
#define AIRCR_VECTKEY_SHIFT 16
#define AIRCR_PRIGROUP_MASK 0x700u
#define AIRCR_PRIGROUP_SHIFT 8

/* SHCSR bits */
#define SHCSR_MEMFAULTENA (1u << 16)
#define SHCSR_BUSFAULTENA (1u << 17)
#define SHCSR_USGFAULTENA (1u << 18)
#define SHCSR_MEMFAULTACT (1u << 0)
#define SHCSR_BUSFAULTACT (1u << 1)
#define SHCSR_HARDFAULTACT (1u << 2)
#define SHCSR_USGFAULTACT (1u << 3)
#define SHCSR_SVCALLACT (1u << 7)
#define SHCSR_MONITORACT (1u << 8)
#define SHCSR_PENDSVACT (1u << 10)
#define SHCSR_SYSTICKACT (1u << 11)
#define SHCSR_USGFAULTPENDED (1u << 12)
#define SHCSR_MEMFAULTPENDED (1u << 13)
#define SHCSR_BUSFAULTPENDED (1u << 14)
#define SHCSR_SVCALLPENDED (1u << 15)

/* Internal pending tracking (using upper bits of shcsr) */
#define INTERNAL_HARDFAULT_PENDING (1u << 27)
#define INTERNAL_PENDSV_PENDING (1u << 28)
#define INTERNAL_SYSTICK_PENDING (1u << 29)
#define INTERNAL_DEBUGMON_PENDING (1u << 30)
#define INTERNAL_NMI_PENDING (1u << 31)

/* ICSR bits */
#define ICSR_VECTACTIVE_MASK 0x1FFu
#define ICSR_VECTPENDING_MASK (0x1FFu << 12)
#define ICSR_VECTPENDING_SHIFT 12
#define ICSR_ISRPENDING (1u << 22)
#define ICSR_PENDSTCLR (1u << 25)
#define ICSR_PENDSTSET (1u << 26)
#define ICSR_PENDSVCLR (1u << 27)
#define ICSR_PENDSVSET (1u << 28)
#define ICSR_NMIPENDSET (1u << 31)

/*============================================================================
 * Internal Helper Functions
 *============================================================================*/

/**
 * Get bit index for an IRQ within its register.
 */
static inline int irq_bit(int irq) { return irq & 31; }

/**
 * Get register index for an IRQ.
 */
static inline int irq_reg(int irq) { return irq >> 5; }

/**
 * Check if an IRQ number is valid.
 */
static inline bool irq_valid(const NVIC *nvic, int irq) {
  return irq >= 0 && irq < nvic->num_irqs;
}

/**
 * Get the priority of an exception.
 * Returns the effective priority considering fixed priorities.
 */
static int get_exception_priority(const NVIC *nvic, int exc) {
  if (exc == ARMV8M_EXC_RESET) {
    return PRIORITY_RESET;
  }
  if (exc == ARMV8M_EXC_NMI) {
    return PRIORITY_NMI;
  }
  if (exc == ARMV8M_EXC_HARDFAULT) {
    return PRIORITY_HARDFAULT;
  }

  if (exc >= 4 && exc <= 15) {
    /* System handlers with configurable priority */
    return (int)nvic->shpr[exc - 4];
  }

  return 256; /* Invalid exception, lowest priority */
}

/**
 * Get the group priority (for preemption comparison) based on PRIGROUP.
 * PRIGROUP determines how the priority field is split between group and
 * subpriority. Only group priority is used for preemption; subpriority is for
 * ordering within same group.
 */
static int get_group_priority(const NVIC *nvic, int raw_priority) {
  /* PRIGROUP determines the binary point position:
   * prigroup=0: all 8 bits are group priority
   * prigroup=7: 1 bit group, 7 bits subpriority
   *
   * With 3 implemented bits (priority values 0x00, 0x20, 0x40, ..., 0xE0),
   * group_bits ranges from 0 to 3 effectively:
   * prigroup >= 4: all 3 implemented bits are group priority
   * prigroup = 5: 2 bits group, 1 bit subpriority
   * prigroup = 6: 1 bit group, 2 bits subpriority
   * prigroup = 7: 0 bits group, 3 bits subpriority
   */
  int group_bits = 7 - nvic->prigroup;
  if (group_bits > NVIC_PRIORITY_BITS) {
    group_bits = NVIC_PRIORITY_BITS;
  }
  if (group_bits < 0) {
    group_bits = 0;
  }

  /* Shift to get only the group priority bits */
  int shift = 8 - group_bits;
  return (raw_priority >> shift) << shift;
}

/**
 * Check if an exception is pending.
 */
static bool is_exception_pending(const NVIC *nvic, int exc) {
  if (exc == ARMV8M_EXC_NMI) {
    return (nvic->shcsr & INTERNAL_NMI_PENDING) != 0;
  }

  if (exc == ARMV8M_EXC_HARDFAULT) {
    return (nvic->shcsr & INTERNAL_HARDFAULT_PENDING) != 0;
  }

  if (exc >= 4 && exc <= 15) {
    /* System exceptions tracked via SHCSR bits */
    switch (exc) {
    case ARMV8M_EXC_MEMMANAGE:
      return (nvic->shcsr & SHCSR_MEMFAULTPENDED) != 0;
    case ARMV8M_EXC_BUSFAULT:
      return (nvic->shcsr & SHCSR_BUSFAULTPENDED) != 0;
    case ARMV8M_EXC_USAGEFAULT:
      return (nvic->shcsr & SHCSR_USGFAULTPENDED) != 0;
    case ARMV8M_EXC_SVCALL:
      return (nvic->shcsr & SHCSR_SVCALLPENDED) != 0;
    case ARMV8M_EXC_DEBUGMON:
      return (nvic->shcsr & INTERNAL_DEBUGMON_PENDING) != 0;
    case ARMV8M_EXC_PENDSV:
      return (nvic->shcsr & INTERNAL_PENDSV_PENDING) != 0;
    case ARMV8M_EXC_SYSTICK:
      return (nvic->shcsr & INTERNAL_SYSTICK_PENDING) != 0;
    default:
      return false;
    }
  }

  return false;
}

/**
 * Check if an exception is active.
 */
static bool is_exception_active(const NVIC *nvic, int exc) {
  /* Handle HardFault (3) specially */
  if (exc == ARMV8M_EXC_HARDFAULT) {
    return (nvic->shcsr & SHCSR_HARDFAULTACT) != 0;
  }

  if (exc >= 4 && exc <= 15) {
    uint32_t mask = 0;
    switch (exc) {
    case ARMV8M_EXC_MEMMANAGE:
      mask = SHCSR_MEMFAULTACT;
      break;
    case ARMV8M_EXC_BUSFAULT:
      mask = SHCSR_BUSFAULTACT;
      break;
    case ARMV8M_EXC_USAGEFAULT:
      mask = SHCSR_USGFAULTACT;
      break;
    case ARMV8M_EXC_SVCALL:
      mask = SHCSR_SVCALLACT;
      break;
    case ARMV8M_EXC_DEBUGMON:
      mask = SHCSR_MONITORACT;
      break;
    case ARMV8M_EXC_PENDSV:
      mask = SHCSR_PENDSVACT;
      break;
    case ARMV8M_EXC_SYSTICK:
      mask = SHCSR_SYSTICKACT;
      break;
    default:
      return false;
    }
    return (nvic->shcsr & mask) != 0;
  }

  if (exc >= ARMV8M_EXC_EXTERNAL_BASE) {
    int irq = exc - ARMV8M_EXC_EXTERNAL_BASE;
    if (irq < nvic->num_irqs) {
      return (nvic->active[irq_reg(irq)] & (1u << irq_bit(irq))) != 0;
    }
  }

  return false;
}

/**
 * Check if an external interrupt is enabled.
 */
static bool is_irq_enabled(const NVIC *nvic, int irq) {
  if (!irq_valid(nvic, irq)) {
    return false;
  }
  return (nvic->enabled[irq_reg(irq)] & (1u << irq_bit(irq))) != 0;
}

/**
 * Set exception active state.
 */
static void set_exception_active(NVIC *nvic, int exc, bool active) {
  /* Handle HardFault (3) specially */
  if (exc == ARMV8M_EXC_HARDFAULT) {
    if (active) {
      nvic->shcsr |= SHCSR_HARDFAULTACT;
    } else {
      nvic->shcsr &= ~SHCSR_HARDFAULTACT;
    }
    return;
  }

  if (exc >= 4 && exc <= 15) {
    uint32_t mask = 0;
    switch (exc) {
    case ARMV8M_EXC_MEMMANAGE:
      mask = SHCSR_MEMFAULTACT;
      break;
    case ARMV8M_EXC_BUSFAULT:
      mask = SHCSR_BUSFAULTACT;
      break;
    case ARMV8M_EXC_USAGEFAULT:
      mask = SHCSR_USGFAULTACT;
      break;
    case ARMV8M_EXC_SVCALL:
      mask = SHCSR_SVCALLACT;
      break;
    case ARMV8M_EXC_DEBUGMON:
      mask = SHCSR_MONITORACT;
      break;
    case ARMV8M_EXC_PENDSV:
      mask = SHCSR_PENDSVACT;
      break;
    case ARMV8M_EXC_SYSTICK:
      mask = SHCSR_SYSTICKACT;
      break;
    default:
      return;
    }

    if (active) {
      nvic->shcsr |= mask;
    } else {
      nvic->shcsr &= ~mask;
    }
    return;
  }

  if (exc >= ARMV8M_EXC_EXTERNAL_BASE) {
    int irq = exc - ARMV8M_EXC_EXTERNAL_BASE;
    if (irq < nvic->num_irqs) {
      if (active) {
        nvic->active[irq_reg(irq)] |= (1u << irq_bit(irq));
      } else {
        nvic->active[irq_reg(irq)] &= ~(1u << irq_bit(irq));
      }
    }
  }
}

/*============================================================================
 * Public API Implementation
 *============================================================================*/

void armv8m_nvic_init(NVIC *nvic, int num_irqs) {
  if (nvic == NULL) {
    return;
  }
  memset(nvic, 0, sizeof(*nvic));

  if (num_irqs > NVIC_MAX_EXTERNAL_IRQS) {
    num_irqs = NVIC_MAX_EXTERNAL_IRQS;
  }
  if (num_irqs < 0) {
    num_irqs = 0;
  }

  nvic->num_irqs = num_irqs;
  nvic->highest_pending = -1;
  nvic->need_rescan = true;

  /* Default CCR value with STKALIGN set */
  nvic->ccr = (1u << 9); /* STKALIGN */

  /* Default AIRCR with PRIGROUP = 0 */
  nvic->aircr = 0;
  nvic->prigroup = 0;
}

void armv8m_nvic_reset(NVIC *nvic) {
  int num_irqs = nvic->num_irqs;
  armv8m_nvic_init(nvic, num_irqs);
}

void armv8m_nvic_set_pending(NVIC *nvic, int irq) {
  if (!irq_valid(nvic, irq)) {
    return;
  }

  EMU_LOG_INFO(EMU_LOG_CAT_NVIC, "IRQ %d pending", irq);
  nvic->pending[irq_reg(irq)] |= (1u << irq_bit(irq));
  nvic->need_rescan = true;
}

void armv8m_nvic_clear_pending(NVIC *nvic, int irq) {
  if (!irq_valid(nvic, irq)) {
    return;
  }

  EMU_LOG_DEBUG(EMU_LOG_CAT_NVIC, "IRQ %d cleared", irq);
  nvic->pending[irq_reg(irq)] &= ~(1u << irq_bit(irq));
  nvic->need_rescan = true;
}

void armv8m_nvic_set_exception_pending(NVIC *nvic, int exc) {
  if (exc == ARMV8M_EXC_NMI) {
    nvic->shcsr |= INTERNAL_NMI_PENDING;
    nvic->need_rescan = true;
    return;
  }

  if (exc == ARMV8M_EXC_HARDFAULT) {
    nvic->shcsr |= INTERNAL_HARDFAULT_PENDING;
    nvic->need_rescan = true;
    return;
  }

  switch (exc) {
  case ARMV8M_EXC_MEMMANAGE:
    nvic->shcsr |= SHCSR_MEMFAULTPENDED;
    break;
  case ARMV8M_EXC_BUSFAULT:
    nvic->shcsr |= SHCSR_BUSFAULTPENDED;
    break;
  case ARMV8M_EXC_USAGEFAULT:
    nvic->shcsr |= SHCSR_USGFAULTPENDED;
    break;
  case ARMV8M_EXC_SVCALL:
    nvic->shcsr |= SHCSR_SVCALLPENDED;
    break;
  case ARMV8M_EXC_DEBUGMON:
    nvic->shcsr |= INTERNAL_DEBUGMON_PENDING;
    break;
  case ARMV8M_EXC_PENDSV:
    nvic->shcsr |= INTERNAL_PENDSV_PENDING;
    break;
  case ARMV8M_EXC_SYSTICK:
    nvic->shcsr |= INTERNAL_SYSTICK_PENDING;
    break;
  default:
    break;
  }

  nvic->need_rescan = true;
}

void armv8m_nvic_clear_exception_pending(NVIC *nvic, int exc) {
  if (exc == ARMV8M_EXC_NMI) {
    nvic->shcsr &= ~INTERNAL_NMI_PENDING;
    nvic->need_rescan = true;
    return;
  }

  if (exc == ARMV8M_EXC_HARDFAULT) {
    nvic->shcsr &= ~INTERNAL_HARDFAULT_PENDING;
    nvic->need_rescan = true;
    return;
  }

  switch (exc) {
  case ARMV8M_EXC_MEMMANAGE:
    nvic->shcsr &= ~SHCSR_MEMFAULTPENDED;
    break;
  case ARMV8M_EXC_BUSFAULT:
    nvic->shcsr &= ~SHCSR_BUSFAULTPENDED;
    break;
  case ARMV8M_EXC_USAGEFAULT:
    nvic->shcsr &= ~SHCSR_USGFAULTPENDED;
    break;
  case ARMV8M_EXC_SVCALL:
    nvic->shcsr &= ~SHCSR_SVCALLPENDED;
    break;
  case ARMV8M_EXC_DEBUGMON:
    nvic->shcsr &= ~INTERNAL_DEBUGMON_PENDING;
    break;
  case ARMV8M_EXC_PENDSV:
    nvic->shcsr &= ~INTERNAL_PENDSV_PENDING;
    break;
  case ARMV8M_EXC_SYSTICK:
    nvic->shcsr &= ~INTERNAL_SYSTICK_PENDING;
    break;
  default:
    break;
  }

  nvic->need_rescan = true;
}

void armv8m_nvic_enable_irq(NVIC *nvic, int irq) {
  if (!irq_valid(nvic, irq)) {
    return;
  }

  nvic->enabled[irq_reg(irq)] |= (1u << irq_bit(irq));
  nvic->need_rescan = true;
}

void armv8m_nvic_disable_irq(NVIC *nvic, int irq) {
  if (!irq_valid(nvic, irq)) {
    return;
  }

  nvic->enabled[irq_reg(irq)] &= ~(1u << irq_bit(irq));
  nvic->need_rescan = true;
}

void armv8m_nvic_set_priority(NVIC *nvic, int irq, uint8_t priority) {
  if (!irq_valid(nvic, irq)) {
    return;
  }

  /* Mask to implemented priority bits */
  nvic->priority[irq] = priority & PRIORITY_MASK;
  nvic->need_rescan = true;
}

uint8_t armv8m_nvic_get_priority(const NVIC *nvic, int irq) {
  if (!irq_valid(nvic, irq)) {
    return 0;
  }

  return nvic->priority[irq];
}

void armv8m_nvic_set_exception_priority(NVIC *nvic, int exc, uint8_t priority) {
  if (exc < 4 || exc > 15) {
    return;
  }

  /* Mask to implemented priority bits */
  nvic->shpr[exc - 4] = priority & PRIORITY_MASK;
  nvic->need_rescan = true;
}

/* Scan pending system exceptions (2-15), updating the best (exc, pri). */
static void nvic_scan_system_exceptions(const NVIC *nvic, uint8_t basepri,
                                        int current_pri, int *best_exc,
                                        int *best_pri) {
  for (int exc = 2; exc <= 15; exc++) {
    /* Skip reserved exception numbers */
    if (exc == 8 || exc == 9 || exc == 10 || exc == 13) {
      continue;
    }

    if (!is_exception_pending(nvic, exc)) {
      continue;
    }

    int pri = get_exception_priority(nvic, exc);

    /* Check BASEPRI masking for configurable priority exceptions */
    if (pri >= 0 && basepri != 0 && (uint8_t)pri >= basepri) {
      continue;
    }

    /* Check if can preempt current priority - use group priority for preemption
     */
    int group_pri = (pri >= 0) ? get_group_priority(nvic, pri) : pri;
    if (group_pri >= current_pri) {
      continue;
    }

    if (pri < *best_pri) {
      *best_exc = exc;
      *best_pri = pri;
    }
  }
}

/* Scan pending external interrupts, updating the best (exc, pri). */
static void nvic_scan_external_irqs(NVIC *nvic, uint8_t basepri,
                                    int current_pri, int *best_exc,
                                    int *best_pri) {
  for (int irq = 0; irq < nvic->num_irqs; irq++) {
    if (!is_irq_enabled(nvic, irq)) {
      continue;
    }

    if ((nvic->pending[irq_reg(irq)] & (1u << irq_bit(irq))) == 0) {
      continue;
    }

    int pri = (int)nvic->priority[irq];

    /* Check BASEPRI masking */
    if (basepri != 0 && (uint8_t)pri >= basepri) {
      continue;
    }

    /* Check if can preempt current priority - use group priority for preemption
     */
    int group_pri = get_group_priority(nvic, pri);
    if (group_pri >= current_pri) {
      continue;
    }

    if (pri < *best_pri) {
      *best_exc = ARMV8M_EXC_EXTERNAL_BASE + irq;
      *best_pri = pri;
    }
  }
}

int armv8m_nvic_get_pending_exception(NVIC *nvic, uint8_t basepri,
                                      uint8_t primask, uint8_t faultmask,
                                      int current_pri) {
  int best_exc = -1;
  int best_pri = 256;

  /* FAULTMASK blocks everything except NMI */
  if (faultmask) {
    /* Check NMI */
    if (is_exception_pending(nvic, ARMV8M_EXC_NMI)) {
      return ARMV8M_EXC_NMI;
    }
    return -1;
  }

  /* Check HardFault - blocked by FAULTMASK (already checked) but NOT by PRIMASK
   */
  if (is_exception_pending(nvic, ARMV8M_EXC_HARDFAULT)) {
    /* HardFault has fixed priority -1, can preempt unless current_pri <= -1 */
    if (PRIORITY_HARDFAULT < current_pri) {
      return ARMV8M_EXC_HARDFAULT;
    }
  }

  /* PRIMASK blocks all configurable-priority exceptions */
  if (primask) {
    /* Only NMI and HardFault can preempt (HardFault checked above) */
    if (is_exception_pending(nvic, ARMV8M_EXC_NMI)) {
      return ARMV8M_EXC_NMI;
    }
    return -1;
  }

  /* Check pending system exceptions (2-15) and external interrupts */
  nvic_scan_system_exceptions(nvic, basepri, current_pri, &best_exc, &best_pri);
  nvic_scan_external_irqs(nvic, basepri, current_pri, &best_exc, &best_pri);

  nvic->highest_pending = best_exc;
  return best_exc;
}

void armv8m_nvic_acknowledge(NVIC *nvic, int exc) {
  /* Mark exception as active */
  set_exception_active(nvic, exc, true);

  /* Clear pending for external interrupts */
  if (exc >= ARMV8M_EXC_EXTERNAL_BASE) {
    int irq = exc - ARMV8M_EXC_EXTERNAL_BASE;
    armv8m_nvic_clear_pending(nvic, irq);
  }

  /* Clear pending for NMI and HardFault */
  if (exc == ARMV8M_EXC_NMI) {
    nvic->shcsr &= ~INTERNAL_NMI_PENDING;
  } else if (exc == ARMV8M_EXC_HARDFAULT) {
    nvic->shcsr &= ~INTERNAL_HARDFAULT_PENDING;
  }

  /* Clear pending for certain system exceptions */
  switch (exc) {
  case ARMV8M_EXC_MEMMANAGE:
    nvic->shcsr &= ~SHCSR_MEMFAULTPENDED;
    break;
  case ARMV8M_EXC_BUSFAULT:
    nvic->shcsr &= ~SHCSR_BUSFAULTPENDED;
    break;
  case ARMV8M_EXC_USAGEFAULT:
    nvic->shcsr &= ~SHCSR_USGFAULTPENDED;
    break;
  case ARMV8M_EXC_SVCALL:
    nvic->shcsr &= ~SHCSR_SVCALLPENDED;
    break;
  case ARMV8M_EXC_DEBUGMON:
    nvic->shcsr &= ~INTERNAL_DEBUGMON_PENDING;
    break;
  case ARMV8M_EXC_PENDSV:
    nvic->shcsr &= ~INTERNAL_PENDSV_PENDING;
    break;
  case ARMV8M_EXC_SYSTICK:
    nvic->shcsr &= ~INTERNAL_SYSTICK_PENDING;
    break;
  default:
    break;
  }

  nvic->need_rescan = true;
}

void armv8m_nvic_deactivate(NVIC *nvic, int exc) {
  set_exception_active(nvic, exc, false);
  nvic->need_rescan = true;
}

/*============================================================================
 * NVIC Register Access (MMIO)
 *============================================================================*/

/* Read one 32-bit register from a banked NVIC array (ISER/ICER/ISPR/...). */
static uint32_t nvic_read_bank(const uint32_t *bank, uint32_t off_in_bank) {
  int idx = (int)(off_in_bank >> 2);
  return (idx < 8) ? bank[idx] : 0;
}

/* Read an IPR (interrupt priority) register, byte- or word-accessible. */
static uint32_t nvic_read_ipr(const NVIC *nvic, uint32_t offset, uint8_t size) {
  int base_irq = (int)(offset - NVIC_IPR_BASE);
  if (size == 1) {
    return (base_irq < nvic->num_irqs) ? nvic->priority[base_irq] : 0;
  }
  /* Word access - pack 4 priorities */
  uint32_t value = 0;
  for (int i = 0; i < 4; i++) {
    int irq = base_irq + i;
    if (irq < nvic->num_irqs) {
      value |= (uint32_t)nvic->priority[irq] << (i * 8);
    }
  }
  return value;
}

uint32_t armv8m_nvic_read(const NVIC *nvic, uint32_t offset, uint8_t size) {
  if (offset < NVIC_ISER_BASE + 0x20) {
    /* ISER0-7: Interrupt Set Enable */
    return nvic_read_bank(nvic->enabled, offset);
  }
  if (offset >= NVIC_ICER_BASE && offset < NVIC_ICER_BASE + 0x20) {
    /* ICER0-7: Interrupt Clear Enable (reads as ISER) */
    return nvic_read_bank(nvic->enabled, offset - NVIC_ICER_BASE);
  }
  if (offset >= NVIC_ISPR_BASE && offset < NVIC_ISPR_BASE + 0x20) {
    /* ISPR0-7: Interrupt Set Pending */
    return nvic_read_bank(nvic->pending, offset - NVIC_ISPR_BASE);
  }
  if (offset >= NVIC_ICPR_BASE && offset < NVIC_ICPR_BASE + 0x20) {
    /* ICPR0-7: Interrupt Clear Pending (reads as ISPR) */
    return nvic_read_bank(nvic->pending, offset - NVIC_ICPR_BASE);
  }
  if (offset >= NVIC_IABR_BASE && offset < NVIC_IABR_BASE + 0x20) {
    /* IABR0-7: Interrupt Active Bit (read-only) */
    return nvic_read_bank(nvic->active, offset - NVIC_IABR_BASE);
  }
  if (offset >= NVIC_IPR_BASE && offset < NVIC_IPR_BASE + 0xF0) {
    /* IPR0-59: Interrupt Priority (byte accessible) */
    return nvic_read_ipr(nvic, offset, size);
  }
  return 0;
}

/* Write one 32-bit register in a banked NVIC array. `set` ORs the value (ISER/
 * ISPR); otherwise it clears the set bits (ICER/ICPR). */
static void nvic_write_bank(uint32_t *bank, uint32_t off_in_bank,
                            uint32_t value, bool set, bool *need_rescan) {
  int idx = (int)(off_in_bank >> 2);
  if (idx < 8) {
    if (set) {
      bank[idx] |= value;
    } else {
      bank[idx] &= ~value;
    }
    *need_rescan = true;
  }
}

/* Write an IPR (interrupt priority) register, byte- or word-accessible. */
static void nvic_write_ipr(NVIC *nvic, uint32_t offset, uint32_t value,
                           uint8_t size) {
  int base_irq = (int)(offset - NVIC_IPR_BASE);
  if (size == 1) {
    if (base_irq < nvic->num_irqs) {
      nvic->priority[base_irq] = (uint8_t)(value & PRIORITY_MASK);
    }
  } else {
    /* Word access - unpack 4 priorities */
    for (int i = 0; i < 4; i++) {
      int irq = base_irq + i;
      if (irq < nvic->num_irqs) {
        nvic->priority[irq] = (uint8_t)((value >> (i * 8)) & PRIORITY_MASK);
      }
    }
  }
  nvic->need_rescan = true;
}

void armv8m_nvic_write(NVIC *nvic, uint32_t offset, uint32_t value,
                       uint8_t size) {
  if (offset < NVIC_ISER_BASE + 0x20) {
    /* ISER0-7: Interrupt Set Enable (write 1 to set) */
    nvic_write_bank(nvic->enabled, offset, value, true, &nvic->need_rescan);
  } else if (offset >= NVIC_ICER_BASE && offset < NVIC_ICER_BASE + 0x20) {
    /* ICER0-7: Interrupt Clear Enable (write 1 to clear) */
    nvic_write_bank(nvic->enabled, offset - NVIC_ICER_BASE, value, false,
                    &nvic->need_rescan);
  } else if (offset >= NVIC_ISPR_BASE && offset < NVIC_ISPR_BASE + 0x20) {
    /* ISPR0-7: Interrupt Set Pending (write 1 to set) */
    nvic_write_bank(nvic->pending, offset - NVIC_ISPR_BASE, value, true,
                    &nvic->need_rescan);
  } else if (offset >= NVIC_ICPR_BASE && offset < NVIC_ICPR_BASE + 0x20) {
    /* ICPR0-7: Interrupt Clear Pending (write 1 to clear) */
    nvic_write_bank(nvic->pending, offset - NVIC_ICPR_BASE, value, false,
                    &nvic->need_rescan);
  } else if (offset >= NVIC_IPR_BASE && offset < NVIC_IPR_BASE + 0xF0) {
    /* IPR0-59: Interrupt Priority (byte accessible) */
    nvic_write_ipr(nvic, offset, value, size);
  }
  /* IABR is read-only, writes ignored */
}

/*============================================================================
 * SCB Register Access (MMIO)
 *============================================================================*/

uint32_t armv8m_scb_read(NVIC *nvic, uint32_t offset, uint8_t size) {
  uint32_t value = 0;

  (void)size; /* SCB registers are word-accessible only */

  switch (offset) {
  case SCB_ICSR:
    /* Interrupt Control and State Register */
    value = 0;

    /* VECTACTIVE - find lowest exception number that is active */
    /* System exceptions 2-15 first (lower number = higher priority for fixed)
     */
    for (int exc = 2; exc <= 15; exc++) {
      if (is_exception_active(nvic, exc)) {
        value |= (uint32_t)exc;
        break;
      }
    }
    /* If no system exception active, check external IRQs */
    if ((value & ICSR_VECTACTIVE_MASK) == 0) {
      for (int irq = 0; irq < nvic->num_irqs; irq++) {
        if ((nvic->active[irq_reg(irq)] & (1u << irq_bit(irq))) != 0) {
          value |= (uint32_t)(ARMV8M_EXC_EXTERNAL_BASE + irq);
          break;
        }
      }
    }

    /* VECTPENDING - highest priority pending exception */
    if (nvic->highest_pending > 0) {
      value |= ((uint32_t)nvic->highest_pending << ICSR_VECTPENDING_SHIFT);
      value |= ICSR_ISRPENDING;
    }
    break;

  case SCB_VTOR:
    value = nvic->vtor;
    break;

  case SCB_AIRCR:
    /* Application Interrupt and Reset Control */
    value = (0xFA05u << 16); /* VECTKEYSTAT */
    value |= (uint32_t)(nvic->prigroup << AIRCR_PRIGROUP_SHIFT);
    break;

  case SCB_SCR:
    value = nvic->scr;
    break;

  case SCB_CCR:
    value = nvic->ccr;
    break;

  case SCB_SHPR1:
    /* System Handler Priority 1 (exceptions 4-7) */
    value = (uint32_t)nvic->shpr[0];
    value |= (uint32_t)nvic->shpr[1] << 8;
    value |= (uint32_t)nvic->shpr[2] << 16;
    value |= (uint32_t)nvic->shpr[3] << 24;
    break;

  case SCB_SHPR2:
    /* System Handler Priority 2 (exceptions 8-11) */
    value = (uint32_t)nvic->shpr[4];
    value |= (uint32_t)nvic->shpr[5] << 8;
    value |= (uint32_t)nvic->shpr[6] << 16;
    value |= (uint32_t)nvic->shpr[7] << 24;
    break;

  case SCB_SHPR3:
    /* System Handler Priority 3 (exceptions 12-15) */
    value = (uint32_t)nvic->shpr[8];
    value |= (uint32_t)nvic->shpr[9] << 8;
    value |= (uint32_t)nvic->shpr[10] << 16;
    value |= (uint32_t)nvic->shpr[11] << 24;
    break;

  case SCB_SHCSR:
    /* Mask out internal tracking bits (27=HardFault pending, 28=PendSV,
     * 29=SysTick, 30=DebugMon, 31=NMI pending) */
    value = nvic->shcsr & 0x07FFFFFFu;
    break;

  default:
    break;
  }

  return value;
}

void armv8m_scb_write(NVIC *nvic, uint32_t offset, uint32_t value,
                      uint8_t size) {
  (void)size; /* SCB registers are word-accessible only */

  switch (offset) {
  case SCB_ICSR:
    /* Write to ICSR - handle pend/clear bits */
    if (value & ICSR_PENDSTCLR) {
      armv8m_nvic_clear_exception_pending(nvic, ARMV8M_EXC_SYSTICK);
    }
    if (value & ICSR_PENDSTSET) {
      armv8m_nvic_set_exception_pending(nvic, ARMV8M_EXC_SYSTICK);
    }
    if (value & ICSR_PENDSVCLR) {
      armv8m_nvic_clear_exception_pending(nvic, ARMV8M_EXC_PENDSV);
    }
    if (value & ICSR_PENDSVSET) {
      armv8m_nvic_set_exception_pending(nvic, ARMV8M_EXC_PENDSV);
    }
    if (value & ICSR_NMIPENDSET) {
      armv8m_nvic_set_exception_pending(nvic, ARMV8M_EXC_NMI);
    }
    break;

  case SCB_VTOR:
    /* Vector Table Offset - must be aligned */
    nvic->vtor = value & 0xFFFFFF80u;
    break;

  case SCB_AIRCR:
    /* Check VECTKEY */
    if ((value >> AIRCR_VECTKEY_SHIFT) != AIRCR_VECTKEY) {
      return; /* Invalid key, ignore write */
    }
    nvic->prigroup =
        (int)((value & AIRCR_PRIGROUP_MASK) >> AIRCR_PRIGROUP_SHIFT);
    nvic->aircr = value & 0xFFFFu;
    /* SYSRESETREQ and other bits handled by caller */
    break;

  case SCB_SCR:
    nvic->scr = value & 0x1Fu;
    break;

  case SCB_CCR:
    nvic->ccr = value;
    break;

  case SCB_SHPR1:
    /* System Handler Priority 1 (exceptions 4-7) */
    nvic->shpr[0] = (uint8_t)(value & PRIORITY_MASK);
    nvic->shpr[1] = (uint8_t)((value >> 8) & PRIORITY_MASK);
    nvic->shpr[2] = (uint8_t)((value >> 16) & PRIORITY_MASK);
    nvic->shpr[3] = (uint8_t)((value >> 24) & PRIORITY_MASK);
    nvic->need_rescan = true;
    break;

  case SCB_SHPR2:
    /* System Handler Priority 2 (exceptions 8-11) */
    nvic->shpr[4] = (uint8_t)(value & PRIORITY_MASK);
    nvic->shpr[5] = (uint8_t)((value >> 8) & PRIORITY_MASK);
    nvic->shpr[6] = (uint8_t)((value >> 16) & PRIORITY_MASK);
    nvic->shpr[7] = (uint8_t)((value >> 24) & PRIORITY_MASK);
    nvic->need_rescan = true;
    break;

  case SCB_SHPR3:
    /* System Handler Priority 3 (exceptions 12-15) */
    nvic->shpr[8] = (uint8_t)(value & PRIORITY_MASK);
    nvic->shpr[9] = (uint8_t)((value >> 8) & PRIORITY_MASK);
    nvic->shpr[10] = (uint8_t)((value >> 16) & PRIORITY_MASK);
    nvic->shpr[11] = (uint8_t)((value >> 24) & PRIORITY_MASK);
    nvic->need_rescan = true;
    break;

  case SCB_SHCSR:
    /* Only certain bits are writable */
    nvic->shcsr = (nvic->shcsr & 0xFFF8FFFFu) | (value & 0x00070000u);
    break;

  default:
    break;
  }
}
