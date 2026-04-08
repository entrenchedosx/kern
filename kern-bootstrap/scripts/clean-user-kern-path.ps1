# Remove dev-repo Kern build dirs and stale .kern\bin from HKCU Path (fresh bootstrapper install).
$ErrorActionPreference = "Stop"
$raw = [Environment]::GetEnvironmentVariable("Path", "User")
if ([string]::IsNullOrEmpty($raw)) {
    Write-Host "User Path empty."
    exit 0
}
$parts = $raw -split ";" | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" }
$kernBin = (Join-Path $env:USERPROFILE ".kern\bin").ToLowerInvariant()
$removed = [System.Collections.Generic.List[string]]::new()
$kept = [System.Collections.Generic.List[string]]::new()
$seen = @{}
foreach ($p in $parts) {
    $lower = $p.ToLowerInvariant()
    if ($lower -match "simple_programming_language[\\/]+build[\\/](release|debug)") {
        $removed.Add($p) | Out-Null
        continue
    }
    if ($lower -eq $kernBin -or $lower.StartsWith($kernBin + "\")) {
        $removed.Add($p) | Out-Null
        continue
    }
    if ($lower -match "program files[\\/]kern(\\|$|\\bin)") {
        $removed.Add($p) | Out-Null
        continue
    }
    if ($seen.ContainsKey($lower)) { continue }
    $seen[$lower] = $true
    $kept.Add($p) | Out-Null
}
$newPath = ($kept -join ";")
[Environment]::SetEnvironmentVariable("Path", $newPath, "User")
Write-Host "PATH cleanup: removed $($removed.Count) segment(s); kept $($kept.Count) (was $($parts.Count))."
foreach ($r in $removed) { Write-Host "  removed: $r" }
