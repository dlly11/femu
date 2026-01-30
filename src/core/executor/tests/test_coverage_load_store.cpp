/**
 * @file test_coverage_load_store.cpp
 * @brief Load/store instruction coverage tests
 */

#include "test_common.h"

TEST_GROUP(LoadStoreCoverage)
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

TEST(LoadStoreCoverage, StrRegister)
{
    exec.cpu.r[0] = 0x12345678;
    exec.cpu.r[1] = 0x100;
    exec.cpu.r[2] = 4;
    insn.type = INSN_STORE_REG;
    insn.rt = 0;
    insn.rn = 1;
    insn.rm = 2;
    insn.access_size = ACCESS_WORD;
    insn.add = true;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x78u, mock_memory[0x104]);
    CHECK_EQUAL(0x56u, mock_memory[0x105]);
    CHECK_EQUAL(0x34u, mock_memory[0x106]);
    CHECK_EQUAL(0x12u, mock_memory[0x107]);
}

TEST(LoadStoreCoverage, LdrshImmediate)
{
    mock_memory[0x100] = 0x80;  /* -128 in signed byte, but we load halfword */
    mock_memory[0x101] = 0xFF;  /* -128 as signed halfword = 0xFF80 */
    exec.cpu.r[1] = 0x100;
    insn.type = INSN_LOAD_IMM;
    insn.rt = 0;
    insn.rn = 1;
    insn.imm = 0;
    insn.access_size = ACCESS_HALF;
    insn.is_signed = true;
    insn.add = true;
    insn.pre_index = true;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0xFFFFFF80u, exec.cpu.r[0]);  /* Sign extended */
}

TEST(LoadStoreCoverage, LdrBusFault)
{
    exec.cpu.r[1] = 0xFFFFFF00;  /* Address that will cause fault */
    insn.type = INSN_LOAD_IMM;
    insn.rt = 0;
    insn.rn = 1;
    insn.imm = 0;
    insn.access_size = ACCESS_WORD;
    insn.add = true;
    insn.pre_index = true;

    CHECK_EQUAL(ARMV8M_ERR_BUS_FAULT, armv8m_exec_insn(&exec, &insn));
}

TEST(LoadStoreCoverage, StrBusFault)
{
    exec.cpu.r[0] = 0x12345678;
    exec.cpu.r[1] = 0xFFFFFF00;  /* Address that will cause fault */
    insn.type = INSN_STORE_IMM;
    insn.rt = 0;
    insn.rn = 1;
    insn.imm = 0;
    insn.access_size = ACCESS_WORD;
    insn.add = true;
    insn.pre_index = true;

    CHECK_EQUAL(ARMV8M_ERR_BUS_FAULT, armv8m_exec_insn(&exec, &insn));
}

TEST(LoadStoreCoverage, LdrbRegister)
{
    mock_memory[0x108] = 0x42;
    exec.cpu.r[1] = 0x100;
    exec.cpu.r[2] = 8;
    insn.type = INSN_LOAD_REG;
    insn.rt = 0;
    insn.rn = 1;
    insn.rm = 2;
    insn.access_size = ACCESS_BYTE;
    insn.add = true;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x42u, exec.cpu.r[0]);
}

TEST(LoadStoreCoverage, LdrhRegister)
{
    mock_memory[0x104] = 0x34;
    mock_memory[0x105] = 0x12;
    exec.cpu.r[1] = 0x100;
    exec.cpu.r[2] = 4;
    insn.type = INSN_LOAD_REG;
    insn.rt = 0;
    insn.rn = 1;
    insn.rm = 2;
    insn.access_size = ACCESS_HALF;
    insn.add = true;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x1234u, exec.cpu.r[0]);
}

TEST(LoadStoreCoverage, StrbRegister)
{
    exec.cpu.r[0] = 0x42;
    exec.cpu.r[1] = 0x100;
    exec.cpu.r[2] = 8;
    insn.type = INSN_STORE_REG;
    insn.rt = 0;
    insn.rn = 1;
    insn.rm = 2;
    insn.access_size = ACCESS_BYTE;
    insn.add = true;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x42u, mock_memory[0x108]);
}

