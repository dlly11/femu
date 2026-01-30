/**
 * @file decode_thumb32.c
 * @brief 32-bit Thumb instruction decoding for ARMv8-M
 *
 * Handles all 32-bit Thumb instruction encodings according to ARMv8-M spec.
 * 32-bit instructions are identified by bits[15:11] of first halfword:
 *   11101 - Various (data processing, branches, etc.)
 *   11110 - Branches and miscellaneous control
 *   11111 - Coprocessor, load/store
 */

#include "armv8m_decoder.h"

/*============================================================================
 * Helper Macros
 *============================================================================*/

/* Extract bit field from 32-bit instruction */
#define EXTRACT(val, start, len) (((uint32_t)(val) >> (start)) & ((1U << (len)) - 1))

/* Extract single bit */
#define BIT(val, n) (((val) >> (n)) & 1)

/* Sign extend a value */
static inline int32_t sign_extend(uint32_t val, int bits)
{
    int32_t shift = 32 - bits;
    return ((int32_t)(val << shift)) >> shift;
}

/*============================================================================
 * Forward Declarations
 *============================================================================*/

static int decode_branch_misc(uint16_t hw1, uint16_t hw2, uint32_t pc, DecodedInsn *out);
static int decode_data_proc_modified_imm(uint16_t hw1, uint16_t hw2, DecodedInsn *out);
static int decode_data_proc_plain_imm(uint16_t hw1, uint16_t hw2, DecodedInsn *out);
static int decode_data_proc_shifted_reg(uint16_t hw1, uint16_t hw2, DecodedInsn *out);
static int decode_load_store_single(uint16_t hw1, uint16_t hw2, DecodedInsn *out);
static int decode_load_store_dual(uint16_t hw1, uint16_t hw2, DecodedInsn *out);
static int decode_load_store_multiple(uint16_t hw1, uint16_t hw2, DecodedInsn *out);
static int decode_multiply(uint16_t hw1, uint16_t hw2, DecodedInsn *out);
static int decode_long_multiply(uint16_t hw1, uint16_t hw2, DecodedInsn *out);
static int decode_parallel_add_sub(uint16_t hw1, uint16_t hw2, DecodedInsn *out);
static int decode_misc_ops(uint16_t hw1, uint16_t hw2, DecodedInsn *out);
static int decode_vfp(uint16_t hw1, uint16_t hw2, DecodedInsn *out);

/*============================================================================
 * Thumb32 Immediate Expansion
 *============================================================================*/

/**
 * Expand a Thumb-2 modified immediate constant (ThumbExpandImm).
 *
 * The 12-bit value encodes various patterns based on bits [11:10]:
 * - If bits[11:10] == 00: Special byte-replicated patterns
 *   - bits[9:8] == 00: Zero-extend imm8
 *   - bits[9:8] == 01: 00000000_imm8_00000000_imm8
 *   - bits[9:8] == 10: imm8_00000000_imm8_00000000
 *   - bits[9:8] == 11: imm8_imm8_imm8_imm8
 * - Otherwise: ROR('1':imm12[6:0], imm12[11:7])
 */
static uint32_t thumb_expand_imm(uint32_t imm12)
{
    uint32_t imm8 = imm12 & 0xFF;

    /* Check bits [11:10] for special patterns */
    if ((imm12 & 0xC00) == 0) {
        /* bits[11:10] == 00: byte-replicated patterns */
        uint32_t op = (imm12 >> 8) & 0x3;  /* bits[9:8] */
        switch (op) {
        case 0: return imm8;
        case 1: return (imm8 << 16) | imm8;
        case 2: return (imm8 << 24) | (imm8 << 8);
        case 3: return (imm8 << 24) | (imm8 << 16) | (imm8 << 8) | imm8;
        default: return imm8;  /* Unreachable, but satisfies compiler */
        }
    }

    /* Rotated 8-bit constant: ROR('1':imm12[6:0], imm12[11:7]) */
    uint32_t val = 0x80 | (imm12 & 0x7F);  /* '1':imm12[6:0] */
    uint32_t rot = (imm12 >> 7) & 0x1F;    /* imm12[11:7] - rotation amount */
    return (val >> rot) | (val << (32 - rot));
}

/*============================================================================
 * Main Entry Point
 *============================================================================*/

/**
 * Decode a 32-bit Thumb instruction.
 *
 * @param hw1   First halfword (at PC)
 * @param hw2   Second halfword (at PC+2)
 * @param pc    Current program counter
 * @param out   Output decoded instruction structure
 * @return      0 on success, negative error code on failure
 */
int decode_thumb32(uint16_t hw1, uint16_t hw2, uint32_t pc, DecodedInsn *out)
{
    uint32_t op1 = EXTRACT(hw1, 11, 2);  /* bits [12:11] of hw1 */
    uint32_t op2 = EXTRACT(hw1, 4, 7);   /* bits [10:4] of hw1 */
    uint32_t op = BIT(hw2, 15);          /* bit 15 of hw2 */

    /* Check for SG (Secure Gateway) - exact encoding 0xE97FE97F */
    if (hw1 == 0xE97F && hw2 == 0xE97F) {
        out->type = INSN_SG;
        return 0;
    }

    /* Check for TT/TTT/TTA/TTAT (Test Target) - hw1[15:4] = 0xE84 */
    if ((hw1 & 0xFFF0) == 0xE840) {
        out->type = INSN_TT;
        out->rn = (uint8_t)(hw1 & 0xF);
        out->rd = (uint8_t)EXTRACT(hw2, 8, 4);
        /* bits[7:6] of hw2 encode variant: 00=TT, 01=TTT, 10=TTA, 11=TTAT */
        out->op = (uint8_t)EXTRACT(hw2, 6, 2);
        return 0;
    }

    /* Check for coprocessor/VFP instructions (hw1[15:9] patterns)
     * VFP encoding: hw1[15:12] = 1110, hw1[11:9] = 11x
     * Coprocessor: hw1[15:12] = 1110 or 1111, with specific patterns
     * Note: hw1[15:9] = 0x78-0x7F are branches (BL, B.W), NOT VFP! */
    uint32_t hw1_top = (hw1 >> 9) & 0x7F;  /* bits [15:9] */
    if (hw1_top == 0x76 || hw1_top == 0x77) {
        /* VFP instructions: hw1[15:9] = 1110_11x (0x76 or 0x77) */
        return decode_vfp(hw1, hw2, out);
    }
    /* Also check for op1=0b11 coprocessor dispatch later in the switch */

    /* Dispatch based on op1 (bits [12:11] of first halfword) */
    switch (op1) {
    case 0x1:  /* 0b01 */
        /* Load/store multiple, load/store dual, table branch */
        if ((op2 & 0x64) == 0x00) {
            /* Load/store multiple */
            return decode_load_store_multiple(hw1, hw2, out);
        } else if ((op2 & 0x64) == 0x04) {
            /* Load/store dual, exclusive, table branch */
            return decode_load_store_dual(hw1, hw2, out);
        } else if ((op2 & 0x60) == 0x20) {
            /* Data processing (shifted register) */
            return decode_data_proc_shifted_reg(hw1, hw2, out);
        }
        break;

    case 0x2:  /* 0b10 */
        if (op == 0) {
            /* Dispatch based on bit 9 of hw1:
             * bit 9 = 0: Data-processing (modified immediate) - AND, ORR, MOV, etc.
             * bit 9 = 1: Data-processing (plain binary immediate) - ADDW, MOVW, BFI, SBFX, etc.
             */
            if (BIT(hw1, 9)) {
                return decode_data_proc_plain_imm(hw1, hw2, out);
            }
            return decode_data_proc_modified_imm(hw1, hw2, out);
        }
        /* op == 1: Branches and miscellaneous control */
        return decode_branch_misc(hw1, hw2, pc, out);

    case 0x3:  /* 0b11 */
        /* Check multiply instructions first - they don't depend on op (bit 15 of hw2) */
        if ((op2 & 0x78) == 0x30) {
            /* Multiply, multiply accumulate - op2 = 0110xxx */
            return decode_multiply(hw1, hw2, out);
        }
        if ((op2 & 0x78) == 0x38) {
            /* Long multiply, long multiply accumulate, divide - op2 = 0111xxx */
            return decode_long_multiply(hw1, hw2, out);
        }

        if (op == 0) {
            /* Load/store single (multiple patterns) */
            if ((op2 & 0x71) == 0x00 || (op2 & 0x67) == 0x01 ||
                (op2 & 0x67) == 0x03 || (op2 & 0x71) == 0x20) {
                return decode_load_store_single(hw1, hw2, out);
            }
            if ((op2 & 0x71) == 0x10) {
                /* Data processing (plain binary immediate) */
                return decode_data_proc_plain_imm(hw1, hw2, out);
            }
            if ((op2 & 0x40) == 0x40) {
                /* Branches and miscellaneous */
                return decode_branch_misc(hw1, hw2, pc, out);
            }
        } else {
            /* Load/store single (multiple patterns) */
            if ((op2 & 0x70) == 0x00 || (op2 & 0x70) == 0x40 ||
                (op2 & 0x70) == 0x50) {
                return decode_load_store_single(hw1, hw2, out);
            }
            if ((op2 & 0x70) == 0x20) {
                /* Data processing (register) - op2 = 010xxxx */
                /* Check for parallel add/sub or misc operations */
                uint32_t op1_reg = EXTRACT(hw1, 4, 4);
                if (op1_reg >= 0x4 && op1_reg <= 0x7) {
                    /* Parallel add/sub: op1 = 01xx */
                    return decode_parallel_add_sub(hw1, hw2, out);
                }
                if (op1_reg == 0x8 || op1_reg == 0x9 || op1_reg == 0xA || op1_reg == 0xB) {
                    /* Misc operations (QADD, QSUB, CLZ, etc.) */
                    return decode_misc_ops(hw1, hw2, out);
                }
                return decode_data_proc_shifted_reg(hw1, hw2, out);
            }
        }
        break;

    default:
        break;
    }

    return ARMV8M_ERR_UNDEFINED_INSN;
}

