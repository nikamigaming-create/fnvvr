[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataDirectory,
    [Parameter(Mandatory = $true)]
    [ValidateRange(-3.2, 3.2)][double]$ScreenYaw,
    [Parameter(Mandatory = $true)]
    [ValidateRange(-1.5, 1.5)][double]$ScreenPitch,
    [Parameter(Mandatory = $true)]
    [ValidateRange(-3.2, 3.2)][double]$ItemsYaw,
    [Parameter(Mandatory = $true)]
    [ValidateRange(-1.5, 1.5)][double]$ItemsPitch,
    [ValidateRange(100, 5000)][int]$ConsumeTimeoutMilliseconds = 3000
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$inputDriver = Join-Path $PSScriptRoot "invoke-openxr-simulator-input.ps1"
if (-not (Test-Path -LiteralPath $inputDriver -PathType Leaf)) {
    throw "The simulator controller IPC driver is missing: $inputDriver"
}
$commands = New-Object 'System.Collections.Generic.List[object]'

function Invoke-PipBoyStep {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][double]$Yaw,
        [Parameter(Mandatory = $true)][double]$Pitch,
        [double]$Grip = 1.0,
        [double]$Trigger = 0.0,
        [ValidateSet("unchanged", "released", "pressed")]
        [string]$Secondary = "released",
        [ValidateRange(0, 7000)][int]$HoldMilliseconds = 150
    )
    $output = @(
        & $inputDriver `
            -DataDirectory $DataDirectory `
            -Hand right `
            -PoseSpace local `
            -PosX 0.18 `
            -PosY 1.37 `
            -PosZ -0.24 `
            -Yaw $Yaw `
            -Pitch $Pitch `
            -Roll 0.0 `
            -Grip $Grip `
            -Trigger $Trigger `
            -Secondary $Secondary `
            -WaitMilliseconds $ConsumeTimeoutMilliseconds
    ) -join [Environment]::NewLine
    $ack = $output | ConvertFrom-Json -ErrorAction Stop
    if ([string]$ack.command.space -cne "local" -or
        [string]$ack.acknowledgement.command -cne "controller_pose" -or
        -not [bool]$ack.acknowledgement.success) {
        throw "The simulator did not acknowledge Pip-Boy proof step '$Name'."
    }
    [void]$commands.Add([ordered]@{
        name = $Name
        sequence = [int64]$ack.sequence
        yaw = $Yaw
        pitch = $Pitch
        grip = $Grip
        trigger = $Trigger
        secondary = $Secondary
        holdMilliseconds = $HoldMilliseconds
    })
    if ($HoldMilliseconds -gt 0) {
        Start-Sleep -Milliseconds $HoldMilliseconds
    }
}

# These analytic LOCAL-space rays are derived by the supervisor from the live
# WeaponFrame v3 retail hand-to-screen calibration. No guessed plane offset
# remains in the proof.
Invoke-PipBoyStep -Name "screen-hover" -Yaw $ScreenYaw -Pitch $ScreenPitch `
    -HoldMilliseconds 500
Invoke-PipBoyStep -Name "items-tab-hover" -Yaw $ItemsYaw -Pitch $ItemsPitch `
    -HoldMilliseconds 700
Invoke-PipBoyStep -Name "items-tab-trigger-down" -Yaw $ItemsYaw -Pitch $ItemsPitch `
    -Trigger 1.0 -HoldMilliseconds 400
Invoke-PipBoyStep -Name "items-tab-trigger-up" -Yaw $ItemsYaw -Pitch $ItemsPitch `
    -Trigger 0.0 -HoldMilliseconds 700
Invoke-PipBoyStep -Name "screen-trigger-down" -Yaw $ScreenYaw -Pitch $ScreenPitch `
    -Trigger 1.0 -HoldMilliseconds 150
Invoke-PipBoyStep -Name "menu-content-hold" -Yaw $ScreenYaw -Pitch $ScreenPitch `
    -Trigger 0.0 -HoldMilliseconds 6500
Invoke-PipBoyStep -Name "close-b-down" -Yaw $ScreenYaw -Pitch $ScreenPitch `
    -Secondary pressed -HoldMilliseconds 150
Invoke-PipBoyStep -Name "close-b-up" -Yaw $ScreenYaw -Pitch $ScreenPitch `
    -Secondary released -HoldMilliseconds 400

$releaseOutput = @(
    & $inputDriver `
        -DataDirectory $DataDirectory `
        -Hand right `
        -ReleaseAll `
        -WaitMilliseconds $ConsumeTimeoutMilliseconds
) -join [Environment]::NewLine
$releaseAck = $releaseOutput | ConvertFrom-Json -ErrorAction Stop
if (-not [bool]$releaseAck.acknowledgement.success) {
    throw "The simulator did not acknowledge the Pip-Boy proof release."
}

[pscustomobject][ordered]@{
    schema = "fnvxr-headless-pipboy-menu-proof/v1"
    scope = "per-run OpenXR controller file IPC only: grip-gated wrist focus, native pointer/tab trigger, native B close, and release; no window, focus, cursor, keyboard, mouse, registry, or simulator GUI control"
    commandCount = $commands.Count + 1
    commands = @($commands.ToArray())
    releaseSequence = [int64]$releaseAck.sequence
    gripGated = $true
    aimSource = "WeaponFrame-v3-stock-left-hand-to-screen"
    screenAim = [ordered]@{ yaw = $ScreenYaw; pitch = $ScreenPitch }
    itemsAim = [ordered]@{ yaw = $ItemsYaw; pitch = $ItemsPitch }
    pointerTargets = @("items-tab", "screen")
    closeBinding = "right B -> native Pip-Boy close"
} | ConvertTo-Json -Depth 8