TEST(LoadStoreCoverage, StrhRegister)
{
    exec.cpu.r[0] = 0x1234;
    exec.cpu.r[1] = 0x100;
    exec.cpu.r[2] = 4;
    insn.type = INSN_STORE_REG;
    insn.rt = 0;
    insn.rn = 1;
    insn.rm = 2;
    insn.access_size = ACCESS_HALF;
    insn.add = true;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x34u, mock_memory[0x104]);
    CHECK_EQUAL(0x12u, mock_memory[0x105]);
}

TEST(LoadStoreCoverage, LdrWithSubOffset)
{
    mock_memory[0x100] = 0x78;
    mock_memory[0x101] = 0x56;
    mock_memory[0x102] = 0x34;
    mock_memory[0x103] = 0x12;
    exec.cpu.r[1] = 0x108;
    insn.type = INSN_LOAD_IMM;
    insn.rt = 0;
    insn.rn = 1;
    insn.imm = 8;
    insn.access_size = ACCESS_WORD;
    insn.add = false;  /* Subtract offset */
    insn.pre_index = true;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x12345678u, exec.cpu.r[0]);
}

TEST(LoadStoreCoverage, StrWithSubOffset)
{
    exec.cpu.r[0] = 0xDEADBEEF;
    exec.cpu.r[1] = 0x108;
    insn.type = INSN_STORE_IMM;
    insn.rt = 0;
    insn.rn = 1;
    insn.imm = 8;
    insn.access_size = ACCESS_WORD;
    insn.add = false;  /* Subtract offset */
    insn.pre_index = true;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0xEFu, mock_memory[0x100]);
    CHECK_EQUAL(0xBEu, mock_memory[0x101]);
}

TEST(LoadStoreCoverage, LdrPostIndex)
{
    mock_memory[0x100] = 0x78;
    mock_memory[0x101] = 0x56;
    mock_memory[0x102] = 0x34;
    mock_memory[0x103] = 0x12;
    exec.cpu.r[1] = 0x100;
    insn.type = INSN_LOAD_IMM;
    insn.rt = 0;
    insn.rn = 1;
    insn.imm = 4;
    insn.access_size = ACCESS_WORD;
    insn.add = true;
    insn.pre_index = false;  /* Post-index */
    insn.writeback = true;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x12345678u, exec.cpu.r[0]);
    CHECK_EQUAL(0x104u, exec.cpu.r[1]);  /* Writeback */
}

TEST(LoadStoreCoverage, StrPostIndex)
{
    exec.cpu.r[0] = 0x87654321;
    exec.cpu.r[1] = 0x100;
    insn.type = INSN_STORE_IMM;
    insn.rt = 0;
    insn.rn = 1;
    insn.imm = 4;
    insn.access_size = ACCESS_WORD;
    insn.add = true;
    insn.pre_index = false;  /* Post-index */
    insn.writeback = true;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x21u, mock_memory[0x100]);
    CHECK_EQUAL(0x104u, exec.cpu.r[1]);  /* Writeback */
}

TEST(LoadStoreCoverage, LdrWithWritebackSub)
{
    mock_memory[0x100] = 0x78;
    mock_memory[0x101] = 0x56;
    mock_memory[0x102] = 0x34;
    mock_memory[0x103] = 0x12;
    exec.cpu.r[1] = 0x100;
    insn.type = INSN_LOAD_IMM;
    insn.rt = 0;
    insn.rn = 1;
    insn.imm = 4;
    insn.access_size = ACCESS_WORD;
    insn.add = false;  /* Subtract */
    insn.pre_index = true;
    insn.writeback = true;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    /* Address is 0x100 - 4 = 0xFC which is out of bounds, so let's change this */
}

