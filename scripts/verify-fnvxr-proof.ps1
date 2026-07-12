param(
    [string]$GameRoot = $(if ($env:FNVXR_GAME_ROOT) { $env:FNVXR_GAME_ROOT } else { "C:\Program Files (x86)\Steam\steamapps\common\Fallout New Vegas" }),
    [string]$HostLogPath = "",
    [string]$RunManifestPath = "",
    [string]$Configuration = "Release",
    [switch]$RequireFreshLogs,
    [datetime]$LogsNotBefore = [datetime]::MinValue,
    [switch]$RequireSharedState,
    [int]$SharedStateSampleDelayMs = 500,
    [string]$SharedStateJsonPath = ""
)

$ErrorActionPreference = "Stop"

function Read-TextOrEmpty {
    param([string]$Path)
    if ($Path -and (Test-Path -LiteralPath $Path)) {
        return Get-Content -Raw -LiteralPath $Path
    }
    return ""
}

function Add-Check {
    param(
        [string]$Name,
        [bool]$Pass,
        [string]$Detail = ""
    )
    $script:Checks += [pscustomobject]@{
        Name = $Name
        Pass = $Pass
        Detail = $Detail
    }
}

function Test-Regex {
    param(
        [string]$Text,
        [string]$Pattern
    )
    return $Text -match $Pattern
}

function Test-FileFresh {
    param(
        [string]$Path,
        [datetime]$NotBefore
    )
    if (-not (Test-Path -LiteralPath $Path)) {
        return $false
    }
    return (Get-Item -LiteralPath $Path).LastWriteTime -ge $NotBefore
}

function Invoke-SharedStateProbe {
    param(
        [string]$Configuration,
        [int]$SampleDelayMs
    )

    $root = Split-Path -Parent $PSScriptRoot
    $buildDir = Join-Path $root "build"
    cmake -S $root -B $buildDir | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed with exit code $LASTEXITCODE"
    }

    cmake --build $buildDir --config $Configuration --target fnvxr_shared_state_probe | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "Shared-state probe build failed with exit code $LASTEXITCODE"
    }

    $probeExe = Join-Path $buildDir "$Configuration\fnvxr_shared_state_probe.exe"
    if (-not (Test-Path -LiteralPath $probeExe)) {
        throw "Missing shared-state probe: $probeExe"
    }

    $probeArgs = @(
        "--require-player",
        "--require-runtime",
        "--require-camera",
        "--require-video",
        "--require-stereo",
        "--require-world-stereo",
        "--require-pose",
        "--require-advancing",
        "--sample-delay-ms",
        ([string]$SampleDelayMs)
    )
    $output = & $probeExe @probeArgs 2>&1
    $exitCode = $LASTEXITCODE
    $text = ($output | Out-String)
    if ($SharedStateJsonPath) {
        $parent = Split-Path -Parent $SharedStateJsonPath
        if ($parent) {
            New-Item -ItemType Directory -Force -Path $parent | Out-Null
        }
        $text | Set-Content -LiteralPath $SharedStateJsonPath -Encoding UTF8
    }

    $json = $null
    try {
        $json = $text | ConvertFrom-Json
    } catch {
        Add-Check "shared-state probe output is JSON" $false $_.Exception.Message
    }

    return [pscustomobject]@{
        ExitCode = $exitCode
        Text = $text
        Json = $json
    }
}

$runManifest = $null
$manifestFailed = $false
$retailLaunched = $true
if ($RunManifestPath) {
    if (-not (Test-Path -LiteralPath $RunManifestPath)) {
        throw "Missing run manifest: $RunManifestPath"
    }
    $runManifest = Get-Content -Raw -LiteralPath $RunManifestPath | ConvertFrom-Json
    $manifestFailed = [bool]$runManifest.failed
    if ($null -ne $runManifest.retailLaunched) {
        $retailLaunched = [bool]$runManifest.retailLaunched
    }
    $runDir = if ($runManifest.runDir) { [string]$runManifest.runDir } else { "" }
    if (-not $HostLogPath -and $runManifest.hostOut) {
        $HostLogPath = $runManifest.hostOut
    }
    if ($runManifest.startedAt) {
        $LogsNotBefore = [datetime]::Parse($runManifest.startedAt)
        $RequireFreshLogs = $true
    }
} else {
    $runDir = ""
}

