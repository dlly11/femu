"""
High-level Python emulator API.

This module provides a Pythonic interface to the ARMv8-M emulator.
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum
from pathlib import Path
from typing import TYPE_CHECKING

from . import _emulator_cffi as cffi
from .elf_loader import ElfInfo, load_elf, suggest_memory_config

if TYPE_CHECKING:
    pass


class EmulatorState(IntEnum):
    """Emulator execution state."""

    STOPPED = cffi.EMU_STATE_STOPPED
    RUNNING = cffi.EMU_STATE_RUNNING
    HALTED = cffi.EMU_STATE_HALTED
    BREAKPOINT = cffi.EMU_STATE_BREAKPOINT
    FAULT = cffi.EMU_STATE_FAULT


class EmulatorError(Exception):
    """Base class for emulator errors."""

    pass


class MemoryFaultError(EmulatorError):
    """Memory access fault."""

    def __init__(self, addr: int, message: str = "Memory fault"):
        self.addr = addr
        super().__init__(f"{message} at 0x{addr:08x}")


class ExecutionError(EmulatorError):
    """Error during execution."""

    def __init__(self, code: int, message: str = "Execution error"):
        self.code = code
        super().__init__(f"{message} (code {code})")


@dataclass
class EmulatorConfig:
    """Emulator configuration."""

    has_fpu: bool = False
    has_dsp: bool = False
    has_trustzone: bool = False
    num_mpu_regions: int = 8
    num_irqs: int = 32
    flash_base: int = 0x08000000
    flash_size: int = 0x80000  # 512KB
    ram_base: int = 0x20000000
    ram_size: int = 0x20000  # 128KB


class Emulator:
    """
    High-level ARMv8-M emulator.

    Example usage:
        emu = Emulator()
        emu.load_elf("firmware.elf")
        cycles = emu.run(max_cycles=1000000)
        print(f"Executed {cycles} cycles, final PC: 0x{emu.pc:08x}")
    """

    # Register names for dump_regs()
    _REG_NAMES = [
        "r0",
        "r1",
        "r2",
        "r3",
        "r4",
        "r5",
        "r6",
        "r7",
        "r8",
        "r9",
        "r10",
        "r11",
        "r12",
        "sp",
        "lr",
        "pc",
    ]

    def __init__(self, config: EmulatorConfig | None = None):
        """
        Initialize emulator.

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

        self._config = config or EmulatorConfig()
        self._elf_info: ElfInfo | None = None

    def __del__(self):
        """Clean up emulator resources."""
        if hasattr(self, "_cleanup"):
            self._cleanup()

    # =========================================================================
    # Memory Setup
    # =========================================================================

    def add_flash(self, base: int, size: int) -> None:
        """
        Add flash memory region.

        Args:
            base: Base address (typically 0x08000000)
            size: Size in bytes
        """
        result = self._lib.armv8m_emu_add_flash(self._emu_ptr, base, size)
        if result != cffi.ARMV8M_OK:
            raise EmulatorError(f"Failed to add flash (code {result})")

    def add_ram(self, base: int, size: int) -> None:
        """
        Add RAM region.

        Args:
            base: Base address (typically 0x20000000)
            size: Size in bytes
        """
        result = self._lib.armv8m_emu_add_ram(self._emu_ptr, base, size)
        if result != cffi.ARMV8M_OK:
            raise EmulatorError(f"Failed to add RAM (code {result})")

    def load(self, addr: int, data: bytes) -> None:
        """
        Load binary data into memory.

        Args:
            addr: Destination address
            data: Binary data to load
        """
        result = self._lib.armv8m_emu_load(self._emu_ptr, addr, data, len(data))
        if result != cffi.ARMV8M_OK:
            raise EmulatorError(f"Failed to load data at 0x{addr:08x} (code {result})")

    def load_elf(self, path: str | Path) -> ElfInfo:
        """
        Load an ELF file into the emulator.

        This will:
        1. Parse the ELF file
        2. Set up memory regions based on segments
        3. Load segment data
        4. Reset the emulator

        Args:
            path: Path to ELF file

        Returns:
            Parsed ELF information
        """
        elf = load_elf(path)
        mem_config = suggest_memory_config(elf)

        # Set up memory regions
        self.add_flash(mem_config["flash_base"], mem_config["flash_size"])
        self.add_ram(mem_config["ram_base"], mem_config["ram_size"])

        # Load segments
        for seg in elf.segments:
            self.load(seg.vaddr, seg.data)

            # Zero-fill BSS (memsz > filesz)
            if seg.memsz > len(seg.data):
                bss_start = seg.vaddr + len(seg.data)
                bss_size = seg.memsz - len(seg.data)
                self.load(bss_start, bytes(bss_size))

        # Reset to apply vector table
        self.reset()

        self._elf_info = elf
        return elf

    # =========================================================================
    # Execution
    # =========================================================================

    def step(self) -> int:
        """
        Execute a single instruction.

        Returns:
            Result code (0 = OK)

        Raises:
            ExecutionError: On execution fault
        """
        result = self._lib.armv8m_emu_step(self._emu_ptr)

        if result == cffi.ARMV8M_ERR_BREAKPOINT:
            return result  # Let caller handle breakpoint

        if result < 0 and result != cffi.ARMV8M_ERR_HALTED:
            raise ExecutionError(result)

        return result

    def run(self, max_cycles: int = 0) -> int:
        """
        Run until stopped, breakpoint, or max cycles reached.

        Args:
            max_cycles: Maximum cycles to execute (0 = unlimited)

        Returns:
            Number of cycles executed

        Raises:
            ExecutionError: On execution fault
        """
        result = self._lib.armv8m_emu_run(self._emu_ptr, max_cycles)

        if result < 0:
            state = self.state
            if state == EmulatorState.FAULT:
                raise ExecutionError(int(-result), "Execution fault")
            # Breakpoint or halt - return absolute value
            return int(-result)

        return int(result)

    def stop(self) -> None:
        """Request emulator to stop (thread-safe)."""
        self._lib.armv8m_emu_stop(self._emu_ptr)

    def reset(self) -> None:
        """Reset emulator to initial state."""
        self._lib.armv8m_emu_reset(self._emu_ptr)

    # =========================================================================
    # State Access
    # =========================================================================

    def get_reg(self, reg: int) -> int:
        """Get general purpose register value (R0-R15)."""
        if not 0 <= reg < 16:
            raise ValueError(f"Invalid register number: {reg}")
        return self._lib.armv8m_emu_get_reg(self._emu_ptr, reg)

    def set_reg(self, reg: int, value: int) -> None:
        """Set general purpose register value."""
        if not 0 <= reg < 16:
            raise ValueError(f"Invalid register number: {reg}")
        self._lib.armv8m_emu_set_reg(self._emu_ptr, reg, value & 0xFFFFFFFF)

    @property
    def pc(self) -> int:
        """Program counter."""
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
        """Program status register."""
        return self._lib.armv8m_emu_get_xpsr(self._emu_ptr)

    @xpsr.setter
    def xpsr(self, value: int) -> None:
        self._lib.armv8m_emu_set_xpsr(self._emu_ptr, value & 0xFFFFFFFF)

    @property
    def cycles(self) -> int:
        """Total cycles executed."""
        return self._lib.armv8m_emu_get_cycles(self._emu_ptr)

    @property
    def state(self) -> EmulatorState:
        """Current emulator state."""
        return EmulatorState(self._lib.armv8m_emu_get_state(self._emu_ptr))

    @property
    def last_error(self) -> int:
        """Last error code."""
        return self._lib.armv8m_emu_get_last_error(self._emu_ptr)

    # =========================================================================
    # Memory Access
    # =========================================================================

    def read_mem(self, addr: int, size: int = 4) -> int:
        """
        Read from memory.

        Args:
            addr: Memory address
            size: Access size (1, 2, or 4 bytes)

        Returns:
            Value read

        Raises:
            MemoryFaultError: On access fault
        """
        if size not in (1, 2, 4):
            raise ValueError(f"Invalid access size: {size}")

        fault = self._ffi.new("bool *")
        value = self._lib.armv8m_emu_read_mem(self._emu_ptr, addr, size, fault)

        if fault[0]:
            raise MemoryFaultError(addr, "Memory read fault")

        return value

    def write_mem(self, addr: int, value: int, size: int = 4) -> None:
        """
        Write to memory.

        Args:
            addr: Memory address
            value: Value to write
            size: Access size (1, 2, or 4 bytes)

        Raises:
            MemoryFaultError: On access fault
        """
        if size not in (1, 2, 4):
            raise ValueError(f"Invalid access size: {size}")

        fault = self._ffi.new("bool *")
        self._lib.armv8m_emu_write_mem(self._emu_ptr, addr, value & 0xFFFFFFFF, size, fault)

        if fault[0]:
            raise MemoryFaultError(addr, "Memory write fault")

    def read_bytes(self, addr: int, length: int) -> bytes:
        """
        Read a block of bytes from memory.

        Args:
            addr: Start address
            length: Number of bytes to read

        Returns:
            Bytes read
        """
        buf = self._ffi.new(f"uint8_t[{length}]")
        read = self._lib.armv8m_emu_read_block(self._emu_ptr, addr, buf, length)
        return bytes(buf[0:read])

    def write_bytes(self, addr: int, data: bytes) -> int:
        """
        Write bytes to memory.

        Args:
            addr: Start address
            data: Bytes to write

        Returns:
            Number of bytes written
        """
        return self._lib.armv8m_emu_write_block(self._emu_ptr, addr, data, len(data))

    # =========================================================================
    # Breakpoints
    # =========================================================================

    def add_breakpoint(self, addr: int) -> None:
        """Add a breakpoint at address."""
        result = self._lib.armv8m_emu_add_breakpoint(self._emu_ptr, addr)
        if result != cffi.ARMV8M_OK:
            raise EmulatorError(f"Failed to add breakpoint (code {result})")

    def remove_breakpoint(self, addr: int) -> None:
        """Remove breakpoint at address."""
        self._lib.armv8m_emu_remove_breakpoint(self._emu_ptr, addr)

    def has_breakpoint(self, addr: int) -> bool:
        """Check if address has a breakpoint."""
        return self._lib.armv8m_emu_has_breakpoint(self._emu_ptr, addr)

    def clear_breakpoints(self) -> None:
        """Remove all breakpoints."""
        self._lib.armv8m_emu_clear_breakpoints(self._emu_ptr)

    # =========================================================================
    # Special Registers (for debugging/GDB)
    # =========================================================================

    def get_special_reg(self, reg_id: int) -> int:
        """Get special register value."""
        return self._lib.armv8m_emu_get_special_reg(self._emu_ptr, reg_id)

    def set_special_reg(self, reg_id: int, value: int) -> None:
        """Set special register value."""
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
    # Utilities
    # =========================================================================

    def dump_regs(self) -> dict[str, int]:
        """
        Dump all general purpose registers.

        Returns:
            Dictionary mapping register names to values
        """
        regs = {}
        for i, name in enumerate(self._REG_NAMES):
            regs[name] = self.get_reg(i)
        regs["xpsr"] = self.xpsr
        return regs

    @property
    def elf_info(self) -> ElfInfo | None:
        """Get ELF info if firmware was loaded from ELF."""
        return self._elf_info
