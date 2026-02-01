/**
 * @file test_control_flow.cpp
 * @brief Branch instructions and execution step/run tests
 */

#include "test_common.h"

TEST_GROUP(Branch)
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

TEST(Branch, Branch)
{
    exec.cpu.r[ARMV8M_REG_PC] = 0x1000;
    insn.type = INSN_BRANCH;
    insn.branch_offset = 0x100;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x1104u, exec.cpu.r[ARMV8M_REG_PC]);
}

TEST(Branch, BranchNegativeOffset)
{
    exec.cpu.r[ARMV8M_REG_PC] = 0x1000;
    insn.type = INSN_BRANCH;
    insn.branch_offset = -0x100;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x0F04u, exec.cpu.r[ARMV8M_REG_PC]);
}

TEST(Branch, BranchLink)
{
    exec.cpu.r[ARMV8M_REG_PC] = 0x1000;
    insn.type = INSN_BRANCH_LINK;
    insn.branch_offset = 0x200;
    insn.size = 4;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x1204u, exec.cpu.r[ARMV8M_REG_PC]);
    CHECK_EQUAL(0x1005u, exec.cpu.r[ARMV8M_REG_LR]);
}

TEST(Branch, BranchExchange)
{
    exec.cpu.r[0] = 0x2001;
    insn.type = INSN_BRANCH_EXCHANGE;
    insn.rm = 0;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x2000u, exec.cpu.r[ARMV8M_REG_PC]);
}

TEST(Branch, BranchExchangeToArm)
{
    exec.cpu.r[0] = 0x2000;
    insn.type = INSN_BRANCH_EXCHANGE;
    insn.rm = 0;

    CHECK_EQUAL(ARMV8M_ERR_USAGE_FAULT, armv8m_exec_insn(&exec, &insn));
}

TEST(Branch, BranchLinkExchange)
{
    exec.cpu.r[ARMV8M_REG_PC] = 0x1000;
    exec.cpu.r[0] = 0x3001;
    insn.type = INSN_BRANCH_LINK_EXCHANGE;
    insn.rm = 0;
    insn.size = 2;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x3000u, exec.cpu.r[ARMV8M_REG_PC]);
    CHECK_EQUAL(0x1003u, exec.cpu.r[ARMV8M_REG_LR]);
}

TEST(Branch, Cbz)
{
    exec.cpu.r[ARMV8M_REG_PC] = 0x1000;
    exec.cpu.r[0] = 0;
    insn.type = INSN_COMPARE_BRANCH;
    insn.rn = 0;
    insn.op = 0;
    insn.branch_offset = 0x20;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x1024u, exec.cpu.r[ARMV8M_REG_PC]);
}

TEST(Branch, CbzNotTaken)
{
    exec.cpu.r[ARMV8M_REG_PC] = 0x1000;
    exec.cpu.r[0] = 1;
    insn.type = INSN_COMPARE_BRANCH;
    insn.rn = 0;
    insn.op = 0;
    insn.branch_offset = 0x20;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x1000u, exec.cpu.r[ARMV8M_REG_PC]);
}

TEST(Branch, Cbnz)
{
    exec.cpu.r[ARMV8M_REG_PC] = 0x1000;
    exec.cpu.r[0] = 5;
    insn.type = INSN_COMPARE_BRANCH;
    insn.rn = 0;
    insn.op = 1;
    insn.branch_offset = 0x30;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x1034u, exec.cpu.r[ARMV8M_REG_PC]);
}

TEST(Branch, Tbb)
{
    mock_memory[0x100] = 0x10;

    exec.cpu.r[ARMV8M_REG_PC] = 0x1000;
    exec.cpu.r[0] = 0x100;
    exec.cpu.r[1] = 0;
    insn.type = INSN_TABLE_BRANCH;
    insn.rn = 0;
    insn.rm = 1;
    insn.access_size = ACCESS_BYTE;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x1024u, exec.cpu.r[ARMV8M_REG_PC]);
}

TEST(Branch, Tbh)
{
    mock_memory[0x100] = 0x20;
    mock_memory[0x101] = 0x00;

    exec.cpu.r[ARMV8M_REG_PC] = 0x1000;
    exec.cpu.r[0] = 0x100;
    exec.cpu.r[1] = 0;
    insn.type = INSN_TABLE_BRANCH;
    insn.rn = 0;
    insn.rm = 1;
    insn.access_size = ACCESS_HALF;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x1044u, exec.cpu.r[ARMV8M_REG_PC]);
}

