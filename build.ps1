<#
.SYNOPSIS
    Kern automated build and packaging â€“ BUILD directory, kern.exe, kern-ide.exe, installer.
.DESCRIPTION
    Cleans and recreates BUILD/, compiles Kern interpreter and IDE, copies runtime files,
    and produces BUILD/installer/kern_installer.exe. Does not delete anything outside the project.
.PARAMETER SkipIDE
    Do not build or copy the IDE.
.PARAMETER SkipInstaller
    Do not build the NSIS installer (requires NSIS/makensis).
.PARAMETER NoGame
    Build Kern without Raylib (no g2d/game).
.EXAMPLE
    .\build.ps1
    Full build: BUILD/bin, modules, examples, docs, installer.
#>
[CmdletBinding()]
param(
    [switch]$SkipIDE,
    [switch]$SkipInstaller,
    [switch]$NoGame
)

$ErrorActionPreference = "Stop"
$Root = [System.IO.Path]::GetFullPath($PSScriptRoot)
$BuildOut = [System.IO.Path]::GetFullPath((Join-Path $Root "BUILD"))
$BuildDir = [System.IO.Path]::GetFullPath((Join-Path $Root "build"))
$DefaultVcpkgToolchain = Join-Path $Root "tools\vcpkg\scripts\buildsystems\vcpkg.cmake"
$DefaultVcpkgExe = Join-Path $Root "tools\vcpkg\vcpkg.exe"
$DefaultQtPrefix = Join-Path $Root "tools\Qt\6.6.3\msvc2019_64"
$NativeIdeExeCandidates = @(
    (Join-Path $BuildDir "Release\kern-ide.exe"),
    (Join-Path $BuildDir "kern\ide\qt-native\Release\kern-ide.exe")
)

# step 0: Clean BUILD only (project-local) ----------
Write-Host "=== Kern Build & Package ===" -ForegroundColor Cyan
Write-Host "[0] Cleaning BUILD (project-local only)..." -ForegroundColor Yellow
if (Test-Path $BuildOut) {
    Remove-Item $BuildOut -Recurse -Force
    Write-Host "  Removed existing BUILD\" -ForegroundColor Gray
}
New-Item -ItemType Directory -Force -Path $BuildOut | Out-Null
$binDir = Join-Path $BuildOut "bin"
$modDir = Join-Path $BuildOut "modules"
$exDir = Join-Path $BuildOut "examples"
$docDir = Join-Path $BuildOut "docs"
$instDir = Join-Path $BuildOut "installer"
foreach ($d in $binDir, $modDir, $exDir, $docDir, $instDir) {
    New-Item -ItemType Directory -Force -Path $d | Out-Null
}
Write-Host "  OK: BUILD\bin, modules, examples, docs, installer" -ForegroundColor Green
Write-Host ""

# step 1: CMake configure ----------
Write-Host "[1] Configuring CMake..." -ForegroundColor Yellow
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
$cmakeArgs = @("-B", $BuildDir, "-DCMAKE_BUILD_TYPE=Release")

if (-not $NoGame) {
    $cmakeArgs += "-DKERN_BUILD_GAME=ON"
    $cmakeArgs += "-DKERN_PREFER_STATIC_RAYLIB=ON"
    if ($env:CMAKE_TOOLCHAIN_FILE) {
        $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$env:CMAKE_TOOLCHAIN_FILE"
        if (-not $env:VCPKG_TARGET_TRIPLET) {
            $cmakeArgs += "-DVCPKG_TARGET_TRIPLET=x64-windows-static"
        }
    } elseif (Test-Path $DefaultVcpkgToolchain) {
        if (Test-Path $DefaultVcpkgExe) {
            Write-Host "  Installing raylib:x64-windows-static via local vcpkg..." -ForegroundColor Gray
            & $DefaultVcpkgExe install raylib:x64-windows-static 2>&1 | Out-Null
            if ($LASTEXITCODE -ne 0) {
                Write-Host "  WARN: vcpkg install raylib:x64-windows-static failed (exit $LASTEXITCODE)." -ForegroundColor Yellow
            }
        }
        $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$DefaultVcpkgToolchain"
        $cmakeArgs += "-DVCPKG_TARGET_TRIPLET=x64-windows-static"
        Write-Host "  Using local vcpkg toolchain: $DefaultVcpkgToolchain" -ForegroundColor Gray
    }
} else {
    $cmakeArgs += "-DKERN_BUILD_GAME=OFF"
}

if ($env:CMAKE_PREFIX_PATH) {
    $cmakeArgs += "-DCMAKE_PREFIX_PATH=$env:CMAKE_PREFIX_PATH"
} elseif (Test-Path $DefaultQtPrefix) {
    $cmakeArgs += "-DCMAKE_PREFIX_PATH=$DefaultQtPrefix"
    Write-Host "  Using local Qt prefix: $DefaultQtPrefix" -ForegroundColor Gray
}

& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
Write-Host "  OK: Configured" -ForegroundColor Green
Write-Host ""

# step 2: Build Kern toolchain ----------
Write-Host "[2] Building Kern toolchain (Release)..." -ForegroundColor Yellow
Get-Process -Name kern,kern_repl,kern_game,kern -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 300
Push-Location $BuildDir
try {
    & cmake --build . --config Release
    if ($LASTEXITCODE -ne 0) { throw "Kern compilation failed" }
} finally {
    Pop-Location
}
$kernExe = Join-Path $BuildDir "Release\kern.exe"
$kerncExe = Join-Path $BuildDir "Release\kern.exe"
if (-not (Test-Path $kernExe)) { throw "kern.exe not found after build at $kernExe" }
if (-not (Test-Path $kerncExe)) { throw "kern.exe not found after build at $kerncExe" }
Copy-Item $kernExe $binDir -Force
Copy-Item $kerncExe $binDir -Force
Write-Host "  OK: kern.exe, kern.exe -> BUILD\bin\" -ForegroundColor Green
$raylibDll = Join-Path $BuildDir "Release\raylib.dll"
if (Test-Path $raylibDll) {
    Copy-Item $raylibDll $binDir -Force
    Write-Host "  OK: raylib.dll -> BUILD\bin\" -ForegroundColor Green
}
Write-Host ""

# step 3: Build IDE (Native Qt) ----------
if (-not $SkipIDE) {
    Write-Host "[3] Building Kern IDE (Native Qt)..." -ForegroundColor Yellow
    $nativeIdeProjCandidates = @(
        (Join-Path $BuildDir "kern_ide_native.vcxproj"),
        (Join-Path $BuildDir "kern\ide\qt-native\kern_ide_native.vcxproj")
    )
    $nativeIdeProj = $nativeIdeProjCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (Test-Path $nativeIdeProj) {
        Push-Location $BuildDir
        try {
            & cmake --build . --config Release --target kern_ide_native
            if ($LASTEXITCODE -ne 0) { Write-Host "  WARN: Native IDE build failed." -ForegroundColor Yellow }
        } finally {
            Pop-Location
        }
    } else {
        Write-Host "  WARN: Native IDE project not generated (Qt6 likely missing)." -ForegroundColor Yellow
    }
    $nativeIdeExe = $NativeIdeExeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($nativeIdeExe -and (Test-Path $nativeIdeExe)) {
        Copy-Item $nativeIdeExe $binDir -Force
        Write-Host "  OK: kern-ide.exe -> BUILD\bin\" -ForegroundColor Green
    } else {
        Write-Host "  WARN: kern-ide.exe not found after build. Install Qt6 and re-run build." -ForegroundColor Yellow
    }
    Write-Host ""
} else {
    Write-Host "[3] Skipping IDE (SkipIDE)" -ForegroundColor Gray
    Write-Host ""
}

