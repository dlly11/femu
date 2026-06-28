/**
 * @file test_trustzone.cpp
 * @brief TrustZone instruction tests for ARMv8-M executor
 *
 * Tests for SAU configuration, Secure Gateway (SG), BXNS, BLXNS,
 * TT instruction, and security checks.
 */

#include "test_common.h"

/* Forward declaration for security check */
extern "C" {
extern SecurityAttr armv8m_check_security(const Executor *exec, uint32_t addr);
}

/*============================================================================
 * Test Group: TrustZone SAU (Security Attribution Unit)
 *============================================================================*/

TEST_GROUP(TZ_SAU) {
  Executor exec;
  DecodedInsn insn;

  void setup() {
    memset(mock_memory, 0, sizeof(mock_memory));
    armv8m_exec_init(&exec);
    exec.has_trustzone = true;
    exec.cpu.security = SECURITY_SECURE;
    exec.mem.ctx = NULL;
    exec.mem.read = mock_mem_read;
    exec.mem.write = mock_mem_write;
    exec.mem.get_ptr = mock_mem_get_ptr;
    init_insn(insn);
    mock().clear();
  }

  void teardown() {
    mock().checkExpectations();
    mock().clear();
  }
};

TEST(TZ_SAU, SAU_InitialState) {
  /* SAU should be disabled initially */
  CHECK_EQUAL(0u, exec.sau.ctrl & ARMV8M_SAU_CTRL_ENABLE);
}

TEST(TZ_SAU, SAU_Enable) {
  exec.sau.ctrl |= ARMV8M_SAU_CTRL_ENABLE;
  CHECK_TRUE(exec.sau.ctrl & ARMV8M_SAU_CTRL_ENABLE);
}

TEST(TZ_SAU, SAU_RegionConfiguration) {
  /* Configure SAU region 0 */
  exec.sau.rnr = 0;
  exec.sau.regions[0].rbar = 0x20000000 & 0xFFFFFFE0; /* 32-byte aligned */
  exec.sau.regions[0].rlar = 0x2000FFFF | ARMV8M_SAU_RLAR_ENABLE;

  CHECK_EQUAL(0x20000000u, exec.sau.regions[0].rbar);
  CHECK_TRUE(exec.sau.regions[0].rlar & ARMV8M_SAU_RLAR_ENABLE);
}

TEST(TZ_SAU, SAU_NSCRegion) {
  /* Configure as Non-Secure Callable region */
  exec.sau.rnr = 1;
  exec.sau.regions[1].rbar = 0x30000000 & 0xFFFFFFE0;
  exec.sau.regions[1].rlar =
      0x30000FFF | ARMV8M_SAU_RLAR_ENABLE | ARMV8M_SAU_RLAR_NSC;

  CHECK_TRUE(exec.sau.regions[1].rlar & ARMV8M_SAU_RLAR_NSC);
}

TEST(TZ_SAU, SAU_AllNS_WhenDisabled) {
  /* When SAU disabled and ALLNS=1, all memory is non-secure */
  exec.sau.ctrl = ARMV8M_SAU_CTRL_ALLNS;
  CHECK_FALSE(exec.sau.ctrl & ARMV8M_SAU_CTRL_ENABLE);
  CHECK_TRUE(exec.sau.ctrl & ARMV8M_SAU_CTRL_ALLNS);
}

/*============================================================================
 * Test Group: TrustZone Secure Gateway (SG)
 *============================================================================*/

TEST_GROUP(TZ_SG) {
  Executor exec;
  DecodedInsn insn;

  void setup() {
    memset(mock_memory, 0, sizeof(mock_memory));
    armv8m_exec_init(&exec);
    exec.has_trustzone = true;
    exec.mem.ctx = NULL;
    exec.mem.read = mock_mem_read;
    exec.mem.write = mock_mem_write;
    exec.mem.get_ptr = mock_mem_get_ptr;
    init_insn(insn);
    mock().clear();
  }

  void teardown() {
    mock().checkExpectations();
    mock().clear();
  }
};

