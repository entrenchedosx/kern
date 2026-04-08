<#
.SYNOPSIS
    Kern automated build and packaging — BUILD directory, kern.exe, kernc.exe, installer.
.DESCRIPTION
    Cleans and recreates BUILD/, compiles the Kern toolchain, copies runtime files,
    and produces BUILD/installer/kern_installer.exe. Does not delete anything outside the project.
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
    [switch]$SkipInstaller,
    [switch]$NoGame
)

$ErrorActionPreference = "Stop"
$Root = [System.IO.Path]::GetFullPath($PSScriptRoot)
$BuildOut = [System.IO.Path]::GetFullPath((Join-Path $Root "BUILD"))
$BuildDir = [System.IO.Path]::GetFullPath((Join-Path $Root "build"))
$DefaultVcpkgToolchain = Join-Path $Root "tools\vcpkg\scripts\buildsystems\vcpkg.cmake"
$DefaultVcpkgExe = Join-Path $Root "tools\vcpkg\vcpkg.exe"

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

& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
Write-Host "  OK: Configured" -ForegroundColor Green
Write-Host ""

# step 2: Build Kern toolchain ----------
Write-Host "[2] Building Kern toolchain (Release)..." -ForegroundColor Yellow
Get-Process -Name kern,kern_repl,kern_game -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 300
Push-Location $BuildDir
try {
    & cmake --build . --config Release
    if ($LASTEXITCODE -ne 0) { throw "Kern compilation failed" }
} finally {
    Pop-Location
}
$kernExe = Join-Path $BuildDir "Release\kern.exe"
$kerncExe = Join-Path $BuildDir "Release\kernc.exe"
$kernImplExe = Join-Path $BuildDir "Release\kern-impl.exe"
if (-not (Test-Path $kernExe)) { throw "kern.exe not found after build at $kernExe" }
if (-not $NoGame) {
    $verOut = & $kernExe --version 2>&1 | Out-String
    if ($verOut -notmatch "graphics:\s+g2d\+g3d\+game\s+\(Raylib linked\)") {
        throw "Kern Release build is missing linked Raylib (expected `graphics: g2d+g3d+game (Raylib linked)` in kern --version). Use vcpkg raylib:x64-windows-static or -NoGame for headless."
    }
    Write-Host "  OK: kern --version reports Raylib (g2d/g3d/game)" -ForegroundColor Green
}
Copy-Item $kernExe $binDir -Force
# Portable layout: ship the real compiler as kernc.exe (Windows dev tree uses kern-impl + launcher).
if (Test-Path $kernImplExe) {
    Copy-Item $kernImplExe (Join-Path $binDir "kernc.exe") -Force
    Write-Host "  OK: kern.exe, kernc.exe (from kern-impl) -> BUILD\bin\" -ForegroundColor Green
} elseif (Test-Path $kerncExe) {
    Copy-Item $kerncExe $binDir -Force
    Write-Host "  OK: kern.exe, kernc.exe -> BUILD\bin\" -ForegroundColor Green
} else {
    Write-Host "  OK: kern.exe -> BUILD\bin\ (kernc not present)" -ForegroundColor Green
}
foreach ($tool in @("kern-scan.exe", "kern_game.exe", "kern_repl.exe", "kern_lsp.exe", "kern_contract_humanize.exe")) {
    $tp = Join-Path $BuildDir "Release\$tool"
    if (Test-Path $tp) {
        Copy-Item $tp $binDir -Force
        Write-Host "  OK: $tool -> BUILD\bin\" -ForegroundColor Gray
    }
}
$raylibDll = Join-Path $BuildDir "Release\raylib.dll"
if (Test-Path $raylibDll) {
    Copy-Item $raylibDll $binDir -Force
    Write-Host "  OK: raylib.dll -> BUILD\bin\" -ForegroundColor Green
}
Write-Host ""

# step 3: Copy modules (lib/kern) and lib for KERN_LIB ----------
Write-Host "[3] Copying modules and lib..." -ForegroundColor Yellow
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

$kargoSrc = Join-Path $Root "kargo"
$kargoDest = Join-Path $BuildOut "lib\kargo"
if (Test-Path $kargoSrc) {
    Write-Host "  Staging kargo for installer..." -ForegroundColor Gray
    New-Item -ItemType Directory -Force -Path (Join-Path $BuildOut "lib") | Out-Null
    if (Test-Path $kargoDest) { Remove-Item $kargoDest -Recurse -Force }
    Copy-Item $kargoSrc $kargoDest -Recurse -Force
    Push-Location $kargoDest
    try {
        npm install --omit=dev 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) { Write-Host "  WARN: npm install in BUILD\lib\kargo failed (exit $LASTEXITCODE); run manually before shipping" -ForegroundColor Yellow }
    } finally {
        Pop-Location
    }
    $kargoCmd = "@echo off`r`nnode `"%~dp0..\lib\kargo\cli\entry.js`" %*`r`n"
    Set-Content -Path (Join-Path $binDir "kargo.cmd") -Value $kargoCmd -Encoding ASCII -Force
    Write-Host "  OK: lib\kargo\, bin\kargo.cmd (NSIS / portable BUILD layout)" -ForegroundColor Green
}
Write-Host ""

# step 4: Copy examples and docs ----------
Write-Host "[4] Copying examples and docs..." -ForegroundColor Yellow
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

# step 5: Create installer (NSIS) ----------
if (-not $SkipInstaller) {
    Write-Host "[5] Building installer (NSIS)..." -ForegroundColor Yellow
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
    Write-Host "[5] Skipping installer (SkipInstaller)" -ForegroundColor Gray
    Write-Host ""
}

@"
Kern Build Output
================
  bin\kern.exe         - Kern interpreter / runner (Raylib linked unless -NoGame)
  bin\kernc.exe        - Standalone compiler (kern-impl on Windows)
  bin\kern-scan.exe, kern_game.exe, kern_repl.exe, kern_lsp.exe, kern_contract_humanize.exe - toolchain (when built)
  lib\kern\            - Standard library (set KERN_LIB to this BUILD folder for imports)
  modules\             - .kn module files
  examples\            - Example scripts
  docs\                - Documentation
  installer\           - kern_installer.exe (if NSIS was used)

Editors (desktop Tk, Qt, VS Code) live in the separate Kern-IDE repository.

Run from this folder (or set KERN_LIB to this folder):
  .\bin\kern.exe .\examples\basic\01_hello_world.kn
"@ | Set-Content (Join-Path $BuildOut "README.txt") -Encoding UTF8

Write-Host "=== Build Complete ===" -ForegroundColor Cyan
Write-Host "  BUILD\bin\kern.exe" -ForegroundColor Gray
Write-Host "  BUILD\modules\, BUILD\examples\, BUILD\docs\" -ForegroundColor Gray
if (-not $SkipInstaller -and (Test-Path (Join-Path $instDir "kern_installer.exe"))) {
    Write-Host "  BUILD\installer\kern_installer.exe" -ForegroundColor Gray
}
Write-Host ""
