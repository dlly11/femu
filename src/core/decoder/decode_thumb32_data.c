/**
 * @file decode_thumb32_data.c
 * @brief Data processing instruction decoding (modified imm, plain imm, shifted reg)
 */

#include "decode_thumb32_internal.h"

/*============================================================================
 * Data Processing (Modified Immediate)
 *============================================================================*/

int decode_data_proc_modified_imm(uint16_t hw1, uint16_t hw2, DecodedInsn *out)
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
            /* TEQ - test equivalence (XOR without writing result) */
            out->op = DP_TEQ;
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

int decode_data_proc_plain_imm(uint16_t hw1, uint16_t hw2, DecodedInsn *out)
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

int decode_data_proc_shifted_reg(uint16_t hw1, uint16_t hw2, DecodedInsn *out)
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
            out->op = DP_TEQ;  /* TEQ - test equivalence (XOR) */
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