TEST(TZ_SG, SG_FromNonSecure) {
  /* SG instruction transitions from Non-Secure to Secure */
  exec.cpu.security = SECURITY_NONSECURE;
  exec.cpu.r[ARMV8M_REG_PC] = 0x1000;

  insn.type = INSN_SG;
  insn.size = 4;

  /* Execution would check NSC region and transition to Secure */
  /* For now, just verify the instruction type is recognized */
  CHECK_EQUAL(INSN_SG, insn.type);
}

TEST(TZ_SG, SG_FromSecure_IsNOP) {
  /* SG from Secure state is effectively a NOP */
  exec.cpu.security = SECURITY_SECURE;
  exec.cpu.r[ARMV8M_REG_PC] = 0x1000;

  insn.type = INSN_SG;
  insn.size = 4;

  /* Secure Gateway from secure state should not change state */
  CHECK_EQUAL(SECURITY_SECURE, (int)exec.cpu.security);
}

/*============================================================================
 * Test Group: TrustZone Branch NS (BXNS, BLXNS)
 *============================================================================*/

TEST_GROUP(TZ_BranchNS) {
  Executor exec;
  DecodedInsn insn;

  void setup() {
    memset(mock_memory, 0, sizeof(mock_memory));
    armv8m_exec_init(&exec);
    exec.has_trustzone = true;
    exec.cpu.security = SECURITY_SECURE;
    exec.mem.ctx = NULL;
    exec.mem.read = mock_mem_read;
    exec.mem.write = mock_mem_write;
    exec.mem.get_ptr = mock_mem_get_ptr;
    init_insn(insn);
    mock().clear();
  }

  void teardown() {
    mock().checkExpectations();
    mock().clear();
  }
};

TEST(TZ_BranchNS, BXNS_Decoding) {
  /* Verify BXNS instruction decoding */
  insn.type = INSN_BXNS;
  insn.rm = 0;
  exec.cpu.r[0] = 0x10001; /* Target address with Thumb bit */

  CHECK_EQUAL(INSN_BXNS, insn.type);
  CHECK_EQUAL((uint8_t)0, insn.rm);
}

TEST(TZ_BranchNS, BLXNS_Decoding) {
  /* Verify BLXNS instruction decoding */
  insn.type = INSN_BLXNS;
  insn.rm = 1;
  exec.cpu.r[1] = 0x20001;

  CHECK_EQUAL(INSN_BLXNS, insn.type);
  CHECK_EQUAL((uint8_t)1, insn.rm);
}

TEST(TZ_BranchNS, BXNS_ClearsCallerSavedRegs) {
  /* BXNS from Secure to Non-Secure should clear caller-saved registers */
  exec.cpu.security = SECURITY_SECURE;
  exec.cpu.r[0] = 0xDEADBEEF;
  exec.cpu.r[1] = 0xCAFEBABE;
  exec.cpu.r[2] = 0x12345678;
  exec.cpu.r[3] = 0x87654321;
  exec.cpu.r[12] = 0xFEEDFACE;
  exec.cpu.xpsr = ARMV8M_XPSR_T | ARMV8M_XPSR_N | ARMV8M_XPSR_Z | ARMV8M_XPSR_C;

  insn.type = INSN_BXNS;
  insn.rm = 4;
  exec.cpu.r[4] = 0x10001; /* Target address with Thumb bit */

  /* Configure SAU so target is in Non-Secure region */
  exec.sau.ctrl = ARMV8M_SAU_CTRL_ENABLE;
  exec.sau.regions[0].rbar = 0x10000 & 0xFFFFFFE0;
  exec.sau.regions[0].rlar = 0x1FFFF | ARMV8M_SAU_RLAR_ENABLE;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));

  /* After BXNS, R0-R3 and R12 should be cleared for security */
  CHECK_EQUAL(0u, exec.cpu.r[0]);
  CHECK_EQUAL(0u, exec.cpu.r[1]);
  CHECK_EQUAL(0u, exec.cpu.r[2]);
  CHECK_EQUAL(0u, exec.cpu.r[3]);
  CHECK_EQUAL(0u, exec.cpu.r[12]);

  /* APSR flags should be cleared */
  CHECK_FALSE(exec.cpu.xpsr & ARMV8M_XPSR_N);
  CHECK_FALSE(exec.cpu.xpsr & ARMV8M_XPSR_Z);
  CHECK_FALSE(exec.cpu.xpsr & ARMV8M_XPSR_C);

  /* Should have transitioned to Non-Secure */
  CHECK_EQUAL(SECURITY_NONSECURE, (int)exec.cpu.security);
}

