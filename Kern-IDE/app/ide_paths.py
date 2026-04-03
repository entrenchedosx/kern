"""Path helpers for Kern IDE (file tabs, open/save). Kept small for unit testing."""

from __future__ import annotations

from pathlib import Path


def resolve_existing(path: Path) -> Path:
    """Best-effort absolute path; falls back to path as-is if resolve fails."""
    try:
        return path.expanduser().resolve()
    except (OSError, RuntimeError):
        try:
            return path.expanduser().absolute()
        except Exception:
            return path


def read_text_flexible(path: Path) -> tuple[str | None, str | None]:
    """
    Read file as UTF-8, or UTF-8 with replacement if needed.
    Returns (content, warning_message_or_None).
    """
    try:
        raw = path.read_bytes()
    except OSError as e:
        return None, str(e)
    try:
        return raw.decode("utf-8"), None
    except UnicodeDecodeError:
        return raw.decode("utf-8", errors="replace"), "Non-UTF-8 bytes were replaced (invalid UTF-8)."


def same_file_path(a: Path | None, b: Path | None) -> bool:
    """True if both exist and refer to the same file (resolved)."""
    if a is None or b is None:
        return False
    try:
        return resolve_existing(a) == resolve_existing(b)
    except Exception:
        return str(a) == str(b)
