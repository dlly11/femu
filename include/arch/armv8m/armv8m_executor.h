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

#include "arch/armv8m/armv8m_blocks.h"
#include "arch/armv8m/armv8m_decoder.h"
#include "arch/armv8m/armv8m_icache.h"
#include "arch/armv8m/armv8m_mpu.h"
#include "arch/armv8m/armv8m_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Lazy Flag Evaluation
 *============================================================================*/

/**
 * Operation type for lazy flag computation.
 */
typedef enum {
  LAZY_OP_NONE = 0, /**< No pending lazy flags */
  LAZY_OP_ADD,      /**< Addition (need carry and overflow) */
  LAZY_OP_SUB,      /**< Subtraction (need borrow and overflow) */
  LAZY_OP_LOGIC,    /**< Logical operation (only N and Z, C from shifter) */
} LazyOpType;

/**
 * Lazy flag state - defers flag computation until flags are read.
 */
typedef struct {
  uint32_t result;    /**< Result of operation (for N and Z) */
  uint32_t op1;       /**< First operand */
  uint32_t op2;       /**< Second operand */
  LazyOpType op_type; /**< Type of operation */
  bool shifter_carry; /**< Carry out from shifter (for logic ops) */
  bool valid;         /**< True if lazy flags are pending */
} LazyFlags;

/*============================================================================
 * CPU State
 *============================================================================*/

/**
 * Complete CPU state.
 */
typedef struct {
  /* General purpose registers */
  uint32_t r[16]; /**< R0-R15 (R13=SP, R14=LR, R15=PC) */

  /* Program status */
  uint32_t xpsr; /**< Combined program status register */

  /* Special registers */
  uint32_t primask;   /**< PRIMASK - interrupt mask */
  uint32_t faultmask; /**< FAULTMASK - fault mask */
  uint32_t basepri;   /**< BASEPRI - base priority mask */
  uint32_t control;   /**< CONTROL - execution control */

  /* Banked stack pointers */
  uint32_t sp_main;    /**< Main stack pointer (MSP) */
  uint32_t sp_process; /**< Process stack pointer (PSP) */

  /* Stack limits (ARMv8-M) */
  uint32_t msplim; /**< MSP limit register */
  uint32_t psplim; /**< PSP limit register */

  /* System control registers */
  uint32_t ccr; /**< Configuration and Control Register */

  /* Fault status registers */
  uint32_t cfsr;  /**< Configurable Fault Status (MMFSR+BFSR+UFSR) */
  uint32_t hfsr;  /**< HardFault Status Register */
  uint32_t mmfar; /**< MemManage Fault Address Register */
  uint32_t bfar;  /**< BusFault Address Register */

  /* TrustZone banked registers (if enabled) */
  uint32_t sp_main_s;     /**< Secure MSP */
  uint32_t sp_main_ns;    /**< Non-secure MSP */
  uint32_t sp_process_s;  /**< Secure PSP */
  uint32_t sp_process_ns; /**< Non-secure PSP */

  /* Execution state */
  ExecutionMode mode;       /**< Thread or Handler mode */
  PrivilegeLevel privilege; /**< Privileged or Unprivileged */
  SecurityState security;   /**< Secure or Non-secure (TrustZone) */

  /* IT block state */
  uint8_t it_state; /**< ITSTATE from EPSR */

  /* Exception state */
  int current_exception; /**< Currently executing exception (0 if none) */
  uint32_t pending_irq;  /**< Pending interrupt flags */
  bool event_registered; /**< For WFE instruction */

  /* Cycle counter */
  uint64_t cycles; /**< Total cycles executed */

  /* Halt state */
  bool halted;   /**< CPU is halted (debug or WFI) */
  bool sleeping; /**< CPU is in sleep mode */

  /* Exclusive access monitor */
  uint32_t exclusive_addr; /**< Address of exclusive access */
  bool exclusive_valid;    /**< Exclusive monitor is valid */

  /* FPU state (if has_fpu) */
  uint32_t s[32];         /**< S0-S31 single-precision registers */
  uint32_t fpscr;         /**< FPU Status and Control Register */
  uint32_t fpccr;         /**< FPU Context Control Register */
  uint32_t fpcar;         /**< FPU Context Address Register */
  uint32_t fpdscr;        /**< FPU Default Status Control Register */
  bool fp_context_active; /**< FPCA bit tracking for lazy preservation */

  /* Lazy flag evaluation */
  LazyFlags lazy_flags; /**< Deferred flag computation state */
} CPUState;

/*============================================================================
 * SAU (Security Attribution Unit) - TrustZone
 *============================================================================*/

/**
 * SAU region configuration.
 */
typedef struct {
  uint32_t rbar; /**< Region Base Address Register */
  uint32_t rlar; /**< Region Limit Address Register */
} SAURegion;

