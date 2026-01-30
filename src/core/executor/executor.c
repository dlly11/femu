/**
 * @file executor.c
 * @brief Main entry point for ARMv8-M instruction executor
 *
 * Implements initialization, reset, single-step execution, and instruction dispatch.
 */

#include "armv8m_executor.h"
#include "armv8m_decoder.h"
#include <string.h>

/*============================================================================
 * Forward Declarations for Instruction Handlers
 *============================================================================*/

/* Data processing (exec_data_proc.c) */
int exec_data_proc_imm(Executor *exec, const DecodedInsn *insn);
int exec_data_proc_reg(Executor *exec, const DecodedInsn *insn);
int exec_data_proc_shifted(Executor *exec, const DecodedInsn *insn);
int exec_multiply(Executor *exec, const DecodedInsn *insn);
int exec_divide(Executor *exec, const DecodedInsn *insn);
int exec_extend(Executor *exec, const DecodedInsn *insn);
int exec_bitfield(Executor *exec, const DecodedInsn *insn);
int exec_saturate(Executor *exec, const DecodedInsn *insn);
int exec_sat_arith(Executor *exec, const DecodedInsn *insn);
int exec_parallel(Executor *exec, const DecodedInsn *insn);
int exec_pack(Executor *exec, const DecodedInsn *insn);

/* Load/store (exec_load_store.c) */
int exec_load_imm(Executor *exec, const DecodedInsn *insn);
int exec_load_reg(Executor *exec, const DecodedInsn *insn);
int exec_load_literal(Executor *exec, const DecodedInsn *insn);
int exec_store_imm(Executor *exec, const DecodedInsn *insn);
int exec_store_reg(Executor *exec, const DecodedInsn *insn);
int exec_load_multiple(Executor *exec, const DecodedInsn *insn);
int exec_store_multiple(Executor *exec, const DecodedInsn *insn);
int exec_load_exclusive(Executor *exec, const DecodedInsn *insn);
int exec_store_exclusive(Executor *exec, const DecodedInsn *insn);
int exec_clear_exclusive(Executor *exec, const DecodedInsn *insn);
int exec_load_acquire(Executor *exec, const DecodedInsn *insn);
int exec_store_release(Executor *exec, const DecodedInsn *insn);

/* Branch (exec_branch.c) */
int exec_branch(Executor *exec, const DecodedInsn *insn);
int exec_branch_link(Executor *exec, const DecodedInsn *insn);
int exec_branch_exchange(Executor *exec, const DecodedInsn *insn);
int exec_branch_link_exchange(Executor *exec, const DecodedInsn *insn);
int exec_compare_branch(Executor *exec, const DecodedInsn *insn);
int exec_table_branch(Executor *exec, const DecodedInsn *insn);
int exec_sg(Executor *exec, const DecodedInsn *insn);
int exec_bxns(Executor *exec, const DecodedInsn *insn);
int exec_blxns(Executor *exec, const DecodedInsn *insn);

/* System (exec_system.c) */
int exec_svc(Executor *exec, const DecodedInsn *insn);
int exec_mrs(Executor *exec, const DecodedInsn *insn);
int exec_msr(Executor *exec, const DecodedInsn *insn);
int exec_cps(Executor *exec, const DecodedInsn *insn);
int exec_barrier(Executor *exec, const DecodedInsn *insn);
int exec_hint(Executor *exec, const DecodedInsn *insn);
int exec_it(Executor *exec, const DecodedInsn *insn);
int exec_mcr(Executor *exec, const DecodedInsn *insn);
int exec_mrc(Executor *exec, const DecodedInsn *insn);
int exec_tt(Executor *exec, const DecodedInsn *insn);

