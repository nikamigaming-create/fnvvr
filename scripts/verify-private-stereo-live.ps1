param(
    [string]$RunDir = "",
    [Alias('MinimumGpuSubmits')]
    [ValidateRange(1, 120)][int]$MinimumEngineCenterSubmits = 2
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($RunDir)) {
    $runRoot = Join-Path $root "local\openxr-retail-sidecar-runs"
    $candidate = Get-ChildItem -LiteralPath $runRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object {
            (Test-Path -LiteralPath (Join-Path $_.FullName "fnvxr_retail_vr.log")) -or
            (Test-Path -LiteralPath (Join-Path $_.FullName "fnvxr_openxr_pose_host.out.log"))
        } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if (-not $candidate) {
        throw "No sidecar run with live telemetry exists under '$runRoot'."
    }
    $RunDir = $candidate.FullName
}
$RunDir = (Resolve-Path -LiteralPath $RunDir).Path

function Read-RunText([string]$Leaf) {
    $path = Join-Path $RunDir $Leaf
    if (Test-Path -LiteralPath $path -PathType Leaf) {
        return Get-Content -LiteralPath $path -Raw
    }
    return ""
}

function Read-JsonEvents([string]$Text, [string]$EventName) {
    $events = @()
    [uint64]$lineNumber = 0
    foreach ($line in ($Text -split "`r?`n")) {
        ++$lineNumber
        $start = $line.IndexOf('{"event":"' + $EventName + '"')
        if ($start -lt 0) { continue }
        try {
            $event = $line.Substring($start) | ConvertFrom-Json -ErrorAction Stop
            # The retail hook emits its source-pixel event while publishing the
            # completed transaction, then logs the transaction completion after
            # the hook returns.  Preserve the source-file order so a replayed
            # pair cannot be joined to an older completion record.
            $event | Add-Member -NotePropertyName "__fnvxrSourceLine" `
                -NotePropertyValue $lineNumber -Force
            $events += $event
        } catch {
        }
    }
    return @($events)
}

function Get-PositiveUInt64($Value) {
    [uint64]$parsed = 0
    if ([uint64]::TryParse(
            [string]$Value,
            [System.Globalization.NumberStyles]::None,
            [System.Globalization.CultureInfo]::InvariantCulture,
            [ref]$parsed) -and $parsed -gt 0) {
        return $parsed
    }
    return $null
}

function Test-PositiveInteger($Value) {
    return $null -ne (Get-PositiveUInt64 $Value)
}

function Get-UInt32Bits($Value) {
    # Shared render-pair and pose counters cross the ABI as signed LONGs.
    # Logs therefore legitimately spell values >= 0x80000000 as negative;
    # compare their raw 32-bit identity instead of their decimal spelling.
    [int64]$parsed = 0
    if (-not [int64]::TryParse(
            [string]$Value,
            [System.Globalization.NumberStyles]::Integer,
            [System.Globalization.CultureInfo]::InvariantCulture,
            [ref]$parsed)) {
        return $null
    }
    if ($parsed -lt [int64][int32]::MinValue -or
        $parsed -gt [int64][uint32]::MaxValue) {
        return $null
    }
    if ($parsed -lt 0) {
        return [uint64]($parsed + 4294967296L)
    }
    return [uint64]$parsed
}

function Test-NonzeroSequenceBits($Value) {
    $bits = Get-UInt32Bits $Value
    return $null -ne $bits -and $bits -ne 0
}

function Test-PositiveUInt32($Value) {
    $parsed = Get-PositiveUInt64 $Value
    return $null -ne $parsed -and $parsed -le [uint64][uint32]::MaxValue
}

function Test-ExactUInt32Bits($Left, $Right) {
    $leftBits = Get-UInt32Bits $Left
    $rightBits = Get-UInt32Bits $Right
    return $null -ne $leftBits -and $null -ne $rightBits -and
        $leftBits -eq $rightBits
}

function Test-ExactPositiveUInt64($Left, $Right) {
    $leftValue = Get-PositiveUInt64 $Left
    $rightValue = Get-PositiveUInt64 $Right
    return $null -ne $leftValue -and $null -ne $rightValue -and
        $leftValue -eq $rightValue
}

function Get-TransactionRenderPairBits($Value) {
    $transaction = Get-PositiveUInt64 $Value
    if ($null -eq $transaction) { return $null }
    $bits = $transaction % [uint64]4294967296
    # The shared protocol reserves zero as unpublished.  The producer maps a
    # low-word zero to one, so the verifier must use that exact normalization.
    if ($bits -eq 0) { return [uint64]1 }
    return [uint64]$bits
}

function Test-UInt32Advanced($Newer, $Older) {
    $newerBits = Get-UInt32Bits $Newer
    $olderBits = Get-UInt32Bits $Older
    if ($null -eq $newerBits -or $null -eq $olderBits -or
        $newerBits -eq 0 -or $olderBits -eq 0) {
        return $false
    }
    $delta = ($newerBits + [uint64]4294967296 - $olderBits) %
        [uint64]4294967296
    return $delta -gt 0 -and $delta -lt [uint64]2147483648
}

function Test-UInt64Advanced($Newer, $Older) {
    $newerValue = Get-PositiveUInt64 $Newer
    $olderValue = Get-PositiveUInt64 $Older
    if ($null -eq $newerValue -or $null -eq $olderValue -or
        $newerValue -eq $olderValue) {
        return $false
    }
    [System.Numerics.BigInteger]$newer = $newerValue
    [System.Numerics.BigInteger]$older = $olderValue
    $modulus = [System.Numerics.BigInteger]::Pow(
        [System.Numerics.BigInteger]::new(2), 64)
    $halfRange = [System.Numerics.BigInteger]::Pow(
        [System.Numerics.BigInteger]::new(2), 63)
    $delta = if ($newer -gt $older) {
        $newer - $older
    } else {
        ($modulus - $older) + $newer
    }
    return $delta -gt [System.Numerics.BigInteger]::Zero -and
        $delta -lt $halfRange
}

function Test-NonzeroHash($Value) {
    $text = [string]$Value
    return $text -match '^0x[0-9a-fA-F]{1,8}$' -and
        $text -notmatch '^0x0+$'
}

function Get-EventValue($Event, [string]$Name, $DefaultValue) {
    if ($null -eq $Event) { return $DefaultValue }
    $property = $Event.PSObject.Properties[$Name]
    if ($null -eq $property) { return $DefaultValue }
    return $property.Value
}

function Get-EventSourceLine($Event) {
    $line = Get-PositiveUInt64 (Get-EventValue $Event "__fnvxrSourceLine" 0)
    if ($null -eq $line) { return [uint64]0 }
    return $line
}

$retailText = Read-RunText "fnvxr_retail_vr.log"
$d3d9Text = Read-RunText "fnvxr_d3d9_proxy.log"
$hostText = Read-RunText "fnvxr_openxr_pose_host.out.log"

# The historic replay diagnostic emitted fnvxrD3d9EyeTarget.  That event is
# intentionally not evidence for the engine-center route: it can describe a
# pair made by an old draw-replay producer.  World stereo is accepted only
# when a completed engine transaction and the CPU pair it published share one
# transaction id.
$centerSamples = Read-JsonEvents $retailText "fnvxrRetailEngineCenterFrame"
$goodCenterSamples = @($centerSamples | Where-Object {
    (Test-PositiveInteger (Get-EventValue $_ "dispatch" 0)) -and
    (Test-PositiveInteger (Get-EventValue $_ "transaction" 0)) -and
    (Test-PositiveInteger (Get-EventValue $_ "visibleSetGeneration" 0)) -and
    (Test-PositiveInteger (Get-EventValue $_ "stereoComplete" 0)) -and
    [bool](Get-EventValue $_ "delivered" $false) -and
    [int](Get-EventValue $_ "controllerFailure" -1) -eq 0 -and
    [int](Get-EventValue $_ "disposition" -1) -eq 2 -and
    [int](Get-EventValue $_ "centerFailure" -1) -eq 0 -and
    [int](Get-EventValue $_ "rendererFailure" -1) -eq 0 -and
    [bool](Get-EventValue $_ "rendererComplete" $false) -and
    [int](Get-EventValue $_ "producerMode" 0) -eq 4 -and
    [int](Get-EventValue $_ "visible" 0) -gt 0 -and
    [string](Get-EventValue $_ "visibleSetGeneration" "") -eq
        [string](Get-EventValue $_ "transaction" "")
})

$completedTransactions = @{}
$duplicateCenterTransactions = @()
foreach ($sample in $goodCenterSamples) {
    $transaction = [string](Get-EventValue $sample "transaction" "")
    if ($completedTransactions.ContainsKey($transaction)) {
        $duplicateCenterTransactions += $sample
        continue
    }
    $completedTransactions[$transaction] = $sample
}

function Test-EngineCenterPairLineage($Pair) {
    try {
        $transaction = [string](Get-EventValue $Pair "transaction" "")
        if (-not (Test-PositiveInteger $transaction) -or
            -not $script:completedTransactions.ContainsKey($transaction)) {
            return $false
        }
        $center = $script:completedTransactions[$transaction]
        $expectedRenderPairBits = Get-TransactionRenderPairBits $transaction
        # publishCpuPair is inside the successful engine transaction;
        # retailVrWorldRenderAdapter logs completion after dispatch returns.
        return $null -ne $center -and
            $null -ne $expectedRenderPairBits -and
            (Get-EventSourceLine $Pair) -gt 0 -and
            (Get-EventSourceLine $center) -gt 0 -and
            (Get-EventSourceLine $Pair) -lt (Get-EventSourceLine $center) -and
            (Test-ExactUInt32Bits `
                (Get-EventValue $Pair "renderPairSequence" "") `
                $expectedRenderPairBits)
    } catch {
        return $false
    }
}

$engineCenterPairs = Read-JsonEvents $retailText "fnvxrRetailEngineCenterCpuStereo"
$sourcePixelProofs = @($engineCenterPairs | Where-Object {
    (Test-EngineCenterPairLineage $_) -and
    (Test-PositiveInteger (Get-EventValue $_ "sourceFrame" 0)) -and
    (Test-NonzeroSequenceBits (Get-EventValue $_ "poseSequence" 0)) -and
    (Test-PositiveInteger (Get-EventValue $_ "runtimeStateSample" 0)) -and
    (Test-PositiveInteger (Get-EventValue $_ "renderedDisplayTime" 0)) -and
    (Test-NonzeroSequenceBits (Get-EventValue $_ "renderPairSequence" 0)) -and
    (Test-PositiveInteger (Get-EventValue $_ "publicationGeneration" 0)) -and
    (Test-PositiveUInt32 (Get-EventValue $_ "referenceSpaceGeneration" 0)) -and
    (Test-PositiveInteger (Get-EventValue $_ "producerEpoch" 0)) -and
    (Test-PositiveInteger (Get-EventValue $_ "rendererProducerEpoch" 0)) -and
    (Test-PositiveUInt32 (Get-EventValue $_ "producerProcessId" 0)) -and
    [int](Get-EventValue $_ "producerMode" 0) -eq 4 -and
    [bool](Get-EventValue $_ "separated" $false) -and
    [bool](Get-EventValue $_ "worldCandidate" $false) -and
    -not [bool](Get-EventValue $_ "uiActive" $true) -and
    [bool](Get-EventValue $_ "pixelProof" $false) -and
    [bool](Get-EventValue $_ "visualCoverage" $false) -and
    [int](Get-EventValue $_ "pixelHashSamples" 0) -ge 64 -and
    [int](Get-EventValue $_ "leftNonBlackSamples" 0) -ge 16 -and
    [int](Get-EventValue $_ "rightNonBlackSamples" 0) -ge 16 -and
    [int](Get-EventValue $_ "differentSamples" 0) -gt 0 -and
    [int](Get-EventValue $_ "differentTiles" 0) -gt 0 -and
    (Test-NonzeroHash (Get-EventValue $_ "leftHash" "")) -and
    (Test-NonzeroHash (Get-EventValue $_ "rightHash" "")) -and
    [string](Get-EventValue $_ "leftHash" "") -ne
        [string](Get-EventValue $_ "rightHash" "")
})
$lineageRejectedEngineCenterPairs = @($engineCenterPairs | Where-Object {
    -not (Test-EngineCenterPairLineage $_)
})
$engineCenterPairsByTransaction = @{}
$duplicateSourceTransactions = @()
foreach ($pair in $sourcePixelProofs) {
    $transaction = [string](Get-EventValue $pair "transaction" "")
    if ($engineCenterPairsByTransaction.ContainsKey($transaction)) {
        $duplicateSourceTransactions += $pair
        continue
    }
    $engineCenterPairsByTransaction[$transaction] = $pair
}
$engineCenterPairsByPublicationGeneration = @{}
$duplicateSourcePublicationGenerations = @()
foreach ($pair in $engineCenterPairsByTransaction.Values) {
    $publicationGeneration = Get-PositiveUInt64 (
        Get-EventValue $pair "publicationGeneration" "")
    if ($null -eq $publicationGeneration) { continue }
    $publicationGenerationKey = [string]$publicationGeneration
    if ($engineCenterPairsByPublicationGeneration.ContainsKey($publicationGenerationKey)) {
        $duplicateSourcePublicationGenerations += $pair
        continue
    }
    $engineCenterPairsByPublicationGeneration[$publicationGenerationKey] = $pair
}
$engineCenterPairsByRenderPairBits = @{}
$ambiguousSourceRenderPairBits = @{}
foreach ($pair in $engineCenterPairsByTransaction.Values) {
    $renderPairBits = Get-UInt32Bits (Get-EventValue $pair "renderPairSequence" "")
    if ($null -eq $renderPairBits) { continue }
    $key = [string]$renderPairBits
    if ($engineCenterPairsByRenderPairBits.ContainsKey($key)) {
        $engineCenterPairsByRenderPairBits.Remove($key)
        $ambiguousSourceRenderPairBits[$key] = $true
        continue
    }
    if (-not $ambiguousSourceRenderPairBits.ContainsKey($key)) {
        $engineCenterPairsByRenderPairBits[$key] = $pair
    }
}
$usableEngineCenterPairsByTransaction = @{}
foreach ($pair in $engineCenterPairsByTransaction.Values) {
    $transaction = [string](Get-EventValue $pair "transaction" "")
    $renderPairBits = Get-UInt32Bits (Get-EventValue $pair "renderPairSequence" "")
    if ($null -ne $renderPairBits -and
        $engineCenterPairsByRenderPairBits.ContainsKey([string]$renderPairBits)) {
        $usableEngineCenterPairsByTransaction[$transaction] = $pair
    }
}
$sourcePixelTransactions = @($usableEngineCenterPairsByTransaction.Keys)

function Test-EngineCenterHostSubmit($Event) {
    try {
        $publicationGeneration = Get-PositiveUInt64 (
            Get-EventValue $Event "sourceStereoPublicationGeneration" "")
        if ($null -eq $publicationGeneration) { return $false }
        $pair = $script:engineCenterPairsByPublicationGeneration[
            [string]$publicationGeneration]
        if ($null -eq $pair) { return $false }
        $renderPairBits = Get-UInt32Bits (
            Get-EventValue $Event "cpuEngineTransaction" "")
        if ($null -eq $renderPairBits -or $renderPairBits -eq 0) {
            return $false
        }

        return [bool](Get-EventValue $Event "cpuEngineStereoActive" $false) -and
            [int](Get-EventValue $Event "cpuEngineProducerMode" 0) -eq 4 -and
            (Test-ExactUInt32Bits `
                (Get-EventValue $Event "cpuEngineTransaction" "") `
                (Get-EventValue $pair "renderPairSequence" "")) -and
            [bool](Get-EventValue $Event "runtimeGameplay" $false) -and
            [bool](Get-EventValue $Event "stereoFullscreen" $false) -and
            [bool](Get-EventValue $Event "runtimeShouldRender" $false) -and
            [bool](Get-EventValue $Event "projectionLayerSubmitted" $false) -and
            [int](Get-EventValue $Event "layerCount" 0) -eq 1 -and
            [string](Get-EventValue $Event "xrEndFrame" "") -eq "XR_SUCCESS" -and
            [bool](Get-EventValue $Event "sourcePoseAgeValid" $false) -and
            (Test-PositiveInteger (Get-EventValue $Event "frame" 0)) -and
            (Test-PositiveInteger `
                (Get-EventValue $Event "hostWallClockUnixMilliseconds" 0)) -and
            (Test-NonzeroSequenceBits (Get-EventValue $Event "sourceStereoSequence" 0)) -and
            (Test-ExactPositiveUInt64 `
                (Get-EventValue $Event "sourceStereoPublicationGeneration" "") `
                (Get-EventValue $pair "publicationGeneration" "")) -and
            (Test-ExactUInt32Bits `
                (Get-EventValue $Event "sourceRenderPairSequence" "") `
                (Get-EventValue $pair "renderPairSequence" "")) -and
            (Test-ExactUInt32Bits `
                (Get-EventValue $Event "sourcePoseSequence" "") `
                (Get-EventValue $pair "poseSequence" "")) -and
            [string](Get-EventValue $Event "sourceReferenceSpaceGeneration" "") -eq
                [string](Get-EventValue $pair "referenceSpaceGeneration" "") -and
            [string](Get-EventValue $Event "sourcePoseProducerEpoch" "") -eq
                [string](Get-EventValue $pair "producerEpoch" "") -and
            [string](Get-EventValue $Event "sourceRendererProducerEpoch" "") -eq
                [string](Get-EventValue $pair "rendererProducerEpoch" "") -and
            [string](Get-EventValue $Event "sourceProducerProcessId" "") -eq
                [string](Get-EventValue $pair "producerProcessId" "") -and
            [string](Get-EventValue $Event "sourceRenderedDisplayTime" "") -eq
                [string](Get-EventValue $pair "renderedDisplayTime" "") -and
            [int](Get-EventValue $Event "pixelSamples" 0) -ge 64 -and
            [int](Get-EventValue $Event "nonBlackSamples" 0) -ge 64 -and
            [int](Get-EventValue $Event "meaningfulDifferentSamples" 0) -ge 64 -and
            [int](Get-EventValue $Event "leftActiveTiles" 0) -ge 12 -and
            [int](Get-EventValue $Event "rightActiveTiles" 0) -ge 12 -and
            [int](Get-EventValue $Event "differentTiles" 0) -ge 8
    } catch {
        return $false
    }
}