/*============================================================================
 * Branch and Miscellaneous Control
 *============================================================================*/

static int decode_branch_misc(uint16_t hw1, uint16_t hw2, uint32_t pc, DecodedInsn *out)
{
    uint32_t op1 = EXTRACT(hw1, 4, 7);
    uint32_t op2 = EXTRACT(hw2, 12, 3);

    (void)pc;

    /*
     * op2 = bits[14:12] of hw2:
     * - Conditional B<cond>.W: hw2[15:14] = 10, hw2[12] = 0 -> op2 = 0xx (0-3)
     * - B.W unconditional:     hw2[15:14] = 10, hw2[12] = 1 -> op2 = 01x (2-3)
     * - BL:                    hw2[15:14] = 11, hw2[12] = 1 -> op2 = 11x (6-7)
     * - BLX:                   hw2[15:14] = 11, hw2[12] = 0 -> op2 = 10x (4-5)
     */

    /* Check for conditional branch B<cond>.W */
    /* hw2[15:14] = 10, hw2[12] = 0 -> op2 & 0x5 = 0 */
    if ((op2 & 0x5) == 0x0 && (op1 & 0x38) != 0x38) {
        /* Conditional branch B<cond>.W */
        uint32_t cond = EXTRACT(hw1, 6, 4);
        uint32_t S = BIT(hw1, 10);
        uint32_t imm6 = EXTRACT(hw1, 0, 6);
        uint32_t J1 = BIT(hw2, 13);
        uint32_t J2 = BIT(hw2, 11);
        uint32_t imm11 = EXTRACT(hw2, 0, 11);

        /* Offset: S:J2:J1:imm6:imm11:0 (21-bit signed, shifted left 1) */
        uint32_t offset_u = (S << 20) | (J2 << 19) | (J1 << 18) |
                            (imm6 << 12) | (imm11 << 1);
        int32_t offset = sign_extend(offset_u, 21);

        out->type = INSN_BRANCH;
        out->cond = (ConditionCode)cond;
        out->branch_offset = offset;
        return 0;
    }

    /* Check for B.W unconditional (op2 = 01x) or BL (op2 = 11x) */
    /* Both have hw2[12] = 1, so (op2 & 0x1) == 0x1 */
    if ((op2 & 0x1) == 0x1) {
        /* B.W (unconditional) or BL - distinguished by hw2[14] */
        uint32_t S = BIT(hw1, 10);
        uint32_t imm10 = EXTRACT(hw1, 0, 10);
        uint32_t J1 = BIT(hw2, 13);
        uint32_t J2 = BIT(hw2, 11);
        uint32_t imm11 = EXTRACT(hw2, 0, 11);
        uint32_t link = BIT(hw2, 14);  /* BL has bit 14 set */

        /* I1 = NOT(J1 XOR S), I2 = NOT(J2 XOR S) */
        uint32_t I1 = !(J1 ^ S);
        uint32_t I2 = !(J2 ^ S);

        /* Offset: S:I1:I2:imm10:imm11:0 (25-bit signed) */
        uint32_t offset_u = (S << 24) | (I1 << 23) | (I2 << 22) |
                            (imm10 << 12) | (imm11 << 1);
        int32_t offset = sign_extend(offset_u, 25);

        if (link) {
            out->type = INSN_BRANCH_LINK;
            out->link = true;
        } else {
            out->type = INSN_BRANCH;
            out->cond = COND_AL;
        }
        out->branch_offset = offset;
        return 0;
    }

    /* Check for BLX (op2 = 10x) -> hw2[14] = 1, hw2[12] = 0 */
    if ((op2 & 0x5) == 0x4) {
        /* BLX */
        uint32_t S = BIT(hw1, 10);
        uint32_t imm10H = EXTRACT(hw1, 0, 10);
        uint32_t J1 = BIT(hw2, 13);
        uint32_t J2 = BIT(hw2, 11);
        uint32_t imm10L = EXTRACT(hw2, 1, 10);

        uint32_t I1 = !(J1 ^ S);
        uint32_t I2 = !(J2 ^ S);

        uint32_t offset_u = (S << 24) | (I1 << 23) | (I2 << 22) |
                            (imm10H << 12) | (imm10L << 2);
        int32_t offset = sign_extend(offset_u, 25);

        out->type = INSN_BRANCH_LINK_EXCHANGE;
        out->link = true;
        out->branch_offset = offset;
        return 0;
    }

    /* Miscellaneous control instructions */
    if ((op1 & 0x38) == 0x38 && (op2 & 0x5) == 0x0) {
        /* The opcode is in hw1[7:4], not hw2[7:4] */
        uint8_t opc = (uint8_t)EXTRACT(hw1, 4, 4);

        switch (opc) {
        case 0x8: /* MSR (R=0) */
        case 0x9: /* MSR (R=1) */
        {
            uint8_t rn = (uint8_t)EXTRACT(hw1, 0, 4);
            uint8_t sysm = (uint8_t)EXTRACT(hw2, 0, 8);

            out->type = INSN_MSR;
            out->rn = rn;
            out->sysreg = sysm;
            return 0;
        }

        case 0xA: /* Change processor state (CPS) and hints */
        case 0xB:
        {
            uint8_t hint = (uint8_t)EXTRACT(hw2, 0, 8);
            if (EXTRACT(hw1, 0, 4) == 0xF) {
                /*
                 * Hints and barriers are distinguished by hint[7:4]:
                 * - 0x0: NOP, YIELD, WFE, WFI, SEV (based on hint[3:0])
                 * - 0x2: CLREX (Clear Exclusive)
                 * - 0x4: DSB (Data Synchronization Barrier)
                 * - 0x5: DMB (Data Memory Barrier)
                 * - 0x6: ISB (Instruction Synchronization Barrier)
                 */
                uint8_t hint_op = (uint8_t)(hint >> 4);
                if (hint_op == 0x2) {
                    /* CLREX - Clear Exclusive Monitor */
                    out->type = INSN_CLEAR_EXCLUSIVE;
                    return 0;
                }
                if (hint_op >= 0x4 && hint_op <= 0x6) {
                    /* Barrier instruction */
                    out->type = INSN_BARRIER;
                    out->op = hint_op;  /* 4=DSB, 5=DMB, 6=ISB */
                    out->imm = hint & 0xF;  /* Barrier option (SY, ST, etc.) */
                    return 0;
                }
                /* Hint instruction */
                out->type = INSN_HINT;
                out->op = hint_op;  /* NOP=0, YIELD=1, WFE=2, WFI=3, SEV=4 */
                out->imm = hint;
                return 0;
            }
            break;
        }

        case 0x4: /* BXNS - Branch and exchange to Non-secure */
        {
            uint8_t rm = (uint8_t)EXTRACT(hw1, 0, 4);
            out->type = INSN_BXNS;
            out->rm = rm;
            return 0;
        }

        case 0x5: /* BLXNS - Branch with link and exchange to Non-secure */
        {
            uint8_t rm = (uint8_t)EXTRACT(hw1, 0, 4);
            out->type = INSN_BLXNS;
            out->rm = rm;
            return 0;
        }

        case 0xC: /* BXJ (deprecated) - treat as BX */
        {
            uint8_t rm = (uint8_t)EXTRACT(hw1, 0, 4);
            out->type = INSN_BRANCH_EXCHANGE;
            out->rm = rm;
            return 0;
        }

        case 0xD: /* Exception return (ERET) */
            out->type = INSN_BRANCH_EXCHANGE;
            out->rm = ARMV8M_REG_LR;
            return 0;

        case 0xE: /* MRS */
        case 0xF:
        {
            uint8_t rd = (uint8_t)EXTRACT(hw2, 8, 4);
            uint8_t sysm = (uint8_t)EXTRACT(hw2, 0, 8);

            out->type = INSN_MRS;
            out->rd = rd;
            out->sysreg = sysm;
            return 0;
        }

        default:
            break;
        }
    }

    return ARMV8M_ERR_UNDEFINED_INSN;
}

