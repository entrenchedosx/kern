param(
    [string]$Exe = "$PSScriptRoot\..\build\Release\kern.exe"
)

$ErrorActionPreference = "Continue"

if (-not (Test-Path $Exe)) {
    Write-Error "kern.exe not found at $Exe"
    exit 1
}

$tests = @(
    "$PSScriptRoot\coverage\test_runtime_guards.kn",
    "$PSScriptRoot\coverage\test_process_runtime_wave1.kn",
    "$PSScriptRoot\coverage\test_stdlib_hashing_and_config.kn",
    "$PSScriptRoot\coverage\test_lang_generators_enums.kn",
    "$PSScriptRoot\coverage\test_wave2_stdlib_network.kn",
    "$PSScriptRoot\coverage\test_system_modules.kn"
)

$failed = 0
foreach ($t in $tests) {
    & $Exe $t
    if ($LASTEXITCODE -eq 0) {
        Write-Host "PASS $([IO.Path]::GetFileName($t))" -ForegroundColor Green
    } else {
        Write-Host "FAIL $([IO.Path]::GetFileName($t))" -ForegroundColor Red
        $failed++
    }
}

& $Exe --check --json "$PSScriptRoot\coverage\test_lang_generators_enums.kn" | Out-Host
if ($failed -gt 0) { exit 1 }
exit 0
