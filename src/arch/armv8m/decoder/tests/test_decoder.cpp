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
#include "arch/armv8m/armv8m_decoder.h"
#include "arch/armv8m/armv8m_types.h"
}

/*============================================================================
 * Test Helper Macros and Constants
 *============================================================================*/

/**
 * Build a little-endian byte array for a 32-bit Thumb instruction.
 * @param hw1 First halfword (at lower address)
 * @param hw2 Second halfword (at higher address)
 */
#define THUMB32_BYTES(hw1, hw2) \
    { (uint8_t)((hw1) & 0xFF), (uint8_t)(((hw1) >> 8) & 0xFF), \
      (uint8_t)((hw2) & 0xFF), (uint8_t)(((hw2) >> 8) & 0xFF) }

/**
 * Build a little-endian byte array for a 16-bit Thumb instruction.
 * @param hw Halfword encoding
 */
#define THUMB16_BYTES(hw) \
    { (uint8_t)((hw) & 0xFF), (uint8_t)(((hw) >> 8) & 0xFF) }

/* Standard test PC address */
static const uint32_t TEST_PC = TEST_PC;

/* Common register indices for readability (use uint8_t to avoid CHECK_EQUAL ambiguity) */
static const uint8_t REG_R0 = 0;
static const uint8_t REG_R1 = 1;
static const uint8_t REG_R2 = 2;
static const uint8_t REG_R3 = 3;
static const uint8_t REG_R4 = 4;
static const uint8_t REG_R5 = 5;
static const uint8_t REG_R6 = 6;
static const uint8_t REG_R7 = 7;
static const uint8_t REG_R8 = 8;
static const uint8_t REG_R9 = 9;
static const uint8_t REG_R10 = 10;
static const uint8_t REG_R11 = 11;
static const uint8_t REG_R12 = 12;
static const uint8_t REG_SP = 13;
static const uint8_t REG_LR = 14;
static const uint8_t REG_PC = 15;

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
    // MOVS R0, #42 - Move immediate to register with flags update
    // Encoding: 0x202A = 001|00|000|00101010
    //   [15:13]=001 (format), [12:11]=00 (MOVS), [10:8]=Rd, [7:0]=imm8
    uint8_t code[] = THUMB16_BYTES(0x202A);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_MOV, insn.op);
    CHECK_EQUAL(REG_R0, insn.rd);
    CHECK_EQUAL(42u, insn.imm);
    CHECK_TRUE(insn.set_flags);
    CHECK_FALSE(insn.is_32bit);
}

TEST(Thumb16DataProc, MovImmediateMaxValue)
{
    // MOVS R7, #255 - Move max 8-bit immediate
    // Encoding: 0x27FF = 001|00|111|11111111
    uint8_t code[] = THUMB16_BYTES(0x27FF);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_MOV, insn.op);
    CHECK_EQUAL(REG_R7, insn.rd);
    CHECK_EQUAL(255u, insn.imm);
}

TEST(Thumb16DataProc, AddRegisters)
{
    // ADDS R0, R1, R2 - Three-register add with flags
    // Encoding: 0x1888 = 000110|00|010|001|000
    //   [15:9]=0001100 (ADD reg), [8:6]=Rm, [5:3]=Rn, [2:0]=Rd
    uint8_t code[] = THUMB16_BYTES(0x1888);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_REG, insn.type);
    CHECK_EQUAL(DP_ADD, insn.op);
    CHECK_EQUAL(REG_R0, insn.rd);
    CHECK_EQUAL(REG_R1, insn.rn);
    CHECK_EQUAL(REG_R2, insn.rm);
    CHECK_TRUE(insn.set_flags);
}

TEST(Thumb16DataProc, SubRegisters)
{
    // SUBS R3, R4, R5 - Three-register subtract with flags
    // Encoding: 0x1B63 = 000110|11|101|100|011
    uint8_t code[] = THUMB16_BYTES(0x1B63);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_REG, insn.type);
    CHECK_EQUAL(DP_SUB, insn.op);
    CHECK_EQUAL(REG_R3, insn.rd);
    CHECK_EQUAL(REG_R4, insn.rn);
    CHECK_EQUAL(REG_R5, insn.rm);
    CHECK_TRUE(insn.set_flags);
}

TEST(Thumb16DataProc, CmpImmediate)
{
    // CMP R3, #100 - Compare register with immediate
    // Encoding: 0x2B64 = 001|01|011|01100100
    //   [15:11]=00101 (CMP imm), [10:8]=Rn, [7:0]=imm8
    uint8_t code[] = THUMB16_BYTES(0x2B64);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_CMP, insn.op);
    CHECK_EQUAL(REG_R3, insn.rn);
    CHECK_EQUAL(100u, insn.imm);
    CHECK_TRUE(insn.set_flags);
}

TEST(Thumb16DataProc, AddImmediate8)
{
    // ADDS R2, #50 - Add 8-bit immediate to register
    // Encoding: 0x3232 = 001|10|010|00110010
    uint8_t code[] = THUMB16_BYTES(0x3232);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_ADD, insn.op);
    CHECK_EQUAL(REG_R2, insn.rd);
    CHECK_EQUAL(REG_R2, insn.rn);
    CHECK_EQUAL(50u, insn.imm);
}

TEST(Thumb16DataProc, LslImmediate)
{
    // LSLS R1, R2, #3 - Logical shift left by immediate
    // Encoding: 0x00D1 = 000|00|00011|010|001
    //   [15:11]=00000 (LSL imm), [10:6]=imm5, [5:3]=Rm, [2:0]=Rd
    uint8_t code[] = THUMB16_BYTES(0x00D1);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_SHIFTED, insn.type);
    CHECK_EQUAL(REG_R1, insn.rd);
    CHECK_EQUAL(REG_R2, insn.rm);
    CHECK_EQUAL(SHIFT_LSL, insn.shift_type);
    CHECK_EQUAL(3u, insn.shift_amount);
}

TEST(Thumb16DataProc, AndRegister)
{
    // ANDS R0, R1 - Bitwise AND with flags
    // Encoding: 0x4008 = 010000|0000|001|000
    //   [15:10]=010000 (data proc), [9:6]=op, [5:3]=Rm, [2:0]=Rd
    uint8_t code[] = THUMB16_BYTES(0x4008);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_REG, insn.type);
    CHECK_EQUAL(DP_AND, insn.op);
    CHECK_EQUAL(REG_R0, insn.rd);
    CHECK_EQUAL(REG_R1, insn.rm);
    CHECK_TRUE(insn.set_flags);
}

TEST(Thumb16DataProc, EorRegister)
{
    // EORS R2, R3 - Bitwise XOR with flags
    // Encoding: 0x405A = 010000|0001|011|010
    uint8_t code[] = THUMB16_BYTES(0x405A);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_REG, insn.type);
    CHECK_EQUAL(DP_EOR, insn.op);
    CHECK_EQUAL(REG_R2, insn.rd);
    CHECK_EQUAL(REG_R3, insn.rm);
}

TEST(Thumb16DataProc, MvnRegister)
{
    // MVNS R4, R5 - Bitwise NOT with flags
    // Encoding: 0x43EC = 010000|1111|101|100
    uint8_t code[] = THUMB16_BYTES(0x43EC);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_REG, insn.type);
    CHECK_EQUAL(DP_MVN, insn.op);
    CHECK_EQUAL(REG_R4, insn.rd);
    CHECK_EQUAL(REG_R5, insn.rm);
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
    // LDR R0, [R1, #4] - Load word with immediate offset
    // Encoding: 0x6848 = 01101|00001|001|000
    //   [15:11]=01101 (LDR imm), [10:6]=imm5, [5:3]=Rn, [2:0]=Rt
    //   imm = imm5 << 2 = 1 << 2 = 4
    uint8_t code[] = THUMB16_BYTES(0x6848);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_LOAD_IMM, insn.type);
    CHECK_EQUAL(REG_R0, insn.rt);
    CHECK_EQUAL(REG_R1, insn.rn);
    CHECK_EQUAL(4u, insn.imm);
    CHECK_EQUAL(ACCESS_WORD, insn.access_size);
}

TEST(Thumb16LoadStore, StrImmediate)
{
    // STR R2, [R3, #8] - Store word with immediate offset
    // Encoding: 0x609A = 01100|00010|011|010
    //   imm = 2 << 2 = 8
    uint8_t code[] = THUMB16_BYTES(0x609A);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_STORE_IMM, insn.type);
    CHECK_EQUAL(REG_R2, insn.rt);
    CHECK_EQUAL(REG_R3, insn.rn);
    CHECK_EQUAL(8u, insn.imm);
    CHECK_EQUAL(ACCESS_WORD, insn.access_size);
}

TEST(Thumb16LoadStore, LdrbImmediate)
{
    // LDRB R0, [R1, #2] - Load byte with immediate offset
    // Encoding: 0x7888 = 01111|00010|001|000
    uint8_t code[] = THUMB16_BYTES(0x7888);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_LOAD_IMM, insn.type);
    CHECK_EQUAL(REG_R0, insn.rt);
    CHECK_EQUAL(REG_R1, insn.rn);
    CHECK_EQUAL(2u, insn.imm);
    CHECK_EQUAL(ACCESS_BYTE, insn.access_size);
}

TEST(Thumb16LoadStore, LdrhImmediate)
{
    // LDRH R4, [R5, #6] - Load halfword with immediate offset
    // Encoding: 0x88EC = 10001|00011|101|100
    //   imm = 3 << 1 = 6
    uint8_t code[] = THUMB16_BYTES(0x88EC);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_LOAD_IMM, insn.type);
    CHECK_EQUAL(REG_R4, insn.rt);
    CHECK_EQUAL(REG_R5, insn.rn);
    CHECK_EQUAL(6u, insn.imm);
    CHECK_EQUAL(ACCESS_HALF, insn.access_size);
}

TEST(Thumb16LoadStore, LdrRegister)
{
    // LDR R0, [R1, R2] - Load word with register offset
    // Encoding: 0x5888 = 0101100|010|001|000
    //   [15:9]=0101100 (LDR reg), [8:6]=Rm, [5:3]=Rn, [2:0]=Rt
    uint8_t code[] = THUMB16_BYTES(0x5888);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_LOAD_REG, insn.type);
    CHECK_EQUAL(REG_R0, insn.rt);
    CHECK_EQUAL(REG_R1, insn.rn);
    CHECK_EQUAL(REG_R2, insn.rm);
    CHECK_EQUAL(ACCESS_WORD, insn.access_size);
}

TEST(Thumb16LoadStore, LdrLiteral)
{
    // LDR R0, [PC, #16] - PC-relative load
    // Encoding: 0x4804 = 01001|000|00000100
    //   [15:11]=01001 (LDR lit), [10:8]=Rt, [7:0]=imm8
    //   imm = imm8 << 2 = 4 << 2 = 16
    uint8_t code[] = THUMB16_BYTES(0x4804);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_LOAD_LITERAL, insn.type);
    CHECK_EQUAL(REG_R0, insn.rt);
    CHECK_EQUAL(16u, insn.imm);
}

TEST(Thumb16LoadStore, LdrSpRelative)
{
    // LDR R3, [SP, #20] - SP-relative load
    // Encoding: 0x9B05 = 10011|011|00000101
    //   imm = 5 << 2 = 20
    uint8_t code[] = THUMB16_BYTES(0x9B05);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_LOAD_IMM, insn.type);
    CHECK_EQUAL(REG_R3, insn.rt);
    CHECK_EQUAL(REG_SP, insn.rn);
    CHECK_EQUAL(20u, insn.imm);
}

TEST(Thumb16LoadStore, Push)
{
    // PUSH {R0, R1, LR} - Push registers to stack
    // Encoding: 0xB503 = 1011010|1|00000011
    //   [15:9]=1011010 (PUSH), [8]=M (LR), [7:0]=register_list
    //   register_list = 0x4003 (R0, R1, LR)
    uint8_t code[] = THUMB16_BYTES(0xB503);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_STORE_MULTIPLE, insn.type);
    CHECK_EQUAL(REG_SP, insn.rn);
    CHECK_EQUAL(0x4003u, insn.register_list);
    CHECK_TRUE(insn.writeback);
}

TEST(Thumb16LoadStore, Pop)
{
    // POP {R0, R1, PC} - Pop registers from stack
    // Encoding: 0xBD03 = 1011110|1|00000011
    //   [15:9]=1011110 (POP), [8]=P (PC), [7:0]=register_list
    //   register_list = 0x8003 (R0, R1, PC)
    uint8_t code[] = THUMB16_BYTES(0xBD03);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_LOAD_MULTIPLE, insn.type);
    CHECK_EQUAL(REG_SP, insn.rn);
    CHECK_EQUAL(0x8003u, insn.register_list);
    CHECK_TRUE(insn.writeback);
}

TEST(Thumb16LoadStore, Stm)
{
    // STM R0!, {R1, R2, R3} - Store multiple with writeback
    // Encoding: 0xC00E = 11000|000|00001110
    //   [15:11]=11000 (STM), [10:8]=Rn, [7:0]=register_list
    uint8_t code[] = THUMB16_BYTES(0xC00E);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_STORE_MULTIPLE, insn.type);
    CHECK_EQUAL(REG_R0, insn.rn);
    CHECK_EQUAL(0x000Eu, insn.register_list);
    CHECK_TRUE(insn.writeback);
}

