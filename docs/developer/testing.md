# Testing FEMU

FEMU uses two test frameworks: CppUTest for C tests and pytest for Python tests.

## Quick Start

```bash
# Run all tests
femu test all

# Run C tests only
femu test c

# Run Python tests only
femu test python
```

## C Tests (CppUTest)

C tests use the [CppUTest](https://cpputest.github.io/) framework. Test files are C++ (`.cpp`) to use CppUTest's macros while testing C code.

### Running C Tests

```bash
# All C tests
femu test c

# With verbose output
femu test c -v

# Filter by test name
femu test c --filter=decoder
femu test c --filter=executor
femu test c --filter=memory
```

### Test File Structure

Tests are located alongside their modules:

```text
src/arch/armv8m/decoder/
├── decoder.c
├── decode_thumb16.c
├── ...
└── tests/
    └── test_decoder.cpp

src/arch/armv8m/executor/
├── executor.c
├── ...
└── tests/
    ├── test_main.cpp
    ├── test_arithmetic.cpp
    ├── test_load_store.cpp
    └── ...
```

### Writing C Tests

```cpp
// test_mymodule.cpp

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

// Include C headers in extern "C" block
extern "C" {
#include "arch/armv8m/armv8m_mymodule.h"
#include "arch/armv8m/armv8m_types.h"
}

// Define a test group
TEST_GROUP(MyModuleTest)
{
    MyState state;

    // Called before each test
    void setup()
    {
        mymodule_init(&state);
    }

    // Called after each test
    void teardown()
    {
        mymodule_destroy(&state);
    }
};

// Individual test
TEST(MyModuleTest, BasicOperation)
{
    int result = mymodule_do_something(&state, 42);
    CHECK_EQUAL(0, result);
    CHECK_EQUAL(42, state.value);
}

// Test with expected failure
TEST(MyModuleTest, InvalidInput)
{
    int result = mymodule_do_something(&state, -1);
    CHECK_EQUAL(-1, result);  // Error code
}

// Main function (in test_main.cpp)
int main(int argc, char** argv)
{
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
```

### CppUTest Assertions

| Macro                     | Description                        |
| ------------------------- | ---------------------------------- |
| `CHECK(condition)`        | Verify condition is true           |
| `CHECK_FALSE(condition)`  | Verify condition is false          |
| `CHECK_EQUAL(exp, act)`   | Verify values are equal            |
| `CHECK_TRUE(condition)`   | Alias for CHECK                    |
| `LONGS_EQUAL(exp, act)`   | Compare long integers              |
| `STRCMP_EQUAL(exp, act)`  | Compare C strings                  |
| `POINTERS_EQUAL(exp, act)`| Compare pointers                   |
| `FAIL(message)`           | Force test failure                 |

### Test Helpers

Common patterns used in FEMU tests:

```cpp
// Build little-endian bytes for Thumb instructions
#define THUMB16_BYTES(hw) \
    { (uint8_t)((hw) & 0xFF), (uint8_t)(((hw) >> 8) & 0xFF) }

#define THUMB32_BYTES(hw1, hw2) \
    { (uint8_t)((hw1) & 0xFF), (uint8_t)(((hw1) >> 8) & 0xFF), \
      (uint8_t)((hw2) & 0xFF), (uint8_t)(((hw2) >> 8) & 0xFF) }

// Example usage
TEST(DecoderTest, MovImmediate)
{
    uint8_t code[] = THUMB16_BYTES(0x202A);  // MOVS R0, #42
    int result = armv8m_decode(code, 0x08000000, &insn);
    CHECK_EQUAL(2, result);
}
```

### Adding New C Tests

1. Create test file in module's `tests/` directory
2. Add to module's `CMakeLists.txt`:

```cmake
add_executable(test_mymodule
    tests/test_mymodule.cpp
)
target_link_libraries(test_mymodule
    mymodule_lib
    CppUTest
    CppUTestExt
)
add_test(NAME MyModuleTests COMMAND test_mymodule)
```

## Python Tests (pytest)

Python tests use [pytest](https://docs.pytest.org/) and test the Python bindings and high-level functionality.

### Running Python Tests

```bash
# All Python tests
femu test python

# With verbose output
femu test python -v

# Filter by expression
femu test python -k watchpoint
femu test python -k "uart or gpio"
```

### Test File Structure

```text
python/tests/
├── test_emulator.py
├── test_gdb_server.py
├── test_machine.py
├── test_peripherals.py
└── conftest.py          # pytest fixtures
```

### Writing Python Tests

```python
# test_myfeature.py

import pytest
from femu import Machine, ARMv8MEmulator

class TestMyFeature:
    """Tests for my feature."""

    def test_basic_operation(self):
        """Test basic operation."""
        machine = Machine.from_dict({
            "machine": {"name": "test", "arch": "armv8m"},
            "memory": [
                {"type": "flash", "base": 0x00000000, "size": "64K"},
                {"type": "ram", "base": 0x20000000, "size": "32K"},
            ],
        })

        # Set up initial state
        machine.write_mem(0x20000000, 0x12345678, size=4)

        # Verify
        value = machine.read_mem(0x20000000, size=4)
        assert value == 0x12345678

    def test_error_handling(self):
        """Test error conditions."""
        machine = Machine.from_dict({...})

        with pytest.raises(ValueError):
            machine.read_mem(0xFFFFFFFF, size=4)  # Invalid address
```

### pytest Fixtures

Define reusable test fixtures in `conftest.py`:

```python
# conftest.py

import pytest
from femu import Machine

@pytest.fixture
def basic_machine():
    """Create a basic machine for testing."""
    return Machine.from_dict({
        "machine": {"name": "test", "arch": "armv8m"},
        "memory": [
            {"type": "flash", "base": 0x00000000, "size": "64K"},
            {"type": "ram", "base": 0x20000000, "size": "32K"},
        ],
    })

@pytest.fixture
def machine_with_uart(basic_machine):
    """Machine with UART peripheral."""
    from femu.peripherals import SimpleUART
    uart = SimpleUART(name="USART1")
    basic_machine.emu.add_peripheral(uart, 0x40000000, 0x400)
    return basic_machine, uart
```

Use fixtures in tests:

```python
def test_with_fixture(basic_machine):
    """Test using the basic_machine fixture."""
    basic_machine.write_mem(0x20000000, 42, size=4)
    assert basic_machine.read_mem(0x20000000, size=4) == 42
```

### pytest Markers

```python
@pytest.mark.slow
def test_long_running():
    """This test takes a while."""
    ...

@pytest.mark.skip(reason="Not implemented yet")
def test_future_feature():
    ...

@pytest.mark.parametrize("value", [0, 1, 255, 0xFFFFFFFF])
def test_with_values(basic_machine, value):
    """Test with multiple values."""
    basic_machine.write_mem(0x20000000, value, size=4)
    assert basic_machine.read_mem(0x20000000, size=4) == value
```

## Running Tests in CI

The CI workflow runs:

```yaml
- name: Run C tests
  run: nix develop --command femu test c

- name: Run Python tests
  run: nix develop --command femu test python
```

## Test Coverage

### C Coverage

```bash
cmake -B build -DENABLE_COVERAGE=ON
cmake --build build
ctest --test-dir build
gcovr --root . --html coverage.html
```

### Python Coverage

```bash
pytest --cov=femu --cov-report=html python/tests/
```

## Debugging Tests

### C Tests with GDB

```bash
# Build with debug symbols (default)
femu build all

# Run specific test under GDB
gdb ./build/src/arch/armv8m/decoder/test_decoder

# Set breakpoint and run
(gdb) break armv8m_decode
(gdb) run
```

### Python Tests with pdb

```python
def test_debugging():
    import pdb; pdb.set_trace()  # Breakpoint here
    result = some_function()
    assert result == expected
```

Or use pytest's built-in debugger:

```bash
pytest --pdb python/tests/test_file.py::test_function
```

## Best Practices

1. **Test one thing per test** - Each test should verify a single behavior
2. **Use descriptive names** - `test_decoder_handles_invalid_opcode` not `test1`
3. **Test edge cases** - Zero, max values, empty inputs, error conditions
4. **Keep tests fast** - Avoid unnecessary setup/teardown
5. **Don't test implementation details** - Test behavior, not internal state
