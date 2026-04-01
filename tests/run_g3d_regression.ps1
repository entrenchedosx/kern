Param(
    [string]$SplExe = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $SplExe) {
    $SplExe = Join-Path $repoRoot "build\Release\kern.exe"
}

if (-not (Test-Path $SplExe)) {
    Write-Host "kern.exe not found: $SplExe"
    exit 1
}

$coverage = Join-Path $repoRoot "tests\coverage"
$scripts = @(
    (Join-Path $coverage "test_g3d_import.kn"),
    (Join-Path $coverage "test_g3d_api_surface.kn"),
    (Join-Path $coverage "test_g3d_objects.kn"),
    (Join-Path $coverage "test_g3d_materials.kn"),
    (Join-Path $coverage "test_g3d_scene_io.kn"),
    (Join-Path $coverage "test_g3d_groups_and_lights.kn"),
    (Join-Path $coverage "test_g3d_input_api.kn"),
    (Join-Path $coverage "test_g3d_status.kn"),
    (Join-Path $coverage "test_g3d_new_features.kn"),
    (Join-Path $coverage "test_g3d_collision.kn")
)

$failed = @()
foreach ($s in $scripts) {
    Write-Host "`n==> $s"
    & $SplExe $s
    if ($LASTEXITCODE -ne 0) {
        $failed += $s
    }
}

if ($failed.Count -gt 0) {
    Write-Host "`nG3D regression FAILED:"
    $failed | ForEach-Object { Write-Host " - $_" }
    exit 1
}

Write-Host "`nG3D regression PASSED."
exit 0