/*============================================================================
 * Test Group: System Instructions
 *============================================================================*/

TEST_GROUP(ExecStep)
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

TEST(ExecStep, SimpleStep)
{
    exec.cpu.r[ARMV8M_REG_PC] = 0x100;

    DecodedInsn insn;
    init_insn(insn);
    insn.type = INSN_DATA_PROC_IMM;
    insn.op = DP_MOV;
    insn.rd = 0;
    insn.imm = 0x42;
    insn.size = 2;
    setup_mock_decode(insn);

    expect_decode_init();
    expect_decode(0x100, 2);

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0x42u, exec.cpu.r[0]);
    CHECK_EQUAL(0x102u, exec.cpu.r[ARMV8M_REG_PC]);
}

TEST(ExecStep, StepWithBranch)
{
    exec.cpu.r[ARMV8M_REG_PC] = 0x1000;

    DecodedInsn insn;
    init_insn(insn);
    insn.type = INSN_BRANCH;
    insn.branch_offset = 0x100;
    insn.size = 4;
    setup_mock_decode(insn);

    expect_decode_init();
    expect_decode(0x1000, 4);

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0x1104u, exec.cpu.r[ARMV8M_REG_PC]);
}

TEST(ExecStep, StepWhenHalted)
{
    exec.cpu.halted = true;

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_ERR_HALTED, result);
}

TEST(ExecStep, StepConditionalBranchTaken)
{
    exec.cpu.r[ARMV8M_REG_PC] = 0x1000;
    exec.cpu.xpsr = ARMV8M_XPSR_T | ARMV8M_XPSR_Z;

    DecodedInsn insn;
    init_insn(insn);
    insn.type = INSN_BRANCH;
    insn.cond = COND_EQ;
    insn.branch_offset = 0x50;
    insn.size = 2;
    setup_mock_decode(insn);

    expect_decode_init();
    expect_decode(0x1000, 2);

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0x1054u, exec.cpu.r[ARMV8M_REG_PC]);
}

TEST(ExecStep, StepConditionalBranchNotTaken)
{
    exec.cpu.r[ARMV8M_REG_PC] = 0x1000;
    exec.cpu.xpsr = ARMV8M_XPSR_T;

    DecodedInsn insn;
    init_insn(insn);
    insn.type = INSN_BRANCH;
    insn.cond = COND_EQ;
    insn.branch_offset = 0x50;
    insn.size = 2;
    setup_mock_decode(insn);

    expect_decode_init();
    expect_decode(0x1000, 2);

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0x1002u, exec.cpu.r[ARMV8M_REG_PC]);
}

/*============================================================================
 * Test Group: Execution Run (with mocked decoder)
 *============================================================================*/

TEST_GROUP(ExecRun)
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

TEST(ExecRun, RunWithCycleLimit)
{
    exec.cpu.r[ARMV8M_REG_PC] = 0x100;

    DecodedInsn insn;
    init_insn(insn);
    insn.type = INSN_DATA_PROC_IMM;
    insn.op = DP_MOV;
    insn.rd = 0;
    insn.imm = 1;
    insn.size = 2;
    setup_mock_decode(insn);

    for (int i = 0; i < 3; i++) {
        expect_decode_init();
        expect_decode(0x100u + (uint32_t)(i * 2), 2);
    }

    int64_t cycles = armv8m_exec_run(&exec, 3);

    CHECK_EQUAL(3, cycles);
}

TEST(ExecRun, RunUntilHalted)
{
    exec.cpu.r[ARMV8M_REG_PC] = 0x100;

    DecodedInsn insn;
    init_insn(insn);
    insn.type = INSN_HINT;
    insn.op = HINT_WFI;
    insn.size = 2;
    setup_mock_decode(insn);

    expect_decode_init();
    expect_decode(0x100, 2);

    int64_t cycles = armv8m_exec_run(&exec, 100);

    CHECK_EQUAL(1, cycles);
    CHECK_TRUE(exec.cpu.sleeping);
}

/*============================================================================
 * Test Group: Bitfield Instructions
 *============================================================================*/

