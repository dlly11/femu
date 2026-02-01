/**
 * @file test_system.cpp
 * @brief System and exception instruction tests
 */

#include "test_common.h"

TEST_GROUP(System)
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

TEST(System, MrsApsr)
{
    exec.cpu.xpsr = ARMV8M_XPSR_T | ARMV8M_XPSR_N | ARMV8M_XPSR_C;
    insn.type = INSN_MRS;
    insn.rd = 0;
    insn.sysreg = 0x00;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(ARMV8M_XPSR_N | ARMV8M_XPSR_C, exec.cpu.r[0]);
}

TEST(System, MrsIpsr)
{
    exec.cpu.current_exception = 11;
    insn.type = INSN_MRS;
    insn.rd = 0;
    insn.sysreg = 0x05;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(11u, exec.cpu.r[0]);
}

TEST(System, MrsMsp)
{
    exec.cpu.sp_main = 0x20001000;
    insn.type = INSN_MRS;
    insn.rd = 0;
    insn.sysreg = 0x08;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x20001000u, exec.cpu.r[0]);
}

TEST(System, MrsPsp)
{
    exec.cpu.sp_process = 0x20002000;
    insn.type = INSN_MRS;
    insn.rd = 0;
    insn.sysreg = 0x09;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x20002000u, exec.cpu.r[0]);
}

TEST(System, MrsPrimask)
{
    exec.cpu.primask = 1;
    insn.type = INSN_MRS;
    insn.rd = 0;
    insn.sysreg = 0x10;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(1u, exec.cpu.r[0]);
}

TEST(System, MrsControl)
{
    exec.cpu.control = ARMV8M_CONTROL_SPSEL;
    insn.type = INSN_MRS;
    insn.rd = 0;
    insn.sysreg = 0x14;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(ARMV8M_CONTROL_SPSEL, exec.cpu.r[0]);
}

TEST(System, MsrApsr)
{
    exec.cpu.r[0] = ARMV8M_XPSR_N | ARMV8M_XPSR_Z;
    exec.cpu.xpsr = ARMV8M_XPSR_T;
    insn.type = INSN_MSR;
    insn.rn = 0;
    insn.sysreg = 0x00;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_N);
    CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_Z);
    CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_T);
}

TEST(System, MsrPrimask)
{
    exec.cpu.r[0] = 1;
    insn.type = INSN_MSR;
    insn.rn = 0;
    insn.sysreg = 0x10;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(1u, exec.cpu.primask);
}

TEST(System, CpsieI)
{
    exec.cpu.primask = 1;
    insn.type = INSN_CPS;
    insn.op = 0;
    insn.imm = 0x02;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0u, exec.cpu.primask);
}

TEST(System, CpsidI)
{
    exec.cpu.primask = 0;
    insn.type = INSN_CPS;
    insn.op = 1;
    insn.imm = 0x12;  /* bit 4 = disable, bit 1 = affect PRIMASK */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(1u, exec.cpu.primask);
}

TEST(System, Nop)
{
    insn.type = INSN_HINT;
    insn.op = HINT_NOP;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
}

TEST(System, Wfi)
{
    insn.type = INSN_HINT;
    insn.op = HINT_WFI;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_TRUE(exec.cpu.sleeping);
}

TEST(System, Wfe)
{
    exec.cpu.event_registered = true;
    insn.type = INSN_HINT;
    insn.op = HINT_WFE;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_FALSE(exec.cpu.event_registered);
    CHECK_FALSE(exec.cpu.sleeping);
}

TEST(System, Sev)
{
    insn.type = INSN_HINT;
    insn.op = HINT_SEV;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_TRUE(exec.cpu.event_registered);
}

TEST(System, Dsb)
{
    insn.type = INSN_BARRIER;
    insn.op = 0;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
}

TEST(System, ITInstruction)
{
    insn.type = INSN_IT;
    insn.it_cond = COND_EQ;
    insn.it_mask = 0x8;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x08u, exec.cpu.it_state);
}

/*============================================================================
 * Test Group: Exception Handling
 *============================================================================*/

TEST_GROUP(Exception)
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

        mock_memory[44] = 0x01;
        mock_memory[45] = 0x02;
        mock_memory[46] = 0x00;
        mock_memory[47] = 0x00;

        mock().clear();
    }

    void teardown()
    {
        mock().checkExpectations();
        mock().clear();
    }
};

