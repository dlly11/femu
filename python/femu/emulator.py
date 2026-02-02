"""
High-level Python emulator API.

This module provides:
1. Factory function to create architecture-specific emulators
2. Re-exports of architecture-specific classes for convenience

Usage:
    emu = create_emulator(ArchType.ARMV8M)
    emu.load_elf("firmware.elf")
    emu.run(max_cycles=1000000)
"""

from __future__ import annotations

from typing import TYPE_CHECKING

from .arch.armv8m import ARMv8MConfig, ARMv8MEmulator

# Re-export from arch module for convenience
from .arch.base import (
    ArchType,
    BaseEmulator,
    BaseEmulatorConfig,
    EmulatorError,
    EmulatorState,
    ExecutionError,
    MemoryFaultError,
)

if TYPE_CHECKING:
    pass

# Type alias for config types
ConfigType = ARMv8MConfig | BaseEmulatorConfig | None


def create_emulator(
    arch: ArchType = ArchType.ARMV8M,
    config: ConfigType = None,
) -> BaseEmulator:
    """
    Factory function to create an emulator for the specified architecture.

    Args:
        arch: Architecture type (default: ARMv8-M)
        config: Architecture-specific configuration

    Returns:
        Architecture-specific emulator instance

    Raises:
        ValueError: If architecture is not supported

    Example:
        emu = create_emulator(ArchType.ARMV8M)
        emu.load_elf("firmware.elf")
        emu.run(max_cycles=1000000)
    """
    if arch == ArchType.ARMV8M:
        arm_config = config if isinstance(config, ARMv8MConfig) else None
        return ARMv8MEmulator(arm_config)
    elif arch == ArchType.ARMV7M:
        # ARMv7-M can use ARMv8-M emulator with limited features
        arm_config = config if isinstance(config, ARMv8MConfig) else ARMv8MConfig()
        arm_config.has_trustzone = False  # ARMv7-M doesn't have TrustZone
        return ARMv8MEmulator(arm_config)
    else:
        raise ValueError(f"Unsupported architecture: {arch}")


def get_supported_architectures() -> list[ArchType]:
    """
    Get list of supported architecture types.

    Returns:
        List of ArchType values for available emulators
    """
    return [ArchType.ARMV8M, ArchType.ARMV7M]


__all__ = [
    # Factory and types
    "create_emulator",
    "get_supported_architectures",
    "ArchType",
    "BaseEmulator",
    "BaseEmulatorConfig",
    # Errors
    "EmulatorError",
    "EmulatorState",
    "ExecutionError",
    "MemoryFaultError",
    # ARMv8-M specific
    "ARMv8MConfig",
    "ARMv8MEmulator",
]
