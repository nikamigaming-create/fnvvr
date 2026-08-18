param(
    [Parameter(Mandatory = $true)][string]$SourceRoot
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$buildPath = Join-Path $SourceRoot "scripts\build-openxr-simulator-headless.ps1"
$inputPath = Join-Path $SourceRoot "scripts\invoke-openxr-simulator-input.ps1"
$headSweepPath =
    Join-Path $SourceRoot "scripts\invoke-openxr-simulator-head-sweep.ps1"
$controllerSweepPath =
    Join-Path $SourceRoot "scripts\invoke-openxr-simulator-controller-sweep.ps1"
$combatDemoPath =
    Join-Path $SourceRoot "scripts\invoke-openxr-simulator-combat-demo.ps1"
$patchPath = Join-Path $SourceRoot "patches\openxr-simulator-fnvxr-headless.patch"
$controllerPosePatchPath =
    Join-Path $SourceRoot "patches\openxr-simulator-controller-local-6dof.patch"
foreach ($path in @(
    $buildPath, $inputPath, $headSweepPath, $controllerSweepPath,
    $combatDemoPath, $patchPath,
    $controllerPosePatchPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Headless simulator CLI contract file is missing: $path"
    }
}

$build = Get-Content -LiteralPath $buildPath -Raw
$inputDriver = Get-Content -LiteralPath $inputPath -Raw
$headSweepDriver = Get-Content -LiteralPath $headSweepPath -Raw
$controllerSweepDriver = Get-Content -LiteralPath $controllerSweepPath -Raw
$combatDemoDriver = Get-Content -LiteralPath $combatDemoPath -Raw
$patch = Get-Content -LiteralPath $patchPath -Raw
$controllerPosePatch = Get-Content -LiteralPath $controllerPosePatchPath -Raw

# Parse both scripts without executing a runtime, game, process, or input.
[void][ScriptBlock]::Create($build)
[void][ScriptBlock]::Create($inputDriver)
[void][ScriptBlock]::Create($headSweepDriver)
[void][ScriptBlock]::Create($controllerSweepDriver)
[void][ScriptBlock]::Create($combatDemoDriver)

foreach ($contract in @(
    '48a70f440ac7d9bda385994937e3da8e15a4d9bb',
    'openxr-simulator-fnvxr-headless.patch',
    'openxr-simulator-controller-local-6dof.patch',
    'LOCAL controller-pose patch',
    'apply --check --reverse',
    'MSBUILDDISABLENODEREUSE',
    'UseSharedCompilation',
    'CreateNoWindow = $true',
    '"Visual Studio 17 2022"',
    '"x64"')) {
    if (-not $build.Contains($contract)) {
        throw "Headless simulator build script lost contract: $contract"
    }
}

foreach ($contract in @(
    'pattern = "forward, noisy-hard-left, backward, right, neutral"',
    'expected = "left-only"',
    'ThumbstickY = $leftY',
    'heldStickRepeatForbidden = $true',
    'expectedTurns = 2',
    '[string]$candidate.command -ceq "head_pose"',
    'retail player world-position delta required')) {
    if (-not $combatDemoDriver.Contains($contract)) {
        throw "Headless simulator combat/locomotion driver lost contract: $contract"
    }
}

foreach ($contract in @(
    'head_pose_command.json',
    '[System.IO.File]::Move($temporaryPath, $commandPath)',
    'Wait-HeadPoseCommandConsumed',
    'XAmplitudeMeters',
    'YAmplitudeMeters',
    'ZAmplitudeMeters',
    'YawAmplitudeDegrees',
    'PitchAmplitudeDegrees',
    'RollAmplitudeDegrees',
    'centerRestored = $true',
    'per-run HMD pose file IPC only')) {
    if (-not $headSweepDriver.Contains($contract)) {
        throw "Headless simulator HMD sweep driver lost contract: $contract"
    }
}

foreach ($contract in @(
    'controller_pose_command.json',
    'command_ack.json',
    '[System.IO.File]::Move($temporaryPath, $commandPath)',
    '[int64]$candidate.sequence -eq $sequence',
    '[switch]$ReleaseAll',
    '[string]$PoseSpace = "unchanged"',
    '[double]$Roll = [double]::NaN',
    'per-run file IPC only; no window, focus, keyboard, mouse, registry, or simulator GUI control')) {
    if (-not $inputDriver.Contains($contract)) {
        throw "Headless simulator input driver lost contract: $contract"
    }
}

foreach ($contract in @(
    'invoke-openxr-simulator-input.ps1',
    'PoseSpace = "local"',
    'translationX',
    'translationY',
    'translationZ',
    'yaw',
    'pitch',
    'roll',
    '[switch]$LivePipBoyFocus',
    'axis = "pipBoyFocus"',
    'livePipBoyFocusCommandCount',
    'headPoseMutated = $false',
    'explicit OpenXR LOCAL controller poses')) {
    if (-not $controllerSweepDriver.Contains($contract)) {
        throw "Headless simulator controller sweep driver lost contract: $contract"
    }
}

foreach ($contract in @(
    'OPENXR_SIMULATOR_HEADLESS',
    'OPENXR_SIMULATOR_LOG_PATH',
    'OPENXR_SIMULATOR_DATA_DIR',
    'XR_SPACE_LOCATION_POSITION_TRACKED_BIT',
    'XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT',
    'GetCurrentHeadAngles',
    'button_a',
    'button_x',
    'thumbstickClick',
    'sequence')) {
    if (-not $patch.Contains($contract)) {
        throw "Reviewed OpenXR-Simulator patch lost contract: $contract"
    }
}

foreach ($contract in @(
    'poseInLocalSpace',
    'QuatFromYawPitchRoll(float yaw, float pitch, float roll)',
    'localSpaceSet',
    'cmd.rollSet',
    '"space"',
    'cmd.localSpace')) {
    if (-not $controllerPosePatch.Contains($contract)) {
        throw "Reviewed LOCAL controller-pose patch lost contract: $contract"
    }
}

foreach ($forbidden in @(
    'register-runtime.ps1',
    'unregister-runtime.ps1',
    'ActiveRuntime',
    'HKLM:',
    'SetForegroundWindow',
    'ShowWindow',
    'SendKeys',
    'SendInput',
    'keybd_event',
    'mouse_event',
    'Start-Process')) {
    if ($build.Contains($forbidden) -or
        $inputDriver.Contains($forbidden) -or
        $headSweepDriver.Contains($forbidden) -or
        $controllerSweepDriver.Contains($forbidden) -or
        $combatDemoDriver.Contains($forbidden)) {
        throw "Headless simulator CLI scripts must not contain: $forbidden"
    }
}

Write-Host "FNVXR headless simulator CLI contracts passed."
