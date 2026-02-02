"""
Build system integration for FEMU.

Handles CMake configuration, compilation, and packaging.
"""

import os
import shutil
import subprocess
from pathlib import Path
from typing import Literal

from rich.console import Console

from . import BUILD_DIR, PROJECT_ROOT

console = Console()

Compiler = Literal["gcc", "clang"]


def get_cpu_count() -> int:
    """Get the number of CPUs for parallel builds."""
    return os.cpu_count() or 1


def run_command(
    cmd: list[str],
    cwd: Path | None = None,
    check: bool = True,
    capture: bool = False,
    env: dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    """Run a command and handle errors."""
    console.print(f"[dim]$ {' '.join(cmd)}[/dim]")

    # Merge with current environment
    full_env = dict(os.environ)
    if env:
        full_env.update(env)

    try:
        result = subprocess.run(
            cmd,
            cwd=cwd,
            check=check,
            capture_output=capture,
            text=True,
            env=full_env,
        )
        return result
    except subprocess.CalledProcessError as e:
        console.print(f"[red]Command failed with exit code {e.returncode}[/red]")
        if e.stdout:
            console.print(e.stdout)
        if e.stderr:
            console.print(f"[red]{e.stderr}[/red]")
        raise


def configure(
    build_type: str = "Debug",
    clean: bool = False,
    compiler: Compiler | None = None,
    sanitizers: bool = True,
) -> None:
    """
    Configure the CMake build.

    Args:
        build_type: Build type (Debug, Release)
        clean: Clean build directory first
        compiler: Compiler to use (gcc, clang, or None for system default)
        sanitizers: Enable sanitizers in Debug builds
    """
    console.print(f"\n[bold blue]Configuring build ({build_type})...[/bold blue]\n")

    if clean and BUILD_DIR.exists():
        console.print(f"[yellow]Cleaning {BUILD_DIR}...[/yellow]")
        shutil.rmtree(BUILD_DIR)

    BUILD_DIR.mkdir(parents=True, exist_ok=True)

    cmd = [
        "cmake",
        f"-DCMAKE_BUILD_TYPE={build_type}",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        f"-DENABLE_SANITIZERS={'ON' if sanitizers else 'OFF'}",
    ]

    # Environment for compiler selection
    env: dict[str, str] = {}

    if compiler == "clang":
        env["CC"] = "clang"
        env["CXX"] = "clang++"
        console.print("[cyan]Using Clang compiler[/cyan]")
    elif compiler == "gcc":
        env["CC"] = "gcc"
        env["CXX"] = "g++"
        console.print("[cyan]Using GCC compiler[/cyan]")

    cmd.append(str(PROJECT_ROOT))

    run_command(cmd, cwd=BUILD_DIR, env=env if env else None)

    # Symlink compile_commands.json to project root for IDE support
    compile_commands = BUILD_DIR / "compile_commands.json"
    root_compile_commands = PROJECT_ROOT / "compile_commands.json"
    if compile_commands.exists() and not root_compile_commands.exists():
        root_compile_commands.symlink_to(compile_commands)
        console.print("[dim]Symlinked compile_commands.json to project root[/dim]")

    console.print("\n[green]Configuration complete.[/green]")


def compile_project(jobs: int | None = None, target: str | None = None) -> None:
    """Compile the project."""
    if not BUILD_DIR.exists():
        console.print("[yellow]Build directory not found. Running configure first...[/yellow]")
        configure()

    console.print("\n[bold blue]Compiling...[/bold blue]\n")

    jobs = jobs or get_cpu_count()
    cmd = ["cmake", "--build", str(BUILD_DIR), "-j", str(jobs)]

    if target:
        cmd.extend(["--target", target])

    run_command(cmd)
    console.print("\n[green]Compilation complete.[/green]")


def clean() -> None:
    """Clean the build directory."""
    if BUILD_DIR.exists():
        console.print(f"[yellow]Removing {BUILD_DIR}...[/yellow]")
        shutil.rmtree(BUILD_DIR)
        console.print("[green]Clean complete.[/green]")
    else:
        console.print("[dim]Build directory does not exist.[/dim]")

    # Also remove compile_commands.json symlink
    compile_commands = PROJECT_ROOT / "compile_commands.json"
    if compile_commands.is_symlink():
        compile_commands.unlink()


def run_ctest(verbose: bool = False, test_filter: str | None = None) -> bool:
    """Run CTest and return success status."""
    if not BUILD_DIR.exists():
        console.print("[red]Build directory not found. Run 'femu build all' first.[/red]")
        return False

    cmd = ["ctest", "--output-on-failure"]

    if verbose:
        cmd.append("-V")

    if test_filter:
        cmd.extend(["-R", test_filter])

    try:
        run_command(cmd, cwd=BUILD_DIR)
        return True
    except subprocess.CalledProcessError:
        return False


def run_analysis(tool: str | None = None) -> bool:
    """
    Run static analysis tools.

    Args:
        tool: Specific tool to run (cppcheck, clang-tidy, or None for all)

    Returns:
        True if analysis passed
    """
    if not BUILD_DIR.exists():
        console.print("[yellow]Build directory not found. Running configure first...[/yellow]")
        configure()

    console.print("\n[bold blue]Running static analysis...[/bold blue]\n")

    target = tool if tool else "analyze"
    cmd = ["cmake", "--build", str(BUILD_DIR), "--target", target]

    try:
        run_command(cmd)
        console.print("\n[green]Static analysis complete.[/green]")
        return True
    except subprocess.CalledProcessError:
        console.print("\n[red]Static analysis found issues.[/red]")
        return False


def package(output_dir: Path | None = None) -> Path | None:
    """
    Package the emulator for distribution.

    Builds a release version and creates a Python wheel with the
    shared library bundled.

    Args:
        output_dir: Output directory for wheel (default: PROJECT_ROOT/dist)

    Returns:
        Path to the created wheel, or None on failure
    """
    import sys

    console.print("\n[bold blue]Packaging emulator...[/bold blue]\n")

    # Step 1: Build Release version
    configure(build_type="Release", sanitizers=False)
    compile_project()

    # Step 2: Create _lib directory and copy the shared library
    lib_dir = PROJECT_ROOT / "python" / "femu" / "_lib"
    lib_dir.mkdir(exist_ok=True)

    # Determine platform-specific library name
    if sys.platform == "darwin":
        lib_name = "libarmv8m_emulator.dylib"
    elif sys.platform == "win32":
        lib_name = "armv8m_emulator.dll"
    else:
        lib_name = "libarmv8m_emulator.so"

    src_lib = PROJECT_ROOT / "build" / "src" / "arch" / "armv8m" / lib_name

    if not src_lib.exists():
        console.print(f"[red]Library not found: {src_lib}[/red]")
        return None

    dest_lib = lib_dir / lib_name
    console.print(f"[dim]Copying {src_lib} -> {dest_lib}[/dim]")
    shutil.copy2(src_lib, dest_lib)

    # Step 3: Build wheel
    console.print("\n[bold]Building wheel...[/bold]\n")

    output_dir = output_dir or PROJECT_ROOT / "dist"
    output_dir.mkdir(parents=True, exist_ok=True)

    cmd = [
        sys.executable,
        "-m",
        "build",
        "--wheel",
        "--outdir",
        str(output_dir),
    ]

    try:
        run_command(cmd, cwd=PROJECT_ROOT)
    except subprocess.CalledProcessError:
        console.print("[red]Failed to build wheel.[/red]")
        return None

    # Step 4: Find and return the wheel path
    wheels = sorted(output_dir.glob("femu-*.whl"), key=lambda p: p.stat().st_mtime, reverse=True)

    if not wheels:
        console.print("[red]No wheel found in output directory.[/red]")
        return None

    wheel_path = wheels[0]
    console.print(f"\n[green]Package created: {wheel_path}[/green]")
    return wheel_path
