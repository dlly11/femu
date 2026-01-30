/**
 * @file exec_system.c
 * @brief System instruction execution for ARMv8-M
 *
 * Implements MRS, MSR, CPS, barriers (DMB, DSB, ISB), hints (NOP, WFI, WFE, SEV).
 */

#include "armv8m_executor.h"
#include "armv8m_types.h"

/*============================================================================
 * Special Register Numbers (for MRS/MSR)
 *============================================================================*/

#define SYSREG_APSR         0x00    /* Application PSR (flags only) */
#define SYSREG_IAPSR        0x01    /* IPSR + APSR */
#define SYSREG_EAPSR        0x02    /* EPSR + APSR */
#define SYSREG_XPSR         0x03    /* Full xPSR */
#define SYSREG_IPSR         0x05    /* Interrupt PSR */
#define SYSREG_EPSR         0x06    /* Execution PSR */
#define SYSREG_IEPSR        0x07    /* IPSR + EPSR */
#define SYSREG_MSP          0x08    /* Main Stack Pointer */
#define SYSREG_PSP          0x09    /* Process Stack Pointer */
#define SYSREG_MSPLIM       0x0A    /* MSP Limit (v8-M) */
#define SYSREG_PSPLIM       0x0B    /* PSP Limit (v8-M) */
#define SYSREG_PRIMASK      0x10    /* Priority Mask */
#define SYSREG_BASEPRI      0x11    /* Base Priority */
#define SYSREG_BASEPRI_MAX  0x12    /* Base Priority Max */
#define SYSREG_FAULTMASK    0x13    /* Fault Mask */
#define SYSREG_CONTROL      0x14    /* CONTROL register */

/* TrustZone banked register encodings (from Secure state) */
#define SYSREG_MSP_NS       0x88    /* Non-secure MSP */
#define SYSREG_PSP_NS       0x89    /* Non-secure PSP */
#define SYSREG_MSPLIM_NS    0x8A    /* Non-secure MSP limit */
#define SYSREG_PSPLIM_NS    0x8B    /* Non-secure PSP limit */
#define SYSREG_PRIMASK_NS   0x90    /* Non-secure PRIMASK */
#define SYSREG_BASEPRI_NS   0x91    /* Non-secure BASEPRI */
#define SYSREG_FAULTMASK_NS 0x93    /* Non-secure FAULTMASK */
#define SYSREG_CONTROL_NS   0x94    /* Non-secure CONTROL */
#define SYSREG_SP_NS        0x98    /* Alias for current NS stack */

/* Note: SAU registers are memory-mapped (0xE000EDD0-0xE000EDE0), not accessible via MRS/MSR.
 * SAU register access is handled through the memory subsystem. */

/* Hint opcodes */
#define HINT_NOP    0
#define HINT_YIELD  1
#define HINT_WFE    2
#define HINT_WFI    3
#define HINT_SEV    4
#define HINT_SEVL   5

/* Barrier opcodes */
#define BARRIER_DSB 4
#define BARRIER_DMB 5
#define BARRIER_ISB 6

/*============================================================================
 * Internal Helpers
 *============================================================================*/

/**
 * Check if currently privileged (can access special registers).
 */
static bool is_privileged(const Executor *exec)
{
    if (exec->cpu.mode == MODE_HANDLER) {
        return true;
    }
    return !(exec->cpu.control & ARMV8M_CONTROL_NPRIV);
}

/**
 * Get register value.
 */
static uint32_t get_reg(const Executor *exec, uint8_t reg)
{
    if (reg == ARMV8M_REG_SP) {
        return armv8m_get_sp(&exec->cpu);
    }
    return exec->cpu.r[reg];
}

/**
 * Set register value.
 */
static void set_reg(Executor *exec, uint8_t reg, uint32_t value)
{
    if (reg == ARMV8M_REG_SP) {
        armv8m_set_sp(&exec->cpu, value);
        exec->cpu.r[ARMV8M_REG_SP] = armv8m_get_sp(&exec->cpu);
    } else if (reg == ARMV8M_REG_PC) {
        exec->cpu.r[ARMV8M_REG_PC] = value & ~1u;
    } else {
        exec->cpu.r[reg] = value;
    }
}

