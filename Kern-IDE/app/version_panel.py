"""Kern compiler version manager UI (GitHub releases, install, active version)."""

from __future__ import annotations

import threading
from typing import TYPE_CHECKING

from tkinter import BOTH, END, LEFT, RIGHT, BooleanVar, StringVar, X, Y, ttk

from services.github_kern_releases import KernRelease, fetch_latest_release_tag, fetch_releases, pick_windows_compiler_zip
from services.ide_logging import append_log
from services.ide_settings import get_active_kern_tag, load_settings, save_settings
from services.kern_version_store import install_zip, list_installed_tags, tag_to_dir_name, verify_installation, version_dir
from services.portable_env import kern_versions_dir

if TYPE_CHECKING:
    from app.ide import KernIDE


class VersionPanel:
    def __init__(self, ide: "KernIDE", parent: ttk.Frame) -> None:
        self.ide = ide
        self._releases: list[KernRelease] = []
        self._busy = False

        top = ttk.Frame(parent)
        top.pack(fill=X, padx=8, pady=8)

        ttk.Label(top, text="Active Kern version:", style="Section.TLabel").pack(side=LEFT, padx=(0, 8))
        self._active_var = StringVar(value="(development PATH)")
        self._active_combo = ttk.Combobox(top, textvariable=self._active_var, width=28, state="readonly")
        self._active_combo.pack(side=LEFT, padx=(0, 12))
        self._active_combo.bind("<<ComboboxSelected>>", self._on_active_changed)

        self._auto_var = BooleanVar(value=False)
        ttk.Checkbutton(top, text="Auto-check for updates on startup", variable=self._auto_var, command=self._save_auto).pack(
            side=LEFT, padx=(12, 0)
        )

        bar = ttk.Frame(parent)
        bar.pack(fill=X, padx=8, pady=(0, 8))
        ttk.Button(bar, text="Refresh release list", command=self._refresh_async).pack(side=LEFT, padx=(0, 8))
        ttk.Button(bar, text="Download / install selected", command=self._install_selected).pack(side=LEFT, padx=(0, 8))
        ttk.Button(bar, text="Reinstall selected (overwrite)", command=lambda: self._install_selected(replace=True)).pack(
            side=LEFT, padx=(0, 8)
        )
        ttk.Button(bar, text="Update to latest release", command=self._update_latest_async).pack(side=LEFT)

        self._progress = ttk.Progressbar(parent, mode="determinate", maximum=100)
        self._progress.pack(fill=X, padx=8, pady=(0, 4))

        self._status = ttk.Label(parent, text="", style="Section.TLabel", wraplength=720)
        self._status.pack(fill=X, padx=8, pady=(0, 4))

        self._banner = ttk.Label(parent, text="", style="Section.TLabel")
        self._banner.pack(fill=X, padx=8, pady=(0, 4))

        tree_frame = ttk.Frame(parent)
        tree_frame.pack(fill=BOTH, expand=True, padx=8, pady=(0, 8))
        cols = ("tag", "date", "status", "asset")
        self.tree = ttk.Treeview(tree_frame, columns=cols, show="headings", height=12)
        self.tree.heading("tag", text="Version")
        self.tree.heading("date", text="Published")
        self.tree.heading("status", text="Status")
        self.tree.heading("asset", text="Windows zip")
        self.tree.column("tag", width=100)
        self.tree.column("date", width=120)
        self.tree.column("status", width=110)
        self.tree.column("asset", width=320)
        vs = ttk.Scrollbar(tree_frame, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscrollcommand=vs.set)
        self.tree.pack(side=LEFT, fill=BOTH, expand=True)
        vs.pack(side=RIGHT, fill=Y)

        self._load_prefs()
        self._refresh_installed_combo()
        self.ide.root.after(400, self._startup_tasks)

    def _load_prefs(self) -> None:
        s = load_settings(self.ide.portable_root)
        self._auto_var.set(bool(s.get("auto_check_updates", False)))
        tag = get_active_kern_tag(s)
        if tag:
            self._active_var.set(tag_to_dir_name(tag))
        else:
            self._active_var.set("(development PATH)")

    def _save_auto(self) -> None:
        s = load_settings(self.ide.portable_root)
        s["auto_check_updates"] = bool(self._auto_var.get())
        save_settings(s, self.ide.portable_root)

    def _on_active_changed(self, _e: object = None) -> None:
        val = self._active_var.get().strip()
        s = load_settings(self.ide.portable_root)
        if val == "(development PATH)":
            s.pop("active_kern_tag", None)
        else:
            s["active_kern_tag"] = val
        save_settings(s, self.ide.portable_root)
        self.ide.ide_settings = s
        self.ide._append_output(f"[kern] active compiler: {val}\n")
        append_log("runtime.log", f"active_kern_tag set to {val}", portable_root=self.ide.portable_root)
        self.ide._refresh_status()

    def _refresh_installed_combo(self) -> None:
        vr = kern_versions_dir(self.ide.portable_root)
        installed = list_installed_tags(vr)
        choices = ["(development PATH)"] + installed
        self._active_combo["values"] = choices
        cur = self._active_var.get()
        if cur not in choices:
            self._active_var.set("(development PATH)")
            self._on_active_changed()

    def _set_status(self, text: str) -> None:
        self.ide.root.after(0, lambda: self._status.configure(text=text))

    def _set_progress(self, mode: str, value: float = 0) -> None:
        def go() -> None:
            try:
                self._progress.stop()
            except Exception:
                pass
            self._progress.configure(mode=mode, maximum=100)
            if mode == "determinate":
                self._progress["value"] = value
            else:
                self._progress.start(12)

        self.ide.root.after(0, go)

    def _stop_progress(self) -> None:
        def stop() -> None:
            try:
                self._progress.stop()
            except Exception:
                pass
            try:
                self._progress.configure(mode="determinate", value=0)
            except Exception:
                pass

        self.ide.root.after(0, stop)

    def _startup_tasks(self) -> None:
        self._refresh_async()
        s = load_settings(self.ide.portable_root)
        if s.get("auto_check_updates"):
            threading.Thread(target=self._check_update_worker, daemon=True).start()

    def _check_update_worker(self) -> None:
        try:
            latest = fetch_latest_release_tag()
            if not latest:
                return
            installed = set(list_installed_tags(kern_versions_dir(self.ide.portable_root)))
            t = tag_to_dir_name(latest)
            if t not in installed:

                def notify() -> None:
                    self._banner.configure(
                        text=f"New Kern release available: {latest}  —  use 'Update to latest release' or install from the list."
                    )

                self.ide.root.after(0, notify)
        except Exception as exc:
            append_log("runtime.log", f"update check failed: {exc}", portable_root=self.ide.portable_root)

    def _refresh_async(self) -> None:
        if self._busy:
            return
        self._busy = True
        self._set_progress("indeterminate")
        self._set_status("Fetching releases from GitHub…")

        def work() -> None:
            try:
                rels = fetch_releases(per_page=40)
                self.ide.root.after(0, lambda: self._apply_releases(rels))
            except Exception as exc:
                self.ide.root.after(0, lambda: self._refresh_failed(exc))
            finally:
                self._busy = False
                self.ide.root.after(0, self._stop_progress)

        threading.Thread(target=work, daemon=True).start()

    def _refresh_failed(self, exc: BaseException) -> None:
        self._set_status(f"Could not fetch releases: {exc}")
        append_log("runtime.log", f"releases fetch failed: {exc}", portable_root=self.ide.portable_root)

    def _apply_releases(self, rels: list[KernRelease]) -> None:
        self._releases = rels
        vr = kern_versions_dir(self.ide.portable_root)
        installed = set(list_installed_tags(vr))
        self.tree.delete(*self.tree.get_children())
        for r in rels:
            asset = pick_windows_compiler_zip(r.assets)
            an = asset.name if asset else "—"
            tname = tag_to_dir_name(r.tag)
            st = "Installed" if tname in installed else "Not installed"
            if tname in installed:
                vd = version_dir(vr, r.tag)
                ok, _ = verify_installation(vd)
                if not ok:
                    st = "Broken"
            pub = r.published_at[:10] if r.published_at else "—"
            self.tree.insert("", END, values=(r.tag, pub, st, an))
        self._set_status(f"{len(rels)} release(s) listed from GitHub.")
        self._refresh_installed_combo()

    def _selected_release(self) -> KernRelease | None:
        sel = self.tree.selection()
        if not sel:
            self._set_status("Select a release row first.")
            return None
        item = sel[0]
        vals = self.tree.item(item, "values")
        if not vals:
            return None
        tag = str(vals[0])
        for r in self._releases:
            if r.tag == tag:
                return r
        return None

    def _install_selected(self, replace: bool = False) -> None:
        r = self._selected_release()
        if not r:
            return
        asset = pick_windows_compiler_zip(r.assets)
        if not asset or not asset.download_url:
            self._set_status("No suitable Windows .zip asset for this release.")
            return
        self._install_url(r.tag, asset.download_url, replace=replace)

    def _update_latest_async(self) -> None:
        if self._busy:
            return

        def work() -> None:
            try:
                rels = fetch_releases(per_page=5)
                if not rels:
                    self.ide.root.after(0, lambda: self._set_status("No releases returned."))
                    return
                latest = rels[0]
                asset = pick_windows_compiler_zip(latest.assets)
                if not asset or not asset.download_url:
                    self.ide.root.after(0, lambda: self._set_status("Latest release has no Windows compiler zip."))
                    return
                self.ide.root.after(0, lambda: self._install_url(latest.tag, asset.download_url, replace=False))
            except Exception as exc:
                self.ide.root.after(0, lambda: self._set_status(f"Update failed: {exc}"))

        threading.Thread(target=work, daemon=True).start()

    def _install_url(self, tag: str, url: str, replace: bool) -> None:
        if self._busy:
            self._set_status("Another operation is running.")
            return
        self._busy = True
        self._set_progress("determinate", 0)
        self._set_status(f"Downloading {tag}…")
        vr = kern_versions_dir(self.ide.portable_root)

        def progress(done: int, total: int | None) -> None:
            if total and total > 0:
                pct = min(99, int(100 * done / total))

                def upd() -> None:
                    self._progress["value"] = pct

                self.ide.root.after(0, upd)
            else:

                def pulse() -> None:
                    self._progress["value"] = (self._progress["value"] + 3) % 100

                self.ide.root.after(0, pulse)

        def work() -> None:
            ok = False
            msg = ""
            try:
                ok, msg = install_zip(
                    zip_url=url,
                    tag=tag,
                    versions_root=vr,
                    on_progress=progress,
                    portable_root=self.ide.portable_root,
                    replace=replace,
                )
            finally:

                def done_ui() -> None:
                    self._busy = False
                    self._stop_progress()
                    if ok:
                        self._set_status(f"Ready: {msg}")
                        s = load_settings(self.ide.portable_root)
                        s["active_kern_tag"] = tag_to_dir_name(tag)
                        s["last_seen_remote_tag"] = tag
                        save_settings(s, self.ide.portable_root)
                        self.ide.ide_settings = s
                        self._active_var.set(tag_to_dir_name(tag))
                        self._refresh_installed_combo()
                        self._apply_releases(self._releases)
                        self.ide._append_output(f"[kern] installed and activated {tag}\n")
                    else:
                        self._set_status(f"Install failed: {msg}")

                self.ide.root.after(0, done_ui)

        threading.Thread(target=work, daemon=True).start()
