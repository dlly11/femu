/**
 * @file test_coverage_system.cpp
 * @brief System instruction coverage tests
 */

#include "test_common.h"

TEST_GROUP(SystemCoverage)
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

/* MRS tests for uncovered system registers */
TEST(SystemCoverage, MrsIapsr)
{
    exec.cpu.xpsr = ARMV8M_XPSR_N | ARMV8M_XPSR_C;
    exec.cpu.current_exception = 5;
    insn.type = INSN_MRS;
    insn.rd = 0;
    insn.sysreg = 0x01;  /* IAPSR */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL((ARMV8M_XPSR_N | ARMV8M_XPSR_C) | 5u, exec.cpu.r[0]);
}

TEST(SystemCoverage, MrsEapsr)
{
    exec.cpu.xpsr = ARMV8M_XPSR_N | ARMV8M_XPSR_Z;
    insn.type = INSN_MRS;
    insn.rd = 0;
    insn.sysreg = 0x02;  /* EAPSR */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(ARMV8M_XPSR_N | ARMV8M_XPSR_Z, exec.cpu.r[0]);
}

TEST(SystemCoverage, MrsXpsr)
{
    exec.cpu.xpsr = ARMV8M_XPSR_V | ARMV8M_XPSR_C;
    exec.cpu.current_exception = 11;
    insn.type = INSN_MRS;
    insn.rd = 0;
    insn.sysreg = 0x03;  /* XPSR */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL((ARMV8M_XPSR_V | ARMV8M_XPSR_C) | 11u, exec.cpu.r[0]);
}

TEST(SystemCoverage, MrsBasepri)
{
    exec.cpu.basepri = 0x40;
    insn.type = INSN_MRS;
    insn.rd = 0;
    insn.sysreg = 0x11;  /* BASEPRI */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x40u, exec.cpu.r[0]);
}

TEST(SystemCoverage, MrsFaultmask)
{
    exec.cpu.faultmask = 1;
    insn.type = INSN_MRS;
    insn.rd = 0;
    insn.sysreg = 0x13;  /* FAULTMASK */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(1u, exec.cpu.r[0]);
}

/* MSR tests for uncovered system registers */
TEST(SystemCoverage, MsrIapsr)
{
    exec.cpu.r[0] = ARMV8M_XPSR_N | ARMV8M_XPSR_Z;
    exec.cpu.xpsr = ARMV8M_XPSR_T;
    insn.type = INSN_MSR;
    insn.rn = 0;
    insn.sysreg = 0x01;  /* IAPSR */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_N);
    CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_Z);
}

TEST(SystemCoverage, MsrMsp)
{
    exec.cpu.r[0] = 0x20004000;
    exec.cpu.mode = MODE_HANDLER;  /* Privileged */
    insn.type = INSN_MSR;
    insn.rn = 0;
    insn.sysreg = 0x08;  /* MSP */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x20004000u, exec.cpu.sp_main);
    CHECK_EQUAL(0x20004000u, exec.cpu.r[13]);  /* Handler mode uses MSP */
}

TEST(SystemCoverage, MsrMspThreadMode)
{
    exec.cpu.r[0] = 0x20004000;
    exec.cpu.mode = MODE_THREAD;
    exec.cpu.control = 0;  /* SPSEL=0, use MSP */
    insn.type = INSN_MSR;
    insn.rn = 0;
    insn.sysreg = 0x08;  /* MSP */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x20004000u, exec.cpu.sp_main);
    CHECK_EQUAL(0x20004000u, exec.cpu.r[13]);
}

TEST(SystemCoverage, MsrPsp)
{
    exec.cpu.r[0] = 0x20005000;
    exec.cpu.mode = MODE_THREAD;
    exec.cpu.control = ARMV8M_CONTROL_SPSEL;  /* Use PSP */
    insn.type = INSN_MSR;
    insn.rn = 0;
    insn.sysreg = 0x09;  /* PSP */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x20005000u, exec.cpu.sp_process);
    CHECK_EQUAL(0x20005000u, exec.cpu.r[13]);
}

