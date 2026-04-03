# Production vision: stable, powerful, developer-friendly Kern

This document maps a **long-horizon** goal (“production-ready language with strong diagnostics, rich stdlib, and controlled OS access”) onto **phased milestones** that align with the existing codebase. It does **not** replace [LANGUAGE_ROADMAP.md](LANGUAGE_ROADMAP.md); it **prioritizes** work for stability and real-world use.

**Related:** [TRUST_MODEL.md](TRUST_MODEL.md) (trust-the-programmer + optional safety), [ERROR_CODES.md](ERROR_CODES.md), [STRICT_TYPES.md](STRICT_TYPES.md), [BUILTIN_REFERENCE.md](BUILTIN_REFERENCE.md), [STDLIB_STD_V1.md](STDLIB_STD_V1.md).

---

## 1. What “production-ready” means here

| Pillar | Target outcome | How we measure it |
|--------|----------------|-------------------|
| **Stability** | No crashes on valid programs; defined behavior on invalid input | Fuzz + CI stress; zero UB in hot paths; consistent VM semantics |
| **Clarity** | Errors are actionable (location, code, hint) | Same codes across CLI / `--check` / IDE; regression tests for messages |
| **Capability** | Filesystem, env, process/shell where needed—**not locked down by default** | Policy flags documented; defaults match [TRUST_MODEL.md](TRUST_MODEL.md) |
| **Ergonomics** | Types and strictness are **opt-in**; stdlib is namespaced (`std.v1.*`) | Users can start scripts quickly; stricter projects flip flags |

---

## 2. Baseline already in the repo

Before opening large new subsystems, use what exists:

- **Diagnostics:** `src/errors.*`, JSON output for tooling, [ERROR_CODES.md](ERROR_CODES.md) (stable categories like `LEX-*`, `VM-*`; not necessarily `KERN001`—extending the catalog is a documentation + emitter task).
- **Strict typing (preview):** `--strict-types`, `typed_builtins.hpp`, semantic paths behind feature flags—**extend** rather than fork.
- **Builtins:** **Hundreds** of natives; registry in `getBuiltinNames()` in [`src/vm/builtins.hpp`](../src/vm/builtins.hpp)—**append-only** indices. See [BUILTIN_REFERENCE.md](BUILTIN_REFERENCE.md).
- **Versioned stdlib:** `std.v1.*` via [stdlib_stdv1_exports.hpp](../src/stdlib_stdv1_exports.hpp) and [stdlib_modules.cpp](../src/stdlib_modules.cpp).
- **Validation:** `kern --scan`, `kern --check`, `kern test`—expand coverage rather than inventing a second harness.
- **IDE:** Kern-IDE (Tk and Electron tracks); improvements should share diagnostics with the compiler where possible.

---

## 3. Phased roadmap (recommended order)

### Phase P0 — Stability and honesty (highest ROI)

**Goals:** fewer crashes, no silent corruption, predictable errors.

- Harden VM paths (bounds, arity, map/array access) with **consistent** `VMError` + codes.
- Ensure **no silent failures** on I/O and OS builtins: return values or explicit error objects; document behavior.
- Expand **automated** regression: `kern test`, stress suites, `--scan` in CI.
- **AST / IR validation:** strengthen the pipeline *before* execution (reuse existing semantic passes; add gates where missing).

**Exit criteria:** CI green on Windows + primary platforms; documented behavior for failure modes on core builtins.

### Phase P1 — Diagnostics and optional strictness

**Goals:** “feels like a modern tool” without forcing types on every script.

- Richer spans (line/column), secondary labels, **“did you mean?”** suggestions (tie into symbol tables / edit distance where cheap).
- **Strict mode** as a **project or flag** contract: `--strict-types`, stricter `kern.json` profiles.
- Centralize **user-facing** error text: map internal codes to stable **public** codes if you adopt `KERN###` (requires updating [ERROR_CODES.md](ERROR_CODES.md) and emitters in one pass).

**Exit criteria:** IDE and CLI show the same code + message for the same failure; strict profile documented.

### Phase P2 — Language surface (careful, one vertical per release)

Per [LANGUAGE_ROADMAP.md](LANGUAGE_ROADMAP.md) **complexity budget**: ship **one** major language vertical per release.

Candidate verticals (not all at once):

- **`const`** and clearer immutability rules.
- **Enums** (algebraic or C-style—pick one design).
- **Structs / simple classes** (field layout + construction).
- **Function overloading** (resolution rules + diagnostics when ambiguous).
- **Modules / namespaces** (sealed names at compile time; complements runtime `import`).

Each needs: grammar, semantic rules, tests, and **migration notes**.

### Phase P3 — Runtime performance (measure first)

**Goals:** speed without sacrificing safety defaults.

- Profile allocation hot paths; pool or reuse where proven.
- **Optional** expression / bytecode caching for repeated eval (e.g. REPL, templating)—only after benchmarks justify it.
- **JIT-style** work is **exploratory**: treat as research unless a clear win on representative workloads.

**Exit criteria:** Benchmarks in-repo; no regression on correctness tests.

### Phase P4 — Standard library and OS surface

**Goals:** batteries included; **permission-based or toggleable** access, **not** restricted by default (see [TRUST_MODEL.md](TRUST_MODEL.md)).

- Grow **`std.v1.*`** (and future tiers) with clear naming; keep globals minimal.
- Filesystem: read/write/exists/delete/list/copy/mkdir—many patterns already exist; add missing pieces **by appending** new builtin indices.
- OS: env, shell/process helpers, system info—implement with explicit **policy** (env var + CLI flag + optional `kern.json`) so CI and embedded users can lock down.

**Exit criteria:** Each new surface has docs, tests, and a default that matches the trust model.

### Phase P5 — Tooling and distribution

- **Testing framework:** extend `kern test` and conventions (filters, fixtures) before inventing a second runner.
- **IDE:** autocomplete and inline errors consume the same JSON diagnostics as Phase P1; **step debugging** hooks into VM (breakpoint map + stepping—roadmap item in LANGUAGE_ROADMAP).
- **Distribution:** portable builds ([GETTING_STARTED.md](GETTING_STARTED.md)); version manager / auto-update are **product** decisions—document constraints (code signing, channels) before coding.

---

## 4. Explicit non-goals for a single change set

The following require **multiple releases** and coordinated design:

- “50+ new builtins” as a **counting exercise** (the VM already exposes a large set)—focus on **coverage and docs**, not raw number.
- Full **JIT**, **auto-update**, and **version manager** without product requirements.
- **Enums + overloading + classes + namespaces** in one merge.

---

## 5. How to use this doc

- **Contributors:** Pick a **phase**; open issues with **exit criteria** from the table above.
- **Maintainers:** Align release notes with phases; avoid shipping half of two verticals.
- **Users:** Expect **progressive** delivery: stability and diagnostics first, language sugar on a controlled schedule.

When this vision and the detailed roadmap diverge, **LANGUAGE_ROADMAP.md** wins on technical design; this file wins on **ordering for production readiness**.
