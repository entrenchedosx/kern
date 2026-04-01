from __future__ import annotations

import os
import threading
from pathlib import Path
from tkinter import BOTH, BOTTOM, END, HORIZONTAL, LEFT, RIGHT, TOP, VERTICAL, X, Y, BooleanVar, Menu, Text, Tk, filedialog, messagebox, simpledialog
from tkinter import ttk

from .editor import EditorTab
from .filesystem import FileExplorer
from .runner import KernRunner
from .state import load_state, save_state
from .theme import Theme, resolve_theme


def default_workspace_root() -> Path:
    if getattr(__import__("sys"), "frozen", False):
        import sys

        return Path(sys.executable).resolve().parent
    return Path(__file__).resolve().parents[2]


class KernIDE:
    def __init__(self, root: Tk) -> None:
        self.root = root
        self.root.title("Kern IDE")
        self.root.geometry("1200x760")
        self.root.minsize(920, 560)

        self.workspace_root = default_workspace_root()
        self.state_path = self.workspace_root / ".kern-ide-state.json"
        self.state = load_state(self.state_path)

        self.dark_mode = bool(self.state.get("dark_mode", True))
        self.show_explorer = BooleanVar(value=bool(self.state.get("show_explorer", True)))
        self.show_console = BooleanVar(value=bool(self.state.get("show_console", True)))
        self.show_debug = BooleanVar(value=bool(self.state.get("show_debug", False)))

        self.theme: Theme = resolve_theme(self.dark_mode)
        self.runner = KernRunner()
        self.editors: dict[str, EditorTab] = {}
        self.untitled_count = 1

        self._build_styles()
        self._build_menu()
        self._build_ui()
        self._bind_keys()
        self._apply_theme()
        self._update_layout()
        self.explorer.refresh()
        self.new_file()

        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    def _build_styles(self) -> None:
        self.style = ttk.Style(self.root)
        try:
            self.style.theme_use("clam")
        except Exception:
            pass

    def _build_menu(self) -> None:
        menubar = Menu(self.root)

        file_menu = Menu(menubar, tearoff=0)
        file_menu.add_command(label="New", command=self.new_file, accelerator="Ctrl+N")
        file_menu.add_command(label="Open...", command=self.open_file, accelerator="Ctrl+O")
        file_menu.add_separator()
        file_menu.add_command(label="Save", command=self.save_current, accelerator="Ctrl+S")
        file_menu.add_command(label="Save As...", command=self.save_current_as)
        file_menu.add_separator()
        file_menu.add_command(label="Exit", command=self._on_close)
        menubar.add_cascade(label="File", menu=file_menu)

        edit_menu = Menu(menubar, tearoff=0)
        edit_menu.add_command(label="Undo", command=lambda: self._current_editor().text.event_generate("<<Undo>>"))
        edit_menu.add_command(label="Redo", command=lambda: self._current_editor().text.event_generate("<<Redo>>"))
        edit_menu.add_separator()
        edit_menu.add_command(label="Find", command=self._open_find_dialog, accelerator="Ctrl+F")
        edit_menu.add_command(label="Autocomplete", command=lambda: self._current_editor()._open_autocomplete(), accelerator="Ctrl+Space")
        menubar.add_cascade(label="Edit", menu=edit_menu)

        run_menu = Menu(menubar, tearoff=0)
        run_menu.add_command(label="Run", command=self.run_current, accelerator="F5")
        run_menu.add_command(label="Check", command=self.check_current)
        run_menu.add_command(label="Stop", command=self.stop_run, accelerator="Shift+F5")
        run_menu.add_separator()
        run_menu.add_command(label="Clear Output", command=self.clear_output)
        menubar.add_cascade(label="Run", menu=run_menu)

        view_menu = Menu(menubar, tearoff=0)
        view_menu.add_checkbutton(label="Show file explorer", variable=self.show_explorer, command=self._update_layout)
        view_menu.add_checkbutton(label="Show console", variable=self.show_console, command=self._update_layout)
        view_menu.add_checkbutton(label="Show debugger panel", variable=self.show_debug, command=self._update_layout)
        view_menu.add_separator()
        view_menu.add_command(label="Toggle light/dark mode", command=self.toggle_theme)
        menubar.add_cascade(label="View", menu=view_menu)

        help_menu = Menu(menubar, tearoff=0)
        help_menu.add_command(label="About Kern IDE", command=self._show_about)
        menubar.add_cascade(label="Help", menu=help_menu)
        self.root.config(menu=menubar)

    def _build_ui(self) -> None:
        self.toolbar = ttk.Frame(self.root)
        self.toolbar.pack(side=TOP, fill=X, padx=8, pady=(8, 4))
        self.app_title = ttk.Label(self.toolbar, text="Kern IDE", anchor="w")
        self.app_title.pack(side=LEFT, padx=(0, 12))
        ttk.Button(self.toolbar, text="Run", command=self.run_current).pack(side=LEFT, padx=(0, 6))
        ttk.Button(self.toolbar, text="Check", command=self.check_current).pack(side=LEFT, padx=(0, 6))
        ttk.Button(self.toolbar, text="Stop", command=self.stop_run).pack(side=LEFT, padx=(0, 10))
        ttk.Button(self.toolbar, text="Clear output", command=self.clear_output).pack(side=LEFT)

        self.main_vertical = ttk.Panedwindow(self.root, orient=VERTICAL)
        self.main_vertical.pack(side=TOP, fill=BOTH, expand=True, padx=8, pady=(0, 6))

        self.top_horizontal = ttk.Panedwindow(self.main_vertical, orient=HORIZONTAL)
        self.main_vertical.add(self.top_horizontal, weight=5)

        self.explorer_frame = ttk.Frame(self.top_horizontal, width=240)
        self.top_horizontal.add(self.explorer_frame, weight=1)
        self.explorer_label = ttk.Label(self.explorer_frame, text="Files", anchor="w")
        self.explorer_label.pack(fill=X, padx=8, pady=(8, 4))
        self.tree = ttk.Treeview(self.explorer_frame, show="tree")
        self.tree.pack(fill=BOTH, expand=True, padx=8, pady=(0, 8))
        self.tree.bind("<Double-1>", self._on_tree_open)
        self.explorer = FileExplorer(self.tree, self.workspace_root)

        self.editor_frame = ttk.Frame(self.top_horizontal)
        self.top_horizontal.add(self.editor_frame, weight=6)
        self.tabs = ttk.Notebook(self.editor_frame)
        self.tabs.pack(fill=BOTH, expand=True)
        self.tabs.bind("<<NotebookTabChanged>>", lambda _e: self._refresh_status())

        self.debug_frame = ttk.Frame(self.top_horizontal, width=240)
        self.debug_label = ttk.Label(self.debug_frame, text="Debugger", anchor="w")
        self.debug_label.pack(fill=X, padx=6, pady=(8, 4))
        self.debug_text = Text(self.debug_frame, height=8, wrap="word", relief="flat")
        self.debug_text.pack(fill=BOTH, expand=True, padx=6, pady=(0, 6))
        self.debug_text.insert("1.0", "Debugger panel\n\nVariables and stack info will appear here while running.")
        self.debug_text.configure(state="disabled")

        self.console_frame = ttk.Frame(self.main_vertical, height=180)
        self.main_vertical.add(self.console_frame, weight=1)
        self.console_label = ttk.Label(self.console_frame, text="Console output", anchor="w")
        self.console_label.pack(fill=X, padx=8, pady=(6, 2))
        self.console = Text(self.console_frame, height=10, wrap="word", relief="flat")
        self.console.pack(fill=BOTH, expand=True, padx=8, pady=(0, 8))
        self.console.configure(state="disabled")

        self.status = ttk.Label(self.root, anchor="w")
        self.status.pack(side=BOTTOM, fill=X, padx=8, pady=(0, 6))

    def _bind_keys(self) -> None:
        self.root.bind("<Control-n>", lambda e: (self.new_file(), "break")[1])
        self.root.bind("<Control-o>", lambda e: (self.open_file(), "break")[1])
        self.root.bind("<Control-s>", lambda e: (self.save_current(), "break")[1])
        self.root.bind("<F5>", lambda e: (self.run_current(), "break")[1])
        self.root.bind("<Shift-F5>", lambda e: (self.stop_run(), "break")[1])

    def _apply_theme(self) -> None:
        t = self.theme
        self.root.configure(bg=t.bg)
        self.style.configure(".", background=t.panel, foreground=t.text)
        self.style.configure("TFrame", background=t.panel)
        self.style.configure("TLabel", background=t.panel, foreground=t.text)
        self.style.configure("Title.TLabel", background=t.panel, foreground=t.text, font=("Segoe UI Semibold", 11))
        self.style.configure("Section.TLabel", background=t.panel, foreground=t.muted, font=("Segoe UI", 9))
        self.app_title.configure(style="Title.TLabel")
        self.explorer_label.configure(style="Section.TLabel")
        self.console_label.configure(style="Section.TLabel")
        self.debug_label.configure(style="Section.TLabel")
        self.style.configure("TButton", background=t.button, foreground=t.text, padding=(10, 6))
        self.style.map("TButton", background=[("active", t.button_hover)])
        self.style.configure("Treeview", background=t.editor, fieldbackground=t.editor, foreground=t.text)
        self.style.configure("Treeview.Heading", background=t.panel, foreground=t.text)
        self.style.configure("Treeview", rowheight=22)
        self.style.configure("TNotebook", background=t.panel)
        self.style.configure("TNotebook.Tab", padding=(14, 7))
        self.style.map(
            "TNotebook.Tab",
            background=[("selected", t.editor_alt)],
            foreground=[("selected", t.text)],
        )

        self.console.configure(bg=t.editor, fg=t.text, insertbackground=t.text)
        self.debug_text.configure(bg=t.editor, fg=t.text, insertbackground=t.text)
        self.status.configure(background=t.status_bg, foreground=t.text, padding=(8, 4))
        for ed in self.editors.values():
            ed.set_theme(t)
        self._refresh_status()

    def _update_layout(self) -> None:
        panes = self.top_horizontal.panes()
        exp = str(self.explorer_frame)
        dbg = str(self.debug_frame)
        if self.show_explorer.get() and exp not in panes:
            self.top_horizontal.insert(0, self.explorer_frame, weight=1)
        if not self.show_explorer.get() and exp in panes:
            self.top_horizontal.forget(self.explorer_frame)

        panes = self.top_horizontal.panes()
        if self.show_debug.get() and dbg not in panes:
            self.top_horizontal.add(self.debug_frame, weight=1)
        if not self.show_debug.get() and dbg in panes:
            self.top_horizontal.forget(self.debug_frame)

        v_panes = self.main_vertical.panes()
        con = str(self.console_frame)
        if self.show_console.get() and con not in v_panes:
            self.main_vertical.add(self.console_frame, weight=1)
        if not self.show_console.get() and con in v_panes:
            self.main_vertical.forget(self.console_frame)

    def toggle_theme(self) -> None:
        self.dark_mode = not self.dark_mode
        self.theme = resolve_theme(self.dark_mode)
        self._apply_theme()

    def _new_editor_tab(self, title: str, content: str = "", file_path: Path | None = None) -> EditorTab:
        container = ttk.Frame(self.tabs)
        editor = EditorTab(container, self.theme, on_cursor_change=self._refresh_status)
        editor.container.pack(fill=BOTH, expand=True)
        editor.load_content(content, file_path)
        self.tabs.add(container, text=title)
        tab_id = str(container)
        self.editors[tab_id] = editor
        self.tabs.select(container)
        self._refresh_status()
        return editor

    def _current_editor(self) -> EditorTab:
        tab_id = self.tabs.select()
        return self.editors[tab_id]

    def _title_for_editor(self, editor: EditorTab) -> str:
        if editor.file_path:
            return editor.file_path.name + (" *" if editor.is_dirty else "")
        return f"untitled-{self.untitled_count}" + (" *" if editor.is_dirty else "")

    def _refresh_tab_title(self, editor: EditorTab) -> None:
        for tab_id, ed in self.editors.items():
            if ed is editor:
                self.tabs.tab(tab_id, text=self._title_for_editor(editor))
                return

    def new_file(self) -> None:
        self._new_editor_tab(f"untitled-{self.untitled_count}", "")
        self.untitled_count += 1

    def open_file(self) -> None:
        path = filedialog.askopenfilename(
            title="Open Kern file",
            initialdir=str(self.workspace_root),
            filetypes=[("Kern files", "*.kn"), ("All files", "*.*")],
        )
        if not path:
            return
        self._open_path(Path(path))

    def _open_path(self, path: Path) -> None:
        try:
            content = path.read_text(encoding="utf-8")
        except Exception as exc:
            messagebox.showerror("Open failed", f"Could not open file:\n{path}\n\n{exc}")
            return
        self._new_editor_tab(path.name, content, path)

    def save_current(self) -> None:
        editor = self._current_editor()
        if editor.file_path is None:
            self.save_current_as()
            return
        self._save_editor(editor, editor.file_path)

    def save_current_as(self) -> None:
        editor = self._current_editor()
        path = filedialog.asksaveasfilename(
            title="Save Kern file",
            initialdir=str(self.workspace_root),
            defaultextension=".kn",
            filetypes=[("Kern files", "*.kn"), ("All files", "*.*")],
        )
        if not path:
            return
        self._save_editor(editor, Path(path))

    def _save_editor(self, editor: EditorTab, path: Path) -> None:
        try:
            path.write_text(editor.get_content(), encoding="utf-8")
        except Exception as exc:
            messagebox.showerror("Save failed", f"Could not save file:\n{path}\n\n{exc}")
            return
        editor.file_path = path
        editor.is_dirty = False
        self._refresh_tab_title(editor)
        self.explorer.refresh()
        self._refresh_status()

    def run_current(self) -> None:
        editor = self._current_editor()
        if editor.file_path is None:
            self.save_current_as()
            if editor.file_path is None:
                return
        self.save_current()
        self.clear_output()
        self._append_output(f"running {editor.file_path}\n\n")

        def done(code: int) -> None:
            self.root.after(0, lambda: self._append_output(f"\nprocess exited with code {code}\n"))
            self.root.after(0, self.check_current)

        started = self.runner.run_script(
            editor.file_path,
            self.workspace_root,
            on_output=lambda t: self.root.after(0, lambda: self._append_output(t)),
            on_done=done,
        )
        if not started:
            self._append_output("another run is already active\n")

    def stop_run(self) -> None:
        self.runner.stop()
        self._append_output("run stopped\n")

    def check_current(self) -> None:
        editor = self._current_editor()
        if editor.file_path is None:
            return
        diagnostics, err = self.runner.check_script(editor.file_path, self.workspace_root)
        if err:
            self._append_output(f"check failed: {err}\n")
            editor.apply_diagnostics([])
            return
        editor.apply_diagnostics(diagnostics)
        err_count = len([d for d in diagnostics if str(d.get("kind", "error")).lower() in {"error", "critical"}])
        if diagnostics:
            self._append_output(f"check completed: {len(diagnostics)} issue(s), {err_count} error(s)\n")
        else:
            self._append_output("check completed: no issues\n")

    def clear_output(self) -> None:
        self.console.configure(state="normal")
        self.console.delete("1.0", END)
        self.console.configure(state="disabled")

    def _append_output(self, text: str) -> None:
        self.console.configure(state="normal")
        self.console.insert(END, text)
        self.console.see(END)
        self.console.configure(state="disabled")

    def _on_tree_open(self, _event: object) -> None:
        selection = self.tree.selection()
        if not selection:
            return
        node = selection[0]
        path = self.explorer.path_for_node(node)
        if not path or not path.is_file():
            return
        self._open_path(path)

    def _open_find_dialog(self) -> None:
        editor = self._current_editor()
        needle = simpledialog.askstring("Find", "Find text:")
        if not needle:
            return
        editor.text.tag_remove("sel", "1.0", END)
        start = editor.text.search(needle, "1.0", nocase=True, stopindex=END)
        if not start:
            messagebox.showinfo("Find", f"'{needle}' not found")
            return
        end = f"{start}+{len(needle)}c"
        editor.text.tag_add("sel", start, end)
        editor.text.mark_set("insert", end)
        editor.text.see(start)
        editor.text.focus_set()

    def _refresh_status(self) -> None:
        if not self.editors:
            self.status.configure(text=f"workspace: {self.workspace_root}")
            return
        editor = self._current_editor()
        idx = editor.text.index("insert")
        line, col = idx.split(".")
        file_name = str(editor.file_path.name) if editor.file_path else "untitled"
        self._refresh_tab_title(editor)
        self.status.configure(text=f"{file_name}    line {line}, col {int(col) + 1}    root: {self.workspace_root}")
        if self.show_debug.get():
            self.debug_text.configure(state="normal")
            self.debug_text.delete("1.0", END)
            self.debug_text.insert(
                "1.0",
                "Debugger panel\n\n"
                f"file: {file_name}\n"
                f"cursor: line {line}, col {int(col) + 1}\n"
                f"dirty: {'yes' if editor.is_dirty else 'no'}\n"
                f"kern.exe: {self.runner.kern_exe or 'not found'}\n",
            )
            self.debug_text.configure(state="disabled")

    def _show_about(self) -> None:
        messagebox.showinfo(
            "About Kern IDE",
            "Kern IDE\n\nMinimal, beginner-friendly editor for Kern.\n\nShortcuts:\n- F5 run\n- Shift+F5 stop\n- Ctrl+N new file\n- Ctrl+O open file\n- Ctrl+S save",
        )

    def _on_close(self) -> None:
        unsaved = [ed for ed in self.editors.values() if ed.is_dirty]
        if unsaved:
            ok = messagebox.askyesno("Unsaved changes", "You have unsaved files. Close anyway?")
            if not ok:
                return
        save_state(
            self.state_path,
            {
                "dark_mode": self.dark_mode,
                "show_explorer": self.show_explorer.get(),
                "show_console": self.show_console.get(),
                "show_debug": self.show_debug.get(),
                "workspace_root": str(self.workspace_root),
            },
        )
        self.stop_run()
        self.root.destroy()


def launch() -> int:
    root = Tk()
    KernIDE(root)
    root.mainloop()
    return 0

