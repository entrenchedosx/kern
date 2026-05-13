"""Portable Kern IDE home: kern_versions/, config/, logs/, projects/."""

from __future__ import annotations

import os
import sys
from pathlib import Path

_LAYOUT = ("kern_versions", "config", "logs", "projects")


def portable_root() -> Path:
    override = os.environ.get("KERN_IDE_HOME", "").strip()
    if override:
        return Path(override).expanduser().resolve()
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent
    return (Path.home() / ".kern_ide").resolve()


def ensure_portable_layout(root: Path | None = None) -> Path:
    r = root or portable_root()
    for name in _LAYOUT:
        (r / name).mkdir(parents=True, exist_ok=True)
    return r


def kern_versions_dir(root: Path | None = None) -> Path:
    return (root or portable_root()) / "kern_versions"


def config_dir(root: Path | None = None) -> Path:
    return (root or portable_root()) / "config"


def logs_dir(root: Path | None = None) -> Path:
    return (root or portable_root()) / "logs"


def projects_dir(root: Path | None = None) -> Path:
    return (root or portable_root()) / "projects"


def settings_path(root: Path | None = None) -> Path:
    return config_dir(root) / "settings.json"
