# adversarial smoke: compile-check stress .kn files + generated huge inputs (no infinite hangs).
# usage: from repo root: powershell -ExecutionPolicy Bypass -File .\tests\stress\run_stress_suite.ps1
# native tools write to stderr; do not treat that as a terminating error.
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

$fail = @()
$dir = $PSScriptRoot

Get-ChildItem -Path $dir -Filter "*.kn" -File | ForEach-Object {
    & $Kern --check $_.FullName *> $null
    if ($LASTEXITCODE -ne 0) { $fail += $_.Name }
}

# generated: very long ?? chain (parser stack safety)
$longCoalesce = Join-Path $env:TEMP "kern_stress_coalesce_$([Guid]::NewGuid().ToString('n')).kn"
try {
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.Append("let x = null")
    for ($i = 0; $i -lt 12000; $i++) { [void]$sb.Append(" ?? null") }
    [void]$sb.Append(" ?? 1`nprint(x)")
    [System.IO.File]::WriteAllText($longCoalesce, $sb.ToString(), [System.Text.UTF8Encoding]::new($false))
    & $Kern --check $longCoalesce *> $null
    if ($LASTEXITCODE -ne 0) { $fail += "generated_long_coalesce.kn" }
} finally {
    if (Test-Path $longCoalesce) { Remove-Item $longCoalesce -Force -ErrorAction SilentlyContinue }
}

# generated: many unary bangs (UTF-8 no BOM so the lexer does not see a stray prefix byte)
$longUnary = Join-Path $env:TEMP "kern_stress_unary_$([Guid]::NewGuid().ToString('n')).kn"
try {
    $ub = New-Object System.Text.StringBuilder
    [void]$ub.Append("print(")
    for ($i = 0; $i -lt 8000; $i++) { [void]$ub.Append("!") }
    [void]$ub.Append(" true)")
    [System.IO.File]::WriteAllText($longUnary, $ub.ToString(), [System.Text.UTF8Encoding]::new($false))
    & $Kern --check $longUnary *> $null
    if ($LASTEXITCODE -ne 0) { $fail += "generated_long_unary.kn" }
} finally {
    if (Test-Path $longUnary) { Remove-Item $longUnary -Force -ErrorAction SilentlyContinue }
}

# oversized source: lexer must reject cleanly
$huge = Join-Path $env:TEMP "kern_stress_huge_$([Guid]::NewGuid().ToString('n')).kn"
try {
    # 49 MiB of spaces — under 48 MiB limit would pass tokenize; use 50 MiB to trigger cap
    $fs = [System.IO.File]::Create($huge)
    $one = [byte][char]' '
    $chunk = New-Object byte[] (1024 * 1024)
    for ($i = 0; $i -lt $chunk.Length; $i++) { $chunk[$i] = $one }
    for ($b = 0; $b -lt 50; $b++) { $fs.Write($chunk, 0, $chunk.Length) }
    $fs.Close()
    & $Kern --check $huge *> $null
    # expect failure (non-zero) — if 0, lexer accepted too much
    if ($LASTEXITCODE -eq 0) { $fail += "huge_file_should_fail" }
} finally {
    if (Test-Path $huge) { Remove-Item $huge -Force -ErrorAction SilentlyContinue }
}

if ($fail.Count -gt 0) {
    Write-Host "FAIL: $($fail -join ', ')"
    exit 1
}
Write-Host "OK: stress suite (kern=$Kern)"
exit 0