/* FPU (exec_fpu.c) */
int exec_fpu_load(Executor *exec, const DecodedInsn *insn);
int exec_fpu_store(Executor *exec, const DecodedInsn *insn);
int exec_fpu_move(Executor *exec, const DecodedInsn *insn);
int exec_fpu_arith(Executor *exec, const DecodedInsn *insn);
int exec_fpu_cmp(Executor *exec, const DecodedInsn *insn);
int exec_fpu_cvt(Executor *exec, const DecodedInsn *insn);
int exec_fpu_multi(Executor *exec, const DecodedInsn *insn);

/*============================================================================
 * TrustZone Security Check
 *============================================================================*/

/**
 * Check security attributes of an address using SAU.
 *
 * @param exec  Executor context
 * @param addr  Address to check
 * @return      Security attribute (SEC_SECURE, SEC_NONSECURE, or SEC_NSC)
 */
SecurityAttr armv8m_check_security(const Executor *exec, uint32_t addr)
{
    if (!exec->has_trustzone) {
        /* No TrustZone - treat all memory as secure */
        return SEC_SECURE;
    }

    const SAUState *sau = &exec->sau;

    /* If SAU is disabled, check ALLNS bit */
    if (!(sau->ctrl & ARMV8M_SAU_CTRL_ENABLE)) {
        if (sau->ctrl & ARMV8M_SAU_CTRL_ALLNS) {
            return SEC_NONSECURE;
        }
        return SEC_SECURE;
    }

    /* Check each SAU region */
    for (uint32_t i = 0; i < ARMV8M_SAU_REGIONS_MAX; i++) {
        const SAURegion *region = &sau->regions[i];

        /* Check if region is enabled */
        if (!(region->rlar & ARMV8M_SAU_RLAR_ENABLE)) {
            continue;
        }

        /* Region base is bits [31:5] of RBAR */
        uint32_t base = region->rbar & 0xFFFFFFE0U;
        /* Region limit is bits [31:5] of RLAR, with bits [4:0] set to 1 */
        uint32_t limit = (region->rlar & 0xFFFFFFE0U) | 0x1FU;

        if (addr >= base && addr <= limit) {
            /* Address is in this region */
            if (region->rlar & ARMV8M_SAU_RLAR_NSC) {
                return SEC_NSC;
            }
            return SEC_NONSECURE;
        }
    }

    /* Address not in any NS region - secure by default */
    return SEC_SECURE;
}

/*============================================================================
 * Helper Functions
 *============================================================================*/

bool armv8m_check_condition(uint32_t xpsr, ConditionCode cond)
{
    bool n = (xpsr >> 31) & 1;
    bool z = (xpsr >> 30) & 1;
    bool c = (xpsr >> 29) & 1;
    bool v = (xpsr >> 28) & 1;

    switch (cond) {
        case COND_EQ: return z;
        case COND_NE: return !z;
        case COND_CS: return c;
        case COND_CC: return !c;
        case COND_MI: return n;
        case COND_PL: return !n;
        case COND_VS: return v;
        case COND_VC: return !v;
        case COND_HI: return c && !z;
        case COND_LS: return !c || z;
        case COND_GE: return n == v;
        case COND_LT: return n != v;
        case COND_GT: return !z && (n == v);
        case COND_LE: return z || (n != v);
        case COND_AL: return true;
        case COND_NV: return true;  /* Unconditional in Thumb */
        default: return true;
    }
}

void armv8m_update_flags(CPUState *cpu, uint32_t result, bool carry, bool overflow)
{
    cpu->xpsr &= ~(ARMV8M_XPSR_N | ARMV8M_XPSR_Z | ARMV8M_XPSR_C | ARMV8M_XPSR_V);

    if (result & 0x80000000) {
        cpu->xpsr |= ARMV8M_XPSR_N;
    }
    if (result == 0) {
        cpu->xpsr |= ARMV8M_XPSR_Z;
    }
    if (carry) {
        cpu->xpsr |= ARMV8M_XPSR_C;
    }
    if (overflow) {
        cpu->xpsr |= ARMV8M_XPSR_V;
    }
}

