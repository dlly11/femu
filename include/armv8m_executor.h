/**
 * @file armv8m_executor.h
 * @brief Instruction executor for ARMv8-M
 *
 * AI INSTRUCTIONS:
 * - This header defines the COMPLETE interface for the executor module
 * - Implementation goes in src/core/executor/
 * - Depends on: armv8m_types.h, armv8m_decoder.h, armv8m_memory.h
 * - See src/core/executor/README.md for implementation guidance
 */

#ifndef ARMV8M_EXECUTOR_H
#define ARMV8M_EXECUTOR_H

#include "armv8m_types.h"
#include "armv8m_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * CPU State
 *============================================================================*/

/**
 * Complete CPU state.
 */
typedef struct {
    /* General purpose registers */
    uint32_t r[16];             /**< R0-R15 (R13=SP, R14=LR, R15=PC) */
    
    /* Program status */
    uint32_t xpsr;              /**< Combined program status register */
    
    /* Special registers */
    uint32_t primask;           /**< PRIMASK - interrupt mask */
    uint32_t faultmask;         /**< FAULTMASK - fault mask */
    uint32_t basepri;           /**< BASEPRI - base priority mask */
    uint32_t control;           /**< CONTROL - execution control */
    
    /* Banked stack pointers */
    uint32_t sp_main;           /**< Main stack pointer (MSP) */
    uint32_t sp_process;        /**< Process stack pointer (PSP) */
    
    /* TrustZone banked registers (if enabled) */
    uint32_t sp_main_s;         /**< Secure MSP */
    uint32_t sp_main_ns;        /**< Non-secure MSP */
    uint32_t sp_process_s;      /**< Secure PSP */
    uint32_t sp_process_ns;     /**< Non-secure PSP */
    
    /* Execution state */
    ExecutionMode mode;         /**< Thread or Handler mode */
    PrivilegeLevel privilege;   /**< Privileged or Unprivileged */
    SecurityState security;     /**< Secure or Non-secure (TrustZone) */
    
    /* IT block state */
    uint8_t it_state;           /**< ITSTATE from EPSR */
    
    /* Exception state */
    int current_exception;      /**< Currently executing exception (0 if none) */
    uint32_t pending_irq;       /**< Pending interrupt flags */
    bool event_registered;      /**< For WFE instruction */
    
    /* Cycle counter */
    uint64_t cycles;            /**< Total cycles executed */
    
    /* Halt state */
    bool halted;                /**< CPU is halted (debug or WFI) */
    bool sleeping;              /**< CPU is in sleep mode */
    
} CPUState;

/*============================================================================
 * Executor Context
 *============================================================================*/

/**
 * Memory access callbacks (provided by memory subsystem).
 */
typedef struct {
    void *ctx;                  /**< Opaque context for callbacks */
    uint32_t (*read)(void *ctx, uint32_t addr, uint8_t size, bool *fault);
    void (*write)(void *ctx, uint32_t addr, uint32_t value, uint8_t size, bool *fault);
    const uint8_t* (*get_ptr)(void *ctx, uint32_t addr, uint32_t size);  /**< For instruction fetch */
} MemoryCallbacks;

/**
 * NVIC callbacks (provided by NVIC module).
 */
typedef struct {
    void *ctx;
    int (*get_pending)(void *ctx);              /**< Get highest priority pending exception */
    int (*get_priority)(void *ctx, int exc);    /**< Get priority of exception */
    void (*clear_pending)(void *ctx, int exc);  /**< Clear pending state */
    void (*set_pending)(void *ctx, int exc);    /**< Set pending state */
} NVICCallbacks;

/**
 * Executor context - combines CPU state with system callbacks.
 */
typedef struct {
    CPUState cpu;               /**< CPU state */
    MemoryCallbacks mem;        /**< Memory access */
    NVICCallbacks nvic;         /**< Interrupt controller */
    
    /* Configuration */
    bool has_fpu;               /**< FPU present? */
    bool has_dsp;               /**< DSP extension present? */
    bool has_trustzone;         /**< TrustZone present? */
    uint32_t num_mpu_regions;   /**< Number of MPU regions (0 if no MPU) */
} Executor;

/*============================================================================
 * Executor API
 *============================================================================*/

/**
 * Initialize executor with default state.
 *
 * @param exec      Executor context to initialize
 */
void armv8m_exec_init(Executor *exec);

/**
 * Reset CPU to initial state.
 *
 * @param exec      Executor context
 * @param vtor      Vector table base address
 */
void armv8m_exec_reset(Executor *exec, uint32_t vtor);

/**
 * Execute a single decoded instruction.
 *
 * @param exec      Executor context
 * @param insn      Decoded instruction to execute
 * @return          ARMV8M_OK on success, error code on exception/fault
 */
int armv8m_exec_insn(Executor *exec, const DecodedInsn *insn);

/**
 * Execute one instruction (fetch, decode, execute).
 *
 * @param exec      Executor context
 * @return          ARMV8M_OK on success, error code on exception/fault
 */
int armv8m_exec_step(Executor *exec);

/**
 * Execute until cycles exhausted or event occurs.
 *
 * @param exec      Executor context
 * @param max_cycles Maximum cycles to execute (0 = unlimited)
 * @return          Number of cycles executed, or negative on error
 */
int64_t armv8m_exec_run(Executor *exec, uint64_t max_cycles);

/**
 * Check if condition code is satisfied given current flags.
 *
 * @param xpsr      Current xPSR value
 * @param cond      Condition code to check
 * @return          true if condition is satisfied
 */
bool armv8m_check_condition(uint32_t xpsr, ConditionCode cond);

/**
 * Update APSR flags after ALU operation.
 *
 * @param cpu       CPU state
 * @param result    Result of operation
 * @param carry     Carry out (for C flag)
 * @param overflow  Overflow occurred (for V flag)
 */
void armv8m_update_flags(CPUState *cpu, uint32_t result, bool carry, bool overflow);

/**
 * Get current stack pointer value.
 *
 * @param cpu       CPU state
 * @return          Current SP value (MSP or PSP depending on CONTROL.SPSEL)
 */
uint32_t armv8m_get_sp(const CPUState *cpu);

/**
 * Set current stack pointer value.
 *
 * @param cpu       CPU state
 * @param value     New SP value
 */
void armv8m_set_sp(CPUState *cpu, uint32_t value);

/**
 * Handle exception entry.
 *
 * @param exec      Executor context
 * @param exception Exception number to enter
 * @return          ARMV8M_OK or error code
 */
int armv8m_exception_entry(Executor *exec, int exception);

/**
 * Handle exception return.
 *
 * @param exec      Executor context
 * @param exc_return EXC_RETURN value from LR
 * @return          ARMV8M_OK or error code
 */
int armv8m_exception_return(Executor *exec, uint32_t exc_return);

#ifdef __cplusplus
}
#endif

#endif /* ARMV8M_EXECUTOR_H */
