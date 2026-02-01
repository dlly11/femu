/**
 * @file armv8m_nvic.h
 * @brief Nested Vectored Interrupt Controller for ARMv8-M
 *
 * AI INSTRUCTIONS:
 * - This header defines the COMPLETE interface for the NVIC module
 * - Implementation goes in src/core/nvic/
 * - Depends on: armv8m_types.h
 * - See src/core/nvic/README.md for implementation guidance
 */

#ifndef ARMV8M_NVIC_H
#define ARMV8M_NVIC_H

#include "arch/armv8m/armv8m_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * NVIC Constants
 *============================================================================*/

#define NVIC_MAX_EXTERNAL_IRQS  240     /**< Max external interrupts */
#define NVIC_NUM_EXCEPTIONS     16      /**< System exceptions (Reset..SysTick) */
#define NVIC_PRIORITY_BITS      3       /**< Priority bits implemented (8 levels) */
#define NVIC_PRIORITY_LEVELS    (1 << NVIC_PRIORITY_BITS)

/* NVIC Register Offsets (from 0xE000E100) */
#define NVIC_ISER_BASE      0x000   /**< Interrupt Set Enable */
#define NVIC_ICER_BASE      0x080   /**< Interrupt Clear Enable */
#define NVIC_ISPR_BASE      0x100   /**< Interrupt Set Pending */
#define NVIC_ICPR_BASE      0x180   /**< Interrupt Clear Pending */
#define NVIC_IABR_BASE      0x200   /**< Interrupt Active Bit */
#define NVIC_IPR_BASE       0x300   /**< Interrupt Priority */

/* System Handler Registers (from 0xE000ED00) */
#define SCB_ICSR        0x04    /**< Interrupt Control State */
#define SCB_VTOR        0x08    /**< Vector Table Offset */
#define SCB_AIRCR       0x0C    /**< Application Interrupt/Reset Control */
#define SCB_SCR         0x10    /**< System Control */
#define SCB_CCR         0x14    /**< Configuration and Control */
#define SCB_SHPR1       0x18    /**< System Handler Priority 1 */
#define SCB_SHPR2       0x1C    /**< System Handler Priority 2 */
#define SCB_SHPR3       0x20    /**< System Handler Priority 3 */
#define SCB_SHCSR       0x24    /**< System Handler Control and State */

/*============================================================================
 * NVIC State
 *============================================================================*/

/**
 * NVIC context.
 */
typedef struct {
    /* Interrupt state (one bit per IRQ) */
    uint32_t enabled[8];        /**< ISER: enabled interrupts */
    uint32_t pending[8];        /**< ISPR: pending interrupts */
    uint32_t active[8];         /**< IABR: active interrupts */

    /* Priority (one byte per IRQ, only upper bits used) */
    uint8_t priority[NVIC_MAX_EXTERNAL_IRQS];

    /* System exception priorities */
    uint8_t shpr[12];           /**< Exception 4-15 priorities */

    /* System Control Block registers */
    uint32_t vtor;              /**< Vector table offset */
    uint32_t aircr;             /**< AIRCR */
    uint32_t scr;               /**< SCR */
    uint32_t ccr;               /**< CCR */
    uint32_t shcsr;             /**< SHCSR */

    /* Derived state */
    int basepri_max;            /**< Effective BASEPRI mask */
    int num_irqs;               /**< Number of implemented IRQs */
    int prigroup;               /**< Priority grouping (from AIRCR) */

    /* Pending exception tracking */
    int highest_pending;        /**< Highest priority pending exception (-1 if none) */
    bool need_rescan;           /**< Flag to rescan pending priorities */
} NVIC;

/*============================================================================
 * NVIC API
 *============================================================================*/

/**
 * Initialize NVIC.
 *
 * @param nvic      NVIC to initialize
 * @param num_irqs  Number of external IRQs to implement (max 240)
 */
void armv8m_nvic_init(NVIC *nvic, int num_irqs);

/**
 * Reset NVIC to default state.
 *
 * @param nvic      NVIC to reset
 */
void armv8m_nvic_reset(NVIC *nvic);

