param(
    [string]$Configuration = "Debug",
    [string]$GameRoot = $(if ($env:FNVXR_GAME_ROOT) { $env:FNVXR_GAME_ROOT } else { "C:\Program Files (x86)\Steam\steamapps\common\Fallout New Vegas" }),
    [int]$GameBackbufferWidth = 2048,
    [int]$GameBackbufferHeight = 1280,
    [int]$HostGameTextureWidth = 3072,
    [int]$HostGameTextureHeight = 1920,
    [int]$UiWidth = 1280,
    [int]$UiHeight = 720,
    [int]$HostReadyTimeoutSeconds = 45,
    [int]$RetailReadyTimeoutSeconds = 60,
    [int]$WorldEntryTimeoutSeconds = 300,
    [int]$WorldProofTimeoutSeconds = 0,
    [switch]$StopExisting,
    [switch]$FocusOnLaunch,
    [switch]$DisableStereoWorld,
    [switch]$EnableRetailRig,
    [switch]$ApplyRetailRig,
    [switch]$StageOnly,
    [switch]$ValidateOnly
)

$ErrorActionPreference = "Stop"

& (Join-Path $PSScriptRoot "audit-launch-safety.ps1") | Out-Host

$launchArgs = @{
    Configuration = $Configuration
    GameRoot = $GameRoot
    Frames = 0
    UiWidth = $UiWidth
    UiHeight = $UiHeight
    HostReadyTimeoutSeconds = $HostReadyTimeoutSeconds
    RetailReadyTimeoutSeconds = $RetailReadyTimeoutSeconds
    WorldEntryTimeoutSeconds = $WorldEntryTimeoutSeconds
    WorldProofTimeoutSeconds = $WorldProofTimeoutSeconds
}

# Let the OpenXR launcher select its verified 1664x1808 runtime-aspect tier
# unless the caller deliberately supplies custom producer/host dimensions.
foreach ($dimensionName in @(
    "GameBackbufferWidth",
    "GameBackbufferHeight",
    "HostGameTextureWidth",
    "HostGameTextureHeight")) {
    if ($PSBoundParameters.ContainsKey($dimensionName)) {
        $launchArgs[$dimensionName] = Get-Variable -Name $dimensionName -ValueOnly
    }
}

if ($StopExisting) {
    $launchArgs.StopExisting = $true
}
if ($FocusOnLaunch) {
    $launchArgs.FocusRetailWindow = $true
}
if (-not $DisableStereoWorld) {
    $launchArgs.EnableStereoWorld = $true
}
if ($EnableRetailRig -or $ApplyRetailRig) {
    $launchArgs.EnableRetailRig = $true
}
if ($ApplyRetailRig) {
    $launchArgs.ApplyRetailRig = $true
}
if ($StageOnly) {
    $launchArgs.StageOnly = $true
}
if ($ValidateOnly) {
    $launchArgs.ValidateOnly = $true
}

& (Join-Path $PSScriptRoot "start-openxr-retail-sidecar.ps1") @launchArgs
