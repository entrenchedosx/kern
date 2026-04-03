# Kern IDE (Electron)

A **VS Code–style** desktop shell for the [Kern](https://github.com/entrenchedosx/kern) language: **Electron** (main ↔ preload IPC), **React + TypeScript + Vite** UI, **Monaco** editor, workbench layout, command palette, quick open, virtualized explorer, filesystem watch, **worker-thread** workspace scan, and a small **extension** loader.

## Requirements

- Node.js 20+
- npm 9+

## Scripts

| Command | Purpose |
|--------|---------|
| `npm install` | Install dependencies |
| `npm run dev` | Dev: Vite + Electron (hot reload for renderer) |
| `npm run build` | Production bundles (`out/`) + `scan-worker.mjs` |
| `npm run dist` | Build + `electron-builder` installers/zips (`release/`) |

## Architecture

- **Main process** (`src/main/`): window lifecycle, secure FS IPC (paths confined to the open workspace), **chokidar** watching, command execution, theme persistence, **dynamic `import()`** of extensions.
- **Preload** (`src/preload/`): `contextBridge.exposeInMainWorld('kernAPI', …)` — the only bridge to the renderer.
- **Renderer** (`src/renderer/`): React workbench; no Node APIs.
- **Heavy scan**: recursive file list uses a **worker thread** (`out/main/workers/scan-worker.mjs`) when present; otherwise falls back to an async walk on the main thread (dev-friendly).
- **Performance**: virtualized explorer and palette/quick-open lists (`@tanstack/react-virtual`); **lazy** `Problems` panel chunk; Monaco in a separate rollup chunk.

## `window.kernAPI`

Typed in `src/renderer/src/kern/kern-api.ts`. Includes folder dialog, `readDir` / `readFile` / `writeFile`, `watchStart` / `watchStop`, `scanWorkspace`, command list/execute, theme get/set/list, custom theme JSON (under userData `themes/`), extension reload, and event unsubscribers for watch + workbench commands.

## Extensions

Drop folders under `extensions/` (repo) or `%APPDATA%/kern-electron-ide/extensions` (name varies by OS) with:

- `kern-extension.json` **or** `package.json` field `kernExtension` — `name`, optional `main`, optional `contributes.commands`.
- Optional `main` module (ESM) exporting `activate(api)` with `api.registerCommand(id, handler)`.

See `extensions/sample/` for a minimal example.

## Theming

- Built-in **light** / **dark** (CSS variables on `document.documentElement` + Monaco `kern-light` / `kern-dark`).
- **Custom**: place `.json` under the userData `themes/` folder. Top-level `colors` map sets CSS custom properties (e.g. `"--kern-bg": "#1a1a1a"`).

## Python / Tk IDE

The original **Kern-IDE** desktop app (`../` — Tk) is unchanged and remains the default documented portable build until this Electron app is promoted in release workflows.
