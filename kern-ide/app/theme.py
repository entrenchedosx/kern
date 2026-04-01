from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class Theme:
    bg: str
    panel: str
    editor: str
    editor_alt: str
    text: str
    muted: str
    border: str
    accent: str
    error: str
    button: str
    button_hover: str
    line_number: str
    status_bg: str


LIGHT = Theme(
    bg="#f6f7fb",
    panel="#ffffff",
    editor="#ffffff",
    editor_alt="#f8faff",
    text="#1f2430",
    muted="#697186",
    border="#d9dfeb",
    accent="#3b82f6",
    error="#d83a3a",
    button="#eef2ff",
    button_hover="#e0e7ff",
    line_number="#7b869f",
    status_bg="#eef2fa",
)


DARK = Theme(
    bg="#1e1f24",
    panel="#25272d",
    editor="#1b1d22",
    editor_alt="#20232b",
    text="#d7dce8",
    muted="#9aa3b8",
    border="#323744",
    accent="#64b5ff",
    error="#ff6b6b",
    button="#2b3140",
    button_hover="#353d4f",
    line_number="#77819a",
    status_bg="#20242d",
)


def resolve_theme(dark_mode: bool) -> Theme:
    return DARK if dark_mode else LIGHT