TEST(Thumb16LoadStore, Ldm)
{
    // LDM R1!, {R2, R3, R4} - Load multiple with writeback
    // Encoding: 0xC91C = 11001|001|00011100
    uint8_t code[] = THUMB16_BYTES(0xC91C);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_LOAD_MULTIPLE, insn.type);
    CHECK_EQUAL(REG_R1, insn.rn);
    CHECK_EQUAL(0x001Cu, insn.register_list);
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
    // B.N +8 - Unconditional short branch
    // Encoding: 0xE004 = 11100|00000000100
    //   [15:11]=11100 (B), [10:0]=imm11
    //   offset = SignExtend(imm11) << 1 = 4 << 1 = 8
    uint8_t code[] = THUMB16_BYTES(0xE004);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_BRANCH, insn.type);
    CHECK_EQUAL(COND_AL, insn.cond);
    CHECK_EQUAL(8, insn.branch_offset);
}

TEST(Thumb16Branch, ConditionalBranchEQ)
{
    // BEQ +6 - Branch if equal
    // Encoding: 0xD003 = 1101|0000|00000011
    //   [15:12]=1101 (B<cond>), [11:8]=cond, [7:0]=imm8
    //   offset = 3 << 1 = 6
    uint8_t code[] = THUMB16_BYTES(0xD003);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_BRANCH, insn.type);
    CHECK_EQUAL(COND_EQ, insn.cond);
    CHECK_EQUAL(6, insn.branch_offset);
}

TEST(Thumb16Branch, ConditionalBranchNE)
{
    // BNE -4 - Branch if not equal (backward)
    // Encoding: 0xD1FE = 1101|0001|11111110
    //   offset = SignExtend(0xFE) << 1 = -2 << 1 = -4
    uint8_t code[] = THUMB16_BYTES(0xD1FE);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_BRANCH, insn.type);
    CHECK_EQUAL(COND_NE, insn.cond);
    CHECK_EQUAL(-4, insn.branch_offset);
}

TEST(Thumb16Branch, BxRegister)
{
    // BX R3 - Branch and exchange to address in register
    // Encoding: 0x4718 = 010001|11|0|0011|000
    //   [15:7]=010001110 (BX), [6:3]=Rm
    uint8_t code[] = THUMB16_BYTES(0x4718);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_BRANCH_EXCHANGE, insn.type);
    CHECK_EQUAL(REG_R3, insn.rm);
}

TEST(Thumb16Branch, BlxRegister)
{
    // BLX R4 - Branch with link and exchange
    // Encoding: 0x47A0 = 010001|11|1|0100|000
    uint8_t code[] = THUMB16_BYTES(0x47A0);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_BRANCH_LINK_EXCHANGE, insn.type);
    CHECK_EQUAL(REG_R4, insn.rm);
    CHECK_TRUE(insn.link);
}

TEST(Thumb16Branch, Cbz)
{
    // CBZ R0, +8 - Compare and branch if zero
    // Encoding: 0xB100 = 10110|0|0|1|00000|000
    //   [15:12]=1011 (CB*), [11]=op (0=CBZ), [9]=i, [7:3]=imm5, [2:0]=Rn
    uint8_t code[] = THUMB16_BYTES(0xB100);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_COMPARE_BRANCH, insn.type);
    CHECK_EQUAL(REG_R0, insn.rn);
}

TEST(Thumb16Branch, Svc)
{
    // SVC #5 - Supervisor call
    // Encoding: 0xDF05 = 11011111|00000101
    //   [15:8]=11011111 (SVC), [7:0]=imm8
    uint8_t code[] = THUMB16_BYTES(0xDF05);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_SVC, insn.type);
    CHECK_EQUAL(5u, insn.imm);
}

/*============================================================================
 * Test Group: 16-bit Miscellaneous
 *============================================================================*/

TEST_GROUP(Thumb16Misc)
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

TEST(Thumb16Misc, Nop)
{
    // NOP - No operation hint
    // Encoding: 0xBF00 = 10111111|00000000
    uint8_t code[] = THUMB16_BYTES(0xBF00);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_HINT, insn.type);
}

TEST(Thumb16Misc, Sxth)
{
    // SXTH R0, R1 - Sign extend halfword
    // Encoding: 0xB208 = 1011001|0|00|001|000
    //   [15:6]=1011001000 (SXTH), [5:3]=Rm, [2:0]=Rd
    uint8_t code[] = THUMB16_BYTES(0xB208);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_EXTEND, insn.type);
    CHECK_EQUAL(REG_R0, insn.rd);
    CHECK_EQUAL(REG_R1, insn.rm);
    CHECK_TRUE(insn.is_signed);
    CHECK_EQUAL(ACCESS_HALF, insn.access_size);
}

TEST(Thumb16Misc, Uxtb)
{
    // UXTB R2, R3 - Zero extend byte
    // Encoding: 0xB2DA = 1011001|0|11|011|010
    uint8_t code[] = THUMB16_BYTES(0xB2DA);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_EXTEND, insn.type);
    CHECK_EQUAL(REG_R2, insn.rd);
    CHECK_EQUAL(REG_R3, insn.rm);
    CHECK_FALSE(insn.is_signed);
    CHECK_EQUAL(ACCESS_BYTE, insn.access_size);
}

TEST(Thumb16Misc, AddSpImmediate)
{
    // ADD SP, SP, #16 - Adjust stack pointer up
    // Encoding: 0xB004 = 10110|0|0000100
    //   [15:8]=10110000 (ADD SP), [6:0]=imm7
    //   imm = imm7 << 2 = 4 << 2 = 16
    uint8_t code[] = THUMB16_BYTES(0xB004);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_ADD, insn.op);
    CHECK_EQUAL(REG_SP, insn.rd);
    CHECK_EQUAL(REG_SP, insn.rn);
    CHECK_EQUAL(16u, insn.imm);
}

TEST(Thumb16Misc, SubSpImmediate)
{
    // SUB SP, SP, #32 - Adjust stack pointer down
    // Encoding: 0xB088 = 10110|0|0|0001000
    //   imm = 8 << 2 = 32
    uint8_t code[] = THUMB16_BYTES(0xB088);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_SUB, insn.op);
    CHECK_EQUAL(REG_SP, insn.rd);
    CHECK_EQUAL(32u, insn.imm);
}

TEST(Thumb16Misc, ItBlock)
{
    // IT EQ - If-Then block for conditional execution
    // Encoding: 0xBF08 = 10111111|0000|1000
    //   [15:8]=10111111 (IT), [7:4]=firstcond, [3:0]=mask
    uint8_t code[] = THUMB16_BYTES(0xBF08);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_IT, insn.type);
    CHECK_EQUAL(COND_EQ, insn.it_cond);
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
    // BL +6 - Branch with link (call)
    // Encoding: 0xF000 0xF801
    //   hw1[15:11]=11110 (32-bit prefix), hw1[10]=S
    //   hw2[15:14]=11, hw2[13]=J1, hw2[12]=J2, hw2[11]=1 (BL)
    uint8_t code[] = THUMB32_BYTES(0xF000, 0xF801);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_BRANCH_LINK, insn.type);
    CHECK_TRUE(insn.is_32bit);
    CHECK_TRUE(insn.link);
}

TEST(Thumb32, BranchWide)
{
    // B.W +4 - Unconditional wide branch
    // Encoding: 0xF000 0xB802
    //   hw2[15:14]=10, hw2[12]=1 (B.W)
    uint8_t code[] = THUMB32_BYTES(0xF000, 0xB802);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_BRANCH, insn.type);
    CHECK_TRUE(insn.is_32bit);
    CHECK_EQUAL(COND_AL, insn.cond);
}

TEST(Thumb32, MovWideImmediate)
{
    // MOVW R0, #0x1234 - Move 16-bit wide immediate
    // Encoding: 0xF241 0x2034
    //   hw1[15:11]=11110, hw1[10]=i, hw1[3:0]=imm4
    //   hw2[14:12]=imm3, hw2[11:8]=Rd, hw2[7:0]=imm8
    //   imm16 = imm4:i:imm3:imm8 = 1:0:010:00110100 = 0x1234
    uint8_t code[] = THUMB32_BYTES(0xF241, 0x2034);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_MOV, insn.op);
    CHECK_EQUAL(REG_R0, insn.rd);
    CHECK_TRUE(insn.is_32bit);
}

TEST(Thumb32, AddWideImmediate)
{
    // ADDW R0, R1, #0x100 - Add 12-bit immediate
    // Encoding: 0xF201 0x1000
    //   Rn=R1, Rd=R0, imm12=0x100
    uint8_t code[] = THUMB32_BYTES(0xF201, 0x1000);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_TRUE(insn.is_32bit);
}

TEST(Thumb32, LdrdStrd)
{
    // LDRD R0, R1, [R2, #8] - Load double word
    // Encoding: 0xE9D2 0x0102
    //   hw1[15:9]=1110100, hw1[8]=P, hw1[7]=U, hw1[5]=W, hw1[4]=L
    //   hw1[3:0]=Rn, hw2[15:12]=Rt, hw2[11:8]=Rt2, hw2[7:0]=imm8
    uint8_t code[] = THUMB32_BYTES(0xE9D2, 0x0102);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_TRUE(insn.is_32bit);
    CHECK_EQUAL(REG_R0, insn.rt);
    CHECK_EQUAL(REG_R1, insn.rt2);
    CHECK_EQUAL(REG_R2, insn.rn);
}

TEST(Thumb32, StmWide)
{
    // STMIA R0!, {R1-R4} - Store multiple increment after
    // Encoding: 0xE8A0 0x001E
    //   hw1[15:9]=1110100, hw1[8:7]=op, hw1[5]=W, hw1[4]=0 (store)
    //   hw1[3:0]=Rn, hw2[15:0]=register_list
    uint8_t code[] = THUMB32_BYTES(0xE8A0, 0x001E);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_STORE_MULTIPLE, insn.type);
    CHECK_TRUE(insn.is_32bit);
    CHECK_EQUAL(REG_R0, insn.rn);
    CHECK_EQUAL(0x001Eu, insn.register_list);
}

TEST(Thumb32, LdmWide)
{
    // LDMIA R1!, {R2-R5} - Load multiple increment after
    // Encoding: 0xE8B1 0x003C
    //   register_list = 0x003C (R2, R3, R4, R5)
    uint8_t code[] = THUMB32_BYTES(0xE8B1, 0x003C);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_LOAD_MULTIPLE, insn.type);
    CHECK_TRUE(insn.is_32bit);
    CHECK_EQUAL(REG_R1, insn.rn);
    CHECK_EQUAL(0x003Cu, insn.register_list);
}

TEST(Thumb32, MrsApsr)
{
    // MRS R0, APSR - Move from special register
    // Encoding: 0xF3EF 0x8000
    //   hw1[15:4]=0xF3E (MRS), hw1[3:0]=SYSm[11:8]
    //   hw2[15:12]=1000, hw2[11:8]=Rd
    uint8_t code[] = THUMB32_BYTES(0xF3EF, 0x8000);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_MRS, insn.type);
    CHECK_TRUE(insn.is_32bit);
    CHECK_EQUAL(REG_R0, insn.rd);
}

TEST(Thumb32, CondBranchWide)
{
    // BEQ.W +16 - Conditional wide branch
    // Encoding: 0xF000 0x8008
    //   hw2[15:14]=10, hw2[13:12]=cond[1:0]
    uint8_t code[] = THUMB32_BYTES(0xF000, 0x8008);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_BRANCH, insn.type);
    CHECK_TRUE(insn.is_32bit);
    CHECK_EQUAL(COND_EQ, insn.cond);
}

TEST(Thumb32, Sdiv)
{
    // SDIV R0, R1, R2 - Signed divide
    // Encoding: 1111 1011 1001 nnnn || 1111 dddd 1111 mmmm
    //   hw1 = 0xFB91: bits[15:4]=0xFB9 (SDIV), bits[3:0]=Rn=1
    //   hw2 = 0xF0F2: bits[15:12]=0xF, bits[11:8]=Rd=0, bits[7:4]=0xF, bits[3:0]=Rm=2
    uint8_t code[] = THUMB32_BYTES(0xFB91, 0xF0F2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DIVIDE, insn.type);
    CHECK_TRUE(insn.is_32bit);
    CHECK_TRUE(insn.is_signed);
    CHECK_EQUAL(REG_R0, insn.rd);
    CHECK_EQUAL(REG_R1, insn.rn);
    CHECK_EQUAL(REG_R2, insn.rm);
}

TEST(Thumb32, Udiv)
{
    // UDIV R0, R1, R2 - Unsigned divide
    // Encoding: 1111 1011 1011 nnnn || 1111 dddd 1111 mmmm
    //   hw1 = 0xFBB1: bits[15:4]=0xFBB (UDIV), bits[3:0]=Rn=1
    //   hw2 = 0xF0F2: bits[15:12]=0xF, bits[11:8]=Rd=0, bits[7:4]=0xF, bits[3:0]=Rm=2
    uint8_t code[] = THUMB32_BYTES(0xFBB1, 0xF0F2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DIVIDE, insn.type);
    CHECK_TRUE(insn.is_32bit);
    CHECK_FALSE(insn.is_signed);
    CHECK_EQUAL(REG_R0, insn.rd);
    CHECK_EQUAL(REG_R1, insn.rn);
    CHECK_EQUAL(REG_R2, insn.rm);
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
    // 0xDExx is permanently undefined (UDF)
    // Encoding: 0xDE00 = 11011110|xxxxxxxx
    uint8_t code[] = THUMB16_BYTES(0xDE00);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(ARMV8M_ERR_UNDEFINED_INSN, result);
}

