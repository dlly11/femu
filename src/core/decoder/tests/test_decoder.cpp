/**
 * @file test_decoder.cpp
 * @brief CppUTest tests for the ARMv8-M instruction decoder
 *
 * Test patterns:
 * - Each test group covers one instruction category
 * - Test both valid encodings and error cases
 * - Use the test vectors from src/core/decoder/README.md
 */

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

extern "C" {
#include "armv8m_decoder.h"
#include "armv8m_types.h"
}

/*============================================================================
 * Test Group: Decoder Initialization
 *============================================================================*/

TEST_GROUP(DecoderInit)
{
    DecodedInsn insn;

    void setup()
    {
        armv8m_decode_init(&insn);
    }

    void teardown()
    {
    }
};

TEST(DecoderInit, InitSetsDefaultValues)
{
    CHECK_EQUAL(INSN_UNDEFINED, insn.type);
    CHECK_EQUAL(ARMV8M_REG_NONE, insn.rd);
    CHECK_EQUAL(ARMV8M_REG_NONE, insn.rn);
    CHECK_EQUAL(ARMV8M_REG_NONE, insn.rm);
    CHECK_EQUAL(COND_AL, insn.cond);
    CHECK_FALSE(insn.set_flags);
    CHECK_FALSE(insn.is_32bit);
}

/*============================================================================
 * Test Group: 16-bit Data Processing
 *============================================================================*/

TEST_GROUP(Thumb16DataProc)
{
    DecodedInsn insn;

    void setup()
    {
        armv8m_decode_init(&insn);
    }

    void teardown()
    {
    }
};

TEST(Thumb16DataProc, MovImmediate)
{
    // MOVS R0, #42  encoded as 0x202A (little-endian: 0x2A, 0x20)
    uint8_t code[] = {0x2A, 0x20};

    int result = armv8m_decode(code, 0x08000000, &insn);

    CHECK_EQUAL(2, result);  // 2 bytes consumed
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_MOV, insn.op);
    CHECK_EQUAL(0, insn.rd);
    CHECK_EQUAL(42, insn.imm);
    CHECK_TRUE(insn.set_flags);
    CHECK_FALSE(insn.is_32bit);
}

TEST(Thumb16DataProc, AddRegisters)
{
    // ADDS R0, R1, R2  encoded as 0x1888
    uint8_t code[] = {0x88, 0x18};

    int result = armv8m_decode(code, 0x08000000, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_REG, insn.type);
    CHECK_EQUAL(DP_ADD, insn.op);
    CHECK_EQUAL(0, insn.rd);
    CHECK_EQUAL(1, insn.rn);
    CHECK_EQUAL(2, insn.rm);
    CHECK_TRUE(insn.set_flags);
}

/*============================================================================
 * Test Group: 16-bit Load/Store
 *============================================================================*/

TEST_GROUP(Thumb16LoadStore)
{
    DecodedInsn insn;

    void setup()
    {
        armv8m_decode_init(&insn);
    }

    void teardown()
    {
    }
};

TEST(Thumb16LoadStore, LdrImmediate)
{
    // LDR R0, [R1, #4]  encoded as 0x6848
    uint8_t code[] = {0x48, 0x68};

    int result = armv8m_decode(code, 0x08000000, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_LOAD_IMM, insn.type);
    CHECK_EQUAL(0, insn.rt);
    CHECK_EQUAL(1, insn.rn);
    CHECK_EQUAL(4, insn.imm);
    CHECK_EQUAL(ACCESS_WORD, insn.access_size);
}

/*============================================================================
 * Test Group: 16-bit Branches
 *============================================================================*/

TEST_GROUP(Thumb16Branch)
{
    DecodedInsn insn;

    void setup()
    {
        armv8m_decode_init(&insn);
    }

    void teardown()
    {
    }
};

TEST(Thumb16Branch, UnconditionalBranch)
{
    // B.N +10  encoded as 0xE004 (offset = 4*2 = 8, plus PC+4 alignment)
    uint8_t code[] = {0x04, 0xE0};

    int result = armv8m_decode(code, 0x08000000, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_BRANCH, insn.type);
    CHECK_EQUAL(COND_AL, insn.cond);
    // Branch offset calculation: imm11 << 1
}

/*============================================================================
 * Test Group: 32-bit Instructions
 *============================================================================*/

TEST_GROUP(Thumb32)
{
    DecodedInsn insn;

    void setup()
    {
        armv8m_decode_init(&insn);
    }

    void teardown()
    {
    }
};

TEST(Thumb32, BranchWithLink)
{
    // BL <somewhere>  - 32-bit encoding starting with 0xF000
    uint8_t code[] = {0x00, 0xF0, 0x01, 0xF8};

    int result = armv8m_decode(code, 0x08000000, &insn);

    CHECK_EQUAL(4, result);  // 4 bytes consumed
    CHECK_EQUAL(INSN_BRANCH_LINK, insn.type);
    CHECK_TRUE(insn.is_32bit);
    CHECK_TRUE(insn.link);
}

/*============================================================================
 * Test Group: Error Cases
 *============================================================================*/

TEST_GROUP(DecoderErrors)
{
    DecodedInsn insn;

    void setup()
    {
        armv8m_decode_init(&insn);
    }

    void teardown()
    {
    }
};

TEST(DecoderErrors, UndefinedInstruction)
{
    // 0xDExx is permanently undefined
    uint8_t code[] = {0x00, 0xDE};

    int result = armv8m_decode(code, 0x08000000, &insn);

    CHECK_EQUAL(ARMV8M_ERR_UNDEFINED_INSN, result);
}

/*============================================================================
 * Test Runner
 *============================================================================*/

int main(int argc, char** argv)
{
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
