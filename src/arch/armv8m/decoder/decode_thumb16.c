/**
 * @file decode_thumb16.c
 * @brief 16-bit Thumb instruction decoding for ARMv8-M
 *
 * Handles all 16-bit Thumb instruction encodings according to ARMv8-M spec.
 * Categories based on bits[15:11]:
 *   00xxx - Shift, add, subtract, move, compare
 *   01000 - Data processing
 *   01001 - Load from literal pool
 *   0101x - Load/store register offset
 *   011xx - Load/store word immediate
 *   100xx - Load/store halfword, SP-relative, PC-relative
 *   10100 - ADR (PC-relative address)
 *   10101 - ADD SP + immediate
 *   1011x - Miscellaneous (push, pop, extend, etc.)
 *   11000 - Store multiple
 *   11001 - Load multiple
 *   1101x - Conditional branch, SVC
 *   11100 - Unconditional branch
 */

#include "arch/armv8m/armv8m_decoder.h"

/*============================================================================
 * Helper Macros
 *============================================================================*/

/* Extract bit field from instruction */
#define EXTRACT(val, start, len)                                               \
  (((uint32_t)(val) >> (start)) & ((1U << (len)) - 1))

/* Extract single bit */
#define BIT(val, n) (((val) >> (n)) & 1)

/* Sign extend a value */
static inline int32_t sign_extend(uint32_t val, int bits) {
  int32_t shift = 32 - bits;
  return ((int32_t)(val << shift)) >> shift;
}

/*============================================================================
 * Forward Declarations
 *============================================================================*/

static int decode_shift_add_sub_mov_cmp(uint16_t insn, DecodedInsn *out);
static int decode_data_proc(uint16_t insn, DecodedInsn *out);
static int decode_special_data_branch(uint16_t insn, DecodedInsn *out);
static int decode_load_literal(uint16_t insn, uint32_t pc, DecodedInsn *out);
static int decode_load_store_reg(uint16_t insn, DecodedInsn *out);
static int decode_load_store_imm_word(uint16_t insn, DecodedInsn *out);
static int decode_load_store_imm_byte(uint16_t insn, DecodedInsn *out);
static int decode_load_store_imm_half(uint16_t insn, DecodedInsn *out);
static int decode_load_store_sp_rel(uint16_t insn, DecodedInsn *out);
static int decode_adr(uint16_t insn, uint32_t pc, DecodedInsn *out);
static int decode_add_sp_imm(uint16_t insn, DecodedInsn *out);
static int decode_misc(uint16_t insn, DecodedInsn *out);
static int decode_stm(uint16_t insn, DecodedInsn *out);
static int decode_ldm(uint16_t insn, DecodedInsn *out);
static int decode_cond_branch_svc(uint16_t insn, uint32_t pc, DecodedInsn *out);
static int decode_uncond_branch(uint16_t insn, uint32_t pc, DecodedInsn *out);

/*============================================================================
 * Main Entry Point
 *============================================================================*/

/**
 * Decode a 16-bit Thumb instruction.
 *
 * @param insn  16-bit instruction word
 * @param pc    Current program counter
 * @param out   Output decoded instruction structure
 * @return      0 on success, negative error code on failure
 */
