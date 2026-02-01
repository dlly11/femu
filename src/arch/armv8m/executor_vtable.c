/**
 * @file executor_vtable.c
 * @brief ARMv8-M executor vtable implementation
 *
 * Implements the abstract EmuExecutor interface for ARMv8-M architecture.
 */

#include "arch/armv8m/armv8m_executor_vtable.h"
#include <string.h>

/*============================================================================
 * VTable Implementation
 *============================================================================*/

static void armv8m_executor_destroy(EmuExecutor *exec)
{
    /* Nothing to free - we don't own any dynamic resources */
    (void)exec;
}

static void armv8m_executor_reset(EmuExecutor *exec, uint64_t reset_vector)
{
    if (!exec) {
        return;
    }
    ARMv8MExecutorWrapper *wrapper = armv8m_executor_from_base(exec);
    armv8m_exec_reset(&wrapper->exec, (uint32_t)reset_vector);
}

static void armv8m_executor_set_memory(EmuExecutor *exec, const EmuMemoryCallbacks *mem)
{
    if (!exec || !mem) {
        return;
    }

    ARMv8MExecutorWrapper *wrapper = armv8m_executor_from_base(exec);

    /* Convert 64-bit callbacks to 32-bit ARM callbacks */
    /* Note: This requires wrapper functions for proper conversion */
    wrapper->exec.mem.ctx = mem->ctx;
    /* The callback signatures differ, so we store the original and use wrappers */
    /* For now, just store the context - full integration would need wrapper callbacks */
    (void)mem;
}

static EmuCPU *armv8m_executor_get_cpu(EmuExecutor *exec)
{
    if (!exec) {
        return NULL;
    }
    ARMv8MExecutorWrapper *wrapper = armv8m_executor_from_base(exec);
    return &wrapper->cpu_wrapper.base;
}

static EmuDecoder *armv8m_executor_get_decoder(EmuExecutor *exec)
{
    if (!exec) {
        return NULL;
    }
    ARMv8MExecutorWrapper *wrapper = armv8m_executor_from_base(exec);
    return &wrapper->dec_wrapper.base;
}

static int armv8m_executor_exec_insn(EmuExecutor *exec, const EmuDecodedInsn *insn)
{
    if (!exec || !insn || !insn->arch_insn) {
        return EMU_ERR_INVALID_PARAM;
    }

    ARMv8MExecutorWrapper *wrapper = armv8m_executor_from_base(exec);
    const DecodedInsn *arm_insn = (const DecodedInsn *)insn->arch_insn;

    return armv8m_exec_insn(&wrapper->exec, arm_insn);
}

static int armv8m_executor_step(EmuExecutor *exec)
{
    if (!exec) {
        return EMU_ERR_INVALID_PARAM;
    }
    ARMv8MExecutorWrapper *wrapper = armv8m_executor_from_base(exec);
    return armv8m_exec_step(&wrapper->exec);
}

static int64_t armv8m_executor_run(EmuExecutor *exec, uint64_t max_cycles)
{
    if (!exec) {
        return EMU_ERR_INVALID_PARAM;
    }
    ARMv8MExecutorWrapper *wrapper = armv8m_executor_from_base(exec);
    return armv8m_exec_run(&wrapper->exec, max_cycles);
}

static bool armv8m_executor_should_stop(EmuExecutor *exec, uint64_t addr)
{
    /* Default implementation - no breakpoints at executor level */
    (void)exec;
    (void)addr;
    return false;
}

static int armv8m_executor_handle_interrupts(EmuExecutor *exec)
{
    if (!exec) {
        return 0;
    }

    ARMv8MExecutorWrapper *wrapper = armv8m_executor_from_base(exec);

    /* Check for pending exceptions via NVIC callback */
    if (wrapper->exec.nvic.get_pending) {
        int pending = wrapper->exec.nvic.get_pending(wrapper->exec.nvic.ctx);
        if (pending > 0) {
            /* Exception entry is handled internally by exec_step */
            return pending;
        }
    }
    return 0;
}

/*============================================================================
 * Static VTable
 *============================================================================*/

static const EmuExecutorVTable armv8m_executor_vtable = {
    .destroy = armv8m_executor_destroy,
    .reset = armv8m_executor_reset,
    .set_memory = armv8m_executor_set_memory,
    .get_cpu = armv8m_executor_get_cpu,
    .get_decoder = armv8m_executor_get_decoder,
    .exec_insn = armv8m_executor_exec_insn,
    .step = armv8m_executor_step,
    .run = armv8m_executor_run,
    .should_stop = armv8m_executor_should_stop,
    .handle_interrupts = armv8m_executor_handle_interrupts,
};

/*============================================================================
 * Public API
 *============================================================================*/

const EmuExecutorVTable *armv8m_executor_get_vtable(void)
{
    return &armv8m_executor_vtable;
}

void armv8m_executor_vtable_init(ARMv8MExecutorWrapper *wrapper)
{
    if (!wrapper) {
        return;
    }

    memset(wrapper, 0, sizeof(*wrapper));

    /* Initialize base EmuExecutor */
    wrapper->base.vtable = &armv8m_executor_vtable;
    wrapper->base.arch = EMU_ARCH_ARMV8M;
    wrapper->base.arch_state = wrapper;

    /* Initialize the ARM executor */
    armv8m_exec_init(&wrapper->exec);

    /* Initialize the CPU wrapper to point to our executor */
    armv8m_cpu_init(&wrapper->cpu_wrapper, &wrapper->exec);

    /* Initialize the decoder wrapper */
    armv8m_decoder_vtable_init(&wrapper->dec_wrapper);
}
