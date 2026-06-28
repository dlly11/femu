/**
 * @file exec_fpu.c
 * @brief FPU instruction execution for ARMv8-M VFPv5
 *
 * Implements VLDR, VSTR, VMOV, VADD, VSUB, VMUL, VDIV, VCMP, VCVT, etc.
 */

#include "arch/armv8m/armv8m_exec_regs.h"
#include "arch/armv8m/armv8m_executor.h"
#include "arch/armv8m/armv8m_types.h"
#include "emu/emu_log.h"
#include <math.h>
#include <string.h>

/* fenv.h is C99 standard but may not be available on all platforms */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#include <fenv.h>
#define HAVE_FENV 1
#else
#define HAVE_FENV 0
#endif

/*============================================================================
 * VFP Operation Codes (must match decoder)
 *============================================================================*/

#define VFP_VMLA 0x00
#define VFP_VMLS 0x01
#define VFP_VNMLA 0x02
#define VFP_VNMLS 0x03
#define VFP_VNMUL 0x04
#define VFP_VMUL 0x05
#define VFP_VADD 0x06
#define VFP_VSUB 0x07
#define VFP_VDIV 0x08
#define VFP_VABS 0x10
#define VFP_VNEG 0x11
#define VFP_VSQRT 0x12
#define VFP_VFMA 0x13  /* Fused multiply-add: Sd += Sn * Sm */
#define VFP_VFMS 0x14  /* Fused multiply-subtract: Sd -= Sn * Sm */
#define VFP_VFNMA 0x15 /* Fused negate multiply-add: Sd = -(Sd + Sn * Sm) */
#define VFP_VFNMS 0x16 /* Fused negate multiply-subtract: Sd = -Sd + Sn*Sm */

#define VFP_VMOV_IMM 0x00
#define VFP_VMOV_REG 0x01
#define VFP_VMOV_ARM 0x02
#define VFP_VMOV_2ARM 0x03

/*============================================================================
 * Helper Functions
 *============================================================================*/

/**
 * Get single-precision value from S register.
 */
static inline float get_s(const CPUState *cpu, uint8_t reg) {
  union {
    uint32_t i;
    float f;
  } u;
  u.i = cpu->s[reg & 31];
  return u.f;
}

/**
 * Set single-precision value in S register.
 */
static inline void set_s(CPUState *cpu, uint8_t reg, float val) {
  union {
    uint32_t i;
    float f;
  } u;
  u.f = val;
  cpu->s[reg & 31] = u.i;
}

/**
 * Get double-precision value from D register (pair of S registers).
 */
static inline double get_d(const CPUState *cpu, uint8_t reg) {
  union {
    uint64_t i;
    double f;
  } u;
  uint8_t s_base = (uint8_t)((reg & 15) * 2);
  u.i = ((uint64_t)cpu->s[s_base + 1] << 32) | cpu->s[s_base];
  return u.f;
}

/**
 * Set double-precision value in D register.
 */
static inline void set_d(CPUState *cpu, uint8_t reg, double val) {
  union {
    uint64_t i;
    double f;
  } u;
  u.f = val;
  uint8_t s_base = (uint8_t)((reg & 15) * 2);
  cpu->s[s_base] = (uint32_t)u.i;
  cpu->s[s_base + 1] = (uint32_t)(u.i >> 32);
}

/*
 * ARM register access for FPU instructions. Reads the raw PC; shared
 * implementations live in armv8m_exec_regs.h.
 */
static inline uint32_t get_reg(const Executor *exec, uint8_t reg) {
  return armv8m_exec_get_reg(exec, reg);
}

static inline void set_reg(Executor *exec, uint8_t reg, uint32_t value) {
  armv8m_exec_set_reg(exec, reg, value);
}

/**
 * Read from memory.
 */
static uint32_t mem_read(Executor *exec, uint32_t addr, uint8_t size,
                         bool *fault) {
  *fault = false;
  if (!exec->mem.read) {
    *fault = true;
    return 0;
  }
  return exec->mem.read(exec->mem.ctx, addr, size, fault);
}