int decode_thumb16(uint16_t insn, uint32_t pc, DecodedInsn *out) {
  uint16_t op = EXTRACT(insn, 11, 5); /* bits [15:11] */

  switch (op) {
  /* 00xxx - Shift, add, subtract, move, compare */
  case 0x00:
  case 0x01:
  case 0x02:
  case 0x03:
  case 0x04:
  case 0x05:
  case 0x06:
  case 0x07:
    return decode_shift_add_sub_mov_cmp(insn, out);

  /* 01000 - Data processing */
  case 0x08:
    if (BIT(insn, 10)) {
      /* Special data instructions and branch/exchange */
      return decode_special_data_branch(insn, out);
    } else {
      return decode_data_proc(insn, out);
    }

  /* 01001 - Load from literal pool (PC-relative) */
  case 0x09:
    return decode_load_literal(insn, pc, out);

  /* 0101x - Load/store register offset */
  case 0x0A:
  case 0x0B:
    return decode_load_store_reg(insn, out);

  /* 011xx - Load/store word/byte immediate */
  case 0x0C:
  case 0x0D:
    return decode_load_store_imm_word(insn, out);
  case 0x0E:
  case 0x0F:
    return decode_load_store_imm_byte(insn, out);

  /* 10000 - Load/store halfword immediate */
  case 0x10:
  case 0x11:
    return decode_load_store_imm_half(insn, out);

  /* 10010 - Load/store SP-relative */
  case 0x12:
  case 0x13:
    return decode_load_store_sp_rel(insn, out);

  /* 10100 - ADR (PC-relative address) */
  case 0x14:
    return decode_adr(insn, pc, out);

  /* 10101 - ADD SP + immediate */
  case 0x15:
    return decode_add_sp_imm(insn, out);

  /* 1011x - Miscellaneous (push, pop, hints, etc.) */
  case 0x16:
  case 0x17:
    return decode_misc(insn, out);

  /* 11000 - Store multiple */
  case 0x18:
    return decode_stm(insn, out);

  /* 11001 - Load multiple */
  case 0x19:
    return decode_ldm(insn, out);

  /* 1101x - Conditional branch, SVC, undefined */
  case 0x1A:
  case 0x1B:
    return decode_cond_branch_svc(insn, pc, out);

  /* 11100 - Unconditional branch */
  case 0x1C:
    return decode_uncond_branch(insn, pc, out);

  /* 11101, 11110, 11111 are 32-bit instruction prefixes */
  default:
    return ARMV8M_ERR_UNDEFINED_INSN;
  }
}

/*============================================================================
 * Shift, Add, Subtract, Move, Compare (bits[15:11] = 00xxx)
 *============================================================================*/

static int decode_shift_add_sub_mov_cmp(uint16_t insn, DecodedInsn *out) {
  uint16_t op = EXTRACT(insn, 11, 5); /* bits[15:11] */

  /*
   * Encoding for bits[15:11]:
   * 0 (00000): LSL immediate
   * 1 (00001): LSR immediate
   * 2 (00010): ASR immediate
   * 3 (00011): ADD/SUB register or 3-bit immediate
   * 4 (00100): MOV immediate 8-bit
   * 5 (00101): CMP immediate 8-bit
   * 6 (00110): ADD immediate 8-bit
   * 7 (00111): SUB immediate 8-bit
   */

  if (op == 0x00) {
    /* LSL Rd, Rm, #imm5 */
    uint8_t imm5 = (uint8_t)EXTRACT(insn, 6, 5);
    uint8_t rm = (uint8_t)EXTRACT(insn, 3, 3);
    uint8_t rd = (uint8_t)EXTRACT(insn, 0, 3);

    if (imm5 == 0) {
      /* MOVS Rd, Rm (LSL #0 is just move) */
      out->type = INSN_DATA_PROC_REG;
      out->op = DP_MOV;
      out->rd = rd;
      out->rm = rm;
      out->set_flags = true;
    } else {
      out->type = INSN_DATA_PROC_SHIFTED;
      out->op = DP_MOV; /* Result is just the shifted value */
      out->rd = rd;
      out->rm = rm;
      out->shift_type = SHIFT_LSL;
      out->shift_amount = imm5;
      out->set_flags = true;
    }
    return 0;
  }

  if (op == 0x01) {
    /* LSR Rd, Rm, #imm5 */
    uint8_t imm5 = (uint8_t)EXTRACT(insn, 6, 5);
    uint8_t rm = (uint8_t)EXTRACT(insn, 3, 3);
    uint8_t rd = (uint8_t)EXTRACT(insn, 0, 3);

    out->type = INSN_DATA_PROC_SHIFTED;
    out->op = DP_MOV; /* Result is just the shifted value */
    out->rd = rd;
    out->rm = rm;
    out->shift_type = SHIFT_LSR;
    /* imm5 == 0 means shift by 32 */
    out->shift_amount = (imm5 == 0) ? 32 : imm5;
    out->set_flags = true;
    return 0;
  }

  if (op == 0x02) {
    /* ASR Rd, Rm, #imm5 */
    uint8_t imm5 = (uint8_t)EXTRACT(insn, 6, 5);
    uint8_t rm = (uint8_t)EXTRACT(insn, 3, 3);
    uint8_t rd = (uint8_t)EXTRACT(insn, 0, 3);

    out->type = INSN_DATA_PROC_SHIFTED;
    out->op = DP_MOV; /* Result is just the shifted value */
    out->rd = rd;
    out->rm = rm;
    out->shift_type = SHIFT_ASR;
    /* imm5 == 0 means shift by 32 */
    out->shift_amount = (imm5 == 0) ? 32 : imm5;
    out->set_flags = true;
    return 0;
  }

  if (op == 0x03) {
    /* ADD/SUB register or 3-bit immediate */
    uint8_t rd = (uint8_t)EXTRACT(insn, 0, 3);
    uint8_t rn = (uint8_t)EXTRACT(insn, 3, 3);
    uint8_t is_imm = (uint8_t)BIT(insn, 10);
    uint8_t is_sub = (uint8_t)BIT(insn, 9);

    if (is_imm) {
      /* ADD/SUB Rd, Rn, #imm3 */
      uint8_t imm3 = (uint8_t)EXTRACT(insn, 6, 3);
      out->type = INSN_DATA_PROC_IMM;
      out->op = is_sub ? DP_SUB : DP_ADD;
      out->rd = rd;
      out->rn = rn;
      out->imm = imm3;
    } else {
      /* ADD/SUB Rd, Rn, Rm */
      uint8_t rm = (uint8_t)EXTRACT(insn, 6, 3);
      out->type = INSN_DATA_PROC_REG;
      out->op = is_sub ? DP_SUB : DP_ADD;
      out->rd = rd;
      out->rn = rn;
      out->rm = rm;
    }
    out->set_flags = true;
    return 0;
  }

  /* op == 0x04..0x07: Move/compare/add/subtract immediate 8-bit */
  {
    uint8_t rd = (uint8_t)EXTRACT(insn, 8, 3);
    uint8_t imm8 = (uint8_t)EXTRACT(insn, 0, 8);

    out->type = INSN_DATA_PROC_IMM;
    out->rd = rd;
    out->imm = imm8;
    out->set_flags = true;

    switch (op) {
    case 0x04: /* MOVS Rd, #imm8 */
      out->op = DP_MOV;
      break;
    case 0x05: /* CMP Rn, #imm8 */
      out->op = DP_CMP;
      out->rn = rd;
      out->rd = ARMV8M_REG_NONE; /* CMP doesn't write */
      break;
    case 0x06: /* ADDS Rd, #imm8 */
      out->op = DP_ADD;
      out->rn = rd;
      break;
    case 0x07: /* SUBS Rd, #imm8 */
      out->op = DP_SUB;
      out->rn = rd;
      break;
    default:
      return ARMV8M_ERR_UNDEFINED_INSN;
    }
    return 0;
  }
}

