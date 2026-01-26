/**
 * @file test_nvic.cpp
 * @brief Comprehensive CppUTest tests for the ARMv8-M NVIC
 */

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

extern "C" {
#include "armv8m_nvic.h"
#include "armv8m_types.h"
}

/*============================================================================
 * Test Group: NVIC Initialization
 *============================================================================*/

TEST_GROUP(NVICInit)
{
    NVIC nvic;

    void setup() {}
    void teardown() {}
};

TEST(NVICInit, InitSetsDefaults)
{
    armv8m_nvic_init(&nvic, 32);
    CHECK_EQUAL(32, nvic.num_irqs);
    CHECK_EQUAL(0u, nvic.vtor);

    for (int i = 0; i < 8; i++) {
        CHECK_EQUAL(0u, nvic.enabled[i]);
        CHECK_EQUAL(0u, nvic.pending[i]);
        CHECK_EQUAL(0u, nvic.active[i]);
    }
}

TEST(NVICInit, InitClampsMaxIRQs)
{
    armv8m_nvic_init(&nvic, 500);
    CHECK_EQUAL(240, nvic.num_irqs);
}

TEST(NVICInit, InitClampsNegativeIRQs)
{
    armv8m_nvic_init(&nvic, -5);
    CHECK_EQUAL(0, nvic.num_irqs);
}

TEST(NVICInit, ResetPreservesNumIRQs)
{
    armv8m_nvic_init(&nvic, 64);
    armv8m_nvic_enable_irq(&nvic, 10);
    armv8m_nvic_reset(&nvic);
    CHECK_EQUAL(64, nvic.num_irqs);
    CHECK_EQUAL(0u, nvic.enabled[0]);
}

/*============================================================================
 * Test Group: Interrupt Enable/Disable
 *============================================================================*/

TEST_GROUP(InterruptEnable)
{
    NVIC nvic;

    void setup()
    {
        armv8m_nvic_init(&nvic, 64);
    }

    void teardown() {}
};

TEST(InterruptEnable, EnableIRQ)
{
    armv8m_nvic_enable_irq(&nvic, 0);
    CHECK_TRUE(nvic.enabled[0] & 1);

    armv8m_nvic_enable_irq(&nvic, 31);
    CHECK_TRUE(nvic.enabled[0] & (1u << 31));

    armv8m_nvic_enable_irq(&nvic, 32);
    CHECK_TRUE(nvic.enabled[1] & 1);
}

TEST(InterruptEnable, DisableIRQ)
{
    armv8m_nvic_enable_irq(&nvic, 5);
    armv8m_nvic_disable_irq(&nvic, 5);
    CHECK_FALSE(nvic.enabled[0] & (1u << 5));
}

TEST(InterruptEnable, EnableInvalidIRQ)
{
    armv8m_nvic_enable_irq(&nvic, -1);
    armv8m_nvic_enable_irq(&nvic, 100);
    CHECK_EQUAL(0u, nvic.enabled[0]);
}

TEST(InterruptEnable, DisableInvalidIRQ)
{
    nvic.enabled[0] = 0xFFFFFFFF;
    armv8m_nvic_disable_irq(&nvic, -1);
    armv8m_nvic_disable_irq(&nvic, 100);
    CHECK_EQUAL(0xFFFFFFFFu, nvic.enabled[0]);
}

/*============================================================================
 * Test Group: Pending State
 *============================================================================*/

TEST_GROUP(PendingState)
{
    NVIC nvic;

    void setup()
    {
        armv8m_nvic_init(&nvic, 64);
    }

    void teardown() {}
};

TEST(PendingState, SetPending)
{
    armv8m_nvic_set_pending(&nvic, 0);
    CHECK_TRUE(nvic.pending[0] & 1);

    armv8m_nvic_set_pending(&nvic, 32);
    CHECK_TRUE(nvic.pending[1] & 1);
}

TEST(PendingState, ClearPending)
{
    armv8m_nvic_set_pending(&nvic, 10);
    armv8m_nvic_clear_pending(&nvic, 10);
    CHECK_FALSE(nvic.pending[0] & (1u << 10));
}

TEST(PendingState, SetPendingInvalidIRQ)
{
    armv8m_nvic_set_pending(&nvic, -1);
    armv8m_nvic_set_pending(&nvic, 100);
    CHECK_EQUAL(0u, nvic.pending[0]);
}

TEST(PendingState, ClearPendingInvalidIRQ)
{
    nvic.pending[0] = 0xFFFFFFFF;
    armv8m_nvic_clear_pending(&nvic, -1);
    armv8m_nvic_clear_pending(&nvic, 100);
    CHECK_EQUAL(0xFFFFFFFFu, nvic.pending[0]);
}

TEST(PendingState, SetExceptionPendingNMI)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_NMI);
    CHECK_TRUE(nvic.shcsr & (1u << 31));
    armv8m_nvic_clear_exception_pending(&nvic, ARMV8M_EXC_NMI);
    CHECK_FALSE(nvic.shcsr & (1u << 31));
}

TEST(PendingState, SetExceptionPendingMemManage)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_MEMMANAGE);
    CHECK_TRUE(nvic.shcsr & (1u << 13));
    armv8m_nvic_clear_exception_pending(&nvic, ARMV8M_EXC_MEMMANAGE);
    CHECK_FALSE(nvic.shcsr & (1u << 13));
}

TEST(PendingState, SetExceptionPendingBusFault)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_BUSFAULT);
    CHECK_TRUE(nvic.shcsr & (1u << 14));
    armv8m_nvic_clear_exception_pending(&nvic, ARMV8M_EXC_BUSFAULT);
    CHECK_FALSE(nvic.shcsr & (1u << 14));
}

TEST(PendingState, SetExceptionPendingUsageFault)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_USAGEFAULT);
    CHECK_TRUE(nvic.shcsr & (1u << 12));
    armv8m_nvic_clear_exception_pending(&nvic, ARMV8M_EXC_USAGEFAULT);
    CHECK_FALSE(nvic.shcsr & (1u << 12));
}

TEST(PendingState, SetExceptionPendingSVCall)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_SVCALL);
    CHECK_TRUE(nvic.shcsr & (1u << 15));
    armv8m_nvic_clear_exception_pending(&nvic, ARMV8M_EXC_SVCALL);
    CHECK_FALSE(nvic.shcsr & (1u << 15));
}

