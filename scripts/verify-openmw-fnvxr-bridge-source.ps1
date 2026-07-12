param(
    [string]$OpenMwSourceRoot = $env:FNVXR_OPENMW_SOURCE_ROOT,
    [string]$ReportPath = ""
)

$ErrorActionPreference = "Stop"

if (-not $OpenMwSourceRoot) {
    throw "Set -OpenMwSourceRoot or FNVXR_OPENMW_SOURCE_ROOT to the local OpenMW source root."
}

$Root = Split-Path -Parent $PSScriptRoot
if (-not $ReportPath) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $ReportPath = Join-Path $Root "local\openmw-bridge-source-checks\$stamp\manifest.json"
}

$script:Checks = @()

function Add-Check {
    param(
        [string]$Name,
        [bool]$Pass,
        [string]$Detail = ""
    )

    $script:Checks += [ordered]@{
        name = $Name
        pass = $Pass
        detail = $Detail
    }
}

function Read-RequiredText {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Missing required source file: $Path"
    }
    return Get-Content -Raw -LiteralPath $Path
}

function Remove-CxxComments {
    param([string]$Text)

    $builder = [System.Text.StringBuilder]::new()
    $state = "code"
    $escape = $false

    for ($i = 0; $i -lt $Text.Length; ++$i) {
        $ch = $Text[$i]
        $next = if ($i + 1 -lt $Text.Length) { $Text[$i + 1] } else { [char]0 }

        switch ($state) {
            "code" {
                if ($ch -eq '"') {
                    [void]$builder.Append($ch)
                    $state = "double"
                    $escape = $false
                } elseif ($ch -eq "'") {
                    [void]$builder.Append($ch)
                    $state = "single"
                    $escape = $false
                } elseif ($ch -eq "/" -and $next -eq "/") {
                    $state = "line"
                    ++$i
                } elseif ($ch -eq "/" -and $next -eq "*") {
                    $state = "block"
                    ++$i
                } else {
                    [void]$builder.Append($ch)
                }
            }
            "line" {
                if ($ch -eq "`n") {
                    [void]$builder.Append($ch)
                    $state = "code"
                }
            }
            "block" {
                if ($ch -eq "*" -and $next -eq "/") {
                    ++$i
                    $state = "code"
                } elseif ($ch -eq "`n") {
                    [void]$builder.Append($ch)
                }
            }
            "double" {
                [void]$builder.Append($ch)
                if ($escape) {
                    $escape = $false
                } elseif ($ch -eq "\") {
                    $escape = $true
                } elseif ($ch -eq '"') {
                    $state = "code"
                }
            }
            "single" {
                [void]$builder.Append($ch)
                if ($escape) {
                    $escape = $false
                } elseif ($ch -eq "\") {
                    $escape = $true
                } elseif ($ch -eq "'") {
                    $state = "code"
                }
            }
        }
    }

    return $builder.ToString()
}

function Remove-CMakeComments {
    param([string]$Text)

    return (($Text -split "`r?`n") | ForEach-Object {
        $_ -replace "\s*#.*$", ""
    }) -join "`n"
}

function Normalize-CxxExpression {
    param([string]$Expression)

    return (($Expression -replace "\s+", " ") -replace "\s*([<>=|+*/(),-])\s*", '$1').Trim()
}

function Get-CxxConst {
    param(
        [string]$Code,
        [string]$Name
    )

    $escapedName = [regex]::Escape($Name)
    $pattern = "constexpr\s+(?:std::)?(?:u?int\d+_t|size_t)\s+$escapedName\s*=\s*([^;]+);"
    $match = [regex]::Match($Code, $pattern)
    if (-not $match.Success) {
        return $null
    }
    return Normalize-CxxExpression $match.Groups[1].Value
}

function Test-CodeRegex {
    param(
        [string]$Code,
        [string]$Pattern
    )

    return [regex]::IsMatch($Code, $Pattern, [System.Text.RegularExpressions.RegexOptions]::Singleline)
}

