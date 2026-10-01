$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$build = Join-Path $root "build"
if (-not (Test-Path $build)) { & (Join-Path $PSScriptRoot "configure.ps1") }
Write-Host "Building Solaris Silverline Release..." -ForegroundColor Cyan
cmake --build $build --config Release --parallel
$artefacts = Join-Path $build "SolarisSilverline_artefacts\Release"
$vst3 = Get-ChildItem -Path $artefacts -Recurse -Filter "*.vst3" -ErrorAction SilentlyContinue | Select-Object -First 1
Write-Host ""
Write-Host "Build complete." -ForegroundColor Green
if ($null -ne $vst3) { Write-Host "VST3: $($vst3.FullName)" -ForegroundColor Green } else { Write-Host "Artefacts: $artefacts" -ForegroundColor Yellow }
