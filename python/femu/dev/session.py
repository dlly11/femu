"""
AI Session Helper.

Helps AI assistants understand which files to read for each module
and provides context for development sessions.
"""

from typing import TypedDict

from rich.console import Console
from rich.table import Table

from .. import PROJECT_ROOT

console = Console()


class ModuleInfo(TypedDict):
    """Type definition for module information."""

    description: str
    header: str
    readme: str
    impl_dir: str
    deps: list[str]
    impl_files: list[str]
    test_files: list[str]


class PeripheralInfo(TypedDict, total=False):
    """Type definition for peripheral information."""

    description: str
    python_file: str
    deps: list[str]


# Module definitions - UPDATED to match actual codebase structure
MODULES: dict[str, ModuleInfo] = {
    "decoder": {
        "description": "Instruction decoder - Thumb-2 to DecodedInsn",
        "header": "include/arch/armv8m/armv8m_decoder.h",
        "readme": "src/arch/armv8m/decoder/README.md",
        "impl_dir": "src/arch/armv8m/decoder/",
        "deps": [
            "include/arch/armv8m/armv8m_types.h",
            "include/emu/emu_types.h",
            "include/emu/emu_decoder.h",
        ],
        "impl_files": [
            "decoder.c",
            "decode_thumb16.c",
            "decode_thumb32.c",
            "decode_thumb32_data.c",
            "decode_thumb32_loadstore.c",
            "decode_thumb32_branch.c",
            "decode_thumb32_multiply.c",
            "decode_thumb32_dsp.c",
            "decode_thumb32_vfp.c",
            "decoder_vtable.c",
        ],
        "test_files": ["tests/test_decoder.cpp"],
    },
    "executor": {
        "description": "Instruction executor - runs decoded instructions",
        "header": "include/arch/armv8m/armv8m_executor.h",
        "readme": "src/arch/armv8m/executor/README.md",
        "impl_dir": "src/arch/armv8m/executor/",
        "deps": [
            "include/arch/armv8m/armv8m_types.h",
            "include/arch/armv8m/armv8m_decoder.h",
            "include/emu/emu_memory.h",
            "include/emu/emu_executor.h",
        ],
        "impl_files": [
            "executor.c",
            "exec_data_proc.c",
            "exec_load_store.c",
            "exec_branch.c",
            "exec_system.c",
            "exec_exception.c",
            "exec_fpu.c",
            "icache.c",
            "blocks.c",
        ],
        "test_files": ["tests/test_main.cpp"],
    },
    "memory": {
        "description": "Memory subsystem - RAM, Flash, MMIO dispatch",
        "header": "include/emu/emu_memory.h",
        "readme": "src/core/memory/README.md",
        "impl_dir": "src/core/memory/",
        "deps": [
            "include/emu/emu_types.h",
            "include/emu/emu_peripheral.h",
        ],
        "impl_files": ["memory.c"],
        "test_files": ["tests/test_memory.cpp"],
    },
    "nvic": {
        "description": "Nested Vectored Interrupt Controller",
        "header": "include/arch/armv8m/armv8m_nvic.h",
        "readme": "src/arch/armv8m/nvic/README.md",
        "impl_dir": "src/arch/armv8m/nvic/",
        "deps": [
            "include/arch/armv8m/armv8m_types.h",
            "include/emu/emu_interrupt.h",
        ],
        "impl_files": ["nvic.c"],
        "test_files": ["tests/test_nvic.cpp"],
    },
    "mpu": {
        "description": "Memory Protection Unit (PMSAv8)",
        "header": "include/arch/armv8m/armv8m_mpu.h",
        "readme": "src/arch/armv8m/mpu/README.md",
        "impl_dir": "src/arch/armv8m/mpu/",
        "deps": [
            "include/arch/armv8m/armv8m_types.h",
        ],
        "impl_files": ["mpu.c"],
        "test_files": ["tests/test_mpu.cpp"],
    },
    "emulator": {
        "description": "Main emulator glue layer",
        "header": "include/arch/armv8m/armv8m_emulator.h",
        "readme": "src/arch/armv8m/README.md",
        "impl_dir": "src/arch/armv8m/",
        "deps": [
            "include/emu/emu_emulator.h",
            "include/arch/armv8m/armv8m_types.h",
        ],
        "impl_files": ["armv8m_emulator.c", "armv8m_cpu.c"],
        "test_files": [],
    },
}

