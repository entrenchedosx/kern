# Kern IDE

A **small, self-contained** desktop editor for the [Kern](https://github.com/entrenchedosx/kern) language: tabbed editor, workspace file tree, integrated run/check against `kern.exe`, syntax highlighting, diagnostics, and a command palette.

**Standalone home (Tk + Qt + VS Code tooling):** [github.com/entrenchedosx/kern-IDE](https://github.com/entrenchedosx/kern-IDE) — this Tk app is published there under **`desktop-tk/`** (see *Publishing* below).

## Requirements

- **Python 3.10+** with Tkinter (included with the official Windows/macOS installers; on Linux install `python3-tk`).
- A built **`kern.exe`** next to the repo (e.g. `build/Release/kern.exe`) or set **`KERN_EXE`** to its full path.

## Run (development)

From this folder (`Kern-IDE/`):

```powershell
python main.py
```

Or:

```powershell
python -c "from app import launch; launch()"
```

Working directory should be **`Kern-IDE`** so imports (`app`, `services`, `ui`) resolve.

## What’s included

| Area | Behavior |
|------|------------|
| **Layout** | Resizable panes: explorer · editor + breadcrumbs · optional debugger · output + problems |
| **Run / Check** | F5 runs the current saved file; Ctrl+K runs `kern --check --json` and shows **Problems** (double-click to jump) |
| **Command palette** | Ctrl+Shift+P — filtered list of actions |
| **Explorer** | Right-click: new file, rename, delete (under the workspace root) |
| **Preferences** | Font size, autosave interval (ms; `0` = off) |
| **Theme** | Light/dark (View menu or palette); persisted in `.kern-ide-state.json` |
| **Onboarding** | First-run welcome dialog (dismiss with “Got it”) |

## Workspace

The IDE picks a default **workspace root** (the Kern repo root when running from a checkout). Use **File → Open workspace…** to point at a project folder that contains `lib/kern` and your `.kn` files. State is stored in **`<workspace>/.kern-ide-state.json`**.

## Packaging

See `packaging/` for PyInstaller specs (`kern-ide.spec`). Build a one-folder or one-file bundle after installing PyInstaller.

## Publishing to [entrenchedosx/kern-IDE](https://github.com/entrenchedosx/kern-IDE.git)

That repository uses a multi-package layout:

| Folder | Contents |
|--------|----------|
| **`desktop-tk/`** | This Tk desktop IDE (mirror of monorepo `Kern-IDE/`) |
| `native-qt/` | Native Qt tooling |
| `vscode-extension/` | VS Code extension |

**Recommended:** clone the standalone repo, **sync** the monorepo `Kern-IDE/` into `desktop-tk/`, then commit and push:

```powershell
# From the main kern repo root (adjust -Destination to your clone path):
git clone https://github.com/entrenchedosx/kern-IDE.git
.\scripts\sync_kern_ide_to_desktop_tk.ps1 -Destination "D:\path\to\kern-IDE\desktop-tk"
cd D:\path\to\kern-IDE
git add desktop-tk
git commit -m "Sync desktop-tk from kern monorepo"
git push origin main
```

`git subtree push` from the monorepo targets the **root** of the remote and does **not** match `desktop-tk/`; use the script or a manual copy instead.

CI for the Tk editor lives in the main kern repo (`.github/workflows/kern-ide-tk.yml`) and runs against `Kern-IDE/`.

## Code layout

- `app/ide.py` — main window, menus, layout, problems list, palette, preferences
- `app/editor.py` — `Text` buffer, highlighting, diagnostics underlines, autocomplete
- `app/runner.py` — locate `kern.exe`, stream run output, `kern --check --json`
- `services/` — diagnostics parsing, completion data, etc.
- `ui/` — command palette, tooltips

## Limitations (honest)

- No stepping debugger or breakpoints yet (panel reserved).
- No LSP; completion is local-word / keyword based.
- Large files are not optimized for huge performance tuning.

Pull requests that keep dependencies at **stdlib + optional PyInstaller** are welcome.
