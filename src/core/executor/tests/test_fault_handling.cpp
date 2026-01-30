/**
 * @file test_fault_handling.cpp
 * @brief Tests for fault handling, stack limits, and unaligned access
 */

#include "test_common.h"

/*============================================================================
 * System Register Numbers (from exec_system.c)
 *============================================================================*/
#define SYSREG_MSPLIM       0x0A    /* MSP Limit (v8-M) */
#define SYSREG_PSPLIM       0x0B    /* PSP Limit (v8-M) */

/*============================================================================
 * Test Group: Coprocessor Instructions
 *============================================================================*/

TEST_GROUP(Coprocessor)
{
    Executor exec;
    DecodedInsn insn;

    void setup()
    {
        memset(mock_memory, 0, sizeof(mock_memory));
        armv8m_exec_init(&exec);
        exec.mem.ctx = NULL;
        exec.mem.read = mock_mem_read;
        exec.mem.write = mock_mem_write;
        exec.mem.get_ptr = mock_mem_get_ptr;
        init_insn(insn);
        mock().clear();
    }

    void teardown()
    {
        mock().checkExpectations();
        mock().clear();
    }
};

TEST(Coprocessor, McrWithoutFpuFaults)
{
    exec.has_fpu = false;
    insn.type = INSN_MCR;

    int result = armv8m_exec_insn(&exec, &insn);

    CHECK_EQUAL(ARMV8M_ERR_USAGE_FAULT, result);
    CHECK_TRUE((exec.cpu.cfsr & ARMV8M_UFSR_NOCP) != 0);
}

TEST(Coprocessor, MrcWithoutFpuFaults)
{
    exec.has_fpu = false;
    insn.type = INSN_MRC;

    int result = armv8m_exec_insn(&exec, &insn);

    CHECK_EQUAL(ARMV8M_ERR_USAGE_FAULT, result);
    CHECK_TRUE((exec.cpu.cfsr & ARMV8M_UFSR_NOCP) != 0);
}

TEST(Coprocessor, McrAlwaysFaultsOnNonVFPCoprocessor)
{
    /* MCR/MRC with INSN_MCR/INSN_MRC type are non-VFP coprocessor accesses.
     * The decoder routes VFP (CP10/11) to the VFP path, so these instructions
     * represent access to non-existent coprocessors and should always fault,
     * regardless of whether FPU is present. */
    exec.has_fpu = true;
    insn.type = INSN_MCR;

    int result = armv8m_exec_insn(&exec, &insn);

    /* Non-VFP coprocessor always faults */
    CHECK_EQUAL(ARMV8M_ERR_USAGE_FAULT, result);
    CHECK_TRUE((exec.cpu.cfsr & ARMV8M_UFSR_NOCP) != 0);
}

TEST(Coprocessor, MrcAlwaysFaultsOnNonVFPCoprocessor)
{
    /* MCR/MRC with INSN_MCR/INSN_MRC type are non-VFP coprocessor accesses.
     * The decoder routes VFP (CP10/11) to the VFP path, so these instructions
     * represent access to non-existent coprocessors and should always fault. */
    exec.has_fpu = true;
    insn.type = INSN_MRC;

    int result = armv8m_exec_insn(&exec, &insn);

    /* Non-VFP coprocessor always faults */
    CHECK_EQUAL(ARMV8M_ERR_USAGE_FAULT, result);
    CHECK_TRUE((exec.cpu.cfsr & ARMV8M_UFSR_NOCP) != 0);
}

/*============================================================================
 * Test Group: Stack Limit Registers
 *============================================================================*/

TEST_GROUP(StackLimits)
{
    Executor exec;
    DecodedInsn insn;

    void setup()
    {
        memset(mock_memory, 0, sizeof(mock_memory));
        armv8m_exec_init(&exec);
        exec.mem.ctx = NULL;
        exec.mem.read = mock_mem_read;
        exec.mem.write = mock_mem_write;
        exec.mem.get_ptr = mock_mem_get_ptr;
        init_insn(insn);
        mock().clear();
    }

    void teardown()
    {
        mock().checkExpectations();
        mock().clear();
    }
};

