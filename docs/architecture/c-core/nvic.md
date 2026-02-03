# NVIC Module

The Nested Vectored Interrupt Controller (NVIC) handles interrupt management for ARMv8-M.

## Purpose

- Manage interrupt priorities
- Track pending and active interrupts
- Determine which interrupt to service
- Support interrupt masking (PRIMASK, BASEPRI, FAULTMASK)

## API

```c
// include/arch/armv8m/armv8m_nvic.h

/**
 * Create NVIC instance.
 */
ARMv8MNVIC* armv8m_nvic_create(uint32_t num_irqs);

/**
 * Destroy NVIC instance.
 */
void armv8m_nvic_destroy(ARMv8MNVIC *nvic);

/**
 * Set interrupt pending.
 */
int armv8m_nvic_set_pending(ARMv8MNVIC *nvic, uint32_t irq);

/**
 * Clear interrupt pending.
 */
int armv8m_nvic_clear_pending(ARMv8MNVIC *nvic, uint32_t irq);

/**
 * Set interrupt priority.
 */
int armv8m_nvic_set_priority(ARMv8MNVIC *nvic, uint32_t irq, uint8_t priority);

/**
 * Enable/disable interrupt.
 */
int armv8m_nvic_enable(ARMv8MNVIC *nvic, uint32_t irq, bool enable);

/**
 * Get highest priority pending interrupt.
 */
int armv8m_nvic_get_pending(ARMv8MNVIC *nvic, int current_priority);
```

## Key Data Structures

### ARMv8MNVIC

```c
typedef struct ARMv8MNVIC {
    uint32_t num_irqs;          // Number of external interrupts

    // Interrupt state (bit per IRQ)
    uint32_t *enabled;          // NVIC_ISER
    uint32_t *pending;          // NVIC_ISPR
    uint32_t *active;           // NVIC_IABR

    // Priority levels (byte per IRQ)
    uint8_t *priority;          // NVIC_IPR

    // System exceptions (-16 to -1)
    uint8_t exception_priority[16];
    uint32_t system_pending;
    uint32_t system_active;
} ARMv8MNVIC;
```

## Exception Numbers

| Number | Name       | Priority | Description              |
| ------ | ---------- | -------- | ------------------------ |
| 1      | Reset      | -3       | Reset                    |
| 2      | NMI        | -2       | Non-maskable interrupt   |
| 3      | HardFault  | -1       | Hard fault               |
| 4      | MemManage  | Config   | Memory management fault  |
| 5      | BusFault   | Config   | Bus fault                |
| 6      | UsageFault | Config   | Usage fault              |
| 11     | SVCall     | Config   | Supervisor call          |
| 14     | PendSV     | Config   | Pendable service request |
| 15     | SysTick    | Config   | System tick timer        |
| 16+    | IRQn       | Config   | External interrupts      |

## Implementation Structure

```text
src/arch/armv8m/nvic/
├── nvic.c             # NVIC implementation
└── tests/
    └── test_nvic.cpp
```

## Priority Model

### Priority Groups

ARMv8-M supports priority grouping:

```c
// AIRCR.PRIGROUP determines preemption levels
// Higher PRIGROUP = more preemption levels
uint8_t get_group_priority(uint8_t priority, uint8_t prigroup) {
    uint8_t subpri_bits = 7 - prigroup;
    return priority >> subpri_bits;
}
```

### Comparison

Lower numeric value = higher priority:

```c
bool should_preempt(int new_priority, int current_priority) {
    return new_priority < current_priority;
}
```

## Register Interface

NVIC registers are memory-mapped at 0xE000E100:

| Offset     | Name     | Description                 |
| ---------- | -------- | --------------------------- |
| 0x000-0x01F| ISER     | Interrupt Set Enable        |
| 0x080-0x09F| ICER     | Interrupt Clear Enable      |
| 0x100-0x11F| ISPR     | Interrupt Set Pending       |
| 0x180-0x19F| ICPR     | Interrupt Clear Pending     |
| 0x200-0x21F| IABR     | Interrupt Active Bit        |
| 0x300-0x3FF| IPR      | Interrupt Priority          |

