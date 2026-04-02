# Kern — Release and Installation Guide

This guide is for **users** who want to run Kern (run scripts, use the REPL) and for **maintainers** preparing a public release. **Editors** are documented in the **Kern-IDE** tree ([Kern-IDE/README.md](Kern-IDE/README.md)).

---

## For users: running Kern

### Option A: Pre-built binaries (when available)

1. Download the latest release for your OS (e.g. `kern-1.0.0-win64.zip`).
2. Unzip to a folder (e.g. `C:\Program Files\Kern` or `~/bin`).
3. Add that folder to your PATH (optional but recommended).
4. Run:
   - `kern script.kn` — run a script
   - `kern` — start the REPL
   - `kern --version` — show version
   - `kern --help` — show all options

The Windows portable build is intended to be self-contained: no extra runtime installs are required. Graphics support (when included) is bundled as part of the distribution.

### Option B: Build from source

**Requirements:** C++17 compiler (e.g. MSVC 2022, GCC 7+, Clang 5+), CMake 3.14+.

**Without graphics (smallest build):**

```bash
git clone <repo-url> kern
cd kern
cmake -B build -DKERN_BUILD_GAME=OFF
cmake --build build --config Release
```

Executables: `build/Release/kern.exe` (Windows) or `build/kern` (Unix).

**With graphics (g2d and game modules):**

1. Install Raylib (build-time only), e.g. with vcpkg:
   ```bash
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg && ./bootstrap-vcpkg.bat   # or bootstrap-vcpkg.sh on Unix
   ./vcpkg install raylib:x64-windows   # or raylib for Linux/Mac
   ```
2. Configure and build:
   ```bash
   cd kern
   cmake -B build -DKERN_BUILD_GAME=ON -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake
   cmake --build build --config Release
   ```

Then run `build/Release/kern.exe examples/graphics/graphics_demo.kn` (or your script).

### Option C: Kern-IDE (editors)

Use the **[Kern-IDE](Kern-IDE/README.md)** directory: Tk desktop IDE, Qt editor, and VS Code extension. They call the **`kern`** binary you install from this repository (or from a release archive).

---

## For maintainers: preparing a release

1. **Version**
   - Set the version in the root `VERSION` file (e.g. `1.0.0`). This is used by `kern --version` and the `kern_version()` builtin.

2. **Changelog**
   - Update `CHANGELOG.md` with the release date and any last-minute notes.

3. **Build**
   - Build Release (and optionally Debug) for each target platform.
   - With Raylib: build with `KERN_BUILD_GAME=ON` and the vcpkg (or system) Raylib so the published binaries include g2d/game.

4. **Test**
   - Run the test suite (see `docs/TESTING.md`).
   - Smoke-test: `kern --version`, `kern --help`, `kern examples/basic/01_hello_world.kn`, `kern` (REPL: exit), and if built with game: `kern examples/graphics/graphics_demo.kn`.

5. **Package**
   - Create archives (e.g. `kern-1.0.0-win64.zip`, `kern-1.0.0-linux64.tar.gz`) containing:
     - `kern` (or `kern.exe`), `kern_repl` (or `kern_repl.exe`), and optionally `kern_game`.
     - Optional: `examples/`, `docs/`, `LICENSE`, `README.md`.
   - Optionally coordinate a **Kern-IDE** release (separate repository) for editor binaries.

6. **Publish**
   - Create a Git tag (e.g. `v1.0.0`).
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

- [ ] `VERSION` file set.
- [ ] `kern --version` and `kern_version()` match.
- [ ] `kern --help` lists all options.
- [ ] Core examples run: `examples/basic/01_hello_world.kn`, `examples/advanced/00_full_feature_smoke.kn`.
- [ ] With Raylib: `examples/graphics/graphics_demo.kn` (or another graphics sample) runs.
- [ ] REPL starts and exits with `exit`/`quit`.
- [ ] `kern --check file.kn` exits 0 for valid script.
- [ ] Docs: README, RELEASE.md, TROUBLESHOOTING up to date.
- [ ] CHANGELOG updated for the release.

### one-command go/no-go gate

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\release_go_no_go.ps1
```

This verifies shareable artifacts, `kern doctor`, IDE startup smoke, parser/VM regression suites, examples, and full coverage/regression (unless `-SkipCoverage` is used).
