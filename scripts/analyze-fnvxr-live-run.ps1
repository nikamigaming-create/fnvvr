param(
    [Parameter(Mandatory = $true)]
    [string]$RunDir,
    [ValidateSet('full-vr','quad-2d-transport','diagnostic-producer')]
    [string]$ExpectedProfile = 'full-vr'
)

$ErrorActionPreference = "Stop"

function Read-RunText([string]$Name) {
    $path = Join-Path $RunDir $Name
    if (Test-Path -LiteralPath $path) { return Get-Content -LiteralPath $path -Raw }
    return ""
}

function Test-Unsigned32Advanced($NewerValue, $OlderValue) {
    try {
        $newer = [uint64]$NewerValue
        $older = [uint64]$OlderValue
        if ($newer -gt [uint32]::MaxValue -or $older -gt [uint32]::MaxValue -or $newer -eq 0) {
            return $false
        }
        $delta = ($newer + 4294967296L - $older) % 4294967296L
        return $delta -gt 0 -and $delta -lt 2147483648L
    } catch {
        return $false
    }
}

function Test-PositiveUnsigned64($Value) {
    [uint64]$parsed = 0
    return [uint64]::TryParse(
        [string]$Value,
        [System.Globalization.NumberStyles]::None,
        [System.Globalization.CultureInfo]::InvariantCulture,
        [ref]$parsed) -and $parsed -gt 0
}

function Test-NonzeroUnsigned32($Value) {
    [uint64]$parsed = 0
    return [uint64]::TryParse(
        [string]$Value,
        [System.Globalization.NumberStyles]::None,
        [System.Globalization.CultureInfo]::InvariantCulture,
        [ref]$parsed) -and $parsed -gt 0 -and $parsed -le [uint32]::MaxValue
}

function Test-Sha256Text($Value) {
    return [string]$Value -match '^[0-9a-fA-F]{64}$'
}

function Get-StereoIdentityKey(
    $Sequence,
    $RenderPairSequence,
    $PoseSequence,
    $ReferenceSpaceGeneration,
    $PoseProducerEpoch,
    $RendererProducerEpoch,
    $ProducerProcessId) {
    try {
        if (-not (Test-NonzeroUnsigned32 $Sequence) -or
            -not (Test-NonzeroUnsigned32 $RenderPairSequence) -or
            -not (Test-NonzeroUnsigned32 $PoseSequence) -or
            [uint64]$ReferenceSpaceGeneration -eq 0 -or
            [uint64]$ReferenceSpaceGeneration -gt [uint32]::MaxValue -or
            -not (Test-PositiveUnsigned64 $PoseProducerEpoch) -or
            -not (Test-PositiveUnsigned64 $RendererProducerEpoch) -or
            [uint64]$ProducerProcessId -eq 0 -or
            [uint64]$ProducerProcessId -gt [uint32]::MaxValue) {
            return $null
        }
        return '{0}|{1}|{2}|{3}|{4}|{5}|{6}' -f `
            ([uint32]$Sequence), ([uint32]$RenderPairSequence), ([uint32]$PoseSequence), `
            ([uint32]$ReferenceSpaceGeneration), ([uint64]$PoseProducerEpoch), `
            ([uint64]$RendererProducerEpoch), ([uint32]$ProducerProcessId)
    } catch {
        return $null
    }
}

function Get-EyeEventIdentityKey($Event) {
    return Get-StereoIdentityKey `
        $Event.sequence $Event.renderPairSequence $Event.poseSequence `
        $Event.referenceSpaceGeneration $Event.poseProducerEpoch `
        $Event.rendererProducerEpoch $Event.producerProcessId
}

function Get-SubmitEventIdentityKey($Event) {
    return Get-StereoIdentityKey `
        $Event.sourceStereoSequence $Event.sourceRenderPairSequence $Event.sourcePoseSequence `
        $Event.sourceReferenceSpaceGeneration $Event.sourcePoseProducerEpoch `
        $Event.sourceRendererProducerEpoch $Event.sourceProducerProcessId
}

function Test-GoodGameplaySubmit($Event) {
    try {
        $predictedDisplayTime = [int64]$Event.predictedDisplayTime
        $sourceRenderedDisplayTime = [int64]$Event.sourceRenderedDisplayTime
        $reportedPoseAge = [int64]$Event.sourcePoseAgeNanoseconds
        $poseAgeLimit = [int64]$Event.sourcePoseAgeLimitNanoseconds
        $futureTolerance = [int64]$Event.sourcePoseFutureToleranceNanoseconds
        if ($predictedDisplayTime -le 0 -or $sourceRenderedDisplayTime -le 0 -or
            $poseAgeLimit -lt 0 -or $futureTolerance -lt 0) {
            return $false
        }
        $computedPoseAge = $predictedDisplayTime - $sourceRenderedDisplayTime
        return [bool]$Event.runtimeGameplay -and [bool]$Event.stereoFullscreen -and
            [bool]$Event.runtimeShouldRender -and
            [bool]$Event.leftOutputProof -and [bool]$Event.rightOutputProof -and
            [int]$Event.leftOutputNonBlackSamples -ge 16 -and
            [int]$Event.rightOutputNonBlackSamples -ge 16 -and
            [int]$Event.leftOutputVariedSamples -ge 16 -and
            [int]$Event.rightOutputVariedSamples -ge 16 -and
            [bool]$Event.projectionLayerSubmitted -and [int]$Event.layerCount -eq 1 -and
            [string]$Event.xrEndFrame -eq 'XR_SUCCESS' -and
            [bool]$Event.sourcePoseAgeValid -and $poseAgeLimit -gt 0 -and
            $reportedPoseAge -eq $computedPoseAge -and
            $computedPoseAge -le $poseAgeLimit -and
            $computedPoseAge -ge -$futureTolerance -and
            $null -ne (Get-SubmitEventIdentityKey $Event) -and
            [int]$Event.meaningfulDifferentSamples -ge 64 -and
            [int]$Event.nonBlackSamples -ge 64 -and
            [int]$Event.leftActiveTiles -ge 12 -and
            [int]$Event.rightActiveTiles -ge 12 -and
            [int]$Event.differentTiles -ge 8
    } catch {
        return $false
    }
}

function Test-GoodQuadSubmit($Event) {
    try {
        return -not [bool]$Event.stereoFullscreen -and
            [bool]$Event.runtimeShouldRender -and [bool]$Event.presentedGameTexture -and
            [bool]$Event.gameTextureTransportFresh -and
            (Test-NonzeroUnsigned32 $Event.gameTextureSequence) -and
            [string]$Event.gameTextureHash -match '^0x[0-9a-fA-F]{1,8}$' -and
            [string]$Event.gameTextureHash -ne '0x0' -and
            [bool]$Event.leftOutputProof -and [bool]$Event.rightOutputProof -and
            [int]$Event.leftOutputNonBlackSamples -ge 16 -and
            [int]$Event.rightOutputNonBlackSamples -ge 16 -and
            [int]$Event.leftOutputVariedSamples -ge 16 -and
            [int]$Event.rightOutputVariedSamples -ge 16 -and
            [bool]$Event.projectionLayerSubmitted -and [int]$Event.layerCount -eq 1 -and
            [string]$Event.xrEndFrame -eq 'XR_SUCCESS' -and
            [int64]$Event.hostWallClockUnixMilliseconds -gt 0
    } catch {
        return $false
    }
}

function Get-SafeCaptureChildPath([string]$Directory, [string]$RelativePath) {
    if ([string]::IsNullOrWhiteSpace($RelativePath) -or [IO.Path]::IsPathRooted($RelativePath)) {
        return $null
    }
    $root = [IO.Path]::GetFullPath($Directory).TrimEnd('\') + '\'
    $candidate = [IO.Path]::GetFullPath((Join-Path $Directory $RelativePath))
    if (-not $candidate.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) {
        return $null
    }
    return $candidate
}

function Test-SceneArtifact(
    [string]$Path,
    $ExpectedArtifactSha256,
    $ExpectedLeftSha256,
    $ExpectedRightSha256,
    $ExpectedWidth,
    $ExpectedHeight,
    $ExpectedFormat,
    $ExpectedSequence,
    [bool]$ExpectedSeparated,
    [bool]$ExpectedWorldCandidate) {
    try {
        if (-not (Test-Path -LiteralPath $Path -PathType Leaf) -or
            -not (Test-Sha256Text $ExpectedArtifactSha256) -or
            -not (Test-Sha256Text $ExpectedLeftSha256) -or
            -not (Test-Sha256Text $ExpectedRightSha256)) {
            return $false
        }
        $fileHash = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($fileHash -ne ([string]$ExpectedArtifactSha256).ToLowerInvariant()) { return $false }

        $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
        $reader = New-Object IO.BinaryReader($stream)
        try {
            $magic = [Text.Encoding]::ASCII.GetString($reader.ReadBytes(8))
            $version = $reader.ReadUInt32()
            $headerBytes = $reader.ReadUInt32()
            $flags = $reader.ReadUInt32()
            $width = $reader.ReadUInt32()
            $height = $reader.ReadUInt32()
            $format = $reader.ReadUInt32()
            $pitch = $reader.ReadUInt32()
            $eyeCount = $reader.ReadUInt32()
            [void]$reader.ReadUInt32()
            [void]$reader.ReadUInt32()
            $sequence = $reader.ReadUInt64()
            $leftOffset = $reader.ReadUInt64()
            $leftBytes = $reader.ReadUInt64()
            $rightOffset = $reader.ReadUInt64()
            $rightBytes = $reader.ReadUInt64()
            $storedLeftHash = -join ($reader.ReadBytes(32) | ForEach-Object { $_.ToString('x2') })
            $storedRightHash = -join ($reader.ReadBytes(32) | ForEach-Object { $_.ToString('x2') })
            $expectedBytes = [uint64]$width * [uint64]$height * [uint32]4
            if ($magic -ne "FNVXSCN`0" -or $version -ne 1 -or $headerBytes -ne 160 -or
                $width -ne [uint32]$ExpectedWidth -or $height -ne [uint32]$ExpectedHeight -or
                $format -ne [uint32]$ExpectedFormat -or $pitch -ne ($width * [uint32]4) -or
                $eyeCount -ne 2 -or $sequence -ne [uint64]$ExpectedSequence -or
                $leftOffset -ne 160 -or $leftBytes -ne $expectedBytes -or
                $rightOffset -ne ($leftOffset + $leftBytes) -or $rightBytes -ne $expectedBytes -or
                [uint64]$stream.Length -ne ($rightOffset + $rightBytes) -or
                (($flags -band [uint32]1) -ne 0) -ne $ExpectedSeparated -or
                (($flags -band [uint32]2) -ne 0) -ne $ExpectedWorldCandidate -or
                ($flags -band [uint32]4) -eq 0) {
                return $false
            }
            $stream.Position = [int64]$leftOffset
            $leftPayload = $reader.ReadBytes([int]$leftBytes)
            $rightPayload = $reader.ReadBytes([int]$rightBytes)
            if ($leftPayload.Length -ne [int]$leftBytes -or $rightPayload.Length -ne [int]$rightBytes) {
                return $false
            }
            $sha = [Security.Cryptography.SHA256]::Create()
            try {
                $actualLeftHash = -join ($sha.ComputeHash($leftPayload) | ForEach-Object { $_.ToString('x2') })
                $actualRightHash = -join ($sha.ComputeHash($rightPayload) | ForEach-Object { $_.ToString('x2') })
            } finally {
                $sha.Dispose()
            }
            $expectedLeft = ([string]$ExpectedLeftSha256).ToLowerInvariant()
            $expectedRight = ([string]$ExpectedRightSha256).ToLowerInvariant()
            return $storedLeftHash -eq $expectedLeft -and $actualLeftHash -eq $expectedLeft -and
                $storedRightHash -eq $expectedRight -and $actualRightHash -eq $expectedRight
        } finally {
            $reader.Dispose()
            $stream.Dispose()
        }
    } catch {
        return $false
    }
}

