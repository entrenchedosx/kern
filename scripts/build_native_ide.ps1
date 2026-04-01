# requires -Version 5.1
<#
.SYNOPSIS
  Configure and build the native Qt Kern IDE (kern-ide) using the repo's bundled vcpkg + qtbase.

.DESCRIPTION
  - Bootstraps tools\vcpkg if vcpkg.exe is missing.
  - Configures build-ide with manifest feature "native-ide" (adds Qt6 via vcpkg).
  - Uses triplet x64-windows (dynamic Qt, /MD) so it matches KERN_PREFER_STATIC_RAYLIB=OFF.
  Uses a minimal qtbase (widgets only, no OpenSSL/ICU/SQL defaults) so vcpkg does not download Perl or build OpenSSL.

  First configure still compiles Qt and can take 15-45+ minutes with little output while MSVC runs; that is normal.

.PARAMETER BuildDir
  CMake binary directory (default: <repo>\build-native-ide; separate from build-ide to avoid cache clashes)

.PARAMETER Config
  MSBuild configuration (default: Release)

.PARAMETER Parallel
  Parallel jobs for MSBuild (default: number of logical processors)

.PARAMETER Clean
  Delete the build directory before configuring (use after upgrading vcpkg.json or if vcpkg state looks wrong).
#>
param(
    [string] $BuildDir = "",
    [ValidateSet("Release", "Debug")]
    [string] $Config = "Release",
    [int] $Parallel = 0,
    [switch] $Clean
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$CMakeLists = Join-Path $RepoRoot "CMakeLists.txt"
if (-not (Test-Path $CMakeLists)) {
    Write-Error "Expected CMakeLists.txt at repo root: $CMakeLists"
}

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $RepoRoot "build-native-ide"
}

if ($Clean) {
    Write-Host "Removing build directory: $BuildDir"
    Remove-Item -LiteralPath $BuildDir -Recurse -Force -ErrorAction SilentlyContinue
}

$VcpkgRoot = Join-Path $RepoRoot "tools\vcpkg"
$VcpkgExe = Join-Path $VcpkgRoot "vcpkg.exe"
$Bootstrap = Join-Path $VcpkgRoot "bootstrap-vcpkg.bat"
if (-not (Test-Path $VcpkgExe)) {
    if (-not (Test-Path $Bootstrap)) {
        Write-Error "Bundled vcpkg not found. Expected: $VcpkgRoot"
    }
    Write-Host "Bootstrapping vcpkg..."
    Push-Location $VcpkgRoot
    try {
        cmd /c "call bootstrap-vcpkg.bat"
        if (-not (Test-Path $VcpkgExe)) { Write-Error "bootstrap-vcpkg.bat did not produce vcpkg.exe" }
    } finally {
        Pop-Location
    }
}

$Toolchain = Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"
if (-not (Test-Path $Toolchain)) {
    Write-Error "Missing vcpkg toolchain: $Toolchain"
}

if ($Parallel -le 0) {
    $Parallel = [Environment]::ProcessorCount
    if ($Parallel -lt 1) { $Parallel = 4 }
}

Write-Host ""
Write-Host "Native IDE build: vcpkg installs a minimal Qt6 (widgets). No OpenSSL/Perl step."
Write-Host "If CMake sits silent for many minutes, vcpkg is usually compiling Qt or deps in the background."
Write-Host "  Source:  $RepoRoot"
Write-Host "  Build:   $BuildDir"
Write-Host ""

$cmakeArgs = @(
    "-S", $RepoRoot,
    "-B", $BuildDir,
    "-G", "Visual Studio 17 2022",
    "-A", "x64",
    "-DCMAKE_TOOLCHAIN_FILE=$Toolchain",
    "-DVCPKG_TARGET_TRIPLET=x64-windows",
    "-DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON",
    "-DVCPKG_MANIFEST_FEATURES=native-ide",
    "-DKERN_BUILD_GAME=OFF",
    "-DKERN_BUILD_NATIVE_IDE=ON",
    "-DKERN_PREFER_STATIC_RAYLIB=OFF"
)
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "Building kern_ide_native ($Config)..."
& cmake --build $BuildDir --config $Config --target kern_ide_native --parallel $Parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$OutExe = Join-Path $BuildDir "$Config\kern-ide.exe"
if (-not (Test-Path $OutExe)) {
    $OutExe = Join-Path $BuildDir "Release\kern-ide.exe"
}
Write-Host ""
Write-Host "Done. If successful, run: $OutExe"
exit 0

