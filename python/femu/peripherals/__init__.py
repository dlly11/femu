"""
Built-in example peripherals.

This module provides example peripheral implementations that demonstrate
how to create peripherals using the femu peripheral system.

Example usage:
    from femu import Machine
    from femu.peripherals import SimpleUART

    # Create machine and add UART
    machine = Machine.from_dict({
        "machine": {"name": "test", "arch": "armv8m"},
        "memory": [{"type": "ram", "base": 0x20000000, "size": "32K"}],
        "peripherals": []
    })

    # Add UART peripheral manually
    uart = SimpleUART(name="USART1")
    machine.emu.add_peripheral(uart, base=0x40013800, size=0x400)

    # Or use it in YAML config (after registering):
    # peripherals:
    #   - type: simple_uart
    #     name: USART1
    #     base: 0x40013800
    #     size: 0x400
"""

from .gpio import SimpleGPIO
from .uart import SimpleUART

__all__ = [
    "SimpleUART",
    "SimpleGPIO",
]
