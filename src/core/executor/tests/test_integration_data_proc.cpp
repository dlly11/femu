/**
 * @file test_integration_data_proc.cpp
 * @brief Data processing integration tests
 */

#include "test_integration_common.h"

TEST_GROUP(IntegrationDataProc)
{
    void setup()
    {
        memset(test_memory, 0, sizeof(test_memory));
        memset(&exec, 0, sizeof(exec));
        exec.mem.read = integ_mem_read;
        exec.mem.write = integ_mem_write;
        exec.mem.get_ptr = integ_mem_get_ptr;
        exec.cpu.sp_main = STACK_BASE;  /* Main stack pointer */  /* SP */
        exec.cpu.r[15] = CODE_BASE;   /* PC */
        exec.cpu.mode = MODE_THREAD;
        exec.cpu.control = 0;         /* Privileged, MSP */
    }

    void teardown() {}
};

TEST(IntegrationDataProc, MovImmediate)
{
    /* MOVS R0, #42 */
    write_insn16(0, MOVS_IMM(0, 42));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(42u, exec.cpu.r[0]);
    CHECK_EQUAL(2u, exec.cpu.r[15]);  /* PC advanced by 2 */
}

TEST(IntegrationDataProc, AddImmediate)
{
    /* R0 = 10, then ADDS R0, R0, #5 */
    exec.cpu.r[0] = 10;
    write_insn16(0, ADDS_IMM3(0, 0, 5));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(15u, exec.cpu.r[0]);
}

TEST(IntegrationDataProc, SubImmediate)
{
    /* R0 = 20, then SUBS R0, R0, #7 */
    exec.cpu.r[0] = 20;
    write_insn16(0, SUBS_IMM3(0, 0, 7));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(13u, exec.cpu.r[0]);
}

TEST(IntegrationDataProc, AddRegisters)
{
    /* R0 = 100, R1 = 50, ADDS R2, R0, R1 */
    exec.cpu.r[0] = 100;
    exec.cpu.r[1] = 50;
    write_insn16(0, ADDS_REG(2, 0, 1));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(150u, exec.cpu.r[2]);
}

TEST(IntegrationDataProc, SubRegisters)
{
    /* R0 = 100, R1 = 30, SUBS R2, R0, R1 */
    exec.cpu.r[0] = 100;
    exec.cpu.r[1] = 30;
    write_insn16(0, SUBS_REG(2, 0, 1));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(70u, exec.cpu.r[2]);
}

TEST(IntegrationDataProc, LogicalAnd)
{
    /* R0 = 0xFF, R1 = 0x0F, ANDS R0, R1 */
    exec.cpu.r[0] = 0xFF;
    exec.cpu.r[1] = 0x0F;
    write_insn16(0, ANDS_REG(0, 1));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0x0Fu, exec.cpu.r[0]);
}

TEST(IntegrationDataProc, LogicalOr)
{
    /* R0 = 0xF0, R1 = 0x0F, ORRS R0, R1 */
    exec.cpu.r[0] = 0xF0;
    exec.cpu.r[1] = 0x0F;
    write_insn16(0, ORRS_REG(0, 1));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0xFFu, exec.cpu.r[0]);
}

TEST(IntegrationDataProc, LogicalXor)
{
    /* R0 = 0xFF, R1 = 0xF0, EORS R0, R1 */
    exec.cpu.r[0] = 0xFF;
    exec.cpu.r[1] = 0xF0;
    write_insn16(0, EORS_REG(0, 1));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0x0Fu, exec.cpu.r[0]);
}

TEST(IntegrationDataProc, BitwiseClear)
{
    /* R0 = 0xFF, R1 = 0x0F, BICS R0, R1 -> R0 = R0 & ~R1 = 0xF0 */
    exec.cpu.r[0] = 0xFF;
    exec.cpu.r[1] = 0x0F;
    write_insn16(0, BICS_REG(0, 1));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0xF0u, exec.cpu.r[0]);
}

TEST(IntegrationDataProc, MoveNot)
{
    /* R1 = 0x00, MVNS R0, R1 -> R0 = ~R1 = 0xFFFFFFFF */
    exec.cpu.r[1] = 0x00;
    write_insn16(0, MVNS_REG(0, 1));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0xFFFFFFFFu, exec.cpu.r[0]);
}

TEST(IntegrationDataProc, Multiply)
{
    /* R0 = 7, R1 = 6, MULS R0, R1 -> R0 = R0 * R1 = 42 */
    exec.cpu.r[0] = 7;
    exec.cpu.r[1] = 6;
    write_insn16(0, MULS_REG(0, 1));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(42u, exec.cpu.r[0]);
}

TEST(IntegrationDataProc, LeftShift)
{
    /* R0 = 1, LSLS R1, R0, #4 -> R1 = 1 << 4 = 16 */
    exec.cpu.r[0] = 1;
    write_insn16(0, LSLS_IMM(1, 0, 4));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(16u, exec.cpu.r[1]);
}

TEST(IntegrationDataProc, RightShiftLogical)
{
    /* R0 = 256, LSRS R1, R0, #4 -> R1 = 256 >> 4 = 16 */
    exec.cpu.r[0] = 256;
    write_insn16(0, LSRS_IMM(1, 0, 4));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(16u, exec.cpu.r[1]);
}

TEST(IntegrationDataProc, RightShiftArithmetic)
{
    /* R0 = -16 (0xFFFFFFF0), ASRS R1, R0, #2 -> R1 = -4 (sign extended) */
    exec.cpu.r[0] = 0xFFFFFFF0;  /* -16 */
    write_insn16(0, ASRS_IMM(1, 0, 2));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0xFFFFFFFCu, exec.cpu.r[1]);  /* -4, sign extended */
}

TEST(IntegrationDataProc, CompareImmediate)
{
    /* R0 = 10, CMP R0, #10 -> Z=1, C=1, N=0 */
    exec.cpu.r[0] = 10;
    write_insn16(0, CMP_IMM8(0, 10));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_TRUE(exec.cpu.xpsr & (1u << 30));   /* Z flag set */
    CHECK_TRUE(exec.cpu.xpsr & (1u << 29));   /* C flag set (no borrow) */
    CHECK_FALSE(exec.cpu.xpsr & (1u << 31));  /* N flag clear */
}

TEST(IntegrationDataProc, CompareImmediateLess)
{
    /* R0 = 5, CMP R0, #10 -> result is negative */
    exec.cpu.r[0] = 5;
    write_insn16(0, CMP_IMM8(0, 10));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_FALSE(exec.cpu.xpsr & (1u << 30));  /* Z flag clear */
    CHECK_TRUE(exec.cpu.xpsr & (1u << 31));   /* N flag set */
}

TEST(IntegrationDataProc, Test)
{
    /* R0 = 0xF0, R1 = 0x0F, TST R0, R1 -> 0xF0 & 0x0F = 0, Z=1 */
    exec.cpu.r[0] = 0xF0;
    exec.cpu.r[1] = 0x0F;
    write_insn16(0, TST_REG(0, 1));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_TRUE(exec.cpu.xpsr & (1u << 30));   /* Z flag set */
}

/*============================================================================
 * Test Group: Load/Store Operations
 *============================================================================*/