TEST(PendingState, SetExceptionPendingPendSV)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_PENDSV);
    CHECK_TRUE(nvic.shcsr & (1u << 28));
    armv8m_nvic_clear_exception_pending(&nvic, ARMV8M_EXC_PENDSV);
    CHECK_FALSE(nvic.shcsr & (1u << 28));
}

TEST(PendingState, SetExceptionPendingSysTick)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_SYSTICK);
    CHECK_TRUE(nvic.shcsr & (1u << 29));
    armv8m_nvic_clear_exception_pending(&nvic, ARMV8M_EXC_SYSTICK);
    CHECK_FALSE(nvic.shcsr & (1u << 29));
}

TEST(PendingState, ClearExceptionPendingInvalid)
{
    nvic.shcsr = 0xFFFFFFFF;
    armv8m_nvic_clear_exception_pending(&nvic, 0);
    armv8m_nvic_clear_exception_pending(&nvic, 100);
    CHECK_EQUAL(0xFFFFFFFFu, nvic.shcsr);
}

/*============================================================================
 * Test Group: Priority
 *============================================================================*/

TEST_GROUP(Priority)
{
    NVIC nvic;

    void setup()
    {
        armv8m_nvic_init(&nvic, 32);
    }

    void teardown() {}
};

TEST(Priority, SetAndGetPriority)
{
    armv8m_nvic_set_priority(&nvic, 0, 0x80);
    CHECK_EQUAL(0x80u, armv8m_nvic_get_priority(&nvic, 0));

    armv8m_nvic_set_priority(&nvic, 15, 0x40);
    CHECK_EQUAL(0x40u, armv8m_nvic_get_priority(&nvic, 15));
}

TEST(Priority, PriorityMasking)
{
    armv8m_nvic_set_priority(&nvic, 0, 0xFF);
    uint8_t pri = armv8m_nvic_get_priority(&nvic, 0);
    CHECK_EQUAL(0xE0u, pri);
}

TEST(Priority, InvalidIRQPriority)
{
    armv8m_nvic_set_priority(&nvic, -1, 0x80);
    armv8m_nvic_set_priority(&nvic, 100, 0x80);
    CHECK_EQUAL(0u, armv8m_nvic_get_priority(&nvic, -1));
    CHECK_EQUAL(0u, armv8m_nvic_get_priority(&nvic, 100));
}

TEST(Priority, SetExceptionPriority)
{
    armv8m_nvic_set_exception_priority(&nvic, ARMV8M_EXC_MEMMANAGE, 0x80);
    CHECK_EQUAL(0x80u, nvic.shpr[0]);

    armv8m_nvic_set_exception_priority(&nvic, ARMV8M_EXC_SVCALL, 0x40);
    CHECK_EQUAL(0x40u, nvic.shpr[7]);

    armv8m_nvic_set_exception_priority(&nvic, ARMV8M_EXC_SYSTICK, 0x60);
    CHECK_EQUAL(0x60u, nvic.shpr[11]);
}

TEST(Priority, SetExceptionPriorityInvalid)
{
    armv8m_nvic_set_exception_priority(&nvic, 0, 0x80);
    armv8m_nvic_set_exception_priority(&nvic, 3, 0x80);
    armv8m_nvic_set_exception_priority(&nvic, 16, 0x80);
    CHECK_EQUAL(0u, nvic.shpr[0]);
}

/*============================================================================
 * Test Group: Exception Priority Selection
 *============================================================================*/

TEST_GROUP(ExceptionSelection)
{
    NVIC nvic;

    void setup()
    {
        armv8m_nvic_init(&nvic, 32);
    }

    void teardown() {}
};

TEST(ExceptionSelection, HigherPriorityWins)
{
    armv8m_nvic_enable_irq(&nvic, 0);
    armv8m_nvic_enable_irq(&nvic, 1);

    armv8m_nvic_set_priority(&nvic, 0, 0x80);
    armv8m_nvic_set_priority(&nvic, 1, 0x40);

    armv8m_nvic_set_pending(&nvic, 0);
    armv8m_nvic_set_pending(&nvic, 1);

    int pending = armv8m_nvic_get_pending_exception(&nvic, 0, 0, 0, 256);
    CHECK_EQUAL(17, pending);
}

TEST(ExceptionSelection, BasepriMasking)
{
    armv8m_nvic_enable_irq(&nvic, 0);
    armv8m_nvic_set_priority(&nvic, 0, 0x80);
    armv8m_nvic_set_pending(&nvic, 0);

    int pending = armv8m_nvic_get_pending_exception(&nvic, 0x40, 0, 0, 256);
    CHECK_EQUAL(-1, pending);

    pending = armv8m_nvic_get_pending_exception(&nvic, 0xC0, 0, 0, 256);
    CHECK_EQUAL(16, pending);
}

TEST(ExceptionSelection, PrimaskBlocksAll)
{
    armv8m_nvic_enable_irq(&nvic, 0);
    armv8m_nvic_set_priority(&nvic, 0, 0x00);
    armv8m_nvic_set_pending(&nvic, 0);

    int pending = armv8m_nvic_get_pending_exception(&nvic, 0, 1, 0, 256);
    CHECK_EQUAL(-1, pending);
}

TEST(ExceptionSelection, FaultmaskBlocksAll)
{
    armv8m_nvic_enable_irq(&nvic, 0);
    armv8m_nvic_set_priority(&nvic, 0, 0x00);
    armv8m_nvic_set_pending(&nvic, 0);

    int pending = armv8m_nvic_get_pending_exception(&nvic, 0, 0, 1, 256);
    CHECK_EQUAL(-1, pending);
}

TEST(ExceptionSelection, FaultmaskAllowsNMI)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_NMI);
    int pending = armv8m_nvic_get_pending_exception(&nvic, 0, 0, 1, 256);
    CHECK_EQUAL(ARMV8M_EXC_NMI, pending);
}

TEST(ExceptionSelection, PrimaskAllowsNMI)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_NMI);
    int pending = armv8m_nvic_get_pending_exception(&nvic, 0, 1, 0, 256);
    CHECK_EQUAL(ARMV8M_EXC_NMI, pending);
}

TEST(ExceptionSelection, CurrentPriorityBlocks)
{
    armv8m_nvic_enable_irq(&nvic, 0);
    armv8m_nvic_set_priority(&nvic, 0, 0x80);
    armv8m_nvic_set_pending(&nvic, 0);

    int pending = armv8m_nvic_get_pending_exception(&nvic, 0, 0, 0, 0x40);
    CHECK_EQUAL(-1, pending);

    pending = armv8m_nvic_get_pending_exception(&nvic, 0, 0, 0, 0xC0);
    CHECK_EQUAL(16, pending);
}