/**
 * Write to memory.
 */
static void mem_write(Executor *exec, uint32_t addr, uint32_t value,
                      uint8_t size, bool *fault) {
  *fault = false;
  if (!exec->mem.write) {
    *fault = true;
    return;
  }
  exec->mem.write(exec->mem.ctx, addr, value, size, fault);
}

/**
 * Update FPSCR flags after comparison.
 */
static void update_fpscr_cmp(CPUState *cpu, int cmp_result, bool is_nan) {
  cpu->fpscr &=
      ~(ARMV8M_FPSCR_N | ARMV8M_FPSCR_Z | ARMV8M_FPSCR_C | ARMV8M_FPSCR_V);

  if (is_nan) {
    /* Unordered: C=1, V=1, Z=0, N=0 */
    cpu->fpscr |= ARMV8M_FPSCR_C | ARMV8M_FPSCR_V;
  } else if (cmp_result == 0) {
    /* Equal: Z=1, C=1, V=0, N=0 */
    cpu->fpscr |= ARMV8M_FPSCR_Z | ARMV8M_FPSCR_C;
  } else if (cmp_result < 0) {
    /* Less than: N=1, Z=0, C=0, V=0 */
    cpu->fpscr |= ARMV8M_FPSCR_N;
  } else {
    /* Greater than: C=1, Z=0, V=0, N=0 */
    cpu->fpscr |= ARMV8M_FPSCR_C;
  }
}

/**
 * Mark FP context as active.
 */
static void mark_fp_active(Executor *exec) {
  exec->cpu.fp_context_active = true;
  exec->cpu.control |= ARMV8M_CONTROL_FPCA;
}

/**
 * Check if FPU is enabled.
 */
static bool is_fpu_enabled(const Executor *exec) { return exec->has_fpu; }

/**
 * Set C library rounding mode based on FPSCR RMode bits [23:22].
 * This should be called before floating-point operations that
 * need to honor the rounding mode setting.
 *
 * @param fpscr The FPSCR register value
 */
static void set_rounding_mode(uint32_t fpscr) {
#if HAVE_FENV
  int mode;
  switch ((fpscr >> 22) & 3) {
  case 0:
    mode = FE_TONEAREST;
    break; /* RN - Round to Nearest */
  case 1:
    mode = FE_UPWARD;
    break; /* RP - Round towards +infinity */
  case 2:
    mode = FE_DOWNWARD;
    break; /* RM - Round towards -infinity */
  case 3:
    mode = FE_TOWARDZERO;
    break; /* RZ - Round towards Zero */
  default:
    mode = FE_TONEAREST;
    break;
  }
  fesetround(mode);
#else
  (void)fpscr; /* Rounding mode not supported without fenv.h */
#endif
}

/**
 * Restore default rounding mode (round to nearest).
 * Should be called after operations that changed the rounding mode.
 */
static void restore_rounding_mode(void) {
#if HAVE_FENV
  fesetround(FE_TONEAREST);
#endif
}

/**
 * Trigger lazy FPU state preservation if active.
 *
 * When FPCCR.LSPACT is set, it means we deferred saving the FPU state
 * during exception entry. If an FPU instruction is executed before
 * exception return, we must now save the state.
 *
 * @param exec Executor context
 * @return ARMV8M_OK on success, error code on fault
 */
