param(
    [Parameter(Mandatory = $true)][string]$SourceRoot
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

$verifierPath = Join-Path $SourceRoot "scripts\verify-fnvxr-phase1-6dof.ps1"
$launcherPath = Join-Path $SourceRoot "scripts\start-fnvxr-product.ps1"
$sweepPath = Join-Path $SourceRoot "scripts\invoke-openxr-simulator-head-sweep.ps1"
$physicalCardinalScriptPath = Join-Path $SourceRoot "scripts\new-fnvxr-phase1-physical-cardinal-script.ps1"
$d3d9SourcePath = Join-Path $SourceRoot "renderhook\fnvxr_d3d9_proxy.cpp"
$hostSourcePath = Join-Path $SourceRoot "host\fnvxr_openxr_pose_host.cpp"
foreach ($path in @(
        $verifierPath,
        $launcherPath,
        $sweepPath,
        $physicalCardinalScriptPath,
        $d3d9SourcePath,
        $hostSourcePath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Phase 1 evidence contract file is missing: $path"
    }
}
[void][ScriptBlock]::Create((Get-Content -LiteralPath $verifierPath -Raw))
[void][ScriptBlock]::Create((Get-Content -LiteralPath $launcherPath -Raw))
[void][ScriptBlock]::Create((Get-Content -LiteralPath $sweepPath -Raw))
[void][ScriptBlock]::Create((Get-Content -LiteralPath $physicalCardinalScriptPath -Raw))

$launcher = Get-Content -LiteralPath $launcherPath -Raw
$sweep = Get-Content -LiteralPath $sweepPath -Raw
$physicalCardinalScript = Get-Content -LiteralPath $physicalCardinalScriptPath -Raw
$d3d9Source = Get-Content -LiteralPath $d3d9SourcePath -Raw
$hostSource = Get-Content -LiteralPath $hostSourcePath -Raw
foreach ($contract in @(
    'verify-fnvxr-phase1-6dof.ps1',
    'new-fnvxr-phase1-physical-cardinal-script.ps1',
    '-Pattern Cardinal',
    'phase1-6dof-evidence.json',
    'physicalHeadsetGateAccepted = $false',
    'rendered-six-dof-cardinal-proven-centered',
    'retained-pending-operator-execution')) {
    if (-not $launcher.Contains($contract)) {
        throw "Product launcher lost Phase 1 evidence contract: $contract"
    }
}
foreach ($contract in @(
    '[ValidateSet("Cardinal", "Sinusoidal")]',
    'pattern = if ($Pattern -ceq "Cardinal") { "cardinal-v1" }',
    '"translationX"',
    '"translationY"',
    '"translationZ"',
    '"yaw"',
    '"pitch"',
    '"roll"',
    'commands = @($commands.ToArray())')) {
    if (-not $sweep.Contains($contract)) {
        throw "Headless simulator cardinal fixture lost contract: $contract"
    }
}
foreach ($contract in @(
    'fnvxr-phase1-physical-cardinal-script-v1',
    'sends no XR, game, controller, desktop, keyboard, mouse, registry, or simulator input',
    '"translationX"',
    '"translationY"',
    '"translationZ"',
    '"yaw"',
    '"pitch"',
    '"roll"')) {
    if (-not $physicalCardinalScript.Contains($contract)) {
        throw "Physical cardinal protocol lost contract: $contract"
    }
}
foreach ($contract in @(
    'originPoseFrame',
    'originReferenceSpaceGeneration',
    'originProducerEpoch',
    'originPosition',
    'leftTranslation',
    'rightTranslation',
    'leftRendererCameraMatches',
    'rightRendererCameraMatches',
    'leftCullerCameraMatches',
    'rightCullerCameraMatches',
    'rendererCullerCameraMatch',
    'stockCameraTransformUsable',
    'eyeBaselineValid',
    'eyeMidpointDistanceMeters',
    'gRetailCpuStereoEvidenceTransaction',
    'cpuPublicationEvidenceForCurrentTransaction',
    'logRetailVrLine(invariant)')) {
    if (-not $d3d9Source.Contains($contract)) {
        throw "Engine-center Phase 1 telemetry lost contract: $contract"
    }
}
foreach ($contract in @(
    'FNVXR_PHASE1_TRACE_TELEMETRY',
    'fnvxrPhase1OpenXrEngineCenterSubmit',
    'lastPhase1TraceCpuTransaction',
    'projectionLayerSubmitted\":true',
    'xrEndFrame\":\"XR_SUCCESS')) {
    if (-not $hostSource.Contains($contract)) {
        throw "OpenXR host lost Phase 1 transaction trace contract: $contract"
    }
}

$encoding = New-Object System.Text.UTF8Encoding($false)
$testRoot = Join-Path ([System.IO.Path]::GetTempPath()) (
    "fnvxr-phase1-verifier-" + [Guid]::NewGuid().ToString("N"))

function Write-Phase1Json {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)]$Value
    )
    [System.IO.File]::WriteAllText(
        $Path,
        ($Value | ConvertTo-Json -Depth 12),
        $encoding)
}

