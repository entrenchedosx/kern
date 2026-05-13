# Standalone `.exe` compilation architecture

This document describes how Kern turns `.kn` sources into a **single Windows executable**, how that fits a **modular compiler driver API**, and why the current backend strategy was chosen.

## Chosen backend: embedded sources + Kern VM (not LLVM / not AOT `.kn` → C semantics)

**Implementation today:** the pipeline lowers the resolved project to an **IR** (`IRProgram`), runs **middle-end passes** (constant folding, dead code elimination, light inlining on typed IR), then the **`CppBackend`** emits a generated C++ translation unit. That TU contains **embedded `.kn` source bytes** (and optional binary assets) plus a `main` that boots the **same lexer, parser, codegen, and VM** used by `kern.exe`. CMake then **statically links** those objects into `program.exe`.

**Why this over LLVM or “compile to C source semantics”?**

| Approach | Fidelity to `kern` interpreter | Effort / risk |
|----------|-------------------------------|---------------|
| **LLVM / native AOT** | Would require a full second lowering of every language + VM feature (generators, `match`, `import`, `unsafe`, modules, stdlib maps, etc.). Easy to diverge from the bytecode VM. | Very large; long-term project. |
| **Transpile to C/C++** | Same problem: you re-implement semantics or embed a runtime anyway. | Large + two sources of truth. |
| **Bytecode blob + tiny VM** | Smaller EXE, faster compile; still need a **stable bytecode file format** and loader tests. | Medium follow-up. |
| **Embedded `.kn` + existing VM** (current) | **Same parser, codegen, and VM** as `kern` — behavior matches the interpreter path for supported features. | **Practical default**: ships today via `kernc --config`. |

**Caveats (honest):**

- The resulting `.exe` **contains the compiler front-end and VM**; it is “standalone” in the sense of **no separate `kern.exe` on disk**, not “no runtime code at all.”
- **`extern` / FFI** in bundled mode is **rejected** in phase 1 (see pipeline diagnostics): native DLL loading is not wired through the standalone stub the same way as `kern --ffi`.
- **Graphics** follow `kern_standalone_graphics.cmake` (optional Raylib); headless programs avoid that cost when disabled.

## Pipeline stages (production shape)

1. **Frontend (existing):** Lexer → Parser → AST → `CodeGenerator` when executing imports at runtime; for packaging, the driver uses **project resolution** + **per-module semantic scan** first.
2. **Dependency resolution:** `resolveProjectGraph` walks `import` from the entry `.kn`, with `include_paths` / `kernconfig` rules.
3. **Middle-end:** `buildIRFromResolvedGraph` then IR passes in `kern/pipeline/ir/passes/` (constant folding, DCE, inlining, typed IR cleanup).
4. **Backend:** `generateCppBundle` → `writeBundleAsCppSource` (packager).
5. **Link:** `buildStandaloneExe` generates a **temporary CMake project** and invokes the toolchain (MSVC-focused flags today).

Incremental builds use `.kern-cache/modules.cache` (content hash + file time) beside the output EXE.

## Library API (`kern::compile`)

Header: `src/compile/compile_pipeline.hpp`

- **`kern::compile::Options`** — high-level knobs (entry, output, includes, assets, icon, optimization, release/debug, console subsystem).
- **`kern::compile::Result`** — `success`, paths, `diagnostics[]` with line/column, `errorSummary`.
- **`kern::compile::StandaloneBackend`** — override `emitHostSources` / `linkExecutable` to plug in another backend later (e.g. bytecode blob loader, LLVM when available).
- **`runStandaloneExecutablePipeline(SplConfig&, kernRepoRoot, PipelineParams, StandaloneBackend*, Result&)`** — full native EXE driver (used by `kernc` CLI).
- **`compileStandaloneExecutable`** — convenience `Options` → `SplConfig` → pipeline.

**Include path for embedders:** add the Kern **`src`** directory to your include path and `#include "compile/compile_pipeline.hpp"`.

The CLI tool **`kernc`** calls this pipeline so behavior stays unified.

## Flags vs `SplConfig` / `Options`

| User concept | Where it lives |
|--------------|----------------|
| `--release` / `--debug` | `SplConfig::release` |
| `--opt 0..3` | `SplConfig::optimizationLevel` → MSVC `/Od`, `/O1`, `/O2`, `/Ox` in standalone CMake |
| `--console` / `--no-console` | `SplConfig::console` → `/SUBSYSTEM:WINDOWS` when off |
| “static” | MSVC **static CRT** in generated standalone CMake (`CMAKE_MSVC_RUNTIME_LIBRARY`); vcpkg static triplet when `tools/vcpkg` is present |
| `--no-runtime` | **Not supported** for fidelity: removing the VM would require a different backend contract. |

## Testing

- **Automated:** `kern --check` over examples + coverage suites (existing CI).
- **Standalone smoke (manual):** from repo root, with `kernc` built, run `kernc --config tests/compile_pipeline_fixture/kernconfig.example.json` (paths are repo-relative) or copy that file and adjust. On Windows, `powershell -File scripts/run_standalone_fixture.ps1` runs the same config; `-CheckOnly` uses `kern --check` on the fixture entry (fast); `-RunInterpreter` runs `kern main.kn` from the fixture folder so the sample `import "helper.kn"` resolves. For richer recipes, see `kern-to-exe` (Python) which writes `kernconfig.json`.
- **Invalid input:** pipeline should return `Result::success == false` with diagnostics; **no VM execution** occurs if semantic errors stop the pipeline before codegen.

## Future work (roadmap hooks)

- **Bytecode bundle backend:** emit serialized `Bytecode` per module + minimal loader; shrink EXE and avoid shipping lexer/parser in the hot path (still same VM opcodes).
- **Cross-compilation:** parameterize `buildStandaloneExe` toolchains (today: host MSVC + optional vcpkg).
- **Asset embedding:** already supported via `kernconfig` `assets` → `BundleManifest`.
- **Plugin commands:** `plugins_pre_build` / `plugins_post_build` in `kernconfig`.

## Related tools

- **`kern-to-exe/`** — Python/UI wrapper that writes `kernconfig.json` and shells out to `kernc`.
- **`scripts/publish_shareable_kern_to_exe.ps1`** — distributable folder with `kernc` + packager.