/**
 * Set interrupt pending.
 *
 * @param nvic      NVIC
 * @param irq       IRQ number (0 = first external interrupt)
 */
void armv8m_nvic_set_pending(NVIC *nvic, int irq);

/**
 * Clear interrupt pending.
 *
 * @param nvic      NVIC
 * @param irq       IRQ number
 */
void armv8m_nvic_clear_pending(NVIC *nvic, int irq);

/**
 * Set system exception pending.
 *
 * @param nvic      NVIC
 * @param exc       Exception number (2-15)
 */
void armv8m_nvic_set_exception_pending(NVIC *nvic, int exc);

/**
 * Clear system exception pending.
 *
 * @param nvic      NVIC
 * @param exc       Exception number
 */
void armv8m_nvic_clear_exception_pending(NVIC *nvic, int exc);

/**
 * Enable interrupt.
 *
 * @param nvic      NVIC
 * @param irq       IRQ number
 */
void armv8m_nvic_enable_irq(NVIC *nvic, int irq);

/**
 * Disable interrupt.
 *
 * @param nvic      NVIC
 * @param irq       IRQ number
 */
void armv8m_nvic_disable_irq(NVIC *nvic, int irq);

/**
 * Set interrupt priority.
 *
 * @param nvic      NVIC
 * @param irq       IRQ number
 * @param priority  Priority value (0-255, only upper bits used)
 */
void armv8m_nvic_set_priority(NVIC *nvic, int irq, uint8_t priority);

/**
 * Get interrupt priority.
 *
 * @param nvic      NVIC
 * @param irq       IRQ number
 * @return          Priority value
 */
uint8_t armv8m_nvic_get_priority(const NVIC *nvic, int irq);

/**
 * Set system exception priority.
 *
 * @param nvic      NVIC
 * @param exc       Exception number (4-15)
 * @param priority  Priority value
 */
void armv8m_nvic_set_exception_priority(NVIC *nvic, int exc, uint8_t priority);

/**
 * Get highest priority pending exception.
 *
 * @param nvic      NVIC
 * @param basepri   Current BASEPRI value
 * @param primask   Current PRIMASK value
 * @param faultmask Current FAULTMASK value
 * @param current_pri Current execution priority (-1 if none)
 * @return          Exception number to take, or -1 if none
 */
int armv8m_nvic_get_pending_exception(NVIC *nvic, uint8_t basepri,
                                       uint8_t primask, uint8_t faultmask,
                                       int current_pri);

/**
 * Acknowledge exception entry (mark as active).
 *
 * @param nvic      NVIC
 * @param exc       Exception number
 */
void armv8m_nvic_acknowledge(NVIC *nvic, int exc);

/**
 * Mark exception as inactive (on return).
 *
 * @param nvic      NVIC
 * @param exc       Exception number
 */
void armv8m_nvic_deactivate(NVIC *nvic, int exc);

/**
 * Read NVIC register (MMIO access).
 *
 * @param nvic      NVIC
 * @param offset    Register offset from NVIC base (0xE000E100)
 * @param size      Access size
 * @return          Register value
 */
uint32_t armv8m_nvic_read(NVIC *nvic, uint32_t offset, uint8_t size);

/**
 * Write NVIC register (MMIO access).
 *
 * @param nvic      NVIC
 * @param offset    Register offset from NVIC base
 * @param value     Value to write
 * @param size      Access size
 */
void armv8m_nvic_write(NVIC *nvic, uint32_t offset, uint32_t value, uint8_t size);

/**
 * Read System Control Block register.
 *
 * @param nvic      NVIC
 * @param offset    Register offset from SCB base (0xE000ED00)
 * @param size      Access size
 * @return          Register value
 */
uint32_t armv8m_scb_read(NVIC *nvic, uint32_t offset, uint8_t size);

/**
 * Write System Control Block register.
 *
 * @param nvic      NVIC
 * @param offset    Register offset from SCB base
 * @param value     Value to write
 * @param size      Access size
 */
void armv8m_scb_write(NVIC *nvic, uint32_t offset, uint32_t value, uint8_t size);

#ifdef __cplusplus
}
#endif

#endif /* ARMV8M_NVIC_H */
