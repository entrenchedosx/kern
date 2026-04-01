$ErrorActionPreference = "Stop"

$root = $PSScriptRoot
$binCandidates = @(
    (Join-Path $root "FINAL\bin"),
    (Join-Path $root "FINAL\kern"),
    (Join-Path $root "build\Release")
)

$bin = $binCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $bin) {
    throw "No Kern binary directory found. Build first (e.g., cmake --build build --config Release)."
}

$current = [Environment]::GetEnvironmentVariable("Path", "User")
if ($current -notlike "*$bin*") {
    $newPath = if ([string]::IsNullOrWhiteSpace($current)) { $bin } else { "$current;$bin" }
    [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
    Write-Host "Added to user PATH: $bin"
} else {
    Write-Host "PATH already contains: $bin"
}

Write-Host "Open a new terminal and run: kern --version"