/*============================================================================
 * MRS - Move from Special Register
 *============================================================================*/

int exec_mrs(Executor *exec, const DecodedInsn *insn)
{
    CPUState *cpu = &exec->cpu;
    uint8_t sysreg = insn->sysreg;
    uint32_t value = 0;
    bool privileged = is_privileged(exec);

    switch (sysreg) {
        case SYSREG_APSR:
            /* Flags only: N, Z, C, V, Q */
            value = cpu->xpsr & 0xF8000000;
            break;

        case SYSREG_IPSR:
            /* Exception number */
            value = cpu->current_exception & 0x1FF;
            break;

        case SYSREG_EPSR:
            /* Execution state - T bit and IT state */
            /* Reading EPSR returns 0 for T bit (RAZ) */
            value = 0;
            break;

        case SYSREG_IAPSR:
            value = (cpu->xpsr & 0xF8000000) | (cpu->current_exception & 0x1FF);
            break;

        case SYSREG_EAPSR:
            value = cpu->xpsr & 0xF8000000;
            break;

        case SYSREG_XPSR:
        case SYSREG_IEPSR:
            value = (cpu->xpsr & 0xF8000000) | (cpu->current_exception & 0x1FF);
            break;

        case SYSREG_MSP:
            if (privileged) {
                value = cpu->sp_main;
            }
            break;

        case SYSREG_PSP:
            if (privileged) {
                value = cpu->sp_process;
            }
            break;

        case SYSREG_PRIMASK:
            if (privileged) {
                value = cpu->primask & 1;
            }
            break;

        case SYSREG_BASEPRI:
        case SYSREG_BASEPRI_MAX:
            if (privileged) {
                value = cpu->basepri;
            }
            break;

        case SYSREG_FAULTMASK:
            if (privileged) {
                value = cpu->faultmask & 1;
            }
            break;

        case SYSREG_CONTROL:
            value = cpu->control & 0x7;  /* nPRIV, SPSEL, FPCA */
            break;

        case SYSREG_MSPLIM:
            if (privileged) {
                value = cpu->msplim;
            }
            break;

        case SYSREG_PSPLIM:
            if (privileged) {
                value = cpu->psplim;
            }
            break;

        /* TrustZone banked registers - only accessible from Secure state */
        case SYSREG_MSP_NS:
            if (privileged && exec->has_trustzone && cpu->security == SECURITY_SECURE) {
                value = exec->tz_regs.msp_ns;
            }
            break;

        case SYSREG_PSP_NS:
            if (privileged && exec->has_trustzone && cpu->security == SECURITY_SECURE) {
                value = exec->tz_regs.psp_ns;
            }
            break;

        case SYSREG_MSPLIM_NS:
            if (privileged && exec->has_trustzone && cpu->security == SECURITY_SECURE) {
                value = exec->tz_regs.msplim_ns;
            }
            break;

        case SYSREG_PSPLIM_NS:
            if (privileged && exec->has_trustzone && cpu->security == SECURITY_SECURE) {
                value = exec->tz_regs.psplim_ns;
            }
            break;

        case SYSREG_PRIMASK_NS:
            if (privileged && exec->has_trustzone && cpu->security == SECURITY_SECURE) {
                value = exec->tz_regs.primask_ns & 1;
            }
            break;

        case SYSREG_BASEPRI_NS:
            if (privileged && exec->has_trustzone && cpu->security == SECURITY_SECURE) {
                value = exec->tz_regs.basepri_ns;
            }
            break;

        case SYSREG_FAULTMASK_NS:
            if (privileged && exec->has_trustzone && cpu->security == SECURITY_SECURE) {
                value = exec->tz_regs.faultmask_ns & 1;
            }
            break;

        case SYSREG_CONTROL_NS:
            if (privileged && exec->has_trustzone && cpu->security == SECURITY_SECURE) {
                value = exec->tz_regs.control_ns & 0x7;
            }
            break;

        case SYSREG_SP_NS:
            if (privileged && exec->has_trustzone && cpu->security == SECURITY_SECURE) {
                /* Return current NS stack based on NS CONTROL.SPSEL */
                if (exec->tz_regs.control_ns & ARMV8M_CONTROL_SPSEL) {
                    value = exec->tz_regs.psp_ns;
                } else {
                    value = exec->tz_regs.msp_ns;
                }
            }
            break;

        default:
            /* Unknown register - return 0 */
            value = 0;
            break;
    }

    set_reg(exec, insn->rd, value);
    return ARMV8M_OK;
}

