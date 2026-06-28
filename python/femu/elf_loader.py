"""
Simple ELF32 loader for ARM firmware.

This module parses ELF32 files and extracts information needed
to load firmware into the emulator. No external dependencies required.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass
from pathlib import Path
from typing import BinaryIO

# ELF constants
ELF_MAGIC = b"\x7fELF"
ELFCLASS32 = 1
ELFDATA2LSB = 1  # Little endian
EM_ARM = 40  # ARM machine type
PT_LOAD = 1  # Loadable segment
PF_X = 0x1  # Executable
PF_W = 0x2  # Writable
PF_R = 0x4  # Readable


@dataclass
class ElfHeader:
    """ELF file header."""

    ei_class: int  # 32 or 64 bit
    ei_data: int  # Endianness
    e_type: int  # Object file type
    e_machine: int  # Architecture
    e_version: int  # Object file version
    e_entry: int  # Entry point
    e_phoff: int  # Program header offset
    e_shoff: int  # Section header offset
    e_flags: int  # Processor-specific flags
    e_ehsize: int  # ELF header size
    e_phentsize: int  # Program header entry size
    e_phnum: int  # Number of program headers
    e_shentsize: int  # Section header entry size
    e_shnum: int  # Number of section headers
    e_shstrndx: int  # Section name string table index


@dataclass
class ProgramHeader:
    """ELF program header (segment descriptor)."""

    p_type: int  # Segment type
    p_offset: int  # File offset
    p_vaddr: int  # Virtual address
    p_paddr: int  # Physical address
    p_filesz: int  # Size in file
    p_memsz: int  # Size in memory
    p_flags: int  # Segment flags
    p_align: int  # Alignment


@dataclass
class LoadSegment:
    """A loadable segment extracted from ELF."""

    vaddr: int  # Virtual (load) address
    paddr: int  # Physical address
    data: bytes  # Segment data
    memsz: int  # Total memory size (may be > len(data) for BSS)
    flags: int  # PF_* flags

    @property
    def is_executable(self) -> bool:
        """Check if segment is executable (code)."""
        return bool(self.flags & PF_X)

    @property
    def is_writable(self) -> bool:
        """Check if segment is writable (data/BSS)."""
        return bool(self.flags & PF_W)


@dataclass
class ElfInfo:
    """Parsed ELF file information."""

    entry_point: int  # Entry point address
    segments: list[LoadSegment]  # Loadable segments
    machine: int  # Machine type
    flags: int  # ELF flags

    @property
    def initial_sp(self) -> int | None:
        """
        Get initial stack pointer from vector table.

        For Cortex-M, the first word at the load address is the initial SP.
        """
        # Find lowest address segment (typically flash at 0x08000000 or 0x00000000)
        if not self.segments:
            return None

        lowest = min(self.segments, key=lambda s: s.vaddr)
        if len(lowest.data) >= 4:
            return int(struct.unpack("<I", lowest.data[0:4])[0])
        return None

    @property
    def reset_vector(self) -> int | None:
        """
        Get reset vector from vector table.

        For Cortex-M, the second word at the load address is the reset vector.
        """
        if not self.segments:
            return None

        lowest = min(self.segments, key=lambda s: s.vaddr)
        if len(lowest.data) >= 8:
            return int(struct.unpack("<I", lowest.data[4:8])[0])
        return None

    @property
    def flash_regions(self) -> list[tuple[int, int]]:
        """
        Get memory regions that should be mapped as flash (read-only executable).

        Returns:
            List of (base_addr, size) tuples
        """
        regions = []
        for seg in self.segments:
            if seg.is_executable and not seg.is_writable:
                regions.append((seg.vaddr, seg.memsz))
        return regions

    @property
    def ram_regions(self) -> list[tuple[int, int]]:
        """
        Get memory regions that should be mapped as RAM (writable).

        Returns:
            List of (base_addr, size) tuples
        """
        regions = []
        for seg in self.segments:
            if seg.is_writable:
                regions.append((seg.vaddr, seg.memsz))
        return regions


class ElfError(Exception):
    """Error parsing ELF file."""

    pass


def _read_elf_header(f: BinaryIO) -> ElfHeader:
    """Read and parse ELF header."""
    # Read e_ident (16 bytes)
    e_ident = f.read(16)
    if len(e_ident) < 16:
        raise ElfError("File too short to be an ELF file")

    if e_ident[0:4] != ELF_MAGIC:
        raise ElfError("Not an ELF file (invalid magic)")

    ei_class = e_ident[4]
    ei_data = e_ident[5]

    if ei_class != ELFCLASS32:
        raise ElfError(f"Not a 32-bit ELF file (class={ei_class})")

    if ei_data != ELFDATA2LSB:
        raise ElfError(f"Not little-endian (data={ei_data})")

    # Read rest of header (36 bytes for ELF32)
    header_data = f.read(36)
    if len(header_data) < 36:
        raise ElfError("Truncated ELF header")

    (
        e_type,
        e_machine,
        e_version,
        e_entry,
        e_phoff,
        e_shoff,
        e_flags,
        e_ehsize,
        e_phentsize,
        e_phnum,
        e_shentsize,
        e_shnum,
        e_shstrndx,
    ) = struct.unpack("<HHIIIIIHHHHHH", header_data)

    return ElfHeader(
        ei_class=ei_class,
        ei_data=ei_data,
        e_type=e_type,
        e_machine=e_machine,
        e_version=e_version,
        e_entry=e_entry,
        e_phoff=e_phoff,
        e_shoff=e_shoff,
        e_flags=e_flags,
        e_ehsize=e_ehsize,
        e_phentsize=e_phentsize,
        e_phnum=e_phnum,
        e_shentsize=e_shentsize,
        e_shnum=e_shnum,
        e_shstrndx=e_shstrndx,
    )


def _read_program_headers(f: BinaryIO, header: ElfHeader) -> list[ProgramHeader]:
    """Read program headers."""
    if header.e_phoff == 0:
        return []

    f.seek(header.e_phoff)
    headers = []

    for _ in range(header.e_phnum):
        data = f.read(header.e_phentsize)
        if len(data) < 32:  # Minimum program header size
            break

        (
            p_type,
            p_offset,
            p_vaddr,
            p_paddr,
            p_filesz,
            p_memsz,
            p_flags,
            p_align,
        ) = struct.unpack("<IIIIIIII", data[:32])

        headers.append(
            ProgramHeader(
                p_type=p_type,
                p_offset=p_offset,
                p_vaddr=p_vaddr,
                p_paddr=p_paddr,
                p_filesz=p_filesz,
                p_memsz=p_memsz,
                p_flags=p_flags,
                p_align=p_align,
            )
        )

    return headers


def _load_segments(f: BinaryIO, phdrs: list[ProgramHeader]) -> list[LoadSegment]:
    """Load PT_LOAD segments from file."""
    segments = []

    for phdr in phdrs:
        if phdr.p_type != PT_LOAD:
            continue

        # Read segment data
        f.seek(phdr.p_offset)
        data = f.read(phdr.p_filesz)

        if len(data) < phdr.p_filesz:
            raise ElfError(
                f"Truncated segment at offset {phdr.p_offset}: "
                f"expected {phdr.p_filesz}, got {len(data)}"
            )

        segments.append(
            LoadSegment(
                vaddr=phdr.p_vaddr,
                paddr=phdr.p_paddr,
                data=data,
                memsz=phdr.p_memsz,
                flags=phdr.p_flags,
            )
        )

    return segments


def load_elf(path: str | Path) -> ElfInfo:
    """
    Load and parse an ELF file.

    Args:
        path: Path to the ELF file

    Returns:
        ElfInfo with parsed file information

    Raises:
        ElfError: If the file is not a valid ARM ELF32 file
        FileNotFoundError: If the file doesn't exist
    """
    path = Path(path)

    with open(path, "rb") as f:
        header = _read_elf_header(f)

        # Verify ARM machine type
        if header.e_machine != EM_ARM:
            raise ElfError(
                f"Not an ARM ELF file (machine type {header.e_machine}, expected {EM_ARM})"
            )

        phdrs = _read_program_headers(f, header)
        segments = _load_segments(f, phdrs)

    return ElfInfo(
        entry_point=header.e_entry,
        segments=segments,
        machine=header.e_machine,
        flags=header.e_flags,
    )


def suggest_memory_config(elf: ElfInfo) -> dict[str, int]:
    """
    Suggest memory configuration based on ELF segments.

    Args:
        elf: Parsed ELF information

    Returns:
        Dictionary with suggested flash_base, flash_size, ram_base, ram_size
    """
    flash_base = None
    flash_end = 0
    ram_base = None
    ram_end = 0

    for seg in elf.segments:
        if seg.is_executable and not seg.is_writable:
            # Flash/ROM segment
            if flash_base is None or seg.vaddr < flash_base:
                flash_base = seg.vaddr
            seg_end = seg.vaddr + seg.memsz
            if seg_end > flash_end:
                flash_end = seg_end
        elif seg.is_writable:
            # RAM segment
            if ram_base is None or seg.vaddr < ram_base:
                ram_base = seg.vaddr
            seg_end = seg.vaddr + seg.memsz
            if seg_end > ram_end:
                ram_end = seg_end

    # Default values if not found in ELF
    if flash_base is None:
        flash_base = 0x08000000
        flash_end = flash_base + 0x80000  # 512KB

    if ram_base is None:
        ram_base = 0x20000000
        ram_end = ram_base + 0x20000  # 128KB

    # Check initial SP from vector table (first word in flash segment)
    # This ensures the stack area is covered by RAM
    for seg in elf.segments:
        if seg.is_executable and len(seg.data) >= 4:
            initial_sp = int.from_bytes(seg.data[:4], "little")
            # If initial SP is in RAM region, extend RAM to cover it
            if ram_base is not None and initial_sp > ram_base and initial_sp > ram_end:
                ram_end = initial_sp
            break

    # Round up sizes to power of 2 for convenience
    def round_up_power2(size: int, min_size: int = 0x1000) -> int:
        size = max(size, min_size)
        power = 1
        while power < size:
            power *= 2
        return power

    flash_size = round_up_power2(flash_end - flash_base)
    ram_size = round_up_power2(ram_end - ram_base)

    return {
        "flash_base": flash_base,
        "flash_size": flash_size,
        "ram_base": ram_base,
        "ram_size": ram_size,
    }
