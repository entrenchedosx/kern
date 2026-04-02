# Build kern-ide.exe with PyInstaller and zip a portable bundle.
# Run from repo root:  .\Kern-IDE\packaging\build_portable_zip.ps1
# Or from Kern-IDE:     .\packaging\build_portable_zip.ps1
# Requires: pip install pyinstaller

$ErrorActionPreference = "Stop"
$Ide = Split-Path $PSScriptRoot -Parent
Set-Location $Ide

python -m pip install --quiet pyinstaller
python -m PyInstaller --noconfirm packaging\kern-ide.spec
if (-not (Test-Path "dist\kern-ide.exe")) {
    throw "dist\kern-ide.exe not produced"
}

$ver = (Get-Content "VERSION" -Raw).Trim()
$zipName = "kern-ide-windows-x64-portable-v$ver.zip"
$out = Join-Path $Ide $zipName
if (Test-Path $out) { Remove-Item $out -Force }

Compress-Archive -Path "dist\kern-ide.exe", "VERSION", "packaging\README_PORTABLE.txt" -DestinationPath $out -Force
Write-Host "OK: $out"