/*============================================================================
 * MSR - Move to Special Register
 *============================================================================*/

int exec_msr(Executor *exec, const DecodedInsn *insn)
{
    CPUState *cpu = &exec->cpu;
    uint8_t sysreg = insn->sysreg;
    uint32_t value = get_reg(exec, insn->rn);
    bool privileged = is_privileged(exec);

    switch (sysreg) {
        case SYSREG_APSR:
            /* Write flags only: N, Z, C, V, Q */
            cpu->xpsr = (cpu->xpsr & ~0xF8000000) | (value & 0xF8000000);
            break;

        case SYSREG_IPSR:
        case SYSREG_EPSR:
        case SYSREG_IEPSR:
            /* IPSR and EPSR are read-only */
            break;

        case SYSREG_IAPSR:
        case SYSREG_EAPSR:
        case SYSREG_XPSR:
            /* Only APSR bits (flags) are writable */
            cpu->xpsr = (cpu->xpsr & ~0xF8000000) | (value & 0xF8000000);
            break;

        case SYSREG_MSP:
            if (privileged) {
                cpu->sp_main = value & ~3u;  /* Word aligned */
                if (cpu->mode == MODE_HANDLER ||
                    !(cpu->control & ARMV8M_CONTROL_SPSEL)) {
                    cpu->r[ARMV8M_REG_SP] = cpu->sp_main;
                }
            }
            break;

        case SYSREG_PSP:
            if (privileged) {
                cpu->sp_process = value & ~3u;
                if (cpu->mode == MODE_THREAD &&
                    (cpu->control & ARMV8M_CONTROL_SPSEL)) {
                    cpu->r[ARMV8M_REG_SP] = cpu->sp_process;
                }
            }
            break;

        case SYSREG_PRIMASK:
            if (privileged) {
                cpu->primask = value & 1;
            }
            break;

        case SYSREG_BASEPRI:
            if (privileged) {
                cpu->basepri = value & 0xFF;
            }
            break;

        case SYSREG_BASEPRI_MAX:
            if (privileged) {
                /* Only write if new value is higher priority (lower number) */
                /* or if current BASEPRI is 0 */
                if (cpu->basepri == 0 || (value & 0xFF) < cpu->basepri) {
                    cpu->basepri = value & 0xFF;
                }
            }
            break;

        case SYSREG_FAULTMASK:
            if (privileged && cpu->mode == MODE_HANDLER) {
                cpu->faultmask = value & 1;
            }
            break;

        case SYSREG_CONTROL:
            if (privileged) {
                /* In Handler mode, SPSEL is ignored (always uses MSP) */
                uint32_t mask = ARMV8M_CONTROL_NPRIV;
                if (cpu->mode == MODE_THREAD) {
                    mask |= ARMV8M_CONTROL_SPSEL;
                }
                cpu->control = (cpu->control & ~mask) | (value & mask);

                /* Update R13 to reflect new SP selection */
                if (cpu->mode == MODE_THREAD) {
                    if (cpu->control & ARMV8M_CONTROL_SPSEL) {
                        cpu->r[ARMV8M_REG_SP] = cpu->sp_process;
                    } else {
                        cpu->r[ARMV8M_REG_SP] = cpu->sp_main;
                    }
                }
            }
            break;

        case SYSREG_MSPLIM:
            if (privileged) {
                /* Stack limit must be 8-byte aligned */
                cpu->msplim = value & ~7u;
            }
            break;

        case SYSREG_PSPLIM:
            if (privileged) {
                /* Stack limit must be 8-byte aligned */
                cpu->psplim = value & ~7u;
            }
            break;

        /* TrustZone banked registers - only accessible from Secure state */
        case SYSREG_MSP_NS:
            if (privileged && exec->has_trustzone && cpu->security == SECURITY_SECURE) {
                exec->tz_regs.msp_ns = value & ~3u;  /* Word aligned */
            }
            break;

        case SYSREG_PSP_NS:
            if (privileged && exec->has_trustzone && cpu->security == SECURITY_SECURE) {
                exec->tz_regs.psp_ns = value & ~3u;  /* Word aligned */
            }
            break;

        case SYSREG_MSPLIM_NS:
            if (privileged && exec->has_trustzone && cpu->security == SECURITY_SECURE) {
                exec->tz_regs.msplim_ns = value & ~7u;  /* 8-byte aligned */
            }
            break;

        case SYSREG_PSPLIM_NS:
            if (privileged && exec->has_trustzone && cpu->security == SECURITY_SECURE) {
                exec->tz_regs.psplim_ns = value & ~7u;  /* 8-byte aligned */
            }
            break;

        case SYSREG_PRIMASK_NS:
            if (privileged && exec->has_trustzone && cpu->security == SECURITY_SECURE) {
                exec->tz_regs.primask_ns = value & 1;
            }
            break;

        case SYSREG_BASEPRI_NS:
            if (privileged && exec->has_trustzone && cpu->security == SECURITY_SECURE) {
                exec->tz_regs.basepri_ns = value & 0xFF;
            }
            break;

        case SYSREG_FAULTMASK_NS:
            if (privileged && exec->has_trustzone && cpu->security == SECURITY_SECURE) {
                exec->tz_regs.faultmask_ns = value & 1;
            }
            break;

        case SYSREG_CONTROL_NS:
            if (privileged && exec->has_trustzone && cpu->security == SECURITY_SECURE) {
                exec->tz_regs.control_ns = value & 0x7;  /* nPRIV, SPSEL, FPCA */
            }
            break;

        case SYSREG_SP_NS:
            if (privileged && exec->has_trustzone && cpu->security == SECURITY_SECURE) {
                /* Write to current NS stack based on NS CONTROL.SPSEL */
                if (exec->tz_regs.control_ns & ARMV8M_CONTROL_SPSEL) {
                    exec->tz_regs.psp_ns = value & ~3u;
                } else {
                    exec->tz_regs.msp_ns = value & ~3u;
                }
            }
            break;

        default:
            /* Unknown register - ignore */
            break;
    }

    return ARMV8M_OK;
}

