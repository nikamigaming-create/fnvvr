[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataDirectory,
    [ValidateRange(10, 20)][int]$ShotsToEmpty = 13,
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
$ackPath = Join-Path $DataDirectory "command_ack.json"
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)

function Test-SimulatorIpcLeaf {
    param([Parameter(Mandatory = $true)][string]$Path)
    for ($attempt = 1; $attempt -le 5; ++$attempt) {
        try {
            return Test-Path -LiteralPath $Path -PathType Leaf
        } catch [System.UnauthorizedAccessException] {
            if ($attempt -ge 5) { throw }
        } catch [System.IO.IOException] {
            if ($attempt -ge 5) { throw }
        }
        Start-Sleep -Milliseconds (10 * $attempt)
    }
}

$intervalMilliseconds = [int][Math]::Round(1000.0 / $UpdatesPerSecond)
$radiansPerDegree = [Math]::PI / 180.0
$stopwatch = [Diagnostics.Stopwatch]::StartNew()
$tick = 0
$lastTrigger = $false
$reloadPressed = $false
$reloadReleased = $false
$triggerPresses = 0
$commands = 0
# Hold each trigger edge long enough for the retail input/animation pipeline
# and leave a full semi-auto recovery interval between shots. The old two-tick
# pulse was acknowledged by IPC but was too short to remain visible in-game.
$ticksPerShot = 9
$triggerHeldTicks = 4
$reloadTicks = 24
$settleTicks = 10
$emptyingTicks = $ShotsToEmpty * $ticksPerShot
$confirmationStartTick = $emptyingTicks + $reloadTicks + $settleTicks
$totalTicks = $confirmationStartTick +
    ($ShotsAfterReload * $ticksPerShot) + $settleTicks
$locomotionStartTick = 12
$locomotionCardinalTicks = 12
$locomotionStopTick = $locomotionStartTick + 4 * $locomotionCardinalTicks
$locomotionPhase = -1
$locomotionReleased = $false

