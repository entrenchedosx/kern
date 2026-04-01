<#
.SYNOPSIS
    Copy freshly built kern.exe and kernc.exe into shareable-ide/compiler.
.DESCRIPTION
    Run after: cmake --build build --config Release --target kern kernc
    Prevents IDE bundles from lagging behind the parser/runtime (e.g. map literals, builtins).
.PARAMETER BuildDir
    CMake build directory (default: <repo>/build).
.PARAMETER DestDir
    Destination folder (default: <repo>/shareable-ide/compiler).
.PARAMETER IncludeLib
    Also mirror repo lib/ into compiler/lib and copy VERSION (full toolchain for IDE).
#>
[CmdletBinding()]
param(
    [string]$BuildDir = "",
    [string]$DestDir = "",
    [switch]$IncludeLib
)

$ErrorActionPreference = "Stop"
$Root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
if (-not $BuildDir) { $BuildDir = Join-Path $Root "build" }
if (-not $DestDir) { $DestDir = Join-Path $Root "shareable-ide\compiler" }

$Release = Join-Path $BuildDir "Release"
$Kern = Join-Path $Release "kern.exe"
$Kernc = Join-Path $Release "kernc.exe"

foreach ($f in @($Kern, $Kernc)) {
    if (-not (Test-Path -LiteralPath $f)) {
        Write-Error "Missing: $f - build Release kern and kernc first."
    }
}

New-Item -ItemType Directory -Force -Path $DestDir | Out-Null
Copy-Item -LiteralPath $Kern -Destination (Join-Path $DestDir "kern.exe") -Force
Copy-Item -LiteralPath $Kernc -Destination (Join-Path $DestDir "kernc.exe") -Force

Write-Host "Copied:" -ForegroundColor Green
Write-Host "  $Kern -> $(Join-Path $DestDir 'kern.exe')"
Write-Host "  $Kernc -> $(Join-Path $DestDir 'kernc.exe')"

try {
    $ver = & $Kern --version 2>&1
    Write-Host "kern --version: $ver" -ForegroundColor Gray
} catch {
    Write-Host "(Could not run kern --version)" -ForegroundColor DarkGray
}

if ($IncludeLib) {
    $LibSrc = Join-Path $Root "lib"
    $destLib = Join-Path $DestDir "lib"
    if (-not (Test-Path $LibSrc)) { Write-Error "Missing lib folder: $LibSrc" }
    if (Test-Path $destLib) { Remove-Item $destLib -Recurse -Force }
    Copy-Item $LibSrc $destLib -Recurse -Force
    $verSrc = Join-Path $Root "VERSION"
    if (Test-Path $verSrc) { Copy-Item $verSrc (Join-Path $DestDir "VERSION") -Force }
    $Repl = Join-Path $Release "kern_repl.exe"
    $Impl = Join-Path $Release "kern-impl.exe"
    $Game = Join-Path $Release "kern_game.exe"
    foreach ($pair in @(@($Repl, "kern_repl.exe"), @($Impl, "kern-impl.exe"), @($Game, "kern_game.exe"))) {
        if (Test-Path -LiteralPath $pair[0]) {
            Copy-Item -LiteralPath $pair[0] -Destination (Join-Path $DestDir $pair[1]) -Force
            Write-Host "Copied $($pair[1])" -ForegroundColor Gray
        }
    }
    Write-Host 'Synced lib/ and VERSION under compiler/' -ForegroundColor Green
}

