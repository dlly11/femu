/**
 * @file decode_thumb32_branch.c
 * @brief Branch and miscellaneous control instruction decoding
 */

#include "decode_thumb32_internal.h"

/*============================================================================
 * Branch and Miscellaneous Control
 *============================================================================*/

int decode_branch_misc(uint16_t hw1, uint16_t hw2, uint32_t pc,
                       DecodedInsn *out) {
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
    uint32_t offset_u =
        (S << 20) | (J2 << 19) | (J1 << 18) | (imm6 << 12) | (imm11 << 1);
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
    uint32_t link = BIT(hw2, 14); /* BL has bit 14 set */

    /* I1 = NOT(J1 XOR S), I2 = NOT(J2 XOR S) */
    uint32_t I1 = !(J1 ^ S);
    uint32_t I2 = !(J2 ^ S);

    /* Offset: S:I1:I2:imm10:imm11:0 (25-bit signed) */
    uint32_t offset_u =
        (S << 24) | (I1 << 23) | (I2 << 22) | (imm10 << 12) | (imm11 << 1);
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

    uint32_t offset_u =
        (S << 24) | (I1 << 23) | (I2 << 22) | (imm10H << 12) | (imm10L << 2);
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
    case 0xB: {
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
          out->op = hint_op;     /* 4=DSB, 5=DMB, 6=ISB */
          out->imm = hint & 0xF; /* Barrier option (SY, ST, etc.) */
          return 0;
        }
        /* Hint instruction */
        out->type = INSN_HINT;
        out->op = hint_op; /* NOP=0, YIELD=1, WFE=2, WFI=3, SEV=4 */
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
    case 0xF: {
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
