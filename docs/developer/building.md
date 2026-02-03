# Building FEMU

This guide covers the build system, compiler options, and static analysis tools.

## Prerequisites

FEMU requires:

- **C Compiler**: GCC 10+ or Clang 12+ (C11 support required)
- **C++ Compiler**: GCC 10+ or Clang 12+ (C++17 support, for tests)
- **CMake**: 3.16 or later
- **Python**: 3.10+ (for Python bindings)

Optional:

- **cppcheck**: Static analysis
- **clang-tidy**: Static analysis
- **Doxygen**: Documentation generation

## Quick Start

Using the Nix development environment (recommended):

```bash
# Enter the development shell
nix develop

# Build with defaults (Debug, GCC, sanitizers enabled)
femu build all
```

## Build Commands

### Configure

```bash
femu build configure [OPTIONS]
```

| Option            | Description                  | Default |
| ----------------- | ---------------------------- | ------- |
| `--build-type`    | `Debug` or `Release`         | Debug   |
| `--compiler`      | `gcc` or `clang`             | gcc     |
| `--no-sanitizers` | Disable ASan/UBSan           | False   |
| `--clean`         | Clean build directory first  | False   |

### Compile

```bash
femu build compile [OPTIONS]
```

| Option       | Description              | Default |
| ------------ | ------------------------ | ------- |
| `-j, --jobs` | Parallel build jobs      | auto    |
| `--target`   | Specific target to build | all     |

### Combined Build

```bash
femu build all [OPTIONS]
```

Runs configure + compile in one step.

### Clean

```bash
femu build clean
```

Removes the `build/` directory.

## CMake Options

When using CMake directly:

```bash
cmake -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DENABLE_SANITIZERS=ON

cmake --build build -j$(nproc)
```

### Available Options

| Option                 | Description                              | Default |
| ---------------------- | ---------------------------------------- | ------- |
| `CMAKE_BUILD_TYPE`     | Debug or Release                         | Debug   |
| `ENABLE_SANITIZERS`    | Enable ASan/UBSan (Debug only)           | ON      |
| `ENABLE_COVERAGE`      | Enable code coverage                     | OFF     |
| `BUILD_ARCH_ARMV8M`    | Build ARMv8-M architecture               | ON      |
| `BUILD_PYTHON_BINDINGS`| Build shared library for Python          | ON      |
| `EMU_DISABLE_LOGGING`  | Disable logging (zero overhead)          | OFF     |

### Module Options

| Option            | Description                  | Default |
| ----------------- | ---------------------------- | ------- |
| `BUILD_DECODER`   | Build decoder module         | ON      |
| `BUILD_EXECUTOR`  | Build executor module        | ON      |
| `BUILD_NVIC`      | Build NVIC module            | ON      |
| `BUILD_MPU`       | Build MPU module             | ON      |
| `BUILD_MEMORY`    | Build memory module          | ON      |
| `BUILD_EMULATOR`  | Build emulator glue layer    | ON      |

## Compiler Selection

### Using GCC

```bash
femu build all --compiler=gcc
```

Or with CMake directly:

```bash
CC=gcc CXX=g++ cmake -B build
```

### Using Clang

```bash
femu build all --compiler=clang
```

Or with CMake directly:

```bash
CC=clang CXX=clang++ cmake -B build
```

## Debug vs Release

### Debug Build (Default)

```bash
femu build all --build-type=Debug
```

- Full debug symbols (`-g3`)
- No optimization (`-O0`)
- Sanitizers enabled (ASan + UBSan)
- All warnings are errors

### Release Build

```bash
femu build all --build-type=Release
```

- Optimization enabled (`-O2` or `-Os`)
- No sanitizers
- Smaller binaries

### Release with Debug Info

```bash
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

## Sanitizers

Debug builds enable AddressSanitizer and UndefinedBehaviorSanitizer by default.

### Disabling Sanitizers

```bash
femu build all --no-sanitizers
```

Or with CMake:

```bash
cmake -B build -DENABLE_SANITIZERS=OFF
```

### What Sanitizers Detect

**AddressSanitizer (ASan):**

- Buffer overflows (stack, heap, global)
- Use-after-free
- Use-after-return
- Memory leaks

**UndefinedBehaviorSanitizer (UBSan):**

- Integer overflow
- Null pointer dereference
- Division by zero
- Invalid shifts
- Out-of-bounds array access

### Interpreting Sanitizer Output

```text
==12345==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x...
READ of size 4 at 0x... thread T0
    #0 0x... in my_function src/file.c:42
    #1 0x... in caller src/other.c:100
```

The stack trace shows where the error occurred.

## Static Analysis

### Running All Analyzers

```bash
femu build analyze
```

### cppcheck Only

```bash
femu build analyze --tool=cppcheck
```

Or with CMake:

```bash
cmake --build build --target cppcheck
```

### clang-tidy Only

```bash
femu build analyze --tool=clang-tidy
```

Or with CMake:

```bash
cmake --build build --target clang-tidy
```

Requires `compile_commands.json` (generated automatically).

### Configuration

**cppcheck**: Configured inline in `CMakeLists.txt` with suppressions for intentional patterns.

**clang-tidy**: Configured via `.clang-tidy` file in the project root.

## Code Coverage

Enable coverage for CI:

```bash
cmake -B build -DENABLE_COVERAGE=ON
cmake --build build
ctest --test-dir build
gcovr --root . --html coverage.html
```

## Build Artifacts

After building:

```text
build/
├── src/
│   ├── arch/armv8m/
│   │   ├── libfemu_armv8m.so      # Main library
│   │   ├── decoder/
│   │   │   └── test_decoder       # Decoder tests
│   │   ├── executor/
│   │   │   └── test_executor      # Executor tests
│   │   └── ...
│   └── core/
│       ├── memory/
│       │   └── test_memory        # Memory tests
│       └── emulator/
│           └── test_emulator      # Emulator tests
└── compile_commands.json          # For clang-tidy
```

## Troubleshooting

### "CppUTest not found"

Initialize git submodules:

```bash
git submodule update --init
```

### Sanitizer errors on startup

Try building without sanitizers:

```bash
femu build all --no-sanitizers
```

### Linker errors with Clang

Ensure you're using matching `clang` and `clang++`:

```bash
CC=clang CXX=clang++ femu build all --compiler=clang
```

### Out of memory during build

Reduce parallel jobs:

```bash
femu build all -j2
```
