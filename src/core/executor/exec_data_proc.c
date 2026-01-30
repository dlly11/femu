/**
 * @file exec_data_proc.c
 * @brief Data processing instruction execution for ARMv8-M
 *
 * Implements ADD, SUB, MOV, CMP, AND, ORR, EOR, shifts, multiply, divide, etc.
 */

#include "armv8m_executor.h"
#include "armv8m_types.h"

/*============================================================================
 * Internal Helpers
 *============================================================================*/

/**
 * Add with carry and overflow detection.
 */
static uint32_t add_with_flags(uint32_t a, uint32_t b, uint32_t carry_in,
                               bool *carry_out, bool *overflow)
{
    uint64_t result = (uint64_t)a + b + carry_in;
    *carry_out = result > 0xFFFFFFFF;
    /* Overflow: both operands same sign, result different sign */
    *overflow = (((a ^ ~b) & (a ^ (uint32_t)result)) >> 31) & 1;
    return (uint32_t)result;
}

/**
 * Subtract with borrow detection: a - b = a + ~b + 1
 */
static uint32_t sub_with_flags(uint32_t a, uint32_t b,
                               bool *carry_out, bool *overflow)
{
    return add_with_flags(a, ~b, 1, carry_out, overflow);
}

/**
 * Subtract with carry: a - b - !C = a + ~b + C
 */
static uint32_t sbc_with_flags(uint32_t a, uint32_t b, bool carry_in,
                               bool *carry_out, bool *overflow)
{
    return add_with_flags(a, ~b, carry_in ? 1 : 0, carry_out, overflow);
}

/**
 * Apply shift operation with carry out.
 */
static uint32_t apply_shift(uint32_t value, ShiftType type, uint8_t amount,
                            bool carry_in, bool *carry_out)
{
    *carry_out = carry_in;

    if (amount == 0 && type != SHIFT_RRX && type != SHIFT_LSR && type != SHIFT_ASR) {
        /* LSR #0 means LSR #32, ASR #0 means ASR #32 - handle in switch */
        return value;
    }

    switch (type) {
        case SHIFT_LSL:
            if (amount == 0) {
                return value;
            } else if (amount < 32) {
                *carry_out = (value >> (32 - amount)) & 1;
                return value << amount;
            } else if (amount == 32) {
                *carry_out = value & 1;
                return 0;
            } else {
                *carry_out = false;
                return 0;
            }

        case SHIFT_LSR:
            if (amount == 0) {
                /* LSR #0 is encoded as LSR #32 */
                *carry_out = (value >> 31) & 1;
                return 0;
            } else if (amount < 32) {
                *carry_out = (value >> (amount - 1)) & 1;
                return value >> amount;
            } else if (amount == 32) {
                *carry_out = (value >> 31) & 1;
                return 0;
            } else {
                *carry_out = false;
                return 0;
            }

        case SHIFT_ASR:
            if (amount == 0) {
                /* ASR #0 is encoded as ASR #32 */
                *carry_out = (value >> 31) & 1;
                return (uint32_t)((int32_t)value >> 31);
            } else if (amount < 32) {
                *carry_out = (value >> (amount - 1)) & 1;
                return (uint32_t)((int32_t)value >> amount);
            } else {
                *carry_out = (value >> 31) & 1;
                return (uint32_t)((int32_t)value >> 31);
            }

        case SHIFT_ROR:
            if (amount == 0) {
                return value;
            } else {
                amount &= 31;
                if (amount == 0) {
                    *carry_out = (value >> 31) & 1;
                    return value;
                }
                *carry_out = (value >> (amount - 1)) & 1;
                return (value >> amount) | (value << (32 - amount));
            }

        case SHIFT_RRX:
            /* Rotate right with extend: shift right by 1, carry in to bit 31 */
            *carry_out = value & 1;
            return (value >> 1) | (carry_in ? 0x80000000 : 0);

        default:
            return value;
    }
}

/**
 * Get register value, handling SP specially for R13.
 */
static uint32_t get_reg(const Executor *exec, uint8_t reg)
{
    if (reg == ARMV8M_REG_SP) {
        return armv8m_get_sp(&exec->cpu);
    }
    return exec->cpu.r[reg];
}

/**
 * Set register value, handling SP specially for R13.
 */
static void set_reg(Executor *exec, uint8_t reg, uint32_t value)
{
    if (reg == ARMV8M_REG_SP) {
        armv8m_set_sp(&exec->cpu, value);
        exec->cpu.r[ARMV8M_REG_SP] = armv8m_get_sp(&exec->cpu);
    } else if (reg == ARMV8M_REG_PC) {
        /* PC writes must be aligned and may trigger exception return */
        exec->cpu.r[ARMV8M_REG_PC] = value & ~1u;
    } else {
        exec->cpu.r[reg] = value;
    }
}

/**
 * Execute a data processing operation.
 */
