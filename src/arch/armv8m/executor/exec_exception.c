/**
 * @file exec_exception.c
 * @brief Exception handling for ARMv8-M
 *
 * Implements exception entry, context push/pop, and EXC_RETURN handling.
 */

#include "arch/armv8m/armv8m_executor.h"
#include "arch/armv8m/armv8m_types.h"
#include "emu/emu_log.h"

/*============================================================================
 * EXC_RETURN Values
 *============================================================================*/

/* EXC_RETURN bit definitions */
#define EXC_RETURN_PREFIX       0xFF000000  /* Magic prefix for EXC_RETURN */
#define EXC_RETURN_S            (1 << 6)    /* Secure stack used (TrustZone) */
#define EXC_RETURN_DCRS         (1 << 5)    /* Default callee register stacking */
#define EXC_RETURN_FTYPE        (1 << 4)    /* Frame type: 0=extended (FP), 1=basic */
#define EXC_RETURN_MODE         (1 << 3)    /* Return mode: 0=Handler, 1=Thread */
#define EXC_RETURN_SPSEL        (1 << 2)    /* Return SP: 0=MSP, 1=PSP */
#define EXC_RETURN_ES           (1 << 0)    /* Exception secure (TrustZone) */

/* Common EXC_RETURN values */
#define EXC_RETURN_HANDLER_MSP  0xFFFFFFF1  /* Return to Handler, MSP, no FP */
#define EXC_RETURN_THREAD_MSP   0xFFFFFFF9  /* Return to Thread, MSP, no FP */
#define EXC_RETURN_THREAD_PSP   0xFFFFFFFD  /* Return to Thread, PSP, no FP */

/* Exception frame sizes */
#define EXCEPTION_FRAME_BASIC    32   /* 8 words: R0-R3, R12, LR, PC, xPSR */
#define EXCEPTION_FRAME_EXTENDED 104  /* Basic + S0-S15, FPSCR, reserved */

/*============================================================================
 * Internal Helpers
 *============================================================================*/

/**
 * Read from memory.
 */
