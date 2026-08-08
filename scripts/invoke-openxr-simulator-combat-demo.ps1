[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataDirectory,
    [ValidateRange(10, 20)][int]$ShotsToEmpty = 14,
    [ValidateRange(1, 5)][int]$ShotsAfterReload = 2,
    [ValidateRange(8, 30)][int]$UpdatesPerSecond = 12,
    [ValidateRange(100, 10000)][int]$ConsumeTimeoutMilliseconds = 5000
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$inputDriver = Join-Path $PSScriptRoot "invoke-openxr-simulator-input.ps1"
if (-not (Test-Path -LiteralPath $inputDriver -PathType Leaf)) {
    throw "The simulator input driver is missing: $inputDriver"
}
$headCommandPath = Join-Path $DataDirectory "head_pose_command.json"
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)

$intervalMilliseconds = [int][Math]::Round(1000.0 / $UpdatesPerSecond)
$radiansPerDegree = [Math]::PI / 180.0
$stopwatch = [Diagnostics.Stopwatch]::StartNew()
$tick = 0
$lastTrigger = $false
$reloadPressed = $false
$reloadReleased = $false
$triggerPresses = 0
$commands = 0
$ticksPerShot = 6
$reloadTicks = 18
$settleTicks = 8
$emptyingTicks = $ShotsToEmpty * $ticksPerShot
$confirmationStartTick = $emptyingTicks + $reloadTicks + $settleTicks
$totalTicks = $confirmationStartTick +
    ($ShotsAfterReload * $ticksPerShot) + $settleTicks

function Invoke-SimulatorInput {
    param([hashtable]$Arguments)
    $output = @(& $inputDriver @Arguments) -join [Environment]::NewLine
    $ack = $output | ConvertFrom-Json -ErrorAction Stop
    if (-not [bool]$ack.acknowledgement.success) {
        throw "The simulator rejected combat-demo command $commands."
    }
}

function Publish-HeadPose {
    param(
        [double]$X, [double]$Y, [double]$Z,
        [double]$Yaw, [double]$Pitch, [double]$Roll,
        [int]$Ordinal
    )
    $deadline = [DateTime]::UtcNow.AddMilliseconds(
        $ConsumeTimeoutMilliseconds)
    while (Test-Path -LiteralPath $headCommandPath -PathType Leaf) {
        if ([DateTime]::UtcNow -ge $deadline) {
            throw "The simulator did not consume head sample $Ordinal."
        }
        Start-Sleep -Milliseconds 5
    }
    $temporaryPath = Join-Path $DataDirectory (
        "head_pose_command.{0}.{1}.tmp" -f $PID, $Ordinal)
    $command = [ordered]@{
        x = $X; y = $Y; z = $Z
        yaw = $Yaw; pitch = $Pitch; roll = $Roll
    }
    [System.IO.File]::WriteAllText(
        $temporaryPath,
        ($command | ConvertTo-Json -Compress),
        $utf8NoBom)
    [System.IO.File]::Move($temporaryPath, $headCommandPath)
}

