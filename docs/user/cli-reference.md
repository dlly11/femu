# CLI Reference

Complete reference for the `femu` command-line interface.

## Global Options

```bash
femu --help      # Show help
femu --version   # Show version
```

## Build Commands

### femu build configure

Configure the CMake build system.

```bash
femu build configure [OPTIONS]
```

**Options:**

| Option             | Description                    | Default |
| ------------------ | ------------------------------ | ------- |
| `--build-type`     | Debug or Release               | Debug   |
| `--compiler`       | gcc or clang                   | gcc     |
| `--no-sanitizers`  | Disable ASan/UBSan             | False   |
| `--clean`          | Clean build directory first    | False   |

**Examples:**

```bash
femu build configure                       # Debug with GCC
femu build configure --build-type=Release  # Release build
femu build configure --compiler=clang      # Use Clang
```

### femu build compile

Compile the project (requires prior configure).

```bash
femu build compile [OPTIONS]
```

**Options:**

| Option       | Description              | Default |
| ------------ | ------------------------ | ------- |
| `-j, --jobs` | Parallel build jobs      | auto    |
| `--target`   | Specific target to build | all     |

### femu build all

Configure and compile in one step.

```bash
femu build all [OPTIONS]
```

**Options:** Same as `configure` plus `--jobs`.

**Examples:**

```bash
femu build all                          # Default build
femu build all --compiler=clang         # Clang build
femu build all --no-sanitizers          # No sanitizers
femu build all -j8                      # 8 parallel jobs
```

### femu build clean

Remove the build directory.

```bash
femu build clean
```

### femu build analyze

Run static analysis tools.

```bash
femu build analyze [OPTIONS]
```

**Options:**

| Option   | Description                          | Default |
| -------- | ------------------------------------ | ------- |
| `--tool` | Specific tool (cppcheck, clang-tidy) | all     |

**Examples:**

```bash
femu build analyze                    # Run all analyzers
femu build analyze --tool=cppcheck    # Only cppcheck
femu build analyze --tool=clang-tidy  # Only clang-tidy
```

## Test Commands

### femu test c

Run C tests using CppUTest.

```bash
femu test c [OPTIONS]
```

**Options:**

| Option          | Description         | Default |
| --------------- | ------------------- | ------- |
| `-v, --verbose` | Verbose output      | False   |
| `--filter`      | Filter tests by name| None    |

**Examples:**

```bash
femu test c                      # All C tests
femu test c --filter=decoder     # Only decoder tests
femu test c --filter=executor    # Only executor tests
femu test c -v                   # Verbose output
```

### femu test python

Run Python tests using pytest.

```bash
femu test python [OPTIONS]
```

**Options:**

| Option          | Description              | Default |
| --------------- | ------------------------ | ------- |
| `-v, --verbose` | Verbose output           | False   |
| `-k`            | Filter by expression     | None    |

**Examples:**

```bash
femu test python              # All Python tests
femu test python -k watchpoint # Tests matching "watchpoint"
```

### femu test all

Run all tests (C and Python).

```bash
femu test all [OPTIONS]
```

**Options:**

| Option          | Description    | Default |
| --------------- | -------------- | ------- |
| `-v, --verbose` | Verbose output | False   |

## Run Commands

### femu run

Run the emulator with firmware.

```bash
femu run FIRMWARE [OPTIONS]
```

**Arguments:**

| Argument   | Description           | Required |
| ---------- | --------------------- | -------- |
| `FIRMWARE` | Path to ELF file      | Yes      |

**Options:**

| Option         | Description                    | Default   |
| -------------- | ------------------------------ | --------- |
| `--gdb-port`   | Start GDB server on port       | None      |
| `--max-cycles` | Maximum cycles (0=unlimited)   | 0         |
| `-v`           | Verbosity (-v, -vv, -vvv)      | WARNING   |
| `--trace`      | Enable TRACE for category      | None      |
| `--log-file`   | Log to file                    | None      |
| `--json-log`   | Use JSON log format            | False     |

**Verbosity levels:**

- No flag: WARNING only
- `-v`: INFO messages
- `-vv`: DEBUG messages
- `-vvv`: TRACE messages

**Trace categories:** decoder, executor, memory, nvic, mpu, peripheral, gdb, emulator

**Examples:**

```bash
femu run firmware.elf                        # Basic run
femu run firmware.elf -v                     # Info output
femu run firmware.elf --gdb-port 3333        # With GDB server
femu run firmware.elf --max-cycles 1000000   # Limit cycles
femu run firmware.elf --trace executor       # Trace executor
femu run firmware.elf -vvv --trace decoder   # Full trace
```

## Development Commands

### femu dev context

Show AI context files for a module.

```bash
femu dev context MODULE
```

**Arguments:**

| Argument | Description    | Required |
| -------- | -------------- | -------- |
| `MODULE` | Module name    | Yes      |

**Available modules:** decoder, executor, memory, nvic, mpu, emulator

**Example:**

```bash
femu dev context decoder
```

### femu dev status

Show status of all modules.

```bash
femu dev status
```

### femu dev list

List all available modules.

```bash
femu dev list
```

### femu dev validate

Validate a module implementation.

```bash
femu dev validate MODULE
```

### femu dev validate-all

Validate all implemented modules.

```bash
femu dev validate-all
```

## Documentation Commands

### femu docs build

Build documentation locally.

```bash
femu docs build [OPTIONS]
```

**Options:**

| Option    | Description               | Default |
| --------- | ------------------------- | ------- |
| `--clean` | Clean build directory first| False  |

### femu docs serve

Serve documentation locally.

```bash
femu docs serve [OPTIONS]
```

**Options:**

| Option   | Description | Default |
| -------- | ----------- | ------- |
| `--port` | HTTP port   | 8000    |

**Example:**

```bash
femu docs build
femu docs serve --port 8080
# Open http://localhost:8080
```

## Environment Variables

These can override CLI options:

| Variable | Description           |
| -------- | --------------------- |
| `CC`     | C compiler            |
| `CXX`    | C++ compiler          |
| `ARM_CC` | ARM cross-compiler    |

**Example:**

```bash
CC=clang CXX=clang++ femu build all
```

## Exit Codes

| Code | Description          |
| ---- | -------------------- |
| 0    | Success              |
| 1    | General error        |
| 2    | Invalid arguments    |
