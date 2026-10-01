param(
    [string]$BuildDir = "build",
    [string]$Config = "Release"
)
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$build = Join-Path $root $BuildDir
$version = (Get-Content (Join-Path $root "VERSION") -Raw).Trim()
$packageRoot = Join-Path $root "dist\SolarisSilverline-$version-windows-x64"
if (Test-Path $packageRoot) { Remove-Item -Recurse -Force $packageRoot }
New-Item -ItemType Directory -Force -Path (Join-Path $packageRoot "VST3") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $packageRoot "Standalone") | Out-Null

$vst3 = Get-ChildItem -Path $build -Recurse -Filter "Solaris Silverline.vst3" -ErrorAction SilentlyContinue | Select-Object -First 1
$standalone = Get-ChildItem -Path $build -Recurse -Filter "Solaris Silverline.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
if ($null -eq $vst3) { throw "VST3 artifact not found under $build" }
if ($null -eq $standalone) { throw "Standalone artifact not found under $build" }
Copy-Item -Recurse -Force $vst3.FullName (Join-Path $packageRoot "VST3")
Copy-Item -Force $standalone.FullName (Join-Path $packageRoot "Standalone")
Copy-Item -Force (Join-Path $root "LICENSE.txt") $packageRoot
Copy-Item -Force (Join-Path $root "THIRD_PARTY_NOTICES.md") $packageRoot
Copy-Item -Force (Join-Path $root "VERSION") $packageRoot
Write-Host "Package: $packageRoot" -ForegroundColor Green
