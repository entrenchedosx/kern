# Sync the monorepo Kern-IDE/ tree into a local clone of https://github.com/entrenchedosx/kern-IDE
# under desktop-tk/ (the layout used by the standalone kern-IDE repo).
#
# Usage (from repo root):
#   .\scripts\sync_kern_ide_to_desktop_tk.ps1 -Destination "D:\path\to\kern-IDE\desktop-tk"
#
# Then in that kern-IDE clone: git add desktop-tk; git commit; git push origin main

param(
    [Parameter(Mandatory = $true)]
    [string] $Destination
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path $PSScriptRoot -Parent
$Source = Join-Path $RepoRoot "Kern-IDE"

if (-not (Test-Path $Source)) {
    throw "Source not found: $Source"
}
$destParent = Split-Path $Destination -Parent
if (-not (Test-Path $destParent)) {
    throw "Parent of destination does not exist: $destParent"
}

Write-Host "Source:      $Source"
Write-Host "Destination: $Destination"

# /XD is picky; also remove known PyInstaller junk if present in the monorepo tree
& robocopy $Source $Destination /E /NDL /NFL /NJH /NJS `
    /XD __pycache__ .git build node_modules packaging\work_verify packaging\work_verify2 packaging\dist_verify packaging\dist_verify2
$code = $LASTEXITCODE
if ($code -ge 8) {
    exit $code
}
foreach ($rel in @(
        "packaging\work_verify", "packaging\work_verify2",
        "packaging\dist_verify", "packaging\dist_verify2"
    )) {
    $p = Join-Path $Destination $rel
    if (Test-Path $p) {
        Remove-Item -Recurse -Force $p
    }
}
Write-Host "Done. Review changes in the kern-IDE repo, then commit and push."
exit 0
