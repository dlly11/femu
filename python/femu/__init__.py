"""FEMU - Fast EMUlator.

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
    ArchType,
    # ARMv8-M specific
    ARMv8MConfig,
    ARMv8MEmulator,
    BaseEmulator,
    BaseEmulatorConfig,
    # Errors
    EmulatorError,
    EmulatorState,
    ExecutionError,
    MemoryFaultError,
    # Factory and types
    create_emulator,
    get_supported_architectures,
)

# Logging system
from .logging import (
    TRACE,
    LogCategory,
    LogLevel,
    configure_logging,
    disable_logging,
    enable_logging,
    get_logger,
)

# Machine configuration
from .machine import (
    CPUConfig,
    Machine,
    MachineDef,
    MemoryRegion,
    PeripheralDef,
)

# Peripheral system
from .peripheral import (
    CPeripheral,
    Peripheral,
    PeripheralBase,
    PluginPeripheral,
)
from .peripheral_registry import (
    PeripheralRegistry,
    PeripheralTypeInfo,
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
    "BUILD_DIR",
    "CPPUTEST_DIR",
    "INCLUDE_DIR",
    "LIB_DIR",
    # Paths
    "PROJECT_ROOT",
    "SRC_DIR",
    "TRACE",
    # ARMv8-M specific
    "ARMv8MConfig",
    "ARMv8MEmulator",
    "ArchType",
    "BaseEmulator",
    "BaseEmulatorConfig",
    "CPUConfig",
    "CPeripheral",
    # Errors
    "EmulatorError",
    "EmulatorState",
    "ExecutionError",
    "LogCategory",
    "LogLevel",
    # Machine configuration
    "Machine",
    "MachineDef",
    "MemoryFaultError",
    "MemoryRegion",
    "Peripheral",
    # Peripheral system
    "PeripheralBase",
    "PeripheralDef",
    "PeripheralRegistry",
    "PeripheralTypeInfo",
    "PluginPeripheral",
    # Version info
    "__version__",
    # Logging system
    "configure_logging",
    # Factory and types
    "create_emulator",
    "disable_logging",
    "enable_logging",
    "get_logger",
    "get_project_root",
    "get_supported_architectures",
    "get_version",
]
