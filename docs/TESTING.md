# Testing

## Full example sweep

From repo root (recurses under `examples/` — `basic/`, `graphics/`, etc.):

```powershell
.\run_all_tests.ps1 -Exe ".\build\Release\kern.exe" -Examples ".\examples"
```

## Kern compiler-focused tests

From repo root (uses `build\Release\kernc.exe` by default):

```powershell
.\tests\kernc\run_kernc_tests.ps1 -Kernc ".\build\Release\kernc.exe" -Root "."
```

`-Splc` is accepted as an alias for `-Kernc` (legacy name).

## Static import graph (`kern graph`)

From repo root (prints resolved `.kn` modules and edges; add `--json` for tooling):

```powershell
.\build\Release\kern.exe graph tests\compile_pipeline_fixture\main.kn
.\build\Release\kern.exe graph --json examples\basic\modules.kn
```

## Lockfile + `kern test` / `--check`

If the current working directory has **`kern.json`**, **`kern test`** and **`kern --check`** require **`kern.lock`** to list the same dependency names as the manifest (same rules as **`kern verify`**). Use **`--skip-lock-verify`** only for local debugging.

## Version output

**`kern --version`** prints the release string, a **`bytecode-schema:`** integer (`kBytecodeSchemaVersion` in `src/vm/bytecode.hpp`), and **`build:`** (git short hash) when the binary was configured from a **`.git`** checkout.

## Watch mode

**`kern --watch script.kn`** re-runs the script when the file changes. Add **`--check`** for compile/semantic only (no VM), and optional **`--strict-types`** (same as **`kern --check`**).

**`kern watch test [options] [dir]`** uses the same flags as **`kern test`** (`--grep`, `--fail-fast`, etc.) and re-runs the whole suite whenever any matching **`.kn`** file’s modification time changes under **`dir`** (default **`tests/coverage`**).

## Capability profiles and permission groups

- List/show/apply capability profiles:
  - `kern capability profile list`
  - `kern capability profile show dev`
  - `kern capability profile apply ci`
- Grouped grants supported by CLI and `require(...)` include:
  - `fs.readonly`, `fs.readwrite`, `net.client`, `proc.control`, `env.manage`

## System-access wave 30 smoke

- Source/API surface + unsafe-execution smoke:
  - `.\build\Release\kern.exe tests\coverage\test_system_access_30_surface.kn`
- Static check:
  - `.\build\Release\kern.exe --check tests\coverage\test_system_access_30_surface.kn`

## Useful options

- `-TimeoutSeconds 60` to increase per-test timeout
- `-Skip @("file1.kn","file2.kn")` to temporarily skip specific scripts

## CI/release sanity

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\release_go_no_go.ps1
```

## Adversarial / stress (parser + lexer limits)

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\stress\run_stress_suite.ps1
powershell -ExecutionPolicy Bypass -File .\tests\stress\run_stress_suite.ps1 -Aggressive
```

Checks UTF-8 BOM acceptance and UTF-16 BOM rejection, generates large `??` chains, long unary prefixes, and an oversized source file to verify the compiler fails safely (no native stack blowups, bounded source size). The default run includes a VM max-depth overflow script; **`-Aggressive`** uses larger generators, confirms the same script fails under **`--release`**, and runs the other stress `.kn` files to completion.