uint32_t armv8m_get_sp(const CPUState *cpu)
{
    if (cpu->mode == MODE_HANDLER) {
        return cpu->sp_main;
    }
    return (cpu->control & ARMV8M_CONTROL_SPSEL) ? cpu->sp_process : cpu->sp_main;
}

void armv8m_set_sp(CPUState *cpu, uint32_t value)
{
    /* SP must be word-aligned */
    value &= ~3u;

    if (cpu->mode == MODE_HANDLER) {
        cpu->sp_main = value;
    } else if (cpu->control & ARMV8M_CONTROL_SPSEL) {
        cpu->sp_process = value;
    } else {
        cpu->sp_main = value;
    }
}

/**
 * Check if SP value would violate stack limits.
 * Returns true if limit would be violated.
 */
bool armv8m_check_stack_limit(const CPUState *cpu, uint32_t sp_value)
{
    uint32_t limit;

    if (cpu->mode == MODE_HANDLER) {
        limit = cpu->msplim;
    } else if (cpu->control & ARMV8M_CONTROL_SPSEL) {
        limit = cpu->psplim;
    } else {
        limit = cpu->msplim;
    }

    return sp_value < limit;
}

/**
 * Check if currently in IT block and get condition for current instruction.
 *
 * ARM ITSTATE encoding:
 * - Bits 7:5 = cond[3:1] (upper 3 bits of base condition)
 * - Bits 4:0 = cond[0]:mask[3:0] (shifts left after each instruction)
 *
 * The effective condition is cond[3:1] : ITSTATE[4].
 * Initially ITSTATE[4] = cond[0], giving the base condition.
 * After shift, ITSTATE[4] becomes mask[3], which encodes T/E for next instruction.
 */
static ConditionCode get_it_condition(const CPUState *cpu)
{
    if (cpu->it_state == 0) {
        return COND_AL;
    }

    /* Extract condition: cond[3:1] from bits 7:5, cond[0] from bit 4 */
    uint8_t cond = (uint8_t)(((cpu->it_state >> 4) & 0xE) | ((cpu->it_state >> 4) & 0x1));
    return (ConditionCode)cond;
}

/**
 * Advance IT state after executing an instruction.
 *
 * Shifts ITSTATE[4:0] left by 1. When the remaining mask bits are all 0,
 * the IT block is complete.
 */
static void advance_it_state(CPUState *cpu)
{
    if (cpu->it_state == 0) {
        return;
    }

    /* Check if this was the last instruction (mask bits 3:0 are 0 after current) */
    if ((cpu->it_state & 0x0F) == 0x08) {
        /* Only one instruction remaining (mask = 1000), now done */
        cpu->it_state = 0;
    } else {
        /* Shift bits 4:0 left, preserving bits 7:5 */
        uint8_t upper = cpu->it_state & 0xE0;  /* cond[3:1] */
        uint8_t lower = cpu->it_state & 0x1F;  /* cond[0]:mask */
        cpu->it_state = upper | ((lower << 1) & 0x1F);
    }
}

/*============================================================================
 * Executor Initialization and Reset
 *============================================================================*/

void armv8m_exec_init(Executor *exec)
{
    memset(exec, 0, sizeof(Executor));

    /* Set default CPU state */
    exec->cpu.mode = MODE_THREAD;
    exec->cpu.privilege = PRIV_PRIVILEGED;
    exec->cpu.security = SECURITY_NONSECURE;
    exec->cpu.xpsr = ARMV8M_XPSR_T;  /* Thumb bit always set */

    /* Initialize FPU state with defaults */
    exec->cpu.fpccr = ARMV8M_FPCCR_ASPEN | ARMV8M_FPCCR_LSPEN;  /* Lazy preservation enabled */

    /* Initialize SAU with default region count */
    exec->sau.type = ARMV8M_SAU_REGIONS_MAX;  /* Report max regions */
}