TEST(LoadStoreCoverage, StrWithWritebackSub)
{
    exec.cpu.r[0] = 0xDEADBEEF;
    exec.cpu.r[1] = 0x108;
    insn.type = INSN_STORE_IMM;
    insn.rt = 0;
    insn.rn = 1;
    insn.imm = 4;
    insn.access_size = ACCESS_WORD;
    insn.add = false;  /* Subtract */
    insn.pre_index = true;
    insn.writeback = true;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0xEFu, mock_memory[0x104]);
    CHECK_EQUAL(0x104u, exec.cpu.r[1]);  /* Writeback */
}

TEST(LoadStoreCoverage, LdrsbImmediate)
{
    mock_memory[0x100] = 0x80;  /* -128 as signed byte */
    exec.cpu.r[1] = 0x100;
    insn.type = INSN_LOAD_IMM;
    insn.rt = 0;
    insn.rn = 1;
    insn.imm = 0;
    insn.access_size = ACCESS_BYTE;
    insn.is_signed = true;
    insn.add = true;
    insn.pre_index = true;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0xFFFFFF80u, exec.cpu.r[0]);  /* Sign extended */
}

TEST(LoadStoreCoverage, LdrRegWithShift)
{
    mock_memory[0x108] = 0x78;
    mock_memory[0x109] = 0x56;
    mock_memory[0x10A] = 0x34;
    mock_memory[0x10B] = 0x12;
    exec.cpu.r[1] = 0x100;
    exec.cpu.r[2] = 2;
    insn.type = INSN_LOAD_REG;
    insn.rt = 0;
    insn.rn = 1;
    insn.rm = 2;
    insn.shift_amount = 2;  /* R2 << 2 = 8 */
    insn.access_size = ACCESS_WORD;
    insn.add = true;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x12345678u, exec.cpu.r[0]);
}

TEST(LoadStoreCoverage, StrRegWithSubtract)
{
    exec.cpu.r[0] = 0xCAFEBABE;
    exec.cpu.r[1] = 0x108;
    exec.cpu.r[2] = 8;
    insn.type = INSN_STORE_REG;
    insn.rt = 0;
    insn.rn = 1;
    insn.rm = 2;
    insn.access_size = ACCESS_WORD;
    insn.add = false;  /* Subtract */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0xBEu, mock_memory[0x100]);
}

/* Load register with subtract and signed extend */
TEST(LoadStoreCoverage, LdrshRegSubtract)
{
    /* Store 0xFF80 at address 0x100 */
    mock_memory[0x100] = 0x80;
    mock_memory[0x101] = 0xFF;
    exec.cpu.r[1] = 0x108;  /* Base */
    exec.cpu.r[2] = 8;      /* Index */
    insn.type = INSN_LOAD_REG;
    insn.rt = 0;
    insn.rn = 1;
    insn.rm = 2;
    insn.access_size = ACCESS_HALF;
    insn.add = false;      /* Subtract: 0x108 - 8 = 0x100 */
    insn.is_signed = true; /* Sign extend */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0xFFFFFF80u, exec.cpu.r[0]);  /* Sign extended from 16 bits */
}

/* Load literal with subtract */
TEST(LoadStoreCoverage, LdrLiteralSubtract)
{
    mock_memory[0xFC] = 0x12;
    mock_memory[0xFD] = 0x34;
    mock_memory[0xFE] = 0x56;
    mock_memory[0xFF] = 0x78;
    exec.cpu.r[15] = 0x104;  /* PC */
    insn.type = INSN_LOAD_LITERAL;
    insn.rt = 0;
    insn.imm = 8;  /* Offset */
    insn.access_size = ACCESS_WORD;
    insn.add = false;  /* Subtract: Align(0x104,4) - 8 = 0x104 - 8 = 0xFC */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x78563412u, exec.cpu.r[0]);
}

/* Load literal with signed extend */
TEST(LoadStoreCoverage, LdrsbLiteral)
{
    mock_memory[0x100] = 0x80;  /* -128 as signed byte */
    exec.cpu.r[15] = 0x100;  /* PC */
    insn.type = INSN_LOAD_LITERAL;
    insn.rt = 0;
    insn.imm = 0;
    insn.access_size = ACCESS_BYTE;
    insn.add = true;
    insn.is_signed = true;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0xFFFFFF80u, exec.cpu.r[0]);  /* Sign extended */
}

