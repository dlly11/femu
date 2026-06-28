"""
Test runner for FEMU.

Runs C tests (CppUTest) and Python tests (pytest).
"""

import os
import subprocess
import sys

from rich.console import Console

from .. import BUILD_DIR, PROJECT_ROOT
from ..build import compile_project, configure, run_ctest

console = Console()


def run_c_tests(verbose: bool = False, test_filter: str | None = None) -> bool:
    """
    Run C tests using CTest.

    Returns True if all tests passed.
    """
    console.print()
    console.rule("[bold blue]Running C Tests (CppUTest)[/bold blue]")
    console.print()

    # Ensure project is built
    if not BUILD_DIR.exists():
        console.print("[yellow]Build directory not found. Building project...[/yellow]")
        configure(build_type="Debug")
        compile_project()

    success = run_ctest(verbose=verbose, test_filter=test_filter)

    if success:
        console.print("\n[green]✓ C tests passed![/green]")
    else:
        console.print("\n[red]✗ C tests failed![/red]")

    return success


def run_python_tests(verbose: bool = False, expression: str | None = None) -> bool:
    """
    Run Python tests using pytest.

    Returns True if all tests passed.
    """
    console.print()
    console.rule("[bold blue]Running Python Tests (pytest)[/bold blue]")
    console.print()

    # Build pytest command
    cmd = [sys.executable, "-m", "pytest", str(PROJECT_ROOT / "python" / "tests")]

    if verbose:
        cmd.append("-v")

    if expression:
        cmd.extend(["-k", expression])

    # Add PYTHONPATH
    env = {"PYTHONPATH": str(PROJECT_ROOT / "python")}

    console.print(f"[dim]$ {' '.join(cmd)}[/dim]\n")

    try:
        result = subprocess.run(
            cmd,
            cwd=PROJECT_ROOT,
            env={**os.environ, **env},
        )
        success = result.returncode == 0
    except (OSError, subprocess.SubprocessError) as e:
        console.print(f"[red]Error running pytest: {e}[/red]")
        success = False

    if success:
        console.print("\n[green]✓ Python tests passed![/green]")
    else:
        console.print("\n[red]✗ Python tests failed![/red]")

    return success


def run_all_tests(verbose: bool = False) -> bool:
    """
    Run all tests (C and Python).

    Returns True if all tests passed.
    """
    console.print()
    console.rule("[bold]Running All Tests[/bold]")

    c_passed = run_c_tests(verbose=verbose)
    python_passed = run_python_tests(verbose=verbose)

    console.print()
    console.rule("[bold]Test Summary[/bold]")
    console.print()

    c_status = "[green]✓ PASS[/green]" if c_passed else "[red]✗ FAIL[/red]"
    py_status = "[green]✓ PASS[/green]" if python_passed else "[red]✗ FAIL[/red]"

    console.print(f"  C tests:      {c_status}")
    console.print(f"  Python tests: {py_status}")
    console.print()

    return c_passed and python_passed