function Write-Phase1JsonLines {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][object[]]$Values
    )
    $lines = @($Values | ForEach-Object {
        $_ | ConvertTo-Json -Compress -Depth 12
    })
    [System.IO.File]::WriteAllLines($Path, $lines, $encoding)
}

function New-CardinalCommands {
    $commands = New-Object 'System.Collections.Generic.List[object]'
    $ordinal = 0
    foreach ($axis in @(
        "translationX", "translationY", "translationZ", "yaw", "pitch", "roll")) {
        foreach ($direction in @(-1, 1)) {
            ++$ordinal
            [void]$commands.Add([ordered]@{
                ordinal = $ordinal
                axis = $axis
                direction = $direction
            })
        }
    }
    [void]$commands.Add([ordered]@{
        ordinal = ++$ordinal
        axis = "center"
        direction = 0
    })
    return @($commands.ToArray())
}

function New-Phase1Manifest {
    param(
        [Parameter(Mandatory = $true)][bool]$Physical,
        [Parameter(Mandatory = $true)][hashtable]$Readiness,
        [AllowNull()][string]$Error = $null,
        [switch]$Simulator
    )

    $sweep = [ordered]@{
        requested = [bool]$Simulator
        status = if ($Simulator) {
            "rendered-six-dof-cardinal-proven-centered"
        } else {
            "disabled"
        }
        evidence = if ($Simulator) {
            [ordered]@{
                pattern = "cardinal-v1"
                commandCount = 13
                centerRestored = $true
                commands = New-CardinalCommands
            }
        } else {
            $null
        }
        renderedCameraProof = if ($Simulator) {
            [ordered]@{ sixDofCameraResponseProven = $true }
        } else {
            $null
        }
    }
    return [ordered]@{
        schema = 1
        state = if ($Error) { "failed" } else { "complete" }
        error = $Error
        physicalHeadsetPlay = [ordered]@{
            requested = $Physical
            profile = if ($Physical) { "retail-vr-play-v1" } else { "disabled" }
            controllerConsumerAcknowledged = $false
        }
        headsetPoseSweep = $sweep
        readiness = $Readiness
        logs = [ordered]@{
            retailVrBridge = "fnvxr_retail_vr.log"
            hostStdout = "host.stdout.log"
            hostStderr = "host.stderr.log"
        }
    }
}

function New-Phase1Run {
    param([Parameter(Mandatory = $true)][string]$Name)
    $directory = Join-Path $testRoot $Name
    [void][System.IO.Directory]::CreateDirectory($directory)
    return $directory
}

function Invoke-Phase1Report {
    param([Parameter(Mandatory = $true)][string]$Directory)
    $output = @(& $verifierPath -RunDirectory $Directory) -join [Environment]::NewLine
    return $output | ConvertFrom-Json -ErrorAction Stop
}

