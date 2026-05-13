# release-assets (local only)

This folder is for **optional local drops** (e.g. old Windows bundles). It is **not** the canonical copy of the standard library or examples in this repository.

- **Do not treat** paths like `kern-windows-x64-v1.0.10/` as current: they are **legacy snapshots** and drift from `lib/kern/`, `examples/`, and the built `kern.exe` from CMake.
- **Source of truth:** the repo root `lib/kern/`, `examples/`, and artifacts produced by **`cmake --build … --target kern`** (or your CI release job).
- The repo **ignores** bulk contents under `release-assets/` via `.gitignore` so accidental copies of `node_modules/` or stale trees are not pushed to GitHub.

If you need a frozen bundle for archival, keep it outside the repo or under a clearly named `archive/` path and document the version and date.
