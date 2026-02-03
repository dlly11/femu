# Executor Module

The executor module interprets decoded instructions and updates CPU state.

## Purpose

- Execute decoded instructions
- Update registers and flags
- Perform memory accesses
- Handle exceptions and interrupts

## API

```c
// include/arch/armv8m/armv8m_executor.h

/**
 * Initialize executor.
 */
int armv8m_executor_init(ARMv8MExecutor *exec, ExecutorConfig *config);

/**
 * Execute one instruction.
 *
 * @param exec Executor state
 * @param insn Decoded instruction
 * @return Status code
 */
EmuStatus armv8m_executor_step(ARMv8MExecutor *exec, const DecodedInsn *insn);

/**
 * Get current CPU state.
 */
ARMv8MCPUState* armv8m_executor_get_cpu(ARMv8MExecutor *exec);
```

## Key Data Structures

### ARMv8MCPUState

```c
typedef struct ARMv8MCPUState {
    uint32_t r[16];         // General purpose registers (R0-R15)
    uint32_t xpsr;          // Combined program status register

    // Banked stack pointers
    uint32_t msp;           // Main stack pointer
    uint32_t psp;           // Process stack pointer

    // Exception mask registers
    uint32_t primask;
    uint32_t faultmask;
    uint32_t basepri;
    uint32_t control;

    // FPU state (if enabled)
    float s[32];            // Single-precision registers
    uint32_t fpscr;         // FP status/control

    // Execution state
    uint8_t it_state;       // IT block state
    bool thumb;             // Always true for ARMv8-M
    ExecMode mode;          // Thread/Handler mode
    bool privileged;        // Privilege level
} ARMv8MCPUState;
```

### xPSR Layout

```text
31 30 29 28 27 26:25 24 23:20 19:16 15:10  9    8:0
N  Z  C  V  Q  IT    T  GE    IT    -      DSP  Exception
```

- N: Negative flag
- Z: Zero flag
- C: Carry flag
- V: Overflow flag
- Q: Saturation flag
- T: Thumb state (always 1)
- IT: If-Then state
- GE: Greater-than-or-equal flags (DSP)

## Implementation Structure

```text
src/arch/armv8m/executor/
├── executor.c              # Main executor, dispatch
├── exec_data_proc.c        # Data processing
├── exec_load_store.c       # Load/store operations
├── exec_branch.c           # Branch instructions
├── exec_multiply.c         # Multiply/accumulate
├── exec_dsp.c              # DSP extensions
├── exec_fpu.c              # FPU operations
├── exec_system.c           # System instructions
├── exec_exception.c        # Exception handling
└── tests/
    ├── test_main.cpp
    ├── test_arithmetic.cpp
    ├── test_load_store.cpp
    └── ...
```

## Execution Flow

```c
EmuStatus armv8m_executor_step(ARMv8MExecutor *exec, const DecodedInsn *insn) {
    // 1. Check IT block condition
    if (!condition_passed(exec, insn->cond)) {
        advance_it_state(exec);
        return EMU_STATUS_OK;
    }

    // 2. Dispatch to handler
    switch (insn->type) {
        case INSN_DATA_PROC_REG:
            return exec_data_proc_reg(exec, insn);
        case INSN_LOAD_STORE:
            return exec_load_store(exec, insn);
        case INSN_BRANCH:
            return exec_branch(exec, insn);
        // ...
    }

    // 3. Advance IT state
    advance_it_state(exec);

    return EMU_STATUS_OK;
}
```

## Flag Updates

Data processing instructions optionally update APSR:

```c
static void update_flags_add(ARMv8MCPUState *cpu, uint32_t a, uint32_t b, uint32_t result) {
    cpu->xpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);

    if (result & 0x80000000) cpu->xpsr |= FLAG_N;
    if (result == 0) cpu->xpsr |= FLAG_Z;
    if (result < a) cpu->xpsr |= FLAG_C;  // Carry
    if (((a ^ result) & (b ^ result)) >> 31) cpu->xpsr |= FLAG_V;  // Overflow
}
```

## Memory Access

The executor calls the memory subsystem:

```c
static EmuStatus exec_ldr(ARMv8MExecutor *exec, const DecodedInsn *insn) {
    uint32_t addr = exec->cpu.r[insn->rn];

    if (insn->add) {
        addr += insn->imm;
    } else {
        addr -= insn->imm;
    }

    uint32_t value;
    int result = exec->mem_read(exec->mem_ctx, addr, &value, 4);

    if (result != 0) {
        return EMU_STATUS_FAULT;
    }

    exec->cpu.r[insn->rd] = value;
    return EMU_STATUS_OK;
}
```

## PC Handling

The PC is R15 but has special semantics:

- Reads return PC + 4 (pipeline offset)
- Writes must have LSB set (Thumb mode)
- BX/BLX handle interworking

```c
static uint32_t read_pc(ARMv8MCPUState *cpu) {
    return cpu->r[15] + 4;  // Pipeline offset
}

static void write_pc(ARMv8MCPUState *cpu, uint32_t value) {
    cpu->r[15] = value & ~1;  // Clear Thumb bit from address
}
```

## IT Block Handling

Conditional execution via IT blocks:

```c
static bool condition_passed(ARMv8MExecutor *exec, Condition cond) {
    if (cond == COND_AL) return true;

    uint32_t flags = exec->cpu.xpsr;
    bool result;

    switch (cond & 0xE) {  // Top 3 bits
        case 0x0: result = flags & FLAG_Z; break;  // EQ/NE
        case 0x2: result = flags & FLAG_C; break;  // CS/CC
        case 0x4: result = flags & FLAG_N; break;  // MI/PL
        // ...
    }

    return (cond & 1) ? !result : result;  // Invert if LSB set
}
```

## Exception Entry

When an exception occurs:

```c
static EmuStatus enter_exception(ARMv8MExecutor *exec, int exception_num) {
    // 1. Push context to stack
    push_exception_frame(exec);

    // 2. Set LR to EXC_RETURN value
    exec->cpu.r[14] = compute_exc_return(exec);

    // 3. Switch to handler mode
    exec->cpu.mode = EXEC_MODE_HANDLER;

    // 4. Load vector and jump
    uint32_t vector_addr = exception_num * 4;
    uint32_t handler;
    exec->mem_read(exec->mem_ctx, vector_addr, &handler, 4);
    exec->cpu.r[15] = handler & ~1;

    return EMU_STATUS_OK;
}
```

## Performance Optimizations

### Computed Goto (Optional)

For performance, instruction dispatch can use computed goto:

```c
#ifdef USE_COMPUTED_GOTO
static void *dispatch_table[] = {
    [INSN_DATA_PROC_REG] = &&do_data_proc_reg,
    [INSN_LOAD_STORE] = &&do_load_store,
    // ...
};

goto *dispatch_table[insn->type];

do_data_proc_reg:
    exec_data_proc_reg(exec, insn);
    goto next;
#endif
```

### Register Caching

Frequently accessed registers can be cached:

```c
uint32_t pc = cpu->r[15];
// ... execute ...
cpu->r[15] = pc;  // Write back once
```

## Testing

Test each instruction category:

```cpp
TEST(Arithmetic, AddRegisters)
{
    cpu.r[0] = 10;
    cpu.r[1] = 20;

    DecodedInsn insn = make_add_reg(REG_R2, REG_R0, REG_R1);
    armv8m_executor_step(&exec, &insn);

    CHECK_EQUAL(30, cpu.r[2]);
}
```

## References

- ARMv8-M Architecture Reference Manual
- ARM Cortex-M33 Technical Reference Manual
