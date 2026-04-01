Param(
    [string]$Exe = "$PSScriptRoot\..\build\Release\kern.exe"
)

$ErrorActionPreference = "Continue"

if (-not (Test-Path $Exe)) {
    Write-Error "kern.exe not found at $Exe"
    exit 1
}

$tests = @(
    "$PSScriptRoot\coverage\test_module_interop.kn",
    "$PSScriptRoot\coverage\test_module_concurrency.kn",
    "$PSScriptRoot\coverage\test_module_observability.kn",
    "$PSScriptRoot\coverage\test_module_security.kn",
    "$PSScriptRoot\coverage\test_module_automation.kn",
    "$PSScriptRoot\coverage\test_module_binary.kn",
    "$PSScriptRoot\coverage\test_module_websec.kn",
    "$PSScriptRoot\coverage\test_module_netops.kn",
    "$PSScriptRoot\coverage\test_module_datatools.kn",
    "$PSScriptRoot\coverage\test_module_runtime_controls.kn",
    "$PSScriptRoot\coverage\test_module_security_negative.kn",
    "$PSScriptRoot\coverage\test_module_datatools_negative.kn",
    "$PSScriptRoot\coverage\test_module_runtime_controls_negative.kn",
    "$PSScriptRoot\coverage\test_module_automation_negative.kn",
    "$PSScriptRoot\coverage\test_module_automation_timeout.kn",
    "$PSScriptRoot\coverage\test_module_websec_negative.kn"
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

& $Exe --ffi --allow-unsafe "$PSScriptRoot\coverage\test_module_interop_invalid_abi_xfail.kn"
if ($LASTEXITCODE -ne 0) {
    Write-Host "PASS (xfail) test_module_interop_invalid_abi_xfail.kn" -ForegroundColor Green
} else {
    Write-Host "FAIL expected failure did not fail: test_module_interop_invalid_abi_xfail.kn" -ForegroundColor Red
    $failed++
}

& $Exe --ffi --allow-unsafe "$PSScriptRoot\coverage\test_module_interop_bad_signature_xfail.kn"
if ($LASTEXITCODE -ne 0) {
    Write-Host "PASS (xfail) test_module_interop_bad_signature_xfail.kn" -ForegroundColor Green
} else {
    Write-Host "FAIL expected failure did not fail: test_module_interop_bad_signature_xfail.kn" -ForegroundColor Red
    $failed++
}

& $Exe --ffi --allow-unsafe "$PSScriptRoot\coverage\test_module_interop_arity_mismatch_xfail.kn"
if ($LASTEXITCODE -ne 0) {
    Write-Host "PASS (xfail) test_module_interop_arity_mismatch_xfail.kn" -ForegroundColor Green
} else {
    Write-Host "FAIL expected failure did not fail: test_module_interop_arity_mismatch_xfail.kn" -ForegroundColor Red
    $failed++
}

if ($failed -gt 0) {
    Write-Host "Advanced module suite FAILED ($failed test(s))." -ForegroundColor Red
    exit 1
}

Write-Host "Advanced module suite PASSED." -ForegroundColor Green
exit 0
