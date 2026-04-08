# Release checklist (GitHub)

Use this before tagging a public release. Pair with [RELEASE.md](../RELEASE.md) and [CHANGELOG.md](../CHANGELOG.md).

## Pre-release

- [ ] **Version:** set root [`KERN_VERSION.txt`](../KERN_VERSION.txt) to semver `X.Y.Z`, then align the same value in [`kern-bootstrap/Cargo.toml`](../kern-bootstrap/Cargo.toml), [`kargo/package.json`](../kargo/package.json), [`kern-registry/package.json`](../kern-registry/package.json), and root [`kern.json`](../kern.json) (`version` field). Run `npm install --package-lock-only` under `kern-registry/` if you change its `package.json` version.
- [ ] **Changelog:** move `[Unreleased]` items into a dated section with the new version; keep `[Unreleased]` empty or stub for the next cycle.
- [ ] **README:** the version badge points at `KERN_VERSION.txt`; confirm the badge text matches after editing.
- [ ] **CI green:** latest `main` passes [Windows Kern](../.github/workflows/windows-kern.yml), [Linux Kern](../.github/workflows/linux-kern.yml), and [macOS Kern](../.github/workflows/macos-kern.yml) (all three build **with** Raylib / `import g2d`).

## Build & test (local or CI)

- [ ] **Release build:** `cmake --build … --config Release --target kern kernc kern-scan`
- [ ] **Smoke:** `kern --version`, `kern --help`, `kern examples/basic/01_hello_world.kn`
- [ ] **Scan:** `kern --scan --registry-only` exits **0**
- [ ] **Tests:** `kern test tests/coverage` (or full `scripts/release_go_no_go.ps1` for maintainers)
- [ ] **Graphics (if shipping Raylib):** at least one graphics sample runs (e.g. under `examples/graphics/`)

## Git & GitHub

- [ ] **Tag:** `git tag -a vX.Y.Z -m "Release vX.Y.Z"` and push `vX.Y.Z`
- [ ] **Release notes:** copy the new **CHANGELOG** section into the GitHub Release description
- [ ] **Artifacts:** confirm [release workflow](../.github/workflows/release.yml) attached **Windows** zip (`kern-windows-x64-v*.zip`), **Windows NSIS installer** (`kern-windows-x64-v*-installer.exe`), **Linux** tarball (`kern-linux-x64-v*.tar.gz`), **macOS** tarball (`kern-macos-v*.tar.gz`), and **Kargo** bundle (`kargo-v*.tar.gz` + checksums + manifest)

## Post-release

- [ ] Open `[Unreleased]` in CHANGELOG for the next iteration
- [ ] Announce if you maintain a forum / Discord / blog (optional)
