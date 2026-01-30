/**
 * @file test_integration_memory.cpp
 * @brief Load/store and stack integration tests
 */

#include "test_integration_common.h"

TEST_GROUP(IntegrationLoadStore)
{
    void setup()
    {
        memset(test_memory, 0, sizeof(test_memory));
        memset(&exec, 0, sizeof(exec));
        exec.mem.read = integ_mem_read;
        exec.mem.write = integ_mem_write;
        exec.mem.get_ptr = integ_mem_get_ptr;
        exec.cpu.sp_main = STACK_BASE;  /* Main stack pointer */
        exec.cpu.r[15] = CODE_BASE;
        exec.cpu.mode = MODE_THREAD;
        exec.cpu.control = 0;
    }

    void teardown() {}
};

TEST(IntegrationLoadStore, StoreWord)
{
    /* R0 = DATA_BASE, R1 = 0xDEADBEEF, STR R1, [R0, #0] */
    exec.cpu.r[0] = DATA_BASE;
    exec.cpu.r[1] = 0xDEADBEEF;
    write_insn16(0, STR_IMM(1, 0, 0));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0xDEADBEEFu, read_word(DATA_BASE));
}

TEST(IntegrationLoadStore, LoadWord)
{
    /* Store value at DATA_BASE, then LDR R1, [R0, #0] */
    write_word(DATA_BASE, 0xCAFEBABE);
    exec.cpu.r[0] = DATA_BASE;
    write_insn16(0, LDR_IMM(1, 0, 0));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0xCAFEBABEu, exec.cpu.r[1]);
}

TEST(IntegrationLoadStore, StoreWordWithOffset)
{
    /* R0 = DATA_BASE, R1 = 0x12345678, STR R1, [R0, #8] */
    exec.cpu.r[0] = DATA_BASE;
    exec.cpu.r[1] = 0x12345678;
    write_insn16(0, STR_IMM(1, 0, 8));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0x12345678u, read_word(DATA_BASE + 8));
}

TEST(IntegrationLoadStore, LoadWordWithOffset)
{
    /* Store value at DATA_BASE+12, then LDR R1, [R0, #12] */
    write_word(DATA_BASE + 12, 0xABCDEF01);
    exec.cpu.r[0] = DATA_BASE;
    write_insn16(0, LDR_IMM(1, 0, 12));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0xABCDEF01u, exec.cpu.r[1]);
}

TEST(IntegrationLoadStore, StoreByte)
{
    /* R0 = DATA_BASE, R1 = 0x42, STRB R1, [R0, #3] */
    exec.cpu.r[0] = DATA_BASE;
    exec.cpu.r[1] = 0x42;
    write_insn16(0, STRB_IMM(1, 0, 3));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0x42u, test_memory[DATA_BASE + 3]);
}

TEST(IntegrationLoadStore, LoadByte)
{
    /* Store byte at DATA_BASE+5, then LDRB R1, [R0, #5] */
    test_memory[DATA_BASE + 5] = 0x99;
    exec.cpu.r[0] = DATA_BASE;
    write_insn16(0, LDRB_IMM(1, 0, 5));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0x99u, exec.cpu.r[1]);
}

TEST(IntegrationLoadStore, StoreHalfword)
{
    /* R0 = DATA_BASE, R1 = 0xBEEF, STRH R1, [R0, #4] */
    exec.cpu.r[0] = DATA_BASE;
    exec.cpu.r[1] = 0xBEEF;
    write_insn16(0, STRH_IMM(1, 0, 4));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0xEFu, test_memory[DATA_BASE + 4]);
    CHECK_EQUAL(0xBEu, test_memory[DATA_BASE + 5]);
}

TEST(IntegrationLoadStore, LoadHalfword)
{
    /* Store halfword at DATA_BASE+6, then LDRH R1, [R0, #6] */
    test_memory[DATA_BASE + 6] = 0xCD;
    test_memory[DATA_BASE + 7] = 0xAB;
    exec.cpu.r[0] = DATA_BASE;
    write_insn16(0, LDRH_IMM(1, 0, 6));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0xABCDu, exec.cpu.r[1]);
}

TEST(IntegrationLoadStore, StoreToStack)
{
    /* R0 = 0x11223344, STR R0, [SP, #0] */
    exec.cpu.r[0] = 0x11223344;
    write_insn16(0, STR_SP(0, 0));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0x11223344u, read_word(STACK_BASE));
}

TEST(IntegrationLoadStore, LoadFromStack)
{
    /* Store at SP+8, then LDR R0, [SP, #8] */
    write_word(STACK_BASE + 8, 0x55667788);
    write_insn16(0, LDR_SP(0, 8));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0x55667788u, exec.cpu.r[0]);
}

