param(
    [string]$Configuration = "Release",
    [string]$GameRoot = $(if ($env:FNVXR_GAME_ROOT) { $env:FNVXR_GAME_ROOT } else { "C:\Program Files (x86)\Steam\steamapps\common\Fallout New Vegas" }),
    [int]$Frames = 0,
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
    [int]$HostReadyTimeoutSeconds = 45,
    [int]$RetailReadyTimeoutSeconds = 60,
    [int]$WorldEntryTimeoutSeconds = 300,
    [int]$WorldProofTimeoutSeconds = 30,
    [int]$WorldProofStableSamples = 12,
    [int]$AutomatedMenuTimeoutSeconds = 60,
    [int]$TestLoadoutTimeoutSeconds = 180,
    [string]$AutomatedSaveName = "FNVXR_HostExitRecovery",
    [switch]$StopExisting,
    [switch]$FocusRetailWindow,
    [switch]$AllowFiniteHostRun,
    [switch]$EnableStereoWorld,
    [switch]$EnableRetailRig,
    [switch]$ApplyRetailRig,
    [switch]$DisableRetailRig,
    [switch]$EnableD3D9ShaderStereo,
    [switch]$StereoProducerProofOnly,
    [switch]$EnableCompositeShield,
    [switch]$EnableAtlasShield,
    [switch]$EnablePeripheralShield,
    [switch]$NoWorldProofWatchdog,
    [switch]$NoSidecarExitWatcher,
    [switch]$NoTelemetryHammer,
    [switch]$NoBuild,
    [switch]$DumpD3D9ShaderBytecode,
    [switch]$NoRetail,
    [switch]$StageOnly,
    [switch]$ValidateOnly,
    [switch]$ApplyTestLoadout,
    [switch]$AutomateContinue,
    [double]$D3D9ShaderSanityOffset = 0,
    [string]$D3D9ShaderSanitySlot = "c1w",
    [string]$D3D9ShaderAllowVertexHashes = "",
    [string]$D3D9ShaderWvpContracts = "",
    [string[]]$VerifiedShaderDiscoveryRunDir = @()
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "fnvxr-sidecar-common.ps1")

if ($StageOnly -and $ValidateOnly) {
    throw "-StageOnly and -ValidateOnly are mutually exclusive."
}
if (-not ($StageOnly -or $ValidateOnly)) {
    throw "All live OpenXR presentation is intentionally blocked. The remaining finite blockers are an authenticated supervisor/completion record, source-correlated asynchronous output proof, isolated color/depth render transactions, conservative stereo visibility, per-eye depth, complete auxiliary-resource twinning, exact VS+PS camera provenance, and a proved retail rig schedule. This launcher can only -StageOnly or -ValidateOnly; it will not launch the game or touch an OpenXR runtime."
}

$Root = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $Root "build"
$BuildWin32Dir = Join-Path $Root "build-win32"
$HostExe = Join-Path $BuildDir "$Configuration\fnvxr_openxr_pose_host.exe"
$ProbeExe = Join-Path $BuildDir "$Configuration\fnvxr_shared_state_probe.exe"
$CommandExe = Join-Path $BuildDir "$Configuration\fnvxr_command.exe"
$InputExe = Join-Path $BuildDir "$Configuration\fnvxr_input.exe"
$NvseLoader = Join-Path $GameRoot "nvse_loader.exe"
$RunRoot = Join-Path $Root "local\openxr-retail-sidecar-runs"
$Stamp = Get-Date -Format "yyyyMMdd-HHmmss-fff"
$StartedAt = (Get-Date).ToString("o")
$RunDir = Join-Path $RunRoot $Stamp
$DebugLog = Join-Path $RunDir "launcher-debug.log"
$ManifestPath = Join-Path $RunDir "manifest.json"
$hostProcess = $null
$fallout = $null
$falloutPid = $null
$watcher = $null

New-Item -ItemType Directory -Force -Path $RunDir | Out-Null

trap {
    $caught = $_
    # The source-level live fuse executes before a run directory exists. Its
    # diagnostic must remain the user-visible error and must not be replaced by
    # an empty-path logging failure from this later script-scope trap.
    if (-not [string]::IsNullOrWhiteSpace([string]$DebugLog)) {
        Write-FnvxrCheckpoint -Path $DebugLog -Message ("ERROR " + $caught.Exception.Message)
    }
    $hostWasRunning = [bool]($hostProcess -and -not $hostProcess.HasExited)
    $retailWasRunning = [bool]($falloutPid -and (Get-Process -Id $falloutPid -ErrorAction SilentlyContinue))
    $launcherWasRunning = [bool]($fallout -and -not $fallout.HasExited)
    try {
        if ($watcher -and -not $watcher.HasExited) {
            Stop-Process -Id $watcher.Id -Force -ErrorAction SilentlyContinue
        }
        if ($falloutPid) {
            Stop-Process -Id $falloutPid -Force -ErrorAction SilentlyContinue
        }
        if ($fallout -and -not $fallout.HasExited) {
            Stop-Process -Id $fallout.Id -Force -ErrorAction SilentlyContinue
        }
        if ($hostProcess -and -not $hostProcess.HasExited) {
            Stop-Process -Id $hostProcess.Id -Force -ErrorAction SilentlyContinue
        }
    } catch {
    }
    try {
        $failureManifest = [ordered]@{}
        if (Test-Path -LiteralPath $ManifestPath) {
            try {
                $existingManifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
                foreach ($property in $existingManifest.PSObject.Properties) {
                    $failureManifest[$property.Name] = $property.Value
                }
            } catch {
            }
        }
        $failureManifest["startedAt"] = $StartedAt
        $failureManifest["failedAt"] = (Get-Date).ToString("o")
        $failureManifest["profile"] = "openxr-sidecar"
        $failureManifest["role"] = "openxr-host-retail-fnv-sidecar"
        $failureManifest["acceptanceProfile"] = $(if ($StereoProducerProofOnly) { "diagnostic-producer" } elseif ($EnableStereoWorld -or $EnableD3D9ShaderStereo) { "full-vr" } else { "quad-2d-transport" })
        $failureManifest["accepted"] = $false
        $failureManifest["failed"] = $true
        $failureManifest["error"] = $caught.Exception.Message
        $failureManifest["runDir"] = $RunDir
        $failureManifest["gameRoot"] = $GameRoot
        $failureManifest["hostOut"] = $(if (Get-Variable -Name hostOut -ErrorAction SilentlyContinue) { $hostOut } else { $null })
        $failureManifest["hostErr"] = $(if (Get-Variable -Name hostErr -ErrorAction SilentlyContinue) { $hostErr } else { $null })
        $failureManifest["retailLaunched"] = [bool]$falloutPid
        $failureManifest["retailWasRunningAtFailure"] = $retailWasRunning
        $failureManifest["launcherWasRunningAtFailure"] = $launcherWasRunning
        $failureManifest["hostWasRunningAtFailure"] = $hostWasRunning
        $failureManifest["runtimeStoppedAfterFailure"] = -not [bool](
            ($falloutPid -and (Get-Process -Id $falloutPid -ErrorAction SilentlyContinue)) -or
            ($hostProcess -and (Get-Process -Id $hostProcess.Id -ErrorAction SilentlyContinue)))
        Write-FnvxrJsonAtomic -Value $failureManifest -Path $ManifestPath -Depth 8
    } catch {
    }
    [Console]::Error.WriteLine($caught.ToString())
    exit 1
}