TEST(ExceptionSelection, SystemExceptionPending)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_MEMMANAGE);
    armv8m_nvic_set_exception_priority(&nvic, ARMV8M_EXC_MEMMANAGE, 0x20);
    int pending = armv8m_nvic_get_pending_exception(&nvic, 0, 0, 0, 256);
    CHECK_EQUAL(ARMV8M_EXC_MEMMANAGE, pending);
}

TEST(ExceptionSelection, BusFaultPending)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_BUSFAULT);
    armv8m_nvic_set_exception_priority(&nvic, ARMV8M_EXC_BUSFAULT, 0x20);
    int pending = armv8m_nvic_get_pending_exception(&nvic, 0, 0, 0, 256);
    CHECK_EQUAL(ARMV8M_EXC_BUSFAULT, pending);
}

TEST(ExceptionSelection, UsageFaultPending)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_USAGEFAULT);
    armv8m_nvic_set_exception_priority(&nvic, ARMV8M_EXC_USAGEFAULT, 0x20);
    int pending = armv8m_nvic_get_pending_exception(&nvic, 0, 0, 0, 256);
    CHECK_EQUAL(ARMV8M_EXC_USAGEFAULT, pending);
}

TEST(ExceptionSelection, SVCallPending)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_SVCALL);
    armv8m_nvic_set_exception_priority(&nvic, ARMV8M_EXC_SVCALL, 0x20);
    int pending = armv8m_nvic_get_pending_exception(&nvic, 0, 0, 0, 256);
    CHECK_EQUAL(ARMV8M_EXC_SVCALL, pending);
}

TEST(ExceptionSelection, PendSVPending)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_PENDSV);
    armv8m_nvic_set_exception_priority(&nvic, ARMV8M_EXC_PENDSV, 0x20);
    int pending = armv8m_nvic_get_pending_exception(&nvic, 0, 0, 0, 256);
    CHECK_EQUAL(ARMV8M_EXC_PENDSV, pending);
}

TEST(ExceptionSelection, SysTickPending)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_SYSTICK);
    armv8m_nvic_set_exception_priority(&nvic, ARMV8M_EXC_SYSTICK, 0x20);
    int pending = armv8m_nvic_get_pending_exception(&nvic, 0, 0, 0, 256);
    CHECK_EQUAL(ARMV8M_EXC_SYSTICK, pending);
}

TEST(ExceptionSelection, DisabledIRQNotPending)
{
    armv8m_nvic_set_priority(&nvic, 0, 0x00);
    armv8m_nvic_set_pending(&nvic, 0);

    int pending = armv8m_nvic_get_pending_exception(&nvic, 0, 0, 0, 256);
    CHECK_EQUAL(-1, pending);
}

TEST(ExceptionSelection, BasepriBlocksSystemException)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_SVCALL);
    armv8m_nvic_set_exception_priority(&nvic, ARMV8M_EXC_SVCALL, 0x80);
    int pending = armv8m_nvic_get_pending_exception(&nvic, 0x40, 0, 0, 256);
    CHECK_EQUAL(-1, pending);
}

TEST(ExceptionSelection, CurrentPriorityBlocksSystemException)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_SVCALL);
    armv8m_nvic_set_exception_priority(&nvic, ARMV8M_EXC_SVCALL, 0x80);
    int pending = armv8m_nvic_get_pending_exception(&nvic, 0, 0, 0, 0x40);
    CHECK_EQUAL(-1, pending);
}

TEST(ExceptionSelection, EnabledButNotPendingIRQ)
{
    armv8m_nvic_enable_irq(&nvic, 0);
    armv8m_nvic_enable_irq(&nvic, 1);
    armv8m_nvic_set_priority(&nvic, 0, 0x40);
    armv8m_nvic_set_priority(&nvic, 1, 0x80);
    armv8m_nvic_set_pending(&nvic, 1);

    int pending = armv8m_nvic_get_pending_exception(&nvic, 0, 0, 0, 256);
    CHECK_EQUAL(17, pending);
}

TEST(ExceptionSelection, NoPendingException)
{
    int pending = armv8m_nvic_get_pending_exception(&nvic, 0, 0, 0, 256);
    CHECK_EQUAL(-1, pending);
}

/*============================================================================
 * Test Group: Acknowledge and Deactivate
 *============================================================================*/

TEST_GROUP(AcknowledgeDeactivate)
{
    NVIC nvic;

    void setup()
    {
        armv8m_nvic_init(&nvic, 32);
    }

    void teardown() {}
};

TEST(AcknowledgeDeactivate, AcknowledgeIRQ)
{
    armv8m_nvic_enable_irq(&nvic, 5);
    armv8m_nvic_set_pending(&nvic, 5);

    armv8m_nvic_acknowledge(&nvic, 16 + 5);
    CHECK_TRUE(nvic.active[0] & (1u << 5));
    CHECK_FALSE(nvic.pending[0] & (1u << 5));
}

TEST(AcknowledgeDeactivate, DeactivateIRQ)
{
    nvic.active[0] = (1u << 5);
    armv8m_nvic_deactivate(&nvic, 16 + 5);
    CHECK_FALSE(nvic.active[0] & (1u << 5));
}

TEST(AcknowledgeDeactivate, AcknowledgeMemFault)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_MEMMANAGE);
    armv8m_nvic_acknowledge(&nvic, ARMV8M_EXC_MEMMANAGE);
    CHECK_TRUE(nvic.shcsr & (1u << 0));
    CHECK_FALSE(nvic.shcsr & (1u << 13));
}

TEST(AcknowledgeDeactivate, DeactivateMemFault)
{
    nvic.shcsr = (1u << 0);
    armv8m_nvic_deactivate(&nvic, ARMV8M_EXC_MEMMANAGE);
    CHECK_FALSE(nvic.shcsr & (1u << 0));
}

TEST(AcknowledgeDeactivate, AcknowledgeBusFault)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_BUSFAULT);
    armv8m_nvic_acknowledge(&nvic, ARMV8M_EXC_BUSFAULT);
    CHECK_TRUE(nvic.shcsr & (1u << 1));
    CHECK_FALSE(nvic.shcsr & (1u << 14));
}

TEST(AcknowledgeDeactivate, DeactivateBusFault)
{
    nvic.shcsr = (1u << 1);
    armv8m_nvic_deactivate(&nvic, ARMV8M_EXC_BUSFAULT);
    CHECK_FALSE(nvic.shcsr & (1u << 1));
}