/*============================================================================
 * Test Group: Branch Operations
 *============================================================================*/

TEST_GROUP(IntegrationStack)
{
    void setup()
    {
        memset(test_memory, 0, sizeof(test_memory));
        memset(&exec, 0, sizeof(exec));
        exec.mem.read = integ_mem_read;
        exec.mem.write = integ_mem_write;
        exec.mem.get_ptr = integ_mem_get_ptr;
        exec.cpu.sp_main = STACK_BASE;  /* Main stack pointer */
        exec.cpu.r[15] = CODE_BASE;
        exec.cpu.mode = MODE_THREAD;
        exec.cpu.control = 0;
    }

    void teardown() {}
};

TEST(IntegrationStack, PushSingleRegister)
{
    /* R0 = 0xAAAAAAAA, PUSH {R0} */
    exec.cpu.r[0] = 0xAAAAAAAA;
    write_insn16(0, PUSH(0x01));  /* R0 */

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(STACK_BASE - 4, exec.cpu.r[13]);  /* SP decremented */
    CHECK_EQUAL(0xAAAAAAAAu, read_word(STACK_BASE - 4));
}

TEST(IntegrationStack, PopSingleRegister)
{
    /* Store value on stack, then POP {R0} */
    write_word(STACK_BASE, 0xBBBBBBBB);
    write_insn16(0, POP(0x01));  /* R0 */

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(STACK_BASE + 4, exec.cpu.r[13]);  /* SP incremented */
    CHECK_EQUAL(0xBBBBBBBBu, exec.cpu.r[0]);
}

TEST(IntegrationStack, PushMultipleRegisters)
{
    /* R0=1, R1=2, R2=3, PUSH {R0, R1, R2} */
    exec.cpu.r[0] = 1;
    exec.cpu.r[1] = 2;
    exec.cpu.r[2] = 3;
    write_insn16(0, PUSH(0x07));  /* R0, R1, R2 */

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(STACK_BASE - 12, exec.cpu.r[13]);
    /* Lower register at lower address */
    CHECK_EQUAL(1u, read_word(STACK_BASE - 12));
    CHECK_EQUAL(2u, read_word(STACK_BASE - 8));
    CHECK_EQUAL(3u, read_word(STACK_BASE - 4));
}

TEST(IntegrationStack, PopMultipleRegisters)
{
    /* Store values on stack, then POP {R0, R1, R2} */
    write_word(STACK_BASE, 10);
    write_word(STACK_BASE + 4, 20);
    write_word(STACK_BASE + 8, 30);
    write_insn16(0, POP(0x07));  /* R0, R1, R2 */

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(STACK_BASE + 12, exec.cpu.r[13]);
    CHECK_EQUAL(10u, exec.cpu.r[0]);
    CHECK_EQUAL(20u, exec.cpu.r[1]);
    CHECK_EQUAL(30u, exec.cpu.r[2]);
}

TEST(IntegrationStack, PushWithLR)
{
    /* R0=1, LR=0x12345678, PUSH {R0, LR} */
    exec.cpu.r[0] = 1;
    exec.cpu.r[14] = 0x12345678;
    write_insn16(0, PUSH_LR(0x01));  /* R0 + LR */

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(STACK_BASE - 8, exec.cpu.r[13]);
    CHECK_EQUAL(1u, read_word(STACK_BASE - 8));         /* R0 */
    CHECK_EQUAL(0x12345678u, read_word(STACK_BASE - 4)); /* LR */
}

TEST(IntegrationStack, PopWithPC)
{
    /* Store return address on stack, then POP {R0, PC} */
    write_word(STACK_BASE, 42);
    write_word(STACK_BASE + 4, 0x101);  /* Return address with Thumb bit */
    write_insn16(0, POP_PC(0x01));  /* R0 + PC */

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(STACK_BASE + 8, exec.cpu.r[13]);
    CHECK_EQUAL(42u, exec.cpu.r[0]);
    CHECK_EQUAL(0x100u, exec.cpu.r[15]);  /* Thumb bit cleared */
}

TEST(IntegrationStack, AddToSP)
{
    /* ADD SP, #16 */
    write_insn16(0, ADD_SP_IMM(16));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(STACK_BASE + 16, exec.cpu.r[13]);
}

TEST(IntegrationStack, SubFromSP)
{
    /* SUB SP, #32 */
    write_insn16(0, SUB_SP_IMM(32));

    int result = armv8m_exec_step(&exec);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(STACK_BASE - 32, exec.cpu.r[13]);
}

/*============================================================================
 * Test Group: Program Execution (exec_run)
 *============================================================================*/

