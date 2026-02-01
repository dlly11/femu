/* ARMv8-M NVIC Register Definitions and Access Functions
 *
 * Provides access to Nested Vectored Interrupt Controller registers:
 * - NVIC_ISER: Set-enable registers
 * - NVIC_ICER: Clear-enable registers
 * - NVIC_ISPR: Set-pending registers
 * - NVIC_ICPR: Clear-pending registers
 * - NVIC_IABR: Active bit registers
 * - NVIC_IPR:  Priority registers
 */

#ifndef ARMV8M_NVIC_H
#define ARMV8M_NVIC_H

#include <stdint.h>

/* ========== NVIC Register Base Addresses ========== */

#define NVIC_BASE       0xE000E100

/* Set-enable registers (ISER0-ISER15) */
#define NVIC_ISER_BASE  0xE000E100
#define NVIC_ISER0      (*(volatile uint32_t *)(NVIC_ISER_BASE + 0x00))
#define NVIC_ISER1      (*(volatile uint32_t *)(NVIC_ISER_BASE + 0x04))
#define NVIC_ISER2      (*(volatile uint32_t *)(NVIC_ISER_BASE + 0x08))
#define NVIC_ISER3      (*(volatile uint32_t *)(NVIC_ISER_BASE + 0x0C))
#define NVIC_ISER4      (*(volatile uint32_t *)(NVIC_ISER_BASE + 0x10))
#define NVIC_ISER5      (*(volatile uint32_t *)(NVIC_ISER_BASE + 0x14))
#define NVIC_ISER6      (*(volatile uint32_t *)(NVIC_ISER_BASE + 0x18))
#define NVIC_ISER7      (*(volatile uint32_t *)(NVIC_ISER_BASE + 0x1C))

/* Clear-enable registers (ICER0-ICER15) */
#define NVIC_ICER_BASE  0xE000E180
#define NVIC_ICER0      (*(volatile uint32_t *)(NVIC_ICER_BASE + 0x00))
#define NVIC_ICER1      (*(volatile uint32_t *)(NVIC_ICER_BASE + 0x04))
#define NVIC_ICER2      (*(volatile uint32_t *)(NVIC_ICER_BASE + 0x08))
#define NVIC_ICER3      (*(volatile uint32_t *)(NVIC_ICER_BASE + 0x0C))
#define NVIC_ICER4      (*(volatile uint32_t *)(NVIC_ICER_BASE + 0x10))
#define NVIC_ICER5      (*(volatile uint32_t *)(NVIC_ICER_BASE + 0x14))
#define NVIC_ICER6      (*(volatile uint32_t *)(NVIC_ICER_BASE + 0x18))
#define NVIC_ICER7      (*(volatile uint32_t *)(NVIC_ICER_BASE + 0x1C))

/* Set-pending registers (ISPR0-ISPR15) */
#define NVIC_ISPR_BASE  0xE000E200
#define NVIC_ISPR0      (*(volatile uint32_t *)(NVIC_ISPR_BASE + 0x00))
#define NVIC_ISPR1      (*(volatile uint32_t *)(NVIC_ISPR_BASE + 0x04))
#define NVIC_ISPR2      (*(volatile uint32_t *)(NVIC_ISPR_BASE + 0x08))
#define NVIC_ISPR3      (*(volatile uint32_t *)(NVIC_ISPR_BASE + 0x0C))
#define NVIC_ISPR4      (*(volatile uint32_t *)(NVIC_ISPR_BASE + 0x10))
#define NVIC_ISPR5      (*(volatile uint32_t *)(NVIC_ISPR_BASE + 0x14))
#define NVIC_ISPR6      (*(volatile uint32_t *)(NVIC_ISPR_BASE + 0x18))
#define NVIC_ISPR7      (*(volatile uint32_t *)(NVIC_ISPR_BASE + 0x1C))

/* Clear-pending registers (ICPR0-ICPR15) */
#define NVIC_ICPR_BASE  0xE000E280
#define NVIC_ICPR0      (*(volatile uint32_t *)(NVIC_ICPR_BASE + 0x00))
#define NVIC_ICPR1      (*(volatile uint32_t *)(NVIC_ICPR_BASE + 0x04))
#define NVIC_ICPR2      (*(volatile uint32_t *)(NVIC_ICPR_BASE + 0x08))
#define NVIC_ICPR3      (*(volatile uint32_t *)(NVIC_ICPR_BASE + 0x0C))
#define NVIC_ICPR4      (*(volatile uint32_t *)(NVIC_ICPR_BASE + 0x10))
#define NVIC_ICPR5      (*(volatile uint32_t *)(NVIC_ICPR_BASE + 0x14))
#define NVIC_ICPR6      (*(volatile uint32_t *)(NVIC_ICPR_BASE + 0x18))
#define NVIC_ICPR7      (*(volatile uint32_t *)(NVIC_ICPR_BASE + 0x1C))

