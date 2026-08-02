param(
    [ValidateSet("Release")][string]$Configuration = "Release",
    [string]$GameRoot = "",
    [ValidateRange(5, 300)][int]$ReadyTimeoutSeconds = 75,
    [switch]$UseAttestedBuild,
    [switch]$ValidateOnly
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

# This is an intentionally non-gameplay TTW gate.  It starts an owned game
# process only to observe the native Start Menu after loading the exact core
# TTW profile.  It has no command mailbox, no save path, no OpenXR host,
# simulator, D3D proxy, input, camera, rig, renderer, or weapon authority.

. (Join-Path $PSScriptRoot "fnvxr-product-common.ps1")

if (Test-FnvxrProductProcessElevated) {
    throw "TTW baseline runner refuses to run elevated."
}
if ($ValidateOnly -and -not $UseAttestedBuild) {
    throw "-ValidateOnly is read-only and requires -UseAttestedBuild."
}

$root = Get-FnvxrProductRoot
if ([string]::IsNullOrWhiteSpace($GameRoot)) {
    $GameRoot = Join-Path $root "local\ttw-retail-sandbox"
}
$sandboxRoot = [System.IO.Path]::GetFullPath($GameRoot)
$allowedSandboxParent = [System.IO.Path]::GetFullPath((Join-Path $root "local"))
$allowedPrefix = $allowedSandboxParent.TrimEnd('\', '/') +
    [System.IO.Path]::DirectorySeparatorChar
if (-not $sandboxRoot.StartsWith(
        $allowedPrefix,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "TTW baseline GameRoot must be an isolated workspace sandbox below: $allowedSandboxParent"
}

$attestationPath = Join-Path $root "local\product-build\fnvxr-product-$Configuration.json"
if ($UseAttestedBuild) {
    $attestation = Assert-FnvxrProductBuildAttestation `
        -Path $attestationPath `
        -Root $root `
        -Configuration $Configuration
} else {
    & (Join-Path $PSScriptRoot "build-fnvxr-product.ps1") `
        -Configuration $Configuration | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "TTW baseline build attestation failed with exit code $LASTEXITCODE."
    }
    $attestation = Assert-FnvxrProductBuildAttestation `
        -Path $attestationPath `
        -Root $root `
        -Configuration $Configuration
}

$game = Assert-FnvxrProductGameRoot -GameRoot $sandboxRoot
Assert-FnvxrProductTtwBaselinePluginData -GameRoot $game.root
$sandboxManifestPath = Join-Path $game.root "fnvxr-ttw-sandbox-manifest.json"
if (-not (Test-Path -LiteralPath $sandboxManifestPath -PathType Leaf)) {
    throw "TTW baseline requires the workspace sandbox manifest: $sandboxManifestPath"
}
$sandboxManifest = Get-Content -LiteralPath $sandboxManifestPath -Raw | ConvertFrom-Json
if ($sandboxManifest.plan.schema -cne "fnvxr-ttw-retail-sandbox-v1" -or
    $sandboxManifest.plan.profile.name -cne "ttw-core-baseline-v1") {
    throw "TTW baseline sandbox manifest is not the exact core profile: $sandboxManifestPath"
}
$expectedPlugins = @(Get-FnvxrProductTtwBaselinePluginNames)
$manifestPlugins = @($sandboxManifest.plan.profile.plugins)
if (($manifestPlugins -join "|") -cne ($expectedPlugins -join "|")) {
    throw "TTW baseline sandbox manifest plugin order differs from the exact core profile."
}

$stagePlan = @(Get-FnvxrProductRetailFixtureStagePlan `
    -Root $root `
    -Configuration $Configuration `
    -GameRoot $game.root)
if ($stagePlan.Count -ne 1 -or $stagePlan[0].key -cne "x86/nvse_fnvxr.dll") {
    throw "TTW baseline runner must stage only the xNVSE observation plugin."
}
$x64Output = Join-Path $root "build-product-x64\$Configuration"
$probePath = Join-Path $x64Output "fnvxr_shared_state_probe.exe"
if (-not (Test-Path -LiteralPath $probePath -PathType Leaf)) {
    throw "Attested TTW baseline runtime probe is missing: $probePath"
}

$profilePath = Get-FnvxrProductRetailVisualTrialPluginsPath
$plan = [ordered]@{
    schema = "fnvxr-ttw-baseline-v1"
    scope = "owned isolated TTW core Start Menu observation only; no console, save, OpenXR, simulator, D3D, input, camera, rig, renderer, weapon, or user-game-root mutation"
    gameRoot = $game.root
    sandboxManifest = $sandboxManifestPath
    plugins = $expectedPlugins
    profilePath = $profilePath
    stagedArtifacts = @($stagePlan | ForEach-Object { $_.key })
    attestation = [ordered]@{
        path = $attestationPath
        sourceSha256 = $attestation.source.sha256
        artifactSha256 = $attestation.artifacts.sha256
        testCount = $attestation.tests.count
    }
}
if ($ValidateOnly) {
    $plan | ConvertTo-Json -Depth 10
    return
}

$existingRuntime = @(
    Get-Process -Name FalloutNV,nvse_loader,fnvxr_openxr_pose_host `
        -ErrorAction SilentlyContinue | Select-Object ProcessName,Id,StartTime)
if ($existingRuntime.Count -ne 0) {
    throw "A pre-existing runtime is present; refusing to attach to, control, or stop it."
}

$runId = "{0}-{1}" -f [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss-fff"),
    [Guid]::NewGuid().ToString("N").Substring(0, 12)
$runDirectory = Join-Path $root "local\ttw-baseline-runs\$runId"
$backupRoot = Join-Path $runDirectory "backup"
New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
$manifestPath = Join-Path $runDirectory "ttw-baseline-manifest.json"
$launcherLog = Join-Path $runDirectory "baseline-supervisor.log"
$probeLog = Join-Path $runDirectory "runtime-probe.log"

function Write-TtwBaselineLog {
    param([Parameter(Mandatory = $true)][string]$Message)
    Add-Content -LiteralPath $launcherLog -Encoding UTF8 -Value (
        "{0} {1}" -f [DateTime]::UtcNow.ToString("o"), $Message)
}

$previousFnvxrEnvironment = @{}
Get-ChildItem Env: | Where-Object { $_.Name -like "FNVXR_*" } | ForEach-Object {
    $previousFnvxrEnvironment[$_.Name] = $_.Value
}
$isolatedRuntimeVariables = @(
    "XR_RUNTIME_JSON",
    "XR_API_LAYER_PATH",
    "XR_ENABLE_API_LAYERS",
    "OPENXR_SIMULATOR_HEADLESS",
    "OPENXR_SIMULATOR_DATA_DIR",
    "OPENXR_SIMULATOR_LOG_PATH")
$previousRuntimeEnvironment = @{}
foreach ($name in $isolatedRuntimeVariables) {
    $value = [Environment]::GetEnvironmentVariable($name, "Process")
    if ($null -ne $value) { $previousRuntimeEnvironment[$name] = $value }
}

$manifest = [ordered]@{
    schema = "fnvxr-ttw-baseline-run-v1"
    runId = $runId
    startedAtUtc = [DateTime]::UtcNow.ToString("o")
    state = "preflight"
    plan = $plan
    process = $null
    readiness = [ordered]@{
        exactPlugin = $false
        runtime = $false
        startMenu = $false
        ttwCoreProfile = $false
    }
    environment = [ordered]@{
        runProfile = "ttw-baseline-v1"
        bridgeDisabled = $true
        stereoWorldDisabled = $true
        openXrOrSimulatorVariablesCleared = $true
    }
    profiles = [ordered]@{
        ttwBaseline = $null
        after = $null
        restored = $false
    }
    staging = [ordered]@{
        records = @()
        restored = $false
    }
    loadedPlugin = $null
    startMenuEvidence = $null
    saveAction = "none"
    error = $null
    endedAtUtc = $null
    normalCompletion = $false
}
$staged = @()
$profileRecord = $null
$nvse = $null
$fallout = $null
$normalCompletion = $false

try {
    $manifest.state = "staging-temporary-ttw-core-profile"
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
    Write-TtwBaselineLog "staging temporary exact TTW core plugins.txt profile; no save command exists in this runner"
    $profileRecord = Install-FnvxrProductTtwBaselinePluginProfile `
        -Path $profilePath `
        -BackupRoot $backupRoot `
        -RunId $runId `
        -GameRoot $game.root
    $manifest.profiles.ttwBaseline = $profileRecord

    foreach ($name in $isolatedRuntimeVariables) {
        Remove-Item -LiteralPath ("Env:{0}" -f $name) -ErrorAction SilentlyContinue
    }
    $environment = [ordered]@{
        FNVXR_RUN_PROFILE = "ttw-baseline-v1"
        FNVXR_HOST_MODE = "ttw-baseline"
        FNVXR_RUN_ID = $runId
        FNVXR_RUN_LOG_DIR = $runDirectory
        FNVXR_SESSION_READY_TIMEOUT_SECONDS = [string]$ReadyTimeoutSeconds
        FNVXR_DISABLE_BRIDGE = "1"
        FNVXR_DISABLE_STEREO_WORLD = "1"
        FNVXR_ENABLE_ENGINE_CENTER_STEREO = "0"
        FNVXR_ENABLE_LEGACY_IMAGE_DIAGNOSTICS = "0"
        FNVXR_ALLOW_STEREO_WORLD_2D_FALLBACK = "0"
        FNVXR_SHOW_GAME_PLANE_ON_STEREO_LOSS = "0"
        FNVXR_STEREO_FALLBACK_MONO_FULLSCREEN = "0"
        FNVXR_TELEMETRY_HAMMER = "0"
        FNVXR_D3D9_TELEMETRY_HAMMER = "0"
    }
    Set-FnvxrProductMinimalEnvironment -Environment $environment

    $manifest.state = "staging-owned-xnvse-observation-plugin-only"
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
    $staged = @(Install-FnvxrProductArtifactSet `
        -Plan $stagePlan `
        -BackupRoot $backupRoot `
        -RunId $runId)
    $manifest.staging.records = $staged

    $manifest.state = "starting-owned-ttw-baseline"
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
    $nvse = Start-Process -FilePath $game.nvseLoader.path `
        -WorkingDirectory $game.root `
        -WindowStyle Hidden `
        -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds($ReadyTimeoutSeconds)
    do {
        $fallout = Get-FnvxrProductExactFalloutProcess -ExpectedPath $game.fallout.path
        if ($fallout) { break }
        $nvse.Refresh()
        if ($nvse.HasExited) {
            throw "The owned NVSE loader exited before FalloutNV.exe started (exit code $($nvse.ExitCode))."
        }
        Start-Sleep -Milliseconds 200
    } while ([DateTime]::UtcNow -lt $deadline)
    if (-not $fallout) {
        throw "Timed out waiting for the owned isolated TTW FalloutNV.exe process."
    }
    $manifest.process = [ordered]@{
        falloutProcessId = $fallout.Id
        falloutPath = $game.fallout.path
        nvseLoaderProcessId = $nvse.Id
    }

    $pluginRecord = $staged[0]
    $manifest.loadedPlugin = Wait-FnvxrProductExactLoadedModule `
        -Process $fallout `
        -ExpectedPath $pluginRecord.destination.path `
        -ExpectedSha256 $pluginRecord.destination.sha256 `
        -TimeoutSeconds ([Math]::Min($ReadyTimeoutSeconds, 30))
    $manifest.readiness.exactPlugin = $true
    Wait-FnvxrProductProbeReady `
        -ProbePath $probePath `
        -Arguments @("--require-runtime", "--require-advancing", "--sample-delay-ms", "100") `
        -RequiredProcess $fallout `
        -TimeoutSeconds $ReadyTimeoutSeconds `
        -LogPath $probeLog `
        -Description "TTW baseline runtime publication with every bridge disabled"
    $manifest.readiness.runtime = $true

    $startMenu = Wait-FnvxrProductRetailFixtureStartMenu `
        -ProbePath $probePath `
        -RequiredProcess $fallout `
        -TimeoutSeconds $ReadyTimeoutSeconds `
        -LogPath $probeLog
    $manifest.readiness.startMenu = $true
    $manifest.readiness.ttwCoreProfile = $true
    $manifest.startMenuEvidence = [ordered]@{
        frame = [uint64]$startMenu.json.runtime.frame
        phase = [uint32]$startMenu.json.runtime.phase
        menuBits = [uint32]$startMenu.json.runtime.menuBits
        stable = [bool]$startMenu.json.runtime.stable
        usable = [bool]$startMenu.json.runtime.usable
        cameraActive = [bool]$startMenu.json.runtime.cameraActive
    }
    $manifest.state = "ttw-core-start-menu-ready"
    $normalCompletion = $true
    Write-TtwBaselineLog "TTW core Start Menu observed; no console command or save action was issued"
} catch {
    $manifest.error = $_.Exception.Message
    $manifest.state = "failed"
    Write-TtwBaselineLog ("ERROR " + $manifest.error)
} finally {
    Stop-FnvxrOwnedProcess -Process $fallout
    Stop-FnvxrOwnedProcess -Process $nvse

    if ($staged.Count -gt 0) {
        try {
            Restore-FnvxrProductArtifactSet -Records $staged
            $manifest.staging.restored = $true
        } catch {
            $manifest.error = if ($manifest.error) {
                "$($manifest.error) Stage restore also failed: $($_.Exception.Message)"
            } else {
                "Stage restore failed: $($_.Exception.Message)"
            }
            $manifest.state = "failed"
        }
    }
    if ($profileRecord) {
        try {
            $manifest.profiles.after = Restore-FnvxrProductTtwBaselinePluginProfile `
                -Record $profileRecord
            $manifest.profiles.restored = $true
        } catch {
            $manifest.error = if ($manifest.error) {
                "$($manifest.error) TTW profile restore also failed: $($_.Exception.Message)"
            } else {
                "TTW profile restore failed: $($_.Exception.Message)"
            }
            $manifest.state = "failed"
        }
    }

    Get-ChildItem Env: | Where-Object { $_.Name -like "FNVXR_*" } | ForEach-Object {
        Remove-Item -LiteralPath ("Env:{0}" -f $_.Name) -ErrorAction SilentlyContinue
    }
    foreach ($name in $previousFnvxrEnvironment.Keys) {
        Set-Item -LiteralPath ("Env:{0}" -f $name) -Value $previousFnvxrEnvironment[$name]
    }
    foreach ($name in $isolatedRuntimeVariables) {
        Remove-Item -LiteralPath ("Env:{0}" -f $name) -ErrorAction SilentlyContinue
    }
    foreach ($name in $previousRuntimeEnvironment.Keys) {
        Set-Item -LiteralPath ("Env:{0}" -f $name) -Value $previousRuntimeEnvironment[$name]
    }
    $manifest.endedAtUtc = [DateTime]::UtcNow.ToString("o")
    $manifest.normalCompletion = $normalCompletion -and -not $manifest.error
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
}

if ($manifest.error) {
    throw "$($manifest.error) See $manifestPath"
}

[pscustomobject][ordered]@{
    gameRoot = $game.root
    manifestPath = $manifestPath
    runDirectory = $runDirectory
    ttwCoreStartMenuObserved = $true
    saveAction = "none"
    noOpenXrOrSimulator = $true
} | ConvertTo-Json -Depth 8
