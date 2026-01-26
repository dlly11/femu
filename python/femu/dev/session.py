"""
AI Session Helper.

Helps AI assistants understand which files to read for each module
and provides context for development sessions.
"""

from pathlib import Path

from rich.console import Console
from rich.table import Table

from .. import PROJECT_ROOT

console = Console()

# Module definitions
MODULES: dict[str, dict] = {
    "decoder": {
        "description": "Instruction decoder - Thumb to DecodedInsn",
        "header": "include/armv8m_decoder.h",
        "readme": "src/core/decoder/README.md",
        "impl_dir": "src/core/decoder/",
        "deps": ["include/armv8m_types.h"],
        "impl_files": ["decoder.c", "decode_thumb16.c", "decode_thumb32.c"],
        "test_files": ["tests/test_decoder.cpp"],
    },
    "executor": {
        "description": "Instruction executor - runs decoded instructions",
        "header": "include/armv8m_executor.h",
        "readme": "src/core/executor/README.md",
        "impl_dir": "src/core/executor/",
        "deps": [
            "include/armv8m_types.h",
            "include/armv8m_decoder.h",
            "include/armv8m_memory.h",
        ],
        "impl_files": [
            "executor.c",
            "exec_data_proc.c",
            "exec_load_store.c",
            "exec_branch.c",
            "exec_system.c",
        ],
        "test_files": ["tests/test_executor.cpp"],
    },
    "memory": {
        "description": "Memory subsystem - RAM, Flash, MMIO dispatch",
        "header": "include/armv8m_memory.h",
        "readme": "src/core/memory/README.md",
        "impl_dir": "src/core/memory/",
        "deps": ["include/armv8m_types.h", "include/peripheral_interface.h"],
        "impl_files": ["memory.c", "bus.c"],
        "test_files": ["tests/test_memory.cpp"],
    },
    "nvic": {
        "description": "Nested Vectored Interrupt Controller",
        "header": "include/armv8m_nvic.h",
        "readme": "src/core/nvic/README.md",
        "impl_dir": "src/core/nvic/",
        "deps": ["include/armv8m_types.h"],
        "impl_files": ["nvic.c"],
        "test_files": ["tests/test_nvic.cpp"],
    },
    "mpu": {
        "description": "Memory Protection Unit (PMSAv8)",
        "header": "include/armv8m_mpu.h",
        "readme": "src/core/mpu/README.md",
        "impl_dir": "src/core/mpu/",
        "deps": ["include/armv8m_types.h"],
        "impl_files": ["mpu.c"],
        "test_files": ["tests/test_mpu.cpp"],
    },
}

# Peripheral modules
PERIPHERALS: dict[str, dict] = {
    "uart": {
        "description": "UART peripheral (STM32-style)",
        "languages": ["c", "python"],
        "c_dir": "peripherals/c/uart/",
        "python_file": "peripherals/python/uart.py",
        "deps": ["include/peripheral_interface.h"],
    },
    "gpio": {
        "description": "GPIO peripheral (STM32-style)",
        "languages": ["rust", "python"],
        "rust_file": "peripherals/rust/src/gpio.rs",
        "python_file": "peripherals/python/gpio.py",
        "deps": ["include/peripheral_interface.h"],
    },
    "timer": {
        "description": "Basic timer peripheral",
        "languages": ["c", "python"],
        "c_dir": "peripherals/c/timer/",
        "python_file": "peripherals/python/timer.py",
        "deps": ["include/peripheral_interface.h"],
    },
}


def file_exists(filepath: str) -> bool:
    """Check if a file exists relative to project root."""
    return (PROJECT_ROOT / filepath).exists()


def get_file_lines(filepath: str) -> int:
    """Get line count of a file."""
    path = PROJECT_ROOT / filepath
    if path.exists():
        return len(path.read_text().splitlines())
    return 0


def show_context(module: str) -> None:
    """Print files AI should read for this module."""
    if module in MODULES:
        _show_module_context(module, MODULES[module])
    elif module in PERIPHERALS:
        _show_peripheral_context(module, PERIPHERALS[module])
    else:
        console.print(f"[red]Unknown module: {module}[/red]")
        console.print(f"Available modules: {', '.join(MODULES.keys())}")
        console.print(f"Available peripherals: {', '.join(PERIPHERALS.keys())}")


