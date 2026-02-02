"""
Architecture-specific emulator implementations.

This package provides architecture-specific emulator classes that implement
the common BaseEmulator interface.

Supported architectures:
- ARMv8-M: Cortex-M33 and similar (armv8m)

Usage:
    from femu.arch import ARMv8MEmulator
    emu = ARMv8MEmulator()
"""

from .armv8m import ARMv8MEmulator
from .base import ArchType, BaseEmulator, EmulatorError, EmulatorState

__all__ = [
    "BaseEmulator",
    "EmulatorState",
    "EmulatorError",
    "ArchType",
    "ARMv8MEmulator",
]
