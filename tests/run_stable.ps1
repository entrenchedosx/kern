# Single entry point for "stable" validation: contract test, bytecode golden, full .kn coverage.
#   -Quick          contract + golden only (fast; e.g. after Debug -Werror build)
#   -BuildDir name  CMake output dir (default: build) — use build-dbg for the Linux Werror job
#   -WithExamples   after coverage, run every examples/*.kn (slower; not with -Quick)
# Usage: powershell -File tests/run_stable.ps1 [-Quick] [-BuildDir build] [-WithExamples]
param(
    [switch]$WithExamples,
    [switch]$Quick,
    [string]$BuildDir = "build"
)
$ErrorActionPreference = "Stop"
if ($Quick -and $WithExamples) {
    throw "Cannot use -Quick and -WithExamples together."
}
$Root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$bd = [System.IO.Path]::GetFullPath([System.IO.Path]::Combine($Root, $BuildDir))

function Find-FirstExisting([string[]]$Paths) {
    foreach ($p in $Paths) {
        if (Test-Path -LiteralPath $p) { return $p }
    }
    return $null
}

$kern = Find-FirstExisting @(
    ([System.IO.Path]::Combine($bd, "Release", "kern.exe")),
    ([System.IO.Path]::Combine($bd, "Debug", "kern.exe")),
    ([System.IO.Path]::Combine($bd, "kern.exe")),
    ([System.IO.Path]::Combine($bd, "kern"))
)
if (-not $kern) { throw "kern not found under $BuildDir/; configure and build first." }

$contract = Find-FirstExisting @(
    ([System.IO.Path]::Combine($bd, "Release", "kern_contract_humanize.exe")),
    ([System.IO.Path]::Combine($bd, "Debug", "kern_contract_humanize.exe")),
    ([System.IO.Path]::Combine($bd, "kern_contract_humanize.exe")),
    ([System.IO.Path]::Combine($bd, "kern_contract_humanize"))
)
if (-not $contract) {
    throw "kern_contract_humanize not found under $BuildDir/; build target kern_contract_humanize"
}

Write-Host "== kern_contract_humanize ==" -ForegroundColor Cyan
& $contract
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "== bytecode golden ==" -ForegroundColor Cyan
$kn = [System.IO.Path]::Combine($Root, "examples", "basic", "01_hello_world.kn")
$expPath = [System.IO.Path]::Combine($Root, "tests", "coverage", "bytecode_golden_hello_world.expected")
$out = & $kern --bytecode-normalized $kn 2>&1
if ($LASTEXITCODE -ne 0) { throw "kern --bytecode-normalized failed" }
$exp = Get-Content -LiteralPath $expPath -Raw
$norm = [string]::Join("`n", ($out -split "`r?`n" | ForEach-Object { $_.TrimEnd() } | Where-Object { $_ -ne "" }))
$expN = [string]::Join("`n", ($exp -split "`r?`n" | ForEach-Object { $_.TrimEnd() } | Where-Object { $_ -ne "" }))
if ($norm -ne $expN) {
    Write-Host "GOLDEN MISMATCH" -ForegroundColor Red
    Write-Host "--- got ---"; Write-Host $out
    Write-Host "--- expected ---"; Write-Host $exp
    exit 1
}
Write-Host "bytecode golden OK"

if ($Quick) {
    Write-Host "== (quick: skipping coverage) ==" -ForegroundColor DarkGray
    exit 0
}

Write-Host "== coverage / regression .kn ==" -ForegroundColor Cyan
& (Join-Path $PSScriptRoot "run_all_coverage_kn.ps1") -Exe $kern
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($WithExamples) {
    Write-Host "== examples/*.kn ==" -ForegroundColor Cyan
    & (Join-Path $PSScriptRoot "run_tests.ps1") -Exe $kern
    exit $LASTEXITCODE
}
exit 0
