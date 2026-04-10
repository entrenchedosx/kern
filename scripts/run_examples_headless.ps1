# Run a headless-safe subset of examples/ (no g2d/g3d/game imports).
# Usage: pwsh -File scripts/run_examples_headless.ps1 [-KernExe path\to\kern.exe]

param(
    [string]$KernExe = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if (-not $KernExe) {
    $candidates = @(
        Join-Path $root "build-plan\Release\kern.exe",
        Join-Path $root "build\Release\kern.exe",
        "kern"
    )
    foreach ($c in $candidates) {
        if ($c -eq "kern") { $KernExe = "kern"; break }
        if (Test-Path -LiteralPath $c) { $KernExe = $c; break }
    }
}

$examples = @(
    "examples\basic\01_hello_world.kn",
    "examples\basic\23_recursion_fib.kn",
    "examples\tour\05_control_flow.kn"
)

Push-Location $root
try {
    foreach ($ex in $examples) {
        $p = Join-Path $root $ex
        if (-not (Test-Path -LiteralPath $p)) {
            Write-Warning "skip missing $p"
            continue
        }
        Write-Host "==> $ex"
        & $KernExe $p
        if ($LASTEXITCODE -ne 0) { throw "failed: $ex ($LASTEXITCODE)" }
    }
    Write-Host "All listed examples OK."
}
finally {
    Pop-Location
}
