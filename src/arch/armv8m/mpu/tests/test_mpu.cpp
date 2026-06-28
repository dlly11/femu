/**
 * @file test_mpu.cpp
 * @brief CppUTest tests for the ARMv8-M MPU
 */

#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTest/TestHarness.h"

extern "C" {
#include "arch/armv8m/armv8m_mpu.h"
#include "arch/armv8m/armv8m_types.h"
}

/*============================================================================
 * Test Group: MPU Initialization
 *============================================================================*/

TEST_GROUP(MPUInit) {
  MPU mpu;

  void setup() {
    armv8m_mpu_init(&mpu, 8); // 8 regions
  }

  void teardown() {}
};

TEST(MPUInit, InitSetsDefaults) {
  CHECK_EQUAL(8, mpu.num_regions);
  CHECK_FALSE(mpu.enabled);
  CHECK_EQUAL(0u, mpu.ctrl);
}

/*============================================================================
 * Test Group: MPU Enable/Disable
 *============================================================================*/

TEST_GROUP(MPUControl) {
  MPU mpu;

  void setup() { armv8m_mpu_init(&mpu, 8); }

  void teardown() {}
};

TEST(MPUControl, EnableMPU) {
  armv8m_mpu_enable(&mpu, true, false, false);
  CHECK_TRUE(mpu.enabled);
  CHECK_TRUE(mpu.ctrl & MPU_CTRL_ENABLE);
}

TEST(MPUControl, EnableWithPrivdefena) {
  armv8m_mpu_enable(&mpu, true, false, true);
  CHECK_TRUE(mpu.privdefena);
  CHECK_TRUE(mpu.ctrl & MPU_CTRL_PRIVDEFENA);
}

TEST(MPUControl, DisableMPU) {
  armv8m_mpu_enable(&mpu, true, false, false);
  armv8m_mpu_enable(&mpu, false, false, false);
  CHECK_FALSE(mpu.enabled);
}

/*============================================================================
 * Test Group: Region Configuration
 *============================================================================*/

TEST_GROUP(RegionConfig) {
  MPU mpu;

  void setup() { armv8m_mpu_init(&mpu, 8); }

  void teardown() {}
};

TEST(RegionConfig, ConfigureRegion) {
  int result = armv8m_mpu_configure_region(&mpu, 0,
                                           0x20000000,    // Base
                                           0x20000FFF,    // Limit (4KB region)
                                           MPU_AP_RW_ALL, // RW for all
                                           false,         // Not XN
                                           MPU_SH_NONE,   // Non-shareable
                                           0,             // Attribute index 0
                                           true);         // Enable

  CHECK_EQUAL(ARMV8M_OK, result);
  CHECK_TRUE(mpu.regions[0].rlar & MPU_RLAR_EN);
}

TEST(RegionConfig, InvalidRegionNumber) {
  int result = armv8m_mpu_configure_region(
      &mpu, 10, // > 8 regions
      0x20000000, 0x20000FFF, MPU_AP_RW_ALL, false, MPU_SH_NONE, 0, true);

  CHECK_TRUE(result < 0); // Should fail
}

/*============================================================================
 * Test Group: Access Checking
 *============================================================================*/

TEST_GROUP(AccessCheck) {
  MPU mpu;
  MPUFaultInfo fault;

  void setup() {
    armv8m_mpu_init(&mpu, 8);
    memset(&fault, 0, sizeof(fault));

    // Configure region 0: 0x20000000-0x20000FFF, RW for all
    armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF, MPU_AP_RW_ALL,
                                false, MPU_SH_NONE, 0, true);

    armv8m_mpu_enable(&mpu, true, false, false);
  }

  void teardown() {}
};

TEST(AccessCheck, AllowedReadInRegion) {
  bool allowed =
      armv8m_mpu_check(&mpu, 0x20000100, 4, false, false, true, false, &fault);
  CHECK_TRUE(allowed);
}

TEST(AccessCheck, AllowedWriteInRegion) {
  bool allowed =
      armv8m_mpu_check(&mpu, 0x20000100, 4, true, false, true, false, &fault);
  CHECK_TRUE(allowed);
}

TEST(AccessCheck, DeniedAccessOutsideRegion) {
  bool allowed =
      armv8m_mpu_check(&mpu, 0x30000000, 4, false, false, true, false, &fault);
  CHECK_FALSE(allowed);
}

/*============================================================================
 * Test Group: Access Permissions
 *============================================================================*/

TEST_GROUP(Permissions) {
  MPU mpu;
  MPUFaultInfo fault;

  void setup() {
    armv8m_mpu_init(&mpu, 8);
    memset(&fault, 0, sizeof(fault));
    armv8m_mpu_enable(&mpu, true, false, false);
  }

  void teardown() {}
};

TEST(Permissions, PrivilegedOnlyRegion) {
  // Region with RW for privileged only
  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF, MPU_AP_RW_PRIV,
                              false, MPU_SH_NONE, 0, true);

  // Privileged access should succeed
  bool allowed =
      armv8m_mpu_check(&mpu, 0x20000100, 4, true, false, true, false, &fault);
  CHECK_TRUE(allowed);

  // Unprivileged access should fail
  allowed =
      armv8m_mpu_check(&mpu, 0x20000100, 4, true, false, false, false, &fault);
  CHECK_FALSE(allowed);
}

TEST(Permissions, ReadOnlyRegion) {
  // Region with RO for all
  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF, MPU_AP_RO_ALL,
                              false, MPU_SH_NONE, 0, true);

  // Read should succeed
  bool allowed =
      armv8m_mpu_check(&mpu, 0x20000100, 4, false, false, true, false, &fault);
  CHECK_TRUE(allowed);

  // Write should fail
  allowed =
      armv8m_mpu_check(&mpu, 0x20000100, 4, true, false, true, false, &fault);
  CHECK_FALSE(allowed);
}