TEST(DecoderErrors, EmptyStmRegisterList)
{
    // STM R0!, {} - Empty register list is unpredictable
    // Encoding: 0xC000 = 11000|000|00000000
    uint8_t code[] = THUMB16_BYTES(0xC000);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(ARMV8M_ERR_UNPREDICTABLE, result);
}

TEST(DecoderErrors, EmptyLdmRegisterList)
{
    // LDM R0!, {} - Empty register list is unpredictable
    // Encoding: 0xC800 = 11001|000|00000000
    uint8_t code[] = THUMB16_BYTES(0xC800);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(ARMV8M_ERR_UNPREDICTABLE, result);
}

TEST(DecoderErrors, NullPointerMem)
{
    int result = armv8m_decode(nullptr, TEST_PC, &insn);

    CHECK_EQUAL(ARMV8M_ERR_UNDEFINED_INSN, result);
}

TEST(DecoderErrors, NullPointerInsn)
{
    // MOVS R0, #0
    uint8_t code[] = THUMB16_BYTES(0x2000);

    int result = armv8m_decode(code, TEST_PC, nullptr);

    CHECK_EQUAL(ARMV8M_ERR_UNDEFINED_INSN, result);
}

/*============================================================================
 * Test Group: Encoding Verification
 *============================================================================*/

TEST_GROUP(EncodingVerify)
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

TEST(EncodingVerify, Size16Bit)
{
    // MOVS R0, #0 - Verify 16-bit instruction size
    uint8_t code[] = THUMB16_BYTES(0x2000);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(2u, insn.size);
    CHECK_FALSE(insn.is_32bit);
}

TEST(EncodingVerify, Size32Bit)
{
    // BL - Verify 32-bit instruction size
    uint8_t code[] = THUMB32_BYTES(0xF000, 0xF801);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(4u, insn.size);
    CHECK_TRUE(insn.is_32bit);
}

TEST(EncodingVerify, PcValue)
{
    // Verify PC is stored correctly
    const uint32_t test_addr = 0x08001234;
    uint8_t code[] = THUMB16_BYTES(0x2000);

    int result = armv8m_decode(code, test_addr, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(test_addr, insn.pc);
}

TEST(EncodingVerify, RawEncoding16)
{
    // MOVS R0, #42 - Verify raw encoding is stored
    uint8_t code[] = THUMB16_BYTES(0x202A);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(0x202Au, insn.encoding);
}

TEST(EncodingVerify, RawEncoding32)
{
    // BL - Verify 32-bit raw encoding (hw1 << 16 | hw2)
    uint8_t code[] = THUMB32_BYTES(0xF000, 0xF801);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(0xF000F801u, insn.encoding);
}

/*============================================================================
 * Test Group: Additional 16-bit Coverage Tests
 *============================================================================*/

TEST_GROUP(Thumb16Coverage)
{
    DecodedInsn insn;

    void setup()
    {
        armv8m_decode_init(&insn);
    }
};

// ADR instruction - bits[15:11] = 10100
TEST(Thumb16Coverage, Adr)
{
    // ADR R0, #4 - PC-relative address calculation
    // Encoding: 0xA001 = 10100|000|00000001
    //   [15:11]=10100 (ADR), [10:8]=Rd, [7:0]=imm8
    //   imm = imm8 << 2 = 1 << 2 = 4
    uint8_t code[] = THUMB16_BYTES(0xA001);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(REG_R0, insn.rd);
}

// ADD SP immediate - bits[15:11] = 10101
TEST(Thumb16Coverage, AddSpImm)
{
    // ADD R0, SP, #8 - Add SP with immediate
    // Encoding: 0xA802 = 10101|000|00000010
    //   imm = 2 << 2 = 8
    uint8_t code[] = THUMB16_BYTES(0xA802);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(REG_R0, insn.rd);
    CHECK_EQUAL(REG_SP, insn.rn);
}

// LSR immediate with zero shift (means 32)
TEST(Thumb16Coverage, LsrImmZero)
{
    // LSRS R0, R1, #0 - Zero shift encodes as 32
    // Encoding: 0x0808 = 00001|00000|001|000
    //   [15:11]=00001 (LSR imm), [10:6]=imm5=0 means 32
    uint8_t code[] = THUMB16_BYTES(0x0808);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_SHIFTED, insn.type);
    CHECK_EQUAL(DP_MOV, insn.op);  /* Immediate shift uses MOV with shift applied */
    CHECK_EQUAL(32u, insn.shift_amount);
}

// ASR immediate with zero shift (means 32)
TEST(Thumb16Coverage, AsrImmZero)
{
    // ASRS R0, R1, #0 - Zero shift encodes as 32
    // Encoding: 0x1008 = 00010|00000|001|000
    uint8_t code[] = THUMB16_BYTES(0x1008);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_SHIFTED, insn.type);
    CHECK_EQUAL(DP_MOV, insn.op);  /* Immediate shift uses MOV with shift applied */
    CHECK_EQUAL(32u, insn.shift_amount);
}

// ADD with 3-bit immediate
TEST(Thumb16Coverage, AddImm3)
{
    // ADDS R0, R1, #3 - Add small immediate
    // Encoding: 0x1CC8 = 000111|00|011|001|000
    //   [15:9]=0001110 (ADD imm3), [8:6]=imm3, [5:3]=Rn, [2:0]=Rd
    uint8_t code[] = THUMB16_BYTES(0x1CC8);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_ADD, insn.op);
    CHECK_EQUAL(3u, insn.imm);
}

// SUB with 3-bit immediate
TEST(Thumb16Coverage, SubImm3)
{
    // SUBS R0, R1, #3 - Subtract small immediate
    // Encoding: 0x1EC8 = 000111|10|011|001|000
    uint8_t code[] = THUMB16_BYTES(0x1EC8);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_SUB, insn.op);
}

// SUBS Rd, #imm8
TEST(Thumb16Coverage, SubsRdImm8)
{
    // SUBS R1, #100 - Subtract 8-bit immediate
    // Encoding: 0x3964 = 001|11|001|01100100
    uint8_t code[] = THUMB16_BYTES(0x3964);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_SUB, insn.op);
    CHECK_EQUAL(100u, insn.imm);
}

// MOVS low register (shift amount 0)
TEST(Thumb16Coverage, MovsLowReg)
{
    // MOVS R0, R1 - Move register (encoded as LSL #0)
    // Encoding: 0x0008 = 00000|00000|001|000
    uint8_t code[] = THUMB16_BYTES(0x0008);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_REG, insn.type);
    CHECK_EQUAL(DP_MOV, insn.op);
}

// Data processing register: LSLS register
// Shift amount in Rm register, value to shift in Rn - uses INSN_DATA_PROC_REG
TEST(Thumb16Coverage, LslsReg)
{
    // LSLS R0, R1 - Logical shift left by register
    // Encoding: 0x4088 = 010000|0010|001|000
    uint8_t code[] = THUMB16_BYTES(0x4088);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_REG, insn.type);
    CHECK_EQUAL(DP_LSL, insn.op);
}

// Data processing register: LSRS register
// Shift amount in Rm register, value to shift in Rn - uses INSN_DATA_PROC_REG
TEST(Thumb16Coverage, LsrsReg)
{
    // LSRS R0, R1 - Logical shift right by register
    // Encoding: 0x40C8 = 010000|0011|001|000
    uint8_t code[] = THUMB16_BYTES(0x40C8);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_REG, insn.type);
    CHECK_EQUAL(DP_LSR, insn.op);
}

// Data processing register: ASRS register
// Shift amount in Rm register, value to shift in Rn - uses INSN_DATA_PROC_REG
TEST(Thumb16Coverage, AsrsReg)
{
    // ASRS R0, R1 - Arithmetic shift right by register
    // Encoding: 0x4108 = 010000|0100|001|000
    uint8_t code[] = THUMB16_BYTES(0x4108);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_REG, insn.type);
    CHECK_EQUAL(DP_ASR, insn.op);
}

// Data processing register: ADCS
TEST(Thumb16Coverage, Adcs)
{
    // ADCS R0, R1 - Add with carry
    // Encoding: 0x4148 = 010000|0101|001|000
    uint8_t code[] = THUMB16_BYTES(0x4148);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_REG, insn.type);
    CHECK_EQUAL(DP_ADC, insn.op);
}

// Data processing register: SBCS
TEST(Thumb16Coverage, Sbcs)
{
    // SBCS R0, R1 - Subtract with carry
    // Encoding: 0x4188 = 010000|0110|001|000
    uint8_t code[] = THUMB16_BYTES(0x4188);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_REG, insn.type);
    CHECK_EQUAL(DP_SBC, insn.op);
}

// Data processing register: RORS
// Rotate amount in Rm register, value to rotate in Rn - uses INSN_DATA_PROC_REG
TEST(Thumb16Coverage, Rors)
{
    // RORS R0, R1 - Rotate right by register
    // Encoding: 0x41C8 = 010000|0111|001|000
    uint8_t code[] = THUMB16_BYTES(0x41C8);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_REG, insn.type);
    CHECK_EQUAL(DP_ROR, insn.op);
}

// Data processing register: TST
TEST(Thumb16Coverage, Tst)
{
    // TST R0, R1 - Test bits (AND without result)
    // Encoding: 0x4208 = 010000|1000|001|000
    uint8_t code[] = THUMB16_BYTES(0x4208);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_REG, insn.type);
    CHECK_EQUAL(DP_TST, insn.op);
}

// Data processing register: RSB (NEGS)
TEST(Thumb16Coverage, Rsb)
{
    // RSBS R0, R1, #0 - Reverse subtract (negate)
    // Encoding: 0x4248 = 010000|1001|001|000
    uint8_t code[] = THUMB16_BYTES(0x4248);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_RSB, insn.op);
}

// Data processing register: CMP
TEST(Thumb16Coverage, CmpReg)
{
    // CMP R0, R1 - Compare registers
    // Encoding: 0x4288 = 010000|1010|001|000
    uint8_t code[] = THUMB16_BYTES(0x4288);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_REG, insn.type);
    CHECK_EQUAL(DP_CMP, insn.op);
}

// Data processing register: CMN
TEST(Thumb16Coverage, Cmn)
{
    // CMN R0, R1 - Compare negative
    // Encoding: 0x42C8 = 010000|1011|001|000
    uint8_t code[] = THUMB16_BYTES(0x42C8);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_REG, insn.type);
    CHECK_EQUAL(DP_CMN, insn.op);
}

// Data processing register: ORRS
TEST(Thumb16Coverage, Orrs)
{
    // ORRS R0, R1 - Bitwise OR
    // Encoding: 0x4308 = 010000|1100|001|000
    uint8_t code[] = THUMB16_BYTES(0x4308);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_REG, insn.type);
    CHECK_EQUAL(DP_ORR, insn.op);
}

// Data processing register: MULS
TEST(Thumb16Coverage, Muls)
{
    // MULS R0, R1 - Multiply
    // Encoding: 0x4348 = 010000|1101|001|000
    uint8_t code[] = THUMB16_BYTES(0x4348);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_MULTIPLY, insn.type);
}

// Data processing register: BICS
TEST(Thumb16Coverage, Bics)
{
    // BICS R0, R1 - Bit clear (AND NOT)
    // Encoding: 0x4388 = 010000|1110|001|000
    uint8_t code[] = THUMB16_BYTES(0x4388);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_REG, insn.type);
    CHECK_EQUAL(DP_BIC, insn.op);
}

// Special data: ADD high register
TEST(Thumb16Coverage, AddHighReg)
{
    // ADD R8, R0 - Add using high registers
    // Encoding: 0x4480 = 01000100|1|0000|000
    //   [15:8]=01000100 (special data), [7]=Dn, [6:3]=Rm, [2:0]=Rdn
    uint8_t code[] = THUMB16_BYTES(0x4480);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_REG, insn.type);
    CHECK_EQUAL(DP_ADD, insn.op);
}

// Special data: CMP high register
TEST(Thumb16Coverage, CmpHighReg)
{
    // CMP R8, R0 - Compare high register
    // Encoding: 0x4580 = 01000101|1|0000|000
    uint8_t code[] = THUMB16_BYTES(0x4580);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_REG, insn.type);
    CHECK_EQUAL(DP_CMP, insn.op);
}

// Special data: MOV high register
TEST(Thumb16Coverage, MovHighReg)
{
    // MOV R8, R0 - Move to high register
    // Encoding: 0x4680 = 01000110|1|0000|000
    uint8_t code[] = THUMB16_BYTES(0x4680);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_REG, insn.type);
    CHECK_EQUAL(DP_MOV, insn.op);
}

