[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$RunDirectory,
    # This only makes an incomplete report return a non-zero process result.
    # It never changes product authority or relabels simulator evidence as
    # physical-headset acceptance.
    [switch]$FailOnIncomplete,
    [switch]$NoReport
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

function Get-FnvxrPhase1Property {
    param(
        [AllowNull()]$Object,
        [Parameter(Mandatory = $true)][string]$Name,
        [AllowNull()]$Default = $null
    )

    if ($null -eq $Object) {
        return $Default
    }
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        return $Default
    }
    return $property.Value
}

function Get-FnvxrPhase1PathValue {
    param(
        [AllowNull()]$Object,
        [Parameter(Mandatory = $true)][string[]]$Names,
        [AllowNull()]$Default = $null
    )

    $value = $Object
    foreach ($name in $Names) {
        $value = Get-FnvxrPhase1Property -Object $value -Name $name
        if ($null -eq $value) {
            return $Default
        }
    }
    return $value
}

function ConvertTo-FnvxrPhase1Boolean {
    param([AllowNull()]$Value)

    if ($Value -is [bool]) {
        return [bool]$Value
    }
    return [string]::Equals(
        [string]$Value,
        "true",
        [System.StringComparison]::OrdinalIgnoreCase)
}

function Test-FnvxrPhase1SameScalar {
    param(
        [AllowNull()]$Left,
        [AllowNull()]$Right
    )

    if ($null -eq $Left -or $null -eq $Right) {
        return $false
    }
    [uint64]$leftNumber = 0
    [uint64]$rightNumber = 0
    $style = [System.Globalization.NumberStyles]::Integer
    $culture = [System.Globalization.CultureInfo]::InvariantCulture
    if ([uint64]::TryParse([string]$Left, $style, $culture, [ref]$leftNumber) -and
        [uint64]::TryParse([string]$Right, $style, $culture, [ref]$rightNumber)) {
        return $leftNumber -eq $rightNumber
    }
    return [string]::Equals(
        [string]$Left,
        [string]$Right,
        [System.StringComparison]::Ordinal)
}

function Test-FnvxrPhase1FiniteVector {
    param(
        [AllowNull()]$Value,
        [Parameter(Mandatory = $true)][int]$Count
    )

    $items = @($Value)
    if ($items.Count -ne $Count) {
        return $false
    }
    foreach ($item in $items) {
        try {
            $number = [double]$item
        } catch {
            return $false
        }
        if ([double]::IsNaN($number) -or [double]::IsInfinity($number)) {
            return $false
        }
    }
    return $true
}

function Test-FnvxrPhase1FiniteNonnegativeAtMost {
    param(
        [AllowNull()]$Value,
        [Parameter(Mandatory = $true)][double]$Maximum
    )

    try {
        [double]$number = $Value
    } catch {
        return $false
    }
    return -not [double]::IsNaN($number) -and
        -not [double]::IsInfinity($number) -and
        $number -ge 0.0 -and
        $number -le $Maximum
}

function Resolve-FnvxrPhase1ArtifactPath {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [AllowNull()][string]$Candidate,
        [Parameter(Mandatory = $true)][string]$Fallback
    )

    $selected = if ([string]::IsNullOrWhiteSpace($Candidate)) {
        $Fallback
    } else {
        $Candidate
    }
    if ([System.IO.Path]::IsPathRooted($selected)) {
        return $selected
    }
    return Join-Path $Root $selected
}

function Read-FnvxrPhase1Text {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return ""
    }
    return Get-Content -LiteralPath $Path -Raw
}

