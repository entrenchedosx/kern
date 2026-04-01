$ErrorActionPreference = "Continue"
$Root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$kern = Join-Path $Root "build\Release\kern.exe"
if (!(Test-Path $kern)) { throw "missing kern executable: $kern" }

$tmpDir = Join-Path $Root "tests\regression\_tmp_vm_failfast"
if (Test-Path $tmpDir) { Remove-Item -LiteralPath $tmpDir -Recurse -Force }
New-Item -ItemType Directory -Path $tmpDir | Out-Null

$callBad = Join-Path $tmpDir "bad_call_target.kn"
Set-Content -LiteralPath $callBad -Value "let x = 7`nx()" -Encoding Ascii

$divBad = Join-Path $tmpDir "bad_div_zero.kn"
Set-Content -LiteralPath $divBad -Value "let a = 10`nlet b = 0`nprint(a / b)" -Encoding Ascii

$opBad = Join-Path $tmpDir "bad_invalid_op.kn"
Set-Content -LiteralPath $opBad -Value "let m = {}`nprint(m * 3)" -Encoding Ascii

function Assert-VMFail([string]$path, [string]$expectedCode) {
    $out = & $kern $path 2>&1
    if ($LASTEXITCODE -eq 0) { throw "expected VM failure: $path" }
    $txt = ($out | Out-String)
    if ($txt -notmatch [Regex]::Escape($expectedCode)) {
        throw "expected code '$expectedCode' for: $path`n$txt"
    }
    if ($txt -match "unknown exception|Unhandled|Access violation|bad_variant_access") {
        throw "detected crash-like output for: $path`n$txt"
    }
}

Assert-VMFail $callBad "VM-INVALID-CALL"
Assert-VMFail $divBad "VM-DIV-ZERO"
Assert-VMFail $opBad "VM-INVALID-OP"

Remove-Item -LiteralPath $tmpDir -Recurse -Force
Write-Host "[vm-fail-fast] PASS"