if ($runDir) {
    $pluginLog = Join-Path $runDir "fnvxr_input_telemetry.log"
    $d3d9Log = Join-Path $runDir "fnvxr_d3d9_proxy.log"
    $dinputLog = Join-Path $runDir "fnvxr_dinput_proxy.log"
    $xinputLog = Join-Path $runDir "fnvxr_xinput_proxy.log"
} else {
    $pluginLog = Join-Path $GameRoot "Data\NVSE\Plugins\fnvxr_input_telemetry.log"
    $d3d9Log = Join-Path $GameRoot "fnvxr_d3d9_proxy.log"
    $dinputLog = Join-Path $GameRoot "Data\NVSE\Plugins\fnvxr_dinput_proxy.log"
    $xinputLog = Join-Path $GameRoot "Data\NVSE\Plugins\fnvxr_xinput_proxy.log"
}
$pluginText = Read-TextOrEmpty $pluginLog
$d3d9Text = Read-TextOrEmpty $d3d9Log
$dinputText = Read-TextOrEmpty $dinputLog
$xinputText = Read-TextOrEmpty $xinputLog
$hostText = Read-TextOrEmpty $HostLogPath

$Checks = @()
$skipRuntimeProof = $manifestFailed
$retailLogsExpected = -not ($manifestFailed -and -not $retailLaunched)

if ($manifestFailed) {
    Add-Check "launch completed" $false $runManifest.error
}
if ($retailLogsExpected) {
    Add-Check "plugin log exists" ($pluginText.Length -gt 0) $pluginLog
    Add-Check "d3d9 log exists" ($d3d9Text.Length -gt 0) $d3d9Log
    Add-Check "dinput log exists" ($dinputText.Length -gt 0) $dinputLog
    Add-Check "xinput log exists" ($xinputText.Length -gt 0) $xinputLog
}
if ($HostLogPath) {
    Add-Check "host log exists" ($hostText.Length -gt 0) $HostLogPath
}
if ($RequireFreshLogs) {
    if ($retailLogsExpected) {
        Add-Check "plugin log is fresh" `
            (Test-FileFresh -Path $pluginLog -NotBefore $LogsNotBefore) `
            "requires LastWriteTime >= $($LogsNotBefore.ToString("o"))"
        Add-Check "d3d9 log is fresh" `
            (Test-FileFresh -Path $d3d9Log -NotBefore $LogsNotBefore) `
            "requires LastWriteTime >= $($LogsNotBefore.ToString("o"))"
    }
    if ($HostLogPath) {
        Add-Check "host log is fresh" `
            (Test-FileFresh -Path $HostLogPath -NotBefore $LogsNotBefore) `
            "requires LastWriteTime >= $($LogsNotBefore.ToString("o"))"
    }
}

