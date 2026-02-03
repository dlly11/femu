/**
 * @file armv8m_cpu.c
 * @brief ARMv8-M CPU vtable implementation
 *
 * Implements the abstract EmuCPU interface for ARMv8-M architecture.
 */

#include "arch/armv8m/armv8m_cpu.h"
#include <string.h>

/*============================================================================
 * Static CPU Info
 *============================================================================*/

static const EmuRegisterDesc armv8m_registers[] = {
    {"r0", 0, 4, 0},    {"r1", 1, 4, 1},     {"r2", 2, 4, 2},
    {"r3", 3, 4, 3},    {"r4", 4, 4, 4},     {"r5", 5, 4, 5},
    {"r6", 6, 4, 6},    {"r7", 7, 4, 7},     {"r8", 8, 4, 8},
    {"r9", 9, 4, 9},    {"r10", 10, 4, 10},  {"r11", 11, 4, 11},
    {"r12", 12, 4, 12}, {"sp", 13, 4, 13},   {"lr", 14, 4, 14},
    {"pc", 15, 4, 15},  {"xpsr", 16, 4, 25}, /* GDB uses 25 for xPSR */
};

static const EmuCPUInfo armv8m_cpu_info = {
    .arch = EMU_ARCH_ARMV8M,
    .arch_name = "ARMv8-M",
    .num_gp_regs = 16,
    .addr_bits = 32,
    .reg_bits = 32,
    .regs = armv8m_registers,
};

/*============================================================================
 * VTable Implementation
 *============================================================================*/

static void armv8m_cpu_destroy(EmuCPU *cpu) {
  /* We don't own the executor, so nothing to free */
  if (cpu) {
    ARMv8MCPU *arm_cpu = armv8m_cpu_from_base(cpu);
    arm_cpu->exec = NULL;
  }
}

static void armv8m_cpu_reset(EmuCPU *cpu, uint64_t entry_point) {
  if (!cpu) {
    return;
  }
  ARMv8MCPU *arm_cpu = armv8m_cpu_from_base(cpu);
  if (arm_cpu->exec) {
    armv8m_exec_reset(arm_cpu->exec, (uint32_t)entry_point);
  }
}

static const EmuCPUInfo *armv8m_cpu_get_info(const EmuCPU *cpu) {
  (void)cpu;
  return &armv8m_cpu_info;
}

static uint64_t armv8m_cpu_get_reg(const EmuCPU *cpu, int reg) {
  if (!cpu) {
    return 0;
  }
  const ARMv8MCPU *arm_cpu = armv8m_cpu_from_base_const(cpu);
  if (!arm_cpu->exec || reg < 0 || reg >= ARMV8M_NUM_REGS) {
    return 0;
  }
  return arm_cpu->exec->cpu.r[reg];
}

static void armv8m_cpu_set_reg(EmuCPU *cpu, int reg, uint64_t value) {
  if (!cpu) {
    return;
  }
  ARMv8MCPU *arm_cpu = armv8m_cpu_from_base(cpu);
  if (!arm_cpu->exec || reg < 0 || reg >= ARMV8M_NUM_REGS) {
    return;
  }
  arm_cpu->exec->cpu.r[reg] = (uint32_t)value;
}

static uint64_t armv8m_cpu_get_pc(const EmuCPU *cpu) {
  if (!cpu) {
    return 0;
  }
  const ARMv8MCPU *arm_cpu = armv8m_cpu_from_base_const(cpu);
  if (!arm_cpu->exec) {
    return 0;
  }
  return arm_cpu->exec->cpu.r[ARMV8M_REG_PC];
}

static void armv8m_cpu_set_pc(EmuCPU *cpu, uint64_t value) {
  if (!cpu) {
    return;
  }
  ARMv8MCPU *arm_cpu = armv8m_cpu_from_base(cpu);
  if (!arm_cpu->exec) {
    return;
  }
  arm_cpu->exec->cpu.r[ARMV8M_REG_PC] = (uint32_t)value;
}

static uint64_t armv8m_cpu_get_status(const EmuCPU *cpu) {
  if (!cpu) {
    return 0;
  }
  const ARMv8MCPU *arm_cpu = armv8m_cpu_from_base_const(cpu);
  if (!arm_cpu->exec) {
    return 0;
  }
  return arm_cpu->exec->cpu.xpsr;
}

static void armv8m_cpu_set_status(EmuCPU *cpu, uint64_t value) {
  if (!cpu) {
    return;
  }
  ARMv8MCPU *arm_cpu = armv8m_cpu_from_base(cpu);
  if (!arm_cpu->exec) {
    return;
  }
  arm_cpu->exec->cpu.xpsr = (uint32_t)value;
}

static uint64_t armv8m_cpu_get_cycles(const EmuCPU *cpu) {
  if (!cpu) {
    return 0;
  }
  const ARMv8MCPU *arm_cpu = armv8m_cpu_from_base_const(cpu);
  if (!arm_cpu->exec) {
    return 0;
  }
  return arm_cpu->exec->cpu.cycles;
}