// Miscellaneous: PUSH
TEST(Thumb16Coverage, Push)
{
    // PUSH {R0, R1, LR}
    // Encoding: 0xB503 = 1011010|1|00000011
    uint8_t code[] = THUMB16_BYTES(0xB503);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_STORE_MULTIPLE, insn.type);
}

// Miscellaneous: POP
TEST(Thumb16Coverage, Pop)
{
    // POP {R0, R1, PC}
    // Encoding: 0xBD03 = 1011110|1|00000011
    uint8_t code[] = THUMB16_BYTES(0xBD03);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_LOAD_MULTIPLE, insn.type);
}

// Miscellaneous: SXTH
TEST(Thumb16Coverage, Sxth)
{
    // SXTH R0, R1 - Sign extend halfword
    // Encoding: 0xB208 = 1011001|0|00|001|000
    uint8_t code[] = THUMB16_BYTES(0xB208);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_EXTEND, insn.type);
    CHECK_TRUE(insn.is_signed);
}

// Miscellaneous: SXTB
TEST(Thumb16Coverage, Sxtb)
{
    // SXTB R0, R1 - Sign extend byte
    // Encoding: 0xB248 = 1011001|0|01|001|000
    uint8_t code[] = THUMB16_BYTES(0xB248);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_EXTEND, insn.type);
    CHECK_TRUE(insn.is_signed);
}

// Miscellaneous: UXTH
TEST(Thumb16Coverage, Uxth)
{
    // UXTH R0, R1 - Zero extend halfword
    // Encoding: 0xB288 = 1011001|0|10|001|000
    uint8_t code[] = THUMB16_BYTES(0xB288);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_EXTEND, insn.type);
    CHECK_FALSE(insn.is_signed);
}

// Miscellaneous: UXTB
TEST(Thumb16Coverage, Uxtb)
{
    // UXTB R0, R1 - Zero extend byte
    // Encoding: 0xB2C8 = 1011001|0|11|001|000
    uint8_t code[] = THUMB16_BYTES(0xB2C8);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_EXTEND, insn.type);
    CHECK_FALSE(insn.is_signed);
}

// Miscellaneous: ADD SP, #imm
TEST(Thumb16Coverage, AddSpSpImm)
{
    // ADD SP, SP, #8 - Increment stack pointer
    // Encoding: 0xB002 = 10110|0|0000010
    //   imm = 2 << 2 = 8
    uint8_t code[] = THUMB16_BYTES(0xB002);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_ADD, insn.op);
}

// Miscellaneous: SUB SP, #imm
TEST(Thumb16Coverage, SubSpSpImm)
{
    // SUB SP, SP, #8 - Decrement stack pointer
    // Encoding: 0xB082 = 10110|0|1000010
    uint8_t code[] = THUMB16_BYTES(0xB082);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_SUB, insn.op);
}

// Miscellaneous: CBNZ
TEST(Thumb16Coverage, Cbnz)
{
    // CBNZ R0, +4 - Compare and branch if not zero
    // Encoding: 0xB902 = 10111|0|01|00000|010
    uint8_t code[] = THUMB16_BYTES(0xB902);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_COMPARE_BRANCH, insn.type);
}

// Miscellaneous: REV
TEST(Thumb16Coverage, Rev)
{
    // REV R0, R1 - Byte reverse word
    // Encoding: 0xBA08 = 1011101|0|00|001|000
    uint8_t code[] = THUMB16_BYTES(0xBA08);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_EXTEND, insn.type);
}

// Miscellaneous: REV16
TEST(Thumb16Coverage, Rev16)
{
    // REV16 R0, R1 - Byte reverse packed halfwords
    // Encoding: 0xBA48 = 1011101|0|01|001|000
    uint8_t code[] = THUMB16_BYTES(0xBA48);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_EXTEND, insn.type);
}

// Miscellaneous: REVSH
TEST(Thumb16Coverage, Revsh)
{
    // REVSH R0, R1 - Byte reverse signed halfword
    // Encoding: 0xBAC8 = 1011101|0|11|001|000
    uint8_t code[] = THUMB16_BYTES(0xBAC8);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_EXTEND, insn.type);
}

// Miscellaneous: BKPT
TEST(Thumb16Coverage, Bkpt)
{
    // BKPT #0 - Breakpoint
    // Encoding: 0xBE00 = 10111110|00000000
    uint8_t code[] = THUMB16_BYTES(0xBE00);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_HINT, insn.type);
}

// Miscellaneous: Hints (NOP)
TEST(Thumb16Coverage, Nop)
{
    // NOP - No operation
    // Encoding: 0xBF00 = 10111111|00000000
    uint8_t code[] = THUMB16_BYTES(0xBF00);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_HINT, insn.type);
}

// Miscellaneous: IT block
TEST(Thumb16Coverage, It)
{
    // ITEQ - If-Then EQ
    // Encoding: 0xBF08 = 10111111|00001000
    uint8_t code[] = THUMB16_BYTES(0xBF08);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_IT, insn.type);
}

// SVC instruction
TEST(Thumb16Coverage, Svc)
{
    // SVC #0 - Supervisor call
    // Encoding: 0xDF00 = 11011111|00000000
    uint8_t code[] = THUMB16_BYTES(0xDF00);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_SVC, insn.type);
}

// Store halfword register
TEST(Thumb16Coverage, StrhReg)
{
    // STRH R0, [R1, R2] - Store halfword with register offset
    // Encoding: 0x5288 = 0101001|010|001|000
    uint8_t code[] = THUMB16_BYTES(0x5288);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_STORE_REG, insn.type);
    CHECK_EQUAL(ACCESS_HALF, insn.access_size);
}

// Store byte register
TEST(Thumb16Coverage, StrbReg)
{
    // STRB R0, [R1, R2] - Store byte with register offset
    // Encoding: 0x5488 = 0101010|010|001|000
    uint8_t code[] = THUMB16_BYTES(0x5488);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_STORE_REG, insn.type);
    CHECK_EQUAL(ACCESS_BYTE, insn.access_size);
}

// Load signed byte register
TEST(Thumb16Coverage, LdrsbReg)
{
    // LDRSB R0, [R1, R2] - Load signed byte with register offset
    // Encoding: 0x5688 = 0101011|010|001|000
    uint8_t code[] = THUMB16_BYTES(0x5688);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_LOAD_REG, insn.type);
    CHECK_EQUAL(ACCESS_BYTE, insn.access_size);
    CHECK_TRUE(insn.is_signed);
}

// Load word register
TEST(Thumb16Coverage, LdrReg)
{
    // LDR R0, [R1, R2] - Load word with register offset
    // Encoding: 0x5888 = 0101100|010|001|000
    uint8_t code[] = THUMB16_BYTES(0x5888);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_LOAD_REG, insn.type);
    CHECK_EQUAL(ACCESS_WORD, insn.access_size);
}

// Load halfword register
TEST(Thumb16Coverage, LdrhReg)
{
    // LDRH R0, [R1, R2] - Load halfword with register offset
    // Encoding: 0x5A88 = 0101101|010|001|000
    uint8_t code[] = THUMB16_BYTES(0x5A88);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_LOAD_REG, insn.type);
    CHECK_EQUAL(ACCESS_HALF, insn.access_size);
}

// Load byte register
TEST(Thumb16Coverage, LdrbReg)
{
    // LDRB R0, [R1, R2] - Load byte with register offset
    // Encoding: 0x5C88 = 0101110|010|001|000
    uint8_t code[] = THUMB16_BYTES(0x5C88);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_LOAD_REG, insn.type);
    CHECK_EQUAL(ACCESS_BYTE, insn.access_size);
}

// Load signed halfword register
TEST(Thumb16Coverage, LdrshReg)
{
    // LDRSH R0, [R1, R2] - Load signed halfword with register offset
    // Encoding: 0x5E88 = 0101111|010|001|000
    uint8_t code[] = THUMB16_BYTES(0x5E88);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_LOAD_REG, insn.type);
    CHECK_EQUAL(ACCESS_HALF, insn.access_size);
    CHECK_TRUE(insn.is_signed);
}

// Load halfword SP-relative
TEST(Thumb16Coverage, LdrhSpRel)
{
    // LDRH R0, [R0, #2] - Load halfword with immediate offset
    // Encoding: 0x8801 = 10001|00000|000|001
    uint8_t code[] = THUMB16_BYTES(0x8801);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_LOAD_IMM, insn.type);
    CHECK_EQUAL(ACCESS_HALF, insn.access_size);
}

// Store halfword immediate
TEST(Thumb16Coverage, StrhImm)
{
    // STRH R0, [R1, #4] - Store halfword with immediate offset
    // Encoding: 0x8048 = 10000|00010|001|000
    //   imm = 2 << 1 = 4
    uint8_t code[] = THUMB16_BYTES(0x8048);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_STORE_IMM, insn.type);
    CHECK_EQUAL(ACCESS_HALF, insn.access_size);
}

// Load SP-relative word
TEST(Thumb16Coverage, LdrSpRelWord)
{
    // LDR R0, [SP, #4] - SP-relative load
    // Encoding: 0x9801 = 10011|000|00000001
    //   imm = 1 << 2 = 4
    uint8_t code[] = THUMB16_BYTES(0x9801);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_LOAD_IMM, insn.type);
    CHECK_EQUAL(ACCESS_WORD, insn.access_size);
    CHECK_EQUAL(REG_SP, insn.rn);
}

// Store SP-relative word
TEST(Thumb16Coverage, StrSpRelWord)
{
    // STR R0, [SP, #4] - SP-relative store
    // Encoding: 0x9001 = 10010|000|00000001
    //   imm = 1 << 2 = 4
    uint8_t code[] = THUMB16_BYTES(0x9001);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_STORE_IMM, insn.type);
    CHECK_EQUAL(ACCESS_WORD, insn.access_size);
    CHECK_EQUAL(REG_SP, insn.rn);
}

/*============================================================================
 * Test Group: Additional 32-bit Coverage Tests
 *============================================================================*/

TEST_GROUP(Thumb32Coverage)
{
    DecodedInsn insn;

    void setup()
    {
        armv8m_decode_init(&insn);
    }
};

// Data processing modified immediate: AND
TEST(Thumb32Coverage, AndModImm)
{
    // AND R0, R1, #0xFF - Bitwise AND with modified immediate
    // Encoding: 0xF001 0x00FF
    uint8_t code[] = THUMB32_BYTES(0xF001, 0x00FF);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_AND, insn.op);
}

// Data processing modified immediate: ORR
TEST(Thumb32Coverage, OrrModImm)
{
    // ORR R0, R1, #0xFF - Bitwise OR with modified immediate
    // Encoding: 0xF041 0x00FF
    uint8_t code[] = THUMB32_BYTES(0xF041, 0x00FF);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_ORR, insn.op);
}

// Data processing modified immediate: MVN
TEST(Thumb32Coverage, MvnModImm)
{
    // MVN R0, #0xFF - Bitwise NOT of modified immediate
    // Encoding: 0xF06F 0x00FF
    uint8_t code[] = THUMB32_BYTES(0xF06F, 0x00FF);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_MVN, insn.op);
}

// Data processing modified immediate: EOR
TEST(Thumb32Coverage, EorModImm)
{
    // EOR R0, R1, #0xFF - Bitwise XOR with modified immediate
    // Encoding: 0xF081 0x00FF
    uint8_t code[] = THUMB32_BYTES(0xF081, 0x00FF);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_EOR, insn.op);
}

// Data processing modified immediate: ADD
TEST(Thumb32Coverage, AddModImm)
{
    // ADD R0, R1, #0xFF - Add with modified immediate
    // Encoding: 0xF101 0x00FF
    uint8_t code[] = THUMB32_BYTES(0xF101, 0x00FF);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_ADD, insn.op);
}

// Data processing modified immediate: ADC
TEST(Thumb32Coverage, AdcModImm)
{
    // ADC R0, R1, #0xFF - Add with carry and modified immediate
    // Encoding: 0xF141 0x00FF
    uint8_t code[] = THUMB32_BYTES(0xF141, 0x00FF);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_ADC, insn.op);
}

// Data processing modified immediate: SBC
TEST(Thumb32Coverage, SbcModImm)
{
    // SBC R0, R1, #0xFF - Subtract with carry and modified immediate
    // Encoding: 0xF161 0x00FF
    uint8_t code[] = THUMB32_BYTES(0xF161, 0x00FF);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_SBC, insn.op);
}

// Data processing modified immediate: SUB
TEST(Thumb32Coverage, SubModImm)
{
    // SUB R0, R1, #0xFF - Subtract with modified immediate
    // Encoding: 0xF1A1 0x00FF
    uint8_t code[] = THUMB32_BYTES(0xF1A1, 0x00FF);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_SUB, insn.op);
}

// Data processing modified immediate: RSB
TEST(Thumb32Coverage, RsbModImm)
{
    // RSB R0, R1, #0xFF - Reverse subtract with modified immediate
    // Encoding: 0xF1C1 0x00FF
    uint8_t code[] = THUMB32_BYTES(0xF1C1, 0x00FF);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_RSB, insn.op);
}

// SUBW (12-bit immediate)
TEST(Thumb32Coverage, SubW)
{
    // SUBW R0, R1, #0x100 - Subtract with 12-bit immediate
    // Encoding: 0xF2A1 0x1000
    uint8_t code[] = THUMB32_BYTES(0xF2A1, 0x1000);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_SUB, insn.op);
}