static int check_lazy_preservation(Executor *exec) {
  CPUState *cpu = &exec->cpu;

  /* Check if lazy preservation is pending */
  if (!(cpu->fpccr & ARMV8M_FPCCR_LSPACT)) {
    return ARMV8M_OK; /* Nothing to do */
  }

  /* Get the frame pointer where we need to save */
  uint32_t frameptr = cpu->fpcar;
  bool fault = false;

  /* Save S0-S15 (16 registers * 4 bytes = 64 bytes) */
  for (int i = 0; i < 16; i++) {
    uint32_t addr = frameptr + 32 + (uint32_t)(i * 4);
    if (!exec->mem.write) {
      return ARMV8M_ERR_BUS_FAULT;
    }
    exec->mem.write(exec->mem.ctx, addr, cpu->s[i], ACCESS_WORD, &fault);
    if (fault) {
      cpu->cfsr |= ARMV8M_BFSR_LSPERR;
      return ARMV8M_ERR_BUS_FAULT;
    }
  }

  /* Save FPSCR at offset 32 + 64 = 96 */
  exec->mem.write(exec->mem.ctx, frameptr + 96, cpu->fpscr, ACCESS_WORD,
                  &fault);
  if (fault) {
    cpu->cfsr |= ARMV8M_BFSR_LSPERR;
    return ARMV8M_ERR_BUS_FAULT;
  }

  /* Clear lazy stacking active flag */
  cpu->fpccr &= ~ARMV8M_FPCCR_LSPACT;

  return ARMV8M_OK;
}

/*============================================================================
 * FPU Load/Store Instructions
 *============================================================================*/

/**
 * Common FPU instruction prologue: require the FPU to be enabled, run any
 * pending lazy state preservation, and mark the FP context active. Returns
 * ARMV8M_OK, or a fault code the caller should propagate.
 */
static int fpu_prologue(Executor *exec) {
  if (!is_fpu_enabled(exec)) {
    exec->cpu.cfsr |= ARMV8M_UFSR_NOCP;
    return ARMV8M_ERR_USAGE_FAULT;
  }
  int lazy_result = check_lazy_preservation(exec);
  if (lazy_result != ARMV8M_OK) {
    return lazy_result;
  }
  mark_fp_active(exec);
  return ARMV8M_OK;
}

int exec_fpu_load(Executor *exec, const DecodedInsn *insn) {
  int prologue = fpu_prologue(exec);
  if (prologue != ARMV8M_OK) {
    return prologue;
  }

  uint32_t base = get_reg(exec, insn->rn);
  uint32_t addr = insn->add ? (base + insn->imm) : (base - insn->imm);
  bool fault = false;

  if (insn->is_double) {
    /* VLDR.64 */
    uint32_t lo = mem_read(exec, addr, ACCESS_WORD, &fault);
    if (fault)
      return ARMV8M_ERR_BUS_FAULT;
    uint32_t hi = mem_read(exec, addr + 4, ACCESS_WORD, &fault);
    if (fault)
      return ARMV8M_ERR_BUS_FAULT;

    uint8_t s_base = (uint8_t)((insn->dd & 15) * 2);
    exec->cpu.s[s_base] = lo;
    exec->cpu.s[s_base + 1] = hi;
    EMU_LOG_TRACE(EMU_LOG_CAT_EXECUTOR, "VLDR.64 D%d, [0x%08X]", insn->dd & 15,
                  addr);
  } else {
    /* VLDR.32 */
    uint32_t val = mem_read(exec, addr, ACCESS_WORD, &fault);
    if (fault)
      return ARMV8M_ERR_BUS_FAULT;

    exec->cpu.s[insn->sd & 31] = val;
    EMU_LOG_TRACE(EMU_LOG_CAT_EXECUTOR, "VLDR.32 S%d, [0x%08X] = 0x%08X",
                  insn->sd & 31, addr, val);
  }

  return ARMV8M_OK;
}