# Peripheral modules - Python only (C/Rust peripherals not yet implemented)
PERIPHERALS: dict[str, PeripheralInfo] = {
    "uart": {
        "description": "UART peripheral",
        "python_file": "python/femu/peripherals/uart.py",
        "deps": ["include/emu/emu_peripheral.h"],
    },
    "gpio": {
        "description": "GPIO peripheral",
        "python_file": "python/femu/peripherals/gpio.py",
        "deps": ["include/emu/emu_peripheral.h"],
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


def _show_module_context(name: str, module: ModuleInfo) -> None:
    """Show context for a core module."""
    console.print()
    console.rule(f"[bold blue]AI CONTEXT FOR: {name}[/bold blue]")
    console.print(f"[dim]{module['description']}[/dim]")
    console.print()

    console.print("[bold]READ THESE FILES:[/bold]\n")

    console.print("  1. docs/architecture/overview.md")
    console.print("     [dim](Focus on: System Layers, C Simulation Core)[/dim]\n")

    header_lines = get_file_lines(module["header"])
    status = "ok" if file_exists(module["header"]) else "MISSING"
    console.print(f"  2. {module['header']}")
    console.print(f"     [{status}] {header_lines} lines - Interface definition\n")

    status = "ok" if file_exists(module["readme"]) else "MISSING"
    console.print(f"  3. {module['readme']}")
    console.print(f"     [{status}] Implementation guidance\n")

    for i, dep in enumerate(module["deps"], start=4):
        lines = get_file_lines(dep)
        status = "ok" if file_exists(dep) else "MISSING"
        console.print(f"  {i}. {dep}")
        console.print(f"     [{status}] {lines} lines - Dependency")

    console.print(f"\n[bold]IMPLEMENT IN:[/bold] {module['impl_dir']}\n")
    for f in module["impl_files"]:
        path = module["impl_dir"] + f
        status = "[green]exists[/green]" if file_exists(path) else "[dim]to create[/dim]"
        console.print(f"     {status} {f}")

    if module["test_files"]:
        console.print(f"\n[bold]TESTS IN:[/bold] {module['impl_dir']}tests/\n")
        for f in module["test_files"]:
            path = module["impl_dir"] + f
            status = "[green]exists[/green]" if file_exists(path) else "[dim]to create[/dim]"
            console.print(f"     {status} {f}")

    console.print()
    console.rule("[yellow]DO NOT READ other module implementations![/yellow]")
    console.print()


def _show_peripheral_context(name: str, periph: PeripheralInfo) -> None:
    """Show context for a peripheral module."""
    console.print()
    console.rule(f"[bold blue]AI CONTEXT FOR PERIPHERAL: {name}[/bold blue]")
    console.print(f"[dim]{periph['description']}[/dim]")
    console.print()

    console.print("[bold]READ THESE FILES:[/bold]\n")
    console.print("  1. docs/architecture/overview.md")
    console.print("     [dim](Focus on: Peripheral Access data flow)[/dim]\n")
    console.print("  2. docs/developer/writing-peripherals.md")
    console.print("     [dim]Peripheral development guide[/dim]\n")

    for i, dep in enumerate(periph["deps"], start=3):
        status = "ok" if file_exists(dep) else "MISSING"
        lines = get_file_lines(dep)
        console.print(f"  {i}. {dep}")
        console.print(f"     [{status}] {lines} lines - Peripheral interface")

    console.print("\n[bold]IMPLEMENTATION:[/bold]\n")
    if "python_file" in periph:
        status = (
            "[green]exists[/green]"
            if file_exists(periph["python_file"])
            else "[dim]to create[/dim]"
        )
        console.print(f"     {status} {periph['python_file']}")
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
            status = "[green]Complete[/green]"
        elif impl_count > 0:
            status = "[yellow]In Progress[/yellow]"
        elif header_ok and readme_ok:
            status = "[blue]Ready[/blue]"
        else:
            status = "[red]Not Started[/red]"

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
    table = Table(title="Peripherals (Python)")
    table.add_column("Peripheral", style="cyan")
    table.add_column("Status")
    table.add_column("Description", style="dim")

    for name, p in PERIPHERALS.items():
        if "python_file" in p and file_exists(p["python_file"]):
            status = "[green]Implemented[/green]"
        else:
            status = "[dim]Not implemented[/dim]"
        table.add_row(name, status, p["description"])

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