void armv8m_exec_reset(Executor *exec, uint32_t vtor)
{
    CPUState *cpu = &exec->cpu;
    bool fault = false;

    /* Store VTOR for exception handling */
    exec->vtor = vtor;
    exec->vtor_ns = 0;  /* Default NS VTOR at address 0 */

    /* Clear registers */
    memset(cpu->r, 0, sizeof(cpu->r));

    /* Reset special registers */
    cpu->primask = 0;
    cpu->faultmask = 0;
    cpu->basepri = 0;
    cpu->control = 0;

    /* Reset stack limits */
    cpu->msplim = 0;
    cpu->psplim = 0;

    /* Reset system control - default CCR has STKALIGN set */
    cpu->ccr = ARMV8M_CCR_STKALIGN;

    /* Reset fault status registers */
    cpu->cfsr = 0;
    cpu->hfsr = 0;
    cpu->mmfar = 0;
    cpu->bfar = 0;

    /* Load SP from vector table (first entry) */
    if (exec->mem.read) {
        cpu->sp_main = exec->mem.read(exec->mem.ctx, vtor, 4, &fault);
        cpu->sp_main &= ~3u;  /* Ensure word alignment */
    }
    cpu->sp_process = 0;
    cpu->r[ARMV8M_REG_SP] = cpu->sp_main;

    /* Load PC from reset vector (second entry) */
    if (exec->mem.read) {
        uint32_t reset_vector = exec->mem.read(exec->mem.ctx, vtor + 4, 4, &fault);
        cpu->r[ARMV8M_REG_PC] = reset_vector & ~1u;  /* Clear Thumb bit for PC */
    }

    /* Reset execution state */
    cpu->mode = MODE_THREAD;
    cpu->privilege = PRIV_PRIVILEGED;
    cpu->xpsr = ARMV8M_XPSR_T;  /* Thumb bit set */
    cpu->it_state = 0;
    cpu->current_exception = 0;
    cpu->pending_irq = 0;
    cpu->event_registered = false;
    cpu->cycles = 0;
    cpu->halted = false;
    cpu->sleeping = false;
    cpu->exclusive_valid = false;
    cpu->exclusive_addr = 0;

    /* Reset FPU state */
    memset(cpu->s, 0, sizeof(cpu->s));
    cpu->fpscr = 0;
    cpu->fpccr = ARMV8M_FPCCR_ASPEN | ARMV8M_FPCCR_LSPEN;  /* Default enabled */
    cpu->fpcar = 0;
    cpu->fpdscr = 0;
    cpu->fp_context_active = false;

    /* Reset SAU state */
    memset(&exec->sau, 0, sizeof(exec->sau));
    exec->sau.type = ARMV8M_SAU_REGIONS_MAX;

    /* Reset TrustZone banked registers */
    memset(&exec->tz_regs, 0, sizeof(exec->tz_regs));

    /* Reset security state - Secure if TrustZone is present */
    if (exec->has_trustzone) {
        cpu->security = SECURITY_SECURE;
    }
}

/*============================================================================
 * Instruction Dispatch
 *============================================================================*/

