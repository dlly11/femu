/**
 * @file exec_branch.c
 * @brief Branch instruction execution for ARMv8-M
 *
 * Implements B, BL, BX, BLX, CBZ, CBNZ, TBB, TBH.
 */

#include "arch/armv8m/armv8m_exec_regs.h"
#include "arch/armv8m/armv8m_executor.h"
#include "arch/armv8m/armv8m_types.h"
#include "emu/emu_log.h"
#include "exec_handlers.h"

/*============================================================================
 * Internal Helpers
 *============================================================================*/

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

/*
 * Register access for branch instructions. Reading PC yields PC + 4 (the ARM
 * Thumb data-operand value), so use the _pc read variant. Shared
 * implementations live in armv8m_exec_regs.h.
 */
static inline uint32_t get_reg(const Executor *exec, uint8_t reg) {
  return armv8m_exec_get_reg_pc(exec, reg);
}

/**
 * Check if value is an EXC_RETURN magic value.
 * EXC_RETURN values have the form 0xFFxxxxxx.
 */
static bool is_exc_return(uint32_t value) {
  return (value & 0xFF000000) == 0xFF000000;
}

/**
 * Check if value is FNC_RETURN (Function Return from Non-Secure).
 * FNC_RETURN = 0xFEFFFFFF
 */
static bool is_fnc_return(uint32_t value) { return value == 0xFEFFFFFF; }

/* Forward declaration for FNC_RETURN handler */
static int exec_fnc_return(Executor *exec);

/*============================================================================
 * Branch Instructions
 *============================================================================*/

int exec_branch(Executor *exec, const DecodedInsn *insn) {
  /* B <label> - PC-relative branch */
  uint32_t pc = exec->cpu.r[ARMV8M_REG_PC];

  /* Calculate target: PC + 4 + offset (PC is already pointing to current insn)
   */
  uint32_t target = (uint32_t)((int32_t)pc + 4 + insn->branch_offset);

  EMU_LOG_TRACE(EMU_LOG_CAT_EXECUTOR, "B 0x%08X -> 0x%08X", pc, target & ~1u);

  /* Thumb mode requires bit 0 to be 0 for execution */
  exec->cpu.r[ARMV8M_REG_PC] = target & ~1u;

  return ARMV8M_OK;
}

int exec_branch_link(Executor *exec, const DecodedInsn *insn) {
  /* BL <label> - Branch with link */
  uint32_t pc = exec->cpu.r[ARMV8M_REG_PC];

  /* Save return address in LR (PC + 4 with Thumb bit set) */
  exec->cpu.r[ARMV8M_REG_LR] = (pc + insn->size) | 1;

  /* Calculate target */
  uint32_t target = (uint32_t)((int32_t)pc + 4 + insn->branch_offset);

  EMU_LOG_TRACE(EMU_LOG_CAT_EXECUTOR, "BL 0x%08X -> 0x%08X (LR=0x%08X)", pc,
                target & ~1u, exec->cpu.r[ARMV8M_REG_LR]);

  exec->cpu.r[ARMV8M_REG_PC] = target & ~1u;

  return ARMV8M_OK;
}

int exec_branch_exchange(Executor *exec, const DecodedInsn *insn) {
  /* BX Rm - Branch and exchange (to address in register) */
  uint32_t target = get_reg(exec, insn->rm);

  /* Check for exception return */
  if (is_exc_return(target)) {
    EMU_LOG_DEBUG(EMU_LOG_CAT_EXECUTOR, "BX EXC_RETURN 0x%08X", target);
    return armv8m_exception_return(exec, target);
  }

  /* Check for FNC_RETURN (TrustZone function return) */
  if (is_fnc_return(target) && exec->has_trustzone) {
    EMU_LOG_DEBUG(EMU_LOG_CAT_EXECUTOR, "BX FNC_RETURN");
    return exec_fnc_return(exec);
  }

  /* In Thumb mode, bit 0 indicates Thumb state (must be 1) */
  if (!(target & 1)) {
    /* Attempting to switch to ARM state is UsageFault on M-profile */
    EMU_LOG_WARNING(EMU_LOG_CAT_EXECUTOR,
                    "BX to ARM state (target=0x%08X) - UsageFault", target);
    return ARMV8M_ERR_USAGE_FAULT;
  }

  EMU_LOG_TRACE(EMU_LOG_CAT_EXECUTOR, "BX R%d -> 0x%08X", insn->rm,
                target & ~1u);
  exec->cpu.r[ARMV8M_REG_PC] = target & ~1u;

  return ARMV8M_OK;
}

