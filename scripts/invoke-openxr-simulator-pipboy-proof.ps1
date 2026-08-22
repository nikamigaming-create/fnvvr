[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataDirectory,
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
        [ValidateRange(0, 2000)][int]$HoldMilliseconds = 150
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
    })
    if ($HoldMilliseconds -gt 0) {
        Start-Sleep -Milliseconds $HoldMilliseconds
    }
}

# The left wrist is fixed at (-0.20, 1.40, -0.40) in this owned headless
# fixture. These are analytic LOCAL-space rays into the center and left-side
# physical tab area of its enlarged wrist plane.
Invoke-PipBoyStep -Name "screen-hover" -Yaw 1.178 -Pitch 0.159 `
    -HoldMilliseconds 500
Invoke-PipBoyStep -Name "items-tab-hover" -Yaw 1.218 -Pitch 0.145 `
    -HoldMilliseconds 700
Invoke-PipBoyStep -Name "items-tab-trigger-down" -Yaw 1.218 -Pitch 0.145 `
    -Trigger 1.0 -HoldMilliseconds 400
Invoke-PipBoyStep -Name "items-tab-trigger-up" -Yaw 1.218 -Pitch 0.145 `
    -Trigger 0.0 -HoldMilliseconds 700
Invoke-PipBoyStep -Name "screen-trigger-down" -Yaw 1.178 -Pitch 0.159 `
    -Trigger 1.0 -HoldMilliseconds 150
Invoke-PipBoyStep -Name "screen-trigger-up" -Yaw 1.178 -Pitch 0.159 `
    -Trigger 0.0 -HoldMilliseconds 500
Invoke-PipBoyStep -Name "close-b-down" -Yaw 1.178 -Pitch 0.159 `
    -Secondary pressed -HoldMilliseconds 150
Invoke-PipBoyStep -Name "close-b-up" -Yaw 1.178 -Pitch 0.159 `
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
    pointerTargets = @("items-tab", "screen")
    closeBinding = "right B -> native Pip-Boy close"
} | ConvertTo-Json -Depth 8