TEST(AcknowledgeDeactivate, AcknowledgeUsageFault)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_USAGEFAULT);
    armv8m_nvic_acknowledge(&nvic, ARMV8M_EXC_USAGEFAULT);
    CHECK_TRUE(nvic.shcsr & (1u << 3));
    CHECK_FALSE(nvic.shcsr & (1u << 12));
}

TEST(AcknowledgeDeactivate, DeactivateUsageFault)
{
    nvic.shcsr = (1u << 3);
    armv8m_nvic_deactivate(&nvic, ARMV8M_EXC_USAGEFAULT);
    CHECK_FALSE(nvic.shcsr & (1u << 3));
}

TEST(AcknowledgeDeactivate, AcknowledgeSVCall)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_SVCALL);
    armv8m_nvic_acknowledge(&nvic, ARMV8M_EXC_SVCALL);
    CHECK_TRUE(nvic.shcsr & (1u << 7));
    CHECK_FALSE(nvic.shcsr & (1u << 15));
}

TEST(AcknowledgeDeactivate, DeactivateSVCall)
{
    nvic.shcsr = (1u << 7);
    armv8m_nvic_deactivate(&nvic, ARMV8M_EXC_SVCALL);
    CHECK_FALSE(nvic.shcsr & (1u << 7));
}

TEST(AcknowledgeDeactivate, AcknowledgeDebugMon)
{
    armv8m_nvic_acknowledge(&nvic, ARMV8M_EXC_DEBUGMON);
    CHECK_TRUE(nvic.shcsr & (1u << 8));
}

TEST(AcknowledgeDeactivate, DeactivateDebugMon)
{
    nvic.shcsr = (1u << 8);
    armv8m_nvic_deactivate(&nvic, ARMV8M_EXC_DEBUGMON);
    CHECK_FALSE(nvic.shcsr & (1u << 8));
}

TEST(AcknowledgeDeactivate, AcknowledgePendSV)
{
    armv8m_nvic_acknowledge(&nvic, ARMV8M_EXC_PENDSV);
    CHECK_TRUE(nvic.shcsr & (1u << 10));
}

TEST(AcknowledgeDeactivate, DeactivatePendSV)
{
    nvic.shcsr = (1u << 10);
    armv8m_nvic_deactivate(&nvic, ARMV8M_EXC_PENDSV);
    CHECK_FALSE(nvic.shcsr & (1u << 10));
}

TEST(AcknowledgeDeactivate, AcknowledgeSysTick)
{
    armv8m_nvic_acknowledge(&nvic, ARMV8M_EXC_SYSTICK);
    CHECK_TRUE(nvic.shcsr & (1u << 11));
}

TEST(AcknowledgeDeactivate, DeactivateSysTick)
{
    nvic.shcsr = (1u << 11);
    armv8m_nvic_deactivate(&nvic, ARMV8M_EXC_SYSTICK);
    CHECK_FALSE(nvic.shcsr & (1u << 11));
}

TEST(AcknowledgeDeactivate, AcknowledgeInvalidException)
{
    armv8m_nvic_acknowledge(&nvic, 3);
    CHECK_EQUAL(0u, nvic.shcsr);
}

TEST(AcknowledgeDeactivate, DeactivateInvalidException)
{
    nvic.shcsr = 0xFFFFFFFF;
    armv8m_nvic_deactivate(&nvic, 3);
    CHECK_EQUAL(0xFFFFFFFFu, nvic.shcsr);
}

/*============================================================================
 * Test Group: NVIC Register Access
 *============================================================================*/

TEST_GROUP(RegisterAccess)
{
    NVIC nvic;

    void setup()
    {
        armv8m_nvic_init(&nvic, 64);
    }

    void teardown() {}
};

TEST(RegisterAccess, WriteISER)
{
    armv8m_nvic_write(&nvic, NVIC_ISER_BASE, 0x0000000F, 4);
    CHECK_EQUAL(0x0Fu, nvic.enabled[0] & 0xF);
}

TEST(RegisterAccess, WriteISER1)
{
    armv8m_nvic_write(&nvic, NVIC_ISER_BASE + 4, 0x0000000F, 4);
    CHECK_EQUAL(0x0Fu, nvic.enabled[1] & 0xF);
}

TEST(RegisterAccess, ReadISER)
{
    nvic.enabled[0] = 0x12345678;
    uint32_t value = armv8m_nvic_read(&nvic, NVIC_ISER_BASE, 4);
    CHECK_EQUAL(0x12345678u, value);
}

TEST(RegisterAccess, WriteICER)
{
    nvic.enabled[0] = 0xFFFFFFFF;
    armv8m_nvic_write(&nvic, NVIC_ICER_BASE, 0x0000000F, 4);
    CHECK_EQUAL(0xFFFFFFF0u, nvic.enabled[0]);
}

TEST(RegisterAccess, ReadICER)
{
    nvic.enabled[0] = 0xABCDEF01;
    uint32_t value = armv8m_nvic_read(&nvic, NVIC_ICER_BASE, 4);
    CHECK_EQUAL(0xABCDEF01u, value);
}

TEST(RegisterAccess, WriteISPR)
{
    armv8m_nvic_write(&nvic, NVIC_ISPR_BASE, 0x000000FF, 4);
    CHECK_EQUAL(0xFFu, nvic.pending[0] & 0xFF);
}

TEST(RegisterAccess, ReadISPR)
{
    nvic.pending[0] = 0x11223344;
    uint32_t value = armv8m_nvic_read(&nvic, NVIC_ISPR_BASE, 4);
    CHECK_EQUAL(0x11223344u, value);
}

TEST(RegisterAccess, WriteICPR)
{
    nvic.pending[0] = 0xFFFFFFFF;
    armv8m_nvic_write(&nvic, NVIC_ICPR_BASE, 0x0000FF00, 4);
    CHECK_EQUAL(0xFFFF00FFu, nvic.pending[0]);
}

TEST(RegisterAccess, ReadICPR)
{
    nvic.pending[0] = 0x55AA55AA;
    uint32_t value = armv8m_nvic_read(&nvic, NVIC_ICPR_BASE, 4);
    CHECK_EQUAL(0x55AA55AAu, value);
}