int exec_fpu_store(Executor *exec, const DecodedInsn *insn) {
  int prologue = fpu_prologue(exec);
  if (prologue != ARMV8M_OK) {
    return prologue;
  }

  uint32_t base = get_reg(exec, insn->rn);
  uint32_t addr = insn->add ? (base + insn->imm) : (base - insn->imm);
  bool fault = false;

  if (insn->is_double) {
    /* VSTR.64 */
    uint8_t s_base = (uint8_t)((insn->dd & 15) * 2);
    mem_write(exec, addr, exec->cpu.s[s_base], ACCESS_WORD, &fault);
    if (fault)
      return ARMV8M_ERR_BUS_FAULT;
    mem_write(exec, addr + 4, exec->cpu.s[s_base + 1], ACCESS_WORD, &fault);
    if (fault)
      return ARMV8M_ERR_BUS_FAULT;
    EMU_LOG_TRACE(EMU_LOG_CAT_EXECUTOR, "VSTR.64 D%d, [0x%08X]", insn->dd & 15,
                  addr);
  } else {
    /* VSTR.32 */
    uint32_t val = exec->cpu.s[insn->sd & 31];
    mem_write(exec, addr, val, ACCESS_WORD, &fault);
    if (fault)
      return ARMV8M_ERR_BUS_FAULT;
    EMU_LOG_TRACE(EMU_LOG_CAT_EXECUTOR, "VSTR.32 S%d, [0x%08X] = 0x%08X",
                  insn->sd & 31, addr, val);
  }

  return ARMV8M_OK;
}

/*============================================================================
 * FPU Multiple Load/Store (VLDM, VSTM, VPUSH, VPOP)
 *============================================================================*/

/**
 * Load or store a single word S-register slot at addr. Returns true on fault.
 */
static bool fpu_xfer_word(Executor *exec, uint32_t addr, uint32_t *slot,
                          bool is_load) {
  bool fault = false;
  if (is_load) {
    *slot = mem_read(exec, addr, ACCESS_WORD, &fault);
  } else {
    mem_write(exec, addr, *slot, ACCESS_WORD, &fault);
  }
  return fault;
}

/**
 * Transfer one register for a VLDM/VSTM/VPUSH/VPOP at *addr, advancing *addr.
 * Returns true on a bus fault (the caller aborts the transfer).
 */
static bool fpu_multi_xfer_one(Executor *exec, const DecodedInsn *insn,
                               uint32_t i, uint8_t start_reg, uint32_t *addr,
                               bool is_load) {
  if (insn->is_double) {
    uint8_t s_base = (uint8_t)(((start_reg + i) & 15) * 2);
    if (insn->add || !insn->pre_index) {
      /* Load ascending or store descending */
      if (fpu_xfer_word(exec, *addr, &exec->cpu.s[s_base], is_load) ||
          fpu_xfer_word(exec, *addr + 4, &exec->cpu.s[s_base + 1], is_load)) {
        return true;
      }
    }
    *addr += 8;
  } else {
    uint8_t s_reg = (uint8_t)((start_reg + i) & 31);
    if (fpu_xfer_word(exec, *addr, &exec->cpu.s[s_reg], is_load)) {
      return true;
    }
    *addr += 4;
  }
  return false;
}

int exec_fpu_multi(Executor *exec, const DecodedInsn *insn) {
  int prologue = fpu_prologue(exec);
  if (prologue != ARMV8M_OK) {
    return prologue;
  }

  uint32_t base = get_reg(exec, insn->rn);
  uint32_t count =
      insn->imm; /* Number of words (for single) or doublewords (for double) */
  uint32_t regs;
  uint32_t bytes;
  bool is_load =
      (insn->op == 1); /* op: 0=store (VSTM/VPUSH), 1=load (VLDM/VPOP) */

  /* Determine actual check based on the instruction format */
  /* For VLDM/VSTM: imm8 is the count of registers * 2 for double */
  if (insn->is_double) {
    regs = count / 2; /* Each double is 2 words */
    bytes = count * 4;
  } else {
    regs = count;
    bytes = count * 4;
  }

  uint32_t addr = base;

  /* Pre-index adjustment */
  if (!insn->add) {
    addr -= bytes;
  }

  uint8_t start_reg = insn->is_double ? insn->dd : insn->sd;

  for (uint32_t i = 0; i < regs; i++) {
    if (fpu_multi_xfer_one(exec, insn, i, start_reg, &addr, is_load)) {
      return ARMV8M_ERR_BUS_FAULT;
    }
  }

  /* Writeback */
  if (insn->writeback) {
    uint32_t new_base = insn->add ? (base + bytes) : (base - bytes);
    set_reg(exec, insn->rn, new_base);
  }

  return ARMV8M_OK;
}

