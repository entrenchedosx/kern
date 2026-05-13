"""Main PySide6 window: Kern → EXE build UI."""

from __future__ import annotations

import json
import os
import shutil
import sys
from pathlib import Path

from PySide6.QtCore import QProcess, QProcessEnvironment, Qt, QUrl, Signal
from PySide6.QtGui import QAction, QDesktopServices, QDragEnterEvent, QDropEvent
from PySide6.QtWidgets import (
    QAbstractItemView,
    QCheckBox,
    QComboBox,
    QFileDialog,
    QFormLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QListWidget,
    QListWidgetItem,
    QMainWindow,
    QMessageBox,
    QPlainTextEdit,
    QPushButton,
    QSpinBox,
    QSplitter,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
)

from kern_build_gui.profile import GuiProfile, load_profile, save_profile
from kern_build_gui.kern_config import BuildSettings, write_kernconfig
from kern_build_gui.kernc_locator import locate_kernc


def suggest_project_root(files: list[str]) -> str:
    if not files:
        return str(Path.cwd())
    paths = [Path(f).resolve() for f in files if Path(f).is_file()]
    if not paths:
        return str(Path.cwd())
    try:
        return str(Path(os.path.commonpath([str(p) for p in paths])))
    except ValueError:
        return str(paths[0].parent)


