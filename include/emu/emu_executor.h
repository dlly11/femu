/**
 * @file emu_executor.h
 * @brief Abstract executor interface for multi-architecture support
 *
 * This file defines the abstract instruction executor interface using a vtable
 * pattern. Architecture-specific implementations provide concrete execution
 * functions.
 */

#ifndef EMU_EXECUTOR_H
#define EMU_EXECUTOR_H

#include "emu_cpu.h"
#include "emu_decoder.h"
#include "emu_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Forward Declarations
 *============================================================================*/

typedef struct EmuExecutor EmuExecutor;
typedef struct EmuExecutorVTable EmuExecutorVTable;
typedef struct EmuMemoryCallbacks EmuMemoryCallbacks;

/*============================================================================
 * Memory Callbacks
 *============================================================================*/

/**
 * Memory access callbacks provided to executor.
 *
 * These callbacks are used by the executor to perform memory operations.
 * The callbacks abstract the memory subsystem from the executor.
 */
struct EmuMemoryCallbacks {
  void *ctx; /**< Opaque context for callbacks */

  /**
   * Read from memory.
   *
   * @param ctx       Callback context
   * @param addr      Address to read
   * @param size      Access size (1, 2, 4, or 8)
   * @param fault     Set to true if fault occurred
   * @return          Value read (zero-extended)
   */
  uint64_t (*read)(void *ctx, uint64_t addr, uint8_t size, bool *fault);

  /**
   * Write to memory.
   *
   * @param ctx       Callback context
   * @param addr      Address to write
   * @param value     Value to write
   * @param size      Access size (1, 2, 4, or 8)
   * @param fault     Set to true if fault occurred
   */
  void (*write)(void *ctx, uint64_t addr, uint64_t value, uint8_t size,
                bool *fault);

  /**
   * Get direct pointer to memory (for instruction fetch).
   * Returns NULL if address is not backed by contiguous RAM/ROM.
   *
   * @param ctx       Callback context
   * @param addr      Address
   * @param size      Required size
   * @return          Pointer to memory or NULL
   */
  const uint8_t *(*get_ptr)(void *ctx, uint64_t addr, uint32_t size);
};

/*============================================================================
 * Executor Virtual Table
 *============================================================================*/

/**
 * Virtual function table for executor operations.
 */
struct EmuExecutorVTable {
  /**
   * Destroy executor and free resources.
   *
   * @param exec      Executor instance
   */
  void (*destroy)(EmuExecutor *exec);

  /**
   * Reset executor to initial state.
   *
   * @param exec          Executor instance
   * @param reset_vector  Reset vector address (architecture-specific meaning)
   */
  void (*reset)(EmuExecutor *exec, uint64_t reset_vector);

  /**
   * Set memory callbacks.
   *
   * @param exec      Executor instance
   * @param mem       Memory callbacks
   */
  void (*set_memory)(EmuExecutor *exec, const EmuMemoryCallbacks *mem);

  /**
   * Get CPU state.
   *
   * @param exec      Executor instance
   * @return          Pointer to CPU state
   */
  EmuCPU *(*get_cpu)(EmuExecutor *exec);

  /**
   * Get decoder.
   *
   * @param exec      Executor instance
   * @return          Pointer to decoder
   */
  EmuDecoder *(*get_decoder)(EmuExecutor *exec);

  /**
   * Execute a single decoded instruction.
   *
   * @param exec      Executor instance
   * @param insn      Decoded instruction to execute
   * @return          EMU_OK on success, error code on exception/fault
   */
  int (*exec_insn)(EmuExecutor *exec, const EmuDecodedInsn *insn);

  /**
   * Execute one instruction (fetch, decode, execute).
   *
   * @param exec      Executor instance
   * @return          EMU_OK on success, error code on exception/fault
   */
  int (*step)(EmuExecutor *exec);

  /**
   * Execute until cycles exhausted or event occurs.
   *
   * @param exec          Executor instance
   * @param max_cycles    Maximum cycles to execute (0 = unlimited)
   * @return              Cycles executed, or negative on error
   */
  int64_t (*run)(EmuExecutor *exec, uint64_t max_cycles);

  /**
   * Check if execution should stop (for breakpoints, etc.).
   *
   * @param exec      Executor instance
   * @param addr      Address to check
   * @return          true if should stop at this address
   */
  bool (*should_stop)(EmuExecutor *exec, uint64_t addr);

  /**
   * Handle pending interrupts/exceptions.
   *
   * @param exec      Executor instance
   * @return          Exception number taken, or 0 if none
   */
  int (*handle_interrupts)(EmuExecutor *exec);
};

/*============================================================================
 * Executor Structure
 *============================================================================*/

/**
 * Abstract executor instance.
 */
struct EmuExecutor {
  const EmuExecutorVTable *vtable; /**< Virtual function table */
  EmuArchType arch;                /**< Architecture type */
  void *arch_state;                /**< Architecture-specific state */
};

/*============================================================================
 * Convenience Macros
 *============================================================================*/

#define EMU_EXECUTOR_INIT(exec, vtbl, arch_type, state)                        \
  do {                                                                         \
    (exec)->vtable = (vtbl);                                                   \
    (exec)->arch = (arch_type);                                                \
    (exec)->arch_state = (state);                                              \
  } while (0)

/*============================================================================
 * Inline Convenience Functions
 *============================================================================*/

static inline void emu_executor_destroy(EmuExecutor *exec) {
  if (exec && exec->vtable && exec->vtable->destroy) {
    exec->vtable->destroy(exec);
  }
}

static inline void emu_executor_reset(EmuExecutor *exec,
                                      uint64_t reset_vector) {
  if (exec && exec->vtable) {
    exec->vtable->reset(exec, reset_vector);
  }
}

static inline void emu_executor_set_memory(EmuExecutor *exec,
                                           const EmuMemoryCallbacks *mem) {
  if (exec && exec->vtable && exec->vtable->set_memory) {
    exec->vtable->set_memory(exec, mem);
  }
}

static inline EmuCPU *emu_executor_get_cpu(EmuExecutor *exec) {
  return (exec && exec->vtable) ? exec->vtable->get_cpu(exec) : NULL;
}

static inline EmuDecoder *emu_executor_get_decoder(EmuExecutor *exec) {
  return (exec && exec->vtable && exec->vtable->get_decoder)
             ? exec->vtable->get_decoder(exec)
             : NULL;
}

static inline int emu_executor_exec_insn(EmuExecutor *exec,
                                         const EmuDecodedInsn *insn) {
  return (exec && exec->vtable) ? exec->vtable->exec_insn(exec, insn)
                                : EMU_ERR_NOT_INITIALIZED;
}

static inline int emu_executor_step(EmuExecutor *exec) {
  return (exec && exec->vtable) ? exec->vtable->step(exec)
                                : EMU_ERR_NOT_INITIALIZED;
}

static inline int64_t emu_executor_run(EmuExecutor *exec, uint64_t max_cycles) {
  return (exec && exec->vtable) ? exec->vtable->run(exec, max_cycles)
                                : EMU_ERR_NOT_INITIALIZED;
}

#ifdef __cplusplus
}
#endif

#endif /* EMU_EXECUTOR_H */
