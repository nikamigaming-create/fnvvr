[CmdletBinding()]
param(
    [string]$SourceRoot = (Split-Path -Parent $PSScriptRoot),
    [ValidateRange(2, 60)][int]$SweepDurationSeconds = 14,
    [ValidateRange(1200, 7200)][int]$HostFrameCount = 1800,
    [ValidateRange(5, 30)][int]$HostReadyTimeoutSeconds = 12
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$SourceRoot = (Resolve-Path -LiteralPath $SourceRoot).Path
$runtimeManifest = Join-Path $SourceRoot "local\OpenXR-Simulator\bin\openxr_simulator.json"
$hostPath = Join-Path $SourceRoot "build-product-x64\Release\fnvxr_openxr_pose_host.exe"
$probePath = Join-Path $SourceRoot "build-product-x64\Release\fnvxr_shared_state_probe.exe"
$controllerSweepPath =
    Join-Path $SourceRoot "scripts\invoke-openxr-simulator-controller-sweep.ps1"
foreach ($requiredPath in @(
        $runtimeManifest, $hostPath, $probePath, $controllerSweepPath)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "The headless controller proof requires: $requiredPath"
    }
}

$runId = "simulator-controller-6dof-" + (Get-Date -Format "yyyyMMdd-HHmmss-fff")
$runDirectory = Join-Path $SourceRoot "local\product-runs\$runId"
$dataDirectory = Join-Path $runDirectory "openxr-simulator"
$hostOut = Join-Path $runDirectory "host.out.log"
$hostErr = Join-Path $runDirectory "host.err.log"
$simulatorLog = Join-Path $runDirectory "openxr-simulator.log"
$sweepOut = Join-Path $runDirectory "controller-sweep.json"
$sweepErr = Join-Path $runDirectory "controller-sweep.err.log"
New-Item -ItemType Directory -Path $dataDirectory -Force | Out-Null

function Get-FnvxrControllerProofSpan {
    param(
        [Parameter(Mandatory = $true)][object[]]$Samples,
        [Parameter(Mandatory = $true)][ValidateSet("hmd", "rightAim")][string]$Field,
        [Parameter(Mandatory = $true)][ValidateRange(0, 2)][int]$Axis
    )

    $values = New-Object 'System.Collections.Generic.List[double]'
    foreach ($sample in $Samples) {
        [void]$values.Add([double]($sample.$Field[$Axis]))
    }
    $maximum = [double](($values | Measure-Object -Maximum).Maximum)
    $minimum = [double](($values | Measure-Object -Minimum).Minimum)
    return $maximum - $minimum
}

function Get-FnvxrControllerProofQuaternionAngle {
    param(
        [Parameter(Mandatory = $true)][double[]]$Left,
        [Parameter(Mandatory = $true)][double[]]$Right
    )

    $dot = 0.0
    for ($index = 0; $index -lt 4; ++$index) {
        $dot += $Left[$index] * $Right[$index]
    }
    $dot = [Math]::Min(1.0, [Math]::Max(-1.0, [Math]::Abs($dot)))
    return 2.0 * [Math]::Acos($dot)
}

function Get-FnvxrControllerProofPoseSample {
    param([Parameter(Mandatory = $true)][string]$ProbePath)

    try {
        $raw = @(& $ProbePath) -join [Environment]::NewLine
        $snapshot = $raw | ConvertFrom-Json -ErrorAction Stop
        if (-not $snapshot.pose.present -or
            -not $snapshot.pose.hmdTracked -or
            -not $snapshot.pose.rightAimCurrent) {
            return $null
        }
        return [pscustomobject]@{
            utc = [DateTime]::UtcNow.ToString("o")
            frame = [double]$snapshot.pose.frame
            hmd = @(
                [double]$snapshot.pose.hmdPos[0],
                [double]$snapshot.pose.hmdPos[1],
                [double]$snapshot.pose.hmdPos[2])
            hmdRotation = @(
                [double]$snapshot.pose.hmdRot[0],
                [double]$snapshot.pose.hmdRot[1],
                [double]$snapshot.pose.hmdRot[2],
                [double]$snapshot.pose.hmdRot[3])
            rightAim = @(
                [double]$snapshot.pose.rightAimPos[0],
                [double]$snapshot.pose.rightAimPos[1],
                [double]$snapshot.pose.rightAimPos[2])
            rightAimRotation = @(
                [double]$snapshot.pose.rightAimRot[0],
                [double]$snapshot.pose.rightAimRot[1],
                [double]$snapshot.pose.rightAimRot[2],
                [double]$snapshot.pose.rightAimRot[3])
        }
    } catch {
        # The host publishes the mapping only after its OpenXR session is
        # ready. A transient read before that point is not pose evidence.
        return $null
    }
}