TEST(Permissions, ExecuteNever) {
  // Region with XN set
  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF, MPU_AP_RW_ALL,
                              true, // XN=true
                              MPU_SH_NONE, 0, true);

  // Data read should succeed
  bool allowed =
      armv8m_mpu_check(&mpu, 0x20000100, 4, false, false, true, false, &fault);
  CHECK_TRUE(allowed);

  // Instruction fetch should fail
  allowed =
      armv8m_mpu_check(&mpu, 0x20000100, 4, false, true, true, false, &fault);
  CHECK_FALSE(allowed);
}

/*============================================================================
 * Test Group: Region Overlap
 *============================================================================*/

TEST_GROUP(RegionOverlap) {
  MPU mpu;
  MPUFaultInfo fault;

  void setup() {
    armv8m_mpu_init(&mpu, 8);
    memset(&fault, 0, sizeof(fault));
    armv8m_mpu_enable(&mpu, true, false, false);
  }

  void teardown() {}
};

TEST(RegionOverlap, HigherRegionWins) {
  // Region 0: Large area, RW
  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF, MPU_AP_RW_ALL,
                              false, MPU_SH_NONE, 0, true);

  // Region 1: Overlapping smaller area, RO
  armv8m_mpu_configure_region(&mpu, 1, 0x20000000, 0x200001FF, MPU_AP_RO_ALL,
                              false, MPU_SH_NONE, 0, true);

  // Write to overlapped area should fail (region 1 wins)
  bool allowed =
      armv8m_mpu_check(&mpu, 0x20000100, 4, true, false, true, false, &fault);
  CHECK_FALSE(allowed);

  // Write outside region 1 but inside region 0 should succeed
  allowed =
      armv8m_mpu_check(&mpu, 0x20000300, 4, true, false, true, false, &fault);
  CHECK_TRUE(allowed);
}

/*============================================================================
 * Test Group: Register Access
 *============================================================================*/

TEST_GROUP(MPURegisters) {
  MPU mpu;

  void setup() { armv8m_mpu_init(&mpu, 8); }

  void teardown() {}
};

TEST(MPURegisters, ReadMPUType) {
  uint32_t type = armv8m_mpu_read(&mpu, MPU_TYPE, 4);
  // TYPE register: DREGION field contains number of regions
  uint32_t dregion = (type >> 8) & 0xFF;
  CHECK_EQUAL(8u, dregion);
}

TEST(MPURegisters, WriteMPUCtrl) {
  armv8m_mpu_write(&mpu, MPU_CTRL, MPU_CTRL_ENABLE | MPU_CTRL_PRIVDEFENA, 4);
  CHECK_TRUE(mpu.enabled);
  CHECK_TRUE(mpu.privdefena);
}

TEST(MPURegisters, WriteRNR) {
  armv8m_mpu_write(&mpu, MPU_RNR, 3, 4);
  CHECK_EQUAL(3u, mpu.rnr);
}

/*============================================================================
 * Test Group: Extended Initialization Tests
 *============================================================================*/

TEST_GROUP(MPUInitExtended) {
  MPU mpu;

  void setup() {}
  void teardown() {}
};

TEST(MPUInitExtended, InitClampsMaxRegions) {
  armv8m_mpu_init(&mpu, 20); // > 16
  CHECK_EQUAL(16, mpu.num_regions);
}

TEST(MPUInitExtended, InitClampsNegativeRegions) {
  armv8m_mpu_init(&mpu, -5);
  CHECK_EQUAL(0, mpu.num_regions);
}

TEST(MPUInitExtended, ResetPreservesRegionCount) {
  armv8m_mpu_init(&mpu, 8);
  armv8m_mpu_enable(&mpu, true, true, true);
  armv8m_mpu_reset(&mpu);
  CHECK_EQUAL(8, mpu.num_regions);
  CHECK_FALSE(mpu.enabled);
}

/*============================================================================
 * Test Group: Extended Control Tests
 *============================================================================*/

TEST_GROUP(MPUControlExtended) {
  MPU mpu;

  void setup() { armv8m_mpu_init(&mpu, 8); }

  void teardown() {}
};

TEST(MPUControlExtended, EnableWithHfnmiena) {
  armv8m_mpu_enable(&mpu, true, true, false);
  CHECK_TRUE(mpu.hfnmiena);
  CHECK_TRUE(mpu.ctrl & MPU_CTRL_HFNMIENA);
}

TEST(MPUControlExtended, EnableAllFlags) {
  armv8m_mpu_enable(&mpu, true, true, true);
  CHECK_TRUE(mpu.enabled);
  CHECK_TRUE(mpu.hfnmiena);
  CHECK_TRUE(mpu.privdefena);
  CHECK_EQUAL(MPU_CTRL_ENABLE | MPU_CTRL_HFNMIENA | MPU_CTRL_PRIVDEFENA,
              mpu.ctrl);
}

/*============================================================================
 * Test Group: HFNMIENA Behavior Tests
 *============================================================================*/

TEST_GROUP(HfnmienaBehavior) {
  MPU mpu;
  MPUFaultInfo fault;

  void setup() {
    armv8m_mpu_init(&mpu, 8);
    memset(&fault, 0, sizeof(fault));
  }

  void teardown() {}
};

TEST(HfnmienaBehavior, HfnmienaDisabledBypassesMPUInFaultHandler) {
  // Configure a restrictive region
  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF, MPU_AP_RO_ALL,
                              false, MPU_SH_NONE, 0, true);

  // Enable MPU without HFNMIENA
  armv8m_mpu_enable(&mpu, true, false, false);

  // Normal access: write should fail (RO region)
  bool allowed =
      armv8m_mpu_check(&mpu, 0x20000100, 4, true, false, true, false, &fault);
  CHECK_FALSE(allowed);

  // In HardFault/NMI handler: write should succeed (MPU bypassed)
  allowed =
      armv8m_mpu_check(&mpu, 0x20000100, 4, true, false, true, true, &fault);
  CHECK_TRUE(allowed);
}

