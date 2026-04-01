# kern Test Runner - runs all example .kn files
$ErrorActionPreference = "Continue"
$kern = "..\FINAL\bin\kern.exe"
if (-not (Test-Path $kern)) { $kern = "..\FINAL\Release\kern.exe" }
if (-not (Test-Path $kern)) { $kern = "..\build\Release\kern.exe" }
if (-not (Test-Path $kern)) { $kern = "..\build\Debug\kern.exe" }
$examples = Get-ChildItem "..\examples" -Filter "*.kn" -File -Recurse
$passed = 0
$failed = 0
foreach ($f in $examples) {
    $out = & $kern $f.FullName 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "PASS $($f.Name)" -ForegroundColor Green
        $passed++
    } else {
        Write-Host "FAIL $($f.Name)" -ForegroundColor Red
        Write-Host $out
        $failed++
    }
}
Write-Host "`n$passed passed, $failed failed"
