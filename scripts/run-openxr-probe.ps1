param(
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"

$OpenXrDiagnosticRuntimeProofComplete = $false
if (-not $OpenXrDiagnosticRuntimeProofComplete) {
    throw "OpenXR diagnostic is intentionally blocked before configure, build, or OpenXR runtime touch: its reviewed source proof is incomplete."
}

$Root = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $Root "build"

cmake -S $Root -B $BuildDir
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed with exit code $LASTEXITCODE"
}

cmake --build $BuildDir --config $Configuration --target fnvxr_openxr_probe
if ($LASTEXITCODE -ne 0) {
    throw "OpenXR probe build failed with exit code $LASTEXITCODE"
}

$Probe = Join-Path $BuildDir "$Configuration\fnvxr_openxr_probe.exe"
if (-not (Test-Path -LiteralPath $Probe)) {
    throw "Missing OpenXR probe executable: $Probe"
}

& $Probe
if ($LASTEXITCODE -ne 0) {
    throw "OpenXR probe failed with exit code $LASTEXITCODE"
}
