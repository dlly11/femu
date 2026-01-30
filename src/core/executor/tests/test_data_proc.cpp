/**
 * @file test_data_proc.cpp
 * @brief Data processing instruction tests
 */

#include "test_common.h"

/*============================================================================
 * Test Group: Data Processing - Immediate
 *============================================================================*/

TEST_GROUP(DataProcImm)
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

TEST(DataProcImm, MovImmediate)
{
    insn.type = INSN_DATA_PROC_IMM;
    insn.op = DP_MOV;
    insn.rd = 0;
    insn.imm = 0x42;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x42u, exec.cpu.r[0]);
}

TEST(DataProcImm, AddImmediate)
{
    exec.cpu.r[1] = 100;
    insn.type = INSN_DATA_PROC_IMM;
    insn.op = DP_ADD;
    insn.rd = 0;
    insn.rn = 1;
    insn.imm = 50;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(150u, exec.cpu.r[0]);
}

TEST(DataProcImm, SubImmediate)
{
    exec.cpu.r[1] = 100;
    insn.type = INSN_DATA_PROC_IMM;
    insn.op = DP_SUB;
    insn.rd = 0;
    insn.rn = 1;
    insn.imm = 30;
    insn.set_flags = true;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(70u, exec.cpu.r[0]);
    CHECK_FALSE(exec.cpu.xpsr & ARMV8M_XPSR_N);
    CHECK_FALSE(exec.cpu.xpsr & ARMV8M_XPSR_Z);
    CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_C);
}

TEST(DataProcImm, CmpImmediate)
{
    exec.cpu.r[0] = 50;
    insn.type = INSN_DATA_PROC_IMM;
    insn.op = DP_CMP;
    insn.rd = ARMV8M_REG_NONE;
    insn.rn = 0;
    insn.imm = 50;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_Z);
    CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_C);
}

TEST(DataProcImm, AndImmediate)
{
    exec.cpu.r[0] = 0xFF00;
    insn.type = INSN_DATA_PROC_IMM;
    insn.op = DP_AND;
    insn.rd = 1;
    insn.rn = 0;
    insn.imm = 0x0FFF;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x0F00u, exec.cpu.r[1]);
}

TEST(DataProcImm, OrrImmediate)
{
    exec.cpu.r[0] = 0xF000;
    insn.type = INSN_DATA_PROC_IMM;
    insn.op = DP_ORR;
    insn.rd = 1;
    insn.rn = 0;
    insn.imm = 0x000F;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0xF00Fu, exec.cpu.r[1]);
}

TEST(DataProcImm, EorImmediate)
{
    exec.cpu.r[0] = 0xFF00;
    insn.type = INSN_DATA_PROC_IMM;
    insn.op = DP_EOR;
    insn.rd = 1;
    insn.rn = 0;
    insn.imm = 0xFFFF;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x00FFu, exec.cpu.r[1]);
}

TEST(DataProcImm, BicImmediate)
{
    exec.cpu.r[0] = 0xFFFF;
    insn.type = INSN_DATA_PROC_IMM;
    insn.op = DP_BIC;
    insn.rd = 1;
    insn.rn = 0;
    insn.imm = 0x00FF;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0xFF00u, exec.cpu.r[1]);
}

TEST(DataProcImm, MvnImmediate)
{
    insn.type = INSN_DATA_PROC_IMM;
    insn.op = DP_MVN;
    insn.rd = 0;
    insn.imm = 0;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0xFFFFFFFFu, exec.cpu.r[0]);
}

TEST(DataProcImm, RsbImmediate)
{
    exec.cpu.r[1] = 10;
    insn.type = INSN_DATA_PROC_IMM;
    insn.op = DP_RSB;
    insn.rd = 0;
    insn.rn = 1;
    insn.imm = 100;
    insn.set_flags = true;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(90u, exec.cpu.r[0]);
}

TEST(DataProcImm, TstImmediate)
{
    exec.cpu.r[0] = 0xFF00;
    insn.type = INSN_DATA_PROC_IMM;
    insn.op = DP_TST;
    insn.rd = ARMV8M_REG_NONE;
    insn.rn = 0;
    insn.imm = 0x00FF;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_Z);
}

TEST(DataProcImm, CmnImmediate)
{
    exec.cpu.r[0] = 0xFFFFFFFF;
    insn.type = INSN_DATA_PROC_IMM;
    insn.op = DP_CMN;
    insn.rd = ARMV8M_REG_NONE;
    insn.rn = 0;
    insn.imm = 1;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_Z);
    CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_C);
}

TEST(DataProcImm, AdcImmediate)
{
    exec.cpu.r[1] = 10;
    exec.cpu.xpsr |= ARMV8M_XPSR_C;
    insn.type = INSN_DATA_PROC_IMM;
    insn.op = DP_ADC;
    insn.rd = 0;
    insn.rn = 1;
    insn.imm = 5;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(16u, exec.cpu.r[0]);
}