$submitEvents = Read-JsonEvents $hostText "fnvxrOpenXrSubmit"
$gameplaySubmits = @($submitEvents | Where-Object { [bool]$_.runtimeGameplay })
$goodEngineCenterSubmits = @($gameplaySubmits | Where-Object {
    Test-EngineCenterHostSubmit $_
})
$goodEngineCenterTransactions = @(
    $goodEngineCenterSubmits |
        ForEach-Object {
            $publicationGeneration = Get-PositiveUInt64 (
                Get-EventValue $_ "sourceStereoPublicationGeneration" "")
            if ($null -ne $publicationGeneration) {
                $pair = $engineCenterPairsByPublicationGeneration[
                    [string]$publicationGeneration]
                if ($null -ne $pair) {
                    [string](Get-EventValue $pair "transaction" "")
                }
            }
        } |
        Where-Object { Test-PositiveInteger $_ } |
        Select-Object -Unique)

function Get-HostFrameOrderViolations($Events) {
    $violations = @()
    [uint64]$previousFrame = 0
    $havePrevious = $false
    foreach ($event in $Events) {
        $frame = Get-PositiveUInt64 (Get-EventValue $event "frame" 0)
        if ($null -eq $frame -or ($havePrevious -and $frame -le $previousFrame)) {
            $violations += $event
            continue
        }
        $previousFrame = $frame
        $havePrevious = $true
    }
    return @($violations)
}

