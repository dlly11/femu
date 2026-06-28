"""Simple GPIO peripheral implementation.

This provides a basic GPIO port with configurable pins.

Register layout (simplified GPIO style):
    0x00: MODER  - Mode register (2 bits per pin)
    0x04: OTYPER - Output type register
    0x08: OSPEED - Output speed register
    0x0C: PUPDR  - Pull-up/pull-down register
    0x10: IDR    - Input data register (read-only)
    0x14: ODR    - Output data register
    0x18: BSRR   - Bit set/reset register (write-only)

Mode bits (per pin):
    00: Input
    01: Output
    10: Alternate function
    11: Analog
"""

from __future__ import annotations

from typing import TYPE_CHECKING

from ..peripheral import Peripheral
from ..peripheral_registry import PeripheralRegistry

if TYPE_CHECKING:
    from collections.abc import Callable


@PeripheralRegistry.register("simple_gpio", "Simple 16-pin GPIO port")
class SimpleGPIO(Peripheral):
    """Simple GPIO port peripheral.

    Example:
        gpio = SimpleGPIO(name="GPIOA")
        machine.emu.add_peripheral(gpio, 0x48000000, 0x400)

        # Set external pin state (simulating button press)
        gpio.set_input_pin(0, True)

        # Run firmware...
        machine.run(max_cycles=100000)

        # Read output state
        print(f"Pin 5 output: {gpio.get_output_pin(5)}")
    """

    # Register offsets
    REG_MODER = 0x00
    REG_OTYPER = 0x04
    REG_OSPEED = 0x08
    REG_PUPDR = 0x0C
    REG_IDR = 0x10
    REG_ODR = 0x14
    REG_BSRR = 0x18

    # Pin modes
    MODE_INPUT = 0
    MODE_OUTPUT = 1
    MODE_ALTFUNC = 2
    MODE_ANALOG = 3

    def __init__(self, name: str = "gpio", num_pins: int = 16, irq: int = -1) -> None:
        """Initialize GPIO peripheral.

        Args:
            name: Instance name (e.g., "GPIOA")
            num_pins: Number of GPIO pins (default 16)
            irq: IRQ number for interrupts (-1 to disable)
        """
        super().__init__(name, "simple_gpio")
        self._num_pins = min(num_pins, 16)
        self._irq = irq

        # Registers
        self._moder = 0
        self._otyper = 0
        self._ospeed = 0
        self._pupdr = 0
        self._odr = 0

        # External input state (set by Python)
        self._external_input = 0

        # Callbacks for pin changes
        self._on_output_change: Callable[[int, bool], None] | None = None

    def read(self, offset: int, size: int) -> int:
        """Handle register reads."""
        register_values = {
            self.REG_MODER: self._moder,
            self.REG_OTYPER: self._otyper,
            self.REG_OSPEED: self._ospeed,
            self.REG_PUPDR: self._pupdr,
            self.REG_IDR: self._get_idr(),
            self.REG_ODR: self._odr,
            self.REG_BSRR: 0,  # Write-only register reads as zero
        }
        return register_values.get(offset, 0)

    def write(self, offset: int, value: int, size: int) -> None:
        """Handle register writes."""
        if offset == self.REG_MODER:
            self._moder = value
        elif offset == self.REG_OTYPER:
            self._otyper = value & 0xFFFF
        elif offset == self.REG_OSPEED:
            self._ospeed = value
        elif offset == self.REG_PUPDR:
            self._pupdr = value
        elif offset == self.REG_ODR:
            old_odr = self._odr
            self._odr = value & 0xFFFF
            self._notify_changes(old_odr, self._odr)
        elif offset == self.REG_BSRR:
            old_odr = self._odr
            # Low 16 bits set, high 16 bits reset
            self._odr |= value & 0xFFFF
            self._odr &= ~((value >> 16) & 0xFFFF)
            self._notify_changes(old_odr, self._odr)

    def reset(self) -> None:
        """Reset GPIO to initial state."""
        self._moder = 0
        self._otyper = 0
        self._ospeed = 0
        self._pupdr = 0
        self._odr = 0
        self._external_input = 0

    def _get_idr(self) -> int:
        """Get input data register value.

        For input pins, returns external input state.
        For output pins, returns ODR value.
        """
        result = 0
        for pin in range(self._num_pins):
            mode = (self._moder >> (pin * 2)) & 0x3
            if mode == self.MODE_INPUT:
                # Input mode: use external input
                if self._external_input & (1 << pin):
                    result |= 1 << pin
            # Output/alternate/analog: return ODR
            elif self._odr & (1 << pin):
                result |= 1 << pin
        return result

    def _notify_changes(self, old_val: int, new_val: int) -> None:
        """Notify callback of pin changes."""
        if self._on_output_change is None:
            return

        changed = old_val ^ new_val
        for pin in range(self._num_pins):
            if changed & (1 << pin):
                self._on_output_change(pin, bool(new_val & (1 << pin)))

    # =========================================================================
    # Python API
    # =========================================================================

    def set_input_pin(self, pin: int, state: bool) -> None:
        """Set external input state for a pin.

        This simulates external signals (button presses, sensor inputs, etc.).

        Args:
            pin: Pin number (0-15)
            state: True for high, False for low
        """
        if not 0 <= pin < self._num_pins:
            raise ValueError(f"Invalid pin number: {pin}")

        if state:
            self._external_input |= 1 << pin
        else:
            self._external_input &= ~(1 << pin)

    def get_input_pin(self, pin: int) -> bool:
        """Get external input state for a pin."""
        if not 0 <= pin < self._num_pins:
            raise ValueError(f"Invalid pin number: {pin}")
        return bool(self._external_input & (1 << pin))

    def get_output_pin(self, pin: int) -> bool:
        """Get output state for a pin.

        Args:
            pin: Pin number (0-15)

        Returns:
            True if pin is high, False if low
        """
        if not 0 <= pin < self._num_pins:
            raise ValueError(f"Invalid pin number: {pin}")
        return bool(self._odr & (1 << pin))

    def get_pin_mode(self, pin: int) -> int:
        """Get mode for a pin.

        Args:
            pin: Pin number (0-15)

        Returns:
            Mode value (0=input, 1=output, 2=altfunc, 3=analog)
        """
        if not 0 <= pin < self._num_pins:
            raise ValueError(f"Invalid pin number: {pin}")
        return (self._moder >> (pin * 2)) & 0x3

    def on_output_change(self, callback: Callable[[int, bool], None]) -> None:
        """Register callback for output changes.

        Args:
            callback: Function(pin, state) called when output changes
        """
        self._on_output_change = callback

    @property
    def output_state(self) -> int:
        """Get full ODR value."""
        return self._odr

    @property
    def input_state(self) -> int:
        """Get full external input value."""
        return self._external_input
