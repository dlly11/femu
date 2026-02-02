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
@click.option("--max-cycles", type=int, default=0, help="Max cycles to execute (0=unlimited).")
@click.option("-v", "--verbose", count=True, help="Verbosity level (-v=INFO, -vv=DEBUG, -vvv=TRACE).")
@click.option("--trace", type=str, multiple=True, help="Enable TRACE for category (executor, decoder, memory, nvic, mpu, peripheral, gdb, emulator).")
@click.option("--log-file", type=click.Path(), default=None, help="Log to file.")
@click.option("--json-log", is_flag=True, help="Use JSON log format.")
def run_emulator(firmware: str, gdb_port: int | None, max_cycles: int, verbose: int,
                 trace: tuple[str, ...], log_file: str | None, json_log: bool) -> None:
    """Run the emulator with a firmware file."""
    from .logging import configure_logging, LogLevel, LogCategory

    # Configure logging based on verbosity
    level_map = {0: LogLevel.WARNING, 1: LogLevel.INFO, 2: LogLevel.DEBUG, 3: LogLevel.TRACE}
    log_level = level_map.get(min(verbose, 3), LogLevel.TRACE)

    # Build category overrides for --trace
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
    category_levels = {}
    for cat_name in trace:
        if cat_name.lower() in category_map:
            category_levels[category_map[cat_name.lower()]] = LogLevel.TRACE
        else:
            console.print(f"[yellow]Unknown trace category: {cat_name}[/yellow]")

    configure_logging(
        level=log_level,
        category_levels=category_levels if category_levels else None,
        log_file=log_file,
        json_format=json_log,
    )

    try:
        from .emulator import Emulator, EmulatorState
        from .gdb_server import GDBServer
    except OSError as e:
        console.print(f"[red]Error loading emulator library:[/red] {e}")
        console.print("[yellow]Build the project first with:[/yellow] femu build all")
        raise SystemExit(1)

    try:
        emu = Emulator()
        elf = emu.load_elf(firmware)

        if verbose:
            console.print(f"[bold]Loaded:[/bold] {firmware}")
            console.print(f"  Entry point: {elf.entry_point:#010x}")
            if elf.initial_sp:
                console.print(f"  Initial SP:  {elf.initial_sp:#010x}")
            if elf.reset_vector:
                console.print(f"  Reset vector: {elf.reset_vector:#010x}")
            console.print(f"  Segments: {len(elf.segments)}")
            for seg in elf.segments:
                flags = ""
                if seg.is_executable:
                    flags += "X"
                if seg.is_writable:
                    flags += "W"
                console.print(f"    {seg.vaddr:#010x} - {seg.vaddr + seg.memsz:#010x} [{flags}]")

        if gdb_port:
            # Start GDB server (blocks until disconnect)
            console.print(f"\n[bold green]Starting GDB server on port {gdb_port}[/bold green]")
            server = GDBServer(emu, port=gdb_port)
            try:
                server.start()
            except KeyboardInterrupt:
                console.print("\n[yellow]Interrupted[/yellow]")
        else:
            # Run directly
            console.print("[bold green]Running...[/bold green]")
            try:
                cycles = emu.run(max_cycles)
                state = emu.state

                console.print(f"\n[bold]Execution stopped[/bold]")
                console.print(f"  State:  {state.name}")
                console.print(f"  Cycles: {cycles:,}")
                console.print(f"  PC:     {emu.pc:#010x}")

                if verbose:
                    console.print("\n[bold]Final registers:[/bold]")
                    regs = emu.dump_regs()
                    # Print in rows of 4
                    reg_names = list(regs.keys())
                    for i in range(0, len(reg_names), 4):
                        row = "  "
                        for name in reg_names[i : i + 4]:
                            row += f"{name:4s}={regs[name]:#010x}  "
                        console.print(row)

            except KeyboardInterrupt:
                console.print("\n[yellow]Interrupted[/yellow]")
                console.print(f"  Cycles: {emu.cycles:,}")
                console.print(f"  PC:     {emu.pc:#010x}")

    except FileNotFoundError as e:
        console.print(f"[red]File not found:[/red] {e}")
        raise SystemExit(1)
    except Exception as e:
        console.print(f"[red]Error:[/red] {e}")
        if verbose:
            import traceback
            traceback.print_exc()
        raise SystemExit(1)


if __name__ == "__main__":
    main()
