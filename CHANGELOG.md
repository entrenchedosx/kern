# Changelog

All notable changes to Kern are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [Unreleased]

---

## [2.2.0] - 2026-05-16

### Added

- **v3.0 Phase 1: VM-Native Cooperative Coroutines** (2026-05-15T15:38 UTC+1)
  - **`kern/runtime/vm/vm.hpp`:** Added `Coroutine` struct with `ip`, `fp`, `sp`, `stack`, `state` (`RUNNABLE` / `YIELDED` / `DEAD`), `yieldedValue`, and `frameStack` for coroutine state capture; `activeCoroutineId_`, `coroutineYieldRequested_`, `coroutines_` vector; public `hasActiveCoroutines()` / `activeCoroutineCount()`; `coroutineReturn()` for `kern_coroutine_return`.
  - **`kern/runtime/vm/vm.cpp`:** Implemented `resumeAll()` — lazy-init coroutine 0 from the main `VM::run()` state, then iterates all non-DEAD coroutines, restores VM registers from saved state, runs instructions via `runInstruction()` loop, saves state on `OP_YIELD` / `DEAD`; `saveCurrentCoroutineState()` / `restoreCoroutineState()`; `kern_start_coroutine` handler allocates new coroutine from a lambda.
  - **`kern/core/bytecode/bytecode.hpp`:** Added `OP_YIELD` opcode (enum), `OP_COROUTINE_START` (calls `kern_start_coroutine` builtin).
  - **`kern/core/compiler/codegen.cpp`:** Codegen for `yield` statement emits `OP_YIELD`; `spawn` statement emits `OP_COROUTINE_START`; coroutine detection via `stmtHasYield` / `exprSpawnsCoroutine` triggers `COROUTINE` flag.
  - **`kern/core/compiler/ast.hpp`:** Added `YieldStmt` node.
  - **`kern/core/compiler/parser.cpp`:** Parses `yield` keyword → `YieldStmt`.
  - **`kern/core/compiler/lexer.cpp`:** Lexes `yield` and `spawn` tokens.
  - **`kern/runtime/vm/builtins.cpp`:** Registered `kern_start_coroutine`, `kern_coroutine_active`, `kern_coroutine_return` builtins.
  - **`tests/coverage/test_coroutines.kn`:** Integration test validating `yield`/`resume` across 32000-iteration loop with `a`/`b`/`c` alternating output and final `OK: test_coroutines`.

- **v3.0 Phase 2: Time-Aware Coroutine Scheduling** (2026-05-15T16:40 UTC+1)
  - **`kern/runtime/vm/vm.hpp`:** Added `uint64_t wakeTimestampMs` to `Coroutine` struct; `resumeAll(uint64_t currentTimeMs = 0)` (default param for backward compat); public `sleepCurrentCoroutine(uint64_t ms)`; private `uint64_t currentTimeMs_`.
  - **`kern/runtime/vm/vm.cpp`:** Time-aware scheduler: `resumeAll()` stores `currentTimeMs_`, skips YIELDED coroutines whose `wakeTimestampMs > 0` and `currentTimeMs < wakeTimestampMs`, clears `wakeTimestampMs` on wake; `sleepCurrentCoroutine()` sets `wakeTimestampMs = currentTimeMs_ + ms`, marks coroutine YIELDED, signals `coroutineYieldRequested_` (reuses existing yield mechanism — pure cooperative, no blocking); yield break path explicitly sets `cor.state = CoroutineState::YIELDED` to handle builtin-triggered yields (not just `OP_YIELD`).
  - **`kern/runtime/vm/builtins.cpp`:** Registered `kern_sleep(ms)` builtin — calls `vm->sleepCurrentCoroutine(ms)`, returns `nil`. Handles missing/negative args gracefully.
  - **`kern/tools/main.cpp`:** Host loop now uses `std::chrono::steady_clock` to pass monotonic millisecond timestamps to `resumeAll()`.
  - **`tests/coverage/test_sleep.kn`:** Integration test: coroutine calls `kern_sleep(800)`, verifies ~800ms pause between `"Before sleep"` and `"After sleep"` without blocking the host thread.

- **v3.1 Phase 1: Safe Hot Reloading** (2026-05-15T17:30 UTC+1)
  - **`kern/runtime/vm/vm.hpp`:** Added `hotReload()` declaration — accepts new bytecode, string constants, and value constants; public method on `VM`.
  - **`kern/runtime/vm/vm.cpp`:** Implemented `hotReload()` — kills all coroutines 1..N (sets DEAD, clears saved state vectors), resets VM execution state (stack, callFrames, callStack, frameLocals_, deferStack, iterStack, tryStack, exceptionStack, codeFrameStack, currentScript, locals_, unsafeDepth_, coroutineYieldRequested_, pendingYield_, doneGenerator_, inGeneratorExecution_, activeGenerator), loads new bytecode/constants, calls `run()` to re-execute top-level and rebind globals.
  - **`kern/tools/main.cpp`:** File-watching hot reload in `runSource()` coroutine loop — stores `std::filesystem::last_write_time()` before the loop; each tick compares current mod-time; on change, re-reads source, re-compiles via full Lexer/Parser/CodeGenerator pipeline, calls `vm.hotReload()`; on compile failure, logs error and continues with old bytecode (retries next tick).
  - **`kern/tools/hot_reload_test.cpp`:** 4 integration tests: `testGlobalRebind` (g=10→hotReload→g=42), `testFunctionRebind` (tick() v1→v2), `testCoroutineFlush` (global_flag=1→2), `testMultipleReloads` (5 iterations counter=N). Exercises full compiler pipeline.
  - **`kern/core/bytecode/value.cpp`:** Added `asInt()`, `asFloat()`, `asBool()`, `asString()`, `asVec3()` getter implementations for native bindings.
  - **`CMakeLists.txt`:** Added `kern_hot_reload_test` executable target linking to `kern_core` with strict warnings.