static uint32_t mem_read(Executor *exec, uint32_t addr, uint8_t size, bool *fault)
{
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
static void mem_write(Executor *exec, uint32_t addr, uint32_t value, uint8_t size, bool *fault)
{
    *fault = false;

    if (!exec->mem.write) {
        *fault = true;
        return;
    }

    exec->mem.write(exec->mem.ctx, addr, value, size, fault);
}

/**
 * Get VTOR (Vector Table Offset Register) value.
 * Returns the appropriate VTOR based on current security state.
 */
static uint32_t get_vtor(const Executor *exec)
{
    if (exec->has_trustzone && exec->cpu.security == SECURITY_NONSECURE) {
        return exec->vtor_ns;
    }
    return exec->vtor;
}

/**
 * Check if FPU context is active and should be preserved.
 */
static bool should_preserve_fpu_context(const Executor *exec)
{
    if (!exec->has_fpu) {
        return false;
    }
    /* Check CONTROL.FPCA (FP context active) and FPCCR.ASPEN */
    return (exec->cpu.control & ARMV8M_CONTROL_FPCA) &&
           (exec->cpu.fpccr & ARMV8M_FPCCR_ASPEN);
}

/**
 * Check if lazy FPU state preservation is enabled.
 */
static bool is_lazy_stacking_enabled(const Executor *exec)
{
    return exec->has_fpu && (exec->cpu.fpccr & ARMV8M_FPCCR_LSPEN);
}

/**
 * Save FPU context to stack (non-lazy).
 */
static int save_fpu_context(Executor *exec, uint32_t frameptr, bool *fault)
{
    CPUState *cpu = &exec->cpu;

    /* Save S0-S15 (16 registers * 4 bytes = 64 bytes) */
    for (int i = 0; i < 16; i++) {
        mem_write(exec, frameptr + 32 + (uint32_t)(i * 4), cpu->s[i], ACCESS_WORD, fault);
        if (*fault) return ARMV8M_ERR_BUS_FAULT;
    }

    /* Save FPSCR at offset 32 + 64 = 96 */
    mem_write(exec, frameptr + 96, cpu->fpscr, ACCESS_WORD, fault);
    if (*fault) return ARMV8M_ERR_BUS_FAULT;

    /* Reserved word at offset 100 */
    mem_write(exec, frameptr + 100, 0, ACCESS_WORD, fault);
    if (*fault) return ARMV8M_ERR_BUS_FAULT;

    return ARMV8M_OK;
}

/**
 * Restore FPU context from stack.
 */
static int restore_fpu_context(Executor *exec, uint32_t frameptr, bool *fault)
{
    CPUState *cpu = &exec->cpu;

    /* Restore S0-S15 */
    for (int i = 0; i < 16; i++) {
        cpu->s[i] = mem_read(exec, frameptr + 32 + (uint32_t)(i * 4), ACCESS_WORD, fault);
        if (*fault) return ARMV8M_ERR_BUS_FAULT;
    }

    /* Restore FPSCR */
    cpu->fpscr = mem_read(exec, frameptr + 96, ACCESS_WORD, fault);
    if (*fault) return ARMV8M_ERR_BUS_FAULT;

    return ARMV8M_OK;
}

/**
 * Validate EXC_RETURN value for exception return.
 */
static int validate_exc_return(const Executor *exec, uint32_t exc_return)
{
    /* Check magic prefix */
    if ((exc_return & 0xFF000000) != EXC_RETURN_PREFIX) {
        return ARMV8M_ERR_USAGE_FAULT;
    }

    /* Must be in Handler mode to perform exception return */
    if (exec->cpu.mode != MODE_HANDLER) {
        return ARMV8M_ERR_USAGE_FAULT;
    }

    /* Check valid bit combinations */
    bool return_to_thread = (exc_return & EXC_RETURN_MODE) != 0;
    bool return_to_psp = (exc_return & EXC_RETURN_SPSEL) != 0;

    /* Can't return to PSP in Handler mode */
    if (!return_to_thread && return_to_psp) {
        return ARMV8M_ERR_USAGE_FAULT;
    }

    /* TrustZone validation */
    if (exec->has_trustzone) {
        bool exc_secure = (exc_return & EXC_RETURN_ES) != 0;
        bool stack_secure = (exc_return & EXC_RETURN_S) != 0;

        /* Non-secure cannot return to secure with secure stack */
        if (exec->cpu.security == SECURITY_NONSECURE && exc_secure && stack_secure) {
            /* This would be a security violation */
        }
    }

    return ARMV8M_OK;
}

/*============================================================================
 * Exception Entry
 *============================================================================*/

int armv8m_exception_entry(Executor *exec, int exception)
{
    CPUState *cpu = &exec->cpu;
    bool fault = false;

    EMU_LOG_INFO(EMU_LOG_CAT_NVIC, "Exception entry: exc=%d from PC=0x%08X mode=%s",
                 exception, cpu->r[ARMV8M_REG_PC],
                 cpu->mode == MODE_THREAD ? "Thread" : "Handler");

    /* 1. Determine which SP to use for stacking */
    uint32_t sp;
    bool use_psp = (cpu->mode == MODE_THREAD) && (cpu->control & ARMV8M_CONTROL_SPSEL);

    if (use_psp) {
        sp = cpu->sp_process;
    } else {
        sp = cpu->sp_main;
    }

    /* 2. Align SP to 8 bytes (ARMv8-M requirement) */
    uint32_t frameptr = sp & ~7U;
    bool sp_align = (sp != frameptr);

    /* 3. Determine frame size based on FPU context */
    bool use_extended_frame = should_preserve_fpu_context(exec);
    bool lazy_stacking = is_lazy_stacking_enabled(exec);
    uint32_t frame_size = use_extended_frame ? EXCEPTION_FRAME_EXTENDED : EXCEPTION_FRAME_BASIC;

    /* 4. Allocate space for exception frame */
    frameptr -= frame_size;

    /* 4a. Check stack limit before pushing */
    if (armv8m_check_stack_limit(cpu, frameptr)) {
        /* Stack overflow during exception entry */
        cpu->cfsr |= ARMV8M_UFSR_STKOF;
        if (exception != ARMV8M_EXC_HARDFAULT && exception != ARMV8M_EXC_NMI) {
            return armv8m_exception_entry(exec, ARMV8M_EXC_HARDFAULT);
        }
        /* Already in HardFault/NMI - lockup */
        cpu->halted = true;
        return ARMV8M_ERR_HARD_FAULT;
    }

    /* 5. Push context: R0, R1, R2, R3, R12, LR, PC, xPSR */
    /* Note: xPSR[9] is set if SP was realigned */
    uint32_t xpsr_to_save = cpu->xpsr;
    if (sp_align) {
        xpsr_to_save |= (1 << 9);  /* Set alignment bit */
    }

    /* Frame layout (low to high address):
     * frameptr + 0:  R0
     * frameptr + 4:  R1
     * frameptr + 8:  R2
     * frameptr + 12: R3
     * frameptr + 16: R12
     * frameptr + 20: LR
     * frameptr + 24: Return address (PC)
     * frameptr + 28: xPSR
     */
    mem_write(exec, frameptr + 0,  cpu->r[0], ACCESS_WORD, &fault);
    mem_write(exec, frameptr + 4,  cpu->r[1], ACCESS_WORD, &fault);
    mem_write(exec, frameptr + 8,  cpu->r[2], ACCESS_WORD, &fault);
    mem_write(exec, frameptr + 12, cpu->r[3], ACCESS_WORD, &fault);
    mem_write(exec, frameptr + 16, cpu->r[12], ACCESS_WORD, &fault);
    mem_write(exec, frameptr + 20, cpu->r[ARMV8M_REG_LR], ACCESS_WORD, &fault);
    mem_write(exec, frameptr + 24, cpu->r[ARMV8M_REG_PC], ACCESS_WORD, &fault);
    mem_write(exec, frameptr + 28, xpsr_to_save, ACCESS_WORD, &fault);

    if (fault) {
        /* Stack push failed - escalate to HardFault */
        if (exception != ARMV8M_EXC_HARDFAULT && exception != ARMV8M_EXC_NMI) {
            return armv8m_exception_entry(exec, ARMV8M_EXC_HARDFAULT);
        }
        /* Already in HardFault/NMI - lockup */
        cpu->halted = true;
        return ARMV8M_ERR_HARD_FAULT;
    }

    /* 5. Handle FPU context preservation */
    if (use_extended_frame) {
        if (lazy_stacking) {
            /* Lazy stacking: reserve space but don't save yet */
            cpu->fpccr |= ARMV8M_FPCCR_LSPACT;
            cpu->fpcar = frameptr;  /* Store frame pointer for later */
        } else {
            /* Eager stacking: save FPU registers now */
            int fpu_result = save_fpu_context(exec, frameptr, &fault);
            if (fpu_result != ARMV8M_OK) {
                if (exception != ARMV8M_EXC_HARDFAULT && exception != ARMV8M_EXC_NMI) {
                    cpu->cfsr |= ARMV8M_MMFSR_MLSPERR;
                    return armv8m_exception_entry(exec, ARMV8M_EXC_HARDFAULT);
                }
                cpu->halted = true;
                return ARMV8M_ERR_HARD_FAULT;
            }
        }
    }

    /* 6. Update SP */
    if (use_psp) {
        cpu->sp_process = frameptr;
    } else {
        cpu->sp_main = frameptr;
    }

    /* 7. Generate EXC_RETURN value for LR */
    uint32_t exc_return = EXC_RETURN_PREFIX;

    /* Set FTYPE: 1 = basic frame (no FP), 0 = extended frame (FP) */
    if (!use_extended_frame) {
        exc_return |= EXC_RETURN_FTYPE;
    }

    if (cpu->mode == MODE_THREAD) {
        exc_return |= EXC_RETURN_MODE;  /* Was in Thread mode */
    }
    if (use_psp) {
        exc_return |= EXC_RETURN_SPSEL;  /* Was using PSP */
    }

    /* TrustZone bits */
    if (exec->has_trustzone) {
        /* S bit: which stack was used (secure or non-secure) */
        if (cpu->security == SECURITY_SECURE) {
            exc_return |= EXC_RETURN_S;
        }
        /* ES bit: exception taken to secure state */
        if (cpu->security == SECURITY_SECURE) {
            exc_return |= EXC_RETURN_ES;
        }
    }

    cpu->r[ARMV8M_REG_LR] = exc_return;

    /* 7. Load vector address */
    uint32_t vtor = get_vtor(exec);
    uint32_t vector_addr = vtor + ((uint32_t)exception * 4);
    uint32_t handler = mem_read(exec, vector_addr, ACCESS_WORD, &fault);

    if (fault) {
        /* Vector fetch failed */
        cpu->halted = true;
        return ARMV8M_ERR_HARD_FAULT;
    }

    /* 8. Acknowledge exception in NVIC */
    if (exec->nvic.clear_pending) {
        exec->nvic.clear_pending(exec->nvic.ctx, exception);
    }

    /* 9. Update CPU state */
    cpu->r[ARMV8M_REG_PC] = handler & ~1u;
    cpu->mode = MODE_HANDLER;
    cpu->current_exception = exception;
    cpu->it_state = 0;  /* Clear IT state */

    /* Handler mode always uses MSP */
    cpu->r[ARMV8M_REG_SP] = cpu->sp_main;

    return ARMV8M_OK;
}

/*============================================================================
 * Exception Return
 *============================================================================*/

int armv8m_exception_return(Executor *exec, uint32_t exc_return)
{
    CPUState *cpu = &exec->cpu;
    bool fault = false;

    EMU_LOG_DEBUG(EMU_LOG_CAT_NVIC, "Exception return: EXC_RETURN=0x%08X current_exc=%d",
                  exc_return, cpu->current_exception);

    /* Validate EXC_RETURN value */
    int validation_result = validate_exc_return(exec, exc_return);
    if (validation_result != ARMV8M_OK) {
        return validation_result;
    }

    /* Determine which SP the frame was pushed to */
    bool return_to_thread = (exc_return & EXC_RETURN_MODE) != 0;
    bool return_to_psp = (exc_return & EXC_RETURN_SPSEL) != 0;
    bool extended_frame = (exc_return & EXC_RETURN_FTYPE) == 0;  /* FTYPE=0 means extended */

    uint32_t frameptr;
    if (return_to_psp && return_to_thread) {
        frameptr = cpu->sp_process;
    } else {
        frameptr = cpu->sp_main;
    }

    /* Pop basic context from stack */
    cpu->r[0]  = mem_read(exec, frameptr + 0, ACCESS_WORD, &fault);
    cpu->r[1]  = mem_read(exec, frameptr + 4, ACCESS_WORD, &fault);
    cpu->r[2]  = mem_read(exec, frameptr + 8, ACCESS_WORD, &fault);
    cpu->r[3]  = mem_read(exec, frameptr + 12, ACCESS_WORD, &fault);
    cpu->r[12] = mem_read(exec, frameptr + 16, ACCESS_WORD, &fault);
    cpu->r[ARMV8M_REG_LR] = mem_read(exec, frameptr + 20, ACCESS_WORD, &fault);
    uint32_t return_addr = mem_read(exec, frameptr + 24, ACCESS_WORD, &fault);
    uint32_t xpsr = mem_read(exec, frameptr + 28, ACCESS_WORD, &fault);

    if (fault) {
        return ARMV8M_ERR_BUS_FAULT;
    }

    /* Restore FPU context if extended frame */
    if (extended_frame && exec->has_fpu) {
        /* Clear lazy stacking active flag */
        cpu->fpccr &= ~ARMV8M_FPCCR_LSPACT;

        /* Restore FPU registers */
        int fpu_result = restore_fpu_context(exec, frameptr, &fault);
        if (fpu_result != ARMV8M_OK) {
            return fpu_result;
        }
    }

    /* Restore xPSR (preserve T bit, restore flags and IT state) */
    bool was_aligned = (xpsr >> 9) & 1;
    cpu->xpsr = (xpsr & ~(1U << 9)) | ARMV8M_XPSR_T;  /* Ensure T bit is set */

    /* Update PC */
    cpu->r[ARMV8M_REG_PC] = return_addr & ~1U;

    /* Calculate frame size and restore SP */
    uint32_t frame_size = extended_frame ? EXCEPTION_FRAME_EXTENDED : EXCEPTION_FRAME_BASIC;
    uint32_t new_sp = frameptr + frame_size;
    if (was_aligned) {
        new_sp += 4;
    }

    /* Deactivate exception in NVIC */
    if (exec->nvic.clear_pending) {
        /* Note: We should call a deactivate function, not clear_pending.
         * For now, just track the exception count going down. */
    }

    /* Handle TrustZone security state switching */
    if (exec->has_trustzone) {
        bool return_to_secure = (exc_return & EXC_RETURN_ES) != 0;

        if (return_to_secure && cpu->security == SECURITY_NONSECURE) {
            /* Transitioning from non-secure to secure on exception return */
            /* This should have been validated, but double-check */
        }

        cpu->security = return_to_secure ? SECURITY_SECURE : SECURITY_NONSECURE;

        /* If returning to non-secure, select the appropriate banked registers */
        if (!return_to_secure) {
            /* Use non-secure stack pointers */
            if (return_to_psp && return_to_thread) {
                new_sp = exec->tz_regs.psp_ns;
            } else {
                new_sp = exec->tz_regs.msp_ns;
            }
        }
    }

    /* Update CPU state based on return mode */
    if (return_to_thread) {
        cpu->mode = MODE_THREAD;
        cpu->current_exception = 0;

        if (return_to_psp) {
            cpu->sp_process = new_sp;
            cpu->r[ARMV8M_REG_SP] = cpu->sp_process;
        } else {
            cpu->sp_main = new_sp;
            cpu->r[ARMV8M_REG_SP] = cpu->sp_main;
        }
    } else {
        /* Returning to Handler mode (tail-chaining or nested exception return) */
        cpu->mode = MODE_HANDLER;
        cpu->sp_main = new_sp;
        cpu->r[ARMV8M_REG_SP] = cpu->sp_main;
        /* current_exception should be set by the caller or NVIC */
    }

    /* Restore IT state from xPSR if present */
    uint8_t it_low = (uint8_t)((xpsr >> 25) & 0x3);
    uint8_t it_high = (uint8_t)((xpsr >> 10) & 0x3F);
    cpu->it_state = (uint8_t)(((uint32_t)it_high << 2) | it_low);

    return ARMV8M_OK;
}