/*============================================================================
 * Data Processing (Modified Immediate)
 *============================================================================*/

static int decode_data_proc_modified_imm(uint16_t hw1, uint16_t hw2, DecodedInsn *out)
{
    uint32_t op = EXTRACT(hw1, 5, 4);
    uint8_t rn = (uint8_t)EXTRACT(hw1, 0, 4);
    uint8_t rd = (uint8_t)EXTRACT(hw2, 8, 4);
    uint32_t S = BIT(hw1, 4);

    /* Build 12-bit immediate: i:imm3:imm8 */
    uint32_t i = BIT(hw1, 10);
    uint32_t imm3 = EXTRACT(hw2, 12, 3);
    uint32_t imm8 = EXTRACT(hw2, 0, 8);
    uint32_t imm12 = (i << 11) | (imm3 << 8) | imm8;

    out->type = INSN_DATA_PROC_IMM;
    out->rd = rd;
    out->rn = rn;
    out->imm = thumb_expand_imm(imm12);
    out->set_flags = (S != 0);

    switch (op) {
    case 0x0:
        if (rd == 0xF && S) {
            out->op = DP_TST;
            out->rd = ARMV8M_REG_NONE;
        } else {
            out->op = DP_AND;
        }
        break;
    case 0x1:
        out->op = DP_BIC;
        break;
    case 0x2:
        if (rn == 0xF) {
            out->op = DP_MOV;
            out->rn = ARMV8M_REG_NONE;
        } else {
            out->op = DP_ORR;
        }
        break;
    case 0x3:
        if (rn == 0xF) {
            out->op = DP_MVN;
            out->rn = ARMV8M_REG_NONE;
        } else {
            out->op = DP_ORN;
        }
        break;
    case 0x4:
        if (rd == 0xF && S) {
            out->op = DP_TST;  /* Actually TEQ - test equivalence */
            out->rd = ARMV8M_REG_NONE;
        } else {
            out->op = DP_EOR;
        }
        break;
    case 0x8:
        if (rd == 0xF && S) {
            out->op = DP_CMN;
            out->rd = ARMV8M_REG_NONE;
        } else {
            out->op = DP_ADD;
        }
        break;
    case 0xA:
        out->op = DP_ADC;
        break;
    case 0xB:
        out->op = DP_SBC;
        break;
    case 0xD:
        if (rd == 0xF && S) {
            out->op = DP_CMP;
            out->rd = ARMV8M_REG_NONE;
        } else {
            out->op = DP_SUB;
        }
        break;
    case 0xE:
        out->op = DP_RSB;
        break;
    default:
        return ARMV8M_ERR_UNDEFINED_INSN;
    }

    return 0;
}

/*============================================================================
 * Data Processing (Plain Binary Immediate)
 *============================================================================*/

static int decode_data_proc_plain_imm(uint16_t hw1, uint16_t hw2, DecodedInsn *out)
{
    uint32_t op = EXTRACT(hw1, 4, 5);
    uint8_t rn = (uint8_t)EXTRACT(hw1, 0, 4);
    uint8_t rd = (uint8_t)EXTRACT(hw2, 8, 4);

    uint32_t i = BIT(hw1, 10);
    uint32_t imm3 = EXTRACT(hw2, 12, 3);
    uint32_t imm8 = EXTRACT(hw2, 0, 8);
    uint32_t imm12 = (i << 11) | (imm3 << 8) | imm8;

    out->rd = rd;
    out->set_flags = false;

    switch (op) {
    case 0x00: /* ADD.W with 12-bit immediate */
        if (rn == 0xF) {
            /* ADR (add PC) */
            out->type = INSN_DATA_PROC_IMM;
            out->op = DP_ADD;
            out->rn = ARMV8M_REG_PC;
            out->imm = imm12;
        } else {
            out->type = INSN_DATA_PROC_IMM;
            out->op = DP_ADD;
            out->rn = rn;
            out->imm = imm12;
        }
        break;

    case 0x04: /* MOV.W with 16-bit immediate (MOVW) */
    {
        uint32_t imm4 = EXTRACT(hw1, 0, 4);
        uint32_t imm16 = (imm4 << 12) | imm12;
        out->type = INSN_DATA_PROC_IMM;
        out->op = DP_MOV;
        out->imm = imm16;
        out->rn = ARMV8M_REG_NONE;
        break;
    }

    case 0x0A: /* SUB.W with 12-bit immediate */
        if (rn == 0xF) {
            /* ADR (sub PC) */
            out->type = INSN_DATA_PROC_IMM;
            out->op = DP_SUB;
            out->rn = ARMV8M_REG_PC;
            out->imm = imm12;
        } else {
            out->type = INSN_DATA_PROC_IMM;
            out->op = DP_SUB;
            out->rn = rn;
            out->imm = imm12;
        }
        break;

    case 0x0C: /* MOVT (move top halfword) */
    {
        uint32_t imm4 = EXTRACT(hw1, 0, 4);
        uint32_t imm16 = (imm4 << 12) | imm12;
        out->type = INSN_DATA_PROC_IMM;
        out->op = DP_MOV;
        out->imm = imm16 << 16;  /* Top halfword */
        out->rn = rd;  /* Read existing value */
        break;
    }

    case 0x10: /* SSAT */
    case 0x12: /* SSAT with shift */
    case 0x18: /* USAT */
    case 0x1A: /* USAT with shift */
    {
        uint8_t sat = (uint8_t)EXTRACT(hw2, 0, 5);
        uint8_t sh = (uint8_t)BIT(hw1, 5);
        uint8_t imm2 = (uint8_t)EXTRACT(hw2, 6, 2);
        uint8_t imm3_val = (uint8_t)EXTRACT(hw2, 12, 3);

        out->type = INSN_SATURATE;
        out->rn = rn;
        out->imm = sat;
        out->shift_type = sh ? SHIFT_ASR : SHIFT_LSL;
        out->shift_amount = (uint8_t)((imm3_val << 2) | imm2);
        out->is_signed = (op & 0x08) == 0;
        break;
    }

    case 0x14: /* SBFX */
    case 0x16: /* BFI/BFC */
    case 0x1C: /* UBFX */
    {
        uint8_t lsb = (uint8_t)(((EXTRACT(hw2, 12, 3) << 2) | EXTRACT(hw2, 6, 2)));
        uint8_t width;

        /* Encoding differs:
         * - BFI/BFC (op=0x16): bits 4-0 = msb, width = msb - lsb + 1
         * - SBFX/UBFX: bits 4-0 = widthm1, width = widthm1 + 1
         */
        if (op == 0x16) {
            uint8_t msb = (uint8_t)EXTRACT(hw2, 0, 5);
            width = (uint8_t)(msb - lsb + 1);
        } else {
            width = (uint8_t)(EXTRACT(hw2, 0, 5) + 1);
        }

        out->type = INSN_BITFIELD;
        out->rn = rn;
        out->imm = ((uint32_t)width << 8) | lsb;  /* Pack width and lsb */
        out->is_signed = (op == 0x14);

        /* Set op to distinguish BFI/BFC from extract operations.
         * BFI/BFC (op=0x16): DP_ORR for insert, DP_BIC for clear (rn=15)
         * SBFX/UBFX: DP_MOV (extract to destination) */
        if (op == 0x16) {
            out->op = (rn == 15) ? DP_BIC : DP_ORR;
        } else {
            out->op = DP_MOV;  /* Extract operations */
        }
        break;
    }

    default:
        return ARMV8M_ERR_UNDEFINED_INSN;
    }

    return 0;
}