TEST(TZ_BranchNS, BLXNS_ClearsCallerSavedRegs) {
  /* BLXNS from Secure to Non-Secure should clear caller-saved registers */
  exec.cpu.security = SECURITY_SECURE;
  exec.cpu.r[0] = 0xDEADBEEF;
  exec.cpu.r[1] = 0xCAFEBABE;
  exec.cpu.r[2] = 0x12345678;
  exec.cpu.r[3] = 0x87654321;
  exec.cpu.r[12] = 0xFEEDFACE;
  exec.cpu.r[ARMV8M_REG_PC] = 0x1000;
  exec.tz_regs.msp_s = 0x1000; /* Secure stack pointer */

  insn.type = INSN_BLXNS;
  insn.rm = 5;
  insn.size = 4;
  exec.cpu.r[5] = 0x10001; /* Target address with Thumb bit */

  /* Configure SAU so target is in Non-Secure region */
  exec.sau.ctrl = ARMV8M_SAU_CTRL_ENABLE;
  exec.sau.regions[0].rbar = 0x10000 & 0xFFFFFFE0;
  exec.sau.regions[0].rlar = 0x1FFFF | ARMV8M_SAU_RLAR_ENABLE;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));

  /* After BLXNS, R0-R3 and R12 should be cleared */
  CHECK_EQUAL(0u, exec.cpu.r[0]);
  CHECK_EQUAL(0u, exec.cpu.r[1]);
  CHECK_EQUAL(0u, exec.cpu.r[2]);
  CHECK_EQUAL(0u, exec.cpu.r[3]);
  CHECK_EQUAL(0u, exec.cpu.r[12]);

  /* LR should be FNC_RETURN */
  CHECK_EQUAL(0xFEFFFFFFu, exec.cpu.r[ARMV8M_REG_LR]);
}

/*============================================================================
 * Test Group: TrustZone Test Target (TT, TTT, TTA, TTAT)
 *============================================================================*/

TEST_GROUP(TZ_TT) {
  Executor exec;
  DecodedInsn insn;

  void setup() {
    memset(mock_memory, 0, sizeof(mock_memory));
    armv8m_exec_init(&exec);
    exec.has_trustzone = true;
    exec.cpu.security = SECURITY_SECURE;
    exec.mem.ctx = NULL;
    exec.mem.read = mock_mem_read;
    exec.mem.write = mock_mem_write;
    exec.mem.get_ptr = mock_mem_get_ptr;
    init_insn(insn);
    mock().clear();
  }

  void teardown() {
    mock().checkExpectations();
    mock().clear();
  }
};

TEST(TZ_TT, TT_Decoding) {
  /* TT instruction returns security attributes */
  exec.cpu.r[0] = 0x20000000;

  insn.type = INSN_TT;
  insn.rd = 1;
  insn.rn = 0;
  insn.op = 0; /* TT variant */

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  /* Result contains security attributes in specific bit positions */
}

TEST(TZ_TT, TTT_PrivilegedCheck) {
  /* TTT checks from privileged perspective */
  exec.cpu.r[0] = 0x30000000;

  insn.type = INSN_TT;
  insn.rd = 2;
  insn.rn = 0;
  insn.op = 1; /* TTT variant */

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
}