TEST(StackLimits, MrsMsplim)
{
    exec.cpu.msplim = 0x20000100;
    insn.type = INSN_MRS;
    insn.rd = 0;
    insn.sysreg = SYSREG_MSPLIM;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x20000100u, exec.cpu.r[0]);
}

TEST(StackLimits, MrsPsplim)
{
    exec.cpu.psplim = 0x20000200;
    insn.type = INSN_MRS;
    insn.rd = 0;
    insn.sysreg = SYSREG_PSPLIM;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x20000200u, exec.cpu.r[0]);
}

TEST(StackLimits, MsrMsplim)
{
    exec.cpu.r[0] = 0x20000107;  /* Not 8-byte aligned (ends in 0x7) */
    insn.type = INSN_MSR;
    insn.rn = 0;
    insn.sysreg = SYSREG_MSPLIM;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x20000100u, exec.cpu.msplim);  /* Should be aligned to 8 */
}

TEST(StackLimits, MsrPsplim)
{
    exec.cpu.r[0] = 0x20000217;  /* Not 8-byte aligned (ends in 0x7) */
    insn.type = INSN_MSR;
    insn.rn = 0;
    insn.sysreg = SYSREG_PSPLIM;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x20000210u, exec.cpu.psplim);  /* Should be aligned to 8 */
}

TEST(StackLimits, MsrMsplimUnprivilegedIgnored)
{
    exec.cpu.mode = MODE_THREAD;
    exec.cpu.control = ARMV8M_CONTROL_NPRIV;  /* Unprivileged */
    exec.cpu.msplim = 0;
    exec.cpu.r[0] = 0x20000100;
    insn.type = INSN_MSR;
    insn.rn = 0;
    insn.sysreg = SYSREG_MSPLIM;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0u, exec.cpu.msplim);  /* Should not change */
}

TEST(StackLimits, MrsMsplimUnprivilegedReturnsZero)
{
    exec.cpu.mode = MODE_THREAD;
    exec.cpu.control = ARMV8M_CONTROL_NPRIV;
    exec.cpu.msplim = 0x20000100;
    exec.cpu.r[0] = 0xDEADBEEF;
    insn.type = INSN_MRS;
    insn.rd = 0;
    insn.sysreg = SYSREG_MSPLIM;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0u, exec.cpu.r[0]);  /* Returns 0 for unprivileged */
}

TEST(StackLimits, CheckStackLimitMsp)
{
    exec.cpu.mode = MODE_HANDLER;  /* Uses MSP */
    exec.cpu.msplim = 0x20000100;

    CHECK_TRUE(armv8m_check_stack_limit(&exec.cpu, 0x200000F0));   /* Below limit */
    CHECK_FALSE(armv8m_check_stack_limit(&exec.cpu, 0x20000100)); /* At limit */
    CHECK_FALSE(armv8m_check_stack_limit(&exec.cpu, 0x20000200)); /* Above limit */
}

TEST(StackLimits, CheckStackLimitPsp)
{
    exec.cpu.mode = MODE_THREAD;
    exec.cpu.control = ARMV8M_CONTROL_SPSEL;  /* Uses PSP */
    exec.cpu.psplim = 0x20000200;

    CHECK_TRUE(armv8m_check_stack_limit(&exec.cpu, 0x200001F0));   /* Below limit */
    CHECK_FALSE(armv8m_check_stack_limit(&exec.cpu, 0x20000200)); /* At limit */
    CHECK_FALSE(armv8m_check_stack_limit(&exec.cpu, 0x20000300)); /* Above limit */
}

TEST(StackLimits, CheckStackLimitThreadMsp)
{
    exec.cpu.mode = MODE_THREAD;
    exec.cpu.control = 0;  /* Uses MSP (SPSEL=0) */
    exec.cpu.msplim = 0x20000100;

    CHECK_TRUE(armv8m_check_stack_limit(&exec.cpu, 0x200000F0));
    CHECK_FALSE(armv8m_check_stack_limit(&exec.cpu, 0x20000100));
}

