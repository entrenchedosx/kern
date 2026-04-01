<#
.SYNOPSIS
    Produce a clean shareable-kern-to-exe folder (Python app + kernc.exe, no cache/garbage).
.DESCRIPTION
    Writes to <repo>\shareable-kern-to-exe:
      kern_to_exe\   (package .py only)
      kern-to-exe.bat, README.md, VERSION, kernc.exe (from build\Release)
    Omits __pycache__, .pyc, .kern-cache, dist artifacts.
.PARAMETER BuildDir
    CMake build root (default: <repo>\build).
#>
[CmdletBinding()]
param(
    [string]$BuildDir = ""
)

$ErrorActionPreference = "Stop"
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
if (-not $BuildDir) { $BuildDir = Join-Path $RepoRoot "build" }

$Release = Join-Path $BuildDir "Release"
$KerncSrc = Join-Path $Release "kernc.exe"
$KernSrc = Join-Path $Release "kern.exe"
if (-not (Test-Path -LiteralPath $KerncSrc)) {
    Write-Error "Missing $KerncSrc - build Release kernc first (e.g. cmake --build build --config Release --target kernc)."
}

$SrcPkg = Join-Path $RepoRoot "kern-to-exe\kern_to_exe"
$SrcBat = Join-Path $RepoRoot "kern-to-exe\kern-to-exe.bat"
$SrcReadme = Join-Path $RepoRoot "kern-to-exe\README.md"
$VerSrc = Join-Path $RepoRoot "VERSION"
if (-not (Test-Path $SrcPkg)) { Write-Error "Missing package folder: $SrcPkg" }

$Dest = Join-Path $RepoRoot "shareable-kern-to-exe"
if (Test-Path $Dest) { Remove-Item $Dest -Recurse -Force }
New-Item -ItemType Directory -Force -Path $Dest | Out-Null

$DestPkg = Join-Path $Dest "kern_to_exe"
New-Item -ItemType Directory -Force -Path $DestPkg | Out-Null

Get-ChildItem -LiteralPath $SrcPkg -Recurse -File | ForEach-Object {
    $rel = $_.FullName.Substring($SrcPkg.Length).TrimStart('\', '/')
    if ($rel -match '\\__pycache__\\' -or $rel -match '^__pycache__\\') { return }
    if ($_.Extension -eq '.pyc') { return }
    $target = Join-Path $DestPkg $rel
    $td = Split-Path $target -Parent
    if (-not (Test-Path $td)) { New-Item -ItemType Directory -Force -Path $td | Out-Null }
    Copy-Item -LiteralPath $_.FullName -Destination $target -Force
}

Copy-Item -LiteralPath $SrcBat -Destination (Join-Path $Dest "kern-to-exe.bat") -Force
if (Test-Path $SrcReadme) { Copy-Item -LiteralPath $SrcReadme -Destination (Join-Path $Dest "README.md") -Force }
if (Test-Path $VerSrc) { Copy-Item -LiteralPath $VerSrc -Destination (Join-Path $Dest "VERSION") -Force }

Copy-Item -LiteralPath $KerncSrc -Destination (Join-Path $Dest "kernc.exe") -Force
if (Test-Path $KernSrc) {
    Copy-Item -LiteralPath $KernSrc -Destination (Join-Path $Dest "kern.exe") -Force
}

@'
# optional: don't commit local Python cache if you version this folder
__pycache__/
*.pyc
.kern-cache/
'@ | Set-Content -LiteralPath (Join-Path $Dest ".gitignore") -Encoding utf8

$start = @'
Kern to EXE packager (shareable drop)
=====================================
Double-click kern-to-exe.bat or run:
  kern-to-exe.bat
  kern-to-exe.bat --recipe your.kern2exe.json

kernc.exe sits in this folder. Override with: set KERNC_EXE=path\to\kernc.exe
Requires Python 3.10+ with tkinter (Windows installer: include tcl/tk).

Version: see file VERSION
'@
$start | Set-Content -LiteralPath (Join-Path $Dest "START_HERE.txt") -Encoding utf8

Write-Host "=== shareable-kern-to-exe ready ===" -ForegroundColor Green
Write-Host $Dest
& (Join-Path $Release "kern.exe") --version 2>&1 | ForEach-Object { Write-Host "  kern: $_" -ForegroundColor Gray }

