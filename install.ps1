#Requires -Version 5.0
<#
.SYNOPSIS
  Build Kern (Release) and install kern.exe into a directory, optionally add it to User PATH (no duplicates).

.PARAMETER Prefix
  Install directory (default: $env:LOCALAPPDATA\Programs\Kern).

.PARAMETER AddToPath
  Add the install directory to the current user's PATH if missing.

.PARAMETER Force
  Overwrite kern.exe without prompting if it already exists in Prefix.

.EXAMPLE
  .\install.ps1
  .\install.ps1 -Prefix "C:\Tools\Kern" -AddToPath
#>
param(
    [string] $Prefix = (Join-Path $env:LOCALAPPDATA "Programs\Kern"),
    [switch] $AddToPath,
    [switch] $Force
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $Root "build"

Write-Host "==> CMake configure (Release)..."
& cmake -S $Root -B $BuildDir -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "==> Build target: kern..."
& cmake --build $BuildDir --config Release --target kern
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$BuiltExe = Join-Path $BuildDir "Release\kern.exe"
if (-not (Test-Path -LiteralPath $BuiltExe)) {
    $BuiltExe = Join-Path $BuildDir "kern.exe"
}
if (-not (Test-Path -LiteralPath $BuiltExe)) {
    $BuiltExe = Join-Path $BuildDir "Debug\kern.exe"
}
if (-not (Test-Path -LiteralPath $BuiltExe)) {
    Write-Error "Could not find built kern.exe under $BuildDir (try building Release)."
}

if (-not (Test-Path -LiteralPath $Prefix)) {
    New-Item -ItemType Directory -Path $Prefix | Out-Null
}

$DestExe = Join-Path $Prefix "kern.exe"
if ((Test-Path -LiteralPath $DestExe) -and -not $Force) {
    Write-Host "Destination exists: $DestExe"
    $ans = Read-Host "Overwrite? [y/N]"
    if ($ans -notmatch '^[yY]') {
        Write-Host "Skipped copy. Binary is still at: $BuiltExe"
        exit 0
    }
}

Copy-Item -LiteralPath $BuiltExe -Destination $DestExe -Force
Write-Host "Installed: $DestExe"

if ($AddToPath) {
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if ($null -eq $userPath) { $userPath = "" }
    $norm = $Prefix.TrimEnd('\')
    $parts = $userPath -split ';' | Where-Object { $_ -ne '' }
    $hit = $false
    foreach ($p in $parts) {
        if ($p.TrimEnd('\') -ieq $norm) { $hit = $true; break }
    }
    if (-not $hit) {
        $newPath = if ($userPath -eq "") { $Prefix } else { "$userPath;$Prefix" }
        [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
        Write-Host "Added to User PATH: $Prefix"
        Write-Host "Open a new terminal (or refresh environment) before running: kern --version"
    } else {
        Write-Host "User PATH already contains: $Prefix"
    }
} else {
    Write-Host "Tip: re-run with -AddToPath to append this folder to your User PATH, or add it manually."
}