$GameRoot = Resolve-FnvxrGameRoot -GameRoot $GameRoot -DebugLog $DebugLog
$NvseLoader = Join-Path $GameRoot "nvse_loader.exe"

Write-FnvxrCheckpoint -Path $DebugLog -Message "start openxr-retail-sidecar"

if (-not ($StageOnly -or $ValidateOnly) -and ($NoRetail -or $NoSidecarExitWatcher)) {
    throw "Live OpenXR host-only and watcher-free modes are intentionally blocked. Every launched host must be paired with retail and the heartbeat/exit watchdog."
}
if ($DisableRetailRig -and ($EnableRetailRig -or $ApplyRetailRig)) {
    throw "-DisableRetailRig cannot be combined with -EnableRetailRig or -ApplyRetailRig."
}
if (-not [string]::IsNullOrWhiteSpace($D3D9ShaderWvpContracts)) {
    throw "Raw -D3D9ShaderWvpContracts text is not accepted; use -VerifiedShaderDiscoveryRunDir."
}
$verifiedShaderDiscoveryRunDirs = @($VerifiedShaderDiscoveryRunDir |
    Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
if ($EnableD3D9ShaderStereo) {
    if ($verifiedShaderDiscoveryRunDirs.Count -eq 0) {
        throw "-EnableD3D9ShaderStereo requires -VerifiedShaderDiscoveryRunDir so contracts are regenerated from captured bytecode."
    }
    $D3D9ShaderWvpContracts = & (Join-Path $PSScriptRoot "get-verified-shader-wvp-contracts.ps1") `
        -RunDir $verifiedShaderDiscoveryRunDirs `
        -ContractsOnly
    if ([string]::IsNullOrWhiteSpace($D3D9ShaderWvpContracts)) {
        throw "Verified shader discovery produced no complete safe contracts."
    }
}
if (-not ($StageOnly -or $ValidateOnly)) {
    if ($EnableStereoWorld -or $EnableD3D9ShaderStereo -or $StereoProducerProofOnly) {
        throw "OpenXR stereo launch is intentionally blocked: isolated color/depth transactions, per-eye OpenXR depth submission, conservative stereo visibility/LOD/portal/particle traversal, complete auxiliary resource twinning, and exact VS+PS camera-semantic contracts are not implemented. A rejected center traversal can already have changed retail pixels and camera-dependent engine state, while D3D replay cannot recover geometry omitted before submission. Only flat 2D capture remains launchable; stereo producer execution is blocked off-headset as well."
    }
    if ($ApplyRetailRig) {
        throw "Retail rig application is intentionally blocked: the exact animation/render commit schedule is not yet proved. Read-only rig telemetry remains available."
    }
}
if (-not ($StageOnly -or $ValidateOnly) -and $Frames -le 0) {
    throw "Infinite OpenXR host runs are intentionally blocked until an authenticated external supervisor lease exists. Use an explicitly bounded -Frames value."
}
if ($Frames -gt 0 -and -not $AllowFiniteHostRun) {
    throw "Finite host runs require -AllowFiniteHostRun."
}
if ($Frames -gt 7200) {
    throw "Finite host runs are capped at 7200 frames."
}
if ($WorldProofStableSamples -lt 1 -or $WorldProofStableSamples -gt 120) {
    throw "-WorldProofStableSamples must be between 1 and 120."
}
if ($AutomateContinue -and $AutomatedMenuTimeoutSeconds -lt 1) {
    throw "-AutomatedMenuTimeoutSeconds must be positive when -AutomateContinue is used."
}
if ($AutomateContinue -and -not (Test-Path -LiteralPath $CommandExe)) {
    throw "-AutomateContinue requires the already-built command helper: $CommandExe"
}
if ($AutomateContinue -and $AutomatedSaveName -notmatch '^[A-Za-z0-9_-]+$') {
    throw "-AutomatedSaveName must contain only letters, numbers, underscores, or hyphens."
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
$AcceptanceProfile = if ($StereoProducerProofOnly) {
    "diagnostic-producer"
} elseif ($StereoWorldActive) {
    "full-vr"
} else {
    "quad-2d-transport"
}
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

if (-not (Test-Path -LiteralPath $NvseLoader)) {
    throw "Missing nvse_loader.exe: $NvseLoader"
}

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

if (-not $NoBuild -and -not $ValidateOnly) {
    & (Join-Path $PSScriptRoot "build-win32.ps1") -Configuration $Configuration
    if ($LASTEXITCODE -ne 0) {
        throw "Win32 build failed with exit code $LASTEXITCODE"
    }

    cmake -S $Root -B $BuildDir -A x64
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed with exit code $LASTEXITCODE"
    }
    cmake --build $BuildDir --config $Configuration --target fnvxr_openxr_pose_host fnvxr_shared_state_probe fnvxr_command
    if ($LASTEXITCODE -ne 0) {
        throw "OpenXR host/probe build failed with exit code $LASTEXITCODE"
    }
} else {
    $runtimeArtifacts = @(
        $HostExe,
        $ProbeExe,
        $CommandExe,
        $InputExe,
        (Join-Path $BuildWin32Dir "$Configuration\nvse_fnvxr.dll"),
        (Join-Path $BuildWin32Dir "$Configuration\d3d9.dll"),
        (Join-Path $BuildWin32Dir "$Configuration\dinput8.dll"),
        (Join-Path $BuildWin32Dir "$Configuration\xinput1_3.dll")
    )
    $missingArtifacts = @($runtimeArtifacts | Where-Object { -not (Test-Path -LiteralPath $_) })
    if ($missingArtifacts.Count -gt 0) {
        throw "Artifact validation refused because runtime artifacts are missing: $($missingArtifacts -join ', ')"
    }
    $sharedBuildSources = @(
        Get-ChildItem -LiteralPath (Join-Path $Root "protocol") -Recurse -File |
            Where-Object { $_.Extension -in @(".cpp", ".h", ".hpp", ".def") }
    )
    $sharedBuildSources += Get-Item -LiteralPath (Join-Path $Root "CMakeLists.txt")
    $artifactDependencySets = @(
        [ordered]@{ artifact=$HostExe; sources=@((Get-Item (Join-Path $Root "host\fnvxr_openxr_pose_host.cpp"))) + $sharedBuildSources },
        [ordered]@{ artifact=$ProbeExe; sources=@((Get-Item (Join-Path $Root "tools\fnvxr_shared_state_probe.cpp"))) + $sharedBuildSources },
        [ordered]@{ artifact=$CommandExe; sources=@((Get-Item (Join-Path $Root "tools\fnvxr_command.cpp"))) + $sharedBuildSources },
        [ordered]@{ artifact=$InputExe; sources=@((Get-Item (Join-Path $Root "tools\fnvxr_input.cpp"))) + $sharedBuildSources },
        [ordered]@{ artifact=(Join-Path $BuildWin32Dir "$Configuration\nvse_fnvxr.dll"); sources=@(Get-ChildItem -LiteralPath (Join-Path $Root "plugin") -Recurse -File | Where-Object { $_.Extension -in @(".cpp", ".h", ".hpp", ".def") }) + $sharedBuildSources },
        [ordered]@{ artifact=(Join-Path $BuildWin32Dir "$Configuration\d3d9.dll"); sources=@((Get-Item (Join-Path $Root "renderhook\fnvxr_d3d9_proxy.cpp")),(Get-Item (Join-Path $Root "renderhook\fnvxr_stereo_math.h"))) + $sharedBuildSources },
        [ordered]@{ artifact=(Join-Path $BuildWin32Dir "$Configuration\dinput8.dll"); sources=@((Get-Item (Join-Path $Root "renderhook\fnvxr_dinput8_proxy.cpp"))) + $sharedBuildSources },
        [ordered]@{ artifact=(Join-Path $BuildWin32Dir "$Configuration\xinput1_3.dll"); sources=@((Get-Item (Join-Path $Root "renderhook\fnvxr_xinput_proxy.cpp"))) + $sharedBuildSources }
    )
    $staleArtifacts = @()
    foreach ($dependencySet in $artifactDependencySets) {
        $artifactInfo = Get-Item -LiteralPath $dependencySet.artifact
        $newestDependency = $dependencySet.sources |
            Sort-Object LastWriteTimeUtc -Descending |
            Select-Object -First 1
        if ($artifactInfo.LastWriteTimeUtc -lt $newestDependency.LastWriteTimeUtc) {
            $staleArtifacts += ("{0} (newer dependency: {1})" -f $artifactInfo.FullName,$newestDependency.FullName)
        }
    }
    if ($staleArtifacts.Count -gt 0) {
        throw "Artifact validation refused because target-specific dependencies are newer: $($staleArtifacts -join ', ')"
    }
}

if (-not (Test-Path -LiteralPath $HostExe)) {
    throw "Missing OpenXR pose host: $HostExe"
}

$OpenXrLoader = if ($ValidateOnly) {
    $loaderSource = Resolve-FnvxrOpenXrLoader -DebugLog $DebugLog
    $loaderDestination = Join-Path (Split-Path -Parent $HostExe) "openxr_loader.dll"
    if (-not (Test-Path -LiteralPath $loaderDestination)) {
        throw "Validate-only artifact check found no staged OpenXR loader: $loaderDestination"
    }
    if ((Get-FileHash -LiteralPath $loaderSource -Algorithm SHA256).Hash -ne
        (Get-FileHash -LiteralPath $loaderDestination -Algorithm SHA256).Hash) {
        throw "Validate-only artifact check found a stale OpenXR loader: $loaderDestination"
    }
    $loaderDestination
} else {
    Copy-FnvxrOpenXrLoader -HostExe $HostExe -DebugLog $DebugLog
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
        profile = "openxr-sidecar"
        validateOnly = [bool]$ValidateOnly
        stageOnly = [bool]$StageOnly
        gameRoot = $GameRoot
        runDir = $RunDir
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
    -Profile "openxr-sidecar" `
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
$StereoRuntimeMode = if ($StereoWorldActive) { "stereo-world" } else { "quad-2d" }
if ($StereoWorldActive) {
    $env:FNVXR_GAME_PLANE_MODE = "stereo3d"
    Set-FnvxrStereoWorldRuntimeEnvironment
    if ($NoTelemetryHammer) {
        $env:FNVXR_TELEMETRY_HAMMER = "0"
        $env:FNVXR_D3D9_TELEMETRY_HAMMER = "0"
    }
    Write-FnvxrCheckpoint -Path $DebugLog -Message ("telemetry hammer={0} d3d9Hammer={1}" -f $env:FNVXR_TELEMETRY_HAMMER, $env:FNVXR_D3D9_TELEMETRY_HAMMER)
    Write-FnvxrCheckpoint -Path $DebugLog -Message ("native head axis mode={0}" -f $env:FNVXR_D3D9_NATIVE_HEAD_AXIS_MODE)
    Write-FnvxrCheckpoint -Path $DebugLog -Message "source2d mode=stereo3d stereo world runtime enabled by explicit launch switch"
} else {
    Write-FnvxrCheckpoint -Path $DebugLog -Message "quad 2D runtime enabled; stereo world disabled by default"
}
if ($StereoWorldActive -and -not $DisableRetailRig) {
    # This is the retail first-person arm/weapon path, not the synthetic host
    # hand renderer. A VR launch that leaves it disabled cannot satisfy the
    # independent controller-to-gun requirement.
    $env:FNVXR_RETAIL_RIG_ENABLE = "1"
    $env:FNVXR_RETAIL_RIG_APPLY = $(if ($ApplyRetailRig) { "1" } else { "0" })
    $env:FNVXR_RETAIL_WEAPON_APPLY = $(if ($ApplyRetailRig) { "1" } else { "0" })
    Write-FnvxrCheckpoint -Path $DebugLog -Message ("retail rig enabled apply={0} source=explicit-only; exact render/animation scheduling is not yet proven" -f $env:FNVXR_RETAIL_RIG_APPLY)
} elseif ($EnableRetailRig -or $ApplyRetailRig) {
    $env:FNVXR_RETAIL_RIG_ENABLE = "1"
    $env:FNVXR_RETAIL_RIG_APPLY = $(if ($ApplyRetailRig) { "1" } else { "0" })
    $env:FNVXR_RETAIL_WEAPON_APPLY = $(if ($ApplyRetailRig) { "1" } else { "0" })
    Write-FnvxrCheckpoint -Path $DebugLog -Message ("retail rig enabled apply={0} solver=FABRIK postAnimation=0x0088882F" -f $env:FNVXR_RETAIL_RIG_APPLY)
} elseif ($DisableRetailRig) {
    Write-FnvxrCheckpoint -Path $DebugLog -Message "retail rig explicitly disabled"
}
if ($EnableD3D9ShaderStereo) {
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
        Write-FnvxrCheckpoint -Path $DebugLog -Message ("stereo producer shader vertex allowlist enabled hashes={0}" -f $env:FNVXR_D3D9_SHADER_STEREO_ALLOW_VERTEX_HASHES)
    }
    if ($D3D9ShaderSanityOffset -ne 0) {
        $env:FNVXR_D3D9_SHADER_SANITY_OFFSET = $D3D9ShaderSanityOffset.ToString([System.Globalization.CultureInfo]::InvariantCulture)
        $env:FNVXR_D3D9_SHADER_SANITY_SLOT = $D3D9ShaderSanitySlot
        $env:FNVXR_D3D9_SHADER_SANITY_START_REGISTER = "0"
        Write-FnvxrCheckpoint -Path $DebugLog -Message ("stereo producer shader sanity offset enabled slot={0} offset={1}" -f $env:FNVXR_D3D9_SHADER_SANITY_SLOT, $env:FNVXR_D3D9_SHADER_SANITY_OFFSET)
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
    Write-FnvxrCheckpoint -Path $DebugLog -Message "stereo producer verified per-hash WVP contract path enabled; generic shader scanner disabled"
}
if ($StereoProducerProofOnly -or $DumpD3D9ShaderBytecode) {
    $env:FNVXR_D3D9_DUMP_SHADER_BYTECODE = "1"
    Write-FnvxrCheckpoint -Path $DebugLog -Message "D3D9 shader bytecode capture enabled by explicit diagnostic launch switch"
}
if ($StereoProducerProofOnly) {
    $env:FNVXR_GAME_FULLSCREEN_IN_XR = "0"
    $env:FNVXR_STEREO_FALLBACK_MONO_FULLSCREEN = "0"
    $env:FNVXR_SHOW_GAME_PLANE = "1"
    $env:FNVXR_SHOW_GAME_PLANE_IN_GAME = "1"
    $env:FNVXR_SHOW_GAME_PLANE_ON_STEREO_LOSS = "1"
    Write-FnvxrCheckpoint -Path $DebugLog -Message "stereo producer proof-only mode; OpenXR fullscreen stereo handoff disabled and gameplay fallback plane forced visible"
}
if ($NoWorldProofWatchdog) {
    $WorldProofTimeoutSeconds = 0
    Write-FnvxrCheckpoint -Path $DebugLog -Message "stereo world proof watchdog disabled for live headset debugging"
} elseif ($WorldProofTimeoutSeconds -gt 30) {
    Write-FnvxrCheckpoint -Path $DebugLog -Message ("stereo world proof timeout clamped from {0}s to 30s" -f $WorldProofTimeoutSeconds)
    $WorldProofTimeoutSeconds = 30
}
Write-FnvxrCheckpoint -Path $DebugLog -Message ("profile=openxr-sidecar environment set stereoRuntime={0}" -f $StereoRuntimeMode)
if ($WorldProofTimeoutSeconds -gt 0 -and -not (Test-Path -LiteralPath $ProbeExe)) {
    Write-FnvxrCheckpoint -Path $DebugLog -Message ("shared-state probe missing; continuing without proof watchdog: {0}" -f $ProbeExe)
    $WorldProofTimeoutSeconds = 0
}

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

$hostOut = Join-Path $RunDir "fnvxr_openxr_pose_host.out.log"
$hostErr = Join-Path $RunDir "fnvxr_openxr_pose_host.err.log"
$initialManifest = [ordered]@{
    startedAt = $StartedAt
    profile = "openxr-sidecar"
    role = "openxr-host-retail-fnv-sidecar"
    acceptanceProfile = $AcceptanceProfile
    acceptanceState = "pending"
    accepted = $false
    failed = $false
    runDir = $RunDir
    gameRoot = $GameRoot
    enableRetailRig = [bool]($env:FNVXR_RETAIL_RIG_ENABLE -eq "1")
    applyRetailRig = [bool]($env:FNVXR_RETAIL_RIG_APPLY -eq "1")
    stereoPipeline = [ordered]@{
        shaderWvpReplay = $env:FNVXR_D3D9_SHADER_WVP_REPLAY
        shaderWvpContracts = $env:FNVXR_D3D9_SHADER_WVP_CONTRACTS
        syntheticBodyRig = $env:FNVXR_SHOW_BODY_RIG
        syntheticHandFingers = $env:FNVXR_SHOW_HAND_FINGERS
    }
}
Write-FnvxrJsonAtomic -Value $initialManifest -Path $ManifestPath -Depth 8
$hostProcess = Start-Process `
    -FilePath $HostExe `
    -ArgumentList $Frames `
    -WorkingDirectory $Root `
    -RedirectStandardOutput $hostOut `
    -RedirectStandardError $hostErr `
    -PassThru `
    -WindowStyle Hidden
Write-FnvxrCheckpoint -Path $DebugLog -Message ("host started pid={0}" -f $hostProcess.Id)

$hostReady = Wait-FnvxrLogPattern `
    -Path $hostOut `
    -Pattern "fnvxrHostBridgeReady xrSessionCreated=1 sharedMappingsReady=1" `
    -TimeoutSeconds $HostReadyTimeoutSeconds `
    -Process $hostProcess
if (-not $hostReady) {
    $hostProcess.Refresh()
    if ($hostProcess.HasExited) {
        $hostProcess.WaitForExit()
        $hostErrTail = ""
        if (Test-Path -LiteralPath $hostErr) {
            $hostErrTail = ((Get-Content -LiteralPath $hostErr -Tail 20) -join " ").Trim()
        }
        $hostOutTail = ""
        if (Test-Path -LiteralPath $hostOut) {
            $hostOutTail = ((Get-Content -LiteralPath $hostOut -Tail 20) -join " ").Trim()
        }
        throw "OpenXR host exited before creating its session/bridge. Retail sidecar was not launched. hostExit=$($hostProcess.ExitCode) openXrLoader=$OpenXrLoader stdout='$hostOutTail' stderr='$hostErrTail'"
    }
    throw "OpenXR host did not create its session/bridge within $HostReadyTimeoutSeconds seconds. Retail sidecar was not launched."
}

$poseReady = $false
Write-FnvxrCheckpoint -Path $DebugLog -Message "host OpenXR session/shared bridge ready; retail launch permitted"

$falloutHwnd = [IntPtr]::Zero
$retailFrameProbe = $null
$activationReachApplied = $false
$testLoadout = [ordered]@{
    requested = [bool]$ApplyTestLoadout
    applied = $false
    skipped = $null
}
$worldEntryProof = $null
$worldProof = $null
$worldProofConsecutive = 0
$worldProofSamples = @()
$retailRigProof = $null
$retailRigProofRequired = [bool](
    $StereoWorldActive -and
    -not $StereoProducerProofOnly -and
    $env:FNVXR_RETAIL_RIG_APPLY -eq "1")
$retailRigTelemetryPath = Join-Path $GameRoot "Data\NVSE\Plugins\fnvxr_input_telemetry.log"
$forcedMenuAction = $false
$resolvedAutomatedSaveName = $null
$automatedLoadProof = $null
$automatedLoadProofConsecutive = 0
$automatedLoadProofSamples = @()

if (-not $NoRetail) {
    $fallout = Start-Process -FilePath $NvseLoader -WorkingDirectory $GameRoot -PassThru
    $falloutHwnd = Find-FnvxrFalloutWindow -Process $fallout -Focus ([bool]$FocusRetailWindow)
    $falloutProcess = Get-Process FalloutNV -ErrorAction SilentlyContinue | Sort-Object StartTime -Descending | Select-Object -First 1
    $falloutPid = if ($falloutProcess) { $falloutProcess.Id } else { $fallout.Id }
    Write-FnvxrCheckpoint -Path $DebugLog -Message ("retail sidecar started pid={0} launcherPid={1} hwnd={2}" -f $falloutPid, $fallout.Id, $falloutHwnd)

    if (-not $NoSidecarExitWatcher) {
        $watcher = Start-Process `
            -FilePath "powershell.exe" `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy", "Bypass",
                "-File", (Join-Path $PSScriptRoot "watch-retail-exit.ps1"),
                "-FalloutPid", $falloutPid,
                "-HostPid", $hostProcess.Id,
                "-RunDir", $RunDir,
                "-ExpectedProfile", $AcceptanceProfile,
                "-LiveAnalyzer", (Join-Path $PSScriptRoot "analyze-fnvxr-live-run.ps1"),
                "-StereoCaptureScript", (Join-Path $PSScriptRoot "capture-scene-cache.ps1"),
                "-CommandExe", $CommandExe,
                "-SaveName", "FNVXR_HostExitRecovery",
                "-SaveQuitOnHostExit"
            ) `
            -PassThru `
            -WindowStyle Hidden
        Write-FnvxrCheckpoint -Path $DebugLog -Message ("sidecar exit watcher started pid={0}" -f $watcher.Id)
    }

    $probeRoot = Join-Path $RunDir "retail-video-probe"
    try {
        $retailFrameProbe = & (Join-Path $PSScriptRoot "probe-retail-video-frame.ps1") `
            -OutputRoot $probeRoot `
            -TimeoutSeconds $RetailReadyTimeoutSeconds 2>&1
    } catch {
        throw "Retail D3D9 shared video frame was not proven within $RetailReadyTimeoutSeconds seconds."
    }
    Write-FnvxrCheckpoint -Path $DebugLog -Message "retail shared video proof observed"
    $poseReady = Wait-FnvxrLogPattern `
        -Path $hostOut `
        -Pattern "poseFrame=" `
        -TimeoutSeconds 10 `
        -Process $hostProcess
    if ($poseReady) {
        Write-FnvxrCheckpoint -Path $DebugLog -Message "OpenXR pose stream proof observed"
    } else {
        Write-FnvxrCheckpoint -Path $DebugLog -Message "OpenXR pose stream pending; headset may be idle, retail remains live with neutral VR input"
    }
    if ($AutomateContinue) {
        Write-FnvxrCheckpoint -Path $DebugLog -Message (
            "waiting for Fallout Start Menu before unattended direct save load; timeout={0}s" -f
            $AutomatedMenuTimeoutSeconds)
        $menuDeadline = (Get-Date).AddSeconds($AutomatedMenuTimeoutSeconds)
        do {
            $menuProbeOutput = & $ProbeExe `
                --require-runtime `
                --require-video `
                --require-advancing `
                --sample-delay-ms 100 2>&1
            if ($LASTEXITCODE -eq 0) {
                $menuProbe = $null
                try {
                    $menuProbe = ($menuProbeOutput -join "`n") | ConvertFrom-Json
                } catch {
                    Write-FnvxrCheckpoint -Path $DebugLog -Message (
                        "unattended load ignored one malformed shared-state sample")
                }
                if ($null -ne $menuProbe) {
                    $startMenuVisible = [bool](
                        $menuProbe.runtime.phase -eq 1 -and
                        (([uint32]$menuProbe.runtime.menuBits -band 2) -ne 0) -and
                        $menuProbe.runtime.uiInputAllowed)
                    if ($startMenuVisible) {
                        $saveRoot = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "My Games\FalloutNV\Saves"
                        $savePath = Join-Path $saveRoot ($AutomatedSaveName + ".fos")
                        if (-not (Test-Path -LiteralPath $savePath -PathType Leaf)) {
                            throw "Automated save '$AutomatedSaveName' was not found at '$savePath'. Direct load was not submitted."
                        }
                        $resolvedAutomatedSaveName = $AutomatedSaveName
                        $loadSubmitted = Invoke-FnvxrRuntimeConsoleCommand `
                            -CommandExe $CommandExe `
                            -Line ("load {0}" -f $resolvedAutomatedSaveName) `
                            -DebugLog $DebugLog `
                            -Label "unattended direct save load" `
                            -WaitMs 10000
                        if (-not $loadSubmitted) {
                            throw "Direct load of '$resolvedAutomatedSaveName' was rejected by the runtime console bridge."
                        }
                        Write-FnvxrCheckpoint -Path $DebugLog -Message (
                            "unattended direct save load submitted save='{0}'; waiting for gameplay/camera proof" -f
                            $resolvedAutomatedSaveName)

                        $loadDeadline = (Get-Date).AddSeconds([Math]::Min([Math]::Max($AutomatedMenuTimeoutSeconds, 30), 90))
                        $closeAllMenusSubmitted = $false
                        do {
                            $loadProbeOutput = & $ProbeExe `
                                --require-runtime `
                                --require-camera `
                                --require-advancing `
                                --sample-delay-ms 250 2>&1
                            if ($LASTEXITCODE -eq 0) {
                                try {
                                    $loadProbe = ($loadProbeOutput -join "`n") | ConvertFrom-Json
                                    if ($loadProbe.runtime.phase -eq 3 -and $loadProbe.runtime.cameraActive) {
                                        if (-not $closeAllMenusSubmitted) {
                                            $closeAllMenusSubmitted = Invoke-FnvxrRuntimeConsoleCommand `
                                                -CommandExe $CommandExe `
                                                -Line "CloseAllMenus" `
                                                -DebugLog $DebugLog `
                                                -Label "unattended post-load menu close" `
                                                -WaitMs 10000
                                            if (-not $closeAllMenusSubmitted) {
                                                throw "Post-load CloseAllMenus was rejected by the runtime console bridge."
                                            }
                                            $automatedLoadProofConsecutive = 0
                                        } else {
                                            $automatedLoadProofConsecutive++
                                            $automatedLoadProofSamples += [ordered]@{
                                                frame = $loadProbe.runtime.frame
                                                phase = $loadProbe.runtime.phase
                                                menuBits = $loadProbe.runtime.menuBits
                                                cameraActive = $loadProbe.runtime.cameraActive
                                            }
                                            if ($automatedLoadProofSamples.Count -gt 24) {
                                                $automatedLoadProofSamples = @($automatedLoadProofSamples | Select-Object -Last 24)
                                            }
                                            if ($automatedLoadProofConsecutive -ge 12) {
                                                $automatedLoadProof = $loadProbe
                                                $forcedMenuAction = $true
                                                break
                                            }
                                        }
                                    } else {
                                        $automatedLoadProofConsecutive = 0
                                    }
                                } catch {
                                    if ($_.Exception.Message -eq "Post-load CloseAllMenus was rejected by the runtime console bridge.") {
                                        throw
                                    }
                                    $automatedLoadProofConsecutive = 0
                                    Write-FnvxrCheckpoint -Path $DebugLog -Message "unattended load ignored one malformed gameplay proof sample"
                                }
                            }
                            Start-Sleep -Milliseconds 250
                        } while ((Get-Date) -lt $loadDeadline)
                        if (-not $forcedMenuAction) {
                            throw "Direct load of '$resolvedAutomatedSaveName' did not reach proven gameplay with an active world camera."
                        }
                        Write-FnvxrCheckpoint -Path $DebugLog -Message (
                            "unattended direct save load proven phase=3 cameraActive=1 stableSamples={0} save='{1}'" -f
                            $automatedLoadProofConsecutive,
                            $resolvedAutomatedSaveName)
                        break
                    }
                }
            }
            Start-Sleep -Milliseconds 500
        } while ((Get-Date) -lt $menuDeadline)
        if (-not $forcedMenuAction) {
            throw "Fallout Start Menu was not proven within $AutomatedMenuTimeoutSeconds seconds; unattended direct save load was not sent."
        }
    }
    $activationReachApplied = Invoke-FnvxrRuntimeConsoleCommand `
        -CommandExe $CommandExe `
        -Line "setgs iActivatePickLength 260" `
        -DebugLog $DebugLog `
        -Label "activation reach"
    if ($FocusRetailWindow -and $falloutHwnd -ne [IntPtr]::Zero) {
        Set-FnvxrWindowForeground -Handle $falloutHwnd -DebugLog $DebugLog | Out-Null
    }

    if ($ApplyTestLoadout) {
        Write-FnvxrCheckpoint -Path $DebugLog -Message ("waiting for gameplay/world camera before deterministic test loadout; timeout={0}s" -f $TestLoadoutTimeoutSeconds)
        $loadoutDeadline = (Get-Date).AddSeconds($TestLoadoutTimeoutSeconds)
        $loadoutProof = $null
        do {
            $loadoutProbeOutput = & $ProbeExe `
                --require-runtime `
                --require-camera `
                --require-advancing `
                --sample-delay-ms 250 2>&1
            if ($LASTEXITCODE -eq 0) {
                $loadoutProof = $loadoutProbeOutput
                Write-FnvxrCheckpoint -Path $DebugLog -Message "gameplay/world camera proof observed; applying deterministic test loadout"
                break
            }
            Start-Sleep -Seconds 1
        } while ((Get-Date) -lt $loadoutDeadline)

        if ($null -ne $loadoutProof) {
            $testLoadout = Invoke-FnvxrTestLoadout `
                -CommandExe $CommandExe `
                -DebugLog $DebugLog
            $testLoadout["worldProofObserved"] = $true
            $testLoadout["worldProof"] = $loadoutProof
        } else {
            $reason = "gameplay/world camera not observed before deterministic loadout timeout"
            Write-FnvxrCheckpoint -Path $DebugLog -Message ("deterministic test loadout skipped reason='{0}' timeout={1}s" -f $reason, $TestLoadoutTimeoutSeconds)
            $testLoadout = [ordered]@{
                requested = $true
                applied = $false
                skipped = $reason
                timeoutSeconds = $TestLoadoutTimeoutSeconds
                worldProofObserved = $false
                slot2ReservedForAmmoSwap = $true
                combatSlots = @(5, 6, 7, 8)
                utilitySlots = @(1, 3, 4)
                commands = @()
            }
        }
    }

    if ($StereoWorldActive -and $WorldProofTimeoutSeconds -gt 0) {
        Write-FnvxrCheckpoint -Path $DebugLog -Message ("waiting for gameplay/world camera before stereo proof; timeout={0}s" -f $WorldEntryTimeoutSeconds)
        $entryDeadline = (Get-Date).AddSeconds($WorldEntryTimeoutSeconds)
        do {
            $entryProbeOutput = & $ProbeExe `
                --require-runtime `
                --require-camera `
                --require-pose `
                --require-advancing `
                --sample-delay-ms 250 2>&1
            if ($LASTEXITCODE -eq 0) {
                $worldEntryProof = $entryProbeOutput
                Write-FnvxrCheckpoint -Path $DebugLog -Message ("gameplay/world camera proof observed; starting stereo proof timeout {0}s" -f $WorldProofTimeoutSeconds)
                break
            }
            Start-Sleep -Seconds 1
        } while ((Get-Date) -lt $entryDeadline)

        if ($null -eq $worldEntryProof) {
            Write-FnvxrCheckpoint -Path $DebugLog -Message "stereo world entry proof timed out before gameplay/camera; runtime remains live and gameplay stereo stays closed"
        }

        Write-FnvxrCheckpoint -Path $DebugLog -Message (
            "requiring sustained stereo proof samples={0} retailRigProof={1}" -f
            $WorldProofStableSamples,
            [int]$retailRigProofRequired)
        $deadline = (Get-Date).AddSeconds($WorldProofTimeoutSeconds)
        do {
            $probeOutput = & $ProbeExe `
                --require-runtime `
                --require-stereo `
                --require-world-stereo `
                --require-pose `
                --require-advancing `
                --sample-delay-ms 250 2>&1
            if ($LASTEXITCODE -eq 0) {
                ++$worldProofConsecutive
                $worldProofSamples += [ordered]@{
                    observedAt = (Get-Date).ToString("o")
                    output = ($probeOutput -join "`n")
                }
                if ($worldProofSamples.Count -gt $WorldProofStableSamples) {
                    $worldProofSamples = @($worldProofSamples | Select-Object -Last $WorldProofStableSamples)
                }

                $retailRigReady = -not $retailRigProofRequired
                if ($retailRigProofRequired -and (Test-Path -LiteralPath $retailRigTelemetryPath)) {
                    $rigDiscovery = Select-String `
                        -LiteralPath $retailRigTelemetryPath `
                        -Pattern 'retailRig discovery[^\r\n]*complete=1' | Select-Object -Last 1
                    $rigSolve = Select-String `
                        -LiteralPath $retailRigTelemetryPath `
                        -Pattern 'retailRig solve[^\r\n]*apply=1 leftSolved=1 rightSolved=1 weaponAligned=1 weaponApply=1' | Select-Object -Last 1
                    $rigApplied = Select-String `
                        -LiteralPath $retailRigTelemetryPath `
                        -Pattern '"event":"fnvxrRigIndependence"[^\r\n]*"apply":true[^\r\n]*"rightSolved":true[^\r\n]*"weaponAligned":true[^\r\n]*"weaponWriteApplied":true' | Select-Object -Last 1
                    if ($rigDiscovery -and $rigSolve -and $rigApplied) {
                        $retailRigReady = $true
                        $retailRigProof = [ordered]@{
                            discovery = $rigDiscovery.Line
                            solve = $rigSolve.Line
                            applied = $rigApplied.Line
                        }
                    }
                }

                Write-FnvxrCheckpoint -Path $DebugLog -Message (
                    "stereo proof progress consecutive={0}/{1} retailRigReady={2}" -f
                    $worldProofConsecutive,
                    $WorldProofStableSamples,
                    [int]$retailRigReady)
                if ($worldProofConsecutive -ge $WorldProofStableSamples -and $retailRigReady) {
                    $worldProof = $worldProofSamples
                    Write-FnvxrCheckpoint -Path $DebugLog -Message (
                        "retail world stereo proof observed sustainedSamples={0} retailRigReady={1}" -f
                        $worldProofConsecutive,
                        [int]$retailRigReady)
                    break
                }
            } else {
                if ($worldProofConsecutive -gt 0) {
                    Write-FnvxrCheckpoint -Path $DebugLog -Message (
                        "stereo proof continuity lost after {0}/{1} samples" -f
                        $worldProofConsecutive,
                        $WorldProofStableSamples)
                }
                $worldProofConsecutive = 0
                $worldProofSamples = @()
            }
            Start-Sleep -Seconds 1
        } while ((Get-Date) -lt $deadline)

        if ($null -eq $worldProof) {
            Write-FnvxrCheckpoint -Path $DebugLog -Message "stereo world proof not observed before timeout; runtime remains live but gameplay stereo fails closed without 2D fallback"
        }
    }
}

$manifest = [ordered]@{
    startedAt = $StartedAt
    profile = "openxr-sidecar"
    role = "openxr-host-retail-fnv-sidecar"
    acceptanceProfile = $AcceptanceProfile
    acceptanceState = "pending"
    accepted = $false
    failed = $false
    runDir = $RunDir
    gameRoot = $GameRoot
    hostPid = $hostProcess.Id
    hostOut = $hostOut
    hostErr = $hostErr
    hostReady = $hostReady
    poseReady = $poseReady
    stereoRuntime = $StereoRuntimeMode
    stereoPipeline = [ordered]@{
        singleTraversalReplay = $env:FNVXR_D3D9_NATIVE_SINGLE_TRAVERSAL_REPLAY
        nativeMultipass = $env:FNVXR_D3D9_NATIVE_MULTIPASS
        drawReplay = $env:FNVXR_D3D9_STEREO_REPLAY
        useSharedCameraView = $env:FNVXR_D3D9_USE_SHARED_CAMERA_VIEW
        applyHmdPoseInReplay = $env:FNVXR_D3D9_APPLY_HMD_POSE
        asymmetricFov = $env:FNVXR_D3D9_NATIVE_ASYMMETRIC_FOV
        centerCameraMaxDelta = $env:FNVXR_D3D9_NATIVE_CENTER_CAMERA_MAX_DELTA
        shaderStereoScanner = $env:FNVXR_D3D9_SHADER_STEREO
        shaderWvpReplay = $env:FNVXR_D3D9_SHADER_WVP_REPLAY
        shaderVerifiedVertexHashes = $env:FNVXR_D3D9_SHADER_STEREO_ALLOW_VERTEX_HASHES
        shaderWvpContracts = $env:FNVXR_D3D9_SHADER_WVP_CONTRACTS
        shaderAllowUnverifiedPatches = $env:FNVXR_D3D9_SHADER_ALLOW_UNVERIFIED_PATCHES
        shaderBytecodeCapture = $env:FNVXR_D3D9_DUMP_SHADER_BYTECODE
        skippedShaderPairs = $env:FNVXR_D3D9_STEREO_SKIP_SHADER_HASH_PAIRS
        visualCoverageGate = $env:FNVXR_D3D9_STEREO_VISUAL_COVERAGE_GATE
        visualStableFrames = $env:FNVXR_D3D9_STEREO_VISUAL_STABLE_FRAMES
        visualMinActiveFraction = $env:FNVXR_D3D9_STEREO_MIN_ACTIVE_FRACTION
        syntheticBodyRig = $env:FNVXR_SHOW_BODY_RIG
        syntheticHandFingers = $env:FNVXR_SHOW_HAND_FINGERS
        retailRigEnabled = $env:FNVXR_RETAIL_RIG_ENABLE
        retailRigApply = $env:FNVXR_RETAIL_RIG_APPLY
        retailWeaponApply = $env:FNVXR_RETAIL_WEAPON_APPLY
        requireCoherentProducer = $env:FNVXR_REQUIRE_NATIVE_STEREO
        allow2dFallback = $env:FNVXR_ALLOW_STEREO_WORLD_2D_FALLBACK
        show2dOnStereoLoss = $env:FNVXR_SHOW_GAME_PLANE_ON_STEREO_LOSS
        stableHandoffFrames = $env:FNVXR_STEREO_STABLE_HANDOFF_FRAMES
        retainProducerFrame = $env:FNVXR_D3D9_STEREO_RETAIN_LAST_VALID_ON_INVALID
        retainHostFrame = $env:FNVXR_STEREO_RETAIN_LAST_VALID_ON_REJECT
        telemetryHammer = $env:FNVXR_TELEMETRY_HAMMER
        d3d9TelemetryHammer = $env:FNVXR_D3D9_TELEMETRY_HAMMER
    }
    sidecarExitWatcher = -not [bool]$NoSidecarExitWatcher
    retailLaunched = -not [bool]$NoRetail
    falloutPid = $falloutPid
    falloutHwnd = $falloutHwnd.ToString()
    retailFrameProof = $retailFrameProbe
    worldEntryTimeoutSeconds = $WorldEntryTimeoutSeconds
    worldEntryProofObserved = $null -ne $worldEntryProof
    worldEntryProof = $worldEntryProof
    worldProofRequestedSeconds = $WorldProofTimeoutSeconds
    worldProofRequiredStableSamples = $WorldProofStableSamples
    worldProofConsecutiveSamples = $worldProofConsecutive
    worldProofWatchdogDisabled = [bool]$NoWorldProofWatchdog
    enableD3D9ShaderStereo = [bool]$EnableD3D9ShaderStereo
    enableRetailRig = [bool]($env:FNVXR_RETAIL_RIG_ENABLE -eq "1")
    applyRetailRig = [bool]($env:FNVXR_RETAIL_RIG_APPLY -eq "1")
    stereoProducerProofOnly = [bool]$StereoProducerProofOnly
    enableCompositeShield = [bool]$EnableCompositeShield
    enableAtlasShield = [bool]$EnableAtlasShield
    enablePeripheralShield = [bool]$EnablePeripheralShield
    worldProofObserved = $null -ne $worldProof
    worldProof = $worldProof
    retailRigProofRequired = $retailRigProofRequired
    retailRigProofObserved = $null -ne $retailRigProof
    retailRigProof = $retailRigProof
    activationReach = [ordered]@{
        gameSetting = "iActivatePickLength"
        value = 260
        applied = [bool]$activationReachApplied
    }
    testLoadoutTimeoutSeconds = $TestLoadoutTimeoutSeconds
    testLoadout = $testLoadout
    uiGrid = "${UiWidth}x${UiHeight}"
    source2DPreset = $Source2DPreset
    fov = [ordered]@{
        default = $DefaultFov
        firstPerson = $FirstPersonFov
        pipboy = $PipboyFov
    }
    retailBackbuffer = "${GameBackbufferWidth}x${GameBackbufferHeight}"
    hostGameTexture = "${HostGameTextureWidth}x${HostGameTextureHeight}"
    forcedMenuAction = $forcedMenuAction
    automatedSaveName = $resolvedAutomatedSaveName
    automatedLoadProof = $automatedLoadProof
    automatedLoadProofConsecutiveSamples = $automatedLoadProofConsecutive
    automatedLoadProofSamples = $automatedLoadProofSamples
    staged = $staged
}
Write-FnvxrJsonAtomic -Value $manifest -Path $ManifestPath -Depth 8
$manifest | ConvertTo-Json -Depth 8
if ($StereoWorldActive -and $WorldProofTimeoutSeconds -gt 0 -and $null -eq $worldProof) {
    throw "Stereo world proof failed. The launch is rejected and all started runtime processes will be stopped; evidence is preserved in $ManifestPath"
}
