param(
    [string]$Configuration = "Release",
    [string]$SceneCacheDir = "",
    [int]$Frames = 7200,
    [switch]$AllowFiniteHostRun
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
if ($Frames -gt 0 -and -not $AllowFiniteHostRun) {
    throw "Unsafe cache-only host run refused: finite runs (-Frames $Frames) require -AllowFiniteHostRun."
}
if (-not $SceneCacheDir) {
    $SceneCacheDir = Join-Path $Root "local\scene-cache-openmw"
}

$hostExe = Join-Path $Root "build\$Configuration\fnvxr_openxr_pose_host.exe"
if (-not (Test-Path -LiteralPath $hostExe)) {
    throw "Host executable not found: $hostExe"
}

Get-Process FalloutNV,nvse_loader,fnvxr_openxr_pose_host -ErrorAction SilentlyContinue |
    Stop-Process -Force -ErrorAction SilentlyContinue

$runStamp = Get-Date -Format "yyyyMMdd-HHmmss"
$runDir = Join-Path $Root "local\cache-only-runs\$runStamp"
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

$env:FNVXR_RUN_PROFILE = "rock-solid"
$env:FNVXR_SCENE_PIPELINE_MODE = "cache-only"
$env:FNVXR_SCENE_CACHE_DIR = $SceneCacheDir
$env:FNVXR_CACHE_ONLY = "1"
$env:FNVXR_HOST_MODE = "vr"
$env:FNVXR_GAME_FULLSCREEN_IN_XR = "1"
$env:FNVXR_USE_STEREO_GAME_TEXTURES = "1"
$env:FNVXR_SHOW_GAME_PLANE = "0"
$env:FNVXR_SHOW_GAME_PLANE_IN_GAME = "0"
$env:FNVXR_SHOW_GAME_PLANE_ON_STEREO_LOSS = "0"
$env:FNVXR_STEREO_FALLBACK_MONO_FULLSCREEN = "0"
$env:FNVXR_ALLOW_DEBUG_SCENE_SHELL = "0"
$env:FNVXR_SHOW_PAUSE_SCENE = "0"

$hostOut = Join-Path $runDir "fnvxr_openxr_pose_host.out.log"
$hostErr = Join-Path $runDir "fnvxr_openxr_pose_host.err.log"
$host = Start-Process `
    -FilePath $hostExe `
    -ArgumentList $Frames `
    -WorkingDirectory $Root `
    -RedirectStandardOutput $hostOut `
    -RedirectStandardError $hostErr `
    -PassThru `
    -WindowStyle Hidden

$manifest = [ordered]@{
    startedAt = (Get-Date).ToString("o")
    profile = "rock-solid"
    scenePipelineMode = "cache-only"
    sceneCacheDir = $SceneCacheDir
    runDir = $runDir
    hostPid = $host.Id
    hostOut = $hostOut
    hostErr = $hostErr
}
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $runDir "manifest.json") -Encoding UTF8
$manifest | ConvertTo-Json -Depth 4