function Get-HostPairSequenceRegressions($Events) {
    $regressions = @()
    [uint64]$previousPairBits = 0
    $havePrevious = $false
    foreach ($event in $Events) {
        $pairBits = Get-UInt32Bits (
            Get-EventValue $event "cpuEngineTransaction" "")
        if ($null -eq $pairBits -or $pairBits -eq 0) {
            $regressions += $event
            continue
        }
        # Repeated presentation of the most recent source pair is allowed at
        # a higher headset cadence; returning to an older pair is not.
        if ($havePrevious -and $pairBits -ne $previousPairBits -and
            -not (Test-UInt32Advanced $pairBits $previousPairBits)) {
            $regressions += $event
            continue
        }
        $previousPairBits = $pairBits
        $havePrevious = $true
    }
    return @($regressions)
}

function Get-HostPublicationGenerationRegressions($Events) {
    $regressions = @()
    [uint64]$previousGeneration = 0
    $havePrevious = $false
    foreach ($event in $Events) {
        $generation = Get-PositiveUInt64 (
            Get-EventValue $event "sourceStereoPublicationGeneration" "")
        if ($null -eq $generation) {
            $regressions += $event
            continue
        }
        # Re-presenting the latest source publication is expected when the
        # headset runs faster than Fallout. Returning to an older publication
        # after a newer one is evidence of an ABA/stale-frame error.
        if ($havePrevious -and $generation -ne $previousGeneration -and
            -not (Test-UInt64Advanced $generation $previousGeneration)) {
            $regressions += $event
            continue
        }
        $previousGeneration = $generation
        $havePrevious = $true
    }
    return @($regressions)
}

