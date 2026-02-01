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

#include "arch/armv8m/armv8m_types.h"
#include "arch/armv8m/armv8m_decoder.h"

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

    /* Stack limits (ARMv8-M) */
    uint32_t msplim;            /**< MSP limit register */
    uint32_t psplim;            /**< PSP limit register */

    /* System control registers */
    uint32_t ccr;               /**< Configuration and Control Register */

    /* Fault status registers */
    uint32_t cfsr;              /**< Configurable Fault Status (MMFSR+BFSR+UFSR) */
    uint32_t hfsr;              /**< HardFault Status Register */
    uint32_t mmfar;             /**< MemManage Fault Address Register */
    uint32_t bfar;              /**< BusFault Address Register */
    
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

    /* Exclusive access monitor */
    uint32_t exclusive_addr;    /**< Address of exclusive access */
    bool exclusive_valid;       /**< Exclusive monitor is valid */

    /* FPU state (if has_fpu) */
    uint32_t s[32];             /**< S0-S31 single-precision registers */
    uint32_t fpscr;             /**< FPU Status and Control Register */
    uint32_t fpccr;             /**< FPU Context Control Register */
    uint32_t fpcar;             /**< FPU Context Address Register */
    uint32_t fpdscr;            /**< FPU Default Status Control Register */
    bool fp_context_active;     /**< FPCA bit tracking for lazy preservation */

} CPUState;

/*============================================================================
 * SAU (Security Attribution Unit) - TrustZone
 *============================================================================*/

/**
 * SAU region configuration.
 */
typedef struct {
    uint32_t rbar;              /**< Region Base Address Register */
    uint32_t rlar;              /**< Region Limit Address Register */
} SAURegion;

/**
 * SAU state.
 */
typedef struct {
    uint32_t ctrl;              /**< SAU_CTRL */
    uint32_t type;              /**< SAU_TYPE (read-only, num regions) */
    uint32_t rnr;               /**< Region Number Register */
    SAURegion regions[ARMV8M_SAU_REGIONS_MAX];
} SAUState;

/**
 * TrustZone banked registers.
 */
typedef struct {
    /* Banked special registers */
    uint32_t primask_s;         /**< Secure PRIMASK */
    uint32_t primask_ns;        /**< Non-secure PRIMASK */
    uint32_t faultmask_s;       /**< Secure FAULTMASK */
    uint32_t faultmask_ns;      /**< Non-secure FAULTMASK */
    uint32_t basepri_s;         /**< Secure BASEPRI */
    uint32_t basepri_ns;        /**< Non-secure BASEPRI */
    uint32_t control_s;         /**< Secure CONTROL */
    uint32_t control_ns;        /**< Non-secure CONTROL */
    uint32_t msplim_s;          /**< Secure MSP limit */
    uint32_t msplim_ns;         /**< Non-secure MSP limit */
    uint32_t psplim_s;          /**< Secure PSP limit */
    uint32_t psplim_ns;         /**< Non-secure PSP limit */
    uint32_t msp_s;             /**< Secure MSP */
    uint32_t msp_ns;            /**< Non-secure MSP */
    uint32_t psp_s;             /**< Secure PSP */
    uint32_t psp_ns;            /**< Non-secure PSP */
} TrustZoneBankedRegs;

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
 * Security attribute for memory access.
 */
typedef enum {
    SEC_SECURE,                 /**< Secure memory */
    SEC_NONSECURE,              /**< Non-secure memory */
    SEC_NSC,                    /**< Non-Secure Callable region */
} SecurityAttr;

/**
 * Executor context - combines CPU state with system callbacks.
 */
typedef struct {
    CPUState cpu;               /**< CPU state */
    MemoryCallbacks mem;        /**< Memory access */
    NVICCallbacks nvic;         /**< Interrupt controller */

    /* TrustZone state (if has_trustzone) */
    SAUState sau;               /**< Security Attribution Unit */
    TrustZoneBankedRegs tz_regs; /**< Banked registers */

    /* Configuration */
    bool has_fpu;               /**< FPU present? */
    bool has_dsp;               /**< DSP extension present? */
    bool has_trustzone;         /**< TrustZone present? */
    uint32_t num_mpu_regions;   /**< Number of MPU regions (0 if no MPU) */

    /* Vector Table Offset Registers */
    uint32_t vtor;              /**< VTOR - Vector Table Offset Register */
    uint32_t vtor_ns;           /**< Non-Secure VTOR (TrustZone only) */
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
 * Check if SP value would violate stack limits.
 *
 * @param cpu       CPU state
 * @param sp_value  SP value to check
 * @return          true if limit would be violated
 */
bool armv8m_check_stack_limit(const CPUState *cpu, uint32_t sp_value);

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
