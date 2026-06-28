/**
 * @file decode_thumb32_vfp.c
 * @brief VFP/Floating point instruction decoding
 */

#include "decode_thumb32_internal.h"

/*============================================================================
 * Helper Functions
 *============================================================================*/

/**
 * Decode VFP S/D register from encoding bits.
 * For single-precision: Sd = D:Vd, Sn = N:Vn, Sm = M:Vm
 * For double-precision: Dd = Vd:D, Dn = Vn:N, Dm = Vm:M
 */
static void decode_vfp_regs(uint16_t hw1, uint16_t hw2, DecodedInsn *out,
                            bool is_double) {
  uint8_t D = (uint8_t)BIT(hw1, 6);
  uint8_t Vd = (uint8_t)EXTRACT(hw2, 12, 4);
  uint8_t N = (uint8_t)BIT(hw2, 7); /* N is in hw2, not hw1 */
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

/*============================================================================
 * VFP Instruction Decoding
 *============================================================================*/

/**
 * Decode the shared VFP data-processing tail (compare, convert, VMOV
 * immediate/register, VABS/VNEG/VSQRT, and the binary arithmetic ops).
 * Reached by the fall-through categories of decode_vfp.
 */
/**
 * Decode the VFP special data-processing ops (compare, convert, VMOV
 * immediate/register, VABS/VNEG/VSQRT). Returns 0 if decoded, 1 to fall
 * through to the binary arithmetic decode.
 */
static int decode_vfp_special_op(uint16_t hw1, uint16_t hw2, DecodedInsn *out) {
  uint32_t opc1_dp = EXTRACT(hw1, 4, 4);
  uint32_t opc2_dp = EXTRACT(hw2, 4, 3);
  uint32_t opc3 = EXTRACT(hw2, 6, 2);
  /* Check for compare (VCMP/VCMPE) - use mask for D bit
   * VCMP/VCMPE encoding: opc1 = 1D11 (0xB or 0xF), Vn = 01xx (4-7), opc2 =
   * 4/5/6/7 Vn bit[0] = 0 for register compare, 1 for compare with #0.0 Must
   * check Vn field to distinguish from VABS (Vn=0), VNEG/VSQRT (Vn=1) */
  uint8_t Vn_field = (uint8_t)EXTRACT(hw1, 0, 4);
  if ((opc1_dp & 0xB) == 0xB && ((Vn_field & 0xC) == 4) &&
      (opc2_dp & 0x5) == 0x4) {
    out->type = INSN_FPU_CMP;
    out->op = (uint8_t)BIT(hw2, 7); /* E bit for VCMPE vs VCMP at hw2[7] */
    /* Check for compare with zero (M=0 and Vm=0000) */
    if (BIT(hw2, 5) == 0 && EXTRACT(hw2, 0, 4) == 0) {
      out->sm = ARMV8M_REG_NONE; /* Compare with zero */
      out->dm = ARMV8M_REG_NONE;
    }
    return 0;
  }

  /* Check for conversion (VCVT) - use mask for D bit
   * VCVT (integer) has hw1[3] = 1, encoding opc2 in hw1[2:0]
   * opc2[2] = direction (1=to int, 0=to float)
   * opc2[1] = unsigned (1=unsigned, 0=signed)
   * opc2[0] = T bit (1=truncate toward zero, 0=use FPSCR rounding)
   * Executor expects: 2/3 for to-int (signed/unsigned), 4/5 for to-float
   * Bit 3 of op indicates truncate mode for float-to-int (2/3 + 8 = 10/11) */
  {
    uint8_t hw1_lo4 = (uint8_t)EXTRACT(hw1, 0, 4);
    if ((opc1_dp & 0xB) == 0xB && (hw1_lo4 & 0x8)) {
      /* This is a VCVT instruction: hw1[3:0] = 1xxx */
      out->type = INSN_FPU_CVT;
      uint8_t cvt_opc2 = hw1_lo4 & 0x7;
      uint8_t base = (cvt_opc2 & 0x4) ? 2 : 4; /* to-int=2, to-float=4 */
      out->op = base + ((cvt_opc2 >> 1) & 1);  /* +1 for unsigned */
      /* For float-to-int, bit 0 of cvt_opc2 indicates truncation mode */
      if ((cvt_opc2 & 0x4) && (cvt_opc2 & 0x1)) {
        out->op |= 0x8; /* Set bit 3 to indicate truncate */
      }
      return 0;
    }
  }

  /* Check for VMOV (immediate) - use mask for D bit */
  if ((opc1_dp & 0xB) == 0xB && (opc2_dp & 0x5) == 0x0 && (opc3 & 0x1) == 0) {
    out->type = INSN_FPU_MOVE;
    out->op = VFP_VMOV_IMM;
    /* Immediate is encoded in hw1[3:0]:hw2[3:0] */
    out->imm = ((uint32_t)EXTRACT(hw1, 0, 4) << 4) | EXTRACT(hw2, 0, 4);
    return 0;
  }

  /* Check for VMOV (register) - use mask for D bit */
  if ((opc1_dp & 0xB) == 0xB && (opc2_dp & 0x5) == 0x0 && opc3 == 0x1) {
    out->type = INSN_FPU_MOVE;
    out->op = VFP_VMOV_REG;
    return 0;
  }

  /* Check for VABS, VNEG, VSQRT - distinguished by Vn field and opc3
   * VABS:  Vn=0, opc3=11
   * VNEG:  Vn=1, opc3=01
   * VSQRT: Vn=1, opc3=11
   */
  if ((opc1_dp & 0xB) == 0xB && Vn_field == 0 && opc3 == 0x3) {
    out->type = INSN_FPU_ARITH;
    out->op = VFP_VABS;
    return 0;
  }
  if ((opc1_dp & 0xB) == 0xB && Vn_field == 1 && opc3 == 0x1) {
    out->type = INSN_FPU_ARITH;
    out->op = VFP_VNEG;
    return 0;
  }
  if ((opc1_dp & 0xB) == 0xB && Vn_field == 1 && opc3 == 0x3) {
    out->type = INSN_FPU_ARITH;
    out->op = VFP_VSQRT;
    return 0;
  }

  return 1;
}

static int decode_vfp_data_proc(uint16_t hw1, uint16_t hw2, DecodedInsn *out,
                                bool is_double) {

  /* VFP data processing instructions */
  uint32_t opc1_dp = EXTRACT(hw1, 4, 4);
  uint32_t opc3 = EXTRACT(hw2, 6, 2);

  decode_vfp_regs(hw1, hw2, out, is_double);

  if (decode_vfp_special_op(hw1, hw2, out) == 0) {
    return 0;
  }

  /* Standard arithmetic operations */
  out->type = INSN_FPU_ARITH;

  switch (opc1_dp & 0xB) {
  case 0x0: /* VMLA, VMLS */
    out->op = (opc3 & 0x1) ? VFP_VMLS : VFP_VMLA;
    break;
  case 0x1: /* VNMLA, VNMLS */
    /* hw2[6] (opc3 & 0x1): 1=VNMLA, 0=VNMLS */
    out->op = (opc3 & 0x1) ? VFP_VNMLA : VFP_VNMLS;
    break;
  case 0x2: /* VMUL, VNMUL */
    /* hw2[6] (opc3 & 0x1): 1=VNMUL, 0=VMUL */
    out->op = (opc3 & 0x1) ? VFP_VNMUL : VFP_VMUL;
    break;
  case 0x3: /* VADD, VSUB */
    out->op = (opc3 & 0x1) ? VFP_VSUB : VFP_VADD;
    break;
  case 0x8: /* VDIV */
    out->op = VFP_VDIV;
    break;
  case 0x9: /* VFNMA, VFNMS (fused negate multiply-add/subtract) */
    out->op = (opc3 & 0x1) ? VFP_VFNMA : VFP_VFNMS;
    break;
  case 0xA: /* VFMA, VFMS (fused multiply-add/subtract) */
    out->op = (opc3 & 0x1) ? VFP_VFMS : VFP_VFMA;
    break;
  default:
    return ARMV8M_ERR_UNDEFINED_INSN;
  }

  return 0;
}

/**
 * Decode VFP load/store: VLDR/VSTR (single) and VLDM/VSTM/VPUSH/VPOP.
 */
static int decode_vfp_load_store(uint16_t hw1, uint16_t hw2, DecodedInsn *out,
                                 bool is_double, uint32_t L) {
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
    out->imm = (uint32_t)imm8 << 2; /* Offset is imm8 * 4 */
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

    /* Set op based on L bit: 0=store (VSTM/VPUSH), 1=load (VLDM/VPOP) */
    out->op = (uint8_t)L;
    return 0;
  }
}