static int exec_dp_operation(Executor *exec, DataProcOp op,
                             uint8_t rd, uint32_t rn_val, uint32_t op2,
                             bool set_flags, bool shift_carry)
{
    CPUState *cpu = &exec->cpu;
    uint32_t result = 0;
    bool carry = shift_carry;
    bool overflow = false;
    bool write_result = true;

    /* Get current carry flag for ADC/SBC */
    bool c_flag = (cpu->xpsr >> 29) & 1;

    switch (op) {
        case DP_AND:
            result = rn_val & op2;
            break;

        case DP_EOR:
            result = rn_val ^ op2;
            break;

        case DP_LSL:
            result = apply_shift(rn_val, SHIFT_LSL, op2 & 0xFF, c_flag, &carry);
            break;

        case DP_LSR:
            result = apply_shift(rn_val, SHIFT_LSR, op2 & 0xFF, c_flag, &carry);
            break;

        case DP_ASR:
            result = apply_shift(rn_val, SHIFT_ASR, op2 & 0xFF, c_flag, &carry);
            break;

        case DP_ADC:
            result = add_with_flags(rn_val, op2, c_flag ? 1 : 0, &carry, &overflow);
            break;

        case DP_SBC:
            result = sbc_with_flags(rn_val, op2, c_flag, &carry, &overflow);
            break;

        case DP_ROR:
            result = apply_shift(rn_val, SHIFT_ROR, op2 & 0xFF, c_flag, &carry);
            break;

        case DP_TST:
            result = rn_val & op2;
            write_result = false;
            set_flags = true;  /* TST always sets flags */
            break;

        case DP_RSB:
            /* Reverse subtract: op2 - rn_val */
            result = sub_with_flags(op2, rn_val, &carry, &overflow);
            break;

        case DP_CMP:
            result = sub_with_flags(rn_val, op2, &carry, &overflow);
            write_result = false;
            set_flags = true;  /* CMP always sets flags */
            break;

        case DP_CMN:
            result = add_with_flags(rn_val, op2, 0, &carry, &overflow);
            write_result = false;
            set_flags = true;  /* CMN always sets flags */
            break;

        case DP_ORR:
            result = rn_val | op2;
            break;

        case DP_MUL:
            result = rn_val * op2;
            /* MUL does not affect C or V flags in M-profile */
            break;

        case DP_BIC:
            result = rn_val & ~op2;
            break;

        case DP_MVN:
            result = ~op2;
            break;

        case DP_ADD:
            result = add_with_flags(rn_val, op2, 0, &carry, &overflow);
            break;

        case DP_SUB:
            result = sub_with_flags(rn_val, op2, &carry, &overflow);
            break;

        case DP_MOV:
            result = op2;
            break;

        case DP_ORN:
            result = rn_val | ~op2;
            break;

        default:
            return ARMV8M_ERR_UNDEFINED_INSN;
    }

    /* Update flags if requested */
    if (set_flags) {
        armv8m_update_flags(cpu, result, carry, overflow);
    }

    /* Write result to destination register */
    if (write_result && rd != ARMV8M_REG_NONE) {
        set_reg(exec, rd, result);
    }

    return ARMV8M_OK;
}

/*============================================================================
 * Public Instruction Handlers
 *============================================================================*/

int exec_data_proc_imm(Executor *exec, const DecodedInsn *insn)
{
    uint32_t rn_val = 0;
    bool shift_carry = (exec->cpu.xpsr >> 29) & 1;

    /* For most operations, Rn is a source. For MOV/MVN, it's not used. */
    if (insn->rn != ARMV8M_REG_NONE) {
        rn_val = get_reg(exec, insn->rn);
    }

    /* In ARM, 16-bit Thumb instructions inside an IT block do NOT update
     * condition flags. Only 32-bit instructions with explicit S suffix do.
     * Check if we're in IT block (it_state != 0) and instruction is 16-bit. */
    bool set_flags = insn->set_flags;
    if (exec->cpu.it_state != 0 && insn->size == 2) {
        set_flags = false;
    }

    return exec_dp_operation(exec, (DataProcOp)insn->op,
                             insn->rd, rn_val, insn->imm,
                             set_flags, shift_carry);
}

/**
 * Reverse the bits in a 32-bit word.
 */
static uint32_t reverse_bits(uint32_t val)
{
    val = ((val & 0x55555555) << 1) | ((val & 0xAAAAAAAA) >> 1);
    val = ((val & 0x33333333) << 2) | ((val & 0xCCCCCCCC) >> 2);
    val = ((val & 0x0F0F0F0F) << 4) | ((val & 0xF0F0F0F0) >> 4);
    val = ((val & 0x00FF00FF) << 8) | ((val & 0xFF00FF00) >> 8);
    val = (val << 16) | (val >> 16);
    return val;
}

/**
 * Count leading zeros in a 32-bit word.
 */
static uint32_t count_leading_zeros(uint32_t val)
{
    if (val == 0) return 32;
    uint32_t count = 0;
    if ((val & 0xFFFF0000) == 0) { count += 16; val <<= 16; }
    if ((val & 0xFF000000) == 0) { count += 8; val <<= 8; }
    if ((val & 0xF0000000) == 0) { count += 4; val <<= 4; }
    if ((val & 0xC0000000) == 0) { count += 2; val <<= 2; }
    if ((val & 0x80000000) == 0) { count += 1; }
    return count;
}

int exec_data_proc_reg(Executor *exec, const DecodedInsn *insn)
{
    uint32_t rn_val = 0;
    uint32_t rm_val = 0;
    bool shift_carry = (exec->cpu.xpsr >> 29) & 1;

    if (insn->rn != ARMV8M_REG_NONE) {
        rn_val = get_reg(exec, insn->rn);
    }

    if (insn->rm != ARMV8M_REG_NONE) {
        rm_val = get_reg(exec, insn->rm);
    }

    /* Check for special byte manipulation instructions encoded with markers */
    if (insn->op == DP_ROR && insn->shift_amount >= 0x10 && insn->shift_amount <= 0x13) {
        uint32_t result;
        switch (insn->shift_amount) {
        case 0x10: /* REV - byte reverse word */
            result = ((rm_val & 0xFF) << 24) |
                     ((rm_val & 0xFF00) << 8) |
                     ((rm_val & 0xFF0000) >> 8) |
                     ((rm_val & 0xFF000000) >> 24);
            break;
        case 0x11: /* REV16 - byte reverse packed halfwords */
            result = ((rm_val & 0x00FF) << 8) |
                     ((rm_val & 0xFF00) >> 8) |
                     ((rm_val & 0x00FF0000) << 8) |
                     ((rm_val & 0xFF000000) >> 8);
            break;
        case 0x12: /* RBIT - bit reverse */
            result = reverse_bits(rm_val);
            break;
        case 0x13: /* REVSH - byte reverse signed halfword */
            result = (uint32_t)(int32_t)(int16_t)(
                ((rm_val & 0xFF) << 8) | ((rm_val & 0xFF00) >> 8)
            );
            break;
        default:
            result = 0;
            break;
        }
        set_reg(exec, insn->rd, result);
        return ARMV8M_OK;
    }

    /* Check for CLZ - encoded with DP_MVN and shift_amount = 0x20 */
    if (insn->op == DP_MVN && insn->shift_amount == 0x20) {
        uint32_t result = count_leading_zeros(rm_val);
        set_reg(exec, insn->rd, result);
        return ARMV8M_OK;
    }

    /* In ARM, 16-bit Thumb instructions inside an IT block do NOT update
     * condition flags. Only 32-bit instructions with explicit S suffix do. */
    bool set_flags = insn->set_flags;
    if (exec->cpu.it_state != 0 && insn->size == 2) {
        set_flags = false;
    }

    return exec_dp_operation(exec, (DataProcOp)insn->op,
                             insn->rd, rn_val, rm_val,
                             set_flags, shift_carry);
}

