# kargo

GitHub-first package manager for **Kern**: clone tagged repositories into `~/.kargo/packages`, write `kargo.lock` (with **commit SHA**), and merge **`.kern/package-paths.json`** so the VM can resolve imports.

- **No central registry** — packages live at `https://github.com/<owner>/<repo>`, versions are **git tags** `vMAJOR.MINOR.PATCH`.
- **Determinism** — lockfile stores `commit_sha` after checkout, plus **constraint provenance** (`resolved_constraints`, `resolved_from`, `resolved_version_range` = sorted conjunction of raw inputs). When possible, **`resolved_version_range_normalized`** gives a single **half-open** range (e.g. `>=1.3.0 <2.0.0`) for intersecting `^` / `~` / `>= … < …` constraints; it is omitted if not expressible that way or if it would not include the **resolved** version. **`kargo.lock` JSON** is written with **sorted package keys** and a **fixed field order** inside each entry for stable diffs. Remote tag lists are **sorted and deduped**; the work queue is **sorted by package id**. Wildcard `*` in a dependency **never overrides** stricter ranges—it adds no restriction.
- **Single version per repo** — every `owner/repo` appears at most once in the graph; the resolver picks the **highest** remote `v*` tag that satisfies **all** incoming constraints (npm-style `^` / `~`, comparators, exact).
- **PAT** — optional `~/.kargo/config.json` (`kargo login --token …`) for private repos and GitHub API (search / releases).

## CLI

```bash
kargo install owner/repo@v1.0.0
kargo install owner/repo@^1.2.0
kargo remove owner/repo
kargo update              # resolve all [dependencies] in kargo.toml (ranges, one version per repo)
kargo update owner/repo@v2.0.0
kargo list
kargo search query
kargo publish --tag v1.0.0
kargo login --token ghp_...
kargo build
kargo run
```

## `kargo.toml` dependencies

Use **`owner/repo` as the key** and a **semver range** as the value (remote tags must look like `vMAJOR.MINOR.PATCH`):

```toml
[dependencies]
acme/widgets = "^1.2.0"
other/lib = ">=2.0.0 <3.0.0"
# or alias key + full spec in the value:
widgets = "acme/widgets@^1.0.0"
```

`kargo update` (no arguments) re-resolves the whole graph from this file and refreshes `kargo.lock`.

### Resolver options

```toml
[kargo]
allow_prerelease = false   # default true — if no stable tag matches, kargo may pick a prerelease (semver rules apply; e.g. ^2.0.0 may still exclude 2.0.0-alpha)
resolution_mode = "locked"  # default "latest" — locked: versions come only from kargo.lock (no remote version choice); use for CI after a fresh lock. latest: resolve from GitHub tags (normal dev flow).
```

### Debug & UX

```bash
kargo install owner/repo@^1.0.0 --resolve-debug
kargo install owner/repo@^1.0.0 --verbose   # prints a short “why this version” for the root package
```

`--resolve-debug` prints a JSON **iteration trace** to **stderr**: `why_selected` (bullets + merged constraints), input constraints, sampled remote versions, stable-phase **rejection samples** (first failing constraint + reason), satisfying candidates, **selection_reason**, `resolution_mode`, plus an ASCII **dependency tree**.

Resolution failures highlight a **minimal conflicting subset** of constraints when it is smaller than the full edge list (irreducible unsat core), then list all edges for context.

### Graph

```bash
kargo graph [--project <dir>]
kargo graph --json
```

Walks **`[dependencies]` in kargo.toml** and each package’s **`kargo.toml` on disk** (only packages present in **`kargo.lock`**). ASCII tree from `__project__`, or JSON for tooling. With no project dependencies, prints a flat list from the lockfile.

## Imports (after `kargo install`)

- `import "package-name"` — from `name` in the dependency’s `kargo.toml`
- `import "owner/repo"`
- `import "github.com/owner/repo"`

## Install

**With Kern (recommended if you use the compiler):** from the Kern repo root, run `install.sh` or `install.ps1`. That copies this tree to `<prefix>/lib/kargo`, runs `npm install --omit=dev`, and installs a `kargo` launcher next to `kern`. The installer requires **Node ≥ 18.17**, **npm on `PATH` and runnable** (`npm --version`), and **fails the whole install** if Kargo’s `npm install` fails (no silent partial install).

**Kargo only:** from a checkout, run `scripts/install-kargo.sh` (POSIX) or `scripts/install-kargo.ps1` (Windows). Defaults: user-level paths (`~/.local` or `%LOCALAPPDATA%\Programs\kargo`), launcher on `PATH`. If an install is already present (app dir and/or our launcher), you get a **menu**: upgrade, reinstall, uninstall, or cancel. Uninstall lists paths and asks for confirmation; optionally removes `~/.kargo`. **Non-interactive shells** default to **upgrade** when an install exists; override with **`KARGO_INSTALL_MODE`** (`upgrade` \| `reinstall` \| `uninstall` \| `cancel`) on POSIX, or **`-InstallMode`** on PowerShell. `FORCE=1` / `-Force` still skips prompts where applicable.

