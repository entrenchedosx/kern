# Compares `kern --bytecode-normalized` output to a checked-in golden file (path-agnostic bytecode listing).
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$kern = $null
foreach ($c in @(
        (Join-Path $root "build\Release\kern.exe"),
        (Join-Path $root "build\Debug\kern.exe"),
        (Join-Path $root "build\kern.exe"),
        (Join-Path $root "build\kern"))) {
    if (Test-Path -LiteralPath $c) { $kern = $c; break }
}
if (-not $kern) { Write-Error "Build kern first (e.g. cmake --build build --config Release --target kern)" }
$kn = [System.IO.Path]::Combine($root, "examples", "basic", "01_hello_world.kn")
$expPath = [System.IO.Path]::Combine($root, "tests", "coverage", "bytecode_golden_hello_world.expected")
$out = & $kern --bytecode-normalized $kn 2>&1
if ($LASTEXITCODE -ne 0) { Write-Error "kern --bytecode-normalized failed" }
$exp = Get-Content -LiteralPath $expPath -Raw
$norm = [string]::Join("`n", ($out -split "`r?`n" | ForEach-Object { $_.TrimEnd() } | Where-Object { $_ -ne "" }))
$expN = [string]::Join("`n", ($exp -split "`r?`n" | ForEach-Object { $_.TrimEnd() } | Where-Object { $_ -ne "" }))
if ($norm -ne $expN) {
    Write-Host "GOLDEN MISMATCH"
    Write-Host "--- got ---"
    Write-Host $out
    Write-Host "--- expected ---"
    Write-Host $exp
    exit 1
}
Write-Host "bytecode golden OK"
