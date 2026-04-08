# kern-bootstrap

Production-grade bootstrapper for **Kern** and **Kargo**. It downloads matching artifacts from a GitHub release (default [`entrenchedosx/kern`](https://github.com/entrenchedosx/kern)), verifies **Kargo** with `kargo-SHA256SUMS` and **Kern** with `kern-SHA256SUMS` when that file exists on the release, extracts with path-traversal checks, normalizes trees to `versions/<tag>/kern` and `versions/<tag>/kargo`, smoke-tests `kern --version`, and **rejects** Kern zips built without **Raylib** (`graphics: none` from `kern --version`) so `import("g2d")` / `import("g3d")` / `import("game")` work. Official release CI builds with **`KERN_BUILD_GAME=ON`**. Then it **renames** the staged tree into `versions/<tag>/` and switches the active pointer (`current` symlink on Unix, `active-release.txt` + optional directory junction on Windows) before refreshing `bin/` launchers. A cross-process lock (`<prefix>/.install.lock`) blocks concurrent installs.

**Versioning:** The repo’s canonical toolchain version is **`KERN_VERSION.txt`** at the repository root (same value as `kern --version`). `kern-bootstrap --version` and `kern-bootstrap/Cargo.toml`’s `version` must stay identical to that file; the build script errors if they drift. On release, also match **`kargo/package.json`**, **`kern-registry/package.json`**, and root **`kern.json`** (see **`docs/RELEASE_CHECKLIST.md`**).

## Install (from GitHub Releases)

On each `v*` release, CI publishes executables named like:

- `kern-bootstrap-<semver>-linux-x64`
- `kern-bootstrap-<semver>-windows-x64.exe`
- `kern-bootstrap-<semver>-macos-arm64`
- `kern-bootstrap-<semver>-macos-x64`

Download the file for your OS, then:

**Linux / macOS**

```bash
chmod +x kern-bootstrap-*-linux-x64   # or macos-*
./kern-bootstrap-*-linux-x64 install
```

**Windows**

```powershell
.\kern-bootstrap-*-windows-x64.exe install
```

Default prefix is `~/.kern` (or `%USERPROFILE%\.kern` on Windows). Payloads live under `<prefix>/versions/<tag>/`; `bin/` holds symlinks (Unix) or `.cmd` wrappers (Windows) that follow the active tag.

**Windows without Node:** If `node` is not on `PATH`, install downloads the official **Node.js** Windows x64 **zip** from [nodejs.org](https://nodejs.org/) (SHA-256 checked), extracts to `<prefix>/tools/nodejs/`, and `kargo.cmd` uses that `node.exe` (npm is included in that bundle). No Python, Rust, or separate Node installer required for end users. Override the version with env **`KERN_BOOTSTRAP_NODE_VERSION`** (e.g. `22.14.0`).

Optional shell PATH snippets use markers:

`# >>> KERN_BOOTSTRAP_BEGIN >>>` … `# <<< KERN_BOOTSTRAP_END <<<`

## CLI

| Command | Purpose |
|--------|---------|
| `kern-bootstrap install` | Install or menu: upgrade, switch, clean, repair PATH, exit, reinstall, uninstall |
| `kern-bootstrap upgrade` | Non-interactive upgrade (skips menu; uses lock fail-fast unless TTY wait) |
| `kern-bootstrap list` | List `versions/*` and mark active |
| `kern-bootstrap use <tag>` | Point `current` / `active-release.txt` at `versions/<tag>` and refresh `bin/` |
| `kern-bootstrap remove <tag>` | Delete `versions/<tag>` (not allowed for the active tag) |
| `kern-bootstrap repair` | Fix active pointer from state, refresh `bin/`, optional `--fix-path` |
| `kern-bootstrap uninstall` | Remove `bin/`, `versions/`, `current`, staging, lock, state; `--purge` nukes `~/.kern` |
| `kern-bootstrap doctor` | `[OK]`/`[WARN]`/`[FAIL]` diagnostics; suggests `repair`; `--fix` adjusts PATH only |

Common flags:

- `--log-json` or `-vv`: append structured JSON lines to `~/.kern/install.log`
- `-v` / `--verbose`: extra stderr detail (use twice for JSON install log, same as `--log-json`)
- `--repo owner/name`: alternate GitHub repo
- `--version v1.2.3` or `--version latest`: release to use (default latest)
- `--prefix /path`: install prefix
- `--system`: default prefix `/usr/local` (Unix) or `C:\Program Files\Kern` (Windows)
- `--no-modify-path`: do not edit shell config or Windows user `PATH`
- `--mirror URL` (repeat) or `KERN_BOOTSTRAP_MIRRORS` (comma-separated): optional URL prefix fallback for GitHub raw download paths
- `GITHUB_TOKEN`: raises GitHub API rate limits for release metadata
- `KERN_BOOTSTRAP_NODE_VERSION` (Windows): pin Node.js zip version when bundling for Kargo (default LTS in source)

Non-interactive installs: `CI=true`, `KERN_BOOTSTRAP_NONINTERACTIVE=1`, or `-y` / `--non-interactive` where supported.

## Layout

```
<prefix>/
  bin/                   # symlinks or .cmd → active versions/<tag>/kern/*
  versions/<tag>/kern/   # normalized Kern bundle
  versions/<tag>/kargo/ # normalized Kargo bundle
  current                # symlink → versions/<tag> (Unix)
  active-release.txt     # active tag (all platforms; source of truth on Windows)
  downloads/             # cached archives + checksum files
  tools/nodejs/          # Windows only: bundled Node.js when system `node` was missing
  bootstrap-state.json   # managed install metadata (schema_version ≥ 2)
  .install.lock          # exclusive lock while installing
```

Logs append to `~/.kern/install.log` when home is available.

## Architecture (Rust modules)

| Module | Role |
|--------|------|
| `github` | GitHub Releases API (latest or tag) |
| `download` | Retries, timeouts, optional mirrors, SHA256 verification for Kargo |
| `extract` | `.tar.gz` (Kern Linux/macOS, Kargo) and `.zip` (Kern Windows) |
| `install` | Version resolution, staging, smoke test, promote, state + PATH |
| `uninstall` | Remove trees and PATH markers |
| `doctor` | Diagnostics and optional `--fix` |
| `env_paths` | Unix snippet / Windows `HKCU\Environment` `PATH` |
| `detect` | PATH probing, default prefixes |
| `state` | JSON state with atomic replace |

## Extending

- **Channels** (e.g. nightly): resolve a different tag pattern in `github::fetch_release` or add a `--channel` flag mapping to a prerelease API filter.
- **Offline**: cache tarballs under `<prefix>/downloads` and skip network when present and checksums match.
- **Self-update**: ship a small `self-update` subcommand that downloads the latest `kern-bootstrap-*` asset for the current target triple.

## License

GPL-3.0 (same as the Kern repository).
