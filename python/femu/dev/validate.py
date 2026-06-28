"""Module validation tools.

Validates that module implementations:
- Exist and have required files
- Compile with strict warnings
- Pass all tests
"""

import subprocess

from rich.console import Console

from .. import BUILD_DIR, PROJECT_ROOT
from ..build import compile_project, configure, run_command
from .session import MODULES

console = Console()


def validate_module(module: str) -> bool:
    """Validate a single module implementation.

    Returns True if validation passed, False otherwise.
    """
    if module not in MODULES:
        console.print(f"[red]Unknown module: {module}[/red]")
        console.print(f"Available modules: {', '.join(MODULES.keys())}")
        return False

    m = MODULES[module]
    impl_dir = PROJECT_ROOT / m["impl_dir"]
    main_file = impl_dir / m["impl_files"][0]

    console.print()
    console.rule(f"[bold blue]Validating: {module}[/bold blue]")
    console.print()

    errors = 0

    # Check if module is implemented
    if not main_file.exists():
        console.print(
            f"[yellow]Module '{module}' not implemented yet (missing {m['impl_files'][0]})[/yellow]"
        )
        return True  # Not an error, just not implemented

    # Step 1: Check required files exist
    console.print("[bold]Step 1:[/bold] Checking source files...")
    for f in m["impl_files"]:
        path = impl_dir / f
        if path.exists():
            console.print(f"  [green]✓[/green] {f}")
        else:
            console.print(f"  [yellow]○[/yellow] {f} (optional)")

    # Step 2: Build module
    console.print("\n[bold]Step 2:[/bold] Building module...")
    try:
        if not BUILD_DIR.exists():
            configure(build_type="Debug")

        # Build specific target
        target = f"armv8m_{module}"
        compile_project(target=target)
        console.print("  [green]✓[/green] Build successful")
    except subprocess.CalledProcessError:
        console.print("  [red]✗[/red] Build failed")
        errors += 1
        return False

    # Step 3: Run tests
    console.print("\n[bold]Step 3:[/bold] Running tests...")
    test_exe = BUILD_DIR / f"test_{module}"

    if test_exe.exists():
        try:
            run_command([str(test_exe), "-v"], cwd=BUILD_DIR)
            console.print("  [green]✓[/green] All tests passed")
        except subprocess.CalledProcessError:
            console.print("  [red]✗[/red] Tests failed")
            errors += 1
    else:
        console.print("  [yellow]○[/yellow] No test executable found")

    # Summary
    console.print()
    if errors == 0:
        console.print(f"[green]✓ Module '{module}' validated successfully![/green]")
        return True
    console.print(f"[red]✗ Module '{module}' has {errors} error(s)[/red]")
    return False


def validate_all_modules() -> bool:
    """Validate all implemented modules.

    Returns True if all validations passed.
    """
    console.print()
    console.rule("[bold]Validating All Modules[/bold]")

    results: dict[str, bool] = {}

    for module, m in MODULES.items():
        impl_dir = PROJECT_ROOT / m["impl_dir"]
        main_file = impl_dir / m["impl_files"][0]

        if main_file.exists():
            results[module] = validate_module(module)
        else:
            console.print(f"\n[dim]Skipping {module} (not implemented)[/dim]")

    # Summary
    console.print()
    console.rule("[bold]Validation Summary[/bold]")
    console.print()

    all_passed = True
    for module, passed in results.items():
        status = "[green]✓ PASS[/green]" if passed else "[red]✗ FAIL[/red]"
        console.print(f"  {module:12} {status}")
        if not passed:
            all_passed = False

    console.print()
    return all_passed