int exec_data_proc_shifted(Executor *exec, const DecodedInsn *insn)
{
    uint32_t rn_val = 0;
    uint32_t rm_val = 0;
    bool carry_in = (exec->cpu.xpsr >> 29) & 1;
    bool shift_carry = carry_in;

    if (insn->rn != ARMV8M_REG_NONE) {
        rn_val = get_reg(exec, insn->rn);
    }

    if (insn->rm != ARMV8M_REG_NONE) {
        rm_val = get_reg(exec, insn->rm);
    }

    /* Apply shift to Rm */
    uint32_t shifted = apply_shift(rm_val, insn->shift_type, insn->shift_amount,
                                   carry_in, &shift_carry);

    return exec_dp_operation(exec, (DataProcOp)insn->op,
                             insn->rd, rn_val, shifted,
                             insn->set_flags, shift_carry);
}

/**
 * Extract bottom halfword as signed value.
 */
static int16_t get_bottom_half(uint32_t val)
{
    return (int16_t)(val & 0xFFFF);
}

/**
 * Extract top halfword as signed value.
 */
static int16_t get_top_half(uint32_t val)
{
    return (int16_t)(val >> 16);
}

/**
 * Saturating add helper - sets Q flag on overflow.
 */
static int32_t saturate_add_q(CPUState *cpu, int64_t val)
{
    if (val > INT32_MAX) {
        cpu->xpsr |= ARMV8M_XPSR_Q;
        return INT32_MAX;
    }
    if (val < INT32_MIN) {
        cpu->xpsr |= ARMV8M_XPSR_Q;
        return INT32_MIN;
    }
    return (int32_t)val;
}

