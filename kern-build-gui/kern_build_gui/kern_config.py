"""Generate kernconfig.json content for kern."""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


@dataclass
class BuildSettings:
    entry: str
    output: str
    project_root: str
    files: list[str] = field(default_factory=list)
    release: bool = True
    optimization: int = 2
    console: bool = True

    def validate(self) -> list[str]:
        errs: list[str] = []
        if not self.entry.strip():
            errs.append("Entry file is required.")
        elif not Path(self.entry).is_file():
            errs.append(f"Entry file does not exist: {self.entry}")
        if not self.output.strip():
            errs.append("Output path is required.")
        if not self.project_root.strip():
            errs.append("Project root is required.")
        elif not Path(self.project_root).is_dir():
            errs.append(f"Project root is not a directory: {self.project_root}")
        if self.optimization < 0 or self.optimization > 3:
            errs.append("Optimization must be 0–3.")
        return errs


def build_kernconfig_dict(settings: BuildSettings) -> dict[str, Any]:
    entry_abs = str(Path(settings.entry).resolve())
    output_abs = str(Path(settings.output).resolve())
    root_abs = str(Path(settings.project_root).resolve())
    files_abs = [str(Path(f).resolve()) for f in settings.files if f.strip()]
    d: dict[str, Any] = {
        "entry": entry_abs.replace("\\", "/"),
        "output": output_abs.replace("\\", "/"),
        "include_paths": [root_abs.replace("\\", "/")],
        "release": settings.release,
        "optimization": settings.optimization,
        "console": settings.console,
    }
    if files_abs:
        d["files"] = [p.replace("\\", "/") for p in files_abs]
    return d


def write_kernconfig(path: Path, settings: BuildSettings) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    data = build_kernconfig_dict(settings)
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