$hostFrameOrderViolations = @(
    Get-HostFrameOrderViolations $goodEngineCenterSubmits)
$hostPairSequenceRegressions = @(
    Get-HostPairSequenceRegressions $goodEngineCenterSubmits)
$hostPublicationGenerationRegressions = @(
    Get-HostPublicationGenerationRegressions $goodEngineCenterSubmits)

$outputPixelProofs = @($goodEngineCenterSubmits | Where-Object {
    [bool](Get-EventValue $_ "leftOutputProof" $false) -and
    [bool](Get-EventValue $_ "rightOutputProof" $false) -and
    [int](Get-EventValue $_ "leftOutputNonBlackSamples" 0) -ge 16 -and
    [int](Get-EventValue $_ "rightOutputNonBlackSamples" 0) -ge 16 -and
    [int](Get-EventValue $_ "leftOutputVariedSamples" 0) -ge 16 -and
    [int](Get-EventValue $_ "rightOutputVariedSamples" 0) -ge 16 -and
    (Test-NonzeroHash (Get-EventValue $_ "leftOutputHash" "")) -and
    (Test-NonzeroHash (Get-EventValue $_ "rightOutputHash" "")) -and
    [string](Get-EventValue $_ "leftOutputHash" "") -ne
        [string](Get-EventValue $_ "rightOutputHash" "")
})