TEST(HfnmienaBehavior, HfnmienaEnabledEnforcesMPUInFaultHandler) {
  // Configure a restrictive region
  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF, MPU_AP_RO_ALL,
                              false, MPU_SH_NONE, 0, true);

  // Enable MPU with HFNMIENA
  armv8m_mpu_enable(&mpu, true, true, false);

  // Normal access: write should fail
  bool allowed =
      armv8m_mpu_check(&mpu, 0x20000100, 4, true, false, true, false, &fault);
  CHECK_FALSE(allowed);

  // In HardFault/NMI handler: write should still fail (HFNMIENA=1)
  allowed =
      armv8m_mpu_check(&mpu, 0x20000100, 4, true, false, true, true, &fault);
  CHECK_FALSE(allowed);
}

TEST(HfnmienaBehavior, HfnmienaBypassAllowsUnmappedAccess) {
  // Enable MPU without HFNMIENA, no regions configured
  armv8m_mpu_enable(&mpu, true, false, false);

  // Normal access to unmapped region: should fail
  bool allowed =
      armv8m_mpu_check(&mpu, 0x30000000, 4, false, false, true, false, &fault);
  CHECK_FALSE(allowed);

  // In HardFault/NMI handler: should succeed (MPU bypassed)
  allowed =
      armv8m_mpu_check(&mpu, 0x30000000, 4, false, false, true, true, &fault);
  CHECK_TRUE(allowed);
}

/*============================================================================
 * Test Group: Extended Region Configuration Tests
 *============================================================================*/

TEST_GROUP(RegionConfigExtended) {
  MPU mpu;

  void setup() { armv8m_mpu_init(&mpu, 8); }

  void teardown() {}
};

TEST(RegionConfigExtended, NegativeRegionNumber) {
  int result =
      armv8m_mpu_configure_region(&mpu, -1, 0x20000000, 0x20000FFF,
                                  MPU_AP_RW_ALL, false, MPU_SH_NONE, 0, true);
  CHECK_TRUE(result < 0);
}

TEST(RegionConfigExtended, InvalidAttributeIndex) {
  int result = armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF,
                                           MPU_AP_RW_ALL, false, MPU_SH_NONE,
                                           10, true); // attr_idx > 7
  CHECK_TRUE(result < 0);
}

TEST(RegionConfigExtended, NegativeAttributeIndex) {
  int result =
      armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF,
                                  MPU_AP_RW_ALL, false, MPU_SH_NONE, -1, true);
  CHECK_TRUE(result < 0);
}

TEST(RegionConfigExtended, DisabledRegion) {
  int result = armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF,
                                           MPU_AP_RW_ALL, false, MPU_SH_NONE, 0,
                                           false); // Enable = false
  CHECK_EQUAL(ARMV8M_OK, result);
  CHECK_FALSE(mpu.regions[0].rlar & MPU_RLAR_EN);
}

TEST(RegionConfigExtended, ShareabilityOuter) {
  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF, MPU_AP_RW_ALL,
                              false, MPU_SH_OUTER, 0, true);
  uint32_t sh = (mpu.regions[0].rbar >> MPU_RBAR_SH_SHIFT) & MPU_RBAR_SH_MASK;
  CHECK_EQUAL(MPU_SH_OUTER, sh);
}

TEST(RegionConfigExtended, ShareabilityInner) {
  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF, MPU_AP_RW_ALL,
                              false, MPU_SH_INNER, 0, true);
  uint32_t sh = (mpu.regions[0].rbar >> MPU_RBAR_SH_SHIFT) & MPU_RBAR_SH_MASK;
  CHECK_EQUAL(MPU_SH_INNER, sh);
}

/*============================================================================
 * Test Group: Extended Permission Tests
 *============================================================================*/

TEST_GROUP(PermissionsExtended) {
  MPU mpu;
  MPUFaultInfo fault;

  void setup() {
    armv8m_mpu_init(&mpu, 8);
    memset(&fault, 0, sizeof(fault));
    armv8m_mpu_enable(&mpu, true, false, false);
  }

  void teardown() {}
};

TEST(PermissionsExtended, ReadOnlyPrivilegedOnly) {
  // Region with RO for privileged only
  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF, MPU_AP_RO_PRIV,
                              false, MPU_SH_NONE, 0, true);

  // Privileged read should succeed
  bool allowed =
      armv8m_mpu_check(&mpu, 0x20000100, 4, false, false, true, false, &fault);
  CHECK_TRUE(allowed);

  // Privileged write should fail
  allowed =
      armv8m_mpu_check(&mpu, 0x20000100, 4, true, false, true, false, &fault);
  CHECK_FALSE(allowed);

  // Unprivileged read should fail
  allowed =
      armv8m_mpu_check(&mpu, 0x20000100, 4, false, false, false, false, &fault);
  CHECK_FALSE(allowed);
}

TEST(PermissionsExtended, DisabledMPUAllowsAllAccess) {
  // Configure a RO region
  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF, MPU_AP_RO_ALL,
                              false, MPU_SH_NONE, 0, true);

  // Disable MPU
  armv8m_mpu_enable(&mpu, false, false, false);

  // Write should succeed when MPU is disabled
  bool allowed =
      armv8m_mpu_check(&mpu, 0x20000100, 4, true, false, true, false, &fault);
  CHECK_TRUE(allowed);
}

