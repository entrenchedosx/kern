"""Save/load GUI profiles (*.kernbuild.json)."""

from __future__ import annotations

import json
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any


PROFILE_VERSION = 1


@dataclass
class GuiProfile:
    version: int = PROFILE_VERSION
    project_root: str = ""
    files: list[str] = field(default_factory=list)
    entry: str = ""
    output: str = "dist/app.exe"
    release: bool = True
    optimization: int = 2
    console: bool = True

    def to_json_dict(self) -> dict[str, Any]:
        return {
            "version": self.version,
            "project_root": self.project_root,
            "files": self.files,
            "entry": self.entry,
            "output": self.output,
            "release": self.release,
            "optimization": self.optimization,
            "console": self.console,
        }

    @staticmethod
    def from_json_dict(d: dict[str, Any]) -> GuiProfile:
        return GuiProfile(
            version=int(d.get("version", PROFILE_VERSION)),
            project_root=str(d.get("project_root", "")),
            files=list(d.get("files", [])),
            entry=str(d.get("entry", "")),
            output=str(d.get("output", "dist/app.exe")),
            release=bool(d.get("release", True)),
            optimization=int(d.get("optimization", 2)),
            console=bool(d.get("console", True)),
        )


def save_profile(path: Path, profile: GuiProfile) -> None:
    path.write_text(json.dumps(profile.to_json_dict(), indent=2) + "\n", encoding="utf-8")


def load_profile(path: Path) -> GuiProfile:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise ValueError("Profile must be a JSON object")
    return GuiProfile.from_json_dict(data)
