# Contributing to Kern

Thanks for helping improve Kern. This document sets expectations so reviews stay focused and releases stay safe.

## What belongs in this repo

- **Design stance:** Kern is **trust-the-programmer** at the core; optional safety belongs in libraries, hosts, and flags — see [docs/TRUST_MODEL.md](docs/TRUST_MODEL.md).
- **In scope:** compiler, VM, builtins, `lib/kern/` stdlib, CLI (`kern`, `kernc`, `kern-scan`), tests, docs, CMake, CI workflows.
- **Editors / IDE UI:** live under [`Kern-IDE/`](Kern-IDE/README.md). Changes there are welcome but are packaged and released separately from the core `kern` binary.

## Before you open a PR

1. **Build** in Release and ensure the tree compiles without new warnings you introduced.
2. **Run tests** relevant to your change:
   - `kern test tests/coverage` (or a narrower path), and/or
   - `.\tests\kernc\run_kernc_tests.ps1` if you touched the compiler pipeline.
3. **Run** `kern --check` on any new or modified `.kn` files.
4. **Prefer small PRs** with a clear description of behavior change and any compatibility notes.

## Code style (C++)

- **C++17** unless a file already uses a newer standard for a specific target.
- Match **surrounding style** in the file you edit (naming, brace placement, error handling).
- **Do not** reorder `getBuiltinNames()` entries or builtin registration indices — additions are **append-only** (see [BUILTIN_REFERENCE.md](docs/BUILTIN_REFERENCE.md)).
- **Strict types:** if a builtin should participate in `kern --check --strict-types` for patterns like `let x: float = sqrt(1.0)`, add an **append-only** row to [`src/compiler/typed_builtins.hpp`](src/compiler/typed_builtins.hpp) (return type name: `int`, `float`, `string`, `bool`). See [docs/STRICT_TYPES.md](docs/STRICT_TYPES.md).
- Avoid drive-by refactors unrelated to the bug or feature.

## Documentation

- Update **CHANGELOG.md** under `[Unreleased]` for user-visible changes (Added / Changed / Fixed).
- If you add CLI flags or modules, link from **README.md** or the relevant **docs/** page.

## Security

- Do not commit secrets, API keys, or machine-specific absolute paths in examples.
- If you find a security issue, report it privately to maintainers if that process exists; otherwise open a discrete issue.

## License

By contributing, you agree your contributions are licensed under the same license as the project ([MIT](LICENSE)).
