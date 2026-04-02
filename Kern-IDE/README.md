# Kern IDE

A **small, self-contained** desktop editor for the [Kern](https://github.com/entrenchedosx/kern) language: tabbed editor, workspace file tree, integrated run/check against `kern.exe`, syntax highlighting, diagnostics, and a command palette.

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

## Publishing to a dedicated **kern-IDE** GitHub repo

This tree often lives under the main Kern repository as `Kern-IDE/`. To mirror **only** this subtree to another remote (replace URL and branch names as needed):

```bash
git subtree split --prefix=Kern-IDE -b kern-ide-export
git remote add kern-ide https://github.com/YOUR_ORG/kern-IDE.git
git push kern-ide kern-ide-export:main
```

Or use `git filter-repo` / manual copy. Ensure CI (`.github/workflows/kern-ide-tk.yml`) runs from the split repo root if you split it out.

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