/*============================================================================
 * Data Processing (Shifted Register)
 *============================================================================*/

static int decode_data_proc_shifted_reg(uint16_t hw1, uint16_t hw2, DecodedInsn *out)
{
    uint32_t op = EXTRACT(hw1, 5, 4);
    uint8_t rn = (uint8_t)EXTRACT(hw1, 0, 4);
    uint8_t rd = (uint8_t)EXTRACT(hw2, 8, 4);
    uint8_t rm = (uint8_t)EXTRACT(hw2, 0, 4);
    uint32_t S = BIT(hw1, 4);

    uint32_t type = EXTRACT(hw2, 4, 2);
    uint32_t imm2 = EXTRACT(hw2, 6, 2);
    uint32_t imm3 = EXTRACT(hw2, 12, 3);
    uint32_t shift_n = (imm3 << 2) | imm2;

    out->type = INSN_DATA_PROC_SHIFTED;
    out->rd = rd;
    out->rn = rn;
    out->rm = rm;
    out->set_flags = (S != 0);
    out->shift_type = (ShiftType)type;
    out->shift_amount = (uint8_t)shift_n;

    /* Handle RRX: type=11 with shift_n=0 */
    if (type == 3 && shift_n == 0) {
        out->shift_type = SHIFT_RRX;
        out->shift_amount = 1;
    }

    switch (op) {
    case 0x0:
        if (rd == 0xF && S) {
            out->op = DP_TST;
            out->rd = ARMV8M_REG_NONE;
        } else {
            out->op = DP_AND;
        }
        break;
    case 0x1:
        out->op = DP_BIC;
        break;
    case 0x2:
        if (rn == 0xF) {
            /* MOV/shift */
            out->op = DP_MOV;
            out->rn = ARMV8M_REG_NONE;
        } else {
            out->op = DP_ORR;
        }
        break;
    case 0x3:
        if (rn == 0xF) {
            out->op = DP_MVN;
            out->rn = ARMV8M_REG_NONE;
        } else {
            out->op = DP_ORN;
        }
        break;
    case 0x4:
        if (rd == 0xF && S) {
            out->op = DP_TST;  /* TEQ */
            out->rd = ARMV8M_REG_NONE;
        } else {
            out->op = DP_EOR;
        }
        break;
    case 0x6: /* PKH (pack halfword) */
        out->type = INSN_PACK;
        /* PKHBT: shift_type = LSL (0), PKHTB: shift_type = ASR (2) */
        /* The shift type in encoding determines PKHBT vs PKHTB */
        out->op = (type == 2) ? 1 : 0;  /* 0=PKHBT, 1=PKHTB */
        return 0;
    case 0x8:
        if (rd == 0xF && S) {
            out->op = DP_CMN;
            out->rd = ARMV8M_REG_NONE;
        } else {
            out->op = DP_ADD;
        }
        break;
    case 0xA:
        out->op = DP_ADC;
        break;
    case 0xB:
        out->op = DP_SBC;
        break;
    case 0xD:
        if (rd == 0xF && S) {
            out->op = DP_CMP;
            out->rd = ARMV8M_REG_NONE;
        } else {
            out->op = DP_SUB;
        }
        break;
    case 0xE:
        out->op = DP_RSB;
        break;
    default:
        return ARMV8M_ERR_UNDEFINED_INSN;
    }

    return 0;
}

/*============================================================================
 * Load/Store Single
 *============================================================================*/

static int decode_load_store_single(uint16_t hw1, uint16_t hw2, DecodedInsn *out)
{
    uint32_t op2 = EXTRACT(hw2, 6, 6);
    uint8_t rn = (uint8_t)EXTRACT(hw1, 0, 4);
    uint8_t rt = (uint8_t)EXTRACT(hw2, 12, 4);

    out->rt = rt;
    out->rn = rn;
    out->add = true;
    out->index = true;

    /* Determine size and load/store */
    bool is_load = BIT(hw1, 4) != 0;
    uint8_t size = (uint8_t)EXTRACT(hw1, 5, 2);

    switch (size) {
    case 0: out->access_size = ACCESS_BYTE; break;
    case 1: out->access_size = ACCESS_HALF; break;
    case 2: out->access_size = ACCESS_WORD; break;
    default: return ARMV8M_ERR_UNDEFINED_INSN;
    }

    /* Check for sign extension */
    out->is_signed = BIT(hw1, 8) != 0;

    if (rn == 0xF) {
        /* PC-relative (literal) */
        uint32_t imm12 = EXTRACT(hw2, 0, 12);
        out->type = is_load ? INSN_LOAD_LITERAL : INSN_STORE_IMM;
        out->imm = imm12;
        out->add = BIT(hw1, 7) == 0;  /* U bit */
        return 0;
    }

    if (op2 == 0) {
        /* Register offset */
        uint8_t rm = (uint8_t)EXTRACT(hw2, 0, 4);
        uint8_t shift = (uint8_t)EXTRACT(hw2, 4, 2);

        out->type = is_load ? INSN_LOAD_REG : INSN_STORE_REG;
        out->rm = rm;
        out->shift_type = SHIFT_LSL;
        out->shift_amount = shift;
        return 0;
    }

    /* Immediate offset variants */
    if ((op2 & 0x24) == 0x24) {
        /* Immediate 12-bit unsigned (positive offset) */
        uint32_t imm12 = EXTRACT(hw2, 0, 12);
        out->type = is_load ? INSN_LOAD_IMM : INSN_STORE_IMM;
        out->imm = imm12;
        out->add = true;
        return 0;
    }

    /* 8-bit immediate with various modes */
    {
        uint8_t imm8 = (uint8_t)EXTRACT(hw2, 0, 8);
        uint8_t P = (uint8_t)BIT(hw2, 10);
        uint8_t U = (uint8_t)BIT(hw2, 9);
        uint8_t W = (uint8_t)BIT(hw2, 8);

        out->type = is_load ? INSN_LOAD_IMM : INSN_STORE_IMM;
        out->imm = imm8;
        out->add = (U != 0);
        out->index = (P != 0);
        out->writeback = (W != 0);
        out->wback = (W != 0);
        out->pre_index = (P != 0);
    }

    return 0;
}

/*============================================================================
 * Load/Store Dual/Exclusive
 *============================================================================*/