TEST(Exception, ExceptionEntry)
{
    exec.cpu.r[0] = 0x11111111;
    exec.cpu.r[1] = 0x22222222;
    exec.cpu.r[2] = 0x33333333;
    exec.cpu.r[3] = 0x44444444;
    exec.cpu.r[12] = 0x55555555;
    exec.cpu.r[ARMV8M_REG_LR] = 0x66666666;
    exec.cpu.r[ARMV8M_REG_PC] = 0x1000;
    exec.cpu.xpsr = ARMV8M_XPSR_T | ARMV8M_XPSR_N;
    exec.cpu.sp_main = 0x400;
    exec.cpu.r[ARMV8M_REG_SP] = 0x400;
    exec.cpu.mode = MODE_THREAD;

    int result = armv8m_exception_entry(&exec, ARMV8M_EXC_SVCALL);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(MODE_HANDLER, exec.cpu.mode);
    CHECK_EQUAL(ARMV8M_EXC_SVCALL, exec.cpu.current_exception);
    CHECK_EQUAL(0x200u, exec.cpu.r[ARMV8M_REG_PC]);
    CHECK_EQUAL(0x3E0u, exec.cpu.sp_main);
}

TEST(Exception, ExceptionReturn)
{
    exec.cpu.mode = MODE_HANDLER;
    exec.cpu.current_exception = ARMV8M_EXC_SVCALL;
    exec.cpu.sp_main = 0x3E0;
    exec.cpu.r[ARMV8M_REG_SP] = 0x3E0;

    mock_memory[0x3E0] = 0x11;
    mock_memory[0x3E1] = 0x11;
    mock_memory[0x3E2] = 0x11;
    mock_memory[0x3E3] = 0x11;
    mock_memory[0x3E4] = 0x22;
    mock_memory[0x3E5] = 0x22;
    mock_memory[0x3E6] = 0x22;
    mock_memory[0x3E7] = 0x22;
    mock_memory[0x3E8] = 0x33;
    mock_memory[0x3E9] = 0x33;
    mock_memory[0x3EA] = 0x33;
    mock_memory[0x3EB] = 0x33;
    mock_memory[0x3EC] = 0x44;
    mock_memory[0x3ED] = 0x44;
    mock_memory[0x3EE] = 0x44;
    mock_memory[0x3EF] = 0x44;
    mock_memory[0x3F0] = 0x55;
    mock_memory[0x3F1] = 0x55;
    mock_memory[0x3F2] = 0x55;
    mock_memory[0x3F3] = 0x55;
    mock_memory[0x3F4] = 0x66;
    mock_memory[0x3F5] = 0x66;
    mock_memory[0x3F6] = 0x66;
    mock_memory[0x3F7] = 0x66;
    mock_memory[0x3F8] = 0x00;
    mock_memory[0x3F9] = 0x10;
    mock_memory[0x3FA] = 0x00;
    mock_memory[0x3FB] = 0x00;
    mock_memory[0x3FC] = 0x00;
    mock_memory[0x3FD] = 0x00;
    mock_memory[0x3FE] = 0x00;
    mock_memory[0x3FF] = 0x01;

    uint32_t exc_return = 0xFFFFFFF9;

    int result = armv8m_exception_return(&exec, exc_return);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(MODE_THREAD, exec.cpu.mode);
    CHECK_EQUAL(0x11111111u, exec.cpu.r[0]);
    CHECK_EQUAL(0x22222222u, exec.cpu.r[1]);
    CHECK_EQUAL(0x1000u, exec.cpu.r[ARMV8M_REG_PC]);
}

TEST(Exception, InvalidExcReturn)
{
    exec.cpu.mode = MODE_HANDLER;

    int result = armv8m_exception_return(&exec, 0x12345678);

    CHECK_EQUAL(ARMV8M_ERR_USAGE_FAULT, result);
}

TEST(Exception, ExcReturnFromThreadMode)
{
    exec.cpu.mode = MODE_THREAD;

    int result = armv8m_exception_return(&exec, 0xFFFFFFF9);

    CHECK_EQUAL(ARMV8M_ERR_USAGE_FAULT, result);
}

/*============================================================================
 * Test Group: Execution Step (with mocked decoder)
 *============================================================================*/