int exec_multiply(Executor *exec, const DecodedInsn *insn)
{
    CPUState *cpu = &exec->cpu;
    uint32_t rn_val = get_reg(exec, insn->rn);
    uint32_t rm_val = get_reg(exec, insn->rm);
    uint32_t ra_val = (insn->rs != ARMV8M_REG_NONE) ? get_reg(exec, insn->rs) : 0;
    uint32_t result = 0;
    uint64_t result64 = 0;
    bool is_long = false;
    bool carry = (cpu->xpsr >> 29) & 1;

    switch ((MultiplyOp)insn->op) {
        /* Basic 32-bit multiply */
        case MUL_MUL:
            result = rn_val * rm_val;
            break;

        case MUL_MLA:
            result = ra_val + (rn_val * rm_val);
            break;

        case MUL_MLS:
            result = ra_val - (rn_val * rm_val);
            break;

        /* Long multiply (64-bit result) */
        case MUL_SMULL:
            result64 = (uint64_t)((int64_t)(int32_t)rn_val * (int64_t)(int32_t)rm_val);
            is_long = true;
            break;

        case MUL_UMULL:
            result64 = (uint64_t)rn_val * (uint64_t)rm_val;
            is_long = true;
            break;

        case MUL_SMLAL: {
            int64_t acc = ((int64_t)(int32_t)get_reg(exec, insn->rt) << 32) |
                          get_reg(exec, insn->rd);
            result64 = (uint64_t)(acc + (int64_t)(int32_t)rn_val * (int64_t)(int32_t)rm_val);
            is_long = true;
            break;
        }

        case MUL_UMLAL: {
            uint64_t acc = ((uint64_t)get_reg(exec, insn->rt) << 32) |
                           get_reg(exec, insn->rd);
            result64 = acc + (uint64_t)rn_val * (uint64_t)rm_val;
            is_long = true;
            break;
        }

        /* DSP halfword multiply */
        case MUL_SMULBB:
            result = (uint32_t)((int32_t)get_bottom_half(rn_val) * (int32_t)get_bottom_half(rm_val));
            break;

        case MUL_SMULBT:
            result = (uint32_t)((int32_t)get_bottom_half(rn_val) * (int32_t)get_top_half(rm_val));
            break;

        case MUL_SMULTB:
            result = (uint32_t)((int32_t)get_top_half(rn_val) * (int32_t)get_bottom_half(rm_val));
            break;

        case MUL_SMULTT:
            result = (uint32_t)((int32_t)get_top_half(rn_val) * (int32_t)get_top_half(rm_val));
            break;

        /* DSP halfword multiply-accumulate with Q flag */
        case MUL_SMLABB: {
            int32_t prod = (int32_t)get_bottom_half(rn_val) * (int32_t)get_bottom_half(rm_val);
            result = (uint32_t)saturate_add_q(cpu, (int64_t)(int32_t)ra_val + prod);
            break;
        }

        case MUL_SMLABT: {
            int32_t prod = (int32_t)get_bottom_half(rn_val) * (int32_t)get_top_half(rm_val);
            result = (uint32_t)saturate_add_q(cpu, (int64_t)(int32_t)ra_val + prod);
            break;
        }

        case MUL_SMLATB: {
            int32_t prod = (int32_t)get_top_half(rn_val) * (int32_t)get_bottom_half(rm_val);
            result = (uint32_t)saturate_add_q(cpu, (int64_t)(int32_t)ra_val + prod);
            break;
        }

        case MUL_SMLATT: {
            int32_t prod = (int32_t)get_top_half(rn_val) * (int32_t)get_top_half(rm_val);
            result = (uint32_t)saturate_add_q(cpu, (int64_t)(int32_t)ra_val + prod);
            break;
        }

        /* Signed halfword x word multiply */
        case MUL_SMULWB:
            result = (uint32_t)(((int64_t)(int32_t)rn_val * (int32_t)get_bottom_half(rm_val)) >> 16);
            break;

        case MUL_SMULWT:
            result = (uint32_t)(((int64_t)(int32_t)rn_val * (int32_t)get_top_half(rm_val)) >> 16);
            break;

        case MUL_SMLAWB: {
            int32_t prod = (int32_t)(((int64_t)(int32_t)rn_val * (int32_t)get_bottom_half(rm_val)) >> 16);
            result = (uint32_t)saturate_add_q(cpu, (int64_t)(int32_t)ra_val + prod);
            break;
        }

        case MUL_SMLAWT: {
            int32_t prod = (int32_t)(((int64_t)(int32_t)rn_val * (int32_t)get_top_half(rm_val)) >> 16);
            result = (uint32_t)saturate_add_q(cpu, (int64_t)(int32_t)ra_val + prod);
            break;
        }

        /* Dual multiply */
        case MUL_SMUAD: {
            int32_t p1 = (int32_t)get_bottom_half(rn_val) * (int32_t)get_bottom_half(rm_val);
            int32_t p2 = (int32_t)get_top_half(rn_val) * (int32_t)get_top_half(rm_val);
            result = (uint32_t)saturate_add_q(cpu, (int64_t)p1 + p2);
            break;
        }

        case MUL_SMUADX: {
            int32_t p1 = (int32_t)get_bottom_half(rn_val) * (int32_t)get_top_half(rm_val);
            int32_t p2 = (int32_t)get_top_half(rn_val) * (int32_t)get_bottom_half(rm_val);
            result = (uint32_t)saturate_add_q(cpu, (int64_t)p1 + p2);
            break;
        }

        case MUL_SMUSD: {
            int32_t p1 = (int32_t)get_bottom_half(rn_val) * (int32_t)get_bottom_half(rm_val);
            int32_t p2 = (int32_t)get_top_half(rn_val) * (int32_t)get_top_half(rm_val);
            result = (uint32_t)(p1 - p2);
            break;
        }

        case MUL_SMUSDX: {
            int32_t p1 = (int32_t)get_bottom_half(rn_val) * (int32_t)get_top_half(rm_val);
            int32_t p2 = (int32_t)get_top_half(rn_val) * (int32_t)get_bottom_half(rm_val);
            result = (uint32_t)(p1 - p2);
            break;
        }

        /* Dual multiply-accumulate */
        case MUL_SMLAD: {
            int32_t p1 = (int32_t)get_bottom_half(rn_val) * (int32_t)get_bottom_half(rm_val);
            int32_t p2 = (int32_t)get_top_half(rn_val) * (int32_t)get_top_half(rm_val);
            result = (uint32_t)saturate_add_q(cpu, (int64_t)(int32_t)ra_val + p1 + p2);
            break;
        }

        case MUL_SMLADX: {
            int32_t p1 = (int32_t)get_bottom_half(rn_val) * (int32_t)get_top_half(rm_val);
            int32_t p2 = (int32_t)get_top_half(rn_val) * (int32_t)get_bottom_half(rm_val);
            result = (uint32_t)saturate_add_q(cpu, (int64_t)(int32_t)ra_val + p1 + p2);
            break;
        }

        case MUL_SMLSD: {
            int32_t p1 = (int32_t)get_bottom_half(rn_val) * (int32_t)get_bottom_half(rm_val);
            int32_t p2 = (int32_t)get_top_half(rn_val) * (int32_t)get_top_half(rm_val);
            result = (uint32_t)saturate_add_q(cpu, (int64_t)(int32_t)ra_val + p1 - p2);
            break;
        }

        case MUL_SMLSDX: {
            int32_t p1 = (int32_t)get_bottom_half(rn_val) * (int32_t)get_top_half(rm_val);
            int32_t p2 = (int32_t)get_top_half(rn_val) * (int32_t)get_bottom_half(rm_val);
            result = (uint32_t)saturate_add_q(cpu, (int64_t)(int32_t)ra_val + p1 - p2);
            break;
        }

        /* Dual long multiply-accumulate */
        case MUL_SMLALD: {
            int64_t acc = ((int64_t)(int32_t)get_reg(exec, insn->rt) << 32) |
                          get_reg(exec, insn->rd);
            int32_t p1 = (int32_t)get_bottom_half(rn_val) * (int32_t)get_bottom_half(rm_val);
            int32_t p2 = (int32_t)get_top_half(rn_val) * (int32_t)get_top_half(rm_val);
            result64 = (uint64_t)(acc + p1 + p2);
            is_long = true;
            break;
        }

        case MUL_SMLALDX: {
            int64_t acc = ((int64_t)(int32_t)get_reg(exec, insn->rt) << 32) |
                          get_reg(exec, insn->rd);
            int32_t p1 = (int32_t)get_bottom_half(rn_val) * (int32_t)get_top_half(rm_val);
            int32_t p2 = (int32_t)get_top_half(rn_val) * (int32_t)get_bottom_half(rm_val);
            result64 = (uint64_t)(acc + p1 + p2);
            is_long = true;
            break;
        }

        case MUL_SMLSLD: {
            int64_t acc = ((int64_t)(int32_t)get_reg(exec, insn->rt) << 32) |
                          get_reg(exec, insn->rd);
            int32_t p1 = (int32_t)get_bottom_half(rn_val) * (int32_t)get_bottom_half(rm_val);
            int32_t p2 = (int32_t)get_top_half(rn_val) * (int32_t)get_top_half(rm_val);
            result64 = (uint64_t)(acc + p1 - p2);
            is_long = true;
            break;
        }

        case MUL_SMLSLDX: {
            int64_t acc = ((int64_t)(int32_t)get_reg(exec, insn->rt) << 32) |
                          get_reg(exec, insn->rd);
            int32_t p1 = (int32_t)get_bottom_half(rn_val) * (int32_t)get_top_half(rm_val);
            int32_t p2 = (int32_t)get_top_half(rn_val) * (int32_t)get_bottom_half(rm_val);
            result64 = (uint64_t)(acc + p1 - p2);
            is_long = true;
            break;
        }

        /* Most significant word multiply */
        case MUL_SMMUL:
            result = (uint32_t)(((int64_t)(int32_t)rn_val * (int64_t)(int32_t)rm_val) >> 32);
            break;

        case MUL_SMMULR:
            result = (uint32_t)(((int64_t)(int32_t)rn_val * (int64_t)(int32_t)rm_val + 0x80000000LL) >> 32);
            break;

        case MUL_SMMLA:
            result = (uint32_t)((((int64_t)(int32_t)ra_val << 32) +
                                ((int64_t)(int32_t)rn_val * (int64_t)(int32_t)rm_val)) >> 32);
            break;

        case MUL_SMMLAR:
            result = (uint32_t)((((int64_t)(int32_t)ra_val << 32) +
                                ((int64_t)(int32_t)rn_val * (int64_t)(int32_t)rm_val) + 0x80000000LL) >> 32);
            break;

        case MUL_SMMLS:
            result = (uint32_t)((((int64_t)(int32_t)ra_val << 32) -
                                ((int64_t)(int32_t)rn_val * (int64_t)(int32_t)rm_val)) >> 32);
            break;

        case MUL_SMMLSR:
            result = (uint32_t)((((int64_t)(int32_t)ra_val << 32) -
                                ((int64_t)(int32_t)rn_val * (int64_t)(int32_t)rm_val) + 0x80000000LL) >> 32);
            break;

        /* Unsigned sum of absolute differences */
        case MUL_USAD8: {
            uint32_t sum = 0;
            for (int i = 0; i < 4; i++) {
                uint8_t a = (rn_val >> (i * 8)) & 0xFF;
                uint8_t b = (rm_val >> (i * 8)) & 0xFF;
                sum += (a > b) ? (a - b) : (b - a);
            }
            result = sum;
            break;
        }

        case MUL_USADA8: {
            uint32_t sum = 0;
            for (int i = 0; i < 4; i++) {
                uint8_t a = (rn_val >> (i * 8)) & 0xFF;
                uint8_t b = (rm_val >> (i * 8)) & 0xFF;
                sum += (a > b) ? (a - b) : (b - a);
            }
            result = ra_val + sum;
            break;
        }

        /* Long halfword multiply-accumulate */
        case MUL_SMLALBB: {
            int64_t acc = ((int64_t)(int32_t)get_reg(exec, insn->rt) << 32) |
                          get_reg(exec, insn->rd);
            int32_t prod = (int32_t)get_bottom_half(rn_val) * (int32_t)get_bottom_half(rm_val);
            result64 = (uint64_t)(acc + prod);
            is_long = true;
            break;
        }

        case MUL_SMLALBT: {
            int64_t acc = ((int64_t)(int32_t)get_reg(exec, insn->rt) << 32) |
                          get_reg(exec, insn->rd);
            int32_t prod = (int32_t)get_bottom_half(rn_val) * (int32_t)get_top_half(rm_val);
            result64 = (uint64_t)(acc + prod);
            is_long = true;
            break;
        }

        case MUL_SMLALTB: {
            int64_t acc = ((int64_t)(int32_t)get_reg(exec, insn->rt) << 32) |
                          get_reg(exec, insn->rd);
            int32_t prod = (int32_t)get_top_half(rn_val) * (int32_t)get_bottom_half(rm_val);
            result64 = (uint64_t)(acc + prod);
            is_long = true;
            break;
        }

        case MUL_SMLALTT: {
            int64_t acc = ((int64_t)(int32_t)get_reg(exec, insn->rt) << 32) |
                          get_reg(exec, insn->rd);
            int32_t prod = (int32_t)get_top_half(rn_val) * (int32_t)get_top_half(rm_val);
            result64 = (uint64_t)(acc + prod);
            is_long = true;
            break;
        }

        default:
            return ARMV8M_ERR_UNDEFINED_INSN;
    }

    if (is_long) {
        /* 64-bit result: store low in rd, high in rt */
        set_reg(exec, insn->rd, (uint32_t)result64);
        set_reg(exec, insn->rt, (uint32_t)(result64 >> 32));
    } else {
        if (insn->set_flags) {
            armv8m_update_flags(cpu, result, carry, false);
        }
        set_reg(exec, insn->rd, result);
    }

    return ARMV8M_OK;
}