TEST(SystemCoverage, MsrPspThreadModeNoSpsel)
{
    exec.cpu.r[0] = 0x20005000;
    exec.cpu.mode = MODE_THREAD;
    exec.cpu.control = 0;  /* SPSEL=0, use MSP */
    exec.cpu.sp_main = 0x20001000;
    exec.cpu.r[13] = 0x20001000;
    insn.type = INSN_MSR;
    insn.rn = 0;
    insn.sysreg = 0x09;  /* PSP */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x20005000u, exec.cpu.sp_process);
    CHECK_EQUAL(0x20001000u, exec.cpu.r[13]);  /* Should not change */
}

TEST(SystemCoverage, MsrBasepri)
{
    exec.cpu.r[0] = 0x80;
    insn.type = INSN_MSR;
    insn.rn = 0;
    insn.sysreg = 0x11;  /* BASEPRI */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x80u, exec.cpu.basepri);
}

TEST(SystemCoverage, MsrBasepriMax)
{
    exec.cpu.basepri = 0;
    exec.cpu.r[0] = 0x40;
    insn.type = INSN_MSR;
    insn.rn = 0;
    insn.sysreg = 0x12;  /* BASEPRI_MAX */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x40u, exec.cpu.basepri);
}

TEST(SystemCoverage, MsrBasepriMaxNoChange)
{
    exec.cpu.basepri = 0x20;
    exec.cpu.r[0] = 0x40;  /* Higher (lower priority) than current */
    insn.type = INSN_MSR;
    insn.rn = 0;
    insn.sysreg = 0x12;  /* BASEPRI_MAX */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x20u, exec.cpu.basepri);  /* Should not change */
}

TEST(SystemCoverage, MsrFaultmask)
{
    exec.cpu.mode = MODE_HANDLER;
    exec.cpu.r[0] = 1;
    insn.type = INSN_MSR;
    insn.rn = 0;
    insn.sysreg = 0x13;  /* FAULTMASK */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(1u, exec.cpu.faultmask);
}

TEST(SystemCoverage, MsrFaultmaskThreadMode)
{
    exec.cpu.mode = MODE_THREAD;
    exec.cpu.faultmask = 0;
    exec.cpu.r[0] = 1;
    insn.type = INSN_MSR;
    insn.rn = 0;
    insn.sysreg = 0x13;  /* FAULTMASK */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0u, exec.cpu.faultmask);  /* Should not change in thread mode */
}

TEST(SystemCoverage, MsrControl)
{
    exec.cpu.mode = MODE_THREAD;
    exec.cpu.control = 0;
    exec.cpu.sp_process = 0x20003000;
    exec.cpu.r[0] = ARMV8M_CONTROL_SPSEL;
    insn.type = INSN_MSR;
    insn.rn = 0;
    insn.sysreg = 0x14;  /* CONTROL */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(ARMV8M_CONTROL_SPSEL, exec.cpu.control);
    CHECK_EQUAL(0x20003000u, exec.cpu.r[13]);  /* Should switch to PSP */
}

TEST(SystemCoverage, MsrControlClearSpsel)
{
    exec.cpu.mode = MODE_THREAD;
    exec.cpu.control = ARMV8M_CONTROL_SPSEL;
    exec.cpu.sp_main = 0x20001000;
    exec.cpu.r[0] = 0;  /* Clear SPSEL */
    insn.type = INSN_MSR;
    insn.rn = 0;
    insn.sysreg = 0x14;  /* CONTROL */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0u, exec.cpu.control);
    CHECK_EQUAL(0x20001000u, exec.cpu.r[13]);  /* Should switch to MSP */
}

