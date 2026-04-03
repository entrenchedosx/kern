# Kern IDE extensions

Each extension is a **folder** containing:

1. **`kern-extension.json`** (or `package.json` with a `kernExtension` object) with at least `"name"`.
2. Optional **`main`** entry (default `main.js`): an **ES module** that exports `activate(api)`.
3. Optional **`contributes.commands`**: palette entries; handlers are no-ops until `activate` registers them with the same command id.

The main process loads extensions from:

- `extensions/` next to the app in development (this repo).
- Packaged: `resources/extensions/` (via `electron-builder` `extraResources`).
- User scope: `<userData>/extensions/`.

See **`sample/`** for a working example.
