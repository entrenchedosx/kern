Param(
    [string]$Exe = "$PSScriptRoot\..\build\Release\kern.exe",
    [string]$OutFile = "$PSScriptRoot\advanced_module_suite_report.json"
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

$xfailTests = @(
    "$PSScriptRoot\coverage\test_module_interop_invalid_abi_xfail.kn",
    "$PSScriptRoot\coverage\test_module_interop_bad_signature_xfail.kn",
    "$PSScriptRoot\coverage\test_module_interop_arity_mismatch_xfail.kn"
)

$results = @()
$passed = 0
$failed = 0
$unexpectedPass = 0

foreach ($t in $tests) {
    & $Exe $t
    $code = $LASTEXITCODE
    $ok = ($code -eq 0)
    if ($ok) { $passed++ } else { $failed++ }
    $results += [PSCustomObject]@{
        name = [IO.Path]::GetFileName($t)
        path = $t
        mode = "normal"
        expected = "pass"
        exit_code = $code
        status = $(if ($ok) { "pass" } else { "fail" })
    }
}

foreach ($t in $xfailTests) {
    & $Exe --ffi --allow-unsafe $t
    $code = $LASTEXITCODE
    $ok = ($code -ne 0)
    if ($ok) { $passed++ } else { $failed++; $unexpectedPass++ }
    $results += [PSCustomObject]@{
        name = [IO.Path]::GetFileName($t)
        path = $t
        mode = "xfail"
        expected = "fail"
        exit_code = $code
        status = $(if ($ok) { "pass" } else { "unexpected_pass" })
    }
}

$summary = [PSCustomObject]@{
    suite = "advanced_module_suite"
    generated_at = [DateTimeOffset]::UtcNow.ToString("o")
    exe = $Exe
    out_file = $OutFile
    total = $results.Count
    passed = $passed
    failed = $failed
    unexpected_pass = $unexpectedPass
    ok = ($failed -eq 0)
    results = $results
}

$outDir = Split-Path -Parent $OutFile
if ($outDir -and -not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

$json = $summary | ConvertTo-Json -Depth 6
Set-Content -Path $OutFile -Value $json -Encoding UTF8

Write-Host $json
if ($summary.ok) { exit 0 } else { exit 1 }
