"""Portable config/settings.json (Kern version selection, auto-update)."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from .portable_env import config_dir, ensure_portable_layout, settings_path


def load_settings(portable_root: Path | None = None) -> dict[str, Any]:
    root = ensure_portable_layout(portable_root)
    p = settings_path(root)
    if not p.exists():
        return {}
    try:
        raw = json.loads(p.read_text(encoding="utf-8"))
        return raw if isinstance(raw, dict) else {}
    except Exception:
        return {}


def save_settings(data: dict[str, Any], portable_root: Path | None = None) -> None:
    root = ensure_portable_layout(portable_root)
    config_dir(root).mkdir(parents=True, exist_ok=True)
    p = settings_path(root)
    try:
        p.write_text(json.dumps(data, indent=2), encoding="utf-8")
    except OSError:
        pass


def get_active_kern_tag(settings: dict[str, Any]) -> str | None:
    v = settings.get("active_kern_tag")
    return str(v).strip() if v else None


def set_active_kern_tag(portable_root: Path | None, tag: str | None) -> None:
    s = load_settings(portable_root)
    if tag:
        s["active_kern_tag"] = tag
    else:
        s.pop("active_kern_tag", None)
    save_settings(s, portable_root)
