# Release checklist (GitHub)

Use this before tagging a public release. Pair with [RELEASE.md](../RELEASE.md) and [CHANGELOG.md](../CHANGELOG.md).

## Pre-release

- [ ] **Version:** update root [`KERN_VERSION.txt`](../KERN_VERSION.txt) (semver, e.g. `1.0.3`).
- [ ] **Changelog:** move `[Unreleased]` items into a dated section with the new version; keep `[Unreleased]` empty or stub for the next cycle.
- [ ] **README:** badge/version line matches `KERN_VERSION.txt` if you display it explicitly.
- [ ] **CI green:** latest `main` passes [Windows Kern](../.github/workflows/windows-kern.yml), [Linux Kern](../.github/workflows/linux-kern.yml), and [macOS Kern](../.github/workflows/macos-kern.yml).

## Build & test (local or CI)

- [ ] **Release build:** `cmake --build … --config Release --target kern kernc kern-scan`
- [ ] **Smoke:** `kern --version`, `kern --help`, `kern examples/basic/01_hello_world.kn`
- [ ] **Scan:** `kern --scan --registry-only` exits **0**
- [ ] **Tests:** `kern test tests/coverage` (or full `scripts/release_go_no_go.ps1` for maintainers)
- [ ] **Graphics (if shipping Raylib):** at least one graphics sample runs (e.g. under `examples/graphics/`)

## Git & GitHub

- [ ] **Tag:** `git tag -a vX.Y.Z -m "Release vX.Y.Z"` and push `vX.Y.Z`
- [ ] **Release notes:** copy the new **CHANGELOG** section into the GitHub Release description
- [ ] **Artifacts:** confirm [release workflow](../.github/workflows/release.yml) attached **Windows** zip (`kern-windows-x64-v*.zip`), **Linux** tarball (`kern-linux-x64-v*.tar.gz`), and **macOS** tarball (`kern-macos-v*.tar.gz`)

## Post-release

- [ ] Open `[Unreleased]` in CHANGELOG for the next iteration
- [ ] Announce if you maintain a forum / Discord / blog (optional)