$protocolPath = Join-Path $Root "protocol\fnvxr_shared_state.h"
$bridgePath = Join-Path $OpenMwSourceRoot "apps\openmw\mwvr\fnvxrliveframesurface.cpp"
$bridgeHeaderPath = Join-Path $OpenMwSourceRoot "apps\openmw\mwvr\fnvxrliveframesurface.hpp"
$vrInputPath = Join-Path $OpenMwSourceRoot "apps\openmw\mwvr\vrinputmanager.cpp"
$vrGuiPath = Join-Path $OpenMwSourceRoot "apps\openmw\mwvr\vrgui.cpp"
$openMwCMakePath = Join-Path $OpenMwSourceRoot "apps\openmw\CMakeLists.txt"
$viewerCppPath = Join-Path $OpenMwSourceRoot "components\vr\viewer.cpp"
$viewerHeaderPath = Join-Path $OpenMwSourceRoot "components\vr\viewer.hpp"

$protocolCode = Remove-CxxComments (Read-RequiredText $protocolPath)
$bridgeCode = Remove-CxxComments (Read-RequiredText $bridgePath)
$bridgeHeaderCode = Remove-CxxComments (Read-RequiredText $bridgeHeaderPath)
$vrInputCode = Remove-CxxComments (Read-RequiredText $vrInputPath)
$vrGuiCode = Remove-CxxComments (Read-RequiredText $vrGuiPath)
$openMwCMakeCode = Remove-CMakeComments (Read-RequiredText $openMwCMakePath)
$viewerCppCode = Remove-CxxComments (Read-RequiredText $viewerCppPath)
$viewerHeaderCode = Remove-CxxComments (Read-RequiredText $viewerHeaderPath)

$constantPairs = @(
    @{ Canonical = "DInputSharedMagic"; OpenMw = "DInputSharedMagic"; Code = $bridgeCode },
    @{ Canonical = "DInputSharedVersion"; OpenMw = "DInputSharedVersion"; Code = $bridgeCode },
    @{ Canonical = "VrPoseSharedMagic"; OpenMw = "VrPoseSharedMagic"; Code = $bridgeCode },
    @{ Canonical = "VrPoseSharedVersion"; OpenMw = "VrPoseSharedVersion"; Code = $bridgeCode },
    @{ Canonical = "CameraSharedMagic"; OpenMw = "CameraSharedMagic"; Code = $bridgeCode },
    @{ Canonical = "CameraSharedVersion"; OpenMw = "CameraSharedVersion"; Code = $bridgeCode },
    @{ Canonical = "RuntimeSharedMagic"; OpenMw = "RuntimeSharedMagic"; Code = $bridgeCode },
    @{ Canonical = "RuntimeSharedVersion"; OpenMw = "RuntimeSharedVersion"; Code = $bridgeCode },
    @{ Canonical = "PlayerSharedMagic"; OpenMw = "PlayerSharedMagic"; Code = $bridgeCode },
    @{ Canonical = "PlayerSharedVersion"; OpenMw = "PlayerSharedVersion"; Code = $bridgeCode },
    @{ Canonical = "OpenMwPlayerSharedMagic"; OpenMw = "OpenMwPlayerSharedMagic"; Code = $bridgeCode },
    @{ Canonical = "OpenMwPlayerSharedVersion"; OpenMw = "OpenMwPlayerSharedVersion"; Code = $bridgeCode },
    @{ Canonical = "D3D9FrameSharedMagic"; OpenMw = "SharedVideoMagic"; Code = $bridgeCode },
    @{ Canonical = "D3D9StereoFrameSharedMagic"; OpenMw = "SharedStereoMagic"; Code = $bridgeCode },
    @{ Canonical = "D3D9SharedFrameMaxWidth"; OpenMw = "SharedVideoMaxWidth"; Code = $bridgeCode },
    @{ Canonical = "D3D9SharedFrameMaxHeight"; OpenMw = "SharedVideoMaxHeight"; Code = $bridgeCode },
    @{ Canonical = "PlayerSharedFlagPlayerNodeValid"; OpenMw = "PlayerSharedFlagPlayerNodeValid"; Code = $bridgeCode },
    @{ Canonical = "PlayerSharedFlagCameraValid"; OpenMw = "PlayerSharedFlagCameraValid"; Code = $bridgeCode },
    @{ Canonical = "PlayerSharedFlagCellKnown"; OpenMw = "PlayerSharedFlagCellKnown"; Code = $bridgeCode },
    @{ Canonical = "PlayerSharedFlagThirdPerson"; OpenMw = "PlayerSharedFlagThirdPerson"; Code = $bridgeCode },
    @{ Canonical = "PlayerSharedFlagGameplay"; OpenMw = "PlayerSharedFlagGameplay"; Code = $bridgeCode },
    @{ Canonical = "XInputSharedMagic"; OpenMw = "XInputSharedMagic"; Code = $vrInputCode },
    @{ Canonical = "XInputSharedVersion"; OpenMw = "XInputSharedVersion"; Code = $vrInputCode }
)

