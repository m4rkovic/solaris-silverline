param([switch]$Clean,[switch]$Validate)
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$build = Join-Path $root "build"
if ($Clean -and (Test-Path $build)) { Remove-Item -Recurse -Force $build }
& (Join-Path $PSScriptRoot "check-env.ps1")
& (Join-Path $PSScriptRoot "configure.ps1")
cmake --build $build --config Release --parallel
$artefacts = Join-Path $build "SolarisSilverline_artefacts\Release"
$vst3 = Get-ChildItem -Path $artefacts -Recurse -Filter "*.vst3" -ErrorAction SilentlyContinue | Select-Object -First 1
if ($null -eq $vst3) { throw "Build finished but no VST3 was found under $artefacts" }
Write-Host "VST3: $($vst3.FullName)" -ForegroundColor Green
if ($Validate) { & (Join-Path $PSScriptRoot "validate.ps1") -PluginPath $vst3.FullName }
