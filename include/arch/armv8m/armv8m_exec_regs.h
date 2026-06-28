/**
 * @file armv8m_exec_regs.h
 * @brief Shared register access helpers for the ARMv8-M executor.
 *
 * The per-instruction executor source files (exec_data_proc.c,
 * exec_load_store.c, exec_system.c, exec_fpu.c, exec_branch.c) all need to read
 * and write CPU registers with the same SP/PC special-casing. These helpers
 * are defined as `static inline` here so each translation unit gets a private,
 * zero-cost copy without duplicating the logic (these are hot paths).
 *
 * Two read variants exist because reading PC is context-dependent in Thumb:
 *  - armv8m_exec_get_reg()    returns the raw PC. Used where the caller already
 *    accounts for the pipeline offset itself (load/store, system, FPU).
 *  - armv8m_exec_get_reg_pc() returns PC + 4 per the ARM spec, for instructions
 *    that use PC as a data operand (data processing, branch).
 *
 * Writes are identical everywhere, so there is a single armv8m_exec_set_reg().
 */

#ifndef ARMV8M_EXEC_REGS_H
#define ARMV8M_EXEC_REGS_H

#include "arch/armv8m/armv8m_executor.h"
#include "arch/armv8m/armv8m_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Read a register, returning the raw PC for register 15. */
static inline uint32_t armv8m_exec_get_reg(const Executor *exec, uint8_t reg) {
  if (reg == ARMV8M_REG_SP) {
    return armv8m_get_sp(&exec->cpu);
  }
  return exec->cpu.r[reg];
}

/** Read a register, returning PC + 4 for register 15 (ARM Thumb data reads). */
static inline uint32_t armv8m_exec_get_reg_pc(const Executor *exec, uint8_t reg) {
  if (reg == ARMV8M_REG_SP) {
    return armv8m_get_sp(&exec->cpu);
  }
  if (reg == ARMV8M_REG_PC) {
    /* ARM Thumb: reading PC returns current instruction + 4 */
    return exec->cpu.r[ARMV8M_REG_PC] + 4;
  }
  return exec->cpu.r[reg];
}

/** Write a register, routing SP through the banked-SP helper and aligning PC. */
static inline void armv8m_exec_set_reg(Executor *exec, uint8_t reg, uint32_t value) {
  if (reg == ARMV8M_REG_SP) {
    armv8m_set_sp(&exec->cpu, value);
    exec->cpu.r[ARMV8M_REG_SP] = armv8m_get_sp(&exec->cpu);
  } else if (reg == ARMV8M_REG_PC) {
    /* PC writes must be halfword-aligned (Thumb) */
    exec->cpu.r[ARMV8M_REG_PC] = value & ~1U;
  } else {
    exec->cpu.r[reg] = value;
  }
}

#ifdef __cplusplus
}
#endif

#endif /* ARMV8M_EXEC_REGS_H */