int exec_divide(Executor *exec, const DecodedInsn *insn)
{
    uint32_t rn_val = get_reg(exec, insn->rn);
    uint32_t rm_val = get_reg(exec, insn->rm);
    uint32_t result;

    if (rm_val == 0) {
        /* Division by zero returns 0 (no exception in ARMv8-M baseline) */
        result = 0;
    } else {
        /* Check if signed or unsigned based on operation encoding */
        if (insn->is_signed) {
            /* SDIV */
            int32_t a = (int32_t)rn_val;
            int32_t b = (int32_t)rm_val;
            /* Handle overflow case: INT_MIN / -1 */
            if (a == (int32_t)0x80000000 && b == -1) {
                result = 0x80000000;
            } else {
                result = (uint32_t)(a / b);
            }
        } else {
            /* UDIV */
            result = rn_val / rm_val;
        }
    }

    set_reg(exec, insn->rd, result);
    return ARMV8M_OK;
}

int exec_extend(Executor *exec, const DecodedInsn *insn)
{
    uint32_t rm_val = get_reg(exec, insn->rm);
    uint32_t result;

    /* Check for byte swap operations (from Thumb-16 decoder) */
    /* op: 0=REV, 1=REV16, 2=unused, 3=REVSH */
    if (insn->op <= 3 && insn->access_size == 0) {
        switch (insn->op) {
        case 0: /* REV - byte reverse word */
            result = ((rm_val & 0xFF) << 24) |
                     ((rm_val & 0xFF00) << 8) |
                     ((rm_val & 0xFF0000) >> 8) |
                     ((rm_val & 0xFF000000) >> 24);
            break;
        case 1: /* REV16 - byte reverse packed halfwords */
            result = ((rm_val & 0x00FF) << 8) |
                     ((rm_val & 0xFF00) >> 8) |
                     ((rm_val & 0x00FF0000) << 8) |
                     ((rm_val & 0xFF000000) >> 8);
            break;
        case 3: /* REVSH - byte reverse signed halfword */
            result = (uint32_t)(int32_t)(int16_t)(
                ((rm_val & 0xFF) << 8) | ((rm_val & 0xFF00) >> 8)
            );
            break;
        default:
            result = rm_val;
            break;
        }
        set_reg(exec, insn->rd, result);
        return ARMV8M_OK;
    }

    /* Apply rotation if specified */
    uint8_t rotation = insn->shift_amount;
    if (rotation > 0) {
        rm_val = (rm_val >> rotation) | (rm_val << (32 - rotation));
    }

    switch (insn->access_size) {
        case ACCESS_BYTE:
            if (insn->is_signed) {
                /* SXTB */
                result = (uint32_t)(int32_t)(int8_t)(rm_val & 0xFF);
            } else {
                /* UXTB */
                result = rm_val & 0xFF;
            }
            break;

        case ACCESS_HALF:
            if (insn->is_signed) {
                /* SXTH */
                result = (uint32_t)(int32_t)(int16_t)(rm_val & 0xFFFF);
            } else {
                /* UXTH */
                result = rm_val & 0xFFFF;
            }
            break;

        default:
            result = rm_val;
            break;
    }

    /* If Rn specified, add to result (SXTAB, UXTAB, etc.) */
    if (insn->rn != ARMV8M_REG_NONE && insn->rn != insn->rm) {
        uint32_t rn_val = get_reg(exec, insn->rn);
        result += rn_val;
    }

    set_reg(exec, insn->rd, result);
    return ARMV8M_OK;
}