TEST(SystemCoverage, MsrControlNpriv)
{
    exec.cpu.mode = MODE_THREAD;
    exec.cpu.control = 0;
    exec.cpu.r[0] = ARMV8M_CONTROL_NPRIV;
    insn.type = INSN_MSR;
    insn.rn = 0;
    insn.sysreg = 0x14;  /* CONTROL */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(ARMV8M_CONTROL_NPRIV, exec.cpu.control);
}

/* CPS tests for FAULTMASK */
TEST(SystemCoverage, CpsidF)
{
    exec.cpu.mode = MODE_HANDLER;
    exec.cpu.faultmask = 0;
    insn.type = INSN_CPS;
    insn.op = 1;  /* Disable */
    insn.imm = 0x11;  /* bit 4 = disable, bit 0 = affect FAULTMASK */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(1u, exec.cpu.faultmask);
}

TEST(SystemCoverage, CpsieF)
{
    exec.cpu.mode = MODE_HANDLER;
    exec.cpu.faultmask = 1;
    insn.type = INSN_CPS;
    insn.op = 0;  /* Enable */
    insn.imm = 0x01;  /* bit 0 = affect FAULTMASK */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0u, exec.cpu.faultmask);
}

/* Hint tests */
TEST(SystemCoverage, Sevl)
{
    exec.cpu.event_registered = false;
    insn.type = INSN_HINT;
    insn.op = HINT_SEVL;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_TRUE(exec.cpu.event_registered);
}

TEST(SystemCoverage, WfeNoEvent)
{
    exec.cpu.event_registered = false;
    insn.type = INSN_HINT;
    insn.op = HINT_WFE;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_TRUE(exec.cpu.sleeping);
}

/* SVC test */
TEST(SystemCoverage, Svc)
{
    /* Set up memory for exception stacking */
    exec.cpu.sp_main = 0x1000;
    exec.cpu.r[13] = 0x1000;
    insn.type = INSN_SVC;
    insn.imm = 0;

    int result = armv8m_exec_insn(&exec, &insn);
    /* SVC triggers exception entry which may succeed or fail depending on memory setup */
    (void)result;
    /* Just verify it doesn't crash - exception entry is tested elsewhere */
}

/* MRS with rd = SP (covers set_reg SP path) */
TEST(SystemCoverage, MrsToSp)
{
    exec.cpu.sp_main = 0x20001000;
    exec.cpu.r[13] = 0x20001000;
    exec.cpu.xpsr = ARMV8M_XPSR_N;
    insn.type = INSN_MRS;
    insn.rd = 13;  /* SP */
    insn.sysreg = 0x00;  /* APSR */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    /* SP should be set to APSR value (0x80000000) */
    CHECK_EQUAL(ARMV8M_XPSR_N, armv8m_get_sp(&exec.cpu));
}

/* MRS with rd = PC (covers set_reg PC path) */
TEST(SystemCoverage, MrsToPc)
{
    exec.cpu.xpsr = 0;
    exec.cpu.r[15] = 0x1000;
    insn.type = INSN_MRS;
    insn.rd = 15;  /* PC */
    insn.sysreg = 0x00;  /* APSR */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    /* PC should be set to APSR value (0) with bit 0 cleared */
    CHECK_EQUAL(0u, exec.cpu.r[15]);
}

/* MSR with rn = SP (covers get_reg SP path) */
TEST(SystemCoverage, MsrFromSp)
{
    exec.cpu.sp_main = 0x80000000;
    exec.cpu.r[13] = 0x80000000;
    exec.cpu.xpsr = 0;
    exec.cpu.mode = MODE_HANDLER;  /* Privileged */
    insn.type = INSN_MSR;
    insn.rn = 13;  /* SP as source */
    insn.sysreg = 0x00;  /* APSR */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    /* APSR flags should be set from SP value (N flag set) */
    CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_N);
}

/*============================================================================
 * Test Group: Additional Load/Store Coverage
 *============================================================================*/

