#!/usr/bin/env python3
"""Smoke test: kern --config + --build-diagnostics-json produces valid JSON."""

from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path


def find_repo_root() -> Path:
    p = Path(__file__).resolve()
    # scripts/ -> kern-build-gui/ -> repo
    return p.parent.parent.parent


def find_kernc(repo: Path) -> Path | None:
    env = os.environ.get("KERNC_EXE", "").strip()
    if env:
        pe = Path(env)
        if pe.is_file():
            return pe
    for c in (
        repo / "build" / "Release" / "kern.exe",
        repo / "build" / "kern",
        repo / "build" / "Release" / "kern-impl.exe",
    ):
        if c.is_file():
            return c
    return None


def main() -> int:
    repo = find_repo_root()
    kern = find_kernc(repo)
    if not kern:
        print("SKIP: kern not found (set KERNC_EXE or build compiler)", file=sys.stderr)
        return 0

    # kern link step expects a real repo layout; cwd must be the Kern repository root.
    work = repo / "build" / "_kern_gui_smoke"
    work.mkdir(parents=True, exist_ok=True)
    entry = repo / "examples" / "05_functions.kn"
    if not entry.is_file():
        print("SKIP: examples/05_functions.kn missing", file=sys.stderr)
        return 0
    out_exe = work / "smoke_out.exe"
    diag = work / "diag.json"
    cfg = work / "kernconfig.json"
    cfg.write_text(
        json.dumps(
            {
                "entry": str(entry.resolve()).replace("\\", "/"),
                "output": str(out_exe.resolve()).replace("\\", "/"),
                "include_paths": [str(repo.resolve()).replace("\\", "/")],
                "release": True,
                "optimization": 0,
                "console": True,
            },
            indent=2,
        ),
        encoding="utf-8",
    )

    cmd = [
        str(kern),
        "--config",
        str(cfg),
        "--build-diagnostics-json",
        str(diag),
    ]
    r = subprocess.run(cmd, cwd=str(repo), capture_output=True, text=True)
    if r.returncode != 0:
        print("kern failed:", r.stdout, r.stderr, file=sys.stderr)
        return 1
    if not diag.is_file():
        print("missing diagnostics file:", diag, file=sys.stderr)
        return 1
    data = json.loads(diag.read_text(encoding="utf-8"))
    if not isinstance(data, list):
        print("diagnostics JSON is not an array", file=sys.stderr)
        return 1
    print("OK: diagnostics array length", len(data))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
