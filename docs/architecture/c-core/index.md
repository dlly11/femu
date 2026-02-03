# C Core Modules

The C core provides the high-performance simulation engine. These modules are designed for independent development and testing.

```{toctree}
:maxdepth: 1
:hidden:

decoder
executor
memory
nvic
mpu
```

## Module Overview

| Module                    | Purpose                           |
| ------------------------- | --------------------------------- |
| [Decoder](decoder.md)     | Thumb-2 instruction decoding      |
| [Executor](executor.md)   | Instruction execution             |
| [Memory](memory.md)       | Memory regions and access         |
| [NVIC](nvic.md)           | Interrupt controller              |
| [MPU](mpu.md)             | Memory protection unit            |

## Architecture Layers

```text
include/emu/          - Generic interfaces (all architectures)
include/arch/armv8m/  - ARMv8-M specific headers

src/core/             - Shared implementations
src/arch/armv8m/      - ARMv8-M specific implementations
```

## Dependencies

```text
          ┌─────────┐
          │ emulator│
          └────┬────┘
               │
    ┌──────────┼──────────┐
    ▼          ▼          ▼
┌────────┐ ┌────────┐ ┌──────┐
│executor│ │  nvic  │ │ mpu  │
└───┬────┘ └────────┘ └──────┘
    │
    ▼
┌────────┐ ┌────────┐
│decoder │ │ memory │
└────────┘ └────────┘
```

## Common Patterns

### Error Handling

All functions return error codes:

```c
int result = some_function(...);
if (result < 0) {
    // Handle error
    return result;
}
```

### Memory Management

Modules use create/destroy pattern:

```c
Module* module_create(void);
void module_destroy(Module* mod);
```

### Testing

Each module has CppUTest tests:

```bash
femu test c --filter=decoder
femu test c --filter=executor
```

## Building

```bash
# Build all modules
femu build all

# Build specific target
cmake --build build --target femu_decoder
```
