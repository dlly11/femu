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
#define EXTRACT(val, start, len) (((val) >> (start)) & ((1U << (len)) - 1))

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
            /* Check for Data processing (plain binary immediate) first */
            /* bits[9:8] of hw1 = 10 indicates MOVW, MOVT, ADDW, SUBW */
            if ((op2 & 0x30) == 0x20) {
                return decode_data_proc_plain_imm(hw1, hw2, out);
            }
            /* Then check for branches/misc or modified immediate */
            if (BIT(hw1, 9)) {
                /* Branches and miscellaneous control */
                return decode_branch_misc(hw1, hw2, pc, out);
            }
            return decode_data_proc_modified_imm(hw1, hw2, out);
        }
        /* Branches and miscellaneous control */
        return decode_branch_misc(hw1, hw2, pc, out);

    case 0x3:  /* 0b11 */
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
                return decode_data_proc_shifted_reg(hw1, hw2, out);
            }
            if ((op2 & 0x78) == 0x30) {
                /* Multiply, multiply accumulate - op2 = 0110xxx */
                return decode_multiply(hw1, hw2, out);
            }
            if ((op2 & 0x78) == 0x38) {
                /* Long multiply, long multiply accumulate, divide - op2 = 0111xxx */
                return decode_long_multiply(hw1, hw2, out);
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
                 * - 0x4: DSB (Data Synchronization Barrier)
                 * - 0x5: DMB (Data Memory Barrier)
                 * - 0x6: ISB (Instruction Synchronization Barrier)
                 */
                uint8_t hint_op = (uint8_t)(hint >> 4);
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
        uint8_t width = (uint8_t)(EXTRACT(hw2, 0, 5) + 1);

        out->type = INSN_BITFIELD;
        out->rn = rn;
        out->imm = ((uint32_t)width << 8) | lsb;  /* Pack width and lsb */
        out->is_signed = (op == 0x14);
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
    case 0x6: /* PKH (pack halfword) - treat as data proc */
        out->op = DP_ORR;  /* Similar behavior */
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

    /* Load/store exclusive */
    if ((op1 & 0x2) == 0x0 && (op2 & 0x2) == 0x0) {
        bool is_load = BIT(op1, 0) != 0;
        uint8_t imm8 = (uint8_t)EXTRACT(hw2, 0, 8);

        if (is_load) {
            out->type = INSN_LOAD_EXCLUSIVE;
        } else {
            out->type = INSN_STORE_EXCLUSIVE;
            out->rd = (uint8_t)EXTRACT(hw2, 0, 4);  /* Status register */
        }
        out->imm = (uint32_t)imm8 << 2;
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
    uint8_t rn = (uint8_t)EXTRACT(hw1, 0, 4);
    uint8_t rd = (uint8_t)EXTRACT(hw2, 8, 4);
    uint8_t rm = (uint8_t)EXTRACT(hw2, 0, 4);
    uint8_t ra = (uint8_t)EXTRACT(hw2, 12, 4);
    uint8_t op2 = (uint8_t)EXTRACT(hw2, 4, 4);

    out->type = INSN_MULTIPLY;
    out->rd = rd;
    out->rn = rn;
    out->rm = rm;
    out->set_flags = false;

    /*
     * MUL/MLA/MLS distinguished by hw2[7:4] (op2) and Ra:
     * - op2 = 0x0, Ra = 0xF: MUL (Rd = Rn * Rm)
     * - op2 = 0x0, Ra != 0xF: MLA (Rd = Ra + Rn * Rm)
     * - op2 = 0x1: MLS (Rd = Ra - Rn * Rm)
     */
    if (op2 == 0x1) {
        /* MLS - multiply and subtract */
        out->op = DP_MUL;
        out->rs = ra;
        out->add = false;  /* Indicates subtract for MLS */
    } else if (ra == 0xF) {
        /* MUL */
        out->op = DP_MUL;
    } else {
        /* MLA - multiply and accumulate */
        out->op = DP_MUL;
        out->rs = ra;
        out->add = true;  /* Indicates add for MLA */
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
     * Long multiply operations - distinguished by op1:
     * - op1[2] (bit 6 of hw1): 1 = accumulate (SMLAL/UMLAL), 0 = multiply only (SMULL/UMULL)
     * - op1[1] (bit 5 of hw1): 1 = unsigned, 0 = signed
     *
     * op1 = 0b000 (0): SMULL - Signed multiply, 64-bit result
     * op1 = 0b010 (2): UMULL - Unsigned multiply, 64-bit result
     * op1 = 0b100 (4): SMLAL - Signed multiply accumulate
     * op1 = 0b110 (6): UMLAL - Unsigned multiply accumulate
     */
    out->type = INSN_MULTIPLY;
    out->rd = rdlo;
    out->rt = rdhi;  /* Use rt for high register */
    out->is_signed = (op1 & 0x2) == 0;  /* Signed if bit 1 is clear */
    out->add = (op1 & 0x4) != 0;        /* Accumulate if bit 2 is set */

    return 0;
}
