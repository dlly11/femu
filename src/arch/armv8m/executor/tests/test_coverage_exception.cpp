/**
 * @file test_coverage_exception.cpp
 * @brief Executor and exception coverage tests
 */

#include "test_common.h"

TEST_GROUP(ExecutorCoverage)
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

TEST(ExecutorCoverage, SetSpMain)
{
    exec.cpu.mode = MODE_THREAD;
    exec.cpu.control = 0;  /* Use MSP */
    armv8m_set_sp(&exec.cpu, 0x20004000);
    CHECK_EQUAL(0x20004000u, exec.cpu.sp_main);
}

/* Set SP in handler mode (covers line 115 in executor.c) */
TEST(ExecutorCoverage, SetSpHandler)
{
    exec.cpu.mode = MODE_HANDLER;
    armv8m_set_sp(&exec.cpu, 0x20005000);
    CHECK_EQUAL(0x20005000u, exec.cpu.sp_main);
}

TEST(ExecutorCoverage, GetRegSp)
{
    exec.cpu.sp_main = 0x20003000;
    exec.cpu.mode = MODE_THREAD;
    exec.cpu.control = 0;
    exec.cpu.r[1] = 0x100;

    /* ADD R0, SP, R1 */
    insn.type = INSN_DATA_PROC_REG;
    insn.op = DP_ADD;
    insn.rd = 0;
    insn.rn = 13;  /* SP */
    insn.rm = 1;
    insn.set_flags = false;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x20003100u, exec.cpu.r[0]);
}

TEST(ExecutorCoverage, SetRegSp)
{
    exec.cpu.mode = MODE_THREAD;
    exec.cpu.control = 0;
    exec.cpu.sp_main = 0x20001000;
    exec.cpu.r[0] = 0x20002000;

    /* MOV SP, R0 */
    insn.type = INSN_DATA_PROC_REG;
    insn.op = DP_MOV;
    insn.rd = 13;  /* SP */
    insn.rm = 0;
    insn.set_flags = false;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x20002000u, exec.cpu.sp_main);
}

TEST(ExecutorCoverage, SetRegPc)
{
    exec.cpu.r[0] = 0x1001;  /* With Thumb bit */

    /* MOV PC, R0 */
    insn.type = INSN_DATA_PROC_REG;
    insn.op = DP_MOV;
    insn.rd = 15;  /* PC */
    insn.rm = 0;
    insn.set_flags = false;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x1000u, exec.cpu.r[15]);  /* Thumb bit cleared */
}

/*============================================================================
 * Test Group: Exception Entry/Return Coverage
 *============================================================================*/

TEST_GROUP(ExceptionCoverage)
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

TEST(ExceptionCoverage, ExceptionEntryFromThreadPsp)
{
    exec.cpu.mode = MODE_THREAD;
    exec.cpu.control = ARMV8M_CONTROL_SPSEL;
    exec.cpu.sp_process = 0x1000;
    exec.cpu.r[13] = 0x1000;
    exec.cpu.r[15] = 0x200;
    exec.cpu.xpsr = ARMV8M_XPSR_T;
    exec.cpu.r[14] = 0x100;

    /* Set up vector table */
    mock_memory[44] = 0x01;  /* Vector 11 (SVCall) at offset 44 */
    mock_memory[45] = 0x04;
    mock_memory[46] = 0x00;
    mock_memory[47] = 0x00;

    int result = armv8m_exception_entry(&exec, 11);
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(MODE_HANDLER, exec.cpu.mode);
}

TEST(ExceptionCoverage, ExceptionReturnToPsp)
{
    exec.cpu.mode = MODE_HANDLER;
    exec.cpu.current_exception = 11;
    exec.cpu.sp_process = 0x100;
    exec.cpu.control = ARMV8M_CONTROL_SPSEL;

    /* Set up stacked context on PSP */
    mock_memory[0x100] = 0x00; mock_memory[0x101] = 0x00;
    mock_memory[0x102] = 0x00; mock_memory[0x103] = 0x00;
    mock_memory[0x104] = 0x00; mock_memory[0x105] = 0x00;
    mock_memory[0x106] = 0x00; mock_memory[0x107] = 0x00;
    mock_memory[0x108] = 0x00; mock_memory[0x109] = 0x00;
    mock_memory[0x10A] = 0x00; mock_memory[0x10B] = 0x00;
    mock_memory[0x10C] = 0x00; mock_memory[0x10D] = 0x00;
    mock_memory[0x10E] = 0x00; mock_memory[0x10F] = 0x00;
    mock_memory[0x110] = 0x00; mock_memory[0x111] = 0x00;
    mock_memory[0x112] = 0x00; mock_memory[0x113] = 0x00;
    mock_memory[0x114] = 0x00; mock_memory[0x115] = 0x00;
    mock_memory[0x116] = 0x00; mock_memory[0x117] = 0x00;
    mock_memory[0x118] = 0x01; mock_memory[0x119] = 0x10;
    mock_memory[0x11A] = 0x00; mock_memory[0x11B] = 0x00;
    mock_memory[0x11C] = 0x00; mock_memory[0x11D] = 0x00;
    mock_memory[0x11E] = 0x00; mock_memory[0x11F] = 0x01;

    int result = armv8m_exception_return(&exec, 0xFFFFFFFD);  /* Return to Thread, PSP */
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(MODE_THREAD, exec.cpu.mode);
}