function Read-FnvxrPhase1JsonEvents {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$EventName
    )

    $records = New-Object 'System.Collections.Generic.List[object]'
    $parseErrors = 0
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return [pscustomobject]@{
            records = @()
            parseErrors = 0
            present = $false
        }
    }

    $lineNumber = 0
    foreach ($line in @(Get-Content -LiteralPath $Path)) {
        ++$lineNumber
        $jsonStart = $line.IndexOf('{"event"')
        if ($jsonStart -lt 0) {
            continue
        }
        try {
            $record = $line.Substring($jsonStart) | ConvertFrom-Json -ErrorAction Stop
        } catch {
            ++$parseErrors
            continue
        }
        if ([string](Get-FnvxrPhase1Property -Object $record -Name "event" -Default "") -cne $EventName) {
            continue
        }
        $record | Add-Member -NotePropertyName "_line" -NotePropertyValue $lineNumber -Force
        [void]$records.Add($record)
    }
    return [pscustomobject]@{
        records = @($records.ToArray())
        parseErrors = $parseErrors
        present = $true
    }
}

function Get-FnvxrPhase1FailureClassification {
    param(
        [Parameter(Mandatory = $true)]$Manifest,
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$HostErrorText,
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$RetailBridgeText
    )

    $physical = ConvertTo-FnvxrPhase1Boolean (
        Get-FnvxrPhase1PathValue -Object $Manifest -Names @("physicalHeadsetPlay", "requested") -Default $false)
    $hostBridge = ConvertTo-FnvxrPhase1Boolean (
        Get-FnvxrPhase1PathValue -Object $Manifest -Names @("readiness", "hostBridge") -Default $false)
    $hostPose = ConvertTo-FnvxrPhase1Boolean (
        Get-FnvxrPhase1PathValue -Object $Manifest -Names @("readiness", "hostPose") -Default $false)
    $retailRuntimeAndPose = ConvertTo-FnvxrPhase1Boolean (
        Get-FnvxrPhase1PathValue -Object $Manifest -Names @("readiness", "retailRuntimeAndPose") -Default $false)
    $retailBridge = ConvertTo-FnvxrPhase1Boolean (
        Get-FnvxrPhase1PathValue -Object $Manifest -Names @("readiness", "retailVrBridge") -Default $false)
    $error = [string](Get-FnvxrPhase1Property -Object $Manifest -Name "error" -Default "")
    $gameExited = $error -match '(?i)FalloutNV.*exited|retail.*exited|game.*exited'
    $sessionNotReady = $HostErrorText -match '(?i)XR_SESSION_STATE_READY|session.*ready'
    $bridgeInitialized = $RetailBridgeText -match '(?i)retail VR bridge initialized'

    if (-not $physical) {
        $sweepRequested = ConvertTo-FnvxrPhase1Boolean (
            Get-FnvxrPhase1PathValue -Object $Manifest -Names @("headsetPoseSweep", "requested") -Default $false)
        if ($sweepRequested) {
            return [ordered]@{
                code = "headless-simulator-evidence"
                terminal = $false
                detail = "Headless simulator evidence is retained separately and cannot satisfy the physical-headset exit gate."
            }
        }
        return [ordered]@{
            code = "nonphysical-run"
            terminal = $false
            detail = "This run did not request the physical-headset profile."
        }
    }
    if (-not $hostBridge) {
        return [ordered]@{
            code = "openxr-runtime-unavailable"
            terminal = $true
            detail = "The OpenXR host never published its session/shared-mapping bridge handshake."
        }
    }
    if ($gameExited) {
        return [ordered]@{
            code = "retail-game-exited-before-controller-ack"
            terminal = $true
            detail = "The retail game exited before the exact controller consumer acknowledged the physical route."
        }
    }
    if (-not $hostPose -and $sessionNotReady) {
        return [ordered]@{
            code = "openxr-session-not-ready"
            terminal = $true
            detail = "The host bridge existed, but the physical OpenXR session never reached XR_SESSION_STATE_READY."
        }
    }
    if (-not $retailBridge -and -not $bridgeInitialized) {
        return [ordered]@{
            code = "retail-bridge-not-ready"
            terminal = $true
            detail = "OpenXR started, but the retail D3D bridge did not initialize."
        }
    }
    if (-not $retailRuntimeAndPose) {
        return [ordered]@{
            code = "retail-runtime-or-pose-not-ready"
            terminal = $true
            detail = "The bridge exists, but advancing retail runtime and tracked-pose publication were not both ready."
        }
    }
    $controllerAcknowledged = ConvertTo-FnvxrPhase1Boolean (
        Get-FnvxrPhase1PathValue -Object $Manifest -Names @("physicalHeadsetPlay", "controllerConsumerAcknowledged") -Default $false)
    if (-not $controllerAcknowledged) {
        return [ordered]@{
            code = "controller-ack-pending"
            terminal = $false
            detail = "The runtime path is ready but the exact retail controller consumer has not acknowledged physical play."
        }
    }
    return [ordered]@{
        code = "physical-evidence-awaiting-cardinal-review"
        terminal = $false
        detail = "Runtime readiness is present; retained cardinal, lineage, head/body, and transition evidence still determines the Phase 1 gate."
    }
}