TEST(StackLimits, PushStackOverflowFaults)
{
    /* Set up SP and stack limit */
    exec.cpu.sp_main = 0x120;  /* Will push to below 0x100 */
    exec.cpu.r[ARMV8M_REG_SP] = 0x120;
    exec.cpu.msplim = 0x100;
    exec.cpu.mode = MODE_HANDLER;

    /* PUSH {r0-r7, lr} = 9 registers = 36 bytes, SP goes to 0x120-0x24=0xFC */
    insn.type = INSN_STORE_MULTIPLE;
    insn.rn = ARMV8M_REG_SP;
    insn.register_list = 0x41FF;  /* r0-r8, lr */
    insn.add = false;  /* Decrement before */
    insn.writeback = true;

    int result = armv8m_exec_insn(&exec, &insn);

    CHECK_EQUAL(ARMV8M_ERR_USAGE_FAULT, result);
    CHECK_TRUE(exec.cpu.cfsr & ARMV8M_UFSR_STKOF);
}

TEST(StackLimits, PushNoOverflowSucceeds)
{
    exec.cpu.sp_main = 0x200;
    exec.cpu.r[ARMV8M_REG_SP] = 0x200;
    exec.cpu.msplim = 0x100;
    exec.cpu.mode = MODE_HANDLER;

    /* PUSH {r0-r3} = 4 registers = 16 bytes, SP goes to 0x1F0 (still > 0x100) */
    insn.type = INSN_STORE_MULTIPLE;
    insn.rn = ARMV8M_REG_SP;
    insn.register_list = 0x000F;  /* r0-r3 */
    insn.add = false;
    insn.writeback = true;

    int result = armv8m_exec_insn(&exec, &insn);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_FALSE(exec.cpu.cfsr & ARMV8M_UFSR_STKOF);
}

/*============================================================================
 * Test Group: Unaligned Access Trapping
 *============================================================================*/

TEST_GROUP(UnalignedAccess)
{
    Executor exec;
    DecodedInsn insn;

    void setup()
    {
        memset(mock_memory, 0, sizeof(mock_memory));
        armv8m_exec_init(&exec);
        exec.mem.ctx = NULL;
        exec.mem.read = mock_mem_read;
        exec.mem.write = mock_mem_write;
        exec.mem.get_ptr = mock_mem_get_ptr;
        init_insn(insn);
        mock().clear();
    }

    void teardown()
    {
        mock().checkExpectations();
        mock().clear();
    }
};

TEST(UnalignedAccess, UnalignedWordLoadWithTrapEnabled)
{
    exec.cpu.ccr |= ARMV8M_CCR_UNALIGN_TRP;
    exec.cpu.r[1] = 0x102;  /* Unaligned address */

    insn.type = INSN_LOAD_IMM;
    insn.rt = 0;
    insn.rn = 1;
    insn.imm = 0;
    insn.access_size = ACCESS_WORD;
    insn.add = true;
    insn.pre_index = true;

    int result = armv8m_exec_insn(&exec, &insn);

    CHECK_EQUAL(ARMV8M_ERR_USAGE_FAULT, result);
    CHECK_TRUE(exec.cpu.cfsr & ARMV8M_UFSR_UNALIGNED);
}

TEST(UnalignedAccess, UnalignedHalfLoadWithTrapEnabled)
{
    exec.cpu.ccr |= ARMV8M_CCR_UNALIGN_TRP;
    exec.cpu.r[1] = 0x101;  /* Unaligned for halfword */

    insn.type = INSN_LOAD_IMM;
    insn.rt = 0;
    insn.rn = 1;
    insn.imm = 0;
    insn.access_size = ACCESS_HALF;
    insn.add = true;
    insn.pre_index = true;

    int result = armv8m_exec_insn(&exec, &insn);

    CHECK_EQUAL(ARMV8M_ERR_USAGE_FAULT, result);
    CHECK_TRUE(exec.cpu.cfsr & ARMV8M_UFSR_UNALIGNED);
}