static int decode_load_store_dual(uint16_t hw1, uint16_t hw2, DecodedInsn *out)
{
    uint32_t op1 = EXTRACT(hw1, 7, 2);
    uint32_t op2 = EXTRACT(hw1, 4, 2);
    uint8_t rn = (uint8_t)EXTRACT(hw1, 0, 4);
    uint8_t rt = (uint8_t)EXTRACT(hw2, 12, 4);
    uint8_t rt2 = (uint8_t)EXTRACT(hw2, 8, 4);

    out->rt = rt;
    out->rt2 = rt2;
    out->rn = rn;
    out->access_size = ACCESS_WORD;

    /* Table branch */
    if (op1 == 0x0 && op2 == 0x1) {
        uint8_t rm = (uint8_t)EXTRACT(hw2, 0, 4);
        bool is_tbh = BIT(hw2, 4) != 0;

        out->type = INSN_TABLE_BRANCH;
        out->rm = rm;
        out->access_size = is_tbh ? ACCESS_HALF : ACCESS_BYTE;
        return 0;
    }

    /* Load/store exclusive (word, byte, halfword)
     * LDREX/STREX:   op1[0]=L (load), op2[1:0] pattern varies
     * LDREXB/STREXB: hw2[5:4]=00, hw2[7:6]=01 (byte)
     * LDREXH/STREXH: hw2[5:4]=01, hw2[7:6]=01 (halfword)
     * LDREX/STREX:   hw2[5:4]=1x (word) or standard pattern */
    if ((op1 & 0x2) == 0x0 && (op2 & 0x2) == 0x0) {
        bool is_load = BIT(op1, 0) != 0;
        uint8_t imm8 = (uint8_t)EXTRACT(hw2, 0, 8);
        uint8_t hw2_bits = (uint8_t)EXTRACT(hw2, 4, 4);  /* bits [7:4] */

        if (is_load) {
            out->type = INSN_LOAD_EXCLUSIVE;
        } else {
            out->type = INSN_STORE_EXCLUSIVE;
            out->rd = (uint8_t)EXTRACT(hw2, 0, 4);  /* Status register */
        }

        /* Determine access size from hw2[5:4]:
         * 00 = byte (LDREXB/STREXB), 01 = halfword (LDREXH/STREXH),
         * 1x = word (LDREX/STREX) */
        uint8_t size_bits = (hw2_bits >> 0) & 0x3;  /* bits [5:4] from hw2 */
        if (size_bits == 0x0) {
            out->access_size = ACCESS_BYTE;
            out->imm = 0;  /* No offset for byte/half exclusive */
        } else if (size_bits == 0x1) {
            out->access_size = ACCESS_HALF;
            out->imm = 0;
        } else {
            out->access_size = ACCESS_WORD;
            out->imm = (uint32_t)imm8 << 2;
        }
        out->add = true;
        return 0;
    }

    /* LDRD/STRD - load/store dual */
    {
        bool is_load = BIT(hw1, 4) != 0;
        uint8_t imm8 = (uint8_t)EXTRACT(hw2, 0, 8);
        bool P = BIT(hw1, 8) != 0;
        bool U = BIT(hw1, 7) != 0;
        bool W = BIT(hw1, 5) != 0;

        if (is_load) {
            out->type = INSN_LOAD_IMM;
        } else {
            out->type = INSN_STORE_IMM;
        }
        out->imm = (uint32_t)imm8 << 2;
        out->add = U;
        out->index = P;
        out->writeback = W;
        out->wback = W;
        out->pre_index = P;

        /* Mark as dual access */
        out->access_size = ACCESS_WORD;  /* Each register is a word */
    }

    return 0;
}

/*============================================================================
 * Load/Store Multiple
 *============================================================================*/

static int decode_load_store_multiple(uint16_t hw1, uint16_t hw2, DecodedInsn *out)
{
    uint32_t op = EXTRACT(hw1, 7, 2);
    uint32_t L = BIT(hw1, 4);
    uint32_t W = BIT(hw1, 5);
    uint8_t rn = (uint8_t)EXTRACT(hw1, 0, 4);
    uint16_t reg_list = hw2 & 0xFFFF;

    /* Check for empty register list */
    if ((reg_list & 0xDFFF) == 0) {
        return ARMV8M_ERR_UNPREDICTABLE;
    }

    out->rn = rn;
    out->register_list = reg_list;
    out->writeback = (W != 0);
    out->wback = (W != 0);

    if (L) {
        out->type = INSN_LOAD_MULTIPLE;
    } else {
        out->type = INSN_STORE_MULTIPLE;
    }

    /* Determine direction based on op */
    switch (op) {
    case 0x0: /* STMDB / LDMDB (decrement before) */
        out->add = false;
        out->pre_index = true;
        break;
    case 0x1: /* STM / LDM (increment after) */
        out->add = true;
        out->pre_index = false;
        break;
    case 0x2: /* STMDB / LDMDB (alias) */
        out->add = false;
        out->pre_index = true;
        break;
    case 0x3: /* STM / LDM (alias) */
        out->add = true;
        out->pre_index = false;
        break;
    default:
        break;
    }

    /* Check unpredictable conditions */
    if (W && (reg_list & (1U << rn))) {
        /* Writeback with Rn in list */
        if (!L) {
            return ARMV8M_ERR_UNPREDICTABLE;
        }
    }

    return 0;
}

/*============================================================================
 * Multiply Instructions
 *============================================================================*/

static int decode_multiply(uint16_t hw1, uint16_t hw2, DecodedInsn *out)
{
    uint8_t op1 = (uint8_t)EXTRACT(hw1, 4, 3);  /* bits [6:4] - multiply type */
    uint8_t rn = (uint8_t)EXTRACT(hw1, 0, 4);
    uint8_t rd = (uint8_t)EXTRACT(hw2, 8, 4);
    uint8_t rm = (uint8_t)EXTRACT(hw2, 0, 4);
    uint8_t ra = (uint8_t)EXTRACT(hw2, 12, 4);
    uint8_t op2 = (uint8_t)EXTRACT(hw2, 4, 2);  /* bits [5:4] - sub-operation */

    out->type = INSN_MULTIPLY;
    out->rd = rd;
    out->rn = rn;
    out->rm = rm;
    out->set_flags = false;

    switch (op1) {
        case 0:
            /*
             * MUL/MLA/MLS
             * - op2 = 0, Ra = 0xF: MUL (Rd = Rn * Rm)
             * - op2 = 0, Ra != 0xF: MLA (Rd = Ra + Rn * Rm)
             * - op2 = 1: MLS (Rd = Ra - Rn * Rm)
             */
            if (op2 == 0x1) {
                out->op = MUL_MLS;
                out->rs = ra;
            } else if (ra == 0xF) {
                out->op = MUL_MUL;
            } else {
                out->op = MUL_MLA;
                out->rs = ra;
            }
            break;

        case 1:
            /*
             * Signed halfword multiply: SMLA<x><y> / SMUL<x><y>
             * op2[1:0] selects which halfwords: BB(0), BT(1), TB(2), TT(3)
             * Ra = 0xF: SMUL<x><y> (multiply only)
             * Ra != 0xF: SMLA<x><y> (multiply-accumulate)
             */
            if (ra == 0xF) {
                /* SMUL<x><y> */
                out->op = (uint8_t)(MUL_SMULBB + op2);
            } else {
                /* SMLA<x><y> */
                out->op = (uint8_t)(MUL_SMLABB + op2);
                out->rs = ra;
            }
            break;

        case 2:
            /*
             * Dual multiply: SMUAD/SMUSD/SMLAD/SMLSD
             * op2[0]: 0 = SMUAD/SMLAD (add), 1 = SMUSD/SMLSD (subtract)
             * op2[1]: X variant (exchange)
             * Ra = 0xF: SMUAD/SMUSD (multiply only)
             * Ra != 0xF: SMLAD/SMLSD (accumulate)
             */
            if (ra == 0xF) {
                /* SMUAD/SMUADX/SMUSD/SMUSDX */
                if (op2 & 1) {
                    out->op = (op2 & 2) ? MUL_SMUSDX : MUL_SMUSD;
                } else {
                    out->op = (op2 & 2) ? MUL_SMUADX : MUL_SMUAD;
                }
            } else {
                /* SMLAD/SMLADX/SMLSD/SMLSDX */
                if (op2 & 1) {
                    out->op = (op2 & 2) ? MUL_SMLSDX : MUL_SMLSD;
                } else {
                    out->op = (op2 & 2) ? MUL_SMLADX : MUL_SMLAD;
                }
                out->rs = ra;
            }
            break;

        case 3:
            /*
             * Signed halfword x word multiply: SMLAWx/SMULWx
             * op2[0]: 0 = bottom halfword (B), 1 = top halfword (T)
             * Ra = 0xF: SMULW<x>
             * Ra != 0xF: SMLAW<x>
             */
            if (ra == 0xF) {
                out->op = (op2 & 1) ? MUL_SMULWT : MUL_SMULWB;
            } else {
                out->op = (op2 & 1) ? MUL_SMLAWT : MUL_SMLAWB;
                out->rs = ra;
            }
            break;

        case 4:
            /*
             * SMLSD (accumulate, long form) - similar encoding to op1=2
             * Actually this maps to same as op1=2 in some reference docs
             */
            if (ra == 0xF) {
                out->op = (op2 & 2) ? MUL_SMUSDX : MUL_SMUSD;
            } else {
                out->op = (op2 & 2) ? MUL_SMLSDX : MUL_SMLSD;
                out->rs = ra;
            }
            break;

        case 5:
            /*
             * Most significant word multiply: SMMUL/SMMLA
             * op2[0]: R variant (round)
             * Ra = 0xF: SMMUL/SMMULR
             * Ra != 0xF: SMMLA/SMMLAR
             */
            if (ra == 0xF) {
                out->op = (op2 & 1) ? MUL_SMMULR : MUL_SMMUL;
            } else {
                out->op = (op2 & 1) ? MUL_SMMLAR : MUL_SMMLA;
                out->rs = ra;
            }
            break;

        case 6:
            /*
             * Most significant word multiply-subtract: SMMLS/SMMLSR
             * op2[0]: R variant (round)
             */
            out->op = (op2 & 1) ? MUL_SMMLSR : MUL_SMMLS;
            out->rs = ra;
            break;

        case 7:
            /*
             * Unsigned sum of absolute differences: USAD8/USADA8
             * Ra = 0xF: USAD8
             * Ra != 0xF: USADA8
             */
            if (ra == 0xF) {
                out->op = MUL_USAD8;
            } else {
                out->op = MUL_USADA8;
                out->rs = ra;
            }
            break;

        default:
            return ARMV8M_ERR_UNDEFINED_INSN;
    }

    return 0;
}