function Test-CapturePixelMetrics($Metrics) {
    return $null -ne $Metrics -and
        [int]$Metrics.nonBlack -ge 64 -and
        [int]$Metrics.meaningfulDifferent -ge 64 -and
        [double]$Metrics.leftActiveFraction -ge 0.50 -and
        [double]$Metrics.rightActiveFraction -ge 0.50 -and
        [int]$Metrics.leftActiveTiles -ge 12 -and
        [int]$Metrics.rightActiveTiles -ge 12 -and
        [int]$Metrics.differentTiles -ge 8
}

function Test-CaptureManifest($Capture) {
    try {
        $manifestPath = [string]$Capture._manifestPath
        $directory = Split-Path -Parent $manifestPath
        if (-not (Test-Path -LiteralPath (Join-Path $directory '.complete') -PathType Leaf) -or
            -not $Capture.separated -or -not $Capture.firstSeparated -or
            -not $Capture.worldCandidate -or -not $Capture.firstWorldCandidate -or
            [int]$Capture.producerMode -ne 3 -or
            -not (Test-Unsigned32Advanced $Capture.sequence $Capture.firstQualifiedSequence) -or
            -not (Test-Unsigned32Advanced $Capture.renderPairSequence $Capture.firstQualifiedRenderPairSequence) -or
            -not (Test-Unsigned32Advanced $Capture.poseSequence $Capture.firstQualifiedPoseSequence) -or
            [int64]$Capture.renderedDisplayTime -le [int64]$Capture.firstRenderedDisplayTime -or
            -not (Test-PositiveUnsigned64 $Capture.producerEpoch) -or
            -not (Test-PositiveUnsigned64 $Capture.firstProducerEpoch) -or
            -not (Test-PositiveUnsigned64 $Capture.rendererProducerEpoch) -or
            -not (Test-PositiveUnsigned64 $Capture.firstRendererProducerEpoch) -or
            -not (Test-PositiveUnsigned64 $Capture.publicationGeneration) -or
            -not (Test-PositiveUnsigned64 $Capture.firstPublicationGeneration) -or
            [uint64]$Capture.publicationGeneration -le [uint64]$Capture.firstPublicationGeneration -or
            [string]$Capture.producerEpoch -ne [string]$Capture.firstProducerEpoch -or
            [string]$Capture.rendererProducerEpoch -ne [string]$Capture.firstRendererProducerEpoch -or
            [int]$Capture.referenceSpaceGeneration -le 0 -or
            [int]$Capture.referenceSpaceGeneration -ne [int]$Capture.firstReferenceSpaceGeneration -or
            [int]$Capture.producerProcessId -le 0 -or
            [int]$Capture.producerProcessId -ne [int]$Capture.firstProducerProcessId -or
            [int]$Capture.width -ne [int]$Capture.firstWidth -or
            [int]$Capture.height -ne [int]$Capture.firstHeight -or
            [int]$Capture.sourceFormat -ne [int]$Capture.firstSourceFormat -or
            -not (Test-CapturePixelMetrics $Capture.firstIndependentPixelMetrics) -or
            -not (Test-CapturePixelMetrics $Capture.independentPixelMetrics)) {
            return $false
        }

        $firstArtifact = Get-SafeCaptureChildPath $directory ([string]$Capture.firstArtifact)
        $artifact = Get-SafeCaptureChildPath $directory ([string]$Capture.artifact)
        $firstPreviewLeft = Get-SafeCaptureChildPath $directory ([string]$Capture.firstPreviewLeft)
        $firstPreviewRight = Get-SafeCaptureChildPath $directory ([string]$Capture.firstPreviewRight)
        $previewLeft = Get-SafeCaptureChildPath $directory ([string]$Capture.previewLeft)
        $previewRight = Get-SafeCaptureChildPath $directory ([string]$Capture.previewRight)
        $previewPaths = @($firstPreviewLeft, $firstPreviewRight, $previewLeft, $previewRight)
        if ($null -eq $firstArtifact -or $null -eq $artifact -or
            @($previewPaths | Where-Object {
                $null -eq $_ -or -not (Test-Path -LiteralPath $_ -PathType Leaf)
            }).Count -gt 0) {
            return $false
        }
        $firstArtifactValid = Test-SceneArtifact `
            $firstArtifact $Capture.firstArtifactSha256 `
            $Capture.firstLeftSha256 $Capture.firstRightSha256 `
            $Capture.firstWidth $Capture.firstHeight $Capture.firstSourceFormat `
            $Capture.firstQualifiedSequence $true $true
        $artifactValid = Test-SceneArtifact `
            $artifact $Capture.artifactSha256 `
            $Capture.leftSha256 $Capture.rightSha256 `
            $Capture.width $Capture.height $Capture.sourceFormat `
            $Capture.sequence $true $true
        return $firstArtifactValid -and $artifactValid
    } catch {
        return $false
    }
}