/*============================================================================
 * Data Processing (bits[15:10] = 010000)
 *============================================================================*/

static int decode_data_proc(uint16_t insn, DecodedInsn *out) {
  uint8_t op = EXTRACT(insn, 6, 4);
  uint8_t rm = EXTRACT(insn, 3, 3);
  uint8_t rdn = EXTRACT(insn, 0, 3);

  out->type = INSN_DATA_PROC_REG;
  out->rd = rdn;
  out->rn = rdn;
  out->rm = rm;
  out->set_flags = true;

  switch (op) {
  case 0x0: /* ANDS */
    out->op = DP_AND;
    break;
  case 0x1: /* EORS */
    out->op = DP_EOR;
    break;
  case 0x2: /* LSLS Rdn, Rs - shift Rdn by amount in Rs */
    out->op = DP_LSL;
    /* Keep INSN_DATA_PROC_REG: rn = value to shift, rm = shift count reg */
    break;
  case 0x3: /* LSRS Rdn, Rs - shift Rdn by amount in Rs */
    out->op = DP_LSR;
    /* Keep INSN_DATA_PROC_REG: rn = value to shift, rm = shift count reg */
    break;
  case 0x4: /* ASRS Rdn, Rs - shift Rdn by amount in Rs */
    out->op = DP_ASR;
    /* Keep INSN_DATA_PROC_REG: rn = value to shift, rm = shift count reg */
    break;
  case 0x5: /* ADCS */
    out->op = DP_ADC;
    break;
  case 0x6: /* SBCS */
    out->op = DP_SBC;
    break;
  case 0x7: /* RORS Rdn, Rs - rotate Rdn by amount in Rs */
    out->op = DP_ROR;
    /* Keep INSN_DATA_PROC_REG: rn = value to rotate, rm = rotate count reg */
    break;
  case 0x8: /* TST */
    out->op = DP_TST;
    out->rd = ARMV8M_REG_NONE; /* No destination */
    break;
  case 0x9: /* RSB (NEGS) */
    out->op = DP_RSB;
    out->type = INSN_DATA_PROC_IMM;
    out->rn = rm;
    out->imm = 0;
    break;
  case 0xA: /* CMP */
    out->op = DP_CMP;
    out->rn = rdn;
    out->rd = ARMV8M_REG_NONE;
    break;
  case 0xB: /* CMN */
    out->op = DP_CMN;
    out->rn = rdn;
    out->rd = ARMV8M_REG_NONE;
    break;
  case 0xC: /* ORRS */
    out->op = DP_ORR;
    break;
  case 0xD: /* MULS */
    out->type = INSN_MULTIPLY;
    out->op = MUL_MUL;
    out->set_flags = true; /* MULS always sets flags in Thumb-16 */
    break;
  case 0xE: /* BICS */
    out->op = DP_BIC;
    break;
  case 0xF: /* MVNS */
    out->op = DP_MVN;
    out->rd = rdn;
    out->rm = rm;
    out->rn = ARMV8M_REG_NONE;
    break;
  default:
    return ARMV8M_ERR_UNDEFINED_INSN;
  }

  return 0;
}