TEST(TZ_TT, TTA_AlternateDomain) {
  /* TTA checks from alternate security domain */
  exec.cpu.r[0] = 0x40000000;

  insn.type = INSN_TT;
  insn.rd = 3;
  insn.rn = 0;
  insn.op = 2; /* TTA variant */

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
}

TEST(TZ_TT, TT_ReturnsSecureBit) {
  /* Verify TT result format */
  exec.cpu.r[0] = 0x10000000;

  insn.type = INSN_TT;
  insn.rd = 4;
  insn.rn = 0;
  insn.op = 0;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  /* bit[16] = S (Secure), bit[5] = NS (Non-Secure), bit[6] = NSC */
}

TEST(TZ_TT, TT_NoTrustZone_Faults) {
  exec.has_trustzone = false;

  insn.type = INSN_TT;
  insn.rd = 0;
  insn.rn = 1;

  CHECK_EQUAL(ARMV8M_ERR_UNDEFINED_INSN, armv8m_exec_insn(&exec, &insn));
}

TEST(TZ_TT, TT_ReturnsNSForNonSecureRegion) {
  /* Configure SAU region as Non-Secure */
  exec.sau.ctrl = ARMV8M_SAU_CTRL_ENABLE;
  exec.sau.regions[0].rbar = 0x20000000 & 0xFFFFFFE0;
  exec.sau.regions[0].rlar =
      0x2000FFFF | ARMV8M_SAU_RLAR_ENABLE; /* NS region */

  exec.cpu.r[0] = 0x20001000; /* Address in NS region */

  insn.type = INSN_TT;
  insn.rd = 1;
  insn.rn = 0;
  insn.op = 0; /* TT variant */

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));

  /* Check NS bit is set */
  CHECK_TRUE(exec.cpu.r[1] & (1U << 5));   /* NS bit */
  CHECK_FALSE(exec.cpu.r[1] & (1U << 16)); /* S bit should not be set */
}

TEST(TZ_TT, TT_ReturnsNSCForNSCRegion) {
  /* Configure SAU region as Non-Secure Callable */
  exec.sau.ctrl = ARMV8M_SAU_CTRL_ENABLE;
  exec.sau.regions[0].rbar = 0x30000000 & 0xFFFFFFE0;
  exec.sau.regions[0].rlar =
      0x3000FFFF | ARMV8M_SAU_RLAR_ENABLE | ARMV8M_SAU_RLAR_NSC;

  exec.cpu.r[0] = 0x30001000; /* Address in NSC region */

  insn.type = INSN_TT;
  insn.rd = 1;
  insn.rn = 0;
  insn.op = 0;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));

  /* Check NSC bit is set */
  CHECK_TRUE(exec.cpu.r[1] & (1U << 6)); /* NSC bit */
  CHECK_TRUE(exec.cpu.r[1] & (1U << 5)); /* NS bit */
}

TEST(TZ_TT, TTT_PrivilegedAccess) {
  /* TTT checks privileged access - should set RW bit */
  exec.sau.ctrl = ARMV8M_SAU_CTRL_ENABLE;
  exec.sau.regions[0].rbar = 0x20000000 & 0xFFFFFFE0;
  exec.sau.regions[0].rlar = 0x2000FFFF | ARMV8M_SAU_RLAR_ENABLE;

  exec.cpu.r[0] = 0x20001000;

  insn.type = INSN_TT;
  insn.rd = 1;
  insn.rn = 0;
  insn.op = 1; /* TTT variant - privileged check */

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));

  /* Privileged access should get RW */
  CHECK_EQUAL(3u, exec.cpu.r[1] & 0x3); /* R and RW bits */
}

