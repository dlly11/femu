"""
Pytest configuration and fixtures for ARMv8-M emulator tests.
"""

import os
import subprocess
import sys
from pathlib import Path

import pytest


@pytest.fixture(scope="session")
def emulator_lib_available() -> bool:
    """Whether the compiled emulator library can be loaded in this environment.

    A sanitizer-enabled build aborts the process (rather than raising) if the
    ASan runtime is not preloaded, which an ``except OSError`` cannot catch. We
    therefore probe loadability in a throwaway subprocess so dependent tests can
    skip cleanly instead of crashing the whole run. CI preloads ASan via
    scripts/ci/test.py, so the probe succeeds there.
    """
    python_dir = Path(__file__).parent.parent
    env = {**os.environ, "PYTHONPATH": str(python_dir)}
    result = subprocess.run(
        [sys.executable, "-c", "from femu._emulator_cffi import get_lib; get_lib()"],
        env=env,
        capture_output=True,
    )
    return result.returncode == 0


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
