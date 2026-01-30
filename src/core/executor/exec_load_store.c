/**
 * @file exec_load_store.c
 * @brief Load/store instruction execution for ARMv8-M
 *
 * Implements LDR, STR, LDM, STM, PUSH, POP with all addressing modes.
 */

#include "armv8m_executor.h"
#include "armv8m_types.h"

/*============================================================================
 * Internal Helpers
 *============================================================================*/

/**
 * Check if address is unaligned for the given access size.
 */
static bool is_unaligned(uint32_t addr, uint8_t size)
{
    return (addr & (size - 1)) != 0;
}

/**
 * Read from memory with proper access context.
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
 * Write to memory with proper access context.
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
 * Get register value, handling SP specially.
 */
static uint32_t get_reg(const Executor *exec, uint8_t reg)
{
    if (reg == ARMV8M_REG_SP) {
        return armv8m_get_sp(&exec->cpu);
    }
    return exec->cpu.r[reg];
}

/**
 * Set register value, handling SP specially.
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

/**
 * Sign-extend a loaded value based on size.
 */
static uint32_t sign_extend(uint32_t value, AccessSize size)
{
    switch (size) {
        case ACCESS_BYTE:
            return (uint32_t)(int32_t)(int8_t)(value & 0xFF);
        case ACCESS_HALF:
            return (uint32_t)(int32_t)(int16_t)(value & 0xFFFF);
        default:
            return value;
    }
}

/**
 * Count number of bits set in register list.
 */
static uint32_t popcount16(uint16_t value)
{
    uint32_t count = 0;
    while (value) {
        count += value & 1;
        value >>= 1;
    }
    return count;
}

/*============================================================================
 * Load Instructions
 *============================================================================*/

int exec_load_imm(Executor *exec, const DecodedInsn *insn)
{
    uint32_t base = get_reg(exec, insn->rn);
    uint32_t offset = insn->imm;
    uint32_t addr;
    bool fault = false;

    /* Calculate address based on addressing mode */
    if (insn->add) {
        addr = insn->pre_index ? (base + offset) : base;
    } else {
        addr = insn->pre_index ? (base - offset) : base;
    }

    /* Check for unaligned access if trapping is enabled */
    if ((exec->cpu.ccr & ARMV8M_CCR_UNALIGN_TRP) &&
        is_unaligned(addr, insn->access_size)) {
        exec->cpu.cfsr |= ARMV8M_UFSR_UNALIGNED;
        return ARMV8M_ERR_USAGE_FAULT;
    }

    /* Read from memory */
    uint32_t value = mem_read(exec, addr, insn->access_size, &fault);

    if (fault) {
        return ARMV8M_ERR_BUS_FAULT;
    }

    /* Sign extend if needed */
    if (insn->is_signed) {
        value = sign_extend(value, insn->access_size);
    }

    /* Store to destination register */
    set_reg(exec, insn->rt, value);

    /* LDRD: Load second word to Rt2 if specified */
    if (insn->rt2 != ARMV8M_REG_NONE) {
        uint32_t value2 = mem_read(exec, addr + 4, ACCESS_WORD, &fault);
        if (fault) {
            return ARMV8M_ERR_BUS_FAULT;
        }
        set_reg(exec, insn->rt2, value2);
    }

    /* Writeback if specified */
    if (insn->writeback || insn->wback) {
        uint32_t wb_addr;
        if (insn->add) {
            wb_addr = base + offset;
        } else {
            wb_addr = base - offset;
        }
        set_reg(exec, insn->rn, wb_addr);
    }

    return ARMV8M_OK;
}

int exec_load_reg(Executor *exec, const DecodedInsn *insn)
{
    uint32_t base = get_reg(exec, insn->rn);
    uint32_t index = get_reg(exec, insn->rm);
    bool fault = false;

    /* Apply shift to index register */
    if (insn->shift_amount > 0) {
        index <<= insn->shift_amount;
    }

    /* Calculate address */
    uint32_t addr;
    if (insn->add) {
        addr = base + index;
    } else {
        addr = base - index;
    }

    /* Check for unaligned access if trapping is enabled */
    if ((exec->cpu.ccr & ARMV8M_CCR_UNALIGN_TRP) &&
        is_unaligned(addr, insn->access_size)) {
        exec->cpu.cfsr |= ARMV8M_UFSR_UNALIGNED;
        return ARMV8M_ERR_USAGE_FAULT;
    }

    /* Read from memory */
    uint32_t value = mem_read(exec, addr, insn->access_size, &fault);

    if (fault) {
        return ARMV8M_ERR_BUS_FAULT;
    }

    /* Sign extend if needed */
    if (insn->is_signed) {
        value = sign_extend(value, insn->access_size);
    }

    /* Store to destination register */
    set_reg(exec, insn->rt, value);

    return ARMV8M_OK;
}

