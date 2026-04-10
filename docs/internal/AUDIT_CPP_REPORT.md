# Phase 0 — C++ audit report (summary)

This report summarizes cross-cutting findings from a full pass over the manifest in [`AUDIT_CPP_MANIFEST.md`](AUDIT_CPP_MANIFEST.md). Use it with per-file notes when hardening.

## Compiler pipeline

- **Lexer → Parser → Semantic → Codegen → Bytecode** live under [`kern/core/compiler/`](../kern/core/compiler/) with IR/optimizer paths in [`kern/pipeline/`](../kern/pipeline/).
- **Execution:** [`kern/runtime/vm/vm.cpp`](../kern/runtime/vm/vm.cpp) + [`bytecode_verifier.cpp`](../kern/runtime/vm/bytecode_verifier.cpp).
- **Glue:** [`src/import_resolution.cpp`](../src/import_resolution.cpp) ties imports, stdlib, and optional native modules.

## Runtime / VM

- VM owns execution and global state; import cache uses **process-wide statics** in `import_resolution.cpp` (`g_importState`, `g_importCacheOwner`, etc.) — **risk:** re-entrancy, tests, future threading.
- Builtins and native modules registered via [`builtin_module_registry`](../kern/modules/builtin_module_registry.cpp).

## CLI

- [`kern/tools/main.cpp`](../kern/tools/main.cpp) is a **large monolith** (run, check, watch, test, scan, fmt, graph, registry helpers, flags). **Recommendation:** split into `cli/*.cpp` with a thin `main` dispatcher (done incrementally in this rebuild via early env parsing only; full split is follow-up).
- Separate entrypoints: `repl_main`, `kernc_main`, `scan_main`, `lsp_main`.

## Kargo / packages (before native rewrite)

- **Legacy:** Node `kargo/` produced `.kern/package-paths.json`; **replaced** by [`config/package-paths.json`](../../config/package-paths.json) relative to resolved `KERN_HOME` / project, maintained by native `kargo.exe`.

## Issues catalog

| Area | Severity | Notes |
|------|----------|--------|
| Import globals | Medium | Narrow to per-VM or per-run context when feasible. |
| Monolithic CLI | Medium | Hard to test and reason about; error paths inconsistent. |
| Package JSON | Low | Regex-based JSON scraping for `main` paths; **fixed** by central path + deterministic `kargo` writer. |
| Portable layout | High | `.kern/bin` and parent walk **removed**; use `kern_env` + `kern-<tag>/` layout. |
| Dead / duplicate | TBD | IR/backend and scanner paths: mark unused TU’s after build graph review. |

## Rewrite vs patch

| Component | Verdict |
|---------|---------|
| Portable bootstrap (`kern-portable-bootstrap`) | **Rewrite** layout + delegation + activation (no `bin/`, no Node kargo). |
| Kargo | **Rewrite** as native `kargo.exe`. |
| `import_resolution` package paths | **Patch** to new root + `config/`; later refactor globals. |
| `main.cpp` | **Patch** now; **split** recommended next milestone. |
| Compiler / VM | **Patch** bugs as found by examples/tests; no full rewrite in this pass. |
