param(
    [string]$Configuration = "Release",
    [string]$GameRoot = $(if ($env:FNVXR_GAME_ROOT) { $env:FNVXR_GAME_ROOT } else { "C:\Program Files (x86)\Steam\steamapps\common\Fallout New Vegas" }),
    [int]$GameBackbufferWidth = 2048,
    [int]$GameBackbufferHeight = 1280,
    [int]$HostGameTextureWidth = 2048,
    [int]$HostGameTextureHeight = 1280,
    [string]$Source2DPreset = "standard",
    [double]$DefaultFov = 0,
    [double]$FirstPersonFov = 0,
    [double]$PipboyFov = 0,
    [int]$UiWidth = 1280,
    [int]$UiHeight = 720,
    [int]$RetailReadyTimeoutSeconds = 60,
    [switch]$StopExisting,
    [switch]$FocusOnLaunch,
    [switch]$EnableStereoWorld,
    [switch]$EnableD3D9ShaderStereo,
    [switch]$StereoProducerProofOnly,
    [switch]$EnableCompositeShield,
    [switch]$EnableAtlasShield,
    [switch]$EnablePeripheralShield,
    [switch]$SkipVideoProbe,
    [switch]$StageOnly,
    [switch]$ValidateOnly,
    [double]$D3D9ShaderSanityOffset = 0,
    [string]$D3D9ShaderSanitySlot = "c1w",
    [string]$D3D9ShaderAllowVertexHashes = "",
    [string]$D3D9ShaderWvpContracts = ""
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "fnvxr-sidecar-common.ps1")

$Root = Split-Path -Parent $PSScriptRoot
$RunRoot = Join-Path $Root "local\retail-sidecar-runs"
$Stamp = Get-Date -Format "yyyyMMdd-HHmmss-fff"
$StartedAt = (Get-Date).ToString("o")
$RunDir = Join-Path $RunRoot $Stamp
$DebugLog = Join-Path $RunDir "launcher-debug.log"
$ManifestPath = Join-Path $RunDir "manifest.json"
$NvseLoader = Join-Path $GameRoot "nvse_loader.exe"
$BuildDir = Join-Path $Root "build"
$CommandExe = Join-Path $BuildDir "$Configuration\fnvxr_command.exe"
$fallout = $null
$falloutPid = $null

New-Item -ItemType Directory -Force -Path $RunDir | Out-Null

trap {
    $caught = $_
    Write-FnvxrCheckpoint -Path $DebugLog -Message ("ERROR " + $caught.Exception.Message)
    try {
        if ($falloutPid -and (Get-Process -Id $falloutPid -ErrorAction SilentlyContinue)) {
            Stop-Process -Id $falloutPid -Force -ErrorAction SilentlyContinue
        }
        if ((Get-Variable -Name fallout -Scope Script -ErrorAction SilentlyContinue) -and $fallout -and -not $fallout.HasExited) {
            Stop-Process -Id $fallout.Id -Force -ErrorAction SilentlyContinue
        }
    } catch {
    }
    try {
        $failureManifest = [ordered]@{
            startedAt = $StartedAt
            failedAt = (Get-Date).ToString("o")
            profile = "retail-sidecar"
            role = "retail-fnv-sidecar-surface-producer"
            failed = $true
            error = $caught.Exception.Message
            runDir = $RunDir
            gameRoot = $GameRoot
            openXrHostLaunched = $false
        }
        Write-FnvxrJsonAtomic -Value $failureManifest -Path $ManifestPath -Depth 8
    } catch {
    }
    [Console]::Error.WriteLine($caught.ToString())
    exit 1
}

$GameRoot = Resolve-FnvxrGameRoot -GameRoot $GameRoot -DebugLog $DebugLog
$NvseLoader = Join-Path $GameRoot "nvse_loader.exe"

Write-FnvxrCheckpoint -Path $DebugLog -Message "start retail-sidecar producer"

if ($StageOnly -and $ValidateOnly) {
    throw "-StageOnly and -ValidateOnly are mutually exclusive."
}
if (-not (Test-Path -LiteralPath $NvseLoader)) {
    throw "Missing nvse_loader.exe: $NvseLoader"
}

