"""
Simple UART peripheral implementation.

This provides a basic UART that can capture output from firmware
and optionally inject input.

Register layout (typical ARM UART style):
    0x00: DATA   - Data register (read/write)
    0x04: STATUS - Status register (read-only)
    0x08: CTRL   - Control register (read/write)

Status register bits:
    bit 0: RXNE - Receive buffer not empty
    bit 1: TXE  - Transmit buffer empty (always 1 in this simple impl)
    bit 7: BUSY - UART busy (always 0 in this simple impl)

Control register bits:
    bit 0: EN   - UART enable
    bit 3: TE   - Transmitter enable
    bit 4: RE   - Receiver enable
"""

from __future__ import annotations

from collections import deque

from ..peripheral import Peripheral
from ..peripheral_registry import PeripheralRegistry


@PeripheralRegistry.register("simple_uart", "Simple UART with TX/RX buffering")
class SimpleUART(Peripheral):
    """
    Simple UART peripheral for capturing firmware output.

    Example:
        uart = SimpleUART(name="USART1")
        machine.emu.add_peripheral(uart, 0x40013800, 0x400)

        # Run firmware...
        machine.run(max_cycles=100000)

        # Get output
        print(uart.get_output())
    """

    # Register offsets
    REG_DATA = 0x00
    REG_STATUS = 0x04
    REG_CTRL = 0x08

    # Status bits
    STATUS_RXNE = 1 << 0  # Receive buffer not empty
    STATUS_TXE = 1 << 1  # Transmit buffer empty

    # Control bits
    CTRL_EN = 1 << 0  # UART enable
    CTRL_TE = 1 << 3  # Transmitter enable
    CTRL_RE = 1 << 4  # Receiver enable

    def __init__(
        self, name: str = "uart", irq: int = -1, echo: bool = False, max_buffer: int = 4096
    ):
        """
        Initialize UART peripheral.

        Args:
            name: Instance name (e.g., "USART1")
            irq: IRQ number for interrupts (-1 to disable)
            echo: If True, print characters as they're transmitted
            max_buffer: Maximum characters to buffer
        """
        super().__init__(name, "simple_uart")
        self._irq = irq
        self._echo = echo
        self._max_buffer = max_buffer

        # State
        self._ctrl = 0
        self._tx_buffer: list[int] = []
        self._rx_buffer: deque[int] = deque(maxlen=max_buffer)

    def read(self, offset: int, size: int) -> int:
        """Handle register reads."""
        if offset == self.REG_DATA:
            # Read from receive buffer
            if self._rx_buffer:
                return self._rx_buffer.popleft()
            return 0

        elif offset == self.REG_STATUS:
            status = self.STATUS_TXE  # TX always ready
            if self._rx_buffer:
                status |= self.STATUS_RXNE
            return status

        elif offset == self.REG_CTRL:
            return self._ctrl

        return 0

    def write(self, offset: int, value: int, size: int) -> None:
        """Handle register writes."""
        if offset == self.REG_DATA:
            # Write to transmit
            if self._ctrl & self.CTRL_EN and self._ctrl & self.CTRL_TE:
                char = value & 0xFF
                self._tx_buffer.append(char)
                if self._echo:
                    print(chr(char), end="", flush=True)

        elif offset == self.REG_CTRL:
            self._ctrl = value

    def reset(self) -> None:
        """Reset UART to initial state."""
        self._ctrl = 0
        self._tx_buffer.clear()
        self._rx_buffer.clear()

    # =========================================================================
    # Python API
    # =========================================================================

    def get_output(self) -> str:
        """
        Get all transmitted data as a string.

        Returns:
            String of all characters transmitted by firmware
        """
        return "".join(chr(c) for c in self._tx_buffer)

    def get_output_bytes(self) -> bytes:
        """
        Get all transmitted data as bytes.

        Returns:
            Bytes of all data transmitted by firmware
        """
        return bytes(self._tx_buffer)

    def clear_output(self) -> None:
        """Clear the transmit buffer."""
        self._tx_buffer.clear()

    def inject_input(self, data: str | bytes) -> None:
        """
        Inject data into the receive buffer.

        This simulates data being received by the UART.

        Args:
            data: String or bytes to inject
        """
        if isinstance(data, str):
            data = data.encode("utf-8")
        for byte in data:
            self._rx_buffer.append(byte)

    @property
    def has_input(self) -> bool:
        """Check if there's data in the receive buffer."""
        return len(self._rx_buffer) > 0

    @property
    def output_length(self) -> int:
        """Number of bytes in transmit buffer."""
        return len(self._tx_buffer)
