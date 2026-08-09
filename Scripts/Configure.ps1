# Configures the CMake build (Ninja Multi-Config, MSVC).
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$envBat = Join-Path $PSScriptRoot "_env.bat"
Push-Location $root
try {
    & $envBat cmake --preset msvc
    if ($LASTEXITCODE -ne 0) { throw "Configure failed ($LASTEXITCODE)" }
} finally { Pop-Location }
