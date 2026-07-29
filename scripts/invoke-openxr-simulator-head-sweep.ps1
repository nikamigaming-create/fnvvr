[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataDirectory,
    [ValidateRange(2, 120)][int]$DurationSeconds = 12,
    [ValidateRange(2, 30)][int]$UpdatesPerSecond = 8,
    [ValidateRange(0.0, 0.5)][double]$XAmplitudeMeters = 0.10,
    [ValidateRange(0.0, 0.5)][double]$YAmplitudeMeters = 0.06,
    [ValidateRange(0.0, 0.5)][double]$ZAmplitudeMeters = 0.08,
    [ValidateRange(0.0, 60.0)][double]$YawAmplitudeDegrees = 20.0,
    [ValidateRange(0.0, 45.0)][double]$PitchAmplitudeDegrees = 10.0,
    [ValidateRange(0.0, 30.0)][double]$RollAmplitudeDegrees = 6.0,
    [ValidateRange(0.05, 1.0)][double]$FrequencyHz = 0.16,
    [ValidateRange(100, 5000)][int]$ConsumeTimeoutMilliseconds = 2000
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if (-not (Test-Path -LiteralPath $DataDirectory -PathType Container)) {
    New-Item -ItemType Directory -Path $DataDirectory -Force | Out-Null
}
$DataDirectory = (Resolve-Path -LiteralPath $DataDirectory).Path
$commandPath = Join-Path $DataDirectory "head_pose_command.json"
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$radiansPerDegree = [Math]::PI / 180.0
$intervalMilliseconds = [Math]::Max(
    1,
    [int][Math]::Round(1000.0 / [double]$UpdatesPerSecond))

function Wait-HeadPoseCommandConsumed {
    $deadline =
        [DateTime]::UtcNow.AddMilliseconds($ConsumeTimeoutMilliseconds)
    while (Test-Path -LiteralPath $commandPath -PathType Leaf) {
        if ([DateTime]::UtcNow -ge $deadline) {
            throw "The headless runtime did not consume its head-pose command: $commandPath"
        }
        Start-Sleep -Milliseconds 5
    }
}

function Publish-HeadPoseCommand {
    param(
        [Parameter(Mandatory = $true)][double]$X,
        [Parameter(Mandatory = $true)][double]$Y,
        [Parameter(Mandatory = $true)][double]$Z,
        [Parameter(Mandatory = $true)][double]$Yaw,
        [Parameter(Mandatory = $true)][double]$Pitch,
        [Parameter(Mandatory = $true)][double]$Roll,
        [Parameter(Mandatory = $true)][int]$Ordinal
    )

    Wait-HeadPoseCommandConsumed
    $command = [ordered]@{
        x = $X
        y = $Y
        z = $Z
        yaw = $Yaw
        pitch = $Pitch
        roll = $Roll
    }
    $temporaryPath = Join-Path $DataDirectory (
        "head_pose_command.{0}.{1}.tmp" -f $PID, $Ordinal)
    [System.IO.File]::WriteAllText(
        $temporaryPath,
        ($command | ConvertTo-Json -Compress),
        $utf8NoBom)
    [System.IO.File]::Move($temporaryPath, $commandPath)
}

$stopwatch = [Diagnostics.Stopwatch]::StartNew()
$published = 0
$minimum = [ordered]@{
    x = [double]::PositiveInfinity
    y = [double]::PositiveInfinity
    z = [double]::PositiveInfinity
    yaw = [double]::PositiveInfinity
    pitch = [double]::PositiveInfinity
    roll = [double]::PositiveInfinity
}
$maximum = [ordered]@{
    x = [double]::NegativeInfinity
    y = [double]::NegativeInfinity
    z = [double]::NegativeInfinity
    yaw = [double]::NegativeInfinity
    pitch = [double]::NegativeInfinity
    roll = [double]::NegativeInfinity
}

while ($stopwatch.Elapsed.TotalSeconds -lt $DurationSeconds) {
    $seconds = $stopwatch.Elapsed.TotalSeconds
    $phase = 2.0 * [Math]::PI * $FrequencyHz * $seconds
    $pose = [ordered]@{
        x = $XAmplitudeMeters * [Math]::Sin($phase)
        y = 1.7 + $YAmplitudeMeters * [Math]::Sin($phase + 1.1)
        z = $ZAmplitudeMeters * [Math]::Sin($phase + 2.2)
        yaw = $YawAmplitudeDegrees * $radiansPerDegree *
            [Math]::Sin($phase)
        pitch = $PitchAmplitudeDegrees * $radiansPerDegree *
            [Math]::Sin($phase + 1.3)
        roll = $RollAmplitudeDegrees * $radiansPerDegree *
            [Math]::Sin($phase + 2.6)
    }
    ++$published
    Publish-HeadPoseCommand `
        -X $pose.x `
        -Y $pose.y `
        -Z $pose.z `
        -Yaw $pose.yaw `
        -Pitch $pose.pitch `
        -Roll $pose.roll `
        -Ordinal $published
    foreach ($key in $pose.Keys) {
        $minimum[$key] = [Math]::Min(
            [double]$minimum[$key],
            [double]$pose[$key])
        $maximum[$key] = [Math]::Max(
            [double]$maximum[$key],
            [double]$pose[$key])
    }
    Start-Sleep -Milliseconds $intervalMilliseconds
}

# Always return the simulated headset to its deterministic local-space center
# and prove the runtime consumed that final command before returning.
$published += 1
Publish-HeadPoseCommand `
    -X 0.0 `
    -Y 1.7 `
    -Z 0.0 `
    -Yaw 0.0 `
    -Pitch 0.0 `
    -Roll 0.0 `
    -Ordinal $published
Wait-HeadPoseCommandConsumed

[pscustomobject][ordered]@{
    schema = "fnvxr-headless-simulator-head-sweep-v1"
    scope = "per-run HMD pose file IPC only; controllers retain their runtime-defined head-relative tracking; no window, focus, keyboard, mouse, registry, game, or simulator GUI control"
    dataDirectory = $DataDirectory
    durationSeconds = $stopwatch.Elapsed.TotalSeconds
    updatesPerSecond = $UpdatesPerSecond
    commandCount = $published
    frequencyHz = $FrequencyHz
    commandedMinimum = $minimum
    commandedMaximum = $maximum
    centerRestored = $true
} | ConvertTo-Json -Depth 5
