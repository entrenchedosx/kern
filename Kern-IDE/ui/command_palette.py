"""VS Code–style command palette: filterable list + keyboard navigation."""

from __future__ import annotations

from collections.abc import Callable
from tkinter import END, LEFT, RIGHT, BOTH, X, Y, Entry, Frame, Listbox, Scrollbar, Toplevel, VERTICAL

from app.theme import Theme


def show_command_palette(
    parent,
    theme: Theme,
    title: str,
    commands: list[tuple[str, Callable[[], None]]],
) -> None:
    """
    Show a modal-ish palette. `commands` is (label, callback); callback runs and window closes.
    """
    if not commands:
        return

    win = Toplevel(parent)
    win.title(title)
    win.transient(parent)
    win.attributes("-topmost", True)
    win.configure(bg=theme.panel)
    win.geometry("560x380")

    entry = Entry(
        win,
        font=("Segoe UI", 11),
        bg=theme.editor,
        fg=theme.text,
        insertbackground=theme.text,
        relief="flat",
    )
    entry.pack(fill=X, padx=10, pady=(10, 6))

    row = Frame(win, bg=theme.panel)
    row.pack(fill=BOTH, expand=True, padx=10, pady=(0, 10))

    lb = Listbox(
        row,
        font=("Segoe UI", 10),
        bg=theme.editor,
        fg=theme.text,
        selectbackground=theme.accent,
        selectforeground="#ffffff",
        relief="flat",
        highlightthickness=0,
        activestyle="none",
    )
    scroll = Scrollbar(row, orient=VERTICAL, command=lb.yview)
    lb.configure(yscrollcommand=scroll.set)
    lb.pack(side=LEFT, fill=BOTH, expand=True)
    scroll.pack(side=RIGHT, fill=Y)

    filtered: list[tuple[str, Callable[[], None]]] = list(commands)

    def refill(query: str) -> None:
        nonlocal filtered
        q = query.strip().lower()
        lb.delete(0, END)
        filtered = [(lab, fn) for lab, fn in commands if q in lab.lower()]
        for lab, _ in filtered:
            lb.insert(END, lab)
        if filtered:
            lb.selection_set(0)

    def run_selected() -> None:
        sel = lb.curselection()
        if not sel or not filtered:
            return
        _, fn = filtered[int(sel[0])]
        try:
            win.destroy()
        except Exception:
            pass
        fn()

    def on_up(_e: object = None) -> str:
        sel = lb.curselection()
        i = int(sel[0]) if sel else 0
        if i > 0:
            lb.selection_clear(0, END)
            lb.selection_set(i - 1)
            lb.activate(i - 1)
        return "break"

    def on_down(_e: object = None) -> str:
        sel = lb.curselection()
        i = int(sel[0]) if sel else -1
        if i < lb.size() - 1:
            lb.selection_clear(0, END)
            lb.selection_set(i + 1)
            lb.activate(i + 1)
        return "break"

    def on_return(_e: object = None) -> str:
        run_selected()
        return "break"

    def on_escape(_e: object = None) -> str:
        try:
            win.destroy()
        except Exception:
            pass
        return "break"

    entry.bind("<KeyRelease>", lambda _e: refill(entry.get()))
    win.bind("<Up>", on_up)
    win.bind("<Down>", on_down)
    win.bind("<Return>", on_return)
    win.bind("<Escape>", on_escape)
    lb.bind("<Double-Button-1>", lambda _e: run_selected())
    lb.bind("<Return>", on_return)

    refill("")
    entry.focus_set()
    win.grab_set()
    win.wait_window(win)