/*============================================================================
 * FPU Move Instructions
 *============================================================================*/

static void fpu_vmov_imm(CPUState *cpu, const DecodedInsn *insn) {
  /* VMOV (immediate) - expand imm8 to float */
  uint8_t a = (uint8_t)((insn->imm >> 7) & 1);
  uint8_t b = (uint8_t)((insn->imm >> 6) & 1);
  uint8_t bcdefgh = (uint8_t)(insn->imm & 0x3F);

  if (insn->is_double) {
    /* F64: aBbbbbbb bbcdefgh 00000000 00000000 00000000 00000000 00000000
     * 00000000 */
    uint64_t exp = b ? 0x3FCULL : 0x400ULL;
    exp |= (uint64_t)(b ? 0U : 3U) << 8;
    uint64_t imm64 =
        ((uint64_t)a << 63) | (exp << 52) | ((uint64_t)bcdefgh << 48);
    union {
      uint64_t i;
      double f;
    } u;
    u.i = imm64;
    set_d(cpu, insn->dd, u.f);
  } else {
    /* F32: aBbbbbbc defgh000 00000000 00000000 */
    uint32_t exp = b ? 0x7C : 0x80;
    uint32_t imm32 =
        ((uint32_t)a << 31) | ((uint32_t)exp << 23) | ((uint32_t)bcdefgh << 19);
    union {
      uint32_t i;
      float f;
    } u;
    u.i = imm32;
    set_s(cpu, insn->sd, u.f);
  }
}

static void fpu_vmov_arm(Executor *exec, const DecodedInsn *insn) {
  CPUState *cpu = &exec->cpu;
  /* VMOV (between ARM and single FPU reg) or VMRS/VMSR */
  if (insn->sysreg == 0x80) {
    /* VMRS/VMSR - transfer FPSCR */
    if (insn->add) {
      /* VMRS: to ARM register */
      if (insn->rd == 15) {
        /* Transfer FPSCR flags to APSR */
        cpu->xpsr = (cpu->xpsr & 0x0FFFFFFF) | (cpu->fpscr & 0xF0000000);
      } else {
        set_reg(exec, insn->rd, cpu->fpscr);
      }
    } else {
      /* VMSR: from ARM register */
      cpu->fpscr = get_reg(exec, insn->rd);
    }
  } else {
    /* VMOV between ARM reg and FPU Sn */
    if (insn->add) {
      /* From FPU to ARM */
      set_reg(exec, insn->rd, cpu->s[insn->sn & 31]);
    } else {
      /* From ARM to FPU */
      cpu->s[insn->sn & 31] = get_reg(exec, insn->rd);
    }
  }
}

int exec_fpu_move(Executor *exec, const DecodedInsn *insn) {
  int prologue = fpu_prologue(exec);
  if (prologue != ARMV8M_OK) {
    return prologue;
  }
  CPUState *cpu = &exec->cpu;

  switch (insn->op) {
  case VFP_VMOV_IMM:
    fpu_vmov_imm(cpu, insn);
    break;

  case VFP_VMOV_REG:
    /* VMOV (register) */
    if (insn->is_double) {
      set_d(cpu, insn->dd, get_d(cpu, insn->dm));
    } else {
      set_s(cpu, insn->sd, get_s(cpu, insn->sm));
    }
    break;

  case VFP_VMOV_ARM:
    fpu_vmov_arm(exec, insn);
    break;

  case VFP_VMOV_2ARM:
    /* VMOV (between two ARM regs and FPU Dm) */
    if (insn->add) {
      /* From FPU to ARM regs */
      uint8_t s_base = (uint8_t)((insn->dm & 15) * 2);
      set_reg(exec, insn->rd, cpu->s[s_base]);
      set_reg(exec, insn->rt, cpu->s[s_base + 1]);
    } else {
      /* From ARM regs to FPU */
      uint8_t s_base = (uint8_t)((insn->dm & 15) * 2);
      cpu->s[s_base] = get_reg(exec, insn->rd);
      cpu->s[s_base + 1] = get_reg(exec, insn->rt);
    }
    break;

  default:
    return ARMV8M_ERR_UNDEFINED_INSN;
  }

  return ARMV8M_OK;
}