$Source2DPreset = $Source2DPreset.ToLowerInvariant()
switch ($Source2DPreset) {
    "standard" {
        if ($DefaultFov -le 0) { $DefaultFov = 75.0 }
        if ($FirstPersonFov -le 0) { $FirstPersonFov = 55.0 }
        if ($PipboyFov -le 0) { $PipboyFov = 47.0 }
    }
    "lite" {
        $GameBackbufferWidth = 1920
        $GameBackbufferHeight = 1200
        $HostGameTextureWidth = 1920
        $HostGameTextureHeight = 1200
        if ($DefaultFov -le 0) { $DefaultFov = 75.0 }
        if ($FirstPersonFov -le 0) { $FirstPersonFov = 55.0 }
        if ($PipboyFov -le 0) { $PipboyFov = 47.0 }
    }
    "balanced" {
        $GameBackbufferWidth = 2560
        $GameBackbufferHeight = 1600
        $HostGameTextureWidth = 2560
        $HostGameTextureHeight = 1600
        if ($DefaultFov -le 0) { $DefaultFov = 75.0 }
        if ($FirstPersonFov -le 0) { $FirstPersonFov = 55.0 }
        if ($PipboyFov -le 0) { $PipboyFov = 47.0 }
    }
    "highres" {
        $GameBackbufferWidth = 3072
        $GameBackbufferHeight = 1920
        $HostGameTextureWidth = 3072
        $HostGameTextureHeight = 1920
        if ($DefaultFov -le 0) { $DefaultFov = 125.0 }
        if ($FirstPersonFov -le 0) { $FirstPersonFov = 130.0 }
        if ($PipboyFov -le 0) { $PipboyFov = 58.0 }
    }
    "wide" {
        $GameBackbufferWidth = 2048
        $GameBackbufferHeight = 864
        $HostGameTextureWidth = 2048
        $HostGameTextureHeight = 864
        if ($DefaultFov -le 0) { $DefaultFov = 105.0 }
        if ($FirstPersonFov -le 0) { $FirstPersonFov = 82.0 }
        if ($PipboyFov -le 0) { $PipboyFov = 55.0 }
    }
    "ultrawide" {
        $GameBackbufferWidth = 2048
        $GameBackbufferHeight = 768
        $HostGameTextureWidth = 2048
        $HostGameTextureHeight = 768
        if ($DefaultFov -le 0) { $DefaultFov = 112.0 }
        if ($FirstPersonFov -le 0) { $FirstPersonFov = 88.0 }
        if ($PipboyFov -le 0) { $PipboyFov = 58.0 }
    }
    default {
        throw "Unknown -Source2DPreset '$Source2DPreset'. Use standard, lite, balanced, highres, wide, or ultrawide."
    }
}

$StereoWorldActive = [bool]($EnableStereoWorld -or $EnableD3D9ShaderStereo -or $StereoProducerProofOnly)
$BakedGamePlaneMode = if ($EnableCompositeShield -or $EnableAtlasShield -or $EnablePeripheralShield) { "shield2d" } else { "center2d" }
if ($StereoWorldActive) {
    $BakedGamePlaneMode = "stereo3d"
}
if ($BakedGamePlaneMode -eq "shield2d") {
    $DefaultFov = 115.0
    $FirstPersonFov = 95.0
    $PipboyFov = 58.0
}
Write-FnvxrCheckpoint -Path $DebugLog -Message ("source2d preset={0} backbuffer={1}x{2} hostTexture={3}x{4} fov(default={5},firstPerson={6},pipboy={7})" -f $Source2DPreset, $GameBackbufferWidth, $GameBackbufferHeight, $HostGameTextureWidth, $HostGameTextureHeight, $DefaultFov, $FirstPersonFov, $PipboyFov)

Assert-FnvxrDimensions `
    -GameBackbufferWidth $GameBackbufferWidth `
    -GameBackbufferHeight $GameBackbufferHeight `
    -HostGameTextureWidth $HostGameTextureWidth `
    -HostGameTextureHeight $HostGameTextureHeight `
    -UiWidth $UiWidth `
    -UiHeight $UiHeight

