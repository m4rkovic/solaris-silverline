$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$build = Join-Path $root "build"
Write-Host "Configuring Solaris Silverline..." -ForegroundColor Cyan
cmake `
    -S $root `
    -B $build `
    -G "Visual Studio 17 2022" `
    -A x64 `
    -DSOLARIS_COPY_PLUGIN_AFTER_BUILD=OFF
Write-Host ""
Write-Host "Configure complete." -ForegroundColor Green
