[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$RunDirectory,
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

if (-not (Test-Path -LiteralPath $RunDirectory -PathType Container)) {
    throw "Physical cardinal-script run directory does not exist: $RunDirectory"
}
$runDirectoryFull = (Resolve-Path -LiteralPath $RunDirectory).Path
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $runDirectoryFull "phase1-physical-cardinal-script.json"
} elseif (-not [System.IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath = Join-Path $runDirectoryFull $OutputPath
}
$outputFull = [System.IO.Path]::GetFullPath($OutputPath)
$runPrefix = $runDirectoryFull.TrimEnd('\') + '\'
if (-not $outputFull.StartsWith(
        $runPrefix,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Physical cardinal-script output must stay below its run directory: $outputFull"
}

function New-FnvxrPhysicalCardinalStep {
    param(
        [Parameter(Mandatory = $true)][int]$Ordinal,
        [Parameter(Mandatory = $true)][string]$Axis,
        [Parameter(Mandatory = $true)][int]$Direction,
        [Parameter(Mandatory = $true)][string]$Target,
        [Parameter(Mandatory = $true)][string]$Instruction
    )
    return [ordered]@{
        ordinal = $Ordinal
        axis = $Axis
        direction = $Direction
        target = $Target
        instruction = $Instruction
        holdSeconds = 2
        returnToCenterBeforeNext = $true
    }
}

$steps = @(
    (New-FnvxrPhysicalCardinalStep -Ordinal 1 -Axis "translationX" -Direction -1 -Target "-100 mm X" -Instruction "Move only along negative local X, then return to the recentered neutral pose.")
    (New-FnvxrPhysicalCardinalStep -Ordinal 2 -Axis "translationX" -Direction 1 -Target "+100 mm X" -Instruction "Move only along positive local X, then return to the recentered neutral pose.")
    (New-FnvxrPhysicalCardinalStep -Ordinal 3 -Axis "translationY" -Direction -1 -Target "-100 mm Y" -Instruction "Move only along negative local Y, then return to the recentered neutral pose.")
    (New-FnvxrPhysicalCardinalStep -Ordinal 4 -Axis "translationY" -Direction 1 -Target "+100 mm Y" -Instruction "Move only along positive local Y, then return to the recentered neutral pose.")
    (New-FnvxrPhysicalCardinalStep -Ordinal 5 -Axis "translationZ" -Direction -1 -Target "-100 mm Z" -Instruction "Move only along negative local Z, then return to the recentered neutral pose.")
    (New-FnvxrPhysicalCardinalStep -Ordinal 6 -Axis "translationZ" -Direction 1 -Target "+100 mm Z" -Instruction "Move only along positive local Z, then return to the recentered neutral pose.")
    (New-FnvxrPhysicalCardinalStep -Ordinal 7 -Axis "yaw" -Direction -1 -Target "-15 degrees yaw" -Instruction "Rotate only negative yaw about local up, then return to the recentered neutral pose.")
    (New-FnvxrPhysicalCardinalStep -Ordinal 8 -Axis "yaw" -Direction 1 -Target "+15 degrees yaw" -Instruction "Rotate only positive yaw about local up, then return to the recentered neutral pose.")
    (New-FnvxrPhysicalCardinalStep -Ordinal 9 -Axis "pitch" -Direction -1 -Target "-15 degrees pitch" -Instruction "Rotate only negative pitch, then return to the recentered neutral pose.")
    (New-FnvxrPhysicalCardinalStep -Ordinal 10 -Axis "pitch" -Direction 1 -Target "+15 degrees pitch" -Instruction "Rotate only positive pitch, then return to the recentered neutral pose.")
    (New-FnvxrPhysicalCardinalStep -Ordinal 11 -Axis "roll" -Direction -1 -Target "-15 degrees roll" -Instruction "Rotate only negative roll, then return to the recentered neutral pose.")
    (New-FnvxrPhysicalCardinalStep -Ordinal 12 -Axis "roll" -Direction 1 -Target "+15 degrees roll" -Instruction "Rotate only positive roll, then return to the recentered neutral pose.")
)

$cardinalScript = [ordered]@{
    schema = "fnvxr-phase1-physical-cardinal-script-v1"
    generatedAtUtc = [DateTime]::UtcNow.ToString("o")
    runDirectory = $runDirectoryFull
    scope = "operator-guided physical HMD movement only; this script sends no XR, game, controller, desktop, keyboard, mouse, registry, or simulator input"
    prerequisites = @(
        "Link or Air Link is active and the HMD remains awake through XR_SESSION_STATE_READY.",
        "The retail world is loaded, the operator is standing or seated at the declared origin, and a clean recenter is retained before step 1.",
        "Each step begins at the same neutral pose; do not turn the body except for an explicit turn or recenter evidence segment."
    )
    steps = $steps
    requiredEvidence = @(
        "host sample, pose producer epoch, reference-space generation, and rendered display time",
        "recentered origin, center/left/right camera transforms, renderer/culler camera match, CPU-v8 publication, and OpenXR submit",
        "head/body invariant showing no actor or locomotion-heading drift during head-only steps",
        "restore/reacquire records after recenter, UI, load, cell, third-person, death, tracking, or runtime changes"
    )
    acceptance = "This protocol is a physical evidence script, not an acceptance result. A simulator or a completed checklist cannot set fullProductAccepted."
}

$temporaryPath = "{0}.{1}.tmp" -f $outputFull, $PID
$encoding = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText(
    $temporaryPath,
    ($cardinalScript | ConvertTo-Json -Depth 8),
    $encoding)
Move-Item -LiteralPath $temporaryPath -Destination $outputFull -Force

$cardinalScript | ConvertTo-Json -Depth 8
