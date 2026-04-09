# Changelog

All notable changes to Kern are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [Unreleased]

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

[Unreleased]: https://github.com/entrenchedosx/kern/compare/v1.0.18...HEAD
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
