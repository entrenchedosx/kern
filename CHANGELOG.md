# Changelog

All notable changes to Kern are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [Unreleased]

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
- **Core implementation files:** [`src/vm/kern_socket.hpp`](src/vm/kern_socket.hpp), [`src/vm/kern_socket.cpp`](src/vm/kern_socket.cpp) (Windows **Winsock** + **`ws2_32`**, Unix **BSD sockets**); [`src/vm/std_builtins_socket.inl`](src/vm/std_builtins_socket.inl); [`src/vm/permissions.hpp`](src/vm/permissions.hpp) (`Perm::kNetworkTcp`, `Perm::kNetworkUdp`).
- **CMake / linkage:** [`CMakeLists.txt`](CMakeLists.txt) adds **`kern_socket.cpp`** to **`kern`**, **`kern_repl`**, **`kernc`**, **`kern-scan`**, **`kern_lsp`**, etc.; on **`_WIN32`**, links **`ws2_32`** with **`winhttp` / `wininet` / `psapi`** as needed.
- **TCP surface:** **`tcp_connect`**, **`tcp_listen`**, **`tcp_accept`**, **`tcp_send`**, **`tcp_recv`**, **`tcp_close`**; **`tcp_connect_start`** / **`tcp_connect_check`** for non-blocking handshake (internal **`TcpConnecting`** state, stored **`sockaddr_storage`**, second **`connect()`** probe + **`select` write** + **`getsockopt(SO_ERROR)`**, including **`SO_ERROR` poll when `select` returns 0**); prefer **`"0.0.0.0"`** listen when clients use **`127.0.0.1`** on Windows (IPv4/IPv6 bind pitfalls).
- **UDP surface:** **`udp_open`**, **`udp_bind`**, **`udp_send`**, **`udp_recv`**, **`udp_close`** with **`would_block`** where applicable.
- **Polling:** **`socket_set_nonblocking`**, **`socket_select_read`**, **`socket_select_write`** ( **`FD_SETSIZE`** cap; **`timeout_ms`** **`-1`** = block, **`0`** = poll ).
- **Registration:** [`src/vm/builtins.hpp`](src/vm/builtins.hpp) — **`kSocketBuiltinCount`**, **`getBuiltinNames()`** order, include **`std_builtins_socket.inl`** after **`std_builtins_v1.inl`**.
- **Examples:** [`examples/network/`](examples/network/) — echo server/client, **`tcp_select_accept.kn`**, **`tcp_async_client.kn`**; [`examples/network/README.md`](examples/network/README.md) documents **`cli_args()`** layout.

#### Bytecode pipeline

- [`src/vm/bytecode_peephole.hpp`](src/vm/bytecode_peephole.hpp) / [`.cpp`](src/vm/bytecode_peephole.cpp) — safe peephole pass (NOP / label remap) wired through VM / codegen paths.
- [`src/vm/bytecode_verifier.hpp`](src/vm/bytecode_verifier.hpp) / [`.cpp`](src/vm/bytecode_verifier.cpp) — bytecode structural verification before execution where enabled.

#### LSP

- [`src/lsp/lsp_main.cpp`](src/lsp/lsp_main.cpp) — **`textDocument/documentSymbol`** and **workspace symbol** support for outline and cross-file symbol search.

#### Documentation

- [`docs/INTERNALS.md`](docs/INTERNALS.md), [`INTERNALS_ARCHITECTURE.md`](docs/INTERNALS_ARCHITECTURE.md), [`INTERNALS_COMPILER_AND_BYTECODE.md`](docs/INTERNALS_COMPILER_AND_BYTECODE.md), [`INTERNALS_VM.md`](docs/INTERNALS_VM.md), [`INTERNALS_MODULES_AND_SECURITY.md`](docs/INTERNALS_MODULES_AND_SECURITY.md), [`NETWORKING_MULTIPLAYER.md`](docs/NETWORKING_MULTIPLAYER.md), [`PRODUCTION_VISION.md`](docs/PRODUCTION_VISION.md); [`mkdocs.yml`](mkdocs.yml) nav; [`docs/overrides/searchbox.html`](docs/overrides/searchbox.html) (search form **`id`** fix for strict HTML).

#### Tests & tooling

- Coverage: [`tests/coverage/socket_tcp_refused.kn`](tests/coverage/socket_tcp_refused.kn), [`test_permissions_smoke.kn`](tests/coverage/test_permissions_smoke.kn), [`test_append_file_builtin.kn`](tests/coverage/test_append_file_builtin.kn), [`test_stack_trace_has_source_path.kn`](tests/coverage/test_stack_trace_has_source_path.kn), bytecode golden artifacts; stable runners [`tests/run_stable.sh`](tests/run_stable.sh), [`.ps1`](tests/run_stable.ps1), [`.cmd`](tests/run_stable.cmd), repo [`stable.ps1`](stable.ps1) / [`stable.cmd`](stable.cmd); [`src/tests/humanize_path_contract_test.cpp`](src/tests/humanize_path_contract_test.cpp); [`.gitattributes`](.gitattributes).
- Site: [`tools/generate_sitemap.py`](tools/generate_sitemap.py), [`normalize_site_search_forms.py`](tools/normalize_site_search_forms.py), [`postprocess_site.py`](tools/postprocess_site.py).

### Changed

- VM / errors / compiler / CLI / modules / stdlib / examples / docs / tests / CI / [`.vscode/tasks.json`](.vscode/tasks.json) — see git history for this tag; highlights include [`src/vm/vm.cpp`](src/vm/vm.cpp), [`vm_error_registry.hpp`](src/vm/vm_error_registry.hpp), [`src/main.cpp`](src/main.cpp), [`src/lsp/lsp_main.cpp`](src/lsp/lsp_main.cpp), workflow YAML under [`.github/workflows/`](.github/workflows/).

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

- **[`src/vm/std_builtins_v1.inl`](src/vm/std_builtins_v1.inl)**, **[`src/stdlib_stdv1_exports.hpp`](src/stdlib_stdv1_exports.hpp)**; **`kern-scan`**; **`lib/kern/stdlib/`** catalog; **IDE** sources; **CI** **`kern --scan --registry-only`**.

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

[Unreleased]: https://github.com/entrenchedosx/kern/compare/v1.0.7...HEAD
[1.0.7]: https://github.com/entrenchedosx/kern/compare/v1.0.6...v1.0.7
[1.0.6]: https://github.com/entrenchedosx/kern/compare/v1.0.5...v1.0.6
[1.0.5]: https://github.com/entrenchedosx/kern/compare/v1.0.4...v1.0.5
[1.0.4]: https://github.com/entrenchedosx/kern/compare/v1.0.3...v1.0.4
[1.0.3]: https://github.com/entrenchedosx/kern/compare/v1.0.2...v1.0.3
[1.0.2]: https://github.com/entrenchedosx/kern/compare/v1.0.1...v1.0.2
[1.0.1]: https://github.com/entrenchedosx/kern/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/entrenchedosx/kern/releases/tag/v1.0.0
