[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataDirectory,
    [ValidateSet("left", "right")][string]$Hand = "right",
    [ValidateRange(-1.0, 1.0)][double]$Trigger = -1.0,
    [ValidateRange(-1.0, 1.0)][double]$Grip = -1.0,
    [ValidateSet("unchanged", "released", "pressed")]
    [string]$Primary = "unchanged",
    [ValidateSet("unchanged", "released", "pressed")]
    [string]$Secondary = "unchanged",
    [ValidateSet("unchanged", "released", "pressed")]
    [string]$Menu = "unchanged",
    [ValidateSet("unchanged", "released", "pressed")]
    [string]$ThumbstickClick = "unchanged",
    [ValidateRange(-2.0, 1.0)][double]$ThumbstickX = -2.0,
    [ValidateRange(-2.0, 1.0)][double]$ThumbstickY = -2.0,
    [double]$PosX = [double]::NaN,
    [double]$PosY = [double]::NaN,
    [double]$PosZ = [double]::NaN,
    [double]$Yaw = [double]::NaN,
    [double]$Pitch = [double]::NaN,
    [switch]$ReleaseAll,
    [ValidateRange(100, 30000)][int]$WaitMilliseconds = 5000
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if (-not (Test-Path -LiteralPath $DataDirectory -PathType Container)) {
    New-Item -ItemType Directory -Path $DataDirectory -Force | Out-Null
}
$DataDirectory = (Resolve-Path -LiteralPath $DataDirectory).Path
$commandPath = Join-Path $DataDirectory "controller_pose_command.json"
$ackPath = Join-Path $DataDirectory "command_ack.json"

# The sequence is an integer so the runtime can echo it without timestamp
# ambiguity. It is unique enough for one bounded local driver invocation.
$sequence = [DateTime]::UtcNow.Ticks
$command = [ordered]@{
    sequence = $sequence
    hand = if ($Hand -ceq "left") { 0 } else { 1 }
}

function Add-ButtonState {
    param(
        [Parameter(Mandatory = $true)][string]$Key,
        [Parameter(Mandatory = $true)][string]$State
    )
    if ($State -ceq "pressed") {
        $command[$Key] = 1
    } elseif ($State -ceq "released") {
        $command[$Key] = 0
    }
}

if ($ReleaseAll) {
    $command.trigger = 0.0
    $command.grip = 0.0
    $command.primary = 0
    $command.secondary = 0
    $command.menu = 0
    $command.thumbstickClick = 0
    $command.thumbstickX = 0.0
    $command.thumbstickY = 0.0
} else {
    if ($Trigger -ge 0.0) { $command.trigger = $Trigger }
    if ($Grip -ge 0.0) { $command.grip = $Grip }
    Add-ButtonState -Key "primary" -State $Primary
    Add-ButtonState -Key "secondary" -State $Secondary
    Add-ButtonState -Key "menu" -State $Menu
    Add-ButtonState -Key "thumbstickClick" -State $ThumbstickClick
    if ($ThumbstickX -ge -1.0) { $command.thumbstickX = $ThumbstickX }
    if ($ThumbstickY -ge -1.0) { $command.thumbstickY = $ThumbstickY }
    foreach ($poseField in @(
        [pscustomobject]@{ key = "posX"; value = $PosX },
        [pscustomobject]@{ key = "posY"; value = $PosY },
        [pscustomobject]@{ key = "posZ"; value = $PosZ },
        [pscustomobject]@{ key = "yaw"; value = $Yaw },
        [pscustomobject]@{ key = "pitch"; value = $Pitch })) {
        if (-not [double]::IsNaN([double]$poseField.value)) {
            $command[$poseField.key] = [double]$poseField.value
        }
    }
}

if ($command.Count -le 2) {
    throw "No simulator input or pose field was requested."
}

$deadline = [DateTime]::UtcNow.AddMilliseconds($WaitMilliseconds)
while (Test-Path -LiteralPath $commandPath -PathType Leaf) {
    if ([DateTime]::UtcNow -ge $deadline) {
        throw "A previous simulator command was not consumed: $commandPath"
    }
    Start-Sleep -Milliseconds 10
}

$json = $command | ConvertTo-Json -Compress
$temporaryPath = Join-Path $DataDirectory (
    "controller_pose_command.{0}.{1}.tmp" -f $PID, $sequence)
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($temporaryPath, $json, $utf8NoBom)
[System.IO.File]::Move($temporaryPath, $commandPath)

$acknowledgement = $null
while ([DateTime]::UtcNow -lt $deadline) {
    if (-not (Test-Path -LiteralPath $commandPath -PathType Leaf) -and
        (Test-Path -LiteralPath $ackPath -PathType Leaf)) {
        try {
            $candidate = Get-Content -LiteralPath $ackPath -Raw |
                ConvertFrom-Json -ErrorAction Stop
            if ([string]$candidate.command -ceq "controller_pose" -and
                [int64]$candidate.sequence -eq $sequence) {
                $acknowledgement = $candidate
                break
            }
        } catch {
            # An older runtime may still be replacing the acknowledgement.
        }
    }
    Start-Sleep -Milliseconds 10
}

if ($null -eq $acknowledgement) {
    throw (
        "The headless runtime did not acknowledge simulator command sequence {0} within {1} ms." -f
        $sequence,
        $WaitMilliseconds)
}
if (-not [bool]$acknowledgement.success) {
    throw "The headless runtime rejected simulator command sequence $sequence."
}

[pscustomobject][ordered]@{
    schema = "fnvxr-headless-simulator-input-v1"
    scope = "per-run file IPC only; no window, focus, keyboard, mouse, registry, or simulator GUI control"
    dataDirectory = $DataDirectory
    sequence = $sequence
    hand = $Hand
    releaseAll = [bool]$ReleaseAll
    command = $command
    acknowledgement = $acknowledgement
} | ConvertTo-Json -Depth 6
