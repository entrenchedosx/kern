#Requires -Version 5.1
<#
.SYNOPSIS
  Stage a BUILD-like tree from Release CMake outputs and compile the NSIS installer.
.DESCRIPTION
  Mirrors build.ps1 staging (bin, lib/kern, lib/kargo + npm, modules, examples, docs) then runs makensis.
  Used by .github/workflows/release.yml so GitHub Releases ship kern-windows-x64-v*-installer.exe alongside the zip.
.PARAMETER RepoRoot
  Repository root (default: parent of scripts/).
.PARAMETER BuildReleaseDir
  Directory containing kern.exe, kern-impl.exe, etc. (default: <RepoRoot>/build/Release).
#>
param(
    [string] $RepoRoot = "",
    [string] $BuildReleaseDir = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
}
if ([string]::IsNullOrWhiteSpace($BuildReleaseDir)) {
    $BuildReleaseDir = Join-Path $RepoRoot "build\Release"
}
$BuildReleaseDir = [System.IO.Path]::GetFullPath($BuildReleaseDir)

$verPath = Join-Path $RepoRoot "KERN_VERSION.txt"
if (-not (Test-Path -LiteralPath $verPath)) { throw "KERN_VERSION.txt not found at $verPath" }
$ver = (Get-Content -LiteralPath $verPath -Raw).Trim()

$stage = Join-Path $RepoRoot "nsis-staging-BUILD"
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }

$bin = Join-Path $stage "bin"
$libRoot = Join-Path $stage "lib"
$kernLibDst = Join-Path $libRoot "kern"
$kargoDst = Join-Path $libRoot "kargo"
$modules = Join-Path $stage "modules"
$examples = Join-Path $stage "examples"
$docs = Join-Path $stage "docs"

foreach ($d in @($bin, $libRoot, $kernLibDst, $kargoDst, $modules, $examples, $docs)) {
    New-Item -ItemType Directory -Path $d -Force | Out-Null
}

$kernExe = Join-Path $BuildReleaseDir "kern.exe"
$implExe = Join-Path $BuildReleaseDir "kern-impl.exe"
$scanExe = Join-Path $BuildReleaseDir "kern-scan.exe"
$humExe = Join-Path $BuildReleaseDir "kern_contract_humanize.exe"
foreach ($p in @($kernExe, $implExe, $scanExe, $humExe)) {
    if (-not (Test-Path -LiteralPath $p)) { throw "Missing build output: $p" }
}
Copy-Item -LiteralPath $kernExe -Destination $bin -Force
Copy-Item -LiteralPath $implExe -Destination (Join-Path $bin "kernc.exe") -Force
Copy-Item -LiteralPath $scanExe -Destination $bin -Force
Copy-Item -LiteralPath $humExe -Destination $bin -Force

$kargoSrc = Join-Path $RepoRoot "kargo"
if (-not (Test-Path -LiteralPath $kargoSrc)) { throw "kargo/ not found at $kargoSrc" }
Push-Location $kargoSrc
try {
    & npm ci --omit=dev
    if ($LASTEXITCODE -ne 0) { throw "npm ci --omit=dev failed in kargo (exit $LASTEXITCODE)" }
} finally {
    Pop-Location
}
Copy-Item -Path (Join-Path $kargoSrc "*") -Destination $kargoDst -Recurse -Force

$kernLibSrc = Join-Path $RepoRoot "lib\kern"
if (Test-Path -LiteralPath $kernLibSrc) {
    Copy-Item -Path (Join-Path $kernLibSrc "*") -Destination $kernLibDst -Recurse -Force
    Get-ChildItem -LiteralPath $kernLibSrc -File -Filter "*.kn" -ErrorAction SilentlyContinue |
        ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $modules -Force }
}

function Copy-Tree([string]$src, [string]$dst) {
    if (-not (Test-Path -LiteralPath $src)) { return }
    $null = robocopy $src $dst /E /NFL /NDL /NJH /NJS /NC /NS
    if ($LASTEXITCODE -ge 8) { throw "robocopy failed $src -> $dst (exit $LASTEXITCODE)" }
}

Copy-Tree (Join-Path $RepoRoot "examples") $examples
Copy-Tree (Join-Path $RepoRoot "docs") $docs

$kargoCmd = "@echo off`r`nnode `"%~dp0..\lib\kargo\cli\entry.js`" %*`r`n"
Set-Content -Path (Join-Path $bin "kargo.cmd") -Value $kargoCmd -Encoding ASCII -Force

$makensis = $null
$cmd = Get-Command makensis -ErrorAction SilentlyContinue | Select-Object -First 1
if ($cmd) { $makensis = $cmd.Source }
if (-not $makensis) {
    foreach ($c in @("${env:ProgramFiles(x86)}\NSIS\makensis.exe", "${env:ProgramFiles}\NSIS\makensis.exe")) {
        if (Test-Path -LiteralPath $c) { $makensis = $c; break }
    }
}
if (-not $makensis) { throw "makensis not found. Install NSIS (e.g. choco install nsis -y on CI)." }

$outName = "kern-windows-x64-v${ver}-installer.exe"
$outPath = Join-Path $RepoRoot $outName
$nsiPath = Join-Path $RepoRoot "installer.nsi"
if (-not (Test-Path -LiteralPath $nsiPath)) { throw "installer.nsi not found at $nsiPath" }

$stageSlash = ($stage -replace "\\", "/")
$outSlash = ($outPath -replace "\\", "/")
& $makensis "/DBUILD_ROOT=$stageSlash" "/DOUTPUT=$outSlash" $nsiPath
if ($LASTEXITCODE -ne 0) { throw "makensis failed (exit $LASTEXITCODE)" }
if (-not (Test-Path -LiteralPath $outPath)) { throw "Expected installer missing: $outPath" }

Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue

Write-Host "NSIS installer OK: $outPath"

if ($env:GITHUB_OUTPUT) {
    Add-Content -LiteralPath $env:GITHUB_OUTPUT -Value "installer_path=$outName"
}