int exec_load_literal(Executor *exec, const DecodedInsn *insn)
{
    /* PC-relative load: address = Align(PC + 4, 4) + imm
     * ARM architecture: PC reads as current instruction address + 4 in Thumb mode */
    uint32_t pc = exec->cpu.r[ARMV8M_REG_PC] + 4;
    uint32_t base = pc & ~3u;  /* Align to word */
    uint32_t addr;
    bool fault = false;

    if (insn->add) {
        addr = base + insn->imm;
    } else {
        addr = base - insn->imm;
    }

    /* Read from memory */
    uint32_t value = mem_read(exec, addr, insn->access_size, &fault);

    if (fault) {
        return ARMV8M_ERR_BUS_FAULT;
    }

    /* Sign extend if needed */
    if (insn->is_signed) {
        value = sign_extend(value, insn->access_size);
    }

    /* Store to destination register */
    set_reg(exec, insn->rt, value);

    return ARMV8M_OK;
}

/*============================================================================
 * Store Instructions
 *============================================================================*/

int exec_store_imm(Executor *exec, const DecodedInsn *insn)
{
    uint32_t base = get_reg(exec, insn->rn);
    uint32_t offset = insn->imm;
    uint32_t value = get_reg(exec, insn->rt);
    uint32_t addr;
    bool fault = false;

    /* Calculate address based on addressing mode */
    if (insn->add) {
        addr = insn->pre_index ? (base + offset) : base;
    } else {
        addr = insn->pre_index ? (base - offset) : base;
    }

    /* Check for unaligned access if trapping is enabled */
    if ((exec->cpu.ccr & ARMV8M_CCR_UNALIGN_TRP) &&
        is_unaligned(addr, insn->access_size)) {
        exec->cpu.cfsr |= ARMV8M_UFSR_UNALIGNED;
        return ARMV8M_ERR_USAGE_FAULT;
    }

    /* Write to memory */
    mem_write(exec, addr, value, insn->access_size, &fault);

    if (fault) {
        return ARMV8M_ERR_BUS_FAULT;
    }

    /* STRD: Store second word from Rt2 if specified */
    if (insn->rt2 != ARMV8M_REG_NONE) {
        uint32_t value2 = get_reg(exec, insn->rt2);
        mem_write(exec, addr + 4, value2, ACCESS_WORD, &fault);
        if (fault) {
            return ARMV8M_ERR_BUS_FAULT;
        }
    }

    /* Writeback if specified */
    if (insn->writeback || insn->wback) {
        uint32_t wb_addr;
        if (insn->add) {
            wb_addr = base + offset;
        } else {
            wb_addr = base - offset;
        }
        set_reg(exec, insn->rn, wb_addr);
    }

    return ARMV8M_OK;
}

int exec_store_reg(Executor *exec, const DecodedInsn *insn)
{
    uint32_t base = get_reg(exec, insn->rn);
    uint32_t index = get_reg(exec, insn->rm);
    uint32_t value = get_reg(exec, insn->rt);
    bool fault = false;

    /* Apply shift to index register */
    if (insn->shift_amount > 0) {
        index <<= insn->shift_amount;
    }

    /* Calculate address */
    uint32_t addr;
    if (insn->add) {
        addr = base + index;
    } else {
        addr = base - index;
    }

    /* Check for unaligned access if trapping is enabled */
    if ((exec->cpu.ccr & ARMV8M_CCR_UNALIGN_TRP) &&
        is_unaligned(addr, insn->access_size)) {
        exec->cpu.cfsr |= ARMV8M_UFSR_UNALIGNED;
        return ARMV8M_ERR_USAGE_FAULT;
    }

    /* Write to memory */
    mem_write(exec, addr, value, insn->access_size, &fault);

    if (fault) {
        return ARMV8M_ERR_BUS_FAULT;
    }

    return ARMV8M_OK;
}

/*============================================================================
 * Load/Store Multiple
 *============================================================================*/

