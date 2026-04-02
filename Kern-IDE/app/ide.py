from __future__ import annotations

import os
import platform
import shutil
import subprocess
from collections.abc import Callable
from pathlib import Path
from tkinter import (
    BOTH,
    BOTTOM,
    END,
    HORIZONTAL,
    LEFT,
    Listbox,
    RIGHT,
    TOP,
    VERTICAL,
    X,
    BooleanVar,
    Menu,
    Text,
    Tk,
    Toplevel,
    filedialog,
    messagebox,
    simpledialog,
    ttk,
)

from services.diagnostics import format_problem_line

from .editor import EditorTab
from .filesystem import FileExplorer
from .runner import KernRunner
from .state import load_state, save_state
from .theme import Theme, resolve_theme
from .version import ide_version
from ui.command_palette import show_command_palette
from ui.tooltip import ToolTip


def default_workspace_root() -> Path:
    if getattr(__import__("sys"), "frozen", False):
        import sys

        return Path(sys.executable).resolve().parent
    return Path(__file__).resolve().parents[2]


class KernIDE:
    def __init__(self, root: Tk) -> None:
        self.root = root
        self._ide_version = ide_version()
        self.root.title(f"Kern IDE {self._ide_version}")
        self.root.geometry("1200x760")
        self.root.minsize(920, 560)

        self.workspace_root = default_workspace_root()
        self.state_path = self.workspace_root / ".kern-ide-state.json"
        self.state = load_state(self.state_path)
        ws = self.state.get("workspace_root")
        if ws:
            p = Path(str(ws))
            if p.is_dir():
                self.workspace_root = p.resolve()
        self.state_path = self.workspace_root / ".kern-ide-state.json"
        self.state = load_state(self.state_path)

        self.dark_mode = bool(self.state.get("dark_mode", True))
        self.show_explorer = BooleanVar(value=bool(self.state.get("show_explorer", True)))
        self.show_console = BooleanVar(value=bool(self.state.get("show_console", True)))
        self.show_debug = BooleanVar(value=bool(self.state.get("show_debug", False)))
        self.editor_font_size = int(self.state.get("editor_font_size", 11))
        self.autosave_ms = int(self.state.get("autosave_ms", 4000))
        self._autosave_job: str | None = None

        self.theme: Theme = resolve_theme(self.dark_mode)
        self.runner = KernRunner()
        self.editors: dict[str, EditorTab] = {}
        self.untitled_count = 1
        self._problem_items: list[dict[str, object]] = []
        self._tooltips: list[ToolTip] = []

        self._build_styles()
        self._build_menu()
        self._build_ui()
        self._bind_keys()
        self._apply_theme()
        self._update_layout()
        self.explorer.refresh()
        self.new_file()
        self.root.after(300, self._maybe_show_onboarding)

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
        file_menu.add_command(label="Open workspace…", command=self.choose_workspace)
        file_menu.add_separator()
        file_menu.add_command(label="Save", command=self.save_current, accelerator="Ctrl+S")
        file_menu.add_command(label="Save As...", command=self.save_current_as)
        file_menu.add_separator()
        file_menu.add_command(label="Preferences…", command=self.show_preferences)
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
        run_menu.add_command(label="Check", command=self.check_current, accelerator="Ctrl+K")
        run_menu.add_command(label="Stop", command=self.stop_run, accelerator="Shift+F5")
        run_menu.add_separator()
        run_menu.add_command(label="Clear Output", command=self.clear_output)
        menubar.add_cascade(label="Run", menu=run_menu)

        view_menu = Menu(menubar, tearoff=0)
        view_menu.add_checkbutton(label="Show file explorer", variable=self.show_explorer, command=self._update_layout)
        view_menu.add_checkbutton(label="Show console", variable=self.show_console, command=self._update_layout)
        view_menu.add_checkbutton(label="Show debugger panel", variable=self.show_debug, command=self._update_layout)
        view_menu.add_separator()
        view_menu.add_command(label="Toggle light/dark mode", command=self.toggle_theme, accelerator="Ctrl+Shift+T")
        view_menu.add_command(label="Command Palette…", command=self.open_command_palette, accelerator="Ctrl+Shift+P")
        menubar.add_cascade(label="View", menu=view_menu)

        help_menu = Menu(menubar, tearoff=0)
        help_menu.add_command(label="Quick start", command=self.show_quick_start)
        help_menu.add_command(label="Keyboard shortcuts", command=self.show_shortcuts_help)
        help_menu.add_separator()
        help_menu.add_command(label="About Kern IDE", command=self._show_about)
        menubar.add_cascade(label="Help", menu=help_menu)
        self.root.config(menu=menubar)

    def _build_ui(self) -> None:
        self.toolbar = ttk.Frame(self.root)
        self.toolbar.pack(side=TOP, fill=X, padx=8, pady=(8, 4))
        self.app_title = ttk.Label(self.toolbar, text=f"Kern IDE {self._ide_version}", anchor="w")
        self.app_title.pack(side=LEFT, padx=(0, 12))
        self.btn_run = ttk.Button(self.toolbar, text="Run", command=self.run_current)
        self.btn_run.pack(side=LEFT, padx=(0, 6))
        self.btn_check = ttk.Button(self.toolbar, text="Check", command=self.check_current)
        self.btn_check.pack(side=LEFT, padx=(0, 6))
        self.btn_stop = ttk.Button(self.toolbar, text="Stop", command=self.stop_run)
        self.btn_stop.pack(side=LEFT, padx=(0, 10))
        self.btn_clear = ttk.Button(self.toolbar, text="Clear output", command=self.clear_output)
        self.btn_clear.pack(side=LEFT, padx=(0, 10))
        ttk.Button(self.toolbar, text="Palette", command=self.open_command_palette).pack(side=LEFT)

        self._tooltips = [
            ToolTip(self.btn_run, "Run the current file with kern.exe (F5)"),
            ToolTip(self.btn_check, "Run kern --check on the current file (Ctrl+K)"),
            ToolTip(self.btn_stop, "Stop the running process (Shift+F5)"),
            ToolTip(self.btn_clear, "Clear console and problems list"),
        ]

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
        self.tree.bind("<Button-3>", self._explorer_context)
        self.explorer = FileExplorer(self.tree, self.workspace_root)
        self.tree.bind("<<TreeviewOpen>>", lambda e: self.explorer.on_tree_open(e))

        self.editor_frame = ttk.Frame(self.top_horizontal)
        self.top_horizontal.add(self.editor_frame, weight=6)
        self.breadcrumb = ttk.Label(self.editor_frame, anchor="w", style="Section.TLabel")
        self.breadcrumb.pack(fill=X, padx=6, pady=(2, 0))
        self.tabs = ttk.Notebook(self.editor_frame)
        self.tabs.pack(fill=BOTH, expand=True)
        self.tabs.bind("<<NotebookTabChanged>>", lambda _e: self._refresh_status())

        self.debug_frame = ttk.Frame(self.top_horizontal, width=240)
        self.debug_label = ttk.Label(self.debug_frame, text="Debugger", anchor="w")
        self.debug_label.pack(fill=X, padx=6, pady=(8, 4))
        self.debug_text = Text(self.debug_frame, height=8, wrap="word", relief="flat")
        self.debug_text.pack(fill=BOTH, expand=True, padx=6, pady=(0, 6))
        self.debug_text.insert("1.0", "Debugger panel\n\nBreakpoints / step execution are not wired yet.\nShows file + cursor context when enabled.")
        self.debug_text.configure(state="disabled")

        self.console_frame = ttk.Frame(self.main_vertical, height=180)
        self.main_vertical.add(self.console_frame, weight=1)
        self.console_label = ttk.Label(self.console_frame, text="Output", anchor="w")
        self.console_label.pack(fill=X, padx=8, pady=(6, 2))

        self.problems_frame = ttk.Frame(self.console_frame)
        self.problems_label = ttk.Label(self.problems_frame, text="Problems (double-click to jump)", anchor="w", style="Section.TLabel")
        self.problems_label.pack(fill=X, padx=0, pady=(0, 2))
        self.problems_list = Listbox(self.problems_frame, height=4, relief="flat", borderwidth=0, highlightthickness=0)
        self.problems_list.pack(fill=X)
        self.problems_list.bind("<Double-Button-1>", self._on_problem_double_click)

        self.console = Text(self.console_frame, height=10, wrap="word", relief="flat")
        self.console.pack(fill=BOTH, expand=True, padx=8, pady=(0, 8))
        self.console.configure(state="disabled")

        self.status = ttk.Label(self.root, anchor="w")
        self.status.pack(side=BOTTOM, fill=X, padx=8, pady=(0, 6))

    def _bind_keys(self) -> None:
        self.root.bind("<Control-n>", lambda e: (self.new_file(), "break")[1])
        self.root.bind("<Control-o>", lambda e: (self.open_file(), "break")[1])
        self.root.bind("<Control-s>", lambda e: (self.save_current(), "break")[1])
        self.root.bind("<Control-k>", lambda e: (self.check_current(), "break")[1])
        self.root.bind("<F5>", lambda e: (self.run_current(), "break")[1])
        self.root.bind("<Shift-F5>", lambda e: (self.stop_run(), "break")[1])
        self.root.bind("<Control-Shift-P>", lambda e: (self.open_command_palette(), "break")[1])
        self.root.bind("<Control-grave>", lambda e: (self._toggle_console(), "break")[1])
        self.root.bind("<Control-Shift-T>", lambda e: (self.toggle_theme(), "break")[1])

    def _toggle_console(self) -> None:
        self.show_console.set(not self.show_console.get())
        self._update_layout()

    def open_command_palette(self) -> None:
        cmds: list[tuple[str, Callable[[], None]]] = [
            ("Run: Run current program", self.run_current),
            ("Run: Check current file (kern --check)", self.check_current),
            ("Run: Stop", self.stop_run),
            ("Run: Clear output", self.clear_output),
            ("File: New", self.new_file),
            ("File: Open…", self.open_file),
            ("File: Save", self.save_current),
            ("File: Save As…", self.save_current_as),
            ("File: Open workspace…", self.choose_workspace),
            ("File: Preferences…", self.show_preferences),
            ("View: Toggle file explorer", lambda: (self.show_explorer.set(not self.show_explorer.get()), self._update_layout())),
            ("View: Toggle console", self._toggle_console),
            ("View: Toggle debugger panel", lambda: (self.show_debug.set(not self.show_debug.get()), self._update_layout())),
            ("View: Toggle light/dark theme", self.toggle_theme),
            ("Help: Quick start", self.show_quick_start),
            ("Help: Keyboard shortcuts", self.show_shortcuts_help),
            ("System: Open workspace folder in file manager", self.open_workspace_in_os),
        ]
        show_command_palette(self.root, self.theme, "Command Palette", cmds)

    def choose_workspace(self) -> None:
        d = filedialog.askdirectory(title="Choose workspace folder", initialdir=str(self.workspace_root))
        if not d:
            return
        self.workspace_root = Path(d).resolve()
        self.state_path = self.workspace_root / ".kern-ide-state.json"
        merged = load_state(self.state_path)
        self.state.update(merged)
        self.explorer.set_root(self.workspace_root)
        self.explorer.refresh()
        self._save_state()
        self._refresh_status()

    def open_workspace_in_os(self) -> None:
        p = str(self.workspace_root)
        try:
            if platform.system() == "Windows":
                os.startfile(p)  # noqa: S606
            elif platform.system() == "Darwin":
                subprocess.Popen(["open", p])
            else:
                subprocess.Popen(["xdg-open", p])
        except Exception as exc:
            messagebox.showerror("Open folder", str(exc))

    def show_preferences(self) -> None:
        win = Toplevel(self.root)
        win.title("Preferences")
        win.transient(self.root)
        win.configure(bg=self.theme.panel)
        fsize = ttk.Scale(win, from_=9, to=22, orient=HORIZONTAL)
        fsize.set(self.editor_font_size)
        ttk.Label(win, text="Editor font size").pack(anchor="w", padx=12, pady=(12, 4))
        fsize.pack(fill=X, padx=12)
        auto_var = ttk.Entry(win)
        auto_var.insert(0, str(self.autosave_ms))
        ttk.Label(win, text="Autosave delay (ms, 0 = off)").pack(anchor="w", padx=12, pady=(12, 4))
        auto_var.pack(fill=X, padx=12)

        def save_prefs() -> None:
            try:
                self.editor_font_size = int(round(float(fsize.get())))
            except Exception:
                self.editor_font_size = 11
            try:
                self.autosave_ms = max(0, int(auto_var.get().strip() or "0"))
            except Exception:
                self.autosave_ms = 4000
            for ed in self.editors.values():
                ed.set_font_size(self.editor_font_size)
            self._save_state()
            win.destroy()

        ttk.Button(win, text="OK", command=save_prefs).pack(pady=12)

    def show_quick_start(self) -> None:
        win = Toplevel(self.root)
        win.title("Welcome to Kern IDE")
        win.transient(self.root)
        win.geometry("520x420")
        txt = Text(win, wrap="word", font=("Segoe UI", 10), padx=12, pady=12)
        txt.pack(fill=BOTH, expand=True)
        body = (
            "Welcome\n\n"
            "1. This window is your workspace — use File → Open workspace… to point at a Kern checkout "
            "(the folder that contains lib/kern and your examples).\n\n"
            "2. Create or open a .kn file, edit in the center, and press Run (or F5) to execute with kern.exe.\n\n"
            "3. Check (Ctrl+K) runs the compiler diagnostics; issues appear under Problems — double-click to jump.\n\n"
            "4. Set KERN_EXE in the environment if kern.exe is not found next to the IDE.\n\n"
            "5. Command Palette: Ctrl+Shift+P for all actions.\n"
        )
        txt.insert("1.0", body)
        txt.configure(state="disabled")

        def ok() -> None:
            self.state["onboarding_done"] = True
            self._save_state()
            win.destroy()

        ttk.Button(win, text="Got it", command=ok).pack(pady=8)

    def _maybe_show_onboarding(self) -> None:
        if self.state.get("onboarding_done"):
            return
        self.show_quick_start()

    def show_shortcuts_help(self) -> None:
        messagebox.showinfo(
            "Keyboard shortcuts",
            "Ctrl+N — New file\n"
            "Ctrl+O — Open file\n"
            "Ctrl+S — Save\n"
            "Ctrl+K — Check (kern --check)\n"
            "F5 — Run\n"
            "Shift+F5 — Stop\n"
            "Ctrl+Shift+P — Command palette\n"
            "Ctrl+` — Toggle console\n"
            "Ctrl+Shift+T — Toggle theme\n"
            "Ctrl+Space — Autocomplete",
        )

    def _explorer_context(self, event: object) -> None:
        row = self.tree.identify_row(event.y)  # type: ignore[attr-defined]
        if not row:
            return
        path = self.explorer.path_for_node(row)
        if not path:
            return
        menu = Menu(self.root, tearoff=0)
        if path.is_dir():
            menu.add_command(label="New file here…", command=lambda: self._explorer_new_file(path))
        else:
            menu.add_command(label="Open", command=lambda: self._open_path(path))
        menu.add_command(label="Rename…", command=lambda: self._explorer_rename(path))
        menu.add_command(label="Delete…", command=lambda: self._explorer_delete(path))
        try:
            menu.tk_popup(event.x_root, event.y_root)  # type: ignore[attr-defined]
        finally:
            menu.grab_release()

    def _explorer_new_file(self, dir_path: Path) -> None:
        name = simpledialog.askstring("New file", "File name (e.g. main.kn):", parent=self.root)
        if not name:
            return
        name = name.strip()
        if not name.endswith(".kn"):
            name += ".kn"
        dest = (dir_path / name).resolve()
        try:
            dest.parent.mkdir(parents=True, exist_ok=True)
            if dest.exists():
                messagebox.showerror("New file", "File already exists.")
                return
            dest.write_text('print("hello")\n', encoding="utf-8")
        except Exception as exc:
            messagebox.showerror("New file", str(exc))
            return
        self.explorer.refresh()
        self._open_path(dest)

    def _explorer_rename(self, path: Path) -> None:
        new_name = simpledialog.askstring("Rename", "New name:", initialvalue=path.name, parent=self.root)
        if not new_name or new_name.strip() == path.name:
            return
        dest = path.parent / new_name.strip()
        try:
            path.rename(dest)
        except Exception as exc:
            messagebox.showerror("Rename", str(exc))
            return
        for ed in self.editors.values():
            if ed.file_path == path:
                ed.file_path = dest
                self._refresh_tab_title(ed)
        self.explorer.refresh()

    def _explorer_delete(self, path: Path) -> None:
        if not messagebox.askyesno("Delete", f"Delete {path.name}?"):
            return
        try:
            if path.is_dir():
                shutil.rmtree(path)
            else:
                path.unlink()
        except Exception as exc:
            messagebox.showerror("Delete", str(exc))
            return
        to_close = [tid for tid, ed in self.editors.items() if ed.file_path == path]
        for tid in to_close:
            self.tabs.forget(tid)
            del self.editors[tid]
        if not self.editors:
            self.new_file()
        else:
            self.tabs.select(list(self.editors.keys())[0])
        self.explorer.refresh()
        self._refresh_status()

    def _schedule_autosave(self) -> None:
        if self.autosave_ms <= 0:
            return
        if self._autosave_job is not None:
            try:
                self.root.after_cancel(self._autosave_job)
            except Exception:
                pass
        self._autosave_job = self.root.after(self.autosave_ms, self._do_autosave)

    def _do_autosave(self) -> None:
        self._autosave_job = None
        ed = self._current_editor()
        if not ed.is_dirty or ed.file_path is None:
            return
        try:
            ed.file_path.write_text(ed.get_content(), encoding="utf-8")
            ed.is_dirty = False
            self._refresh_tab_title(ed)
            self._append_output(f"[autosave] {ed.file_path.name}\n")
        except Exception as exc:
            self._append_output(f"[autosave failed] {exc}\n")

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
        self.problems_label.configure(style="Section.TLabel")
        self.breadcrumb.configure(style="Section.TLabel")
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
        self.problems_list.configure(bg=t.editor, fg=t.text, selectbackground=t.accent, selectforeground="#ffffff")
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
        self._save_state()

    def _new_editor_tab(self, title: str, content: str = "", file_path: Path | None = None) -> EditorTab:
        container = ttk.Frame(self.tabs)
        editor = EditorTab(
            container,
            self.theme,
            on_cursor_change=self._refresh_status,
            on_buffer_change=self._schedule_autosave,
        )
        editor.set_font_size(self.editor_font_size)
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

    def _refresh_breadcrumb(self, editor: EditorTab) -> None:
        try:
            rel = editor.file_path.relative_to(self.workspace_root) if editor.file_path else None
        except Exception:
            rel = editor.file_path
        if rel:
            self.breadcrumb.configure(text=f"{self.workspace_root.name}  ›  {rel}")
        else:
            self.breadcrumb.configure(text=f"{self.workspace_root.name}  ›  (unsaved buffer)")

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
            self._set_problems([])
            return
        editor.apply_diagnostics(diagnostics)
        self._set_problems(diagnostics)
        err_count = len([d for d in diagnostics if str(d.get("kind", "error")).lower() in {"error", "critical"}])
        if diagnostics:
            self._append_output(f"check completed: {len(diagnostics)} issue(s), {err_count} error(s)\n")
        else:
            self._append_output("check completed: no issues\n")

    def _set_problems(self, items: list[dict[str, object]]) -> None:
        self._problem_items = list(items)
        self.problems_list.delete(0, END)
        fp = ""
        ed = self._current_editor()
        if ed.file_path:
            fp = str(ed.file_path)
        for it in items:
            self.problems_list.insert(END, format_problem_line(it, fallback_file=fp))
        if items:
            self.problems_frame.pack(fill=X, padx=8, pady=(0, 4), before=self.console)
        else:
            self.problems_frame.pack_forget()

    def _on_problem_double_click(self, _event: object) -> None:
        sel = self.problems_list.curselection()
        if not sel:
            return
        i = int(sel[0])
        if i < 0 or i >= len(self._problem_items):
            return
        it = self._problem_items[i]
        line = int(it.get("line", 0) or 0)
        col = int(it.get("column", 0) or 0)
        if line > 0:
            self._current_editor().jump_to_line(line, max(1, col))

    def clear_output(self) -> None:
        self.console.configure(state="normal")
        self.console.delete("1.0", END)
        self.console.configure(state="disabled")
        self._set_problems([])

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
        self._refresh_breadcrumb(editor)
        hint = editor.diagnostic_at_cursor()
        base = f"{file_name}    line {line}, col {int(col) + 1}    {self.workspace_root}"
        if hint:
            self.status.configure(text=f"{base}    |    {hint}")
        else:
            self.status.configure(text=base)
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
            f"Kern IDE {self._ide_version}\n"
            "Tk-based editor for the Kern language.\n\n"
            "F5 run · Ctrl+K check · Ctrl+Shift+P palette · Ctrl+` toggle console\n"
            "See Help → Quick start for a short tour.",
        )

    def _persist_dict(self) -> dict:
        return {
            "dark_mode": self.dark_mode,
            "show_explorer": self.show_explorer.get(),
            "show_console": self.show_console.get(),
            "show_debug": self.show_debug.get(),
            "workspace_root": str(self.workspace_root),
            "editor_font_size": self.editor_font_size,
            "autosave_ms": self.autosave_ms,
            "onboarding_done": bool(self.state.get("onboarding_done", False)),
        }

    def _save_state(self) -> None:
        save_state(self.state_path, self._persist_dict())

    def _on_close(self) -> None:
        unsaved = [ed for ed in self.editors.values() if ed.is_dirty]
        if unsaved:
            ok = messagebox.askyesno("Unsaved changes", "You have unsaved files. Close anyway?")
            if not ok:
                return
        self._save_state()
        self.stop_run()
        self.root.destroy()


def launch() -> int:
    root = Tk()
    KernIDE(root)
    root.mainloop()
    return 0
