# Run every examples/**/*.kn with kern.exe (timeout per script). Exit 1 if any real failure.
# Skips: blocking TCP servers, optional headless graphics (see -SkipGraphics).
#
# Usage:
#   pwsh -File scripts/run_all_examples.ps1 [-KernExe path] [-TimeoutSeconds 30] [-SkipGraphics]
param(
    [string]$KernExe = "",
    [int]$TimeoutSeconds = 35,
    [switch]$SkipGraphics = $true
)

$ErrorActionPreference = "Continue"
$Root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))

if ([string]::IsNullOrWhiteSpace($KernExe)) {
    foreach ($c in @(
            (Join-Path $Root "build-debug\Debug\kern.exe"),
            (Join-Path $Root "build\Release\kern.exe"),
            (Join-Path $Root "build\Debug\kern.exe"),
            (Join-Path $Root "build-plan\Release\kern.exe")
        )) {
        if (Test-Path -LiteralPath $c) { $KernExe = $c; break }
    }
}
if (-not (Test-Path -LiteralPath $KernExe)) {
    throw "kern.exe not found. Pass -KernExe or build Debug/Release."
}

$blockingServer = @{
    "tcp_echo_server.kn"       = $true
    "tcp_select_accept.kn"     = $true
}

function Test-IsGraphicsExample {
    param([string]$RelPath)
    if ($RelPath -match '[\\/]examples[\\/]graphics[\\/]') { return $true }
    $n = [System.IO.Path]::GetFileName($RelPath)
    if ($n -eq "kern_mini_browser.kn") { return $true }
    if ($n -eq "browserkit_render_demo.kn") { return $true }
    if ($n -eq "browserkit_load_page.kn") { return $true }
    if ($n -eq "gamekit_simple_game.kn") { return $true }
    if ($n -eq "gamekit_gui_app.kn") { return $true }
    return $false
}

$files = Get-ChildItem -LiteralPath (Join-Path $Root "examples") -Filter "*.kn" -File -Recurse | Sort-Object FullName
$timeoutMs = [Math]::Max(8000, $TimeoutSeconds * 1000)
$passed = 0
$skipped = 0
$failed = @()

foreach ($f in $files) {
    $rel = $f.FullName.Substring($Root.Length + 1).TrimStart('\', '/')
    $base = $f.Name

    if ($blockingServer.ContainsKey($base)) {
        Write-Host "[SKIP] $rel (blocking TCP server; run manually with client)" -ForegroundColor DarkYellow
        $skipped++
        continue
    }
    if ($SkipGraphics -and (Test-IsGraphicsExample -RelPath $rel)) {
        Write-Host "[SKIP] $rel (graphics/windowed; run locally or -SkipGraphics:`$false)" -ForegroundColor DarkYellow
        $skipped++
        continue
    }

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $KernExe
    $psi.Arguments = "--unsafe `"$($f.FullName)`""
    $psi.WorkingDirectory = $Root
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $p = [System.Diagnostics.Process]::Start($psi)
    if (-not $p.WaitForExit($timeoutMs)) {
        try { $p.Kill() } catch {}
        $failed += [pscustomobject]@{ File = $rel; Reason = "TIMEOUT" }
        continue
    }
    if ($p.ExitCode -ne 0) {
        $stderr = $p.StandardError.ReadToEnd()
        $stdout = $p.StandardOutput.ReadToEnd()
        $tail = ($stderr + "`n" + $stdout).Split("`n") | Select-Object -Last 12
        $failed += [pscustomobject]@{ File = $rel; Reason = "EXIT_$($p.ExitCode)"; Tail = ($tail -join "`n").Trim() }
        continue
    }
    $passed++
    Write-Host "[OK]   $rel" -ForegroundColor DarkGreen
}

Write-Host ""
Write-Host "examples: passed=$passed skipped=$skipped failed=$($failed.Count) kern=$KernExe"
if ($failed.Count -gt 0) {
    foreach ($x in $failed) {
        Write-Host "FAIL $($x.File) $($x.Reason)" -ForegroundColor Red
        if ($x.Tail) { Write-Host $x.Tail }
    }
    exit 1
}
exit 0
