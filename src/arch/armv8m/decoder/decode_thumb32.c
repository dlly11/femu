/**
 * @file decode_thumb32.c
 * @brief 32-bit Thumb instruction decoding for ARMv8-M
 *
 * Handles all 32-bit Thumb instruction encodings according to ARMv8-M spec.
 * 32-bit instructions are identified by bits[15:11] of first halfword:
 *   11101 - Various (data processing, branches, etc.)
 *   11110 - Branches and miscellaneous control
 *   11111 - Coprocessor, load/store
 *
 * The actual decoding is split across multiple files:
 *   decode_thumb32_branch.c    - Branch and miscellaneous control
 *   decode_thumb32_data.c      - Data processing (imm, shifted reg)
 *   decode_thumb32_loadstore.c - Load/store (single, dual, multiple)
 *   decode_thumb32_multiply.c  - Multiply and divide
 *   decode_thumb32_dsp.c       - DSP parallel add/sub and misc ops
 *   decode_thumb32_vfp.c       - VFP/FPU instructions
 */

#include "decode_thumb32_internal.h"

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
int decode_thumb32(uint16_t hw1, uint16_t hw2, uint32_t pc, DecodedInsn *out) {
  uint32_t op1 = EXTRACT(hw1, 11, 2); /* bits [12:11] of hw1 */
  uint32_t op2 = EXTRACT(hw1, 4, 7);  /* bits [10:4] of hw1 */
  uint32_t op = BIT(hw2, 15);         /* bit 15 of hw2 */

  /* Check for SG (Secure Gateway) - exact encoding 0xE97FE97F */
  if (hw1 == 0xE97F && hw2 == 0xE97F) {
    out->type = INSN_SG;
    return 0;
  }

  /* Check for TT/TTT/TTA/TTAT (Test Target) - hw1[15:4] = 0xE84, hw2[5:0] =
   * 0x3F This distinguishes TT from STREX which shares the same hw1[15:4]
   * pattern */
  if ((hw1 & 0xFFF0) == 0xE840 && (hw2 & 0x3F) == 0x3F) {
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
  uint32_t hw1_top = (hw1 >> 9) & 0x7F; /* bits [15:9] */
  if (hw1_top == 0x76 || hw1_top == 0x77) {
    /* VFP instructions: hw1[15:9] = 1110_11x (0x76 or 0x77) */
    return decode_vfp(hw1, hw2, out);
  }
  /* Also check for op1=0b11 coprocessor dispatch later in the switch */

  /* Dispatch based on op1 (bits [12:11] of first halfword) */
  switch (op1) {
  case 0x1: /* 0b01 */
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

  case 0x2: /* 0b10 */
    if (op == 0) {
      /* Dispatch based on bit 9 of hw1:
       * bit 9 = 0: Data-processing (modified immediate) - AND, ORR, MOV, etc.
       * bit 9 = 1: Data-processing (plain binary immediate) - ADDW, MOVW, BFI,
       * SBFX, etc.
       */
      if (BIT(hw1, 9)) {
        return decode_data_proc_plain_imm(hw1, hw2, out);
      }
      return decode_data_proc_modified_imm(hw1, hw2, out);
    }
    /* op == 1: Branches and miscellaneous control */
    return decode_branch_misc(hw1, hw2, pc, out);

  case 0x3: /* 0b11 */
    /* Check multiply instructions first - they don't depend on op (bit 15 of
     * hw2) */
    if ((op2 & 0x78) == 0x30) {
      /* Multiply, multiply accumulate - op2 = 0110xxx */
      return decode_multiply(hw1, hw2, out);
    }
    if ((op2 & 0x78) == 0x38) {
      /* Long multiply, long multiply accumulate, divide - op2 = 0111xxx */
      return decode_long_multiply(hw1, hw2, out);
    }

    if (op == 0) {
      /* Load/store single (multiple patterns)
       * op2 bits: [6]=hw1[10], [5]=hw1[9], [4]=hw1[8], [3]=hw1[7],
       *           [2:1]=size (00=byte,01=half,10=word), [0]=L (load)
       * Store: L=0, op2[0]=0 -> (op2 & 0x71) == 0x00
       * Load byte: L=1, size=00 -> (op2 & 0x67) == 0x01
       * Load half: L=1, size=01 -> (op2 & 0x67) == 0x03
       * Load word: L=1, size=10 -> (op2 & 0x67) == 0x05 */
      if ((op2 & 0x71) == 0x00 || (op2 & 0x67) == 0x01 ||
          (op2 & 0x67) == 0x03 || (op2 & 0x67) == 0x05 ||
          (op2 & 0x71) == 0x20) {
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
        uint32_t op1_reg = EXTRACT(hw1, 4, 4);

        /* Check for extend instructions: hw2[7:6] = 10 (value 2) AND op1_reg <=
         * 5 */
        if (EXTRACT(hw2, 6, 2) == 2 && op1_reg <= 5) {
          /* Signed/unsigned extend (and add) instructions:
           * SXTH/SXTAH (op1_reg=0), UXTH/UXTAH (op1_reg=1),
           * SXTB16/SXTAB16 (op1_reg=2), UXTB16/UXTAB16 (op1_reg=3),
           * SXTB/SXTAB (op1_reg=4), UXTB/UXTAB (op1_reg=5) */
          uint8_t rn = (uint8_t)EXTRACT(hw1, 0, 4);
          uint8_t rd = (uint8_t)EXTRACT(hw2, 8, 4);
          uint8_t rm = (uint8_t)EXTRACT(hw2, 0, 4);
          uint8_t rotate = (uint8_t)EXTRACT(hw2, 4, 2);

          out->type = INSN_EXTEND;
          out->rd = rd;
          out->rm = rm;
          out->rn =
              (rn == 0xF) ? ARMV8M_REG_NONE : rn; /* Simple extend vs add */
          out->shift_amount = rotate * 8; /* Rotation: 0, 8, 16, or 24 */

          /* Determine signed/unsigned and size from op1_reg */
          switch (op1_reg) {
          case 0: /* SXTH/SXTAH */
            out->is_signed = true;
            out->access_size = ACCESS_HALF;
            break;
          case 1: /* UXTH/UXTAH */
            out->is_signed = false;
            out->access_size = ACCESS_HALF;
            break;
          case 2: /* SXTB16/SXTAB16 - dual byte to halfword */
            out->is_signed = true;
            out->access_size = ACCESS_BYTE;
            out->op = 1; /* Mark as B16 variant */
            break;
          case 3: /* UXTB16/UXTAB16 */
            out->is_signed = false;
            out->access_size = ACCESS_BYTE;
            out->op = 1; /* Mark as B16 variant */
            break;
          case 4: /* SXTB/SXTAB */
            out->is_signed = true;
            out->access_size = ACCESS_BYTE;
            break;
          default: /* case 5: UXTB/UXTAB */
            out->is_signed = false;
            out->access_size = ACCESS_BYTE;
            break;
          }
          return 0;
        }

        /* Check for register-controlled shifts: hw2[15:12]=0xF, hw2[7:4]=0000
         * AND hw1[7:5] must be a valid shift type (0-3). Otherwise it's
         * a parallel add/sub instruction with similar encoding. */
        if (EXTRACT(hw2, 12, 4) == 0xF && EXTRACT(hw2, 4, 4) == 0) {
          uint8_t shift_type = (uint8_t)EXTRACT(hw1, 5, 3);

          /* Only shift types 0-3 are valid for register-controlled shifts */
          if (shift_type <= 3) {
            /* LSL.W, LSR.W, ASR.W, ROR.W by register amount
             * hw1[7:5] = shift type: 000=LSL, 001=LSR, 010=ASR, 011=ROR
             * hw1[4] = S bit
             * hw1[3:0] = Rm (source register - value to shift)
             * hw2[11:8] = Rd
             * hw2[3:0] = Rs (shift amount register)
             *
             * exec_dp_operation expects: shift(rn_val, shift_type, op2)
             * So rn = Rm (value), rm = Rs (amount) */
            uint8_t S = (uint8_t)BIT(hw1, 4);
            uint8_t rn = (uint8_t)EXTRACT(hw1, 0, 4); /* Rm: value to shift */
            uint8_t rd = (uint8_t)EXTRACT(hw2, 8, 4);
            uint8_t rs = (uint8_t)EXTRACT(hw2, 0, 4); /* Rs: shift amount */

            out->type = INSN_DATA_PROC_REG;
            out->rd = rd;
            out->rn = rn; /* Value to shift (Rm in encoding) */
            out->rm = rs; /* Shift amount register (Rs) */
            out->set_flags = (S != 0);

            /* Map shift type to DataProcOp */
            switch (shift_type) {
            case 0:
              out->op = DP_LSL;
              break;
            case 1:
              out->op = DP_LSR;
              break;
            case 2:
              out->op = DP_ASR;
              break;
            default:
              out->op = DP_ROR;
              break; /* case 3 */
            }
            return 0;
          }
          /* Fall through to parallel add/sub check for invalid shift types */
        }

        /* Check for parallel add/sub or misc operations.
         * Parallel add/sub: op1_reg = 4-15, hw2[7:6] = 00 (16-bit) or 01
         * (8-bit) Misc ops (QADD, etc.): op1_reg = 8-11, hw2[7:6] = 10 */
        if (op1_reg >= 0x4) {
          uint8_t hw2_76 = (uint8_t)EXTRACT(hw2, 6, 2);
          if (hw2_76 <= 1) {
            /* Parallel add/sub: hw2[7:6] = 00 or 01 */
            return decode_parallel_add_sub(hw1, hw2, out);
          }
          if (hw2_76 == 2 && op1_reg >= 0x8 && op1_reg <= 0xB) {
            /* Misc operations (QADD, QSUB, CLZ, etc.): hw2[7:6] = 10 */
            return decode_misc_ops(hw1, hw2, out);
          }
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