TEST(PermissionsExtended, NullFaultPointer) {
  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF, MPU_AP_RW_ALL,
                              false, MPU_SH_NONE, 0, true);

  // Pass NULL for fault - should still work
  bool allowed =
      armv8m_mpu_check(&mpu, 0x20000100, 4, false, false, true, false, NULL);
  CHECK_TRUE(allowed);

  // Fault case with NULL pointer
  allowed =
      armv8m_mpu_check(&mpu, 0x30000000, 4, false, false, true, false, NULL);
  CHECK_FALSE(allowed);
}

TEST(PermissionsExtended, FaultInfoForInstructionFetch) {
  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF, MPU_AP_RW_ALL,
                              true, // XN
                              MPU_SH_NONE, 0, true);

  bool allowed =
      armv8m_mpu_check(&mpu, 0x20000100, 4, false, true, true, false, &fault);
  CHECK_FALSE(allowed);
  CHECK_EQUAL(MPU_FAULT_IACCVIOL, fault.type);
  CHECK_EQUAL(0x20000100u, fault.addr);
  CHECK_TRUE(fault.addr_valid);
}

TEST(PermissionsExtended, FaultInfoForDataAccess) {
  // No region configured - access outside region
  bool allowed =
      armv8m_mpu_check(&mpu, 0x30000000, 4, true, false, true, false, &fault);
  CHECK_FALSE(allowed);
  CHECK_EQUAL(MPU_FAULT_DACCVIOL, fault.type);
  CHECK_EQUAL(0x30000000u, fault.addr);
  CHECK_TRUE(fault.addr_valid);
}

/*============================================================================
 * Test Group: PRIVDEFENA (Default Memory Map) Tests
 *============================================================================*/

TEST_GROUP(PrivdefenaTests) {
  MPU mpu;
  MPUFaultInfo fault;

  void setup() {
    armv8m_mpu_init(&mpu, 8);
    memset(&fault, 0, sizeof(fault));
  }

  void teardown() {}
};

TEST(PrivdefenaTests, PrivilegedAccessWithPrivdefena) {
  // Enable MPU with PRIVDEFENA
  armv8m_mpu_enable(&mpu, true, false, true);

  // Privileged data access to unmapped region should use default map
  bool allowed =
      armv8m_mpu_check(&mpu, 0x20000100, 4, false, false, true, false, &fault);
  CHECK_TRUE(allowed);
}

TEST(PrivdefenaTests, UnprivilegedAccessDeniedWithPrivdefena) {
  // Enable MPU with PRIVDEFENA
  armv8m_mpu_enable(&mpu, true, false, true);

  // Unprivileged access to unmapped region should fail
  bool allowed =
      armv8m_mpu_check(&mpu, 0x20000100, 4, false, false, false, false, &fault);
  CHECK_FALSE(allowed);
}

TEST(PrivdefenaTests, InstructionFetchInCodeRegion) {
  // Enable MPU with PRIVDEFENA
  armv8m_mpu_enable(&mpu, true, false, true);

  // Instruction fetch from code region (< 0x20000000) should succeed
  bool allowed =
      armv8m_mpu_check(&mpu, 0x00001000, 4, false, true, true, false, &fault);
  CHECK_TRUE(allowed);
}

TEST(PrivdefenaTests, InstructionFetchInSRAMRegionFails) {
  // Enable MPU with PRIVDEFENA
  armv8m_mpu_enable(&mpu, true, false, true);

  // Instruction fetch from SRAM region should fail (XN)
  bool allowed =
      armv8m_mpu_check(&mpu, 0x20000100, 4, false, true, true, false, &fault);
  CHECK_FALSE(allowed);
}

/*============================================================================
 * Test Group: Boundary Crossing Tests
 *============================================================================*/

TEST_GROUP(BoundaryCrossing) {
  MPU mpu;
  MPUFaultInfo fault;

  void setup() {
    armv8m_mpu_init(&mpu, 8);
    memset(&fault, 0, sizeof(fault));
    armv8m_mpu_enable(&mpu, true, false, false);
  }

  void teardown() {}
};

TEST(BoundaryCrossing, AccessSpanningTwoRegions) {
  // Region 0: 0x20000000-0x200000FF, RW
  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x200000FF, MPU_AP_RW_ALL,
                              false, MPU_SH_NONE, 0, true);

  // Region 1: 0x20000100-0x200001FF, RW
  armv8m_mpu_configure_region(&mpu, 1, 0x20000100, 0x200001FF, MPU_AP_RW_ALL,
                              false, MPU_SH_NONE, 0, true);

  // Access spanning both regions (starts at 0xFE, size 4 ends at 0x101)
  bool allowed =
      armv8m_mpu_check(&mpu, 0x200000FE, 4, true, false, true, false, &fault);
  CHECK_TRUE(allowed);
}

TEST(BoundaryCrossing, AccessSpanningRegionToUnmapped) {
  // Region 0: 0x20000000-0x200000FF, RW
  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x200000FF, MPU_AP_RW_ALL,
                              false, MPU_SH_NONE, 0, true);

  // Access starting in region, ending outside (no PRIVDEFENA)
  bool allowed =
      armv8m_mpu_check(&mpu, 0x200000FE, 4, true, false, true, false, &fault);
  CHECK_FALSE(allowed);
}

TEST(BoundaryCrossing, AccessSpanningWithPrivdefena) {
  // Enable with PRIVDEFENA
  armv8m_mpu_enable(&mpu, true, false, true);

  // Region 0: 0x20000000-0x200000FF, RW
  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x200000FF, MPU_AP_RW_ALL,
                              false, MPU_SH_NONE, 0, true);

  // Privileged access spanning region to default map should work
  bool allowed =
      armv8m_mpu_check(&mpu, 0x200000FE, 4, false, false, true, false, &fault);
  CHECK_TRUE(allowed);
}