class FileDropListWidget(QListWidget):
    """List with internal reorder and external .kn file drops."""

    files_dropped = Signal(list)

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setAcceptDrops(True)
        self.setDragEnabled(True)
        self.setDropIndicatorShown(True)
        self.setDragDropMode(QAbstractItemView.DragDrop)
        self.setDefaultDropAction(Qt.MoveAction)
        self.setSelectionMode(QAbstractItemView.ExtendedSelection)

    def dragEnterEvent(self, e: QDragEnterEvent) -> None:
        if e.mimeData().hasUrls():
            e.acceptProposedAction()
        else:
            super().dragEnterEvent(e)

    def dragMoveEvent(self, e) -> None:  # noqa: ANN001
        if e.mimeData().hasUrls():
            e.acceptProposedAction()
        else:
            super().dragMoveEvent(e)

    def dropEvent(self, e: QDropEvent) -> None:
        if e.mimeData().hasUrls():
            added: list[str] = []
            for u in e.mimeData().urls():
                p = Path(u.toLocalFile())
                if p.is_file() and p.suffix.lower() == ".kn":
                    added.append(str(p.resolve()))
            if added:
                self.files_dropped.emit(added)
            e.acceptProposedAction()
            return
        super().dropEvent(e)


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("Kern Build GUI — Kern → EXE")
        self.resize(980, 720)

        self._process: QProcess | None = None
        self._diag_path: Path | None = None

        central = QWidget()
        self.setCentralWidget(central)
        root_layout = QVBoxLayout(central)

        splitter = QSplitter(Qt.Vertical)
        root_layout.addWidget(splitter, 1)

        top = QWidget()
        top_l = QHBoxLayout(top)
        splitter.addWidget(top)

        # left: files + entry
        left_col = QVBoxLayout()
        top_l.addLayout(left_col, 1)

        grp_files = QGroupBox("Input files (.kn)")
        gf_l = QVBoxLayout(grp_files)
        self.file_list = FileDropListWidget()
        self.file_list.files_dropped.connect(self._on_external_drop)
        gf_l.addWidget(self.file_list)
        row_btns = QHBoxLayout()
        self.btn_add = QPushButton("Add files…")
        self.btn_add_folder = QPushButton("Add folder…")
        self.btn_remove = QPushButton("Remove")
        self.btn_up = QPushButton("Up")
        self.btn_down = QPushButton("Down")
        row_btns.addWidget(self.btn_add)
        row_btns.addWidget(self.btn_add_folder)
        row_btns.addWidget(self.btn_remove)
        row_btns.addWidget(self.btn_up)
        row_btns.addWidget(self.btn_down)
        row_btns.addStretch(1)
        gf_l.addLayout(row_btns)
        left_col.addWidget(grp_files)

        grp_entry = QGroupBox("Entry point")
        ge_l = QFormLayout(grp_entry)
        self.entry_combo = QComboBox()
        self.entry_combo.setMinimumContentsLength(40)
        ge_l.addRow("Main file:", self.entry_combo)
        left_col.addWidget(grp_entry)

        # right: project root, output, options
        right_col = QVBoxLayout()
        top_l.addLayout(right_col, 1)

        grp_root = QGroupBox("Project")
        gr_l = QHBoxLayout(grp_root)
        self.project_root_edit = QPlainTextEdit()
        self.project_root_edit.setMaximumHeight(60)
        self.project_root_edit.setPlaceholderText("Directory used as include root (working directory for kern)")
        self.btn_root = QPushButton("Browse…")
        gr_l.addWidget(self.project_root_edit, 1)
        gr_l.addWidget(self.btn_root)
        right_col.addWidget(grp_root)

        grp_out = QGroupBox("Output")
        go_l = QHBoxLayout(grp_out)
        self.output_edit = QPlainTextEdit()
        self.output_edit.setMaximumHeight(60)
        self.output_edit.setPlaceholderText("Output .exe path")
        self.btn_out = QPushButton("Browse…")
        go_l.addWidget(self.output_edit, 1)
        go_l.addWidget(self.btn_out)
        right_col.addWidget(grp_out)

        grp_opt = QGroupBox("Build options")
        gof = QFormLayout(grp_opt)
        self.chk_release = QCheckBox("Release (uncheck for Debug)")
        self.chk_release.setChecked(True)
        self.spin_opt = QSpinBox()
        self.spin_opt.setRange(0, 3)
        self.spin_opt.setValue(2)
        self.chk_console = QCheckBox("Console subsystem")
        self.chk_console.setChecked(True)
        self.chk_force = QCheckBox("Force full rebuild (delete output + .kern-cache)")
        self.chk_force.setChecked(False)
        gof.addRow(self.chk_release)
        gof.addRow("Optimization (0–3):", self.spin_opt)
        gof.addRow(self.chk_console)
        gof.addRow(self.chk_force)
        right_col.addWidget(grp_opt)
        right_col.addStretch(1)

        # build actions
        build_row = QHBoxLayout()
        self.btn_build = QPushButton("Build EXE")
        self.btn_build.setMinimumHeight(36)
        self.btn_rebuild = QPushButton("Rebuild")
        self.btn_rebuild.setMinimumHeight(36)
        self.btn_open_out = QPushButton("Open output folder")
        self.btn_clear_log = QPushButton("Clear log")
        build_row.addWidget(self.btn_build)
        build_row.addWidget(self.btn_rebuild)
        build_row.addWidget(self.btn_open_out)
        build_row.addWidget(self.btn_clear_log)
        build_row.addStretch(1)
        root_layout.addLayout(build_row)

        # diagnostics table
        grp_diag = QGroupBox("Diagnostics (double-click row to open file)")
        gd_l = QVBoxLayout(grp_diag)
        self.diag_table = QTableWidget(0, 5)
        self.diag_table.setHorizontalHeaderLabels(["Severity", "File", "Line", "Code", "Message"])
        self.diag_table.horizontalHeader().setStretchLastSection(True)
        gd_l.addWidget(self.diag_table)
        splitter.addWidget(grp_diag)

        # log
        grp_log = QGroupBox("Console output")
        gl_l = QVBoxLayout(grp_log)
        self.log = QPlainTextEdit()
        self.log.setReadOnly(True)
        self.log.setFont(self.log.font())
        self.log.setMinimumHeight(160)
        f = self.log.font()
        f.setFamily("Consolas")
        self.log.setFont(f)
        gl_l.addWidget(self.log)
        btn_save_log = QPushButton("Save log…")
        gl_l.addWidget(btn_save_log)
        splitter.addWidget(grp_log)
        splitter.setSizes([420, 140, 200])

        # menu
        mb = self.menuBar()
        file_m = mb.addMenu("&File")
        act_load = QAction("Load profile…", self)
        act_save = QAction("Save profile…", self)
        act_quit = QAction("Quit", self)
        file_m.addAction(act_load)
        file_m.addAction(act_save)
        file_m.addSeparator()
        file_m.addAction(act_quit)

        help_m = mb.addMenu("&Help")
        act_about = QAction("About", self)
        help_m.addAction(act_about)

        # connections
        self.btn_add.clicked.connect(self._add_files)
        self.btn_add_folder.clicked.connect(self._add_folder)
        self.btn_remove.clicked.connect(self._remove_selected)
        self.btn_up.clicked.connect(self._move_up)
        self.btn_down.clicked.connect(self._move_down)
        self.btn_root.clicked.connect(self._browse_root)
        self.btn_out.clicked.connect(self._browse_output)
        self.btn_build.clicked.connect(lambda: self._start_build(rebuild=False))
        self.btn_rebuild.clicked.connect(lambda: self._start_build(rebuild=True))
        self.btn_open_out.clicked.connect(self._open_output_dir)
        self.btn_clear_log.clicked.connect(self.log.clear)
        btn_save_log.clicked.connect(self._save_log)
        act_load.triggered.connect(self._load_profile)
        act_save.triggered.connect(self._save_profile)
        act_quit.triggered.connect(self.close)
        act_about.triggered.connect(self._about)
        self.diag_table.cellDoubleClicked.connect(self._diag_double_click)

        self._sync_entry_combo()

    def _append_log(self, text: str) -> None:
        self.log.moveCursor(self.log.textCursor().MoveOperation.End)
        self.log.insertPlainText(text)
        self.log.moveCursor(self.log.textCursor().MoveOperation.End)

    def _file_paths(self) -> list[str]:
        out: list[str] = []
        for i in range(self.file_list.count()):
            it = self.file_list.item(i)
            if it:
                out.append(it.text())
        return out

    def _sync_entry_combo(self) -> None:
        cur = self.entry_combo.currentText()
        self.entry_combo.clear()
        for p in self._file_paths():
            self.entry_combo.addItem(p)
        if cur and self.entry_combo.findText(cur) >= 0:
            self.entry_combo.setCurrentText(cur)
        elif self.entry_combo.count() == 1:
            self.entry_combo.setCurrentIndex(0)

    def _on_external_drop(self, paths: list[str]) -> None:
        existing = set(self._file_paths())
        for p in paths:
            if p not in existing:
                self.file_list.addItem(QListWidgetItem(p))
                existing.add(p)
        pr = self.project_root_edit.toPlainText().strip()
        if not pr:
            self.project_root_edit.setPlainText(suggest_project_root(self._file_paths()))
        self._sync_entry_combo()

    def _add_files(self) -> None:
        files, _ = QFileDialog.getOpenFileNames(self, "Add Kern files", "", "Kern files (*.kn);;All files (*)")
        self._on_external_drop([str(Path(f).resolve()) for f in files])

    def _add_folder(self) -> None:
        d = QFileDialog.getExistingDirectory(self, "Folder to scan for .kn")
        if not d:
            return
        root = Path(d)
        found = sorted(root.rglob("*.kn"))
        self._on_external_drop([str(p.resolve()) for p in found])

    def _remove_selected(self) -> None:
        for it in self.file_list.selectedItems():
            self.file_list.takeItem(self.file_list.row(it))
        self._sync_entry_combo()
        if not self._file_paths():
            self.entry_combo.clear()

    def _move_up(self) -> None:
        row = self.file_list.currentRow()
        if row <= 0:
            return
        it = self.file_list.takeItem(row)
        self.file_list.insertItem(row - 1, it)
        self.file_list.setCurrentRow(row - 1)
        self._sync_entry_combo()

    def _move_down(self) -> None:
        row = self.file_list.currentRow()
        if row < 0 or row >= self.file_list.count() - 1:
            return
        it = self.file_list.takeItem(row)
        self.file_list.insertItem(row + 1, it)
        self.file_list.setCurrentRow(row + 1)
        self._sync_entry_combo()

    def _browse_root(self) -> None:
        d = QFileDialog.getExistingDirectory(self, "Project root", self.project_root_edit.toPlainText().strip())
        if d:
            self.project_root_edit.setPlainText(str(Path(d).resolve()))

    def _browse_output(self) -> None:
        path, _ = QFileDialog.getSaveFileName(
            self, "Output executable", self.output_edit.toPlainText().strip(), "Executable (*.exe);;All files (*)"
        )
        if path:
            self.output_edit.setPlainText(str(Path(path).resolve()))

    def _open_output_dir(self) -> None:
        out = self.output_edit.toPlainText().strip()
        if not out:
            return
        p = Path(out).parent
        if p.is_dir():
            QDesktopServices.openUrl(QUrl.fromLocalFile(str(p)))

    def _save_log(self) -> None:
        path, _ = QFileDialog.getSaveFileName(self, "Save log", "", "Text (*.txt);;All files (*)")
        if path:
            Path(path).write_text(self.log.toPlainText(), encoding="utf-8")

    def _gather_settings(self) -> BuildSettings | None:
        files = self._file_paths()
        entry = self.entry_combo.currentText().strip()
        root = self.project_root_edit.toPlainText().strip()
        output = self.output_edit.toPlainText().strip()
        bs = BuildSettings(
            entry=entry,
            output=output,
            project_root=root,
            files=files,
            release=self.chk_release.isChecked(),
            optimization=self.spin_opt.value(),
            console=self.chk_console.isChecked(),
        )
        errs = bs.validate()
        if errs:
            QMessageBox.warning(self, "Invalid settings", "\n".join(errs))
            return None
        if entry not in files:
            r = QMessageBox.question(
                self,
                "Entry not in list",
                "Entry file is not in the input list. Continue anyway?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            )
            if r != QMessageBox.StandardButton.Yes:
                return None
        return bs

    def _apply_force_rebuild(self, output: str) -> None:
        outp = Path(output)
        cache_dir = outp.parent / ".kern-cache"
        try:
            if outp.is_file():
                outp.unlink()
        except OSError as e:
            self._append_log(f"[gui] could not delete output: {e}\n")
        if cache_dir.is_dir():
            try:
                shutil.rmtree(cache_dir)
                self._append_log(f"[gui] removed cache: {cache_dir}\n")
            except OSError as e:
                self._append_log(f"[gui] could not remove cache: {e}\n")

    def _start_build(self, *, rebuild: bool) -> None:
        if self._process is not None and self._process.state() != QProcess.ProcessState.NotRunning:
            QMessageBox.information(self, "Busy", "A build is already running.")
            return

        kern = locate_kernc()
        if not kern:
            QMessageBox.critical(
                self,
                "kern not found",
                "Could not find kern.\nSet KERNC_EXE or build the compiler (build/Release/kernc.exe).",
            )
            return

        bs = self._gather_settings()
        if not bs:
            return

        if rebuild or self.chk_force.isChecked():
            self._apply_force_rebuild(bs.output)

        root = Path(bs.project_root)
        cfg_path = root / ".kn-build-gui-config.json"
        self._diag_path = root / ".kn-build-gui-diagnostics.json"
        try:
            write_kernconfig(cfg_path, bs)
        except OSError as e:
            QMessageBox.critical(self, "Config", f"Failed to write config:\n{e}")
            return

        self.diag_table.setRowCount(0)
        self._append_log(f"\n--- Build started ---\n")
        self._append_log(f"$ {kern} --config {cfg_path}\n")
        self._append_log(f"cwd: {root}\n")

        self._set_build_ui_busy(True)

        proc = QProcess(self)
        self._process = proc
        proc.setWorkingDirectory(str(root))
        proc.setProgram(kern)
        args = ["--config", str(cfg_path), "--build-diagnostics-json", str(self._diag_path)]
        proc.setArguments(args)

        env = QProcessEnvironment.systemEnvironment()
        proc.setProcessEnvironment(env)

        proc.readyReadStandardOutput.connect(lambda: self._read_stream(proc, out=True))
        proc.readyReadStandardError.connect(lambda: self._read_stream(proc, out=False))
        proc.finished.connect(self._on_finished)

        proc.start()
        if not proc.waitForStarted(5000):
            self._append_log("[gui] failed to start kern\n")
            self._set_build_ui_busy(False)
            self._process = None

    def _read_stream(self, proc: QProcess, *, out: bool) -> None:
        if out:
            data = proc.readAllStandardOutput().data().decode("utf-8", errors="replace")
            self._append_log(data)
        else:
            data = proc.readAllStandardError().data().decode("utf-8", errors="replace")
            self._append_log(data)

    def _load_diagnostics(self) -> None:
        self.diag_table.setRowCount(0)
        if not self._diag_path or not self._diag_path.is_file():
            return
        try:
            data = json.loads(self._diag_path.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError):
            return
        if not isinstance(data, list):
            return
        for item in data:
            if not isinstance(item, dict):
                continue
            row = self.diag_table.rowCount()
            self.diag_table.insertRow(row)
            self.diag_table.setItem(row, 0, QTableWidgetItem(str(item.get("severity", ""))))
            self.diag_table.setItem(row, 1, QTableWidgetItem(str(item.get("file", ""))))
            self.diag_table.setItem(row, 2, QTableWidgetItem(str(item.get("line", ""))))
            self.diag_table.setItem(row, 3, QTableWidgetItem(str(item.get("code", ""))))
            self.diag_table.setItem(row, 4, QTableWidgetItem(str(item.get("message", ""))))

    def _on_finished(self, code: int, status: QProcess.ExitStatus) -> None:
        self._append_log(f"\n--- Exit code {code} ---\n")
        self._load_diagnostics()
        self._set_build_ui_busy(False)
        self._process = None

    def _set_build_ui_busy(self, busy: bool) -> None:
        self.btn_build.setEnabled(not busy)
        self.btn_rebuild.setEnabled(not busy)
        for w in (
            self.file_list,
            self.btn_add,
            self.btn_add_folder,
            self.btn_remove,
            self.btn_up,
            self.btn_down,
            self.entry_combo,
            self.project_root_edit,
            self.output_edit,
            self.btn_root,
            self.btn_out,
            self.chk_release,
            self.spin_opt,
            self.chk_console,
            self.chk_force,
        ):
            w.setEnabled(not busy)

    def _diag_double_click(self, row: int, col: int) -> None:
        del col
        fp = self.diag_table.item(row, 1)
        if not fp:
            return
        path = fp.text().strip()
        if path and Path(path).is_file():
            QDesktopServices.openUrl(QUrl.fromLocalFile(path))

    def _load_profile(self) -> None:
        path, _ = QFileDialog.getOpenFileName(self, "Load profile", "", "Kern Build profile (*.kernbuild.json);;JSON (*.json)")
        if not path:
            return
        try:
            prof = load_profile(Path(path))
        except (OSError, ValueError, json.JSONDecodeError) as e:
            QMessageBox.critical(self, "Profile", str(e))
            return
        self.file_list.clear()
        for f in prof.files:
            if Path(f).is_file():
                self.file_list.addItem(QListWidgetItem(f))
        self.project_root_edit.setPlainText(prof.project_root)
        self.output_edit.setPlainText(prof.output)
        self.chk_release.setChecked(prof.release)
        self.spin_opt.setValue(prof.optimization)
        self.chk_console.setChecked(prof.console)
        self._sync_entry_combo()
        if prof.entry and self.entry_combo.findText(prof.entry) >= 0:
            self.entry_combo.setCurrentText(prof.entry)

    def _save_profile(self) -> None:
        path, _ = QFileDialog.getSaveFileName(self, "Save profile", "", "Kern Build profile (*.kernbuild.json)")
        if not path:
            return
        if not path.endswith(".json"):
            path += ".kernbuild.json"
        prof = GuiProfile(
            project_root=self.project_root_edit.toPlainText().strip(),
            files=self._file_paths(),
            entry=self.entry_combo.currentText().strip(),
            output=self.output_edit.toPlainText().strip(),
            release=self.chk_release.isChecked(),
            optimization=self.spin_opt.value(),
            console=self.chk_console.isChecked(),
        )
        try:
            save_profile(Path(path), prof)
        except OSError as e:
            QMessageBox.critical(self, "Profile", str(e))

    def _about(self) -> None:
        QMessageBox.about(
            self,
            "About Kern Build GUI",
            "Kern Build GUI — drives kern with kernconfig.json.\n"
            "Set KERNC_EXE if kern is not auto-detected.\n\n"
            "Profiles: .kernbuild.json",
        )

    def closeEvent(self, event) -> None:  # noqa: ANN001
        if self._process is not None and self._process.state() != QProcess.ProcessState.NotRunning:
            self._process.terminate()
        event.accept()


def main() -> None:
    from PySide6.QtWidgets import QApplication

    app = QApplication(sys.argv)
    app.setApplicationName("Kern Build GUI")
    w = MainWindow()
    w.show()
    sys.exit(app.exec())
