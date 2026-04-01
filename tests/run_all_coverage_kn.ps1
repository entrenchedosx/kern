# run every tests/coverage/*.kn and tests/regression/*.kn with kern.exe (timeout per script).
# usage: .\tests\run_all_coverage_kn.ps1 [-Exe path\to\kern.exe] [-TimeoutSeconds 45]
param(
    [string]$Exe = "",
    [int]$TimeoutSeconds = 45
)
$ErrorActionPreference = "Continue"
$Root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
if ([string]::IsNullOrWhiteSpace($Exe)) {
    $Exe = Join-Path $Root "build\Release\kern.exe"
    if (-not (Test-Path $Exe)) { $Exe = Join-Path $Root "build\Debug\kern.exe" }
}
if (-not (Test-Path $Exe)) { throw "kern.exe not found: $Exe" }

$dirs = @(
    (Join-Path $Root "tests\coverage"),
    (Join-Path $Root "tests\regression")
)
$files = @()
foreach ($d in $dirs) {
    if (Test-Path $d) {
        $files += Get-ChildItem -Path $d -Filter "*.kn" -File -ErrorAction SilentlyContinue
    }
}
$files = $files | Sort-Object FullName -Unique
if (-not $files -or $files.Count -eq 0) { throw "No .kn files under tests/coverage or tests/regression" }

$timeoutMs = [Math]::Max(5000, $TimeoutSeconds * 1000)
$passed = 0
$expectedFailPassed = 0
$failed = @()
$expectedFailSet = @{}
@(
    # negative-path suites that intentionally raise diagnostics/non-zero exits.
    "test_browserkit_import_fail_loud.kn",
    "test_diag_json_compile_range.kn",
    "test_diag_span_fallback.kn",
    "test_diag_span_multiline.kn",
    "test_diag_span_range.kn",
    "test_module_loading.kn",
    "test_phase2_ffi_typed.kn"
) | ForEach-Object { $expectedFailSet[$_] = $true }

foreach ($f in $files) {
    $name = $f.FullName.Substring($Root.Length + 1)
    $base = [System.IO.Path]::GetFileName($f.FullName)
    $isExpectedFail = ($base -like "*_xfail.kn") -or $expectedFailSet.ContainsKey($base)
    $code = -1
    try {
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = $Exe
        $psi.Arguments = "`"$($f.FullName)`""
        $psi.WorkingDirectory = $Root
        $psi.UseShellExecute = $false
        $psi.CreateNoWindow = $true
        $p = [System.Diagnostics.Process]::Start($psi)
        if (-not $p.WaitForExit($timeoutMs)) {
            $p.Kill()
            $code = -2
        } else {
            $code = $p.ExitCode
        }
    } catch {
        $code = -3
    }
    if ($code -eq -2) {
        Write-Host "[FAIL] $name (timeout)" -ForegroundColor Red
        $failed += $name
    } elseif ($code -eq 0 -and -not $isExpectedFail) {
        Write-Host "[PASS] $name" -ForegroundColor Green
        $passed++
    } elseif ($code -ne 0 -and $isExpectedFail) {
        Write-Host "[PASS] $name (xfail)" -ForegroundColor Green
        $passed++
    } elseif ($code -eq 0 -and $isExpectedFail) {
        Write-Host "[UNEXPECTED PASS] $name" -ForegroundColor Yellow
        $expectedFailPassed++
        $failed += "$name (unexpected pass)"
    } else {
        Write-Host "[FAIL] $name (exit $code)" -ForegroundColor Red
        $failed += $name
    }
}
Write-Host ""
Write-Host "Coverage/regression summary: $passed passed, $($failed.Count) failed, $expectedFailPassed unexpected-pass (total $($files.Count))"
if ($failed.Count -gt 0) { exit 1 }
exit 0