$processEnvironment = [ordered]@{
    XR_RUNTIME_JSON = $runtimeManifest
    OPENXR_SIMULATOR_HEADLESS = "1"
    OPENXR_SIMULATOR_DATA_DIR = $dataDirectory
    OPENXR_SIMULATOR_LOG_PATH = $simulatorLog
    FNVXR_RUN_PROFILE = "stereo-visual-trial-v5"
    FNVXR_RUN_ID = $runId
    FNVXR_RUN_LOG_DIR = $runDirectory
    FNVXR_ENABLE_ENGINE_CENTER_STEREO = "0"
    FNVXR_DISABLE_STEREO_WORLD = "1"
}
$previousEnvironment = @{}
$hostProcess = $null

try {
    foreach ($key in $processEnvironment.Keys) {
        $previousEnvironment[$key] = [Environment]::GetEnvironmentVariable(
            $key,
            [EnvironmentVariableTarget]::Process)
        [Environment]::SetEnvironmentVariable(
            $key,
            [string]$processEnvironment[$key],
            [EnvironmentVariableTarget]::Process)
    }
    try {
        $hostProcess = Start-Process `
            -FilePath $hostPath `
            -ArgumentList ([string]$HostFrameCount) `
            -WorkingDirectory (Split-Path -Parent $hostPath) `
            -RedirectStandardOutput $hostOut `
            -RedirectStandardError $hostErr `
            -WindowStyle Hidden `
            -PassThru
    } finally {
        foreach ($key in $processEnvironment.Keys) {
            [Environment]::SetEnvironmentVariable(
                $key,
                $previousEnvironment[$key],
                [EnvironmentVariableTarget]::Process)
        }
    }

    $ready = $false
    $readyDeadline = [DateTime]::UtcNow.AddSeconds($HostReadyTimeoutSeconds)
    do {
        if (Test-Path -LiteralPath $hostOut -PathType Leaf) {
            $hostText = Get-Content -LiteralPath $hostOut -Raw
            if ($null -ne $hostText -and $hostText.Contains(
                    "fnvxrHostBridgeReady xrSessionCreated=1 sharedMappingsReady=1")) {
                $ready = $true
                break
            }
        }
        $hostProcess.Refresh()
        if ($hostProcess.HasExited) {
            break
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $readyDeadline)
    if (-not $ready) {
        throw "The headless OpenXR host did not publish its bridge handshake. See $hostOut and $hostErr"
    }

    $powershellPath = (Get-Command powershell.exe -ErrorAction Stop).Source
    $sweepProcess = Start-Process `
        -FilePath $powershellPath `
        -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-File", $controllerSweepPath,
            "-DataDirectory", $dataDirectory,
            "-Hand", "right",
            "-DurationSeconds", ([string]$SweepDurationSeconds),
            "-ConsumeTimeoutMilliseconds", "2000") `
        -WorkingDirectory $SourceRoot `
        -RedirectStandardOutput $sweepOut `
        -RedirectStandardError $sweepErr `
        -WindowStyle Hidden `
        -PassThru

    $samples = New-Object 'System.Collections.Generic.List[object]'
    $sweepDeadline = [DateTime]::UtcNow.AddSeconds($SweepDurationSeconds + 10)
    do {
        $sample = Get-FnvxrControllerProofPoseSample -ProbePath $probePath
        if ($null -ne $sample) {
            [void]$samples.Add($sample)
        }
        $sweepProcess.Refresh()
        if ($sweepProcess.HasExited) {
            break
        }
        Start-Sleep -Milliseconds 180
    } while ([DateTime]::UtcNow -lt $sweepDeadline)
    $sweepProcess.Refresh()
    if (-not $sweepProcess.HasExited) {
        throw "The bounded LOCAL controller sweep exceeded its deadline."
    }
    if (-not (Test-Path -LiteralPath $sweepOut -PathType Leaf)) {
        throw "The bounded LOCAL controller sweep produced no evidence. See $sweepErr"
    }
    $sweep = Get-Content -LiteralPath $sweepOut -Raw |
        ConvertFrom-Json -ErrorAction Stop
    if ([string]$sweep.schema -cne "fnvxr-headless-simulator-controller-sweep-v1" -or
        [int]$sweep.commandCount -ne 14 -or
        [string]$sweep.poseSpace -cne "local" -or
        @($sweep.commands).Count -ne 14) {
        throw "The LOCAL controller worker did not emit a complete acknowledged sweep. See $sweepOut and $sweepErr"
    }
    $localSamples = @($samples | Where-Object { $_.rightAim[1] -gt 1.0 })
    if ($localSamples.Count -lt 8) {
        throw "The host did not publish enough LOCAL right-hand samples; found $($localSamples.Count)."
    }

    $baseline = $localSamples |
        Sort-Object {
            [Math]::Abs($_.rightAim[0] - 0.20) +
            [Math]::Abs($_.rightAim[1] - 1.40) +
            [Math]::Abs($_.rightAim[2] + 0.40)
        } |
        Select-Object -First 1
    $maxHandAngle = 0.0
    $maxHeadAngle = 0.0
    foreach ($sample in $localSamples) {
        $handAngle = Get-FnvxrControllerProofQuaternionAngle `
            -Left ([double[]]$baseline.rightAimRotation) `
            -Right ([double[]]$sample.rightAimRotation)
        $headAngle = Get-FnvxrControllerProofQuaternionAngle `
            -Left ([double[]]$baseline.hmdRotation) `
            -Right ([double[]]$sample.hmdRotation)
        $maxHandAngle = [Math]::Max($maxHandAngle, $handAngle)
        $maxHeadAngle = [Math]::Max($maxHeadAngle, $headAngle)
    }

    $hmdSpan = [ordered]@{
        x = Get-FnvxrControllerProofSpan -Samples $localSamples -Field hmd -Axis 0
        y = Get-FnvxrControllerProofSpan -Samples $localSamples -Field hmd -Axis 1
        z = Get-FnvxrControllerProofSpan -Samples $localSamples -Field hmd -Axis 2
    }
    $rightAimSpan = [ordered]@{
        x = Get-FnvxrControllerProofSpan -Samples $localSamples -Field rightAim -Axis 0
        y = Get-FnvxrControllerProofSpan -Samples $localSamples -Field rightAim -Axis 1
        z = Get-FnvxrControllerProofSpan -Samples $localSamples -Field rightAim -Axis 2
    }
    $hmdStatic =
        $hmdSpan.x -lt 0.0005 -and
        $hmdSpan.y -lt 0.0005 -and
        $hmdSpan.z -lt 0.0005 -and
        $maxHeadAngle -lt 0.001
    $translationsIndependent =
        $rightAimSpan.x -gt 0.15 -and
        $rightAimSpan.y -gt 0.15 -and
        $rightAimSpan.z -gt 0.15
    $rotationsIndependent = $maxHandAngle -gt 0.20

    $proof = [pscustomobject][ordered]@{
        schema = "fnvxr-runtime-controller-local-6dof-proof-v1"
        source = "headless-simulator"
        runDirectory = $runDirectory
        simulatorLog = $simulatorLog
        hostLog = $hostOut
        controllerSweep = $sweepOut
        controllerWorkerExitCode = [int]$sweepProcess.ExitCode
        commandCount = [int]$sweep.commandCount
        acknowledgedLocal = ([string]$sweep.poseSpace -ceq "local")
        headPoseMutated = [bool]$sweep.headPoseMutated
        centerRestored = [bool]$sweep.centerRestored
        sampledFrames = $localSamples.Count
        hmdSpanMeters = $hmdSpan
        hmdMaxAngularDeltaRadians = $maxHeadAngle
        rightAimSpanMeters = $rightAimSpan
        rightAimMaxAngularDeltaRadians = $maxHandAngle
        hmdStatic = $hmdStatic
        translationsIndependent = $translationsIndependent
        rotationsIndependent = $rotationsIndependent
        passed = (
            $hmdStatic -and
            $translationsIndependent -and
            $rotationsIndependent -and
            ([string]$sweep.poseSpace -ceq "local") -and
            -not [bool]$sweep.headPoseMutated -and
            [bool]$sweep.centerRestored)
    }
    $proof | ConvertTo-Json -Depth 6
    if (-not [bool]$proof.passed) {
        exit 1
    }
} finally {
    if ($null -ne $hostProcess) {
        $hostProcess.Refresh()
        if (-not $hostProcess.HasExited) {
            # This is the finite, process-owned headless helper started above;
            # stop it once the proof is complete rather than leaving it active.
            Stop-Process -Id $hostProcess.Id -Force
        }
    }
}
