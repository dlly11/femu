/**
 * @file armv8m_executor_vtable.h
 * @brief ARMv8-M executor adapter for abstract EmuExecutor interface
 *
 * Provides the ARMv8-M implementation of the abstract executor interface,
 * wrapping the existing Executor with the EmuExecutor vtable.
 */

#ifndef ARMV8M_EXECUTOR_VTABLE_H
#define ARMV8M_EXECUTOR_VTABLE_H

#include "arch/armv8m/armv8m_cpu.h"
#include "arch/armv8m/armv8m_decoder_vtable.h"
#include "arch/armv8m/armv8m_executor.h"
#include "emu/emu_executor.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * ARMv8-M Executor Wrapper
 *============================================================================*/

/**
 * ARMv8-M specific executor wrapper.
 *
 * Embeds EmuExecutor as first member for safe casting.
 */
typedef struct {
  EmuExecutor base;          /**< Base EmuExecutor (must be first) */
  Executor exec;             /**< ARMv8-M executor */
  ARMv8MCPU cpu_wrapper;     /**< CPU vtable wrapper */
  ARMv8MDecoder dec_wrapper; /**< Decoder vtable wrapper */
} ARMv8MExecutorWrapper;

/*============================================================================
 * ARMv8-M Executor API
 *============================================================================*/

/**
 * Get the vtable for ARMv8-M executor.
 *
 * @return      Pointer to static vtable
 */
const EmuExecutorVTable *armv8m_executor_get_vtable(void);

/**
 * Initialize ARMv8-M executor wrapper.
 *
 * @param wrapper   Executor wrapper to initialize
 */
void armv8m_executor_vtable_init(ARMv8MExecutorWrapper *wrapper);

/**
 * Get ARMv8-M executor wrapper from EmuExecutor pointer.
 *
 * @param exec      EmuExecutor pointer (must be ARMv8MExecutorWrapper)
 * @return          ARMv8MExecutorWrapper pointer
 */
static inline ARMv8MExecutorWrapper *
armv8m_executor_from_base(EmuExecutor *exec) {
  return (ARMv8MExecutorWrapper *)exec;
}

/**
 * Get const ARMv8-M executor wrapper from const EmuExecutor pointer.
 *
 * @param exec      Const EmuExecutor pointer
 * @return          Const ARMv8MExecutorWrapper pointer
 */
static inline const ARMv8MExecutorWrapper *
armv8m_executor_from_base_const(const EmuExecutor *exec) {
  return (const ARMv8MExecutorWrapper *)exec;
}

#ifdef __cplusplus
}
#endif

#endif /* ARMV8M_EXECUTOR_VTABLE_H */
