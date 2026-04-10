# Getting started

## What you get

- **`kern`** — run `.kn` scripts, REPL, checks, tests.
- **`kargo`** — native package manager (`kargo.json` / `kargo.lock` in the project dir; packages live under **`<KernRoot>/packages/`**).
- **`kern-portable`** (Windows) — downloads a release and creates **`kern-NN/`** (unique two-digit folder, e.g. `kern-42`) with `kern.exe`, `kargo.exe`, `lib/`, `runtime/`, `packages/`, `cache/`, `config/` + **`config/env.json`** (no `bin/` subfolder).

## Environment resolution

The VM resolves the toolchain layout in this order:

1. **`kern --root <path>`** — must be a **strict** root: `kern` + `kargo` binaries, `lib/`, `lib/kern/`, `runtime/`.
2. **`KERN_HOME`** — same **strict** layout (no “repo-only” roots here).
3. **Directory containing the running executable** — same **strict** check when that folder is the portable tree.
4. **`config/env.json`** beside the executable (`"root": "..."`) — accepted only if that path passes the **strict** check.
5. **Debug builds only:** walk parents to find **`lib/kern/`** (monorepo / CMake); **Release** builds do not walk.
6. **Cache file** — `%APPDATA%\kern\root.txt` or `~/.config/kern/root.txt`: must be **strict**; otherwise it is **deleted**.

**Developing from CMake:** use a **Debug** `kern`/`kargo` to pick up the repo via the parent walk, or point **`KERN_HOME`** at a full **`kern-NN/`** portable install. **Release** binaries expect **`KERN_HOME`**, **`--root`**, or running from inside **`kern-NN/`**.

`kern-portable` always sets **`KERN_HOME`** for child processes. Package imports use **`config/package-paths.json`** under the resolved Kern root (paths inside it are relative to that root).

## Windows: portable install

From a project directory (requires a GitHub release that ships `kern-core.exe`, `kern-runtime.zip`, `kern-portable.exe`, and **`kargo.exe`** with matching `kern-SHA256SUMS`):

```powershell
.\kern-portable.exe init --latest
```

Activate (use the `kern-NN` folder the installer printed):

```powershell
. .\kern-42\Scripts\Activate.ps1
kern --version
kargo.exe --version
```

## Build from source (CMake)

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=...\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build build --config Release --target kern kargo
.\build\Release\kern.exe examples\basic\01_hello_world.kn
```

See [GETTING_STARTED.md](GETTING_STARTED.md) for vcpkg/Raylib details.

## Next steps

- [Language guide](language-guide.md) — syntax overview and links.
- [Kargo guide](kargo-guide.md) — manifests and commands.
- [Examples](examples.md) — running the example tree and CI tiers.
