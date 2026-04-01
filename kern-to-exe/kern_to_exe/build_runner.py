"""Run kernc from a Kern2ExeSpec (shared by GUI and headless CLI)."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

from kern_to_exe.kernc_locator import locate_kernc
from kern_to_exe.spec import Kern2ExeSpec


def apply_force_rebuild(spec: Kern2ExeSpec) -> None:
    outp = Path(spec.output)
    try:
        if outp.is_file():
            outp.unlink()
    except OSError:
        pass
    cache = outp.parent / ".kern-cache"
    if cache.is_dir():
        try:
            shutil.rmtree(cache)
        except OSError:
            pass


def run_kernc_build(spec: Kern2ExeSpec) -> int:
    """
    Write .kern-to-exe/kernconfig.json under project_root and invoke kernc.
    Returns process exit code. Prints stdout/stderr of kernc to this process's stdout.
    """
    kernc = locate_kernc()
    if not kernc:
        print("kernc not found — build the Kern toolchain or set KERNC_EXE.", file=sys.stderr)
        return 1
    errs = spec.validate()
    if errs:
        for e in errs:
            print(e, file=sys.stderr)
        return 1
    if spec.force_rebuild:
        apply_force_rebuild(spec)

    root = Path(spec.project_root)
    cfg_dir = root / ".kern-to-exe"
    cfg_path = cfg_dir / "kernconfig.json"
    try:
        spec.write_kernconfig(cfg_path)
    except OSError as e:
        print(f"Failed to write kernconfig: {e}", file=sys.stderr)
        return 1

    args: list[str] = [kernc, "--config", str(cfg_path)]
    diag = spec.diagnostics_json.strip()
    if diag:
        Path(diag).parent.mkdir(parents=True, exist_ok=True)
        args.extend(["--build-diagnostics-json", diag])
    if spec.machine_json:
        args.append("--json")

    env = os.environ.copy()
    if spec.kern_repo_root:
        env["KERN_REPO_ROOT"] = spec.kern_repo_root

    try:
        proc = subprocess.run(
            args,
            cwd=str(root),
            env=env,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        return int(proc.returncode)
    except OSError as e:
        print(f"Failed to run kernc: {e}", file=sys.stderr)
        return 1