$firstGoodFrame = if ($goodEngineCenterSubmits.Count -gt 0) {
    Get-PositiveUInt64 (Get-EventValue $goodEngineCenterSubmits[0] "frame" 0)
} else {
    [uint64]0
}
$badGameplayAfterHandoff = @($gameplaySubmits | Where-Object {
    $frame = Get-PositiveUInt64 (Get-EventValue $_ "frame" 0)
    $firstGoodFrame -gt 0 -and ($null -eq $frame -or
        ($frame -ge $firstGoodFrame -and -not (Test-EngineCenterHostSubmit $_)))
})

$hostSubmissionHz = 0.0
if ($goodEngineCenterSubmits.Count -ge 2 -and
    $hostFrameOrderViolations.Count -eq 0) {
    $first = $goodEngineCenterSubmits[0]
    $last = $goodEngineCenterSubmits[-1]
    $firstMilliseconds = Get-PositiveUInt64 (
        Get-EventValue $first "hostWallClockUnixMilliseconds" 0)
    $lastMilliseconds = Get-PositiveUInt64 (
        Get-EventValue $last "hostWallClockUnixMilliseconds" 0)
    $firstFrame = Get-PositiveUInt64 (Get-EventValue $first "frame" 0)
    $lastFrame = Get-PositiveUInt64 (Get-EventValue $last "frame" 0)
    if ($null -ne $firstMilliseconds -and $null -ne $lastMilliseconds -and
        $null -ne $firstFrame -and $null -ne $lastFrame) {
        $milliseconds = [int64]$lastMilliseconds - [int64]$firstMilliseconds
        $frames = $lastFrame - $firstFrame
        if ($milliseconds -gt 0 -and $frames -gt 0) {
            $hostSubmissionHz = [double]$frames * 1000.0 / [double]$milliseconds
        }
    }
}