TEST(RegisterAccess, ReadIABR)
{
    nvic.active[0] = 0xDEADBEEF;
    uint32_t value = armv8m_nvic_read(&nvic, NVIC_IABR_BASE, 4);
    CHECK_EQUAL(0xDEADBEEFu, value);
}

TEST(RegisterAccess, WriteIPRWord)
{
    armv8m_nvic_write(&nvic, NVIC_IPR_BASE, 0x80604020, 4);
    CHECK_EQUAL(0x20u, nvic.priority[0]);
    CHECK_EQUAL(0x40u, nvic.priority[1]);
    CHECK_EQUAL(0x60u, nvic.priority[2]);
    CHECK_EQUAL(0x80u, nvic.priority[3]);
}

TEST(RegisterAccess, WriteIPRByte)
{
    armv8m_nvic_write(&nvic, NVIC_IPR_BASE + 5, 0xA0, 1);
    CHECK_EQUAL(0xA0u, nvic.priority[5]);
}

TEST(RegisterAccess, ReadIPRWord)
{
    nvic.priority[0] = 0x20;
    nvic.priority[1] = 0x40;
    nvic.priority[2] = 0x60;
    nvic.priority[3] = 0x80;
    uint32_t value = armv8m_nvic_read(&nvic, NVIC_IPR_BASE, 4);
    CHECK_EQUAL(0x80604020u, value);
}

TEST(RegisterAccess, ReadIPRByte)
{
    nvic.priority[7] = 0xC0;
    uint32_t value = armv8m_nvic_read(&nvic, NVIC_IPR_BASE + 7, 1);
    CHECK_EQUAL(0xC0u, value);
}

TEST(RegisterAccess, ReadUnmappedRegion)
{
    uint32_t value = armv8m_nvic_read(&nvic, 0x40, 4);
    CHECK_EQUAL(0u, value);
}

TEST(RegisterAccess, WriteUnmappedRegion)
{
    armv8m_nvic_write(&nvic, 0x40, 0xFFFFFFFF, 4);
    CHECK_EQUAL(0u, nvic.enabled[0]);
}

TEST(RegisterAccess, ReadISERBeyondRange)
{
    uint32_t value = armv8m_nvic_read(&nvic, NVIC_ISER_BASE + 0x1C, 4);
    CHECK_EQUAL(0u, value);
}

/*============================================================================
 * Test Group: SCB Register Access
 *============================================================================*/

TEST_GROUP(SCBAccess)
{
    NVIC nvic;

    void setup()
    {
        armv8m_nvic_init(&nvic, 32);
    }

    void teardown() {}
};

TEST(SCBAccess, ReadVTOR)
{
    nvic.vtor = 0x20000000;
    uint32_t value = armv8m_scb_read(&nvic, SCB_VTOR, 4);
    CHECK_EQUAL(0x20000000u, value);
}

TEST(SCBAccess, WriteVTOR)
{
    armv8m_scb_write(&nvic, SCB_VTOR, 0x10000100, 4);
    CHECK_EQUAL(0x10000100u, nvic.vtor);
}

TEST(SCBAccess, WriteVTORAlignment)
{
    armv8m_scb_write(&nvic, SCB_VTOR, 0x1000007F, 4);
    CHECK_EQUAL(0x10000000u, nvic.vtor);
}

TEST(SCBAccess, ReadAIRCR)
{
    nvic.prigroup = 5;
    uint32_t value = armv8m_scb_read(&nvic, SCB_AIRCR, 4);
    CHECK_EQUAL(0xFA050500u, value);
}

TEST(SCBAccess, WriteAIRCRWithKey)
{
    armv8m_scb_write(&nvic, SCB_AIRCR, 0x05FA0300, 4);
    CHECK_EQUAL(3, nvic.prigroup);
}

TEST(SCBAccess, WriteAIRCRBadKey)
{
    nvic.prigroup = 2;
    armv8m_scb_write(&nvic, SCB_AIRCR, 0x12340500, 4);
    CHECK_EQUAL(2, nvic.prigroup);
}

TEST(SCBAccess, ReadSCR)
{
    nvic.scr = 0x14;
    uint32_t value = armv8m_scb_read(&nvic, SCB_SCR, 4);
    CHECK_EQUAL(0x14u, value);
}

TEST(SCBAccess, WriteSCR)
{
    armv8m_scb_write(&nvic, SCB_SCR, 0xFFFFFFFF, 4);
    CHECK_EQUAL(0x1Fu, nvic.scr);
}

TEST(SCBAccess, ReadCCR)
{
    nvic.ccr = 0x200;
    uint32_t value = armv8m_scb_read(&nvic, SCB_CCR, 4);
    CHECK_EQUAL(0x200u, value);
}

TEST(SCBAccess, WriteCCR)
{
    armv8m_scb_write(&nvic, SCB_CCR, 0x12345678, 4);
    CHECK_EQUAL(0x12345678u, nvic.ccr);
}

TEST(SCBAccess, ReadSHPR1)
{
    nvic.shpr[0] = 0x20;
    nvic.shpr[1] = 0x40;
    nvic.shpr[2] = 0x60;
    nvic.shpr[3] = 0x80;
    uint32_t value = armv8m_scb_read(&nvic, SCB_SHPR1, 4);
    CHECK_EQUAL(0x80604020u, value);
}

TEST(SCBAccess, WriteSHPR1)
{
    armv8m_scb_write(&nvic, SCB_SHPR1, 0xE0C0A080, 4);
    CHECK_EQUAL(0x80u, nvic.shpr[0]);
    CHECK_EQUAL(0xA0u, nvic.shpr[1]);
    CHECK_EQUAL(0xC0u, nvic.shpr[2]);
    CHECK_EQUAL(0xE0u, nvic.shpr[3]);
}

TEST(SCBAccess, ReadSHPR2)
{
    nvic.shpr[4] = 0x11;
    nvic.shpr[5] = 0x22;
    nvic.shpr[6] = 0x33;
    nvic.shpr[7] = 0x44;
    uint32_t value = armv8m_scb_read(&nvic, SCB_SHPR2, 4);
    CHECK_EQUAL(0x44332211u, value);
}

TEST(SCBAccess, WriteSHPR2)
{
    armv8m_scb_write(&nvic, SCB_SHPR2, 0x80604020, 4);
    CHECK_EQUAL(0x20u, nvic.shpr[4]);
    CHECK_EQUAL(0x40u, nvic.shpr[5]);
    CHECK_EQUAL(0x60u, nvic.shpr[6]);
    CHECK_EQUAL(0x80u, nvic.shpr[7]);
}

