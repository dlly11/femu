"""
Tests for the peripheral and machine configuration system.

Run with: python -m pytest tests/test_peripherals.py -v
"""

import pytest


def _emulator_available() -> bool:
    """Check if the emulator library is available."""
    try:
        from femu import _emulator_cffi as cffi

        cffi.get_lib()
        return True
    except OSError:
        return False


class TestPeripheralRegistry:
    """Test PeripheralRegistry functionality."""

    def test_register_decorator(self):
        """Test registering a peripheral with decorator."""
        from femu import Peripheral, PeripheralRegistry

        # Clear registry first for isolation
        PeripheralRegistry.clear()

        @PeripheralRegistry.register("test_periph", "Test peripheral")
        class TestPeriph(Peripheral):
            def __init__(self, name: str = "test"):
                super().__init__(name, "test_periph")

            def read(self, offset: int, size: int) -> int:
                return 0x42

            def write(self, offset: int, value: int, size: int) -> None:
                pass

        assert PeripheralRegistry.has_type("test_periph")
        info = PeripheralRegistry.get_type_info("test_periph")
        assert info is not None
        assert info.source == "python"
        assert info.description == "Test peripheral"

    def test_create_peripheral(self):
        """Test creating a peripheral from registry."""
        from femu import Peripheral, PeripheralRegistry

        PeripheralRegistry.clear()

        @PeripheralRegistry.register("counter")
        class Counter(Peripheral):
            def __init__(self, name: str = "cnt", initial: int = 0):
                super().__init__(name, "counter")
                self._value = initial

            def read(self, offset: int, size: int) -> int:
                return self._value

            def write(self, offset: int, value: int, size: int) -> None:
                self._value = value

        # Create via registry
        periph = PeripheralRegistry.create("counter", "CNT1", config={"initial": 100})
        assert periph.name == "CNT1"

    def test_list_types(self):
        """Test listing registered types."""
        from femu import Peripheral, PeripheralRegistry

        PeripheralRegistry.clear()

        @PeripheralRegistry.register("type_a")
        class TypeA(Peripheral):
            def __init__(self, name: str = "a"):
                super().__init__(name, "type_a")

            def read(self, offset, size):
                return 0

            def write(self, offset, value, size):
                pass

        @PeripheralRegistry.register("type_b")
        class TypeB(Peripheral):
            def __init__(self, name: str = "b"):
                super().__init__(name, "type_b")

            def read(self, offset, size):
                return 0

            def write(self, offset, value, size):
                pass

        types = PeripheralRegistry.list_types()
        names = [t.name for t in types]
        assert "type_a" in names
        assert "type_b" in names