/*============================================================================
 * Long Multiply and Divide
 *============================================================================*/

static int decode_long_multiply(uint16_t hw1, uint16_t hw2, DecodedInsn *out)
{
    uint32_t op1 = EXTRACT(hw1, 4, 3);
    uint32_t op2 = EXTRACT(hw2, 4, 4);
    uint8_t rn = (uint8_t)EXTRACT(hw1, 0, 4);
    uint8_t rdlo = (uint8_t)EXTRACT(hw2, 12, 4);
    uint8_t rdhi = (uint8_t)EXTRACT(hw2, 8, 4);
    uint8_t rm = (uint8_t)EXTRACT(hw2, 0, 4);

    out->rn = rn;
    out->rm = rm;
    out->set_flags = false;

    if (op1 == 0x1 && op2 == 0xF) {
        /* SDIV: 1111 1011 1001 nnnn || 1111 dddd 1111 mmmm
         * Rd is at hw2[11:8] (rdhi), not hw2[15:12] (which is always 0xF) */
        out->type = INSN_DIVIDE;
        out->rd = rdhi;
        out->is_signed = true;
        return 0;
    }

    if (op1 == 0x3 && op2 == 0xF) {
        /* UDIV: 1111 1011 1011 nnnn || 1111 dddd 1111 mmmm
         * Rd is at hw2[11:8] (rdhi), not hw2[15:12] (which is always 0xF) */
        out->type = INSN_DIVIDE;
        out->rd = rdhi;
        out->is_signed = false;
        return 0;
    }

    /*
     * Long multiply operations - distinguished by op1 and op2:
     *
     * Basic long multiply (op2 = 0000):
     * - op1 = 0b000 (0): SMULL - Signed multiply, 64-bit result
     * - op1 = 0b010 (2): UMULL - Unsigned multiply, 64-bit result
     * - op1 = 0b100 (4): SMLAL - Signed multiply accumulate
     * - op1 = 0b110 (6): UMLAL - Unsigned multiply accumulate
     *
     * DSP halfword long multiply (op2 = 10xx, op1 = 100):
     * - op2 = 1000: SMLALBB
     * - op2 = 1001: SMLALBT
     * - op2 = 1010: SMLALTB
     * - op2 = 1011: SMLALTT
     *
     * DSP dual long multiply (op2 = 110x):
     * - op1 = 100, op2 = 1100: SMLALD
     * - op1 = 100, op2 = 1101: SMLALDX
     * - op1 = 101, op2 = 1100: SMLSLD
     * - op1 = 101, op2 = 1101: SMLSLDX
     */
    out->type = INSN_MULTIPLY;
    out->rd = rdlo;
    out->rt = rdhi;  /* Use rt for high register */

    /* Check for DSP variants */
    if ((op2 & 0x8) != 0) {
        /* DSP long multiply variants */
        if ((op2 & 0xC) == 0x8) {
            /* SMLAL<x><y>: op2 = 10xx */
            uint8_t xy = (uint8_t)(op2 & 0x3);
            out->op = (uint8_t)(MUL_SMLALBB + xy);
        } else if ((op2 & 0xC) == 0xC) {
            /* SMLALD/SMLSLD: op2 = 110x */
            bool is_x = (op2 & 1) != 0;
            if (op1 == 4) {
                out->op = is_x ? MUL_SMLALDX : MUL_SMLALD;
            } else if (op1 == 5) {
                out->op = is_x ? MUL_SMLSLDX : MUL_SMLSLD;
            } else {
                return ARMV8M_ERR_UNDEFINED_INSN;
            }
        } else {
            return ARMV8M_ERR_UNDEFINED_INSN;
        }
    } else {
        /* Basic long multiply */
        switch (op1) {
            case 0:
                out->op = MUL_SMULL;
                break;
            case 2:
                out->op = MUL_UMULL;
                break;
            case 4:
                out->op = MUL_SMLAL;
                break;
            case 6:
                out->op = MUL_UMLAL;
                break;
            default:
                return ARMV8M_ERR_UNDEFINED_INSN;
        }
    }

    return 0;
}

/*============================================================================
 * Parallel Add/Sub (DSP Extension)
 *============================================================================*/

/**
 * Parallel add/sub operation codes.
 * Stored in insn->op to distinguish operations.
 */
#define PARALLEL_SADD8      0x00
#define PARALLEL_SSUB8      0x04
#define PARALLEL_SADD16     0x01
#define PARALLEL_SSUB16     0x05
#define PARALLEL_SASX       0x02
#define PARALLEL_SSAX       0x06
#define PARALLEL_SHADD8     0x20
#define PARALLEL_SHSUB8     0x24
#define PARALLEL_SHADD16    0x21
#define PARALLEL_SHSUB16    0x25
#define PARALLEL_SHASX      0x22
#define PARALLEL_SHSAX      0x26
#define PARALLEL_UADD8      0x40
#define PARALLEL_USUB8      0x44
#define PARALLEL_UADD16     0x41
#define PARALLEL_USUB16     0x45
#define PARALLEL_UASX       0x42
#define PARALLEL_USAX       0x46
#define PARALLEL_UHADD8     0x60
#define PARALLEL_UHSUB8     0x64
#define PARALLEL_UHADD16    0x61
#define PARALLEL_UHSUB16    0x65
#define PARALLEL_UHASX      0x62
#define PARALLEL_UHSAX      0x66
#define PARALLEL_QADD8      0x10
#define PARALLEL_QSUB8      0x14
#define PARALLEL_QADD16     0x11
#define PARALLEL_QSUB16     0x15
#define PARALLEL_QASX       0x12
#define PARALLEL_QSAX       0x16
#define PARALLEL_UQADD8     0x50
#define PARALLEL_UQSUB8     0x54
#define PARALLEL_UQADD16    0x51
#define PARALLEL_UQSUB16    0x55
#define PARALLEL_UQASX      0x52
#define PARALLEL_UQSAX      0x56

static int decode_parallel_add_sub(uint16_t hw1, uint16_t hw2, DecodedInsn *out)
{
    uint8_t op2 = (uint8_t)EXTRACT(hw2, 4, 2);  /* bits [5:4] of hw2 - sub-operation */
    uint8_t rn = (uint8_t)EXTRACT(hw1, 0, 4);
    uint8_t rd = (uint8_t)EXTRACT(hw2, 8, 4);
    uint8_t rm = (uint8_t)EXTRACT(hw2, 0, 4);

    out->type = INSN_PARALLEL;
    out->rd = rd;
    out->rn = rn;
    out->rm = rm;
    out->set_flags = false;

    /*
     * Encoding based on hw1 bits [7:4]:
     * 0100 = signed regular (SADD16, SASX, SSAX, SSUB16, SADD8, SSUB8)
     * 0101 = signed saturating (QADD16, QASX, QSAX, QSUB16, QADD8, QSUB8)
     * 0110 = signed halving (SHADD16, SHASX, SHSAX, SHSUB16, SHADD8, SHSUB8)
     * 0111 = unsigned regular (UADD16, etc.)
     * 1000-1011 are similar patterns
     *
     * hw2[7:6] = 0 for 16-bit ops, 1 for 8-bit ops
     * hw2[5:4] = sub-operation (00=add, 01=asx, 10=sax, 11=sub for 16-bit)
     */
    uint8_t type_bits = (uint8_t)EXTRACT(hw1, 4, 4);
    uint8_t width = (uint8_t)EXTRACT(hw2, 6, 2);  /* 0=16-bit, 1=8-bit variants */

    /* Combine into single op code for executor:
     * bits[7:4] = type (signed/unsigned, regular/halving/saturating)
     * bits[3:2] = width (16-bit or 8-bit)
     * bits[1:0] = sub-operation (add, sub, asx, sax)
     */
    out->op = (uint8_t)((type_bits << 4) | (width << 2) | op2);

    return 0;
}

