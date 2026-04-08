# Kern — Release and Installation Guide

This guide is for **users** who want to run Kern (run scripts, use the REPL) and for **maintainers** preparing a public release. **Editors** are documented in the **Kern-IDE** tree ([Kern-IDE/README.md](Kern-IDE/README.md)).

---

## For users: running Kern

### Option A: Pre-built binaries (when available)

1. Download the latest release for your OS (e.g. `kern-windows-x64-v1.0.7.zip`, `kern-linux-x64-v1.0.7.tar.gz`, or `kern-macos-v1.0.7.tar.gz` from the GitHub **Releases** page).
2. Unzip to a folder (e.g. `C:\Program Files\Kern` or `~/bin`).
3. Add that folder to your PATH (optional but recommended).
4. Run:
   - `kern script.kn` — run a script
   - `kern` — start the REPL
   - `kern --version` — show version
   - `kern --help` — show all options

The Windows portable build is intended to be self-contained: no extra runtime installs are required. Graphics support (when included) is bundled as part of the distribution.

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
   - Set the version in the root `KERN_VERSION.txt` file (e.g. `1.0.7`). This is used by `kern --version` and the `kern_version()` builtin.

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
   - Create archives (e.g. `kern-windows-x64-v1.0.7.zip`, `kern-linux-x64-v1.0.7.tar.gz`, `kern-macos-v1.0.7.tar.gz`) containing:
     - `kern`, `kernc`, `kern-scan`, `kern_game`, `kern_repl`, `kern_lsp`, `kern_contract_humanize` (with OS-specific `.exe` on Windows).
     - Optional: `examples/`, `docs/`, `LICENSE`, `README.md`.
   - Optionally coordinate a **Kern-IDE** release (separate repository) for editor binaries.

6. **Publish**
   - Create a Git tag (e.g. `v1.0.7`).
   - Attach the package(s) to the GitHub (or other) release and copy the relevant part of `CHANGELOG.md` into the release notes.

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
