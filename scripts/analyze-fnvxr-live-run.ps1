param(
    [Parameter(Mandatory = $true)]
    [string]$RunDir
)

$ErrorActionPreference = "Stop"

function Read-RunText([string]$Name) {
    $path = Join-Path $RunDir $Name
    if (Test-Path -LiteralPath $path) { return Get-Content -LiteralPath $path -Raw }
    return ""
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
$nativeStereoRun = $runtimeMode -eq "full-vr"
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
$stereoRejects = [regex]::Matches($d3d9, 'native stereo pair rejected').Count
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
$readyMeasuredEyeEvents = @($eyeTargetEvents | Where-Object { $_.ready -and $_.visualCoverageMeasured })
$lowCoverageEyeEvents = @($readyMeasuredEyeEvents | Where-Object {
    [double]$_.leftActiveFraction -lt 0.50 -or [double]$_.rightActiveFraction -lt 0.50 -or
    [int]$_.leftActiveTiles -lt 12 -or [int]$_.rightActiveTiles -lt 12 -or
    [int]$_.differentTiles -lt 8
})

$rigEvents = Read-JsonEvents $plugin "fnvxrRigIndependence"
$projectileNodeEvents = Read-JsonEvents $plugin "fnvxrProjectileNodeConsume"
$submitEvents = Read-JsonEvents $hostText "fnvxrOpenXrSubmit"
$successfulGameplaySubmits = @($submitEvents | Where-Object {
    $_.runtimeGameplay -and $_.stereoFullscreen -and $_.projectionLayerSubmitted -and
    [int]$_.layerCount -eq 1 -and $_.xrEndFrame -eq 'XR_SUCCESS' -and
    [int]$_.sourceStereoSequence -gt 0 -and [int]$_.sourcePoseSequence -gt 0 -and
    [int]$_.meaningfulDifferentSamples -ge 64 -and [int]$_.nonBlackSamples -ge 64 -and
    [int]$_.leftActiveTiles -ge 12 -and [int]$_.rightActiveTiles -ge 12 -and
    [int]$_.differentTiles -ge 8
})
$readyPoseSequences = @{}
foreach ($event in $readyMeasuredEyeEvents) {
    if ($event.ready -and [int]$event.poseSequence -gt 0) {
        $readyPoseSequences[[string][int]$event.poseSequence] = $true
    }
}
$submitPoseJoinFailures = @($successfulGameplaySubmits | Where-Object {
    -not $readyPoseSequences.ContainsKey([string][int]$_.sourcePoseSequence)
})
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
        try { $captureManifests += (Get-Content -LiteralPath $path.FullName -Raw | ConvertFrom-Json -ErrorAction Stop) }
        catch { $jsonParseErrors++ }
    }
}
$invalidCaptures = @($captureManifests | Where-Object {
    -not $_.separated -or -not $_.worldCandidate -or [int]$_.producerMode -ne 3 -or
    [int]$_.sequence -le [int]$_.firstQualifiedSequence -or
    [int]$_.renderPairSequence -le [int]$_.firstQualifiedRenderPairSequence -or
    [int]$_.poseSequence -le 0 -or [int]$_.producerEpoch -le 0 -or
    [int]$_.independentPixelMetrics.nonBlack -lt 64 -or
    [int]$_.independentPixelMetrics.meaningfulDifferent -lt 64 -or
    [double]$_.independentPixelMetrics.leftActiveFraction -lt 0.50 -or
    [double]$_.independentPixelMetrics.rightActiveFraction -lt 0.50 -or
    [int]$_.independentPixelMetrics.leftActiveTiles -lt 12 -or
    [int]$_.independentPixelMetrics.rightActiveTiles -lt 12 -or
    [int]$_.independentPixelMetrics.differentTiles -lt 8
})
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
    [ordered]@{ name="manifest_authority"; applicable=$true; pass=($null -ne $manifest -and -not [bool]$manifest.failed -and $runtimeMode -in @('full-vr','quad-2d-transport','diagnostic-producer')); detail=("profile={0} failed={1}" -f $runtimeMode,$(if($manifest){[bool]$manifest.failed}else{$true})) },
    [ordered]@{ name="telemetry_json_integrity"; applicable=$true; pass=($jsonParseErrors -eq 0); detail="jsonParseErrors=$jsonParseErrors" },
    [ordered]@{ name="openxr_pose_stream"; applicable=$true; pass=($hostText -match 'poseFrame='); detail="host must publish tracked poses" },
    [ordered]@{ name="flat_surface_mode"; applicable=(-not $nativeStereoRun); pass=($runtimeMode -eq "quad-2d-transport"); detail="2D validates only the compositor surface, never native camera/arms/stereo" },
    [ordered]@{ name="head_axis_basis"; applicable=$nativeStereoRun; pass=($launcher -match 'native head axis mode=retail-camera'); detail="native temporary-camera yaw/roll/pitch slots must receive the selected camera-local mapping" },
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
    [ordered]@{ name="independent_two_frame_pixel_capture"; applicable=$nativeStereoRun; pass=($captureManifests.Count -ge 1 -and $invalidCaptures.Count -eq 0); detail=("captures={0} invalid={1}; requires two advancing mode-3 frames, >=0.50 active fraction, >=12/16 active tiles per eye, and >=8/16 different tiles" -f $captureManifests.Count,$invalidCaptures.Count) },
    [ordered]@{ name="openxr_exact_pose_submit"; applicable=$nativeStereoRun; pass=($successfulGameplaySubmits.Count -ge 3 -and $submitPoseJoinFailures.Count -eq 0); detail=("successfulGameplaySubmits={0} producerPoseJoinFailures={1}" -f $successfulGameplaySubmits.Count,$submitPoseJoinFailures.Count) },
    [ordered]@{ name="stereo_pairs_not_rejected"; applicable=$nativeStereoRun; pass=($stereoRejects -eq 0); detail="rejectedPairs=$stereoRejects" },
    [ordered]@{ name="no_gameplay_flat_transitions"; applicable=$nativeStereoRun; pass=($gameplayFlatTransitions -eq 0 -and $gameplayPlaneTransitions -eq 0 -and $gameplayStereoDropouts -eq 0); detail=("gameplayFlatTransitions={0} gameplayPlaneTransitions={1} stereoDropoutsAfterFirstActive={2}" -f $gameplayFlatTransitions,$gameplayPlaneTransitions,$gameplayStereoDropouts) }
)

$failed = @($checks | Where-Object { $_.applicable -and -not $_.pass })
$verdict = [ordered]@{
    generatedAt = (Get-Date).ToString("o")
    runDir = (Resolve-Path -LiteralPath $RunDir).Path
    runtimeMode = $runtimeMode
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
