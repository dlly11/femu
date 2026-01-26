/**
 * @file test_mpu.cpp
 * @brief CppUTest tests for the ARMv8-M MPU
 */

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

extern "C" {
#include "armv8m_mpu.h"
#include "armv8m_types.h"
}

/*============================================================================
 * Test Group: MPU Initialization
 *============================================================================*/

TEST_GROUP(MPUInit)
{
    MPU mpu;

    void setup()
    {
        armv8m_mpu_init(&mpu, 8);  // 8 regions
    }

    void teardown()
    {
    }
};

TEST(MPUInit, InitSetsDefaults)
{
    CHECK_EQUAL(8, mpu.num_regions);
    CHECK_FALSE(mpu.enabled);
    CHECK_EQUAL(0u, mpu.ctrl);
}

/*============================================================================
 * Test Group: MPU Enable/Disable
 *============================================================================*/

TEST_GROUP(MPUControl)
{
    MPU mpu;

    void setup()
    {
        armv8m_mpu_init(&mpu, 8);
    }

    void teardown()
    {
    }
};

TEST(MPUControl, EnableMPU)
{
    armv8m_mpu_enable(&mpu, true, false, false);
    CHECK_TRUE(mpu.enabled);
    CHECK_TRUE(mpu.ctrl & MPU_CTRL_ENABLE);
}

TEST(MPUControl, EnableWithPrivdefena)
{
    armv8m_mpu_enable(&mpu, true, false, true);
    CHECK_TRUE(mpu.privdefena);
    CHECK_TRUE(mpu.ctrl & MPU_CTRL_PRIVDEFENA);
}

TEST(MPUControl, DisableMPU)
{
    armv8m_mpu_enable(&mpu, true, false, false);
    armv8m_mpu_enable(&mpu, false, false, false);
    CHECK_FALSE(mpu.enabled);
}

/*============================================================================
 * Test Group: Region Configuration
 *============================================================================*/

TEST_GROUP(RegionConfig)
{
    MPU mpu;

    void setup()
    {
        armv8m_mpu_init(&mpu, 8);
    }

    void teardown()
    {
    }
};

TEST(RegionConfig, ConfigureRegion)
{
    int result = armv8m_mpu_configure_region(&mpu, 0,
        0x20000000,         // Base
        0x20000FFF,         // Limit (4KB region)
        MPU_AP_RW_ALL,      // RW for all
        false,              // Not XN
        MPU_SH_NONE,        // Non-shareable
        0,                  // Attribute index 0
        true);              // Enable

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_TRUE(mpu.regions[0].rlar & MPU_RLAR_EN);
}

TEST(RegionConfig, InvalidRegionNumber)
{
    int result = armv8m_mpu_configure_region(&mpu, 10,  // > 8 regions
        0x20000000, 0x20000FFF, MPU_AP_RW_ALL, false,
        MPU_SH_NONE, 0, true);

    CHECK_TRUE(result < 0);  // Should fail
}

/*============================================================================
 * Test Group: Access Checking
 *============================================================================*/

TEST_GROUP(AccessCheck)
{
    MPU mpu;
    MPUFaultInfo fault;

    void setup()
    {
        armv8m_mpu_init(&mpu, 8);
        memset(&fault, 0, sizeof(fault));

        // Configure region 0: 0x20000000-0x20000FFF, RW for all
        armv8m_mpu_configure_region(&mpu, 0,
            0x20000000, 0x20000FFF, MPU_AP_RW_ALL, false,
            MPU_SH_NONE, 0, true);

        armv8m_mpu_enable(&mpu, true, false, false);
    }

    void teardown()
    {
    }
};

TEST(AccessCheck, AllowedReadInRegion)
{
    bool allowed = armv8m_mpu_check(&mpu, 0x20000100, 4,
        false, false, true, &fault);
    CHECK_TRUE(allowed);
}

TEST(AccessCheck, AllowedWriteInRegion)
{
    bool allowed = armv8m_mpu_check(&mpu, 0x20000100, 4,
        true, false, true, &fault);
    CHECK_TRUE(allowed);
}

TEST(AccessCheck, DeniedAccessOutsideRegion)
{
    bool allowed = armv8m_mpu_check(&mpu, 0x30000000, 4,
        false, false, true, &fault);
    CHECK_FALSE(allowed);
}

/*============================================================================
 * Test Group: Access Permissions
 *============================================================================*/

TEST_GROUP(Permissions)
{
    MPU mpu;
    MPUFaultInfo fault;

    void setup()
    {
        armv8m_mpu_init(&mpu, 8);
        memset(&fault, 0, sizeof(fault));
        armv8m_mpu_enable(&mpu, true, false, false);
    }

    void teardown()
    {
    }
};

