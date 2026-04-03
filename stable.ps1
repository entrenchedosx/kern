# Repo-root shortcut for tests/run_stable.ps1
param(
    [switch]$WithExamples,
    [switch]$Quick,
    [string]$BuildDir = "build"
)
$ErrorActionPreference = "Stop"
& ([System.IO.Path]::Combine($PSScriptRoot, "tests", "run_stable.ps1")) -WithExamples:$WithExamples -Quick:$Quick -BuildDir $BuildDir