/**
 * VFP register/FPSCR transfers for categories 0b100/0b101 (64-bit VMOV,
 * single VMOV, VMRS/VMSR). Returns 0 if decoded, 1 to fall through to the
 * shared data-processing decode.
 */
static int decode_vfp_xfer_45(uint16_t hw1, uint16_t hw2, DecodedInsn *out,
                              bool is_double, uint32_t opc1) {
  /* Check for 64-bit transfer (VMOV between ARM and FPU) */
  if (opc1 == 0x4 || opc1 == 0x5) {
    /* VMOV (two ARM regs and FPU) */
    out->type = INSN_FPU_MOVE;
    out->op = VFP_VMOV_2ARM;
    out->rd = (uint8_t)EXTRACT(hw2, 12, 4); /* Rt */
    out->rt = (uint8_t)EXTRACT(hw1, 0, 4);  /* Rt2 */
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
    out->add = (opc1 == 0x5); /* Direction: 0=to FPU, 1=from FPU */
    return 0;
  }

  /* VMOV (single ARM reg and FPU) */
  if (opc1 == 0x0 || opc1 == 0x7) {
    out->type = INSN_FPU_MOVE;
    out->op = VFP_VMOV_ARM;
    out->rd = (uint8_t)EXTRACT(hw2, 12, 4); /* Rt */

    uint8_t Vn = (uint8_t)EXTRACT(hw1, 0, 4);
    uint8_t N = (uint8_t)BIT(hw2, 7); /* N bit is in hw2[7] */
    out->sn = (uint8_t)((Vn << 1) | N);
    out->dn = (uint8_t)(out->sn / 2);

    out->add = (opc1 == 0x7);      /* Direction: 0=to FPU, 1=from FPU */
    out->imm = EXTRACT(hw2, 5, 2); /* opc2 for scalar access */
    return 0;
  }

  /* VMRS/VMSR (transfer FPSCR) - verify additional fields to avoid
   * false matches with other instructions like VCVT */
  {
    uint8_t Vn_chk = (uint8_t)EXTRACT(hw1, 0, 4);
    uint8_t hw2_lo = (uint8_t)EXTRACT(hw2, 0, 8);
    if ((opc1 == 0xF || opc1 == 0xE) && Vn_chk == 1 && hw2_lo == 0x10) {
      out->type = INSN_FPU_MOVE;
      out->op = VFP_VMOV_ARM;
      out->rd = (uint8_t)EXTRACT(hw2, 12, 4);
      out->add = (opc1 == 0xF); /* VMRS (to ARM) */
      out->sysreg = 0x80;       /* Special marker for FPSCR */
      return 0;
    }
  }

  /* Fall through to data processing */
  return 1;
}