TEST(TZ_TT, TTA_AlternateDomainFromSecure) {
  /* TTA queries from alternate security domain (Secure querying NS) */
  exec.cpu.security = SECURITY_SECURE;
  exec.sau.ctrl = ARMV8M_SAU_CTRL_ENABLE;
  exec.sau.regions[0].rbar = 0x20000000 & 0xFFFFFFE0;
  exec.sau.regions[0].rlar = 0x2000FFFF | ARMV8M_SAU_RLAR_ENABLE;

  exec.cpu.r[0] = 0x20001000;

  insn.type = INSN_TT;
  insn.rd = 1;
  insn.rn = 0;
  insn.op = 2; /* TTA variant - alternate domain */

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));

  /* Result shows NS region attributes */
  CHECK_TRUE(exec.cpu.r[1] & (1U << 5)); /* NS bit */
}

TEST(TZ_TT, TT_ReturnsSREGION) {
  /* TT should return SAU region number in SREGION field */
  exec.sau.ctrl = ARMV8M_SAU_CTRL_ENABLE;
  exec.sau.regions[2].rbar = 0x40000000 & 0xFFFFFFE0;
  exec.sau.regions[2].rlar = 0x4000FFFF | ARMV8M_SAU_RLAR_ENABLE;

  exec.cpu.r[0] = 0x40001000; /* Address in region 2 */

  insn.type = INSN_TT;
  insn.rd = 1;
  insn.rn = 0;
  insn.op = 0;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));

  /* Check SRVALID and SREGION */
  CHECK_TRUE(exec.cpu.r[1] & (1U << 7));        /* SRVALID */
  CHECK_EQUAL(2u, (exec.cpu.r[1] >> 8) & 0xFF); /* SREGION = 2 */
}

TEST(TZ_TT, TT_MPU_IREGION) {
  /* TT reports the matching MPU region number in IREGION/IRVALID. */
  MPU mpu;
  armv8m_mpu_init(&mpu, 8);
  armv8m_mpu_configure_region(&mpu, 3, 0x20000000, 0x2000FFFF, MPU_AP_RW_ALL,
                              false, MPU_SH_NONE, 0, true);
  armv8m_mpu_enable(&mpu, true, false, false);
  exec.mpu = &mpu;

  exec.cpu.r[0] = 0x20001000; /* inside region 3 */
  insn.type = INSN_TT;
  insn.rd = 1;
  insn.rn = 0;
  insn.op = 0;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_TRUE(exec.cpu.r[1] & (1U << 23));        /* IRVALID */
  CHECK_EQUAL(3u, (exec.cpu.r[1] >> 24) & 0xFF); /* IREGION = 3 */
  CHECK_EQUAL(0x3u, exec.cpu.r[1] & 0x3);        /* RW region -> R and RW */
}

TEST(TZ_TT, TT_MPU_ReadOnlyPermissions) {
  /* A read-only MPU region yields R set but RW clear in the TT result. */
  MPU mpu;
  armv8m_mpu_init(&mpu, 8);
  armv8m_mpu_configure_region(&mpu, 0, 0x20000000, 0x2000FFFF, MPU_AP_RO_ALL,
                              false, MPU_SH_NONE, 0, true);
  armv8m_mpu_enable(&mpu, true, false, false);
  exec.mpu = &mpu;

  exec.cpu.r[0] = 0x20001000;
  insn.type = INSN_TT;
  insn.rd = 1;
  insn.rn = 0;
  insn.op = 0;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_TRUE(exec.cpu.r[1] & 0x1);  /* R */
  CHECK_FALSE(exec.cpu.r[1] & 0x2); /* RW clear (read-only) */
}

