Param(
    [string]$Exe = "$PSScriptRoot\..\build\Release\kern.exe"
)

$ErrorActionPreference = "Continue"

if (-not (Test-Path $Exe)) {
    Write-Error "kern.exe not found at $Exe"
    exit 1
}

$passTests = @(
    "$PSScriptRoot\coverage\test_browserkit_dom_text.kn",
    "$PSScriptRoot\coverage\test_browserkit_layout_snapshot.kn",
    "$PSScriptRoot\coverage\test_browserkit_html_css_js_pipeline.kn",
    "$PSScriptRoot\coverage\test_browserkit_protocol_fail_loud.kn",
    "$PSScriptRoot\coverage\test_browserkit_ws_sandbox_guard.kn"
)

$xfailTests = @()

$failed = 0

foreach ($t in $passTests) {
    & $Exe $t
    if ($LASTEXITCODE -eq 0) {
        Write-Host "PASS $([IO.Path]::GetFileName($t))" -ForegroundColor Green
    } else {
        Write-Host "FAIL $([IO.Path]::GetFileName($t))" -ForegroundColor Red
        $failed++
    }
}

foreach ($t in $xfailTests) {
    & $Exe $t
    if ($LASTEXITCODE -ne 0) {
        Write-Host "PASS (xfail) $([IO.Path]::GetFileName($t))" -ForegroundColor Green
    } else {
        Write-Host "FAIL expected failure did not fail: $([IO.Path]::GetFileName($t))" -ForegroundColor Red
        $failed++
    }
}

$importXfail = "$PSScriptRoot\coverage\test_browserkit_import_fail_loud.kn"
$importOut = & $Exe $importXfail 2>&1 | Out-String
if ($LASTEXITCODE -ne 0 -and $importOut -match "Code:\s+IMPORT-NOT-FOUND") {
    Write-Host "PASS (xfail+code) $([IO.Path]::GetFileName($importXfail))" -ForegroundColor Green
} elseif ($LASTEXITCODE -ne 0) {
    Write-Host "FAIL expected IMPORT-NOT-FOUND code not found: $([IO.Path]::GetFileName($importXfail))" -ForegroundColor Red
    Write-Host $importOut
    $failed++
} else {
    Write-Host "FAIL expected failure did not fail: $([IO.Path]::GetFileName($importXfail))" -ForegroundColor Red
    $failed++
}

$uncaught = "$PSScriptRoot\coverage\test_browserkit_protocol_uncaught_xfail.kn"
$uncaughtOut = & $Exe $uncaught 2>&1 | Out-String
if ($LASTEXITCODE -ne 0 -and $uncaughtOut -match "Code:\s+BROWSERKIT-PROTOCOL-ERROR") {
    Write-Host "PASS (xfail+code) $([IO.Path]::GetFileName($uncaught))" -ForegroundColor Green
} elseif ($LASTEXITCODE -ne 0) {
    Write-Host "FAIL expected BROWSERKIT-PROTOCOL-ERROR code not found: $([IO.Path]::GetFileName($uncaught))" -ForegroundColor Red
    Write-Host $uncaughtOut
    $failed++
} else {
    Write-Host "FAIL expected failure did not fail: $([IO.Path]::GetFileName($uncaught))" -ForegroundColor Red
    $failed++
}

if ($failed -gt 0) {
    Write-Host "BrowserKit stability suite FAILED ($failed issue(s))." -ForegroundColor Red
    exit 1
}

Write-Host "BrowserKit stability suite PASSED." -ForegroundColor Green
exit 0
