# run every tests/coverage/*.kn and tests/regression/*.kn with kern.exe (timeout per script).
#
# Defaults are chosen so the suite passes on typical dev machines and headless CI:
#   - ExtraKernArgs: --unsafe  (tests are trusted; many need filesystem/process permissions)
#   - Graphics tests (g2d/g3d) are skipped when `import g2d` fails (e.g. KERN_BUILD_GAME=OFF), or when
#     $env:KERN_COVERAGE_SKIP_GRAPHICS is 1/true (headless release CI with Raylib but no display).
#
# usage: .\tests\run_all_coverage_kn.ps1 [-Exe path\to\kern] [-TimeoutSeconds 45] [-ExtraKernArgs "--unsafe"]
param(
    [string]$Exe = "",
    [int]$TimeoutSeconds = 45,
    [string]$ExtraKernArgs = "--unsafe"
)
$ErrorActionPreference = "Continue"
$Root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))

if ([string]::IsNullOrWhiteSpace($Exe)) {
    $candidates = @(
        (Join-Path $Root "build\Release\kern.exe"),
        (Join-Path $Root "build\Debug\kern.exe"),
        (Join-Path $Root "build\kern.exe"),
        (Join-Path $Root "build\kern")
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { $Exe = $c; break }
    }
}
if (-not (Test-Path $Exe)) { throw "kern executable not found. Build kern first (e.g. cmake --build build --config Release --target kern)." }

function Test-KernHasG2dModule {
    param([string]$KernExe, [string]$ExtraArgs, [string]$WorkDir)
    $tmp = [System.IO.Path]::GetTempFileName() + ".kn"
    try {
        Set-Content -LiteralPath $tmp -Value "import g2d`nprint(`"ok`")" -Encoding UTF8
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = $KernExe
        if ([string]::IsNullOrWhiteSpace($ExtraArgs)) {
            $psi.Arguments = "`"$tmp`""
        } else {
            $psi.Arguments = "$ExtraArgs `"$tmp`""
        }
        $psi.WorkingDirectory = $WorkDir
        $psi.UseShellExecute = $false
        $psi.CreateNoWindow = $true
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError = $true
        $p = [System.Diagnostics.Process]::Start($psi)
        if (-not $p.WaitForExit(20000)) {
            try { $p.Kill() } catch {}
            return $false
        }
        return ($p.ExitCode -eq 0)
    } catch {
        return $false
    } finally {
        Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue
    }
}

function Test-ShouldSkipGraphicsTest {
    param([string]$BaseName, [bool]$HasGraphics)
    if ($HasGraphics) { return $false }
    if ($BaseName -like "test_g2d_*") { return $true }
    if ($BaseName -like "test_g3d_*") { return $true }
    if ($BaseName -like "test_graphics_*") { return $true }
    if ($BaseName -like "*g3d*") { return $true }
    return $false
}

function Test-IsWindowsOs {
    if ($PSVersionTable.PSVersion.Major -ge 6) { return $IsWindows }
    return $env:OS -eq "Windows_NT"
}

function Test-ShouldSkipWindowsOnlyTest {
    param([string]$BaseName)
    if (Test-IsWindowsOs) { return $false }
    # These scripts spawn cmd.exe / Windows-only automation paths (no cmd on Linux/macOS CI).
    if ($BaseName -eq "test_module_automation_timeout.kn") { return $true }
    if ($BaseName -eq "test_process_runtime_wave1.kn") { return $true }
    return $false
}

function Build-KernArguments {
    param([string]$ScriptPath, [string]$ExtraArgs)
    if ([string]::IsNullOrWhiteSpace($ExtraArgs)) { return "`"$ScriptPath`"" }
    return "$ExtraArgs `"$ScriptPath`""
}

$evSkip = $env:KERN_COVERAGE_SKIP_GRAPHICS
if ($evSkip -eq "1" -or $evSkip -ieq "true" -or $evSkip -ieq "yes") {
    $hasGraphics = $false
} else {
    $hasGraphics = Test-KernHasG2dModule -KernExe $Exe -ExtraArgs $ExtraKernArgs -WorkDir $Root
}

$dirs = @(
    ([System.IO.Path]::Combine($Root, "tests", "coverage")),
    ([System.IO.Path]::Combine($Root, "tests", "regression"))
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
$skipped = 0
$expectedFailPassed = 0
$failed = @()
$expectedFailSet = @{}
@(
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
    if (Test-ShouldSkipGraphicsTest -BaseName $base -HasGraphics $hasGraphics) {
        Write-Host "[SKIP] $name (g2d/g3d not in this binary)" -ForegroundColor DarkYellow
        $skipped++
        continue
    }
    if (Test-ShouldSkipWindowsOnlyTest -BaseName $base) {
        Write-Host "[SKIP] $name (Windows-only: cmd.exe / automation)" -ForegroundColor DarkYellow
        $skipped++
        continue
    }
    $isExpectedFail = ($base -like "*_xfail.kn") -or $expectedFailSet.ContainsKey($base)
    $code = -1
    try {
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = $Exe
        $psi.Arguments = (Build-KernArguments -ScriptPath $f.FullName -ExtraArgs $ExtraKernArgs)
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
Write-Host "Coverage/regression summary: $passed passed, $($failed.Count) failed, $skipped skipped, $expectedFailPassed unexpected-pass (ran $($files.Count - $skipped) of $($files.Count))"
if ($failed.Count -gt 0) { exit 1 }
exit 0