static bool armv8m_cpu_is_halted(const EmuCPU *cpu) {
  if (!cpu) {
    return true;
  }
  const ARMv8MCPU *arm_cpu = armv8m_cpu_from_base_const(cpu);
  if (!arm_cpu->exec) {
    return true;
  }
  return arm_cpu->exec->cpu.halted;
}

static uint64_t armv8m_cpu_get_special_reg(const EmuCPU *cpu, int reg_id) {
  if (!cpu) {
    return 0;
  }
  const ARMv8MCPU *arm_cpu = armv8m_cpu_from_base_const(cpu);
  if (!arm_cpu->exec) {
    return 0;
  }

  const CPUState *state = &arm_cpu->exec->cpu;

  switch (reg_id) {
  case ARMV8M_CPU_SREG_MSP:
    return state->sp_main;
  case ARMV8M_CPU_SREG_PSP:
    return state->sp_process;
  case ARMV8M_CPU_SREG_PRIMASK:
    return state->primask;
  case ARMV8M_CPU_SREG_BASEPRI:
    return state->basepri;
  case ARMV8M_CPU_SREG_FAULTMASK:
    return state->faultmask;
  case ARMV8M_CPU_SREG_CONTROL:
    return state->control;
  case ARMV8M_CPU_SREG_MSPLIM:
    return state->msplim;
  case ARMV8M_CPU_SREG_PSPLIM:
    return state->psplim;
  case ARMV8M_CPU_SREG_FPSCR:
    return state->fpscr;
  default:
    /* FPU S registers */
    if (reg_id >= ARMV8M_CPU_SREG_S0 && reg_id < ARMV8M_CPU_SREG_S0 + 32) {
      return state->s[reg_id - ARMV8M_CPU_SREG_S0];
    }
    return 0;
  }
}

static void armv8m_cpu_set_special_reg(EmuCPU *cpu, int reg_id,
                                       uint64_t value) {
  if (!cpu) {
    return;
  }
  ARMv8MCPU *arm_cpu = armv8m_cpu_from_base(cpu);
  if (!arm_cpu->exec) {
    return;
  }

  CPUState *state = &arm_cpu->exec->cpu;

  switch (reg_id) {
  case ARMV8M_CPU_SREG_MSP:
    state->sp_main = (uint32_t)value;
    break;
  case ARMV8M_CPU_SREG_PSP:
    state->sp_process = (uint32_t)value;
    break;
  case ARMV8M_CPU_SREG_PRIMASK:
    state->primask = (uint32_t)value;
    break;
  case ARMV8M_CPU_SREG_BASEPRI:
    state->basepri = (uint32_t)value;
    break;
  case ARMV8M_CPU_SREG_FAULTMASK:
    state->faultmask = (uint32_t)value;
    break;
  case ARMV8M_CPU_SREG_CONTROL:
    state->control = (uint32_t)value;
    break;
  case ARMV8M_CPU_SREG_MSPLIM:
    state->msplim = (uint32_t)value;
    break;
  case ARMV8M_CPU_SREG_PSPLIM:
    state->psplim = (uint32_t)value;
    break;
  case ARMV8M_CPU_SREG_FPSCR:
    state->fpscr = (uint32_t)value;
    break;
  default:
    /* FPU S registers */
    if (reg_id >= ARMV8M_CPU_SREG_S0 && reg_id < ARMV8M_CPU_SREG_S0 + 32) {
      state->s[reg_id - ARMV8M_CPU_SREG_S0] = (uint32_t)value;
    }
    break;
  }
}

/*============================================================================
 * Static VTable
 *============================================================================*/

static const EmuCPUVTable armv8m_cpu_vtable = {
    .destroy = armv8m_cpu_destroy,
    .reset = armv8m_cpu_reset,
    .get_info = armv8m_cpu_get_info,
    .get_reg = armv8m_cpu_get_reg,
    .set_reg = armv8m_cpu_set_reg,
    .get_pc = armv8m_cpu_get_pc,
    .set_pc = armv8m_cpu_set_pc,
    .get_status = armv8m_cpu_get_status,
    .set_status = armv8m_cpu_set_status,
    .get_cycles = armv8m_cpu_get_cycles,
    .is_halted = armv8m_cpu_is_halted,
    .get_special_reg = armv8m_cpu_get_special_reg,
    .set_special_reg = armv8m_cpu_set_special_reg,
};

/*============================================================================
 * Public API
 *============================================================================*/

const EmuCPUVTable *armv8m_cpu_get_vtable(void) { return &armv8m_cpu_vtable; }

const EmuCPUInfo *armv8m_cpu_get_info_static(void) { return &armv8m_cpu_info; }

void armv8m_cpu_init(ARMv8MCPU *cpu, Executor *exec) {
  if (!cpu) {
    return;
  }

  memset(cpu, 0, sizeof(*cpu));
  cpu->base.vtable = &armv8m_cpu_vtable;
  cpu->base.arch_state = exec;
  cpu->exec = exec;
}
