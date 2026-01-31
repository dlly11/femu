/**
 * @file decode_thumb32_loadstore.c
 * @brief Load/store instruction decoding (single, dual, multiple)
 */

#include "decode_thumb32_internal.h"

/*============================================================================
 * Load/Store Single
 *============================================================================*/

int decode_load_store_single(uint16_t hw1, uint16_t hw2, DecodedInsn *out)
{
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
        /* PC-relative (literal) - always pre-indexed, no writeback */
        uint32_t imm12 = EXTRACT(hw2, 0, 12);
        out->type = is_load ? INSN_LOAD_LITERAL : INSN_STORE_IMM;
        out->imm = imm12;
        out->add = BIT(hw1, 7) != 0;  /* U bit: 1 = add, 0 = subtract */
        out->index = true;
        out->pre_index = true;
        out->writeback = false;
        out->wback = false;
        return 0;
    }

    /* Check hw1[7] (U bit) to distinguish encoding types:
     * - U=1 (bit7=1): 12-bit unsigned immediate (T1 encoding for signed loads)
     * - U=0 (bit7=0): Register offset or 8-bit immediate modes */
    if (BIT(hw1, 7) != 0) {
        /* 12-bit positive immediate offset - always pre-indexed, no writeback */
        uint32_t imm12 = EXTRACT(hw2, 0, 12);
        out->type = is_load ? INSN_LOAD_IMM : INSN_STORE_IMM;
        out->imm = imm12;
        out->add = true;
        out->index = true;       /* Indexed addressing */
        out->pre_index = true;   /* Offset added before access */
        out->writeback = false;  /* No writeback for this encoding */
        out->wback = false;
        return 0;
    }

    /* U=0: Distinguish between different encoding forms using hw2[11]:
     * - hw2[11]=1: T4 encoding - 8-bit immediate with P/U/W bits
     * - hw2[11]=0: T2 encoding - register offset or unprivileged access */
    if (BIT(hw2, 11) != 0) {
        /* T4 encoding: 8-bit immediate with P/U/W mode bits */
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
        return 0;
    }

    /* T2 encoding (hw2[11]=0): Check hw2[11:6] for register vs unprivileged
     * Register offset: hw2[11:6] = 000000
     * Unprivileged (LDRT, etc.): hw2[11:6] = 1110xx */
    if (EXTRACT(hw2, 6, 6) == 0) {
        /* Register offset: STR/LDR Rt, [Rn, Rm, LSL #imm2] */
        uint8_t rm = (uint8_t)EXTRACT(hw2, 0, 4);
        uint8_t shift = (uint8_t)EXTRACT(hw2, 4, 2);

        out->type = is_load ? INSN_LOAD_REG : INSN_STORE_REG;
        out->rm = rm;
        out->shift_type = SHIFT_LSL;
        out->shift_amount = shift;
        return 0;
    }

    /* Unprivileged access (LDRT, STRT, etc.) - 8-bit immediate, no pre/post */
    {
        uint8_t imm8 = (uint8_t)EXTRACT(hw2, 0, 8);
        out->type = is_load ? INSN_LOAD_IMM : INSN_STORE_IMM;
        out->imm = imm8;
        out->add = true;
        /* Note: Unprivileged access should set a flag, but we treat as normal for now */
    }

    return 0;
}

/*============================================================================
 * Load/Store Dual/Exclusive
 *============================================================================*/