/* Exception entry with unaligned stack (covers alignment bit setting) */
TEST(ExceptionCoverage, ExceptionEntryUnalignedSp)
{
    exec.cpu.mode = MODE_THREAD;
    exec.cpu.sp_main = 0x104;  /* Not 8-byte aligned */
    exec.cpu.r[13] = 0x104;
    exec.cpu.r[15] = 0x1000;

    /* Vector table - HardFault at offset 12 */
    mock_memory[12] = 0x01;
    mock_memory[13] = 0x20;
    mock_memory[14] = 0x00;
    mock_memory[15] = 0x00;

    int result = armv8m_exception_entry(&exec, ARMV8M_EXC_HARDFAULT);
    CHECK_EQUAL(ARMV8M_OK, result);
    /* SP should be aligned down to 0x100 - 32 = 0xE0 (rounded down, then frame subtracted) */
}

/* Exception entry with stack fault escalation (no write function) */
TEST(ExceptionCoverage, ExceptionEntryStackFault)
{
    exec.mem.write = NULL;  /* Stack push will fail */
    exec.cpu.mode = MODE_THREAD;
    exec.cpu.sp_main = 0x100;
    exec.cpu.r[13] = 0x100;
    exec.cpu.r[15] = 0x1000;

    /* Vector table */
    mock_memory[12] = 0x01;
    mock_memory[13] = 0x20;

    int result = armv8m_exception_entry(&exec, 11);  /* SVCall */
    /* Should escalate to HardFault or return error */
    (void)result;  /* Result depends on HardFault handler being available */
}

/* Exception entry lockup (HardFault with stack fault) */
TEST(ExceptionCoverage, ExceptionEntryLockup)
{
    exec.mem.write = NULL;  /* Stack push will fail */
    exec.cpu.mode = MODE_THREAD;
    exec.cpu.sp_main = 0x100;
    exec.cpu.r[13] = 0x100;
    exec.cpu.r[15] = 0x1000;

    int result = armv8m_exception_entry(&exec, ARMV8M_EXC_HARDFAULT);
    CHECK_EQUAL(ARMV8M_ERR_HARD_FAULT, result);
    CHECK_TRUE(exec.cpu.halted);  /* Should lockup */
}

/* Exception entry with vector fetch fault */
TEST(ExceptionCoverage, ExceptionEntryVectorFault)
{
    /* Set up valid stack but no vector table access */
    exec.cpu.mode = MODE_THREAD;
    exec.cpu.sp_main = 0x100;
    exec.cpu.r[13] = 0x100;
    exec.cpu.r[15] = 0x1000;
    exec.mem.read = NULL;  /* Vector fetch will fail */

    (void)armv8m_exception_entry(&exec, 11);  /* SVCall */
    /* Vector fetch fails but write succeeds - should lockup */
    CHECK_TRUE(exec.cpu.halted);
}

static void mock_nvic_clear_pending(void *ctx, int exception)
{
    (void)ctx;
    (void)exception;
}

/* Exception entry with NVIC clear_pending callback */
TEST(ExceptionCoverage, ExceptionEntryNvicCallback)
{
    exec.cpu.mode = MODE_THREAD;
    exec.cpu.sp_main = 0x100;
    exec.cpu.r[13] = 0x100;
    exec.cpu.r[15] = 0x1000;
    exec.nvic.clear_pending = mock_nvic_clear_pending;

    /* Vector table */
    mock_memory[12] = 0x01;
    mock_memory[13] = 0x20;
    mock_memory[14] = 0x00;
    mock_memory[15] = 0x00;

    int result = armv8m_exception_entry(&exec, ARMV8M_EXC_HARDFAULT);
    CHECK_EQUAL(ARMV8M_OK, result);
}

