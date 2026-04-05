# kern-github-registry

Standalone repo that uses GitHub as package registry + auth server.

## Model

- Packages are published as GitHub release assets.
- Registry metadata is stored in repo files:
  - `registry/index.json`
  - `registry/packages/<name>.json`
- Auth uses GitHub PAT (`ghp_...`), saved to `~/.kern/config.json`.

## Security Defaults

- Token is validated against GitHub during `login` (and must include `repo` scope).
- Package name/version are validated before publish.
- Tarballs are integrity-pinned with SHA256 and verified on install.
- Installs only accept HTTPS artifact URLs from GitHub hosts.
- Tar extraction rejects unsafe paths and symlinks.
- Package size hard limit: 200MB.

## Setup

```bash
cd kern-github-registry
npm install
```

## Login

```bash
node ./cli/entry.js login --token ghp_xxx --repo owner/your-registry-repo
node ./cli/entry.js login --show
# optional if offline: skip token validation
# node ./cli/entry.js login --token ghp_xxx --repo owner/repo --no-validate
```

## Publish

```bash
node ./cli/entry.js publish --dir /path/to/package
```

This will:
- create/find release tag `<pkg>-v<version>`
- upload `<pkg>-<version>.tgz`
- update `registry/index.json` and `registry/packages/<pkg>.json` in your registry repo

## Install / Search / Info

```bash
node ./cli/entry.js search sec
node ./cli/entry.js info sec.crypto ^1.0.0
node ./cli/entry.js install sec.crypto@^1.0.0 --project /path/to/project
```

Install writes:
- `kern.json` dependency entries
- `kern.lock` (v2)
- `.kern/package-paths.json`
- `.kern/packages/<pkg>/<version>/...`

## Notes

- The target repo should be writable by your token (`repo` scope).
- Public repos are easiest for install/download.
- This is intentionally serverless: GitHub API handles auth, storage, and access control.

## Integrate With `kern`

Set these env vars so `kern install/publish/search/info` can use this backend:

```powershell
$env:KERN_PACKAGE_BACKEND = "github"
$env:KERN_GITHUB_REGISTRY_CLI = "D:\simple_programming_language\kern-github-registry\cli\entry.js"
```
