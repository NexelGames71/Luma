# Builds and runs LumaEngine.exe. Usage: Run.ps1 [-Config Development]
param([string]$Config = "Development")
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$envBat = Join-Path $PSScriptRoot "_env.bat"
Push-Location $root
try {
    & $envBat cmake --build build --config $Config --target LumaEngine
    if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)" }
    $exe = Join-Path $root "build/bin/$Config/LumaEngine.exe"
    Write-Host "[Luma] launching $exe" -ForegroundColor Green
    & $exe
} finally { Pop-Location }