try {
    [void][System.IO.Directory]::CreateDirectory($testRoot)

    # The physical companion is an operator-only record.  It must retain every
    # signed axis but cannot manufacture a pose sample or claim acceptance.
    $physicalProtocolDirectory = New-Phase1Run -Name "physical-cardinal-protocol"
    $physicalProtocolOutput = @(
        & $physicalCardinalScriptPath -RunDirectory $physicalProtocolDirectory
    ) -join [Environment]::NewLine
    $physicalProtocol = $physicalProtocolOutput | ConvertFrom-Json -ErrorAction Stop
    if ($physicalProtocol.schema -cne "fnvxr-phase1-physical-cardinal-script-v1" -or
        @($physicalProtocol.steps).Count -ne 12) {
        throw "The physical cardinal protocol did not retain its 12 signed steps."
    }
    foreach ($axis in @(
        "translationX", "translationY", "translationZ", "yaw", "pitch", "roll")) {
        foreach ($direction in @(-1, 1)) {
            if (@($physicalProtocol.steps | Where-Object {
                    [string]$_.axis -ceq $axis -and
                    [int]$_.direction -eq $direction
                }).Count -ne 1) {
                throw "The physical cardinal protocol omitted $axis/$direction."
            }
        }
    }
    $physicalProtocolManifest = New-Phase1Manifest -Physical $true -Readiness @{
        hostBridge = $true
        hostPose = $true
        retailRuntimeAndPose = $true
        retailVrBridge = $true
    }
    $physicalProtocolManifest.physicalHeadsetPlay.cardinalScript =
        Join-Path $physicalProtocolDirectory "phase1-physical-cardinal-script.json"
    Write-Phase1Json -Path (Join-Path $physicalProtocolDirectory "manifest.json") -Value $physicalProtocolManifest
    $physicalProtocolReport = Invoke-Phase1Report -Directory $physicalProtocolDirectory
    if (-not [bool]$physicalProtocolReport.physicalCardinalProtocol.retained -or
        -not [bool]$physicalProtocolReport.cardinal.physicalManualProtocolRetained -or
        [bool]$physicalProtocolReport.physicalHeadsetGate.accepted) {
        throw "A retained physical protocol was not distinguished from physical acceptance."
    }

    # Exercise the simulator driver without a runtime by supplying the same
    # bounded per-run file-consumer contract used by the headless runtime.
    # This proves the retained cardinal records describe commands that were
    # actually published and consumed, not just static source text.
    $sweepDirectory = Join-Path $testRoot "head-sweep"
    [void][System.IO.Directory]::CreateDirectory($sweepDirectory)
    $sweepCommandPath = Join-Path $sweepDirectory "head_pose_command.json"
    $consumer = Start-Job -ArgumentList $sweepCommandPath -ScriptBlock {
        param([string]$CommandPath)
        $consumed = 0
        $deadline = [DateTime]::UtcNow.AddSeconds(15)
        while ($consumed -lt 13 -and [DateTime]::UtcNow -lt $deadline) {
            if (Test-Path -LiteralPath $CommandPath -PathType Leaf) {
                Remove-Item -LiteralPath $CommandPath -Force
                ++$consumed
            }
            Start-Sleep -Milliseconds 2
        }
        return $consumed
    }
    try {
        $sweepOutput = @(& $sweepPath `
            -DataDirectory $sweepDirectory `
            -Pattern Cardinal `
            -DurationSeconds 2 `
            -ConsumeTimeoutMilliseconds 4000) -join [Environment]::NewLine
        $sweepEvidence = $sweepOutput | ConvertFrom-Json -ErrorAction Stop
        $null = Wait-Job -Job $consumer -Timeout 10
        $consumed = @(Receive-Job -Job $consumer)
        if ($consumed.Count -ne 1 -or [int]$consumed[0] -ne 13 -or
            $sweepEvidence.pattern -cne "cardinal-v1" -or
            [int]$sweepEvidence.commandCount -ne 13 -or
            -not [bool]$sweepEvidence.centerRestored) {
            throw "The headless simulator did not publish and consume the complete cardinal script."
        }
        foreach ($axis in @(
            "translationX", "translationY", "translationZ", "yaw", "pitch", "roll")) {
            foreach ($direction in @(-1, 1)) {
                if (@($sweepEvidence.commands | Where-Object {
                        [string]$_.axis -ceq $axis -and
                        [int]$_.direction -eq $direction
                    }).Count -ne 1) {
                    throw "The headless simulator cardinal script omitted $axis/$direction."
                }
            }
        }
        $baseline = [ordered]@{
            x = 0.0; y = 1.7; z = 0.0
            yaw = 0.0; pitch = 0.0; roll = 0.0
        }
        $axisContract = [ordered]@{
            translationX = [ordered]@{ field = "x"; magnitude = 0.10 }
            translationY = [ordered]@{ field = "y"; magnitude = 0.10 }
            translationZ = [ordered]@{ field = "z"; magnitude = 0.10 }
            yaw = [ordered]@{ field = "yaw"; magnitude = [Math]::PI / 12.0 }
            pitch = [ordered]@{ field = "pitch"; magnitude = [Math]::PI / 12.0 }
            roll = [ordered]@{ field = "roll"; magnitude = [Math]::PI / 12.0 }
        }
        foreach ($axis in $axisContract.Keys) {
            foreach ($direction in @(-1, 1)) {
                $command = @($sweepEvidence.commands | Where-Object {
                    [string]$_.axis -ceq $axis -and
                    [int]$_.direction -eq $direction
                })[0]
                foreach ($field in $baseline.Keys) {
                    $expected = [double]$baseline[$field]
                    if ($field -ceq [string]$axisContract[$axis].field) {
                        $expected += [double]$direction *
                            [double]$axisContract[$axis].magnitude
                    }
                    if ([Math]::Abs([double]$command.$field - $expected) -gt 0.000001) {
                        throw "The headless simulator cardinal script coupled or mis-scaled $axis/$direction."
                    }
                }
            }
        }
    } finally {
        if ($consumer) {
            Stop-Job -Job $consumer -ErrorAction SilentlyContinue
            Remove-Job -Job $consumer -Force -ErrorAction SilentlyContinue
        }
    }

    $simulator = New-Phase1Run -Name "simulator"
    Write-Phase1Json -Path (Join-Path $simulator "manifest.json") -Value (
        New-Phase1Manifest -Physical $false -Simulator -Readiness @{
            hostBridge = $true
            hostPose = $true
            retailRuntimeAndPose = $true
            retailVrBridge = $true
        })
    $publication = [ordered]@{
        event = "fnvxrRetailEngineCenterCpuStereo"
        transaction = 42
        sourceFrame = 42
        poseFrame = 777
        poseSequence = 35
        runtimeStateSample = 99
        renderedDisplayTime = 123456
        producerMode = 4
        renderPairSequence = 42
        publicationGeneration = "9"
        referenceSpaceGeneration = 5
        producerEpoch = "17"
        rendererProducerEpoch = "23"
        producerProcessId = 321
        separated = $true
        worldCandidate = $true
        pixelProof = $true
    }
    $centerFrame = [ordered]@{
        event = "fnvxrRetailEngineCenterFrame"
        transaction = 42
        delivered = $true
        cameraPoseValid = $true
        producerMode = 4
        centerTranslation = @(1.0, 2.0, 3.0)
        originPoseFrame = 700
        originPoseSequence = 35
        originPosition = @(0.0, 1.7, 0.0)
        originReferenceSpaceGeneration = 5
        originProducerEpoch = "17"
        leftTranslation = @(0.96, 2.0, 3.0)
        rightTranslation = @(1.04, 2.0, 3.0)
        rendererCullerCameraMatch = $true
        stockCameraTransformUsable = $true
        eyeBaselineValid = $true
        eyeMidpointDistanceMeters = 0.0
    }
    Write-Phase1JsonLines -Path (Join-Path $simulator "fnvxr_retail_vr.log") -Values @(
        $publication, $centerFrame)
    $submit = [ordered]@{
        event = "fnvxrPhase1OpenXrEngineCenterSubmit"
        cpuEngineStereoActive = $true
        cpuEngineProducerMode = 4
        cpuEngineTransaction = 42
        sourceStereoSequence = "9"
        sourceRenderPairSequence = 42
        sourcePoseSequence = 35
        sourceReferenceSpaceGeneration = 5
        sourcePoseProducerEpoch = "17"
        sourceRendererProducerEpoch = "23"
        sourceProducerProcessId = 321
        sourceRenderedDisplayTime = 123456
        projectionLayerSubmitted = $true
        xrEndFrame = "XR_SUCCESS"
    }
    Write-Phase1JsonLines -Path (Join-Path $simulator "host.stdout.log") -Values @($submit)
    [System.IO.File]::WriteAllText(
        (Join-Path $simulator "host.stderr.log"), "", $encoding)
    $simulatorReport = Invoke-Phase1Report -Directory $simulator
    if ($simulatorReport.source -cne "headless-simulator" -or
        -not $simulatorReport.simulatorOnly -or
        $simulatorReport.status -cne "simulator-evidence-retained" -or
        -not $simulatorReport.cardinal.simulatorEvidenceComplete -or
        -not $simulatorReport.trace.renderedLineageComplete -or
        -not $simulatorReport.trace.phase1TraceComplete -or
        $simulatorReport.trace.headBodyEvidenceSource -cne "engine-center-frame" -or
        [bool]$simulatorReport.physicalHeadsetGate.accepted) {
        throw "A complete simulator fixture was not retained as simulator-only Phase 1 evidence."
    }
    if (-not (Test-Path -LiteralPath (Join-Path $simulator "phase1-6dof-evidence.json") -PathType Leaf)) {
        throw "The simulator report was not retained next to its run manifest."
    }

    $runtimeUnavailable = New-Phase1Run -Name "runtime-unavailable"
    Write-Phase1Json -Path (Join-Path $runtimeUnavailable "manifest.json") -Value (
        New-Phase1Manifest -Physical $true -Readiness @{
            hostBridge = $false
            hostPose = $false
            retailRuntimeAndPose = $false
            retailVrBridge = $false
        } -Error "OpenXR host exited before publishing bridge")
    [System.IO.File]::WriteAllText(
        (Join-Path $runtimeUnavailable "host.stderr.log"),
        "xrGetSystem did not find an HMD", $encoding)
    $runtimeUnavailableReport = Invoke-Phase1Report -Directory $runtimeUnavailable
    if ($runtimeUnavailableReport.classification.code -cne "openxr-runtime-unavailable") {
        throw "A missing OpenXR bridge was not classified as runtime unavailable."
    }

    $gameExit = New-Phase1Run -Name "game-exit"
    Write-Phase1Json -Path (Join-Path $gameExit "manifest.json") -Value (
        New-Phase1Manifest -Physical $true -Readiness @{
            hostBridge = $true
            hostPose = $false
            retailRuntimeAndPose = $false
            retailVrBridge = $false
        } -Error "FalloutNV:1234 exited with code 1.")
    $gameExitReport = Invoke-Phase1Report -Directory $gameExit
    if ($gameExitReport.classification.code -cne "retail-game-exited-before-controller-ack") {
        throw "A retail exit before acknowledgement was not separately classified."
    }

    $bridgeNotReady = New-Phase1Run -Name "bridge-not-ready"
    Write-Phase1Json -Path (Join-Path $bridgeNotReady "manifest.json") -Value (
        New-Phase1Manifest -Physical $true -Readiness @{
            hostBridge = $true
            hostPose = $true
            retailRuntimeAndPose = $true
            retailVrBridge = $false
        })
    $bridgeNotReadyReport = Invoke-Phase1Report -Directory $bridgeNotReady
    if ($bridgeNotReadyReport.classification.code -cne "retail-bridge-not-ready") {
        throw "A missing retail bridge was not separately classified."
    }
} finally {
    if (Test-Path -LiteralPath $testRoot -PathType Container) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
}

Write-Host "FNVXR Phase 1 6DoF verifier tests passed."
