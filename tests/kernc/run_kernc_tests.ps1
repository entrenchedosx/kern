[CmdletBinding()]
param(
    [string]$Splc = "",
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ([string]::IsNullOrWhiteSpace($Root)) { $Root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..")) }
if ([string]::IsNullOrWhiteSpace($Splc)) {
    $Splc = Join-Path $Root "build\Release\kernc.exe"
}
if (-not (Test-Path $Splc)) { throw "kernc.exe not found: $Splc" }

# standalone kern links against Kern sources; use repo cwd so relative paths in configs/tools match, and subprocesses inherit it.
Push-Location $Root
try {

$multifileMain = Join-Path $Root "tests\kernc\multifile\main.kn"
$outExe = Join-Path $Root "build\Release\tests\kernc_multifile.exe"
New-Item -ItemType Directory -Force -Path ([System.IO.Path]::GetDirectoryName($outExe)) | Out-Null
if (Test-Path $outExe) { Remove-Item $outExe -Force }

Write-Host "[kern-test] compile multifile project"
& $Splc $multifileMain -o $outExe --release --opt 2 --console
if ($LASTEXITCODE -ne 0) { throw "multifile compile failed" }
if (-not (Test-Path $outExe)) {
    # on some Windows hosts, file materialization can lag briefly after kern returns.
    $maxWaitMs = 15000
    $slept = 0
    while ($slept -lt $maxWaitMs -and -not (Test-Path $outExe)) {
        Start-Sleep -Milliseconds 250
        $slept += 250
    }
}
if (-not (Test-Path $outExe)) { throw "output exe missing: $outExe" }

Write-Host "[kern-test] run standalone exe"
Push-Location (Join-Path $Root "tests\kernc\multifile")
try {
    $runOut = & $outExe
    $joined = ($runOut -join "`n")
} finally {
    Pop-Location
}
if ($joined -notmatch "15") { throw "expected output 15 missing" }
if ($joined -notmatch "14") { throw "expected output 14 missing" }

Write-Host "[kern-test] clean-machine style run (isolated temp folder)"
$isoDir = Join-Path $Root "build\Release\tests\kernc_isolated"
if (Test-Path $isoDir) { Remove-Item $isoDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $isoDir | Out-Null
$isoExe = Join-Path $isoDir "standalone.exe"
Copy-Item $outExe $isoExe -Force
Push-Location $isoDir
try {
    $prevSplLib = $env:KERN_LIB
    $env:KERN_LIB = (Join-Path $Root "tests\kernc\multifile")
    $isoOut = & ".\standalone.exe"
    if ($null -eq $prevSplLib) { Remove-Item Env:KERN_LIB -ErrorAction SilentlyContinue } else { $env:KERN_LIB = $prevSplLib }
    $isoJoined = ($isoOut -join "`n")
    if ($isoJoined -notmatch "15") { throw "isolated run missing expected output 15" }
    if ($isoJoined -notmatch "14") { throw "isolated run missing expected output 14" }
} finally {
    Pop-Location
}

Write-Host "[kern-test] config-driven build"
$cfg = Join-Path $Root "tests\kernc\kernconfig_multifile.json"
& $Splc --config $cfg
if ($LASTEXITCODE -ne 0) { throw "config build failed" }

Write-Host "[kern-test] cycle detection"
$cycleMain = Join-Path $Root "tests\kernc\cycle\a.kn"
$cycleOut = Join-Path $Root "build\Release\tests\kernc_cycle.exe"
$prevEa = $ErrorActionPreference
$ErrorActionPreference = "Continue"
& $Splc $cycleMain -o $cycleOut --release --opt 2 --console 2>&1 | Out-Null
$cycleExit = $LASTEXITCODE
$ErrorActionPreference = $prevEa
if ($cycleExit -eq 0) { throw "cycle detection should fail with non-zero exit" }

Write-Host "[kern-test] fix-all dry-run/apply/undo"
$fixRoot = Join-Path $Root "tests\kernc\fixall"
$fixMain = Join-Path $fixRoot "main.kn"
$origText = Get-Content -Path $fixMain -Raw
$dryReport = Join-Path $Root "build\Release\tests\kernc_fixall_dryrun.json"
$applyReport = Join-Path $Root "build\Release\tests\kernc_fixall_apply.json"

& $Splc --fix-all $fixRoot --dry-run --report-json $dryReport
if (-not (Test-Path $dryReport)) { throw "fix-all dry-run did not emit report" }

& $Splc --fix-all $fixRoot --report-json $applyReport
if (-not (Test-Path $applyReport)) { throw "fix-all apply did not emit report" }
$afterText = Get-Content -Path $fixMain -Raw

$reportJson = Get-Content -Path $applyReport -Raw | ConvertFrom-Json
$backupRoot = [string]$reportJson.backupRoot
if (-not [string]::IsNullOrWhiteSpace($backupRoot)) {
    & $Splc --undo-fixes $backupRoot
    if ($LASTEXITCODE -ne 0) { throw "undo-fixes failed" }
    $restored = Get-Content -Path $fixMain -Raw
    if ($restored -ne $origText) { throw "undo-fixes did not restore original fixture" }
}

Write-Host "[kern-test] analyzer + pointer syntax smoke"
$analysisJson = & $Splc --analyze $fixRoot --json
if (-not $analysisJson) { throw "analyze mode emitted no JSON payload" }
$kernExe = Join-Path $Root "build\Release\kern.exe"
if (Test-Path $kernExe) {
    $ptrDemo = Join-Path $Root "examples\system\oskit_pointer_demo.kn"
    & $kernExe --check $ptrDemo
    if ($LASTEXITCODE -ne 0) { throw "pointer demo syntax check failed" }
}

Write-Host "[kern-test] package workflow smoke"
$pkgRoot = Join-Path $Root "tests\kernc\pkgdemo"
if (Test-Path $pkgRoot) { Remove-Item $pkgRoot -Recurse -Force }
New-Item -ItemType Directory -Force -Path $pkgRoot | Out-Null
& $Splc --pkg-init $pkgRoot
if ($LASTEXITCODE -ne 0) { throw "pkg-init failed" }
& $Splc --pkg-lock $pkgRoot
if ($LASTEXITCODE -ne 0) { throw "pkg-lock failed" }
& $Splc --pkg-validate $pkgRoot
if ($LASTEXITCODE -ne 0) { throw "pkg-validate failed" }

Write-Host "[kern-test] PASS"

} finally {
    Pop-Location
}

exit 0
