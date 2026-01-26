# NVIC Module

## Purpose

Implements the Nested Vectored Interrupt Controller: interrupt enable/disable, pending state, priority management, and exception arbitration.

## Interface

See `include/armv8m_nvic.h`

## Files to Implement

| File | Description | ~LOC |
|------|-------------|------|
| `nvic.c` | NVIC implementation | 500 |

## Dependencies

- `armv8m_types.h` (types only)

## AI Development Notes

### What You Need to Know

1. **Exception Numbers**:
   ```
   1   Reset
   2   NMI
   3   HardFault
   4   MemManage
   5   BusFault
   6   UsageFault
   7   SecureFault (TrustZone)
   11  SVCall
   12  DebugMonitor
   14  PendSV
   15  SysTick
   16+ External interrupts (IRQ0, IRQ1, ...)
   ```

2. **Priority Model**:
   - Lower number = higher priority
   - 3 bits implemented = 8 priority levels (0, 32, 64, 96, 128, 160, 192, 224)
   - Priority stored in upper bits of byte
   - PRIGROUP determines group/subpriority split

3. **Interrupt State**:
   - **Enabled**: Can become pending
   - **Pending**: Waiting to be serviced
   - **Active**: Currently being serviced

4. **NVIC Registers** (at 0xE000E100):
   - ISER[0-7]: Interrupt Set Enable
   - ICER[0-7]: Interrupt Clear Enable
   - ISPR[0-7]: Interrupt Set Pending
   - ICPR[0-7]: Interrupt Clear Pending
   - IABR[0-7]: Interrupt Active Bit (read-only)
   - IPR[0-59]: Interrupt Priority (one byte per IRQ)

5. **Exception Priority**:
   - Reset: -3 (highest)
   - NMI: -2
   - HardFault: -1
   - Others: Configurable (0-255)

### What You Do NOT Need

- Executor implementation (just track state)
- Memory implementation (NVIC is accessed via its own API)
- TrustZone secure/non-secure partitioning (optional)

### Implementation Tips

```c
// Get group priority (determines preemption)
static int get_group_priority(NVIC *nvic, int exc) {
    uint8_t raw_pri = get_priority(nvic, exc);
    int prigroup = (nvic->aircr >> 8) & 0x7;
    // Group priority is upper bits based on PRIGROUP
    int group_bits = 7 - prigroup;
    return raw_pri >> (8 - group_bits);
}

// Find highest priority pending exception
int find_highest_pending(NVIC *nvic) {
    int best = -1;
    int best_pri = 256;

    // Check system exceptions (2-15)
    for (int exc = 2; exc <= 15; exc++) {
        if (is_pending(nvic, exc)) {
            int pri = get_priority(nvic, exc);
            if (pri < best_pri) {
                best = exc;
                best_pri = pri;
            }
        }
    }

    // Check external interrupts
    for (int irq = 0; irq < nvic->num_irqs; irq++) {
        if (is_enabled(nvic, irq) && is_pending(nvic, irq)) {
            int pri = nvic->priority[irq];
            if (pri < best_pri) {
                best = irq + 16;
                best_pri = pri;
            }
        }
    }

    return best;
}
```

### Test Vectors

```c
// Enable IRQ 0, set pending -> should be returned as pending
// Set IRQ 0 priority to 0x80, IRQ 1 priority to 0x40
// Both pending -> IRQ 1 should be returned (lower = higher priority)

// Set BASEPRI to 0x80 -> only IRQs with priority < 0x80 can preempt
// Set PRIMASK -> all interrupts masked except NMI/HardFault
```

### Edge Cases

1. **BASEPRI filtering**: Only exceptions with priority < BASEPRI can preempt
2. **PRIMASK**: When set, only NMI and HardFault can execute
3. **FAULTMASK**: When set, only NMI can execute
4. **Tail-chaining**: When returning from exception, may go directly to next pending
5. **Late-arriving**: Higher priority exception arriving during stacking takes over

## Building & Testing

```bash
femu build all
femu test c --filter nvic
```

## Checklist

- [ ] `nvic.c` - Complete implementation
- [ ] Interrupt enable/disable
- [ ] Pending state management
- [ ] Priority handling
- [ ] Exception priority calculation
- [ ] BASEPRI/PRIMASK/FAULTMASK filtering
- [ ] Register read/write (MMIO interface)
- [ ] All tests pass
