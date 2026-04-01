# kern Test Runner - runs all examples/*.kn files
param(
    [string]$Exe = "",
    [int]$TimeoutSeconds = 45
)

$ErrorActionPreference = "Continue"
$Root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))

if ([string]::IsNullOrWhiteSpace($Exe)) {
    $candidates = @(
        (Join-Path $Root "build\Release\kern.exe"),
        (Join-Path $Root "build\Debug\kern.exe"),
        (Join-Path $Root "shareable-ide\compiler\kern.exe"),
        (Join-Path $Root "FINAL\bin\kern.exe"),
        (Join-Path $Root "FINAL\Release\kern.exe")
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { $Exe = $c; break }
    }
}

if (-not (Test-Path $Exe)) {
    throw "kern executable not found. Checked build/shareable/final locations."
}

$examplesDir = Join-Path $Root "examples"
$examples = Get-ChildItem -Path $examplesDir -Filter "*.kn" -File
$passed = 0
$failed = 0
$timeoutMs = [Math]::Max(5000, $TimeoutSeconds * 1000)

Push-Location $Root
try {
    foreach ($f in $examples) {
        $code = -1
        $outText = ""
        try {
            $psi = New-Object System.Diagnostics.ProcessStartInfo
            $psi.FileName = $Exe
            $psi.Arguments = "`"$($f.FullName)`""
            $psi.WorkingDirectory = $Root
            $psi.UseShellExecute = $false
            $psi.CreateNoWindow = $true
            $psi.RedirectStandardOutput = $true
            $psi.RedirectStandardError = $true
            $p = [System.Diagnostics.Process]::Start($psi)
            if (-not $p.WaitForExit($timeoutMs)) {
                $p.Kill()
                $code = -2
            } else {
                $code = $p.ExitCode
            }
            $outText = ($p.StandardOutput.ReadToEnd() + "`n" + $p.StandardError.ReadToEnd()).Trim()
        } catch {
            $code = -3
            $outText = $_.Exception.Message
        }

        if ($code -eq 0) {
            Write-Host "PASS $($f.Name)" -ForegroundColor Green
            $passed++
        } else {
            if ($code -eq -2) {
                Write-Host "FAIL $($f.Name) (timeout)" -ForegroundColor Red
            } else {
                Write-Host "FAIL $($f.Name) (exit $code)" -ForegroundColor Red
            }
            if (-not [string]::IsNullOrWhiteSpace($outText)) {
                Write-Host $outText
            }
            $failed++
        }
    }
} finally {
    Pop-Location
}

Write-Host "`n$passed passed, $failed failed"
if ($failed -gt 0) { exit 1 }
exit 0
