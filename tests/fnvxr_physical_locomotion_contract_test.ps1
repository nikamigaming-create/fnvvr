param(
    [Parameter(Mandatory = $true)][string]$SourceRoot
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

function Require-Text {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Required,
        [Parameter(Mandatory = $true)][string]$Reason
    )

    if (-not $Text.Contains($Required)) {
        throw "Physical locomotion contract missing ${Reason}: $Required"
    }
}

function Get-RequiredFileText {
    param([Parameter(Mandatory = $true)][string]$RelativePath)

    $path = Join-Path $SourceRoot $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Physical locomotion contract file is missing: $path"
    }
    return Get-Content -LiteralPath $path -Raw
}

$hostSource = Get-RequiredFileText "host\fnvxr_openxr_pose_host.cpp"
$plugin = Get-RequiredFileText "plugin\fnvxr_nvse_plugin.cpp"
$authority = Get-RequiredFileText "runtime\fnvxr_physical_input_authority.h"
$proxySafety = Get-RequiredFileText "renderhook\fnvxr_input_proxy_safety.h"

foreach ($required in @(
        '#include "fnvxr_physical_input_authority.h"',
        'physicalGameplayAuthority',
        'physicalGameplayAuthority.granted()',
        'locomotionControlsActive',
        'physicalLocomotionHostAuthority',
        '#include "fnvxr_stereo_visual_trial.h"',
        'const bool presentedBinocularWorld = productionBinocularWorld')) {
    Require-Text -Text $hostSource -Required $required -Reason "host authority/render boundary"
}

foreach ($required in @(
        'GameplayAuthorityBlocker',
        'ControllerConsumerUnacknowledged',
        'MenuOwnsInput',
        'RuntimeNotGameplay',
        'GameplayClassificationUnavailable',
        'InProcessNvseDirectInput',
        'classifyLocomotion')) {
    Require-Text -Text $authority -Required $required -Reason "shared physical authority boundary"
}

foreach ($required in @(
        '#include "fnvxr_physical_input_authority.h"',
        'bool holdGameplayMovementKey(UInt32 keycode, bool held)',
        'physicalLocomotionDirectInput',
        'g_directInputHook->keys[keycode].hold = held;',
        'physicalLocomotionConsumerAuthority',
        'physicalLocomotionFinal',
        'physicalLocomotionRearmPending',
        'physicalLocomotionAllowed',
        'holdGameplayMovementKey(DIK_W, finalForwardHeld)',
        'holdGameplayMovementKey(DIK_A, moveLeft)',
        'holdGameplayMovementKey(DIK_S, moveBackward)',
        'holdGameplayMovementKey(DIK_D, moveRight)')) {
    Require-Text -Text $plugin -Required $required -Reason "xNVSE final-consumer boundary"
}

$legacyStart = $plugin.IndexOf('void updateControllerAxes(')
$legacyEnd = $plugin.IndexOf('HWND currentProcessWindow()', $legacyStart)
if ($legacyStart -lt 0 -or $legacyEnd -le $legacyStart) {
    throw "Could not isolate the legacy pose-axis helper."
}
$legacyAxes = $plugin.Substring($legacyStart, $legacyEnd - $legacyStart)
if ($legacyAxes.Contains('holdGameplayMovementKey(')) {
    throw "Legacy pose-axis handling must not bypass the authenticated external physical bridge."
}

foreach ($required in @(
        'ProductionInputMutationProofComplete = false',
        'ProductInputControllerIntegrated = false',
        'NvseMainGameLoop')) {
    Require-Text -Text $proxySafety -Required $required -Reason "transparent proxy fuse"
}

Write-Host "Physical locomotion authority and consumer contracts passed."
