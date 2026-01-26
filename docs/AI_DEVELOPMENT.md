# AI Development Guide

This project is designed for AI-assisted development. Follow this guide to work on any module.

## Quick Start

1. Identify which module you're working on
2. Run: `python tools/ai_session.py context <module>`
3. Read ONLY the files listed
4. Implement, following the header contract exactly

## Available Modules

| Module | Header | Status |
|--------|--------|--------|
| decoder | `include/armv8m_decoder.h` | 🔴 Not started |
| executor | `include/armv8m_executor.h` | 🔴 Not started |
| memory | `include/armv8m_memory.h` | 🔴 Not started |
| nvic | `include/armv8m_nvic.h` | 🔴 Not started |
| mpu | `include/armv8m_mpu.h` | 🔴 Not started |

## Session Protocol

### Before You Start

```markdown
I am implementing the [MODULE] module for an ARMv8-M emulator.

I have read:
- [ ] docs/ARCHITECTURE.md (Parts 5 and 9)
- [ ] include/armv8m_[module].h
- [ ] src/core/[module]/README.md  
- [ ] include/armv8m_types.h

I will produce:
- [ ] src/core/[module]/[module].c
- [ ] src/core/[module]/tests/test_[module].c
```

### Implementation Rules

1. **Match the header exactly** - Function signatures, types, return values
2. **Handle all errors** - Return error codes, never crash
3. **No undefined behavior** - Check array bounds, null pointers
4. **Compile clean** - `-Wall -Werror -pedantic`
5. **Test everything** - Each public function needs tests

### Code Style

```c
// Use snake_case for functions and variables
int armv8m_decode_instruction(const uint8_t *mem, DecodedInsn *insn);

// Use UPPER_CASE for constants and macros
#define INSN_TYPE_DATA_PROC 0x01

// Use PascalCase for type names
typedef struct DecodedInsn DecodedInsn;

// Document all public functions
/**
 * Brief description.
 *
 * @param name Description
 * @return Description
 */
```

### Testing Pattern

```c
// tests/test_decoder.c

#include "armv8m_decoder.h"
#include <assert.h>
#include <stdio.h>

static void test_decode_mov_immediate(void) {
    // MOVS R0, #42 = 0x202A
    uint8_t code[] = {0x2A, 0x20};
    DecodedInsn insn;
    
    int result = armv8m_decode(code, 0x08000000, &insn);
    
    assert(result == 2);  // 2 bytes consumed
    assert(insn.type == INSN_DATA_PROC);
    assert(insn.rd == 0);
    assert(insn.imm == 42);
    assert(insn.set_flags == true);
    
    printf("✓ test_decode_mov_immediate\n");
}

int main(void) {
    test_decode_mov_immediate();
    // ... more tests
    printf("\nAll tests passed!\n");
    return 0;
}
```

## Peripheral Development

Peripherals can be implemented in C, Rust, or Python.

### C Peripheral Template

```c
// peripherals/c/myperiph/myperiph.c

#include "peripheral_interface.h"

typedef struct {
    uint32_t reg0;
    uint32_t reg1;
    // ... state
} MyPeriphState;

static uint32_t myperiph_read(void *ctx, uint32_t offset, uint8_t size) {
    MyPeriphState *s = (MyPeriphState *)ctx;
    switch (offset) {
        case 0x00: return s->reg0;
        case 0x04: return s->reg1;
        default: return 0;
    }
}

static void myperiph_write(void *ctx, uint32_t offset, uint32_t val, uint8_t size) {
    MyPeriphState *s = (MyPeriphState *)ctx;
    switch (offset) {
        case 0x00: s->reg0 = val; break;
        case 0x04: s->reg1 = val; break;
    }
}

// Factory function - called from Python
Peripheral* myperiph_create(void) {
    Peripheral *p = calloc(1, sizeof(Peripheral));
    MyPeriphState *s = calloc(1, sizeof(MyPeriphState));
    
    p->name = "myperiph";
    p->context = s;
    p->vtable.read = myperiph_read;
    p->vtable.write = myperiph_write;
    // ... set other vtable entries
    
    return p;
}
```

### Rust Peripheral Template

```rust
// peripherals/rust/src/myperiph.rs

use std::ffi::c_void;

#[repr(C)]
pub struct MyPeriphState {
    reg0: u32,
    reg1: u32,
}

#[no_mangle]
pub extern "C" fn myperiph_read(ctx: *mut c_void, offset: u32, _size: u8) -> u32 {
    let state = unsafe { &*(ctx as *const MyPeriphState) };
    match offset {
        0x00 => state.reg0,
        0x04 => state.reg1,
        _ => 0,
    }
}

#[no_mangle]
pub extern "C" fn myperiph_write(ctx: *mut c_void, offset: u32, value: u32, _size: u8) {
    let state = unsafe { &mut *(ctx as *mut MyPeriphState) };
    match offset {
        0x00 => state.reg0 = value,
        0x04 => state.reg1 = value,
        _ => {}
    }
}
```

## Debugging Tips

### When Tests Fail

1. Check the header contract - are you matching it exactly?
2. Check endianness - ARM is little-endian
3. Check bit extraction - use masks carefully
4. Add printf debugging - remove before committing

### Common Mistakes

- Forgetting Thumb instructions are little-endian in memory
- Off-by-one in PC calculations (PC is +4 ahead in ARM)
- Not handling the IT block state for conditional execution
- Forgetting to update flags when `set_flags` is true

## Git Workflow

```bash
# Create feature branch for your module
git checkout -b module/decoder

# Make atomic commits
git add src/core/decoder/decoder.c
git commit -m "decoder: Add 16-bit data processing decode"

# Push for review
git push origin module/decoder
```

## Getting Help

If you're stuck:

1. Re-read the module README
2. Check the test vectors for expected behavior
3. Look at ARMv8-M Architecture Reference Manual
4. Check similar code in QEMU (target/arm/) or Unicorn