/*============================================================================
 * Miscellaneous Operations (QADD, QSUB, CLZ, REV, etc.)
 *============================================================================*/

/* Saturating arithmetic operation codes */
#define SAT_QADD    0
#define SAT_QSUB    1
#define SAT_QDADD   2
#define SAT_QDSUB   3

static int decode_misc_ops(uint16_t hw1, uint16_t hw2, DecodedInsn *out)
{
    uint8_t op1 = (uint8_t)EXTRACT(hw1, 4, 2);  /* bits [5:4] */
    uint8_t op2 = (uint8_t)EXTRACT(hw2, 4, 2);  /* bits [5:4] of hw2 */
    uint8_t rn = (uint8_t)EXTRACT(hw1, 0, 4);
    uint8_t rd = (uint8_t)EXTRACT(hw2, 8, 4);
    uint8_t rm = (uint8_t)EXTRACT(hw2, 0, 4);

    /*
     * op1 = bits[5:4] of hw1, op2 = bits[5:4] of hw2
     *
     * op1=00:
     *   op2=00: QADD Rd, Rm, Rn
     *   op2=01: QDADD Rd, Rm, Rn
     *   op2=10: QSUB Rd, Rm, Rn
     *   op2=11: QDSUB Rd, Rm, Rn
     *
     * op1=01:
     *   op2=00: REV Rd, Rm
     *   op2=01: REV16 Rd, Rm
     *   op2=10: RBIT Rd, Rm
     *   op2=11: REVSH Rd, Rm
     *
     * op1=10:
     *   op2=00: SEL Rd, Rn, Rm (select bytes based on GE flags)
     *
     * op1=11:
     *   op2=00: CLZ Rd, Rm
     */

    switch (op1) {
    case 0x0:
        /* Saturating arithmetic: QADD, QDADD, QSUB, QDSUB */
        out->type = INSN_SAT_ARITH;
        out->rd = rd;
        out->rn = rn;  /* Second operand */
        out->rm = rm;  /* First operand (Rm comes first in QADD Rd, Rm, Rn) */
        out->op = op2; /* 0=QADD, 1=QDADD, 2=QSUB, 3=QDSUB */
        return 0;

    case 0x1:
        /* REV, REV16, RBIT, REVSH - use data proc with special encoding */
        out->type = INSN_DATA_PROC_REG;
        out->rd = rd;
        out->rm = rm;
        out->rn = ARMV8M_REG_NONE;
        out->set_flags = false;

        /* Use DP_ROR as base and encode variant in shift_amount */
        switch (op2) {
        case 0x0: out->op = DP_ROR; out->shift_amount = 0x10; break;  /* REV */
        case 0x1: out->op = DP_ROR; out->shift_amount = 0x11; break;  /* REV16 */
        case 0x2: out->op = DP_ROR; out->shift_amount = 0x12; break;  /* RBIT */
        case 0x3: out->op = DP_ROR; out->shift_amount = 0x13; break;  /* REVSH */
        default: return ARMV8M_ERR_UNDEFINED_INSN;
        }
        return 0;

    case 0x2:
        /* SEL - select bytes based on GE flags */
        if (op2 == 0x0) {
            out->type = INSN_PARALLEL;  /* Reuse parallel type for SEL */
            out->rd = rd;
            out->rn = rn;
            out->rm = rm;
            out->op = 0xFF;  /* Special code for SEL */
            return 0;
        }
        return ARMV8M_ERR_UNDEFINED_INSN;

    case 0x3:
        /* CLZ - count leading zeros */
        if (op2 == 0x0) {
            out->type = INSN_DATA_PROC_REG;
            out->rd = rd;
            out->rm = rm;
            out->rn = ARMV8M_REG_NONE;
            out->set_flags = false;
            out->op = DP_MVN;  /* Reuse MVN encoding */
            out->shift_amount = 0x20;  /* Special marker for CLZ */
            return 0;
        }
        return ARMV8M_ERR_UNDEFINED_INSN;

    default:
        return ARMV8M_ERR_UNDEFINED_INSN;
    }
}

/*============================================================================
 * VFP/Floating Point Instructions
 *============================================================================*/

/* VFP operation codes for INSN_FPU_ARITH */
#define VFP_VMLA    0x00
#define VFP_VMLS    0x01
#define VFP_VNMLA   0x02
#define VFP_VNMLS   0x03
#define VFP_VNMUL   0x04
#define VFP_VMUL    0x05
#define VFP_VADD    0x06
#define VFP_VSUB    0x07
#define VFP_VDIV    0x08
#define VFP_VABS    0x10
#define VFP_VNEG    0x11
#define VFP_VSQRT   0x12

/* VFP move operations */
#define VFP_VMOV_IMM    0x00
#define VFP_VMOV_REG    0x01
#define VFP_VMOV_ARM    0x02  /* Between ARM and FPU reg */
#define VFP_VMOV_2ARM   0x03  /* Between two ARM regs and FPU */

/* VFP conversion operations */
#define VFP_VCVT_F32_F64    0x00
#define VFP_VCVT_F64_F32    0x01
#define VFP_VCVT_S32_F      0x02
#define VFP_VCVT_U32_F      0x03
#define VFP_VCVT_F_S32      0x04
#define VFP_VCVT_F_U32      0x05

/**
 * Decode VFP S/D register from encoding bits.
 * For single-precision: Sd = D:Vd, Sn = N:Vn, Sm = M:Vm
 * For double-precision: Dd = Vd:D, Dn = Vn:N, Dm = Vm:M
 */
static void decode_vfp_regs(uint16_t hw1, uint16_t hw2, DecodedInsn *out, bool is_double)
{
    uint8_t D = (uint8_t)BIT(hw1, 6);
    uint8_t Vd = (uint8_t)EXTRACT(hw2, 12, 4);
    uint8_t N = (uint8_t)BIT(hw1, 7);
    uint8_t Vn = (uint8_t)EXTRACT(hw1, 0, 4);
    uint8_t M = (uint8_t)BIT(hw2, 5);
    uint8_t Vm = (uint8_t)EXTRACT(hw2, 0, 4);

    out->is_double = is_double;

    if (is_double) {
        out->dd = (uint8_t)((Vd << 1) | D);
        out->dn = (uint8_t)((Vn << 1) | N);
        out->dm = (uint8_t)((Vm << 1) | M);
        /* Also set sd/sn/sm for compatibility */
        out->sd = (uint8_t)(out->dd * 2);
        out->sn = (uint8_t)(out->dn * 2);
        out->sm = (uint8_t)(out->dm * 2);
    } else {
        out->sd = (uint8_t)((Vd << 1) | D);
        out->sn = (uint8_t)((Vn << 1) | N);
        out->sm = (uint8_t)((Vm << 1) | M);
        /* Also set dd/dn/dm */
        out->dd = (uint8_t)(out->sd / 2);
        out->dn = (uint8_t)(out->sn / 2);
        out->dm = (uint8_t)(out->sm / 2);
    }
}