/*============================================================================
 * Special Data and Branch/Exchange (bits[15:10] = 010001)
 *============================================================================*/

static int decode_special_data_branch(uint16_t insn, DecodedInsn *out) {
  uint8_t op = EXTRACT(insn, 8, 2);
  uint8_t rm = EXTRACT(insn, 3, 4); /* 4 bits for high registers */
  uint8_t rdn = (uint8_t)(EXTRACT(insn, 0, 3) | ((uint32_t)BIT(insn, 7) << 3));

  switch (op) {
  case 0x0: /* ADD (high registers) */
    out->type = INSN_DATA_PROC_REG;
    out->op = DP_ADD;
    out->rd = rdn;
    out->rn = rdn;
    out->rm = rm;
    out->set_flags = false; /* ADD high reg doesn't set flags */
    break;

  case 0x1: /* CMP (high registers) */
    out->type = INSN_DATA_PROC_REG;
    out->op = DP_CMP;
    out->rn = rdn;
    out->rm = rm;
    out->rd = ARMV8M_REG_NONE;
    out->set_flags = true;
    break;

  case 0x2: /* MOV (high registers) */
    out->type = INSN_DATA_PROC_REG;
    out->op = DP_MOV;
    out->rd = rdn;
    out->rm = rm;
    out->set_flags = false;
    break;

  case 0x3: /* BX or BLX */
    if (BIT(insn, 7)) {
      /* BLX Rm */
      out->type = INSN_BRANCH_LINK_EXCHANGE;
      out->rm = rm;
      out->link = true;
    } else {
      /* BX Rm */
      out->type = INSN_BRANCH_EXCHANGE;
      out->rm = rm;
    }
    break;
  default:
    return ARMV8M_ERR_UNDEFINED_INSN;
  }

  return 0;
}

/*============================================================================
 * Load from Literal Pool (bits[15:11] = 01001)
 *============================================================================*/

static int decode_load_literal(uint16_t insn, uint32_t pc, DecodedInsn *out) {
  uint8_t rt = EXTRACT(insn, 8, 3);
  uint8_t imm8 = EXTRACT(insn, 0, 8);

  out->type = INSN_LOAD_LITERAL;
  out->rt = rt;
  /* Word-aligned: imm8 << 2, relative to PC aligned down to 4 bytes */
  out->imm = (uint32_t)imm8 << 2;
  out->rn = ARMV8M_REG_PC;
  out->access_size = ACCESS_WORD;
  out->add = true;
  (void)pc; /* PC used for address calculation in executor */

  return 0;
}

/*============================================================================
 * Load/Store Register Offset (bits[15:11] = 0101x)
 *============================================================================*/