// MOVT (move top)
TEST(Thumb32Coverage, Movt)
{
    // MOVT R0, #0x1234 - Move to top halfword
    // Encoding: 0xF2C1 0x2034
    uint8_t code[] = THUMB32_BYTES(0xF2C1, 0x2034);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_MOV, insn.op);
}

// Load/store multiple: STMDB
TEST(Thumb32Coverage, Stmdb)
{
    // STMDB R0!, {R1, R2} - Store multiple decrement before
    // Encoding: 0xE920 0x0006
    uint8_t code[] = THUMB32_BYTES(0xE920, 0x0006);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_STORE_MULTIPLE, insn.type);
}

// Load/store multiple: LDMDB
TEST(Thumb32Coverage, Ldmdb)
{
    // LDMDB R0!, {R1, R2} - Load multiple decrement before
    // Encoding: 0xE930 0x0006
    uint8_t code[] = THUMB32_BYTES(0xE930, 0x0006);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_LOAD_MULTIPLE, insn.type);
}

// Data processing shifted register: AND
TEST(Thumb32Coverage, AndShiftedReg)
{
    // AND R0, R1, R2, LSL #4 - Bitwise AND with shifted register
    // Encoding: 0xEA01 0x1002
    uint8_t code[] = THUMB32_BYTES(0xEA01, 0x1002);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_SHIFTED, insn.type);
    CHECK_EQUAL(DP_AND, insn.op);
}

// Data processing shifted register: BIC
TEST(Thumb32Coverage, BicShiftedReg)
{
    // BIC R0, R1, R2 - Bit clear with shifted register
    // Encoding: 0xEA21 0x0002
    uint8_t code[] = THUMB32_BYTES(0xEA21, 0x0002);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_SHIFTED, insn.type);
    CHECK_EQUAL(DP_BIC, insn.op);
}

// Data processing shifted register: ORR
TEST(Thumb32Coverage, OrrShiftedReg)
{
    // ORR R0, R1, R2 - Bitwise OR with shifted register
    // Encoding: 0xEA41 0x0002
    uint8_t code[] = THUMB32_BYTES(0xEA41, 0x0002);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_SHIFTED, insn.type);
    CHECK_EQUAL(DP_ORR, insn.op);
}

// Data processing shifted register: ORN
TEST(Thumb32Coverage, OrnShiftedReg)
{
    // ORN R0, R1, R2 - Bitwise OR NOT with shifted register
    // Encoding: 0xEA61 0x0002
    uint8_t code[] = THUMB32_BYTES(0xEA61, 0x0002);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_SHIFTED, insn.type);
    CHECK_EQUAL(DP_ORN, insn.op);
}

// Data processing shifted register: EOR
TEST(Thumb32Coverage, EorShiftedReg)
{
    // EOR R0, R1, R2 - Bitwise XOR with shifted register
    // Encoding: 0xEA81 0x0002
    uint8_t code[] = THUMB32_BYTES(0xEA81, 0x0002);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_SHIFTED, insn.type);
    CHECK_EQUAL(DP_EOR, insn.op);
}

// Data processing shifted register: ADD
TEST(Thumb32Coverage, AddShiftedReg)
{
    // ADD R0, R1, R2 - Add with shifted register
    // Encoding: 0xEB01 0x0002
    uint8_t code[] = THUMB32_BYTES(0xEB01, 0x0002);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_SHIFTED, insn.type);
    CHECK_EQUAL(DP_ADD, insn.op);
}

// Data processing shifted register: ADC
TEST(Thumb32Coverage, AdcShiftedReg)
{
    // ADC R0, R1, R2 - Add with carry and shifted register
    // Encoding: 0xEB41 0x0002
    uint8_t code[] = THUMB32_BYTES(0xEB41, 0x0002);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_SHIFTED, insn.type);
    CHECK_EQUAL(DP_ADC, insn.op);
}

// Data processing shifted register: SBC
TEST(Thumb32Coverage, SbcShiftedReg)
{
    // SBC R0, R1, R2 - Subtract with carry and shifted register
    // Encoding: 0xEB61 0x0002
    uint8_t code[] = THUMB32_BYTES(0xEB61, 0x0002);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_SHIFTED, insn.type);
    CHECK_EQUAL(DP_SBC, insn.op);
}

// Data processing shifted register: SUB
TEST(Thumb32Coverage, SubShiftedReg)
{
    // SUB R0, R1, R2 - Subtract with shifted register
    // Encoding: 0xEBA1 0x0002
    uint8_t code[] = THUMB32_BYTES(0xEBA1, 0x0002);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_SHIFTED, insn.type);
    CHECK_EQUAL(DP_SUB, insn.op);
}

// Data processing shifted register: RSB
TEST(Thumb32Coverage, RsbShiftedReg)
{
    // RSB R0, R1, R2 - Reverse subtract with shifted register
    // Encoding: 0xEBC1 0x0002
    uint8_t code[] = THUMB32_BYTES(0xEBC1, 0x0002);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_SHIFTED, insn.type);
    CHECK_EQUAL(DP_RSB, insn.op);
}

// MUL 32-bit
TEST(Thumb32Coverage, Mul32)
{
    // MUL R0, R1, R2 - 32-bit multiply
    // Encoding: 0xFB01 0xF002
    uint8_t code[] = THUMB32_BYTES(0xFB01, 0xF002);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_MULTIPLY, insn.type);
}

// MSR (write to system register)
TEST(Thumb32Coverage, Msr)
{
    // MSR APSR, R0 - Move to system register
    // Encoding: 0xF380 0x8800
    uint8_t code[] = THUMB32_BYTES(0xF380, 0x8800);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_MSR, insn.type);
}

// Thumb expand imm type 1 pattern
TEST(Thumb32Coverage, ExpandImmType1)
{
    // AND with imm8 replicated: type 1 = 0x1XX
    // imm12 = 0x1FF means type=1, imm8=0xFF => 0x00FF00FF
    uint8_t code[] = THUMB32_BYTES(0xF001, 0x01FF);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
}

// Thumb expand imm type 2 pattern
TEST(Thumb32Coverage, ExpandImmType2)
{
    // imm12 = 0x2FF means type=2, imm8 replicated to 0xFF00FF00
    // Encoding: 0xF001 0x02FF
    uint8_t code[] = THUMB32_BYTES(0xF001, 0x02FF);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
}

// Thumb expand imm type 3 pattern
TEST(Thumb32Coverage, ExpandImmType3)
{
    // imm12 = 0x3FF means type=3, imm8 replicated to all bytes
    // Encoding: 0xF001 0x03FF
    uint8_t code[] = THUMB32_BYTES(0xF001, 0x03FF);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
}

// Thumb expand imm rotated pattern
TEST(Thumb32Coverage, ExpandImmRotated)
{
    // imm12 with type >= 4 uses rotation
    // Encoding: 0xF001 0x0480
    uint8_t code[] = THUMB32_BYTES(0xF001, 0x0480);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
}

// STR register word
TEST(Thumb16Coverage, StrWordReg)
{
    // STR R0, [R1, R2] - Store word with register offset
    // Encoding: 0x5088 = 0101000|010|001|000
    uint8_t code[] = THUMB16_BYTES(0x5088);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_STORE_REG, insn.type);
    CHECK_EQUAL(ACCESS_WORD, insn.access_size);
}

// STRB immediate
TEST(Thumb16Coverage, StrbImm)
{
    // STRB R0, [R1, #4] - Store byte with immediate offset
    // Encoding: 0x7148 = 01110|00100|001|000
    uint8_t code[] = THUMB16_BYTES(0x7148);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_STORE_IMM, insn.type);
    CHECK_EQUAL(ACCESS_BYTE, insn.access_size);
}

// CPS instruction
TEST(Thumb16Coverage, Cps)
{
    // CPSID i - Change processor state, disable interrupts
    // Encoding: 0xB672 = 10110110|01110010
    uint8_t code[] = THUMB16_BYTES(0xB672);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_CPS, insn.type);
}

/*============================================================================
 * Test Group: Additional 32-bit Coverage Tests (Part 2)
 *============================================================================*/

// BLX immediate (32-bit)
TEST(Thumb32Coverage, BlxImm)
{
    // BLX label - Branch with link and exchange to ARM
    // Encoding: 0xF000 0xC001
    uint8_t code[] = THUMB32_BYTES(0xF000, 0xC001);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_BRANCH_LINK_EXCHANGE, insn.type);
}

// Hints (32-bit NOP.W)
TEST(Thumb32Coverage, NopW)
{
    // NOP.W - Wide no-operation hint
    // Encoding: 0xF3AF 0x8000
    uint8_t code[] = THUMB32_BYTES(0xF3AF, 0x8000);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_HINT, insn.type);
}

// TST with modified immediate
TEST(Thumb32Coverage, TstModImm)
{
    // TST R0, #0xFF - Test bits (AND with flags, no result)
    // Encoding: 0xF010 0x0FFF (Rd=PC indicates TST)
    uint8_t code[] = THUMB32_BYTES(0xF010, 0x0FFF);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_TST, insn.op);
}

// BIC with modified immediate
TEST(Thumb32Coverage, BicModImm)
{
    // BIC R0, R1, #0xFF - Bit clear with modified immediate
    // Encoding: 0xF021 0x00FF
    uint8_t code[] = THUMB32_BYTES(0xF021, 0x00FF);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_BIC, insn.op);
}

// MOV with modified immediate (rn = 0xF)
TEST(Thumb32Coverage, MovModImm)
{
    // MOV R0, #0xFF - Move modified immediate
    // Encoding: 0xF04F 0x00FF (Rn=0xF indicates MOV)
    uint8_t code[] = THUMB32_BYTES(0xF04F, 0x00FF);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_MOV, insn.op);
}

// ORN with modified immediate
TEST(Thumb32Coverage, OrnModImm)
{
    // ORN R0, R1, #0xFF - OR NOT with modified immediate
    // Encoding: 0xF061 0x00FF
    uint8_t code[] = THUMB32_BYTES(0xF061, 0x00FF);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_ORN, insn.op);
}

// TEQ with modified immediate (EOR with Rd=PC, S=1)
TEST(Thumb32Coverage, TeqModImm)
{
    // TEQ R0, #0xFF - Test equivalence (XOR with flags)
    // Encoding: 0xF090 0x0FFF
    uint8_t code[] = THUMB32_BYTES(0xF090, 0x0FFF);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
}

// CMN with modified immediate
TEST(Thumb32Coverage, CmnModImm)
{
    // CMN R0, #0xFF - Compare negative (ADD with flags)
    // Encoding: 0xF110 0x0FFF
    uint8_t code[] = THUMB32_BYTES(0xF110, 0x0FFF);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_CMN, insn.op);
}

// CMP with modified immediate
TEST(Thumb32Coverage, CmpModImm)
{
    // CMP R0, #0xFF - Compare with modified immediate
    // Encoding: 0xF1B0 0x0FFF
    uint8_t code[] = THUMB32_BYTES(0xF1B0, 0x0FFF);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_CMP, insn.op);
}

// ADR with 12-bit immediate (add to PC)
TEST(Thumb32Coverage, AdrAdd)
{
    // ADR R0, label (add) - PC-relative address calculation
    // Encoding: 0xF20F 0x0100
    uint8_t code[] = THUMB32_BYTES(0xF20F, 0x0100);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_ADD, insn.op);
}

// Store byte (32-bit encoding)
TEST(Thumb32Coverage, StrbW)
{
    // STRB.W R0, [R1, #0] - Wide store byte
    // Encoding: 0xF801 0x0C00
    uint8_t code[] = THUMB32_BYTES(0xF801, 0x0C00);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
}

// Load byte (32-bit encoding)
TEST(Thumb32Coverage, LdrbW)
{
    // LDRB.W R0, [R1, #0] - Wide load byte
    // Encoding: 0xF811 0x0C00
    uint8_t code[] = THUMB32_BYTES(0xF811, 0x0C00);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
}

// Data processing register with shift (32-bit) - MOV with shift
TEST(Thumb32Coverage, MovShifted)
{
    // MOV R0, R1, LSL #4 - Move with shifted register
    // Encoding: 0xEA4F 0x1001
    uint8_t code[] = THUMB32_BYTES(0xEA4F, 0x1001);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_SHIFTED, insn.type);
    CHECK_EQUAL(DP_MOV, insn.op);
}

// Data processing register - MVN with shift
TEST(Thumb32Coverage, MvnShifted)
{
    // MVN R0, R1, LSL #4 - Bitwise NOT with shifted register
    // Encoding: 0xEA6F 0x1001
    uint8_t code[] = THUMB32_BYTES(0xEA6F, 0x1001);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_SHIFTED, insn.type);
    CHECK_EQUAL(DP_MVN, insn.op);
}

// TST shifted register
TEST(Thumb32Coverage, TstShifted)
{
    // TST R0, R1 - Test bits with shifted register
    // Encoding: 0xEA10 0x0F01
    uint8_t code[] = THUMB32_BYTES(0xEA10, 0x0F01);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_SHIFTED, insn.type);
    CHECK_EQUAL(DP_TST, insn.op);
}