int exec_branch_link_exchange(Executor *exec, const DecodedInsn *insn) {
  /* BLX Rm - Branch with link and exchange */
  uint32_t pc = exec->cpu.r[ARMV8M_REG_PC];
  uint32_t target = get_reg(exec, insn->rm);

  /* Save return address in LR */
  exec->cpu.r[ARMV8M_REG_LR] = (pc + insn->size) | 1;

  /* Check for exception return (unusual but possible) */
  if (is_exc_return(target)) {
    EMU_LOG_DEBUG(EMU_LOG_CAT_EXECUTOR, "BLX EXC_RETURN 0x%08X", target);
    return armv8m_exception_return(exec, target);
  }

  /* In Thumb mode, bit 0 indicates Thumb state (must be 1) */
  if (!(target & 1)) {
    EMU_LOG_WARNING(EMU_LOG_CAT_EXECUTOR,
                    "BLX to ARM state (target=0x%08X) - UsageFault", target);
    return ARMV8M_ERR_USAGE_FAULT;
  }

  EMU_LOG_TRACE(EMU_LOG_CAT_EXECUTOR, "BLX R%d -> 0x%08X (LR=0x%08X)", insn->rm,
                target & ~1u, exec->cpu.r[ARMV8M_REG_LR]);
  exec->cpu.r[ARMV8M_REG_PC] = target & ~1u;

  return ARMV8M_OK;
}

int exec_compare_branch(Executor *exec, const DecodedInsn *insn) {
  /* CBZ/CBNZ Rn, <label> - Compare and branch if (not) zero */
  uint32_t rn_val = get_reg(exec, insn->rn);
  uint32_t pc = exec->cpu.r[ARMV8M_REG_PC];

  bool take_branch;
  if (insn->op == 0) {
    /* CBZ: branch if Rn == 0 */
    take_branch = (rn_val == 0);
  } else {
    /* CBNZ: branch if Rn != 0 */
    take_branch = (rn_val != 0);
  }

  if (take_branch) {
    uint32_t target = (uint32_t)((int32_t)pc + 4 + insn->branch_offset);
    EMU_LOG_TRACE(EMU_LOG_CAT_EXECUTOR, "%s R%d=0x%X -> 0x%08X (taken)",
                  insn->op == 0 ? "CBZ" : "CBNZ", insn->rn, rn_val,
                  target & ~1u);
    exec->cpu.r[ARMV8M_REG_PC] = target & ~1u;
  } else {
    EMU_LOG_TRACE(EMU_LOG_CAT_EXECUTOR, "%s R%d=0x%X (not taken)",
                  insn->op == 0 ? "CBZ" : "CBNZ", insn->rn, rn_val);
  }

  return ARMV8M_OK;
}

