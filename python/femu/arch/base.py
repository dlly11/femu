"""Abstract base class for architecture-specific emulators.

This module defines the common interface that all architecture implementations
must provide.
"""

from __future__ import annotations

from abc import ABC, abstractmethod
from dataclasses import dataclass
from enum import IntEnum
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from pathlib import Path

    from ..elf_loader import ElfInfo
    from ..peripheral import PeripheralBase


class ArchType(IntEnum):
    """Supported architecture types."""

    UNKNOWN = 0
    ARMV8M = 1
    ARMV7M = 2
    RISCV32 = 3
    RISCV64 = 4


class EmulatorState(IntEnum):
    """Emulator execution state."""

    STOPPED = 0
    RUNNING = 1
    HALTED = 2
    BREAKPOINT = 3
    WATCHPOINT = 4
    FAULT = 5


class EmulatorError(Exception):
    """Base class for emulator errors."""


class MemoryFaultError(EmulatorError):
    """Memory access fault."""

    def __init__(self, addr: int, message: str = "Memory fault") -> None:
        """Record the faulting address and build the error message."""
        self.addr = addr
        super().__init__(f"{message} at 0x{addr:08x}")


class ExecutionError(EmulatorError):
    """Error during execution."""

    def __init__(self, code: int, message: str = "Execution error") -> None:
        """Record the error code and build the error message."""
        self.code = code
        super().__init__(f"{message} (code {code})")


@dataclass
class BaseEmulatorConfig:
    """Base configuration shared by all architectures."""

    flash_base: int = 0
    flash_size: int = 0
    ram_base: int = 0
    ram_size: int = 0
    num_irqs: int = 32