TEST(SCBAccess, ReadSHPR3)
{
    nvic.shpr[8] = 0xAA;
    nvic.shpr[9] = 0xBB;
    nvic.shpr[10] = 0xCC;
    nvic.shpr[11] = 0xDD;
    uint32_t value = armv8m_scb_read(&nvic, SCB_SHPR3, 4);
    CHECK_EQUAL(0xDDCCBBAAu, value);
}

TEST(SCBAccess, WriteSHPR3)
{
    armv8m_scb_write(&nvic, SCB_SHPR3, 0xE0C0A080, 4);
    CHECK_EQUAL(0x80u, nvic.shpr[8]);
    CHECK_EQUAL(0xA0u, nvic.shpr[9]);
    CHECK_EQUAL(0xC0u, nvic.shpr[10]);
    CHECK_EQUAL(0xE0u, nvic.shpr[11]);
}

TEST(SCBAccess, ReadSHCSR)
{
    nvic.shcsr = 0x00070003;
    uint32_t value = armv8m_scb_read(&nvic, SCB_SHCSR, 4);
    CHECK_EQUAL(0x00070003u, value);
}

TEST(SCBAccess, WriteSHCSR)
{
    nvic.shcsr = 0;
    armv8m_scb_write(&nvic, SCB_SHCSR, 0xFFFFFFFF, 4);
    CHECK_EQUAL(0x00070000u, nvic.shcsr);
}

TEST(SCBAccess, ReadICSR)
{
    armv8m_nvic_enable_irq(&nvic, 0);
    armv8m_nvic_set_priority(&nvic, 0, 0x20);
    armv8m_nvic_set_pending(&nvic, 0);
    armv8m_nvic_get_pending_exception(&nvic, 0, 0, 0, 256);

    uint32_t value = armv8m_scb_read(&nvic, SCB_ICSR, 4);
    CHECK_TRUE(value & (1u << 22));
    CHECK_EQUAL(16u, (value >> 12) & 0x1FF);
}

TEST(SCBAccess, WriteICSRPendSVSet)
{
    armv8m_scb_write(&nvic, SCB_ICSR, (1u << 28), 4);
    CHECK_TRUE(nvic.shcsr & (1u << 28));
}

TEST(SCBAccess, WriteICSRPendSVClear)
{
    nvic.shcsr = (1u << 28);
    armv8m_scb_write(&nvic, SCB_ICSR, (1u << 27), 4);
    CHECK_FALSE(nvic.shcsr & (1u << 28));
}

TEST(SCBAccess, WriteICSRSysTickSet)
{
    armv8m_scb_write(&nvic, SCB_ICSR, (1u << 26), 4);
    CHECK_TRUE(nvic.shcsr & (1u << 29));
}

TEST(SCBAccess, WriteICSRSysTickClear)
{
    nvic.shcsr = (1u << 29);
    armv8m_scb_write(&nvic, SCB_ICSR, (1u << 25), 4);
    CHECK_FALSE(nvic.shcsr & (1u << 29));
}

TEST(SCBAccess, WriteICSRNMISet)
{
    armv8m_scb_write(&nvic, SCB_ICSR, (1u << 31), 4);
    CHECK_TRUE(nvic.shcsr & (1u << 31));
}

TEST(SCBAccess, ReadUnmappedSCB)
{
    uint32_t value = armv8m_scb_read(&nvic, 0x100, 4);
    CHECK_EQUAL(0u, value);
}

TEST(SCBAccess, WriteUnmappedSCB)
{
    armv8m_scb_write(&nvic, 0x100, 0xFFFFFFFF, 4);
}

/*============================================================================
 * Test Group: Edge Cases for Full Branch Coverage
 *============================================================================*/

TEST_GROUP(EdgeCases)
{
    NVIC nvic;

    void setup()
    {
        armv8m_nvic_init(&nvic, 32);
    }

    void teardown() {}
};

TEST(EdgeCases, ReadIPRBeyondNumIRQs)
{
    uint32_t value = armv8m_nvic_read(&nvic, NVIC_IPR_BASE + 100, 4);
    CHECK_EQUAL(0u, value);
}

TEST(EdgeCases, WriteIPRBeyondNumIRQs)
{
    armv8m_nvic_write(&nvic, NVIC_IPR_BASE + 100, 0xFFFFFFFF, 4);
    CHECK_EQUAL(0u, nvic.priority[31]);
}

TEST(EdgeCases, ReadIPRByteBeyondNumIRQs)
{
    uint32_t value = armv8m_nvic_read(&nvic, NVIC_IPR_BASE + 50, 1);
    CHECK_EQUAL(0u, value);
}

TEST(EdgeCases, WriteIPRByteBeyondNumIRQs)
{
    armv8m_nvic_write(&nvic, NVIC_IPR_BASE + 50, 0xFF, 1);
}

TEST(EdgeCases, AcknowledgeInvalidIRQ)
{
    armv8m_nvic_acknowledge(&nvic, 16 + 100);
    CHECK_EQUAL(0u, nvic.active[0]);
}

TEST(EdgeCases, DeactivateInvalidIRQ)
{
    armv8m_nvic_deactivate(&nvic, 16 + 100);
}

TEST(EdgeCases, AcknowledgeSecureFault)
{
    armv8m_nvic_acknowledge(&nvic, ARMV8M_EXC_SECUREFAULT);
    CHECK_EQUAL(0u, nvic.shcsr);
}

TEST(EdgeCases, DeactivateSecureFault)
{
    armv8m_nvic_deactivate(&nvic, ARMV8M_EXC_SECUREFAULT);
}

TEST(EdgeCases, SetExceptionPendingHardFault)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_HARDFAULT);
}

TEST(EdgeCases, ClearExceptionPendingHardFault)
{
    armv8m_nvic_clear_exception_pending(&nvic, ARMV8M_EXC_HARDFAULT);
}

TEST(EdgeCases, ReadICERAtOffset4)
{
    nvic.enabled[1] = 0xDEADBEEF;
    uint32_t value = armv8m_nvic_read(&nvic, NVIC_ICER_BASE + 4, 4);
    CHECK_EQUAL(0xDEADBEEFu, value);
}

TEST(EdgeCases, WriteICERAtOffset4)
{
    nvic.enabled[1] = 0xFFFFFFFF;
    armv8m_nvic_write(&nvic, NVIC_ICER_BASE + 4, 0x0F0F0F0F, 4);
    CHECK_EQUAL(0xF0F0F0F0u, nvic.enabled[1]);
}