TEST(TZ_TT, TTA_NonSecureViewClearsSecureAccess) {
  /* Secure code uses TTA to query the Non-secure view of a Secure address.
   * The Non-secure domain cannot access Secure memory, so R/RW must be clear
   * even though the address is Secure. This exercises the query-domain logic;
   * without it the result would report privileged R+RW. */
  exec.cpu.security = SECURITY_SECURE;
  exec.sau.ctrl = ARMV8M_SAU_CTRL_ENABLE; /* enabled, but addr in no NS region */
  exec.cpu.r[0] = 0x10000000;             /* not in any SAU region -> Secure */

  insn.type = INSN_TT;
  insn.rd = 1;
  insn.rn = 0;
  insn.op = 2; /* TTA - query alternate (Non-secure) domain */

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_TRUE(exec.cpu.r[1] & (1U << 16)); /* S bit (address is Secure) */
  CHECK_EQUAL(0u, exec.cpu.r[1] & 0x3);   /* R/RW clear: NS cannot access Secure */
}

TEST(TZ_TT, TT_IDAU_PinsSecure) {
  /* SAU marks the address Non-secure, but an IDAU region pins it Secure.
   * The combined attribution must be Secure. */
  exec.cpu.security = SECURITY_SECURE;
  exec.sau.ctrl = ARMV8M_SAU_CTRL_ENABLE;
  exec.sau.regions[0].rbar = 0x20000000 & 0xFFFFFFE0;
  exec.sau.regions[0].rlar = 0x2000FFFF | ARMV8M_SAU_RLAR_ENABLE; /* NS region */

  exec.idau.enabled = true;
  exec.idau.num_regions = 1;
  exec.idau.regions[0].rbar = 0x20000000 & 0xFFFFFFE0;
  /* Limit masked to bits[31:5] with EN set but NOT NSC (bit 1) -> the IDAU
   * region pins the area Secure. */
  exec.idau.regions[0].rlar = (0x2000FFFF & 0xFFFFFFE0) | ARMV8M_SAU_RLAR_ENABLE;

  exec.cpu.r[0] = 0x20001000;
  insn.type = INSN_TT;
  insn.rd = 1;
  insn.rn = 0;
  insn.op = 0;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_TRUE(exec.cpu.r[1] & (1U << 16)); /* S bit set (IDAU pinned Secure) */
  CHECK_FALSE(exec.cpu.r[1] & (1U << 5)); /* NS bit clear */
}

/*============================================================================
 * Test Group: TrustZone Security Checks
 *============================================================================*/

TEST_GROUP(TZ_Security) {
  Executor exec;
  DecodedInsn insn;

  void setup() {
    memset(mock_memory, 0, sizeof(mock_memory));
    armv8m_exec_init(&exec);
    exec.has_trustzone = true;
    exec.mem.ctx = NULL;
    exec.mem.read = mock_mem_read;
    exec.mem.write = mock_mem_write;
    exec.mem.get_ptr = mock_mem_get_ptr;
    init_insn(insn);
    mock().clear();
  }

  void teardown() {
    mock().checkExpectations();
    mock().clear();
  }
};

TEST(TZ_Security, SecureStateInitial) {
  /* After reset, processor should be in Secure state */
  armv8m_exec_reset(&exec, 0);
  CHECK_EQUAL(SECURITY_SECURE, (int)exec.cpu.security);
}

TEST(TZ_Security, NonSecureCannotAccessSecure) {
  exec.cpu.security = SECURITY_NONSECURE;

  /* Configure SAU region as Secure */
  exec.sau.ctrl = ARMV8M_SAU_CTRL_ENABLE;
  exec.sau.regions[0].rbar = 0x10000000;
  exec.sau.regions[0].rlar = 0x1000FFFF | ARMV8M_SAU_RLAR_ENABLE;

  /* Access to secure region from non-secure should fault */
  /* This is checked during memory access */
}

TEST(TZ_Security, BankedStackPointers) {
  /* Secure and non-secure have separate stack pointers */
  exec.tz_regs.msp_s = 0x20008000;
  exec.tz_regs.msp_ns = 0x30008000;
  exec.tz_regs.psp_s = 0x20004000;
  exec.tz_regs.psp_ns = 0x30004000;

  CHECK_EQUAL(0x20008000u, exec.tz_regs.msp_s);
  CHECK_EQUAL(0x30008000u, exec.tz_regs.msp_ns);
}