static int decode_load_store_reg(uint16_t insn, DecodedInsn *out) {
  uint8_t opc = EXTRACT(insn, 9, 3);
  uint8_t rm = EXTRACT(insn, 6, 3);
  uint8_t rn = EXTRACT(insn, 3, 3);
  uint8_t rt = EXTRACT(insn, 0, 3);

  out->rt = rt;
  out->rn = rn;
  out->rm = rm;
  out->add = true;

  switch (opc) {
  case 0x0: /* STR (register) */
    out->type = INSN_STORE_REG;
    out->access_size = ACCESS_WORD;
    break;
  case 0x1: /* STRH (register) */
    out->type = INSN_STORE_REG;
    out->access_size = ACCESS_HALF;
    break;
  case 0x2: /* STRB (register) */
    out->type = INSN_STORE_REG;
    out->access_size = ACCESS_BYTE;
    break;
  case 0x3: /* LDRSB (register) */
    out->type = INSN_LOAD_REG;
    out->access_size = ACCESS_BYTE;
    out->is_signed = true;
    break;
  case 0x4: /* LDR (register) */
    out->type = INSN_LOAD_REG;
    out->access_size = ACCESS_WORD;
    break;
  case 0x5: /* LDRH (register) */
    out->type = INSN_LOAD_REG;
    out->access_size = ACCESS_HALF;
    break;
  case 0x6: /* LDRB (register) */
    out->type = INSN_LOAD_REG;
    out->access_size = ACCESS_BYTE;
    break;
  case 0x7: /* LDRSH (register) */
    out->type = INSN_LOAD_REG;
    out->access_size = ACCESS_HALF;
    out->is_signed = true;
    break;
  default:
    return ARMV8M_ERR_UNDEFINED_INSN;
  }

  return 0;
}

/*============================================================================
 * Load/Store Word Immediate (bits[15:11] = 011xx)
 *============================================================================*/

static int decode_load_store_imm_word(uint16_t insn, DecodedInsn *out) {
  uint8_t imm5 = EXTRACT(insn, 6, 5);
  uint8_t rn = EXTRACT(insn, 3, 3);
  uint8_t rt = EXTRACT(insn, 0, 3);
  bool is_load = BIT(insn, 11);

  out->rt = rt;
  out->rn = rn;
  out->imm = (uint32_t)imm5 << 2; /* Word-aligned */
  out->access_size = ACCESS_WORD;
  out->add = true;
  out->pre_index = true; /* Always pre-indexed for T1 encoding */

  if (is_load) {
    out->type = INSN_LOAD_IMM;
  } else {
    out->type = INSN_STORE_IMM;
  }

  return 0;
}

/*============================================================================
 * Load/Store Byte Immediate (bits[15:11] = 0111x)
 *============================================================================*/

static int decode_load_store_imm_byte(uint16_t insn, DecodedInsn *out) {
  uint8_t imm5 = EXTRACT(insn, 6, 5);
  uint8_t rn = EXTRACT(insn, 3, 3);
  uint8_t rt = EXTRACT(insn, 0, 3);
  bool is_load = BIT(insn, 11);

  out->rt = rt;
  out->rn = rn;
  out->imm = imm5; /* Byte-aligned (no shift) */
  out->access_size = ACCESS_BYTE;
  out->add = true;
  out->pre_index = true; /* Always pre-indexed for T1 encoding */

  if (is_load) {
    out->type = INSN_LOAD_IMM;
  } else {
    out->type = INSN_STORE_IMM;
  }

  return 0;
}

/*============================================================================
 * Load/Store Halfword Immediate (bits[15:11] = 1000x)
 *============================================================================*/

static int decode_load_store_imm_half(uint16_t insn, DecodedInsn *out) {
  uint8_t imm5 = EXTRACT(insn, 6, 5);
  uint8_t rn = EXTRACT(insn, 3, 3);
  uint8_t rt = EXTRACT(insn, 0, 3);
  bool is_load = BIT(insn, 11);

  out->rt = rt;
  out->rn = rn;
  out->imm = (uint32_t)imm5 << 1; /* Halfword-aligned */
  out->access_size = ACCESS_HALF;
  out->add = true;
  out->pre_index = true; /* Always pre-indexed for T1 encoding */

  if (is_load) {
    out->type = INSN_LOAD_IMM;
  } else {
    out->type = INSN_STORE_IMM;
  }

  return 0;
}

/*============================================================================
 * Load/Store SP-relative (bits[15:11] = 1001x)
 *============================================================================*/

static int decode_load_store_sp_rel(uint16_t insn, DecodedInsn *out) {
  uint8_t rt = EXTRACT(insn, 8, 3);
  uint8_t imm8 = EXTRACT(insn, 0, 8);
  bool is_load = BIT(insn, 11);

  out->rt = rt;
  out->rn = ARMV8M_REG_SP;
  out->imm = (uint32_t)imm8 << 2; /* Word-aligned */
  out->access_size = ACCESS_WORD;
  out->add = true;
  out->pre_index = true; /* Always pre-indexed for T2 encoding */

  if (is_load) {
    out->type = INSN_LOAD_IMM;
  } else {
    out->type = INSN_STORE_IMM;
  }

  return 0;
}