$plugin = Read-RunText "fnvxr_input_telemetry.log"
$d3d9 = Read-RunText "fnvxr_d3d9_proxy.log"
$hostText = Read-RunText "fnvxr_openxr_pose_host.out.log"
$launcher = Read-RunText "launcher-debug.log"
$manifestPath = Join-Path $RunDir "manifest.json"
$manifest = if (Test-Path -LiteralPath $manifestPath) {
    Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
} else {
    $null
}
$runtimeMode = if ($null -ne $manifest) { [string]$manifest.acceptanceProfile } else { "missing-manifest" }
$nativeStereoRun = $ExpectedProfile -eq "full-vr"
$retailRigRequested = $null -ne $manifest -and ([bool]$manifest.enableRetailRig -or [bool]$manifest.applyRetailRig)
$jsonParseErrors = 0

function Read-JsonEvents([string]$Text, [string]$EventName) {
    $events = @()
    foreach ($line in ($Text -split "`r?`n")) {
        $jsonStart = $line.IndexOf('{"event":"' + $EventName + '"')
        if ($jsonStart -lt 0) { continue }
        try {
            $events += ($line.Substring($jsonStart) | ConvertFrom-Json -ErrorAction Stop)
        } catch {
            $script:jsonParseErrors++
        }
    }
    return @($events)
}

$solveMatches = [regex]::Matches($plugin, 'retailRig solve[^\r\n]* seq=(\d+)')
$duplicatePoseSolves = @($solveMatches | Group-Object { $_.Groups[1].Value } | Where-Object Count -gt 1).Count
$stereoRejects = [regex]::Matches(
    $d3d9,
    '(?:native stereo pair rejected|single-traversal stereo rejected)').Count
$gameplayFlatTransitions = [regex]::Matches(
    $hostText,
    'renderMode[^\r\n]*stereoFullscreen=0[^\r\n]*stereoLossPlane=1[^\r\n]*runtimeGameplay=1').Count
$gameplayPlaneTransitions = [regex]::Matches(
    $hostText,
    'renderMode[^\r\n]*showGameplayPlane=1[^\r\n]*runtimeGameplay=1').Count
$gameplayStereoDropouts = 0
$gameplayStereoWasActive = $false
foreach ($line in ($hostText -split "`r?`n")) {
    if ($line -notmatch '^renderMode ' -or $line -notmatch 'runtimeGameplay=1') { continue }
    if ($line -match 'stereoFullscreen=1') {
        $gameplayStereoWasActive = $true
    } elseif ($gameplayStereoWasActive -and $line -match 'stereoFullscreen=0') {
        $gameplayStereoDropouts++
    }
}
$singleTraversalEvents = Read-JsonEvents $d3d9 "fnvxrSingleTraversalStereo"
$singleTraversalBadCount = @($singleTraversalEvents | Where-Object {
    [int]$_.originalTraversals -ne 1 -or [int]$_.replayDraws -le 0 -or
    [int]$_.programmableWorldCandidates -le 0 -or
    [int]$_.programmableWorldDraws -ne [int]$_.programmableWorldCandidates -or
    [int]$_.contractCoveredWorldDraws -ne [int]$_.programmableWorldCandidates -or
    [double]$_.shaderContractCoverage -lt 1.0 -or
    [int]$_.verifiedShaderContracts -le 0
}).Count
$headMatrixMatches = [regex]::Matches(
    $d3d9,
    '"event":"fnvxrSingleTraversalStereo"[^\r\n]*"centerCameraDelta":([0-9.eE+-]+),"headDeterminant":([0-9.eE+-]+),"headOrthonormalError":([0-9.eE+-]+)')
$invalidHeadMatrixSamples = @($headMatrixMatches | Where-Object {
    $cameraDelta = [Math]::Abs([double]$_.Groups[1].Value)
    $determinant = [double]$_.Groups[2].Value
    $orthonormalError = [Math]::Abs([double]$_.Groups[3].Value)
    $cameraDelta -gt 0.05 -or [Math]::Abs($determinant - 1.0) -gt 0.001 -or $orthonormalError -gt 0.001
}).Count
$singleTraversalPublished = [regex]::Matches(
    $d3d9,
    '"event":"fnvxrD3d9EyeTarget"[^\r\n]*"ready":true[^\r\n]*"producerMode":3[^\r\n]*"renderPairSequence":([1-9]\d*)').Count