TEST(BoundaryCrossing, AccessSpanningWithMismatchedPermissions) {
  // Region 0: 0x20000000-0x200000FF, RW
  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x200000FF, MPU_AP_RW_ALL,
                              false, MPU_SH_NONE, 0, true);

  // Region 1: 0x20000100-0x200001FF, RO
  armv8m_mpu_configure_region(&mpu, 1, 0x20000100, 0x200001FF, MPU_AP_RO_ALL,
                              false, MPU_SH_NONE, 0, true);

  // Write spanning RW to RO should fail
  bool allowed =
      armv8m_mpu_check(&mpu, 0x200000FE, 4, true, false, true, false, &fault);
  CHECK_FALSE(allowed);
}

/*============================================================================
 * Test Group: Extended Register Access Tests
 *============================================================================*/

TEST_GROUP(MPURegistersExtended) {
  MPU mpu;

  void setup() { armv8m_mpu_init(&mpu, 8); }

  void teardown() {}
};

TEST(MPURegistersExtended, ReadMPUCtrl) {
  armv8m_mpu_enable(&mpu, true, true, true);
  uint32_t ctrl = armv8m_mpu_read(&mpu, MPU_CTRL, 4);
  CHECK_EQUAL(MPU_CTRL_ENABLE | MPU_CTRL_HFNMIENA | MPU_CTRL_PRIVDEFENA, ctrl);
}

TEST(MPURegistersExtended, ReadMPURNR) {
  armv8m_mpu_write(&mpu, MPU_RNR, 5, 4);
  uint32_t rnr = armv8m_mpu_read(&mpu, MPU_RNR, 4);
  CHECK_EQUAL(5u, rnr);
}

TEST(MPURegistersExtended, ReadWriteRBAR) {
  armv8m_mpu_write(&mpu, MPU_RNR, 0, 4);
  armv8m_mpu_write(&mpu, MPU_RBAR, 0x20000000 | MPU_RBAR_XN, 4);
  uint32_t rbar = armv8m_mpu_read(&mpu, MPU_RBAR, 4);
  CHECK_EQUAL(0x20000000u | MPU_RBAR_XN, rbar);
}

TEST(MPURegistersExtended, ReadWriteRLAR) {
  armv8m_mpu_write(&mpu, MPU_RNR, 0, 4);
  uint32_t write_val = 0x20000FE0 | MPU_RLAR_EN;
  armv8m_mpu_write(&mpu, MPU_RLAR, write_val, 4);
  uint32_t rlar = armv8m_mpu_read(&mpu, MPU_RLAR, 4);
  CHECK_EQUAL(write_val, rlar);
}

TEST(MPURegistersExtended, ReadWriteRBARAlias1) {
  armv8m_mpu_write(&mpu, MPU_RNR, 0, 4);
  armv8m_mpu_write(&mpu, MPU_RBAR_A1, 0x30000000, 4);
  uint32_t rbar = armv8m_mpu_read(&mpu, MPU_RBAR_A1, 4);
  CHECK_EQUAL(0x30000000u, rbar);
  // Verify it's in region 1
  CHECK_EQUAL(0x30000000u, mpu.regions[1].rbar);
}

TEST(MPURegistersExtended, ReadWriteRLARAlias1) {
  armv8m_mpu_write(&mpu, MPU_RNR, 0, 4);
  uint32_t write_val = 0x30000FE0 | MPU_RLAR_EN;
  armv8m_mpu_write(&mpu, MPU_RLAR_A1, write_val, 4);
  uint32_t rlar = armv8m_mpu_read(&mpu, MPU_RLAR_A1, 4);
  CHECK_EQUAL(write_val, rlar);
}

TEST(MPURegistersExtended, ReadWriteRBARAlias2) {
  armv8m_mpu_write(&mpu, MPU_RNR, 0, 4);
  armv8m_mpu_write(&mpu, MPU_RBAR_A2, 0x40000000, 4);
  uint32_t rbar = armv8m_mpu_read(&mpu, MPU_RBAR_A2, 4);
  CHECK_EQUAL(0x40000000u, rbar);
}

TEST(MPURegistersExtended, ReadWriteRLARAlias2) {
  armv8m_mpu_write(&mpu, MPU_RNR, 0, 4);
  uint32_t write_val = 0x40000FE0 | MPU_RLAR_EN;
  armv8m_mpu_write(&mpu, MPU_RLAR_A2, write_val, 4);
  uint32_t rlar = armv8m_mpu_read(&mpu, MPU_RLAR_A2, 4);
  CHECK_EQUAL(write_val, rlar);
}

TEST(MPURegistersExtended, ReadWriteRBARAlias3) {
  armv8m_mpu_write(&mpu, MPU_RNR, 0, 4);
  armv8m_mpu_write(&mpu, MPU_RBAR_A3, 0x50000000, 4);
  uint32_t rbar = armv8m_mpu_read(&mpu, MPU_RBAR_A3, 4);
  CHECK_EQUAL(0x50000000u, rbar);
}

TEST(MPURegistersExtended, ReadWriteRLARAlias3) {
  armv8m_mpu_write(&mpu, MPU_RNR, 0, 4);
  uint32_t write_val = 0x50000FE0 | MPU_RLAR_EN;
  armv8m_mpu_write(&mpu, MPU_RLAR_A3, write_val, 4);
  uint32_t rlar = armv8m_mpu_read(&mpu, MPU_RLAR_A3, 4);
  CHECK_EQUAL(write_val, rlar);
}

TEST(MPURegistersExtended, ReadWriteMAIR0) {
  armv8m_mpu_write(&mpu, MPU_MAIR0, 0x44BB00FF, 4);
  uint32_t mair0 = armv8m_mpu_read(&mpu, MPU_MAIR0, 4);
  CHECK_EQUAL(0x44BB00FFu, mair0);
}

TEST(MPURegistersExtended, ReadWriteMAIR1) {
  armv8m_mpu_write(&mpu, MPU_MAIR1, 0xAABBCCDD, 4);
  uint32_t mair1 = armv8m_mpu_read(&mpu, MPU_MAIR1, 4);
  CHECK_EQUAL(0xAABBCCDDu, mair1);
}