/*============================================================================
 * ADR (PC-relative address) (bits[15:11] = 10100)
 *============================================================================*/

static int decode_adr(uint16_t insn, uint32_t pc, DecodedInsn *out) {
  uint8_t rd = EXTRACT(insn, 8, 3);
  uint8_t imm8 = EXTRACT(insn, 0, 8);

  /* ADR Rd, label -> ADD Rd, PC, #imm */
  out->type = INSN_DATA_PROC_IMM;
  out->op = DP_ADD;
  out->rd = rd;
  out->rn = ARMV8M_REG_PC;
  out->imm = (uint32_t)imm8 << 2; /* Word-aligned */
  out->set_flags = false;
  (void)pc;

  return 0;
}

/*============================================================================
 * ADD SP + Immediate (bits[15:11] = 10101)
 *============================================================================*/

static int decode_add_sp_imm(uint16_t insn, DecodedInsn *out) {
  uint8_t rd = EXTRACT(insn, 8, 3);
  uint8_t imm8 = EXTRACT(insn, 0, 8);

  out->type = INSN_DATA_PROC_IMM;
  out->op = DP_ADD;
  out->rd = rd;
  out->rn = ARMV8M_REG_SP;
  out->imm = (uint32_t)imm8 << 2; /* Word-aligned */
  out->set_flags = false;

  return 0;
}

/*============================================================================
 * Miscellaneous Instructions (bits[15:11] = 1011x)
 *============================================================================*/

static int decode_misc(uint16_t insn, DecodedInsn *out) {
  uint8_t op = EXTRACT(insn, 8, 4);

  switch (op) {
  case 0x0: {
    /* ADD SP, SP, #imm7 or SUB SP, SP, #imm7 */
    uint8_t imm7 = EXTRACT(insn, 0, 7);
    bool is_sub = BIT(insn, 7);

    out->type = INSN_DATA_PROC_IMM;
    out->op = is_sub ? DP_SUB : DP_ADD;
    out->rd = ARMV8M_REG_SP;
    out->rn = ARMV8M_REG_SP;
    out->imm = (uint32_t)imm7 << 2;
    out->set_flags = false;
    break;
  }

  case 0x1: /* CBZ */
  case 0x3: /* CBZ */
  case 0x9: /* CBNZ */
  case 0xB: /* CBNZ */
  {
    uint8_t rn = EXTRACT(insn, 0, 3);
    uint8_t imm5 = EXTRACT(insn, 3, 5);
    uint8_t i = BIT(insn, 9);
    bool is_nz = BIT(insn, 11);

    out->type = INSN_COMPARE_BRANCH;
    out->rn = rn;
    out->branch_offset = (int32_t)(((i << 5) | imm5) << 1);
    /* is_nz stored in op: 1 = CBNZ, 0 = CBZ */
    out->op = is_nz ? 1 : 0;
    break;
  }

  case 0x2: /* SXTH, SXTB, UXTH, UXTB */
  {
    uint8_t opc = EXTRACT(insn, 6, 2);
    uint8_t rm = EXTRACT(insn, 3, 3);
    uint8_t rd = EXTRACT(insn, 0, 3);

    out->type = INSN_EXTEND;
    out->rd = rd;
    out->rm = rm;
    out->is_signed = !BIT(opc, 1); /* Signed if bit 1 is 0 */
    out->access_size = BIT(opc, 0) ? ACCESS_BYTE : ACCESS_HALF;
    break;
  }

  case 0x4:
  case 0x5: /* PUSH */
  {
    uint8_t reg_list = EXTRACT(insn, 0, 8);
    bool push_lr = BIT(insn, 8);

    out->type = INSN_STORE_MULTIPLE;
    out->rn = ARMV8M_REG_SP;
    out->register_list = reg_list | (push_lr ? (1 << ARMV8M_REG_LR) : 0);
    out->writeback = true;
    out->add = false; /* PUSH decrements */
    break;
  }

  case 0x6: /* CPS */
    out->type = INSN_CPS;
    out->imm = EXTRACT(insn, 0, 5);
    break;

  case 0xA: /* REV, REV16, REVSH */
  {
    uint8_t opc = EXTRACT(insn, 6, 2);
    uint8_t rm = EXTRACT(insn, 3, 3);
    uint8_t rd = EXTRACT(insn, 0, 3);

    out->type = INSN_EXTEND; /* Using EXTEND for byte swaps */
    out->rd = rd;
    out->rm = rm;
    out->op = opc; /* 0=REV, 1=REV16, 3=REVSH */
    break;
  }

  case 0xC:
  case 0xD: /* POP */
  {
    uint8_t reg_list = EXTRACT(insn, 0, 8);
    bool pop_pc = BIT(insn, 8);

    out->type = INSN_LOAD_MULTIPLE;
    out->rn = ARMV8M_REG_SP;
    out->register_list = reg_list | (pop_pc ? (1 << ARMV8M_REG_PC) : 0);
    out->writeback = true;
    out->add = true;
    break;
  }

  case 0xE: /* BKPT */
    out->type = INSN_HINT;
    out->op = 0xBE; /* BKPT encoding */
    out->imm = EXTRACT(insn, 0, 8);
    break;

  case 0xF: /* Hints or IT */
  {
    uint8_t mask = EXTRACT(insn, 0, 4);
    uint8_t hint = EXTRACT(insn, 4, 4);

    if (mask != 0) {
      /* IT instruction */
      out->type = INSN_IT;
      out->it_cond = hint;
      out->it_mask = mask;
    } else {
      /* Hints: NOP, YIELD, WFE, WFI, SEV */
      out->type = INSN_HINT;
      out->op = hint;
    }
    break;
  }

  default:
    return ARMV8M_ERR_UNDEFINED_INSN;
  }

  return 0;
}

