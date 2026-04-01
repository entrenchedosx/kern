from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path
from tkinter import END, INSERT, RIGHT, VERTICAL, Y, Canvas, Frame, Listbox, Scrollbar, Text, Toplevel
from typing import Callable

from .theme import Theme


KEYWORDS = {
    "and", "as", "break", "case", "catch", "class", "const", "continue", "def", "do", "elif", "else",
    "enum", "export", "false", "finally", "fn", "for", "function", "if", "import", "in", "init", "lambda",
    "let", "match", "new", "nil", "not", "null", "or", "private", "protected", "public", "range", "repeat",
    "return", "rethrow", "super", "this", "throw", "true", "try", "var", "while", "with", "yield", "async",
    "await", "spawn", "extern", "struct", "unsafe",
}


@dataclass
class EditorDiagnostics:
    line: int
    column: int
    line_end: int
    column_end: int
    message: str


class EditorTab:
    def __init__(self, parent: Frame, theme: Theme, on_cursor_change: Callable[[], None]) -> None:
        self.file_path: Path | None = None
        self.is_dirty = False
        self._theme = theme
        self._on_cursor_change = on_cursor_change
        self._autocomplete: Toplevel | None = None
        self._autocomplete_list: Listbox | None = None
        self._diagnostics: list[EditorDiagnostics] = []

        self.container = Frame(parent, bg=theme.editor, highlightthickness=0, bd=0)
        self.line_canvas = Canvas(self.container, width=46, bg=theme.panel, highlightthickness=0)
        self.line_canvas.pack(side="left", fill="y")

        self.text = Text(
            self.container,
            undo=True,
            wrap="none",
            font=("Consolas", 11),
            bg=theme.editor,
            fg=theme.text,
            insertbackground=theme.text,
            selectbackground=theme.accent,
            relief="flat",
            padx=8,
            pady=8,
            tabs=("4c",),
        )
        self.v_scroll = Scrollbar(self.container, orient=VERTICAL, command=self._on_vscroll)
        self.text.configure(yscrollcommand=self.v_scroll.set)
        self.v_scroll.pack(side=RIGHT, fill=Y)
        self.text.pack(side="left", fill="both", expand=True)

        self._configure_tags()
        self._bind_events()
        self.refresh_line_numbers()
        self.highlight_syntax()

    def _configure_tags(self) -> None:
        darkish = self._theme.bg.startswith("#1") or self._theme.bg.startswith("#2")
        if darkish:
            kw = "#c586c0"
            st = "#ce9178"
            cm = "#6a9955"
            nm = "#b5cea8"
        else:
            kw = "#7f3fbf"
            st = "#a13f22"
            cm = "#2f7d32"
            nm = "#1f6b4f"
        self.text.tag_configure("kw", foreground=kw)
        self.text.tag_configure("string", foreground=st)
        self.text.tag_configure("comment", foreground=cm)
        self.text.tag_configure("number", foreground=nm)
        self.text.tag_configure("error", underline=True, foreground=self._theme.error)

    def _bind_events(self) -> None:
        self.text.bind("<KeyRelease>", self._on_key_release)
        self.text.bind("<ButtonRelease-1>", self._on_cursor_event)
        self.text.bind("<MouseWheel>", self._on_scroll_event)
        self.text.bind("<Return>", self._on_return)
        self.text.bind("<Control-space>", self._open_autocomplete)
        self.text.bind("<Tab>", self._autocomplete_accept_or_tab)
        self.text.bind("<Escape>", self._close_autocomplete)

    def set_theme(self, theme: Theme) -> None:
        self._theme = theme
        self.container.configure(bg=theme.editor)
        self.line_canvas.configure(bg=theme.panel)
        self.text.configure(
            bg=theme.editor,
            fg=theme.text,
            insertbackground=theme.text,
            selectbackground=theme.accent,
        )
        self._configure_tags()
        self.refresh_line_numbers()
        self.highlight_syntax()

    def _on_vscroll(self, *args: object) -> None:
        self.text.yview(*args)
        self.refresh_line_numbers()

    def _on_scroll_event(self, _event: object) -> None:
        self.refresh_line_numbers()

    def _on_cursor_event(self, _event: object) -> None:
        self.refresh_line_numbers()
        self._on_cursor_change()

    def _on_key_release(self, _event: object) -> None:
        self.is_dirty = True
        self.highlight_syntax()
        self.refresh_line_numbers()
        self._on_cursor_change()

    def _on_return(self, _event: object) -> str:
        line_start = self.text.index("insert linestart")
        line_text = self.text.get(line_start, "insert")
        indent = re.match(r"[ \t]*", line_text)
        prefix = indent.group(0) if indent else ""
        extra = "    " if line_text.rstrip().endswith("{") else ""
        self.text.insert(INSERT, "\n" + prefix + extra)
        return "break"

    def refresh_line_numbers(self) -> None:
        self.line_canvas.delete("all")
        start = self.text.index("@0,0")
        while True:
            dline = self.text.dlineinfo(start)
            if dline is None:
                break
            y = dline[1]
            line_no = start.split(".")[0]
            self.line_canvas.create_text(40, y, anchor="ne", text=line_no, fill=self._theme.line_number, font=("Consolas", 10))
            start = self.text.index(f"{start}+1line")

    def highlight_syntax(self) -> None:
        content = self.text.get("1.0", END)
        for tag in ("kw", "string", "comment", "number"):
            self.text.tag_remove(tag, "1.0", END)

        for match in re.finditer(r"//.*?$|#.*?$", content, flags=re.MULTILINE):
            self.text.tag_add("comment", self._idx(match.start()), self._idx(match.end()))
        for match in re.finditer(r"\"([^\"\\\\]|\\\\.)*\"|'([^'\\\\]|\\\\.)*'", content):
            self.text.tag_add("string", self._idx(match.start()), self._idx(match.end()))
        for match in re.finditer(r"\b\d+(\.\d+)?\b", content):
            self.text.tag_add("number", self._idx(match.start()), self._idx(match.end()))
        for match in re.finditer(r"\b[A-Za-z_][A-Za-z0-9_]*\b", content):
            tok = match.group(0)
            if tok in KEYWORDS:
                self.text.tag_add("kw", self._idx(match.start()), self._idx(match.end()))

        self._apply_diagnostic_tags()

    def _idx(self, offset: int) -> str:
        return f"1.0+{offset}c"

    def get_content(self) -> str:
        return self.text.get("1.0", END).rstrip("\n")

    def load_content(self, text: str, file_path: Path | None) -> None:
        self.text.delete("1.0", END)
        self.text.insert("1.0", text)
        self.file_path = file_path
        self.is_dirty = False
        self._diagnostics = []
        self.highlight_syntax()
        self.refresh_line_numbers()

    def apply_diagnostics(self, items: list[dict[str, object]]) -> None:
        self._diagnostics = []
        for item in items:
            line = int(item.get("line", 0) or 0)
            col = int(item.get("column", 0) or 0)
            line_end = int(item.get("lineEnd", line) or line)
            col_end = int(item.get("columnEnd", col + 1) or (col + 1))
            message = str(item.get("message", ""))
            if line > 0:
                self._diagnostics.append(EditorDiagnostics(line, max(1, col), max(line, line_end), max(1, col_end), message))
        self._apply_diagnostic_tags()

    def _apply_diagnostic_tags(self) -> None:
        self.text.tag_remove("error", "1.0", END)
        for d in self._diagnostics:
            start = f"{d.line}.{max(0, d.column - 1)}"
            end = f"{d.line_end}.{max(0, d.column_end - 1)}"
            try:
                self.text.tag_add("error", start, end)
            except Exception:
                continue

    def _open_autocomplete(self, _event: object = None) -> str:
        prefix = self._current_prefix()
        if not prefix:
            return "break"
        words = self._collect_words()
        candidates = sorted({w for w in words if w.startswith(prefix) and w != prefix})
        if not candidates:
            return "break"
        self._close_autocomplete()
        popup = Toplevel(self.text)
        popup.wm_overrideredirect(True)
        popup.configure(bg=self._theme.panel)
        lb = Listbox(
            popup,
            font=("Consolas", 10),
            bg=self._theme.editor,
            fg=self._theme.text,
            selectbackground=self._theme.accent,
            relief="flat",
            height=min(8, len(candidates)),
        )
        for c in candidates:
            lb.insert(END, c)
        lb.selection_set(0)
        lb.pack(fill="both", expand=True)
        bbox = self.text.bbox(INSERT)
        if bbox:
            x, y, _, h = bbox
            popup.geometry(f"+{self.text.winfo_rootx() + x}+{self.text.winfo_rooty() + y + h + 2}")
        self._autocomplete = popup
        self._autocomplete_list = lb
        lb.bind("<Return>", self._autocomplete_accept)
        lb.bind("<Double-Button-1>", self._autocomplete_accept)
        lb.bind("<Escape>", self._close_autocomplete)
        lb.focus_set()
        return "break"

    def _autocomplete_accept_or_tab(self, _event: object) -> str:
        if self._autocomplete and self._autocomplete_list:
            return self._autocomplete_accept(_event)
        self.text.insert(INSERT, "    ")
        return "break"

    def _autocomplete_accept(self, _event: object) -> str:
        if not self._autocomplete_list:
            return "break"
        sel = self._autocomplete_list.curselection()
        if not sel:
            return "break"
        value = self._autocomplete_list.get(sel[0])
        prefix = self._current_prefix()
        if prefix:
            self.text.delete(f"insert-{len(prefix)}c", INSERT)
        self.text.insert(INSERT, value)
        self._close_autocomplete()
        self.is_dirty = True
        self.highlight_syntax()
        return "break"

    def _close_autocomplete(self, _event: object = None) -> str:
        if self._autocomplete:
            self._autocomplete.destroy()
        self._autocomplete = None
        self._autocomplete_list = None
        self.text.focus_set()
        return "break"

    def _current_prefix(self) -> str:
        before = self.text.get("insert linestart", INSERT)
        match = re.search(r"([A-Za-z_][A-Za-z0-9_]*)$", before)
        return match.group(1) if match else ""

    def _collect_words(self) -> set[str]:
        words = set(KEYWORDS)
        for w in re.findall(r"\b[A-Za-z_][A-Za-z0-9_]*\b", self.text.get("1.0", END)):
            words.add(w)
        return words