if (-not $skipRuntimeProof) {
Add-Check "world camera becomes active" `
    (Test-Regex $pluginText '"event":"fnvxrWorldCameraState"[^\r\n]*"active":true') `
    "requires live world camera, not menu/loading/studio only"
Add-Check "menu showroom does not mutate live game" `
    (!(Test-Regex $pluginText '"event":"fnvxrShowroomCommand"') -and
        !(Test-Regex $pluginText '"event":"fnvxrShowroomScene"[^\r\n]*"phase":"request"')) `
    "the menu proof must not use coc/player movement against the live retail menu state"
Add-Check "camera apply proof emitted" `
    (Test-Regex $pluginText '"event":"fnvxrCameraApply"') `
    "requires camera hook candidate/applied telemetry"
Add-Check "no applied camera before explicit enable" `
    (!(Test-Regex $pluginText '"event":"fnvxrCameraApply"[^\r\n]*"applied":true') -or
        (Test-Regex $pluginText '"event":"fnvxrCameraApply"[^\r\n]*"applied":true[^\r\n]*"yawOnly":true')) `
    "first playable pass should be yaw-only before full rotation/translation"
Add-Check "d3d9 shared camera used" `
    (Test-Regex $d3d9Text '"event":"fnvxrD3d9EyePoseScale"[^\r\n]*"sharedCamera":true') `
    "requires D3D9 proxy to consume xNVSE camera state"
Add-Check "d3d9 stereo target proof emitted" `
    (Test-Regex $d3d9Text '"event":"fnvxrD3d9EyeTarget"') `
    "requires stereo replay/shared publish proof"
Add-Check "d3d9 world stereo candidate" `
    (Test-Regex $d3d9Text '"event":"fnvxrD3d9EyeTarget"[^\r\n]*"worldCandidate":true[^\r\n]*"separated":true') `
    "requires non-menu world candidate with separated left/right frames"

if ($HostLogPath) {
    $controllerSpaceOk =
        (Test-Regex $hostText '"event":"fnvxrControllerPoseSpace"[^\r\n]*"headRelative":false') -and
        !(Test-Regex $hostText '"event":"fnvxrControllerPoseSpace"[^\r\n]*"headRelative":true')

    Add-Check "host projection submit proof emitted" `
        (Test-Regex $hostText '"event":"fnvxrProjectionSubmit"') `
        "requires OpenXR host render submission proof"
    Add-Check "host submits projection layer" `
        (Test-Regex $hostText '"event":"fnvxrProjectionSubmit"[^\r\n]*"ready":true') `
        "requires both eyes rendered by host"
    Add-Check "host keeps retail menu as the UI texture" `
        (Test-Regex $hostText 'renderMode [^\r\n]*gameUi=1') `
        "requires the normal FNV menu capture to remain the active UI surface"
    Add-Check "controller poses are not head-relative" `
        $controllerSpaceOk `
        "blocks head-locked controller/hand proof"
}

if ($RequireSharedState) {
    $sharedState = Invoke-SharedStateProbe -Configuration $Configuration -SampleDelayMs $SharedStateSampleDelayMs
    $state = $sharedState.Json
    Add-Check "shared-state probe exit code is zero" `
        ($sharedState.ExitCode -eq 0) `
        "exitCode=$($sharedState.ExitCode)"
    if ($state) {
        Add-Check "shared runtime state is usable" `
            ($state.runtime.usable -eq $true) `
            "requires Local\FNVXR_Runtime_State"
        Add-Check "shared retail player state is usable" `
            ($state.player.usableForOpenMwCellPositionSync -eq $true) `
            "requires player node, cell, and finite position"
        Add-Check "shared camera state is active" `
            ($state.camera.usable -eq $true) `
            "requires active world camera state"
        Add-Check "shared mono/video surface is usable" `
            ($state.video.usable -eq $true) `
            "requires Local\FNVXR_D3D9_Frame_v1"
        Add-Check "shared stereo surface is usable for host" `
            ($state.stereo.usableForHostStereo -eq $true) `
            "requires pose-valid non-UI stereo frame"
        Add-Check "shared stereo surface is world stereo" `
            ($state.stereo.usableWorldStereo -eq $true) `
            "requires worldCandidate=true and separated=true"
        Add-Check "shared OpenXR pose state is usable" `
            ($state.pose.usable -eq $true) `
            "requires Local\FNVXR_VR_Pose_State"
        Add-Check "shared state is advancing" `
            ($state.freshness.allRequiredAdvanced -eq $true) `
            "requires player/runtime/camera/video/stereo/pose counters to advance over ${SharedStateSampleDelayMs}ms"
    }
}
}

$failed = @($Checks | Where-Object { -not $_.Pass })
$Checks | ForEach-Object {
    $status = if ($_.Pass) { "PASS" } else { "FAIL" }
    if ($_.Detail) {
        Write-Output ("[{0}] {1} - {2}" -f $status, $_.Name, $_.Detail)
    }
    else {
        Write-Output ("[{0}] {1}" -f $status, $_.Name)
    }
}

if ($failed.Count -gt 0) {
    throw "$($failed.Count) FNVXR proof check(s) failed"
}

Write-Output "FNVXR proof checks passed."
