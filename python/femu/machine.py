"""Machine definition and YAML loader.

The Machine class provides a high-level way to define complete emulated systems
including CPU configuration, memory regions, and peripherals.

Example using YAML::

    # stm32l5.yaml
    machine:
      name: STM32L5
      arch: armv8m

    cpu:
      has_fpu: true
      has_dsp: true

    memory:
      - type: flash
        base: 0x08000000
        size: 512K
      - type: ram
        base: 0x20000000
        size: 256K

    peripherals:
      - type: simple_uart
        name: USART1
        base: 0x40013800
        size: 0x400

    # In Python:
    machine = Machine.from_yaml("stm32l5.yaml")
    machine.load_elf("firmware.elf")
    machine.run()

Example using dict::

    machine = Machine.from_dict({
        "machine": {"name": "test", "arch": "armv8m"},
        "cpu": {"has_fpu": True},
        "memory": [
            {"type": "flash", "base": 0x08000000, "size": 0x10000},
        ],
        "peripherals": []
    })
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import TYPE_CHECKING, Any

from .arch.armv8m import ARMv8MConfig
from .emulator import ArchType, create_emulator
from .peripheral import PeripheralBase, PluginPeripheral
from .peripheral_registry import PeripheralRegistry

if TYPE_CHECKING:
    from .arch.base import BaseEmulator, EmulatorState
    from .elf_loader import ElfInfo

# =============================================================================
# Helper Functions
# =============================================================================


def _parse_size(value: str | int) -> int:
    """Parse size value with optional K/M/G suffix.

    Args:
        value: Integer or string like "512K", "1M", "0x10000"

    Returns:
        Size in bytes
    """
    if isinstance(value, int):
        return value

    value = str(value).strip().upper()

    # Handle suffixes
    multipliers = {
        "K": 1024,
        "KB": 1024,
        "M": 1024 * 1024,
        "MB": 1024 * 1024,
        "G": 1024 * 1024 * 1024,
        "GB": 1024 * 1024 * 1024,
    }

    for suffix, mult in multipliers.items():
        if value.endswith(suffix):
            num_str = value[: -len(suffix)].strip()
            return int(num_str) * mult

    # Handle hex (0x prefix)
    if value.startswith("0X"):
        return int(value, 16)

    return int(value)


def _parse_address(value: str | int) -> int:
    """Parse address value (supports hex strings).

    Args:
        value: Integer or string like "0x08000000"

    Returns:
        Address as integer
    """
    if isinstance(value, int):
        return value

    # int() with base 0 auto-detects hex/octal/decimal
    return int(str(value), 0)


# =============================================================================
# Configuration Dataclasses
# =============================================================================


@dataclass
class MemoryRegion:
    """Memory region definition."""

    type: str  # "flash" or "ram"
    base: int
    size: int

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> MemoryRegion:
        """Create from dictionary."""
        return cls(
            type=str(data["type"]).lower(),
            base=_parse_address(data["base"]),
            size=_parse_size(data["size"]),
        )


@dataclass
class PeripheralDef:
    """Peripheral definition from configuration."""

    type: str
    name: str
    base: int
    size: int
    config: dict[str, str | int | bool] = field(default_factory=dict)
    plugin: str | None = None  # Explicit plugin path (optional)

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> PeripheralDef:
        """Create from dictionary."""
        return cls(
            type=str(data["type"]),
            name=str(data["name"]),
            base=_parse_address(data["base"]),
            size=_parse_size(data["size"]),
            config=data.get("config", {}),
            plugin=data.get("plugin"),
        )


@dataclass
class CPUConfig:
    """CPU configuration."""

    has_fpu: bool = False
    has_dsp: bool = False
    has_trustzone: bool = False
    num_mpu_regions: int = 8
    num_irqs: int = 32

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> CPUConfig:
        """Create from dictionary."""
        return cls(
            has_fpu=bool(data.get("has_fpu", False)),
            has_dsp=bool(data.get("has_dsp", False)),
            has_trustzone=bool(data.get("has_trustzone", False)),
            num_mpu_regions=int(data.get("num_mpu_regions", 8)),
            num_irqs=int(data.get("num_irqs", 32)),
        )


@dataclass
class MachineDef:
    """Complete machine definition."""

    name: str
    arch: str
    cpu: CPUConfig
    memory: list[MemoryRegion]
    peripherals: list[PeripheralDef]
    plugins: list[str] = field(default_factory=list)
    plugin_dirs: list[str] = field(default_factory=list)

    @classmethod
    def from_dict(cls, data: dict[str, Any], base_path: Path | None = None) -> MachineDef:
        """Create from dictionary.

        Args:
            data: Configuration dictionary
            base_path: Base path for resolving relative plugin paths
        """
        machine_data: dict[str, Any] = data.get("machine", {})
        cpu_data: dict[str, Any] = data.get("cpu", {})
        memory_list: list[dict[str, Any]] = data.get("memory", [])
        periph_list: list[dict[str, Any]] = data.get("peripherals", [])

        # Parse plugins section
        plugins_raw: list[str] = data.get("plugins", [])
        plugin_dirs_raw: list[str] = data.get("plugin_dirs", [])

        # Resolve relative paths if base_path provided
        plugins: list[str]
        plugin_dirs: list[str]
        if base_path:
            plugins = [
                str((base_path / p).resolve()) if not Path(p).is_absolute() else p
                for p in plugins_raw
            ]
            plugin_dirs = [
                str((base_path / d).resolve()) if not Path(d).is_absolute() else d
                for d in plugin_dirs_raw
            ]
        else:
            plugins = plugins_raw
            plugin_dirs = plugin_dirs_raw

        return cls(
            name=str(machine_data.get("name", "unnamed")),
            arch=str(machine_data.get("arch", "armv8m")),
            cpu=CPUConfig.from_dict(cpu_data),
            memory=[MemoryRegion.from_dict(m) for m in memory_list],
            peripherals=[PeripheralDef.from_dict(p) for p in periph_list],
            plugins=plugins,
            plugin_dirs=plugin_dirs,
        )


# =============================================================================
# Machine Class
# =============================================================================


class Machine:
    """Machine instance with emulator and peripherals.

    A Machine combines:
    - CPU emulator with configuration
    - Memory regions (flash, RAM)
    - Peripherals (Python, C, or plugin)

    Machines can be defined programmatically or loaded from YAML files.
    """

    def __init__(self, definition: MachineDef) -> None:
        """Initialize machine from definition.

        Args:
            definition: MachineDef with complete configuration
        """
        self._def = definition
        self._peripherals: list[PeripheralBase] = []
        self._peripheral_map: dict[str, PeripheralBase] = {}

        # Load plugins first (before creating peripherals)
        self._load_plugins()

        # Create emulator with configuration
        self._emu = self._create_emulator()

        # Add memory regions
        for mem in definition.memory:
            self._add_memory_region(mem)

        # Add peripherals
        for pdef in definition.peripherals:
            periph = self._create_peripheral(pdef)
            self._emu.add_peripheral(periph, pdef.base, pdef.size)
            self._peripherals.append(periph)
            self._peripheral_map[pdef.name] = periph

    def _load_plugins(self) -> None:
        """Load all plugins specified in definition."""
        # Load explicit plugin files
        for plugin_path in self._def.plugins:
            try:
                PeripheralRegistry.load_plugin(plugin_path)
            except Exception as e:
                raise RuntimeError(f"Failed to load plugin {plugin_path}: {e}") from e

        # Load from plugin directories
        for plugin_dir in self._def.plugin_dirs:
            try:
                PeripheralRegistry.load_plugins_from_directory(plugin_dir)
            except Exception as e:
                raise RuntimeError(f"Failed to load plugins from {plugin_dir}: {e}") from e

    def _create_emulator(self) -> BaseEmulator:
        """Create emulator with appropriate configuration."""
        cpu = self._def.cpu

        arch_lower = self._def.arch.lower().replace("-", "")

        if arch_lower in ("armv8m", "armv8m33", "cortexm33"):
            config = ARMv8MConfig(
                has_fpu=cpu.has_fpu,
                has_dsp=cpu.has_dsp,
                has_trustzone=cpu.has_trustzone,
                num_mpu_regions=cpu.num_mpu_regions,
            )
            return create_emulator(ArchType.ARMV8M, config)

        if arch_lower in ("armv7m", "cortexm4", "cortexm3"):
            config = ARMv8MConfig(
                has_fpu=cpu.has_fpu,
                has_dsp=cpu.has_dsp,
                has_trustzone=False,  # ARMv7-M doesn't have TrustZone
                num_mpu_regions=cpu.num_mpu_regions,
            )
            return create_emulator(ArchType.ARMV7M, config)

        raise ValueError(f"Unsupported architecture: {self._def.arch}. Supported: armv8m, armv7m")

    def _add_memory_region(self, mem: MemoryRegion) -> None:
        """Add a memory region to the emulator."""
        if mem.type == "flash":
            self._emu.add_flash(mem.base, mem.size)
        elif mem.type == "ram":
            self._emu.add_ram(mem.base, mem.size)
        else:
            raise ValueError(f"Unknown memory type: {mem.type}. Supported: flash, ram")

    def _create_peripheral(self, pdef: PeripheralDef) -> PeripheralBase:
        """Create a peripheral from its definition."""
        # If plugin path is specified explicitly, load directly from there
        if pdef.plugin:
            return PluginPeripheral.from_plugin(pdef.plugin, pdef.type, pdef.name, pdef.config)

        # Otherwise use the registry (handles Python, C, and loaded plugins)
        return PeripheralRegistry.create(pdef.type, pdef.name, pdef.config)

    # =========================================================================
    # Factory Methods
    # =========================================================================

    @classmethod
    def from_yaml(cls, path: str | Path) -> Machine:
        """Load machine from YAML file.

        Args:
            path: Path to YAML configuration file

        Returns:
            Machine instance

        Example:
            machine = Machine.from_yaml("boards/stm32l5.yaml")
        """
        # Import yaml here to make it optional
        try:
            import yaml
        except ImportError as e:
            raise ImportError(
                "PyYAML is required to load YAML files. Install it with: uv pip install pyyaml"
            ) from e

        path = Path(path)

        if not path.exists():
            raise FileNotFoundError(f"Machine definition not found: {path}")

        with path.open(encoding="utf-8") as f:
            data = yaml.safe_load(f)

        if not isinstance(data, dict):
            msg = f"Invalid YAML format in {path}: expected a mapping"
            raise TypeError(msg)

        definition = MachineDef.from_dict(data, base_path=path.parent)
        return cls(definition)

    @classmethod
    def from_dict(cls, config: dict[str, Any], base_path: Path | None = None) -> Machine:
        """Create machine from dictionary.

        Args:
            config: Configuration dictionary
            base_path: Base path for resolving relative plugin paths

        Returns:
            Machine instance

        Example::

            machine = Machine.from_dict({
                "machine": {"name": "test", "arch": "armv8m"},
                "cpu": {"has_fpu": True},
                "memory": [
                    {"type": "flash", "base": 0x08000000, "size": "64K"},
                    {"type": "ram", "base": 0x20000000, "size": "32K"},
                ],
                "peripherals": []
            })
        """
        definition = MachineDef.from_dict(config, base_path)
        return cls(definition)

    # =========================================================================
    # Properties
    # =========================================================================

    @property
    def name(self) -> str:
        """Machine name from definition."""
        return self._def.name

    @property
    def arch(self) -> str:
        """Architecture name."""
        return self._def.arch

    @property
    def emu(self) -> BaseEmulator:
        """Underlying emulator instance."""
        return self._emu

    @property
    def peripherals(self) -> list[PeripheralBase]:
        """List of peripheral instances."""
        return list(self._peripherals)

    @property
    def peripheral_names(self) -> list[str]:
        """List of peripheral instance names."""
        return list(self._peripheral_map.keys())

    def get_peripheral(self, name: str) -> PeripheralBase | None:
        """Get peripheral by instance name.

        Args:
            name: Peripheral instance name (e.g., "USART1")

        Returns:
            Peripheral instance or None if not found
        """
        return self._peripheral_map.get(name)

    def __getitem__(self, name: str) -> PeripheralBase:
        """Get peripheral by name (dict-style access).

        Args:
            name: Peripheral instance name

        Returns:
            Peripheral instance

        Raises:
            KeyError: If peripheral not found
        """
        periph = self._peripheral_map.get(name)
        if periph is None:
            raise KeyError(f"No peripheral named '{name}'")
        return periph

    # =========================================================================
    # Delegated Emulator Methods
    # =========================================================================

    def load_elf(self, path: str | Path) -> ElfInfo:
        """Load ELF firmware file.

        Args:
            path: Path to ELF file

        Returns:
            ELF info object
        """
        return self._emu.load_elf(path)

    def run(self, max_cycles: int = 0) -> int:
        """Run emulation.

        Args:
            max_cycles: Maximum cycles to execute (0 = unlimited)

        Returns:
            Number of cycles executed
        """
        return self._emu.run(max_cycles)

    def step(self) -> int:
        """Execute single instruction.

        Returns:
            Result code
        """
        return self._emu.step()

    def reset(self) -> None:
        """Reset machine to initial state."""
        self._emu.reset()

    def stop(self) -> None:
        """Request emulation to stop."""
        self._emu.stop()

    @property
    def pc(self) -> int:
        """Program counter."""
        return self._emu.pc

    @pc.setter
    def pc(self, value: int) -> None:
        self._emu.pc = value

    @property
    def sp(self) -> int:
        """Stack pointer."""
        return self._emu.sp

    @property
    def cycles(self) -> int:
        """Total cycles executed."""
        return self._emu.cycles

    @property
    def state(self) -> EmulatorState:
        """Emulator state."""
        return self._emu.state

    def get_reg(self, reg: int) -> int:
        """Get general purpose register."""
        return self._emu.get_reg(reg)

    def set_reg(self, reg: int, value: int) -> None:
        """Set general purpose register."""
        self._emu.set_reg(reg, value)

    def read_mem(self, addr: int, size: int = 4) -> int:
        """Read from memory."""
        return self._emu.read_mem(addr, size)

    def write_mem(self, addr: int, value: int, size: int = 4) -> None:
        """Write to memory."""
        self._emu.write_mem(addr, value, size)

    def read_bytes(self, addr: int, length: int) -> bytes:
        """Read bytes from memory."""
        return self._emu.read_bytes(addr, length)

    def write_bytes(self, addr: int, data: bytes) -> int:
        """Write bytes to memory."""
        return self._emu.write_bytes(addr, data)

    # =========================================================================
    # Debugging
    # =========================================================================

    def dump_regs(self) -> dict[str, int]:
        """Dump all registers."""
        return self._emu.dump_regs()

    def __repr__(self) -> str:
        """Return a concise machine summary for debugging."""
        return (
            f"Machine(name={self._def.name!r}, arch={self._def.arch!r}, "
            f"peripherals={len(self._peripherals)})"
        )


# =============================================================================
# Exports
# =============================================================================

__all__ = [
    "CPUConfig",
    "Machine",
    "MachineDef",
    "MemoryRegion",
    "PeripheralDef",
]