$legacyNativePairs = [regex]::Matches($d3d9, 'native same-frame stereo pair=').Count
$leftWvpPatches = [regex]::Matches($d3d9, 'shader WVP replay[^\r\n]*eye=left[^\r\n]*result=0x00000000').Count
$rightWvpPatches = [regex]::Matches($d3d9, 'shader WVP replay[^\r\n]*eye=right[^\r\n]*result=0x00000000').Count
$unverifiedShaderSkips = [regex]::Matches($d3d9, 'reason=empty_verified_vertex_allowlist').Count
$wvpReplayConfigured = $d3d9 -match 'stereo config[^\r\n]*wvpReplay=1'
$visualCoverageRejects = [regex]::Matches($d3d9, 'shared stereo visual coverage rejected').Count
$eyeTargetEvents = Read-JsonEvents $d3d9 "fnvxrD3d9EyeTarget"
$readyMeasuredEyeEvents = @($eyeTargetEvents | Where-Object {
    $_.ready -and $_.visualCoverageMeasured -and [int]$_.producerMode -eq 3 -and
    $null -ne (Get-EyeEventIdentityKey $_)
})
$lowCoverageEyeEvents = @($readyMeasuredEyeEvents | Where-Object {
    [double]$_.leftActiveFraction -lt 0.50 -or [double]$_.rightActiveFraction -lt 0.50 -or
    [int]$_.leftActiveTiles -lt 12 -or [int]$_.rightActiveTiles -lt 12 -or
    [int]$_.differentTiles -lt 8
})

$rigEvents = Read-JsonEvents $plugin "fnvxrRigIndependence"
$projectileNodeEvents = Read-JsonEvents $plugin "fnvxrProjectileNodeConsume"
$submitEvents = Read-JsonEvents $hostText "fnvxrOpenXrSubmit"
$gameplaySubmitEvents = @($submitEvents | Where-Object { [bool]$_.runtimeGameplay })
$successfulGameplaySubmits = @($gameplaySubmitEvents | Where-Object { Test-GoodGameplaySubmit $_ } | Sort-Object { [uint64]$_.frame })
$duplicateGameplaySubmitFrames = $successfulGameplaySubmits.Count -
    @($successfulGameplaySubmits | Select-Object -ExpandProperty frame -Unique).Count
$firstSuccessfulSubmitFrame = if ($successfulGameplaySubmits.Count -gt 0) {
    [uint64]$successfulGameplaySubmits[0].frame
} else { [uint64]0 }
$badGameplaySubmitsAfterHandoff = @($submitEvents | Where-Object {
    $firstSuccessfulSubmitFrame -gt 0 -and [uint64]$_.frame -ge $firstSuccessfulSubmitFrame -and
    -not (Test-GoodGameplaySubmit $_)
})
$successfulSubmitFrameSpan = if ($successfulGameplaySubmits.Count -gt 1) {
    [uint64]$successfulGameplaySubmits[-1].frame - [uint64]$successfulGameplaySubmits[0].frame
} else { [uint64]0 }
$submitStereoAdvances = 0
$submitRenderPairAdvances = 0
$submitPoseAdvances = 0
$submitCounterDiscontinuities = 0
for ($i = 1; $i -lt $successfulGameplaySubmits.Count; $i++) {
    $older = $successfulGameplaySubmits[$i - 1]
    $newer = $successfulGameplaySubmits[$i]
    foreach ($counter in @(
        @('sourceStereoSequence', 'stereo'),
        @('sourceRenderPairSequence', 'renderPair'),
        @('sourcePoseSequence', 'pose'))) {
        $name = $counter[0]
        $olderValue = [uint32]$older.$name
        $newerValue = [uint32]$newer.$name
        if ($newerValue -eq $olderValue) { continue }
        if (Test-Unsigned32Advanced $newerValue $olderValue) {
            if ($counter[1] -eq 'stereo') { $submitStereoAdvances++ }
            elseif ($counter[1] -eq 'renderPair') { $submitRenderPairAdvances++ }
            else { $submitPoseAdvances++ }
        } else {
            $submitCounterDiscontinuities++
        }
    }
}
$latestSubmitAgeMilliseconds = [double]::PositiveInfinity
if ($successfulGameplaySubmits.Count -gt 0) {
    try {
        $nowUnixMilliseconds = [int64](([DateTime]::UtcNow - [DateTime]'1970-01-01').TotalMilliseconds)
        $latestSubmitAgeMilliseconds = [Math]::Abs(
            [double]($nowUnixMilliseconds - [int64]$successfulGameplaySubmits[-1].hostWallClockUnixMilliseconds))
    } catch {
        $latestSubmitAgeMilliseconds = [double]::PositiveInfinity
    }
}

$successfulQuadSubmits = @($submitEvents | Where-Object { Test-GoodQuadSubmit $_ } | Sort-Object { [uint64]$_.frame })
$duplicateQuadSubmitFrames = $successfulQuadSubmits.Count -
    @($successfulQuadSubmits | Select-Object -ExpandProperty frame -Unique).Count
$firstSuccessfulQuadFrame = if ($successfulQuadSubmits.Count -gt 0) {
    [uint64]$successfulQuadSubmits[0].frame
} else { [uint64]0 }
$badQuadSubmitsAfterHandoff = @($submitEvents | Where-Object {
    $firstSuccessfulQuadFrame -gt 0 -and [uint64]$_.frame -ge $firstSuccessfulQuadFrame -and
    -not (Test-GoodQuadSubmit $_)
})
$successfulQuadFrameSpan = if ($successfulQuadSubmits.Count -gt 1) {
    [uint64]$successfulQuadSubmits[-1].frame - [uint64]$successfulQuadSubmits[0].frame
} else { [uint64]0 }
$quadTextureAdvances = 0
$quadTextureDiscontinuities = 0
for ($i = 1; $i -lt $successfulQuadSubmits.Count; $i++) {
    $olderSequence = [uint32]$successfulQuadSubmits[$i - 1].gameTextureSequence
    $newerSequence = [uint32]$successfulQuadSubmits[$i].gameTextureSequence
    if ($newerSequence -eq $olderSequence) { continue }
    if (Test-Unsigned32Advanced $newerSequence $olderSequence) {
        $quadTextureAdvances++
    } else {
        $quadTextureDiscontinuities++
    }
}
$latestQuadSubmitAgeMilliseconds = [double]::PositiveInfinity
if ($successfulQuadSubmits.Count -gt 0) {
    try {
        $nowUnixMilliseconds = [int64](([DateTime]::UtcNow - [DateTime]'1970-01-01').TotalMilliseconds)
        $latestQuadSubmitAgeMilliseconds = [Math]::Abs(
            [double]($nowUnixMilliseconds - [int64]$successfulQuadSubmits[-1].hostWallClockUnixMilliseconds))
    } catch {
        $latestQuadSubmitAgeMilliseconds = [double]::PositiveInfinity
    }
}
$gameTextureMatches = [regex]::Matches(
    $hostText,
    'gameTexture update=(\d+) captured=1 transportFresh=1[^\r\n]* seq=([1-9]\d*)')
