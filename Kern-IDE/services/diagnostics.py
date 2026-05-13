"""Normalize Kern `kern --check --json` output for the IDE problems list."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any


def normalize_kern_check_item(raw: dict[str, Any], default_file: str = "") -> dict[str, object]:
    """Map `filename` → `file` and ensure current buffer path when missing."""
    out: dict[str, object] = dict(raw)
    fp = str(out.get("file") or out.get("filename") or "").strip()
    if fp:
        out["file"] = fp
    elif default_file:
        out["file"] = default_file
    hint = str(out.get("hint", "") or "").strip()
    if hint:
        out["hint"] = hint
    return out


def parse_kern_check_output(
    stdout_text: str,
    *,
    default_file: str = "",
) -> tuple[list[dict[str, object]], str | None]:
    """
    Parse JSON from kern --check --json.
    Returns (items, error_message). error_message set if parse fails or output empty when errors expected.
    """
    text = (stdout_text or "").strip().lstrip("\ufeff")
    if not text:
        return [], "empty stdout from kern --check (is kern.exe built with --check support?)"

    data: dict[str, Any] | None = None
    try:
        parsed: Any = json.loads(text)
        data = parsed if isinstance(parsed, dict) else None
    except json.JSONDecodeError:
        i = text.find("{")
        j = text.rfind("}")
        if i >= 0 and j > i:
            try:
                parsed = json.loads(text[i : j + 1])
                data = parsed if isinstance(parsed, dict) else None
            except json.JSONDecodeError as exc:
                return [], f"invalid diagnostics JSON: {exc}"
        else:
            return [], "stdout is not JSON (first 200 chars): " + text[:200].replace("\n", " ")

    if data is None:
        return [], "diagnostics root is not a JSON object"

    items_raw = data.get("items", [])
    if not isinstance(items_raw, list):
        return [], "'items' is not a JSON array"

    out: list[dict[str, object]] = []
    for it in items_raw:
        if isinstance(it, dict):
            out.append(normalize_kern_check_item(it, default_file))
    return out, None


def format_problem_line(item: dict[str, object], *, fallback_file: str = "") -> str:
    kind = str(item.get("kind", "error")).upper()
    line = int(item.get("line", 0) or 0)
    col = int(item.get("column", 0) or 0)
    msg = str(item.get("message", ""))
    hint = str(item.get("hint", "") or "").strip()
    if hint:
        msg = f"{msg}  — {hint}"
    fp = str(item.get("file") or item.get("filename") or fallback_file or "").strip()
    name = Path(fp).name if fp else ""
    marker = "[!]" if kind in {"ERROR", "CRITICAL"} else "[~]" if kind in {"WARNING", "WARN"} else "[i]"
    if name:
        return f"{marker} {kind} {name} L{line}:{col} - {msg}"
    return f"{marker} {kind} L{line}:{col} - {msg}"
