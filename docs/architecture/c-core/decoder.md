# Decoder Module

The decoder module parses raw instruction bytes into structured `DecodedInsn` representations.

## Purpose

- Decode Thumb-2 instruction bytes (16-bit and 32-bit)
- Extract operands, addressing modes, and flags
- Provide consistent interface for the executor

## API

```c
// include/arch/armv8m/armv8m_decoder.h

/**
 * Initialize a DecodedInsn structure.
 */
void armv8m_decode_init(DecodedInsn *insn);

/**
 * Decode an instruction.
 *
 * @param code Pointer to instruction bytes (little-endian)
 * @param pc Current program counter
 * @param insn Output: decoded instruction
 * @return Bytes consumed (2 or 4), or negative error code
 */
int armv8m_decode(const uint8_t *code, uint32_t pc, DecodedInsn *insn);

/**
 * Get instruction mnemonic string.
 */
const char* armv8m_decode_mnemonic(InsnType type);
```

## Key Data Structures

### DecodedInsn

```c
typedef struct DecodedInsn {
    InsnType type;          // Instruction type (ADD, LDR, etc.)
    uint8_t rd, rn, rm, rs; // Register operands (ARMV8M_REG_NONE if unused)
    uint32_t imm;           // Immediate value
    ShiftType shift_type;   // Shift type for rm
    uint8_t shift_amount;   // Shift amount
    Condition cond;         // Condition code
    bool set_flags;         // Update APSR flags
    bool is_32bit;          // 32-bit instruction
    AddrMode addr_mode;     // Addressing mode for load/store

    // Load/store specific
    bool writeback;         // Write back to base register
    bool post_index;        // Post-indexed addressing
    bool add;               // Add offset (vs subtract)

    // Multiple register ops
    uint16_t reg_list;      // Register list bitmap

    // Branch
    int32_t branch_offset;  // Signed branch offset

    // FPU
    uint8_t sd, sn, sm;     // Single-precision registers
    uint8_t dd, dn, dm;     // Double-precision registers
} DecodedInsn;
```

### InsnType

Categories of instructions:

```c
typedef enum {
    INSN_UNDEFINED = 0,

    // Data processing
    INSN_DATA_PROC_REG,     // ADD, SUB, etc. with register
    INSN_DATA_PROC_IMM,     // ADD, SUB, etc. with immediate
    INSN_DATA_PROC_SHIFTED, // Shifted register operations

    // Load/store
    INSN_LOAD_STORE,        // LDR, STR, etc.
    INSN_LOAD_STORE_MULTI,  // LDM, STM, PUSH, POP

    // Branch
    INSN_BRANCH,            // B, BL, BX, BLX

    // System
    INSN_SYSTEM,            // SVC, MRS, MSR, etc.

    // ... more categories
} InsnType;
```

## Implementation Structure

```text
src/arch/armv8m/decoder/
├── decoder.c              # Main decoder, instruction dispatch
├── decode_thumb16.c       # 16-bit Thumb instructions
├── decode_thumb32.c       # 32-bit Thumb-2 main dispatch
├── decode_thumb32_data.c  # Data processing instructions
├── decode_thumb32_loadstore.c  # Load/store instructions
├── decode_thumb32_branch.c     # Branch instructions
├── decode_thumb32_multiply.c   # Multiply/accumulate
├── decode_thumb32_dsp.c        # DSP instructions
├── decode_thumb32_vfp.c        # VFP/FPU instructions
├── decoder_vtable.c       # Instruction mnemonic table
└── tests/
    └── test_decoder.cpp   # CppUTest tests
```

## Decoding Flow

1. **Instruction Fetch**: Read 2 bytes at PC
2. **Width Detection**: Check if 32-bit prefix (0b11101/11110/11111)
3. **Dispatch**: Route to 16-bit or 32-bit decoder
4. **Field Extraction**: Extract operands using bit masks
5. **Validation**: Check for undefined encodings

```c
int armv8m_decode(const uint8_t *code, uint32_t pc, DecodedInsn *insn) {
    uint16_t hw1 = code[0] | (code[1] << 8);

    // Check for 32-bit instruction
    if ((hw1 & 0xE000) == 0xE000 && (hw1 & 0x1800) != 0) {
        uint16_t hw2 = code[2] | (code[3] << 8);
        return decode_thumb32(hw1, hw2, pc, insn);
    }

    return decode_thumb16(hw1, pc, insn);
}
```

## Little-Endian Considerations

ARM Thumb instructions are stored little-endian:

```c
// Memory: 0x2A 0x20 (at addresses 0, 1)
// Halfword: 0x202A
// Instruction: MOVS R0, #42

uint16_t hw = code[0] | (code[1] << 8);  // Correct
uint16_t hw = *(uint16_t*)code;          // Wrong on big-endian host
```

## Register Encoding

Registers are encoded as 3-4 bit fields:

| Encoding | Register | Notes                    |
| -------- | -------- | ------------------------ |
| 0-7      | R0-R7    | Low registers            |
| 8-12     | R8-R12   | High registers           |
| 13       | SP       | Stack pointer            |
| 14       | LR       | Link register            |
| 15       | PC       | Program counter (special)|

## Error Handling

Return negative error codes:

```c
#define DECODE_ERROR_UNDEFINED  (-1)
#define DECODE_ERROR_TRUNCATED  (-2)
#define DECODE_ERROR_INVALID    (-3)
```

## Testing

Test vectors cover each instruction category:

```cpp
TEST(Thumb16DataProc, MovImmediate)
{
    uint8_t code[] = THUMB16_BYTES(0x202A);  // MOVS R0, #42
    int result = armv8m_decode(code, TEST_PC, &insn);

    CHECK_EQUAL(2, result);
    CHECK_EQUAL(INSN_DATA_PROC_IMM, insn.type);
    CHECK_EQUAL(REG_R0, insn.rd);
    CHECK_EQUAL(42, insn.imm);
    CHECK_TRUE(insn.set_flags);
}
```

## References

- ARMv8-M Architecture Reference Manual
- ARM Thumb-2 Supplement
