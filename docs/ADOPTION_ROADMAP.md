# Adoption & production-readiness roadmap

This is a **living plan** toward wider adoption, stability, and professional tooling. Items are phased; not everything ships at once.

## Phase A — Baseline (in progress)

| Track | Goal |
|--------|------|
| **CI** | Windows + Linux smoke on every push; tagged releases on Windows (see `.github/workflows/`). |
| **Docs** | `docs/` handbook + optional MkDocs; `kern docs` entry point. |
| **Releases** | Semver in `KERN_VERSION.txt`, changelog, tags `v*`, artifacts on GitHub Releases. |
| **Contributing** | CONTRIBUTING, Code of Conduct, release checklist. |

## Phase B — Quality & coverage

- Expand automated tests (`kern test`, coverage targets) and keep regression suites green.
- Optional coverage reporting in CI.
- Stricter diagnostic tests for edge cases; document stable error codes.
- **Fuzzing** (parser / semantic): introduce behind a dedicated script or CI job when ready.

## Phase C — CLI & packaging

- Standardize **`kern build`** (CMake wrapper or documented passthrough) vs today’s informational hint.
- **Installers**: NSIS / deb / Homebrew / Scoop / Chocolatey — add per-platform recipes or community-maintained taps.
- **Docker** image for reproducible CI and “try Kern” (see root `Dockerfile` when present).

## Phase D — Editor & IDE

- VS Code extension: diagnostics, run/test tasks (see `editors/vscode-kern/`).
- Kern-IDE portable releases (separate repo).

## Phase E — Community

- GitHub **Discussions** or linked chat (optional).
- Security policy (`SECURITY.md`) when reporting process is defined.

## Versioning

- **Language/tooling version:** `KERN_VERSION.txt` at repo root (semver).
- **Git tags:** `v1.0.5` style for releases.
- **Compatibility:** prefer backward-compatible changes; breaking changes require changelog entry and migration notes.