**GitHub Releases (with Kern `v*` tags):** CI packages `kargo-<tag>.tar.gz` (Node app + production `node_modules`, tests stripped), `kargo-SHA256SUMS` (`sha256sum -c` compatible), and `kargo-release.json` (`git_tag`, `package_version`, `tarball`, `sha256`). Built by `kargo/scripts/package-release.sh` from the root **Release** workflow.

**Install from a release (POSIX):** after a release includes the Kargo assets above, from a checkout run:

```bash
export KARGO_RELEASE_REPO=yourfork/kern   # default: entrenchedosx/kern
./kargo/scripts/install-from-release.sh latest
# or: ./kargo/scripts/install-from-release.sh v1.0.0
```

The script resolves assets via the GitHub API (`gh-release-resolve.mjs`), **downloads `kargo-SHA256SUMS` and the tarball, verifies with `sha256sum -c` / `shasum -a 256 -c` before extract**, validates the tarball with **`tar -tzf`** before touching your install prefix, optionally cross-checks `kargo-release.json`, then installs under `~/.local` like `install-kargo.sh`. Requires **curl**, **tar**, and **Node ≥ 18.17** (no `npm` at install time). Downloads use **curl retries and timeouts**; release asset URLs must be **https** to allowed GitHub hosts (enforced in `gh-release-resolve.mjs`).

**Installer ergonomics:** progress is printed to **stderr** in numbered steps (`1/5` … `5/5`). Set **`KARGO_INSTALL_QUIET=1`** for errors-only output. Append extra curl flags with **`KARGO_CURL_EXTRA`** (e.g. `"--ipv4"` or proxy options). Local **`install-kargo.sh`** supports the same quiet flag.

**One-liner (no clone):** `bootstrap-kargo-release.sh` downloads `install-from-release.sh`, `gh-release-resolve.mjs`, and `kargo-install-common.sh` from `raw.githubusercontent.com`, then runs the same verified install (including lifecycle menu when appropriate). Use `sh -s --` so arguments reach the installer.

| Bootstrap ref | Trust | Typical use |
|---------------|-------|----------------|
| `main` (default) | Lower — branch moves | Quick try; script prints a **WARNING** |
| Release tag `v*` | High — matches a release | **Recommended** for real installs |
| Commit SHA (7–40 hex) | Highest — immutable | Audited / air-gapped workflows |

**Pinned (recommended)** — `KARGO_BOOTSTRAP_REF` must match the ref in the URL (the script cannot infer the URL when piped from `curl`):

```bash
TAG=v1.0.0   # Kern release tag that contains these scripts
curl -fsSL "https://raw.githubusercontent.com/entrenchedosx/kern/${TAG}/kargo/scripts/bootstrap-kargo-release.sh" \
  | KARGO_BOOTSTRAP_REF="$TAG" sh -s -- latest
```

**Convenience (mutable):**

```bash
curl -fsSL https://raw.githubusercontent.com/entrenchedosx/kern/main/kargo/scripts/bootstrap-kargo-release.sh | sh -s -- latest
```

Set `KARGO_BOOTSTRAP_NO_WARN=1` to silence the mutable-ref warning (e.g. CI).

**Strict ref alignment:** `KARGO_STRICT_BOOTSTRAP=1` makes `install-from-release.sh` require an explicit release tag (not `latest`) and **`KARGO_BOOTSTRAP_REF` must exactly equal that tag** (e.g. both `v1.0.0`). The bootstrap script **exports** `KARGO_BOOTSTRAP_REF` to the child so the default `main` is visible. **Interactive “Upgrade” sets the release request to `latest`, which conflicts with strict mode**—use reinstall with an explicit tag, or turn strict off. Commit-SHA bootstrap refs will not match a `v*` release tag—skip strict or use tags only.

```bash
TAG=v1.0.0
curl -fsSL "https://raw.githubusercontent.com/entrenchedosx/kern/${TAG}/kargo/scripts/bootstrap-kargo-release.sh" \
  | KARGO_BOOTSTRAP_REF="$TAG" KARGO_STRICT_BOOTSTRAP=1 sh -s -- "$TAG"
```

Further out: signed checksums, `kargo self-update`, optional native binary (no Node).

## Requirements

- **git** on `PATH`
- **Node.js** ≥ 18.17 and **npm** on `PATH` (installers verify `npm --version` before `npm install`)
