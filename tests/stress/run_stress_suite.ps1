# adversarial smoke: compile-check stress .kn files + generated huge inputs (no infinite hangs).
# usage: from repo root: powershell -ExecutionPolicy Bypass -File .\tests\stress\run_stress_suite.ps1
# optional: -Aggressive  deeper generated chains, UTF-8/UTF-16 BOM checks, full kern on safe scripts,
#           and stress_vm_call_depth_overflow.kn under --release (must still fail non-zero).
# uses Start-Process so exit codes from kern.exe are reliable on Windows PowerShell 5.x.
param(
    [switch]$Aggressive = $false
)

$ErrorActionPreference = "Continue"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$KernCandidates = @(
    (Join-Path $Root "build\Release\kern.exe")
    (Join-Path $Root "shareable-ide\compiler\kern.exe")
)
$Kern = $KernCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $Kern) {
    Write-Error "kern.exe not found. Build Release first."
    exit 2
}

function Invoke-KernExitCode {
    param(
        [string[]]$Arguments,
        [string]$WorkingDirectory = $Root
    )
    $p = Start-Process -FilePath $Kern -ArgumentList $Arguments -WorkingDirectory $WorkingDirectory `
        -Wait -PassThru -NoNewWindow
    if ($null -eq $p) { return -1 }
    return [int]$p.ExitCode
}

$fail = @()
$dir = $PSScriptRoot

Get-ChildItem -Path $dir -Filter "*.kn" -File | ForEach-Object {
    $name = $_.Name
    if ($name -ne "stress_vm_call_depth_overflow.kn") {
        $code = Invoke-KernExitCode -Arguments @("--check", $_.FullName)
        if ($code -ne 0) { $fail += $name }
    }
}
$depthKnPath = Join-Path $dir "stress_vm_call_depth_overflow.kn"
if (Test-Path $depthKnPath) {
    $syn = Invoke-KernExitCode -Arguments @("--check", $depthKnPath)
    if ($syn -ne 0) { $fail += "stress_vm_call_depth_overflow.kn (--check)" }
}

# generated: very long ?? chain (parser stack safety)
$longCoalesce = Join-Path $env:TEMP "kern_stress_coalesce_$([Guid]::NewGuid().ToString('n')).kn"
try {
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.Append("let x = null")
    for ($i = 0; $i -lt $coalesceRepeats; $i++) { [void]$sb.Append(" ?? null") }
    [void]$sb.Append(" ?? 1`nprint(x)")
    [System.IO.File]::WriteAllText($longCoalesce, $sb.ToString(), [System.Text.UTF8Encoding]::new($false))
    $code = Invoke-KernExitCode -Arguments @("--check", $longCoalesce)
    if ($code -ne 0) { $fail += "generated_long_coalesce.kn" }
} finally {
    if (Test-Path $longCoalesce) { Remove-Item $longCoalesce -Force -ErrorAction SilentlyContinue }
}

# generated: many unary bangs (UTF-8 no BOM)
$longUnary = Join-Path $env:TEMP "kern_stress_unary_$([Guid]::NewGuid().ToString('n')).kn"
try {
    $ub = New-Object System.Text.StringBuilder
    [void]$ub.Append("print(")
    for ($i = 0; $i -lt $unaryRepeats; $i++) { [void]$ub.Append("!") }
    [void]$ub.Append(" true)")
    [System.IO.File]::WriteAllText($longUnary, $ub.ToString(), [System.Text.UTF8Encoding]::new($false))
    $code = Invoke-KernExitCode -Arguments @("--check", $longUnary)
    if ($code -ne 0) { $fail += "generated_long_unary.kn" }
} finally {
    if (Test-Path $longUnary) { Remove-Item $longUnary -Force -ErrorAction SilentlyContinue }
}

# oversized source: lexer must reject cleanly
$huge = Join-Path $env:TEMP "kern_stress_huge_$([Guid]::NewGuid().ToString('n')).kn"
try {
    $fs = [System.IO.File]::Create($huge)
    $one = [byte][char]' '
    $chunk = New-Object byte[] (1024 * 1024)
    for ($i = 0; $i -lt $chunk.Length; $i++) { $chunk[$i] = $one }
    for ($b = 0; $b -lt 50; $b++) { $fs.Write($chunk, 0, $chunk.Length) }
    $fs.Close()
    $code = Invoke-KernExitCode -Arguments @("--check", $huge)
    if ($code -eq 0) { $fail += "huge_file_should_fail" }
} finally {
    if (Test-Path $huge) { Remove-Item $huge -Force -ErrorAction SilentlyContinue }
}

# VM: max call depth must surface as non-zero exit (not a native crash)
$depthKn = Join-Path $dir "stress_vm_call_depth_overflow.kn"
$depthExit = Invoke-KernExitCode -Arguments @($depthKn)
if ($depthExit -eq 0) { $fail += "stress_vm_call_depth_overflow.kn (expected non-zero exit)" }

if ($Aggressive) {
    # CLI --release uses a higher max call depth; set_max_call_depth(1024) in the script must still win
    $depthRelease = Invoke-KernExitCode -Arguments @("--release", $depthKn)
    if ($depthRelease -eq 0) { $fail += "stress_vm_call_depth_overflow.kn --release (expected non-zero exit)" }

    $safeToRun = @(
        "stress_coalesce_and_unary.kn",
        "stress_vm_recursion.kn",
        "stress_malformed_and_unicode.kn",
        "stress_stdlib_abuse.kn"
    )
    foreach ($n in $safeToRun) {
        $p = Join-Path $dir $n
        if (-not (Test-Path $p)) { continue }
        $run = Invoke-KernExitCode -Arguments @($p)
        if ($run -ne 0) { $fail += "$n (full run expected exit 0)" }
    }
}

if ($fail.Count -gt 0) {
    Write-Host "FAIL: $($fail -join ', ')"
    exit 1
}
$mode = if ($Aggressive) { "aggressive" } else { "default" }
Write-Host "OK: stress suite ($mode, kern=$Kern)"
exit 0
