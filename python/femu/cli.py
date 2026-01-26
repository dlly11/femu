"""
FEMU CLI.

Main entry point for the emulator and development tools.
"""

import click
from rich.console import Console

from . import __version__

console = Console()


@click.group(invoke_without_command=True)
@click.option("--version", is_flag=True, help="Show version and exit.")
@click.pass_context
def main(ctx: click.Context, version: bool) -> None:
    """FEMU - Fast EMUlator. A lightweight, extensible CPU emulator."""
    if version:
        console.print(f"femu {__version__}")
        return

    if ctx.invoked_subcommand is None:
        console.print(ctx.get_help())


# =============================================================================
# Build Commands
# =============================================================================


@main.group()
def build() -> None:
    """Build commands for the emulator."""
    pass


@build.command("configure")
@click.option("--build-type", type=click.Choice(["Debug", "Release"]), default="Debug")
@click.option("--clean", is_flag=True, help="Clean build directory first.")
@click.option("--compiler", type=click.Choice(["gcc", "clang"]), default=None, help="Compiler to use.")
@click.option("--no-sanitizers", is_flag=True, help="Disable sanitizers in Debug builds.")
def build_configure(build_type: str, clean: bool, compiler: str | None, no_sanitizers: bool) -> None:
    """Configure the CMake build."""
    from .build import configure

    configure(build_type=build_type, clean=clean, compiler=compiler, sanitizers=not no_sanitizers)


@build.command("compile")
@click.option("-j", "--jobs", type=int, default=None, help="Number of parallel jobs.")
@click.option("--target", type=str, default=None, help="Specific target to build.")
def build_compile(jobs: int | None, target: str | None) -> None:
    """Compile the project."""
    from .build import compile_project

    compile_project(jobs=jobs, target=target)


@build.command("all")
@click.option("--build-type", type=click.Choice(["Debug", "Release"]), default="Debug")
@click.option("-j", "--jobs", type=int, default=None, help="Number of parallel jobs.")
@click.option("--compiler", type=click.Choice(["gcc", "clang"]), default=None, help="Compiler to use.")
@click.option("--no-sanitizers", is_flag=True, help="Disable sanitizers in Debug builds.")
def build_all(build_type: str, jobs: int | None, compiler: str | None, no_sanitizers: bool) -> None:
    """Configure and compile the project."""
    from .build import compile_project, configure

    configure(build_type=build_type, compiler=compiler, sanitizers=not no_sanitizers)
    compile_project(jobs=jobs)


@build.command("clean")
def build_clean() -> None:
    """Clean the build directory."""
    from .build import clean

    clean()


@build.command("analyze")
@click.option("--tool", type=click.Choice(["cppcheck", "clang-tidy"]), default=None,
              help="Specific tool to run (default: all).")
def build_analyze(tool: str | None) -> None:
    """Run static analysis tools."""
    from .build import run_analysis

    success = run_analysis(tool=tool)
    if not success:
        raise SystemExit(1)


# =============================================================================
# Test Commands
# =============================================================================


@main.group()
def test() -> None:
    """Test commands."""
    pass


@test.command("c")
@click.option("-v", "--verbose", is_flag=True, help="Verbose output.")
@click.option("--filter", "test_filter", type=str, help="Filter tests by name.")
def test_c(verbose: bool, test_filter: str | None) -> None:
    """Run C tests (CppUTest)."""
    from .dev.test import run_c_tests

    run_c_tests(verbose=verbose, test_filter=test_filter)


@test.command("python")
@click.option("-v", "--verbose", is_flag=True, help="Verbose output.")
@click.option("-k", "expression", type=str, help="Filter tests by expression.")
def test_python(verbose: bool, expression: str | None) -> None:
    """Run Python tests (pytest)."""
    from .dev.test import run_python_tests

    run_python_tests(verbose=verbose, expression=expression)


@test.command("all")
@click.option("-v", "--verbose", is_flag=True, help="Verbose output.")
def test_all(verbose: bool) -> None:
    """Run all tests (C and Python)."""
    from .dev.test import run_all_tests

    run_all_tests(verbose=verbose)


# =============================================================================
# Dev Commands
# =============================================================================


@main.group()
def dev() -> None:
    """Development tools."""
    pass


@dev.command("validate")
@click.argument("module", type=str)
def dev_validate(module: str) -> None:
    """Validate a module implementation."""
    from .dev.validate import validate_module

    validate_module(module)


@dev.command("validate-all")
def dev_validate_all() -> None:
    """Validate all implemented modules."""
    from .dev.validate import validate_all_modules

    validate_all_modules()


@dev.command("context")
@click.argument("module", type=str)
def dev_context(module: str) -> None:
    """Show AI context files for a module."""
    from .dev.session import show_context

    show_context(module)


@dev.command("status")
def dev_status() -> None:
    """Show status of all modules."""
    from .dev.session import show_status

    show_status()


@dev.command("list")
def dev_list() -> None:
    """List all available modules."""
    from .dev.session import list_modules

    list_modules()


# =============================================================================
# Run Command (future)
# =============================================================================


@main.command("run")
@click.argument("firmware", type=click.Path(exists=True))
@click.option("--gdb-port", type=int, default=None, help="Start GDB server on port.")
def run_emulator(firmware: str, gdb_port: int | None) -> None:
    """Run the emulator with a firmware file."""
    console.print("[yellow]Emulator not yet implemented.[/yellow]")
    console.print(f"Would run: {firmware}")
    if gdb_port:
        console.print(f"GDB server would listen on port {gdb_port}")


if __name__ == "__main__":
    main()
