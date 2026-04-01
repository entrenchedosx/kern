# validates every .kn example under examples/ (recursive). Exit 1 if any --check fails.
# warnings on stderr must not abort the script (PowerShell treats native stderr as error records when ErrorAction is Stop).
$ErrorActionPreference = "Continue"
$Root = Split-Path -Parent $PSScriptRoot
$KernCandidates = @(
    Join-Path $Root "shareable-ide\compiler\kern.exe"
    Join-Path $Root "build\Release\kern.exe"
)
$Kern = $KernCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $Kern) {
    Write-Error "kern.exe not found. Build first or run publish_shareable_drops.ps1."
    exit 1
}

$files = Get-ChildItem -LiteralPath (Join-Path $Root "examples") -Filter "*.kn" -File -Recurse | Sort-Object FullName
$fail = @()
foreach ($f in $files) {
    & $Kern --check $f.FullName *> $null
    if ($LASTEXITCODE -ne 0) { $fail += $f.FullName }
}
Write-Host "check_examples: $($files.Count) file(s), $($fail.Count) failure(s), kern=$Kern"
if ($fail.Count -gt 0) {
    $fail | ForEach-Object { Write-Host "  FAIL $_" }
    exit 1
}
exit 0
