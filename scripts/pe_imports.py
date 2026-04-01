from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


class PEFormatError(RuntimeError):
    pass


@dataclass(frozen=True)
class Section:
    name: str
    virtual_address: int
    virtual_size: int
    raw_ptr: int
    raw_size: int


def _u16(b: bytes, o: int) -> int:
    return int.from_bytes(b[o : o + 2], "little", signed=False)


def _u32(b: bytes, o: int) -> int:
    return int.from_bytes(b[o : o + 4], "little", signed=False)


def _read_cstr(b: bytes, o: int) -> str:
    end = b.find(b"\x00", o)
    if end < 0:
        raise PEFormatError("unterminated string")
    return b[o:end].decode("ascii", errors="replace")


def _rva_to_file_offset(rva: int, sections: list[Section]) -> int | None:
    for s in sections:
        size = max(s.virtual_size, s.raw_size)
        if s.virtual_address <= rva < s.virtual_address + size:
            return s.raw_ptr + (rva - s.virtual_address)
    return None


def list_imported_dlls(path: str | Path) -> list[str]:
    p = Path(path)
    data = p.read_bytes()
    if len(data) < 0x100:
        raise PEFormatError("file too small")
    if data[0:2] != b"MZ":
        raise PEFormatError("missing MZ header")
    e_lfanew = _u32(data, 0x3C)
    if e_lfanew <= 0 or e_lfanew + 4 > len(data):
        raise PEFormatError("invalid e_lfanew")
    if data[e_lfanew : e_lfanew + 4] != b"PE\x00\x00":
        raise PEFormatError("missing PE signature")

    file_header_off = e_lfanew + 4
    number_of_sections = _u16(data, file_header_off + 2)
    size_of_optional_header = _u16(data, file_header_off + 16)
    optional_off = file_header_off + 20
    if optional_off + size_of_optional_header > len(data):
        raise PEFormatError("optional header out of range")

    magic = _u16(data, optional_off + 0)
    if magic not in (0x10B, 0x20B):
        raise PEFormatError(f"unsupported optional header magic: 0x{magic:x}")

    # Data directory offset differs between PE32 and PE32+.
    # PE32: data dir starts at optional_off + 96
    # PE32+: data dir starts at optional_off + 112
    data_dir_off = optional_off + (96 if magic == 0x10B else 112)
    if data_dir_off + 8 * 16 > optional_off + size_of_optional_header:
        raise PEFormatError("data directory out of range")

    # Import Table is entry 1.
    import_rva = _u32(data, data_dir_off + 8 * 1 + 0)
    import_size = _u32(data, data_dir_off + 8 * 1 + 4)
    if import_rva == 0 or import_size == 0:
        return []

    # Section headers follow optional header.
    sect_off = optional_off + size_of_optional_header
    sections: list[Section] = []
    for i in range(number_of_sections):
        o = sect_off + i * 40
        if o + 40 > len(data):
            raise PEFormatError("section header out of range")
        name = data[o : o + 8].split(b"\x00", 1)[0].decode("ascii", errors="replace")
        virtual_size = _u32(data, o + 8)
        virtual_address = _u32(data, o + 12)
        raw_size = _u32(data, o + 16)
        raw_ptr = _u32(data, o + 20)
        sections.append(
            Section(
                name=name,
                virtual_address=virtual_address,
                virtual_size=virtual_size,
                raw_ptr=raw_ptr,
                raw_size=raw_size,
            )
        )

    imp_off = _rva_to_file_offset(import_rva, sections)
    if imp_off is None:
        raise PEFormatError("import RVA not in any section")

    out: list[str] = []
    # IMAGE_IMPORT_DESCRIPTOR is 20 bytes; null descriptor terminates.
    idx = 0
    while True:
        o = imp_off + idx * 20
        if o + 20 > len(data):
            raise PEFormatError("import descriptor out of range")
        original_first_thunk = _u32(data, o + 0)
        time_date_stamp = _u32(data, o + 4)
        forwarder_chain = _u32(data, o + 8)
        name_rva = _u32(data, o + 12)
        first_thunk = _u32(data, o + 16)
        if (
            original_first_thunk == 0
            and time_date_stamp == 0
            and forwarder_chain == 0
            and name_rva == 0
            and first_thunk == 0
        ):
            break
        name_off = _rva_to_file_offset(name_rva, sections)
        if name_off is None:
            raise PEFormatError("import name RVA not in any section")
        dll = _read_cstr(data, name_off).strip()
        if dll:
            out.append(dll)
        idx += 1

    # Normalize and stable sort.
    norm = sorted({x.lower() for x in out})
    return norm


def iter_exes(paths: Iterable[Path]) -> list[Path]:
    out: list[Path] = []
    for p in paths:
        if not p.exists():
            continue
        if p.is_file() and p.suffix.lower() == ".exe":
            out.append(p)
        elif p.is_dir():
            out.extend(sorted(p.rglob("*.exe")))
    return out