/*============================================================================
 * FPU Arithmetic Instructions
 *============================================================================*/

int exec_fpu_arith(Executor *exec, const DecodedInsn *insn) {
  int prologue = fpu_prologue(exec);
  if (prologue != ARMV8M_OK) {
    return prologue;
  }
  CPUState *cpu = &exec->cpu;

  /* Apply FPSCR rounding mode for operations that use it */
  set_rounding_mode(cpu->fpscr);

  if (insn->is_double) {
    double dn = get_d(cpu, insn->dn);
    double dm = get_d(cpu, insn->dm);
    double dd = get_d(cpu, insn->dd);
    double result;

    switch (insn->op) {
    case VFP_VMLA:
      result = dd + (dn * dm);
      break;
    case VFP_VMLS:
      result = dd - (dn * dm);
      break;
    case VFP_VNMLA:
      result = -dd - (dn * dm);
      break;
    case VFP_VNMLS:
      result = -dd + (dn * dm);
      break;
    case VFP_VNMUL:
      result = -(dn * dm);
      break;
    case VFP_VMUL:
      result = dn * dm;
      break;
    case VFP_VADD:
      result = dn + dm;
      break;
    case VFP_VSUB:
      result = dn - dm;
      break;
    case VFP_VDIV:
      result = dn / dm;
      break;
    case VFP_VABS:
      result = fabs(dm);
      break;
    case VFP_VNEG:
      result = -dm;
      break;
    case VFP_VSQRT:
      result = sqrt(dm);
      break;
    case VFP_VFMA:
      result = fma(dn, dm, dd); /* Fused: dd + (dn * dm) */
      break;
    case VFP_VFMS:
      result = fma(-dn, dm, dd); /* Fused: dd - (dn * dm) */
      break;
    case VFP_VFNMA:
      result = -fma(dn, dm, dd); /* Fused: -(dd + (dn * dm)) */
      break;
    case VFP_VFNMS:
      result = fma(dn, dm, -dd); /* Fused: -dd + (dn * dm) */
      break;
    default:
      return ARMV8M_ERR_UNDEFINED_INSN;
    }

    set_d(cpu, insn->dd, result);
  } else {
    float sn = get_s(cpu, insn->sn);
    float sm = get_s(cpu, insn->sm);
    float sd = get_s(cpu, insn->sd);
    float result;

    switch (insn->op) {
    case VFP_VMLA:
      result = sd + (sn * sm);
      break;
    case VFP_VMLS:
      result = sd - (sn * sm);
      break;
    case VFP_VNMLA:
      result = -sd - (sn * sm);
      break;
    case VFP_VNMLS:
      result = -sd + (sn * sm);
      break;
    case VFP_VNMUL:
      result = -(sn * sm);
      break;
    case VFP_VMUL:
      result = sn * sm;
      break;
    case VFP_VADD:
      result = sn + sm;
      break;
    case VFP_VSUB:
      result = sn - sm;
      break;
    case VFP_VDIV:
      result = sn / sm;
      break;
    case VFP_VABS:
      result = fabsf(sm);
      break;
    case VFP_VNEG:
      result = -sm;
      break;
    case VFP_VSQRT:
      result = sqrtf(sm);
      break;
    case VFP_VFMA:
      result = fmaf(sn, sm, sd); /* Fused: sd + (sn * sm) */
      break;
    case VFP_VFMS:
      result = fmaf(-sn, sm, sd); /* Fused: sd - (sn * sm) */
      break;
    case VFP_VFNMA:
      result = -fmaf(sn, sm, sd); /* Fused: -(sd + (sn * sm)) */
      break;
    case VFP_VFNMS:
      result = fmaf(sn, sm, -sd); /* Fused: -sd + (sn * sm) */
      break;
    default:
      return ARMV8M_ERR_UNDEFINED_INSN;
    }

    set_s(cpu, insn->sd, result);
  }

  /* Restore default rounding mode */
  restore_rounding_mode();

  return ARMV8M_OK;
}

