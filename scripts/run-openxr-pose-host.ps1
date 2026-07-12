param(
    [string]$Configuration = "Debug",
    [int]$Frames = 0,
    [switch]$AllowFiniteHostRun
)

$ErrorActionPreference = "Stop"

if ($Frames -gt 0 -and -not $AllowFiniteHostRun) {
    throw "Unsafe host run refused: finite runs (-Frames $Frames) require -AllowFiniteHostRun so they cannot be mistaken for a stable PCVR launch."
}

$Root = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $Root "build"

cmake -S $Root -B $BuildDir
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed with exit code $LASTEXITCODE"
}

cmake --build $BuildDir --config $Configuration --target fnvxr_openxr_pose_host
if ($LASTEXITCODE -ne 0) {
    throw "OpenXR pose host build failed with exit code $LASTEXITCODE"
}

$PoseHost = Join-Path $BuildDir "$Configuration\fnvxr_openxr_pose_host.exe"
if (-not (Test-Path -LiteralPath $PoseHost)) {
    throw "Missing OpenXR pose host executable: $PoseHost"
}

& $PoseHost $Frames
if ($LASTEXITCODE -ne 0) {
    throw "OpenXR pose host failed with exit code $LASTEXITCODE"
}
