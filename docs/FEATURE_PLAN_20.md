# Plan: 20 features for Kern

This is a **concrete backlog** of twenty additive capabilities. It **does not replace** [LANGUAGE_ROADMAP.md](LANGUAGE_ROADMAP.md) (language design) or [ADOPTION_ROADMAP.md](ADOPTION_ROADMAP.md) (shipping/adoption); it **sequences** work you can pull into releases.

**Principles:** one diagnostic story, extend existing pipelines (`semantic`, `ir`, `vm`, `kernc`, LSP), avoid parallel parsers. Items are ordered roughly **foundation → language/stdlib → tooling → packaging → ambition**.

---

## Phase 1 — Execution & packaging (highest leverage)

| # | Feature | Value | Notes / deps |
|---|---------|--------|----------------|
| **1** | **Bytecode bundle backend for standalone** | Smaller `.exe`, faster startup, no embedded `.kn` sources in the hot path | Stable on-disk bytecode format + loader in generated host; same opcodes as VM today. Extends `StandaloneBackend`. |
| **2** | **Cross-host standalone builds** | Linux/macOS artifacts from the same `kern::compile` driver | Generalize `buildStandaloneExe` beyond MSVC-first CMake; align with release workflow. |
| **3** | **Debug symbols for standalone** | PDB on Windows; DWARF where applicable | Flags on generated CMake (`/DEBUG`, `strip` for release). |
| **4** | **FFI-capable standalone (allowlisted)** | Real apps need `extern`/`unsafe` in shipped EXEs | Policy table in `kernconfig` + host links needed libs; security story in [TRUST_MODEL.md](TRUST_MODEL.md). |
| **5** | **Bundle integrity manifest** | Optional hash list of embedded modules/assets verified at startup | Extends `BundleManifest` / packager. |

---

## Phase 2 — Modules, types, diagnostics

| # | Feature | Value | Notes / deps |
|---|---------|--------|----------------|
| **6** | **`kern graph` (static import graph)** | IDE, CI, and “what did I pull in?” | **Shipped:** `kern graph [--json] [--include-path dir]* <entry.kn>` uses `resolveProjectGraph`. Complements `kern-scan`. |
| **7** | **Lockfile enforcement everywhere** | Same guarantees for `kernc`, `kern test`, IDE | **`kern test`** and **`kern --check`** run the same dependency-set check as **`kern verify`** when **`kern.json`** exists in cwd (use **`--skip-lock-verify`** to opt out). **`kernc --pkg-validate`** unchanged for package manifests. |
| **8** | **Gradual typing: file or block pragma** | Safety without full-file strictness | Builds on `--strict-types` + semantic engine flags. |
| **9** | **Diagnostic spans: multi-token highlights** | Better UX for “this whole call is wrong” | Reporter + LSP range consistency. |
| **10** | **Stable bytecode / build IDs in `--version`** | Support + crash triage | **Shipped:** `kern --version` prints **`bytecode-schema:`** (`kern::kBytecodeSchemaVersion` in `bytecode.hpp`). Optional **`build:`** line when CMake sees a **`.git`** short hash (`KERN_BUILD_ID`). |

---

## Phase 3 — Standard library (`std.v1.*` and friends)

| # | Feature | Value | Notes / deps |
|---|---------|--------|----------------|
| **11** | **`std.v1` process & env parity** | Scripts need predictable `cwd`, `argv`, env maps | Align with existing `process` / `sys` builtins; document migration. |
| **12** | **Date/time: parsing + time zones (subset)** | Practical apps without pulling all of ICU | Start with UTC + named offset; avoid huge deps. |
| **13** | **Binary / bytes I/O helpers** | Network and file protocols | Extend `std.v1.bytes` + `fs` read/write binary if missing. |
| **14** | **Structured logging channel** | `log.*` with levels + optional JSON lines | Maps cleanly to observability story. |

---

## Phase 4 — Tooling & developer experience

| # | Feature | Value | Notes / deps |
|---|---------|--------|----------------|
| **15** | **`kern watch`** | Rerun `--check` or `kern test --grep` on save | Simple file watcher; defer to IDE for complex cases. |
| **16** | **`kern fmt` completeness** | Format new syntax (`match`, `\|>`, comprehensions) | **Partial:** `--fmt` still brace-indents only, but **`{` / `}` inside `//`, `/* */`, `"` / `'`, and `"""` / `'''`** no longer change depth (fewer broken reformats). Full AST-aware formatting + goldens still TBD. |
| **17** | **LSP: cross-file go-to-definition** | Jump from symbol to defining `.kn` | Project root + resolver; reuse scan graph. |
| **18** | **LSP / IDE: doc comments → hover** | `///` or agreed comment form → markdown hover | Parser trivia or lightweight preparse. |
| **19** | **Test runner: stdout / stderr snapshots** | Regress CLI programs without brittle string compares | Golden files next to `tests/`. |
| **20** | **VM observability hooks** | Opcode counters or optional trace zones | Behind flag; feeds perf work in [PRODUCTION_VISION.md](PRODUCTION_VISION.md). |

---

## How to use this list

- **Pick 1–2 items per release** from early phases unless you accept schedule slip ([LANGUAGE_ROADMAP.md](LANGUAGE_ROADMAP.md) “complexity budget”).
- **Mark dependencies** in issues (e.g. **#1** before shrinking standalone; **#6** before **#17**).
- **De-scope** aggressively: “subset of time zones”, “Windows PDB first”, “watch only `.kn` under one folder”.

---

## Out of scope for *this* list (already elsewhere or too large)

- Full **LLVM / native AOT** of Kern (see [STANDALONE_COMPILE_ARCHITECTURE.md](STANDALONE_COMPILE_ARCHITECTURE.md)).
- **Green threads / full async** language (see Tier C in language roadmap).
- **Self-hosted compiler in `.kn`** (multi-year; not one of the twenty above).

---

## Suggested first sprint (if you want momentum)

1. **#6** `kern graph` — unlocks IDE and docs.  
2. **#16** formatter gaps — daily pain.  
3. **#1** bytecode bundle — biggest standalone win.  

Then rotate toward **#2–#4** for shipping parity across OSes and real apps.
