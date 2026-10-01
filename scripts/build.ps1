$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$build = Join-Path $root "build"

if (-not (Test-Path $build)) {
    Write-Host "Build directory not found. Running configure first..." -ForegroundColor Yellow
    & (Join-Path $PSScriptRoot "configure.ps1")
}

Write-Host "Building Solaris Silverline Release..." -ForegroundColor Cyan

cmake --build $build --config Release --parallel

Write-Host ""
Write-Host "Build complete." -ForegroundColor Green
Write-Host "Check: build\SolarisSilverline_artefacts\Release\" -ForegroundColor Green