function Get-FnvxrPhase1CardinalEvidence {
    param([Parameter(Mandatory = $true)]$Manifest)

    $sweep = Get-FnvxrPhase1Property -Object $Manifest -Name "headsetPoseSweep"
    $requested = ConvertTo-FnvxrPhase1Boolean (
        Get-FnvxrPhase1Property -Object $sweep -Name "requested" -Default $false)
    $evidence = Get-FnvxrPhase1Property -Object $sweep -Name "evidence"
    $rendered = Get-FnvxrPhase1Property -Object $sweep -Name "renderedCameraProof"
    $commands = @(
        Get-FnvxrPhase1Property -Object $evidence -Name "commands" -Default @())
    $expectedAxes = @(
        "translationX", "translationY", "translationZ", "yaw", "pitch", "roll")
    $missing = New-Object 'System.Collections.Generic.List[string]'
    foreach ($axis in $expectedAxes) {
        foreach ($direction in @(-1, 1)) {
            $found = @($commands | Where-Object {
                [string](Get-FnvxrPhase1Property -Object $_ -Name "axis" -Default "") -ceq $axis -and
                [int](Get-FnvxrPhase1Property -Object $_ -Name "direction" -Default 0) -eq $direction
            }).Count -gt 0
            if (-not $found) {
                $missingAxis = "{0}/{1}" -f @($axis, $direction)
                [void]$missing.Add($missingAxis)
            }
        }
    }
    $pattern = [string](Get-FnvxrPhase1Property -Object $evidence -Name "pattern" -Default "")
    $centerRestored = ConvertTo-FnvxrPhase1Boolean (
        Get-FnvxrPhase1Property -Object $evidence -Name "centerRestored" -Default $false)
    $renderedSixDof = ConvertTo-FnvxrPhase1Boolean (
        Get-FnvxrPhase1Property -Object $rendered -Name "sixDofCameraResponseProven" -Default $false)
    $commandCoverageComplete = $requested -and $pattern -ceq "cardinal-v1" -and
        $centerRestored -and $missing.Count -eq 0
    return [ordered]@{
        requested = $requested
        pattern = if ([string]::IsNullOrWhiteSpace($pattern)) { "unrecorded" } else { $pattern }
        commandCount = [int](Get-FnvxrPhase1Property -Object $evidence -Name "commandCount" -Default 0)
        centerRestored = $centerRestored
        missingSignedAxes = @($missing.ToArray())
        commandedCoverageComplete = $commandCoverageComplete
        renderedSixDofResponseProven = $renderedSixDof
        simulatorEvidenceComplete = $commandCoverageComplete -and $renderedSixDof
        physicalManualProtocolRequired = $true
        physicalEvidenceComplete = $false
    }
}