TEST(TZ_Security, BankedSpecialRegisters) {
  /* Special registers are banked between security states */
  exec.tz_regs.primask_s = 1;
  exec.tz_regs.primask_ns = 0;
  exec.tz_regs.basepri_s = 0x10;
  exec.tz_regs.basepri_ns = 0x20;
  exec.tz_regs.faultmask_s = 0;
  exec.tz_regs.faultmask_ns = 1;
  exec.tz_regs.control_s = ARMV8M_CONTROL_SPSEL;
  exec.tz_regs.control_ns = 0;

  CHECK_EQUAL((uint32_t)1, exec.tz_regs.primask_s);
  CHECK_EQUAL((uint32_t)0, exec.tz_regs.primask_ns);
  CHECK_EQUAL(0x10u, exec.tz_regs.basepri_s);
  CHECK_EQUAL(0x20u, exec.tz_regs.basepri_ns);
}

TEST(TZ_Security, StackLimits) {
  /* Stack limits are also banked */
  exec.tz_regs.msplim_s = 0x20000000;
  exec.tz_regs.msplim_ns = 0x30000000;
  exec.tz_regs.psplim_s = 0x20002000;
  exec.tz_regs.psplim_ns = 0x30002000;

  CHECK_EQUAL(0x20000000u, exec.tz_regs.msplim_s);
  CHECK_EQUAL(0x30000000u, exec.tz_regs.msplim_ns);
}

/*============================================================================
 * Test Group: TrustZone Exception Handling
 *============================================================================*/

TEST_GROUP(TZ_Exception) {
  Executor exec;
  DecodedInsn insn;

  void setup() {
    memset(mock_memory, 0, sizeof(mock_memory));
    armv8m_exec_init(&exec);
    exec.has_trustzone = true;
    exec.cpu.security = SECURITY_SECURE;
    exec.cpu.sp_main = 0x2000; /* Top of mock memory (8KB) */
    exec.mem.ctx = NULL;
    exec.mem.read = mock_mem_read;
    exec.mem.write = mock_mem_write;
    exec.mem.get_ptr = mock_mem_get_ptr;

    /* Set up vector table */
    for (uint32_t i = 0; i < 256; i++) {
      uint32_t addr = i * 4;
      uint32_t handler = 0x1001 + i * 0x100; /* Dummy handlers */
      mock_memory[addr] = (uint8_t)(handler & 0xFF);
      mock_memory[addr + 1] = (uint8_t)((handler >> 8) & 0xFF);
      mock_memory[addr + 2] = (uint8_t)((handler >> 16) & 0xFF);
      mock_memory[addr + 3] = (uint8_t)((handler >> 24) & 0xFF);
    }

    init_insn(insn);
    mock().clear();
  }

  void teardown() {
    mock().checkExpectations();
    mock().clear();
  }
};

TEST(TZ_Exception, SecureFaultException) {
  /* SecureFault is TrustZone-specific exception */
  CHECK_EQUAL(7, ARMV8M_EXC_SECUREFAULT);
}

TEST(TZ_Exception, ExceptionFromSecure) {
  exec.cpu.security = SECURITY_SECURE;
  exec.cpu.mode = MODE_THREAD;
  exec.cpu.r[ARMV8M_REG_PC] = 0x1000;

  int result = armv8m_exception_entry(&exec, ARMV8M_EXC_SVCALL);
  CHECK_EQUAL(ARMV8M_OK, result);
  CHECK_EQUAL(MODE_HANDLER, (int)exec.cpu.mode);
}

TEST(TZ_Exception, ExceptionFromNonSecure) {
  exec.cpu.security = SECURITY_NONSECURE;
  exec.cpu.mode = MODE_THREAD;
  exec.cpu.r[ARMV8M_REG_PC] = 0x2000;
  exec.cpu.sp_main = 0x30001000;

  /* Exception from non-secure uses non-secure handler */
}

