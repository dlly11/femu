"""
Pytest configuration and fixtures for ARMv8-M emulator tests.
"""

from pathlib import Path

import pytest


@pytest.fixture
def project_root() -> Path:
    """Return the project root directory."""
    return Path(__file__).parent.parent.parent


@pytest.fixture
def include_dir(project_root: Path) -> Path:
    """Return the include directory."""
    return project_root / "include"


@pytest.fixture
def test_firmware_dir(project_root: Path) -> Path:
    """Return the test firmware directory."""
    return project_root / "tests" / "firmware"
