/**
 * @file decode_thumb32_multiply.c
 * @brief Multiply and divide instruction decoding
 */

#include "decode_thumb32_internal.h"

/*============================================================================
 * Multiply Instructions
 *============================================================================*/

/* Idiomatic flat dispatch switch (one trivial case per opcode/register); the
 * high cognitive-complexity score is a false positive, so it is suppressed. */
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
int decode_multiply(uint16_t hw1, uint16_t hw2, DecodedInsn *out) {
  uint8_t op1 = (uint8_t)EXTRACT(hw1, 4, 3); /* bits [6:4] - multiply type */
  uint8_t rn = (uint8_t)EXTRACT(hw1, 0, 4);
  uint8_t rd = (uint8_t)EXTRACT(hw2, 8, 4);
  uint8_t rm = (uint8_t)EXTRACT(hw2, 0, 4);
  uint8_t ra = (uint8_t)EXTRACT(hw2, 12, 4);
  uint8_t op2 = (uint8_t)EXTRACT(hw2, 4, 2); /* bits [5:4] - sub-operation */

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
     * op2[0]: X variant (exchange halfwords of Rm)
     * op2[1]: 0 = SMUAD/SMLAD (add), 1 = SMUSD/SMLSD (subtract)
     * Ra = 0xF: SMUAD/SMUSD (multiply only)
     * Ra != 0xF: SMLAD/SMLSD (accumulate)
     */
    if (ra == 0xF) {
      /* SMUAD/SMUADX/SMUSD/SMUSDX */
      if (op2 & 2) {
        out->op = (op2 & 1) ? MUL_SMUSDX : MUL_SMUSD;
      } else {
        out->op = (op2 & 1) ? MUL_SMUADX : MUL_SMUAD;
      }
    } else {
      /* SMLAD/SMLADX/SMLSD/SMLSDX */
      if (op2 & 2) {
        out->op = (op2 & 1) ? MUL_SMLSDX : MUL_SMLSD;
      } else {
        out->op = (op2 & 1) ? MUL_SMLADX : MUL_SMLAD;
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
     * op2[0]: X variant (exchange halfwords of Rm)
     * op2[1]: reserved (but treated like op1=2)
     */
    if (ra == 0xF) {
      out->op = (op2 & 1) ? MUL_SMUSDX : MUL_SMUSD;
    } else {
      out->op = (op2 & 1) ? MUL_SMLSDX : MUL_SMLSD;
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

int decode_long_multiply(uint16_t hw1, uint16_t hw2, DecodedInsn *out) {
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
  out->rt = rdhi; /* Use rt for high register */

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
