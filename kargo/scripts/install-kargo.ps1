# Install only Kargo (Node CLI) on Windows. No Kern build.
# Default: copy to $env:LOCALAPPDATA\Programs\kargo\lib\kargo, launcher kargo.cmd next to it in ..\bin
#
# Optional: -Force overwrites without prompt; -Prefix <dir> overrides install root
# PATH: user PATH is updated for the default prefix only. Custom -Prefix skips PATH
# unless you also pass -AddToPath. Use -NoPath to never change PATH.
# -Quiet: only errors, prompts, and final summary
# -InstallMode: Auto | Upgrade | Reinstall | Uninstall | Cancel — Auto shows a menu when an install exists
#
# Usage (from repo root, with PowerShell):
#   .\kargo\scripts\install-kargo.ps1

param(
    [string] $Prefix,
    [switch] $Force,
    [switch] $NoPath,
    [switch] $AddToPath,
    [switch] $Quiet,
    [ValidateSet("Auto", "Upgrade", "Reinstall", "Uninstall", "Cancel")]
    [string] $InstallMode = "Auto"
)

$ErrorActionPreference = "Stop"

function Write-Info([string] $msg) {
    if (-not $Quiet) {
        Write-Host $msg
    }
}

function Write-Step([string] $n, [string] $msg) {
    Write-Info ("kargo-install: [{0}] {1}" -f $n, $msg)
}

function Write-Die([string] $msg) {
    Write-Error "kargo-install: $msg"
    exit 1
}

function Test-KargoLauncherOurs([string] $path) {
    if (-not (Test-Path -LiteralPath $path)) { return $false }
    $raw = Get-Content -LiteralPath $path -Raw -ErrorAction SilentlyContinue
    if ($null -eq $raw) { return $false }
    return ($raw -match '@echo off' -and $raw -match 'cli\\entry\.js')
}

function Test-StdinInteractive {
    try {
        return -not [Console]::IsInputRedirected
    } catch {
        return $true
    }
}

