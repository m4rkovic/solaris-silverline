$ErrorActionPreference = "Continue"

Write-Host ""
Write-Host "Solaris Silverline - Environment Check" -ForegroundColor Cyan
Write-Host "=====================================" -ForegroundColor Cyan

function Check-Command {
    param(
        [string]$Name,
        [string[]]$Args = @("--version")
    )

    $cmd = Get-Command $Name -ErrorAction SilentlyContinue

    if ($null -eq $cmd) {
        Write-Host "[MISSING] $Name" -ForegroundColor Red
        return $false
    }

    Write-Host "[OK] $Name -> $($cmd.Source)" -ForegroundColor Green
    try {
        & $Name @Args | Select-Object -First 2
    } catch {
        Write-Host "  Found, but version check failed." -ForegroundColor Yellow
    }

    return $true
}

$gitOk = Check-Command "git"
$cmakeOk = Check-Command "cmake"

$cl = Get-Command "cl.exe" -ErrorAction SilentlyContinue
if ($null -eq $cl) {
    Write-Host "[MISSING] cl.exe / MSVC compiler" -ForegroundColor Red
    Write-Host "Run this from 'Developer PowerShell for Visual Studio'." -ForegroundColor Yellow
    $clOk = $false
} else {
    Write-Host "[OK] cl.exe -> $($cl.Source)" -ForegroundColor Green
    $clOk = $true
}

Write-Host ""
if ($gitOk -and $cmakeOk -and $clOk) {
    Write-Host "Toolchain looks ready." -ForegroundColor Green
} else {
    Write-Host "One or more required tools are missing." -ForegroundColor Yellow
}