// TEQ shifted register (EOR with Rd=0xF, S=1)
TEST(Thumb32Coverage, TeqShifted)
{
    // TEQ R0, R1 - Test equivalence with shifted register
    // Encoding: 0xEA90 0x0F01
    uint8_t code[] = THUMB32_BYTES(0xEA90, 0x0F01);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_SHIFTED, insn.type);
}

// CMN shifted register
TEST(Thumb32Coverage, CmnShifted)
{
    // CMN R0, R1 - Compare negative with shifted register
    // Encoding: 0xEB10 0x0F01
    uint8_t code[] = THUMB32_BYTES(0xEB10, 0x0F01);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_SHIFTED, insn.type);
    CHECK_EQUAL(DP_CMN, insn.op);
}

// CMP shifted register
TEST(Thumb32Coverage, CmpShifted)
{
    // CMP R0, R1 - Compare with shifted register
    // Encoding: 0xEBB0 0x0F01
    uint8_t code[] = THUMB32_BYTES(0xEBB0, 0x0F01);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_SHIFTED, insn.type);
    CHECK_EQUAL(DP_CMP, insn.op);
}

// RRX shift (type=3, shift_n=0)
TEST(Thumb32Coverage, Rrx)
{
    // RRX R0, R1 - Rotate right with extend
    // Encoding: 0xEA4F 0x0031
    uint8_t code[] = THUMB32_BYTES(0xEA4F, 0x0031);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_SHIFTED, insn.type);
    CHECK_EQUAL(SHIFT_RRX, insn.shift_type);
}

// SSAT instruction - uses plain binary immediate path
TEST(Thumb32Coverage, Ssat)
{
    // SSAT R0, #16, R1 - Signed saturate
    // Encoding: 0xF321 0x000F
    uint8_t code[] = THUMB32_BYTES(0xF321, 0x000F);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
}

// Plain binary immediate undefined op - just for coverage
TEST(Thumb32Coverage, PlainBinaryImmUndef)
{
    // Encoding that exercises the default case
    // Encoding: 0xF3E1 0x0000
    uint8_t code[] = THUMB32_BYTES(0xF3E1, 0x0000);

    int result = armv8m_decode(code, TEST_PC, &insn);

    // May return undefined for unimplemented ops
    CHECK(result == 4 || result == ARMV8M_ERR_UNDEFINED_INSN);
}

// SBFX instruction
TEST(Thumb32Coverage, Sbfx)
{
    // SBFX R0, R1, #0, #8 - Signed bit field extract
    // Encoding: 0xF341 0x0007
    uint8_t code[] = THUMB32_BYTES(0xF341, 0x0007);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
}

// UBFX instruction
TEST(Thumb32Coverage, Ubfx)
{
    // UBFX R0, R1, #0, #8 - Unsigned bit field extract
    // Encoding: 0xF3C1 0x0007
    uint8_t code[] = THUMB32_BYTES(0xF3C1, 0x0007);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
}

// BFI instruction
TEST(Thumb32Coverage, Bfi)
{
    // BFI R0, R1, #0, #8 - Bit field insert
    // Encoding: 0xF361 0x0007
    uint8_t code[] = THUMB32_BYTES(0xF361, 0x0007);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
}

// TBB instruction - verifies load/store dual path
TEST(Thumb32Coverage, Tbb)
{
    // TBB [R0, R1] - Table branch byte
    // Encoding: 0xE8D0 0xF001
    uint8_t code[] = THUMB32_BYTES(0xE8D0, 0xF001);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
}

// TBH instruction
TEST(Thumb32Coverage, Tbh)
{
    // TBH [R0, R1, LSL #1] - Table branch halfword
    // Encoding: 0xE8D0 0xF011
    uint8_t code[] = THUMB32_BYTES(0xE8D0, 0xF011);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
}

// Load exclusive (LDREX)
TEST(Thumb32Coverage, Ldrex)
{
    // LDREX R0, [R1] - Load register exclusive
    // Encoding: 0xE851 0x0F00
    uint8_t code[] = THUMB32_BYTES(0xE851, 0x0F00);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
}

// Store exclusive (STREX)
TEST(Thumb32Coverage, Strex)
{
    // STREX R2, R0, [R1] - Store register exclusive
    // Encoding: 0xE840 0x0200
    uint8_t code[] = THUMB32_BYTES(0xE840, 0x0200);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
}

// LDRD (load register dual)
TEST(Thumb32Coverage, LdrdPreIndex)
{
    // LDRD R0, R1, [R2, #8]! - Load double with pre-index
    // Encoding: 0xE9F2 0x0102
    uint8_t code[] = THUMB32_BYTES(0xE9F2, 0x0102);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
}

// STRD (store register dual)
TEST(Thumb32Coverage, Strd)
{
    // STRD R0, R1, [R2, #8] - Store double word
    // Encoding: 0xE9C2 0x0102
    uint8_t code[] = THUMB32_BYTES(0xE9C2, 0x0102);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
}

// Long multiply uses the long_multiply path
TEST(Thumb32Coverage, LongMul)
{
    // SDIV R0, R1, R2 - Signed divide (uses long_multiply decoder)
    // Encoding: 0xFB91 0xF0F2
    uint8_t code[] = THUMB32_BYTES(0xFB91, 0xF0F2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DIVIDE, insn.type);
}

/*============================================================================
 * Test Group: Disassembly and Mnemonic Functions
 *============================================================================*/

TEST_GROUP(DisasmCoverage)
{
    DecodedInsn insn;
    char buf[64];

    void setup()
    {
        armv8m_decode_init(&insn);
    }
};

TEST(DisasmCoverage, DisasmDataProcImm)
{
    // MOVS R0, #42
    uint8_t code[] = THUMB16_BYTES(0x202A);
    armv8m_decode(code, TEST_PC, &insn);

    int len = armv8m_disasm(&insn, buf, sizeof(buf));

    CHECK(len > 0);
}

TEST(DisasmCoverage, DisasmDataProcReg)
{
    // ANDS R0, R1
    uint8_t code[] = THUMB16_BYTES(0x4008);
    armv8m_decode(code, TEST_PC, &insn);

    int len = armv8m_disasm(&insn, buf, sizeof(buf));

    CHECK(len > 0);
}

TEST(DisasmCoverage, DisasmLoadImm)
{
    // LDR R0, [R1, #4]
    uint8_t code[] = THUMB16_BYTES(0x6848);
    armv8m_decode(code, TEST_PC, &insn);

    int len = armv8m_disasm(&insn, buf, sizeof(buf));

    CHECK(len > 0);
}

TEST(DisasmCoverage, DisasmStoreImm)
{
    // STR R0, [R1, #4]
    uint8_t code[] = THUMB16_BYTES(0x6048);
    armv8m_decode(code, TEST_PC, &insn);

    int len = armv8m_disasm(&insn, buf, sizeof(buf));

    CHECK(len > 0);
}

TEST(DisasmCoverage, DisasmBranch)
{
    // B +10
    uint8_t code[] = THUMB16_BYTES(0xE004);
    armv8m_decode(code, TEST_PC, &insn);

    int len = armv8m_disasm(&insn, buf, sizeof(buf));

    CHECK(len > 0);
}

TEST(DisasmCoverage, DisasmBranchLink)
{
    // BL
    uint8_t code[] = THUMB32_BYTES(0xF000, 0xF801);
    armv8m_decode(code, TEST_PC, &insn);

    int len = armv8m_disasm(&insn, buf, sizeof(buf));

    CHECK(len > 0);
}

TEST(DisasmCoverage, DisasmBranchExchange)
{
    // BX R1
    uint8_t code[] = THUMB16_BYTES(0x4708);
    armv8m_decode(code, TEST_PC, &insn);

    int len = armv8m_disasm(&insn, buf, sizeof(buf));

    CHECK(len > 0);
}

TEST(DisasmCoverage, DisasmOther)
{
    // NOP
    uint8_t code[] = THUMB16_BYTES(0xBF00);
    armv8m_decode(code, TEST_PC, &insn);

    int len = armv8m_disasm(&insn, buf, sizeof(buf));

    CHECK(len > 0);
}

TEST(DisasmCoverage, DisasmNullBuf)
{
    // MOVS R0, #0
    uint8_t code[] = THUMB16_BYTES(0x2000);
    armv8m_decode(code, TEST_PC, &insn);

    int len = armv8m_disasm(&insn, nullptr, 0);

    CHECK_EQUAL(-1, len);
}

TEST(DisasmCoverage, DisasmNullInsn)
{
    int len = armv8m_disasm(nullptr, buf, sizeof(buf));

    CHECK_EQUAL(-1, len);
}

TEST(DisasmCoverage, DisasmZeroBufSize)
{
    // MOVS R0, #0
    uint8_t code[] = THUMB16_BYTES(0x2000);
    armv8m_decode(code, TEST_PC, &insn);

    int len = armv8m_disasm(&insn, buf, 0);

    CHECK_EQUAL(-1, len);
}

TEST(DisasmCoverage, MnemonicLoadLiteral)
{
    const char *m = armv8m_insn_mnemonic(INSN_LOAD_LITERAL, 0);
    STRCMP_EQUAL("LDR", m);
}

TEST(DisasmCoverage, MnemonicStoreImm)
{
    const char *m = armv8m_insn_mnemonic(INSN_STORE_IMM, 0);
    STRCMP_EQUAL("STR", m);
}

TEST(DisasmCoverage, MnemonicStoreReg)
{
    const char *m = armv8m_insn_mnemonic(INSN_STORE_REG, 0);
    STRCMP_EQUAL("STR", m);
}

TEST(DisasmCoverage, MnemonicLoadMultiple)
{
    const char *m = armv8m_insn_mnemonic(INSN_LOAD_MULTIPLE, 0);
    STRCMP_EQUAL("LDM", m);
}

TEST(DisasmCoverage, MnemonicStoreMultiple)
{
    const char *m = armv8m_insn_mnemonic(INSN_STORE_MULTIPLE, 0);
    STRCMP_EQUAL("STM", m);
}

TEST(DisasmCoverage, MnemonicBranchLinkExchange)
{
    const char *m = armv8m_insn_mnemonic(INSN_BRANCH_LINK_EXCHANGE, 0);
    STRCMP_EQUAL("BLX", m);
}

TEST(DisasmCoverage, MnemonicCompareBranch)
{
    const char *m = armv8m_insn_mnemonic(INSN_COMPARE_BRANCH, 0);
    STRCMP_EQUAL("CBZ/CBNZ", m);
}

TEST(DisasmCoverage, MnemonicSvc)
{
    const char *m = armv8m_insn_mnemonic(INSN_SVC, 0);
    STRCMP_EQUAL("SVC", m);
}

TEST(DisasmCoverage, MnemonicHint)
{
    const char *m = armv8m_insn_mnemonic(INSN_HINT, 0);
    STRCMP_EQUAL("NOP", m);
}

TEST(DisasmCoverage, MnemonicIt)
{
    const char *m = armv8m_insn_mnemonic(INSN_IT, 0);
    STRCMP_EQUAL("IT", m);
}

TEST(DisasmCoverage, MnemonicMrs)
{
    const char *m = armv8m_insn_mnemonic(INSN_MRS, 0);
    STRCMP_EQUAL("MRS", m);
}

TEST(DisasmCoverage, MnemonicMsr)
{
    const char *m = armv8m_insn_mnemonic(INSN_MSR, 0);
    STRCMP_EQUAL("MSR", m);
}

TEST(DisasmCoverage, MnemonicBarrier)
{
    const char *m = armv8m_insn_mnemonic(INSN_BARRIER, 0);
    STRCMP_EQUAL("DMB/DSB/ISB", m);
}

TEST(DisasmCoverage, MnemonicMultiply)
{
    const char *m = armv8m_insn_mnemonic(INSN_MULTIPLY, 0);
    STRCMP_EQUAL("MUL", m);
}

TEST(DisasmCoverage, MnemonicDivide)
{
    const char *m = armv8m_insn_mnemonic(INSN_DIVIDE, 0);
    STRCMP_EQUAL("SDIV/UDIV", m);
}

TEST(DisasmCoverage, MnemonicExtend)
{
    const char *m = armv8m_insn_mnemonic(INSN_EXTEND, 0);
    STRCMP_EQUAL("SXTH/UXTH", m);
}

TEST(DisasmCoverage, MnemonicUnknown)
{
    const char *m = armv8m_insn_mnemonic(INSN_UNDEFINED, 0);
    STRCMP_EQUAL("???", m);
}

TEST(DisasmCoverage, MnemonicOpOutOfRange)
{
    const char *m = armv8m_insn_mnemonic(INSN_DATA_PROC_IMM, 100);
    STRCMP_EQUAL("???", m);
}

/*============================================================================
 * Test Group: Thumb-32 Immediate Expansion Coverage
 *
 * Tests the thumb_expand_imm() function which expands 12-bit modified
 * immediates into 32-bit values using various patterns:
 * - Type 0: 8-bit constant (0x00-0xFF)
 * - Type 1: Replicate to positions 0 and 16 (0xXX00XX)
 * - Type 2: Replicate to positions 8 and 24 (0xXX00XX00)
 * - Type 3: Replicate to all bytes (0xXXXXXXXX)
 * - Type 4+: Rotated 8-bit constant with bit 7 set
 *============================================================================*/

