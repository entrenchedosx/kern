# Building Kern v2.0.2 Binary

## Quick Build Command

The binary is being built with:

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1 -SkipInstaller
```

This will create: `build/Release/kern.exe`

## Manual Build (Alternative)

If the PowerShell script fails, build manually:

```powershell
# 1. Create build directory
mkdir build
cd build

# 2. Configure with CMake
cmake -B . -S .. -DCMAKE_BUILD_TYPE=Release

# 3. Build
cmake --build . --config Release

# 4. Verify
./Release/kern.exe --version
```

## Expected Output

```
Kern v2.0.2
graphics: g2d+g3d+game (Raylib linked)
```

## Binary Location

After successful build:
- **Main binary:** `d:\simple_programming_language\build\Release\kern.exe`
- **Compiler:** `d:\simple_programming_language\build\Release\kernc.exe`

## Test the Binary

```powershell
# Test language
.\build\Release\kern.exe examples\language\01_variables_and_types.kn

# Test graphics
.\build\Release\kern.exe examples\graphics\clean_ursina_test.kn

# Check version
.\build\Release\kern.exe --version
```

## Build Status

⏳ Build in progress... This typically takes 2-5 minutes.

## Troubleshooting

**If Raylib linking fails:**
```powershell
# Build without graphics (headless mode)
cmake -B . -S .. -DCMAKE_BUILD_TYPE=Release -DKERN_BUILD_GAME=OFF
cmake --build . --config Release
```

**If CMake is not found:**
- Install CMake from https://cmake.org/download/
- Or use Visual Studio's built-in CMake

**If Visual Studio is not found:**
- Install Visual Studio 2022 with "Desktop development with C++" workload
- Or use MinGW/Clang if preferred