$gameTextureSequences = @($gameTextureMatches | ForEach-Object { [uint32]$_.Groups[2].Value })
$gameTextureAdvances = 0
$gameTextureDiscontinuities = 0
for ($i = 1; $i -lt $gameTextureSequences.Count; $i++) {
    if ($gameTextureSequences[$i] -eq $gameTextureSequences[$i - 1]) { continue }
    if (Test-Unsigned32Advanced $gameTextureSequences[$i] $gameTextureSequences[$i - 1]) {
        $gameTextureAdvances++
    } else {
        $gameTextureDiscontinuities++
    }
}
$lastGameTextureUpdateIndex = $hostText.LastIndexOf('gameTexture update=')
$lastGameTextureStaleIndex = $hostText.LastIndexOf('gameTexture staleExpired=1')
$lastGameTextureBlackIndex = [Math]::Max(
    $hostText.LastIndexOf('gameTexture rejectedMostlyBlack=1'),
    $hostText.LastIndexOf('gameTexture staleFallbackRejectedMostlyBlack=1'))
$flatTextureCurrentlyFresh = $lastGameTextureUpdateIndex -ge 0 -and
    $lastGameTextureStaleIndex -lt $lastGameTextureUpdateIndex -and
    $lastGameTextureBlackIndex -lt $lastGameTextureUpdateIndex
$readyEyeIdentityKeys = @{}
$readyPoseSequences = @{}
foreach ($event in $readyMeasuredEyeEvents) {
    $identityKey = Get-EyeEventIdentityKey $event
    if ($null -ne $identityKey) {
        $readyEyeIdentityKeys[$identityKey] = $true
        $readyPoseSequences[[string][int]$event.poseSequence] = $true
    }
}
$submitIdentityJoinFailures = @($successfulGameplaySubmits | Where-Object {
    $identityKey = Get-SubmitEventIdentityKey $_
    $null -eq $identityKey -or -not $readyEyeIdentityKeys.ContainsKey($identityKey)
})
$successfulSubmitIdentityKeys = @{}
foreach ($event in $successfulGameplaySubmits) {
    $identityKey = Get-SubmitEventIdentityKey $event
    if ($null -ne $identityKey) { $successfulSubmitIdentityKeys[$identityKey] = $true }
}
$rigAppliedEvents = @($rigEvents | Where-Object { $_.apply -and $_.rightSolved -and $_.weaponAligned })
$rigHeadOnlyEvents = @($rigAppliedEvents | Where-Object { $_.classification.headOnly })
$rigControllerOnlyEvents = @($rigAppliedEvents | Where-Object { $_.classification.controllerOnly })
$rigHeadOnlyViolations = @($rigHeadOnlyEvents | Where-Object {
    [double]$_.delta.targetUnits -gt 0.25 -or
    [double]$_.delta.handUnits -gt 0.75 -or
    [double]$_.delta.weaponUnits -gt 0.75 -or
    [double]$_.delta.weaponRadians -gt 0.04 -or
    [double]$_.delta.bodyUnits -gt 0.25 -or
    [double]$_.delta.bodyAnchorUnits -gt 0.25
})
function Get-VectorLength($Value) {
    return [Math]::Sqrt(
        [double]$Value[0] * [double]$Value[0] +
        [double]$Value[1] * [double]$Value[1] +
        [double]$Value[2] * [double]$Value[2])
}
function Get-VectorCosine($A, $B) {
    $aLength = Get-VectorLength $A
    $bLength = Get-VectorLength $B
    if ($aLength -le 1e-9 -or $bLength -le 1e-9) { return 0.0 }
    return (
        [double]$A[0] * [double]$B[0] +
        [double]$A[1] * [double]$B[1] +
        [double]$A[2] * [double]$B[2]) / ($aLength * $bLength)
}
$rigControllerFollowFailures = @($rigControllerOnlyEvents | Where-Object {
    $controllerTranslation = [double]$_.delta.controllerMeters
    $controllerRotation = [double]$_.delta.controllerRadians
    $targetTranslation = [double]$_.delta.targetUnits
    $handTranslation = [double]$_.delta.handUnits
    $weaponRotation = [double]$_.delta.weaponRadians
    $translationExpected = $controllerTranslation -ge 0.005
    $rotationExpected = $controllerRotation -ge 0.03
    $expectedUnits = $controllerTranslation * 70.0
    $targetGain = if ($expectedUnits -gt 0) { $targetTranslation / $expectedUnits } else { 0.0 }
    $handGain = if ($targetTranslation -gt 0) { $handTranslation / $targetTranslation } else { 0.0 }
    $weaponGain = if ($targetTranslation -gt 0) { [double]$_.delta.weaponUnits / $targetTranslation } else { 0.0 }
    $controllerGameVector = @(
        [double]$_.deltaVectors.controllerMeters[0],
        -[double]$_.deltaVectors.controllerMeters[2],
        [double]$_.deltaVectors.controllerMeters[1])
    $targetDirection = Get-VectorCosine $controllerGameVector $_.deltaVectors.targetUnits
    $handDirection = Get-VectorCosine $_.deltaVectors.targetUnits $_.deltaVectors.handUnits
    $weaponDirection = Get-VectorCosine $_.deltaVectors.targetUnits $_.deltaVectors.weaponUnits
    $translationFollowed = -not $translationExpected -or
        ($targetGain -ge 0.75 -and $targetGain -le 1.25 -and $targetDirection -ge 0.90 -and
         $handGain -ge 0.50 -and $handGain -le 1.50 -and $handDirection -ge 0.75 -and
         $weaponGain -ge 0.50 -and $weaponGain -le 1.50 -and $weaponDirection -ge 0.75)
    $rotationFollowed = -not $rotationExpected -or
        ($weaponRotation -ge ($controllerRotation * 0.50) -and
         $weaponRotation -le ($controllerRotation * 1.50))
    -not ($translationFollowed -and $rotationFollowed)
})
$rigTrackingErrors = @($rigAppliedEvents | Where-Object {
    [double]$_.handTargetErrorUnits -gt 0.25 -or
    -not $_.gravityAlignedOrigin -or
    [double]$_.originUpDotWorldUp -lt 0.9999 -or
    $_.originSource -ne 'd3d9-native-camera' -or
    $_.anchorSource -ne 'exact-d3d9-render-camera' -or
    [uint32]$_.referenceGeneration -eq 0 -or
    [uint32]$_.originPoseSeq -eq 0 -or
    [double]$_.headTermInRigTransform -ne 0.0 -or
    [int]$_.poseSeq -ne [int]$_.renderPoseSeq -or
    $_.culling.rootAppCulled -or $_.culling.rightHandAppCulled -or $_.culling.weaponAppCulled -or
    [int]$_.culling.rightHandVisibleLeaves -le 0 -or [int]$_.culling.weaponVisibleLeaves -le 0 -or
    [double]$_.weaponPositionResidualUnits -gt 0.25 -or
    -not $_.muzzleMeasured -or -not $_.muzzleInWeaponBranch -or
    [double]$_.muzzleAimResidualRadians -gt 0.08 -or
    -not $_.projectileNodeConsumeHookInstalled -or
    -not $_.weaponWriteRequested -or -not $_.weaponWriteAttempted -or -not $_.weaponWriteApplied
})
$rigPoseJoinFailures = @($rigAppliedEvents | Where-Object {
    -not $readyPoseSequences.ContainsKey([string][int]$_.renderPoseSeq)
})
$projectileLaunchProofEvents = @($projectileNodeEvents | Where-Object {
    $_.playerProcess -and $_.endpointMatches -and [int]$_.rightTrigger -gt 64 -and
    [int]$_.poseSeq -gt 0 -and [double]$_.aimResidualRadians -le 0.08 -and
    $readyPoseSequences.ContainsKey([string][int]$_.poseSeq)
})

