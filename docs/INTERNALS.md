# Kern internals — overview

This section is for **engineers shipping Kern in real projects**: contributors, tooling authors, CI maintainers, and anyone embedding or auditing the runtime. User-facing language docs live elsewhere ([LANGUAGE_SYNTAX.md](LANGUAGE_SYNTAX.md), [BUILTIN_REFERENCE.md](BUILTIN_REFERENCE.md)).

## Audience

| Role | Start here |
|------|------------|
| **Application team** adopting Kern in production | [INTERNALS_MODULES_AND_SECURITY.md](INTERNALS_MODULES_AND_SECURITY.md), [TRUST_MODEL.md](TRUST_MODEL.md), [TESTING.md](TESTING.md) |
| **Tool / IDE author** | [INTERNALS_ARCHITECTURE.md](INTERNALS_ARCHITECTURE.md), [INTERNALS_COMPILER_AND_BYTECODE.md](INTERNALS_COMPILER_AND_BYTECODE.md), [ERROR_CODES.md](ERROR_CODES.md) |
| **Runtime / embedder** | [INTERNALS_VM.md](INTERNALS_VM.md), [MEMORY_MODEL.md](MEMORY_MODEL.md), [PRODUCTION_VISION.md](PRODUCTION_VISION.md) |

## What “stable” means (honest contract)

Kern aims for **predictable behavior** and **actionable diagnostics**, not informal “it works on my machine.”

- **Language & stdlib surface** evolve under semver ([`KERN_VERSION.txt`](../KERN_VERSION.txt)); breaking changes should be called out in `CHANGELOG.md`.
- **Bytecode layout** is an **implementation detail** between `kernc`/codegen and the VM. Treat serialized bytecode as **not** a stable cross-version file format unless you explicitly version and pin a toolchain.
- **Builtin indices** in [`kern/runtime/vm/builtins.hpp`](../kern/runtime/vm/builtins.hpp) are **append-only** by policy; new builtins extend the table rather than reshuffling indices used by existing scripts.
- **Diagnostics** use stable string **codes** (e.g. `VM-DIV-ZERO`, `LEX-TOKENIZE`) for automation; see [ERROR_CODES.md](ERROR_CODES.md).

## Verification you can rely on

For production pipelines, combine:

1. **`kern --scan`** — registry / export consistency ([KERN_SCAN.md](KERN_SCAN.md)).
2. **`kern --check`** (and `--json` where applicable) — compile-time errors with structured output.
3. **`kern test`** — project and regression harness ([TESTING.md](TESTING.md)).
4. **Lockfile / manifest** — `kern verify` and `kern.json` + `kern.lock` for reproducible dependency names.
5. **CI** — Windows / Linux / macOS jobs plus release artifacts (see `.github/workflows/`).

“Flawless” in practice means **defining SLOs** (e.g. no uncaught native crashes on valid scripts, bounded VM steps in untrusted contexts) and **measuring** them — the machinery above is how you get there.

## Internals map

| Document | Contents |
|----------|----------|
| [INTERNALS_ARCHITECTURE.md](INTERNALS_ARCHITECTURE.md) | Repo layout, major subsystems, request/compile/run path |
| [INTERNALS_COMPILER_AND_BYTECODE.md](INTERNALS_COMPILER_AND_BYTECODE.md) | Lexer → AST → codegen, bytecode model, verification, peephole |
| [INTERNALS_VM.md](INTERNALS_VM.md) | VM execution, guards, limits, builtins, errors |
| [INTERNALS_MODULES_AND_SECURITY.md](INTERNALS_MODULES_AND_SECURITY.md) | Imports, `kern.json`, permissions, FFI boundary |

## Related documents

- [IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md) — recent feature-level implementation notes (not a full architecture spec).
- [PRODUCTION_VISION.md](PRODUCTION_VISION.md) — phased roadmap for product hardening.
- [ADOPTION_ROADMAP.md](ADOPTION_ROADMAP.md) — ecosystem and packaging themes.
