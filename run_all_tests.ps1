# kern Full Test Runner - Runs all example scripts and reports pass/fail.
# usage: .\run_all_tests.ps1 [ -Exe path\to\kern.exe ] [ -Examples path\to\examples ] [ -TimeoutSeconds N ]
# when run from FINAL: prefers kern\kern.exe, then bin\kern.exe. From repo root: build\Release\kern.exe.
param(
    [string]$Exe = "",
    [string]$Examples = "$PSScriptRoot\examples",
    # interactive / long-running demos: exceed default timeout in CI batch runs.
    [string[]]$Skip = @(
        "gamekit_gui_app.kn",
        "gamekit_simple_game.kn",
        "sprite_movement_integrity.kn",
        "13_graphics.kn",
        "15_bouncy_ball.kn",
        "graphics_demo.kn",
        "g2d_render_target_gradient.kn",
        "g2d_shapes_plus.kn",
        "3d_camera_modes.kn",
        "3d_camera_object_tools.kn",
        "3d_multi_shapes.kn",
        "3d_controls_reference.kn",
        "3d_interactive_scene.kn",
        "3d_object_registry.kn",
        "3d_rotating_cube.kn",
        "3d_scene_load_save.kn",
        "3d_textured_scene.kn"
    ),
    [string[]]$ExpectNonZero = @("error_demo_bad.kn", "error_demo_runtime.kn", "exit_code_demo.kn", "exit_code_simple.kn"),
    [int]$TimeoutSeconds = 30
)
# powerShell 5.x does not support inline if-expressions in param defaults.
if ([string]::IsNullOrWhiteSpace($Exe)) {
    if (Test-Path "$PSScriptRoot\kern\kern.exe") {
        $Exe = "$PSScriptRoot\kern\kern.exe"
    } elseif (Test-Path "$PSScriptRoot\BUILD\bin\kern.exe") {
        $Exe = "$PSScriptRoot\BUILD\bin\kern.exe"
    } elseif (Test-Path "$PSScriptRoot\bin\kern.exe") {
        $Exe = "$PSScriptRoot\bin\kern.exe"
    } else {
        $Exe = "$PSScriptRoot\build\Release\kern.exe"
    }
}
# resolve relative paths relative to script directory so it works from FINAL or repo root
if (-not [System.IO.Path]::IsPathRooted($Exe)) { $Exe = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot $Exe)) }
if (-not [System.IO.Path]::IsPathRooted($Examples)) { $Examples = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot $Examples)) }

if (-not (Test-Path $Exe)) {
    Write-Error "kern.exe not found at $Exe. From repo root build with: cmake --build build --config Release. From packaged layouts use kern\\kern.exe or BUILD\\bin\\kern.exe (see RELEASE.md)."
    exit 1
}
if (-not (Test-Path $Examples)) {
    Write-Error "Examples folder not found: $Examples"
    exit 1
}

# examples are organized in subfolders (basic/, golden/, …); recurse so batch runs stay useful.
$files = Get-ChildItem -Path $Examples -Filter "*.kn" -File -Recurse -ErrorAction SilentlyContinue | Sort-Object FullName
if (-not $files -or $files.Count -eq 0) {
    Write-Error "No .kn files found in $Examples"
    exit 1
}
$passed = 0
$failed = @()
$skipped = 0
$timeoutMs = [Math]::Max(5000, $TimeoutSeconds * 1000)

foreach ($f in $files) {
    $name = $f.Name
    if ($Skip -contains $name) {
        Write-Host "[SKIP] $name (interactive / long-running - skipped in batch)"
        $skipped++
        continue
    }
    $fullPath = $f.FullName
    $prevErr = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $code = -1
    try {
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = $Exe
        $psi.Arguments = "`"$fullPath`""
        $psi.WorkingDirectory = $PSScriptRoot
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
    $ErrorActionPreference = $prevErr
    $expectFail = $ExpectNonZero -contains $name
    if ($code -eq -2) {
        Write-Host "[FAIL] $name (timeout)"
        $failed += $name
    } elseif ($code -eq 0) {
        Write-Host "[PASS] $name"
        $passed++
    } elseif ($expectFail) {
        Write-Host "[PASS] $name (expected non-zero exit $code)"
        $passed++
    } else {
        Write-Host "[FAIL] $name (exit $code)"
        $failed += $name
    }
}

Write-Host ""
Write-Host "=============================================="
Write-Host "Kern Test Summary"
Write-Host "=============================================="
Write-Host "Passed:  $passed"
Write-Host "Failed:  $($failed.Count)"
Write-Host "Skipped: $skipped"
Write-Host "Total:   $($files.Count)"
if ($failed.Count -gt 0) {
    Write-Host "Failed files: $($failed -join ', ')"
    exit 1
}
Write-Host "All tests passed."
exit 0
