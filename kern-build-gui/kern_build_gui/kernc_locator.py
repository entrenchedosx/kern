"""Locate kernc.exe (Windows launcher) or kern binary; honor KERNC_EXE."""

from __future__ import annotations

import os
import sys
from pathlib import Path


def _repo_root_from_package() -> Path:
    # kern_build_gui/ -> kern-build-gui/ -> repo root
    return Path(__file__).resolve().parent.parent.parent


def _final_layout_kernc_paths(exe_dir: Path) -> list[Path]:
    """When frozen under FINAL/kern-gui/, toolchain is ../kern/kernc.exe (walk up to find FINAL)."""
    out: list[Path] = []
    cur = exe_dir.resolve()
    for _ in range(10):
        kern = cur / "kern"
        out.append(kern / "kernc.exe")
        parent = cur.parent
        if parent == cur:
            break
        cur = parent
    return out


def locate_kernc() -> str | None:
    env_override = os.environ.get("KERNC_EXE", "").strip()
    if env_override:
        p = Path(env_override).expanduser()
        if p.is_file():
            return str(p.resolve())

    script_dir = Path(__file__).resolve().parent
    exe_dir = Path(sys.executable).resolve().parent if getattr(sys, "frozen", False) else script_dir
    cwd = Path.cwd()
    repo = _repo_root_from_package()

    candidates = [
        *_final_layout_kernc_paths(exe_dir),
        exe_dir / "kernc.exe",
        exe_dir.parent / "kernc.exe",
        exe_dir.parent / "Release" / "kernc.exe",
        exe_dir.parent / "build" / "Release" / "kernc.exe",
        script_dir.parent.parent / "build" / "Release" / "kernc.exe",
        repo / "build" / "Release" / "kernc.exe",
        cwd / "build" / "Release" / "kernc.exe",
        repo / "build" / "kernc.exe",
        cwd / "build" / "kernc.exe",
    ]
    for c in candidates:
        if c.is_file():
            return str(c.resolve())
    return None