/**
 * SAU state.
 */
typedef struct {
  uint32_t ctrl; /**< SAU_CTRL */
  uint32_t type; /**< SAU_TYPE (read-only, num regions) */
  uint32_t rnr;  /**< Region Number Register */
  SAURegion regions[ARMV8M_SAU_REGIONS_MAX];
} SAUState;

/* SAU memory-mapped register offsets (relative to 0xE000EDD0). */
#define ARMV8M_SAU_REG_CTRL 0x00 /**< SAU_CTRL */
#define ARMV8M_SAU_REG_TYPE 0x04 /**< SAU_TYPE (read-only) */
#define ARMV8M_SAU_REG_RNR 0x08  /**< SAU_RNR (region number) */
#define ARMV8M_SAU_REG_RBAR 0x0C /**< SAU_RBAR (region base) */
#define ARMV8M_SAU_REG_RLAR 0x10 /**< SAU_RLAR (region limit) */

/**
 * Read a memory-mapped SAU register.
 *
 * @param sau     SAU state.
 * @param offset  Register offset (ARMV8M_SAU_REG_*).
 * @param size    Access size in bytes (SAU registers are word-sized).
 * @return        Register value (0 for unimplemented offsets).
 */
uint32_t armv8m_sau_read(const SAUState *sau, uint32_t offset, uint8_t size);

/**
 * Write a memory-mapped SAU register.
 *
 * @param sau     SAU state.
 * @param offset  Register offset (ARMV8M_SAU_REG_*).
 * @param value   Value to write.
 * @param size    Access size in bytes (SAU registers are word-sized).
 */
void armv8m_sau_write(SAUState *sau, uint32_t offset, uint32_t value,
                      uint8_t size);

/**
 * IDAU (Implementation Defined Attribution Unit) state.
 *
 * Minimal fixed-attribution model: a region table that pins addresses to a
 * security attribute regardless of SAU programming. Each region reuses the SAU
 * region layout; a region with the NSC bit set marks Non-Secure-Callable,
 * otherwise it marks Secure. Addresses outside every enabled IDAU region (and
 * the whole unit when @ref enabled is false) contribute no opinion, so the SAU
 * result governs. The final attribution is the more-secure of SAU and IDAU.
 * Defaults to disabled/empty, preserving SAU-only behaviour.
 */
typedef struct {
  bool enabled;
  uint32_t num_regions;
  SAURegion regions[ARMV8M_SAU_REGIONS_MAX];
} IDAUState;

/**
 * TrustZone banked registers.
 */
typedef struct {
  /* Banked special registers */
  uint32_t primask_s;    /**< Secure PRIMASK */
  uint32_t primask_ns;   /**< Non-secure PRIMASK */
  uint32_t faultmask_s;  /**< Secure FAULTMASK */
  uint32_t faultmask_ns; /**< Non-secure FAULTMASK */
  uint32_t basepri_s;    /**< Secure BASEPRI */
  uint32_t basepri_ns;   /**< Non-secure BASEPRI */
  uint32_t control_s;    /**< Secure CONTROL */
  uint32_t control_ns;   /**< Non-secure CONTROL */
  uint32_t msplim_s;     /**< Secure MSP limit */
  uint32_t msplim_ns;    /**< Non-secure MSP limit */
  uint32_t psplim_s;     /**< Secure PSP limit */
  uint32_t psplim_ns;    /**< Non-secure PSP limit */
  uint32_t msp_s;        /**< Secure MSP */
  uint32_t msp_ns;       /**< Non-secure MSP */
  uint32_t psp_s;        /**< Secure PSP */
  uint32_t psp_ns;       /**< Non-secure PSP */
} TrustZoneBankedRegs;

/*============================================================================
 * Executor Context
 *============================================================================*/

/**
 * Memory access callbacks (provided by memory subsystem).
 */
typedef struct {
  void *ctx; /**< Opaque context for callbacks */
  uint32_t (*read)(void *ctx, uint32_t addr, uint8_t size, bool *fault);
  void (*write)(void *ctx, uint32_t addr, uint32_t value, uint8_t size,
                bool *fault);
  const uint8_t *(*get_ptr)(void *ctx, uint32_t addr,
                            uint32_t size); /**< For instruction fetch */
} MemoryCallbacks;

/**
 * NVIC callbacks (provided by NVIC module).
 */
typedef struct {
  void *ctx;
  int (*get_pending)(void *ctx); /**< Get highest priority pending exception */
  int (*get_priority)(void *ctx, int exc);   /**< Get priority of exception */
  void (*clear_pending)(void *ctx, int exc); /**< Clear pending state */
  void (*set_pending)(void *ctx, int exc);   /**< Set pending state */
} NVICCallbacks;

/**
 * Security attribute for memory access.
 */
