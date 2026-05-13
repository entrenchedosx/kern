# Kern Build GUI

Desktop app (PySide6) to build **Kern → native EXE** using the existing **`kern`** compiler: pick files, entry point, output path, options, then **Build EXE** with a live log and diagnostics table.

## Requirements

- Python 3.10+
- `pip install -r requirements.txt`
- Built **`kern`** (Windows: `build/Release/kernc.exe`, or set **`KERNC_EXE`** to `kern` / `kern-impl`)

## Run

From the repository root:

```powershell
cd kern-build-gui
pip install -r requirements.txt
python -m kern_build_gui
```

Or double-click / run `kern-build-gui.bat` from this folder (uses `python -m kern_build_gui`).

## Features

- Multi-file list (add / remove / reorder, drag-and-drop `.kn` from Explorer)
- Entry point selector
- Project root (include path for `kern`), output `.exe` path
- Release / Debug, optimization 0–3, console / no-console
- Force full rebuild (deletes output and `.kern-cache` next to the output)
- **`kern --config`** with generated `.kn-build-gui-config.json` in the project root
- **`--build-diagnostics-json`** → `.kn-build-gui-diagnostics.json` (parsed into the diagnostics table; double-click opens the file)
- Profiles: **File → Save/Load** `*.kernbuild.json`

## kernconfig fields written

The GUI writes JSON compatible with [`kernconfig`](../kern/core/utils/kernconfig.hpp): `entry`, `output`, `include_paths`, `release`, `optimization`, `console`, and optional **`files`** (explicit list; each must be reachable from the entry via imports).

## Packaging (Windows, PyInstaller)

From the repo root, **`build\build_ecosystem.ps1`** builds `FINAL\kern-gui\` (onedir: `kern-gui.exe` + bundled Qt). Or manually:

```powershell
pip install -r requirements.txt pyinstaller
cd kern-build-gui
pyinstaller --noconfirm packaging\kern-gui.spec
```

When frozen next to **`FINAL\kern\kernc.exe`**, the GUI resolves `kern` by walking up from the executable directory to find a `kern\kern.exe` folder. Override anytime with **`KERNC_EXE`**.

## Troubleshooting

- If **`kern` not found**, set `KERNC_EXE` to your compiler executable or build the project (`cmake --build build --config Release --target kernc`).
- On Windows, prefer `build\Release\kern.exe` (launcher) or `FINAL\kern\kernc.exe` after an ecosystem build.
