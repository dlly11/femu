/**
 * @file test_arithmetic.cpp
 * @brief Multiply, divide, extend, bitfield, and saturation tests
 */

#include "test_common.h"

TEST_GROUP(MulDiv)
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

TEST(MulDiv, Multiply)
{
    exec.cpu.r[0] = 12;
    exec.cpu.r[1] = 7;
    insn.type = INSN_MULTIPLY;
    insn.op = MUL_MUL;
    insn.rd = 2;
    insn.rn = 0;
    insn.rm = 1;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(84u, exec.cpu.r[2]);
}

TEST(MulDiv, UnsignedDivide)
{
    exec.cpu.r[0] = 100;
    exec.cpu.r[1] = 7;
    insn.type = INSN_DIVIDE;
    insn.rd = 2;
    insn.rn = 0;
    insn.rm = 1;
    insn.is_signed = false;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(14u, exec.cpu.r[2]);
}

TEST(MulDiv, SignedDivide)
{
    exec.cpu.r[0] = (uint32_t)-100;
    exec.cpu.r[1] = 7;
    insn.type = INSN_DIVIDE;
    insn.rd = 2;
    insn.rn = 0;
    insn.rm = 1;
    insn.is_signed = true;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL((uint32_t)-14, exec.cpu.r[2]);
}

TEST(MulDiv, DivideByZero)
{
    exec.cpu.r[0] = 100;
    exec.cpu.r[1] = 0;
    insn.type = INSN_DIVIDE;
    insn.rd = 2;
    insn.rn = 0;
    insn.rm = 1;
    insn.is_signed = false;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0u, exec.cpu.r[2]);
}

/*============================================================================
 * Test Group: Extend Instructions
 *============================================================================*/

TEST_GROUP(Extend)
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

TEST(Extend, Sxtb)
{
    exec.cpu.r[0] = 0x000000FF;
    insn.type = INSN_EXTEND;
    insn.rd = 1;
    insn.rm = 0;
    insn.access_size = ACCESS_BYTE;
    insn.is_signed = true;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0xFFFFFFFFu, exec.cpu.r[1]);
}

TEST(Extend, Uxtb)
{
    exec.cpu.r[0] = 0x12345678;
    insn.type = INSN_EXTEND;
    insn.rd = 1;
    insn.rm = 0;
    insn.access_size = ACCESS_BYTE;
    insn.is_signed = false;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x78u, exec.cpu.r[1]);
}

TEST(Extend, Sxth)
{
    exec.cpu.r[0] = 0x0000FFFF;
    insn.type = INSN_EXTEND;
    insn.rd = 1;
    insn.rm = 0;
    insn.access_size = ACCESS_HALF;
    insn.is_signed = true;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0xFFFFFFFFu, exec.cpu.r[1]);
}

TEST(Extend, Uxth)
{
    exec.cpu.r[0] = 0x12345678;
    insn.type = INSN_EXTEND;
    insn.rd = 1;
    insn.rm = 0;
    insn.access_size = ACCESS_HALF;
    insn.is_signed = false;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x5678u, exec.cpu.r[1]);
}

TEST(Extend, SxtbWithRotation)
{
    exec.cpu.r[0] = 0xFF000000;
    insn.type = INSN_EXTEND;
    insn.rd = 1;
    insn.rm = 0;
    insn.access_size = ACCESS_BYTE;
    insn.is_signed = true;
    insn.shift_amount = 24;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0xFFFFFFFFu, exec.cpu.r[1]);
}

/*============================================================================
 * Test Group: Load Instructions
 *============================================================================*/

TEST_GROUP(Bitfield)
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

TEST(Bitfield, Bfc)
{
    /* BFC: clear bits [lsb+width-1:lsb] in Rd
     * lsb = 4, width = 8 -> clear bits 4-11 */
    exec.cpu.r[0] = 0xFFFFFFFF;
    insn.type = INSN_BITFIELD;
    insn.op = DP_BIC;
    insn.rd = 0;
    insn.rn = 15;  /* BFC uses rn=15 */
    insn.imm = (8 << 8) | 4;  /* Decoder packs: imm = (width << 8) | lsb */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0xFFFFF00Fu, exec.cpu.r[0]);
}

TEST(Bitfield, Bfi)
{
    /* BFI: insert Rn[width-1:0] into Rd[lsb+width-1:lsb]
     * lsb = 8, width = 4 -> insert 4 bits starting at bit 8 */
    exec.cpu.r[0] = 0xFFFF0000;  /* Destination */
    exec.cpu.r[1] = 0x0000000A;  /* Source: 0xA */
    insn.type = INSN_BITFIELD;
    insn.op = DP_ORR;
    insn.rd = 0;
    insn.rn = 1;
    insn.imm = (4 << 8) | 8;  /* width=4, lsb=8 */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0xFFFF0A00u, exec.cpu.r[0]);
}

