# DRIFT build helper.
#
# Usage:
#   .\build.ps1                # Configure + build release
#   .\build.ps1 -Debug         # Configure + build debug
#   .\build.ps1 -Clean         # Wipe build dir first
#   .\build.ps1 -Run           # Run the standalone after build
param(
    [switch]$Debug,
    [switch]$Clean,
    [switch]$Run
)

$ErrorActionPreference = 'Stop'

$preset    = if ($Debug) { 'debug' }   else { 'default' }
$buildDir  = if ($Debug) { 'build-debug' } else { 'build' }

# Kill a running standalone first — the linker can't overwrite a locked .exe.
Get-Process -Name DRIFT -ErrorAction SilentlyContinue | Stop-Process -Force

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "Removing $buildDir..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $buildDir
}

Write-Host "Configuring (preset: $preset)..." -ForegroundColor Cyan
& "C:\Program Files\CMake\bin\cmake.exe" --preset $preset
if ($LASTEXITCODE -ne 0) { throw "Configure failed" }

Write-Host "Building..." -ForegroundColor Cyan
& "C:\Program Files\CMake\bin\cmake.exe" --build --preset $preset --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

# JUCE puts the standalone at:
#   <build>/DRIFT_artefacts/<Config>/Standalone/DRIFT.exe
$config = if ($Debug) { 'Debug' } else { 'Release' }
$exe = Join-Path $buildDir "DRIFT_artefacts/$config/Standalone/DRIFT.exe"

if (Test-Path $exe) {
    Write-Host "Built: $exe" -ForegroundColor Green
    if ($Run) {
        Write-Host "Launching..." -ForegroundColor Cyan
        & $exe
    }
} else {
    Write-Warning "Expected exe not found at $exe"
}
