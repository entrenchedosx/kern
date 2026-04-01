"""
kern-to-exe main window — tabbed UI for packaging Kern .kn programs into a native .exe.
Uses only the Python standard library (tkinter).
"""

from __future__ import annotations

import json
import os
import queue
import shlex
import shutil
import subprocess
import sys
import threading
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

from kern_to_exe.kernc_locator import kernc_probe_report, locate_kernc
from kern_to_exe.spec import Kern2ExeSpec, load_recipe, save_recipe


def _norm_list(widget: tk.Listbox) -> list[str]:
    return [widget.get(i) for i in range(widget.size()) if widget.get(i).strip()]


class KernToExeApp(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("kern-to-exe")
        self.minsize(920, 640)
        self.geometry("980x700")

        self._out_q: queue.Queue[tuple[str, str]] = queue.Queue()
        self._proc: subprocess.Popen[str] | None = None

        self._build_style()
        self._build_menu()
        self._build_ui()
        self.after(120, self._drain_queue)

    def _build_style(self) -> None:
        style = ttk.Style(self)
        try:
            style.theme_use("clam")
        except tk.TclError:
            pass
        style.configure("TNotebook.Tab", padding=[12, 6])
        style.configure("TLabelframe.Label", font=("Segoe UI", 9, "bold"))
        style.configure("Header.TLabel", font=("Segoe UI", 11, "bold"))

    def _build_menu(self) -> None:
        mb = tk.Menu(self)
        self.config(menu=mb)
        file_m = tk.Menu(mb, tearoff=0)
        mb.add_cascade(label="File", menu=file_m)
        file_m.add_command(label="Load recipe…", command=self._load_recipe)
        file_m.add_command(label="Save recipe…", command=self._save_recipe)
        file_m.add_separator()
        file_m.add_command(label="Quit", command=self.destroy)
        help_m = tk.Menu(mb, tearoff=0)
        mb.add_cascade(label="Help", menu=help_m)
        help_m.add_command(label="Where is kernc?…", command=self._show_kernc_probe)
        help_m.add_separator()
        help_m.add_command(label="About kern-to-exe", command=self._about)

    def _build_ui(self) -> None:
        outer = ttk.Frame(self, padding=8)
        outer.pack(fill=tk.BOTH, expand=True)

        hdr = ttk.Label(
            outer,
            text="Convert Kern programs to standalone executables",
            style="Header.TLabel",
        )
        hdr.pack(anchor=tk.W, pady=(0, 6))

        nb = ttk.Notebook(outer)
        nb.pack(fill=tk.BOTH, expand=True, pady=(0, 8))

        self._tab_script(nb)
        self._tab_options(nb)
        self._tab_files(nb)
        self._tab_advanced(nb)

        log_frame = ttk.LabelFrame(outer, text="Build output log")
        log_frame.pack(fill=tk.BOTH, expand=True)
        self.log = tk.Text(log_frame, height=12, wrap=tk.WORD, font=("Consolas", 10), state=tk.DISABLED)
        self.log.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=4, pady=4)
        sb = ttk.Scrollbar(log_frame, command=self.log.yview)
        sb.pack(side=tk.RIGHT, fill=tk.Y, pady=4)
        self.log.config(yscrollcommand=sb.set)
        self.log.tag_configure("stderr", foreground="#c00")
        self.log.tag_configure("ok", foreground="#080")
        self.log.tag_configure("cmd", foreground="#06c")

        btn_row = ttk.Frame(outer)
        btn_row.pack(fill=tk.X, pady=(4, 0))
        ttk.Button(btn_row, text="CONVERT .KN TO .EXE", command=self._start_convert).pack(side=tk.LEFT, padx=(0, 8))
        ttk.Button(btn_row, text="Open output folder", command=self._open_out_dir).pack(side=tk.LEFT, padx=(0, 8))
        ttk.Button(btn_row, text="Clear log", command=self._clear_log).pack(side=tk.LEFT)

        self._append_log("Ready. Select your main .kn file, set output, then click CONVERT.\n", "cmd")
        self._log_kernc_status()

    def _tab_script(self, nb: ttk.Notebook) -> None:
        tab = ttk.Frame(nb, padding=10)
        nb.add(tab, text="Script location")

        f1 = ttk.LabelFrame(tab, text="Main script (entry point)")
        f1.pack(fill=tk.X, pady=(0, 8))
        row = ttk.Frame(f1)
        row.pack(fill=tk.X, padx=6, pady=6)
        self.var_entry = tk.StringVar()
        ttk.Entry(row, textvariable=self.var_entry, width=70).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 6))
        ttk.Button(row, text="Browse…", command=self._browse_entry).pack(side=tk.LEFT)

        f2 = ttk.LabelFrame(tab, text="Additional .kn modules (explicit bundle list — optional)")
        f2.pack(fill=tk.BOTH, expand=True, pady=(0, 8))
        self.list_kn = tk.Listbox(f2, height=8, selectmode=tk.EXTENDED)
        self.list_kn.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=6, pady=6)
        sby = ttk.Scrollbar(f2, command=self.list_kn.yview)
        sby.pack(side=tk.RIGHT, fill=tk.Y, pady=6)
        self.list_kn.config(yscrollcommand=sby.set)
        br = ttk.Frame(f2)
        br.pack(side=tk.RIGHT, fill=tk.Y, padx=6, pady=6)
        ttk.Button(br, text="Add files…", command=self._add_kn).pack(fill=tk.X, pady=2)
        ttk.Button(br, text="Remove", command=self._remove_kn).pack(fill=tk.X, pady=2)

        f3 = ttk.LabelFrame(tab, text="Project root (working directory & import search)")
        f3.pack(fill=tk.X)
        row3 = ttk.Frame(f3)
        row3.pack(fill=tk.X, padx=6, pady=6)
        self.var_root = tk.StringVar()
        ttk.Entry(row3, textvariable=self.var_root, width=70).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 6))
        ttk.Button(row3, text="Browse…", command=self._browse_root).pack(side=tk.LEFT)

    def _tab_options(self, nb: ttk.Notebook) -> None:
        tab = ttk.Frame(nb, padding=10)
        nb.add(tab, text="Onefile / options")

        info = ttk.Label(
            tab,
            text=(
                "Kern always produces a single native .exe (no separate “onedir” mode). "
                "Assets and extra .kn files are embedded in the binary when listed."
            ),
            wraplength=840,
        )
        info.pack(anchor=tk.W, pady=(0, 12))

        self.var_release = tk.BooleanVar(value=True)
        ttk.Checkbutton(tab, text="Release build (uncheck for Debug)", variable=self.var_release).pack(anchor=tk.W)

        row = ttk.Frame(tab)
        row.pack(anchor=tk.W, pady=8)
        ttk.Label(row, text="Optimization level (0–3):").pack(side=tk.LEFT, padx=(0, 8))
        self.spin_opt = tk.Spinbox(row, from_=0, to=3, width=4)
        self.spin_opt.delete(0, tk.END)
        self.spin_opt.insert(0, "2")
        self.spin_opt.pack(side=tk.LEFT)

        self.var_console = tk.BooleanVar(value=True)
        ttk.Checkbutton(
            tab,
            text="Console based (shows terminal). Uncheck for windowed subsystem (no console).",
            variable=self.var_console,
        ).pack(anchor=tk.W, pady=(8, 0))

        self.var_force = tk.BooleanVar(value=False)
        ttk.Checkbutton(
            tab,
            text="Force full rebuild (delete previous output exe + .kern-cache next to output)",
            variable=self.var_force,
        ).pack(anchor=tk.W, pady=(12, 0))

        f = ttk.LabelFrame(tab, text="Output executable")
        f.pack(fill=tk.X, pady=(16, 0))
        row = ttk.Frame(f)
        row.pack(fill=tk.X, padx=6, pady=6)
        self.var_output = tk.StringVar()
        ttk.Entry(row, textvariable=self.var_output, width=70).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 6))
        ttk.Button(row, text="Browse…", command=self._browse_output).pack(side=tk.LEFT)

    def _tab_files(self, nb: ttk.Notebook) -> None:
        tab = ttk.Frame(nb, padding=10)
        nb.add(tab, text="Additional files")

        ttk.Label(
            tab,
            text="Files and folders listed here are embedded as assets (read at runtime via bundled paths).",
            wraplength=840,
        ).pack(anchor=tk.W, pady=(0, 8))

        self.list_assets = tk.Listbox(tab, height=14, selectmode=tk.EXTENDED)
        self.list_assets.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, pady=(0, 8))
        sb = ttk.Scrollbar(tab, command=self.list_assets.yview)
        sb.pack(side=tk.LEFT, fill=tk.Y, pady=(0, 8))
        self.list_assets.config(yscrollcommand=sb.set)
        br = ttk.Frame(tab)
        br.pack(side=tk.RIGHT, fill=tk.Y, padx=8, pady=(0, 8))
        ttk.Button(br, text="Add files…", command=self._add_assets_files).pack(fill=tk.X, pady=2)
        ttk.Button(br, text="Add folder…", command=self._add_assets_dir).pack(fill=tk.X, pady=2)
        ttk.Button(br, text="Remove", command=self._remove_assets).pack(fill=tk.X, pady=2)

    def _tab_advanced(self, nb: ttk.Notebook) -> None:
        tab = ttk.Frame(nb, padding=10)
        nb.add(tab, text="Advanced")

        f0 = ttk.LabelFrame(tab, text="Windows icon (.ico) — optional")
        f0.pack(fill=tk.X, pady=(0, 8))
        row = ttk.Frame(f0)
        row.pack(fill=tk.X, padx=6, pady=6)
        self.var_icon = tk.StringVar()
        ttk.Entry(row, textvariable=self.var_icon, width=70).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 6))
        ttk.Button(row, text="Browse…", command=self._browse_icon).pack(side=tk.LEFT)

        f1 = ttk.LabelFrame(tab, text="Environment")
        f1.pack(fill=tk.X, pady=(0, 8))
        ttk.Label(f1, text="KERN_REPO_ROOT (only if kernc cannot find the Kern source tree):").pack(anchor=tk.W, padx=6, pady=(6, 2))
        self.var_kern_repo = tk.StringVar()
        ttk.Entry(f1, textvariable=self.var_kern_repo, width=90).pack(fill=tk.X, padx=6, pady=(0, 6))

        self.var_json = tk.BooleanVar(value=False)
        ttk.Checkbutton(tab, text="Print machine-readable JSON status (--json)", variable=self.var_json).pack(anchor=tk.W)

        f2 = ttk.LabelFrame(tab, text="Pre-build / post-build shell commands (kernconfig plugins)")
        f2.pack(fill=tk.BOTH, expand=True, pady=(8, 0))
        sub = ttk.Frame(f2)
        sub.pack(fill=tk.BOTH, expand=True, padx=6, pady=6)
        ttk.Label(sub, text="Pre-build (one per line in list)").grid(row=0, column=0, sticky=tk.W)
        ttk.Label(sub, text="Post-build").grid(row=0, column=1, sticky=tk.W)
        self.list_pre = tk.Listbox(sub, height=5, width=42)
        self.list_post = tk.Listbox(sub, height=5, width=42)
        self.list_pre.grid(row=1, column=0, padx=(0, 8), sticky=tk.NSEW)
        self.list_post.grid(row=1, column=1, sticky=tk.NSEW)
        sub.columnconfigure(0, weight=1)
        sub.columnconfigure(1, weight=1)
        bp = ttk.Frame(sub)
        bp.grid(row=2, column=0, columnspan=2, sticky=tk.W, pady=6)
        ttk.Button(bp, text="Add pre-build…", command=lambda: self._add_cmd(self.list_pre)).pack(side=tk.LEFT, padx=(0, 6))
        ttk.Button(bp, text="Add post-build…", command=lambda: self._add_cmd(self.list_post)).pack(side=tk.LEFT, padx=(0, 6))
        ttk.Button(bp, text="Remove selected", command=self._remove_cmd).pack(side=tk.LEFT)

        f3 = ttk.LabelFrame(tab, text="Diagnostics JSON path (optional — passed as --build-diagnostics-json)")
        f3.pack(fill=tk.X, pady=(8, 0))
        row = ttk.Frame(f3)
        row.pack(fill=tk.X, padx=6, pady=6)
        self.var_diag = tk.StringVar()
        ttk.Entry(row, textvariable=self.var_diag, width=70).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 6))
        ttk.Button(row, text="Default…", command=self._default_diag).pack(side=tk.LEFT)

    # —— file dialogs ——
    def _browse_entry(self) -> None:
        p = filedialog.askopenfilename(filetypes=[("Kern", "*.kn"), ("All", "*.*")])
        if p:
            self.var_entry.set(str(Path(p).resolve()))
            if not self.var_root.get().strip():
                self.var_root.set(str(Path(p).resolve().parent))
            if not self.var_output.get().strip():
                stem = Path(p).stem
                self.var_output.set(str(Path(p).resolve().parent / "dist" / f"{stem}.exe"))

    def _browse_root(self) -> None:
        p = filedialog.askdirectory()
        if p:
            self.var_root.set(str(Path(p).resolve()))

    def _browse_output(self) -> None:
        p = filedialog.asksaveasfilename(defaultextension=".exe", filetypes=[("Executable", "*.exe"), ("All", "*.*")])
        if p:
            self.var_output.set(str(Path(p).resolve()))

    def _browse_icon(self) -> None:
        p = filedialog.askopenfilename(filetypes=[("Icon", "*.ico"), ("All", "*.*")])
        if p:
            self.var_icon.set(str(Path(p).resolve()))

    def _add_kn(self) -> None:
        paths = filedialog.askopenfilenames(filetypes=[("Kern", "*.kn"), ("All", "*.*")])
        for p in paths:
            self._list_add_unique(self.list_kn, str(Path(p).resolve()))

    def _remove_kn(self) -> None:
        self._list_remove_sel(self.list_kn)

    def _add_assets_files(self) -> None:
        paths = filedialog.askopenfilenames(title="Add asset files", filetypes=[("All", "*.*")])
        for p in paths:
            self._list_add_unique(self.list_assets, str(Path(p).resolve()))

    def _add_assets_dir(self) -> None:
        p = filedialog.askdirectory(title="Add folder as asset root")
        if p:
            self._list_add_unique(self.list_assets, str(Path(p).resolve()))

    def _remove_assets(self) -> None:
        self._list_remove_sel(self.list_assets)

    @staticmethod
    def _list_add_unique(lb: tk.Listbox, item: str) -> None:
        cur = {lb.get(i) for i in range(lb.size())}
        if item not in cur:
            lb.insert(tk.END, item)

    @staticmethod
    def _list_remove_sel(lb: tk.Listbox) -> None:
        for i in reversed(lb.curselection()):
            lb.delete(i)

    def _add_cmd(self, lb: tk.Listbox) -> None:
        d = tk.Toplevel(self)
        d.title("Command")
        d.geometry("520x120")
        v = tk.StringVar()
        ttk.Entry(d, textvariable=v, width=70).pack(padx=10, pady=10, fill=tk.X)
        def ok() -> None:
            s = v.get().strip()
            if s:
                lb.insert(tk.END, s)
            d.destroy()

        ttk.Button(d, text="OK", command=ok).pack(pady=6)

    def _remove_cmd(self) -> None:
        self._list_remove_sel(self.list_pre)
        self._list_remove_sel(self.list_post)

    def _default_diag(self) -> None:
        root = self.var_root.get().strip()
        if root:
            self.var_diag.set(str(Path(root) / ".kern-to-exe" / "build-diagnostics.json"))
        else:
            self.var_diag.set(str(Path.cwd() / ".kern-to-exe" / "build-diagnostics.json"))

    def _gather_spec(self) -> Kern2ExeSpec | None:
        try:
            opt = int(self.spin_opt.get().strip())
        except ValueError:
            opt = 2
        spec = Kern2ExeSpec(
            entry=self.var_entry.get().strip(),
            output=self.var_output.get().strip(),
            project_root=self.var_root.get().strip(),
            extra_kn_files=_norm_list(self.list_kn),
            assets=_norm_list(self.list_assets),
            icon=self.var_icon.get().strip(),
            release=self.var_release.get(),
            opt=opt,
            console=self.var_console.get(),
            force_rebuild=self.var_force.get(),
            kern_repo_root=self.var_kern_repo.get().strip(),
            machine_json=self.var_json.get(),
            pre_build=_norm_list(self.list_pre),
            post_build=_norm_list(self.list_post),
            diagnostics_json=self.var_diag.get().strip(),
        )
        errs = spec.validate()
        if errs:
            messagebox.showerror("Invalid settings", "\n".join(errs))
            return None
        return spec

    def _apply_force(self, spec: Kern2ExeSpec) -> None:
        outp = Path(spec.output)
        try:
            if outp.is_file():
                outp.unlink()
                self._append_log(f"[kern-to-exe] removed previous: {outp}\n", "cmd")
        except OSError as e:
            self._append_log(f"[kern-to-exe] could not delete output: {e}\n", "stderr")
        cache = outp.parent / ".kern-cache"
        if cache.is_dir():
            try:
                shutil.rmtree(cache)
                self._append_log(f"[kern-to-exe] removed cache: {cache}\n", "cmd")
            except OSError as e:
                self._append_log(f"[kern-to-exe] could not remove cache: {e}\n", "stderr")

    def _start_convert(self) -> None:
        if self._proc is not None and self._proc.poll() is None:
            messagebox.showinfo("Busy", "A build is already running.")
            return
        kernc = locate_kernc()
        if not kernc:
            messagebox.showerror(
                "kernc not found",
                "Could not find kernc.exe.\nBuild the Kern toolchain or set KERNC_EXE to the full path.",
            )
            return
        spec = self._gather_spec()
        if not spec:
            return
        if spec.force_rebuild:
            self._apply_force(spec)

        root = Path(spec.project_root)
        cfg_dir = root / ".kern-to-exe"
        cfg_path = cfg_dir / "kernconfig.json"
        try:
            spec.write_kernconfig(cfg_path)
        except OSError as e:
            messagebox.showerror("Config", f"Failed to write kernconfig:\n{e}")
            return

        args = [kernc, "--config", str(cfg_path)]
        diag = spec.diagnostics_json.strip()
        if diag:
            Path(diag).parent.mkdir(parents=True, exist_ok=True)
            args.extend(["--build-diagnostics-json", diag])
        if spec.machine_json:
            args.append("--json")

        env = os.environ.copy()
        if spec.kern_repo_root:
            env["KERN_REPO_ROOT"] = spec.kern_repo_root

        self._append_log(f"\n--- CONVERT ---\n", "cmd")
        try:
            cmd_display = shlex.join(args)
        except (TypeError, ValueError):
            cmd_display = " ".join(args)
        self._append_log(f"$ {cmd_display}\n", "cmd")
        self._append_log(f"cwd: {root}\n", "cmd")

        def worker() -> None:
            try:
                cflags = 0
                if os.name == "nt":
                    cflags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
                proc = subprocess.Popen(
                    args,
                    cwd=str(root),
                    env=env,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True,
                    encoding="utf-8",
                    errors="replace",
                    creationflags=cflags,
                )
                self._proc = proc
                assert proc.stdout
                for line in proc.stdout:
                    self._out_q.put(("out", line))
                code = proc.wait()
                self._out_q.put(("done", str(code)))
            except Exception as exc:  # noqa: BLE001
                self._out_q.put(("err", f"{exc}\n"))
                self._out_q.put(("done", "1"))
            finally:
                self._proc = None

        threading.Thread(target=worker, daemon=True).start()

    def _drain_queue(self) -> None:
        try:
            while True:
                kind, text = self._out_q.get_nowait()
                if kind == "out":
                    self._append_log(text)
                elif kind == "err":
                    self._append_log(text, "stderr")
                elif kind == "done":
                    code = text
                    self._append_log(f"\n--- Exit code {code} ---\n", "ok" if code == "0" else "stderr")
        except queue.Empty:
            pass
        self.after(120, self._drain_queue)

    def _append_log(self, text: str, tag: str | None = None) -> None:
        self.log.config(state=tk.NORMAL)
        self.log.insert(tk.END, text, tag or "")
        self.log.see(tk.END)
        self.log.config(state=tk.DISABLED)

    def _clear_log(self) -> None:
        self.log.config(state=tk.NORMAL)
        self.log.delete("1.0", tk.END)
        self.log.config(state=tk.DISABLED)

    def _open_out_dir(self) -> None:
        spec = self._gather_spec()
        if not spec:
            return
        p = Path(spec.output).parent
        if p.is_dir():
            try:
                if os.name == "nt":
                    os.startfile(str(p))  # noqa: S606
                elif sys.platform == "darwin":
                    subprocess.run(["open", str(p)], check=False)
                else:
                    subprocess.run(["xdg-open", str(p)], check=False)
            except OSError as e:
                messagebox.showerror("Open folder", str(e))

    def _load_recipe(self) -> None:
        p = filedialog.askopenfilename(filetypes=[("kern-to-exe recipe", "*.kern2exe.json"), ("JSON", "*.json")])
        if not p:
            return
        try:
            spec = load_recipe(Path(p))
        except (OSError, json.JSONDecodeError, ValueError) as e:
            messagebox.showerror("Recipe", str(e))
            return
        self._apply_spec_to_ui(spec)
        self._append_log(f"Loaded recipe: {p}\n", "ok")

    def _save_recipe(self) -> None:
        spec = self._gather_spec()
        if not spec:
            return
        p = filedialog.asksaveasfilename(
            defaultextension=".kern2exe.json",
            filetypes=[("kern-to-exe recipe", "*.kern2exe.json"), ("JSON", "*.json")],
        )
        if not p:
            return
        try:
            save_recipe(Path(p), spec)
        except OSError as e:
            messagebox.showerror("Recipe", str(e))
            return
        self._append_log(f"Saved recipe: {p}\n", "ok")

    def _apply_spec_to_ui(self, spec: Kern2ExeSpec) -> None:
        self.var_entry.set(spec.entry)
        self.var_output.set(spec.output)
        self.var_root.set(spec.project_root)
        self.var_icon.set(spec.icon)
        self.var_release.set(spec.release)
        self.spin_opt.delete(0, tk.END)
        self.spin_opt.insert(0, str(spec.opt))
        self.var_console.set(spec.console)
        self.var_force.set(spec.force_rebuild)
        self.var_kern_repo.set(spec.kern_repo_root)
        self.var_json.set(spec.machine_json)
        self.var_diag.set(spec.diagnostics_json)
        self.list_kn.delete(0, tk.END)
        for x in spec.extra_kn_files:
            self.list_kn.insert(tk.END, x)
        self.list_assets.delete(0, tk.END)
        for x in spec.assets:
            self.list_assets.insert(tk.END, x)
        self.list_pre.delete(0, tk.END)
        for x in spec.pre_build:
            self.list_pre.insert(tk.END, x)
        self.list_post.delete(0, tk.END)
        for x in spec.post_build:
            self.list_post.insert(tk.END, x)

    def _log_kernc_status(self) -> None:
        k = locate_kernc()
        if k:
            self._append_log(f"kernc found: {k}\n", "ok")
        else:
            self._append_log(
                "kernc not found — build the repo (Release\\kernc.exe), copy it beside this app, "
                "or set environment variable KERNC_EXE to the full path.\n",
                "stderr",
            )
            self._append_log('Help → "Where is kernc?" lists every path that was checked.\n', "cmd")

    def _show_kernc_probe(self) -> None:
        found, lines = kernc_probe_report()
        top = tk.Toplevel(self)
        top.title("kernc search")
        top.geometry("720x420")
        top.minsize(480, 280)
        ttk.Label(
            top,
            text=("Using: " + found) if found else "No kernc.exe found — set KERNC_EXE or build the toolchain.",
            wraplength=680,
        ).pack(anchor=tk.W, padx=8, pady=(8, 4))
        fr = ttk.Frame(top)
        fr.pack(fill=tk.BOTH, expand=True, padx=8, pady=(0, 8))
        tb = tk.Text(fr, height=16, wrap=tk.NONE, font=("Consolas", 9))
        sy = ttk.Scrollbar(fr, command=tb.yview)
        sx = ttk.Scrollbar(fr, orient=tk.HORIZONTAL, command=tb.xview)
        tb.config(yscrollcommand=sy.set, xscrollcommand=sx.set)
        tb.grid(row=0, column=0, sticky=tk.NSEW)
        sy.grid(row=0, column=1, sticky=tk.NS)
        sx.grid(row=1, column=0, sticky=tk.EW)
        fr.rowconfigure(0, weight=1)
        fr.columnconfigure(0, weight=1)
        tb.insert("1.0", "\n".join(lines))
        tb.config(state=tk.DISABLED)
        ttk.Button(top, text="Close", command=top.destroy).pack(pady=6)

    def _about(self) -> None:
        k = locate_kernc()
        kline = f"kernc: {k}\n\n" if k else "kernc: not detected (set KERNC_EXE).\n\n"
        messagebox.showinfo(
            "About kern-to-exe",
            "kern-to-exe — Kern → native EXE packager\n\n"
            + kline
            + "Pick your main .kn, options, extra files, then CONVERT.\n\n"
            "If the built .exe closes instantly, run it from Command Prompt to see errors,\n"
            "or set KERN_STANDALONE_NO_PAUSE=1 to skip Enter-to-close on failure.",
        )


def main() -> None:
    app = KernToExeApp()
    app.mainloop()


if __name__ == "__main__":
    main()