foreach ($pair in $constantPairs) {
    $canonicalValue = Get-CxxConst -Code $protocolCode -Name $pair.Canonical
    $openMwValue = Get-CxxConst -Code $pair.Code -Name $pair.OpenMw
    Add-Check "ABI constant $($pair.Canonical) matches OpenMW $($pair.OpenMw)" `
        ($null -ne $canonicalValue -and $canonicalValue -eq $openMwValue) `
        "canonical=$canonicalValue openmw=$openMwValue"
}

$sourceChecks = @(
    @{ Name = "OpenMW build lists FNVXR live frame surface"; Pass = ($openMwCMakeCode -match "\bfnvxrliveframesurface\b"); Detail = $openMwCMakePath },
    @{ Name = "VR GUI initializes retail frame surface"; Pass = ($vrGuiCode.Contains("FNVXRLiveFrameSurface::instance().init")); Detail = $vrGuiPath },
    @{ Name = "VR GUI updates retail frame surface"; Pass = ($vrGuiCode.Contains("FNVXRLiveFrameSurface::instance().update")); Detail = $vrGuiPath },
    @{ Name = "retail world readiness requires camera/player state by default"; Pass = (Test-CodeRegex $bridgeCode 'const\s+bool\s+runtimeWorldReady\s*=\s*retailRuntimeWorldReady\s*\(\s*\)\s*;\s*const\s+bool\s+retailGameplay\s*=\s*runtimeWorldReady\s*&&\s*\(\s*envEnabled\s*\(\s*"OPENMW_FNVXR_ALLOW_RUNTIME_ONLY_WORLD_READY"\s*,\s*false\s*\)\s*\|\|\s*retailWorldActive\s*\(\s*\)\s*\)\s*;'); Detail = $bridgePath },
    @{ Name = "OpenMW publishes player shared state"; Pass = ($bridgeCode.Contains('"Local\\FNVXR_OpenMW_Player_State"') -and $bridgeCode.Contains("CreateFileMappingA")); Detail = $bridgePath },
    @{ Name = "OpenMW reads retail player shared state"; Pass = ($bridgeCode.Contains('"Local\\FNVXR_Player_State"') -and $bridgeCode.Contains("tryReadPlayerState")); Detail = $bridgePath },
    @{ Name = "OpenMW reads retail camera state"; Pass = ($bridgeCode.Contains('"Local\\FNVXR_Camera_State"') -and $bridgeCode.Contains("tryReadCameraActive")); Detail = $bridgePath },
    @{ Name = "OpenMW reads retail runtime state"; Pass = ($bridgeCode.Contains('"Local\\FNVXR_Runtime_State"') -and $bridgeCode.Contains("retailRuntimeWorldReady")); Detail = $bridgePath },
    @{ Name = "OpenMW reads retail stereo frame"; Pass = ($bridgeCode.Contains('"Local\\FNVXR_D3D9_StereoFrame_v1"') -and $bridgeCode.Contains("retailStereoWorldReady")); Detail = $bridgePath },
    @{ Name = "OpenMW reads retail mono frame"; Pass = ($bridgeCode.Contains('"Local\\FNVXR_D3D9_Frame_v1"') -and $bridgeCode.Contains("readFrame")); Detail = $bridgePath },
    @{ Name = "OpenMW publishes VR pose to retail mapping"; Pass = ($bridgeCode.Contains('"Local\\FNVXR_VR_Pose_State"') -and $bridgeCode.Contains("publishVrPose")); Detail = $bridgePath },
    @{ Name = "OpenMW syncs player cell, position, and rotation"; Pass = ($bridgeCode.Contains("changeToCell") -and $bridgeCode.Contains("moveObject") -and $bridgeCode.Contains("rotateObject")); Detail = $bridgePath },
    @{ Name = "OpenMW projection takeover can suppress primary layer"; Pass = ($bridgeCode.Contains("setPrimaryProjectionLayerEnabled(false)") -and $bridgeCode.Contains("setPrimaryProjectionLayerEnabled(true)")); Detail = $bridgePath },
    @{ Name = "OpenMW uploads stereo frame into projection swapchains"; Pass = ($bridgeCode.Contains("glTexSubImage2D") -and $bridgeCode.Contains("mProjectionUploadPixels")); Detail = $bridgePath },
    @{ Name = "Viewer exposes primary projection layer gate"; Pass = ($viewerHeaderCode.Contains("setPrimaryProjectionLayerEnabled") -and $viewerCppCode.Contains("mPrimaryProjectionLayerEnabled")); Detail = $viewerHeaderPath },
    @{ Name = "Shared player layout is asserted in OpenMW bridge"; Pass = ($bridgeCode.Contains("static_assert(sizeof(SharedPlayerState) == 160") -and $bridgeCode.Contains("offsetof(SharedPlayerState, currentCellFormId) == 24") -and $bridgeCode.Contains("offsetof(SharedPlayerState, playerWorldPos) == 76")); Detail = $bridgePath },
    @{ Name = "Shared stereo layout is asserted in OpenMW bridge"; Pass = ($bridgeCode.Contains("static_assert(sizeof(SharedStereoHeader) == 136") -and $bridgeCode.Contains("offsetof(SharedStereoHeader, poseValid) == 40")); Detail = $bridgePath },
    @{ Name = "Shared XInput layout is asserted in OpenMW input bridge"; Pass = ($vrInputCode.Contains("static_assert(sizeof(SharedXInputState) == 28")); Detail = $vrInputPath },
    @{ Name = "OpenMW publishes XInput state to retail mapping"; Pass = ($vrInputCode.Contains('"Local\\FNVXR_XInput_State"') -and $vrInputCode.Contains("shared->magic = XInputSharedMagic")); Detail = $vrInputPath },
    @{ Name = "OpenMW frame surface header declares frame callbacks"; Pass = ($bridgeHeaderCode.Contains("onFrameUpdate") -and $bridgeHeaderCode.Contains("onFrameEnd")); Detail = $bridgeHeaderPath }
)

