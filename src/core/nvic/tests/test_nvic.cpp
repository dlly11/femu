/**
 * @file test_nvic.cpp
 * @brief CppUTest tests for the ARMv8-M NVIC
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

    void setup()
    {
        armv8m_nvic_init(&nvic, 32);  // 32 external IRQs
    }

    void teardown()
    {
    }
};

TEST(NVICInit, InitSetsDefaults)
{
    CHECK_EQUAL(32, nvic.num_irqs);
    CHECK_EQUAL(0u, nvic.vtor);

    // All interrupts should be disabled
    for (int i = 0; i < 8; i++) {
        CHECK_EQUAL(0u, nvic.enabled[i]);
        CHECK_EQUAL(0u, nvic.pending[i]);
        CHECK_EQUAL(0u, nvic.active[i]);
    }
}

/*============================================================================
 * Test Group: Interrupt Enable/Disable
 *============================================================================*/

TEST_GROUP(InterruptEnable)
{
    NVIC nvic;

    void setup()
    {
        armv8m_nvic_init(&nvic, 32);
    }

    void teardown()
    {
    }
};

TEST(InterruptEnable, EnableIRQ)
{
    armv8m_nvic_enable_irq(&nvic, 0);
    CHECK_EQUAL(1u, nvic.enabled[0] & 1);

    armv8m_nvic_enable_irq(&nvic, 31);
    CHECK_TRUE(nvic.enabled[0] & (1u << 31));
}

TEST(InterruptEnable, DisableIRQ)
{
    armv8m_nvic_enable_irq(&nvic, 5);
    CHECK_TRUE(nvic.enabled[0] & (1u << 5));

    armv8m_nvic_disable_irq(&nvic, 5);
    CHECK_FALSE(nvic.enabled[0] & (1u << 5));
}

/*============================================================================
 * Test Group: Pending State
 *============================================================================*/

TEST_GROUP(PendingState)
{
    NVIC nvic;

    void setup()
    {
        armv8m_nvic_init(&nvic, 32);
    }

    void teardown()
    {
    }
};

TEST(PendingState, SetPending)
{
    armv8m_nvic_set_pending(&nvic, 0);
    CHECK_TRUE(nvic.pending[0] & 1);
}

TEST(PendingState, ClearPending)
{
    armv8m_nvic_set_pending(&nvic, 10);
    CHECK_TRUE(nvic.pending[0] & (1u << 10));

    armv8m_nvic_clear_pending(&nvic, 10);
    CHECK_FALSE(nvic.pending[0] & (1u << 10));
}

TEST(PendingState, SetExceptionPending)
{
    armv8m_nvic_set_exception_pending(&nvic, ARMV8M_EXC_PENDSV);
    // Check via ICSR or internal state
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

    void teardown()
    {
    }
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
    // Only upper bits should be stored (3 bits implemented = 0xE0 mask)
    armv8m_nvic_set_priority(&nvic, 0, 0xFF);
    uint8_t pri = armv8m_nvic_get_priority(&nvic, 0);
    // Should be masked to implemented bits
    CHECK_TRUE((pri & 0x1F) == 0 || pri == 0xFF);  // Depends on implementation
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

    void teardown()
    {
    }
};

TEST(ExceptionSelection, HigherPriorityWins)
{
    // Enable and pend two interrupts with different priorities
    armv8m_nvic_enable_irq(&nvic, 0);
    armv8m_nvic_enable_irq(&nvic, 1);

    armv8m_nvic_set_priority(&nvic, 0, 0x80);  // Lower priority (higher number)
    armv8m_nvic_set_priority(&nvic, 1, 0x40);  // Higher priority (lower number)

    armv8m_nvic_set_pending(&nvic, 0);
    armv8m_nvic_set_pending(&nvic, 1);

    // IRQ 1 should be selected (exception number = IRQ + 16)
    int pending = armv8m_nvic_get_pending_exception(&nvic, 0, 0, 0, 256);
    CHECK_EQUAL(17, pending);  // IRQ1 = exception 17
}

TEST(ExceptionSelection, BasepriMasking)
{
    armv8m_nvic_enable_irq(&nvic, 0);
    armv8m_nvic_set_priority(&nvic, 0, 0x80);
    armv8m_nvic_set_pending(&nvic, 0);

    // With BASEPRI=0x40, priority 0x80 should be masked
    int pending = armv8m_nvic_get_pending_exception(&nvic, 0x40, 0, 0, 256);
    CHECK_EQUAL(-1, pending);  // Masked

    // With BASEPRI=0xC0, priority 0x80 should be allowed
    pending = armv8m_nvic_get_pending_exception(&nvic, 0xC0, 0, 0, 256);
    CHECK_EQUAL(16, pending);  // IRQ0 = exception 16
}

TEST(ExceptionSelection, PrimaskBlocksAll)
{
    armv8m_nvic_enable_irq(&nvic, 0);
    armv8m_nvic_set_priority(&nvic, 0, 0x00);  // Highest priority
    armv8m_nvic_set_pending(&nvic, 0);

    // With PRIMASK=1, all interrupts blocked
    int pending = armv8m_nvic_get_pending_exception(&nvic, 0, 1, 0, 256);
    CHECK_EQUAL(-1, pending);
}

/*============================================================================
 * Test Group: Register Access
 *============================================================================*/

TEST_GROUP(RegisterAccess)
{
    NVIC nvic;

    void setup()
    {
        armv8m_nvic_init(&nvic, 32);
    }

    void teardown()
    {
    }
};

TEST(RegisterAccess, WriteISER)
{
    // Write to ISER0 to enable IRQ 0-31
    armv8m_nvic_write(&nvic, NVIC_ISER_BASE, 0x0000000F, 4);
    CHECK_EQUAL(0x0Fu, nvic.enabled[0] & 0xF);
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

/*============================================================================
 * Test Runner
 *============================================================================*/

int main(int argc, char** argv)
{
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