/**
 * VFP single VMOV / VMRS-VMSR transfers for category 0b111. Returns 0 if
 * decoded, 1 to fall through to the shared data-processing decode.
 */
static int decode_vfp_xfer_7(uint16_t hw1, uint16_t hw2, DecodedInsn *out,
                             uint32_t opc1) {
  /* Check for VMOV (single ARM reg and FPU) first
   * VMOV Sn,Rt: opc1=0x0, VMOV Rt,Sn: opc1=0x1
   * VMOV encoding: hw2[4]=1, hw2[3:0]=0000
   * Must distinguish from VMLA which has opc1=0x0 but different hw2 */
  if ((opc1 == 0x0 || opc1 == 0x1) && BIT(hw2, 4) && EXTRACT(hw2, 0, 4) == 0) {
    out->type = INSN_FPU_MOVE;
    out->op = VFP_VMOV_ARM;
    out->rd = (uint8_t)EXTRACT(hw2, 12, 4); /* Rt */

    uint8_t Vn = (uint8_t)EXTRACT(hw1, 0, 4);
    uint8_t N = (uint8_t)BIT(hw2, 7); /* N bit is in hw2[7] */
    out->sn = (uint8_t)((Vn << 1) | N);
    out->dn = (uint8_t)(out->sn / 2);

    out->add = (opc1 == 0x1); /* Direction: 0=to FPU, 1=from FPU */
    return 0;
  }

  /* Check for VMRS/VMSR (transfer FPSCR)
   * VMRS/VMSR have hw1[3:0] = 0001 and hw2[7:0] = 0x10 */
  uint8_t Vn_check = (uint8_t)EXTRACT(hw1, 0, 4);
  uint8_t hw2_low = (uint8_t)EXTRACT(hw2, 0, 8);
  if ((opc1 == 0xF || opc1 == 0xE) && Vn_check == 1 && hw2_low == 0x10) {
    out->type = INSN_FPU_MOVE;
    out->op = VFP_VMOV_ARM;
    out->rd = (uint8_t)EXTRACT(hw2, 12, 4);
    out->add = (opc1 == 0xF); /* VMRS (to ARM) */
    out->sysreg = 0x80;       /* Special marker for FPSCR */
    return 0;
  }
  /* Fall through to VFP data processing */
  return 1;
}