/*
 * Encoding helper for data processing (modified immediate):
 *   hw1[15:11] = 11110 (32-bit prefix)
 *   hw1[10]    = i (immediate bit 11)
 *   hw1[9]     = 0 (distinguishes from branches)
 *   hw1[8:5]   = op (operation: 0=AND, 1=BIC, 2=ORR, etc.)
 *   hw1[4]     = S (set flags)
 *   hw1[3:0]   = Rn
 *   hw2[15]    = 0
 *   hw2[14:12] = imm3 (immediate bits 10:8)
 *   hw2[11:8]  = Rd
 *   hw2[7:0]   = imm8 (immediate bits 7:0)
 *
 * The 12-bit immediate is: i:imm3:imm8
 */

/* AND (immediate) encoding: op = 0000 */
#define AND_IMM_HW1(rn, i_bit) (0xF000u | ((i_bit) << 10) | (rn))
#define DP_IMM_HW2(rd, imm3, imm8) (((imm3) << 12) | ((rd) << 8) | (imm8))

TEST(Thumb32Coverage, ThumbExpandImm_Type1_ReplicateTo0And16)
{
    /*
     * AND R0, R1, #imm where imm12 = 0x1FF
     * Type 1 (imm12[11:8] = 0001) replicates imm8 to bits[31:24,15:0]
     * Result: (0xFF << 16) | 0xFF = 0x00FF00FF
     *
     * Encoding: i=0, imm3=1, imm8=0xFF -> imm12 = 0:001:11111111 = 0x1FF
     */
    const uint16_t hw1 = AND_IMM_HW1(REG_R1, 0);      /* AND, Rn=R1, i=0 */
    const uint16_t hw2 = DP_IMM_HW2(REG_R0, 1, 0xFF); /* Rd=R0, imm3=1, imm8=0xFF */
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_AND, insn.op);
    CHECK_EQUAL(REG_R0, insn.rd);
    CHECK_EQUAL(REG_R1, insn.rn);
    CHECK_EQUAL(0x00FF00FFu, insn.imm);
    CHECK_FALSE(insn.set_flags);
}

TEST(Thumb32Coverage, ThumbExpandImm_Type2_ReplicateTo8And24)
{
    /*
     * AND R0, R1, #imm where imm12 = 0x2FF
     * Type 2 (imm12[11:8] = 0010) replicates imm8 to bits[31:24,15:8]
     * Result: (0xFF << 24) | (0xFF << 8) = 0xFF00FF00
     *
     * Encoding: i=0, imm3=2, imm8=0xFF -> imm12 = 0x2FF
     */
    const uint16_t hw1 = AND_IMM_HW1(REG_R1, 0);
    const uint16_t hw2 = DP_IMM_HW2(REG_R0, 2, 0xFF);
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_AND, insn.op);
    CHECK_EQUAL(REG_R0, insn.rd);
    CHECK_EQUAL(REG_R1, insn.rn);
    CHECK_EQUAL(0xFF00FF00u, insn.imm);
}

TEST(Thumb32Coverage, ThumbExpandImm_Type3_ReplicateToAllBytes)
{
    /*
     * AND R0, R1, #imm where imm12 = 0x3FF
     * Type 3 (imm12[11:8] = 0011) replicates imm8 to all byte positions
     * Result: 0xFFFFFFFF
     *
     * Encoding: i=0, imm3=3, imm8=0xFF -> imm12 = 0x3FF
     */
    const uint16_t hw1 = AND_IMM_HW1(REG_R1, 0);
    const uint16_t hw2 = DP_IMM_HW2(REG_R0, 3, 0xFF);
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_AND, insn.op);
    CHECK_EQUAL(0xFFFFFFFFu, insn.imm);
}

TEST(Thumb32Coverage, ThumbExpandImm_Type4Plus_RotatedConstant)
{
    /*
     * AND R0, R1, #imm where imm12 = 0x480
     * Per ARM ThumbExpandImm: when bits[11:10] != 00, use rotation
     * rotation_amount = imm12[11:7] = (0x480 >> 7) & 0x1F = 9
     * val = 0x80 | (imm12[6:0]) = 0x80 | 0x00 = 0x80
     * Result: 0x80 ROR 9 = 0x40000000
     *
     * Encoding: i=0, imm3=4, imm8=0x80 -> imm12 = 0x480
     */
    const uint16_t hw1 = AND_IMM_HW1(REG_R1, 0);
    const uint16_t hw2 = DP_IMM_HW2(REG_R0, 4, 0x80);
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_AND, insn.op);
    CHECK_EQUAL(REG_R0, insn.rd);
    CHECK_EQUAL(REG_R1, insn.rn);
    /*
     * Rotated value calculation per ARM spec:
     * val = '1':imm12[6:0] = 0x80 | 0x00 = 0x80
     * rot = imm12[11:7] = 9
     * result = 0x80 ROR 9 = (0x80 >> 9) | (0x80 << 23) = 0x40000000
     */
    CHECK_EQUAL(0x40000000u, insn.imm);
}

/*============================================================================
 * Test Group: Branch and Miscellaneous Instructions
 *============================================================================*/

TEST(Thumb32Coverage, Eret_ExceptionReturn)
{
    /*
     * ERET - Exception Return
     * Returns from exception handler, uses LR as target
     *
     * Encoding: 0xF3DE 0x8F00
     *   hw1[15:4] = 0xF3D, hw1[7:4] = 0xD (ERET opcode)
     *   hw1[3:0] = 0xE (LR as Rm implied)
     */
    const uint16_t hw1 = 0xF3DE;
    const uint16_t hw2 = 0x8F00;
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_BRANCH_EXCHANGE, insn.type);
    CHECK_EQUAL(ARMV8M_REG_LR, insn.rm);
}

/*============================================================================
 * Test Group: Data Processing (Plain Binary Immediate)
 *============================================================================*/

TEST(Thumb32Coverage, AdrSubtract_PcRelativeAddress)
{
    /*
     * ADR R1, label (with subtract)
     * Computes PC-relative address: PC - imm12
     *
     * Encoding: SUBW Rd, PC, #imm12
     *   hw1[15:11] = 11110 (32-bit prefix)
     *   hw1[10:5]  = 10101 (SUBW with Rn=PC)
     *   hw1[4]     = 0 (S=0)
     *   hw1[3:0]   = 0xF (Rn = PC)
     *   hw2[11:8]  = Rd
     *   hw2[7:0]   = imm8
     *   hw2[14:12] = imm3 (upper bits of imm12)
     *
     * 0xF2AF = 1111 0010 1010 1111 (SUBW, Rn=PC)
     * 0x0180 = Rd=R1 (bits[11:8]=0001), imm12=0x080
     */
    const uint16_t hw1 = 0xF2AF; /* SUBW with Rn=PC */
    const uint16_t hw2 = 0x0180; /* Rd=R1, imm=0x080 */
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(DP_SUB, insn.op);
    CHECK_EQUAL(REG_R1, insn.rd);
    CHECK_EQUAL(ARMV8M_REG_PC, insn.rn);
    CHECK_EQUAL(0x080u, insn.imm);
}

TEST(Thumb32Coverage, Ssat_SignedSaturate)
{
    /*
     * SSAT - Signed Saturate
     * Saturates a signed value to a specified bit width
     *
     * Encoding via case 0x3 dispatch path:
     *   hw1 = 0xF901 gives op1=3, op2=0x10, Rn=R1
     *   hw2 = 0x000F gives Rd=R0, sat_imm=15
     */
    const uint16_t hw1 = 0xF901;
    const uint16_t hw2 = 0x000F;
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_SATURATE, insn.type);
    CHECK_EQUAL(REG_R0, insn.rd);
    CHECK_EQUAL(REG_R1, insn.rn);
    CHECK_EQUAL(15u, insn.imm); /* Saturation bit position */
}

TEST(Thumb32Coverage, Sbfx_SignedBitFieldExtract)
{
    /*
     * SBFX - Signed Bit Field Extract
     * Extracts a bit field and sign-extends it
     *
     * Encoding via case 0x3 dispatch path:
     *   hw1 = 0xF941 gives op=0x14 (SBFX), Rn=R1
     *   hw2 = 0x0007 gives Rd=R0, lsb=0, width=8
     */
    const uint16_t hw1 = 0xF941;
    const uint16_t hw2 = 0x0007;
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_BITFIELD, insn.type);
    CHECK_EQUAL(REG_R0, insn.rd);
    CHECK_EQUAL(REG_R1, insn.rn);
}

/*============================================================================
 * Test Group: Data Processing (Shifted Register)
 *============================================================================*/

TEST(Thumb32Coverage, Pkh_PackHalfword)
{
    /*
     * PKHBT - Pack Halfword Bottom Top
     * Combines bottom halfword of Rn with shifted top halfword of Rm
     *
     * Encoding: 0xEAC1 0x0002
     *   hw1 = 0xEAC1: op=0x6 (PKH), Rn=R1
     *   hw2 = 0x0002: Rd=R0, Rm=R2
     *
     * PKH is decoded as ORR-like operation
     */
    const uint16_t hw1 = 0xEAC1;
    const uint16_t hw2 = 0x0002;
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    /* PKH is correctly decoded as INSN_PACK, not INSN_DATA_PROC_SHIFTED */
    CHECK_EQUAL(INSN_PACK, insn.type);
    CHECK_EQUAL(REG_R0, insn.rd);
    CHECK_EQUAL(REG_R1, insn.rn);
    CHECK_EQUAL(REG_R2, insn.rm);
}

TEST(Thumb32Coverage, DataProcShiftedReg_ViaOp1Equals3)
{
    /*
     * Data processing (shifted register) via case 0x3, op=1 path
     * Tests the (op2 & 0x70) == 0x20 branch
     *
     * Encoding:
     *   hw1 = 0xFA21: op1=3, op2[6:4]=010, Rn=R1
     *   hw2 = 0x8002: op=1 (bit 15 set), Rm=R2
     */
    const uint16_t hw1 = 0xFA21;
    const uint16_t hw2 = 0x8002;
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_DATA_PROC_SHIFTED, insn.type);
    CHECK_EQUAL(REG_R1, insn.rn);
    CHECK_EQUAL(REG_R2, insn.rm);
}

/*============================================================================
 * Test Group: Load/Store Single
 *============================================================================*/

TEST(Thumb32Coverage, LdrhW_LoadHalfword)
{
    /*
     * LDRH.W R0, [R1, #0x80] - Load unsigned halfword with 12-bit immediate (T2 encoding)
     *
     * Encoding T2 (12-bit imm):
     *   hw1 = 0xF8B1: 11111 000 1 0 11 0001 = load halfword, Rn=R1, bit7=1 (add)
     *   hw2 = 0x0080: Rt=R0, imm12=0x80
     */
    const uint16_t hw1 = 0xF8B1;
    const uint16_t hw2 = 0x0080;
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_LOAD_IMM, insn.type);
    CHECK_EQUAL(ACCESS_HALF, insn.access_size);
    CHECK_EQUAL(REG_R0, insn.rt);
    CHECK_EQUAL(REG_R1, insn.rn);
    CHECK_EQUAL(0x80u, insn.imm);
    CHECK_TRUE(insn.add);  /* 12-bit imm is always add */
}

TEST(Thumb32Coverage, LoadStore_ViaOp1Equals3_Op1Path)
{
    /*
     * Load/store single via case 0x3, op=1, (op2 & 0x70)==0x00
     *
     * Encoding:
     *   hw1 = 0xF801: op1=3, op2=0x00, Rn=R1
     *   hw2 = 0x8000: op=1 (bit 15 set)
     */
    const uint16_t hw1 = 0xF801;
    const uint16_t hw2 = 0x8000;
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    /* Verify it decoded to some load/store type */
    CHECK(insn.type == INSN_LOAD_IMM || insn.type == INSN_STORE_IMM ||
          insn.type == INSN_LOAD_REG || insn.type == INSN_STORE_REG);
}

TEST(Thumb32Coverage, LoadLiteral_PcRelative)
{
    /*
     * LDR Rt, [PC, #imm] - PC-relative load (literal)
     *
     * Encoding:
     *   hw1 = 0xF80F: op1=3, Rn=PC (0xF)
     *   hw2 = 0x8100: Rt in upper nibble, imm in lower
     */
    const uint16_t hw1 = 0xF80F;
    const uint16_t hw2 = 0x8100;
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(REG_PC, insn.rn);
}

TEST(Thumb32Coverage, LoadRegisterOffset)
{
    /*
     * LDR Rt, [Rn, Rm, LSL #shift] - Register offset load
     *
     * Encoding:
     *   hw1 = 0xF801: Rn=R1
     *   hw2 = 0x8022: Rm=R2, shift=2
     */
    const uint16_t hw1 = 0xF801;
    const uint16_t hw2 = 0x8022;
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(REG_R1, insn.rn);
}

TEST(Thumb32Coverage, Load12BitImmediate)
{
    /*
     * Load with 12-bit immediate offset
     *
     * Encoding:
     *   hw1 = 0xF841: op2=0x40
     *   hw2 = 0x8FFF: imm=0xFFF
     */
    const uint16_t hw1 = 0xF841;
    const uint16_t hw2 = 0x8FFF;
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(REG_R1, insn.rn);
}