int exec_table_branch(Executor *exec, const DecodedInsn *insn) {
  /* TBB/TBH - Table branch byte/halfword */
  uint32_t base = get_reg(exec, insn->rn);
  uint32_t index = get_reg(exec, insn->rm);
  uint32_t pc = exec->cpu.r[ARMV8M_REG_PC];
  bool fault = false;

  uint32_t offset;

  if (insn->access_size == ACCESS_BYTE) {
    /* TBB: offset = mem[Rn + Rm] */
    uint32_t addr = base + index;
    offset = mem_read(exec, addr, ACCESS_BYTE, &fault);
  } else {
    /* TBH: offset = mem[Rn + Rm*2] */
    uint32_t addr = base + (index * 2);
    offset = mem_read(exec, addr, ACCESS_HALF, &fault);
  }

  if (fault) {
    EMU_LOG_WARNING(EMU_LOG_CAT_EXECUTOR, "%s fault reading table",
                    insn->access_size == ACCESS_BYTE ? "TBB" : "TBH");
    return ARMV8M_ERR_BUS_FAULT;
  }

  /* Branch target = (PC+4) + 2*offset
   * Per ARM spec, target is always relative to PC+4 */
  uint32_t target = pc + 4 + (offset * 2);
  EMU_LOG_TRACE(EMU_LOG_CAT_EXECUTOR, "%s [R%d+R%d*%d] offset=%u -> 0x%08X",
                insn->access_size == ACCESS_BYTE ? "TBB" : "TBH", insn->rn,
                insn->rm, insn->access_size == ACCESS_BYTE ? 1 : 2, offset,
                target & ~1u);
  exec->cpu.r[ARMV8M_REG_PC] = target & ~1u;

  return ARMV8M_OK;
}

/*============================================================================
 * TrustZone Branch Instructions
 *============================================================================*/

/**
 * Clear caller-saved registers for security when transitioning S->NS.
 * Per ARM ARMv8-M: R0-R3, R12 must be cleared, and APSR flags.
 * This prevents information leakage from Secure to Non-Secure code.
 */
static void clear_caller_saved_regs(CPUState *cpu) {
  cpu->r[0] = 0;
  cpu->r[1] = 0;
  cpu->r[2] = 0;
  cpu->r[3] = 0;
  cpu->r[12] = 0;
  /* Clear APSR flags (N, Z, C, V, Q) */
  cpu->xpsr &= ~(ARMV8M_XPSR_N | ARMV8M_XPSR_Z | ARMV8M_XPSR_C | ARMV8M_XPSR_V |
                 ARMV8M_XPSR_Q);
}

/**
 * SG security handling: when non-secure and executing from an NSC region,
 * transition to secure; a NOP (logs) when already secure. Returns ARMV8M_OK or
 * a SecureFault.
 */
static int sg_handle_security(Executor *exec) {
  if (exec->cpu.security != SECURITY_NONSECURE) {
    EMU_LOG_TRACE(EMU_LOG_CAT_EXECUTOR, "SG (NOP in secure state)");
    return ARMV8M_OK;
  }
  uint32_t pc = exec->cpu.r[ARMV8M_REG_PC];
  if (armv8m_check_security(exec, pc) != SEC_NSC) {
    /* SG not in NSC region - SecureFault (INVTRAN) */
    EMU_LOG_WARNING(EMU_LOG_CAT_EXECUTOR,
                    "SG at 0x%08X not in NSC region - SecureFault", pc);
    return ARMV8M_ERR_SECURE_FAULT;
  }
  EMU_LOG_INFO(EMU_LOG_CAT_EXECUTOR, "SG: NS -> S transition at 0x%08X", pc);
  exec->cpu.security = SECURITY_SECURE;
  return ARMV8M_OK;
}

/**
 * BXNS security handling: when secure, validate the target and drop to
 * non-secure (clearing caller-saved registers); a no-op (logs) when already
 * non-secure. Returns ARMV8M_OK or a fault.
 */
static int bxns_handle_security(Executor *exec, const DecodedInsn *insn,
                                uint32_t target_addr) {
  if (exec->cpu.security != SECURITY_SECURE) {
    EMU_LOG_TRACE(EMU_LOG_CAT_EXECUTOR, "BXNS R%d -> 0x%08X (already NS)",
                  insn->rm, target_addr);
    return ARMV8M_OK;
  }
  if (armv8m_check_security(exec, target_addr) == SEC_SECURE) {
    /* Cannot branch to secure memory with BXNS from secure */
    EMU_LOG_WARNING(EMU_LOG_CAT_EXECUTOR,
                    "BXNS to secure address 0x%08X - SecureFault", target_addr);
    return ARMV8M_ERR_SECURE_FAULT;
  }
  clear_caller_saved_regs(&exec->cpu);
  EMU_LOG_INFO(EMU_LOG_CAT_EXECUTOR, "BXNS: S -> NS transition to 0x%08X",
               target_addr);
  exec->cpu.security = SECURITY_NONSECURE;
  return ARMV8M_OK;
}