/* Active bit registers (IABR0-IABR15) */
#define NVIC_IABR_BASE  0xE000E300
#define NVIC_IABR0      (*(volatile uint32_t *)(NVIC_IABR_BASE + 0x00))
#define NVIC_IABR1      (*(volatile uint32_t *)(NVIC_IABR_BASE + 0x04))
#define NVIC_IABR2      (*(volatile uint32_t *)(NVIC_IABR_BASE + 0x08))
#define NVIC_IABR3      (*(volatile uint32_t *)(NVIC_IABR_BASE + 0x0C))
#define NVIC_IABR4      (*(volatile uint32_t *)(NVIC_IABR_BASE + 0x10))
#define NVIC_IABR5      (*(volatile uint32_t *)(NVIC_IABR_BASE + 0x14))
#define NVIC_IABR6      (*(volatile uint32_t *)(NVIC_IABR_BASE + 0x18))
#define NVIC_IABR7      (*(volatile uint32_t *)(NVIC_IABR_BASE + 0x1C))

/* Priority registers (IPR0-IPR123, one byte per interrupt) */
#define NVIC_IPR_BASE   0xE000E400
#define NVIC_IPR(n)     (*(volatile uint8_t *)(NVIC_IPR_BASE + (n)))

/* Interrupt Priority registers (32-bit access, 4 IRQs per register) */
#define NVIC_IPR0       (*(volatile uint32_t *)(NVIC_IPR_BASE + 0x00))
#define NVIC_IPR1       (*(volatile uint32_t *)(NVIC_IPR_BASE + 0x04))
#define NVIC_IPR2       (*(volatile uint32_t *)(NVIC_IPR_BASE + 0x08))
#define NVIC_IPR3       (*(volatile uint32_t *)(NVIC_IPR_BASE + 0x0C))

/* Software Trigger Interrupt Register */
#define NVIC_STIR_ADDR  0xE000EF00
#define NVIC_STIR       (*(volatile uint32_t *)NVIC_STIR_ADDR)

/* ========== System Control Block Interrupt Registers ========== */

/* Interrupt Control State Register */
#define SCB_ICSR_ADDR   0xE000ED04
#define SCB_ICSR        (*(volatile uint32_t *)SCB_ICSR_ADDR)

/* ICSR bit definitions */
#define SCB_ICSR_VECTACTIVE_Pos     0
#define SCB_ICSR_VECTACTIVE_Msk     (0x1FFUL << SCB_ICSR_VECTACTIVE_Pos)
#define SCB_ICSR_RETTOBASE_Pos      11
#define SCB_ICSR_RETTOBASE_Msk      (1UL << SCB_ICSR_RETTOBASE_Pos)
#define SCB_ICSR_VECTPENDING_Pos    12
#define SCB_ICSR_VECTPENDING_Msk    (0x1FFUL << SCB_ICSR_VECTPENDING_Pos)
#define SCB_ICSR_ISRPENDING_Pos     22
#define SCB_ICSR_ISRPENDING_Msk     (1UL << SCB_ICSR_ISRPENDING_Pos)
#define SCB_ICSR_PENDSTCLR_Pos      25
#define SCB_ICSR_PENDSTCLR_Msk      (1UL << SCB_ICSR_PENDSTCLR_Pos)
#define SCB_ICSR_PENDSTSET_Pos      26
#define SCB_ICSR_PENDSTSET_Msk      (1UL << SCB_ICSR_PENDSTSET_Pos)
#define SCB_ICSR_PENDSVCLR_Pos      27
#define SCB_ICSR_PENDSVCLR_Msk      (1UL << SCB_ICSR_PENDSVCLR_Pos)
#define SCB_ICSR_PENDSVSET_Pos      28
#define SCB_ICSR_PENDSVSET_Msk      (1UL << SCB_ICSR_PENDSVSET_Pos)
#define SCB_ICSR_NMIPENDSET_Pos     31
#define SCB_ICSR_NMIPENDSET_Msk     (1UL << SCB_ICSR_NMIPENDSET_Pos)

/* Application Interrupt and Reset Control Register */
#define SCB_AIRCR_ADDR  0xE000ED0C
#define SCB_AIRCR       (*(volatile uint32_t *)SCB_AIRCR_ADDR)