TEST(UnalignedAccess, UnalignedLoadWithTrapDisabled)
{
    exec.cpu.ccr &= ~ARMV8M_CCR_UNALIGN_TRP;  /* Trapping disabled */
    exec.cpu.r[1] = 0x102;  /* Unaligned */

    /* Store test value at unaligned address */
    mock_memory[0x102] = 0x11;
    mock_memory[0x103] = 0x22;
    mock_memory[0x104] = 0x33;
    mock_memory[0x105] = 0x44;

    insn.type = INSN_LOAD_IMM;
    insn.rt = 0;
    insn.rn = 1;
    insn.imm = 0;
    insn.access_size = ACCESS_WORD;
    insn.add = true;
    insn.pre_index = true;

    int result = armv8m_exec_insn(&exec, &insn);

    CHECK_EQUAL(ARMV8M_OK, result);  /* No fault */
    CHECK_EQUAL(0x44332211u, exec.cpu.r[0]);
}

TEST(UnalignedAccess, UnalignedWordStoreWithTrapEnabled)
{
    exec.cpu.ccr |= ARMV8M_CCR_UNALIGN_TRP;
    exec.cpu.r[0] = 0x12345678;
    exec.cpu.r[1] = 0x102;  /* Unaligned */

    insn.type = INSN_STORE_IMM;
    insn.rt = 0;
    insn.rn = 1;
    insn.imm = 0;
    insn.access_size = ACCESS_WORD;
    insn.add = true;
    insn.pre_index = true;

    int result = armv8m_exec_insn(&exec, &insn);

    CHECK_EQUAL(ARMV8M_ERR_USAGE_FAULT, result);
    CHECK_TRUE(exec.cpu.cfsr & ARMV8M_UFSR_UNALIGNED);
}

TEST(UnalignedAccess, UnalignedStoreWithTrapDisabled)
{
    exec.cpu.ccr &= ~ARMV8M_CCR_UNALIGN_TRP;
    exec.cpu.r[0] = 0x12345678;
    exec.cpu.r[1] = 0x102;

    insn.type = INSN_STORE_IMM;
    insn.rt = 0;
    insn.rn = 1;
    insn.imm = 0;
    insn.access_size = ACCESS_WORD;
    insn.add = true;
    insn.pre_index = true;

    int result = armv8m_exec_insn(&exec, &insn);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0x78u, mock_memory[0x102]);
    CHECK_EQUAL(0x56u, mock_memory[0x103]);
    CHECK_EQUAL(0x34u, mock_memory[0x104]);
    CHECK_EQUAL(0x12u, mock_memory[0x105]);
}

TEST(UnalignedAccess, ByteAccessNeverFaults)
{
    exec.cpu.ccr |= ARMV8M_CCR_UNALIGN_TRP;
    exec.cpu.r[1] = 0x101;  /* Any address is fine for bytes */
    mock_memory[0x101] = 0xAB;

    insn.type = INSN_LOAD_IMM;
    insn.rt = 0;
    insn.rn = 1;
    insn.imm = 0;
    insn.access_size = ACCESS_BYTE;
    insn.add = true;
    insn.pre_index = true;

    int result = armv8m_exec_insn(&exec, &insn);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0xABu, exec.cpu.r[0]);
}

TEST(UnalignedAccess, UnalignedRegisterLoadWithTrapEnabled)
{
    exec.cpu.ccr |= ARMV8M_CCR_UNALIGN_TRP;
    exec.cpu.r[1] = 0x100;
    exec.cpu.r[2] = 0x02;  /* Result is 0x102 (unaligned) */

    insn.type = INSN_LOAD_REG;
    insn.rt = 0;
    insn.rn = 1;
    insn.rm = 2;
    insn.access_size = ACCESS_WORD;
    insn.add = true;
    insn.shift_amount = 0;

    int result = armv8m_exec_insn(&exec, &insn);

    CHECK_EQUAL(ARMV8M_ERR_USAGE_FAULT, result);
    CHECK_TRUE(exec.cpu.cfsr & ARMV8M_UFSR_UNALIGNED);
}

