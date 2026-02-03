# Contributing to FEMU

Thank you for your interest in contributing to FEMU. This guide covers code style, git workflow, and the review process.

## Getting Started

1. Fork the repository
2. Clone your fork
3. Set up the development environment:

```bash
# Using Nix (recommended)
nix develop

# Or install dependencies manually (see docs/user/getting-started.md)
pip install -e .
```

4. Build and verify:

```bash
femu build all
femu test all
```

## Code Style

### C Code

FEMU follows a consistent C style:

```c
// Use snake_case for functions and variables
int armv8m_decode_instruction(const uint8_t *mem, DecodedInsn *insn);

// Use UPPER_CASE for constants and macros
#define INSN_TYPE_DATA_PROC 0x01

// Use PascalCase for type names
typedef struct DecodedInsn DecodedInsn;

// Prefix with module name
int armv8m_nvic_set_pending(ARMv8MNVIC *nvic, uint32_t irq);
int emu_memory_read(EmuMemory *mem, uint32_t addr, uint32_t *value);
```

**Formatting:**

- 4 spaces for indentation (no tabs)
- Opening brace on same line
- Maximum line length: 100 characters
- One statement per line

```c
// Good
if (condition) {
    do_something();
} else {
    do_other();
}

// Bad
if (condition) { do_something(); } else { do_other(); }
```

**Documentation:**

Use Doxygen-style comments for public APIs:

```c
/**
 * Decode a Thumb instruction.
 *
 * @param code Pointer to instruction bytes (little-endian)
 * @param pc Current program counter (for PC-relative addressing)
 * @param insn Output: decoded instruction
 * @return Number of bytes consumed (2 or 4), or negative on error
 */
int armv8m_decode(const uint8_t *code, uint32_t pc, DecodedInsn *insn);
```

### Python Code

Follow PEP 8 with these specifics:

```python
# Use snake_case for functions and variables
def load_firmware(path: str) -> ElfInfo:
    ...

# Use PascalCase for classes
class ARMv8MEmulator:
    ...

# Use type hints
def read_mem(self, addr: int, size: int = 4) -> int:
    ...

# Use Google-style docstrings
def add_peripheral(self, periph: Peripheral, base: int, size: int) -> None:
    """
    Add a peripheral to the emulator.

    Args:
        periph: Peripheral instance to add
        base: Base address in memory map
        size: Size of peripheral's address space

    Raises:
        ValueError: If address overlaps existing peripheral
    """
```

**Type Annotations:**

All public APIs must have type annotations:

```python
# Good
def create_machine(config: dict[str, Any]) -> Machine:
    ...

# Bad
def create_machine(config):
    ...
```

### Compiler Warnings

All code must compile cleanly with:

```text
-Wall -Wextra -Werror -pedantic
```

Do not add warning suppressions without justification.

## Git Workflow

### Branch Naming

```text
feature/add-uart-peripheral
fix/decoder-branch-offset
docs/update-architecture-guide
refactor/simplify-executor-loop
```

### Commit Messages

Follow the conventional commits format:

```text
<type>(<scope>): <description>

[optional body]

[optional footer]
```

**Types:**

- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation only
- `style`: Formatting, no code change
- `refactor`: Code change that neither fixes nor adds
- `test`: Adding or correcting tests
- `chore`: Maintenance tasks

**Examples:**

```text
feat(decoder): Add VFP instruction decoding

Implements single-precision floating point instruction decoding
for ARMv8-M targets with FPU support.

Closes #42
```

```text
fix(executor): Correct LDMIA writeback behavior

The stack pointer was not updated correctly when the base register
was included in the register list.

Fixes #123
```

### Pull Request Process

1. **Create a feature branch:**

```bash
git checkout -b feature/my-feature
```

2. **Make atomic commits:**

```bash
git add src/arch/armv8m/decoder/decode_vfp.c
git commit -m "feat(decoder): Add VFP VADD instruction"
```

3. **Ensure tests pass:**

```bash
femu build all
femu test all
femu build analyze
```

4. **Push and create PR:**

```bash
git push origin feature/my-feature
```

5. **PR description should include:**
   - Summary of changes
   - Motivation/context
   - Test plan
   - Breaking changes (if any)

### Code Review

- All PRs require at least one approval
- Address all review comments
- Keep PRs focused and reasonably sized
- Squash fixup commits before merge

## Testing Requirements

### New Features

- Must include tests covering the feature
- Both positive and negative test cases
- Edge cases and boundary conditions

### Bug Fixes

- Must include a test that would have caught the bug
- Test should fail without the fix

### Coverage

Aim to maintain or improve test coverage. Check coverage locally:

```bash
# C coverage
cmake -B build -DENABLE_COVERAGE=ON
cmake --build build
ctest --test-dir build
gcovr --root .

# Python coverage
pytest --cov=femu python/tests/
```

## Architecture Guidelines

### Module Boundaries

Respect the layered architecture:

```text
Python (Machine, GDB Server, Peripherals)
              ↓ CFFI
C Core (Emulator, Decoder, Executor, Memory, NVIC, MPU)
```

- Python should not bypass CFFI to access C internals
- C modules communicate through defined interfaces
- Peripheral callbacks go through the vtable

### Adding New Modules

1. Create header in appropriate location:
   - Generic: `include/emu/emu_newmodule.h`
   - Architecture-specific: `include/arch/armv8m/armv8m_newmodule.h`

2. Implement in corresponding source directory

3. Add CMakeLists.txt for the module

4. Add tests

5. Update documentation

### Error Handling

C functions return error codes:

```c
// Good
int result = decode_instruction(code, &insn);
if (result < 0) {
    return result;  // Propagate error
}

// Bad
decode_instruction(code, &insn);  // Ignoring errors
```

Python uses exceptions:

```python
# Good
try:
    machine.load_elf(path)
except FileNotFoundError:
    print(f"Firmware not found: {path}")
```

## Getting Help

- Read the architecture documentation in `docs/architecture/`
- Check existing code for patterns
- Open an issue for discussion before large changes
- Ask questions in PR comments

## License

By contributing, you agree that your contributions will be licensed under the project's license.