# step 4: Copy modules (lib/kern) and lib for KERN_LIB ----------
Write-Host "[4] Copying modules and lib..." -ForegroundColor Yellow
$libSpl = Join-Path $Root "lib\kern"
$buildLib = Join-Path $BuildOut "lib\kern"
if (Test-Path $libSpl) {
    New-Item -ItemType Directory -Force -Path (Join-Path $BuildOut "lib") | Out-Null
    if (Test-Path $buildLib) { Remove-Item $buildLib -Recurse -Force }
    Copy-Item $libSpl $buildLib -Recurse -Force
    Write-Host "  OK: lib\kern\ -> BUILD\lib\kern\" -ForegroundColor Green
}
if (Test-Path $libSpl) {
    Get-ChildItem -Path $libSpl -File -Filter "*.kn" | ForEach-Object { Copy-Item $_.FullName $modDir -Force }
    Write-Host "  OK: modules\ (*.kn)" -ForegroundColor Green
}
$canonicalModules = Join-Path $Root "kern\modules"
if (Test-Path $canonicalModules) {
    Get-ChildItem -Path $canonicalModules -Recurse -File -Filter "*.kn" | ForEach-Object {
        $rel = $_.FullName.Substring($canonicalModules.Length).TrimStart("\")
        $dest = Join-Path $modDir $rel
        $destDir = [System.IO.Path]::GetDirectoryName($dest)
        if (-not (Test-Path $destDir)) { New-Item -ItemType Directory -Force -Path $destDir | Out-Null }
        Copy-Item $_.FullName $dest -Force
    }
    Write-Host "  OK: canonical modules from kern\modules\" -ForegroundColor Green
}
Write-Host ""

# step 5: Copy examples and docs ----------
Write-Host "[5] Copying examples and docs..." -ForegroundColor Yellow
$examplesSrc = Join-Path $Root "examples"
$docsSrc = Join-Path $Root "docs"
if (Test-Path $examplesSrc) {
    Get-ChildItem -Path $examplesSrc -Recurse -File | ForEach-Object {
        $rel = $_.FullName.Substring($examplesSrc.Length).TrimStart("\")
        $dest = Join-Path $exDir $rel
        $destDir = [System.IO.Path]::GetDirectoryName($dest)
        if (-not (Test-Path $destDir)) { New-Item -ItemType Directory -Force -Path $destDir | Out-Null }
        Copy-Item $_.FullName $dest -Force
    }
    Write-Host "  OK: examples\" -ForegroundColor Green
}
if (Test-Path $docsSrc) {
    Get-ChildItem -Path $docsSrc -Recurse -File | ForEach-Object {
        $rel = $_.FullName.Substring($docsSrc.Length).TrimStart("\")
        $dest = Join-Path $docDir $rel
        $destDir = [System.IO.Path]::GetDirectoryName($dest)
        if (-not (Test-Path $destDir)) { New-Item -ItemType Directory -Force -Path $destDir | Out-Null }
        Copy-Item $_.FullName $dest -Force
    }
    Write-Host "  OK: docs\" -ForegroundColor Green
}
Write-Host ""

# step 6: Create installer (NSIS) ----------
if (-not $SkipInstaller) {
    Write-Host "[6] Building installer (NSIS)..." -ForegroundColor Yellow
    $nsiPath = Join-Path $Root "installer.nsi"
    if (-not (Test-Path $nsiPath)) {
        Write-Host "  WARN: installer.nsi not found; skipping installer" -ForegroundColor Yellow
    } else {
        $makensis = (Get-Command makensis -ErrorAction SilentlyContinue).Source
        if (-not $makensis) {
            $makensis = "${env:ProgramFiles(x86)}\NSIS\makensis.exe"
            if (-not (Test-Path $makensis)) { $makensis = "$env:ProgramFiles\NSIS\makensis.exe" }
        }
        if (Test-Path $makensis) {
            $outPath = (Join-Path $instDir "kern_installer.exe") -replace '\\', '/'
            & $makensis "/DBUILD_ROOT=$($BuildOut -replace '\\','/')" "/DOUTPUT=$outPath" $nsiPath
            if ($LASTEXITCODE -ne 0) { Write-Host "  WARN: makensis failed" -ForegroundColor Yellow }
            elseif (Test-Path (Join-Path $instDir "kern_installer.exe")) {
                Write-Host "  OK: BUILD\installer\kern_installer.exe" -ForegroundColor Green
            }
        } else {
            Write-Host "  WARN: NSIS (makensis) not found. Install NSIS or use -SkipInstaller" -ForegroundColor Yellow
        }
    }
    Write-Host ""
} else {
    Write-Host "[6] Skipping installer (SkipInstaller)" -ForegroundColor Gray
    Write-Host ""
}

# rEADME in BUILD ----------
@"
Kern Build Output
================
  bin\kern.exe         - Kern interpreter
  bin\kern.exe        - Kern standalone compiler
  bin\kern-ide.exe    - Kern IDE (if built)
  lib\kern\           - Standard library (set KERN_LIB to this BUILD folder for imports)
  modules\           - .kn module files
  examples\          - Example scripts
  docs\              - Documentation
  installer\         - kern_installer.exe (if NSIS was used)

Run from this folder (or set KERN_LIB to this folder):
  .\bin\kern.exe .\examples\basic\01_hello_world.kn
  .\bin\kern.exe .\examples\basic\05_functions.kn -o .\examples\dist\functions.exe
"@ | Set-Content (Join-Path $BuildOut "README.txt") -Encoding UTF8

Write-Host "=== Build Complete ===" -ForegroundColor Cyan
Write-Host "  BUILD\bin\kern.exe" -ForegroundColor Gray
Write-Host "  BUILD\bin\kern.exe" -ForegroundColor Gray
if (-not $SkipIDE -and (Test-Path (Join-Path $binDir "kern-ide.exe"))) { Write-Host "  BUILD\bin\kern-ide.exe" -ForegroundColor Gray }
Write-Host "  BUILD\modules\, BUILD\examples\, BUILD\docs\" -ForegroundColor Gray
if (-not $SkipInstaller -and (Test-Path (Join-Path $instDir "kern_installer.exe"))) {
    Write-Host "  BUILD\installer\kern_installer.exe" -ForegroundColor Gray
}
Write-Host ""