int exec_load_multiple(Executor *exec, const DecodedInsn *insn)
{
    uint32_t base = get_reg(exec, insn->rn);
    uint16_t reglist = insn->register_list;
    uint32_t num_regs = popcount16(reglist);
    uint32_t addr = base;
    bool fault = false;

    /* For LDMDB/LDMEA, decrement first */
    if (!insn->add) {
        addr = base - (num_regs * 4);
    }

    /* Load registers in order */
    for (uint8_t i = 0; i < 16; i++) {
        if (reglist & (1 << i)) {
            uint32_t value = mem_read(exec, addr, ACCESS_WORD, &fault);
            if (fault) {
                return ARMV8M_ERR_BUS_FAULT;
            }
            set_reg(exec, i, value);
            addr += 4;
        }
    }

    /* Writeback */
    if (insn->writeback || insn->wback) {
        uint32_t wb_addr;
        if (insn->add) {
            wb_addr = base + (num_regs * 4);
        } else {
            wb_addr = base - (num_regs * 4);
        }
        /* Don't writeback if base register was in the list */
        if (!(reglist & (1 << insn->rn))) {
            set_reg(exec, insn->rn, wb_addr);
        }
    }

    return ARMV8M_OK;
}

int exec_store_multiple(Executor *exec, const DecodedInsn *insn)
{
    uint32_t base = get_reg(exec, insn->rn);
    uint16_t reglist = insn->register_list;
    uint32_t num_regs = popcount16(reglist);
    uint32_t addr = base;
    bool fault = false;

    /* For STMDB/STMFD (PUSH), decrement first */
    if (!insn->add) {
        addr = base - (num_regs * 4);
    }

    /* Check stack limit for PUSH operations (when base is SP) */
    if (insn->rn == ARMV8M_REG_SP && !insn->add) {
        if (armv8m_check_stack_limit(&exec->cpu, addr)) {
            exec->cpu.cfsr |= ARMV8M_UFSR_STKOF;
            return ARMV8M_ERR_USAGE_FAULT;
        }
    }

    /* Store registers in order */
    for (uint8_t i = 0; i < 16; i++) {
        if (reglist & (1 << i)) {
            uint32_t value = get_reg(exec, i);
            mem_write(exec, addr, value, ACCESS_WORD, &fault);
            if (fault) {
                return ARMV8M_ERR_BUS_FAULT;
            }
            addr += 4;
        }
    }

    /* Writeback */
    if (insn->writeback || insn->wback) {
        uint32_t wb_addr;
        if (insn->add) {
            wb_addr = base + (num_regs * 4);
        } else {
            wb_addr = base - (num_regs * 4);
        }
        set_reg(exec, insn->rn, wb_addr);
    }

    return ARMV8M_OK;
}

/*============================================================================
 * Exclusive Load/Store Instructions
 *============================================================================*/

int exec_load_exclusive(Executor *exec, const DecodedInsn *insn)
{
    uint32_t base = get_reg(exec, insn->rn);
    uint32_t offset = insn->imm;
    uint32_t addr;
    bool fault = false;

    /* Calculate address */
    if (insn->add) {
        addr = base + offset;
    } else {
        addr = base - offset;
    }

    /* Determine access size (byte, halfword, or word) */
    uint8_t size = insn->access_size;
    if (size == 0) {
        size = ACCESS_WORD;  /* Default to word for LDREX */
    }

    /* Check alignment based on access size */
    if (size == ACCESS_WORD && (addr & 3)) {
        return ARMV8M_ERR_USAGE_FAULT;
    } else if (size == ACCESS_HALF && (addr & 1)) {
        return ARMV8M_ERR_USAGE_FAULT;
    }
    /* Byte access has no alignment requirement */

    /* Load from memory */
    uint32_t value = mem_read(exec, addr, size, &fault);

    if (fault) {
        return ARMV8M_ERR_BUS_FAULT;
    }

    /* Set exclusive monitor */
    exec->cpu.exclusive_addr = addr;
    exec->cpu.exclusive_valid = true;

    /* Write loaded value to Rt */
    set_reg(exec, insn->rt, value);

    return ARMV8M_OK;
}

