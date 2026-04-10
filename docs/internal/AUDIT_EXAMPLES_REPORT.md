# Phase 0.5 — Examples audit

Scope: all [`examples/**/*.kn`](../../examples/) (excluding `release-assets/` mirrors).

## Summary

| Category | Count (approx.) | Notes |
|----------|-----------------|--------|
| basic / tour | ~35 | Core language: variables, control flow, functions, I/O, errors, modules |
| advanced | ~35 | Networking, projects, kits — many need Raylib, HTTP, or OS-specific APIs |
| graphics | ~20 | Require `KERN_BUILD_GAME=ON` and display (CI uses `KERN_COVERAGE_SKIP_GRAPHICS`) |
| network | ~4 | May need open ports / timing |
| system | ~10 | Windows-specific FFI and tooling |
| golden | ~3 | Async / runtime demos |

## Classification

- **Working (headless CI):** `examples/basic/*.kn`, most `examples/tour/*.kn`, and non-graphic `examples/math/*.kn` when run with repo `lib/kern` on `PATH`/`KERN_LIB` as in [`tests/run_stable.ps1`](../../tests/run_stable.ps1).
- **Working (full build + GPU):** `examples/graphics/*`, `examples/advanced/*` that import `g2d` / `g3d` / `game`.
- **Broken / fragile:** Any example depending on removed `.kern/package-paths.json` alone — fixed by using `./config/package-paths.json` from native **kargo** or project `config/`.
- **Redundant:** Overlapping tour vs basic topics (kept intentionally for learning paths).

## Missing categories (addressed)

- **Recursion:** [`examples/basic/23_recursion_fib.kn`](../../examples/basic/23_recursion_fib.kn) (added).
- **Native Kargo:** [`examples/kargo/README.md`](../../examples/kargo/README.md) documents `kargo.json` / `kargo install`.

## Runner

Use [`scripts/run_examples_headless.ps1`](../../scripts/run_examples_headless.ps1) from the repo root with `kern.exe` on `PATH`.
