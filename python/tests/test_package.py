"""
Basic package tests for FEMU.
"""

import pytest
from pathlib import Path


def test_package_import() -> None:
    """Test that the package can be imported."""
    import femu

    assert hasattr(femu, "__version__")
    assert hasattr(femu, "PROJECT_ROOT")


def test_version_format() -> None:
    """Test that version follows semver format."""
    import femu

    parts = femu.__version__.split(".")
    assert len(parts) >= 2
    assert all(part.isdigit() for part in parts[:2])


def test_project_root_exists() -> None:
    """Test that PROJECT_ROOT points to a valid directory."""
    import femu

    assert femu.PROJECT_ROOT.exists()
    assert femu.PROJECT_ROOT.is_dir()
    assert (femu.PROJECT_ROOT / "include").exists()


def test_cli_import() -> None:
    """Test that the CLI module can be imported."""
    from femu import cli

    assert hasattr(cli, "main")


def test_dev_modules_import() -> None:
    """Test that dev modules can be imported."""
    from femu.dev import session, validate, test

    assert hasattr(session, "MODULES")
    assert hasattr(session, "show_context")
    assert hasattr(validate, "validate_module")
    assert hasattr(test, "run_all_tests")


def test_modules_definition() -> None:
    """Test that MODULES contains expected modules."""
    from femu.dev.session import MODULES

    expected_modules = ["decoder", "executor", "memory", "nvic", "mpu"]
    for module in expected_modules:
        assert module in MODULES
        assert "description" in MODULES[module]
        assert "header" in MODULES[module]
        assert "impl_files" in MODULES[module]