foreach ($check in $sourceChecks) {
    Add-Check -Name $check.Name -Pass ([bool]$check.Pass) -Detail $check.Detail
}

$failed = @($Checks | Where-Object { -not $_.pass })
$manifest = [ordered]@{
    generatedAt = (Get-Date).ToString("o")
    openMwSourceRoot = $OpenMwSourceRoot
    protocolPath = $protocolPath
    status = if ($failed.Count -eq 0) { "passed" } else { "failed" }
    checks = $Checks
}

$reportDir = Split-Path -Parent $ReportPath
if ($reportDir) {
    New-Item -ItemType Directory -Force -Path $reportDir | Out-Null
}
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ReportPath -Encoding UTF8

foreach ($check in $Checks) {
    $status = if ($check.pass) { "PASS" } else { "FAIL" }
    if ($check.detail) {
        Write-Output ("[{0}] {1} - {2}" -f $status, $check.name, $check.detail)
    } else {
        Write-Output ("[{0}] {1}" -f $status, $check.name)
    }
}

if ($failed.Count -gt 0) {
    throw "$($failed.Count) OpenMW FNVXR bridge source check(s) failed. Manifest: $ReportPath"
}

Write-Output "OpenMW FNVXR bridge source checks passed."
Write-Output "Manifest: $ReportPath"
