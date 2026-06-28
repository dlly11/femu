"""Register-level unit tests for the built-in UART and GPIO peripherals.

These instantiate the peripherals directly and exercise their read/write
behaviour; no C library or emulator is required.
"""

from __future__ import annotations

from femu.peripherals.gpio import SimpleGPIO
from femu.peripherals.uart import SimpleUART


class TestUart:
    def test_tx_requires_enable_and_te(self) -> None:
        uart = SimpleUART()
        # Not enabled: write is dropped
        uart.write(uart.REG_DATA, ord("X"), 1)
        assert uart.get_output() == ""

    def test_transmit_collects_output(self) -> None:
        uart = SimpleUART()
        uart.write(uart.REG_CTRL, uart.CTRL_EN | uart.CTRL_TE, 4)
        for ch in "Hello":
            uart.write(uart.REG_DATA, ord(ch), 1)
        assert uart.get_output() == "Hello"
        assert uart.output_length == 5

    def test_status_txe_always_ready(self) -> None:
        uart = SimpleUART()
        status = uart.read(uart.REG_STATUS, 4)
        assert status & uart.STATUS_TXE

    def test_inject_input_sets_rxne_and_reads_back(self) -> None:
        uart = SimpleUART()
        uart.write(uart.REG_CTRL, uart.CTRL_EN | uart.CTRL_RE, 4)
        uart.inject_input("AB")
        assert uart.has_input
        assert uart.read(uart.REG_STATUS, 4) & uart.STATUS_RXNE
        assert uart.read(uart.REG_DATA, 1) == ord("A")
        assert uart.read(uart.REG_DATA, 1) == ord("B")

    def test_reset_clears_output(self) -> None:
        uart = SimpleUART()
        uart.write(uart.REG_CTRL, uart.CTRL_EN | uart.CTRL_TE, 4)
        uart.write(uart.REG_DATA, ord("Z"), 1)
        uart.reset()
        assert uart.get_output() == ""

    def test_clear_output(self) -> None:
        uart = SimpleUART()
        uart.write(uart.REG_CTRL, uart.CTRL_EN | uart.CTRL_TE, 4)
        uart.write(uart.REG_DATA, ord("Q"), 1)
        uart.clear_output()
        assert uart.output_length == 0


class TestGpio:
    def test_output_pin_via_odr(self) -> None:
        gpio = SimpleGPIO()
        gpio.write(gpio.REG_MODER, gpio.MODE_OUTPUT, 4)  # pin 0 -> output
        gpio.write(gpio.REG_ODR, 0x1, 4)
        assert gpio.get_output_pin(0) is True
        assert gpio.output_state & 0x1

    def test_input_pin_reflected_in_idr(self) -> None:
        gpio = SimpleGPIO()
        gpio.set_input_pin(3, True)
        assert gpio.get_input_pin(3) is True
        assert gpio.read(gpio.REG_IDR, 4) & (1 << 3)

    def test_pin_mode_readback(self) -> None:
        gpio = SimpleGPIO()
        gpio.write(gpio.REG_MODER, gpio.MODE_OUTPUT << (2 * 5), 4)  # pin 5 output
        assert gpio.get_pin_mode(5) == gpio.MODE_OUTPUT

    def test_idr_is_read_only(self) -> None:
        gpio = SimpleGPIO()
        before = gpio.read(gpio.REG_IDR, 4)
        gpio.write(gpio.REG_IDR, 0xFFFF, 4)  # writes to IDR must be ignored
        assert gpio.read(gpio.REG_IDR, 4) == before

    def test_output_change_callback(self) -> None:
        gpio = SimpleGPIO()
        events: list[tuple[int, bool]] = []
        gpio.on_output_change(lambda pin, state: events.append((pin, state)))
        gpio.write(gpio.REG_MODER, gpio.MODE_OUTPUT, 4)
        gpio.write(gpio.REG_ODR, 0x1, 4)
        assert (0, True) in events

    def test_reset_clears_outputs(self) -> None:
        gpio = SimpleGPIO()
        gpio.write(gpio.REG_MODER, gpio.MODE_OUTPUT, 4)
        gpio.write(gpio.REG_ODR, 0x1, 4)
        gpio.reset()
        assert gpio.output_state == 0
