# Kern — Release and Installation Guide

This guide is for **users** who want to run Kern (run scripts, use the REPL) and for **maintainers** preparing a public release. **Editors** are documented in the **Kern-IDE** tree ([Kern-IDE/README.md](Kern-IDE/README.md)).

---

## For users: running Kern

### Option A: Pre-built binaries (when available)

1. Download the latest release for your OS (artifact names follow `kern-windows-x64-vX.Y.Z.zip`, `kern-linux-x64-vX.Y.Z.tar.gz`, `kern-macos-arm64-vX.Y.Z.tar.gz` / `kern-macos-x64-vX.Y.Z.tar.gz` — replace `X.Y.Z` with the release semver from the **Releases** page).
2. Unzip to a folder (e.g. `C:\Program Files\Kern` or `~/bin`).
3. Add that folder to your PATH (optional but recommended).
4. Run:
   - `kern script.kn` — run a script
   - `kern` — start the REPL
   - `kern --version` — show version
   - `kern --help` — show all options

The Windows portable build is intended to be self-contained: no extra runtime installs are required. Graphics support (when included) is bundled as part of the distribution.

### Option A2: Project-local `kern-<version>/` (Windows, no global install)

GitHub Releases ship **portable-env** artifacts built by CI:

| Asset | Role |
|--------|------|
| `kern-portable.exe` | Bootstrapper: `kern-portable init` downloads `kern-core.exe`, `kern-runtime.zip`, and **`kargo.exe`**, lays out **`kern-<tag>/`** with **`kern.exe`** and **`kargo.exe` at the folder root** (no `bin/`), plus `lib/`, `runtime/`, `packages/`, `cache/`, `config/`. **`kern-portable <args>`** runs `kern.exe` and sets **`KERN_HOME`** for the child. |
| `kern-core.exe` | Copy of the full `kern` compiler (same as in the big zip). |
| `kern-runtime.zip` | `kern-registry` + `lib/kern` trees extracted under `runtime/kern-registry/` and `lib/kern/`. |

**Note:** The bootstrapper’s `init` is **not** the same as the compiler’s `kern init` (which scaffolds `kern.json`). After `kern-portable init`, use `kern-<tag>\kern.exe init` if you need the project scaffold. On Windows there is no Unix `exec`; the bootstrapper **spawns** the local `kern.exe` and exits with its status.

#### Portable Kern environments (Windows)

Kern supports fully isolated, per-project environments under **`kern-<version>/`**.

**Initialize**

```text
kern-portable init
```

Useful options: `--latest`, `--nightly`, `--version <tag>`, `--force`, `--project <dir>`.

**Upgrade** (keeps `packages/` and `lock.toml` when present)

```text
kern-portable upgrade
```

**Layout**

After init, `kern-<version>/` contains `kern.exe`, `kargo.exe`, `lib/`, `runtime/`, `packages/`, `cache/`, `config/`, and optional `Scripts/` activation helpers. Project-local **native Kargo** uses `kargo.json` in the working directory.

**Security**

Downloads are verified with SHA256. Official releases **must** ship **`kern-SHA256SUMS`** listing `kern-core.exe`, `kern-runtime.zip`, `kern-portable.exe`, and **`kargo.exe`** (merged from `kern-SHA256.partial-portable` where used).

**Portability**

Projects are portable: zip → move → unzip → run. No machine-wide install is required on the target system.

**Reliability**

Installs use temp staging and rollback on failure; upgrades can preserve your package tree and lockfile.

**Delegation**

The bootstrapper resolves `kern.exe` via **`KERN_HOME`**, a **unique** `./kern-*/kern.exe` in the current directory, or the project’s single installed `kern-*` folder. It does **not** walk parents looking for `.kern/bin`.

**Naming**

- **`kern-portable.exe`** (bootstrapper) vs **`kern-<tag>/kern.exe`** (full compiler copied from **`kern-core.exe`**). **`kern-core.exe`** is the release artifact name only.

