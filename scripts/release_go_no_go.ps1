<#
.SYNOPSIS
    Run a production shareability gate and print GO/NO-GO.
.DESCRIPTION
    Checks shareable artifacts, binary smoke tests, regressions, examples, and coverage.
    Exits non-zero on any blocker.
#>
[CmdletBinding()]
param(
    [switch]$SkipCoverage
)

$ErrorActionPreference = "Stop"
$Root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
Set-Location -LiteralPath $Root

$ok = $true
function Check-Step([string]$name, [scriptblock]$run) {
    Write-Host ""
    Write-Host "[check] $name" -ForegroundColor DarkGray
    try {
        & $run
        Write-Host "[pass]  $name" -ForegroundColor Green
    } catch {
        $script:ok = $false
        Write-Host "[fail]  $name :: $($_.Exception.Message)" -ForegroundColor Red
    }
}

Check-Step "shareable artifacts exist" {
    $required = @(
        "shareable-ide\compiler\kern.exe",
        "shareable-ide\compiler\kern_repl.exe",
        "shareable-ide\compiler\kernc.exe",
        "shareable-kern-to-exe\kern.exe",
        "shareable-kern-to-exe\kernc.exe",
        "shareable-kern-to-exe\kern-to-exe.bat"
    )
    foreach ($r in $required) {
        if (-not (Test-Path (Join-Path $Root $r))) {
            throw "missing required artifact: $r"
        }
    }
}

Check-Step "shareable kern doctor" {
    & (Join-Path $Root "shareable-ide\compiler\kern.exe") doctor *> $null
    if ($LASTEXITCODE -ne 0) { throw "kern doctor failed (exit $LASTEXITCODE)" }
}

Check-Step "shareable dependency audit" {
    $py = (Get-Command python -ErrorAction SilentlyContinue).Source
    if (-not $py) { $py = "python" }
    & $py (Join-Path $Root "scripts\check_shareable_deps.py") --roots `
        (Join-Path $Root "shareable-ide\compiler") `
        (Join-Path $Root "shareable-kern-to-exe") `
        *> $null
    if ($LASTEXITCODE -ne 0) { throw "shareable dependency audit failed" }
}

Check-Step "parser noncrash regression" {
    & powershell -ExecutionPolicy Bypass -File (Join-Path $Root "tests\regression\run_parser_noncrash_suite.ps1") *> $null
    if ($LASTEXITCODE -ne 0) { throw "parser noncrash suite failed" }
}

Check-Step "vm fail-fast regression" {
    & powershell -ExecutionPolicy Bypass -File (Join-Path $Root "tests\regression\run_vm_fail_fast_suite.ps1") *> $null
    if ($LASTEXITCODE -ne 0) { throw "vm fail-fast suite failed" }
}

Check-Step "examples check" {
    & powershell -ExecutionPolicy Bypass -File (Join-Path $Root "scripts\check_examples.ps1") *> $null
    if ($LASTEXITCODE -ne 0) { throw "examples check failed" }
}

if (-not $SkipCoverage) {
    Check-Step "full coverage/regression suite" {
        & powershell -ExecutionPolicy Bypass -File (Join-Path $Root "tests\run_all_coverage_kn.ps1") -Exe (Join-Path $Root "build\Release\kern.exe") -TimeoutSeconds 45 *> $null
        if ($LASTEXITCODE -ne 0) { throw "coverage/regression suite failed" }
    }
}

Write-Host ""
if ($ok) {
    Write-Host "GO: release gate passed." -ForegroundColor Green
    exit 0
}
Write-Host "NO-GO: release gate failed." -ForegroundColor Red
exit 1