function Invoke-SimulatorInput {
    param([hashtable]$Arguments)
    $output = $null
    for ($attempt = 1; $attempt -le 5; ++$attempt) {
        try {
            $output = @(& $inputDriver @Arguments) -join [Environment]::NewLine
            break
        } catch {
            if ($attempt -ge 5 -or
                $_.Exception.Message -notmatch 'access.*denied') {
                throw
            }
            Start-Sleep -Milliseconds (20 * $attempt)
        }
    }
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
    while (Test-SimulatorIpcLeaf -Path $headCommandPath) {
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
    for ($attempt = 1; $attempt -le 5; ++$attempt) {
        try {
            [System.IO.File]::WriteAllText(
                $temporaryPath,
                ($command | ConvertTo-Json -Compress),
                $utf8NoBom)
            break
        } catch [System.UnauthorizedAccessException] {
            if ($attempt -ge 5) { throw }
            Start-Sleep -Milliseconds (20 * $attempt)
        }
    }
    $published = $false
    while (-not $published -and [DateTime]::UtcNow -lt $deadline) {
        try {
            [System.IO.File]::Move($temporaryPath, $headCommandPath)
            $published = $true
        } catch [System.UnauthorizedAccessException] {
            Start-Sleep -Milliseconds 5
        } catch [System.IO.IOException] {
            Start-Sleep -Milliseconds 5
        }
    }
    if (-not $published) {
        throw "Could not publish head sample $Ordinal within the bounded IPC timeout."
    }

    # The simulator exposes one acknowledgement file for both controller and
    # head commands. Do not return merely because the head command disappeared:
    # if the next controller command is published first, this head ack can
    # overwrite its sequence-specific controller ack and create a false IPC
    # timeout. Fully serialize the two command types here.
    $consumedAt = $null
    while ([DateTime]::UtcNow -lt $deadline) {
        $commandConsumed = -not (Test-SimulatorIpcLeaf -Path $headCommandPath)
        if ($commandConsumed -and $null -eq $consumedAt) {
            $consumedAt = [DateTime]::UtcNow
        }
        if ($commandConsumed -and
            (Test-SimulatorIpcLeaf -Path $ackPath)) {
            $headRejected = $false
            try {
                $candidate = Get-Content -LiteralPath $ackPath -Raw |
                    ConvertFrom-Json -ErrorAction Stop
                if ([string]$candidate.command -ceq "head_pose") {
                    if ([bool]$candidate.success) { return }
                    $headRejected = $true
                }
            } catch {
                # The runtime may be replacing the acknowledgement atomically.
            }
            if ($headRejected) {
                throw "The simulator rejected head sample $Ordinal."
            }
        }
        # This function does not publish the next controller command until it
        # returns. Removal of this exact per-run head command therefore proves
        # runtime consumption even if a delayed duplicate controller ack
        # replaces the shared acknowledgement file. The failed run at sample
        # 101 demonstrated precisely that ordering: command consumed, runtime
        # pose updated, stale controller ack left in the single ack slot.
        if ($null -ne $consumedAt -and
            ([DateTime]::UtcNow - $consumedAt).TotalMilliseconds -ge 250) {
            return
        }
        Start-Sleep -Milliseconds 5
    }
    throw "The simulator did not acknowledge head sample $Ordinal."
}

# Normalize a fixture that may have been saved with a partial magazine. This
# is still controller input through the simulator and the real engine reload
# consumer; it prevents a previous trial's ammo state from shifting the
# recorded empty/reload/two-shot sequence.
Invoke-SimulatorInput -Arguments @{
    DataDirectory = $DataDirectory
    Hand = "left"
    Primary = "pressed"
    WaitMilliseconds = $ConsumeTimeoutMilliseconds
}
Start-Sleep -Milliseconds 100
Invoke-SimulatorInput -Arguments @{
    DataDirectory = $DataDirectory
    Hand = "left"
    Primary = "released"
    WaitMilliseconds = $ConsumeTimeoutMilliseconds
}
Start-Sleep -Milliseconds 2400

while ($tick -lt $totalTicks) {
    $seconds = $stopwatch.Elapsed.TotalSeconds
    # A slow Lissajous aim path crosses left, center, right, high, and low
    # target regions with meaningful reach. Smooth per-tick interpolation
    # avoids the old cardinal-step jerk while preserving full 6DoF rotation.
    $phase = 2.0 * [Math]::PI * 0.085 * $seconds
    # Deliberately large, slow presentation motion: cross the view left/right,
    # lift/drop the pistol, move it in/out, and visibly point the muzzle both
    # above and below the horizon. This must read as hand control on video,
    # not as idle weapon sway.
    # Stay inside a human arm envelope while still making an unmistakably
    # large cowboy presentation: cross-body aim, raised muzzle, forward point,
    # and a deep right-hip/side drop that reads as a holster posture.
    $x = 0.18 + 0.34 * [Math]::Sin($phase)
    $y = 1.27 + 0.33 * [Math]::Sin(1.3 * $phase + 0.7)
    $z = -0.42 + 0.24 * [Math]::Sin(0.8 * $phase + 1.4)
    $yaw = 55.0 * $radiansPerDegree * [Math]::Sin($phase)
    $pitch = -0.20 + 50.0 * $radiansPerDegree *
        [Math]::Sin(1.1 * $phase + 0.5)
    $roll = 28.0 * $radiansPerDegree *
        [Math]::Sin(0.9 * $phase + 1.1)

    $trigger = $false
    if ($tick -lt $emptyingTicks) {
        $trigger = ($tick % $ticksPerShot) -lt $triggerHeldTicks
    } elseif ($tick -ge $confirmationStartTick -and
        $tick -lt $confirmationStartTick +
            ($ShotsAfterReload * $ticksPerShot)) {
        $trigger = (($tick - $confirmationStartTick) % $ticksPerShot) -lt
            $triggerHeldTicks
    }
    if ($trigger -and -not $lastTrigger) { ++$triggerPresses }

    # Prove the comfort-turn latch concurrently with the hand pose: one right
    # snap, neutral rearm, then one left snap. Sustained deflection spans
    # several updates so a held stick would be obvious in telemetry.
    $rightStickX = 0.0
    if ($tick -ge 24 -and $tick -lt 30) { $rightStickX = 1.0 }
    elseif ($tick -ge 36 -and $tick -lt 42) { $rightStickX = -1.0 }

    ++$commands
    Invoke-SimulatorInput -Arguments @{
        DataDirectory = $DataDirectory
        Hand = "right"
        PoseSpace = "local"
        PosX = $x; PosY = $y; PosZ = $z
        Yaw = $yaw; Pitch = $pitch; Roll = $roll
        Trigger = if ($trigger) { 1.0 } else { 0.0 }
        ThumbstickX = $rightStickX
        WaitMilliseconds = $ConsumeTimeoutMilliseconds
    }
    $lastTrigger = $trigger

    # Sustain a real left-stick deflection long enough to cross multiple
    # Fallout movement updates. The product supervisor accepts this only when
    # the retail player's world coordinates measurably change.
    $requestedLocomotionPhase = if ($tick -lt $locomotionStartTick) {
        -1
    } elseif ($tick -lt $locomotionStopTick) {
        [int](($tick - $locomotionStartTick) / $locomotionCardinalTicks)
    } else {
        4
    }
    if ($requestedLocomotionPhase -ne $locomotionPhase) {
        $locomotionPhase = $requestedLocomotionPhase
        $leftX = 0.0
        $leftY = 0.0
        switch ($locomotionPhase) {
            0 { $leftY = 1.0 }                 # forward
            1 { $leftX = -1.0; $leftY = 0.29 } # noisy hard-left
            2 { $leftY = -1.0 }                # backward
            3 { $leftX = 1.0 }                 # right
            4 { $locomotionReleased = $true }  # neutral
        }
        ++$commands
        Invoke-SimulatorInput -Arguments @{
            DataDirectory = $DataDirectory
            Hand = "left"
            ThumbstickX = $leftX
            ThumbstickY = $leftY
            WaitMilliseconds = $ConsumeTimeoutMilliseconds
        }
    }

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
        -X (0.040 * [Math]::Sin($headPhase)) `
        -Y (1.70 + 0.030 * [Math]::Sin($headPhase + 1.1)) `
        -Z (0.030 * [Math]::Sin($headPhase + 2.2)) `
        -Yaw (12.0 * $radiansPerDegree * [Math]::Sin($headPhase)) `
        -Pitch (8.0 * $radiansPerDegree * [Math]::Sin($headPhase + 1.3)) `
        -Roll (3.0 * $radiansPerDegree * [Math]::Sin($headPhase + 2.6)) `
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
while (Test-SimulatorIpcLeaf -Path $headCommandPath) {
    if ([DateTime]::UtcNow -ge $headDeadline) {
        throw "The simulator did not consume the final centered head pose."
    }
    Start-Sleep -Milliseconds 5
}
Invoke-SimulatorInput -Arguments @{
    DataDirectory = $DataDirectory
    Hand = "left"
    ReleaseAll = $true
    WaitMilliseconds = $ConsumeTimeoutMilliseconds
}

[pscustomobject][ordered]@{
    schema = "fnvxr-headless-combat-demo-v1"
    scope = "per-run OpenXR simulator file IPC only; no desktop, window, focus, keyboard, mouse, registry, game UI, or simulator UI control"
    durationSeconds = $stopwatch.Elapsed.TotalSeconds
    updatesPerSecond = $UpdatesPerSecond
    controllerPoseSpace = "local"
    controllerPath = "smooth multi-target Lissajous 6DoF"
    xAmplitudeMeters = 0.34
    yAmplitudeMeters = 0.33
    zAmplitudeMeters = 0.24
    yawAmplitudeDegrees = 55.0
    pitchAmplitudeDegrees = 50.0
    rollAmplitudeDegrees = 28.0
    shotsToEmpty = $ShotsToEmpty
    reloads = 1
    preparationReloads = 1
    shotsAfterReload = $ShotsAfterReload
    triggerPresses = $triggerPresses
    commandCount = $commands + 2
    centerRestored = $true
    controlsReleased = $true
    locomotion = [ordered]@{
        hand = "left"
        pattern = "forward, noisy-hard-left, backward, right, neutral"
        noisyHardLeft = [ordered]@{ x = -1.0; y = 0.29; expected = "left-only" }
        startTick = $locomotionStartTick
        stopTick = $locomotionStopTick
        durationSeconds = [double]($locomotionStopTick -
            $locomotionStartTick) / $UpdatesPerSecond
        neutralRestored = $locomotionReleased
        acceptance = "retail player world-position delta required"
    }
    snapTurn = [ordered]@{
        degrees = 30
        pattern = "right-held, neutral, left-held, neutral"
        expectedTurns = 2
        heldStickRepeatForbidden = $true
    }
    headMotion = [ordered]@{
        pattern = "gentle-sinusoidal-v1"
        xAmplitudeMeters = 0.040
        yAmplitudeMeters = 0.030
        zAmplitudeMeters = 0.030
        yawAmplitudeDegrees = 12.0
        pitchAmplitudeDegrees = 8.0
        rollAmplitudeDegrees = 3.0
        concurrentWithController = $true
        centerRestored = $true
    }
} | ConvertTo-Json -Depth 5

exit 0