int exec_sg(Executor *exec, const DecodedInsn *insn) {
  (void)insn; /* SG has no operands */

  /* SG (Secure Gateway) is used to enter secure state from non-secure.
   * When called from non-secure state and the instruction is in an NSC region,
   * it transitions to secure state.
   * When called from secure state, it's a NOP. */

  if (!exec->has_trustzone) {
    /* No TrustZone - SG is undefined */
    EMU_LOG_WARNING(EMU_LOG_CAT_EXECUTOR, "SG without TrustZone - undefined");
    return ARMV8M_ERR_UNDEFINED_INSN;
  }

  return sg_handle_security(exec);
}

int exec_bxns(Executor *exec, const DecodedInsn *insn) {
  /* BXNS Rm - Branch and exchange to Non-secure state */

  if (!exec->has_trustzone) {
    /* No TrustZone - BXNS is undefined */
    EMU_LOG_WARNING(EMU_LOG_CAT_EXECUTOR, "BXNS without TrustZone - undefined");
    return ARMV8M_ERR_UNDEFINED_INSN;
  }

  uint32_t target = get_reg(exec, insn->rm);

  /* Clear bit 0 to determine if transition to NS */
  /* The LSB of the target indicates Thumb state, not security */
  uint32_t target_addr = target & ~1U;

  int sec = bxns_handle_security(exec, insn, target_addr);
  if (sec != ARMV8M_OK) {
    return sec;
  }

  /* Check Thumb bit */
  if (!(target & 1)) {
    EMU_LOG_WARNING(EMU_LOG_CAT_EXECUTOR, "BXNS to ARM state - UsageFault");
    return ARMV8M_ERR_USAGE_FAULT;
  }

  exec->cpu.r[ARMV8M_REG_PC] = target_addr;
  return ARMV8M_OK;
}

/**
 * Apply the BLXNS security transition. If running secure, validate the target,
 * push the return address to the secure stack, switch LR to FNC_RETURN, clear
 * caller-saved registers, and drop to non-secure. No-op (logs) when already
 * non-secure. Returns ARMV8M_OK or a fault to propagate.
 */
static int blxns_handle_security(Executor *exec, const DecodedInsn *insn,
                                 uint32_t pc, uint32_t target_addr) {
  if (exec->cpu.security != SECURITY_SECURE) {
    EMU_LOG_TRACE(EMU_LOG_CAT_EXECUTOR,
                  "BLXNS R%d -> 0x%08X (already NS, LR=0x%08X)", insn->rm,
                  target_addr, exec->cpu.r[ARMV8M_REG_LR]);
    return ARMV8M_OK;
  }

  if (armv8m_check_security(exec, target_addr) == SEC_SECURE) {
    /* Cannot branch to secure memory with BLXNS from secure */
    EMU_LOG_WARNING(EMU_LOG_CAT_EXECUTOR,
                    "BLXNS to secure address 0x%08X - SecureFault",
                    target_addr);
    return ARMV8M_ERR_SECURE_FAULT;
  }

  /* Save return address on secure stack before transitioning (popped on
   * FNC_RETURN). */
  exec->tz_regs.msp_s -= 4;
  bool fault = false;
  if (exec->mem.write) {
    exec->mem.write(exec->mem.ctx, exec->tz_regs.msp_s, (pc + insn->size) | 1,
                    ACCESS_WORD, &fault);
  }
  if (fault) {
    exec->tz_regs.msp_s += 4; /* Restore SP on failure */
    EMU_LOG_WARNING(EMU_LOG_CAT_EXECUTOR, "BLXNS stack push fault");
    return ARMV8M_ERR_BUS_FAULT;
  }

  /* Set LR to FNC_RETURN value for secure-to-NS function call */
  exec->cpu.r[ARMV8M_REG_LR] = 0xFEFFFFFFU;

  /* Clear caller-saved registers for security compliance */
  clear_caller_saved_regs(&exec->cpu);

  /* Transition to non-secure state */
  EMU_LOG_INFO(EMU_LOG_CAT_EXECUTOR,
               "BLXNS: S -> NS call to 0x%08X (LR=FNC_RETURN)", target_addr);
  exec->cpu.security = SECURITY_NONSECURE;
  return ARMV8M_OK;
}

