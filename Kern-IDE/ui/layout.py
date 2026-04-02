from __future__ import annotations

from dataclasses import dataclass
from typing import Any

TIPS_TEXT = (
    "Tips\n"
    "- Ctrl+N / Ctrl+O: New / Open   Ctrl+S: Save   Ctrl+P: Quick open\n"
    "- Ctrl+Space: Rich suggestions (imports, stdlib members, locals, snippets)\n"
    "- Ctrl+click: Go to import target (opens .kn or built-in reference)\n"
    "- Ctrl+F / Ctrl+H: Find & replace   Ctrl+Shift+E: Find in workspace\n"
    "- Ctrl+R / F5: Run   Ctrl+B: Build .exe   Ctrl+K: Check\n"
    "- F11: Zen (focus editor)   Alt+Z: Word wrap   Ctrl+Shift+↑/↓: Move line\n"
    "- Double-click a problem to jump. Edit → Preferences: trim whitespace on save."
)

SHORTCUT_BINDINGS = (
    ("<Control-n>", "_on_shortcut_new_file"),
    ("<Control-p>", "_on_shortcut_quick_open"),
    ("<Control-s>", "_on_shortcut_save"),
    ("<Control-o>", "_on_shortcut_open"),
    ("<Control-r>", "_on_shortcut_run"),
    ("<Control-b>", "_on_shortcut_compile"),
    ("<Control-k>", "_on_shortcut_check"),
    ("<Control-K>", "_on_shortcut_fix_all"),
    ("<F5>", "_on_shortcut_run"),
    ("<Shift-F5>", "_on_shortcut_stop"),
    ("<F8>", "_on_shortcut_toggle_output"),
    ("<Control-Shift-D>", "_on_shortcut_toggle_density"),
    ("<Control-Shift-E>", "_on_shortcut_find_workspace"),
    ("<Control-Shift-F>", "_on_shortcut_toggle_sidebar"),
    ("<Control-Shift-T>", "_on_shortcut_toggle_theme"),
    ("<F11>", "_on_shortcut_zen"),
    ("<Alt-z>", "_on_shortcut_word_wrap"),
    ("<Escape>", "_on_shortcut_clear_output"),
)


@dataclass(frozen=True)
class PaneLayoutState:
    sidebar_visible: bool
    output_visible: bool
    assistant_visible: bool
    sidebar_width: int
    output_height: int


def safe_forget(pane: Any, child: Any) -> None:
    try:
        pane.forget(child)
    except Exception:
        pass


def safe_add(pane: Any, child: Any, **kwargs: object) -> None:
    try:
        pane.add(child, **kwargs)
    except Exception:
        pass


def apply_main_layout(
    content_pane: Any,
    center_pane: Any,
    sidebar: Any,
    assistant_panel: Any,
    console_frame: Any,
    state: PaneLayoutState,
) -> None:
    """Apply a deterministic pane layout order and visibility."""
    safe_forget(content_pane, sidebar)
    safe_forget(content_pane, center_pane)
    safe_forget(content_pane, assistant_panel)
    safe_forget(center_pane, console_frame)

    if state.sidebar_visible:
        safe_add(content_pane, sidebar, minsize=170, width=max(170, int(state.sidebar_width)))
    safe_add(content_pane, center_pane, minsize=520)
    if state.assistant_visible:
        safe_add(content_pane, assistant_panel, minsize=220, width=280)
    if state.output_visible:
        safe_add(center_pane, console_frame, minsize=120, height=max(120, int(state.output_height)))
