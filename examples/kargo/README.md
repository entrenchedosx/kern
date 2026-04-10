# Kargo example (native `kargo.exe`)

Native Kargo uses **`kargo.json`** in the **current directory** and installs into **`./packages/`**, updating **`./config/package-paths.json`** for `import "owner/repo"` style keys.

## Commands

```bash
kargo install user/repo@v1.0.0
kargo remove user/repo
kargo update
kargo list
```

Requires **`git`** on `PATH` for clone.

## Sample manifest

See [`kargo.json`](kargo.json) (empty dependencies — add packages with `kargo install`).