function Get-FnvxrPhase1PhysicalCardinalProtocol {
    param(
        [Parameter(Mandatory = $true)]$Manifest,
        [Parameter(Mandatory = $true)][string]$RunDirectory
    )

    $physicalRequested = ConvertTo-FnvxrPhase1Boolean (
        Get-FnvxrPhase1PathValue -Object $Manifest -Names @("physicalHeadsetPlay", "requested") -Default $false)
    if (-not $physicalRequested) {
        return [ordered]@{
            requested = $false
            path = $null
            present = $false
            retained = $false
            detail = "No physical-headset protocol is applicable to this nonphysical run."
        }
    }

    $path = Resolve-FnvxrPhase1ArtifactPath `
        -Root $RunDirectory `
        -Candidate ([string](Get-FnvxrPhase1PathValue `
            -Object $Manifest `
            -Names @("physicalHeadsetPlay", "cardinalScript") `
            -Default "")) `
        -Fallback "phase1-physical-cardinal-script.json"
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        return [ordered]@{
            requested = $true
            path = $path
            present = $false
            retained = $false
            missingSignedAxes = @(
                "translationX/-1", "translationX/1", "translationY/-1", "translationY/1",
                "translationZ/-1", "translationZ/1", "yaw/-1", "yaw/1",
                "pitch/-1", "pitch/1", "roll/-1", "roll/1")
            detail = "The input-free physical cardinal protocol was not retained with this run."
        }
    }

    try {
        $protocol = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json -ErrorAction Stop
    } catch {
        return [ordered]@{
            requested = $true
            path = $path
            present = $true
            retained = $false
            parseError = $_.Exception.Message
            detail = "The retained physical cardinal protocol could not be parsed."
        }
    }

    $steps = @(Get-FnvxrPhase1Property -Object $protocol -Name "steps" -Default @())
    $missing = New-Object 'System.Collections.Generic.List[string]'
    foreach ($axis in @(
        "translationX", "translationY", "translationZ", "yaw", "pitch", "roll")) {
        foreach ($direction in @(-1, 1)) {
            $found = @($steps | Where-Object {
                [string](Get-FnvxrPhase1Property -Object $_ -Name "axis" -Default "") -ceq $axis -and
                [int](Get-FnvxrPhase1Property -Object $_ -Name "direction" -Default 0) -eq $direction
            }).Count -eq 1
            if (-not $found) {
                [void]$missing.Add(("{0}/{1}" -f @($axis, $direction)))
            }
        }
    }
    $schema = [string](Get-FnvxrPhase1Property -Object $protocol -Name "schema" -Default "")
    $retained = $schema -ceq "fnvxr-phase1-physical-cardinal-script-v1" -and
        $steps.Count -eq 12 -and $missing.Count -eq 0
    return [ordered]@{
        requested = $true
        path = $path
        present = $true
        schema = if ([string]::IsNullOrWhiteSpace($schema)) { "unrecorded" } else { $schema }
        stepCount = $steps.Count
        missingSignedAxes = @($missing.ToArray())
        retained = $retained
        detail = if ($retained) {
            "The operator-only physical cardinal protocol is retained; recorded physical telemetry still determines acceptance."
        } else {
            "The retained physical cardinal protocol is incomplete or does not match the required schema."
        }
    }
}

function Find-FnvxrPhase1Trace {
    param(
        [Parameter(Mandatory = $true)]$Publications,
        [Parameter(Mandatory = $true)]$CenterFrames,
        [Parameter(Mandatory = $true)]$Submits,
        [Parameter(Mandatory = $true)]$HeadBodyInvariants
    )

    foreach ($publication in $Publications) {
        $transaction = Get-FnvxrPhase1Property -Object $publication -Name "transaction"
        $validPublication =
            -not (Test-FnvxrPhase1SameScalar $transaction 0) -and
            (Test-FnvxrPhase1SameScalar (
                Get-FnvxrPhase1Property -Object $publication -Name "producerMode") 4) -and
            (ConvertTo-FnvxrPhase1Boolean (
                Get-FnvxrPhase1Property -Object $publication -Name "separated" -Default $false)) -and
            (ConvertTo-FnvxrPhase1Boolean (
                Get-FnvxrPhase1Property -Object $publication -Name "worldCandidate" -Default $false)) -and
            (ConvertTo-FnvxrPhase1Boolean (
                Get-FnvxrPhase1Property -Object $publication -Name "pixelProof" -Default $false))
        if ($validPublication) {
            $frame = @($CenterFrames | Where-Object {
                (Test-FnvxrPhase1SameScalar (
                    Get-FnvxrPhase1Property -Object $_ -Name "transaction") $transaction) -and
                (Test-FnvxrPhase1SameScalar (
                    Get-FnvxrPhase1Property -Object $_ -Name "producerMode") 4) -and
                (ConvertTo-FnvxrPhase1Boolean (
                    Get-FnvxrPhase1Property -Object $_ -Name "delivered" -Default $false)) -and
                (ConvertTo-FnvxrPhase1Boolean (
                    Get-FnvxrPhase1Property -Object $_ -Name "cameraPoseValid" -Default $false)) -and
                $_._line -gt $publication._line
            } | Sort-Object -Property _line | Select-Object -First 1)
            if ($frame.Count -eq 0) {
                continue
            }
            $matchingSubmit = @($Submits | Where-Object {
                (ConvertTo-FnvxrPhase1Boolean (
                    Get-FnvxrPhase1Property -Object $_ -Name "cpuEngineStereoActive" -Default $false)) -and
                (ConvertTo-FnvxrPhase1Boolean (
                    Get-FnvxrPhase1Property -Object $_ -Name "projectionLayerSubmitted" -Default $false)) -and
                ([string](Get-FnvxrPhase1Property -Object $_ -Name "xrEndFrame" -Default "") -ceq "XR_SUCCESS") -and
                (Test-FnvxrPhase1SameScalar (
                    Get-FnvxrPhase1Property -Object $_ -Name "cpuEngineProducerMode") 4) -and
                (Test-FnvxrPhase1SameScalar (
                    Get-FnvxrPhase1Property -Object $_ -Name "cpuEngineTransaction") $transaction) -and
                (Test-FnvxrPhase1SameScalar (
                    Get-FnvxrPhase1Property -Object $_ -Name "sourceStereoSequence") (
                    Get-FnvxrPhase1Property -Object $publication -Name "publicationGeneration")) -and
                (Test-FnvxrPhase1SameScalar (
                    Get-FnvxrPhase1Property -Object $_ -Name "sourceRenderPairSequence") (
                    Get-FnvxrPhase1Property -Object $publication -Name "renderPairSequence")) -and
                (Test-FnvxrPhase1SameScalar (
                    Get-FnvxrPhase1Property -Object $_ -Name "sourcePoseSequence") (
                    Get-FnvxrPhase1Property -Object $publication -Name "poseSequence")) -and
                (Test-FnvxrPhase1SameScalar (
                    Get-FnvxrPhase1Property -Object $_ -Name "sourceReferenceSpaceGeneration") (
                    Get-FnvxrPhase1Property -Object $publication -Name "referenceSpaceGeneration")) -and
                (Test-FnvxrPhase1SameScalar (
                    Get-FnvxrPhase1Property -Object $_ -Name "sourcePoseProducerEpoch") (
                    Get-FnvxrPhase1Property -Object $publication -Name "producerEpoch")) -and
                (Test-FnvxrPhase1SameScalar (
                    Get-FnvxrPhase1Property -Object $_ -Name "sourceRendererProducerEpoch") (
                    Get-FnvxrPhase1Property -Object $publication -Name "rendererProducerEpoch")) -and
                (Test-FnvxrPhase1SameScalar (
                    Get-FnvxrPhase1Property -Object $_ -Name "sourceProducerProcessId") (
                    Get-FnvxrPhase1Property -Object $publication -Name "producerProcessId")) -and
                (Test-FnvxrPhase1SameScalar (
                    Get-FnvxrPhase1Property -Object $_ -Name "sourceRenderedDisplayTime") (
                    Get-FnvxrPhase1Property -Object $publication -Name "renderedDisplayTime"))
            } | Select-Object -First 1)
            if ($matchingSubmit.Count -eq 0) {
                continue
            }

            $poseSequence = Get-FnvxrPhase1Property -Object $publication -Name "poseSequence"
            $headBody = @($HeadBodyInvariants | Where-Object {
                (Test-FnvxrPhase1SameScalar (
                    Get-FnvxrPhase1Property -Object $_ -Name "poseSeq") $poseSequence) -and
                (ConvertTo-FnvxrPhase1Boolean (
                    Get-FnvxrPhase1Property -Object $_ -Name "playerValid" -Default $false))
            } | Select-Object -First 1)
            # The active exact-retail path makes this proof inside the
            # engine-center transaction. The older shared-D3D9 marker is
            # retained as a diagnostic fallback, but it is not permitted to
            # displace the transaction-scoped stock-camera/eye-baseline proof.
            $headBodyFromCenterFrame =
                (Test-FnvxrPhase1SameScalar (
                    Get-FnvxrPhase1Property -Object $frame[0] -Name "originPoseSequence") $poseSequence) -and
                (ConvertTo-FnvxrPhase1Boolean (
                    Get-FnvxrPhase1Property -Object $frame[0] -Name "stockCameraTransformUsable" -Default $false)) -and
                (ConvertTo-FnvxrPhase1Boolean (
                    Get-FnvxrPhase1Property -Object $frame[0] -Name "eyeBaselineValid" -Default $false)) -and
                (Test-FnvxrPhase1FiniteNonnegativeAtMost (
                    Get-FnvxrPhase1Property -Object $frame[0] -Name "eyeMidpointDistanceMeters") 0.25)
            $headBodyRecorded = $headBody.Count -ne 0 -or $headBodyFromCenterFrame
            $hasOrigin = $null -ne (Get-FnvxrPhase1Property -Object $frame[0] -Name "originPoseFrame") -and
                (Test-FnvxrPhase1FiniteVector (
                    Get-FnvxrPhase1Property -Object $frame[0] -Name "originPosition") 3) -and
                (Test-FnvxrPhase1SameScalar (
                    Get-FnvxrPhase1Property -Object $frame[0] -Name "originReferenceSpaceGeneration") (
                    Get-FnvxrPhase1Property -Object $publication -Name "referenceSpaceGeneration")) -and
                (Test-FnvxrPhase1SameScalar (
                    Get-FnvxrPhase1Property -Object $frame[0] -Name "originProducerEpoch") (
                    Get-FnvxrPhase1Property -Object $publication -Name "producerEpoch"))
            $hasEyeTransforms =
                (Test-FnvxrPhase1FiniteVector (
                    Get-FnvxrPhase1Property -Object $frame[0] -Name "leftTranslation") 3) -and
                (Test-FnvxrPhase1FiniteVector (
                    Get-FnvxrPhase1Property -Object $frame[0] -Name "rightTranslation") 3)
            $rendererCullerCameraProven = ConvertTo-FnvxrPhase1Boolean (
                Get-FnvxrPhase1Property -Object $frame[0] -Name "rendererCullerCameraMatch" -Default $false)
            return [ordered]@{
                available = $true
                transaction = $transaction
                sourceFrame = Get-FnvxrPhase1Property -Object $publication -Name "sourceFrame"
                poseFrame = Get-FnvxrPhase1Property -Object $publication -Name "poseFrame"
                poseSequence = $poseSequence
                referenceSpaceGeneration = Get-FnvxrPhase1Property -Object $publication -Name "referenceSpaceGeneration"
                poseProducerEpoch = Get-FnvxrPhase1Property -Object $publication -Name "producerEpoch"
                rendererProducerEpoch = Get-FnvxrPhase1Property -Object $publication -Name "rendererProducerEpoch"
                producerProcessId = Get-FnvxrPhase1Property -Object $publication -Name "producerProcessId"
                renderedDisplayTime = Get-FnvxrPhase1Property -Object $publication -Name "renderedDisplayTime"
                centerCameraRecorded = Test-FnvxrPhase1FiniteVector (
                    Get-FnvxrPhase1Property -Object $frame[0] -Name "centerTranslation") 3
                originRecorded = $hasOrigin
                leftRightCameraRecorded = $hasEyeTransforms
                rendererCullerCameraMatch = $rendererCullerCameraProven
                headBodySampleRecorded = $headBodyRecorded
                headBodyEvidenceSource = if ($headBodyFromCenterFrame) {
                    "engine-center-frame"
                } elseif ($headBody.Count -ne 0) {
                    "shared-d3d9-diagnostic"
                } else {
                    "none"
                }
                renderedLineageComplete = $true
                phase1TraceComplete = $hasOrigin -and $hasEyeTransforms -and
                    $rendererCullerCameraProven -and $headBodyRecorded
                missingEvidence = @(
                    if (-not $hasOrigin) { "recentered-origin" }
                    if (-not $hasEyeTransforms) { "left-right-camera-transforms" }
                    if (-not $rendererCullerCameraProven) { "renderer-culler-camera-orientation" }
                    if (-not $headBodyRecorded) { "head-body-same-pose-sample" }
                )
            }
        }
    }
    return [ordered]@{
        available = $false
        renderedLineageComplete = $false
        phase1TraceComplete = $false
        missingEvidence = @(
            "matched-engine-center-publication",
            "completed-engine-center-frame",
            "matching-openxr-submit")
    }
}

if (-not (Test-Path -LiteralPath $RunDirectory -PathType Container)) {
    throw "Phase 1 run directory does not exist: $RunDirectory"
}
$runDirectoryFull = (Resolve-Path -LiteralPath $RunDirectory).Path
$manifestPath = Join-Path $runDirectoryFull "manifest.json"
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Phase 1 run manifest does not exist: $manifestPath"
}
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json -ErrorAction Stop
$logs = Get-FnvxrPhase1Property -Object $manifest -Name "logs"
$retailLogPath = Resolve-FnvxrPhase1ArtifactPath `
    -Root $runDirectoryFull `
    -Candidate ([string](Get-FnvxrPhase1Property -Object $logs -Name "retailVrBridge" -Default "")) `
    -Fallback "fnvxr_retail_vr.log"
$hostOutPath = Resolve-FnvxrPhase1ArtifactPath `
    -Root $runDirectoryFull `
    -Candidate ([string](Get-FnvxrPhase1Property -Object $logs -Name "hostStdout" -Default "")) `
    -Fallback "host.stdout.log"