int exec_blxns(Executor *exec, const DecodedInsn *insn) {
  /* BLXNS Rm - Branch with link and exchange to Non-secure state */

  if (!exec->has_trustzone) {
    /* No TrustZone - BLXNS is undefined */
    EMU_LOG_WARNING(EMU_LOG_CAT_EXECUTOR,
                    "BLXNS without TrustZone - undefined");
    return ARMV8M_ERR_UNDEFINED_INSN;
  }

  uint32_t pc = exec->cpu.r[ARMV8M_REG_PC];
  uint32_t target = get_reg(exec, insn->rm);
  uint32_t target_addr = target & ~1U;

  /* Save return address in LR with security state info */
  /* For non-secure callable returns, use FNC_RETURN magic value */
  exec->cpu.r[ARMV8M_REG_LR] = (pc + insn->size) | 1;

  int sec = blxns_handle_security(exec, insn, pc, target_addr);
  if (sec != ARMV8M_OK) {
    return sec;
  }

  /* Check Thumb bit */
  if (!(target & 1)) {
    EMU_LOG_WARNING(EMU_LOG_CAT_EXECUTOR, "BLXNS to ARM state - UsageFault");
    return ARMV8M_ERR_USAGE_FAULT;
  }

  exec->cpu.r[ARMV8M_REG_PC] = target_addr;
  return ARMV8M_OK;
}

/*============================================================================
 * FNC_RETURN Handler (TrustZone Function Return)
 *============================================================================*/

/**
 * Handle Function Return from Non-Secure to Secure state.
 *
 * When BLXNS was used to call NS code, LR was set to FNC_RETURN (0xFEFFFFFF).
 * On return via BX with FNC_RETURN:
 * 1. Verify we're in Non-Secure state
 * 2. Transition back to Secure state
 * 3. Pop return address from secure stack
 *
 * @param exec Executor context
 * @return ARMV8M_OK or error code
 */
static int exec_fnc_return(Executor *exec) {
  CPUState *cpu = &exec->cpu;

  /* FNC_RETURN only valid from Non-Secure state */
  if (cpu->security != SECURITY_NONSECURE) {
    /* Attempting FNC_RETURN from Secure state is a UsageFault */
    EMU_LOG_WARNING(EMU_LOG_CAT_EXECUTOR,
                    "FNC_RETURN from Secure state - UsageFault");
    exec->cpu.cfsr |= ARMV8M_UFSR_INVSTATE;
    return ARMV8M_ERR_USAGE_FAULT;
  }

  /* Transition to Secure state */
  cpu->security = SECURITY_SECURE;

  /* Pop return state from secure stack.
   * The BLXNS instruction saved the return address on the secure stack
   * before transitioning to NS. */
  bool fault = false;
  uint32_t sp = exec->tz_regs.msp_s;

  /* Read return address from secure stack */
  uint32_t ret_addr = mem_read(exec, sp, ACCESS_WORD, &fault);
  if (fault) {
    EMU_LOG_WARNING(EMU_LOG_CAT_EXECUTOR,
                    "FNC_RETURN stack read fault at 0x%08X", sp);
    return ARMV8M_ERR_BUS_FAULT;
  }

  /* Update secure MSP (pop 4 bytes) */
  exec->tz_regs.msp_s = sp + 4;

  /* Set PC to return address (clear Thumb bit for storage) */
  cpu->r[ARMV8M_REG_PC] = ret_addr & ~1U;

  EMU_LOG_INFO(EMU_LOG_CAT_EXECUTOR, "FNC_RETURN: NS -> S, returning to 0x%08X",
               ret_addr & ~1U);

  return ARMV8M_OK;
}