TEST(Bitfield, Ubfx)
{
    /* UBFX: extract Rn[lsb+width-1:lsb] to Rd, zero-extended
     * lsb = 8, width = 8 -> extract bits 8-15 */
    exec.cpu.r[1] = 0xABCD1234;
    insn.type = INSN_BITFIELD;
    insn.op = DP_MOV;
    insn.rd = 0;
    insn.rn = 1;
    insn.is_signed = false;
    insn.imm = (8 << 8) | 8;  /* width=8, lsb=8 */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x12u, exec.cpu.r[0]);
}

TEST(Bitfield, Sbfx)
{
    /* SBFX: extract Rn[lsb+width-1:lsb] to Rd, sign-extended
     * lsb = 8, width = 8 -> extract bits 8-15 = 0xFF, sign extend */
    exec.cpu.r[1] = 0x0000FF00;
    insn.type = INSN_BITFIELD;
    insn.op = DP_MOV;
    insn.rd = 0;
    insn.rn = 1;
    insn.is_signed = true;
    insn.imm = (8 << 8) | 8;  /* width=8, lsb=8 */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0xFFFFFFFFu, exec.cpu.r[0]);
}

/*============================================================================
 * Test Group: Additional System Register Coverage
 *============================================================================*/

TEST_GROUP(Saturate)
{
    Executor exec;
    DecodedInsn insn;

    void setup()
    {
        armv8m_exec_init(&exec);
        exec.mem.read = mock_mem_read;
        exec.mem.write = mock_mem_write;
        exec.mem.get_ptr = mock_mem_get_ptr;
        memset(mock_memory, 0, sizeof(mock_memory));
        init_insn(insn);
        mock().clear();
    }

    void teardown()
    {
        mock().checkExpectations();
        mock().clear();
    }
};

TEST(Saturate, SsatNoSaturation)
{
    /* SSAT R0, #16, R1 - value within 16-bit signed range */
    insn.type = INSN_SATURATE;
    insn.rd = 0;
    insn.rn = 1;
    insn.imm = 15;  /* width-1 = 15, so width = 16 */
    insn.is_signed = true;
    insn.shift_type = SHIFT_LSL;
    insn.shift_amount = 0;

    exec.cpu.r[1] = 1000;  /* Within [-32768, 32767] */
    exec.cpu.xpsr = ARMV8M_XPSR_T;  /* Clear Q flag */

    int result = armv8m_exec_insn(&exec, &insn);
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(1000u, exec.cpu.r[0]);
    CHECK_FALSE(exec.cpu.xpsr & ARMV8M_XPSR_Q);  /* Q not set */
}

TEST(Saturate, SsatPositiveSaturation)
{
    /* SSAT R0, #8, R1 - value exceeds 8-bit signed max */
    insn.type = INSN_SATURATE;
    insn.rd = 0;
    insn.rn = 1;
    insn.imm = 7;  /* width = 8 */
    insn.is_signed = true;
    insn.shift_type = SHIFT_LSL;
    insn.shift_amount = 0;

    exec.cpu.r[1] = 500;  /* Exceeds 127 */
    exec.cpu.xpsr = ARMV8M_XPSR_T;

    int result = armv8m_exec_insn(&exec, &insn);
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(127u, exec.cpu.r[0]);  /* Saturated to max 8-bit signed */
    CHECK(exec.cpu.xpsr & ARMV8M_XPSR_Q);  /* Q set */
}

TEST(Saturate, SsatNegativeSaturation)
{
    /* SSAT R0, #8, R1 - value below 8-bit signed min */
    insn.type = INSN_SATURATE;
    insn.rd = 0;
    insn.rn = 1;
    insn.imm = 7;  /* width = 8 */
    insn.is_signed = true;
    insn.shift_type = SHIFT_LSL;
    insn.shift_amount = 0;

    exec.cpu.r[1] = (uint32_t)-500;  /* Below -128 */
    exec.cpu.xpsr = ARMV8M_XPSR_T;

    int result = armv8m_exec_insn(&exec, &insn);
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL((uint32_t)-128, exec.cpu.r[0]);  /* Saturated to min 8-bit signed */
    CHECK(exec.cpu.xpsr & ARMV8M_XPSR_Q);
}

