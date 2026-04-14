# Release checklist (GitHub)

Use this before tagging a public release. Pair with [RELEASE.md](../RELEASE.md) and [CHANGELOG.md](../CHANGELOG.md).

## Pre-release

- [ ] **Version:** set root [`KERN_VERSION.txt`](../KERN_VERSION.txt) to semver `X.Y.Z`, then align the same value in [`kern-bootstrap/Cargo.toml`](../kern-bootstrap/Cargo.toml), [`kargo/package.json`](../kargo/package.json), [`kern-registry/package.json`](../kern-registry/package.json), and root [`kern.json`](../kern.json) (`version` field). Run `npm install --package-lock-only` under `kern-registry/` if you change its `package.json` version.
- [ ] **Changelog:** move `[Unreleased]` items into a dated section with the new version; keep `[Unreleased]` empty or stub for the next cycle.
- [ ] **README:** the version badge points at `KERN_VERSION.txt`; confirm the badge text matches after editing.
- [ ] **CI green:** latest `main` passes [Windows Kern](../.github/workflows/windows-kern.yml), [Linux Kern](../.github/workflows/linux-kern.yml), and [macOS Kern](../.github/workflows/macos-kern.yml) (all three build **with** Raylib / `import g2d`).

## Build & test (local or CI)

- [ ] **Release build:** `cmake --build … --config Release --target kern kernc kern-scan kern_game kern_repl kern_lsp kern_contract_humanize kargo` (Windows portable zip: `powershell -File scripts/package-windows-kern-github-release-zip.ps1` after `npm ci --prefix kern-registry --omit=dev` if refreshing the bootstrapper asset locally)
- [ ] **Smoke:** `kern --version`, `kern --help`, `kern examples/basic/01_hello_world.kn`
- [ ] **Scan:** `kern --scan --registry-only` exits **0**
- [ ] **Tests:** `kern test tests/coverage` (or full `scripts/release_go_no_go.ps1` for maintainers)
- [ ] **Graphics (if shipping Raylib):** at least one graphics sample runs (e.g. under `examples/graphics/`)

## Git & GitHub

- [ ] **Tag:** `git tag -a vX.Y.Z -m "Release vX.Y.Z"` and push `vX.Y.Z`
- [ ] **Release notes:** copy the new **CHANGELOG** section into the GitHub Release description
- [ ] **Artifacts:** confirm [release workflow](../.github/workflows/release.yml) attached **Windows** zip (`kern-windows-x64-v*.zip`), **Windows NSIS installer** (`kern-windows-x64-v*-installer.exe`), **Linux** tarball (`kern-linux-x64-v*.tar.gz`), **macOS** tarballs (`kern-macos-arm64-v*.tar.gz`, `kern-macos-x64-v*.tar.gz`), optional **legacy Node Kargo** bundle (`kargo-v*.tar.gz` + checksums + manifest), and **portable env** files (`kern-core.exe`, `kern-runtime.zip`, `kern-portable.exe`, **`kargo.exe`** — see [RELEASE.md](../RELEASE.md) Option A2)
- [ ] **Portable mirror:** with **`KERN_INSTALLER_SRC_TOKEN`** set on the **kern** repo (PAT: Contents write on **entrenchedosx/kern-installer-src**), the same workflow uploads the portable quartet + **`kern-SHA256SUMS`** to **[kern-installer-src releases](https://github.com/entrenchedosx/kern-installer-src/releases)** for the same tag. Without the secret, mirror is skipped (set token or upload those files manually there).

## Post-release

- [ ] Open `[Unreleased]` in CHANGELOG for the next iteration
- [ ] Announce if you maintain a forum / Discord / blog (optional)
