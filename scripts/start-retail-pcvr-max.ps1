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
    [int]$WorldProofStableSamples = 12,
    [int]$AutomatedMenuTimeoutSeconds = 60,
    [string]$AutomatedSaveName = "FNVXR_HostExitRecovery",
    [switch]$StopExisting,
    [switch]$FocusOnLaunch,
    [switch]$DisableStereoWorld,
    [switch]$EnableStereoWorld,
    [switch]$DiscoverShaderContracts,
    [switch]$EnableRetailRig,
    [switch]$ApplyRetailRig,
    [switch]$DisableRetailRig,
    [switch]$ApplyTestLoadout,
    [switch]$AutomateContinue,
    [string]$D3D9ShaderWvpContracts = "",
    [string]$VerifiedShaderDiscoveryRunDir = "",
    [string]$D3D9ShaderAllowVertexHashes = "",
    [switch]$StageOnly,
    [switch]$ValidateOnly
)

$ErrorActionPreference = "Stop"

& (Join-Path $PSScriptRoot "audit-launch-safety.ps1") | Out-Host

if ([string]::IsNullOrWhiteSpace($D3D9ShaderWvpContracts) -and
    -not [string]::IsNullOrWhiteSpace($VerifiedShaderDiscoveryRunDir)) {
    $D3D9ShaderWvpContracts = & (Join-Path $PSScriptRoot "get-verified-shader-wvp-contracts.ps1") `
        -RunDir $VerifiedShaderDiscoveryRunDir `
        -ContractsOnly
    if ([string]::IsNullOrWhiteSpace($D3D9ShaderWvpContracts)) {
        throw "Verified shader discovery produced an empty contract string."
    }
    Write-Host ("Verified {0} shader-contract characters from {1}." -f `
        $D3D9ShaderWvpContracts.Length, $VerifiedShaderDiscoveryRunDir)
}

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
    WorldProofStableSamples = $WorldProofStableSamples
    AutomatedMenuTimeoutSeconds = $AutomatedMenuTimeoutSeconds
    AutomatedSaveName = $AutomatedSaveName
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
if (($EnableStereoWorld -or $DiscoverShaderContracts) -and $DisableStereoWorld) {
    throw "-EnableStereoWorld/-DiscoverShaderContracts and -DisableStereoWorld are mutually exclusive."
}
if ($DiscoverShaderContracts) {
    # Safe producer discovery: run the exact native stereo traversal and dump
    # strong-fingerprint shader bytecode and keep headset presentation on the
    # known transport. The rig remains read-only unless explicitly requested.
    $launchArgs.EnableStereoWorld = $true
    $launchArgs.StereoProducerProofOnly = $true
    if (-not ($EnableRetailRig -or $ApplyRetailRig)) {
        $launchArgs.DisableRetailRig = $true
    }
} elseif (-not $DisableStereoWorld) {
    # Full headset runs keep proof telemetry but disable the high-frequency
    # per-draw hammer; it added more than 100 ms to some Fallout traversals.
    $launchArgs.NoTelemetryHammer = $true
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
if ($AutomateContinue) {
    $launchArgs.AutomateContinue = $true
}
if ($StageOnly) {
    $launchArgs.StageOnly = $true
}
if ($ValidateOnly) {
    $launchArgs.ValidateOnly = $true
}

& (Join-Path $PSScriptRoot "start-openxr-retail-sidecar.ps1") @launchArgs
