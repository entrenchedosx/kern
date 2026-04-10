# Kern documentation

**Kern** is a small compiled language with a lexer → parser → bytecode VM pipeline, a growing standard library under `lib/kern/`, and a `kern` CLI.

## Quick links

| Topic | Document |
|--------|----------|
| **Start here (portable + KERN_HOME)** | [getting-started.md](getting-started.md) |
| Build & install (detailed) | [GETTING_STARTED.md](GETTING_STARTED.md) |
| Kargo (native) | [kargo-guide.md](kargo-guide.md) |
| Examples | [examples.md](examples.md) |
| Language overview | [language-guide.md](language-guide.md) |
| Run tests | [TESTING.md](TESTING.md) |
| `std.v1.*` modules | [STDLIB_STD_V1.md](STDLIB_STD_V1.md) |
| Diagnostics & errors | [ERROR_CODES.md](ERROR_CODES.md), [TROUBLESHOOTING.md](TROUBLESHOOTING.md) |
| Language & evolution | [LANGUAGE_SYNTAX.md](LANGUAGE_SYNTAX.md), [LANGUAGE_ROADMAP.md](LANGUAGE_ROADMAP.md) |
| Production readiness plan | [ADOPTION_ROADMAP.md](ADOPTION_ROADMAP.md) |
| **Engine internals** (compiler, VM, security) | [INTERNALS.md](INTERNALS.md) |

## Browsing locally

From the repository root:

```bash
pip install mkdocs
mkdocs serve
```

Then open the URL shown (usually `http://127.0.0.1:8000`).

## CLI

```bash
kern --version
kern docs          # print documentation pointers
kern build         # CMake build hints (toolchain is built with CMake)
kern test tests/coverage
```

See the main [README](https://github.com/entrenchedosx/kern/blob/main/README.md) for the full command list.
