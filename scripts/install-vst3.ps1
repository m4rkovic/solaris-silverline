param(
    [Parameter(Mandatory=$true)][string]$PluginPath,
    [string]$Destination = "$env:CommonProgramFiles\VST3"
)
$ErrorActionPreference = "Stop"
$source = Resolve-Path $PluginPath
if (-not $source.Path.EndsWith(".vst3", [System.StringComparison]::OrdinalIgnoreCase)) { throw "PluginPath must point to a .vst3 bundle." }
if (-not (Test-Path $Destination)) { New-Item -ItemType Directory -Force -Path $Destination | Out-Null }
Copy-Item -Recurse -Force $source.Path $Destination
Write-Host "Installed $($source.Path) -> $Destination" -ForegroundColor Green