/*============================================================================
 * Store Multiple (bits[15:11] = 11000)
 *============================================================================*/

static int decode_stm(uint16_t insn, DecodedInsn *out) {
  uint8_t rn = EXTRACT(insn, 8, 3);
  uint8_t reg_list = EXTRACT(insn, 0, 8);

  if (reg_list == 0) {
    return ARMV8M_ERR_UNPREDICTABLE;
  }

  out->type = INSN_STORE_MULTIPLE;
  out->rn = rn;
  out->register_list = reg_list;
  out->writeback = true;
  out->add = true;

  return 0;
}

/*============================================================================
 * Load Multiple (bits[15:11] = 11001)
 *============================================================================*/

static int decode_ldm(uint16_t insn, DecodedInsn *out) {
  uint8_t rn = EXTRACT(insn, 8, 3);
  uint8_t reg_list = EXTRACT(insn, 0, 8);

  if (reg_list == 0) {
    return ARMV8M_ERR_UNPREDICTABLE;
  }

  out->type = INSN_LOAD_MULTIPLE;
  out->rn = rn;
  out->register_list = reg_list;
  /* Writeback if Rn not in register list */
  out->writeback = !(reg_list & (1 << rn));
  out->add = true;

  return 0;
}

/*============================================================================
 * Conditional Branch and SVC (bits[15:11] = 1101x)
 *============================================================================*/

static int decode_cond_branch_svc(uint16_t insn, uint32_t pc,
                                  DecodedInsn *out) {
  uint8_t op = EXTRACT(insn, 8, 4);
  (void)pc;

  if (op == 0xE) {
    /* Permanently undefined */
    return ARMV8M_ERR_UNDEFINED_INSN;
  }

  if (op == 0xF) {
    /* SVC */
    out->type = INSN_SVC;
    out->imm = EXTRACT(insn, 0, 8);
    return 0;
  }

  /* Conditional branch */
  int8_t imm8 = (int8_t)EXTRACT(insn, 0, 8);

  out->type = INSN_BRANCH;
  out->cond = (ConditionCode)op;
  out->branch_offset = ((int32_t)imm8) << 1;

  return 0;
}

/*============================================================================
 * Unconditional Branch (bits[15:11] = 11100)
 *============================================================================*/

static int decode_uncond_branch(uint16_t insn, uint32_t pc, DecodedInsn *out) {
  uint32_t imm11 = EXTRACT(insn, 0, 11);
  (void)pc;

  out->type = INSN_BRANCH;
  out->cond = COND_AL;
  out->branch_offset = sign_extend(imm11 << 1, 12);

  return 0;
}
