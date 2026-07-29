param(
    [Parameter(Mandatory = $true)][string]$SourceRoot
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$buildPath = Join-Path $SourceRoot "scripts\build-openxr-simulator-headless.ps1"
$inputPath = Join-Path $SourceRoot "scripts\invoke-openxr-simulator-input.ps1"
$patchPath = Join-Path $SourceRoot "patches\openxr-simulator-fnvxr-headless.patch"
foreach ($path in @($buildPath, $inputPath, $patchPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Headless simulator CLI contract file is missing: $path"
    }
}

$build = Get-Content -LiteralPath $buildPath -Raw
$inputDriver = Get-Content -LiteralPath $inputPath -Raw
$patch = Get-Content -LiteralPath $patchPath -Raw

# Parse both scripts without executing a runtime, game, process, or input.
[void][ScriptBlock]::Create($build)
[void][ScriptBlock]::Create($inputDriver)

foreach ($contract in @(
    '48a70f440ac7d9bda385994937e3da8e15a4d9bb',
    'openxr-simulator-fnvxr-headless.patch',
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
    'controller_pose_command.json',
    'command_ack.json',
    '[System.IO.File]::Move($temporaryPath, $commandPath)',
    '[int64]$candidate.sequence -eq $sequence',
    '[switch]$ReleaseAll',
    'per-run file IPC only; no window, focus, keyboard, mouse, registry, or simulator GUI control')) {
    if (-not $inputDriver.Contains($contract)) {
        throw "Headless simulator input driver lost contract: $contract"
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
        $inputDriver.Contains($forbidden)) {
        throw "Headless simulator CLI scripts must not contain: $forbidden"
    }
}

Write-Host "FNVXR headless simulator CLI contracts passed."