int exec_bitfield(Executor *exec, const DecodedInsn *insn)
{
    uint32_t rd_val = get_reg(exec, insn->rd);
    uint32_t rn_val = (insn->rn != ARMV8M_REG_NONE && insn->rn != 15)
                      ? get_reg(exec, insn->rn) : 0;

    /* Extract LSB and width from immediate encoding.
     * Decoder packs: imm = (width << 8) | lsb */
    uint8_t lsb = insn->imm & 0xFF;
    uint8_t width = (insn->imm >> 8) & 0xFF;
    uint32_t mask = ((1U << width) - 1);

    uint32_t result;

    switch ((DataProcOp)insn->op) {
        case DP_BIC:
            /* BFC: Bitfield clear - clear bits [lsb+width-1:lsb]
             * This is BFI with Rn=15 (use 0 as source) */
            result = rd_val & ~(mask << lsb);
            break;

        case DP_ORR:
            /* BFI: Bitfield insert - insert rn[width-1:0] into rd[lsb+width-1:lsb] */
            result = (rd_val & ~(mask << lsb)) | ((rn_val & mask) << lsb);
            break;

        case DP_MOV:
        default:
            /* SBFX/UBFX: Bitfield extract */
            if (insn->is_signed) {
                /* SBFX: Signed bitfield extract */
                uint32_t field = (rn_val >> lsb) & mask;
                /* Sign extend */
                if (field & (1U << (width - 1))) {
                    field |= ~mask;
                }
                result = field;
            } else {
                /* UBFX: Unsigned bitfield extract */
                result = (rn_val >> lsb) & mask;
            }
            break;
    }

    set_reg(exec, insn->rd, result);
    return ARMV8M_OK;
}

int exec_saturate(Executor *exec, const DecodedInsn *insn)
{
    CPUState *cpu = &exec->cpu;

    /* Get source value and apply optional pre-shift */
    int32_t value = (int32_t)get_reg(exec, insn->rn);

    if (insn->shift_amount > 0) {
        if (insn->shift_type == SHIFT_LSL) {
            value <<= insn->shift_amount;
        } else if (insn->shift_type == SHIFT_ASR) {
            value >>= insn->shift_amount;
        }
    }

    /* Saturation bit width: imm encodes width-1, so width is imm+1 */
    uint8_t sat_width = (insn->imm & 0x1F) + 1;
    uint32_t result;
    bool saturated = false;

    if (insn->is_signed) {
        /* SSAT: Saturate to signed n-bit range [-2^(n-1), 2^(n-1)-1] */
        int32_t max_val = (1 << (sat_width - 1)) - 1;
        int32_t min_val = -(1 << (sat_width - 1));

        if (value > max_val) {
            result = (uint32_t)max_val;
            saturated = true;
        } else if (value < min_val) {
            result = (uint32_t)min_val;
            saturated = true;
        } else {
            result = (uint32_t)value;
        }
    } else {
        /* USAT: Saturate signed value to unsigned n-bit range [0, 2^n-1] */
        uint32_t max_val = (1U << sat_width) - 1;

        if (value < 0) {
            result = 0;
            saturated = true;
        } else if ((uint32_t)value > max_val) {
            result = max_val;
            saturated = true;
        } else {
            result = (uint32_t)value;
        }
    }

    /* Set Q flag (sticky) if saturation occurred */
    if (saturated) {
        cpu->xpsr |= ARMV8M_XPSR_Q;
    }

    set_reg(exec, insn->rd, result);
    return ARMV8M_OK;
}

/*============================================================================
 * DSP Extension: Saturating Arithmetic
 *============================================================================*/

/**
 * Signed saturating add.
 * Returns saturated result and sets *saturated if saturation occurred.
 */
static int32_t signed_sat_add(int32_t a, int32_t b, bool *saturated)
{
    int64_t result = (int64_t)a + (int64_t)b;

    if (result > INT32_MAX) {
        *saturated = true;
        return INT32_MAX;
    } else if (result < INT32_MIN) {
        *saturated = true;
        return INT32_MIN;
    }
    return (int32_t)result;
}

/**
 * Signed saturating subtract.
 */
static int32_t signed_sat_sub(int32_t a, int32_t b, bool *saturated)
{
    int64_t result = (int64_t)a - (int64_t)b;

    if (result > INT32_MAX) {
        *saturated = true;
        return INT32_MAX;
    } else if (result < INT32_MIN) {
        *saturated = true;
        return INT32_MIN;
    }
    return (int32_t)result;
}

/**
 * Signed saturating double.
 */
static int32_t signed_sat_double(int32_t a, bool *saturated)
{
    int64_t result = (int64_t)a * 2;

    if (result > INT32_MAX) {
        *saturated = true;
        return INT32_MAX;
    } else if (result < INT32_MIN) {
        *saturated = true;
        return INT32_MIN;
    }
    return (int32_t)result;
}

