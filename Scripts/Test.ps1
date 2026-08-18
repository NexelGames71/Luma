or # Runs the automated test suite. Usage: Test.ps1 [-Config Development]
param([string]$Config = "Development")
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$envBat = Join-Path $PSScriptRoot "_env.bat"
Push-Location $root
try {
    & $envBat cmake --build build --config $Config
    if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)" }
    & $envBat ctest --test-dir build -C $Config --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Tests failed ($LASTEXITCODE)" }
} finally { Pop-Location }