class BaseEmulator(ABC):
    """Abstract base class for all architecture emulators.

    Architecture-specific implementations inherit from this class and
    implement the abstract methods.
    """

    @property
    @abstractmethod
    def arch(self) -> ArchType:
        """Get architecture type."""

    @property
    @abstractmethod
    def arch_name(self) -> str:
        """Get architecture name string."""

    # =========================================================================
    # Memory Setup
    # =========================================================================

    @abstractmethod
    def add_flash(self, base: int, size: int) -> None:
        """Add flash memory region."""

    @abstractmethod
    def add_ram(self, base: int, size: int) -> None:
        """Add RAM region."""

    @abstractmethod
    def load(self, addr: int, data: bytes) -> None:
        """Load binary data into memory."""

    def load_elf(self, path: str | Path) -> ElfInfo:
        """Load an ELF file into the emulator.

        Default implementation uses elf_loader module. Architecture implementations
        may override for custom loading behavior.
        """
        from ..elf_loader import load_elf, suggest_memory_config

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

        return elf

    # =========================================================================
    # Execution
    # =========================================================================

    @abstractmethod
    def step(self) -> int:
        """Execute a single instruction."""

    @abstractmethod
    def run(self, max_cycles: int = 0) -> int:
        """Run until stopped, breakpoint, or max cycles reached."""

    @abstractmethod
    def stop(self) -> None:
        """Request emulator to stop (thread-safe)."""

    @abstractmethod
    def reset(self) -> None:
        """Reset emulator to initial state."""

    # =========================================================================
    # State Access
    # =========================================================================

    @abstractmethod
    def get_reg(self, reg: int) -> int:
        """Get general purpose register value."""

    @abstractmethod
    def set_reg(self, reg: int, value: int) -> None:
        """Set general purpose register value."""

    @property
    @abstractmethod
    def pc(self) -> int:
        """Program counter."""

    @pc.setter
    @abstractmethod
    def pc(self, value: int) -> None:
        pass

    @property
    @abstractmethod
    def status(self) -> int:
        """Status/flags register (architecture-specific format)."""

    @status.setter
    @abstractmethod
    def status(self, value: int) -> None:
        pass

    @property
    @abstractmethod
    def cycles(self) -> int:
        """Total cycles executed."""

    @property
    @abstractmethod
    def state(self) -> EmulatorState:
        """Current emulator state."""

    @property
    @abstractmethod
    def last_error(self) -> int:
        """Last error code."""

    # =========================================================================
    # Memory Access
    # =========================================================================

    @abstractmethod
    def read_mem(self, addr: int, size: int = 4) -> int:
        """Read from memory."""

    @abstractmethod
    def write_mem(self, addr: int, value: int, size: int = 4) -> None:
        """Write to memory."""

    @abstractmethod
    def read_bytes(self, addr: int, length: int) -> bytes:
        """Read a block of bytes from memory."""

    @abstractmethod
    def write_bytes(self, addr: int, data: bytes) -> int:
        """Write bytes to memory."""

    # =========================================================================
    # Peripherals
    # =========================================================================

    @abstractmethod
    def add_peripheral(self, peripheral: PeripheralBase, base: int, size: int) -> None:
        """Add a peripheral to the emulator.

        Args:
            peripheral: Peripheral instance to add
            base: Base address for MMIO region
            size: Size of MMIO region in bytes
        """

    # =========================================================================
    # Common Register Aliases
    # =========================================================================

    @property
    @abstractmethod
    def sp(self) -> int:
        """Stack pointer."""

    @sp.setter
    @abstractmethod
    def sp(self, value: int) -> None:
        pass

    @property
    @abstractmethod
    def lr(self) -> int:
        """Link register (return address)."""

    @lr.setter
    @abstractmethod
    def lr(self, value: int) -> None:
        pass

    # =========================================================================
    # Breakpoints
    # =========================================================================

    @abstractmethod
    def add_breakpoint(self, addr: int) -> None:
        """Add a breakpoint at address."""

    @abstractmethod
    def remove_breakpoint(self, addr: int) -> None:
        """Remove breakpoint at address."""

    @abstractmethod
    def has_breakpoint(self, addr: int) -> bool:
        """Check if address has a breakpoint."""

    @abstractmethod
    def clear_breakpoints(self) -> None:
        """Remove all breakpoints."""

    # =========================================================================
    # Watchpoints
    # =========================================================================

    @abstractmethod
    def add_watchpoint(self, addr: int, size: int, wp_type: int) -> None:
        """Add a watchpoint at address.

        Args:
            addr: Memory address to watch
            size: Size of memory region in bytes
            wp_type: Watchpoint type (2=write, 3=read, 4=access)
        """

    @abstractmethod
    def remove_watchpoint(self, addr: int, size: int, wp_type: int) -> None:
        """Remove a watchpoint."""

    @abstractmethod
    def clear_watchpoints(self) -> None:
        """Remove all watchpoints."""

    @property
    @abstractmethod
    def watchpoint_hit_addr(self) -> int:
        """Address that triggered the last watchpoint hit."""

    @property
    @abstractmethod
    def watchpoint_hit_type(self) -> int:
        """Type of the last watchpoint hit."""

    # =========================================================================
    # Special/Architecture-Specific Registers
    # =========================================================================

    @abstractmethod
    def get_special_reg(self, reg_id: int) -> int:
        """Get architecture-specific special register."""

    @abstractmethod
    def set_special_reg(self, reg_id: int, value: int) -> None:
        """Set architecture-specific special register."""

    # =========================================================================
    # FPU Registers (Optional - default raises NotImplementedError)
    # =========================================================================

    def get_fpu_reg(self, reg: int) -> int:
        """Get FPU register value.

        Override in architectures with FPU support.
        """
        raise NotImplementedError(f"{self.arch_name} does not support FPU registers")

    def set_fpu_reg(self, reg: int, value: int) -> None:
        """Set FPU register value.

        Override in architectures with FPU support.
        """
        raise NotImplementedError(f"{self.arch_name} does not support FPU registers")

    @property
    def fpscr(self) -> int:
        """FPU status/control register.

        Override in architectures with FPU support.
        """
        raise NotImplementedError(f"{self.arch_name} does not support FPU registers")

    @fpscr.setter
    def fpscr(self, value: int) -> None:
        raise NotImplementedError(f"{self.arch_name} does not support FPU registers")

    @property
    def has_fpu(self) -> bool:
        """Whether this emulator instance has FPU support."""
        return False

    # =========================================================================
    # Utilities
    # =========================================================================

    @abstractmethod
    def dump_regs(self) -> dict[str, int]:
        """Dump all general purpose registers."""
