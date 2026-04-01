"""Build specification → kernconfig.json subset for kernc."""

from __future__ import annotations

import json
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any


@dataclass
class Kern2ExeSpec:
    """User recipe / saved UI state for a build."""

    entry: str = ""
    output: str = ""
    project_root: str = ""
    extra_kn_files: list[str] = field(default_factory=list)
    assets: list[str] = field(default_factory=list)
    icon: str = ""
    release: bool = True
    opt: int = 2
    console: bool = True
    force_rebuild: bool = False
    kern_repo_root: str = ""
    machine_json: bool = False
    pre_build: list[str] = field(default_factory=list)
    post_build: list[str] = field(default_factory=list)
    diagnostics_json: str = ""

    def validate(self) -> list[str]:
        errs: list[str] = []
        if not (self.entry or "").strip():
            errs.append("Script / entry .kn file is required.")
        elif not Path(self.entry).is_file():
            errs.append(f"Entry file not found: {self.entry}")
        if not (self.output or "").strip():
            errs.append("Output .exe path is required.")
        root = (self.project_root or "").strip()
        if not root:
            errs.append("Project root directory is required.")
        elif not Path(root).is_dir():
            errs.append(f"Project root is not a directory: {root}")
        if self.opt < 0 or self.opt > 3:
            errs.append("Optimization must be 0–3.")
        for p in self.extra_kn_files:
            if p.strip() and not Path(p).is_file():
                errs.append(f"Extra module not found: {p}")
        for p in self.assets:
            if p.strip() and not Path(p).exists():
                errs.append(f"Asset path not found: {p}")
        ic = (self.icon or "").strip()
        if ic and not Path(ic).is_file():
            errs.append(f"Icon file not found: {ic}")
        return errs

    def all_kn_files(self) -> list[str]:
        """Union of entry + extra modules for explicit `files` in kernconfig."""
        seen: set[str] = set()
        out: list[str] = []
        for p in [self.entry, *self.extra_kn_files]:
            s = (p or "").strip()
            if not s or s in seen:
                continue
            seen.add(s)
            out.append(str(Path(s).resolve()))
        return out

    def to_kernconfig_dict(self) -> dict[str, Any]:
        entry_abs = str(Path(self.entry).resolve())
        output_abs = str(Path(self.output).resolve())
        root_abs = str(Path(self.project_root).resolve())
        files_abs = self.all_kn_files()
        d: dict[str, Any] = {
            "entry": entry_abs.replace("\\", "/"),
            "output": output_abs.replace("\\", "/"),
            "include_paths": [root_abs.replace("\\", "/")],
            "release": self.release,
            "optimization": self.opt,
            "console": self.console,
        }
        if self.extra_kn_files:
            d["files"] = [p.replace("\\", "/") for p in files_abs]
        assets_abs = [str(Path(p).resolve()) for p in self.assets if (p or "").strip()]
        if assets_abs:
            d["assets"] = [p.replace("\\", "/") for p in assets_abs]
        ic = (self.icon or "").strip()
        if ic:
            d["icon"] = str(Path(ic).resolve()).replace("\\", "/")
        if self.pre_build:
            d["plugins_pre_build"] = list(self.pre_build)
        if self.post_build:
            d["plugins_post_build"] = list(self.post_build)
        return d

    def write_kernconfig(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(self.to_kernconfig_dict(), indent=2) + "\n", encoding="utf-8")

    def to_recipe_dict(self) -> dict[str, Any]:
        d = asdict(self)
        d["version"] = 1
        return d

    @staticmethod
    def from_recipe_dict(data: dict[str, Any]) -> Kern2ExeSpec:
        if int(data.get("version", 1)) != 1:
            raise ValueError("Unsupported recipe version")
        return Kern2ExeSpec(
            entry=str(data.get("entry", "")),
            output=str(data.get("output", "")),
            project_root=str(data.get("project_root", "")),
            extra_kn_files=list(data.get("extra_kn_files", [])),
            assets=list(data.get("assets", [])),
            icon=str(data.get("icon", "")),
            release=bool(data.get("release", True)),
            opt=int(data.get("opt", 2)),
            console=bool(data.get("console", True)),
            force_rebuild=bool(data.get("force_rebuild", False)),
            kern_repo_root=str(data.get("kern_repo_root", "")),
            machine_json=bool(data.get("machine_json", False)),
            pre_build=list(data.get("pre_build", [])),
            post_build=list(data.get("post_build", [])),
            diagnostics_json=str(data.get("diagnostics_json", "")),
        )


def load_recipe(path: Path) -> Kern2ExeSpec:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise ValueError("Recipe must be a JSON object")
    return Kern2ExeSpec.from_recipe_dict(data)


def save_recipe(path: Path, spec: Kern2ExeSpec) -> None:
    path.write_text(json.dumps(spec.to_recipe_dict(), indent=2) + "\n", encoding="utf-8")