```c
static uint32_t nvic_read(void *ctx, uint32_t offset, uint8_t size) {
    ARMv8MNVIC *nvic = (ARMv8MNVIC *)ctx;

    if (offset >= 0x000 && offset < 0x020) {
        // ISER - read enabled status
        uint32_t reg_idx = (offset - 0x000) / 4;
        return nvic->enabled[reg_idx];
    }
    // ... more registers
}
```

## Pending Interrupt Selection

Get the highest priority pending interrupt:

```c
int armv8m_nvic_get_pending(ARMv8MNVIC *nvic, int current_priority) {
    int best_irq = -1;
    int best_priority = current_priority;

    // Check system exceptions first
    for (int exc = 2; exc < 16; exc++) {
        if (is_system_pending(nvic, exc)) {
            int prio = nvic->exception_priority[exc];
            if (prio < best_priority) {
                best_priority = prio;
                best_irq = exc;
            }
        }
    }

    // Check external interrupts
    for (uint32_t irq = 0; irq < nvic->num_irqs; irq++) {
        if (is_enabled(nvic, irq) && is_pending(nvic, irq)) {
            int prio = nvic->priority[irq];
            if (prio < best_priority) {
                best_priority = prio;
                best_irq = 16 + irq;
            }
        }
    }

    return best_irq;
}
```

## Masking

Interrupt masking via special registers:

```c
bool is_masked(ARMv8MCPUState *cpu, int exception_priority) {
    // PRIMASK blocks all interrupts
    if (cpu->primask & 1) {
        return exception_priority >= 0;  // Only NMI/HardFault pass
    }

    // FAULTMASK blocks all except NMI
    if (cpu->faultmask & 1) {
        return exception_priority >= -1;  // Only NMI passes
    }

    // BASEPRI blocks below threshold
    if (cpu->basepri != 0) {
        return exception_priority >= (int)cpu->basepri;
    }

    return false;
}
```

## Exception Entry

When taking an exception:

1. Push exception frame (R0-R3, R12, LR, PC, xPSR)
2. Set LR to EXC_RETURN value
3. Update NVIC active status
4. Clear pending status
5. Jump to vector

```c
void nvic_enter_exception(ARMv8MNVIC *nvic, int exception_num) {
    if (exception_num >= 16) {
        // External IRQ
        int irq = exception_num - 16;
        set_active(nvic, irq, true);
        clear_pending(nvic, irq);
    } else {
        // System exception
        nvic->system_active |= (1 << exception_num);
        nvic->system_pending &= ~(1 << exception_num);
    }
}
```

## Exception Return

EXC_RETURN value determines return behavior:

| Value       | Meaning                              |
| ----------- | ------------------------------------ |
| 0xFFFFFFF1  | Return to Handler mode, MSP          |
| 0xFFFFFFF9  | Return to Thread mode, MSP           |
| 0xFFFFFFFD  | Return to Thread mode, PSP           |

## Testing

```cpp
TEST(NVIC, PriorityPreemption)
{
    ARMv8MNVIC *nvic = armv8m_nvic_create(32);

    // Set up two interrupts with different priorities
    armv8m_nvic_enable(nvic, 0, true);
    armv8m_nvic_enable(nvic, 1, true);
    armv8m_nvic_set_priority(nvic, 0, 0x40);  // Lower priority
    armv8m_nvic_set_priority(nvic, 1, 0x20);  // Higher priority

    // Both pending, higher priority should be selected
    armv8m_nvic_set_pending(nvic, 0);
    armv8m_nvic_set_pending(nvic, 1);

    int pending = armv8m_nvic_get_pending(nvic, 0xFF);
    CHECK_EQUAL(17, pending);  // IRQ 1 = exception 17

    armv8m_nvic_destroy(nvic);
}
```

## See Also

- ARMv8-M Architecture Reference Manual, Chapter B3 (Exception Model)
- [Executor Module](executor.md) - Exception entry/return
