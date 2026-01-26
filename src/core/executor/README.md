# Executor Module

## Purpose

Executes decoded ARMv8-M instructions, managing CPU state, memory access, and exception handling.

## Interface

See `include/armv8m_executor.h`

## Files to Implement

| File | Description | ~LOC |
|------|-------------|------|
| `executor.c` | Main entry point, instruction dispatch | 200 |
| `exec_data_proc.c` | Data processing instructions | 300 |
| `exec_load_store.c` | Load/store instructions | 250 |
| `exec_branch.c` | Branch instructions | 150 |
| `exec_system.c` | System instructions (MSR, MRS, barriers) | 150 |
| `exec_exception.c` | Exception entry/return | 200 |

## Dependencies

- `armv8m_types.h` (types)
- `armv8m_decoder.h` (DecodedInsn)
- `armv8m_memory.h` (memory interface - via callbacks)

## AI Development Notes

### What You Need to Know

1. **CPU State Management**:
   - R0-R12: General purpose registers
   - R13 (SP): Banked (MSP/PSP), selected by CONTROL.SPSEL
   - R14 (LR): Link register
   - R15 (PC): Program counter (always +4 ahead during execution)
   - xPSR: Flags (N, Z, C, V) in upper bits

2. **Instruction Execution Flow**:
   ```
   fetch -> decode -> check condition -> execute -> update PC
   ```

3. **Flag Updates**:
   - N: Result is negative (bit 31 set)
   - Z: Result is zero
   - C: Carry out from ALU
   - V: Signed overflow

4. **Memory Access**:
   - Use provided callbacks (don't implement memory directly)
   - Handle faults via callback return values
   - Little-endian byte order

5. **Exception Model**:
   - On exception: push context, load PC from vector table
   - EXC_RETURN value in LR indicates return mode
   - Tail-chaining: return directly to next pending exception

### What You Do NOT Need

- Decoder implementation (use DecodedInsn from decoder)
- Memory implementation (use callbacks)
- NVIC details (use callbacks)
- Peripheral knowledge

### Implementation Tips

```c
// Flag calculation helpers
static inline bool is_negative(uint32_t val) {
    return (val & 0x80000000) != 0;
}

static inline bool is_zero(uint32_t val) {
    return val == 0;
}

// Add with carry and overflow detection
static inline uint32_t add_with_flags(uint32_t a, uint32_t b, uint32_t c_in,
                                       bool *carry, bool *overflow) {
    uint64_t result = (uint64_t)a + b + c_in;
    *carry = result > 0xFFFFFFFF;
    *overflow = ((a ^ ~b) & (a ^ result)) >> 31;
    return (uint32_t)result;
}

// Condition check
bool check_condition(uint32_t xpsr, ConditionCode cond) {
    bool n = (xpsr >> 31) & 1;
    bool z = (xpsr >> 30) & 1;
    bool c = (xpsr >> 29) & 1;
    bool v = (xpsr >> 28) & 1;

    switch (cond) {
        case COND_EQ: return z;
        case COND_NE: return !z;
        case COND_CS: return c;
        case COND_CC: return !c;
        // ... etc
    }
}
```

### Test Vectors

```c
// MOV R0, #42 - should set R0=42
// CMP R0, #42 - should set Z=1
// ADD R0, R1, R2 - should set R0=R1+R2, update flags
// LDR R0, [R1] - should load word from mem[R1] into R0
// B.N +10 - should set PC = PC + 4 + 10
```

### Edge Cases

1. **PC writes**: PC writes must be word-aligned, clear bit 0
2. **SP alignment**: Stack must be 8-byte aligned on exception entry
3. **IT blocks**: Execute only if ITSTATE condition passes
4. **Unaligned access**: May fault depending on CCR.UNALIGN_TRP

## Building & Testing

```bash
femu build all
femu test c --filter executor
```

## Checklist

- [ ] `executor.c` - Main dispatch and CPU init
- [ ] `exec_data_proc.c` - All data processing operations
- [ ] `exec_load_store.c` - Load/store with all addressing modes
- [ ] `exec_branch.c` - All branch types
- [ ] `exec_system.c` - System instructions
- [ ] `exec_exception.c` - Exception entry/return
- [ ] All tests pass
- [ ] No memory leaks (sanitizers clean)
