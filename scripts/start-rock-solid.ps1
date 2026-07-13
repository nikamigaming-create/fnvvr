param(
    [string]$Configuration = "Release",
    [int]$Frames = 0,
    [int]$HostStartupDelaySeconds = 0,
    [string]$GameRoot = $(if ($env:FNVXR_GAME_ROOT) { $env:FNVXR_GAME_ROOT } else { "C:\Program Files (x86)\Steam\steamapps\common\Fallout New Vegas" }),
    [string]$ScenePipelineMode = "consumer",
    [int]$GameBackbufferWidth = 2048,
    [int]$GameBackbufferHeight = 1280,
    [int]$HostGameTextureWidth = 3072,
    [int]$HostGameTextureHeight = 1920,
    [int]$UiSharedWidth = 1280,
    [int]$UiSharedHeight = 720,
    [int]$UiInputWidth = 1280,
    [int]$UiInputHeight = 720,
    [switch]$StopExisting,
    [switch]$FocusOnLaunch,
    [switch]$EnableHostWatchdog,
    [int]$MaxHostRestarts = 0,
    [switch]$AllowFiniteHostRun,
    [switch]$DisableStereoWorld,
    [switch]$StageOnly,
    [switch]$ValidateOnly
)

$ErrorActionPreference = "Stop"

Write-Warning "start-rock-solid.ps1 is deprecated. It no longer contains launch logic; delegating to start-openxr-retail-sidecar.ps1."

if ($HostStartupDelaySeconds -ne 0) {
    Write-Warning "-HostStartupDelaySeconds is ignored. OpenXR host readiness, not a sleep, gates retail launch."
}
if ($ScenePipelineMode -ne "consumer") {
    Write-Warning "-ScenePipelineMode is ignored. The sidecar profile does not mutate retail scenes."
}
if ($EnableHostWatchdog -or $MaxHostRestarts -ne 0) {
    throw "Host watchdog restart loops are not allowed in the OpenXR sidecar path."
}
if ($UiSharedWidth -ne $UiInputWidth -or $UiSharedHeight -ne $UiInputHeight) {
    throw "UI shared grid and input grid must match exactly."
}

$launchArgs = @{
    Configuration = $Configuration
    Frames = $Frames
    GameRoot = $GameRoot
    GameBackbufferWidth = $GameBackbufferWidth
    GameBackbufferHeight = $GameBackbufferHeight
    HostGameTextureWidth = $HostGameTextureWidth
    HostGameTextureHeight = $HostGameTextureHeight
    UiWidth = $UiSharedWidth
    UiHeight = $UiSharedHeight
    EnableStereoWorld = $true
}

if ($DisableStereoWorld) {
    [void]$launchArgs.Remove("EnableStereoWorld")
}

if ($StopExisting) {
    $launchArgs.StopExisting = $true
}
if ($FocusOnLaunch) {
    $launchArgs.FocusRetailWindow = $true
}
if ($AllowFiniteHostRun) {
    $launchArgs.AllowFiniteHostRun = $true
}
if ($StageOnly) {
    $launchArgs.StageOnly = $true
}
if ($ValidateOnly) {
    $launchArgs.ValidateOnly = $true
}

& (Join-Path $PSScriptRoot "start-openxr-retail-sidecar.ps1") @launchArgs