typedef enum {
  SEC_SECURE,    /**< Secure memory */
  SEC_NONSECURE, /**< Non-secure memory */
  SEC_NSC,       /**< Non-Secure Callable region */
} SecurityAttr;

/**
 * Executor context - combines CPU state with system callbacks.
 */
typedef struct {
  CPUState cpu;        /**< CPU state */
  MemoryCallbacks mem; /**< Memory access */
  NVICCallbacks nvic;  /**< Interrupt controller */

  /* TrustZone state (if has_trustzone) */
  SAUState sau;                /**< Security Attribution Unit */
  IDAUState idau;              /**< Implementation Defined Attribution Unit */
  TrustZoneBankedRegs tz_regs; /**< Banked registers */

  /* Configuration */
  bool has_fpu;             /**< FPU present? */
  bool has_dsp;             /**< DSP extension present? */
  bool has_trustzone;       /**< TrustZone present? */
  uint32_t num_mpu_regions; /**< Number of MPU regions (0 if no MPU) */
  MPU *mpu;                 /**< MPU state for TT queries (NULL if no MPU) */

  /* Vector Table Offset Registers */
  uint32_t vtor;    /**< VTOR - Vector Table Offset Register */
  uint32_t vtor_ns; /**< Non-Secure VTOR (TrustZone only) */

  /* Performance optimization caches */
  InsnCache *icache;  /**< Decoded instruction cache (optional) */
  BlockCache *blocks; /**< Basic block cache (optional) */
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
void armv8m_update_flags(CPUState *cpu, uint32_t result, bool carry,
                         bool overflow);

/**
 * Set lazy flags for deferred flag computation.
 * Use this instead of armv8m_update_flags() for add/sub/logic operations.
 *
 * @param cpu       CPU state
 * @param op_type   Type of operation
 * @param result    Result of operation
 * @param op1       First operand
 * @param op2       Second operand
 * @param shifter_carry  Carry from shifter (for logic ops)
 */
void armv8m_set_lazy_flags(CPUState *cpu, LazyOpType op_type, uint32_t result,
                           uint32_t op1, uint32_t op2, bool shifter_carry);

/**
 * Materialize lazy flags into APSR.
 * Call this before reading flags (condition checks, MRS, exception entry).
 *
 * @param cpu       CPU state
 */
void armv8m_materialize_flags(CPUState *cpu);

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

/**
 * Set instruction cache for executor.
 *
 * @param exec      Executor context
 * @param cache     Instruction cache (NULL to disable)
 */
void armv8m_exec_set_icache(Executor *exec, InsnCache *cache);

/**
 * Invalidate instruction cache (if set).
 *
 * @param exec      Executor context
 */
void armv8m_exec_invalidate_icache(Executor *exec);

/**
 * Set block cache for executor.
 *
 * @param exec      Executor context
 * @param cache     Block cache (NULL to disable)
 */
void armv8m_exec_set_blocks(Executor *exec, BlockCache *cache);

/**
 * Invalidate block cache (if set).
 *
 * @param exec      Executor context
 */
void armv8m_exec_invalidate_blocks(Executor *exec);

/**
 * Execute a single basic block.
 *
 * @param exec      Executor context
 * @param block     Block to execute
 * @return          ARMV8M_OK or error code
 */
int armv8m_exec_block(Executor *exec, BasicBlock *block);

/**
 * Execute using basic blocks until cycles exhausted.
 * Uses block cache for faster execution.
 *
 * @param exec      Executor context
 * @param max_cycles Maximum cycles to execute
 * @return          Number of cycles executed, or negative on error
 */
int64_t armv8m_exec_run_blocks(Executor *exec, uint64_t max_cycles);

/**
 * Execute using linked basic blocks until cycles exhausted.
 * Uses block linking for faster block-to-block transitions.
 *
 * @param exec      Executor context
 * @param max_cycles Maximum cycles to execute
 * @return          Number of cycles executed, or negative on error
 */
int64_t armv8m_exec_run_linked(Executor *exec, uint64_t max_cycles);

/**
 * Execute a basic block using threaded interpretation (computed goto).
 * Falls back to switch dispatch on non-GCC/Clang compilers.
 *
 * @param exec      Executor context
 * @param block     Block to execute
 * @return          ARMV8M_OK or error code
 */
int armv8m_exec_block_threaded(Executor *exec, BasicBlock *block);

/**
 * Execute using threaded interpretation until cycles exhausted.
 * Combines block caching with computed goto dispatch for maximum performance.
 *
 * @param exec      Executor context
 * @param max_cycles Maximum cycles to execute
 * @return          Number of cycles executed, or negative on error
 */
int64_t armv8m_exec_run_threaded(Executor *exec, uint64_t max_cycles);

#ifdef __cplusplus
}
#endif

#endif /* ARMV8M_EXECUTOR_H */