$hostErrorPath = Resolve-FnvxrPhase1ArtifactPath `
    -Root $runDirectoryFull `
    -Candidate ([string](Get-FnvxrPhase1Property -Object $logs -Name "hostStderr" -Default "")) `
    -Fallback "host.stderr.log"

$retailText = Read-FnvxrPhase1Text -Path $retailLogPath
$hostErrorText = Read-FnvxrPhase1Text -Path $hostErrorPath
$publications = Read-FnvxrPhase1JsonEvents -Path $retailLogPath -EventName "fnvxrRetailEngineCenterCpuStereo"
$centerFrames = Read-FnvxrPhase1JsonEvents -Path $retailLogPath -EventName "fnvxrRetailEngineCenterFrame"
$headBody = Read-FnvxrPhase1JsonEvents -Path $retailLogPath -EventName "fnvxrHeadBodyInvariant"
$openXrSubmits = Read-FnvxrPhase1JsonEvents -Path $hostOutPath -EventName "fnvxrOpenXrSubmit"
$phase1OpenXrSubmits = Read-FnvxrPhase1JsonEvents `
    -Path $hostOutPath `
    -EventName "fnvxrPhase1OpenXrEngineCenterSubmit"
$submits = [pscustomobject]@{
    records = @($openXrSubmits.records) + @($phase1OpenXrSubmits.records)
    parseErrors = $openXrSubmits.parseErrors + $phase1OpenXrSubmits.parseErrors
}
$classification = Get-FnvxrPhase1FailureClassification `
    -Manifest $manifest `
    -HostErrorText $hostErrorText `
    -RetailBridgeText $retailText
