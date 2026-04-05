# kern-registry

Registry service and CLI for Kern packages (API-first, Python-like workflow).

## What This Contains

- `server/`: hosted package registry API (publish/search/info/download).
- `cli/`: package manager used by `kern install|publish|search|info`.
- `registry/`: optional static mirror/export (compatibility and snapshots).

## API-First Flow

- `kern publish` uploads package artifacts + metadata to a registry API.
- `kern install` resolves versions, downloads artifacts, verifies SHA256 integrity, writes `kern.lock`.
- `kern search` / `kern info` read from the same registry API index.

This is intentionally similar to `pip`/PyPI behavior: package publishing is not tied to GitHub release assets.

## API Endpoints (v1)

- `POST /api/v1/packages` (auth optional, enabled by API keys)
- `GET /api/v1/simple`
- `GET /api/v1/simple/:name`
- `GET /api/v1/packages/:name`
- `GET /api/v1/packages/:name/:version`
- `GET /api/v1/files/:name/:version/:filename`
- `GET /health`

## CLI Commands

- `kern-pkg publish [--dir <path>] [--bump patch|minor|major] [--api <url>] [--dry-run]`
- `kern-pkg install [<pkg>[@range]] [--project <path>] [--update] [--api <url>]`
- `kern-pkg search <query> [--api <url>]`
- `kern-pkg info <pkg> [range] [--api <url>]`

## Quickstart

```bash
# 1) Start registry server
cd kern-registry
npm install
PORT=4873 node ./server/index.js

# 2) Publish a package
KERN_REGISTRY_API_URL=http://127.0.0.1:4873 \
node ./cli/entry.js publish --dir /path/to/package

# 3) Install from a Kern project
KERN_REGISTRY_API_URL=http://127.0.0.1:4873 \
node ../kern-registry/cli/entry.js install package-name@^1.0.0
```

## Security Model

- Artifacts are hashed on publish (`sha256-...`) and verified on install.
- Publish endpoint can require API keys.
- Installer supports offline cache reuse in `~/.kern/cache`.

## Environment Variables

- `KERN_REGISTRY_API_URL`: registry API base URL (default `http://127.0.0.1:4873`)
- `KERN_REGISTRY_API_KEY`: publish auth token for `POST /api/v1/packages`
- `KERN_REGISTRY_API_KEYS`: comma-separated server-side key list
- `KERN_REGISTRY_STORAGE_ROOT`: server artifact storage directory
- `KERN_REGISTRY_MAX_BODY_BYTES`: max publish payload size
- `KERN_REGISTRY_URL`: legacy static index URL (compatibility fallback)
- `KERN_REGISTRY_ROOT`: optional static mirror output path during publish

## Compatibility

- Existing lockfiles with `file://`, GitHub, or HTTP artifact URLs still install.
- API mode is preferred; static index mode remains as fallback/mirror.