/*============================================================================
 * CPS - Change Processor State
 *============================================================================*/

int exec_cps(Executor *exec, const DecodedInsn *insn)
{
    CPUState *cpu = &exec->cpu;

    /* CPS is only effective in privileged mode */
    if (!is_privileged(exec)) {
        return ARMV8M_OK;
    }

    /* insn->imm contains the effect (enable=0, disable=1) and flags */
    bool disable = (insn->imm >> 4) & 1;
    bool affect_i = (insn->imm >> 1) & 1;  /* PRIMASK */
    bool affect_f = insn->imm & 1;          /* FAULTMASK */

    if (affect_i) {
        cpu->primask = disable ? 1 : 0;
    }

    if (affect_f && cpu->mode == MODE_HANDLER) {
        cpu->faultmask = disable ? 1 : 0;
    }

    return ARMV8M_OK;
}

/*============================================================================
 * Barrier Instructions
 *============================================================================*/

int exec_barrier(Executor *exec, const DecodedInsn *insn)
{
    /* In an emulator, barriers are typically NOPs since we don't have
     * out-of-order execution or caches to synchronize.
     * However, ISB should flush any prefetch buffer simulation. */

    (void)exec;

    switch (insn->op) {
        case BARRIER_DSB:
            /* Data Synchronization Barrier */
            break;

        case BARRIER_DMB:
            /* Data Memory Barrier */
            break;

        case BARRIER_ISB:
            /* Instruction Synchronization Barrier */
            /* Would flush pipeline/prefetch if we had one */
            break;

        default:
            break;
    }

    return ARMV8M_OK;
}

/*============================================================================
 * Hint Instructions
 *============================================================================*/

