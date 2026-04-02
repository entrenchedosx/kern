"""Append-only logs under portable logs/."""

from __future__ import annotations

import threading
from datetime import datetime, timezone
from pathlib import Path

from .portable_env import ensure_portable_layout, logs_dir

_lock = threading.Lock()


def _line(msg: str) -> str:
    ts = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    return f"{ts} {msg}\n"


def append_log(name: str, message: str, *, portable_root: Path | None = None) -> None:
    root = ensure_portable_layout(portable_root)
    path = logs_dir(root) / name
    try:
        with _lock:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.open("a", encoding="utf-8").write(_line(message))
    except OSError:
        pass
