"""Unit tests for femu.machine.

The dataclass parsing / size helpers are pure Python and always run. Full
Machine construction needs the compiled C library and is skip-guarded.
"""

from __future__ import annotations

from typing import Any

import pytest
from femu.machine import (
    CPUConfig,
    Machine,
    MachineDef,
    MemoryRegion,
    PeripheralDef,
    _parse_address,
    _parse_size,
)


class TestSizeParsing:
    @pytest.mark.parametrize(
        ("value", "expected"),
        [
            ("512K", 512 * 1024),
            ("2M", 2 * 1024 * 1024),
            ("0x1000", 0x1000),
            (4096, 4096),
        ],
    )
    def test_parse_size(self, value: str | int, expected: int) -> None:
        assert _parse_size(value) == expected

    @pytest.mark.parametrize(
        ("value", "expected"),
        [("0x08000000", 0x08000000), (0x20000000, 0x20000000)],
    )
    def test_parse_address(self, value: str | int, expected: int) -> None:
        assert _parse_address(value) == expected


class TestDataclassParsing:
    def test_memory_region(self) -> None:
        mr = MemoryRegion.from_dict({"type": "flash", "base": "0x08000000", "size": "1M"})
        assert mr.type == "flash"
        assert mr.base == 0x08000000
        assert mr.size == 1024 * 1024

    def test_peripheral_def_defaults(self) -> None:
        pd = PeripheralDef.from_dict(
            {"type": "simple_uart", "name": "USART1", "base": "0x40000000", "size": "0x100"}
        )
        assert pd.name == "USART1"
        assert pd.base == 0x40000000
        assert pd.config == {}
        assert pd.plugin is None

    def test_cpu_config_overrides_and_defaults(self) -> None:
        cpu = CPUConfig.from_dict({"has_fpu": True, "num_irqs": 48})
        assert cpu.has_fpu is True
        assert cpu.num_irqs == 48
        # Untouched fields keep their defaults
        assert cpu.has_dsp is False
        assert cpu.num_mpu_regions == 8


def sample_config() -> dict[str, Any]:
    return {
        "machine": {"name": "test-board", "arch": "armv8m"},
        "cpu": {"has_fpu": True},
        "memory": [
            {"type": "flash", "base": "0x08000000", "size": "1M"},
            {"type": "ram", "base": "0x20000000", "size": "128K"},
        ],
        "peripherals": [
            {"type": "simple_uart", "name": "U1", "base": "0x40004400", "size": "0x100"},
        ],
    }


class TestMachineDef:
    def test_from_dict_reads_machine_section(self) -> None:
        md = MachineDef.from_dict(sample_config())
        assert md.name == "test-board"
        assert md.arch == "armv8m"
        assert len(md.memory) == 2
        assert len(md.peripherals) == 1
        assert md.cpu.has_fpu is True

    def test_from_dict_defaults_when_machine_section_absent(self) -> None:
        md = MachineDef.from_dict({"memory": [], "peripherals": []})
        assert md.name == "unnamed"
        assert md.arch == "armv8m"


class TestMachineConstruction:
    """Full construction requires the compiled emulator library."""

    def test_build_machine_from_dict(self, emulator_lib_available: bool) -> None:
        if not emulator_lib_available:
            pytest.skip("emulator library not loadable in this environment")
        machine = Machine.from_dict(sample_config())
        assert machine.name == "test-board"
        assert "U1" in machine.peripheral_names
        assert machine.get_peripheral("U1") is not None
        assert machine.get_peripheral("does-not-exist") is None
