# Kargo (native)

The in-repo **`kargo`** executable is built by CMake alongside **`kern`**. It replaces the legacy Node-based CLI for normal workflows.

## Kern install root vs project directory

**Project files** (current working directory):

- `kargo.json` — dependencies you declare  
- `kargo.lock` — generated lockfile  

**Kern environment** (resolved install root — same rules as `kern`):

1. `--root <path>` (passed before the subcommand) — **strict** portable layout only  
2. `KERN_HOME` — **strict** layout (`kern` + `kargo` + `lib/kern` + `runtime`)  
3. Exe directory, then `config/env.json`, then validated cache — see [getting-started.md](getting-started.md)  
4. **Debug** `kargo` only: parent walk to a repo with `lib/kern/`  

**Packages** are installed under the Kern root, not next to your project:

```text
kern-<version>/
  packages/          ← cloned dependencies live here
  config/
    package-paths.json   ← import map (paths relative to this Kern root)
```

So you can run `kargo` from any project folder; installs stay inside the active Kern environment (like pip/npm global site-packages tied to one Python/Node install).

## Manifests

**`kargo.json`** (project directory):

```json
{
  "dependencies": {
    "owner/repo": "v1.0.0"
  }
}
```

The value is a **Git tag or branch** (default `main` if empty).

**`kargo.lock`** — generated with sorted keys (`lockfile_version` 2): `ref`, relative `path`, `commit`, plus **`version`** (same as the locked ref for now) and **`source`** (`github:owner/repo`) for reproducibility and future update logic.

## Commands

| Command | Purpose |
|---------|---------|
| `kargo install owner/repo[@tag]` | Fetch into `<KernRoot>/packages/`, update manifests and `config/package-paths.json` |
| `kargo remove owner/repo` | Remove dependency and tree under `<KernRoot>/packages/` |
| `kargo update` | Re-fetch all dependencies from `kargo.json` |
| `kargo list` | Show locked packages |

## Imports in `.kn` files

Use the same key as in `kargo.json`, e.g. `import "owner/repo"` — resolution uses `config/package-paths.json` (see [getting-started.md](getting-started.md)). Entries use **paths relative to the Kern root** so you can move the whole `kern-<version>/` folder without rewriting the file.

## Requirements

- **`git`** on `PATH` for the preferred `git clone` path.  
- If `git` fails, **kargo** tries a **GitHub archive `.zip`** download via **curl**, then unpacks with **tar** (or **PowerShell Expand-Archive** on Windows).  
- For finding `main` entry: `index.kn`, `main.kn`, or `src/index.kn` inside the package directory.

## Legacy Node Kargo

The `kargo/` npm package and `kargo.toml` workflow remain in the repository for migration reference; new projects should use **`kargo.json`** and native **`kargo`**.