$captureManifests = @()
$captureRoot = Join-Path $RunDir "live-stereo-captures"
if (Test-Path -LiteralPath $captureRoot) {
    foreach ($path in (Get-ChildItem -LiteralPath $captureRoot -Recurse -Filter "scene-cache.json" -File -ErrorAction SilentlyContinue)) {
        try {
            $capture = Get-Content -LiteralPath $path.FullName -Raw | ConvertFrom-Json -ErrorAction Stop
            $capture | Add-Member -NotePropertyName _manifestPath -NotePropertyValue $path.FullName -Force
            $captureManifests += $capture
        }
        catch { $jsonParseErrors++ }
    }
}
$invalidCaptures = @($captureManifests | Where-Object {
    -not (Test-CaptureManifest $_)
})
$captureIdentityJoinFailures = @($captureManifests | Where-Object {
    if (-not (Test-CaptureManifest $_)) { return $true }
    $firstIdentity = Get-StereoIdentityKey `
        $_.firstQualifiedSequence $_.firstQualifiedRenderPairSequence $_.firstQualifiedPoseSequence `
        $_.firstReferenceSpaceGeneration $_.firstProducerEpoch `
        $_.firstRendererProducerEpoch $_.firstProducerProcessId
    $currentIdentity = Get-StereoIdentityKey `
        $_.sequence $_.renderPairSequence $_.poseSequence `
        $_.referenceSpaceGeneration $_.producerEpoch `
        $_.rendererProducerEpoch $_.producerProcessId
    return $null -eq $firstIdentity -or $null -eq $currentIdentity -or
        -not $readyEyeIdentityKeys.ContainsKey($firstIdentity) -or
        -not $readyEyeIdentityKeys.ContainsKey($currentIdentity) -or
        -not $successfulSubmitIdentityKeys.ContainsKey($firstIdentity) -or
        -not $successfulSubmitIdentityKeys.ContainsKey($currentIdentity)
})

# These are implementation prerequisites, not tunable acceptance thresholds.
# Keeping them explicit makes a partial render/replay build fail closed instead
# of turning a visually plausible headset sample into a production PASS.
$knownStructuralBlockers = @(
    'isolated transaction render target/depth state is not implemented',
    'conservative stereo visibility/LOD/portal/particle traversal is not implemented',
    'per-eye depth is not submitted to OpenXR',
    'auxiliary render/resource graph twinning is incomplete',
    'exact VS+PS view-dependent semantic contracts and camera provenance are incomplete'
)
$productionStructuralProofComplete = $knownStructuralBlockers.Count -eq 0

$sixDofTransactionEvents = Read-JsonEvents $d3d9 "fnvxrSixDofTransaction"
$validSixDofTransactionEvents = @($sixDofTransactionEvents | Where-Object {
    $identity = Get-StereoIdentityKey `
        $_.sequence $_.renderPairSequence $_.poseSequence $_.referenceSpaceGeneration `
        $_.poseProducerEpoch $_.rendererProducerEpoch $_.producerProcessId
    $axis = [string]$_.axis
    $direction = [int]$_.direction
    $gain = [double]$_.signedGain
    $inputMagnitude = [Math]::Abs([double]$_.inputMagnitude)
    $orthogonalLeakage = [Math]::Abs([double]$_.orthogonalLeakage)
    $null -ne $identity -and $readyEyeIdentityKeys.ContainsKey($identity) -and
        $successfulSubmitIdentityKeys.ContainsKey($identity) -and
        $axis -in @('translationX','translationY','translationZ','yaw','pitch','roll') -and
        $direction -in @(-1,1) -and $inputMagnitude -ge 0.01 -and
        $gain -ge 0.75 -and $gain -le 1.25 -and $orthogonalLeakage -le 0.10
})
$missingSixDofDirections = @()
foreach ($axis in @('translationX','translationY','translationZ','yaw','pitch','roll')) {
    foreach ($direction in @(-1,1)) {
        if (@($validSixDofTransactionEvents | Where-Object {
            [string]$_.axis -eq $axis -and [int]$_.direction -eq $direction
        }).Count -eq 0) {
            $missingSixDofDirections += "$axis/$direction"
        }
    }
}
$invariantMatches = [regex]::Matches(
    $d3d9,
    '"event":"fnvxrHeadBodyInvariant"[^\r\n]*"playerValid":true[^\r\n]*"hmdYaw":(-?[0-9.]+),"playerYaw":(-?[0-9.]+)[^\r\n]*"eyeCenterError":([0-9.]+)')
$coupledHeadSamples = 0
$movingHeadSamples = 0
$maxEyeCenterError = 0.0
$sumHeadSquared = 0.0
$sumPlayerSquared = 0.0
$sumHeadPlayer = 0.0
function Get-AngularDelta([double]$Current, [double]$Previous) {
    $delta = $Current - $Previous
    return [Math]::Atan2([Math]::Sin($delta), [Math]::Cos($delta))
}
for ($i = 1; $i -lt $invariantMatches.Count; $i++) {
    $previous = $invariantMatches[$i - 1]
    $current = $invariantMatches[$i]
    $signedHmdDelta = Get-AngularDelta ([double]$current.Groups[1].Value) ([double]$previous.Groups[1].Value)
    $signedPlayerDelta = Get-AngularDelta ([double]$current.Groups[2].Value) ([double]$previous.Groups[2].Value)
    $hmdDelta = [Math]::Abs($signedHmdDelta)
    $playerDelta = [Math]::Abs($signedPlayerDelta)
    $maxEyeCenterError = [Math]::Max($maxEyeCenterError, [double]$current.Groups[3].Value)
    if ($hmdDelta -ge 0.015) {
        $movingHeadSamples++
        if ($playerDelta -ge ($hmdDelta * 0.25)) { $coupledHeadSamples++ }
        $sumHeadSquared += $signedHmdDelta * $signedHmdDelta
        $sumPlayerSquared += $signedPlayerDelta * $signedPlayerDelta
        $sumHeadPlayer += $signedHmdDelta * $signedPlayerDelta
    }
}
$headCouplingFraction = if ($movingHeadSamples -gt 0) { $coupledHeadSamples / $movingHeadSamples } else { 0.0 }
$headBodyCorrelation = if ($sumHeadSquared -gt 0.0 -and $sumPlayerSquared -gt 0.0) {
    $sumHeadPlayer / [Math]::Sqrt($sumHeadSquared * $sumPlayerSquared)
} else { 0.0 }
$headToBodyGain = if ($sumHeadSquared -gt 0.0) { $sumHeadPlayer / $sumHeadSquared } else { 0.0 }
$headBodyActuallyCoupled = [Math]::Abs($headBodyCorrelation) -ge 0.75 -and [Math]::Abs($headToBodyGain) -ge 0.25