TEST(EdgeCases, ReadISPRAtOffset4)
{
    nvic.pending[1] = 0xCAFEBABE;
    uint32_t value = armv8m_nvic_read(&nvic, NVIC_ISPR_BASE + 4, 4);
    CHECK_EQUAL(0xCAFEBABEu, value);
}

TEST(EdgeCases, ReadICPRAtOffset4)
{
    nvic.pending[1] = 0x12345678;
    uint32_t value = armv8m_nvic_read(&nvic, NVIC_ICPR_BASE + 4, 4);
    CHECK_EQUAL(0x12345678u, value);
}

TEST(EdgeCases, WriteICPRAtOffset4)
{
    nvic.pending[1] = 0xFFFFFFFF;
    armv8m_nvic_write(&nvic, NVIC_ICPR_BASE + 4, 0x00FF00FF, 4);
    CHECK_EQUAL(0xFF00FF00u, nvic.pending[1]);
}

TEST(EdgeCases, ReadIABRAtOffset4)
{
    nvic.active[1] = 0xABCDEF12;
    uint32_t value = armv8m_nvic_read(&nvic, NVIC_IABR_BASE + 4, 4);
    CHECK_EQUAL(0xABCDEF12u, value);
}

TEST(EdgeCases, MultipleIRQsWithSamePriority)
{
    armv8m_nvic_enable_irq(&nvic, 5);
    armv8m_nvic_enable_irq(&nvic, 10);
    armv8m_nvic_set_priority(&nvic, 5, 0x40);
    armv8m_nvic_set_priority(&nvic, 10, 0x40);
    armv8m_nvic_set_pending(&nvic, 5);
    armv8m_nvic_set_pending(&nvic, 10);

    int pending = armv8m_nvic_get_pending_exception(&nvic, 0, 0, 0, 256);
    CHECK_EQUAL(16 + 5, pending);
}

TEST(EdgeCases, ICSRNoPending)
{
    uint32_t value = armv8m_scb_read(&nvic, SCB_ICSR, 4);
    CHECK_FALSE(value & (1u << 22));
}

TEST(EdgeCases, ExceptionPriorityMasking)
{
    armv8m_nvic_set_exception_priority(&nvic, ARMV8M_EXC_PENDSV, 0xFF);
    CHECK_EQUAL(0xE0u, nvic.shpr[10]);
}

TEST(EdgeCases, ReadISERMultipleRegisters)
{
    nvic.enabled[2] = 0x11111111;
    uint32_t value = armv8m_nvic_read(&nvic, NVIC_ISER_BASE + 8, 4);
    CHECK_EQUAL(0x11111111u, value);
}

TEST(EdgeCases, WriteISPRAtOffset4)
{
    armv8m_nvic_write(&nvic, NVIC_ISPR_BASE + 4, 0xAAAAAAAA, 4);
    CHECK_EQUAL(0xAAAAAAAAu, nvic.pending[1]);
}

/*============================================================================
 * Test Group: ARM Architecture Compliance Tests
 *============================================================================*/

TEST_GROUP(ARMCompliance)
{
    NVIC nvic;

    void setup()
    {
        armv8m_nvic_init(&nvic, 32);
    }

    void teardown() {}
};

/* Issue 1: SHCSR Read Should Mask Internal Bits */
TEST(ARMCompliance, SHCSRReadMasksInternalBits)
{
    /* Set internal tracking bits that shouldn't be visible */
    nvic.shcsr = 0xFFFFFFFF;

    uint32_t value = armv8m_scb_read(&nvic, SCB_SHCSR, 4);

    /* Bits 27-31 (internal tracking: HardFault pending, PendSV, SysTick, DebugMon, NMI) should be masked out */
    CHECK_EQUAL(0x07FFFFFFu, value);
}

TEST(ARMCompliance, SHCSRReadPreservesVisibleBits)
{
    /* Set some visible SHCSR bits */
    nvic.shcsr = (1u << 0) | (1u << 7) | (1u << 16);  /* MEMFAULTACT, SVCALLACT, MEMFAULTENA */

    uint32_t value = armv8m_scb_read(&nvic, SCB_SHCSR, 4);

    CHECK_EQUAL((1u << 0) | (1u << 7) | (1u << 16), value);
}

/* Issue 2: PendSV/SysTick Pending Cleared on Acknowledge */
TEST(ARMCompliance, AcknowledgeClearsPendSVPending)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_PENDSV);
    CHECK_TRUE(nvic.shcsr & (1u << 28));

    armv8m_nvic_acknowledge(&nvic, ARMV8M_EXC_PENDSV);

    /* Pending should be cleared, active should be set */
    CHECK_FALSE(nvic.shcsr & (1u << 28));
    CHECK_TRUE(nvic.shcsr & (1u << 10));  /* PENDSVACT */
}

TEST(ARMCompliance, AcknowledgeClearsSysTickPending)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_SYSTICK);
    CHECK_TRUE(nvic.shcsr & (1u << 29));

    armv8m_nvic_acknowledge(&nvic, ARMV8M_EXC_SYSTICK);

    /* Pending should be cleared, active should be set */
    CHECK_FALSE(nvic.shcsr & (1u << 29));
    CHECK_TRUE(nvic.shcsr & (1u << 11));  /* SYSTICKACT */
}

/* Issue 4: DebugMonitor Pending State */
TEST(ARMCompliance, DebugMonitorPendingState)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_DEBUGMON);
    CHECK_TRUE(nvic.shcsr & (1u << 30));

    armv8m_nvic_clear_exception_pending(&nvic, ARMV8M_EXC_DEBUGMON);
    CHECK_FALSE(nvic.shcsr & (1u << 30));
}

TEST(ARMCompliance, DebugMonitorPendingSelection)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_DEBUGMON);
    armv8m_nvic_set_exception_priority(&nvic, ARMV8M_EXC_DEBUGMON, 0x20);

    int pending = armv8m_nvic_get_pending_exception(&nvic, 0, 0, 0, 256);
    CHECK_EQUAL(ARMV8M_EXC_DEBUGMON, pending);
}

TEST(ARMCompliance, AcknowledgeClearsDebugMonitorPending)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_DEBUGMON);
    armv8m_nvic_acknowledge(&nvic, ARMV8M_EXC_DEBUGMON);

    CHECK_FALSE(nvic.shcsr & (1u << 30));
    CHECK_TRUE(nvic.shcsr & (1u << 8));  /* MONITORACT */
}