if ($StopExisting) {
    Stop-FnvxrLaunchProcess -DebugLog $DebugLog
} elseif ($StageOnly) {
    $existing = @(Get-FnvxrLaunchProcess)
    if ($existing.Count -gt 0) {
        throw "Stage-only refused while Fallout/FNVXR processes are running."
    }
} elseif (-not $ValidateOnly) {
    $existing = @(Get-FnvxrLaunchProcess)
    if ($existing.Count -gt 0) {
        throw "Launch refused because another FNVXR process or launcher is already running. Re-run with -StopExisting to take ownership."
    }
}

$staged = Copy-FnvxrRetailArtifacts `
    -Root $Root `
    -Configuration $Configuration `
    -GameRoot $GameRoot `
    -Copy (-not $ValidateOnly)

if ($ValidateOnly) {
    $failed = @($staged | Where-Object { -not $_.optional -and -not $_.verified })
    if ($failed.Count -gt 0) {
        $failed | ConvertTo-Json -Depth 6 | Write-Output
        throw "Validate-only artifact check failed."
    }
}

if ($ValidateOnly -or $StageOnly) {
    $manifest = [ordered]@{
        startedAt = $StartedAt
        profile = "retail-sidecar"
        validateOnly = [bool]$ValidateOnly
        stageOnly = [bool]$StageOnly
        runDir = $RunDir
        gameRoot = $GameRoot
        uiGrid = "${UiWidth}x${UiHeight}"
        enableCompositeShield = [bool]$EnableCompositeShield
        source2DPreset = $Source2DPreset
        fov = [ordered]@{
            default = $DefaultFov
            firstPerson = $FirstPersonFov
            pipboy = $PipboyFov
        }
        retailBackbuffer = "${GameBackbufferWidth}x${GameBackbufferHeight}"
        hostGameTexture = "${HostGameTextureWidth}x${HostGameTextureHeight}"
        openXrHostLaunched = $false
        staged = $staged
    }
    $manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $ManifestPath -Encoding UTF8
    $manifest | ConvertTo-Json -Depth 6
    return
}

Set-FnvxrFalloutIni `
    -Width $GameBackbufferWidth `
    -Height $GameBackbufferHeight `
    -DefaultFov $DefaultFov `
    -FirstPersonFov $FirstPersonFov `
    -PipboyFov $PipboyFov `
    -DisableMultisampling:$StereoWorldActive `
    -DebugLog $DebugLog
Set-FnvxrSidecarEnvironment `
    -Profile "retail-sidecar" `
    -UiWidth $UiWidth `
    -UiHeight $UiHeight `
    -InputWidth $GameBackbufferWidth `
    -InputHeight $GameBackbufferHeight `
    -HostGameTextureWidth $HostGameTextureWidth `
    -HostGameTextureHeight $HostGameTextureHeight `
    -RunDir $RunDir `
    -RunId $Stamp