int exec_hint(Executor *exec, const DecodedInsn *insn)
{
    CPUState *cpu = &exec->cpu;

    switch (insn->op) {
        case HINT_NOP:
            /* No operation */
            break;

        case HINT_YIELD:
            /* Yield to other threads - NOP in single-threaded emulator */
            break;

        case HINT_WFE:
            /* Wait For Event */
            if (cpu->event_registered) {
                /* Event already registered, clear it and continue */
                cpu->event_registered = false;
            } else {
                /* Enter sleep until event */
                cpu->sleeping = true;
            }
            break;

        case HINT_WFI:
            /* Wait For Interrupt */
            cpu->sleeping = true;
            break;

        case HINT_SEV:
            /* Send Event - set event flag for other processors */
            /* In single-processor emulator, just set our own flag */
            cpu->event_registered = true;
            break;

        case HINT_SEVL:
            /* Send Event Local - like SEV but only to this processor */
            cpu->event_registered = true;
            break;

        default:
            /* Unknown hints are NOPs */
            break;
    }

    return ARMV8M_OK;
}

/*============================================================================
 * IT Instruction
 *============================================================================*/

int exec_it(Executor *exec, const DecodedInsn *insn)
{
    /* IT instruction sets up conditional execution block */
    /* it_state = firstcond[3:0] : mask[3:0] */
    exec->cpu.it_state = (insn->it_cond << 4) | (insn->it_mask & 0xF);

    return ARMV8M_OK;
}

/*============================================================================
 * SVC - Supervisor Call
 *============================================================================*/

int exec_svc(Executor *exec, const DecodedInsn *insn)
{
    (void)insn;  /* SVC number is in insn->imm, but not used for entry */

    /* Trigger SVCall exception */
    return armv8m_exception_entry(exec, ARMV8M_EXC_SVCALL);
}

/*============================================================================
 * Coprocessor Instructions
 *============================================================================*/

/**
 * MCR - Move to Coprocessor from ARM Register
 *
 * In ARMv8-M, the only standard coprocessor is CP10/CP11 for FPU access.
 * The decoder routes CP10/CP11 accesses to the VFP handler, so this function
 * only receives non-VFP coprocessor accesses, which always fault.
 *
 * Note: insn->imm encodes (opc1 << 4) | opc2, and insn->rd is the ARM register.
 */
int exec_mcr(Executor *exec, const DecodedInsn *insn)
{
    (void)insn;

    /* Non-VFP coprocessor access - always faults with NOCP.
     * FPU coprocessor (CP10/CP11) accesses are decoded as VFP instructions
     * and handled in exec_fpu.c, not here. */
    exec->cpu.cfsr |= ARMV8M_UFSR_NOCP;
    return ARMV8M_ERR_USAGE_FAULT;
}

/**
 * MRC - Move to ARM Register from Coprocessor
 *
 * In ARMv8-M, the only standard coprocessor is CP10/CP11 for FPU access.
 * The decoder routes CP10/CP11 accesses to the VFP handler, so this function
 * only receives non-VFP coprocessor accesses, which always fault.
 *
 * Note: insn->imm encodes (opc1 << 4) | opc2, and insn->rd is the destination.
 */
int exec_mrc(Executor *exec, const DecodedInsn *insn)
{
    (void)insn;

    /* Non-VFP coprocessor access - always faults with NOCP.
     * FPU coprocessor (CP10/CP11) accesses are decoded as VFP instructions
     * and handled in exec_fpu.c, not here. */
    exec->cpu.cfsr |= ARMV8M_UFSR_NOCP;
    return ARMV8M_ERR_USAGE_FAULT;
}

/*============================================================================
 * TrustZone Test Target Instructions
 *============================================================================*/

/* Forward declaration for security check */
extern SecurityAttr armv8m_check_security(const Executor *exec, uint32_t addr);

/**
 * TT - Test Target (TrustZone)
 *
 * Returns security attributes of the target address.
 * Variants (encoded in insn->op):
 *   TT   (op=0) - Test target from current security, unprivileged check
 *   TTT  (op=1) - Test target from current security, privileged check
 *   TTA  (op=2) - Test target from alternate security, unprivileged check
 *   TTAT (op=3) - Test target from alternate security, privileged check
 */
