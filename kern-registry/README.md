# kern-registry

Production-grade package registry system for the Kern programming language.

## What This Contains

- A static, GitHub-hostable registry in `registry/` that works with no backend.
- A future-ready Node.js server in `server/`.
- A production CLI in `cli/` used by Kern commands (`kern publish`, `kern install`, `kern search`, `kern info`).

## Architecture

```text
kern-registry/
  registry/
    registry.json
    packages/<name>/metadata.json
    packages/<name>/versions/<version>.json
  server/
    index.js
    routes/
      publish.js
      install.js
      search.js
  cli/
    entry.js
    publish.js
    install.js
    search.js
    info.js
    utils/
  schemas/
    package.schema.json
    registry.schema.json
    version-manifest.schema.json
```

## Static Registry Mode

The static registry is plain JSON content and can be hosted on GitHub Pages, raw GitHub, S3, or any static host.

- Root index: `registry/registry.json`
- Package metadata: `registry/packages/<pkg>/metadata.json`
- Version manifests: `registry/packages/<pkg>/versions/<version>.json`

## CLI Commands

- `kern-pkg publish [--dir <path>] [--bump patch|minor|major] [--public] [--dry-run]`
- `kern-pkg install [<pkg>]`
- `kern-pkg search <query>`
- `kern-pkg info <pkg> [range]`

## 60-second Quickstart

```bash
# 1) install deps
cd kern-registry
npm install

# 2) query local static registry
node ./cli/entry.js search example
node ./cli/entry.js info example-http

# 3) from a Kern project: install a package and lock it
# (creates/updates kern.json, kern.lock, .kern/package-paths.json)
node ../kern-registry/cli/entry.js install example-http@^1.0.0

# 4) publish your package into static registry checkout
KERN_REGISTRY_ROOT=/path/to/kern-registry node ./cli/entry.js publish --dir /path/to/pkg

# 5) publish publicly and auto-upload tarball with gh
KERN_REGISTRY_ROOT=/path/to/kern-registry KERN_REGISTRY_GH_REPO=owner/repo node ./cli/entry.js publish --dir /path/to/pkg --public

# 6) preview public publish without creating/uploading release assets
KERN_REGISTRY_ROOT=/path/to/kern-registry KERN_REGISTRY_GH_REPO=owner/repo node ./cli/entry.js publish --dir /path/to/pkg --public --dry-run
```

## Core Behavior

- Semver resolution (`exact`, `^`, `~`)
- Recursive dependency resolution
- Cycle detection
- Deduplication
- Lockfile generation (`kern.lock`, lockVersion 2)
- Integrity verification with SHA256
- Local cache in `~/.kern/cache`

## Security Model

- Every resolved artifact is validated against `integrity` (`sha256-...`) before extraction.
- Tampered artifacts fail installation.
- Optional package trust signal via `trusted: true` in version metadata.

## Environment Variables

- `KERN_REGISTRY_URL`: URL to `registry.json`
- `KERN_REGISTRY_ROOT`: writable local checkout path for static registry updates during publish
- `KERN_REGISTRY_CLI`: absolute path to CLI entry for Kern runtime delegation
- `GH_TOKEN`: optional token for release upload workflows
- `KERN_REGISTRY_GH_REPO`: required for `--public` auto-upload (`owner/repo`)

## Run

```bash
cd kern-registry
npm install
node ./cli/entry.js --help
node ./server/index.js
```

## Migration Path To Server

1. Start with static registry (`registry/`) as source of truth.
2. Serve static index from `server/` APIs for faster search/info.
3. Move write traffic (`publish`) to server endpoints.
4. Keep static JSON snapshots for auditability and CDN replication.