if ($BakedGamePlaneMode -eq "shield2d") {
    $env:FNVXR_GAME_PLANE_MODE = "shield2d"
    $env:FNVXR_D3D9_WIDE_WORLD_REPLAY = "0"
    $env:FNVXR_D3D9_WIDE_WORLD_SHADER_MATRIX_DELTA = "0"
    $env:FNVXR_D3D9_HIDE_GAMEPLAY_HUD = "0"
    Write-FnvxrCheckpoint -Path $DebugLog -Message "source2d mode=shield2d baked wide-source-crop sourceFov(default=115,firstPerson=95,pipboy=58) centerFov(default=95,firstPerson=95) centerCrop=0.695240 hudOverlay=0 wideReplay=0 hudHidden=0"
} elseif ($BakedGamePlaneMode -eq "center2d") {
    $env:FNVXR_GAME_PLANE_MODE = "center2d"
    $env:FNVXR_D3D9_WIDE_WORLD_REPLAY = "0"
    $env:FNVXR_D3D9_WIDE_WORLD_SHADER_MATRIX_DELTA = "0"
    $env:FNVXR_D3D9_HIDE_GAMEPLAY_HUD = "0"
    Write-FnvxrCheckpoint -Path $DebugLog -Message "source2d mode=center2d center-only no-surround"
}
if ($EnableStereoWorld -or $EnableD3D9ShaderStereo -or $StereoProducerProofOnly) {
    $env:FNVXR_GAME_PLANE_MODE = "stereo3d"
    Set-FnvxrStereoWorldRuntimeEnvironment
    $StereoRuntimeMode = "stereo-world"
    Write-FnvxrCheckpoint -Path $DebugLog -Message "source2d mode=stereo3d stereo world runtime enabled by explicit launch switch"
} else {
    $StereoRuntimeMode = "quad-2d"
    Write-FnvxrCheckpoint -Path $DebugLog -Message "quad 2D runtime enabled; stereo world disabled by default"
}
if ($EnableD3D9ShaderStereo) {
    if ([string]::IsNullOrWhiteSpace($D3D9ShaderWvpContracts)) {
        throw "-EnableD3D9ShaderStereo requires verified -D3D9ShaderWvpContracts entries in fnv8/sha256/byteCount@register@column-or-row form."
    }
    $env:FNVXR_D3D9_SHADER_STEREO = "0"
    $env:FNVXR_D3D9_SHADER_MATRIX_DELTA = "0"
    $env:FNVXR_D3D9_SHADER_MATRIX_ORDER = "column"
    $env:FNVXR_D3D9_SHADER_MATRIX_REQUIRE_SHARED_CAMERA = "1"
    $env:FNVXR_D3D9_SHADER_WVP_REPLAY = "1"
    $env:FNVXR_D3D9_SHADER_WVP_CONTRACTS = $D3D9ShaderWvpContracts
    $env:FNVXR_D3D9_SHADER_MATRIX_MAX_CANDIDATES = "1"
    $env:FNVXR_D3D9_SHADER_PATCH_START_REGISTER = "0"
    if (-not [string]::IsNullOrWhiteSpace($D3D9ShaderAllowVertexHashes)) {
        $env:FNVXR_D3D9_SHADER_STEREO_ALLOW_VERTEX_HASHES = $D3D9ShaderAllowVertexHashes
        Write-FnvxrCheckpoint -Path $DebugLog -Message ("retail producer shader vertex allowlist enabled hashes={0}" -f $env:FNVXR_D3D9_SHADER_STEREO_ALLOW_VERTEX_HASHES)
    }
    if ($D3D9ShaderSanityOffset -ne 0) {
        $env:FNVXR_D3D9_SHADER_SANITY_OFFSET = $D3D9ShaderSanityOffset.ToString([System.Globalization.CultureInfo]::InvariantCulture)
        $env:FNVXR_D3D9_SHADER_SANITY_SLOT = $D3D9ShaderSanitySlot
        $env:FNVXR_D3D9_SHADER_SANITY_START_REGISTER = "0"
        Write-FnvxrCheckpoint -Path $DebugLog -Message ("retail producer shader sanity offset enabled slot={0} offset={1}" -f $env:FNVXR_D3D9_SHADER_SANITY_SLOT, $env:FNVXR_D3D9_SHADER_SANITY_OFFSET)
    }
    $env:FNVXR_D3D9_STEREO_SKIP_SHADER_HASH_PAIRS = "e7106f46/323e1098;5dbbefdc/0a008802;0187cba7/79ed2742;5f8e2513/d2b33434"
    if ($StereoProducerProofOnly) {
        $env:FNVXR_D3D9_DEBUG_COMPARE_REPLAY_DRAWS = "24"
        $env:FNVXR_D3D9_STEREO_COLLAPSE_AUDIT = "1"
        $env:FNVXR_D3D9_STEREO_AUTO_SKIP_COLLAPSE_SHADER_PAIRS = "1"
        $env:FNVXR_D3D9_STEREO_COLLAPSE_AUDIT_DRAWS = "2048"
        $env:FNVXR_D3D9_STEREO_COLLAPSE_AUDIT_LOG_STRIDE = "128"
        $env:FNVXR_D3D9_STEREO_TARGET_DIFF_PROBE = "1"
    } else {
        $env:FNVXR_D3D9_DEBUG_COMPARE_REPLAY_DRAWS = "0"
        $env:FNVXR_D3D9_STEREO_COLLAPSE_AUDIT = "0"
        $env:FNVXR_D3D9_STEREO_AUTO_SKIP_COLLAPSE_SHADER_PAIRS = "0"
        $env:FNVXR_D3D9_STEREO_TARGET_DIFF_PROBE = "0"
    }
    $env:FNVXR_D3D9_STEREO_SNAPSHOT_FIXED_DRAW = "0"
    $env:FNVXR_D3D9_STEREO_SNAPSHOT_DRAW_STRIDE = "8"
    Write-FnvxrCheckpoint -Path $DebugLog -Message "retail producer shader camera-gated VP delta path enabled"
}
Write-FnvxrCheckpoint -Path $DebugLog -Message ("profile=retail-sidecar environment set stereoRuntime={0}" -f $StereoRuntimeMode)