int exec_tt(Executor *exec, const DecodedInsn *insn)
{
    if (!exec->has_trustzone) {
        /* No TrustZone - TT is undefined */
        return ARMV8M_ERR_UNDEFINED_INSN;
    }

    uint32_t addr = exec->cpu.r[insn->rn];
    uint32_t result = 0;

    /* Decode variant from op field */
    bool is_alt = (insn->op & 2) != 0;      /* TTA/TTAT query alternate domain */
    bool is_priv = (insn->op & 1) != 0;     /* TTT/TTAT query privileged access */

    /* Determine which security state to query */
    SecurityState query_state;
    if (is_alt) {
        /* Query alternate security state */
        query_state = (exec->cpu.security == SECURITY_SECURE)
                      ? SECURITY_NONSECURE : SECURITY_SECURE;
    } else {
        query_state = exec->cpu.security;
    }

    /* TTA/TTAT from Non-Secure can only query Non-Secure (security restriction) */
    if (exec->cpu.security == SECURITY_NONSECURE && query_state == SECURITY_SECURE) {
        /* Cannot query secure attributes from non-secure */
        query_state = SECURITY_NONSECURE;
    }

    /* Get security attributes of the address */
    SecurityAttr attr = armv8m_check_security(exec, addr);

    /*
     * TT result format:
     * bits[31:24] = IREGION (MPU region, if valid)
     * bit[23] = IRVALID (IREGION valid)
     * bits[22:17] = 0
     * bit[16] = S (Secure: 0=NS, 1=S)
     * bits[15:8] = SREGION (SAU region, if valid)
     * bit[7] = SRVALID (SREGION valid)
     * bit[6] = NSC (Non-Secure Callable)
     * bit[5] = NS (Non-Secure)
     * bits[4:2] = 0
     * bit[1] = RW (Read-Write allowed)
     * bit[0] = R (Read allowed)
     */

    /* Set security bits based on queried attributes */
    if (attr == SEC_SECURE) {
        result |= (1U << 16);  /* S bit = 1 (Secure) */
        /* NS bit stays 0 */
    } else if (attr == SEC_NSC) {
        result |= (1U << 6);   /* NSC bit */
        result |= (1U << 5);   /* NS bit */
    } else {
        /* SEC_NONSECURE */
        result |= (1U << 5);   /* NS bit */
    }

    /* Find matching SAU region (if any) for SREGION field */
    for (uint32_t i = 0; i < ARMV8M_SAU_REGIONS_MAX; i++) {
        const SAURegion *region = &exec->sau.regions[i];
        if (!(region->rlar & ARMV8M_SAU_RLAR_ENABLE)) {
            continue;
        }
        uint32_t base = region->rbar & 0xFFFFFFE0U;
        uint32_t limit = (region->rlar & 0xFFFFFFE0U) | 0x1FU;
        if (addr >= base && addr <= limit) {
            result |= (i << 8);      /* SREGION */
            result |= (1U << 7);     /* SRVALID */
            break;
        }
    }

    /* Determine R/RW bits based on privilege level being queried */
    bool check_privileged;
    if (is_priv) {
        /* TTT/TTAT: Always check privileged access */
        check_privileged = true;
    } else {
        /* TT/TTA: Check based on current thread privilege */
        check_privileged = (exec->cpu.mode == MODE_HANDLER) ||
                          !(exec->cpu.control & ARMV8M_CONTROL_NPRIV);
    }

    /* MPU region lookup for IREGION field.
     *
     * TODO: For full TT instruction support, the executor would need:
     * 1. A pointer to the MPU state, OR
     * 2. A callback to query MPU regions
     *
     * Currently, the executor doesn't have direct MPU access (MPU is managed
     * by the memory subsystem). For now, IREGION is always 0 and IRVALID is 0.
     *
     * When MPU integration is added, this should:
     * 1. Find the matching MPU region (if MPU is enabled)
     * 2. Set IREGION to the region number
     * 3. Set IRVALID if a region matches
     * 4. Check MPU permissions for R/RW bits
     */
    if (exec->num_mpu_regions > 0) {
        /* MPU is present but we can't query it directly.
         * Leave IREGION=0, IRVALID=0 for now. */
    }

    /* For R/RW bits, without MPU access we use a simplified model:
     * - Privileged access gets R+RW
     * - Unprivileged access gets R only
     * With MPU integration, this should check actual MPU permissions. */
    if (check_privileged) {
        result |= 0x3;  /* R and RW bits */
    } else {
        result |= 0x1;  /* R bit only */
    }

    exec->cpu.r[insn->rd] = result;
    return ARMV8M_OK;
}