class TestBuiltinPeripherals:
    """Test built-in peripheral implementations."""

    def test_simple_uart_tx(self):
        """Test SimpleUART transmit functionality."""
        from femu.peripherals import SimpleUART

        uart = SimpleUART(name="USART1")

        # Enable UART and TX
        uart.write(uart.REG_CTRL, uart.CTRL_EN | uart.CTRL_TE, 4)

        # Write characters
        uart.write(uart.REG_DATA, ord("H"), 4)
        uart.write(uart.REG_DATA, ord("i"), 4)

        assert uart.get_output() == "Hi"
        assert uart.output_length == 2

    def test_simple_uart_rx(self):
        """Test SimpleUART receive functionality."""
        from femu.peripherals import SimpleUART

        uart = SimpleUART(name="USART1")

        # Inject input
        uart.inject_input("Test")

        # Check status shows data available
        status = uart.read(uart.REG_STATUS, 4)
        assert status & uart.STATUS_RXNE

        # Read characters
        assert uart.read(uart.REG_DATA, 4) == ord("T")
        assert uart.read(uart.REG_DATA, 4) == ord("e")
        assert uart.read(uart.REG_DATA, 4) == ord("s")
        assert uart.read(uart.REG_DATA, 4) == ord("t")

        # Buffer should be empty now
        assert not uart.has_input

    def test_simple_gpio_output(self):
        """Test SimpleGPIO output functionality."""
        from femu.peripherals import SimpleGPIO

        gpio = SimpleGPIO(name="GPIOA")

        # Configure pin 5 as output
        gpio.write(gpio.REG_MODER, 0b01 << 10, 4)  # Pin 5 = output

        # Set pin 5 high
        gpio.write(gpio.REG_ODR, 1 << 5, 4)
        assert gpio.get_output_pin(5) is True

        # Set pin 5 low
        gpio.write(gpio.REG_ODR, 0, 4)
        assert gpio.get_output_pin(5) is False

    def test_simple_gpio_input(self):
        """Test SimpleGPIO input functionality."""
        from femu.peripherals import SimpleGPIO

        gpio = SimpleGPIO(name="GPIOA")

        # Configure pin 0 as input (default)
        gpio.write(gpio.REG_MODER, 0, 4)

        # Set external input
        gpio.set_input_pin(0, True)

        # Read IDR
        idr = gpio.read(gpio.REG_IDR, 4)
        assert idr & (1 << 0)

        gpio.set_input_pin(0, False)
        idr = gpio.read(gpio.REG_IDR, 4)
        assert not (idr & (1 << 0))

    def test_simple_gpio_bsrr(self):
        """Test SimpleGPIO BSRR (bit set/reset) register."""
        from femu.peripherals import SimpleGPIO

        gpio = SimpleGPIO(name="GPIOA")

        # Set pin 3
        gpio.write(gpio.REG_BSRR, 1 << 3, 4)
        assert gpio.get_output_pin(3) is True

        # Reset pin 3 (bit 19 = 3 + 16)
        gpio.write(gpio.REG_BSRR, 1 << 19, 4)
        assert gpio.get_output_pin(3) is False


class TestMachine:
    """Test Machine class functionality."""

    def test_machine_from_dict(self):
        """Test creating Machine from dict."""
        from femu import Machine

        machine = Machine.from_dict(
            {
                "machine": {"name": "test", "arch": "armv8m"},
                "cpu": {"has_fpu": True},
                "memory": [
                    {"type": "flash", "base": 0x08000000, "size": "64K"},
                    {"type": "ram", "base": 0x20000000, "size": "32K"},
                ],
                "peripherals": [],
            }
        )

        assert machine.name == "test"
        assert machine.arch == "armv8m"
        assert machine.emu is not None

    def test_machine_memory_parsing(self):
        """Test memory size parsing in Machine."""
        from femu.machine import _parse_address, _parse_size

        assert _parse_size("64K") == 64 * 1024
        assert _parse_size("1M") == 1024 * 1024
        assert _parse_size("0x10000") == 0x10000
        assert _parse_size(4096) == 4096

        assert _parse_address("0x08000000") == 0x08000000
        assert _parse_address(0x20000000) == 0x20000000

    def test_machine_with_peripheral(self):
        """Test Machine with peripheral in config."""
        from femu import Machine, Peripheral, PeripheralRegistry

        PeripheralRegistry.clear()

        @PeripheralRegistry.register("test_led")
        class TestLED(Peripheral):
            def __init__(self, name: str = "led"):
                super().__init__(name, "test_led")
                self._state = 0

            def read(self, offset: int, size: int) -> int:
                return self._state

            def write(self, offset: int, value: int, size: int) -> None:
                self._state = value & 0xFF

        machine = Machine.from_dict(
            {
                "machine": {"name": "led_test", "arch": "armv8m"},
                "cpu": {},
                "memory": [
                    {"type": "ram", "base": 0x20000000, "size": "32K"},
                ],
                "peripherals": [
                    {"type": "test_led", "name": "LED1", "base": 0x40000000, "size": 0x100}
                ],
            }
        )

        assert "LED1" in machine.peripheral_names
        led = machine.get_peripheral("LED1")
        assert led is not None
        assert led.name == "LED1"