function Invoke-KargoUninstall([string] $dest, [string] $launcher) {
    Write-Info "kargo-install: This will delete:"
    if (Test-Path -LiteralPath $dest) {
        Write-Info ("kargo-install:   {0}" -f $dest)
    } else {
        Write-Info "kargo-install:   (application directory already absent)"
    }
    if ((Test-Path -LiteralPath $launcher) -and (Test-KargoLauncherOurs $launcher)) {
        Write-Info ("kargo-install:   {0}" -f $launcher)
    } elseif (Test-Path -LiteralPath $launcher) {
        Write-Info ("kargo-install:   (launcher is not from this installer — will not remove)")
    }
    $a = Read-Host "kargo-install: Are you sure? [y/N]"
    if ($a -notmatch '^[yY]') {
        Write-Info "kargo-install: uninstall cancelled."
        return $false
    }
    if (Test-Path -LiteralPath $dest) {
        Remove-Item -LiteralPath $dest -Recurse -Force
    }
    if ((Test-Path -LiteralPath $launcher) -and (Test-KargoLauncherOurs $launcher)) {
        Remove-Item -LiteralPath $launcher -Force
    }
    $kd = Join-Path $env:USERPROFILE ".kargo"
    if (Test-Path -LiteralPath $kd) {
        $b = Read-Host "kargo-install: Remove ~/.kargo (config + cached packages)? [y/N]"
        if ($b -match '^[yY]') {
            Remove-Item -LiteralPath $kd -Recurse -Force
            Write-Info ("kargo-install: removed {0}" -f $kd)
        } else {
            Write-Info "kargo-install: left ~/.kargo in place"
        }
    }
    Write-Info "kargo-install: uninstall complete."
    return $true
}

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$DefaultPrefix = Join-Path $env:LOCALAPPDATA "Programs\kargo"
if (-not $PSBoundParameters.ContainsKey("Prefix")) {
    $Prefix = $DefaultPrefix
}
$usedDefaultPrefix = ($Prefix.TrimEnd('\') -ieq $DefaultPrefix.TrimEnd('\'))
$Dest = Join-Path $Prefix "lib\kargo"
$BinDir = Join-Path $Prefix "bin"
$Launcher = Join-Path $BinDir "kargo.cmd"

Write-Info "kargo-install: installing from $Root"

$node = Get-Command node -ErrorAction SilentlyContinue | Select-Object -First 1
if ($null -eq $node) {
    Write-Die "Node.js is not on PATH. Install Node 18.17+ from https://nodejs.org/"
}

$v = (& node -p "process.versions.node" 2>$null)
$parts = $v.Split(".")
$maj = [int]$parts[0]
$min = if ($parts.Length -gt 1) { [int]$parts[1] } else { 0 }
$pat = if ($parts.Length -gt 2) { [int]$parts[2] } else { 0 }
$ok = ($maj -gt 18) -or (($maj -eq 18) -and (($min -gt 17) -or (($min -eq 17) -and ($pat -ge 0))))
if (-not $ok) {
    Write-Die "Node $v is too old; need >= 18.17.0 (see kargo/package.json engines)."
}

$npm = Get-Command npm -ErrorAction SilentlyContinue | Select-Object -First 1
if ($null -eq $npm) {
    Write-Die "npm is not on PATH; install Node.js (includes npm) or fix PATH."
}
try {
    & npm --version | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "npm exit $LASTEXITCODE" }
} catch {
    Write-Die "npm failed (npm --version). Repair your Node.js installation."
}

if (-not (Test-Path -LiteralPath (Join-Path $Root "package.json")) -or
    -not (Test-Path -LiteralPath (Join-Path $Root "cli\entry.js"))) {
    Write-Die "expected kargo package at $Root"
}

$hasApp = (Test-Path -LiteralPath (Join-Path $Dest "cli\entry.js")) -and (Test-Path -LiteralPath (Join-Path $Dest "package.json"))
$hasOurLauncher = Test-KargoLauncherOurs $Launcher
$detected = $hasApp -or $hasOurLauncher

$verShown = "(none)"
if ($hasApp) {
    try {
        $verShown = (& node (Join-Path $Dest "cli\entry.js") --version 2>$null | Out-String).Trim()
    } catch {
        $verShown = "unknown"
    }
}

if ($InstallMode -eq "Uninstall" -and -not $detected) {
    Write-Host "kargo-install: nothing to uninstall."
    exit 0
}

if ($detected) {
    $choice = $null
    switch ($InstallMode) {
        "Upgrade" { $choice = 1 }
        "Reinstall" { $choice = 2 }
        "Uninstall" { $choice = 3 }
        "Cancel" { $choice = 4 }
        "Auto" {
            if (-not (Test-StdinInteractive)) {
                Write-Info "kargo-install: non-interactive: existing install detected — upgrading."
                $choice = 1
            } else {
                Write-Info "kargo-install: Existing Kargo installation detected."
                Write-Info ("kargo-install:   Version:      {0}" -f $verShown)
                Write-Info ("kargo-install:   Application:  {0}" -f $Dest)
                if ($hasOurLauncher) {
                    Write-Info ("kargo-install:   Launcher:     {0} (this installer)" -f $Launcher)
                } else {
                    Write-Info "kargo-install:   Launcher:     (missing or not from this installer)"
                }
                Write-Info "kargo-install:"
                Write-Info "kargo-install: What would you like to do?"
                Write-Info "kargo-install:   [1] Upgrade   — replace from this tree (~/.kargo kept)"
                Write-Info "kargo-install:   [2] Reinstall — remove install tree, then fresh copy"
                Write-Info "kargo-install:   [3] Uninstall — remove application and our launcher"
                Write-Info "kargo-install:   [4] Cancel"
                $in = Read-Host "kargo-install: Enter choice [1-4]"
                switch ($in) {
                    "1" { $choice = 1 }
                    "2" { $choice = 2 }
                    "3" { $choice = 3 }
                    "4" { $choice = 4 }
                    default {
                        Write-Info "kargo-install: invalid choice; exiting."
                        $choice = 4
                    }
                }
            }
        }
    }

    if ($choice -eq 4) { exit 0 }
    if ($choice -eq 3) {
        if (-not (Invoke-KargoUninstall $Dest $Launcher)) { exit 0 }
        exit 0
    }
    if ($choice -eq 1 -or $choice -eq 2) {
        $Force = $true
    }
}

if ((Test-Path -LiteralPath $Launcher) -and -not $Force) {
    $ans = Read-Host "kargo-install: $Launcher exists. Overwrite? [y/N]"
    if ($ans -notmatch '^[yY]') {
        Write-Host "kargo-install: cancelled."
        exit 0
    }
}

Write-Step "1/3" "copying package and running npm install --omit=dev…"
Write-Info ("kargo-install: destination {0}" -f $Dest)

New-Item -ItemType Directory -Path $BinDir -Force | Out-Null
if (Test-Path -LiteralPath $Dest) {
    Remove-Item -LiteralPath $Dest -Recurse -Force
}
New-Item -ItemType Directory -Path (Split-Path -Parent $Dest) -Force | Out-Null
Copy-Item -LiteralPath $Root -Destination $Dest -Recurse -Force

Push-Location $Dest
try {
    & npm install --omit=dev
    if ($LASTEXITCODE -ne 0) {
        Write-Die "npm install --omit=dev failed in $Dest — fix network/npm cache and retry."
    }
} finally {
    Pop-Location
}

Write-Step "2/3" "writing launcher…"
$entry = Join-Path $Dest "cli\entry.js"
$cmd = "@echo off`r`nnode `"$entry`" %*`r`n"
Set-Content -Path $Launcher -Value $cmd -Encoding ASCII -Force

Write-Step "3/3" "verifying kargo --version…"
try {
    $verOut = & $Launcher --version 2>&1
    if ($LASTEXITCODE -ne 0) { throw "exit $LASTEXITCODE" }
} catch {
    Write-Die "smoke test failed: $Launcher --version"
}

$ver = ($verOut | Out-String).Trim()
Write-Info "kargo-install: done — Kargo $ver"
Write-Info ("kargo-install:   application:  {0}" -f $Dest)
Write-Info ("kargo-install:   launcher:     {0}" -f $Launcher)

$shouldAddPath = -not $NoPath -and ($AddToPath -or $usedDefaultPrefix)
if ($shouldAddPath) {
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if ($null -eq $userPath) { $userPath = "" }
    $parts = $userPath -split ';' | Where-Object { $_ -ne '' }
    $hit = $false
    foreach ($p in $parts) {
        if ($p.TrimEnd('\') -ieq $BinDir.TrimEnd('\')) { $hit = $true; break }
    }
    if (-not $hit) {
        $newPath = if ($userPath -eq "") { $BinDir } else { "$userPath;$BinDir" }
        [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
        Write-Info ("kargo-install:   PATH:         added User PATH entry: {0}" -f $BinDir)
    } else {
        Write-Info ("kargo-install:   PATH:         User PATH already contains {0}" -f $BinDir)
    }
    Write-Info "kargo-install:   next step:    open a new terminal and run: kargo --version"
} else {
    Write-Info ("kargo-install:   PATH:         not modified; run: {0}" -f $Launcher)
    if (-not $usedDefaultPrefix -and -not $NoPath) {
        Write-Info ("kargo-install:   tip:          pass -AddToPath to append {0} to User PATH." -f $BinDir)
    }
}