- **v4.0 Phase 1: Non-blocking Async File I/O (`fs_read_async`)** (2026-05-15T18:00 UTC+1)
  - **`kern/runtime/vm/vm.hpp`:** Added `#include <future>`, `#include <optional>`; `std::optional<std::future<std::string>> pendingStringTask` field to `Coroutine` struct — stores background I/O future per coroutine; `void startAsyncFileRead(const std::string& path)` public method declaration.
  - **`kern/runtime/vm/vm.cpp`:** Upgraded `resumeAll()` scheduler — before restoring a yielded coroutine, checks `pendingStringTask` future; if not ready (`wait_for(0s) != ready`), skips the coroutine; if ready, extracts string via `future.get()`, resets the optional, replaces the top of the saved stack (the `nil` from the builtin's pre-yield return) with the file contents string, then resumes normally. Added `startAsyncFileRead()` implementation — launches `std::async(std::launch::async, ...)` to read the file on a background thread, stores the future in the coroutine, marks it `YIELDED`, and signals `coroutineYieldRequested_`. `hotReload()` also clears `pendingStringTask` (via `.reset()`) to abandon in-flight I/O when flushing the coroutine pool.
  - **`kern/runtime/vm/builtins.cpp`:** Registered `kern_fs_read_async(path)` builtin — calls `vm->startAsyncFileRead(path)`, returns `nil` (the actual file contents replace `nil` on the coroutine stack when the future completes).
  - **`kern/tools/async_fs_test.cpp`:** 4 integration tests: `testAsyncFileRead` (reads known content from coroutine), `testEmptyFileRead` (empty file returns `""`), `testNonexistentFile` (missing file returns `""`), `testConcurrentReads` (two coroutines read different files simultaneously). Exercises full compiler pipeline with `resumeAll()` polling loop.
  - **`CMakeLists.txt`:** Added `kern_async_fs_test` executable target linking to `kern_core` with strict warnings.

- **v4.0 Phase 2: Non-blocking Async HTTP Requests (`http_get_async`)** (2026-05-15T18:45 UTC+1)
  - **`kern/runtime/vm/vm.hpp`:** Added `void startAsyncHttpGet(const std::string& url)` public method declaration — reuses the same `pendingStringTask` future pipeline introduced in Phase 1.
  - **`kern/runtime/vm/vm.cpp`:** Implemented `startAsyncHttpGet()` — launches `std::async(std::launch::async, ...)` that calls `kernHttpGetWinHttp(url)` (WinHTTP) on a background thread, stores the future in the active coroutine's `pendingStringTask`, marks it `YIELDED`, and signals `coroutineYieldRequested_`. On non-Windows platforms, returns an empty string as a safe fallback.
  - **`kern/runtime/vm/builtins.cpp`:** Registered `kern_http_get_async(url)` builtin — calls `vm->startAsyncHttpGet(url)`, returns `nil` (the actual response body replaces `nil` on the coroutine stack when the future completes). Error handling: failed/invalid URLs return empty string, never crash the VM.
  - **`kern/tools/async_http_test.cpp`:** 4 integration tests: `testHttpGetBasic` (fetches http://example.com, verifies non-empty response), `testHttpGetContent` (response contains expected HTML), `testHttpGetInvalidUrl` (invalid URL returns empty string), `testHttpGetConcurrent` (two coroutines fetch simultaneously).
  - **`CMakeLists.txt`:** Added `kern_async_http_test` executable target linking to `kern_core` with strict warnings.

- **v4.0 Phase 3: The Web Suite Complete (JSON Parsing and Async POST)** (2026-05-15T19:10 UTC+1)
  - **`kern/runtime/vm/json_parser.hpp` / `json_parser.cpp`:** Created zero-dependency recursive-descent JSON parser. Maps JSON types to Kern `Value` types: Object → Map, Array → Array, String → String, Number → Int (integer) / Float (fractional/overflow), `true`/`false` → Bool, `null` → Nil. Supports `\uXXXX` Unicode escape sequences (4-hex-digit → UTF-8 encoding), trailing commas in objects/arrays, and number overflow fallback (`strtoll` ERANGE → `strtod` to Float). On malformed JSON input, returns `Value::nil()` — never crashes.
  - **`kern/runtime/vm/builtins.cpp`:** Registered `kern_json_parse(json_string)` synchronous builtin — instantiates `JsonParser`, calls `parser.parse()`, returns the resulting Kern `Value`. Registered `kern_http_post_async(url, payload)` async builtin — calls `vm->startAsyncHttpPost(url, payload)`, returns `nil` (the response body replaces `nil` on the coroutine stack when the future completes).
  - **`kern/runtime/vm/http_get_winhttp.cpp`:** Added `winHttpPostTry()` — opens WinHTTP POST request, sets `Content-Type: application/json` header, sends payload via `WinHttpWriteData()` with UTF-8 bytes, reads response. Added `kernHttpPostWinHttp()` public wrapper with retry logic (ignore-cert-errors fallback).
  - **`kern/runtime/vm/vm.hpp` / `vm.cpp`:** Added `void startAsyncHttpPost(const std::string& url, const std::string& payload)` public method — reuses the same `pendingStringTask` future pipeline from Phase 1/Phase 2. Launches `std::async` calling `kernHttpPostWinHttp()`, stores future, marks coroutine YIELDED.
  - **`kern/tools/test_web_suite.cpp`:** 7 integration tests: `testJsonParseObject` (map with string/int values), `testJsonParseArray` (array of integers), `testJsonParseNested` (nested object/array/bool), `testJsonParseInvalid` (malformed JSON returns nil), `testJsonParseKeywords` (null/true/false), `testHttpPostAsync` (POST to httpbin.org, verify echoed response), `testJsonParseAndPost` (end-to-end: parse JSON → construct payload → POST → verify echoed data).
  - **`CMakeLists.txt`:** Added `json_parser.cpp` to `VM_SOURCES`; added `kern_web_suite_test` executable target linking to `kern_core` with strict warnings.

- **v5.0 Phase 1: The Type Optimizer — Gradual Typing and Fast-Path Opcodes** (2026-05-15T20:13 UTC+1)
  - **`kern/core/bytecode/bytecode.hpp`:** Added 14 new fast-path opcodes to the `Opcode` enum: `ADD_INT`, `SUB_INT`, `MUL_INT`, `DIV_INT`, `ADD_FLOAT`, `SUB_FLOAT`, `MUL_FLOAT`, `DIV_FLOAT`, `EQ_INT`, `EQ_FLOAT`, `LT_INT`, `LT_FLOAT` (binary arithmetic/comparison), `NEG_INT`, `NEG_FLOAT` (unary negation). These bypass `std::variant` dispatch in the VM hot loop for typed int/float operations.
  - **`kern/core/compiler/codegen.hpp`:** Added soft typing infrastructure to `CodeGenerator` — `std::vector<std::unordered_map<std::string, std::string>> typeScopes_` (mirrors `scopes_` vector, maps variable name → `"int"` / `"float"` / `""` unknown); `std::string resolveType(const std::string& name)` walks `typeScopes_` inner-to-outer to find a variable's declared type.
  - **`kern/core/compiler/codegen.cpp`:** `VarDeclStmt` handler records declared types into `typeScopes_` when `typeName` is `"int"` or `"float"`; `beginScope()` / `endScope()` manage the parallel `typeScopes_` stack; `BinaryExpr` handler calls `resolveType()` on both operands — when both are `"int"` emits `ADD_INT`/`SUB_INT`/`MUL_INT`/`DIV_INT`/`EQ_INT`/`LT_INT` instead of generic `ADD`/`SUB`/`MUL`/`DIV`/`EQ`/`LT`; when both are `"float"` emits `ADD_FLOAT`/`SUB_FLOAT`/`MUL_FLOAT`/`DIV_FLOAT`/`EQ_FLOAT`/`LT_FLOAT`; mixed or unknown types fall back to generic opcodes with a soft warning via `codegenLog()`.
  - **`kern/runtime/vm/vm.cpp`:** Added fast-path handlers in `VM::runInstruction()` for all 14 new opcodes — `ADD_INT`/`SUB_INT`/`MUL_INT`/`DIV_INT` operate on `int64_t` via `Value::fromInt()`, `ADD_FLOAT`/`SUB_FLOAT`/`MUL_FLOAT`/`DIV_FLOAT` operate on `double` via `Value::fromFloat()`, `EQ_INT`/`LT_INT`/`EQ_FLOAT`/`LT_FLOAT` push boolean `Value::fromBool()`, `NEG_INT`/`NEG_FLOAT` are unary negation. Each fast-path case reads/writes the stack slot directly without `std::visit`, `valueDowncast`, or runtime type checks.
  - **`kern/runtime/vm/bytecode_verifier.cpp`:** Updated `stackMinAndDelta()` switch to recognize all 14 new opcodes — binary arithmetic/comparison opcodes set `minDepth=2`, `delta=-1`; unary negation opcodes set `minDepth=1`, `delta=0`; `checkOperandOnly()` needed no changes (new opcodes have no operands, default case returns `true`).
  - **`kern/tools/type_optimizer_test.cpp`:** 12 integration tests exercising the complete fast-path matrix: `testTypedIntAdd` (5+3=8), `testTypedIntSub` (10-4=6), `testTypedIntMul` (6×7=42), `testTypedIntDiv` (20÷5=4), `testTypedFloatAdd` (2.5+1.5=4.0), `testTypedFloatSub` (10.0-3.5=6.5), `testTypedFloatMul` (3.0×4.5=13.5), `testTypedFloatDiv` (7.5÷2.5=3.0), `testUntypedBackwardCompat` (untyped 5+3=8 via generic ADD), `testTypedIntEq` (5==5 → true via EQ_INT), `testTypedIntLt` (3<7 → true via LT_INT), `testMixedTypeFallback` (int 5 + float 2.0 = 7.0 via generic ADD fallback). All tests use top-level `let` declarations with type annotations.
  - **`CMakeLists.txt`:** Added `kern_type_optimizer_test` executable target linking to `kern_core` with strict warnings.

- **v5.0 Phase 2: The Dynamic FFI Bridge** (2026-05-15T21:07 UTC+1)
  - **Purpose:** First-class **shared library loading** and **C function binding** from `.kn` scripts, enabling Kern programs to call arbitrary native C exports (e.g., `msvcrt!atoi`, `kernel32!SetLastError`/`GetLastError`). All entry points are **builtins** guarded by the **`ffi`** capability namespace.
  - **Core implementation:** [`kern/runtime/vm/ffi_module.hpp`](kern/runtime/vm/ffi_module.hpp) / [`ffi_module.cpp`](kern/runtime/vm/ffi_module.cpp) — `ffiLoadLibrary()` / `ffiFreeLibrary()` (platform abstraction over `LoadLibraryA`/`FreeLibrary` on Windows, `dlopen`/`dlclose` on Unix); `ffiGetProcAddress()` (`GetProcAddress` / `dlsym`); `callFfiClosure()` — the "Poor Man's FFI" callback dispatcher that marshals Kern `Value` arguments to C types (`int64_t`, `double`, `const char*`, `void*`) via a 10-element `void*` array trampoline, invokes the target function through `ffiFunctionCall()` (casts to the correct function pointer signature based on return type), and marshals the result back to a Kern `Value`.
  - **Value types:** [`kern/core/bytecode/value.hpp`](kern/core/bytecode/value.hpp) — added `FFI_FN` (value `14`) to `Value::Type` enum; `FfiClosure` struct (stores `void* fnPtr`, `std::string returnType`, `std::vector<std::string> paramTypes`); `FfiClosurePtr` alias; `Value::fromFfi()` factory; `Value::Type::FFI_FN` serialization in [`bytecode_serializer.hpp`](kern/core/bytecode/bytecode_serializer.hpp).
  - **VM dispatch:** [`kern/runtime/vm/vm.cpp`](kern/runtime/vm/vm.cpp) — `CALL` opcode handler branched on `callee->type == FFI_FN`, extracts `FfiClosure` via `std::get<FfiClosurePtr>(callee->data)`, calls `callFfiFunction()`, pushes result onto the stack.
  - **Builtins (3 surface APIs):** [`kern/runtime/vm/builtins.cpp`](kern/runtime/vm/builtins.cpp) — registered `ffi_load(path)` → loads a shared library by path, returns opaque ptr (or `nil` on failure); `ffi_bind(handle, name, return_type, param_types)` → resolves a symbol by name, returns FFI_FN closure (or `nil` on failure); `ffi_free(handle)` → unloads the library.
  - **`SmallString` ODR fix:** [`kern/core/value.cpp`](kern/core/value.cpp) — removed out-of-line `SmallString` constructor, destructor, copy/move assignment, `c_str()`, `size()`, `toString()`, `operator==`, and `operator<` implementations (lines 9–132) that conflicted with inline definitions in [`kern/core/value.hpp`](kern/core/value.hpp), causing linker multiply-defined-symbol errors.
  - **CMake:** [`CMakeLists.txt`](CMakeLists.txt) — added `ffi_module.cpp` to `VM_SOURCES`; added `kern_ffi_test` executable target linking to `kern_core` with `KERN_BUILD_ID` and strict warnings.
  - **Integration tests:** [`kern/tools/ffi_test.cpp`](kern/tools/ffi_test.cpp) — 10 tests covering the full FFI surface: `testFfiLoadAndCallNoArgs` (call no-arg function, read exit code), `testFfiBindAtoi` (bind `atoi`, parse multiple strings to ints), `testFfiFromKernScript` (FFI from Kern source via compiler pipeline), `testFfiInvalidLibrary` (non-existent path returns nil), `testFfiInvalidFunction` (non-existent export returns nil), `testFfiFree` (load → bind → free → verify handle released), `testFfiMessageBoxSignature` (4-param signature `ptr+string+string+int`), `testFfiFloatReturn` (bind `sqrt` from `msvcrt.dll`, verify float return), `testFfiAtoiFromKern` (end-to-end Kern script loading `msvcrt.dll`, binding `atoi`, calling it, summing results; Windows uses raw string literal to avoid concatenation issues with indented `if` blocks).

---

## [2.1.0] - 2026-05-13

### Added

- Structs and UFCS (Uniform Function Call Syntax)
- Defer statements and Result? sugar
- Collection library enhancements
- Native Vec3 math support
- Bytecode verifier hardening
- Various performance and stability improvements

---

## [1.0.20] - 2026-04-06

### Added

- **`kern-portable-bootstrap/`** — Windows-first **`kern-portable.exe`**: `init` / `upgrade` / `doctor` with SHA256 verification, artifact cache (`integrity.json` v2), atomic staging and rollback, `KERN_ROOT_CACHE`, and delegation to **`.kern/bin/kern.exe`** with inherited stdio. Release ships **`kern-core.exe`**, **`kern-runtime.zip`**, **`kern-portable.exe`**, **`kern-SHA256SUMS`**, **`kargo-<tag>.tar.gz`**, **`kargo-SHA256SUMS`**.

### Changed

- **`kern` (Windows):** if **`kargo.toml`** exists and **`.kern/bin/kargo.cmd`** is found walking upward, **`kern add`** / **`kern remove`** delegate to the portable Kargo shim (`install` / `remove`).

---

## [1.0.19] - 2026-04-10

### Fixed

- **kern-bootstrap:** `install` / `upgrade` now default to this binary’s **semver** (same as `KERN_VERSION.txt` / `Cargo.toml`) instead of GitHub **`releases/latest`**, avoiding install races after a tag push and checksum failures when `latest` still referred to an older release.
- **kern-bootstrap (Windows):** strict verify no longer runs `cmd /c` with **canonical `\\?\`…** shim paths (cmd rejects them); it invokes **`kern.cmd` / `kargo.cmd`** via normal paths while still using canonical paths for under-`bin/` checks.

---

## [1.0.18] - 2026-04-09

### Fixed

- **CLI permissions:** `kern` now defaults to **permissive** permission enforcement (trust-the-programmer for local scripts and examples). Set **`KERN_ENFORCE_PERMISSIONS=1`** to restore strict gating. Debug-mode script runs use an **unlimited VM step budget** so game-style loops no longer hit `VM-STEP-LIMIT` under the default `--debug` profile.
- **kern-bootstrap / releases:** macOS installs now target **arch-specific** Kern tarballs (`kern-macos-arm64-v*`, `kern-macos-x64-v*`) with a **legacy** `kern-macos-v*` fallback for older tags; previously a single Apple-Silicon tarball was offered under the generic name, which could not run on **Intel Macs**. Release CI builds both slices (full stable suite on arm64; smoke tests on x64).
- **kern-bootstrap:** GitHub API **User-Agent** uses the real bootstrapper version (from `KERN_VERSION.txt` / `Cargo.toml`) instead of a hard-coded `kern-bootstrap/0.1`.

### Added

- **`kargo/`** — standalone Node CLI for **GitHub-only** packages: `install` / `remove` / `update` / `list` / `search` / `publish` / `login` / `build` / `run`. Caches under `~/.kargo/packages`, writes **`kargo.lock`** (tag + **commit SHA**, **`resolved_constraints`** / **`resolved_from`** / **`resolved_version_range`** / optional **`resolved_version_range_normalized`**, v2), **deterministic JSON** (sorted `packages`, fixed per-entry key order), merges **`.kern/package-paths.json`**. **Resolver:** `[dependencies]` **semver ranges**, **deterministic** ordering (sorted package ids / constraints / edges), **per-run `ls-remote` cache**, **prerelease fallback** when no stable tag matches (toggle via **`[kargo] allow_prerelease`**), structured **resolution failure** messages with sampled versions; **`--resolve-debug`** prints an expanded **decision trace** (`why_selected`, rejections + selection reason, `resolution_mode`) + tree on stderr; **`--verbose`** prints a short **why this version** for the root on **`install` / `update <spec>`**. **Conflict errors** include a **minimal unsatisfiable core** when smaller than the full constraint list. **`[kargo] resolution_mode = "locked"`** uses **kargo.lock** only for versions (CI-style); **`latest`** resolves from remote tags. **`kargo graph`** (`--json` optional) draws the dependency tree from **kargo.toml** + on-disk manifests + lock. **`kargo update`** (no args) resolves the full graph from **`kargo.toml`**. **`install.ps1`** / **`install.sh`** copy `kargo` to `<prefix>/lib/kargo`, run `npm install --omit=dev`, and add **`kargo`** beside **`kern`**. **`kern`** import resolution accepts **`owner/repo`** and **`github.com/owner/repo`** when listed in `package-paths.json`. **`build.ps1`** stages **`BUILD/lib/kargo`** and **`BUILD/bin/kargo.cmd`** for **NSIS**. **`install.sh`** sets **`PATH_BIN`** before the kargo shim copy.

### Changed

- **Build layout (Phase 11):** Remaining heavy **`src/`** trees moved into **`kern/`**: **`src/game/`** → **`kern/modules/game/`**, **`src/backend/`** → **`kern/pipeline/backend/`**, **`src/utils/`** → **`kern/core/utils/`**. CMake: **`KERN_GAME_MODULE_DIR`**, **`KERN_BACKEND_DIR`**, **`KERN_UTILS_DIR`**; **`KERN_LEGACY_SRC_INCLUDES`** replaced by **`KERN_SRC_GLUE_INCLUDES`** (only transitional **`src/`** glue). Includes unchanged at the source level (**`game/...`**, **`backend/...`**, **`utils/...`**) via layer include roots.

- **Build layout (Phase 10):** Native VM modules moved from **`src/modules/`** to **`kern/modules/`** (`g2d/`, `g3d/`, `system/`). CMake: **`KERN_MODULES_DIR`**, **`KERN_MODULES_SRC_DIR`**, **`KERN_MODULES_INCLUDES`**; **`KERN_TOOLCHAIN_PRIVATE_INCLUDES`** lists modules before legacy **`src/`**. Added **`kern/modules/builtin_module_registry.*`** with **`get_builtin_modules()`** (placeholder `init` pointers for future plugins). Standalone / graphics CMake and **`cpp_backend`** emitted project lists updated for the new paths.

- **Build layout (Phase 7):** C++ CLI entrypoints moved to **`kern/tools/`** (`main.cpp`, `kernc_main.cpp`, `repl_main.cpp`, `lsp_main.cpp`, `scan_main.cpp`, **`version_info.rc.in`**). Shared toolchain objects compile into static **`kern_core`**; Raylib/game surface is **`kern_gfx`** linked only by binaries that need graphics (not **`kern_lsp`**). CMake: **`KERN_TOOLS_DIR`** / **`KERN_CLI_DIR`** → **`kern/tools/`**; repo-root launchers use **`KERN_REPO_TOOLS_DIR`** (`tools/`).

- **Version metadata:** `kern.json`, `kern-registry/package.json`, and `kern-registry/package-lock.json` root entries aligned with **`KERN_VERSION.txt`**; **`README.md`**, **`RELEASE.md`**, and **`docs/RELEASE_CHECKLIST.md`** document matching files and avoid stale example versions.

---

## [1.0.17] - 2026-04-08

### Fixed

- **kern-bootstrap (Windows):** reinstall menu `[6]` removed `versions/` but did not recreate it before `rename`, causing `Promoting version directory` to fail with “path not found” (os error 3).
- **kern-bootstrap (Windows):** post-install strict verify now invokes `kern.cmd` / `kargo.cmd` by full path instead of `where kern` / `where kargo`, so a stray `C:\Windows\System32\kern` cannot make verification fail when managed shims are correct.

---

## [1.0.16] - 2026-04-08

### Fixed

- **Graphics imports:** `kern_core` is now compiled with `KERN_BUILD_GAME` when game/Raylib is enabled. Previously only CLI targets received the define, so `import_resolution.cpp` omitted the `g2d` / `g3d` / `game` branches while `kern --version` still reported Raylib — release zips could not load graphics modules.

---

## [1.0.15] - 2026-04-08

### Fixed

- **Release CI:** `kern-bootstrap` builds on Linux/macOS (import `DownloadContext` for the non-Windows `ensure_windows_node_for_kargo` stub).
- **Linux link (graphics):** `kern_gfx` now links `kern_core` so `game_builtins` / g2d resolve `VM` / `Value` symbols under GNU `ld`.
- **Release CI:** stable `.kn` suite runs with default permission enforcement (`KERN_ENFORCE_PERMISSIONS` was incorrectly forced off for the coverage step, breaking `test_permissions_smoke`).
- **Release CI:** Intel macOS bootstrapper builds on `macos-latest` via `x86_64-apple-darwin` (replaces removed `macos-13` runner).

---

## [1.0.14] - 2026-04-06

### Fixed

- **VM builtins:** `setGlobalFn("…", index)` now matches each `makeBuiltin` slot (fixes wrong builtins for `__spawn_task`, async, web helpers, `readFile` / `writeFile`, and related `.kn` tests).
- **Decorators:** pending `@command` / `@event` registry is stored on the VM instance so module imports no longer overwrite shared global decorator state.

---

## [1.0.13] - 2026-04-06

### Added

- **`kern-bootstrap/`** — production installer for Kern + Kargo from GitHub Releases; CI workflow **`.github/workflows/kern-bootstrap-ci.yml`**. After staging, **`install`** runs **`kern --version`** and **rejects** builds that report **`graphics: none`** (expects Raylib-backed **`g2d` / `g3d` / `game`** in official release zips); older Kern without a **`graphics:`** line logs a warning only.
- **`kern --version`** prints **`graphics: g2d+g3d+game (Raylib linked)`** or **`graphics: none`** for diagnostics and bootstrap checks.
- **`kargo search`** queries the **Kern registry** index by default (env **`KERN_REGISTRY_URL`** / local **`registry/registry.json`**); **`kargo search --github`** preserves GitHub repository search.

### Changed

- **CMake:** with **`KERN_BUILD_GAME=ON`**, configuration **fails** if Raylib cannot be resolved (no silent headless **`kern`**). **`kern_lsp`** links **`kern_gfx`** when graphics are enabled. **Release / NSIS / `build.ps1`** ship **`kern_game`**, **`kern_repl`**, **`kern_lsp`** where built; CI verifies the **`graphics:`** line on **`kern --version`**.

---

## [1.0.12] - 2026-04-08

### Added

- **Windows NSIS on releases:** The [Release workflow](.github/workflows/release.yml) now produces **`kern-windows-x64-v*-installer.exe`** (installs under Program Files with PATH shortcuts — same payload as **`build.ps1`** + **`installer.nsi`**) using **`scripts/package-windows-nsis-release.ps1`**, from the same fresh **`build/Release`** binaries as the portable zip.

---

## [1.0.11] - 2026-04-07

### Fixed

- **g2d:** `strokeRoundedRect` targets the 4-parameter `DrawRectangleRoundedLines` API and approximates line thickness when Raylib does not provide the thickness overload, restoring Release CI builds (Windows, Linux, macOS).
- **Release CI:** ship missing `cmake/kern_paths.cmake` and builtin-module sources; set `KERN_ENFORCE_PERMISSIONS=0` for verify/coverage steps so trusted smoke and stable suites match permission-heavy std/process paths; Release workflow runs `run_stable.ps1 -Quick` (contract + bytecode golden) so tag builds are not blocked by the full `.kn` coverage matrix (still exercised on branch CI).
- **`append_file`:** implement append via `fopen(..., "ab")` / `fwrite` / `fclose`; fix `setGlobalFn` indices for `append_file` / `appendFile` / `require` (were computed after socket builtins, so globals called the wrong slots).

---

## [1.0.10] - 2026-04-05

### Added

#### Package registry foundation (`kern-registry`) + CLI integration

- **Purpose:** Introduce a production-oriented package workflow for Kern with registry metadata, semver resolution, lockfile generation, integrity checks, offline cache reuse, and first-class CLI entrypoints.
- **New monorepo:** [`kern-registry/`](kern-registry/) with:
  - static registry layout (`registry/registry.json`, `packages/<name>/metadata.json`, `packages/<name>/versions/<ver>.json`)
  - Node API server (`server/index.js`, routes for publish/package/search)
  - CLI (`kern-pkg`) with `publish`, `install`, `search`, `info`
  - schemas and unit tests
- **Install pipeline:** lockfile-aware recursive dependency resolution with semver (`exact`, `^`, `~`), cycle detection, dedupe, SHA256 verification, extraction into `.kern/packages/<pkg>/<version>/`, and cache in `~/.kern/cache`.
- **Publish pipeline:** package validation + tarball creation + SHA256, static registry index updates, optional public release flow (`--public`) and preview mode (`--dry-run`).
- **Kern command surface:** `kern install [pkg@range]`, `kern publish`, `kern search`, `kern info` delegate to registry CLI; `kern add pkg@range` uses registry install flow when available.

### Changed

- **Manifest/lock compatibility:** `kern.json` dependency parsing now accepts both legacy array and object map forms; lockfile refresh emits lockVersion 2 package map shape.
- **Import resolution:** bare package imports now resolve through `.kern/package-paths.json` when present, enabling runtime loading of installed packages by package name.
- **Documentation:** root [`README.md`](README.md) now includes package command quickstart and links to [`kern-registry/README.md`](kern-registry/README.md).

### Security

- **Integrity enforcement:** downloaded package artifacts are validated against SHA256 integrity metadata before extraction; tampered artifacts fail installation.

---

## [1.0.7] - 2026-04-03

### Added

#### TCP/UDP networking builtins (permission-gated)

- **Purpose:** First-class **TCP** and **UDP** socket access from `.kn` scripts for custom protocols, small servers, and multiplayer experiments — **not** a full netcode stack. All entry points are **builtins** registered alongside `std.v1.*` and guarded by **`network.tcp`** / **`network.udp`** (via `require("network.tcp")` or `kern --allow=network.tcp`, etc.).
- **Core implementation files:** [`kern/runtime/vm/kern_socket.hpp`](kern/runtime/vm/kern_socket.hpp), [`kern/runtime/vm/kern_socket.cpp`](kern/runtime/vm/kern_socket.cpp) (Windows **Winsock** + **`ws2_32`**, Unix **BSD sockets**); [`kern/runtime/vm/std_builtins_socket.inl`](kern/runtime/vm/std_builtins_socket.inl); [`kern/runtime/vm/permissions.hpp`](kern/runtime/vm/permissions.hpp) (`Perm::kNetworkTcp`, `Perm::kNetworkUdp`).
- **CMake / linkage:** [`CMakeLists.txt`](CMakeLists.txt) adds **`kern_socket.cpp`** to **`kern`**, **`kern_repl`**, **`kernc`**, **`kern-scan`**, **`kern_lsp`**, etc.; on **`_WIN32`**, links **`ws2_32`** with **`winhttp` / `wininet` / `psapi`** as needed.
- **TCP surface:** **`tcp_connect`**, **`tcp_listen`**, **`tcp_accept`**, **`tcp_send`**, **`tcp_recv`**, **`tcp_close`**; **`tcp_connect_start`** / **`tcp_connect_check`** for non-blocking handshake (internal **`TcpConnecting`** state, stored **`sockaddr_storage`**, second **`connect()`** probe + **`select` write** + **`getsockopt(SO_ERROR)`**, including **`SO_ERROR` poll when `select` returns 0**); prefer **`"0.0.0.0"`** listen when clients use **`127.0.0.1`** on Windows (IPv4/IPv6 bind pitfalls).
- **UDP surface:** **`udp_open`**, **`udp_bind`**, **`udp_send`**, **`udp_recv`**, **`udp_close`** with **`would_block`** where applicable.
- **Polling:** **`socket_set_nonblocking`**, **`socket_select_read`**, **`socket_select_write`** ( **`FD_SETSIZE`** cap; **`timeout_ms`** **`-1`** = block, **`0`** = poll ).
- **Registration:** [`kern/runtime/vm/builtins.hpp`](kern/runtime/vm/builtins.hpp) — **`kSocketBuiltinCount`**, **`getBuiltinNames()`** order, include **`std_builtins_socket.inl`** after **`std_builtins_v1.inl`**.
- **Examples:** [`examples/network/`](examples/network/) — echo server/client, **`tcp_select_accept.kn`**, **`tcp_async_client.kn`**; [`examples/network/README.md`](examples/network/README.md) documents **`cli_args()`** layout.

#### Bytecode pipeline

- [`kern/core/bytecode/bytecode_peephole.hpp`](kern/core/bytecode/bytecode_peephole.hpp) / [`.cpp`](kern/core/bytecode/bytecode_peephole.cpp) — safe peephole pass (NOP / label remap) wired through VM / codegen paths.
- [`kern/runtime/vm/bytecode_verifier.hpp`](kern/runtime/vm/bytecode_verifier.hpp) / [`.cpp`](kern/runtime/vm/bytecode_verifier.cpp) — bytecode structural verification before execution where enabled.

#### LSP

- [`src/lsp/lsp_main.cpp`](src/lsp/lsp_main.cpp) — **`textDocument/documentSymbol`** and **workspace symbol** support for outline and cross-file symbol search.

#### Documentation

- [`docs/INTERNALS.md`](docs/INTERNALS.md), [`INTERNALS_ARCHITECTURE.md`](docs/INTERNALS_ARCHITECTURE.md), [`INTERNALS_COMPILER_AND_BYTECODE.md`](docs/INTERNALS_COMPILER_AND_BYTECODE.md), [`INTERNALS_VM.md`](docs/INTERNALS_VM.md), [`INTERNALS_MODULES_AND_SECURITY.md`](docs/INTERNALS_MODULES_AND_SECURITY.md), [`NETWORKING_MULTIPLAYER.md`](docs/NETWORKING_MULTIPLAYER.md), [`PRODUCTION_VISION.md`](docs/PRODUCTION_VISION.md); [`mkdocs.yml`](mkdocs.yml) nav; [`docs/overrides/searchbox.html`](docs/overrides/searchbox.html) (search form **`id`** fix for strict HTML).

#### Tests & tooling

- Coverage: [`tests/coverage/socket_tcp_refused.kn`](tests/coverage/socket_tcp_refused.kn), [`test_permissions_smoke.kn`](tests/coverage/test_permissions_smoke.kn), [`test_append_file_builtin.kn`](tests/coverage/test_append_file_builtin.kn), [`test_stack_trace_has_source_path.kn`](tests/coverage/test_stack_trace_has_source_path.kn), bytecode golden artifacts; stable runners [`tests/run_stable.sh`](tests/run_stable.sh), [`.ps1`](tests/run_stable.ps1), [`.cmd`](tests/run_stable.cmd), repo [`stable.ps1`](stable.ps1) / [`stable.cmd`](stable.cmd); [`src/tests/humanize_path_contract_test.cpp`](src/tests/humanize_path_contract_test.cpp); [`.gitattributes`](.gitattributes).
- Site: [`tools/generate_sitemap.py`](tools/generate_sitemap.py), [`normalize_site_search_forms.py`](tools/normalize_site_search_forms.py), [`postprocess_site.py`](tools/postprocess_site.py).

### Changed

- VM / errors / compiler / CLI / modules / stdlib / examples / docs / tests / CI / [`.vscode/tasks.json`](.vscode/tasks.json) — see git history for this tag; highlights include [`kern/runtime/vm/vm.cpp`](kern/runtime/vm/vm.cpp), [`vm_error_registry.hpp`](kern/core/errors/vm_error_registry.hpp), [`src/main.cpp`](src/main.cpp), [`src/lsp/lsp_main.cpp`](src/lsp/lsp_main.cpp), workflow YAML under [`.github/workflows/`](.github/workflows/).

### Notes for script authors

- Prefer **`socket_select_write`** with **positive** **`timeout_ms`** between **`tcp_connect_check`** polls to avoid tight loops that hit **VM step limits** on some hosts.
- Networking requires explicit permissions; see [`docs/TRUST_MODEL.md`](docs/TRUST_MODEL.md) and [`docs/INTERNALS_MODULES_AND_SECURITY.md`](docs/INTERNALS_MODULES_AND_SECURITY.md).

---

## [1.0.6] - 2026-04-02

### Summary

Cross-platform **CI**, **macOS** **`#include <version>`** / case-insensitive **`VERSION`** fix, **GCC `-Werror`** on Linux, **`kern docs`** / **`kern build`**, **MkDocs** site skeleton, **Docker** Linux image, **Linux/macOS** release tarballs, and **code-of-conduct** doc.

### Fixed (detailed)

- **`VERSION` → `KERN_VERSION.txt`:** On **case-insensitive** APFS/HFS+, a root file **`VERSION`** can shadow **C++20 `<version>`**; semver moved to **[`KERN_VERSION.txt`](KERN_VERSION.txt)**; [`CMakeLists.txt`](CMakeLists.txt) reads it for **`KERN_VERSION`** and Windows **`version_info.rc`**.
- **macOS `env_all()`:** **`_NSGetEnviron()`** from **`<crt_externs.h>`** (Darwin does not expose **`::environ`** like glibc).
- **`version_info.rc`:** Compiled only on **Windows** so **Apple Clang** / Linux do not invoke the PE resource compiler.
- **GCC `-Werror` (Linux CI):** Integer overload disambiguation; remove pessimizing **`std::move`** on **`parameterList`**; **`dynamic_cast`** assignment warnings; indentation in **`mem_fill_pattern`** / **`uuid`**; **`std::tm`** / **`sigVals`** scoped to Windows where needed; **`system()`** result in REPL **`clear`**; **`env_all`** uses **`::environ`**; gate **`process_module`** **`toInt`** on **`_WIN32`**; **`<cstdint>`** in **`build_cache.hpp`** for **`uint64_t`**.

### Added (detailed)

- **`kern docs`**, **`kern build`**; **Linux/macOS** workflows; **Linux** **`KERN_WERROR=ON`** + **`mkdocs build --strict`**; **releases** attach **Linux**/**macOS** tarballs; **[`docs/index.md`](docs/index.md)**, **ADOPTION_ROADMAP**, **`mkdocs.yml`**; **`CODE_OF_CONDUCT.md`**; **`Dockerfile`**; **`.gitignore`** **`site/`**.

### Changed (detailed)

- **Version file** name is **`KERN_VERSION.txt`** only; update any scripts that referenced **`VERSION`**.

---

## [1.0.5] - 2026-04-02

### Summary

**`from "m" import a, b`**, **`kern verify`**, JSON stack **`filename`**, **`kern --trace`**, REPL **`last`**, **`--strict-types`** + **`typed_builtins.hpp`**, **`kern test --grep/--list/--fail-fast`**, **`kern doctor`**, expanded docs.

### Added (detailed)

- **Lexer** **`FROM`**; **`kern verify`** + **`tests/coverage/kern_verify_fixture/`**; **`docs/ERROR_CODES.md`**; **`docs/STRICT_TYPES.md`**, **`tests/strict_types_phase2/`**; **`lib/kern/stdlib/strict_types_slice.kn`**; roadmap/memory/trust/implementation docs; **`kern test`** filters.

### Changed (detailed)

- **`CONTRIBUTING.md`**; **`kern test`** skips strict-only negative **`fail_mismatch.kn`** in default runtime sweep.

---

## [1.0.4] - 2026-04-02

### Summary

**CI** skips headless **g3d**/**coverage** full runs on Windows runners; **GitHub Releases** via **`softprops/action-gh-release`**.

### Changed (detailed)

- **Version bump** to repair failed **`v1.0.3`** publish on headless runners.

---

## [1.0.3] - 2026-04-02

### Summary

**`std.v1.*`** + **`std_*`** builtins, **`kern --scan`**, **`lib/kern/stdlib/`**, **`Kern-IDE/`**, docs refresh, **Windows** release zip on **`v*`** tags.

### Added (detailed)

- **[`kern/runtime/vm/std_builtins_v1.inl`](kern/runtime/vm/std_builtins_v1.inl)**, **[`src/stdlib_stdv1_exports.hpp`](src/stdlib_stdv1_exports.hpp)**; **`kern-scan`**; **`lib/kern/stdlib/`** catalog; **IDE** sources; **CI** **`kern --scan --registry-only`**.

### Changed (detailed)

- **Layout:** see **[`docs/NESTED_KERN_TREE_REMOVED.md`](docs/NESTED_KERN_TREE_REMOVED.md)**.

---

## [1.0.2] - 2026-04-02

### Summary

**Lambda closures** (**`BUILD_CLOSURE`**, **`FunctionObject::captures`**), **`kern run`**, BOM/shebang, **install** scripts, **`kern::process`** **VirtualQueryEx** example.

### Added (detailed)

- **VM** closure calling convention; **CLI** script discovery; **CMake** **`install`**; **`system_process_safe_read.kn`**.

### Fixed (detailed)

- **Example** console-safe messages.

---

## [1.0.1] - 2026-04-01

### Summary

**Doc consolidation**, recursive **examples** test, **`kernc -o`** **HTTP** link on Windows, **bounded tracebacks** (256 frames), **stress** suite, **VM** tail-call vs **max depth**, **lexer** size/token limits, **non-recursive `??`**.

### Changed (detailed)

- **Docs** lean set; **`run_all_tests.ps1`** recursive **`examples/`**; **TESTING.md** paths; **STRESS** pointer to **`tests/coverage/`**; **TROUBLESHOOTING** VM notes.

### Fixed (detailed)

- **`kernc -o`**: **`http_get_winhttp.cpp`**, **`winhttp`/`wininet`**; **traceback** bounds.

### Added (detailed)

- **`tests/stress/`**, **`run_stress_suite.ps1`**; **VM** **`maxCallDepth_`** / tail-call policy; **lexer** BOM handling; **ASCII** diagnostic bullets.

### Security / robustness (detailed)

- **Lexer** 48 MiB / 8M token caps; **parser** iterative **`??`**.

---

## [1.0.0] - 2025-03-07

### Summary

Initial **Kern** release: **language** + **VM** + **builtins** + **`import`** modules (**`g2d`**, **`game`**, …) + **CLI** + **diagnostics** + optional **Raylib** + **Electron IDE** docs.

### Language (detailed)

- **Paradigms:** imperative, functional, **OOP**, **pattern matching**, **destructuring**; **`let`/`var`/`const`**; control flow; **`def`**, lambdas; arrays/maps; **`?.`**, **`??`**, f-strings, ranges; **`class`**, **`extends`**.

### Standard library (detailed)

- **Builtins:** math, strings, collections, I/O, **JSON**, time, env, reflection; **`import`**, **`kern_version`**, **`cli_args`**, **`platform`**, etc.

### Graphics (detailed)

- **`g2d`**, **`game`** (**Raylib**), **`KERN_BUILD_GAME`**.

### CLI (detailed)

- **Run**, **REPL**, **`--version`**, **`--check`**, **`--fmt`**, **`--ast`**, **`--bytecode`**.

### Errors (detailed)

- Line/column, snippets, hints, traces, categories.

### IDE (detailed)

- **Electron + Monaco** (historical layout; see later tags for **`Kern-IDE/`**).

### Docs (detailed)

- **README**, **GETTING_STARTED**, **TROUBLESHOOTING**, **RELEASE.md**.

### Build (detailed)

- **CMake 3.14+**, **C++17**, optional **Raylib**; version from **`KERN_VERSION.txt`** (in post-1.0.0 trees; **`VERSION`** renamed in **1.0.6**).

[Unreleased]: https://github.com/entrenchedosx/kern/compare/v1.0.20...HEAD
[1.0.20]: https://github.com/entrenchedosx/kern/compare/v1.0.19...v1.0.20
[1.0.19]: https://github.com/entrenchedosx/kern/compare/v1.0.18...v1.0.19
[1.0.18]: https://github.com/entrenchedosx/kern/compare/v1.0.17...v1.0.18
[1.0.17]: https://github.com/entrenchedosx/kern/compare/v1.0.16...v1.0.17
[1.0.16]: https://github.com/entrenchedosx/kern/compare/v1.0.15...v1.0.16
[1.0.15]: https://github.com/entrenchedosx/kern/compare/v1.0.14...v1.0.15
[1.0.14]: https://github.com/entrenchedosx/kern/compare/v1.0.13...v1.0.14
[1.0.13]: https://github.com/entrenchedosx/kern/compare/v1.0.12...v1.0.13
[1.0.12]: https://github.com/entrenchedosx/kern/compare/v1.0.11...v1.0.12
[1.0.11]: https://github.com/entrenchedosx/kern/compare/v1.0.10...v1.0.11
[1.0.10]: https://github.com/entrenchedosx/kern/compare/v1.0.9...v1.0.10
[1.0.7]: https://github.com/entrenchedosx/kern/compare/v1.0.6...v1.0.7
[1.0.6]: https://github.com/entrenchedosx/kern/compare/v1.0.5...v1.0.6
[1.0.5]: https://github.com/entrenchedosx/kern/compare/v1.0.4...v1.0.5
[1.0.4]: https://github.com/entrenchedosx/kern/compare/v1.0.3...v1.0.4
[1.0.3]: https://github.com/entrenchedosx/kern/compare/v1.0.2...v1.0.3
[1.0.2]: https://github.com/entrenchedosx/kern/compare/v1.0.1...v1.0.2
[1.0.1]: https://github.com/entrenchedosx/kern/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/entrenchedosx/kern/releases/tag/v1.0.0