class TestPeripheralIntegration:
    """Integration tests for peripherals with the emulator."""

    @pytest.mark.skipif(not _emulator_available(), reason="Emulator library not built")
    def test_peripheral_registration(self):
        """Test peripheral registration with emulator."""
        from femu import ARMv8MEmulator, Peripheral, PeripheralRegistry

        PeripheralRegistry.clear()

        @PeripheralRegistry.register("mmio_test")
        class MMIOTest(Peripheral):
            def __init__(self, name: str = "mmio"):
                super().__init__(name, "mmio_test")
                self._reg = 0

            def read(self, offset: int, size: int) -> int:
                if offset == 0:
                    return self._reg
                return 0xDEADBEEF

            def write(self, offset: int, value: int, size: int) -> None:
                if offset == 0:
                    self._reg = value

        emu = ARMv8MEmulator()
        emu.add_ram(0x20000000, 0x8000)

        periph = PeripheralRegistry.create("mmio_test", "TEST1")
        emu.add_peripheral(periph, 0x40000000, 0x100)

        assert len(emu.peripherals) == 1

    @pytest.mark.skipif(not _emulator_available(), reason="Emulator library not built")
    def test_peripheral_mmio_with_firmware(self):
        """Test peripheral MMIO read/write via actual firmware execution."""
        from pathlib import Path

        from femu import ARMv8MEmulator, EmulatorState, Peripheral

        # Path to test firmware
        firmware_path = Path(__file__).parent / "firmware" / "test_peripheral.elf"
        if not firmware_path.exists():
            pytest.skip(f"Test firmware not found: {firmware_path}")

        # Create a test peripheral that tracks all accesses
        class TrackedPeripheral(Peripheral):
            def __init__(self, name: str = "tracked"):
                super().__init__(name, "tracked")
                self.reads: list[tuple[int, int]] = []  # (offset, size)
                self.writes: list[tuple[int, int, int]] = []  # (offset, value, size)
                self._data_reg = 0  # offset 0x04 - read/write register
                self.tick_count = 0
                self.reset_count = 0

            def read(self, offset: int, size: int) -> int:
                self.reads.append((offset, size))
                if offset == 0x00:
                    return 0x12345678  # Fixed value for testing
                elif offset == 0x04:
                    return self._data_reg
                return 0

            def write(self, offset: int, value: int, size: int) -> None:
                self.writes.append((offset, value, size))
                if offset == 0x04:
                    self._data_reg = value

            def tick(self, cycles: int) -> None:
                self.tick_count += cycles

            def reset(self) -> None:
                self.reset_count += 1

        # Set up emulator
        emu = ARMv8MEmulator()

        # Create and attach peripheral at 0x40000000
        periph = TrackedPeripheral(name="TEST")
        emu.add_peripheral(periph, 0x40000000, 0x100)

        # Load and run firmware
        emu.load_elf(firmware_path)
        cycles = emu.run(max_cycles=100000)

        # Check firmware completed successfully
        assert emu.state == EmulatorState.BREAKPOINT, f"Unexpected state: {emu.state}"

        # Verify peripheral received reads
        # Firmware reads: offset 0x00, then offset 0x04
        assert len(periph.reads) >= 2, f"Expected at least 2 reads, got {len(periph.reads)}"
        assert (0x00, 4) in periph.reads, f"Missing read from offset 0x00: {periph.reads}"
        assert (0x04, 4) in periph.reads, f"Missing read from offset 0x04: {periph.reads}"

        # Verify peripheral received writes
        # Firmware writes: 0xAABBCCDD to offset 0x04, 0x42 to offset 0x08
        assert len(periph.writes) >= 2, f"Expected at least 2 writes, got {len(periph.writes)}"
        assert (
            0x04,
            0xAABBCCDD,
            4,
        ) in periph.writes, f"Missing write to offset 0x04: {periph.writes}"
        assert (0x08, 0x42, 4) in periph.writes, f"Missing write to offset 0x08: {periph.writes}"

        # Verify tick was called (should be called every cycle)
        assert periph.tick_count > 0, "tick() was never called"

        # Verify memory results stored by firmware
        # Firmware stores read results to RAM at 0x20000000
        result_0x00 = emu.read_mem(0x20000000, 4)
        result_0x04 = emu.read_mem(0x20000004, 4)
        done_marker = emu.read_mem(0x20000008, 4)

        assert result_0x00 == 0x12345678, f"Read from 0x00 mismatch: got 0x{result_0x00:08x}"
        assert result_0x04 == 0xAABBCCDD, f"Read from 0x04 mismatch: got 0x{result_0x04:08x}"
        assert done_marker == 0xC0FFEE42, f"Done marker mismatch: got 0x{done_marker:08x}"

    @pytest.mark.skipif(not _emulator_available(), reason="Emulator library not built")
    def test_peripheral_reset(self):
        """Test that peripheral reset() is called on emulator reset."""
        from femu import ARMv8MEmulator, Peripheral

        class ResetTracker(Peripheral):
            def __init__(self, name: str = "reset_tracker"):
                super().__init__(name, "reset_tracker")
                self.reset_count = 0

            def read(self, offset: int, size: int) -> int:
                return 0

            def write(self, offset: int, value: int, size: int) -> None:
                pass

            def reset(self) -> None:
                self.reset_count += 1

        emu = ARMv8MEmulator()
        emu.add_ram(0x20000000, 0x8000)

        periph = ResetTracker(name="RESET_TEST")
        emu.add_peripheral(periph, 0x40000000, 0x100)

        # Reset emulator
        emu.reset()

        assert periph.reset_count == 1, f"Expected 1 reset call, got {periph.reset_count}"

        # Reset again
        emu.reset()
        assert periph.reset_count == 2, f"Expected 2 reset calls, got {periph.reset_count}"

    @pytest.mark.skipif(not _emulator_available(), reason="Emulator library not built")
    def test_peripheral_irq(self):
        """Test peripheral IRQ triggering via firmware execution."""
        from pathlib import Path

        from femu import ARMv8MEmulator, EmulatorState, Peripheral

        # Path to test firmware
        firmware_path = Path(__file__).parent / "firmware" / "test_irq.elf"
        if not firmware_path.exists():
            pytest.skip(f"Test firmware not found: {firmware_path}")

        # Create a peripheral that triggers IRQ when written to
        class IRQPeripheral(Peripheral):
            def __init__(self, name: str = "irq_periph", irq: int = 0):
                super().__init__(name, "irq_periph")
                self._irq = irq
                self.irq_asserted = False
                self.write_count = 0

            def read(self, offset: int, size: int) -> int:
                return 0

            def write(self, offset: int, value: int, size: int) -> None:
                self.write_count += 1
                if offset == 0x00:
                    if value != 0:
                        # Assert IRQ
                        self.irq_asserted = True
                        self.assert_irq(self._irq)
                    else:
                        # Deassert IRQ
                        self.irq_asserted = False
                        self.deassert_irq(self._irq)

        # Set up emulator
        emu = ARMv8MEmulator()

        # Create and attach peripheral at 0x40000000
        periph = IRQPeripheral(name="IRQ_TEST", irq=0)
        emu.add_peripheral(periph, 0x40000000, 0x100)

        # Load and run firmware
        emu.load_elf(firmware_path)
        cycles = emu.run(max_cycles=10000)

        # Check firmware completed
        assert emu.state == EmulatorState.BREAKPOINT, f"Unexpected state: {emu.state}"

        # Check peripheral was written to (trigger + clear)
        assert periph.write_count >= 2, f"Expected at least 2 writes, got {periph.write_count}"

        # Verify memory results
        pre_isr = emu.read_mem(0x20000000, 4)
        isr_marker = emu.read_mem(0x20000004, 4)
        done_marker = emu.read_mem(0x20000008, 4)

        assert pre_isr == 0x11111111, f"Pre-ISR marker wrong: 0x{pre_isr:08x}"
        assert isr_marker == 0x22222222, f"ISR marker wrong: 0x{isr_marker:08x} (ISR didn't run)"
        assert done_marker == 0xC0FFEE42, f"Done marker wrong: 0x{done_marker:08x}"


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
