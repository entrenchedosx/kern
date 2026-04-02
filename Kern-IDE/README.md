# Kern IDE

A **small, self-contained** desktop editor for the [Kern](https://github.com/entrenchedosx/kern) language: tabbed editor, workspace file tree, integrated run/check against `kern.exe`, syntax highlighting, diagnostics, and a command palette.

**Standalone home (Tk + Qt + VS Code tooling):** [github.com/entrenchedosx/kern-IDE](https://github.com/entrenchedosx/kern-IDE) — this Tk app is published there under **`desktop-tk/`** (see *Publishing* below). Release version is in **`VERSION`** (shown in the window title and About).

## Requirements

- **Python 3.10+** with Tkinter (included with the official Windows/macOS installers; on Linux install `python3-tk`).
- **Compiler:** use the **Kern versions** tab to download Windows zips from [GitHub Releases](https://github.com/entrenchedosx/kern/releases), *or* point at a local build (`build/Release/kern.exe`) / **`KERN_EXE`** when **Active Kern version** is set to **(development PATH)**.

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
| **Portable root** | Frozen EXE: folder next to `kern-ide.exe`. Dev: `~/.kern_ide`. Override with **`KERN_IDE_HOME`**. Subfolders: `kern_versions/`, `config/settings.json`, `logs/`, `projects/` |
| **Kern versions** | Tab lists [entrenchedosx/kern](https://github.com/entrenchedosx/kern) releases; download/install zips; active version dropdown; auto-check for updates |
| **Layout** | Explorer · editor + breadcrumbs · optional debugger · bottom tabs **Output** (console + problems) and **Kern versions** |
| **Run / Check** | F5 / Ctrl+K use the **active** `kern.exe` under `kern_versions/<tag>/` or dev PATH |
| **Command palette** | Ctrl+Shift+P — filtered list of actions |
| **Explorer** | Right-click: new file, rename, delete (under the workspace root) |
| **Preferences** | Font size, autosave interval (ms; `0` = off) |
| **Theme** | Light/dark (View menu or palette); persisted in `.kern-ide-state.json` |
| **Onboarding** | First-run welcome dialog (dismiss with “Got it”) |

## Workspace

- **Packaged (frozen):** default workspace is **`projects/`** under the portable root (created on launch).
- **From source:** default workspace is the Kern **repository root** (parent of `Kern-IDE/`).
- Use **File → Open workspace…** for a project folder that contains `lib/kern` and your `.kn` files. UI state is stored in **`<workspace>/.kern-ide-state.json`**; compiler selection lives in **`config/settings.json`** under the portable root.

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

- `app/ide.py` — main window, menus, layout, problems list, palette, preferences, portable root
- `app/version_panel.py` — GitHub release list, downloads, active version UI
- `app/editor.py` — `Text` buffer, highlighting, diagnostics underlines, autocomplete
- `app/runner.py` — resolves active `kern.exe` (managed install or dev PATH), run/check
- `services/portable_env.py`, `ide_settings.py`, `ide_logging.py` — layout, `config/settings.json`, `logs/*.log`
- `services/github_kern_releases.py`, `kern_version_store.py` — API + zip install / verify
- `services/` — diagnostics, completion data, etc.
- `ui/` — command palette, tooltips

## Limitations (honest)

- No stepping debugger or breakpoints yet (panel reserved).
- No LSP; completion is local-word / keyword based.
- Large files are not optimized for huge performance tuning.

Pull requests that keep dependencies at **stdlib + optional PyInstaller** are welcome.
