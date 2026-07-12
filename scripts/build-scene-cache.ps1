param(
    [string]$Configuration = "Release",
    [string]$GameRoot = $(if ($env:FNVXR_GAME_ROOT) { $env:FNVXR_GAME_ROOT } else { "C:\Program Files (x86)\Steam\steamapps\common\Fallout New Vegas" }),
    [string]$SceneName = "goodsprings-doc-mitchell",
    [int]$WarmupSeconds = 14,
    [int]$CaptureTimeoutSeconds = 45,
    [switch]$AllowMono,
    [switch]$AllowNonWorldCandidate
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$CacheRoot = Join-Path $Root "local\scene-cache"

Write-Host "starting scene producer..."
& (Join-Path $PSScriptRoot "start-rock-solid.ps1") `
    -Configuration $Configuration `
    -GameRoot $GameRoot `
    -ScenePipelineMode producer `
    -StopExisting `
    -Frames 7200 `
    -AllowFiniteHostRun

Write-Host "warming producer for $WarmupSeconds seconds..."
Start-Sleep -Seconds $WarmupSeconds

try {
    $captureArgs = @(
        "-ExecutionPolicy", "Bypass",
        "-File", (Join-Path $PSScriptRoot "capture-scene-cache.ps1"),
        "-OutputRoot", $CacheRoot,
        "-SceneName", $SceneName,
        "-TimeoutSeconds", $CaptureTimeoutSeconds
    )
    if ($AllowMono) {
        $captureArgs += "-AllowMono"
    }
    if ($AllowNonWorldCandidate) {
        $captureArgs += "-AllowNonWorldCandidate"
    }

    & powershell @captureArgs
}
finally {
    Write-Host "stopping scene producer..."
    Get-Process FalloutNV,nvse_loader,fnvxr_openxr_pose_host -ErrorAction SilentlyContinue |
        Stop-Process -Force -ErrorAction SilentlyContinue
}

Write-Host "scene cache root: $CacheRoot"