**Requirements for `kern-portable init`:** network access to GitHub. The release must include **`kargo.exe`** beside the other portable assets.

**First run (Windows):** The first time you start `kern.exe` for your user account, Kern self-registers under **HKCU** (no admin): **`.kn`** → **KernFile**, type name **“Kern Script”**, **Content Type** `text/x-kern`, **DefaultIcon** from **`<exe_dir>\\kern_logo.ico`** when present else **`kern.exe,0`**, **open** command **`\"path\\to\\kern.exe\" \"%1\"`**, optional context menu **Edit with Kern**, then **`SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, …)`** (plus a flush notify). It writes **`%APPDATA%\\kern\\setup_done.flag`** so later launches only stat that file and skip. Run **`kern --repair-association`** to re-apply if something breaks. Set **`KERN_SKIP_FILE_ASSOCIATION`** to any value (e.g. in CI) to disable auto setup. Set **`KERN_RESTART_EXPLORER_AFTER_ASSOC`** to force **Explorer restart** after association (brief desktop blink; last resort for stubborn icon cache). Errors go to **`%APPDATA%\\kern\\setup.log`**; Kern keeps running.

### Option B: Build from source

**Requirements:** C++17 compiler (e.g. MSVC 2022, GCC 7+, Clang 5+), CMake 3.14+.

**Default (full Kern — g2d, g3d, game via Raylib):** `KERN_BUILD_GAME` defaults to **ON**. CMake **fails** if Raylib cannot be resolved (vcpkg, system install, or `KERN_AUTO_FETCH_RAYLIB` FetchContent). Confirm with `kern --version` → `graphics: g2d+g3d+game (Raylib linked)`.

1. **Windows (recommended):** use the repo’s `vcpkg.json` + static triplet, e.g.  
   `cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=tools/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static`  
   or run `.\build.ps1` (installs `raylib:x64-windows-static` via local vcpkg when present).
2. **Linux:** install X11/OpenGL/ALSA dev packages (see `.github/workflows/linux-kern.yml`), then `cmake -B build -DCMAKE_BUILD_TYPE=Release` and build.
3. **macOS:** Xcode Command Line Tools are usually enough; CMake may FetchContent Raylib on first configure.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release   # add -G / toolchain flags on Windows as above
cmake --build build --config Release
```

**Headless only (no graphics — opt-in):**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DKERN_BUILD_GAME=OFF
cmake --build build --config Release
```

Executables: `build/Release/kern.exe` (Windows) or `build/kern` (Unix), plus `kern-scan`, `kern_repl`, `kern_game`, `kern_lsp`, etc., when those targets are built.

### Option C: Kern-IDE (editors)

Use the **[Kern-IDE](Kern-IDE/README.md)** directory: Tk desktop IDE, Qt editor, and VS Code extension. They call the **`kern`** binary you install from this repository (or from a release archive).

---

## For maintainers: preparing a release

1. **Version**
   - Set the version in the root `KERN_VERSION.txt` file (semver `X.Y.Z`). This is used by `kern --version` and the `kern_version()` builtin. Match **`kern-bootstrap/Cargo.toml`**, **`kargo/package.json`**, **`kern-registry/package.json`**, and root **`kern.json`** `version` to the same value.

2. **Changelog**
   - Update `CHANGELOG.md` with the release date and any last-minute notes.
   - Follow [docs/RELEASE_CHECKLIST.md](docs/RELEASE_CHECKLIST.md) for a concise maintainer checklist.

3. **Build**
   - Build Release (and optionally Debug) for each target platform.
   - **Release artifacts must be graphics-capable:** default `KERN_BUILD_GAME=ON` requires Raylib; CI checks `kern --version` for `graphics: g2d+g3d+game (Raylib linked)`. Use `-DKERN_BUILD_GAME=OFF` only for intentional headless builds.

4. **Test**
   - Run the test suite (see `docs/TESTING.md`).
   - Smoke-test: `kern --version`, `kern --help`, `kern examples/basic/01_hello_world.kn`, `kern` (REPL: exit), and if built with game: `kern examples/graphics/graphics_demo.kn`.

