from __future__ import annotations

import os
import subprocess
import sys
from collections.abc import Callable
from pathlib import Path
from typing import TYPE_CHECKING

from services.diagnostics import parse_kern_check_output
from services.process_runner import StreamingProcessRunner

if TYPE_CHECKING:
    pass


def _default_workspace_root() -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent
    return Path(__file__).resolve().parents[2]


def locate_dev_kern_exe() -> str | None:
    """Find kern.exe for development (repo layout, env override)."""
    override = os.environ.get("KERN_EXE", "").strip()
    if override and Path(override).exists():
        return str(Path(override).resolve())

    root = _default_workspace_root()
    candidates = [
        root / "build" / "Release" / "kern.exe",
        root / "shareable-ide" / "compiler" / "kern.exe",
        root / "compiler" / "kern.exe",
    ]
    exe_dir = Path(sys.executable).resolve().parent if getattr(sys, "frozen", False) else None
    if exe_dir:
        candidates.extend(
            [
                exe_dir / "compiler" / "kern.exe",
                exe_dir / "kern.exe",
                exe_dir.parent / "compiler" / "kern.exe",
            ]
        )
    for c in candidates:
        if c.exists():
            return str(c.resolve())
    return None


class KernRunner:
    def __init__(self, resolve_exe: Callable[[], str | None]) -> None:
        self._runner = StreamingProcessRunner()
        self._resolve_exe = resolve_exe

    @property
    def kern_exe(self) -> str | None:
        return self._resolve_exe()

    def is_running(self) -> bool:
        return self._runner.is_running()

    def stop(self) -> None:
        self._runner.stop()

    def run_script(
        self,
        script_path: Path,
        cwd: Path,
        on_output: Callable[[str], None],
        on_done: Callable[[int], None],
    ) -> bool:
        exe = self.kern_exe
        if not exe:
            on_output("kern.exe not found. Install a Kern version (Kern versions tab) or set KERN_EXE.\n")
            on_done(1)
            return False

        def _out(_stream: str, text: str) -> None:
            on_output(text)

        def _err(text: str) -> None:
            on_output(text)

        return self._runner.run(
            [exe, str(script_path)],
            cwd,
            on_output=_out,
            on_done=on_done,
            on_error=_err,
        )

    def check_script(self, script_path: Path, cwd: Path) -> tuple[list[dict[str, object]], str | None]:
        exe = self.kern_exe
        if not exe:
            return [], "kern.exe not found"
        try:
            proc = subprocess.run(
                [exe, "--check", "--json", str(script_path)],
                cwd=str(cwd),
                text=True,
                capture_output=True,
                encoding="utf-8",
                errors="replace",
            )
            stdout = proc.stdout or ""
            diagnostics, err = parse_kern_check_output(stdout, default_file=str(script_path))
            return diagnostics, err
        except Exception as exc:
            return [], f"check failed: {type(exc).__name__}: {exc}"
