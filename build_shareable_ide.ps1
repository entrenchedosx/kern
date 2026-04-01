<#
.SYNOPSIS
    Build native Kern tools, PyInstaller IDE as shareable-ide\ide.exe, and copy toolchain into shareable-ide\compiler\.
.DESCRIPTION
    Output layout (repo root):
      shareable-ide\ide.exe
      shareable-ide\_internal\   (PyInstaller)
      shareable-ide\compiler\    kern.exe, kern_repl.exe, kernc.exe, kern-impl.exe, lib\, VERSION
.PARAMETER SkipNative
    Do not run CMake build; use existing build\Release\*.exe.
.PARAMETER NoGame
    Pass through to ecosystem configure if native build runs (default: use build\CMakeCache or quick cmake).
#>
[CmdletBinding()]
param(
    [switch]$SkipNative,
    [switch]$NoGame
)

$ErrorActionPreference = "Stop"
$RepoRoot = [System.IO.Path]::GetFullPath($PSScriptRoot)
$BuildDir = Join-Path $RepoRoot "build"
$Config = "Release"
$OutDir = Join-Path $BuildDir $Config
$DestRoot = Join-Path $RepoRoot "shareable-ide"
$CompilerDir = Join-Path $DestRoot "compiler"
$LibSrc = Join-Path $RepoRoot "lib"

Set-Location -LiteralPath $RepoRoot

if (-not $SkipNative) {
    $eco = Join-Path $RepoRoot "build\build_ecosystem.ps1"
    if (-not (Test-Path $eco)) { throw "Missing $eco" }
    $ecoArgs = @{
        Configuration = $Config
        NoPackage     = $true
        SkipGui       = $true
        SkipIde       = $true
        SkipTests     = $true
        Quick         = $true
    }
    if ($NoGame) { $ecoArgs.NoGame = $true }
    & $eco @ecoArgs
    if ($LASTEXITCODE -ne 0) { throw "Native build failed" }
}

$kernExe = Join-Path $OutDir "kern.exe"
$replExe = Join-Path $OutDir "kern_repl.exe"
$kerncExe = Join-Path $OutDir "kernc.exe"
$kernImplExe = Join-Path $OutDir "kern-impl.exe"
foreach ($req in @($kernExe, $replExe, $kerncExe)) {
    if (-not (Test-Path $req)) { throw "Required binary missing: $req (build Release first or omit -SkipNative)" }
}

$py = (Get-Command python -ErrorAction SilentlyContinue).Source
if (-not $py) { $py = "python" }
& $py -m pip install -q pyinstaller
if ($LASTEXITCODE -ne 0) { throw "pip install pyinstaller failed" }

$ideRoot = Join-Path $RepoRoot "kern-ide"
$spec = Join-Path $ideRoot "packaging\shareable-ide.spec"
if (-not (Test-Path $spec)) { throw "Missing $spec" }
$workPath = Join-Path $BuildDir "pyinstaller-shareable-ide"
Push-Location $ideRoot
try {
    & $py -m PyInstaller --noconfirm --distpath $RepoRoot --workpath $workPath $spec
    if ($LASTEXITCODE -ne 0) { throw "PyInstaller failed" }
} finally {
    Pop-Location
}

if (-not (Test-Path (Join-Path $DestRoot "ide.exe"))) { throw "Expected $DestRoot\ide.exe" }

New-Item -ItemType Directory -Force -Path $CompilerDir | Out-Null
Copy-Item $kernExe (Join-Path $CompilerDir "kern.exe") -Force
Copy-Item $replExe (Join-Path $CompilerDir "kern_repl.exe") -Force
Copy-Item $kerncExe (Join-Path $CompilerDir "kernc.exe") -Force
if (Test-Path $kernImplExe) {
    Copy-Item $kernImplExe (Join-Path $CompilerDir "kern-impl.exe") -Force
}
$gameExe = Join-Path $OutDir "kern_game.exe"
if (Test-Path $gameExe) {
    Copy-Item $gameExe (Join-Path $CompilerDir "kern_game.exe") -Force
}

$destLib = Join-Path $CompilerDir "lib"
if (Test-Path $destLib) { Remove-Item $destLib -Recurse -Force }
if (-not (Test-Path $LibSrc)) { throw "Missing lib folder: $LibSrc" }
Copy-Item $LibSrc $destLib -Recurse -Force

$verSrc = Join-Path $RepoRoot "shareable\VERSION"
if (-not (Test-Path $verSrc)) { $verSrc = Join-Path $RepoRoot "VERSION" }
if (Test-Path $verSrc) {
    Copy-Item $verSrc (Join-Path $CompilerDir "VERSION") -Force
}

# drop IDE runtime state so the bundle is clean for redistribution
Get-ChildItem -LiteralPath $DestRoot -Filter ".kern-*.json" -File -ErrorAction SilentlyContinue |
    Remove-Item -Force -ErrorAction SilentlyContinue

Write-Output ""
Write-Output "=== shareable-ide ready ==="
Write-Output "  $DestRoot\ide.exe"
Write-Output "  $CompilerDir\ (kern + lib)"
exit 0