TEST(MPURegistersExtended, WriteToTypeRegisterIgnored) {
  uint32_t type_before = armv8m_mpu_read(&mpu, MPU_TYPE, 4);
  armv8m_mpu_write(&mpu, MPU_TYPE, 0xFFFFFFFF, 4);
  uint32_t type_after = armv8m_mpu_read(&mpu, MPU_TYPE, 4);
  CHECK_EQUAL(type_before, type_after);
}

TEST(MPURegistersExtended, ReadUnknownRegister) {
  uint32_t val = armv8m_mpu_read(&mpu, 0xFF, 4);
  CHECK_EQUAL(0u, val);
}

TEST(MPURegistersExtended, WriteUnknownRegister) {
  // Should not crash
  armv8m_mpu_write(&mpu, 0xFF, 0x12345678, 4);
}

TEST(MPURegistersExtended, RNROutOfRange) {
  // Try to set RNR beyond num_regions - should be ignored
  armv8m_mpu_write(&mpu, MPU_RNR, 10, 4); // 10 > 8
  CHECK_EQUAL(0u, mpu.rnr);               // Should remain at 0
}

TEST(MPURegistersExtended, ReadRBARWithInvalidRNR) {
  // Set RNR to max regions - reading should return 0
  mpu.rnr = 8; // Out of bounds
  uint32_t rbar = armv8m_mpu_read(&mpu, MPU_RBAR, 4);
  CHECK_EQUAL(0u, rbar);
}

TEST(MPURegistersExtended, ReadRLARWithInvalidRNR) {
  mpu.rnr = 8;
  uint32_t rlar = armv8m_mpu_read(&mpu, MPU_RLAR, 4);
  CHECK_EQUAL(0u, rlar);
}

TEST(MPURegistersExtended, ReadAliasWithOverflow) {
  // Set RNR such that alias would overflow
  armv8m_mpu_write(&mpu, MPU_RNR, 7, 4); // rnr=7, alias 1 = 8 (invalid)
  uint32_t rbar = armv8m_mpu_read(&mpu, MPU_RBAR_A1, 4);
  CHECK_EQUAL(0u, rbar);
}

TEST(MPURegistersExtended, WriteRBARWithInvalidRNR) {
  mpu.rnr = 8;
  armv8m_mpu_write(&mpu, MPU_RBAR, 0x12345678, 4);
  // Should be ignored - verify no crash and region 0 unchanged
}

TEST(MPURegistersExtended, WriteRLARWithInvalidRNR) {
  mpu.rnr = 8;
  armv8m_mpu_write(&mpu, MPU_RLAR, 0x12345678, 4);
  // Should be ignored
}

TEST(MPURegistersExtended, WriteAliasWithOverflow) {
  armv8m_mpu_write(&mpu, MPU_RNR, 7, 4);
  armv8m_mpu_write(&mpu, MPU_RBAR_A1, 0x12345678, 4);
  // Should be ignored
}

TEST(MPURegistersExtended, WriteAliasRLARWithOverflow) {
  armv8m_mpu_write(&mpu, MPU_RNR, 7, 4);
  armv8m_mpu_write(&mpu, MPU_RLAR_A1, 0x12345678, 4);
  // Should be ignored
}

TEST(MPURegistersExtended, WriteAliasA2WithOverflow) {
  armv8m_mpu_write(&mpu, MPU_RNR, 6, 4); // alias 2 = 8 (invalid)
  armv8m_mpu_write(&mpu, MPU_RBAR_A2, 0x12345678, 4);
  armv8m_mpu_write(&mpu, MPU_RLAR_A2, 0x12345678, 4);
}

TEST(MPURegistersExtended, WriteAliasA3WithOverflow) {
  armv8m_mpu_write(&mpu, MPU_RNR, 5, 4); // alias 3 = 8 (invalid)
  armv8m_mpu_write(&mpu, MPU_RBAR_A3, 0x12345678, 4);
  armv8m_mpu_write(&mpu, MPU_RLAR_A3, 0x12345678, 4);
}

TEST(MPURegistersExtended, ReadAliasA2WithOverflow) {
  armv8m_mpu_write(&mpu, MPU_RNR, 6, 4);
  CHECK_EQUAL(0u, armv8m_mpu_read(&mpu, MPU_RBAR_A2, 4));
  CHECK_EQUAL(0u, armv8m_mpu_read(&mpu, MPU_RLAR_A2, 4));
}

TEST(MPURegistersExtended, ReadAliasA3WithOverflow) {
  armv8m_mpu_write(&mpu, MPU_RNR, 5, 4);
  CHECK_EQUAL(0u, armv8m_mpu_read(&mpu, MPU_RBAR_A3, 4));
  CHECK_EQUAL(0u, armv8m_mpu_read(&mpu, MPU_RLAR_A3, 4));
}

/*============================================================================
 * Test Group: Additional Coverage Tests
 *============================================================================*/

TEST_GROUP(AdditionalCoverage) {
  MPU mpu;
  MPUFaultInfo fault;

  void setup() {
    armv8m_mpu_init(&mpu, 8);
    memset(&fault, 0, sizeof(fault));
  }

  void teardown() {}
};

TEST(AdditionalCoverage, SingleByteAccess) {
  armv8m_mpu_enable(&mpu, true, false, false);
  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF, MPU_AP_RW_ALL,
                              false, MPU_SH_NONE, 0, true);

  // Size = 1 byte access
  bool allowed =
      armv8m_mpu_check(&mpu, 0x20000100, 1, false, false, true, false, &fault);
  CHECK_TRUE(allowed);
}