int decode_vfp(uint16_t hw1, uint16_t hw2, DecodedInsn *out) {
  uint32_t opc1 = EXTRACT(hw1, 4, 4); /* bits [7:4] of hw1 */
  uint32_t opc2 = EXTRACT(hw2, 4, 4); /* bits [7:4] of hw2 */
  uint32_t cp = EXTRACT(hw2, 8, 4);   /* Coprocessor number */

  /* Check for VFP coprocessors (CP10 or CP11) */
  if (cp != 10 && cp != 11) {
    /* Not a VFP instruction - generic coprocessor */
    out->type = (BIT(hw1, 4)) ? INSN_MRC : INSN_MCR;
    out->rd = (uint8_t)EXTRACT(hw2, 12, 4);
    out->imm = (opc1 << 4) | opc2;
    return 0;
  }

  bool is_double = (cp == 11);

  /* Categorize by hw1[11:9] and hw1[4] */
  uint32_t category = EXTRACT(hw1, 9, 3);
  uint32_t L = BIT(hw1, 4);

  switch (category) {
  case 0x6: /* 110 - VLDR/VSTR, VLDM/VSTM */
    return decode_vfp_load_store(hw1, hw2, out, is_double, L);

  case 0x4: /* 100 - VFP data processing, 64-bit transfers */
  case 0x5: /* 101 - same */
    /* 64-bit / single transfers, then the VMOV-single / VMRS-VMSR checks that
     * categories 0b100/0b101 share with 0b111. */
    if (decode_vfp_xfer_45(hw1, hw2, out, is_double, opc1) == 0) {
      return 0;
    }
    if (decode_vfp_xfer_7(hw1, hw2, out, opc1) == 0) {
      return 0;
    }
    break;

  case 0x7: /* 111 - VFP data processing, VMOV single, or VMRS/VMSR */
    if (decode_vfp_xfer_7(hw1, hw2, out, opc1) == 0) {
      return 0;
    }
    break;

  default:
    break;
  }

  return decode_vfp_data_proc(hw1, hw2, out, is_double);
}