int decode_load_store_dual(uint16_t hw1, uint16_t hw2, DecodedInsn *out)
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

    /* Table branch: TBB/TBH
     * Encoding: hw1 = 0xE8D0 | Rn, hw2 = 0xF000 | (H << 4) | Rm
     * op1 (bits[8:7]) = 01, op2 (bits[5:4]) = 01
     * Key: hw2[15:12] = 0xF distinguishes TBB/TBH from LDREXB/LDREXH */
    if (op1 == 0x1 && op2 == 0x1 && rt == 0xF) {
        uint8_t rm = (uint8_t)EXTRACT(hw2, 0, 4);
        bool is_tbh = BIT(hw2, 4) != 0;

        out->type = INSN_TABLE_BRANCH;
        out->rm = rm;
        out->access_size = is_tbh ? ACCESS_HALF : ACCESS_BYTE;
        return 0;
    }

    /* Load-Acquire / Store-Release (LDA, STL, LDAB, STLB, LDAH, STLH)
     * and exclusive variants (LDAEX, STLEX, LDAEXB, STLEXB, LDAEXH, STLEXH)
     * These are distinguished by:
     *   - hw2[11:8] = 0xF (rt2 field = 15)
     *   - hw2[7:4] >= 0x8 (distinguishes from LDREXB/H at 0x4/0x5)
     * hw2[7:4] encodes the variant:
     *   0x8 = byte, 0x9 = halfword, 0xA = word (non-exclusive)
     *   0xC = exclusive byte, 0xD = exclusive halfword, 0xE = exclusive word
     */
    {
        uint8_t variant = (uint8_t)EXTRACT(hw2, 4, 4);
        if (rt2 == 0xF && variant >= 0x8) {
            bool is_load = BIT(hw1, 4) != 0;
            bool is_exclusive = (variant >= 0xC);

            if (is_load) {
                out->type = INSN_LOAD_ACQUIRE;
            } else {
                out->type = INSN_STORE_RELEASE;
            }

            /* Decode size from variant */
            switch (variant) {
            case 0x8: /* LDAB/STLB */
            case 0xC: /* LDAEXB/STLEXB */
                out->access_size = ACCESS_BYTE;
                break;
            case 0x9: /* LDAH/STLH */
            case 0xD: /* LDAEXH/STLEXH */
                out->access_size = ACCESS_HALF;
                break;
            case 0xA: /* LDA/STL */
            case 0xE: /* LDAEX/STLEX */
            default:
                out->access_size = ACCESS_WORD;
                break;
            }

            /* Use 'op' field to indicate exclusive (1) vs non-exclusive (0) */
            out->op = is_exclusive ? 1 : 0;

            /* For exclusive stores, Rd (status) is in hw2[3:0] */
            if (!is_load && is_exclusive) {
                out->rd = (uint8_t)EXTRACT(hw2, 0, 4);
            }

            out->imm = 0;  /* No offset for LDA/STL */
            out->add = true;
            return 0;
        }
    }

    /* Load/store exclusive (word, byte, halfword)
     * Encoding distinction is based on hw1[7] (U bit in op1):
     * - hw1[7]=0: LDREX/STREX (word) with imm8 offset, Rd at hw2[11:8]
     * - hw1[7]=1: LDREXB/STREXB or LDREXH/STREXH, Rd at hw2[3:0] */
    if ((op1 & 0x2) == 0x0 && (op2 & 0x2) == 0x0) {
        bool is_load = BIT(hw1, 4) != 0;
        uint8_t imm8 = (uint8_t)EXTRACT(hw2, 0, 8);

        if (is_load) {
            out->type = INSN_LOAD_EXCLUSIVE;
        } else {
            out->type = INSN_STORE_EXCLUSIVE;
        }

        /* Check hw1[7] to determine encoding variant:
         * hw1[7]=0: Word exclusive (LDREX/STREX) with scaled imm8 offset
         * hw1[7]=1: Byte/half exclusive, size from hw2[5:4] (00=byte, 01=half) */
        if (BIT(hw1, 7) == 0) {
            /* Word exclusive: LDREX/STREX - Rd at hw2[11:8] */
            out->access_size = ACCESS_WORD;
            out->imm = (uint32_t)imm8 << 2;
            if (!is_load) {
                out->rd = rt2;  /* Status register at bits [11:8] for word */
            }
        } else {
            /* Byte or halfword exclusive - Rd at hw2[3:0] */
            uint8_t size_bits = (uint8_t)EXTRACT(hw2, 4, 2);  /* bits [5:4] */
            if (size_bits == 0x0) {
                out->access_size = ACCESS_BYTE;
            } else {
                out->access_size = ACCESS_HALF;
            }
            out->imm = 0;  /* No offset for byte/half exclusive */
            if (!is_load) {
                out->rd = (uint8_t)EXTRACT(hw2, 0, 4);  /* Status register at bits [3:0] */
            }
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

int decode_load_store_multiple(uint16_t hw1, uint16_t hw2, DecodedInsn *out)
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