/* Issue 6: HardFault Handling */
TEST(ARMCompliance, HardFaultPendingState)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_HARDFAULT);
    CHECK_TRUE(nvic.shcsr & (1u << 27));

    armv8m_nvic_clear_exception_pending(&nvic, ARMV8M_EXC_HARDFAULT);
    CHECK_FALSE(nvic.shcsr & (1u << 27));
}

TEST(ARMCompliance, HardFaultPreemptsConfigurable)
{
    /* Set up a configurable priority exception */
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_SVCALL);
    armv8m_nvic_set_exception_priority(&nvic, ARMV8M_EXC_SVCALL, 0x20);

    /* Also set HardFault pending */
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_HARDFAULT);

    /* HardFault has priority -1, should be selected */
    int pending = armv8m_nvic_get_pending_exception(&nvic, 0, 0, 0, 256);
    CHECK_EQUAL(ARMV8M_EXC_HARDFAULT, pending);
}

TEST(ARMCompliance, HardFaultNotBlockedByPRIMASK)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_HARDFAULT);

    /* PRIMASK=1 should NOT block HardFault */
    int pending = armv8m_nvic_get_pending_exception(&nvic, 0, 1, 0, 256);
    CHECK_EQUAL(ARMV8M_EXC_HARDFAULT, pending);
}

TEST(ARMCompliance, HardFaultBlockedByFAULTMASK)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_HARDFAULT);

    /* FAULTMASK=1 should block HardFault */
    int pending = armv8m_nvic_get_pending_exception(&nvic, 0, 0, 1, 256);
    CHECK_EQUAL(-1, pending);
}

TEST(ARMCompliance, AcknowledgeClearsHardFaultPending)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_HARDFAULT);
    armv8m_nvic_acknowledge(&nvic, ARMV8M_EXC_HARDFAULT);

    CHECK_FALSE(nvic.shcsr & (1u << 27));
}

TEST(ARMCompliance, AcknowledgeClearsNMIPending)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_NMI);
    armv8m_nvic_acknowledge(&nvic, ARMV8M_EXC_NMI);

    CHECK_FALSE(nvic.shcsr & (1u << 31));
}

/* Issue 5: ICSR VECTACTIVE */
TEST(ARMCompliance, ICSRVECTACTIVEShowsActiveException)
{
    /* Manually set an exception as active */
    nvic.shcsr |= (1u << 7);  /* SVCALLACT */

    uint32_t value = armv8m_scb_read(&nvic, SCB_ICSR, 4);

    /* VECTACTIVE should show SVCall (exception 11) */
    CHECK_EQUAL(11u, value & 0x1FF);
}

TEST(ARMCompliance, ICSRVECTACTIVEShowsActiveIRQ)
{
    /* Set IRQ 5 as active */
    nvic.active[0] = (1u << 5);

    uint32_t value = armv8m_scb_read(&nvic, SCB_ICSR, 4);

    /* VECTACTIVE should show external interrupt 5 (exception 16+5=21) */
    CHECK_EQUAL(21u, value & 0x1FF);
}

TEST(ARMCompliance, ICSRVECTACTIVEZeroWhenNoActive)
{
    uint32_t value = armv8m_scb_read(&nvic, SCB_ICSR, 4);
    CHECK_EQUAL(0u, value & 0x1FF);
}

TEST(ARMCompliance, ICSRVECTACTIVEPrioritizesSystemExceptions)
{
    /* Set both a system exception and an IRQ as active */
    nvic.shcsr |= (1u << 3);  /* USGFAULTACT */
    nvic.active[0] = (1u << 0);  /* IRQ 0 */

    uint32_t value = armv8m_scb_read(&nvic, SCB_ICSR, 4);

    /* Should show UsageFault (exception 6), not the IRQ */
    CHECK_EQUAL(6u, value & 0x1FF);
}

/* Issue 3: PRIGROUP Priority Grouping */
TEST(ARMCompliance, PRIGROUPAffectsPreemption)
{
    /* Set PRIGROUP=5 (2 bits group, 1 bit subpriority for 3 impl bits) */
    armv8m_scb_write(&nvic, SCB_AIRCR, 0x05FA0500, 4);
    CHECK_EQUAL(5, nvic.prigroup);

    /* Set up two interrupts with same group priority but different subpriority */
    armv8m_nvic_enable_irq(&nvic, 0);
    armv8m_nvic_enable_irq(&nvic, 1);
    armv8m_nvic_set_priority(&nvic, 0, 0x40);  /* Group 01, Sub 0 */
    armv8m_nvic_set_priority(&nvic, 1, 0x60);  /* Group 01, Sub 1 */
    armv8m_nvic_set_pending(&nvic, 0);
    armv8m_nvic_set_pending(&nvic, 1);

    /* IRQ 0 should be selected (lower exception number wins for same group) */
    int pending = armv8m_nvic_get_pending_exception(&nvic, 0, 0, 0, 256);
    CHECK_EQUAL(16, pending);  /* IRQ 0 = exception 16 */
}

TEST(ARMCompliance, PRIGROUPPreemptionBlocked)
{
    /* Set PRIGROUP=5 */
    armv8m_scb_write(&nvic, SCB_AIRCR, 0x05FA0500, 4);

    armv8m_nvic_enable_irq(&nvic, 0);
    armv8m_nvic_set_priority(&nvic, 0, 0x60);  /* Group priority 01 */
    armv8m_nvic_set_pending(&nvic, 0);

    /* Current priority is group 01 (0x40), same group - should NOT preempt */
    int pending = armv8m_nvic_get_pending_exception(&nvic, 0, 0, 0, 0x40);
    CHECK_EQUAL(-1, pending);
}

TEST(ARMCompliance, PRIGROUPPreemptionAllowed)
{
    /* Set PRIGROUP=5 */
    armv8m_scb_write(&nvic, SCB_AIRCR, 0x05FA0500, 4);

    armv8m_nvic_enable_irq(&nvic, 0);
    armv8m_nvic_set_priority(&nvic, 0, 0x40);  /* Group priority 01 */
    armv8m_nvic_set_pending(&nvic, 0);

    /* Current priority is group 10 (0x80), lower - SHOULD preempt */
    int pending = armv8m_nvic_get_pending_exception(&nvic, 0, 0, 0, 0x80);
    CHECK_EQUAL(16, pending);
}

/*============================================================================
 * Test Runner
 *============================================================================*/

int main(int argc, char** argv)
{
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