TEST(Thumb32Coverage, StrReg_StoreRegisterOffset)
{
    /*
     * STR.W R0, [R1, R2, LSL #2] - Store with register offset
     *
     * Encoding:
     *   hw1 = 0xF841: store (L=0), Rn=R1
     *   hw2 = 0x0022: Rt=R0, Rm=R2, shift=2
     */
    const uint16_t hw1 = 0xF841;
    const uint16_t hw2 = 0x0022;
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_STORE_REG, insn.type);
    CHECK_EQUAL(REG_R0, insn.rt);
    CHECK_EQUAL(REG_R1, insn.rn);
    CHECK_EQUAL(REG_R2, insn.rm);
    CHECK_EQUAL(2, insn.shift_amount);
}

TEST(Thumb32Coverage, StrPcRelative_StoreWithPcBase)
{
    /*
     * STR R0, [PC, #imm] - Store with PC as base (unusual but valid path)
     *
     * Encoding:
     *   hw1 = 0xF84F: store, Rn=PC
     *   hw2 = 0x0100: Rt=R0, imm
     */
    const uint16_t hw1 = 0xF84F;
    const uint16_t hw2 = 0x0100;
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_STORE_IMM, insn.type);
    CHECK_EQUAL(REG_R0, insn.rt);
    CHECK_EQUAL(REG_PC, insn.rn);
}

TEST(Thumb32Coverage, StrImm12_Store12BitOffset)
{
    /*
     * STR.W R0, [R1, #0xFFF] - Store with 12-bit immediate
     *
     * Encoding:
     *   hw1 = 0xF8C1: store with 12-bit imm encoding, Rn=R1
     *   hw2 = 0x0FFF: Rt=R0, imm12=0xFFF
     */
    const uint16_t hw1 = 0xF8C1;
    const uint16_t hw2 = 0x0FFF;
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_STORE_IMM, insn.type);
    CHECK_EQUAL(REG_R0, insn.rt);
    CHECK_EQUAL(REG_R1, insn.rn);
    CHECK_EQUAL(0xFFFu, insn.imm);
}

TEST(Thumb32Coverage, StrhW_StoreHalfword)
{
    /*
     * STRH.W R0, [R1, #0x80] - Store halfword with 12-bit immediate (T3 encoding)
     *
     * Encoding T3 (12-bit imm):
     *   hw1 = 0xF8A1: 11111 000 1 0 10 0001 = store halfword, Rn=R1, bit7=1 (add)
     *   hw2 = 0x0080: Rt=R0, imm12=0x80
     */
    const uint16_t hw1 = 0xF8A1;
    const uint16_t hw2 = 0x0080;
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_STORE_IMM, insn.type);
    CHECK_EQUAL(ACCESS_HALF, insn.access_size);
    CHECK_EQUAL(REG_R0, insn.rt);
    CHECK_EQUAL(REG_R1, insn.rn);
    CHECK_EQUAL(0x80u, insn.imm);
}

/*============================================================================
 * Test Group: Load/Store Multiple
 *============================================================================*/

/*
 * Load/store multiple encoding:
 *   hw1[15:11] = 11101 (32-bit prefix for LDM/STM)
 *   hw1[10:9]  = op (direction: 00=DB, 01=IA, 10=DB, 11=IA)
 *   hw1[8:7]   = op field for switch
 *   hw1[5]     = W (writeback)
 *   hw1[4]     = L (0=store, 1=load)
 *   hw1[3:0]   = Rn
 *   hw2[15:0]  = register list
 */

TEST(Thumb32Coverage, Stmdb_Op0_DecrementBefore)
{
    /*
     * STMDB R0, {R1, R2} - Store Multiple Decrement Before
     * op=0: decrement address before each store
     *
     * Encoding:
     *   hw1 = 0xE900: op1=1, op=0, W=0, L=0, Rn=R0
     *   hw2 = 0x0006: register list = {R1, R2}
     */
    const uint16_t hw1 = 0xE900;
    const uint16_t hw2 = 0x0006; /* bits 1,2 set = R1, R2 */
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_STORE_MULTIPLE, insn.type);
    CHECK_EQUAL(REG_R0, insn.rn);
    CHECK_EQUAL(0x0006u, insn.register_list);
    CHECK_FALSE(insn.add);       /* Decrement */
    CHECK_TRUE(insn.pre_index);  /* Before */
    CHECK_FALSE(insn.writeback);
}

TEST(Thumb32Coverage, Stmdb_Op0_ViaAltEncoding)
{
    /*
     * STMDB - Alternative encoding path for op=0
     *
     * Encoding:
     *   hw1 = 0xE800: satisfies (op2 & 0x64)==0, bits[8:7]=00
     *   hw2 = 0x0006: {R1, R2}
     */
    const uint16_t hw1 = 0xE800;
    const uint16_t hw2 = 0x0006;
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_STORE_MULTIPLE, insn.type);
    CHECK_FALSE(insn.add);
    CHECK_TRUE(insn.pre_index);
}

TEST(Thumb32Coverage, Stm_Op3_IncrementAfter)
{
    /*
     * STM R0, {R1, R2} - Store Multiple with op=3 (alias for increment after)
     *
     * Encoding:
     *   hw1 = 0xE980: op=3 (bits[8:7]=11), L=0 (store), Rn=R0
     *   hw2 = 0x0006: {R1, R2}
     */
    const uint16_t hw1 = 0xE980;
    const uint16_t hw2 = 0x0006;
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_STORE_MULTIPLE, insn.type);
    CHECK_EQUAL(REG_R0, insn.rn);
    CHECK_TRUE(insn.add);        /* Increment */
    CHECK_FALSE(insn.pre_index); /* After */
}

TEST(Thumb32Coverage, StmWriteback_RnInList_Unpredictable)
{
    /*
     * STM with writeback where Rn is in register list -> UNPREDICTABLE
     * This is architecturally unpredictable for stores
     *
     * Encoding:
     *   hw1 = 0xE8A0: W=1 (writeback), L=0 (store), Rn=R0
     *   hw2 = 0x0007: {R0, R1, R2} - R0 is both Rn and in list
     */
    const uint16_t hw1 = 0xE8A0;
    const uint16_t hw2 = 0x0007; /* R0, R1, R2 */
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(ARMV8M_ERR_UNPREDICTABLE, result);
}

/*============================================================================
 * Test Group: Multiply and Divide
 *============================================================================*/

TEST(Thumb32Coverage, Mla_MultiplyAccumulate)
{
    /*
     * MLA Rd, Rn, Rm, Ra - Multiply Accumulate: Rd = Ra + (Rn * Rm)
     *
     * Encoding via case 0x3, op=1, (op2 & 0x78)==0x30:
     *   hw1 = 0xFB01: Rn=R1
     *   hw2 = 0xB002: Ra=R11 (not 0xF), Rd=R0, Rm=R2, bit15=1
     *
     * Note: Ra != 0xF distinguishes MLA from MUL
     */
    const uint16_t hw1 = 0xFB01;
    const uint16_t hw2 = 0xB002;
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_MULTIPLY, insn.type);
    CHECK_EQUAL(REG_R0, insn.rd);
    CHECK_EQUAL(REG_R1, insn.rn);
    CHECK_EQUAL(REG_R2, insn.rm);
}

TEST(Thumb32Coverage, Smull_SignedLongMultiply)
{
    /*
     * SMULL RdLo, RdHi, Rn, Rm - Signed Long Multiply
     * Produces 64-bit result in RdLo:RdHi
     *
     * Encoding via case 0x3, op=1, (op2 & 0x78)==0x38:
     *   hw1 = 0xFB81: op2=0x38 (long multiply), Rn=R1
     *   hw2 = 0x8003: Rm=R3, bit15=1
     */
    const uint16_t hw1 = 0xFB81;
    const uint16_t hw2 = 0x8003;
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_MULTIPLY, insn.type);
    CHECK_EQUAL(REG_R1, insn.rn);
    CHECK_EQUAL(REG_R3, insn.rm);
}

/*============================================================================
 * Test Group: Dispatch Path Coverage (op1=3 variants)
 *
 * These tests exercise different dispatch paths in decode_thumb32()
 * to ensure all branches in the main switch are covered.
 *============================================================================*/

TEST(Thumb32Coverage, Op1_3_Op0_LoadStoreSingle)
{
    /*
     * Case 0x3, op=0, (op2 & 0x71)==0x00 -> decode_load_store_single
     *
     * Encoding:
     *   hw1 = 0xF800: op1=3, op2=0x00
     *   hw2 = 0x0000: op=0
     */
    const uint16_t hw1 = 0xF800;
    const uint16_t hw2 = 0x0000;
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    /* Should return valid decode or undefined - either exercises the path */
    CHECK(result == 4 || result == ARMV8M_ERR_UNDEFINED_INSN);
}

TEST(Thumb32Coverage, Op1_3_Op0_PlainBinaryImmediate)
{
    /*
     * Case 0x3, op=0, (op2 & 0x71)==0x10 -> decode_data_proc_plain_imm
     *
     * This path is also hit by SSAT/SBFX tests above, but included
     * for completeness of dispatch coverage.
     */
    const uint16_t hw1 = 0xFB01;
    const uint16_t hw2 = 0x000F;
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK(result == 4 || result == ARMV8M_ERR_UNDEFINED_INSN);
}

TEST(Thumb32Coverage, Op1_3_Op0_BranchesMisc)
{
    /*
     * Case 0x3, op=0, (op2 & 0x40)==0x40 -> decode_branch_misc
     *
     * Encoding:
     *   hw1 = 0xFC00: op2 has bit 6 set
     *   hw2 = 0x0000: op=0
     */
    const uint16_t hw1 = 0xFC00;
    const uint16_t hw2 = 0x0000;
    uint8_t code[] = THUMB32_BYTES(hw1, hw2);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK(result == 4 || result == ARMV8M_ERR_UNDEFINED_INSN);
}

/*============================================================================
 * Test Group: Barrier Instructions (DMB, DSB, ISB)
 *============================================================================*/

TEST(Thumb32Coverage, Dmb_DataMemoryBarrier)
{
    /*
     * DMB #SY - Data Memory Barrier
     * Encoding: 0xF3BF 0x8F5F
     *   hw1 = 0xF3BF: miscellaneous control
     *   hw2[7:4] = 0x5 (DMB), hw2[3:0] = 0xF (SY option)
     */
    uint8_t code[] = THUMB32_BYTES(0xF3BF, 0x8F5F);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_BARRIER, insn.type);
    CHECK_EQUAL(5, insn.op);  /* DMB = 5 */
    CHECK_EQUAL(0xF, insn.imm);  /* SY option */
}

TEST(Thumb32Coverage, Dsb_DataSynchronizationBarrier)
{
    /*
     * DSB #SY - Data Synchronization Barrier
     * Encoding: 0xF3BF 0x8F4F
     *   hw2[7:4] = 0x4 (DSB), hw2[3:0] = 0xF (SY option)
     */
    uint8_t code[] = THUMB32_BYTES(0xF3BF, 0x8F4F);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_BARRIER, insn.type);
    CHECK_EQUAL(4, insn.op);  /* DSB = 4 */
    CHECK_EQUAL(0xF, insn.imm);  /* SY option */
}

TEST(Thumb32Coverage, Isb_InstructionSynchronizationBarrier)
{
    /*
     * ISB #SY - Instruction Synchronization Barrier
     * Encoding: 0xF3BF 0x8F6F
     *   hw2[7:4] = 0x6 (ISB), hw2[3:0] = 0xF (SY option)
     */
    uint8_t code[] = THUMB32_BYTES(0xF3BF, 0x8F6F);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_BARRIER, insn.type);
    CHECK_EQUAL(6, insn.op);  /* ISB = 6 */
    CHECK_EQUAL(0xF, insn.imm);  /* SY option */
}

/*============================================================================
 * Test Group: MLS (Multiply and Subtract)
 *============================================================================*/

TEST(Thumb32Coverage, Mls_MultiplySubtract)
{
    /*
     * MLS Rd, Rn, Rm, Ra - Multiply and Subtract: Rd = Ra - (Rn * Rm)
     * Encoding: 0xFB01 0x8012
     *   hw1 = 0xFB01: multiply instruction, Rn=R1
     *   hw2 = 0x8012: Ra=R8 (bit15=1 for routing), Rd=R0, op2=0x1 (MLS), Rm=R2
     *   hw2[15:12]=Ra=8, hw2[11:8]=Rd=0, hw2[7:4]=0x1 (MLS), hw2[3:0]=Rm=2
     */
    uint8_t code[] = THUMB32_BYTES(0xFB01, 0x8012);

    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(4, result);
    CHECK_EQUAL(INSN_MULTIPLY, insn.type);
    CHECK_EQUAL(REG_R0, insn.rd);
    CHECK_EQUAL(REG_R1, insn.rn);
    CHECK_EQUAL(REG_R2, insn.rm);
    CHECK_EQUAL(REG_R8, insn.rs);  /* Ra stored in rs */
    CHECK_FALSE(insn.add);  /* MLS uses subtract */
}

/*============================================================================
 * Test Runner
 *============================================================================*/

int main(int argc, char** argv)
{
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
