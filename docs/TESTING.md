# Testing

## Full example sweep

From repo root:

```powershell
.\run_all_tests.ps1 -Exe ".\build\Release\kern.exe" -Examples ".\examples"
```

## Kern compiler-focused tests

```powershell
.\tests\kern\run_kernc_tests.ps1 -Splc ".\build\Release\kern.exe" -Root "."
```

## Useful options

- `-TimeoutSeconds 60` to increase per-test timeout
- `-Skip @("file1.kn","file2.kn")` to temporarily skip specific scripts

## CI/release sanity

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\release_go_no_go.ps1
```