/*============================================================================
 * FPU Compare Instructions
 *============================================================================*/

int exec_fpu_cmp(Executor *exec, const DecodedInsn *insn) {
  int prologue = fpu_prologue(exec);
  if (prologue != ARMV8M_OK) {
    return prologue;
  }
  CPUState *cpu = &exec->cpu;

  bool is_nan = false;
  int cmp_result = 0;

  if (insn->is_double) {
    /* VCMP compares Dd with Dm (not Dn) */
    double dd = get_d(cpu, insn->dd);
    double dm = (insn->dm == ARMV8M_REG_NONE) ? 0.0 : get_d(cpu, insn->dm);

    if (isnan(dd) || isnan(dm)) {
      is_nan = true;
    } else if (dd < dm) {
      cmp_result = -1;
    } else if (dd > dm) {
      cmp_result = 1;
    } else {
      cmp_result = 0;
    }
  } else {
    /* VCMP compares Sd with Sm (not Sn) */
    float sd = get_s(cpu, insn->sd);
    float sm = (insn->sm == ARMV8M_REG_NONE) ? 0.0f : get_s(cpu, insn->sm);

    if (isnan(sd) || isnan(sm)) {
      is_nan = true;
    } else if (sd < sm) {
      cmp_result = -1;
    } else if (sd > sm) {
      cmp_result = 1;
    } else {
      cmp_result = 0;
    }
  }

  update_fpscr_cmp(cpu, cmp_result, is_nan);

  return ARMV8M_OK;
}

/*============================================================================
 * FPU Conversion Instructions
 *============================================================================*/

static void fpu_cvt_to_int(CPUState *cpu, const DecodedInsn *insn) {
  bool to_signed = ((insn->op & 0x1) == 0);
  bool truncate = ((insn->op & 0x8) != 0);
  int32_t result;

  if (truncate) {
    /* VCVT: Round toward zero (truncation) - C cast does this */
  } else {
    /* VCVTR: Use FPSCR rounding mode */
    set_rounding_mode(cpu->fpscr);
  }

  if (insn->is_double) {
    double dm = get_d(cpu, insn->dm);
#if HAVE_FENV
    if (!truncate) {
      dm = nearbyint(dm); /* Round according to FPSCR mode */
    }
#endif
    if (to_signed) {
      result = (int32_t)dm;
    } else {
      result = (int32_t)(uint32_t)dm;
    }
  } else {
    float sm = get_s(cpu, insn->sm);
#if HAVE_FENV
    if (!truncate) {
      sm = nearbyintf(sm); /* Round according to FPSCR mode */
    }
#endif
    if (to_signed) {
      result = (int32_t)sm;
    } else {
      result = (int32_t)(uint32_t)sm;
    }
  }
  cpu->s[insn->sd & 31] = (uint32_t)result;

  if (!truncate) {
    restore_rounding_mode();
  }
}

static void fpu_cvt_to_float(CPUState *cpu, const DecodedInsn *insn) {
  bool from_signed = ((insn->op & 0x1) == 0);
  uint32_t sm_raw = cpu->s[insn->sm & 31];

  if (insn->is_double) {
    double result;
    if (from_signed) {
      result = (double)(int32_t)sm_raw;
    } else {
      result = (double)sm_raw;
    }
    set_d(cpu, insn->dd, result);
  } else {
    float result;
    if (from_signed) {
      result = (float)(int32_t)sm_raw;
    } else {
      result = (float)sm_raw;
    }
    set_s(cpu, insn->sd, result);
  }
}

