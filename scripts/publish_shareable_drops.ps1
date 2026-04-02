<#
.SYNOPSIS
    Publish shareable-kern-to-exe and refresh shareable-ide\compiler (optionally rebuild ide.exe).
.DESCRIPTION
    Default: publish shareable-kern-to-exe + sync shareable-ide\compiler (exes, lib, KERN_VERSION.txt).
    -FullIde: also run PyInstaller (build_shareable_ide.ps1) for a fresh ide.exe.
    -SkipIde: only shareable-kern-to-exe.
.PARAMETER SkipNative
    Skip CMake; use existing build\Release.
#>
[CmdletBinding()]
param(
    [switch]$SkipNative,
    [switch]$FullIde,
    [switch]$SkipIde,
    [switch]$NoGame
)

$ErrorActionPreference = "Stop"
$Root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$BuildDir = Join-Path $Root "build"
Set-Location -LiteralPath $Root

if (-not $SkipNative) {
    foreach ($t in @("kern", "kernc", "kern_repl")) {
        cmake --build $BuildDir --config Release --target $t 2>&1 | Write-Host
        if ($LASTEXITCODE -ne 0) { Write-Warning "Target $t build reported exit $LASTEXITCODE" }
    }
}

& (Join-Path $PSScriptRoot "publish_shareable_kern_to_exe.ps1")

if (-not $SkipIde) {
    if ($FullIde) {
        $ba = @{ SkipNative = $true }
        if ($NoGame) { $ba.NoGame = $true }
        & (Join-Path $Root "build_shareable_ide.ps1") @ba
    } else {
        & (Join-Path $PSScriptRoot "refresh_shareable_ide_compiler.ps1") -IncludeLib
    }
}

Write-Host ""
Write-Host "[publish] validating examples (kern --check, recursive)..." -ForegroundColor DarkGray
& (Join-Path $PSScriptRoot "check_examples.ps1")
if ($LASTEXITCODE -ne 0) { throw "check_examples failed; fix examples/ before publishing." }

Write-Host ""
Write-Host "shareable-ide:      $(Join-Path $Root 'shareable-ide')" -ForegroundColor Cyan
Write-Host "shareable-kern-to-exe: $(Join-Path $Root 'shareable-kern-to-exe')" -ForegroundColor Cyan

