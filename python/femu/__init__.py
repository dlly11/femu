"""
FEMU - Fast EMUlator.

A lightweight, extensible CPU emulator designed for AI-assisted development.
Currently supports ARMv8-M (Cortex-M33 class).

This package provides:
- CLI interface for the emulator
- Build and packaging tools
- Development utilities (validation, testing, AI session helpers)
- GDB server (planned)
- Machine configuration (planned)
- Peripheral management (planned)
"""

from pathlib import Path

__version__ = "0.1.0"

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
