# Phase 0.75 — Documentation audit (canonical tree)

## Canonical sources

- [`README.md`](../../README.md)
- [`docs/*.md`](..) (internals, language, testing)
- [`kargo/README.md`](../../kargo/README.md) — **legacy Node kargo**; primary user-facing Kargo story moved to [`docs/kargo-guide.md`](../kargo-guide.md).

## Issues found (pre-rebuild)

| Topic | Problem |
|-------|---------|
| Portable layout | Described `.kern/bin/kern.exe` and parent-walk — **obsolete** after `kern-*/` + `KERN_HOME`. |
| RELEASE.md | Same portable story — needs alignment with `kern-portable` + `kargo.exe`. |
| kargo/README.md | Node installer, `kargo.toml` — **superseded** for end users by native `kargo.json` CLI; kept as reference for migration. |
| Duplicates | `release-assets/**/docs` mirror `docs/` — can drift; release pipeline should regenerate or single-source. |

## New / updated docs (this pass)

- [`docs/getting-started.md`](../getting-started.md) — install, build, `KERN_HOME`, portable init.
- [`docs/language-guide.md`](../language-guide.md) — pointers to syntax + builtins.
- [`docs/kargo-guide.md`](../kargo-guide.md) — native `kargo` commands and manifest format.
- [`docs/examples.md`](../examples.md) — how to run examples and CI tiers.

## Consistency

After changes, **verify** against:

- `kern --help` / `kargo --version`
- [`.github/workflows/release.yml`](../../.github/workflows/release.yml) portable assets (`kern-core.exe`, `kern-runtime.zip`, `kern-portable.exe`, `kargo.exe`).