$cardinal = Get-FnvxrPhase1CardinalEvidence -Manifest $manifest
$physicalCardinalProtocol = Get-FnvxrPhase1PhysicalCardinalProtocol `
    -Manifest $manifest `
    -RunDirectory $runDirectoryFull
$cardinal.physicalManualProtocolRetained =
    ConvertTo-FnvxrPhase1Boolean $physicalCardinalProtocol.retained
$cardinal.physicalManualProtocolPath = $physicalCardinalProtocol.path
$trace = Find-FnvxrPhase1Trace `
    -Publications $publications.records `
    -CenterFrames $centerFrames.records `
    -Submits $submits.records `
    -HeadBodyInvariants $headBody.records
$physicalRequested = ConvertTo-FnvxrPhase1Boolean (
    Get-FnvxrPhase1PathValue -Object $manifest -Names @("physicalHeadsetPlay", "requested") -Default $false)
$source = if ($physicalRequested) {
    "physical-headset"
} elseif ($cardinal.requested) {
    "headless-simulator"
} else {
    "other"
}
$simulatorOnly = $source -eq "headless-simulator"
$physicalGateCandidate = $physicalRequested -and
    $trace.phase1TraceComplete -and
    $cardinal.physicalEvidenceComplete

$report = [ordered]@{
    schema = "fnvxr-phase1-6dof-evidence-v1"
    generatedAtUtc = [DateTime]::UtcNow.ToString("o")
    runDirectory = $runDirectoryFull
    source = $source
    simulatorOnly = $simulatorOnly
    classification = $classification
    readiness = [ordered]@{
        hostBridge = ConvertTo-FnvxrPhase1Boolean (
            Get-FnvxrPhase1PathValue -Object $manifest -Names @("readiness", "hostBridge") -Default $false)
        hostPose = ConvertTo-FnvxrPhase1Boolean (
            Get-FnvxrPhase1PathValue -Object $manifest -Names @("readiness", "hostPose") -Default $false)
        retailRuntimeAndPose = ConvertTo-FnvxrPhase1Boolean (
            Get-FnvxrPhase1PathValue -Object $manifest -Names @("readiness", "retailRuntimeAndPose") -Default $false)
        retailVrBridge = ConvertTo-FnvxrPhase1Boolean (
            Get-FnvxrPhase1PathValue -Object $manifest -Names @("readiness", "retailVrBridge") -Default $false)
        retailBridgeLogInitialized = $retailText -match '(?i)retail VR bridge initialized'
    }
    telemetry = [ordered]@{
        retailLogPath = $retailLogPath
        hostStdoutPath = $hostOutPath
        hostStderrPath = $hostErrorPath
        centerPublicationCount = $publications.records.Count
        centerFrameCount = $centerFrames.records.Count
        openXrSubmitCount = $openXrSubmits.records.Count
        phase1OpenXrEngineCenterSubmitCount = $phase1OpenXrSubmits.records.Count
        headBodyInvariantCount = $headBody.records.Count
        jsonParseErrors = $publications.parseErrors + $centerFrames.parseErrors +
            $headBody.parseErrors + $submits.parseErrors
    }
    cardinal = $cardinal
    physicalCardinalProtocol = $physicalCardinalProtocol
    trace = $trace
    physicalHeadsetGate = [ordered]@{
        candidateSatisfied = $physicalGateCandidate
        accepted = $false
        detail = "Only retained physical evidence can satisfy Phase 1. This verifier never changes fullProductAccepted."
    }
    status = if ($physicalRequested) {
        "physical-gate-open"
    } elseif ($cardinal.simulatorEvidenceComplete -and $trace.renderedLineageComplete) {
        "simulator-evidence-retained"
    } elseif ($cardinal.requested) {
        "simulator-evidence-incomplete"
    } else {
        "not-applicable"
    }
}

$reportPath = Join-Path $runDirectoryFull "phase1-6dof-evidence.json"
if (-not $NoReport) {
    $temporaryPath = "{0}.{1}.tmp" -f $reportPath, $PID
    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText(
        $temporaryPath,
        ($report | ConvertTo-Json -Depth 12),
        $encoding)
    Move-Item -LiteralPath $temporaryPath -Destination $reportPath -Force
}

$report | ConvertTo-Json -Depth 12
if ($FailOnIncomplete -and -not $physicalGateCandidate) {
    exit 2
}