foreach ($log in @(
    (Join-Path $GameRoot "Data\NVSE\Plugins\fnvxr_input_telemetry.log"),
    (Join-Path $GameRoot "Data\NVSE\Plugins\fnvxr_dinput_proxy.log"),
    (Join-Path $GameRoot "Data\NVSE\Plugins\fnvxr_xinput_proxy.log"),
    (Join-Path $GameRoot "fnvxr_d3d9_proxy.log")
)) {
    if (Test-Path -LiteralPath $log) {
        Move-Item -LiteralPath $log -Destination (Join-Path $RunDir ("prelaunch-" + (Split-Path -Leaf $log))) -Force
    }
}

$fallout = Start-Process -FilePath $NvseLoader -WorkingDirectory $GameRoot -PassThru
$falloutHwnd = Find-FnvxrFalloutWindow -Process $fallout -Focus ([bool]$FocusOnLaunch)
$falloutProcess = Get-Process FalloutNV -ErrorAction SilentlyContinue | Sort-Object StartTime -Descending | Select-Object -First 1
$falloutPid = if ($falloutProcess) { $falloutProcess.Id } else { $fallout.Id }
Write-FnvxrCheckpoint -Path $DebugLog -Message ("retail started pid={0} launcherPid={1} hwnd={2}" -f $falloutPid, $fallout.Id, $falloutHwnd)

$retailFrameProbe = $null
$activationReachApplied = $false
if (-not $SkipVideoProbe) {
    $probeRoot = Join-Path $RunDir "retail-video-probe"
    $retailFrameProbe = & (Join-Path $PSScriptRoot "probe-retail-video-frame.ps1") `
        -OutputRoot $probeRoot `
        -TimeoutSeconds $RetailReadyTimeoutSeconds 2>&1
    Write-FnvxrCheckpoint -Path $DebugLog -Message "retail shared video proof observed"
    $activationReachApplied = Invoke-FnvxrRuntimeConsoleCommand `
        -CommandExe $CommandExe `
        -Line "setgs iActivatePickLength 260" `
        -DebugLog $DebugLog `
        -Label "activation reach"
}

$manifest = [ordered]@{
    startedAt = $StartedAt
    profile = "retail-sidecar"
    role = "retail-fnv-sidecar-surface-producer"
    runDir = $RunDir
    gameRoot = $GameRoot
    stereoRuntime = $StereoRuntimeMode
    enableD3D9ShaderStereo = [bool]$EnableD3D9ShaderStereo
    stereoProducerProofOnly = [bool]$StereoProducerProofOnly
    enableCompositeShield = [bool]$EnableCompositeShield
    enableAtlasShield = [bool]$EnableAtlasShield
    enablePeripheralShield = [bool]$EnablePeripheralShield
    falloutPid = $falloutPid
    launcherPid = $fallout.Id
    falloutHwnd = $falloutHwnd.ToString()
    retailFrameProof = $retailFrameProbe
    activationReach = [ordered]@{
        gameSetting = "iActivatePickLength"
        value = 260
        applied = [bool]$activationReachApplied
    }
    mapping = "Local\FNVXR_D3D9_Frame_v1"
    stereoMapping = "Local\FNVXR_D3D9_StereoFrame_v3"
    openXrHostLaunched = $false
    forcedMenuAction = $false
    uiGrid = "${UiWidth}x${UiHeight}"
    source2DPreset = $Source2DPreset
    fov = [ordered]@{
        default = $DefaultFov
        firstPerson = $FirstPersonFov
        pipboy = $PipboyFov
    }
    retailBackbuffer = "${GameBackbufferWidth}x${GameBackbufferHeight}"
    hostGameTexture = "${HostGameTextureWidth}x${HostGameTextureHeight}"
    staged = $staged
}
$manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $ManifestPath -Encoding UTF8
$manifest | ConvertTo-Json -Depth 6
