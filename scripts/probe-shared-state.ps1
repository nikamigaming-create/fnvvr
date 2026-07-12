param(
    [string]$Configuration = "Debug",
    [switch]$RequirePlayer,
    [switch]$RequireOpenMwPlayer,
    [switch]$RequirePlayerMatch,
    [double]$PlayerMatchTolerance = 1.0,
    [double]$PlayerRotationTolerance = 0.01,
    [switch]$RequireRuntime,
    [switch]$RequireCamera,
    [switch]$RequireVideo,
    [switch]$RequireStereo,
    [switch]$RequireWorldStereo,
    [switch]$RequirePose,
    [switch]$RequireAdvancing,
    [int]$SampleDelayMs = 250
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $Root "build"

cmake -S $Root -B $BuildDir
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed with exit code $LASTEXITCODE"
}

cmake --build $BuildDir --config $Configuration --target fnvxr_shared_state_probe
if ($LASTEXITCODE -ne 0) {
    throw "Shared-state probe build failed with exit code $LASTEXITCODE"
}

$ProbeExe = Join-Path $BuildDir "$Configuration\fnvxr_shared_state_probe.exe"
if (-not (Test-Path -LiteralPath $ProbeExe)) {
    throw "Missing shared-state probe: $ProbeExe"
}

$ProbeArgs = @()
if ($RequirePlayer) {
    $ProbeArgs += "--require-player"
}
if ($RequireOpenMwPlayer) {
    $ProbeArgs += "--require-openmw-player"
}
if ($RequirePlayerMatch) {
    $ProbeArgs += "--require-player-match"
    $ProbeArgs += "--match-position-tolerance"
    $ProbeArgs += ([string]$PlayerMatchTolerance)
    $ProbeArgs += "--rotation-tolerance"
    $ProbeArgs += ([string]$PlayerRotationTolerance)
}
if ($RequireRuntime) {
    $ProbeArgs += "--require-runtime"
}
if ($RequireCamera) {
    $ProbeArgs += "--require-camera"
}
if ($RequireVideo) {
    $ProbeArgs += "--require-video"
}
if ($RequireStereo) {
    $ProbeArgs += "--require-stereo"
}
if ($RequireWorldStereo) {
    $ProbeArgs += "--require-world-stereo"
}
if ($RequirePose) {
    $ProbeArgs += "--require-pose"
}
if ($RequireAdvancing) {
    $ProbeArgs += "--require-advancing"
    $ProbeArgs += "--sample-delay-ms"
    $ProbeArgs += ([string]$SampleDelayMs)
}

& $ProbeExe @ProbeArgs
exit $LASTEXITCODE