static int decode_vfp(uint16_t hw1, uint16_t hw2, DecodedInsn *out)
{
    uint32_t opc1 = EXTRACT(hw1, 4, 4);   /* bits [7:4] of hw1 */
    uint32_t opc2 = EXTRACT(hw2, 4, 4);   /* bits [7:4] of hw2 */
    uint32_t cp = EXTRACT(hw2, 8, 4);     /* Coprocessor number */

    /* Check for VFP coprocessors (CP10 or CP11) */
    if (cp != 10 && cp != 11) {
        /* Not a VFP instruction - generic coprocessor */
        out->type = (BIT(hw1, 4)) ? INSN_MRC : INSN_MCR;
        out->rd = (uint8_t)EXTRACT(hw2, 12, 4);
        out->imm = ((uint32_t)opc1 << 4) | opc2;
        return 0;
    }

    bool is_double = (cp == 11);

    /* Categorize by hw1[11:9] and hw1[4] */
    uint32_t category = EXTRACT(hw1, 9, 3);
    uint32_t L = BIT(hw1, 4);

    switch (category) {
    case 0x6:  /* 110 - VLDR/VSTR, VLDM/VSTM */
    case 0x7:  /* 111 - same */
    {
        uint32_t P = BIT(hw1, 8);
        uint32_t U = BIT(hw1, 7);
        uint32_t W = BIT(hw1, 5);
        uint8_t Vd = (uint8_t)EXTRACT(hw2, 12, 4);
        uint8_t D = (uint8_t)BIT(hw1, 6);
        uint8_t Rn = (uint8_t)EXTRACT(hw1, 0, 4);
        uint8_t imm8 = (uint8_t)EXTRACT(hw2, 0, 8);

        if (P && !W) {
            /* VLDR/VSTR - single load/store */
            out->type = L ? INSN_FPU_LOAD : INSN_FPU_STORE;
            out->rn = Rn;
            out->imm = (uint32_t)imm8 << 2;  /* Offset is imm8 * 4 */
            out->add = (U != 0);
            out->is_double = is_double;

            if (is_double) {
                out->dd = (uint8_t)((Vd << 1) | D);
                out->sd = (uint8_t)(out->dd * 2);
            } else {
                out->sd = (uint8_t)((Vd << 1) | D);
                out->dd = (uint8_t)(out->sd / 2);
            }
            return 0;
        } else {
            /* VLDM/VSTM or VPUSH/VPOP */
            out->type = INSN_FPU_MULTI;
            out->rn = Rn;
            out->add = (U != 0);
            out->writeback = (W != 0);
            out->pre_index = (P != 0);
            out->is_double = is_double;

            /* Register count is in imm8 */
            out->imm = imm8;

            if (is_double) {
                out->dd = (uint8_t)((Vd << 1) | D);
                out->sd = (uint8_t)(out->dd * 2);
            } else {
                out->sd = (uint8_t)((Vd << 1) | D);
                out->dd = (uint8_t)(out->sd / 2);
            }

            /* Check for VPUSH/VPOP encoding */
            if (Rn == 13 && W) {
                /* VPUSH: P=1, U=0, W=1; VPOP: P=0, U=1, W=1 */
                out->op = (P && !U) ? 0 : 1;  /* 0=VPUSH, 1=VPOP */
            }
            return 0;
        }
    }

    case 0x4:  /* 100 - VFP data processing, 64-bit transfers */
    case 0x5:  /* 101 - same */
    {
        /* Check for 64-bit transfer (VMOV between ARM and FPU) */
        if (opc1 == 0x4 || opc1 == 0x5) {
            /* VMOV (two ARM regs and FPU) */
            out->type = INSN_FPU_MOVE;
            out->op = VFP_VMOV_2ARM;
            out->rd = (uint8_t)EXTRACT(hw2, 12, 4);  /* Rt */
            out->rt = (uint8_t)EXTRACT(hw1, 0, 4);   /* Rt2 */
            out->is_double = is_double;

            uint8_t Vm = (uint8_t)EXTRACT(hw2, 0, 4);
            uint8_t M = (uint8_t)BIT(hw2, 5);
            if (is_double) {
                out->dm = (uint8_t)((Vm << 1) | M);
                out->sm = (uint8_t)(out->dm * 2);
            } else {
                out->sm = (uint8_t)((Vm << 1) | M);
                out->dm = (uint8_t)(out->sm / 2);
            }
            out->add = (opc1 == 0x5);  /* Direction: 0=to FPU, 1=from FPU */
            return 0;
        }

        /* VMOV (single ARM reg and FPU) */
        if (opc1 == 0x0 || opc1 == 0x7) {
            out->type = INSN_FPU_MOVE;
            out->op = VFP_VMOV_ARM;
            out->rd = (uint8_t)EXTRACT(hw2, 12, 4);  /* Rt */

            uint8_t Vn = (uint8_t)EXTRACT(hw1, 0, 4);
            uint8_t N = (uint8_t)BIT(hw1, 7);
            out->sn = (uint8_t)((Vn << 1) | N);
            out->dn = (uint8_t)(out->sn / 2);

            out->add = (opc1 == 0x7);  /* Direction: 0=to FPU, 1=from FPU */
            out->imm = EXTRACT(hw2, 5, 2);  /* opc2 for scalar access */
            return 0;
        }

        /* VMRS/VMSR (transfer FPSCR) */
        if (opc1 == 0xF || opc1 == 0xE) {
            out->type = INSN_FPU_MOVE;
            out->op = VFP_VMOV_ARM;
            out->rd = (uint8_t)EXTRACT(hw2, 12, 4);
            out->add = (opc1 == 0xF);  /* VMRS (to ARM) */
            out->sysreg = 0x80;  /* Special marker for FPSCR */
            return 0;
        }

        /* Fall through to data processing */
    }
    /* Fall through */

    default:
        break;
    }

    /* VFP data processing instructions */
    uint32_t opc1_dp = EXTRACT(hw1, 4, 4);
    uint32_t opc2_dp = EXTRACT(hw2, 4, 3);
    uint32_t opc3 = EXTRACT(hw2, 6, 2);

    decode_vfp_regs(hw1, hw2, out, is_double);

    /* Check for compare (VCMP/VCMPE) */
    if (opc1_dp == 0xB && (opc2_dp & 0x6) == 0x4) {
        out->type = INSN_FPU_CMP;
        out->op = BIT(hw2, 4);  /* E bit for VCMPE vs VCMP */
        /* Check for compare with zero */
        if (BIT(hw2, 0) == 0 && BIT(hw2, 5) == 1) {
            out->rm = ARMV8M_REG_NONE;  /* Compare with zero */
        }
        return 0;
    }

    /* Check for conversion (VCVT) */
    if (opc1_dp == 0xB && (opc2_dp & 0x6) == 0x2) {
        out->type = INSN_FPU_CVT;
        out->op = (uint8_t)(opc3 | (BIT(hw2, 4) << 2));
        return 0;
    }

    /* Check for VMOV (immediate) */
    if (opc1_dp == 0xB && opc2_dp == 0x0 && (opc3 & 0x1) == 0) {
        out->type = INSN_FPU_MOVE;
        out->op = VFP_VMOV_IMM;
        /* Immediate is encoded in hw1[3:0]:hw2[3:0] */
        out->imm = ((uint32_t)EXTRACT(hw1, 0, 4) << 4) | EXTRACT(hw2, 0, 4);
        return 0;
    }

    /* Check for VMOV (register) */
    if (opc1_dp == 0xB && opc2_dp == 0x1 && opc3 == 0x1) {
        out->type = INSN_FPU_MOVE;
        out->op = VFP_VMOV_REG;
        return 0;
    }

    /* Check for VABS, VNEG, VSQRT */
    if (opc1_dp == 0xB && opc2_dp == 0x0 && (opc3 & 0x1) == 0x1) {
        out->type = INSN_FPU_ARITH;
        out->op = VFP_VABS;
        return 0;
    }
    if (opc1_dp == 0xB && opc2_dp == 0x1 && opc3 == 0x1) {
        out->type = INSN_FPU_ARITH;
        out->op = VFP_VNEG;
        return 0;
    }
    if (opc1_dp == 0xB && opc2_dp == 0x1 && opc3 == 0x3) {
        out->type = INSN_FPU_ARITH;
        out->op = VFP_VSQRT;
        return 0;
    }

    /* Standard arithmetic operations */
    out->type = INSN_FPU_ARITH;

    switch (opc1_dp & 0xB) {
    case 0x0:  /* VMLA, VMLS */
        out->op = (opc3 & 0x1) ? VFP_VMLS : VFP_VMLA;
        break;
    case 0x1:  /* VNMLA, VNMLS, VNMUL */
        if (opc3 & 0x1) {
            out->op = VFP_VNMUL;
        } else {
            out->op = (opc3 & 0x2) ? VFP_VNMLA : VFP_VNMLS;
        }
        break;
    case 0x2:  /* VMUL */
        out->op = VFP_VMUL;
        break;
    case 0x3:  /* VADD, VSUB */
        out->op = (opc3 & 0x1) ? VFP_VSUB : VFP_VADD;
        break;
    case 0x8:  /* VDIV */
        out->op = VFP_VDIV;
        break;
    default:
        return ARMV8M_ERR_UNDEFINED_INSN;
    }

    return 0;
}
