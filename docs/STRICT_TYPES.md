# Strict types (`--strict-types`)

Phase 2 of the language roadmap wires **optional static checks** into `kern --check --strict-types` and `kern --scan --strict-types`.

## What is checked

1. **Typed `let` / `const` / `var`** — `let x: T = rhs` where `rhs` is either:
   - a **literal** (`bool`, `int`, `float`, `string`), or
   - a **single top-level builtin call** whose callee appears in `kern/core/compiler/typed_builtins.hpp` (return type is taken from that table).

2. **Assignments** — `name = rhs` when `name` was previously given a type via the regex pre-scan on `let name: Type = ...`.

## What is not checked

- Function parameters, method types, generics, or full inference across expressions.
- Callees not listed in the typed-builtin map: strict mode **skips** the mismatch check if the RHS type cannot be inferred (no false error).

## Tests

- **Pass (CI via `kern test`):** `tests/coverage/test_strict_types_phase2_pass.kn`
- **Negative (strict check only):** `tests/strict_types_phase2/fail_mismatch.kn` — run  
  `kern --check --strict-types tests/strict_types_phase2/fail_mismatch.kn` — expect non-zero exit.  
  This file is **skipped** by `kern test` (it is valid at runtime but wrong under strict typing).
- **Script:** `tests/strict_types_phase2/run_strict_types.ps1` (from repo root after building `kern`).

## Append-only policy

New typed surfaces for builtins **must** add a row to `typed_builtins.hpp` (see `CONTRIBUTING.md`).

---

## Breaking changes

**None.** `--strict-types` is fully opt-in. Existing programs behave exactly as before when the flag is not used.

## Design principles

- **Incremental typing** over full enforcement.
- **Predictability over complexity** — controlled inference only (literals + narrow builtin map).
- **Zero disruption** to existing codebases by default.
- **Append-only evolution** of `typed_builtins.hpp` for stability.

## Out of scope (next phases)

Not part of Phase 2:

- Full type inference (roadmap **A3.3**).
- LSP-driven type rules.
- Phase **3** features (**B1** / **B2**).
- Phase **4** systems (async runtime, performance work).

Those items stay on `docs/LANGUAGE_ROADMAP.md` and ship in later, controlled phases — not merged prematurely.

## Bottom line

Phase 2 delivers:

- A **working strict-typing foundation** tied to the existing semantic pass.
- **End-to-end integration:** compiler → stdlib slice → tests → docs.
- A **scalable base** for future type-system expansion.

Kern moves from purely dynamic toward a **hybrid, gradually typed** language without sacrificing simplicity or stability.