5. **Package**
   - **Windows (bootstrapper-compatible zip from your local Release build):** after `cmake --build build --config Release --target kern kernc kern-scan kern_game kern_repl kern_lsp kern_contract_humanize`, run:
     - `pwsh -NoProfile -File scripts/package-windows-kern-github-release-zip.ps1`
     - Produces `kern-windows-x64-vX.Y.Z.zip` and `kern-SHA256.partial-windows` at the repo root, matching [`.github/workflows/release.yml`](.github/workflows/release.yml) layout (including embedded `kern-registry` with `npm ci`).
   - **Upload that zip to an existing GitHub release** (same asset name the bootstrapper downloads): install [GitHub CLI](https://cli.github.com/), `gh auth login`, then:
     - `pwsh -NoProfile -File scripts/upload-kern-windows-zip-to-github-release.ps1 -Tag vX.Y.Z`
     - The tag’s release must already exist. For a **full** multi-platform release with merged checksums and `kern-bootstrap-*` binaries, prefer pushing tag `vX.Y.Z` and letting CI attach all artifacts.
   - Other platforms: create archives (e.g. `kern-linux-x64-vX.Y.Z.tar.gz`, `kern-macos-arm64-vX.Y.Z.tar.gz`, `kern-macos-x64-vX.Y.Z.tar.gz`) per the workflow, or rely on CI.
   - Each archive should include: `kern`, `kernc`, `kern-scan`, `kern_game`, `kern_repl`, `kern_lsp`, `kern_contract_humanize` (`.exe` on Windows), plus `LICENSE` / `README.md` / `KERN_VERSION.txt` as in CI.
   - Optionally coordinate a **Kern-IDE** release (separate repository) for editor binaries.

6. **Publish**
   - Create a Git tag `vX.Y.Z` matching `KERN_VERSION.txt` and push it so **Release** CI builds and attaches all assets, **or** attach packages manually / via `gh release upload` as above.
   - Copy the relevant part of `CHANGELOG.md` into the release notes.

### shareable drops used in this repository

- `shareable-ide/compiler/` — toolchain (`kern.exe`, `kern_repl.exe`, `kernc.exe`, `lib/`) for portable drops (IDE packaging lives in **Kern-IDE**).
- `shareable-kern-to-exe/`
  - `kern-to-exe.bat`
  - `kern.exe`
  - `kernc.exe`
  - `kern_to_exe/` python package

---

## Production and advanced use

- **Exit code:** Scripts can call `exit_code(n)` (0–255) to stop and set the process exit code; useful for CLI tools.
- **Step limit (optional):** The VM supports `setStepLimit(n)` to cap execution steps (0 = unlimited). Builders can expose this via an env var or `--max-steps` if needed to guard against infinite loops in untrusted scripts.
- **Errors:** Compile and runtime errors report line/column, snippets, and hints; set `NO_COLOR=1` for plain text.
- **REPL:** Commands `help`, `clear`, `exit`/`quit`; same modules and builtins as batch runs.

## Production checklist (quick)

- [ ] `KERN_VERSION.txt` set.
- [ ] `kern --version` and `kern_version()` match.
- [ ] `kern --help` lists all options.
- [ ] Core examples run: `examples/basic/01_hello_world.kn`, `examples/advanced/00_full_feature_smoke.kn`.
- [ ] With Raylib: `examples/graphics/graphics_demo.kn` (or another graphics sample) runs.
- [ ] REPL starts and exits with `exit`/`quit`.
- [ ] `kern --check file.kn` exits 0 for valid script.
- [ ] `kern --scan --registry-only` exits 0.
- [ ] Docs: README, RELEASE.md, TROUBLESHOOTING up to date.
- [ ] CHANGELOG updated for the release.

### one-command go/no-go gate

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\release_go_no_go.ps1
```

This verifies shareable artifacts, `kern doctor`, IDE startup smoke, parser/VM regression suites, examples, and full coverage/regression (unless `-SkipCoverage` is used).
