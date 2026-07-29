param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$GameRoot = $(if ($env:FNVXR_GAME_ROOT) {
        $env:FNVXR_GAME_ROOT
    } elseif (Test-Path -LiteralPath "D:\SteamLibrary\steamapps\common\Fallout New Vegas\FalloutNV.exe" -PathType Leaf) {
        "D:\SteamLibrary\steamapps\common\Fallout New Vegas"
    } else {
        "C:\Program Files (x86)\Steam\steamapps\common\Fallout New Vegas"
    }),
    [switch]$RequireNoRunningGame,
    [switch]$RequireAutomationRecoverySave
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

# This is intentionally a read-only preflight.  It does not build, stage,
# start, stop, or configure a process.  The returned environment is a review
# record for a later, explicitly approved desktop-only run.
$Root = Split-Path -Parent $PSScriptRoot
$Win32Build = Join-Path $Root "build-product-win32\$Configuration"
$X64Build = Join-Path $Root "build-product-x64\$Configuration"

function Get-DesktopAssistFileRecord {
    param(
        [Parameter(Mandatory = $true)][string]$Key,
        [Parameter(Mandatory = $true)][string]$Path,
        [bool]$Required = $true
    )

    $exists = Test-Path -LiteralPath $Path -PathType Leaf
    $hash = $null
    $bytes = $null
    if ($exists) {
        $item = Get-Item -LiteralPath $Path
        $bytes = [int64]$item.Length
        $hash = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    return [ordered]@{
        key = $Key
        path = $Path
        required = $Required
        exists = $exists
        bytes = $bytes
        sha256 = $hash
    }
}

$gameRootResolved = $null
if (Test-Path -LiteralPath $GameRoot -PathType Container) {
    $gameRootResolved = (Resolve-Path -LiteralPath $GameRoot).Path
}

$artifactRecords = @(
    (Get-DesktopAssistFileRecord -Key "win32Plugin" -Path (Join-Path $Win32Build "nvse_fnvxr.dll"))
    (Get-DesktopAssistFileRecord -Key "win32D3D9Proxy" -Path (Join-Path $Win32Build "d3d9.dll"))
    (Get-DesktopAssistFileRecord -Key "assistHarness" -Path (Join-Path $X64Build "fnvxr_assist.exe"))
    (Get-DesktopAssistFileRecord -Key "stateProbe" -Path (Join-Path $X64Build "fnvxr_shared_state_probe.exe"))
    (Get-DesktopAssistFileRecord -Key "retailRuntimeProbe" -Path (Join-Path $X64Build "fnvxr_retail_runtime_probe.exe"))
    (Get-DesktopAssistFileRecord -Key "commandHelper" -Path (Join-Path $X64Build "fnvxr_command.exe"))
    (Get-DesktopAssistFileRecord -Key "gameExecutable" -Path (Join-Path $GameRoot "FalloutNV.exe"))
    Get-DesktopAssistFileRecord -Key "nvseLoader" -Path (Join-Path $GameRoot "nvse_loader.exe")
)

$installedPlugin = Get-DesktopAssistFileRecord `
    -Key "installedPluginObservation" `
    -Path (Join-Path $GameRoot "Data\NVSE\Plugins\nvse_fnvxr.dll") `
    -Required $false

$installedD3D9Proxy = Get-DesktopAssistFileRecord `
    -Key "installedD3D9ProxyObservation" `
    -Path (Join-Path $GameRoot "d3d9.dll") `
    -Required $false

$automationRecoverySave = Get-DesktopAssistFileRecord `
    -Key "automationRecoverySave" `
    -Path (Join-Path ([Environment]::GetFolderPath("MyDocuments")) "My Games\FalloutNV\Saves\FNVXR_HostExitRecovery.fos") `
    -Required ([bool]$RequireAutomationRecoverySave)

$requiredArtifactsPresent = @($artifactRecords | Where-Object { $_.required -and -not $_.exists }).Count -eq 0
$runningFallout = @(Get-Process -Name "FalloutNV" -ErrorAction SilentlyContinue | ForEach-Object {
    [ordered]@{ id = $_.Id; path = $_.Path }
})
$noRunningGame = $runningFallout.Count -eq 0

$environment = [ordered]@{
    FNVXR_RUN_PROFILE = "desktop-assist"
    FNVXR_DESKTOP_ASSIST_CAMERA_ONLY = "1"
    FNVXR_DESKTOP_ASSIST_UI_CAPTURE = "1"
    FNVXR_INSTALL_CAMERA_HOOK = "1"
    FNVXR_CAMERA_HOOK = "1"
    FNVXR_CAMERA_APPLY = "1"
    FNVXR_CAMERA_APPLY_ROTATION = "1"
    FNVXR_CAMERA_YAW_ONLY = "0"
    FNVXR_CAMERA_APPLY_TRANSLATION = "0"
    FNVXR_CAMERA_POSITION_SCALE = "0"
    FNVXR_CAMERA_WRITE_WORLD = "0"
    FNVXR_CAMERA_UPDATE_TRANSFORM = "0"
    FNVXR_RETAIL_RIG_ENABLE = "0"
    FNVXR_RETAIL_WEAPON_APPLY = "0"
    FNVXR_ENABLE_ENGINE_CENTER_STEREO = "0"
    FNVXR_ENABLE_LEGACY_IMAGE_DIAGNOSTICS = "0"
    FNVXR_DISABLE_STEREO_WORLD = "1"
}

$result = [ordered]@{
    schema = "fnvxr-desktop-assist-preflight-v1"
    scope = "read-only preflight; does not build, stage, launch, stop, mutate a game tree, or set process environment"
    gameRootRequested = $GameRoot
    gameRootResolved = $gameRootResolved
    configuration = $Configuration
    requiredArtifactsPresent = $requiredArtifactsPresent
    noRunningGame = $noRunningGame
    requireNoRunningGame = [bool]$RequireNoRunningGame
    runningFallout = $runningFallout
    artifacts = $artifactRecords
    installedPluginObservation = $installedPlugin
    installedD3D9ProxyObservation = $installedD3D9Proxy
    automation = [ordered]@{
        supported = $true
        requested = [bool]$RequireAutomationRecoverySave
        fixedRecoverySave = $automationRecoverySave
        scope = "when separately requested by the approved supervisor: fixed recovery load plus two verified Escape taps; no OpenXR, controller, mouse, or arbitrary command input"
    }
    requiredEnvironment = $environment
    desktopAssistLimits = @(
        "rotation-local camera only",
        "no local translation",
        "no world-transform write",
        "no unverified Ni transform update",
        "no input injection",
        "optional unattended acceptance is separately opt-in and limited to the fixed recovery save plus two exact foreground-verified Escape taps",
        "no weapon or rig mutation",
        "menu-only CPU backbuffer capture to a dedicated local mapping",
        "optional external process-memory evidence collection is read-only and does not authorize stereo",
        "no world stereo, legacy D3D replay, or OpenXR presentation"
    )
    nextStep = "Requires explicit approval before any temporary stage or desktop game launch."
}

$result | ConvertTo-Json -Depth 6

if (-not $requiredArtifactsPresent) {
    exit 2
}
if ($RequireNoRunningGame -and -not $noRunningGame) {
    exit 3
}
if ($RequireAutomationRecoverySave -and -not $automationRecoverySave.exists) {
    exit 4
}
exit 0
