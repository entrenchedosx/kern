## Kern IDE 0.3.0 — portable + version manager

### Portable layout

- **Frozen:** data lives next to `kern-ide.exe` (override with `KERN_IDE_HOME`).
- **From source:** `~/.kern_ide` on Windows.
- Folders: `kern_versions/`, `config/settings.json`, `logs/`, `projects/`.

### Kern versions (GitHub)

- **Kern versions** tab: list [compiler releases](https://github.com/entrenchedosx/kern/releases), download Windows zips, pick **Active Kern version**, optional **auto-check for updates**.
- Run / Check use the selected `kern.exe` under `kern_versions/<tag>/`, or **(development PATH)** with `KERN_EXE` / local build.

### UI

- Bottom tabs: **Output** (console + problems) and **Kern versions**.

### Asset

- **`kern-ide-windows-x64-portable-0.3.0.zip`** — extract anywhere; run `kern-ide.exe`.
