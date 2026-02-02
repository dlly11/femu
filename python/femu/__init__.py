"""
FEMU - Fast EMUlator.

A lightweight, extensible CPU emulator designed for AI-assisted development.
Currently supports ARMv8-M (Cortex-M33 class).

This package provides:
- CLI interface for the emulator
- Build and packaging tools
- Development utilities (validation, testing, AI session helpers)
- Machine configuration (YAML-based machine definitions)
- Peripheral management (Python, C, and plugin peripherals)
- GDB server (planned)
"""

from pathlib import Path

__version__ = "0.1.0"

# Re-export emulator classes and types for convenience
from .emulator import (
    # Factory and types
    create_emulator,
    get_supported_architectures,
    ArchType,
    BaseEmulator,
    BaseEmulatorConfig,
    # Errors
    EmulatorError,
    EmulatorState,
    ExecutionError,
    MemoryFaultError,
    # ARMv8-M specific
    ARMv8MConfig,
    ARMv8MEmulator,
)

# Peripheral system
from .peripheral import (
    PeripheralBase,
    Peripheral,
    CPeripheral,
    PluginPeripheral,
)
from .peripheral_registry import (
    PeripheralRegistry,
    PeripheralTypeInfo,
)

# Machine configuration
from .machine import (
    Machine,
    MachineDef,
    MemoryRegion,
    PeripheralDef,
    CPUConfig,
)

# Project root directory
PROJECT_ROOT = Path(__file__).parent.parent.parent.resolve()

# Key directories
INCLUDE_DIR = PROJECT_ROOT / "include"
SRC_DIR = PROJECT_ROOT / "src"
BUILD_DIR = PROJECT_ROOT / "build"
LIB_DIR = PROJECT_ROOT / "lib"
CPPUTEST_DIR = LIB_DIR / "cpputest"


def get_version() -> str:
    """Return the package version."""
    return __version__


def get_project_root() -> Path:
    """Return the project root directory."""
    return PROJECT_ROOT


__all__ = [
    # Version info
    "__version__",
    "get_version",
    "get_project_root",
    # Paths
    "PROJECT_ROOT",
    "INCLUDE_DIR",
    "SRC_DIR",
    "BUILD_DIR",
    "LIB_DIR",
    "CPPUTEST_DIR",
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
    # Peripheral system
    "PeripheralBase",
    "Peripheral",
    "CPeripheral",
    "PluginPeripheral",
    "PeripheralRegistry",
    "PeripheralTypeInfo",
    # Machine configuration
    "Machine",
    "MachineDef",
    "MemoryRegion",
    "PeripheralDef",
    "CPUConfig",
]