TEST(Permissions, PrivilegedOnlyRegion)
{
    // Region with RW for privileged only
    armv8m_mpu_configure_region(&mpu, 0,
        0x20000000, 0x20000FFF, MPU_AP_RW_PRIV, false,
        MPU_SH_NONE, 0, true);

    // Privileged access should succeed
    bool allowed = armv8m_mpu_check(&mpu, 0x20000100, 4,
        true, false, true, &fault);
    CHECK_TRUE(allowed);

    // Unprivileged access should fail
    allowed = armv8m_mpu_check(&mpu, 0x20000100, 4,
        true, false, false, &fault);
    CHECK_FALSE(allowed);
}

TEST(Permissions, ReadOnlyRegion)
{
    // Region with RO for all
    armv8m_mpu_configure_region(&mpu, 0,
        0x20000000, 0x20000FFF, MPU_AP_RO_ALL, false,
        MPU_SH_NONE, 0, true);

    // Read should succeed
    bool allowed = armv8m_mpu_check(&mpu, 0x20000100, 4,
        false, false, true, &fault);
    CHECK_TRUE(allowed);

    // Write should fail
    allowed = armv8m_mpu_check(&mpu, 0x20000100, 4,
        true, false, true, &fault);
    CHECK_FALSE(allowed);
}

TEST(Permissions, ExecuteNever)
{
    // Region with XN set
    armv8m_mpu_configure_region(&mpu, 0,
        0x20000000, 0x20000FFF, MPU_AP_RW_ALL, true,  // XN=true
        MPU_SH_NONE, 0, true);

    // Data read should succeed
    bool allowed = armv8m_mpu_check(&mpu, 0x20000100, 4,
        false, false, true, &fault);
    CHECK_TRUE(allowed);

    // Instruction fetch should fail
    allowed = armv8m_mpu_check(&mpu, 0x20000100, 4,
        false, true, true, &fault);
    CHECK_FALSE(allowed);
}

/*============================================================================
 * Test Group: Region Overlap
 *============================================================================*/

TEST_GROUP(RegionOverlap)
{
    MPU mpu;
    MPUFaultInfo fault;

    void setup()
    {
        armv8m_mpu_init(&mpu, 8);
        memset(&fault, 0, sizeof(fault));
        armv8m_mpu_enable(&mpu, true, false, false);
    }

    void teardown()
    {
    }
};

TEST(RegionOverlap, HigherRegionWins)
{
    // Region 0: Large area, RW
    armv8m_mpu_configure_region(&mpu, 0,
        0x20000000, 0x20000FFF, MPU_AP_RW_ALL, false,
        MPU_SH_NONE, 0, true);

    // Region 1: Overlapping smaller area, RO
    armv8m_mpu_configure_region(&mpu, 1,
        0x20000000, 0x200001FF, MPU_AP_RO_ALL, false,
        MPU_SH_NONE, 0, true);

    // Write to overlapped area should fail (region 1 wins)
    bool allowed = armv8m_mpu_check(&mpu, 0x20000100, 4,
        true, false, true, &fault);
    CHECK_FALSE(allowed);

    // Write outside region 1 but inside region 0 should succeed
    allowed = armv8m_mpu_check(&mpu, 0x20000300, 4,
        true, false, true, &fault);
    CHECK_TRUE(allowed);
}

/*============================================================================
 * Test Group: Register Access
 *============================================================================*/

TEST_GROUP(MPURegisters)
{
    MPU mpu;

    void setup()
    {
        armv8m_mpu_init(&mpu, 8);
    }

    void teardown()
    {
    }
};

TEST(MPURegisters, ReadMPUType)
{
    uint32_t type = armv8m_mpu_read(&mpu, MPU_TYPE, 4);
    // TYPE register: DREGION field contains number of regions
    uint32_t dregion = (type >> 8) & 0xFF;
    CHECK_EQUAL(8u, dregion);
}

TEST(MPURegisters, WriteMPUCtrl)
{
    armv8m_mpu_write(&mpu, MPU_CTRL, MPU_CTRL_ENABLE | MPU_CTRL_PRIVDEFENA, 4);
    CHECK_TRUE(mpu.enabled);
    CHECK_TRUE(mpu.privdefena);
}

TEST(MPURegisters, WriteRNR)
{
    armv8m_mpu_write(&mpu, MPU_RNR, 3, 4);
    CHECK_EQUAL(3u, mpu.rnr);
}

/*============================================================================
 * Test Runner
 *============================================================================*/

int main(int argc, char** argv)
{
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
