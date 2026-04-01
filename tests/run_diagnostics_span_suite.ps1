Param(
    [string]$Exe = "$PSScriptRoot\..\build\Release\kern.exe"
)

$ErrorActionPreference = "Continue"

if (-not (Test-Path $Exe)) {
    Write-Error "kern.exe not found at $Exe"
    exit 1
}

$failed = 0

function Assert-Contains {
    param(
        [string]$Text,
        [string]$Needle,
        [string]$Name
    )
    if ($Text.Contains($Needle)) { return $true }
    Write-Host "FAIL $Name (text not found: $Needle)" -ForegroundColor Red
    return $false
}

$rangeTest = "$PSScriptRoot\coverage\test_diag_span_range.kn"
$rangeOut = & $Exe $rangeTest 2>&1 | Out-String
if ($LASTEXITCODE -eq 0) {
    Write-Host "FAIL test_diag_span_range.kn expected failure" -ForegroundColor Red
    $failed++
} else {
    $ok = $true
    $ok = (Assert-Contains $rangeOut "test_diag_span_range.kn:3:5" "range location") -and $ok
    $ok = (Assert-Contains $rangeOut "Code: IMPORT-NOT-FOUND" "range code") -and $ok
    $ok = (Assert-Contains $rangeOut "~" "range underline") -and $ok
    if ($ok) { Write-Host "PASS test_diag_span_range.kn" -ForegroundColor Green } else { $failed++ }
}

$fallbackTest = "$PSScriptRoot\coverage\test_diag_span_fallback.kn"
$fallbackOut = & $Exe $fallbackTest 2>&1 | Out-String
if ($LASTEXITCODE -eq 0) {
    Write-Host "FAIL test_diag_span_fallback.kn expected failure" -ForegroundColor Red
    $failed++
} else {
    $ok = $true
    $ok = (Assert-Contains $fallbackOut "test_diag_span_fallback.kn:3:1" "fallback location") -and $ok
    $ok = (Assert-Contains $fallbackOut "Code: IMPORT-NOT-FOUND" "fallback code") -and $ok
    if ($ok) { Write-Host "PASS test_diag_span_fallback.kn" -ForegroundColor Green } else { $failed++ }
}

$multilineTest = "$PSScriptRoot\coverage\test_diag_span_multiline.kn"
$multiOut = & $Exe $multilineTest 2>&1 | Out-String
if ($LASTEXITCODE -eq 0) {
    Write-Host "FAIL test_diag_span_multiline.kn expected failure" -ForegroundColor Red
    $failed++
} else {
    $ok = $true
    $ok = (Assert-Contains $multiOut "test_diag_span_multiline.kn:3:5" "multiline location") -and $ok
    $ok = (Assert-Contains $multiOut "3 |" "multiline first line") -and $ok
    $ok = (Assert-Contains $multiOut "4 |" "multiline second line") -and $ok
    $ok = (Assert-Contains $multiOut "~" "multiline underline") -and $ok
    if ($ok) { Write-Host "PASS test_diag_span_multiline.kn" -ForegroundColor Green } else { $failed++ }
}

$jsonPath = "$PSScriptRoot\coverage\test_diag_json_compile_range.kn"
$jsonOut = & $Exe --check --json $jsonPath 2>&1 | Out-String
if ($LASTEXITCODE -eq 0) {
    Write-Host "FAIL --check --json expected compile failure for invalid file" -ForegroundColor Red
    $failed++
} else {
    $ok = $true
    $ok = (Assert-Contains $jsonOut "\"lineEnd\":" "json lineEnd") -and $ok
    $ok = (Assert-Contains $jsonOut "\"columnEnd\":" "json columnEnd") -and $ok
    $ok = (Assert-Contains $jsonOut "\"range\":{" "json range object") -and $ok
    if ($ok) { Write-Host "PASS check-json sanity" -ForegroundColor Green } else { $failed++ }
}

if ($failed -gt 0) {
    Write-Host "Diagnostics span suite FAILED ($failed issue(s))." -ForegroundColor Red
    exit 1
}

Write-Host "Diagnostics span suite PASSED." -ForegroundColor Green
exit 0
