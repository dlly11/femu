# Writing Peripherals

FEMU supports three types of peripherals:

1. **Python Peripherals** - Pure Python, easiest to develop
2. **C Peripherals** - Compiled into the emulator library
3. **Plugin Peripherals** - Loaded dynamically from shared libraries

This guide focuses on Python peripherals, which are recommended for most use cases.

## Python Peripherals

### Basic Structure

```python
from femu import Peripheral, PeripheralRegistry

@PeripheralRegistry.register("my_peripheral")
class MyPeripheral(Peripheral):
    """My custom peripheral."""

    def __init__(self, name: str = "myperiph", irq: int = -1):
        super().__init__(name, "my_peripheral")
        self._irq = irq
        self._data_reg = 0
        self._ctrl_reg = 0

    def read(self, offset: int, size: int) -> int:
        """Handle register reads."""
        if offset == 0x00:
            return self._data_reg
        elif offset == 0x04:
            return self._ctrl_reg
        return 0

    def write(self, offset: int, value: int, size: int) -> None:
        """Handle register writes."""
        if offset == 0x00:
            self._data_reg = value
        elif offset == 0x04:
            self._ctrl_reg = value

    def reset(self) -> None:
        """Reset to initial state."""
        self._data_reg = 0
        self._ctrl_reg = 0
```

### Required Methods

| Method                      | Description                           |
| --------------------------- | ------------------------------------- |
| `read(offset, size) -> int` | Read register at byte offset          |
| `write(offset, value, size)`| Write value to register at byte offset|

### Optional Methods

| Method          | Description                              |
| --------------- | ---------------------------------------- |
| `reset()`       | Called on emulator reset                 |
| `tick(cycles)`  | Called after each instruction with cycle count |

### Triggering Interrupts

Peripherals can trigger interrupts:

```python
class MyTimer(Peripheral):
    def __init__(self, name: str = "timer", irq: int = 10):
        super().__init__(name, "my_timer")
        self._irq = irq
        self._counter = 0

    def tick(self, cycles: int) -> None:
        if self._counter > 0:
            self._counter -= 1
            if self._counter == 0:
                # Pulse interrupt (edge-triggered)
                self.pulse_irq(self._irq)

    # Or for level-triggered:
    def on_condition(self):
        self.assert_irq(self._irq)    # Assert (set level high)

    def on_clear(self):
        self.deassert_irq(self._irq)  # Deassert (set level low)
```

### Registration

Use the decorator to register with the global registry:

```python
@PeripheralRegistry.register("type_name", "Description for listings")
class MyPeripheral(Peripheral):
    ...
```

Or register manually:

```python
PeripheralRegistry.register_python("type_name", MyPeripheral, "Description")
```

### Using in Machine Configuration

Once registered, peripherals can be used in YAML:

```yaml
peripherals:
  - type: my_peripheral
    name: PERIPH1
    base: 0x40000000
    size: 0x400
    config:
      irq: 25
```

## Complete Example: Timer Peripheral