static void fpu_cvt_fixed_to_float(CPUState *cpu, const DecodedInsn *insn) {
  /* The imm field contains the number of fraction bits (fbits).
   * For VCVT with <fbits> encoded as (32 - imm).
   * Value = integer / 2^fbits */
  uint8_t fbits = (uint8_t)(32 - insn->imm);
  if (fbits == 0 || fbits > 32) {
    fbits = 16; /* Default fallback */
  }
  uint32_t raw = cpu->s[insn->sm & 31];
  float scale = (float)(1U << fbits);

  /* Apply rounding mode for this conversion */
  set_rounding_mode(cpu->fpscr);

  if (insn->is_double) {
    double result;
    if (insn->is_signed) {
      result = (double)(int32_t)raw / (double)(1U << fbits);
    } else {
      result = (double)raw / (double)(1U << fbits);
    }
    set_d(cpu, insn->dd, result);
  } else {
    float result;
    if (insn->is_signed) {
      result = (float)(int32_t)raw / scale;
    } else {
      result = (float)raw / scale;
    }
    set_s(cpu, insn->sd, result);
  }

  restore_rounding_mode();
}

static void fpu_cvt_to_fixed(CPUState *cpu, const DecodedInsn *insn) {
  /* The imm field contains the number of fraction bits (fbits).
   * Result = integer part of (float_value * 2^fbits) */
  uint8_t fbits = (uint8_t)(32 - insn->imm);
  if (fbits == 0 || fbits > 32) {
    fbits = 16; /* Default fallback */
  }

  /* Apply rounding mode for this conversion */
  set_rounding_mode(cpu->fpscr);

  int32_t result;
  if (insn->is_double) {
    double dm = get_d(cpu, insn->dm);
    double scaled = dm * (double)(1U << fbits);
    if (insn->is_signed) {
      result = (int32_t)scaled;
    } else {
      result = (int32_t)(uint32_t)scaled;
    }
  } else {
    float sm = get_s(cpu, insn->sm);
    float scaled = sm * (float)(1U << fbits);
    if (insn->is_signed) {
      result = (int32_t)scaled;
    } else {
      result = (int32_t)(uint32_t)scaled;
    }
  }
  cpu->s[insn->sd & 31] = (uint32_t)result;

  restore_rounding_mode();
}

int exec_fpu_cvt(Executor *exec, const DecodedInsn *insn) {
  int prologue = fpu_prologue(exec);
  if (prologue != ARMV8M_OK) {
    return prologue;
  }
  CPUState *cpu = &exec->cpu;

  /* The op field encodes the conversion type */
  switch (insn->op & 0x7) {
  case 0: /* F32 <-> F64 conversions based on direction */
    if (insn->is_double) {
      /* VCVT.F64.F32: Convert F32 to F64 */
      float sm = get_s(cpu, insn->sm);
      set_d(cpu, insn->dd, (double)sm);
    } else {
      /* VCVT.F32.F64: Convert F64 to F32 */
      double dm = get_d(cpu, insn->dm);
      set_s(cpu, insn->sd, (float)dm);
    }
    break;

  case 2:  /* VCVTR.S32: Float to signed int (FPSCR rounding) */
  case 3:  /* VCVTR.U32: Float to unsigned int (FPSCR rounding) */
  case 10: /* VCVT.S32: Float to signed int (truncate) */
  case 11: /* VCVT.U32: Float to unsigned int (truncate) */
    fpu_cvt_to_int(cpu, insn);
    break;

  case 4: /* VCVT.F32/F64.S32: Signed int to float */
  case 5: /* VCVT.F32/F64.U32: Unsigned int to float */
    fpu_cvt_to_float(cpu, insn);
    break;

  case 6: /* VCVT.F32.FX / VCVT.F64.FX: Fixed-point to float */
    fpu_cvt_fixed_to_float(cpu, insn);
    break;

  case 7: /* VCVT.FX.F32 / VCVT.FX.F64: Float to fixed-point */
    fpu_cvt_to_fixed(cpu, insn);
    break;

  default:
    return ARMV8M_ERR_UNDEFINED_INSN;
  }

  return ARMV8M_OK;
}
