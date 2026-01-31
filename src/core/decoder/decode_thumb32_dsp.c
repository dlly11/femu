/**
 * @file decode_thumb32_dsp.c
 * @brief DSP parallel add/sub and miscellaneous operations decoding
 */

#include "decode_thumb32_internal.h"

/*============================================================================
 * Parallel Add/Sub (DSP Extension)
 *============================================================================*/

int decode_parallel_add_sub(uint16_t hw1, uint16_t hw2, DecodedInsn *out)
{
    uint8_t rn = (uint8_t)EXTRACT(hw1, 0, 4);
    uint8_t rd = (uint8_t)EXTRACT(hw2, 8, 4);
    uint8_t rm = (uint8_t)EXTRACT(hw2, 0, 4);

    out->type = INSN_PARALLEL;
    out->rd = rd;
    out->rn = rn;
    out->rm = rm;
    out->set_flags = false;

    /*
     * Parallel add/sub encoding:
     * hw1 = 0b1111_1010_1ooo_nnnn (ooo = op1, determines operation)
     * hw2 = 0b1111_dddd_0TTT_mmmm (TTT determines signed/unsigned/halving/saturating)
     *
     * op1 = hw1[6:4] determines the operation:
     *   000 = ADD8  (8-bit add)
     *   001 = ADD16 (16-bit add)
     *   010 = ASX   (add-subtract exchange)
     *   100 = SUB8  (8-bit subtract)
     *   101 = SUB16 (16-bit subtract)
     *   110 = SAX   (subtract-add exchange)
     *
     * hw2[6:4] (TTT) determines the type:
     *   000 = S (signed regular)
     *   001 = Q (signed saturating)
     *   010 = SH (signed halving)
     *   100 = U (unsigned regular)
     *   101 = UQ (unsigned saturating)
     *   110 = UH (unsigned halving)
     */
    uint8_t op1 = (uint8_t)EXTRACT(hw1, 4, 3);  /* hw1[6:4] */
    uint8_t type_ttt = (uint8_t)EXTRACT(hw2, 4, 3);  /* TTT field from hw2[6:4] */

    /* Map op1 to is_16bit and subop for executor:
     * subop: 0=ADD, 1=ASX, 2=SUB, 3=SAX */
    uint8_t is_16bit;
    uint8_t subop;

    switch (op1) {
    case 0:  /* ADD8 */
        is_16bit = 0;
        subop = 0;
        break;
    case 1:  /* ADD16 */
        is_16bit = 1;
        subop = 0;
        break;
    case 2:  /* ASX */
        is_16bit = 1;
        subop = 1;
        break;
    case 4:  /* SUB8 */
        is_16bit = 0;
        subop = 3;  /* Non-zero for 8-bit SUB */
        break;
    case 5:  /* SUB16 */
        is_16bit = 1;
        subop = 2;
        break;
    case 6:  /* SAX */
        is_16bit = 1;
        subop = 3;
        break;
    default:
        return ARMV8M_ERR_UNDEFINED_INSN;
    }

    /* Combine into single op code for executor:
     * bit[7] = is_16bit (0=8-bit, 1=16-bit)
     * bits[6:4] = type (TTT from hw2) - signed/unsigned/halving/saturating
     * bits[1:0] = sub-operation (add=0, asx=1, sub=2, sax=3)
     */
    out->op = (uint8_t)((is_16bit << 7) | (type_ttt << 4) | subop);

    return 0;
}

/*============================================================================
 * Miscellaneous Operations (QADD, QSUB, CLZ, REV, etc.)
 *============================================================================*/

int decode_misc_ops(uint16_t hw1, uint16_t hw2, DecodedInsn *out)
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
