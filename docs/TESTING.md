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
```

Generates large `??` chains, long unary prefixes, and an oversized source file to verify the compiler fails safely (no native stack blowups, bounded source size).

