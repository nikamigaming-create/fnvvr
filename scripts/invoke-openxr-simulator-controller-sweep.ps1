[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataDirectory,
    [ValidateSet("left", "right")][string]$Hand = "right",
    [ValidateRange(2, 120)][int]$DurationSeconds = 14,
    [ValidateRange(0.0, 0.5)][double]$XAmplitudeMeters = 0.10,
    [ValidateRange(0.0, 0.5)][double]$YAmplitudeMeters = 0.10,
    [ValidateRange(0.0, 0.5)][double]$ZAmplitudeMeters = 0.10,
    [ValidateRange(0.0, 60.0)][double]$YawAmplitudeDegrees = 15.0,
    [ValidateRange(0.0, 45.0)][double]$PitchAmplitudeDegrees = 15.0,
    [ValidateRange(0.0, 30.0)][double]$RollAmplitudeDegrees = 15.0,
    [ValidateRange(100, 5000)][int]$ConsumeTimeoutMilliseconds = 2000
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if (-not (Test-Path -LiteralPath $DataDirectory -PathType Container)) {
    New-Item -ItemType Directory -Path $DataDirectory -Force | Out-Null
}
$DataDirectory = (Resolve-Path -LiteralPath $DataDirectory).Path
$inputDriver = Join-Path $PSScriptRoot "invoke-openxr-simulator-input.ps1"
if (-not (Test-Path -LiteralPath $inputDriver -PathType Leaf)) {
    throw "The simulator controller IPC driver is missing: $inputDriver"
}

# These are absolute LOCAL-space poses. They are intentionally not offsets
# from the HMD: a real tracked hand must remain fixed when only the headset
# moves, and it must move when only this controller stream moves.
$radiansPerDegree = [Math]::PI / 180.0
$baseline = [ordered]@{
    x = if ($Hand -ceq "left") { -0.20 } else { 0.20 }
    y = 1.40
    z = -0.40
    yaw = 0.0
    pitch = -0.30
    roll = 0.0
}
$steps = @(
    [ordered]@{ axis = "center"; direction = 0; x = $baseline.x; y = $baseline.y; z = $baseline.z; yaw = $baseline.yaw; pitch = $baseline.pitch; roll = $baseline.roll },
    [ordered]@{ axis = "translationX"; direction = -1; x = $baseline.x - $XAmplitudeMeters; y = $baseline.y; z = $baseline.z; yaw = $baseline.yaw; pitch = $baseline.pitch; roll = $baseline.roll },
    [ordered]@{ axis = "translationX"; direction = 1; x = $baseline.x + $XAmplitudeMeters; y = $baseline.y; z = $baseline.z; yaw = $baseline.yaw; pitch = $baseline.pitch; roll = $baseline.roll },
    [ordered]@{ axis = "translationY"; direction = -1; x = $baseline.x; y = $baseline.y - $YAmplitudeMeters; z = $baseline.z; yaw = $baseline.yaw; pitch = $baseline.pitch; roll = $baseline.roll },
    [ordered]@{ axis = "translationY"; direction = 1; x = $baseline.x; y = $baseline.y + $YAmplitudeMeters; z = $baseline.z; yaw = $baseline.yaw; pitch = $baseline.pitch; roll = $baseline.roll },
    [ordered]@{ axis = "translationZ"; direction = -1; x = $baseline.x; y = $baseline.y; z = $baseline.z - $ZAmplitudeMeters; yaw = $baseline.yaw; pitch = $baseline.pitch; roll = $baseline.roll },
    [ordered]@{ axis = "translationZ"; direction = 1; x = $baseline.x; y = $baseline.y; z = $baseline.z + $ZAmplitudeMeters; yaw = $baseline.yaw; pitch = $baseline.pitch; roll = $baseline.roll },
    [ordered]@{ axis = "yaw"; direction = -1; x = $baseline.x; y = $baseline.y; z = $baseline.z; yaw = $baseline.yaw - $YawAmplitudeDegrees * $radiansPerDegree; pitch = $baseline.pitch; roll = $baseline.roll },
    [ordered]@{ axis = "yaw"; direction = 1; x = $baseline.x; y = $baseline.y; z = $baseline.z; yaw = $baseline.yaw + $YawAmplitudeDegrees * $radiansPerDegree; pitch = $baseline.pitch; roll = $baseline.roll },
    [ordered]@{ axis = "pitch"; direction = -1; x = $baseline.x; y = $baseline.y; z = $baseline.z; yaw = $baseline.yaw; pitch = $baseline.pitch - $PitchAmplitudeDegrees * $radiansPerDegree; roll = $baseline.roll },
    [ordered]@{ axis = "pitch"; direction = 1; x = $baseline.x; y = $baseline.y; z = $baseline.z; yaw = $baseline.yaw; pitch = $baseline.pitch + $PitchAmplitudeDegrees * $radiansPerDegree; roll = $baseline.roll },
    [ordered]@{ axis = "roll"; direction = -1; x = $baseline.x; y = $baseline.y; z = $baseline.z; yaw = $baseline.yaw; pitch = $baseline.pitch; roll = $baseline.roll - $RollAmplitudeDegrees * $radiansPerDegree },
    [ordered]@{ axis = "roll"; direction = 1; x = $baseline.x; y = $baseline.y; z = $baseline.z; yaw = $baseline.yaw; pitch = $baseline.pitch; roll = $baseline.roll + $RollAmplitudeDegrees * $radiansPerDegree },
    [ordered]@{ axis = "center"; direction = 0; x = $baseline.x; y = $baseline.y; z = $baseline.z; yaw = $baseline.yaw; pitch = $baseline.pitch; roll = $baseline.roll }
)
$holdMilliseconds = [Math]::Max(
    100,
    [int][Math]::Floor(($DurationSeconds * 1000.0) / $steps.Count))
$commands = New-Object 'System.Collections.Generic.List[object]'
$ordinal = 0

foreach ($pose in $steps) {
    ++$ordinal
    $invokeArguments = @{
        DataDirectory = $DataDirectory
        Hand = $Hand
        PoseSpace = "local"
        PosX = [double]$pose.x
        PosY = [double]$pose.y
        PosZ = [double]$pose.z
        Yaw = [double]$pose.yaw
        Pitch = [double]$pose.pitch
        Roll = [double]$pose.roll
        WaitMilliseconds = $ConsumeTimeoutMilliseconds
    }
    $output = @(& $inputDriver @invokeArguments) -join [Environment]::NewLine
    $ack = $output | ConvertFrom-Json -ErrorAction Stop
    if ([string]$ack.command.space -cne "local" -or
        [string]$ack.acknowledgement.command -cne "controller_pose" -or
        -not [bool]$ack.acknowledgement.success) {
        throw "The headless simulator did not acknowledge LOCAL controller pose command $ordinal."
    }
    [void]$commands.Add([ordered]@{
        ordinal = $ordinal
        axis = [string]$pose.axis
        direction = [int]$pose.direction
        x = [double]$pose.x
        y = [double]$pose.y
        z = [double]$pose.z
        yaw = [double]$pose.yaw
        pitch = [double]$pose.pitch
        roll = [double]$pose.roll
        sequence = [int64]$ack.sequence
    })
    if ($ordinal -lt $steps.Count) {
        Start-Sleep -Milliseconds $holdMilliseconds
    }
}

[pscustomobject][ordered]@{
    schema = "fnvxr-headless-simulator-controller-sweep-v1"
    scope = "per-run file IPC only; explicit OpenXR LOCAL controller poses; no HMD, window, focus, keyboard, mouse, registry, game, or simulator GUI control"
    dataDirectory = $DataDirectory
    hand = $Hand
    poseSpace = "local"
    durationSeconds = [double]($holdMilliseconds * ($steps.Count - 1)) / 1000.0
    commandCount = $commands.Count
    commands = @($commands.ToArray())
    headPoseMutated = $false
    centerRestored = $true
} | ConvertTo-Json -Depth 6

# The script deliberately emits one final evidence object. Make a successful
# file-IPC sequence an explicit process success for callers that supervise it
# through a hidden child PowerShell process.
exit 0
