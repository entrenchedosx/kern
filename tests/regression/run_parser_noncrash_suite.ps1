$ErrorActionPreference = "Continue"
$Root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$kern = Join-Path $Root "build\Release\kern.exe"
if (!(Test-Path $kern)) { throw "missing kern executable: $kern" }

$tmpDir = Join-Path $Root "tests\regression\_tmp_parser_noncrash"
if (Test-Path $tmpDir) { Remove-Item -LiteralPath $tmpDir -Recurse -Force }
New-Item -ItemType Directory -Path $tmpDir | Out-Null

$bad1 = Join-Path $tmpDir "bad_unclosed_expr.kn"
Set-Content -LiteralPath $bad1 -Value "let x = (1 +`nprint(x)" -Encoding Ascii

$bad2 = Join-Path $tmpDir "bad_module_return.kn"
Set-Content -LiteralPath $bad2 -Value "return 1" -Encoding Ascii

function Assert-ParserFail([string]$path) {
    $out = & $kern --check $path 2>&1
    if ($LASTEXITCODE -eq 0) { throw "expected parse failure: $path" }
    $txt = ($out | Out-String)
    if ($txt -notmatch "PARSE-SYNTAX|LEX-TOKENIZE|SyntaxError") {
        throw "expected structured syntax diagnostic for: $path`n$txt"
    }
    if ($txt -match "unknown exception|Unhandled|Access violation") {
        throw "detected crash-like output for: $path`n$txt"
    }
}

Assert-ParserFail $bad1
Assert-ParserFail $bad2

Remove-Item -LiteralPath $tmpDir -Recurse -Force
Write-Host "[parser-noncrash] PASS"
