"""
ARMv8-M emulator implementation.

This module provides the ARMv8-M (Cortex-M33) specific emulator that
implements the BaseEmulator interface.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import TYPE_CHECKING

from .base import (
    ArchType,
    BaseEmulator,
    BaseEmulatorConfig,
    EmulatorError,
    EmulatorState,
    ExecutionError,
    MemoryFaultError,
)

from .. import _emulator_cffi as cffi

if TYPE_CHECKING:
    from ..elf_loader import ElfInfo
    from ..peripheral import PeripheralBase


@dataclass
class ARMv8MConfig(BaseEmulatorConfig):
    """ARMv8-M specific configuration."""

    has_fpu: bool = False
    has_dsp: bool = False
    has_trustzone: bool = False
    num_mpu_regions: int = 8

    # ARM defaults
    flash_base: int = 0x08000000
    flash_size: int = 0x80000  # 512KB
    ram_base: int = 0x20000000
    ram_size: int = 0x20000  # 128KB


class ARMv8MEmulator(BaseEmulator):
    """
    ARMv8-M (Cortex-M33) emulator.

    Example usage:
        emu = ARMv8MEmulator()
        emu.load_elf("firmware.elf")
        cycles = emu.run(max_cycles=1000000)
        print(f"Executed {cycles} cycles, final PC: 0x{emu.pc:08x}")
    """

    # Register names for dump_regs()
    _REG_NAMES = [
        "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
        "r8", "r9", "r10", "r11", "r12", "sp", "lr", "pc",
    ]

    def __init__(self, config: ARMv8MConfig | None = None):
        """
        Initialize ARMv8-M emulator.

        Args:
            config: Optional configuration. If None, uses defaults.
        """
        self._lib = cffi.get_lib()
        self._ffi = cffi.get_ffi()

        # Create emulator instance
        self._emu_ptr, self._cleanup, self._emu_buffer = cffi.create_emulator()

        # Apply configuration
        if config is None:
            result = self._lib.armv8m_emu_init(self._emu_ptr, self._ffi.NULL)
        else:
            c_config, _ = cffi.create_config()
            c_config.has_fpu = config.has_fpu
            c_config.has_dsp = config.has_dsp
            c_config.has_trustzone = config.has_trustzone
            c_config.num_mpu_regions = config.num_mpu_regions
            c_config.num_irqs = config.num_irqs
            c_config.default_flash_base = config.flash_base
            c_config.default_flash_size = config.flash_size
            c_config.default_ram_base = config.ram_base
            c_config.default_ram_size = config.ram_size
            result = self._lib.armv8m_emu_init(self._emu_ptr, c_config)

        if result != cffi.ARMV8M_OK:
            raise EmulatorError(f"Failed to initialize emulator (code {result})")

        self._config = config or ARMv8MConfig()
        self._elf_info: ElfInfo | None = None
        self._peripherals: list[PeripheralBase] = []

    def __del__(self):
        """Clean up emulator resources."""
        if hasattr(self, "_cleanup"):
            self._cleanup()

    # =========================================================================
    # Architecture Info
    # =========================================================================

    @property
    def arch(self) -> ArchType:
        return ArchType.ARMV8M

    @property
    def arch_name(self) -> str:
        return "ARMv8-M"

    # =========================================================================
    # Memory Setup
    # =========================================================================

    def add_flash(self, base: int, size: int) -> None:
        result = self._lib.armv8m_emu_add_flash(self._emu_ptr, base, size)
        if result != cffi.ARMV8M_OK:
            raise EmulatorError(f"Failed to add flash (code {result})")

    def add_ram(self, base: int, size: int) -> None:
        result = self._lib.armv8m_emu_add_ram(self._emu_ptr, base, size)
        if result != cffi.ARMV8M_OK:
            raise EmulatorError(f"Failed to add RAM (code {result})")

    def load(self, addr: int, data: bytes) -> None:
        result = self._lib.armv8m_emu_load(self._emu_ptr, addr, data, len(data))
        if result != cffi.ARMV8M_OK:
            raise EmulatorError(f"Failed to load data at 0x{addr:08x} (code {result})")

    def load_elf(self, path: str | Path) -> "ElfInfo":
        elf = super().load_elf(path)
        self._elf_info = elf
        return elf

    # =========================================================================
    # Execution
    # =========================================================================

    def step(self) -> int:
        result = self._lib.armv8m_emu_step(self._emu_ptr)

        if result == cffi.ARMV8M_ERR_BREAKPOINT:
            return result  # Let caller handle breakpoint

        if result < 0 and result != cffi.ARMV8M_ERR_HALTED:
            raise ExecutionError(result)

        return result

    def run(self, max_cycles: int = 0) -> int:
        result = self._lib.armv8m_emu_run(self._emu_ptr, max_cycles)

        if result < 0:
            state = self.state
            if state == EmulatorState.FAULT:
                raise ExecutionError(int(-result), "Execution fault")
            # Breakpoint or halt - return absolute value
            return int(-result)

        return int(result)

    def stop(self) -> None:
        self._lib.armv8m_emu_stop(self._emu_ptr)

    def reset(self) -> None:
        self._lib.armv8m_emu_reset(self._emu_ptr)

    # =========================================================================
    # State Access
    # =========================================================================

    def get_reg(self, reg: int) -> int:
        if not 0 <= reg < 16:
            raise ValueError(f"Invalid register number: {reg}")
        return self._lib.armv8m_emu_get_reg(self._emu_ptr, reg)

    def set_reg(self, reg: int, value: int) -> None:
        if not 0 <= reg < 16:
            raise ValueError(f"Invalid register number: {reg}")
        self._lib.armv8m_emu_set_reg(self._emu_ptr, reg, value & 0xFFFFFFFF)

    @property
    def pc(self) -> int:
        return self._lib.armv8m_emu_get_pc(self._emu_ptr)

    @pc.setter
    def pc(self, value: int) -> None:
        self._lib.armv8m_emu_set_pc(self._emu_ptr, value & 0xFFFFFFFF)

    @property
    def sp(self) -> int:
        """Stack pointer (R13)."""
        return self.get_reg(cffi.ARMV8M_REG_SP)

    @sp.setter
    def sp(self, value: int) -> None:
        self.set_reg(cffi.ARMV8M_REG_SP, value)

    @property
    def lr(self) -> int:
        """Link register (R14)."""
        return self.get_reg(cffi.ARMV8M_REG_LR)

    @lr.setter
    def lr(self, value: int) -> None:
        self.set_reg(cffi.ARMV8M_REG_LR, value)

    @property
    def xpsr(self) -> int:
        """Program status register (ARM-specific alias for status)."""
        return self._lib.armv8m_emu_get_xpsr(self._emu_ptr)

    @xpsr.setter
    def xpsr(self, value: int) -> None:
        self._lib.armv8m_emu_set_xpsr(self._emu_ptr, value & 0xFFFFFFFF)

    @property
    def status(self) -> int:
        """Status register (generic name, same as xpsr for ARM)."""
        return self.xpsr

    @status.setter
    def status(self, value: int) -> None:
        self.xpsr = value

    @property
    def cycles(self) -> int:
        return self._lib.armv8m_emu_get_cycles(self._emu_ptr)

    @property
    def state(self) -> EmulatorState:
        return EmulatorState(self._lib.armv8m_emu_get_state(self._emu_ptr))

    @property
    def last_error(self) -> int:
        return self._lib.armv8m_emu_get_last_error(self._emu_ptr)

    # =========================================================================
    # Memory Access
    # =========================================================================

    def read_mem(self, addr: int, size: int = 4) -> int:
        if size not in (1, 2, 4):
            raise ValueError(f"Invalid access size: {size}")

        fault = self._ffi.new("bool *")
        value = self._lib.armv8m_emu_read_mem(self._emu_ptr, addr, size, fault)

        if fault[0]:
            raise MemoryFaultError(addr, "Memory read fault")

        return value

    def write_mem(self, addr: int, value: int, size: int = 4) -> None:
        if size not in (1, 2, 4):
            raise ValueError(f"Invalid access size: {size}")

        fault = self._ffi.new("bool *")
        self._lib.armv8m_emu_write_mem(self._emu_ptr, addr, value & 0xFFFFFFFF, size, fault)

        if fault[0]:
            raise MemoryFaultError(addr, "Memory write fault")

    def read_bytes(self, addr: int, length: int) -> bytes:
        buf = self._ffi.new(f"uint8_t[{length}]")
        read = self._lib.armv8m_emu_read_block(self._emu_ptr, addr, buf, length)
        return bytes(buf[0:read])

    def write_bytes(self, addr: int, data: bytes) -> int:
        return self._lib.armv8m_emu_write_block(self._emu_ptr, addr, data, len(data))

    # =========================================================================
    # Breakpoints
    # =========================================================================

    def add_breakpoint(self, addr: int) -> None:
        result = self._lib.armv8m_emu_add_breakpoint(self._emu_ptr, addr)
        if result != cffi.ARMV8M_OK:
            raise EmulatorError(f"Failed to add breakpoint (code {result})")

    def remove_breakpoint(self, addr: int) -> None:
        self._lib.armv8m_emu_remove_breakpoint(self._emu_ptr, addr)

    def has_breakpoint(self, addr: int) -> bool:
        return self._lib.armv8m_emu_has_breakpoint(self._emu_ptr, addr)

    def clear_breakpoints(self) -> None:
        self._lib.armv8m_emu_clear_breakpoints(self._emu_ptr)

    # =========================================================================
    # Special Registers
    # =========================================================================

    def get_special_reg(self, reg_id: int) -> int:
        return self._lib.armv8m_emu_get_special_reg(self._emu_ptr, reg_id)

    def set_special_reg(self, reg_id: int, value: int) -> None:
        self._lib.armv8m_emu_set_special_reg(self._emu_ptr, reg_id, value & 0xFFFFFFFF)

    def get_fpu_reg(self, reg: int) -> int:
        """Get FPU register (S0-S31) as uint32."""
        if not 0 <= reg < 32:
            raise ValueError(f"Invalid FPU register: {reg}")
        return self._lib.armv8m_emu_get_fpu_reg(self._emu_ptr, reg)

    def set_fpu_reg(self, reg: int, value: int) -> None:
        """Set FPU register (S0-S31)."""
        if not 0 <= reg < 32:
            raise ValueError(f"Invalid FPU register: {reg}")
        self._lib.armv8m_emu_set_fpu_reg(self._emu_ptr, reg, value & 0xFFFFFFFF)

    @property
    def fpscr(self) -> int:
        """FPU status/control register."""
        return self._lib.armv8m_emu_get_fpscr(self._emu_ptr)

    @fpscr.setter
    def fpscr(self, value: int) -> None:
        self._lib.armv8m_emu_set_fpscr(self._emu_ptr, value & 0xFFFFFFFF)

    # =========================================================================
    # Peripherals
    # =========================================================================

    def add_peripheral(self, peripheral: "PeripheralBase", base: int, size: int) -> None:
        """
        Add a peripheral to the emulator.

        Args:
            peripheral: Peripheral instance (Python, C, or plugin)
            base: Base address for MMIO region
            size: Size of MMIO region in bytes
        """
        # Get the C peripheral structure from the peripheral
        c_periph = peripheral.c_struct

        # Register with the emulator
        result = self._lib.armv8m_emu_add_peripheral(
            self._emu_ptr, c_periph, base, size
        )

        if result != cffi.ARMV8M_OK:
            raise EmulatorError(f"Failed to add peripheral (code {result})")

        # Keep reference to prevent GC
        self._peripherals.append(peripheral)

    @property
    def peripherals(self) -> list["PeripheralBase"]:
        """List of attached peripherals."""
        return list(self._peripherals)

    # =========================================================================
    # Utilities
    # =========================================================================

    def dump_regs(self) -> dict[str, int]:
        regs = {}
        for i, name in enumerate(self._REG_NAMES):
            regs[name] = self.get_reg(i)
        regs["xpsr"] = self.xpsr
        return regs

    @property
    def elf_info(self) -> "ElfInfo | None":
        """Get ELF info if firmware was loaded from ELF."""
        return self._elf_info

    @property
    def config(self) -> ARMv8MConfig:
        """Get current configuration."""
        return self._config
