# Phase 0 — First-party C++ / header manifest

Generated for exhaustive audit. **Excluded:** `build*/`, `**/CMakeFiles/**`, `CompilerId*.cpp`, vendored trees outside this list.

## `kern/core/` (compiler, bytecode core, diagnostics, errors, platform, utils)

- `bytecode/bytecode.hpp`, `bytecode.hpp` (value, script_code, peephole)
- `compiler/ast.hpp`, `builtin_names.hpp`, `codegen.hpp`, `codegen.cpp`, `import_aliases.hpp`, `import_aliases.cpp`, `lexer.hpp`, `lexer.cpp`, `parser.hpp`, `parser.cpp`, `project_resolver.hpp`, `project_resolver.cpp`, `semantic.hpp`, `semantic.cpp`, `token.hpp`, `typed_builtins.hpp`, `source_encoding.hpp`
- `diagnostics/source_span.hpp`, `traceback_limits.hpp`
- `errors/errors.hpp`, `errors.cpp`, `vm_error_codes.hpp`, `vm_error_registry.hpp`
- `platform/env_compat.hpp`, `win32_associate_kn.hpp`, `win32_associate_kn.cpp`, **`kern_env.hpp`, `kern_env.cpp`** (added by rebuild)
- `utils/build_cache.hpp`, `build_cache.cpp`, `kernconfig.hpp`, `kernconfig.cpp`

## `kern/pipeline/`

- `analyzer/project_analyzer.hpp`, `project_analyzer.cpp`
- `backend/cpp_backend.hpp`, `cpp_backend.cpp`
- `ir/ir.hpp`, `ir_builder.hpp`, `ir_builder.cpp`, `typed_ir_builder.hpp`, `typed_ir_builder.cpp`, `passes/passes.hpp`, `constant_folding.cpp`, `dead_code_elim.cpp`, `inline_basic.cpp`, `typed_ir_passes.cpp`

## `kern/runtime/vm/`

- `vm.hpp`, `vm.cpp`, `builtins.hpp`, `bytecode_verifier.hpp`, `bytecode_verifier.cpp`, `permissions.hpp`, `kern_socket.hpp`, `kern_socket.cpp`, `http_get_winhttp.hpp`, `http_get_winhttp.cpp`

## `kern/modules/`

- `builtin_module_registry.hpp`, `builtin_module_registry.cpp`
- `game/*`, `g2d/*`, `g3d/*`, `system/*`

## `kern/tools/`

- `main.cpp`, `repl_main.cpp`, `kernc_main.cpp`, `scan_main.cpp`, `lsp_main.cpp`, **`kargo_main.cpp`** (added)

## `kern/ide/qt-native/` (optional IDE)

- `src/main.cpp`, `MainWindow.cpp`, `SplHighlighter.cpp`, headers under `include/`

## `src/` (glue)

- `import_resolution.hpp`, `import_resolution.cpp`
- `stdlib_modules.hpp`, `stdlib_modules.cpp`, `stdlib_stdv1_exports.hpp`
- `compile/compile_pipeline.hpp`, `compile_pipeline.cpp`
- `packager/bundle_manifest.hpp`, `bundle_writer.hpp`, `bundle_writer.cpp`
- `process/process_module.hpp`, `process_module.cpp`
- `scanner/*`, `system/event_bus.hpp`, `event_bus.cpp`, `runtime_services.hpp`
- `tests/humanize_path_contract_test.cpp`

## `framework/` (doc runtime demo target)

- Headers under `include/fw/**`, sources under `src/**`, `demo/main.cpp`

## Counts

- **kern/** (excluding ide): ~80 translation units
- **kern/ide/qt-native**: 5
- **src/**: 22
- **framework/**: ~40 (approximate; see glob in repo)

Use this checklist when marking each file **audited** in `AUDIT_CPP_REPORT.md`.
