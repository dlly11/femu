"""FEMU CLI.

Main entry point for the emulator and development tools.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

import click
from rich.console import Console

from . import __version__

if TYPE_CHECKING:
    from .arch.base import BaseEmulator
    from .elf_loader import ElfInfo
    from .logging import LogCategory, LogLevel

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


@build.command("configure")
@click.option("--build-type", type=click.Choice(["Debug", "Release"]), default="Debug")
@click.option("--clean", is_flag=True, help="Clean build directory first.")
@click.option(
    "--compiler", type=click.Choice(["gcc", "clang"]), default=None, help="Compiler to use."
)
@click.option("--no-sanitizers", is_flag=True, help="Disable sanitizers in Debug builds.")
def build_configure(
    build_type: str, clean: bool, compiler: str | None, no_sanitizers: bool
) -> None:
    """Configure the CMake build."""
    from typing import cast

    from .build import Compiler, configure

    # Cast compiler to Literal type (click.Choice validates the value)
    typed_compiler = cast("Compiler | None", compiler)
    configure(
        build_type=build_type,
        clean=clean,
        compiler=typed_compiler,
        sanitizers=not no_sanitizers,
    )


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
@click.option(
    "--compiler", type=click.Choice(["gcc", "clang"]), default=None, help="Compiler to use."
)
@click.option("--no-sanitizers", is_flag=True, help="Disable sanitizers in Debug builds.")
def build_all(build_type: str, jobs: int | None, compiler: str | None, no_sanitizers: bool) -> None:
    """Configure and compile the project."""
    from typing import cast

    from .build import Compiler, compile_project, configure

    # Cast compiler to Literal type (click.Choice validates the value)
    typed_compiler = cast("Compiler | None", compiler)
    configure(build_type=build_type, compiler=typed_compiler, sanitizers=not no_sanitizers)
    compile_project(jobs=jobs)


@build.command("clean")
def build_clean() -> None:
    """Clean the build directory."""
    from .build import clean

    clean()


@build.command("analyze")
@click.option(
    "--tool",
    type=click.Choice(["cppcheck", "clang-tidy"]),
    default=None,
    help="Specific tool to run (default: all).",
)
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
# Documentation Commands
# =============================================================================


@main.group()
def docs() -> None:
    """Documentation commands."""


@docs.command("build")
@click.option("--clean", is_flag=True, help="Clean build directory first.")
def docs_build(clean: bool) -> None:
    """Build documentation locally."""
    import shutil
    import subprocess

    from . import PROJECT_ROOT

    docs_dir = PROJECT_ROOT / "docs"
    build_dir = docs_dir / "_build"
    doxygen_dir = PROJECT_ROOT / "build" / "docs"

    if clean and build_dir.exists():
        console.print("[yellow]Cleaning docs build directory...[/yellow]")
        shutil.rmtree(build_dir)

    # Ensure doxygen output directory exists
    doxygen_dir.mkdir(parents=True, exist_ok=True)

    # Run Doxygen first
    console.print("[bold]Running Doxygen...[/bold]")
    try:
        subprocess.run(
            ["doxygen", "Doxyfile"],
            cwd=PROJECT_ROOT,
            check=True,
        )
        console.print("[green]Doxygen complete[/green]")
    except FileNotFoundError:
        console.print("[yellow]Doxygen not found - skipping C API docs[/yellow]")
    except subprocess.CalledProcessError as e:
        console.print(f"[red]Doxygen failed:[/red] {e}")

    # Run Sphinx
    console.print("[bold]Running Sphinx...[/bold]")
    try:
        subprocess.run(
            ["sphinx-build", "-b", "html", str(docs_dir), str(build_dir / "html")],
            cwd=PROJECT_ROOT,
            check=True,
        )
        console.print(f"[green]Documentation built at:[/green] {build_dir / 'html'}")
    except FileNotFoundError:
        console.print("[red]sphinx-build not found[/red]")
        console.print("Install with: uv pip install sphinx")
        raise SystemExit(1) from None
    except subprocess.CalledProcessError as e:
        console.print(f"[red]Sphinx failed:[/red] {e}")
        raise SystemExit(1) from None


@docs.command("serve")
@click.option("--port", type=int, default=8000, help="Port to serve on.")
def docs_serve(port: int) -> None:
    """Serve documentation locally."""
    import http.server
    import os
    import socketserver

    from . import PROJECT_ROOT

    docs_html = PROJECT_ROOT / "docs" / "_build" / "html"

    if not docs_html.exists():
        console.print("[yellow]Documentation not built yet.[/yellow]")
        console.print("Run: femu docs build")
        raise SystemExit(1)

    os.chdir(docs_html)
    handler = http.server.SimpleHTTPRequestHandler

    with socketserver.TCPServer(("", port), handler) as httpd:
        console.print(f"[bold green]Serving docs at:[/bold green] http://localhost:{port}")
        console.print("Press Ctrl+C to stop")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            console.print("\n[yellow]Stopped[/yellow]")


# =============================================================================
# Run Command (future)
# =============================================================================


def _resolve_trace_levels(trace: tuple[str, ...]) -> dict[LogCategory, LogLevel] | None:
    """Map --trace category names to per-category TRACE level overrides."""
    from .logging import LogCategory, LogLevel

    category_map = {
        "decoder": LogCategory.DECODER,
        "executor": LogCategory.EXECUTOR,
        "memory": LogCategory.MEMORY,
        "nvic": LogCategory.NVIC,
        "mpu": LogCategory.MPU,
        "peripheral": LogCategory.PERIPHERAL,
        "gdb": LogCategory.GDB,
        "emulator": LogCategory.EMULATOR,
    }
    category_levels: dict[LogCategory, LogLevel] = {}
    for cat_name in trace:
        category = category_map.get(cat_name.lower())
        if category is None:
            console.print(f"[yellow]Unknown trace category: {cat_name}[/yellow]")
        else:
            category_levels[category] = LogLevel.TRACE
    return category_levels or None


def _print_loaded_elf(elf: ElfInfo) -> None:
    """Print entry point, vectors, and segment map for a loaded ELF."""
    console.print(f"  Entry point: {elf.entry_point:#010x}")
    if elf.initial_sp:
        console.print(f"  Initial SP:  {elf.initial_sp:#010x}")
    if elf.reset_vector:
        console.print(f"  Reset vector: {elf.reset_vector:#010x}")
    console.print(f"  Segments: {len(elf.segments)}")
    for seg in elf.segments:
        flags = ("X" if seg.is_executable else "") + ("W" if seg.is_writable else "")
        console.print(f"    {seg.vaddr:#010x} - {seg.vaddr + seg.memsz:#010x} [{flags}]")


def _print_registers(emu: BaseEmulator) -> None:
    """Print the final register file in rows of four."""
    console.print("\n[bold]Final registers:[/bold]")
    regs = emu.dump_regs()
    reg_names = list(regs.keys())
    for i in range(0, len(reg_names), 4):
        row = "  " + "".join(f"{name:4s}={regs[name]:#010x}  " for name in reg_names[i : i + 4])
        console.print(row)


def _serve_gdb(emu: BaseEmulator, gdb_port: int) -> None:
    """Start the GDB server, blocking until the client disconnects."""
    from .gdb_server import GDBServer

    console.print(f"\n[bold green]Starting GDB server on port {gdb_port}[/bold green]")
    server = GDBServer(emu, port=gdb_port)
    try:
        server.start()
    except KeyboardInterrupt:
        console.print("\n[yellow]Interrupted[/yellow]")


def _run_until_stop(emu: BaseEmulator, max_cycles: int, *, verbose: bool) -> None:
    """Run the emulator directly and report the final state."""
    console.print("[bold green]Running...[/bold green]")
    try:
        cycles = emu.run(max_cycles)
        console.print("\n[bold]Execution stopped[/bold]")
        console.print(f"  State:  {emu.state.name}")
        console.print(f"  Cycles: {cycles:,}")
        console.print(f"  PC:     {emu.pc:#010x}")
        if verbose:
            _print_registers(emu)
    except KeyboardInterrupt:
        console.print("\n[yellow]Interrupted[/yellow]")
        console.print(f"  Cycles: {emu.cycles:,}")
        console.print(f"  PC:     {emu.pc:#010x}")


@main.command("run")
@click.argument("firmware", type=click.Path(exists=True))
@click.option("--gdb-port", type=int, default=None, help="Start GDB server on port.")
@click.option("--max-cycles", type=int, default=0, help="Max cycles to execute (0=unlimited).")
@click.option(
    "-v", "--verbose", count=True, help="Verbosity level (-v=INFO, -vv=DEBUG, -vvv=TRACE)."
)
@click.option(
    "--trace",
    type=str,
    multiple=True,
    help="Enable TRACE for category (executor, decoder, memory, nvic, mpu, etc.).",
)
@click.option("--log-file", type=click.Path(), default=None, help="Log to file.")
@click.option("--json-log", is_flag=True, help="Use JSON log format.")
def run_emulator(  # noqa: PLR0913 - each parameter maps one-to-one to a click option
    firmware: str,
    gdb_port: int | None,
    max_cycles: int,
    verbose: int,
    trace: tuple[str, ...],
    log_file: str | None,
    json_log: bool,
) -> None:
    """Run the emulator with a firmware file."""
    from .logging import LogLevel, configure_logging

    # Configure logging based on verbosity
    level_map = {0: LogLevel.WARNING, 1: LogLevel.INFO, 2: LogLevel.DEBUG, 3: LogLevel.TRACE}
    log_level = level_map.get(min(verbose, 3), LogLevel.TRACE)

    configure_logging(
        level=log_level,
        category_levels=_resolve_trace_levels(trace),
        log_file=log_file,
        json_format=json_log,
    )

    try:
        from .emulator import create_emulator
    except OSError as e:
        console.print(f"[red]Error loading emulator library:[/red] {e}")
        console.print("[yellow]Build the project first with:[/yellow] femu build all")
        raise SystemExit(1) from None

    try:
        emu = create_emulator()
        elf = emu.load_elf(firmware)

        if verbose:
            console.print(f"[bold]Loaded:[/bold] {firmware}")
            _print_loaded_elf(elf)

        if gdb_port:
            _serve_gdb(emu, gdb_port)
        else:
            _run_until_stop(emu, max_cycles, verbose=bool(verbose))

    except FileNotFoundError as e:
        console.print(f"[red]File not found:[/red] {e}")
        raise SystemExit(1) from None
    except Exception as e:  # noqa: BLE001 - top-level CLI handler reports and exits
        console.print(f"[red]Error:[/red] {e}")
        if verbose:
            import traceback

            traceback.print_exc()
        raise SystemExit(1) from None


# =============================================================================
# CI Commands
# =============================================================================


@main.group()
def ci() -> None:
    """Run CI workflows locally."""


@ci.command("build")
def ci_build() -> None:
    """Run build workflow locally."""
    import subprocess

    subprocess.run(["act", "-j", "build", "-W", ".github/workflows/build.yml"], check=False)


@ci.command("test")
def ci_test() -> None:
    """Run test workflow locally."""
    import subprocess

    subprocess.run(["act", "-j", "test", "-W", ".github/workflows/test.yml"], check=False)


@ci.command("lint")
def ci_lint() -> None:
    """Run lint workflow locally."""
    import subprocess

    subprocess.run(["act", "-j", "lint", "-W", ".github/workflows/lint.yml"], check=False)


@ci.command("format")
def ci_format() -> None:
    """Run format workflow locally."""
    import subprocess

    subprocess.run(["act", "-j", "format", "-W", ".github/workflows/format.yml"], check=False)


@ci.command("all")
def ci_all() -> None:
    """Run all CI workflows locally."""
    import subprocess

    for workflow in ["build", "test", "lint", "format"]:
        subprocess.run(
            ["act", "-j", workflow, "-W", f".github/workflows/{workflow}.yml"], check=False
        )


if __name__ == "__main__":
    main()