TEST(DataProcImm, SbcImmediate)
{
    exec.cpu.r[1] = 20;
    exec.cpu.xpsr |= ARMV8M_XPSR_C;
    insn.type = INSN_DATA_PROC_IMM;
    insn.op = DP_SBC;
    insn.rd = 0;
    insn.rn = 1;
    insn.imm = 5;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(15u, exec.cpu.r[0]);
}

TEST(DataProcImm, OrnImmediate)
{
    exec.cpu.r[0] = 0xF0F0F0F0;
    insn.type = INSN_DATA_PROC_IMM;
    insn.op = DP_ORN;
    insn.rd = 1;
    insn.rn = 0;
    insn.imm = 0xFF00FF00;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0xF0FFF0FFu, exec.cpu.r[1]);
}

/*============================================================================
 * Test Group: Data Processing - Register
 *============================================================================*/

TEST_GROUP(DataProcReg)
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

TEST(DataProcReg, AddRegisters)
{
    exec.cpu.r[1] = 100;
    exec.cpu.r[2] = 50;
    insn.type = INSN_DATA_PROC_REG;
    insn.op = DP_ADD;
    insn.rd = 0;
    insn.rn = 1;
    insn.rm = 2;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(150u, exec.cpu.r[0]);
}

TEST(DataProcReg, SubRegisters)
{
    exec.cpu.r[1] = 100;
    exec.cpu.r[2] = 30;
    insn.type = INSN_DATA_PROC_REG;
    insn.op = DP_SUB;
    insn.rd = 0;
    insn.rn = 1;
    insn.rm = 2;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(70u, exec.cpu.r[0]);
}

TEST(DataProcReg, LslRegister)
{
    exec.cpu.r[0] = 0x10;
    exec.cpu.r[1] = 4;
    insn.type = INSN_DATA_PROC_REG;
    insn.op = DP_LSL;
    insn.rd = 2;
    insn.rn = 0;
    insn.rm = 1;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x100u, exec.cpu.r[2]);
}

TEST(DataProcReg, LsrRegister)
{
    exec.cpu.r[0] = 0x100;
    exec.cpu.r[1] = 4;
    insn.type = INSN_DATA_PROC_REG;
    insn.op = DP_LSR;
    insn.rd = 2;
    insn.rn = 0;
    insn.rm = 1;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x10u, exec.cpu.r[2]);
}

TEST(DataProcReg, AsrRegister)
{
    exec.cpu.r[0] = 0x80000000;
    exec.cpu.r[1] = 4;
    insn.type = INSN_DATA_PROC_REG;
    insn.op = DP_ASR;
    insn.rd = 2;
    insn.rn = 0;
    insn.rm = 1;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0xF8000000u, exec.cpu.r[2]);
}

TEST(DataProcReg, RorRegister)
{
    exec.cpu.r[0] = 0x0000000F;
    exec.cpu.r[1] = 4;
    insn.type = INSN_DATA_PROC_REG;
    insn.op = DP_ROR;
    insn.rd = 2;
    insn.rn = 0;
    insn.rm = 1;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0xF0000000u, exec.cpu.r[2]);
}

TEST(DataProcReg, MulRegister)
{
    exec.cpu.r[0] = 7;
    exec.cpu.r[1] = 8;
    insn.type = INSN_DATA_PROC_REG;
    insn.op = DP_MUL;
    insn.rd = 2;
    insn.rn = 0;
    insn.rm = 1;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(56u, exec.cpu.r[2]);
}

/*============================================================================
 * Test Group: Data Processing - Shifted Register
 *============================================================================*/

TEST_GROUP(DataProcShift)
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

TEST(DataProcShift, AddWithLsl)
{
    exec.cpu.r[1] = 100;
    exec.cpu.r[2] = 0x10;
    insn.type = INSN_DATA_PROC_SHIFTED;
    insn.op = DP_ADD;
    insn.rd = 0;
    insn.rn = 1;
    insn.rm = 2;
    insn.shift_type = SHIFT_LSL;
    insn.shift_amount = 4;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(100u + 0x100u, exec.cpu.r[0]);
}

TEST(DataProcShift, MovWithRrx)
{
    exec.cpu.r[0] = 0x00000003;
    exec.cpu.xpsr |= ARMV8M_XPSR_C;
    insn.type = INSN_DATA_PROC_SHIFTED;
    insn.op = DP_MOV;
    insn.rd = 1;
    insn.rm = 0;
    insn.shift_type = SHIFT_RRX;
    insn.set_flags = true;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x80000001u, exec.cpu.r[1]);
    CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_C);
}

/*============================================================================
 * Test Group: Multiply/Divide
 *============================================================================*/