TEST(Saturate, SsatWithShiftLsl)
{
    /* SSAT R0, #8, R1, LSL #2 */
    insn.type = INSN_SATURATE;
    insn.rd = 0;
    insn.rn = 1;
    insn.imm = 7;  /* width = 8 */
    insn.is_signed = true;
    insn.shift_type = SHIFT_LSL;
    insn.shift_amount = 2;

    exec.cpu.r[1] = 100;  /* 100 << 2 = 400, exceeds 127 */
    exec.cpu.xpsr = ARMV8M_XPSR_T;

    int result = armv8m_exec_insn(&exec, &insn);
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(127u, exec.cpu.r[0]);  /* Saturated */
    CHECK(exec.cpu.xpsr & ARMV8M_XPSR_Q);
}

TEST(Saturate, SsatWithShiftAsr)
{
    /* SSAT R0, #8, R1, ASR #4 */
    insn.type = INSN_SATURATE;
    insn.rd = 0;
    insn.rn = 1;
    insn.imm = 7;  /* width = 8 */
    insn.is_signed = true;
    insn.shift_type = SHIFT_ASR;
    insn.shift_amount = 4;

    exec.cpu.r[1] = 0x800;  /* 2048 >> 4 = 128, exceeds 127 */
    exec.cpu.xpsr = ARMV8M_XPSR_T;

    int result = armv8m_exec_insn(&exec, &insn);
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(127u, exec.cpu.r[0]);  /* Saturated to 127 */
    CHECK(exec.cpu.xpsr & ARMV8M_XPSR_Q);
}

TEST(Saturate, UsatNoSaturation)
{
    /* USAT R0, #8, R1 - positive value within range */
    insn.type = INSN_SATURATE;
    insn.rd = 0;
    insn.rn = 1;
    insn.imm = 8;  /* sat_imm = 8, range [0, 255] */
    insn.is_signed = false;
    insn.shift_type = SHIFT_LSL;
    insn.shift_amount = 0;

    exec.cpu.r[1] = 200;  /* Within [0, 255] */
    exec.cpu.xpsr = ARMV8M_XPSR_T;

    int result = armv8m_exec_insn(&exec, &insn);
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(200u, exec.cpu.r[0]);
    CHECK_FALSE(exec.cpu.xpsr & ARMV8M_XPSR_Q);
}

TEST(Saturate, UsatPositiveSaturation)
{
    /* USAT R0, #8, R1 - value exceeds unsigned max */
    insn.type = INSN_SATURATE;
    insn.rd = 0;
    insn.rn = 1;
    insn.imm = 8;  /* sat_imm = 8, range [0, 255] */
    insn.is_signed = false;
    insn.shift_type = SHIFT_LSL;
    insn.shift_amount = 0;

    exec.cpu.r[1] = 500;  /* Exceeds 255 */
    exec.cpu.xpsr = ARMV8M_XPSR_T;

    int result = armv8m_exec_insn(&exec, &insn);
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(255u, exec.cpu.r[0]);  /* Saturated to max */
    CHECK(exec.cpu.xpsr & ARMV8M_XPSR_Q);
}

TEST(Saturate, UsatNegativeToZero)
{
    /* USAT R0, #8, R1 - negative value saturates to 0 */
    insn.type = INSN_SATURATE;
    insn.rd = 0;
    insn.rn = 1;
    insn.imm = 8;  /* sat_imm = 8, range [0, 255] */
    insn.is_signed = false;
    insn.shift_type = SHIFT_LSL;
    insn.shift_amount = 0;

    exec.cpu.r[1] = (uint32_t)-50;  /* Negative */
    exec.cpu.xpsr = ARMV8M_XPSR_T;

    int result = armv8m_exec_insn(&exec, &insn);
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0u, exec.cpu.r[0]);  /* Saturated to 0 */
    CHECK(exec.cpu.xpsr & ARMV8M_XPSR_Q);
}

TEST(Saturate, QFlagIsSticky)
{
    /* Q flag should remain set even if no saturation occurs */
    insn.type = INSN_SATURATE;
    insn.rd = 0;
    insn.rn = 1;
    insn.imm = 15;  /* width = 16 */
    insn.is_signed = true;
    insn.shift_type = SHIFT_LSL;
    insn.shift_amount = 0;

    exec.cpu.r[1] = 100;  /* Within range, no saturation */
    exec.cpu.xpsr = ARMV8M_XPSR_T | ARMV8M_XPSR_Q;  /* Q already set */

    int result = armv8m_exec_insn(&exec, &insn);
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(100u, exec.cpu.r[0]);
    CHECK(exec.cpu.xpsr & ARMV8M_XPSR_Q);  /* Q still set (sticky) */
}

/*============================================================================
 * Test Group: Exclusive Load/Store Instructions
 *============================================================================*/