$checks = @(
    [ordered]@{ name="manifest_authority"; applicable=$true; pass=($null -ne $manifest -and -not [bool]$manifest.failed -and $runtimeMode -eq $ExpectedProfile); detail=("profile={0} expected={1} failed={2}" -f $runtimeMode,$ExpectedProfile,$(if($manifest){[bool]$manifest.failed}else{$true})) },
    [ordered]@{ name="telemetry_json_integrity"; applicable=$true; pass=($jsonParseErrors -eq 0); detail="jsonParseErrors=$jsonParseErrors" },
    [ordered]@{ name="openxr_pose_stream"; applicable=$true; pass=($hostText -match 'poseFrame='); detail="host must publish tracked poses" },
    [ordered]@{ name="production_structural_proof"; applicable=$nativeStereoRun; pass=$productionStructuralProofComplete; detail=("blocked: {0}" -f ($knownStructuralBlockers -join '; ')) },
    [ordered]@{ name="flat_surface_mode"; applicable=(-not $nativeStereoRun); pass=($runtimeMode -eq "quad-2d-transport"); detail="2D validates only the compositor surface, never native camera/arms/stereo" },
    [ordered]@{ name="flat_surface_live_pixels_and_submit"; applicable=(-not $nativeStereoRun); pass=($successfulQuadSubmits.Count -ge 14 -and $duplicateQuadSubmitFrames -eq 0 -and $successfulQuadFrameSpan -ge 119 -and $badQuadSubmitsAfterHandoff.Count -eq 0 -and $quadTextureAdvances -ge 2 -and $quadTextureDiscontinuities -eq 0 -and $gameTextureMatches.Count -ge 3 -and $gameTextureAdvances -ge 2 -and $gameTextureDiscontinuities -eq 0 -and $flatTextureCurrentlyFresh -and $latestQuadSubmitAgeMilliseconds -le 10000.0); detail=("goodSubmits={0} duplicateFrames={1} frameSpan={2} badAfterHandoff={3} submitTextureAdvances={4} submitDiscontinuities={5} capturedUpdates={6} logAdvances={7} logDiscontinuities={8} currentlyFresh={9} latestSubmitAgeMs={10:N0}" -f $successfulQuadSubmits.Count,$duplicateQuadSubmitFrames,$successfulQuadFrameSpan,$badQuadSubmitsAfterHandoff.Count,$quadTextureAdvances,$quadTextureDiscontinuities,$gameTextureMatches.Count,$gameTextureAdvances,$gameTextureDiscontinuities,$flatTextureCurrentlyFresh,$latestQuadSubmitAgeMilliseconds) },
    [ordered]@{ name="head_axis_basis"; applicable=$nativeStereoRun; pass=($launcher -match 'native head axis mode=openxr-camera-local'); detail="native temporary-camera yaw/roll/pitch slots must receive the selected OpenXR camera-local mapping" },
    [ordered]@{ name="six_axis_signed_gain"; applicable=$nativeStereoRun; pass=($missingSixDofDirections.Count -eq 0); detail=("validTransactions={0} missingSignedAxes={1}" -f $validSixDofTransactionEvents.Count,($missingSixDofDirections -join ',')) },
    [ordered]@{ name="single_traversal_config"; applicable=$nativeStereoRun; pass=($d3d9 -match 'stereo config runtime=1 singleTraversal=1 nativeMultipass=0'); detail="one HMD-centered Gamebryo traversal must replace divergent native multipass" },
    [ordered]@{ name="one_world_traversal_two_eye_replay"; applicable=$nativeStereoRun; pass=($singleTraversalEvents.Count -ge 3 -and $singleTraversalBadCount -eq 0); detail=("transactions={0} invalidOrIncompleteContractTransactions={1}" -f $singleTraversalEvents.Count,$singleTraversalBadCount) },
    [ordered]@{ name="camera_matrix_invariants"; applicable=$nativeStereoRun; pass=($headMatrixMatches.Count -ge 3 -and $invalidHeadMatrixSamples -eq 0); detail=("samples={0} invalidSamples={1}; requires stable center camera, det(R)=1, and orthonormal R" -f $headMatrixMatches.Count,$invalidHeadMatrixSamples) },
    [ordered]@{ name="single_traversal_provenance_published"; applicable=$nativeStereoRun; pass=($singleTraversalPublished -ge 1); detail=("readyProducerMode3Frames={0}" -f $singleTraversalPublished) },
    [ordered]@{ name="visual_scene_coverage"; applicable=$nativeStereoRun; pass=($readyMeasuredEyeEvents.Count -ge 1 -and $lowCoverageEyeEvents.Count -eq 0 -and $visualCoverageRejects -eq 0); detail=("readyMeasuredFrames={0} lowCoverageAccepted={1} rejectedFrames={2}; each eye requires >=0.50 non-dominant samples across >=12/16 tiles and stereo disparity across >=8/16 tiles" -f $readyMeasuredEyeEvents.Count,$lowCoverageEyeEvents.Count,$visualCoverageRejects) },
    [ordered]@{ name="verified_shader_contract_coverage"; applicable=$nativeStereoRun; pass=($wvpReplayConfigured -and -not [string]::IsNullOrWhiteSpace([string]$manifest.stereoPipeline.shaderWvpContracts) -and $leftWvpPatches -gt 0 -and $rightWvpPatches -gt 0 -and $singleTraversalBadCount -eq 0); detail=("wvpReplayConfigured={0} successfulWvpPatches left={1} right={2} fullCoverageTransactions={3}" -f $wvpReplayConfigured,$leftWvpPatches,$rightWvpPatches,$singleTraversalEvents.Count) },
    [ordered]@{ name="legacy_double_traversal_absent"; applicable=$nativeStereoRun; pass=($legacyNativePairs -eq 0); detail=("legacyNativePairs={0}" -f $legacyNativePairs) },
    [ordered]@{ name="head_mouse_lane_disabled"; applicable=$nativeStereoRun; pass=($hostText -match 'headspaceLook=0'); detail="HMD motion must not rotate the retail body through mouse look" },
    [ordered]@{ name="head_body_decoupled"; applicable=$nativeStereoRun; pass=($movingHeadSamples -ge 3 -and -not $headBodyActuallyCoupled); detail=("movingHeadSamples={0} coincidentSamples={1} fraction={2:N3} signedCorrelation={3:N3} bodyGain={4:N3}" -f $movingHeadSamples,$coupledHeadSamples,$headCouplingFraction,$headBodyCorrelation,$headToBodyGain) },
    [ordered]@{ name="eyes_centered_on_head"; applicable=$nativeStereoRun; pass=($invariantMatches.Count -ge 3 -and $maxEyeCenterError -le 0.001); detail=("samples={0} maxCenterError={1:N6}m" -f $invariantMatches.Count,$maxEyeCenterError) },
    [ordered]@{ name="synthetic_host_hands_disabled"; applicable=$nativeStereoRun; pass=($null -ne $manifest -and $manifest.stereoPipeline.syntheticBodyRig -eq "0" -and $manifest.stereoPipeline.syntheticHandFingers -eq "0"); detail="host-rendered synthetic hands/body must stay disabled; only the retail arm/gun path may own gameplay visuals" },
    [ordered]@{ name="retail_rig_required"; applicable=$nativeStereoRun; pass=($retailRigRequested -and [bool]$manifest.applyRetailRig); detail="full VR acceptance requires the independent retail arm/gun write lane" },
    [ordered]@{ name="complete_arm_chains"; applicable=$nativeStereoRun; pass=($plugin -match 'retailRig discovery[^\r\n]*complete=1'); detail="both retail arm chains must be discovered" },
    [ordered]@{ name="weapon_node"; applicable=$nativeStereoRun; pass=($plugin -match 'retailRig discovery[^\r\n]*weapon=(?!00000000)[0-9A-F]+'); detail="retail weapon node must exist" },
    [ordered]@{ name="muzzle_or_projectile_node"; applicable=$nativeStereoRun; pass=($plugin -match 'retailRig discovery[^\r\n]*(projectile|muzzleFlash)=(?!00000000)[0-9A-F]+'); detail="authoritative firing alignment needs a retail endpoint" },
    [ordered]@{ name="one_rig_solve_per_pose"; applicable=$nativeStereoRun; pass=($duplicatePoseSolves -eq 0); detail="duplicatePoseSequences=$duplicatePoseSolves" },
    [ordered]@{ name="rig_uses_exact_render_transaction"; applicable=$nativeStereoRun; pass=($rigAppliedEvents.Count -ge 3 -and $rigTrackingErrors.Count -eq 0 -and $rigPoseJoinFailures.Count -eq 0); detail=("appliedSamples={0} transformOrTrackingErrors={1} stereoPoseJoinFailures={2}" -f $rigAppliedEvents.Count,$rigTrackingErrors.Count,$rigPoseJoinFailures.Count) },
    [ordered]@{ name="head_moves_gun_stays"; applicable=$nativeStereoRun; pass=($rigHeadOnlyEvents.Count -ge 3 -and $rigHeadOnlyViolations.Count -eq 0); detail=("headOnlySamples={0} violations={1}; target<=0.25u hand<=0.75u weapon translation/rotation stable" -f $rigHeadOnlyEvents.Count,$rigHeadOnlyViolations.Count) },
    [ordered]@{ name="controller_moves_gun_follows"; applicable=$nativeStereoRun; pass=($rigControllerOnlyEvents.Count -ge 3 -and $rigControllerFollowFailures.Count -eq 0); detail=("controllerOnlySamples={0} followFailures={1}" -f $rigControllerOnlyEvents.Count,$rigControllerFollowFailures.Count) },
    [ordered]@{ name="retail_projectile_node_consumes_controller_aim"; applicable=$nativeStereoRun; pass=($projectileLaunchProofEvents.Count -ge 1); detail=("triggeredGetProjectileNodeProofEvents={0}; requires player process, exact endpoint, joined pose, and <=0.08rad aim residual" -f $projectileLaunchProofEvents.Count) },
    [ordered]@{ name="independent_two_frame_pixel_capture"; applicable=$nativeStereoRun; pass=($captureManifests.Count -ge 1 -and $invalidCaptures.Count -eq 0 -and $captureIdentityJoinFailures.Count -eq 0); detail=("captures={0} invalid={1} fullIdentityJoinFailures={2}; requires two immutable advancing mode-3 artifacts joined to both D3D publication and OpenXR submit" -f $captureManifests.Count,$invalidCaptures.Count,$captureIdentityJoinFailures.Count) },
    [ordered]@{ name="openxr_exact_pose_submit"; applicable=$nativeStereoRun; pass=($successfulGameplaySubmits.Count -ge 14 -and $duplicateGameplaySubmitFrames -eq 0 -and $successfulSubmitFrameSpan -ge 120 -and $badGameplaySubmitsAfterHandoff.Count -eq 0 -and $submitIdentityJoinFailures.Count -eq 0 -and $submitCounterDiscontinuities -eq 0 -and $submitStereoAdvances -ge 2 -and $submitRenderPairAdvances -ge 2 -and $submitPoseAdvances -ge 2 -and $latestSubmitAgeMilliseconds -le 10000.0); detail=("good={0} duplicateFrames={1} frameSpan={2} badAfterHandoff={3} fullIdentityJoinFailures={4} discontinuities={5} advances={6}/{7}/{8} latestAgeMs={9:N0}" -f $successfulGameplaySubmits.Count,$duplicateGameplaySubmitFrames,$successfulSubmitFrameSpan,$badGameplaySubmitsAfterHandoff.Count,$submitIdentityJoinFailures.Count,$submitCounterDiscontinuities,$submitStereoAdvances,$submitRenderPairAdvances,$submitPoseAdvances,$latestSubmitAgeMilliseconds) },
    [ordered]@{ name="stereo_pairs_not_rejected"; applicable=$nativeStereoRun; pass=($stereoRejects -eq 0); detail="rejectedPairs=$stereoRejects" },
    [ordered]@{ name="no_gameplay_flat_transitions"; applicable=$nativeStereoRun; pass=($gameplayFlatTransitions -eq 0 -and $gameplayPlaneTransitions -eq 0 -and $gameplayStereoDropouts -eq 0); detail=("gameplayFlatTransitions={0} gameplayPlaneTransitions={1} stereoDropoutsAfterFirstActive={2}" -f $gameplayFlatTransitions,$gameplayPlaneTransitions,$gameplayStereoDropouts) }
)

$failed = @($checks | Where-Object { $_.applicable -and -not $_.pass })
$verdict = [ordered]@{
    generatedAt = (Get-Date).ToString("o")
    runDir = (Resolve-Path -LiteralPath $RunDir).Path
    runtimeMode = $runtimeMode
    expectedProfile = $ExpectedProfile
    passed = $failed.Count -eq 0
    failedCount = $failed.Count
    checks = $checks
}
$verdictPath = Join-Path $RunDir "live-acceptance.json"
$verdict | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $verdictPath -Encoding UTF8
$checks | ForEach-Object {
    "[{0}] {1} - {2}" -f $(if (-not $_.applicable) { "N/A" } elseif ($_.pass) { "PASS" } else { "FAIL" }), $_.name, $_.detail
}
if ($failed.Count -gt 0) { exit 2 }
exit 0
