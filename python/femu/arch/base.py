"""
Abstract base class for architecture-specific emulators.

This module defines the common interface that all architecture implementations
must provide.
"""

from __future__ import annotations

from abc import ABC, abstractmethod
from dataclasses import dataclass
from enum import IntEnum
from pathlib import Path
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from ..elf_loader import ElfInfo


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
    FAULT = 4


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
class BaseEmulatorConfig:
    """Base configuration shared by all architectures."""

    flash_base: int = 0
    flash_size: int = 0
    ram_base: int = 0
    ram_size: int = 0
    num_irqs: int = 32


class BaseEmulator(ABC):
    """
    Abstract base class for all architecture emulators.

    Architecture-specific implementations inherit from this class and
    implement the abstract methods.
    """

    @property
    @abstractmethod
    def arch(self) -> ArchType:
        """Get architecture type."""
        pass

    @property
    @abstractmethod
    def arch_name(self) -> str:
        """Get architecture name string."""
        pass

    # =========================================================================
    # Memory Setup
    # =========================================================================

    @abstractmethod
    def add_flash(self, base: int, size: int) -> None:
        """Add flash memory region."""
        pass

    @abstractmethod
    def add_ram(self, base: int, size: int) -> None:
        """Add RAM region."""
        pass

    @abstractmethod
    def load(self, addr: int, data: bytes) -> None:
        """Load binary data into memory."""
        pass

    def load_elf(self, path: str | Path) -> "ElfInfo":
        """
        Load an ELF file into the emulator.

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
        pass

    @abstractmethod
    def run(self, max_cycles: int = 0) -> int:
        """Run until stopped, breakpoint, or max cycles reached."""
        pass

    @abstractmethod
    def stop(self) -> None:
        """Request emulator to stop (thread-safe)."""
        pass

    @abstractmethod
    def reset(self) -> None:
        """Reset emulator to initial state."""
        pass

    # =========================================================================
    # State Access
    # =========================================================================

    @abstractmethod
    def get_reg(self, reg: int) -> int:
        """Get general purpose register value."""
        pass

    @abstractmethod
    def set_reg(self, reg: int, value: int) -> None:
        """Set general purpose register value."""
        pass

    @property
    @abstractmethod
    def pc(self) -> int:
        """Program counter."""
        pass

    @pc.setter
    @abstractmethod
    def pc(self, value: int) -> None:
        pass

    @property
    @abstractmethod
    def status(self) -> int:
        """Status/flags register (architecture-specific format)."""
        pass

    @status.setter
    @abstractmethod
    def status(self, value: int) -> None:
        pass

    @property
    @abstractmethod
    def cycles(self) -> int:
        """Total cycles executed."""
        pass

    @property
    @abstractmethod
    def state(self) -> EmulatorState:
        """Current emulator state."""
        pass

    @property
    @abstractmethod
    def last_error(self) -> int:
        """Last error code."""
        pass

    # =========================================================================
    # Memory Access
    # =========================================================================

    @abstractmethod
    def read_mem(self, addr: int, size: int = 4) -> int:
        """Read from memory."""
        pass

    @abstractmethod
    def write_mem(self, addr: int, value: int, size: int = 4) -> None:
        """Write to memory."""
        pass

    @abstractmethod
    def read_bytes(self, addr: int, length: int) -> bytes:
        """Read a block of bytes from memory."""
        pass

    @abstractmethod
    def write_bytes(self, addr: int, data: bytes) -> int:
        """Write bytes to memory."""
        pass

    # =========================================================================
    # Breakpoints
    # =========================================================================

    @abstractmethod
    def add_breakpoint(self, addr: int) -> None:
        """Add a breakpoint at address."""
        pass

    @abstractmethod
    def remove_breakpoint(self, addr: int) -> None:
        """Remove breakpoint at address."""
        pass

    @abstractmethod
    def has_breakpoint(self, addr: int) -> bool:
        """Check if address has a breakpoint."""
        pass

    @abstractmethod
    def clear_breakpoints(self) -> None:
        """Remove all breakpoints."""
        pass

    # =========================================================================
    # Special/Architecture-Specific Registers
    # =========================================================================

    @abstractmethod
    def get_special_reg(self, reg_id: int) -> int:
        """Get architecture-specific special register."""
        pass

    @abstractmethod
    def set_special_reg(self, reg_id: int, value: int) -> None:
        """Set architecture-specific special register."""
        pass

    # =========================================================================
    # Utilities
    # =========================================================================

    @abstractmethod
    def dump_regs(self) -> dict[str, int]:
        """Dump all general purpose registers."""
        pass