int exec_sat_arith(Executor *exec, const DecodedInsn *insn)
{
    CPUState *cpu = &exec->cpu;
    int32_t rm_val = (int32_t)get_reg(exec, insn->rm);
    int32_t rn_val = (int32_t)get_reg(exec, insn->rn);
    int32_t result;
    bool saturated = false;

    /*
     * op encoding:
     * 0 = QADD:  Rd = SAT(Rm + Rn)
     * 1 = QDADD: Rd = SAT(Rm + SAT(Rn * 2))
     * 2 = QSUB:  Rd = SAT(Rm - Rn)
     * 3 = QDSUB: Rd = SAT(Rm - SAT(Rn * 2))
     */
    switch (insn->op) {
    case 0: /* QADD */
        result = signed_sat_add(rm_val, rn_val, &saturated);
        break;

    case 1: /* QDADD */
        {
            int32_t doubled = signed_sat_double(rn_val, &saturated);
            result = signed_sat_add(rm_val, doubled, &saturated);
        }
        break;

    case 2: /* QSUB */
        result = signed_sat_sub(rm_val, rn_val, &saturated);
        break;

    case 3: /* QDSUB */
        {
            int32_t doubled = signed_sat_double(rn_val, &saturated);
            result = signed_sat_sub(rm_val, doubled, &saturated);
        }
        break;

    default:
        return ARMV8M_ERR_UNDEFINED_INSN;
    }

    /* Set Q flag if saturation occurred */
    if (saturated) {
        cpu->xpsr |= ARMV8M_XPSR_Q;
    }

    set_reg(exec, insn->rd, (uint32_t)result);
    return ARMV8M_OK;
}

/*============================================================================
 * DSP Extension: Parallel Add/Sub Operations
 *============================================================================*/

/**
 * Set GE flags based on parallel operation results.
 * For 16-bit operations, sets GE[1:0] and GE[3:2].
 * For 8-bit operations, sets GE[0], GE[1], GE[2], GE[3] individually.
 */
static void set_ge_flags_16(CPUState *cpu, int res0, int res1)
{
    uint32_t ge = 0;

    /* GE[1:0] based on low halfword result */
    if (res0 >= 0) {
        ge |= 0x3;
    }

    /* GE[3:2] based on high halfword result */
    if (res1 >= 0) {
        ge |= 0xC;
    }

    cpu->xpsr = (cpu->xpsr & ~ARMV8M_XPSR_GE_MASK) | (ge << ARMV8M_XPSR_GE_SHIFT);
}

static void set_ge_flags_8(CPUState *cpu, int res0, int res1, int res2, int res3)
{
    uint32_t ge = 0;

    /* GE flags are set based on the 8-bit truncated result interpreted as signed */
    if ((int8_t)(res0 & 0xFF) >= 0) ge |= 0x1;
    if ((int8_t)(res1 & 0xFF) >= 0) ge |= 0x2;
    if ((int8_t)(res2 & 0xFF) >= 0) ge |= 0x4;
    if ((int8_t)(res3 & 0xFF) >= 0) ge |= 0x8;

    cpu->xpsr = (cpu->xpsr & ~ARMV8M_XPSR_GE_MASK) | (ge << ARMV8M_XPSR_GE_SHIFT);
}