int armv8m_exec_insn(Executor *exec, const DecodedInsn *insn)
{
    switch (insn->type) {
        /* Data processing */
        case INSN_DATA_PROC_IMM:
            return exec_data_proc_imm(exec, insn);
        case INSN_DATA_PROC_REG:
            return exec_data_proc_reg(exec, insn);
        case INSN_DATA_PROC_SHIFTED:
            return exec_data_proc_shifted(exec, insn);
        case INSN_MULTIPLY:
            return exec_multiply(exec, insn);
        case INSN_DIVIDE:
            return exec_divide(exec, insn);
        case INSN_EXTEND:
            return exec_extend(exec, insn);
        case INSN_BITFIELD:
            return exec_bitfield(exec, insn);
        case INSN_SATURATE:
            return exec_saturate(exec, insn);
        case INSN_SAT_ARITH:
            return exec_sat_arith(exec, insn);
        case INSN_PARALLEL:
            return exec_parallel(exec, insn);
        case INSN_PACK:
            return exec_pack(exec, insn);

        /* Load/store */
        case INSN_LOAD_IMM:
            return exec_load_imm(exec, insn);
        case INSN_LOAD_REG:
            return exec_load_reg(exec, insn);
        case INSN_LOAD_LITERAL:
            return exec_load_literal(exec, insn);
        case INSN_STORE_IMM:
            return exec_store_imm(exec, insn);
        case INSN_STORE_REG:
            return exec_store_reg(exec, insn);
        case INSN_LOAD_MULTIPLE:
            return exec_load_multiple(exec, insn);
        case INSN_STORE_MULTIPLE:
            return exec_store_multiple(exec, insn);
        case INSN_LOAD_EXCLUSIVE:
            return exec_load_exclusive(exec, insn);
        case INSN_STORE_EXCLUSIVE:
            return exec_store_exclusive(exec, insn);
        case INSN_CLEAR_EXCLUSIVE:
            return exec_clear_exclusive(exec, insn);
        case INSN_LOAD_ACQUIRE:
            return exec_load_acquire(exec, insn);
        case INSN_STORE_RELEASE:
            return exec_store_release(exec, insn);

        /* Branch */
        case INSN_BRANCH:
            return exec_branch(exec, insn);
        case INSN_BRANCH_LINK:
            return exec_branch_link(exec, insn);
        case INSN_BRANCH_EXCHANGE:
            return exec_branch_exchange(exec, insn);
        case INSN_BRANCH_LINK_EXCHANGE:
            return exec_branch_link_exchange(exec, insn);
        case INSN_COMPARE_BRANCH:
            return exec_compare_branch(exec, insn);
        case INSN_TABLE_BRANCH:
            return exec_table_branch(exec, insn);

        /* TrustZone */
        case INSN_SG:
            return exec_sg(exec, insn);
        case INSN_BXNS:
            return exec_bxns(exec, insn);
        case INSN_BLXNS:
            return exec_blxns(exec, insn);
        case INSN_TT:
            return exec_tt(exec, insn);

        /* System */
        case INSN_SVC:
            return exec_svc(exec, insn);
        case INSN_MRS:
            return exec_mrs(exec, insn);
        case INSN_MSR:
            return exec_msr(exec, insn);
        case INSN_CPS:
            return exec_cps(exec, insn);
        case INSN_BARRIER:
            return exec_barrier(exec, insn);
        case INSN_HINT:
            return exec_hint(exec, insn);
        case INSN_IT:
            return exec_it(exec, insn);

        /* Coprocessor */
        case INSN_MCR:
            return exec_mcr(exec, insn);
        case INSN_MRC:
            return exec_mrc(exec, insn);

        /* FPU */
        case INSN_FPU_LOAD:
            return exec_fpu_load(exec, insn);
        case INSN_FPU_STORE:
            return exec_fpu_store(exec, insn);
        case INSN_FPU_MOVE:
            return exec_fpu_move(exec, insn);
        case INSN_FPU_ARITH:
            return exec_fpu_arith(exec, insn);
        case INSN_FPU_CMP:
            return exec_fpu_cmp(exec, insn);
        case INSN_FPU_CVT:
            return exec_fpu_cvt(exec, insn);
        case INSN_FPU_MULTI:
            return exec_fpu_multi(exec, insn);

        case INSN_UNDEFINED:
        default:
            return ARMV8M_ERR_UNDEFINED_INSN;
    }
}

/*============================================================================
 * Single-Step Execution
 *============================================================================*/

