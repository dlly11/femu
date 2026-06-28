"""Unit tests for the pure-Python ELF loader (femu.elf_loader).

Uses the prebuilt firmware ELFs under tests/firmware. No C library required.
"""

from __future__ import annotations

from pathlib import Path

import pytest
from femu.elf_loader import ElfError, load_elf, suggest_memory_config

EM_ARM = 40


@pytest.fixture
def simple_elf(test_firmware_dir: Path) -> Path:
    path = test_firmware_dir / "test_simple.elf"
    if not path.exists():
        pytest.skip(f"test firmware not built: {path}")
    return path


class TestLoadValidElf:
    def test_entry_point_in_flash(self, simple_elf: Path) -> None:
        info = load_elf(simple_elf)
        assert info.entry_point >= 0x08000000

    def test_machine_is_arm(self, simple_elf: Path) -> None:
        assert load_elf(simple_elf).machine == EM_ARM

    def test_has_segments(self, simple_elf: Path) -> None:
        info = load_elf(simple_elf)
        assert len(info.segments) > 0

    def test_accepts_path_as_string(self, simple_elf: Path) -> None:
        info = load_elf(str(simple_elf))
        assert info.entry_point >= 0x08000000

    def test_initial_sp_present(self, simple_elf: Path) -> None:
        info = load_elf(simple_elf)
        assert info.initial_sp is not None
        # Cortex-M reset stack pointer lives in the RAM region
        assert info.initial_sp >= 0x20000000

    def test_executable_segment_flagged(self, simple_elf: Path) -> None:
        info = load_elf(simple_elf)
        assert any(seg.is_executable for seg in info.segments)

    def test_flash_regions_nonempty(self, simple_elf: Path) -> None:
        info = load_elf(simple_elf)
        assert info.flash_regions
        for base, size in info.flash_regions:
            assert base >= 0x08000000
            assert size > 0


class TestSuggestMemoryConfig:
    def test_returns_expected_keys(self, simple_elf: Path) -> None:
        cfg = suggest_memory_config(load_elf(simple_elf))
        assert set(cfg) >= {"flash_base", "flash_size", "ram_base", "ram_size"}

    def test_flash_base_matches_segment(self, simple_elf: Path) -> None:
        info = load_elf(simple_elf)
        cfg = suggest_memory_config(info)
        assert cfg["flash_base"] == 0x08000000
        assert cfg["flash_size"] >= info.flash_regions[0][1]


class TestMalformedInput:
    def test_missing_file_raises(self) -> None:
        with pytest.raises(FileNotFoundError):
            load_elf("/nonexistent/does-not-exist.elf")

    def test_empty_file_raises_elf_error(self, tmp_path: Path) -> None:
        bad = tmp_path / "empty.elf"
        bad.write_bytes(b"")
        with pytest.raises(ElfError):
            load_elf(bad)

    def test_bad_magic_raises_elf_error(self, tmp_path: Path) -> None:
        bad = tmp_path / "bad.elf"
        bad.write_bytes(b"NOTANELF" + b"\x00" * 64)
        with pytest.raises(ElfError):
            load_elf(bad)
