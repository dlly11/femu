/**
 * @file emu_interrupt.h
 * @brief Abstract interrupt controller interface for multi-architecture support
 *
 * This file defines the abstract interrupt controller interface using a vtable pattern.
 * Architecture-specific implementations (NVIC for ARM, PLIC for RISC-V) provide
 * concrete interrupt handling.
 */

#ifndef EMU_INTERRUPT_H
#define EMU_INTERRUPT_H

#include "emu_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Forward Declarations
 *============================================================================*/

typedef struct EmuInterruptController EmuInterruptController;
typedef struct EmuInterruptVTable EmuInterruptVTable;

/*============================================================================
 * Interrupt Controller Virtual Table
 *============================================================================*/

/**
 * Virtual function table for interrupt controller operations.
 */
struct EmuInterruptVTable {
    /**
     * Destroy interrupt controller and free resources.
     *
     * @param ic        Interrupt controller instance
     */
    void (*destroy)(EmuInterruptController *ic);

    /**
     * Reset interrupt controller to initial state.
     *
     * @param ic        Interrupt controller instance
     */
    void (*reset)(EmuInterruptController *ic);

    /**
     * Set interrupt pending.
     *
     * @param ic        Interrupt controller
     * @param irq       IRQ number
     */
    void (*set_pending)(EmuInterruptController *ic, int irq);

    /**
     * Clear interrupt pending.
     *
     * @param ic        Interrupt controller
     * @param irq       IRQ number
     */
    void (*clear_pending)(EmuInterruptController *ic, int irq);

    /**
     * Check if interrupt is pending.
     *
     * @param ic        Interrupt controller
     * @param irq       IRQ number
     * @return          true if pending
     */
    bool (*is_pending)(const EmuInterruptController *ic, int irq);

    /**
     * Enable interrupt.
     *
     * @param ic        Interrupt controller
     * @param irq       IRQ number
     */
    void (*enable)(EmuInterruptController *ic, int irq);

    /**
     * Disable interrupt.
     *
     * @param ic        Interrupt controller
     * @param irq       IRQ number
     */
    void (*disable)(EmuInterruptController *ic, int irq);

    /**
     * Check if interrupt is enabled.
     *
     * @param ic        Interrupt controller
     * @param irq       IRQ number
     * @return          true if enabled
     */
    bool (*is_enabled)(const EmuInterruptController *ic, int irq);

    /**
     * Set interrupt priority.
     *
     * @param ic        Interrupt controller
     * @param irq       IRQ number
     * @param priority  Priority value (lower = higher priority)
     */
    void (*set_priority)(EmuInterruptController *ic, int irq, int priority);

    /**
     * Get interrupt priority.
     *
     * @param ic        Interrupt controller
     * @param irq       IRQ number
     * @return          Priority value
     */
    int (*get_priority)(const EmuInterruptController *ic, int irq);

    /**
     * Get highest priority pending interrupt.
     *
     * @param ic                Interrupt controller
     * @param current_priority  Current execution priority
     * @return                  IRQ number to take, or -1 if none
     */
    int (*get_pending_irq)(EmuInterruptController *ic, int current_priority);

    /**
     * Acknowledge interrupt entry (mark as active).
     *
     * @param ic        Interrupt controller
     * @param irq       IRQ number
     */
    void (*acknowledge)(EmuInterruptController *ic, int irq);

    /**
     * Mark interrupt as inactive (on return).
     *
     * @param ic        Interrupt controller
     * @param irq       IRQ number
     */
    void (*deactivate)(EmuInterruptController *ic, int irq);

    /**
     * Read interrupt controller register (MMIO access).
     *
     * @param ic        Interrupt controller
     * @param offset    Register offset from base
     * @param size      Access size
     * @return          Register value
     */
    uint32_t (*read_reg)(EmuInterruptController *ic, uint32_t offset, uint8_t size);

    /**
     * Write interrupt controller register (MMIO access).
     *
     * @param ic        Interrupt controller
     * @param offset    Register offset from base
     * @param value     Value to write
     * @param size      Access size
     */
    void (*write_reg)(EmuInterruptController *ic, uint32_t offset, uint32_t value, uint8_t size);

    /**
     * Get number of supported IRQs.
     *
     * @param ic        Interrupt controller
     * @return          Number of IRQs
     */
    int (*get_num_irqs)(const EmuInterruptController *ic);
};

/*============================================================================
 * Interrupt Controller Structure
 *============================================================================*/

/**
 * Abstract interrupt controller instance.
 */
struct EmuInterruptController {
    const EmuInterruptVTable *vtable;   /**< Virtual function table */
    EmuArchType arch;                   /**< Architecture type */
    void *arch_state;                   /**< Architecture-specific state */
};

/*============================================================================
 * Convenience Macros
 *============================================================================*/

#define EMU_IC_INIT(ic, vtbl, arch_type, state) do { \
    (ic)->vtable = (vtbl);                            \
    (ic)->arch = (arch_type);                         \
    (ic)->arch_state = (state);                       \
} while(0)

/*============================================================================
 * Inline Convenience Functions
 *============================================================================*/

static inline void emu_ic_destroy(EmuInterruptController *ic) {
    if (ic && ic->vtable && ic->vtable->destroy) {
        ic->vtable->destroy(ic);
    }
}

static inline void emu_ic_reset(EmuInterruptController *ic) {
    if (ic && ic->vtable && ic->vtable->reset) {
        ic->vtable->reset(ic);
    }
}

static inline void emu_ic_set_pending(EmuInterruptController *ic, int irq) {
    if (ic && ic->vtable) {
        ic->vtable->set_pending(ic, irq);
    }
}

static inline void emu_ic_clear_pending(EmuInterruptController *ic, int irq) {
    if (ic && ic->vtable) {
        ic->vtable->clear_pending(ic, irq);
    }
}

static inline void emu_ic_enable(EmuInterruptController *ic, int irq) {
    if (ic && ic->vtable) {
        ic->vtable->enable(ic, irq);
    }
}

static inline void emu_ic_disable(EmuInterruptController *ic, int irq) {
    if (ic && ic->vtable) {
        ic->vtable->disable(ic, irq);
    }
}

static inline void emu_ic_set_priority(EmuInterruptController *ic, int irq, int priority) {
    if (ic && ic->vtable) {
        ic->vtable->set_priority(ic, irq, priority);
    }
}

static inline int emu_ic_get_priority(const EmuInterruptController *ic, int irq) {
    return (ic && ic->vtable) ? ic->vtable->get_priority(ic, irq) : 0;
}

static inline int emu_ic_get_pending_irq(EmuInterruptController *ic, int current_priority) {
    return (ic && ic->vtable) ? ic->vtable->get_pending_irq(ic, current_priority) : -1;
}

#ifdef __cplusplus
}
#endif

#endif /* EMU_INTERRUPT_H */