/* AIRCR bit definitions */
#define SCB_AIRCR_VECTKEY_Pos       16
#define SCB_AIRCR_VECTKEY_Msk       (0xFFFFUL << SCB_AIRCR_VECTKEY_Pos)
#define SCB_AIRCR_VECTKEY           0x05FA  /* Write key */
#define SCB_AIRCR_PRIGROUP_Pos      8
#define SCB_AIRCR_PRIGROUP_Msk      (0x7UL << SCB_AIRCR_PRIGROUP_Pos)
#define SCB_AIRCR_SYSRESETREQ_Pos   2
#define SCB_AIRCR_SYSRESETREQ_Msk   (1UL << SCB_AIRCR_SYSRESETREQ_Pos)

/* ========== Helper Functions ========== */

/* Enable an interrupt */
static inline void nvic_enable_irq(uint32_t irq) {
    volatile uint32_t *iser = (volatile uint32_t *)(NVIC_ISER_BASE + (irq / 32) * 4);
    *iser = 1UL << (irq % 32);
}

/* Disable an interrupt */
static inline void nvic_disable_irq(uint32_t irq) {
    volatile uint32_t *icer = (volatile uint32_t *)(NVIC_ICER_BASE + (irq / 32) * 4);
    *icer = 1UL << (irq % 32);
}

/* Check if interrupt is enabled */
static inline uint32_t nvic_is_enabled(uint32_t irq) {
    volatile uint32_t *iser = (volatile uint32_t *)(NVIC_ISER_BASE + (irq / 32) * 4);
    return (*iser >> (irq % 32)) & 1;
}

/* Set interrupt pending */
static inline void nvic_set_pending(uint32_t irq) {
    volatile uint32_t *ispr = (volatile uint32_t *)(NVIC_ISPR_BASE + (irq / 32) * 4);
    *ispr = 1UL << (irq % 32);
}

/* Clear interrupt pending */
static inline void nvic_clear_pending(uint32_t irq) {
    volatile uint32_t *icpr = (volatile uint32_t *)(NVIC_ICPR_BASE + (irq / 32) * 4);
    *icpr = 1UL << (irq % 32);
}

/* Check if interrupt is pending */
static inline uint32_t nvic_is_pending(uint32_t irq) {
    volatile uint32_t *ispr = (volatile uint32_t *)(NVIC_ISPR_BASE + (irq / 32) * 4);
    return (*ispr >> (irq % 32)) & 1;
}

/* Check if interrupt is active */
static inline uint32_t nvic_is_active(uint32_t irq) {
    volatile uint32_t *iabr = (volatile uint32_t *)(NVIC_IABR_BASE + (irq / 32) * 4);
    return (*iabr >> (irq % 32)) & 1;
}

/* Set interrupt priority (0-255, lower = higher priority) */
static inline void nvic_set_priority(uint32_t irq, uint8_t priority) {
    NVIC_IPR(irq) = priority;
}

/* Get interrupt priority */
static inline uint8_t nvic_get_priority(uint32_t irq) {
    return NVIC_IPR(irq);
}

/* Software trigger interrupt */
static inline void nvic_trigger_irq(uint32_t irq) {
    NVIC_STIR = irq;
}

/* Get active exception number (0 = Thread mode) */
static inline uint32_t nvic_get_active_exception(void) {
    return SCB_ICSR & SCB_ICSR_VECTACTIVE_Msk;
}

/* Get pending exception number */
static inline uint32_t nvic_get_pending_exception(void) {
    return (SCB_ICSR & SCB_ICSR_VECTPENDING_Msk) >> SCB_ICSR_VECTPENDING_Pos;
}

/* Set priority grouping (determines preemption priority vs sub-priority split) */
static inline void nvic_set_priority_grouping(uint32_t group) {
    SCB_AIRCR = (SCB_AIRCR_VECTKEY << SCB_AIRCR_VECTKEY_Pos) |
                ((group & 0x7) << SCB_AIRCR_PRIGROUP_Pos);
}

/* Trigger PendSV */
static inline void nvic_trigger_pendsv(void) {
    SCB_ICSR = SCB_ICSR_PENDSVSET_Msk;
}

/* Clear PendSV */
static inline void nvic_clear_pendsv(void) {
    SCB_ICSR = SCB_ICSR_PENDSVCLR_Msk;
}

/* Trigger SysTick (set pending) */
static inline void nvic_trigger_systick(void) {
    SCB_ICSR = SCB_ICSR_PENDSTSET_Msk;
}

/* Clear SysTick pending */
static inline void nvic_clear_systick(void) {
    SCB_ICSR = SCB_ICSR_PENDSTCLR_Msk;
}

#endif /* ARMV8M_NVIC_H */