TEST(UnalignedAccess, UnalignedRegisterStoreWithTrapEnabled)
{
    exec.cpu.ccr |= ARMV8M_CCR_UNALIGN_TRP;
    exec.cpu.r[0] = 0xDEADBEEF;
    exec.cpu.r[1] = 0x100;
    exec.cpu.r[2] = 0x02;

    insn.type = INSN_STORE_REG;
    insn.rt = 0;
    insn.rn = 1;
    insn.rm = 2;
    insn.access_size = ACCESS_WORD;
    insn.add = true;
    insn.shift_amount = 0;

    int result = armv8m_exec_insn(&exec, &insn);

    CHECK_EQUAL(ARMV8M_ERR_USAGE_FAULT, result);
    CHECK_TRUE(exec.cpu.cfsr & ARMV8M_UFSR_UNALIGNED);
}

/*============================================================================
 * Test Group: Reset Behavior
 *============================================================================*/

TEST_GROUP(ResetFaults)
{
    Executor exec;

    void setup()
    {
        memset(mock_memory, 0, sizeof(mock_memory));
        armv8m_exec_init(&exec);
        exec.mem.ctx = NULL;
        exec.mem.read = mock_mem_read;
        exec.mem.write = mock_mem_write;
        exec.mem.get_ptr = mock_mem_get_ptr;
        mock().clear();
    }

    void teardown()
    {
        mock().checkExpectations();
        mock().clear();
    }
};

TEST(ResetFaults, ResetClearsFaultRegisters)
{
    /* Set some fault bits */
    exec.cpu.cfsr = ARMV8M_UFSR_NOCP | ARMV8M_UFSR_UNALIGNED;
    exec.cpu.hfsr = 0x80000000;
    exec.cpu.mmfar = 0x12345678;
    exec.cpu.bfar = 0xDEADBEEF;

    /* Set up reset vectors */
    mock_memory[0] = 0x00;
    mock_memory[1] = 0x10;
    mock_memory[2] = 0x00;
    mock_memory[3] = 0x20;  /* SP = 0x20001000 */
    mock_memory[4] = 0x01;
    mock_memory[5] = 0x01;
    mock_memory[6] = 0x00;
    mock_memory[7] = 0x00;  /* PC = 0x100 */

    armv8m_exec_reset(&exec, 0);

    CHECK_EQUAL(0u, exec.cpu.cfsr);
    CHECK_EQUAL(0u, exec.cpu.hfsr);
    CHECK_EQUAL(0u, exec.cpu.mmfar);
    CHECK_EQUAL(0u, exec.cpu.bfar);
}

TEST(ResetFaults, ResetClearsStackLimits)
{
    exec.cpu.msplim = 0x20000100;
    exec.cpu.psplim = 0x20000200;

    mock_memory[0] = 0x00;
    mock_memory[1] = 0x10;
    mock_memory[2] = 0x00;
    mock_memory[3] = 0x20;
    mock_memory[4] = 0x01;
    mock_memory[5] = 0x01;
    mock_memory[6] = 0x00;
    mock_memory[7] = 0x00;

    armv8m_exec_reset(&exec, 0);

    CHECK_EQUAL(0u, exec.cpu.msplim);
    CHECK_EQUAL(0u, exec.cpu.psplim);
}

TEST(ResetFaults, ResetSetsCcrDefaults)
{
    exec.cpu.ccr = 0;

    mock_memory[0] = 0x00;
    mock_memory[1] = 0x10;
    mock_memory[2] = 0x00;
    mock_memory[3] = 0x20;
    mock_memory[4] = 0x01;
    mock_memory[5] = 0x01;
    mock_memory[6] = 0x00;
    mock_memory[7] = 0x00;

    armv8m_exec_reset(&exec, 0);

    CHECK_TRUE(exec.cpu.ccr & ARMV8M_CCR_STKALIGN);  /* STKALIGN defaults to 1 */
}
