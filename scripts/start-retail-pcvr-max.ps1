param(
    [string]$Configuration = "Release",
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
    [int]$WorldProofTimeoutSeconds = 30,
    [switch]$StopExisting,
    [switch]$FocusOnLaunch,
    [switch]$DisableStereoWorld,
    [switch]$EnableStereoWorld,
    [switch]$EnableRetailRig,
    [switch]$ApplyRetailRig,
    [switch]$DisableRetailRig,
    [switch]$ApplyTestLoadout,
    [string]$D3D9ShaderWvpContracts = "",
    [string]$D3D9ShaderAllowVertexHashes = "",
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
if ($EnableStereoWorld -and $DisableStereoWorld) {
    throw "-EnableStereoWorld and -DisableStereoWorld are mutually exclusive."
}
if (-not $DisableStereoWorld) {
    if ([string]::IsNullOrWhiteSpace($D3D9ShaderWvpContracts) -and -not ($StageOnly -or $ValidateOnly)) {
        throw "Full VR is fail-closed until -D3D9ShaderWvpContracts supplies reviewed fnv8/sha256/byteCount@register@column-or-row contracts. Run the producer-only shader discovery profile first."
    }
    $launchArgs.EnableStereoWorld = $true
    if (-not [string]::IsNullOrWhiteSpace($D3D9ShaderWvpContracts)) {
        $launchArgs.EnableD3D9ShaderStereo = $true
        $launchArgs.D3D9ShaderWvpContracts = $D3D9ShaderWvpContracts
        if (-not [string]::IsNullOrWhiteSpace($D3D9ShaderAllowVertexHashes)) {
            $launchArgs.D3D9ShaderAllowVertexHashes = $D3D9ShaderAllowVertexHashes
        }
    }
}
if ($EnableRetailRig -or $ApplyRetailRig) {
    $launchArgs.EnableRetailRig = $true
}
if ($ApplyRetailRig) {
    $launchArgs.ApplyRetailRig = $true
}
if ($DisableRetailRig) {
    $launchArgs.DisableRetailRig = $true
}
if ($ApplyTestLoadout) {
    $launchArgs.ApplyTestLoadout = $true
}
if ($StageOnly) {
    $launchArgs.StageOnly = $true
}
if ($ValidateOnly) {
    $launchArgs.ValidateOnly = $true
}

& (Join-Path $PSScriptRoot "start-openxr-retail-sidecar.ps1") @launchArgs
