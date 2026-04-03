# Run kernc against tests/compile_pipeline_fixture/kernconfig.example.json (repo root as cwd).
# Requires: built kernc (Release). Does not run the produced .exe.
param(
    [string]$RepoRoot = "",
    [switch]$CheckOnly,
    [switch]$RunInterpreter
)
$ErrorActionPreference = "Stop"
if (-not $RepoRoot) { $RepoRoot = Split-Path -Parent $PSScriptRoot }
Set-Location -LiteralPath $RepoRoot

$kernc = @(
    (Join-Path $RepoRoot "build\Release\kern-impl.exe"),
    (Join-Path $RepoRoot "build\Release\kernc.exe"),
    (Join-Path $RepoRoot "build\Debug\kern-impl.exe")
) | Where-Object { Test-Path $_ } | Select-Object -First 1

$kern = @(
    (Join-Path $RepoRoot "build\Release\kern.exe"),
    (Join-Path $RepoRoot "build\Debug\kern.exe")
) | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $kernc) {
    Write-Error "No kernc/kern-impl.exe found under build/Release or build/Debug. Build target kernc first."
}

$mainKn = Join-Path $RepoRoot "tests\compile_pipeline_fixture\main.kn"
$cfg = Join-Path $RepoRoot "tests\compile_pipeline_fixture\kernconfig.example.json"

if ($CheckOnly) {
    if (-not $kern) { Write-Error "No kern.exe for --check. Build target kern first." }
    & $kern --check $mainKn
    exit $LASTEXITCODE
}

if ($RunInterpreter) {
    if (-not $kern) { Write-Error "No kern.exe. Build target kern first." }
    $fixtureDir = Split-Path -Parent $mainKn
    Push-Location -LiteralPath $fixtureDir
    try {
        & $kern main.kn
        exit $LASTEXITCODE
    } finally {
        Pop-Location
    }
}

if (-not (Test-Path $cfg)) {
    Write-Error "Missing $cfg"
}

& $kernc --config $cfg
exit $LASTEXITCODE