int exec_store_exclusive(Executor *exec, const DecodedInsn *insn)
{
    uint32_t base = get_reg(exec, insn->rn);
    uint32_t offset = insn->imm;
    uint32_t addr;
    bool fault = false;

    /* Calculate address */
    if (insn->add) {
        addr = base + offset;
    } else {
        addr = base - offset;
    }

    /* Determine access size (byte, halfword, or word) */
    uint8_t size = insn->access_size;
    if (size == 0) {
        size = ACCESS_WORD;  /* Default to word for STREX */
    }

    /* Check alignment based on access size */
    if (size == ACCESS_WORD && (addr & 3)) {
        return ARMV8M_ERR_USAGE_FAULT;
    } else if (size == ACCESS_HALF && (addr & 1)) {
        return ARMV8M_ERR_USAGE_FAULT;
    }
    /* Byte access has no alignment requirement */

    uint32_t status;

    if (exec->cpu.exclusive_valid && exec->cpu.exclusive_addr == addr) {
        /* Exclusive monitor valid and address matches - store succeeds */
        uint32_t value = get_reg(exec, insn->rt);
        mem_write(exec, addr, value, size, &fault);

        if (fault) {
            exec->cpu.exclusive_valid = false;
            return ARMV8M_ERR_BUS_FAULT;
        }

        status = 0;  /* Success */
    } else {
        /* Exclusive monitor invalid or address mismatch - store fails */
        status = 1;  /* Failure */
    }

    /* Clear exclusive monitor */
    exec->cpu.exclusive_valid = false;

    /* Write status to Rd */
    set_reg(exec, insn->rd, status);

    return ARMV8M_OK;
}

int exec_clear_exclusive(Executor *exec, const DecodedInsn *insn)
{
    (void)insn;  /* CLREX has no operands */

    /* Clear the exclusive access monitor.
     * This is typically used when context switching or when the software
     * decides to abandon an exclusive access sequence. */
    exec->cpu.exclusive_valid = false;

    return ARMV8M_OK;
}

/*============================================================================
 * Load-Acquire / Store-Release Instructions (ARMv8-M)
 *============================================================================*/

int exec_load_acquire(Executor *exec, const DecodedInsn *insn)
{
    /* LDA/LDAB/LDAH - Load with acquire semantics.
     * Also handles LDAEX/LDAEXB/LDAEXH (exclusive variants).
     * In single-threaded execution, acquire semantics are a no-op.
     *
     * insn->op: 0 = non-exclusive, 1 = exclusive
     * insn->access_size: byte/half/word */
    uint32_t base = get_reg(exec, insn->rn);
    bool fault = false;

    /* Determine access size */
    uint8_t size = insn->access_size;
    if (size == 0) {
        size = ACCESS_WORD;
    }

    /* Check alignment */
    if (size == ACCESS_WORD && (base & 3)) {
        return ARMV8M_ERR_USAGE_FAULT;
    } else if (size == ACCESS_HALF && (base & 1)) {
        return ARMV8M_ERR_USAGE_FAULT;
    }

    /* Load from memory (acquire semantics is memory barrier in hardware) */
    uint32_t value = mem_read(exec, base, size, &fault);

    if (fault) {
        return ARMV8M_ERR_BUS_FAULT;
    }

    /* For exclusive variants, set the exclusive monitor */
    if (insn->op != 0) {
        exec->cpu.exclusive_addr = base;
        exec->cpu.exclusive_valid = true;
    }

    set_reg(exec, insn->rt, value);
    return ARMV8M_OK;
}

int exec_store_release(Executor *exec, const DecodedInsn *insn)
{
    /* STL/STLB/STLH - Store with release semantics.
     * Also handles STLEX/STLEXB/STLEXH (exclusive variants).
     * In single-threaded execution, release semantics are a no-op.
     *
     * insn->op: 0 = non-exclusive, 1 = exclusive
     * insn->access_size: byte/half/word */
    uint32_t base = get_reg(exec, insn->rn);
    uint32_t value = get_reg(exec, insn->rt);
    bool fault = false;

    /* Determine access size */
    uint8_t size = insn->access_size;
    if (size == 0) {
        size = ACCESS_WORD;
    }

    /* Check alignment */
    if (size == ACCESS_WORD && (base & 3)) {
        return ARMV8M_ERR_USAGE_FAULT;
    } else if (size == ACCESS_HALF && (base & 1)) {
        return ARMV8M_ERR_USAGE_FAULT;
    }

    /* For exclusive variants, check the monitor */
    if (insn->op != 0) {
        if (!exec->cpu.exclusive_valid || exec->cpu.exclusive_addr != base) {
            /* Exclusive store fails */
            set_reg(exec, insn->rd, 1);  /* Status = fail */
            exec->cpu.exclusive_valid = false;
            return ARMV8M_OK;
        }
    }

    /* Store to memory (release semantics is memory barrier in hardware) */
    mem_write(exec, base, value, size, &fault);

    if (fault) {
        exec->cpu.exclusive_valid = false;
        return ARMV8M_ERR_BUS_FAULT;
    }

    /* For exclusive variants, write status and clear monitor */
    if (insn->op != 0) {
        set_reg(exec, insn->rd, 0);  /* Status = success */
        exec->cpu.exclusive_valid = false;
    }

    return ARMV8M_OK;
}
