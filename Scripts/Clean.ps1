# Removes the build directory.
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$build = Join-Path $root "build"
if (Test-Path $build) {
    Remove-Item -Recurse -Force $build
    Write-Host "[Luma] removed $build" -ForegroundColor Yellow
} else {
    Write-Host "[Luma] nothing to clean" -ForegroundColor Yellow
}