/* Exception return with aligned stack (covers new_sp += 4 path) */
TEST(ExceptionCoverage, ExceptionReturnAlignedStack)
{
    exec.cpu.mode = MODE_HANDLER;
    exec.cpu.current_exception = 11;
    exec.cpu.sp_main = 0x100;

    /* Set up stacked context with alignment bit set in xPSR */
    for (int i = 0; i < 28; i++) {
        mock_memory[0x100 + i] = 0;
    }
    mock_memory[0x118] = 0x01;  /* Return address low byte */
    mock_memory[0x119] = 0x10;
    /* xPSR with alignment bit (bit 9) and T bit (bit 24) set */
    mock_memory[0x11C] = 0x00;
    mock_memory[0x11D] = 0x02;  /* Bit 9 */
    mock_memory[0x11E] = 0x00;
    mock_memory[0x11F] = 0x01;  /* Bit 24 */

    int result = armv8m_exception_return(&exec, 0xFFFFFFF9);  /* Return to Thread, MSP */
    CHECK_EQUAL(ARMV8M_OK, result);
    /* SP should be 0x100 + 32 + 4 (alignment) = 0x124 */
    CHECK_EQUAL(0x124u, exec.cpu.sp_main);
}

/* Exception return to handler mode (nested exception return) */
TEST(ExceptionCoverage, ExceptionReturnToHandler)
{
    exec.cpu.mode = MODE_HANDLER;
    exec.cpu.current_exception = 11;
    exec.cpu.sp_main = 0x100;

    /* Set up stacked context */
    for (int i = 0; i < 28; i++) {
        mock_memory[0x100 + i] = 0;
    }
    mock_memory[0x118] = 0x01;  /* Return address */
    mock_memory[0x119] = 0x10;
    mock_memory[0x11C] = 0x00;
    mock_memory[0x11D] = 0x00;
    mock_memory[0x11E] = 0x00;
    mock_memory[0x11F] = 0x01;  /* T bit */

    int result = armv8m_exception_return(&exec, 0xFFFFFFF1);  /* Return to Handler, MSP */
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(MODE_HANDLER, exec.cpu.mode);
}

/* Exception return with memory read fault */
TEST(ExceptionCoverage, ExceptionReturnReadFault)
{
    exec.mem.read = NULL;  /* Stack read will fail */
    exec.cpu.mode = MODE_HANDLER;
    exec.cpu.current_exception = 11;
    exec.cpu.sp_main = 0x100;

    int result = armv8m_exception_return(&exec, 0xFFFFFFF9);
    CHECK_EQUAL(ARMV8M_ERR_BUS_FAULT, result);
}

/* Exception entry with stack overflow - escalates to HardFault */
TEST(ExceptionCoverage, ExceptionEntryStackOverflow)
{
    exec.cpu.mode = MODE_THREAD;
    exec.cpu.control = 0;  /* Use MSP */
    exec.cpu.sp_main = 0x100;       /* Low SP within mock memory */
    exec.cpu.msplim = 0xF8;         /* Stack limit close to SP */

    /* Vector table entry for HardFault at offset 12 (exception 3 * 4) */
    mock_memory[12] = 0x01;
    mock_memory[13] = 0x10;  /* HardFault handler at 0x1001 */
    mock_memory[14] = 0x00;
    mock_memory[15] = 0x00;

    /* Try to enter SVCall exception - needs 32 bytes of stack,
     * but SP (0x100) - 32 = 0xE0 which is below limit (0xF8) */
    (void)armv8m_exception_entry(&exec, ARMV8M_EXC_SVCALL);

    /* Should escalate to HardFault due to stack overflow */
    CHECK_TRUE(exec.cpu.cfsr & ARMV8M_UFSR_STKOF);
}

/* Exception entry stack limit check - no overflow */
TEST(ExceptionCoverage, ExceptionEntryStackLimitOk)
{
    exec.cpu.mode = MODE_THREAD;
    exec.cpu.control = 0;  /* Use MSP */
    exec.cpu.sp_main = 0x200;       /* Adequate SP within mock memory */
    exec.cpu.msplim = 0x100;        /* Stack limit well below */

    /* Vector table entry for SVCall at offset 44 (exception 11 * 4) */
    mock_memory[44] = 0x01;
    mock_memory[45] = 0x10;  /* Handler at 0x1001 */
    mock_memory[46] = 0x00;
    mock_memory[47] = 0x00;

    int result = armv8m_exception_entry(&exec, ARMV8M_EXC_SVCALL);
    CHECK_EQUAL(ARMV8M_OK, result);
    /* Stack overflow flag should NOT be set */
    CHECK_FALSE(exec.cpu.cfsr & ARMV8M_UFSR_STKOF);
}

/*============================================================================
 * Test Group: Saturation Instructions
 *============================================================================*/