int armv8m_exec_step(Executor *exec)
{
    CPUState *cpu = &exec->cpu;
    DecodedInsn insn;
    int result;

    /* Check if halted or sleeping */
    if (cpu->halted) {
        return ARMV8M_ERR_HALTED;
    }

    if (cpu->sleeping) {
        /* Check for pending interrupts to wake up */
        if (exec->nvic.get_pending && exec->nvic.get_pending(exec->nvic.ctx) >= 0) {
            cpu->sleeping = false;
        } else {
            return ARMV8M_OK;  /* Still sleeping */
        }
    }

    /* Fetch instruction bytes */
    uint32_t pc = cpu->r[ARMV8M_REG_PC];
    const uint8_t *mem = NULL;

    if (exec->mem.get_ptr) {
        mem = exec->mem.get_ptr(exec->mem.ctx, pc, 4);
    }

    if (!mem) {
        return ARMV8M_ERR_BUS_FAULT;
    }

    /* Decode instruction */
    armv8m_decode_init(&insn);
    result = armv8m_decode(mem, pc, &insn);

    if (result < 0) {
        return result;
    }

    /* Check IT block condition */
    ConditionCode it_cond = get_it_condition(cpu);
    if (it_cond != COND_AL) {
        if (!armv8m_check_condition(cpu->xpsr, it_cond)) {
            /* Condition failed, skip instruction */
            cpu->r[ARMV8M_REG_PC] += insn.size;
            advance_it_state(cpu);
            cpu->cycles++;
            return ARMV8M_OK;
        }
    }

    /* Check instruction's own condition (for conditional branches) */
    if (insn.cond != COND_AL && insn.type == INSN_BRANCH) {
        if (!armv8m_check_condition(cpu->xpsr, insn.cond)) {
            /* Condition failed, skip instruction */
            cpu->r[ARMV8M_REG_PC] += insn.size;
            advance_it_state(cpu);
            cpu->cycles++;
            return ARMV8M_OK;
        }
    }

    /* Save PC for branch detection */
    uint32_t old_pc = cpu->r[ARMV8M_REG_PC];

    /* Execute instruction */
    result = armv8m_exec_insn(exec, &insn);

    if (result != ARMV8M_OK) {
        return result;
    }

    /* Update PC if not modified by instruction (branch, etc.) */
    if (cpu->r[ARMV8M_REG_PC] == old_pc) {
        cpu->r[ARMV8M_REG_PC] += insn.size;
    }

    /* Advance IT state (but not after IT instruction itself - it sets up the block) */
    if (insn.type != INSN_IT) {
        advance_it_state(cpu);
    }

    /* Update cycle counter */
    cpu->cycles++;

    /* Check for pending exceptions */
    if (exec->nvic.get_pending) {
        int pending = exec->nvic.get_pending(exec->nvic.ctx);
        if (pending >= 0) {
            /* Get current execution priority */
            int current_pri = -1;
            if (cpu->current_exception > 0 && exec->nvic.get_priority) {
                current_pri = exec->nvic.get_priority(exec->nvic.ctx, cpu->current_exception);
            }

            int pending_pri = exec->nvic.get_priority ?
                              exec->nvic.get_priority(exec->nvic.ctx, pending) : 0;

            if (pending_pri < current_pri || current_pri < 0) {
                result = armv8m_exception_entry(exec, pending);
            }
        }
    }

    return result;
}

/*============================================================================
 * Continuous Execution
 *============================================================================*/

int64_t armv8m_exec_run(Executor *exec, uint64_t max_cycles)
{
    uint64_t start_cycles = exec->cpu.cycles;
    int result = ARMV8M_OK;

    while (result == ARMV8M_OK) {
        if (max_cycles > 0 && (exec->cpu.cycles - start_cycles) >= max_cycles) {
            break;
        }

        result = armv8m_exec_step(exec);

        if (result == ARMV8M_ERR_HALTED) {
            break;
        }

        /* Stop when CPU enters sleep (WFI/WFE) or halt state */
        if (exec->cpu.sleeping || exec->cpu.halted) {
            break;
        }
    }

    return (int64_t)(exec->cpu.cycles - start_cycles);
}