int exec_parallel(Executor *exec, const DecodedInsn *insn)
{
    CPUState *cpu = &exec->cpu;
    uint32_t rn_val = get_reg(exec, insn->rn);
    uint32_t rm_val = get_reg(exec, insn->rm);
    uint32_t result = 0;

    /* Check for SEL instruction (special case) */
    if (insn->op == 0xFF) {
        /* SEL: Select bytes based on GE flags */
        uint32_t ge = (cpu->xpsr >> ARMV8M_XPSR_GE_SHIFT) & 0xF;

        result = 0;
        if (ge & 0x1) result |= (rn_val & 0x000000FF);
        else          result |= (rm_val & 0x000000FF);

        if (ge & 0x2) result |= (rn_val & 0x0000FF00);
        else          result |= (rm_val & 0x0000FF00);

        if (ge & 0x4) result |= (rn_val & 0x00FF0000);
        else          result |= (rm_val & 0x00FF0000);

        if (ge & 0x8) result |= (rn_val & 0xFF000000);
        else          result |= (rm_val & 0xFF000000);

        set_reg(exec, insn->rd, result);
        return ARMV8M_OK;
    }

    /* Decode operation type from insn->op:
     * bits[7:4] = type from hw1[7:4]
     * bits[3:2] = width (0=16-bit, 1=8-bit)
     * bits[1:0] = operation (add, sub, asx, sax)
     */
    uint8_t type = (insn->op >> 4) & 0xF;
    uint8_t width = (insn->op >> 2) & 0x3;
    uint8_t subop = insn->op & 0x3;

    /* Determine operation characteristics:
     * type & 0x1 = 0 for signed, 1 for unsigned (in some encodings)
     * But actual encoding from ARM is more complex
     */
    bool is_unsigned = (type >= 0x7);
    bool is_halving = ((type & 0x2) != 0);
    bool is_saturating = ((type & 0x1) != 0) && !is_halving;

    if (width == 0) {
        /* 16-bit parallel operations */
        int32_t lo_n = (int16_t)(rn_val & 0xFFFF);
        int32_t hi_n = (int16_t)((rn_val >> 16) & 0xFFFF);
        int32_t lo_m = (int16_t)(rm_val & 0xFFFF);
        int32_t hi_m = (int16_t)((rm_val >> 16) & 0xFFFF);

        if (is_unsigned) {
            lo_n = (uint16_t)(rn_val & 0xFFFF);
            hi_n = (uint16_t)((rn_val >> 16) & 0xFFFF);
            lo_m = (uint16_t)(rm_val & 0xFFFF);
            hi_m = (uint16_t)((rm_val >> 16) & 0xFFFF);
        }

        int32_t res_lo, res_hi;

        switch (subop) {
        case 0: /* ADD16 */
            res_lo = lo_n + lo_m;
            res_hi = hi_n + hi_m;
            break;
        case 1: /* ASX (Add/Subtract Exchange) */
            res_lo = lo_n - hi_m;
            res_hi = hi_n + lo_m;
            break;
        case 2: /* SAX (Subtract/Add Exchange) */
            res_lo = lo_n + hi_m;
            res_hi = hi_n - lo_m;
            break;
        case 3: /* SUB16 */
            res_lo = lo_n - lo_m;
            res_hi = hi_n - hi_m;
            break;
        default:
            return ARMV8M_ERR_UNDEFINED_INSN;
        }

        /* Apply halving or saturating */
        if (is_halving) {
            res_lo >>= 1;
            res_hi >>= 1;
        } else if (is_saturating) {
            int32_t min_val = is_unsigned ? 0 : -32768;
            int32_t max_val = is_unsigned ? 65535 : 32767;

            if (res_lo < min_val) res_lo = min_val;
            else if (res_lo > max_val) res_lo = max_val;

            if (res_hi < min_val) res_hi = min_val;
            else if (res_hi > max_val) res_hi = max_val;
        }

        /* Pack result */
        result = ((uint32_t)(res_hi & 0xFFFF) << 16) | (uint32_t)(res_lo & 0xFFFF);

        /* Update GE flags for non-saturating operations */
        if (!is_saturating) {
            set_ge_flags_16(cpu, res_lo, res_hi);
        }

    } else {
        /* 8-bit parallel operations */
        int32_t b0_n, b1_n, b2_n, b3_n;
        int32_t b0_m, b1_m, b2_m, b3_m;

        if (is_unsigned) {
            b0_n = (int32_t)((rn_val >> 0) & 0xFFU);
            b1_n = (int32_t)((rn_val >> 8) & 0xFFU);
            b2_n = (int32_t)((rn_val >> 16) & 0xFFU);
            b3_n = (int32_t)((rn_val >> 24) & 0xFFU);
            b0_m = (int32_t)((rm_val >> 0) & 0xFFU);
            b1_m = (int32_t)((rm_val >> 8) & 0xFFU);
            b2_m = (int32_t)((rm_val >> 16) & 0xFFU);
            b3_m = (int32_t)((rm_val >> 24) & 0xFFU);
        } else {
            b0_n = (int32_t)(int8_t)((rn_val >> 0) & 0xFFU);
            b1_n = (int32_t)(int8_t)((rn_val >> 8) & 0xFFU);
            b2_n = (int32_t)(int8_t)((rn_val >> 16) & 0xFFU);
            b3_n = (int32_t)(int8_t)((rn_val >> 24) & 0xFFU);
            b0_m = (int32_t)(int8_t)((rm_val >> 0) & 0xFFU);
            b1_m = (int32_t)(int8_t)((rm_val >> 8) & 0xFFU);
            b2_m = (int32_t)(int8_t)((rm_val >> 16) & 0xFFU);
            b3_m = (int32_t)(int8_t)((rm_val >> 24) & 0xFFU);
        }

        int32_t res0, res1, res2, res3;

        if (subop == 0) {
            /* ADD8 */
            res0 = b0_n + b0_m;
            res1 = b1_n + b1_m;
            res2 = b2_n + b2_m;
            res3 = b3_n + b3_m;
        } else {
            /* SUB8 */
            res0 = b0_n - b0_m;
            res1 = b1_n - b1_m;
            res2 = b2_n - b2_m;
            res3 = b3_n - b3_m;
        }

        /* Apply halving or saturating */
        if (is_halving) {
            res0 >>= 1;
            res1 >>= 1;
            res2 >>= 1;
            res3 >>= 1;
        } else if (is_saturating) {
            int32_t min_val = is_unsigned ? 0 : -128;
            int32_t max_val = is_unsigned ? 255 : 127;

            if (res0 < min_val) res0 = min_val;
            else if (res0 > max_val) res0 = max_val;

            if (res1 < min_val) res1 = min_val;
            else if (res1 > max_val) res1 = max_val;

            if (res2 < min_val) res2 = min_val;
            else if (res2 > max_val) res2 = max_val;

            if (res3 < min_val) res3 = min_val;
            else if (res3 > max_val) res3 = max_val;
        }

        /* Pack result */
        result = ((uint32_t)(res3 & 0xFF) << 24) |
                 ((uint32_t)(res2 & 0xFF) << 16) |
                 ((uint32_t)(res1 & 0xFF) << 8) |
                 ((uint32_t)(res0 & 0xFF));

        /* Update GE flags for non-saturating operations */
        if (!is_saturating) {
            set_ge_flags_8(cpu, res0, res1, res2, res3);
        }
    }

    set_reg(exec, insn->rd, result);
    return ARMV8M_OK;
}

/*============================================================================
 * DSP Extension: Pack Halfword
 *============================================================================*/

int exec_pack(Executor *exec, const DecodedInsn *insn)
{
    uint32_t rn_val = get_reg(exec, insn->rn);
    uint32_t rm_val = get_reg(exec, insn->rm);
    uint32_t result;

    /* Apply shift to Rm */
    uint32_t shifted_rm = rm_val;
    if (insn->shift_amount > 0) {
        if (insn->shift_type == SHIFT_LSL) {
            shifted_rm = rm_val << insn->shift_amount;
        } else if (insn->shift_type == SHIFT_ASR) {
            shifted_rm = (uint32_t)((int32_t)rm_val >> insn->shift_amount);
        }
    }

    if (insn->op == 0) {
        /* PKHBT: Pack halfword bottom-top
         * Rd[15:0] = Rn[15:0], Rd[31:16] = Rm[31:16] (after shift) */
        result = (rn_val & 0xFFFF) | (shifted_rm & 0xFFFF0000);
    } else {
        /* PKHTB: Pack halfword top-bottom
         * Rd[15:0] = Rm[15:0] (after shift), Rd[31:16] = Rn[31:16] */
        result = (shifted_rm & 0xFFFF) | (rn_val & 0xFFFF0000);
    }

    set_reg(exec, insn->rd, result);
    return ARMV8M_OK;
}
