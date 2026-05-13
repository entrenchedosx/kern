from __future__ import annotations

from dataclasses import dataclass
from typing import Any


@dataclass(frozen=True)
class ThemeTokens:
    main: str
    panel: str
    text: str
    muted: str
    line: str
    btn: str
    hover: str


def tokens_for_mode(dark: bool) -> ThemeTokens:
    if dark:
        return ThemeTokens(
            main="#1e1e1e",
            panel="#252526",
            text="#d4d4d4",
            muted="#9e9e9e",
            line="#303338",
            btn="#2d2d30",
            hover="#35353a",
        )
    return ThemeTokens(
        main="#ffffff",
        panel="#f3f4f8",
        text="#1f2430",
        muted="#5b6270",
        line="#e2ebff",
        btn="#e5e8ef",
        hover="#dbe0ea",
    )


def recolor_tree(widget: Any, tokens: ThemeTokens) -> None:
    try:
        cls = widget.winfo_class()
        if cls in {"Frame", "Toplevel", "Panedwindow"}:
            widget.configure(bg=tokens.panel)
        elif cls == "Label":
            widget.configure(bg=tokens.panel, fg=tokens.text)
        elif cls in {"Text", "Listbox"}:
            widget.configure(bg=tokens.main, fg=tokens.text)
    except Exception:
        # some widgets reject bg/fg (e.g. themed ttk children)
        pass
    for c in widget.winfo_children():
        recolor_tree(c, tokens)
