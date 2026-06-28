"""Type definitions for CFFI bindings.

This module provides Protocol classes that describe CFFI interfaces
without using `Any`. These types enable proper type checking across
the codebase while maintaining compatibility with CFFI's dynamic nature.
"""

from __future__ import annotations

from typing import Protocol, runtime_checkable


@runtime_checkable
class CData(Protocol):
    """Protocol for CFFI CData objects (opaque pointers).

    CFFI returns CData objects for pointers and structs. This protocol
    describes the minimal interface needed for type checking.
    """

    def __bool__(self) -> bool:
        """Check if pointer is non-NULL."""
        ...


class EmulatorLib(Protocol):
    """Protocol describing the emulator C library interface.

    This defines all the functions exported by libarmv8m_emulator.so
    that are used by the Python bindings.
    """

    # Lifecycle API
    def armv8m_emu_default_config(self, config: ConfigStruct) -> None: ...
    def armv8m_emu_init(self, emu: CData, config: ConfigStruct | None) -> int: ...
    def armv8m_emu_destroy(self, emu: CData) -> None: ...
    def armv8m_emu_reset(self, emu: CData) -> None: ...

    # Memory Setup API
    def armv8m_emu_add_flash(self, emu: CData, base: int, size: int) -> int: ...
    def armv8m_emu_add_ram(self, emu: CData, base: int, size: int) -> int: ...
    def armv8m_emu_load(self, emu: CData, addr: int, data: bytes, size: int) -> int: ...

    # Execution API
    def armv8m_emu_step(self, emu: CData) -> int: ...
    def armv8m_emu_run(self, emu: CData, max_cycles: int) -> int: ...
    def armv8m_emu_stop(self, emu: CData) -> None: ...

    # State Access API
    def armv8m_emu_get_reg(self, emu: CData, reg: int) -> int: ...
    def armv8m_emu_set_reg(self, emu: CData, reg: int, value: int) -> None: ...
    def armv8m_emu_get_pc(self, emu: CData) -> int: ...
    def armv8m_emu_set_pc(self, emu: CData, value: int) -> None: ...
    def armv8m_emu_get_xpsr(self, emu: CData) -> int: ...
    def armv8m_emu_set_xpsr(self, emu: CData, value: int) -> None: ...
    def armv8m_emu_get_cycles(self, emu: CData) -> int: ...
    def armv8m_emu_get_state(self, emu: CData) -> int: ...
    def armv8m_emu_get_last_error(self, emu: CData) -> int: ...

    # Memory Access API
    def armv8m_emu_read_mem(self, emu: CData, addr: int, size: int, fault: CData) -> int: ...
    def armv8m_emu_write_mem(
        self, emu: CData, addr: int, value: int, size: int, fault: CData
    ) -> None: ...
    def armv8m_emu_read_block(self, emu: CData, addr: int, data: CData, size: int) -> int: ...
    def armv8m_emu_write_block(self, emu: CData, addr: int, data: bytes, size: int) -> int: ...

    # Breakpoint API
    def armv8m_emu_add_breakpoint(self, emu: CData, addr: int) -> int: ...
    def armv8m_emu_remove_breakpoint(self, emu: CData, addr: int) -> None: ...
    def armv8m_emu_has_breakpoint(self, emu: CData, addr: int) -> bool: ...
    def armv8m_emu_clear_breakpoints(self, emu: CData) -> None: ...

    # Watchpoint API
    def armv8m_emu_add_watchpoint(self, emu: CData, addr: int, size: int, wp_type: int) -> int: ...
    def armv8m_emu_remove_watchpoint(
        self, emu: CData, addr: int, size: int, wp_type: int
    ) -> None: ...
    def armv8m_emu_clear_watchpoints(self, emu: CData) -> None: ...
    def armv8m_emu_get_watchpoint_hit_addr(self, emu: CData) -> int: ...
    def armv8m_emu_get_watchpoint_hit_type(self, emu: CData) -> int: ...

    # Special Register Access
    def armv8m_emu_get_special_reg(self, emu: CData, reg: int) -> int: ...
    def armv8m_emu_set_special_reg(self, emu: CData, reg: int, value: int) -> None: ...
    def armv8m_emu_get_fpu_reg(self, emu: CData, reg: int) -> int: ...
    def armv8m_emu_set_fpu_reg(self, emu: CData, reg: int, value: int) -> None: ...
    def armv8m_emu_get_fpscr(self, emu: CData) -> int: ...
    def armv8m_emu_set_fpscr(self, emu: CData, value: int) -> None: ...

    # Peripheral API
    def armv8m_emu_add_peripheral(
        self, emu: CData, periph: PeripheralStruct, base: int, size: int
    ) -> int: ...

    # Logging API
    def emu_log_set_callback(self, callback: CData, ctx: CData) -> None: ...
    def emu_log_set_level(self, level: int) -> None: ...
    def emu_log_set_category_level(self, category: int, level: int) -> None: ...
    def emu_log_set_enabled(self, enabled: bool) -> None: ...
    def emu_log_is_enabled(self, level: int, category: int) -> bool: ...


class VTableStruct(Protocol):
    """Protocol for EmuPeripheralVTable C struct."""

    read: CData | None
    write: CData | None
    reset: CData | None
    tick: CData | None
    destroy: CData | None
    set_irq_callback: CData | None
    set_dma_callback: CData | None
    debug_state: CData | None


class PeripheralStruct(Protocol):
    """Protocol for EmuPeripheral C struct."""

    name: CData
    type: CData
    context: CData
    vtable: VTableStruct
    base_addr: int
    size: int
    emu_ctx: CData


class ConfigStruct(Protocol):
    """Protocol for EmulatorConfig C struct."""

    has_fpu: bool
    has_dsp: bool
    has_trustzone: bool
    num_mpu_regions: int
    num_irqs: int
    default_flash_base: int
    default_flash_size: int
    default_ram_base: int
    default_ram_size: int


class PluginInfo(Protocol):
    """Protocol for EmuPluginInfo C struct."""

    api_version: int
    name: CData
    version: CData
    author: CData
    description: CData


class PeripheralType(Protocol):
    """Protocol for EmuPeripheralType C struct."""

    type_name: CData
    description: CData
    create: CData
    destroy: CData


# Type aliases for callback signatures
IRQCallback = "Callable[[CData, int, int], None]"
DMACallback = "Callable[[CData, int, int], None]"
ReadCallback = "Callable[[CData, int, int], int]"
WriteCallback = "Callable[[CData, int, int, int], None]"
ResetCallback = "Callable[[CData], None]"
TickCallback = "Callable[[CData, int], None]"


__all__ = [
    "CData",
    "ConfigStruct",
    "DMACallback",
    "EmulatorLib",
    "IRQCallback",
    "PeripheralStruct",
    "PeripheralType",
    "PluginInfo",
    "ReadCallback",
    "ResetCallback",
    "TickCallback",
    "VTableStruct",
    "WriteCallback",
]