$checks = @(
    [ordered]@{
        name = "retail_engine_center_transaction"
        pass = $goodCenterSamples.Count -gt 0 -and
            $duplicateCenterTransactions.Count -eq 0
        detail = "completed=$($goodCenterSamples.Count) total=$($centerSamples.Count) duplicateTransactions=$($duplicateCenterTransactions.Count)"
    },
    [ordered]@{
        name = "retail_source_pixels"
        pass = $sourcePixelTransactions.Count -ge $MinimumEngineCenterSubmits -and
            $duplicateSourceTransactions.Count -eq 0 -and
            $duplicateSourcePublicationGenerations.Count -eq 0 -and
            $ambiguousSourceRenderPairBits.Count -eq 0
        detail = "matchedTransactions=$($sourcePixelTransactions.Count) totalCpuPairs=$($engineCenterPairs.Count) lineageRejectedPairs=$($lineageRejectedEngineCenterPairs.Count) duplicateTransactions=$($duplicateSourceTransactions.Count) duplicatePublicationGenerations=$($duplicateSourcePublicationGenerations.Count) ambiguousRenderPairBits=$($ambiguousSourceRenderPairBits.Count) required=$MinimumEngineCenterSubmits"
    },
    [ordered]@{
        name = "engine_center_projection_submit"
        pass = $goodEngineCenterTransactions.Count -ge $MinimumEngineCenterSubmits
        detail = "matchedSubmits=$($goodEngineCenterSubmits.Count) uniqueTransactions=$($goodEngineCenterTransactions.Count) required=$MinimumEngineCenterSubmits"
    },
    [ordered]@{
        name = "post_composition_eye_pixels"
        pass = $outputPixelProofs.Count -gt 0
        detail = "sampledNonblackSeparatedOutputs=$($outputPixelProofs.Count)"
    },
    [ordered]@{
        name = "stable_after_engine_center_handoff"
        pass = $goodEngineCenterSubmits.Count -gt 0 -and
            $badGameplayAfterHandoff.Count -eq 0 -and
            $hostFrameOrderViolations.Count -eq 0 -and
            $hostPairSequenceRegressions.Count -eq 0 -and
            $hostPublicationGenerationRegressions.Count -eq 0
        detail = "badGameplaySubmitsAfterFirstGood=$($badGameplayAfterHandoff.Count) hostFrameOrderViolations=$($hostFrameOrderViolations.Count) sourcePairRegressions=$($hostPairSequenceRegressions.Count) sourcePublicationGenerationRegressions=$($hostPublicationGenerationRegressions.Count)"
    },
    [ordered]@{
        name = "openxr_90hz_class_cadence"
        pass = $hostSubmissionHz -ge 80.0
        detail = ("measuredHz={0:N2}" -f $hostSubmissionHz)
    }
)