def _show_module_context(name: str, module: dict) -> None:
    """Show context for a core module."""
    console.print()
    console.rule(f"[bold blue]AI CONTEXT FOR: {name}[/bold blue]")
    console.print(f"[dim]{module['description']}[/dim]")
    console.print()

    console.print("[bold]📖 READ THESE FILES:[/bold]\n")

    console.print("  1. docs/ARCHITECTURE.md")
    console.print("     [dim](Focus on: Part 5, Part 9)[/dim]\n")

    header_lines = get_file_lines(module["header"])
    status = "✓" if file_exists(module["header"]) else "✗ MISSING"
    console.print(f"  2. {module['header']}")
    console.print(f"     [{status}] {header_lines} lines - Interface definition\n")

    status = "✓" if file_exists(module["readme"]) else "✗ MISSING"
    console.print(f"  3. {module['readme']}")
    console.print(f"     [{status}] Implementation guidance\n")

    for i, dep in enumerate(module["deps"], start=4):
        lines = get_file_lines(dep)
        status = "✓" if file_exists(dep) else "✗ MISSING"
        console.print(f"  {i}. {dep}")
        console.print(f"     [{status}] {lines} lines - Dependency")

    console.print(f"\n[bold]📝 IMPLEMENT IN:[/bold] {module['impl_dir']}\n")
    for f in module["impl_files"]:
        path = module["impl_dir"] + f
        status = "[green]✓ exists[/green]" if file_exists(path) else "[dim]○ to create[/dim]"
        console.print(f"     {status} {f}")

    console.print(f"\n[bold]🧪 TESTS IN:[/bold] {module['impl_dir']}tests/\n")
    for f in module["test_files"]:
        path = module["impl_dir"] + f
        status = "[green]✓ exists[/green]" if file_exists(path) else "[dim]○ to create[/dim]"
        console.print(f"     {status} {f}")

    console.print()
    console.rule("[yellow]DO NOT READ other module implementations![/yellow]")
    console.print()


def _show_peripheral_context(name: str, periph: dict) -> None:
    """Show context for a peripheral module."""
    console.print()
    console.rule(f"[bold blue]AI CONTEXT FOR PERIPHERAL: {name}[/bold blue]")
    console.print(f"[dim]{periph['description']}[/dim]")
    console.print()

    console.print("[bold]📖 READ THESE FILES:[/bold]\n")
    console.print("  1. docs/ARCHITECTURE.md")
    console.print("     [dim](Focus on: Part 8)[/dim]\n")

    for i, dep in enumerate(periph["deps"], start=2):
        status = "✓" if file_exists(dep) else "✗ MISSING"
        console.print(f"  {i}. {dep}")
        console.print(f"     [{status}] Peripheral ABI")

    console.print("\n[bold]📝 IMPLEMENT IN:[/bold]\n")
    if "c_dir" in periph:
        console.print(f"     C:      {periph['c_dir']}")
    if "rust_file" in periph:
        console.print(f"     Rust:   {periph['rust_file']}")
    if "python_file" in periph:
        console.print(f"     Python: {periph['python_file']}")
    console.print()


def show_status() -> None:
    """Show status of all modules."""
    console.print()
    console.rule("[bold]MODULE STATUS[/bold]")
    console.print()

    table = Table(title="Core Modules")
    table.add_column("Module", style="cyan")
    table.add_column("Status")
    table.add_column("Impl", justify="center")
    table.add_column("Tests", justify="center")
    table.add_column("Description", style="dim")

    for name, m in MODULES.items():
        header_ok = file_exists(m["header"])
        readme_ok = file_exists(m["readme"])

        impl_count = sum(1 for f in m["impl_files"] if file_exists(m["impl_dir"] + f))
        impl_total = len(m["impl_files"])

        test_count = sum(1 for f in m["test_files"] if file_exists(m["impl_dir"] + f))
        test_total = len(m["test_files"])

        if impl_count == impl_total and impl_count > 0:
            status = "[green]✅ Complete[/green]"
        elif impl_count > 0:
            status = "[yellow]🔨 In Progress[/yellow]"
        elif header_ok and readme_ok:
            status = "[blue]📋 Ready[/blue]"
        else:
            status = "[red]❌ Not Started[/red]"

        table.add_row(
            name,
            status,
            f"{impl_count}/{impl_total}",
            f"{test_count}/{test_total}",
            m["description"],
        )

    console.print(table)
    console.print()

    # Peripherals table
    table = Table(title="Peripherals")
    table.add_column("Peripheral", style="cyan")
    table.add_column("Implemented In")
    table.add_column("Description", style="dim")

    for name, p in PERIPHERALS.items():
        langs = []
        if "c_dir" in p and file_exists(p["c_dir"]):
            langs.append("C")
        if "rust_file" in p and file_exists(p["rust_file"]):
            langs.append("Rust")
        if "python_file" in p and file_exists(p["python_file"]):
            langs.append("Python")

        lang_str = ", ".join(langs) if langs else "[dim]none[/dim]"
        table.add_row(name, lang_str, p["description"])

    console.print(table)
    console.print()


def list_modules() -> None:
    """List all available modules."""
    console.print("\n[bold]Core Modules:[/bold]")
    for name, m in MODULES.items():
        console.print(f"  [cyan]{name:12}[/cyan] - {m['description']}")

    console.print("\n[bold]Peripherals:[/bold]")
    for name, p in PERIPHERALS.items():
        console.print(f"  [cyan]{name:12}[/cyan] - {p['description']}")

    console.print()
