param(
    [string]$PluginPath,
    [string]$PluginValPath,
    [ValidateRange(1,10)][int]$Strictness=5
)
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$build = Join-Path $root "build"
if ([string]::IsNullOrWhiteSpace($PluginPath)) {
    $artefacts = Join-Path $build "SolarisSilverline_artefacts\Release"
    $plugin = Get-ChildItem -Path $artefacts -Recurse -Filter "*.vst3" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -eq $plugin) { throw "No VST3 found. Build first." }
    $PluginPath = $plugin.FullName
}
if ([string]::IsNullOrWhiteSpace($PluginValPath)) {
    if (-not [string]::IsNullOrWhiteSpace($env:PLUGINVAL_EXE)) { $PluginValPath=$env:PLUGINVAL_EXE }
    else { $command=Get-Command "pluginval.exe" -ErrorAction SilentlyContinue; if ($null -ne $command) { $PluginValPath=$command.Source } }
}
if ([string]::IsNullOrWhiteSpace($PluginValPath) -or -not (Test-Path $PluginValPath)) { throw "pluginval not found. Put pluginval.exe on PATH or set PLUGINVAL_EXE." }
& $PluginValPath --strictness-level $Strictness $PluginPath
if ($LASTEXITCODE -ne 0) { throw "pluginval failed with exit code $LASTEXITCODE" }
Write-Host "pluginval passed." -ForegroundColor Green
