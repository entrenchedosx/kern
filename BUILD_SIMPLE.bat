@echo off
echo ===========================================
echo 3DENGINE BUILD - SIMPLE MODE
echo ===========================================
echo.
echo Make sure you are running from:
echo x64 Native Tools Command Prompt for VS 2022
echo.

REM Check VS environment
if "%VSCMD_VER%"=="" (
    echo ERROR: Not in Visual Studio Command Prompt
echo Run this from: Start Menu -^> VS 2022 -^> x64 Native Tools Command Prompt
echo.
    pause
    exit /b 1
)

echo VS Version: %VSCMD_VER%
echo.

REM Go to project directory
cd /d "%~dp0"
echo Project: %CD%

REM Check source files exist
echo.
echo Checking source files...
if not exist "kern\modules\3dengine\3dengine.cpp" (
    echo ERROR: 3dengine.cpp not found
    exit /b 1
)
if not exist "kern\modules\3dengine\ursina_api.cpp" (
    echo ERROR: ursina_api.cpp not found
    exit /b 1
)
echo OK: Source files found
echo.

REM Create build directory
echo Creating build directory...
if exist "build-3dengine-new" rmdir /s /q "build-3dengine-new"
mkdir "build-3dengine-new"
cd "build-3dengine-new"

REM Run CMake
echo.
echo ===========================================
echo Running CMake...
echo ===========================================
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release

if errorlevel 1 (
    echo.
    echo ERROR: CMake failed
    pause
    exit /b 1
)

REM Build
echo.
echo ===========================================
echo Building kern.exe...
echo ===========================================
cmake --build . --config Release --target kern

if errorlevel 1 (
    echo.
    echo ERROR: Build failed
    pause
    exit /b 1
)

echo.
echo ===========================================
echo BUILD SUCCESS
echo ===========================================
echo.
echo Binary location:
echo   %CD%\Release\kern.exe
echo.
echo Test with:
echo   Release\kern.exe -e "print(import(\"3dengine\")[\"VERSION\"])"
echo.
pause