while ($tick -lt $totalTicks) {
    $seconds = $stopwatch.Elapsed.TotalSeconds
    # A slow Lissajous aim path crosses left, center, right, high, and low
    # target regions with meaningful reach. Smooth per-tick interpolation
    # avoids the old cardinal-step jerk while preserving full 6DoF rotation.
    $phase = 2.0 * [Math]::PI * 0.115 * $seconds
    $x = 0.20 + 0.22 * [Math]::Sin($phase)
    $y = 1.40 + 0.14 * [Math]::Sin(1.7 * $phase + 0.7)
    $z = -0.40 + 0.16 * [Math]::Sin(1.3 * $phase + 1.4)
    $yaw = 28.0 * $radiansPerDegree * [Math]::Sin($phase)
    $pitch = -0.30 + 18.0 * $radiansPerDegree *
        [Math]::Sin(1.5 * $phase + 0.5)
    $roll = 12.0 * $radiansPerDegree *
        [Math]::Sin(1.2 * $phase + 1.1)

    $trigger = $false
    if ($tick -lt $emptyingTicks) {
        $trigger = ($tick % $ticksPerShot) -lt 2
    } elseif ($tick -ge $confirmationStartTick -and
        $tick -lt $confirmationStartTick +
            ($ShotsAfterReload * $ticksPerShot)) {
        $trigger = (($tick - $confirmationStartTick) % $ticksPerShot) -lt 2
    }
    if ($trigger -and -not $lastTrigger) { ++$triggerPresses }

    ++$commands
    Invoke-SimulatorInput -Arguments @{
        DataDirectory = $DataDirectory
        Hand = "right"
        PoseSpace = "local"
        PosX = $x; PosY = $y; PosZ = $z
        Yaw = $yaw; Pitch = $pitch; Roll = $roll
        Trigger = if ($trigger) { 1.0 } else { 0.0 }
        WaitMilliseconds = $ConsumeTimeoutMilliseconds
    }
    $lastTrigger = $trigger

    if (-not $reloadPressed -and $tick -ge $emptyingTicks + 2) {
        ++$commands
        Invoke-SimulatorInput -Arguments @{
            DataDirectory = $DataDirectory
            Hand = "left"
            Primary = "pressed"
            WaitMilliseconds = $ConsumeTimeoutMilliseconds
        }
        $reloadPressed = $true
    }
    if (-not $reloadReleased -and
        $tick -ge $emptyingTicks + $reloadTicks - 2) {
        ++$commands
        Invoke-SimulatorInput -Arguments @{
            DataDirectory = $DataDirectory
            Hand = "left"
            Primary = "released"
            WaitMilliseconds = $ConsumeTimeoutMilliseconds
        }
        $reloadReleased = $true
    }

    # Publish the matching gentle head sample only after the controller
    # acknowledgement is complete. Both poses advance in the same visual
    # tick without racing over the simulator's single acknowledgement file.
    $headPhase = 2.0 * [Math]::PI * 0.12 * $seconds
    Publish-HeadPose `
        -X (0.025 * [Math]::Sin($headPhase)) `
        -Y (1.70 + 0.020 * [Math]::Sin($headPhase + 1.1)) `
        -Z (0.020 * [Math]::Sin($headPhase + 2.2)) `
        -Yaw (6.0 * $radiansPerDegree * [Math]::Sin($headPhase)) `
        -Pitch (4.0 * $radiansPerDegree * [Math]::Sin($headPhase + 1.3)) `
        -Roll (2.0 * $radiansPerDegree * [Math]::Sin($headPhase + 2.6)) `
        -Ordinal ($tick + 1)

    ++$tick
    $nextTickMilliseconds = $tick * $intervalMilliseconds
    $remaining = $nextTickMilliseconds - $stopwatch.ElapsedMilliseconds
    if ($remaining -gt 0) { Start-Sleep -Milliseconds $remaining }
}

# Deterministic rest pose and released controls leave no input held.
Invoke-SimulatorInput -Arguments @{
    DataDirectory = $DataDirectory
    Hand = "right"
    PoseSpace = "local"
    PosX = 0.20; PosY = 1.40; PosZ = -0.40
    Yaw = 0.0; Pitch = -0.30; Roll = 0.0
    Trigger = 0.0
    WaitMilliseconds = $ConsumeTimeoutMilliseconds
}
Publish-HeadPose `
    -X 0.0 -Y 1.70 -Z 0.0 `
    -Yaw 0.0 -Pitch 0.0 -Roll 0.0 `
    -Ordinal ($totalTicks + 1)
$headDeadline = [DateTime]::UtcNow.AddMilliseconds(
    $ConsumeTimeoutMilliseconds)
while (Test-Path -LiteralPath $headCommandPath -PathType Leaf) {
    if ([DateTime]::UtcNow -ge $headDeadline) {
        throw "The simulator did not consume the final centered head pose."
    }
    Start-Sleep -Milliseconds 5
}
Invoke-SimulatorInput -Arguments @{
    DataDirectory = $DataDirectory
    Hand = "left"
    Primary = "released"
    WaitMilliseconds = $ConsumeTimeoutMilliseconds
}

[pscustomobject][ordered]@{
    schema = "fnvxr-headless-combat-demo-v1"
    scope = "per-run OpenXR simulator file IPC only; no desktop, window, focus, keyboard, mouse, registry, game UI, or simulator UI control"
    durationSeconds = $stopwatch.Elapsed.TotalSeconds
    updatesPerSecond = $UpdatesPerSecond
    controllerPoseSpace = "local"
    controllerPath = "smooth multi-target Lissajous 6DoF"
    xAmplitudeMeters = 0.22
    yAmplitudeMeters = 0.14
    zAmplitudeMeters = 0.16
    yawAmplitudeDegrees = 28.0
    pitchAmplitudeDegrees = 18.0
    rollAmplitudeDegrees = 12.0
    shotsToEmpty = $ShotsToEmpty
    reloads = 1
    shotsAfterReload = $ShotsAfterReload
    triggerPresses = $triggerPresses
    commandCount = $commands + 2
    centerRestored = $true
    controlsReleased = $true
    headMotion = [ordered]@{
        pattern = "gentle-sinusoidal-v1"
        xAmplitudeMeters = 0.025
        yAmplitudeMeters = 0.020
        zAmplitudeMeters = 0.020
        yawAmplitudeDegrees = 6.0
        pitchAmplitudeDegrees = 4.0
        rollAmplitudeDegrees = 2.0
        concurrentWithController = $true
        centerRestored = $true
    }
} | ConvertTo-Json -Depth 5

exit 0
