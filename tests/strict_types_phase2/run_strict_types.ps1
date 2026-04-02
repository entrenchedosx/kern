# Phase 2: verify strict-types pass/fail expectations (run from repo root).
$ErrorActionPreference = "Stop"
$kern = Join-Path $PSScriptRoot "..\..\build\Release\kern.exe"
if (-not (Test-Path $kern)) {
    Write-Error "Build kern first: $kern"
}
& $kern --check --strict-types (Join-Path $PSScriptRoot "..\coverage\test_strict_types_phase2_pass.kn")
if ($LASTEXITCODE -ne 0) { exit 1 }
& $kern --check --strict-types (Join-Path $PSScriptRoot "fail_mismatch.kn")
if ($LASTEXITCODE -eq 0) { Write-Error "expected fail_mismatch.kn to fail strict check"; exit 1 }
Write-Host "strict_types_phase2: OK"
