# Build script for integrating 3dengine into Kern
# Run in PowerShell with Visual Studio 2019/2022 installed

$ErrorActionPreference = "Stop"

Write-Host "===========================================" -ForegroundColor Cyan
Write-Host "3DENGINE BUILD SCRIPT (PowerShell)" -ForegroundColor Cyan
Write-Host "===========================================" -ForegroundColor Cyan
Write-Host ""

# Check for Visual Studio
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsWhere) {
    $vsPath = & $vsWhere -latest -property installationPath
    Write-Host "Found Visual Studio at: $vsPath" -ForegroundColor Green
} else {
    Write-Host "ERROR: Visual Studio not found!" -ForegroundColor Red
    Write-Host "Please install Visual Studio 2019 or 2022 with C++ workload"
    exit 1
}

# Import VS environment
$vcvarsPath = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
if (Test-Path $vcvarsPath) {
    Write-Host "Loading Visual Studio environment..." -ForegroundColor Yellow
    cmd /c "`"$vcvarsPath`" && set" | ForEach-Object {
        if ($_ -match "^(.*?)=(.*)$") {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2])
        }
    }
} else {
    Write-Host "ERROR: Could not find vcvars64.bat" -ForegroundColor Red
    exit 1
}

# Navigate to project
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $projectRoot
Write-Host "Working directory: $(Get-Location)" -ForegroundColor Gray

# Check source files
Write-Host ""
Write-Host "Checking source files..." -ForegroundColor Yellow
$requiredFiles = @(
    "kern\modules\3dengine\3dengine.cpp",
    "kern\modules\3dengine\3dengine.h",
    "kern\modules\3dengine\ursina_api.cpp",
    "kern\modules\3dengine\ursina_api.h",
    "src\stdlib_modules.cpp"
)

foreach ($file in $requiredFiles) {
    if (Test-Path $file) {
        Write-Host "  [OK] $file" -ForegroundColor Green
    } else {
        Write-Host "  [MISSING] $file" -ForegroundColor Red
        exit 1
    }
}

# Setup build directory
Write-Host ""
Write-Host "Setting up build directory..." -ForegroundColor Yellow
$buildDir = "build-3dengine-ps"
if (Test-Path $buildDir) {
    Remove-Item -Recurse -Force $buildDir
}
New-Item -ItemType Directory -Path $buildDir | Out-Null
Set-Location $buildDir

# Configure with CMake
Write-Host ""
Write-Host "===========================================" -ForegroundColor Cyan
Write-Host "Configuring with CMake..." -ForegroundColor Cyan
Write-Host "===========================================" -ForegroundColor Cyan

& cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DKERN_PREFER_STATIC_RAYLIB=ON

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: CMake configuration failed!" -ForegroundColor Red
    exit 1
}

# Build
Write-Host ""
Write-Host "===========================================" -ForegroundColor Cyan
Write-Host "Building with MSBuild..." -ForegroundColor Cyan
Write-Host "===========================================" -ForegroundColor Cyan

& cmake --build . --config Release --target kern --parallel 4

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Build failed!" -ForegroundColor Red
    exit 1
}

# Build additional targets
Write-Host ""
Write-Host "Building additional targets..." -ForegroundColor Yellow
& cmake --build . --config Release --target kern_repl --parallel 4
& cmake --build . --config Release --target kernc --parallel 4

# Verify
Write-Host ""
Write-Host "===========================================" -ForegroundColor Green
Write-Host "BUILD SUCCESSFUL!" -ForegroundColor Green
Write-Host "===========================================" -ForegroundColor Green
Write-Host ""

$kernExe = "Release\kern.exe"
if (Test-Path $kernExe) {
    Write-Host "Binary location: $(Resolve-Path $kernExe)" -ForegroundColor Green
    Write-Host ""
    
    # Quick test
    Write-Host "Testing 3dengine import..." -ForegroundColor Yellow
    $testResult = "let e = import(`"3dengine`"); print(e[`"VERSION`"])" | & $kernExe - 2>&1
    Write-Host "  Result: $testResult" -ForegroundColor Gray
    
    Write-Host ""
    Write-Host "Run full tests:" -ForegroundColor Cyan
    Write-Host "  .\Release\kern.exe ..\tests\3dengine_import.kn"
    Write-Host "  .\Release\kern.exe ..\tests\3dengine_ursina_style.kn"
} else {
    Write-Host "WARNING: Could not find $kernExe" -ForegroundColor Red
}

Write-Host ""
Write-Host "===========================================" -ForegroundColor Cyan