TEST(AdditionalCoverage, SpanningAccessStartRegionFails) {
  armv8m_mpu_enable(&mpu, true, false, false);

  // Region 0: RO (will fail write check)
  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x200000FF, MPU_AP_RO_ALL,
                              false, MPU_SH_NONE, 0, true);

  // Region 1: RW
  armv8m_mpu_configure_region(&mpu, 1, 0x20000100, 0x200001FF, MPU_AP_RW_ALL,
                              false, MPU_SH_NONE, 0, true);

  // Write spanning from RO region to RW region - start should fail
  bool allowed =
      armv8m_mpu_check(&mpu, 0x200000FE, 4, true, false, true, false, &fault);
  CHECK_FALSE(allowed);
}

TEST(AdditionalCoverage, FetchFaultWithNullFaultPtr) {
  armv8m_mpu_enable(&mpu, true, false, false);

  // Region with XN
  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF, MPU_AP_RW_ALL,
                              true, MPU_SH_NONE, 0, true);

  // Instruction fetch fault with NULL fault pointer
  bool allowed =
      armv8m_mpu_check(&mpu, 0x20000100, 4, false, true, true, false, NULL);
  CHECK_FALSE(allowed);
}

TEST(AdditionalCoverage, DataFaultWithFaultPtr) {
  armv8m_mpu_enable(&mpu, true, false, false);

  // No region at this address
  bool allowed =
      armv8m_mpu_check(&mpu, 0x30000000, 1, false, false, true, false, &fault);
  CHECK_FALSE(allowed);
  CHECK_EQUAL(MPU_FAULT_DACCVIOL, fault.type);
}

TEST(AdditionalCoverage, ReadRLARAlias1WithOverflow) {
  // Set RNR to 7, so alias 1 points to region 8 (invalid for 8 regions)
  armv8m_mpu_write(&mpu, MPU_RNR, 7, 4);
  uint32_t rlar = armv8m_mpu_read(&mpu, MPU_RLAR_A1, 4);
  CHECK_EQUAL(0u, rlar);
}

TEST(AdditionalCoverage, SpanningAccessWithNullFault) {
  armv8m_mpu_enable(&mpu, true, false, false);

  // Region 0: 0x20000000-0x200000FF, RO
  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x200000FF, MPU_AP_RO_ALL,
                              false, MPU_SH_NONE, 0, true);

  // Region 1: 0x20000100-0x200001FF, RO
  armv8m_mpu_configure_region(&mpu, 1, 0x20000100, 0x200001FF, MPU_AP_RO_ALL,
                              false, MPU_SH_NONE, 0, true);

  // Write spanning both RO regions with NULL fault pointer
  bool allowed =
      armv8m_mpu_check(&mpu, 0x200000FE, 4, true, false, true, false, NULL);
  CHECK_FALSE(allowed);
}

TEST(AdditionalCoverage, ZeroSizeAccessDenied) {
  armv8m_mpu_enable(&mpu, true, false, false);
  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF, MPU_AP_RW_ALL,
                              false, MPU_SH_NONE, 0, true);

  // Size = 0 should be denied
  bool allowed =
      armv8m_mpu_check(&mpu, 0x20000100, 0, false, false, true, false, &fault);
  CHECK_FALSE(allowed);
  CHECK_EQUAL(MPU_FAULT_DACCVIOL, fault.type);
}

TEST(AdditionalCoverage, AddressOverflowDenied) {
  armv8m_mpu_enable(&mpu, true, false, false);
  armv8m_mpu_configure_region(&mpu, 0, 0xFFFFFF00, 0xFFFFFFFF, MPU_AP_RW_ALL,
                              false, MPU_SH_NONE, 0, true);

  // Access that would overflow: 0xFFFFFFFE + 4 - 1 wraps
  bool allowed =
      armv8m_mpu_check(&mpu, 0xFFFFFFFE, 4, false, false, true, false, &fault);
  CHECK_FALSE(allowed);
}

/*============================================================================
 * Test Group: Memory Attributes Tests
 *============================================================================*/

TEST_GROUP(MemoryAttributes) {
  MPU mpu;

  void setup() { armv8m_mpu_init(&mpu, 8); }

  void teardown() {}
};

TEST(MemoryAttributes, GetAttributesDisabledMPU) {
  uint8_t attr = armv8m_mpu_get_attributes(&mpu, 0x20000000, true);
  CHECK_EQUAL(0, attr);
}

TEST(MemoryAttributes, GetAttributesFromMAIR0) {
  armv8m_mpu_enable(&mpu, true, false, false);
  armv8m_mpu_write(&mpu, MPU_MAIR0, 0x44332211, 4);

  // Configure region with attr_idx = 0
  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF, MPU_AP_RW_ALL,
                              false, MPU_SH_NONE, 0, true);

  uint8_t attr = armv8m_mpu_get_attributes(&mpu, 0x20000100, true);
  CHECK_EQUAL(0x11, attr);
}

TEST(MemoryAttributes, GetAttributesFromMAIR0Index1) {
  armv8m_mpu_enable(&mpu, true, false, false);
  armv8m_mpu_write(&mpu, MPU_MAIR0, 0x44332211, 4);

  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF, MPU_AP_RW_ALL,
                              false, MPU_SH_NONE, 1, true);

  uint8_t attr = armv8m_mpu_get_attributes(&mpu, 0x20000100, true);
  CHECK_EQUAL(0x22, attr);
}

TEST(MemoryAttributes, GetAttributesFromMAIR0Index2) {
  armv8m_mpu_enable(&mpu, true, false, false);
  armv8m_mpu_write(&mpu, MPU_MAIR0, 0x44332211, 4);

  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF, MPU_AP_RW_ALL,
                              false, MPU_SH_NONE, 2, true);

  uint8_t attr = armv8m_mpu_get_attributes(&mpu, 0x20000100, true);
  CHECK_EQUAL(0x33, attr);
}

