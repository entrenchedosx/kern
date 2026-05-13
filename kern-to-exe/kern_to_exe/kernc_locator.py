"""Locate kernc.exe (Kern standalone compiler). Honors KERNC_EXE, then common dev layouts."""

from __future__ import annotations

import os
import sys
from pathlib import Path


def _repo_root_from_package() -> Path:
    # kern_to_exe/ -> kern-to-exe/ -> repository root
    return Path(__file__).resolve().parent.parent.parent


def _final_layout_kernc_paths(exe_dir: Path) -> list[Path]:
    out: list[Path] = []
    cur = exe_dir.resolve()
    for _ in range(12):
        kern = cur / "kern"
        out.append(kern / "kernc.exe")
        parent = cur.parent
        if parent == cur:
            break
        cur = parent
    return out


def _kern_repo_root_paths() -> list[Path]:
    raw = os.environ.get("KERN_REPO_ROOT", "").strip()
    if not raw:
        return []
    root = Path(raw).expanduser().resolve()
    return [
        root / "build" / "Release" / "kernc.exe",
        root / "build" / "Debug" / "kernc.exe",
        root / "build" / "kernc.exe",
    ]


def _candidate_kernc_paths() -> list[Path]:
    """Ordered search list (may contain duplicates; locate_kernc dedupes)."""
    script_dir = Path(__file__).resolve().parent
    exe_dir = Path(sys.executable).resolve().parent if getattr(sys, "frozen", False) else script_dir
    cwd = Path.cwd()
    repo = _repo_root_from_package()

    return [
        *_kern_repo_root_paths(),
        *_final_layout_kernc_paths(exe_dir),
        exe_dir / "kernc.exe",
        exe_dir.parent / "kernc.exe",
        exe_dir.parent / "Release" / "kernc.exe",
        exe_dir.parent / "build" / "Release" / "kernc.exe",
        script_dir.parent.parent / "build" / "Release" / "kernc.exe",
        repo / "build" / "Release" / "kernc.exe",
        repo / "build" / "Debug" / "kernc.exe",
        cwd / "build" / "Release" / "kernc.exe",
        cwd / "build" / "Debug" / "kernc.exe",
        repo / "build" / "kernc.exe",
        cwd / "build" / "kernc.exe",
        # iDE / portable bundles checked into the same repo
        repo / "shareable-ide" / "compiler" / "kernc.exe",
        repo / "shareable-kern-to-exe" / "kernc.exe",
        repo / "kern-to-exe" / "compiler" / "kernc.exe",
    ]


def locate_kernc() -> str | None:
    env_override = os.environ.get("KERNC_EXE", "").strip()
    if env_override:
        p = Path(env_override).expanduser()
        if p.is_file():
            return str(p.resolve())

    seen: set[Path] = set()
    for c in _candidate_kernc_paths():
        try:
            resolved = c.resolve()
        except OSError:
            continue
        if resolved in seen:
            continue
        seen.add(resolved)
        if resolved.is_file():
            return str(resolved)
    return None


def kernc_probe_report() -> tuple[str | None, list[str]]:
    """
    Return (first_found_path_or_none, lines for UI).
    Mirrors locate_kernc search order (KERNC_EXE first when valid file).
    """
    lines: list[str] = []
    found: str | None = None

    env_override = os.environ.get("KERNC_EXE", "").strip()
    if env_override:
        ep = Path(env_override).expanduser()
        lines.append(f"KERNC_EXE → {ep}")
        if ep.is_file():
            found = str(ep.resolve())
            lines.append("  (active: environment override)")
            return found, lines
        lines.append("  (not a file — scanning defaults below)")

    seen: set[Path] = set()
    for c in _candidate_kernc_paths():
        try:
            r = c.resolve()
        except OSError:
            lines.append(f"  [err] {c}")
            continue
        if r in seen:
            continue
        seen.add(r)
        ok = r.is_file()
        if ok and found is None:
            found = str(r)
        lines.append(f"  [{'+' if ok else '·'}] {r}")

    return found, lines
