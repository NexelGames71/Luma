# Builds Luma Engine. Usage: Build.ps1 [-Config Debug|Development|Release|Shipping]
param([string]$Config = "Development")
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$envBat = Join-Path $PSScriptRoot "_env.bat"
Push-Location $root
try {
    if (-not (Test-Path (Join-Path $root "build"))) {
        & $envBat cmake --preset msvc
        if ($LASTEXITCODE -ne 0) { throw "Configure failed ($LASTEXITCODE)" }
    }
    & $envBat cmake --build build --config $Config
    if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)" }
} finally { Pop-Location }
