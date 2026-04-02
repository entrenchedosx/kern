"""IDE release version (see VERSION at Kern-IDE root; bundled next to exe when frozen)."""

from __future__ import annotations

import sys
from pathlib import Path


def ide_version() -> str:
    candidates: list[Path] = []
    if getattr(sys, "frozen", False):
        candidates.append(Path(sys.executable).resolve().parent / "VERSION")
        meipass = getattr(sys, "_MEIPASS", None)
        if meipass:
            candidates.append(Path(meipass) / "VERSION")
    root = Path(__file__).resolve().parent.parent
    candidates.append(root / "VERSION")
    for p in candidates:
        try:
            t = p.read_text(encoding="utf-8").strip()
            if t:
                return t
        except OSError:
            continue
    return "dev"