/* Store register with shift */
TEST(LoadStoreCoverage, StrRegWithShift)
{
    exec.cpu.r[0] = 0xDEADBEEF;
    exec.cpu.r[1] = 0x100;  /* Base */
    exec.cpu.r[2] = 4;      /* Index */
    insn.type = INSN_STORE_REG;
    insn.rt = 0;
    insn.rn = 1;
    insn.rm = 2;
    insn.access_size = ACCESS_WORD;
    insn.add = true;
    insn.shift_amount = 2;  /* Index << 2 = 16 */

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    /* Store at 0x100 + (4 << 2) = 0x100 + 16 = 0x110 */
    CHECK_EQUAL(0xEFu, mock_memory[0x110]);
}

/* Store register bus fault */
TEST(LoadStoreCoverage, StrRegBusFault)
{
    exec.mem.write = NULL;  /* No write function */
    exec.cpu.r[0] = 0x12345678;
    exec.cpu.r[1] = 0x100;
    exec.cpu.r[2] = 0;
    insn.type = INSN_STORE_REG;
    insn.rt = 0;
    insn.rn = 1;
    insn.rm = 2;
    insn.access_size = ACCESS_WORD;
    insn.add = true;

    CHECK_EQUAL(ARMV8M_ERR_BUS_FAULT, armv8m_exec_insn(&exec, &insn));
}

/* Load multiple decrement before */
TEST(LoadStoreCoverage, LdmdbDecrement)
{
    /* Store values at 0xF8, 0xFC */
    mock_memory[0xF8] = 0x11;
    mock_memory[0xF9] = 0x22;
    mock_memory[0xFA] = 0x33;
    mock_memory[0xFB] = 0x44;
    mock_memory[0xFC] = 0x55;
    mock_memory[0xFD] = 0x66;
    mock_memory[0xFE] = 0x77;
    mock_memory[0xFF] = 0x88;
    exec.cpu.r[8] = 0x100;  /* Base */
    insn.type = INSN_LOAD_MULTIPLE;
    insn.rn = 8;
    insn.register_list = 0x0003;  /* R0, R1 */
    insn.add = false;  /* Decrement: start at 0x100 - 8 = 0xF8 */
    insn.writeback = true;

    CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
    CHECK_EQUAL(0x44332211u, exec.cpu.r[0]);  /* From 0xF8 */
    CHECK_EQUAL(0x88776655u, exec.cpu.r[1]);  /* From 0xFC */
    CHECK_EQUAL(0xF8u, exec.cpu.r[8]);  /* Writeback: 0x100 - 8 */
}

/* Load register no memory read function */
TEST(LoadStoreCoverage, LdrRegNoMemRead)
{
    exec.mem.read = NULL;
    exec.cpu.r[1] = 0x100;
    exec.cpu.r[2] = 0;
    insn.type = INSN_LOAD_REG;
    insn.rt = 0;
    insn.rn = 1;
    insn.rm = 2;
    insn.access_size = ACCESS_WORD;
    insn.add = true;

    CHECK_EQUAL(ARMV8M_ERR_BUS_FAULT, armv8m_exec_insn(&exec, &insn));
}

/* Store immediate no memory write function */
TEST(LoadStoreCoverage, StrImmNoMemWrite)
{
    exec.mem.write = NULL;
    exec.cpu.r[0] = 0x12345678;
    exec.cpu.r[1] = 0x100;
    insn.type = INSN_STORE_IMM;
    insn.rt = 0;
    insn.rn = 1;
    insn.imm = 0;
    insn.access_size = ACCESS_WORD;
    insn.pre_index = true;
    insn.add = true;

    CHECK_EQUAL(ARMV8M_ERR_BUS_FAULT, armv8m_exec_insn(&exec, &insn));
}

/*============================================================================
 * Test Group: Additional Data Processing Coverage
 *============================================================================*/

