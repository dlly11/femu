/**
 * @file armv8m_cpu.h
 * @brief ARMv8-M CPU adapter for abstract EmuCPU interface
 *
 * This file provides the ARMv8-M implementation of the abstract CPU interface,
 * wrapping the existing CPUState/Executor with the EmuCPU vtable pattern.
 */

#ifndef ARMV8M_CPU_H
#define ARMV8M_CPU_H

#include "emu/emu_cpu.h"
#include "arch/armv8m/armv8m_executor.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * ARMv8-M CPU Wrapper
 *============================================================================*/

/**
 * ARMv8-M specific CPU wrapper.
 *
 * Embeds EmuCPU as first member for safe casting.
 */
typedef struct {
    EmuCPU base;                /**< Base EmuCPU (must be first) */
    Executor *exec;             /**< Pointer to ARMv8-M executor */
} ARMv8MCPU;

/*============================================================================
 * ARMv8-M CPU API
 *============================================================================*/

/**
 * Get the vtable for ARMv8-M CPU.
 *
 * @return      Pointer to static vtable
 */
const EmuCPUVTable *armv8m_cpu_get_vtable(void);

/**
 * Get the CPU info for ARMv8-M.
 *
 * @return      Pointer to static CPU info
 */
const EmuCPUInfo *armv8m_cpu_get_info_static(void);

/**
 * Initialize ARMv8-M CPU wrapper.
 *
 * @param cpu       CPU wrapper to initialize
 * @param exec      Pointer to existing executor (not owned)
 */
void armv8m_cpu_init(ARMv8MCPU *cpu, Executor *exec);

/**
 * Get ARMv8-M CPU wrapper from EmuCPU pointer.
 *
 * @param cpu       EmuCPU pointer (must be ARMv8MCPU)
 * @return          ARMv8MCPU pointer
 */
static inline ARMv8MCPU *armv8m_cpu_from_base(EmuCPU *cpu) {
    return (ARMv8MCPU *)cpu;
}

/**
 * Get const ARMv8-M CPU wrapper from const EmuCPU pointer.
 *
 * @param cpu       Const EmuCPU pointer
 * @return          Const ARMv8MCPU pointer
 */
static inline const ARMv8MCPU *armv8m_cpu_from_base_const(const EmuCPU *cpu) {
    return (const ARMv8MCPU *)cpu;
}

/*============================================================================
 * Register Descriptions
 *============================================================================*/

/**
 * ARMv8-M register IDs for get_reg/set_reg.
 *
 * Matches GDB register numbering for ARM.
 */
typedef enum {
    ARMV8M_CPU_REG_R0 = 0,
    ARMV8M_CPU_REG_R1,
    ARMV8M_CPU_REG_R2,
    ARMV8M_CPU_REG_R3,
    ARMV8M_CPU_REG_R4,
    ARMV8M_CPU_REG_R5,
    ARMV8M_CPU_REG_R6,
    ARMV8M_CPU_REG_R7,
    ARMV8M_CPU_REG_R8,
    ARMV8M_CPU_REG_R9,
    ARMV8M_CPU_REG_R10,
    ARMV8M_CPU_REG_R11,
    ARMV8M_CPU_REG_R12,
    ARMV8M_CPU_REG_SP,      /* R13 */
    ARMV8M_CPU_REG_LR,      /* R14 */
    ARMV8M_CPU_REG_PC,      /* R15 */
    ARMV8M_CPU_REG_XPSR,    /* 16 - combined PSR */
    ARMV8M_CPU_REG_COUNT
} ARMv8MCPURegister;

/**
 * ARMv8-M special register IDs for get_special_reg/set_special_reg.
 */
typedef enum {
    ARMV8M_CPU_SREG_MSP = 0,        /* Main Stack Pointer */
    ARMV8M_CPU_SREG_PSP,            /* Process Stack Pointer */
    ARMV8M_CPU_SREG_PRIMASK,
    ARMV8M_CPU_SREG_BASEPRI,
    ARMV8M_CPU_SREG_FAULTMASK,
    ARMV8M_CPU_SREG_CONTROL,
    ARMV8M_CPU_SREG_MSPLIM,
    ARMV8M_CPU_SREG_PSPLIM,
    ARMV8M_CPU_SREG_FPSCR,          /* FPU status */
    ARMV8M_CPU_SREG_S0,             /* FPU S0-S31 start at this ID */
    /* S1-S31 follow sequentially */
    ARMV8M_CPU_SREG_COUNT = ARMV8M_CPU_SREG_S0 + 32
} ARMv8MCPUSpecialReg;

#ifdef __cplusplus
}
#endif

#endif /* ARMV8M_CPU_H */