```python
"""Simple timer peripheral with countdown and interrupt support."""

from femu import Peripheral, PeripheralRegistry


@PeripheralRegistry.register("simple_timer", "Simple countdown timer")
class SimpleTimer(Peripheral):
    """
    Simple countdown timer peripheral.

    Registers:
        0x00: COUNTER - Current counter value (read/write)
        0x04: RELOAD  - Reload value (read/write)
        0x08: CTRL    - Control register
                        bit 0: EN - Enable timer
                        bit 1: IE - Interrupt enable
        0x0C: STATUS  - Status register (read-only)
                        bit 0: UF - Underflow flag
    """

    REG_COUNTER = 0x00
    REG_RELOAD = 0x04
    REG_CTRL = 0x08
    REG_STATUS = 0x0C

    CTRL_EN = 1 << 0
    CTRL_IE = 1 << 1
    STATUS_UF = 1 << 0

    def __init__(self, name: str = "timer", irq: int = -1):
        super().__init__(name, "simple_timer")
        self._irq = irq
        self._counter = 0
        self._reload = 0
        self._ctrl = 0
        self._status = 0

    def read(self, offset: int, size: int) -> int:
        if offset == self.REG_COUNTER:
            return self._counter
        elif offset == self.REG_RELOAD:
            return self._reload
        elif offset == self.REG_CTRL:
            return self._ctrl
        elif offset == self.REG_STATUS:
            return self._status
        return 0

    def write(self, offset: int, value: int, size: int) -> None:
        if offset == self.REG_COUNTER:
            self._counter = value & 0xFFFFFFFF
        elif offset == self.REG_RELOAD:
            self._reload = value & 0xFFFFFFFF
        elif offset == self.REG_CTRL:
            self._ctrl = value & 0x3
        elif offset == self.REG_STATUS:
            # Writing 1 clears flags
            self._status &= ~(value & 0x1)

    def reset(self) -> None:
        self._counter = 0
        self._reload = 0
        self._ctrl = 0
        self._status = 0

    def tick(self, cycles: int) -> None:
        if not (self._ctrl & self.CTRL_EN):
            return

        if self._counter > 0:
            # Decrement by cycles (simplified - real timer would be clock-based)
            if self._counter > cycles:
                self._counter -= cycles
            else:
                self._counter = 0

        if self._counter == 0:
            # Set underflow flag
            self._status |= self.STATUS_UF

            # Reload counter
            self._counter = self._reload

            # Trigger interrupt if enabled
            if (self._ctrl & self.CTRL_IE) and self._irq >= 0:
                self.pulse_irq(self._irq)

    # Python API for testing
    @property
    def enabled(self) -> bool:
        return bool(self._ctrl & self.CTRL_EN)

    @property
    def underflow(self) -> bool:
        return bool(self._status & self.STATUS_UF)
```

## Testing Peripherals

```python
import pytest
from femu import Machine
from my_peripherals import SimpleTimer


class TestSimpleTimer:
    def test_countdown(self):
        machine = Machine.from_dict({
            "machine": {"name": "test", "arch": "armv8m"},
            "memory": [
                {"type": "flash", "base": 0x00000000, "size": "64K"},
                {"type": "ram", "base": 0x20000000, "size": "32K"},
            ],
        })

        timer = SimpleTimer(name="TIM1", irq=25)
        machine.emu.add_peripheral(timer, 0x40000000, 0x100)

        # Write to registers via emulator (simulates firmware)
        machine.write_mem(0x40000000, 100, size=4)  # COUNTER = 100
        machine.write_mem(0x40000008, 0x01, size=4)  # CTRL = EN

        # Tick the timer
        timer.tick(50)
        assert machine.read_mem(0x40000000, size=4) == 50

        timer.tick(50)
        assert timer.underflow

    def test_interrupt(self):
        # ... test interrupt triggering
        pass
```

## C Peripherals

For performance-critical peripherals, you can implement in C. See `docs/PLUGINS.md` for the full C peripheral interface.

The key structure is `EmuPeripheralVTable`:

```c
typedef struct {
    uint32_t (*read)(void *ctx, uint32_t offset, uint8_t size);
    void (*write)(void *ctx, uint32_t offset, uint32_t value, uint8_t size);
    void (*reset)(void *ctx);
    void (*tick)(void *ctx, uint64_t cycles);
    void (*destroy)(void *ctx);
    void (*set_irq_callback)(void *ctx, EmuPeripheralIRQCallback cb, void *emu_ctx);
} EmuPeripheralVTable;
```

## Plugin Peripherals

Plugins are shared libraries that export peripheral factories. See `docs/PLUGINS.md` for details on:

- Plugin structure and entry point
- Building plugins for different platforms
- Loading plugins at runtime

## Best Practices

1. **Document registers clearly** - Include register map in docstring
2. **Use constants** - Define register offsets and bit masks as constants
3. **Handle partial access** - Some firmware reads bytes instead of words
4. **Reset completely** - Reset all state, not just visible registers
5. **Test thoroughly** - Test register access, timing, and interrupts
6. **Log for debugging** - Use the logging system for debug output:

```python
from femu.logging import get_logger, LogCategory

logger = get_logger(LogCategory.PERIPHERAL)

class MyPeripheral(Peripheral):
    def write(self, offset: int, value: int, size: int) -> None:
        logger.debug("Write 0x%08x to offset 0x%02x", value, offset)
        ...
```
