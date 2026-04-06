#Requires -Version 5.0
<#
.SYNOPSIS
  Build and install Kern with user/global/portable modes.

.PARAMETER Mode
  Install mode: User, Global, Portable (default: User).

.PARAMETER Prefix
  Optional install root override. If omitted, defaults by mode.

.PARAMETER AddToPath
  Force PATH update (default: on for User/Global, off for Portable).

.PARAMETER NoPath
  Disable PATH updates.

.PARAMETER Force
  Overwrite existing installed files without prompt.
#>
param(
    [ValidateSet("User", "Global", "Portable")]
    [string] $Mode = "User",
    [string] $Prefix,
    [switch] $AddToPath,
    [switch] $NoPath,
    [switch] $Force
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $Root "build"
$VersionFile = Join-Path $Root "KERN_VERSION.txt"
$TargetVersion = "unknown"
if (Test-Path -LiteralPath $VersionFile) {
    $TargetVersion = (Get-Content -LiteralPath $VersionFile -Raw).Trim()
}

function Get-KernVersion([string]$text) {
    if ($null -eq $text) { return "" }
    $m = [regex]::Match($text, '(\d+\.\d+\.\d+)')
    if ($m.Success) { return $m.Groups[1].Value }
    return ""
}

function Compare-SemVer([string]$a, [string]$b) {
    if ([string]::IsNullOrWhiteSpace($a) -or [string]::IsNullOrWhiteSpace($b)) { return 0 }
    $aa = $a.Split('.') | ForEach-Object { [int]$_ }
    $bb = $b.Split('.') | ForEach-Object { [int]$_ }
    for ($i = 0; $i -lt 3; $i++) {
        if ($aa[$i] -lt $bb[$i]) { return -1 }
        if ($aa[$i] -gt $bb[$i]) { return 1 }
    }
    return 0
}

function Test-IsAdmin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($id)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if (-not $PSBoundParameters.ContainsKey("Prefix")) {
    if ($Mode -eq "Global") { $Prefix = Join-Path $env:ProgramFiles "Kern" }
    elseif ($Mode -eq "Portable") { $Prefix = Join-Path $Root ".kern-portable" }
    else { $Prefix = Join-Path $env:LOCALAPPDATA "Programs\Kern" }
}

if ($Mode -eq "Global" -and -not (Test-IsAdmin)) {
    Write-Error "Global install requires Administrator PowerShell. Re-run as admin or use -Mode User."
}

$ShouldAddPath = $false
if (-not $NoPath) {
    if ($AddToPath) { $ShouldAddPath = $true }
    elseif ($Mode -ne "Portable") { $ShouldAddPath = $true }
}

$BinDir = Join-Path $Prefix "bin"
$LibDir = Join-Path $Prefix "lib\kern"
$VersionRoot = Join-Path $Prefix ("versions\" + $TargetVersion)
$VersionBinDir = Join-Path $VersionRoot "bin"
$VersionLibDir = Join-Path $VersionRoot "lib\kern"

Write-Host "==> Kern installer"
Write-Host ("    Mode: " + $Mode)
Write-Host ("    Prefix: " + $Prefix)
Write-Host ("    Target version: " + $TargetVersion)

$existing = Get-Command kern -ErrorAction SilentlyContinue | Select-Object -First 1
if ($existing -ne $null) {
    $existingPath = $existing.Source
    $existingRaw = ""
    try { $existingRaw = (& $existingPath --version 2>$null | Out-String) } catch { $existingRaw = "" }
    $existingVersion = Get-KernVersion($existingRaw)
    Write-Host ("==> Detected existing kern: " + $existingPath)
    if ($existingVersion -ne "") {
        Write-Host ("    Installed version: " + $existingVersion)
        $cmp = Compare-SemVer $existingVersion $TargetVersion
        if ($cmp -lt 0) { Write-Host "    Recommendation: upgrade (newer installer version detected)." }
        elseif ($cmp -gt 0) { Write-Host "    Recommendation: installed version is newer; this install will downgrade." }
        else { Write-Host "    Recommendation: same version detected; reinstall only if needed." }
    } else {
        Write-Host "    Recommendation: unable to parse installed version; proceeding with install/update."
    }
}

Write-Host "==> CMake configure (Release)..."
& cmake -S $Root -B $BuildDir -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "==> Build target: kern..."
& cmake --build $BuildDir --config Release --target kern
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$BuiltExe = Join-Path $BuildDir "Release\kern.exe"
if (-not (Test-Path -LiteralPath $BuiltExe)) { $BuiltExe = Join-Path $BuildDir "kern.exe" }
if (-not (Test-Path -LiteralPath $BuiltExe)) { $BuiltExe = Join-Path $BuildDir "Debug\kern.exe" }
if (-not (Test-Path -LiteralPath $BuiltExe)) { Write-Error "Could not find built kern.exe under $BuildDir." }

if (-not (Test-Path -LiteralPath $BinDir)) { New-Item -ItemType Directory -Path $BinDir -Force | Out-Null }
if (-not (Test-Path -LiteralPath $VersionBinDir)) { New-Item -ItemType Directory -Path $VersionBinDir -Force | Out-Null }
if (-not (Test-Path -LiteralPath (Join-Path $Prefix "packages"))) { New-Item -ItemType Directory -Path (Join-Path $Prefix "packages") -Force | Out-Null }
if (-not (Test-Path -LiteralPath (Join-Path $Prefix "config"))) { New-Item -ItemType Directory -Path (Join-Path $Prefix "config") -Force | Out-Null }
if (-not (Test-Path -LiteralPath (Join-Path $Prefix "cache"))) { New-Item -ItemType Directory -Path (Join-Path $Prefix "cache") -Force | Out-Null }

$ActiveExe = Join-Path $BinDir "kern.exe"
if ((Test-Path -LiteralPath $ActiveExe) -and -not $Force) {
    $ans = Read-Host ("Active binary exists at " + $ActiveExe + ". Overwrite? [y/N]")
    if ($ans -notmatch '^[yY]') { Write-Host "Install cancelled by user."; exit 0 }
}

Copy-Item -LiteralPath $BuiltExe -Destination (Join-Path $VersionBinDir "kern.exe") -Force
Copy-Item -LiteralPath $BuiltExe -Destination $ActiveExe -Force

$StdSrc = Join-Path $Root "lib\kern"
if (Test-Path -LiteralPath $StdSrc) {
    New-Item -ItemType Directory -Path $LibDir -Force | Out-Null
    New-Item -ItemType Directory -Path $VersionLibDir -Force | Out-Null
    Copy-Item -Path (Join-Path $StdSrc "*") -Destination $LibDir -Recurse -Force
    Copy-Item -Path (Join-Path $StdSrc "*") -Destination $VersionLibDir -Recurse -Force
}

$sha = (Get-FileHash -Algorithm SHA256 -LiteralPath $ActiveExe).Hash.ToLower()
Write-Host ("Installed binary: " + $ActiveExe)
Write-Host ("SHA256: " + $sha)

if ($ShouldAddPath) {
    if ($Mode -eq "Global") {
        $machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
        if ($null -eq $machinePath) { $machinePath = "" }
        $parts = $machinePath -split ';' | Where-Object { $_ -ne '' }
        $hit = $false
        foreach ($p in $parts) { if ($p.TrimEnd('\') -ieq $BinDir.TrimEnd('\')) { $hit = $true; break } }
        if (-not $hit) {
            $newPath = if ($machinePath -eq "") { $BinDir } else { "$machinePath;$BinDir" }
            [Environment]::SetEnvironmentVariable("Path", $newPath, "Machine")
            Write-Host ("Added to Machine PATH: " + $BinDir)
        } else { Write-Host ("Machine PATH already contains: " + $BinDir) }
    } else {
        $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
        if ($null -eq $userPath) { $userPath = "" }
        $parts = $userPath -split ';' | Where-Object { $_ -ne '' }
        $hit = $false
        foreach ($p in $parts) { if ($p.TrimEnd('\') -ieq $BinDir.TrimEnd('\')) { $hit = $true; break } }
        if (-not $hit) {
            $newPath = if ($userPath -eq "") { $BinDir } else { "$userPath;$BinDir" }
            [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
            Write-Host ("Added to User PATH: " + $BinDir)
        } else { Write-Host ("User PATH already contains: " + $BinDir) }
    }
}

Write-Host ""
Write-Host ("Kern " + $TargetVersion + " installed successfully.")
Write-Host "Next steps:"
Write-Host "  - Open a new terminal and run: kern --version"
Write-Host ("  - Upgrade later: re-run install.ps1 (or choose another version once available)")
Write-Host ("  - Uninstall: remove '" + $Prefix + "' and remove '" + $BinDir + "' from PATH")