$failed = @($checks | Where-Object { -not $_.pass })
$verdict = [ordered]@{
    generatedAt = [DateTime]::UtcNow.ToString("o")
    runDir = $RunDir
    passed = $failed.Count -eq 0
    failedCount = $failed.Count
    hostSubmissionHz = [Math]::Round($hostSubmissionHz, 3)
    counts = [ordered]@{
        centerSamples = $centerSamples.Count
        goodCenterSamples = $goodCenterSamples.Count
        duplicateCenterTransactions = $duplicateCenterTransactions.Count
        engineCenterPairs = $engineCenterPairs.Count
        lineageRejectedEngineCenterPairs = $lineageRejectedEngineCenterPairs.Count
        duplicateSourceTransactions = $duplicateSourceTransactions.Count
        duplicateSourcePublicationGenerations = $duplicateSourcePublicationGenerations.Count
        ambiguousSourceRenderPairBits = $ambiguousSourceRenderPairBits.Count
        sourcePixelProofs = $sourcePixelProofs.Count
        sourcePixelTransactions = $sourcePixelTransactions.Count
        gameplaySubmits = $gameplaySubmits.Count
        goodEngineCenterSubmits = $goodEngineCenterSubmits.Count
        goodEngineCenterTransactions = $goodEngineCenterTransactions.Count
        outputPixelProofs = $outputPixelProofs.Count
        badGameplayAfterHandoff = $badGameplayAfterHandoff.Count
        hostFrameOrderViolations = $hostFrameOrderViolations.Count
        hostPairSequenceRegressions = $hostPairSequenceRegressions.Count
        hostPublicationGenerationRegressions = $hostPublicationGenerationRegressions.Count
    }
    checks = $checks
}
$verdictPath = Join-Path $RunDir "private-stereo-verdict.json"
$verdict | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $verdictPath -Encoding UTF8
$verdict | ConvertTo-Json -Depth 6
if ($failed.Count -gt 0) { exit 2 }
exit 0