TEST(MemoryAttributes, GetAttributesFromMAIR0Index3) {
  armv8m_mpu_enable(&mpu, true, false, false);
  armv8m_mpu_write(&mpu, MPU_MAIR0, 0x44332211, 4);

  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF, MPU_AP_RW_ALL,
                              false, MPU_SH_NONE, 3, true);

  uint8_t attr = armv8m_mpu_get_attributes(&mpu, 0x20000100, true);
  CHECK_EQUAL(0x44, attr);
}

TEST(MemoryAttributes, GetAttributesFromMAIR1) {
  armv8m_mpu_enable(&mpu, true, false, false);
  armv8m_mpu_write(&mpu, MPU_MAIR1, 0xDDCCBBAA, 4);

  // Configure region with attr_idx = 4 (first byte of MAIR1)
  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF, MPU_AP_RW_ALL,
                              false, MPU_SH_NONE, 4, true);

  uint8_t attr = armv8m_mpu_get_attributes(&mpu, 0x20000100, true);
  CHECK_EQUAL(0xAA, attr);
}

TEST(MemoryAttributes, GetAttributesFromMAIR1Index5) {
  armv8m_mpu_enable(&mpu, true, false, false);
  armv8m_mpu_write(&mpu, MPU_MAIR1, 0xDDCCBBAA, 4);

  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF, MPU_AP_RW_ALL,
                              false, MPU_SH_NONE, 5, true);

  uint8_t attr = armv8m_mpu_get_attributes(&mpu, 0x20000100, true);
  CHECK_EQUAL(0xBB, attr);
}

TEST(MemoryAttributes, GetAttributesFromMAIR1Index6) {
  armv8m_mpu_enable(&mpu, true, false, false);
  armv8m_mpu_write(&mpu, MPU_MAIR1, 0xDDCCBBAA, 4);

  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF, MPU_AP_RW_ALL,
                              false, MPU_SH_NONE, 6, true);

  uint8_t attr = armv8m_mpu_get_attributes(&mpu, 0x20000100, true);
  CHECK_EQUAL(0xCC, attr);
}

TEST(MemoryAttributes, GetAttributesFromMAIR1Index7) {
  armv8m_mpu_enable(&mpu, true, false, false);
  armv8m_mpu_write(&mpu, MPU_MAIR1, 0xDDCCBBAA, 4);

  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x20000FFF, MPU_AP_RW_ALL,
                              false, MPU_SH_NONE, 7, true);

  uint8_t attr = armv8m_mpu_get_attributes(&mpu, 0x20000100, true);
  CHECK_EQUAL(0xDD, attr);
}

TEST(MemoryAttributes, GetAttributesNoMatchingRegion) {
  armv8m_mpu_enable(&mpu, true, false, false);
  uint8_t attr = armv8m_mpu_get_attributes(&mpu, 0x30000000, true);
  CHECK_EQUAL(0, attr);
}

TEST(MemoryAttributes, GetAttributesWithPrivdefena) {
  armv8m_mpu_enable(&mpu, true, false, true);
  uint8_t attr = armv8m_mpu_get_attributes(&mpu, 0x30000000, true);
  CHECK_EQUAL(0, attr); // Default attributes
}

/*============================================================================
 * Region lookup and permission helpers (used by the TT instruction)
 *============================================================================*/

TEST_GROUP(RegionHelpers) {
  MPU mpu;

  void setup() {
    armv8m_mpu_init(&mpu, 8);
    armv8m_mpu_configure_region(&mpu, 2, 0x20000000, 0x2000FFFF, MPU_AP_RO_ALL,
                                false, MPU_SH_NONE, 0, true);
    armv8m_mpu_enable(&mpu, true, false, false);
  }

  void teardown() {}
};

TEST(RegionHelpers, RegionForAddrMatch) {
  CHECK_EQUAL(2, armv8m_mpu_region_for_addr(&mpu, 0x20001000));
}

TEST(RegionHelpers, RegionForAddrNoMatch) {
  CHECK_EQUAL(-1, armv8m_mpu_region_for_addr(&mpu, 0x30000000));
}

TEST(RegionHelpers, AccessBitsReadOnly) {
  bool r = false;
  bool rw = true;
  armv8m_mpu_access_bits(&mpu, 2, true, &r, &rw);
  CHECK_TRUE(r);
  CHECK_FALSE(rw); /* RO_ALL -> read only */
}

TEST(RegionHelpers, AccessBitsReadWrite) {
  armv8m_mpu_configure_region(&mpu, 3, 0x40000000, 0x4000FFFF, MPU_AP_RW_ALL,
                              false, MPU_SH_NONE, 0, true);
  bool r = false;
  bool rw = false;
  armv8m_mpu_access_bits(&mpu, 3, false, &r, &rw);
  CHECK_TRUE(r);
  CHECK_TRUE(rw);
}

TEST(RegionHelpers, AccessBitsPrivilegedOnlyDeniesUnprivileged) {
  armv8m_mpu_configure_region(&mpu, 4, 0x50000000, 0x5000FFFF, MPU_AP_RW_PRIV,
                              false, MPU_SH_NONE, 0, true);
  bool r = true;
  bool rw = true;
  armv8m_mpu_access_bits(&mpu, 4, false, &r, &rw); /* unprivileged */
  CHECK_FALSE(r);
  CHECK_FALSE(rw);
}

TEST(RegionHelpers, AccessBitsInvalidRegion) {
  bool r = true;
  bool rw = true;
  armv8m_mpu_access_bits(&mpu, -1, true, &r, &rw);
  CHECK_FALSE(r);
  CHECK_FALSE(rw);
}

/*============================================================================
 * Test Runner
 *============================================================================*/

int main(int argc, char **argv) {
  return CommandLineTestRunner::RunAllTests(argc, argv);
}
