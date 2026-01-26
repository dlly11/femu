# Decoder Module

## Purpose

Decodes ARMv8-M Thumb instructions into a structured `DecodedInsn` representation that the executor can use.

## Interface

See `include/armv8m_decoder.h`

## Files to Implement

| File | Description | ~LOC |
|------|-------------|------|
| `decoder.c` | Main entry point, dispatch logic | 150 |
| `decode_thumb16.c` | 16-bit instruction decoding | 400 |
| `decode_thumb32.c` | 32-bit instruction decoding | 500 |

## Dependencies

- `armv8m_types.h` (types only)

## AI Development Notes

### What You Need to Know

1. **Thumb instruction encoding basics**:
   - All instructions are 16-bit or 32-bit
   - 32-bit instructions have bits [15:11] = 0b11101, 0b11110, or 0b11111
   - Little-endian byte order in memory

2. **16-bit instruction categories** (bits [15:11]):
   - `00xxx` - Shift, add, subtract, move, compare
   - `01000` - Data processing
   - `01001` - Load from literal pool
   - `0101x` - Load/store register offset
   - `011xx` - Load/store word immediate
   - `100xx` - Load/store halfword, SP-relative, PC-relative
   - `10100` - ADR (PC-relative address)
   - `10101` - ADD SP + immediate
   - `1011x` - Miscellaneous (push, pop, extend, etc.)
   - `11000` - Store multiple
   - `11001` - Load multiple
   - `1101x` - Conditional branch, SVC
   - `11100` - Unconditional branch

3. **32-bit instruction categories** (bits [15:11] of first halfword):
   - `11101` - Various (branches, data processing, etc.)
   - `11110` - Branches and miscellaneous control
   - `11111` - Coprocessor, load/store

### What You Do NOT Need

- Executor implementation (just fill in the struct)
- Memory system details
- NVIC or exception handling
- How peripherals work

### Implementation Tips

```c
// Reading instruction bytes (little-endian)
static inline uint16_t read_hw(const uint8_t *mem) {
    return mem[0] | (mem[1] << 8);
}

// Extract bit field
static inline uint32_t extract(uint32_t val, int start, int len) {
    return (val >> start) & ((1U << len) - 1);
}

// Sign extend
static inline int32_t sign_extend(uint32_t val, int bits) {
    int32_t shift = 32 - bits;
    return ((int32_t)(val << shift)) >> shift;
}
```

### Test Vectors

Here are some test cases to validate your implementation:

```c
// MOVS R0, #42  (0x202A little-endian: 0x2A, 0x20)
// Expected: type=INSN_DATA_PROC_IMM, op=DP_MOV, rd=0, imm=42, set_flags=true

// ADDS R0, R1, R2  (0x1888)
// Expected: type=INSN_DATA_PROC_REG, op=DP_ADD, rd=0, rn=1, rm=2, set_flags=true

// LDR R0, [R1, #4]  (0x6848)
// Expected: type=INSN_LOAD_IMM, rt=0, rn=1, imm=4, access_size=ACCESS_WORD

// B.N +10  (0xE004 - branch offset is 10, so PC+4+10=PC+14)
// Expected: type=INSN_BRANCH, branch_offset=10, cond=COND_AL

// BL somewhere  (32-bit: 0xF000 0xF801)
// Expected: type=INSN_BRANCH_LINK, link=true, is_32bit=true
```

### Edge Cases to Handle

1. **Undefined encodings** - Return `ARMV8M_ERR_UNDEFINED_INSN`
2. **Unpredictable encodings** - Return `ARMV8M_ERR_UNPREDICTABLE` (e.g., PC as Rd in some instructions)
3. **IT block** - The decoder doesn't track IT state; it just decodes the instruction. The executor/caller tracks IT state.

### Reference Material

- ARMv8-M Architecture Reference Manual, Chapter A5
- ARMv7-M Architecture Reference Manual (similar encodings)
- QEMU source: `target/arm/t32.decode`, `target/arm/t16.decode`

## Building & Testing

```bash
cd src/core/decoder
make
make test
```

## Checklist

- [ ] `decoder.c` - Main entry point
- [ ] `decode_thumb16.c` - All 16-bit instruction categories
- [ ] `decode_thumb32.c` - All 32-bit instruction categories  
- [ ] `tests/test_decoder.c` - Unit tests
- [ ] All tests pass
- [ ] Compiles with `-Wall -Werror -pedantic`