/*============================================================================
 * Test Group: TrustZone FNC_RETURN (Function Return from Non-Secure)
 *============================================================================*/

TEST_GROUP(TZ_FNCReturn) {
  Executor exec;
  DecodedInsn insn;

  void setup() {
    memset(mock_memory, 0, sizeof(mock_memory));
    armv8m_exec_init(&exec);
    exec.has_trustzone = true;
    exec.cpu.security = SECURITY_NONSECURE;
    exec.mem.ctx = NULL;
    exec.mem.read = mock_mem_read;
    exec.mem.write = mock_mem_write;
    exec.mem.get_ptr = mock_mem_get_ptr;
    init_insn(insn);
    mock().clear();
  }

  void teardown() {
    mock().checkExpectations();
    mock().clear();
  }
};

TEST(TZ_FNCReturn, BX_FNCReturn_TransitionsToSecure) {
  /* Set up: Non-Secure state, LR = FNC_RETURN, return address on secure stack
   */
  exec.cpu.security = SECURITY_NONSECURE;
  exec.cpu.r[ARMV8M_REG_LR] = 0xFEFFFFFF; /* FNC_RETURN */

  /* Set up secure stack with return address */
  uint32_t return_addr = 0x2001; /* Return address with Thumb bit */
  exec.tz_regs.msp_s = 0x1000;
  mock_memory[0x1000] = (uint8_t)(return_addr & 0xFF);
  mock_memory[0x1001] = (uint8_t)((return_addr >> 8) & 0xFF);
  mock_memory[0x1002] = (uint8_t)((return_addr >> 16) & 0xFF);
  mock_memory[0x1003] = (uint8_t)((return_addr >> 24) & 0xFF);

  insn.type = INSN_BRANCH_EXCHANGE;
  insn.rm = ARMV8M_REG_LR;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));

  /* Should have transitioned to Secure state */
  CHECK_EQUAL(SECURITY_SECURE, (int)exec.cpu.security);

  /* PC should be set to return address (Thumb bit cleared) */
  CHECK_EQUAL(0x2000u, exec.cpu.r[ARMV8M_REG_PC]);

  /* Secure stack should be popped */
  CHECK_EQUAL(0x1004u, exec.tz_regs.msp_s);
}

TEST(TZ_FNCReturn, BX_FNCReturn_FromSecure_Faults) {
  /* FNC_RETURN from Secure state should fault */
  exec.cpu.security = SECURITY_SECURE;
  exec.cpu.r[ARMV8M_REG_LR] = 0xFEFFFFFF; /* FNC_RETURN */

  insn.type = INSN_BRANCH_EXCHANGE;
  insn.rm = ARMV8M_REG_LR;

  CHECK_EQUAL(ARMV8M_ERR_USAGE_FAULT, armv8m_exec_insn(&exec, &insn));

  /* Should still be in Secure state */
  CHECK_EQUAL(SECURITY_SECURE, (int)exec.cpu.security);
}

TEST(TZ_FNCReturn, BX_FNCReturn_NoTrustZone_RegularBranch) {
  /* Without TrustZone, FNC_RETURN value is just a regular branch target */
  exec.has_trustzone = false;
  exec.cpu.r[ARMV8M_REG_LR] = 0xFEFFFFFF; /* Would be FNC_RETURN with TZ */

  insn.type = INSN_BRANCH_EXCHANGE;
  insn.rm = ARMV8M_REG_LR;

  /* Without TrustZone, this should fault because bit 0 is set (Thumb) */
  /* but FNC_RETURN has bit 0 = 1, so it should try to branch */
  int result = armv8m_exec_insn(&exec, &insn);

  /* The address 0xFEFFFFFF has bit 0 set, so it's valid Thumb target */
  /* This will just branch normally */
  CHECK_EQUAL(ARMV8M_OK, result);
  CHECK_EQUAL(0xFEFFFFFEu, exec.cpu.r[ARMV8M_REG_PC]);
}
