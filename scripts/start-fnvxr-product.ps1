param(
    [ValidateSet("Release")][string]$Configuration = "Release",
    [string]$GameRoot = "D:\SteamLibrary\steamapps\common\Fallout New Vegas",
    [string]$OpenXrLoaderPath = "",
    [ValidateRange(1, 7200)][int]$HostFrames = 7200,
    [ValidateRange(5, 45)][int]$MaximumRunSeconds = 40,
    [ValidateRange(5, 120)][int]$HostReadyTimeoutSeconds = 45,
    [ValidateRange(5, 120)][int]$RetailReadyTimeoutSeconds = 60,
    [switch]$UseAttestedBuild,
    [switch]$ValidateOnly
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

. (Join-Path $PSScriptRoot "fnvxr-product-common.ps1")

$root = Get-FnvxrProductRoot
$attestationPath = Join-Path $root "local\product-build\fnvxr-product-$Configuration.json"
$x64Output = Join-Path $root "build-product-x64\$Configuration"
$hostPath = Join-Path $x64Output "fnvxr_openxr_pose_host.exe"
$probePath = Join-Path $x64Output "fnvxr_shared_state_probe.exe"
$stagedLoaderPath = Join-Path $x64Output "openxr_loader.dll"

if ($ValidateOnly -and -not $UseAttestedBuild) {
    throw "-ValidateOnly is read-only and therefore requires -UseAttestedBuild."
}

if ($UseAttestedBuild) {
    $attestation = Assert-FnvxrProductBuildAttestation `
        -Path $attestationPath `
        -Root $root `
        -Configuration $Configuration
} else {
    & (Join-Path $PSScriptRoot "build-fnvxr-product.ps1") `
        -Configuration $Configuration `
        -OpenXrLoaderPath $OpenXrLoaderPath | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Attested product build failed with exit code $LASTEXITCODE." }
    $attestation = Assert-FnvxrProductBuildAttestation `
        -Path $attestationPath `
        -Root $root `
        -Configuration $Configuration
}

$game = Assert-FnvxrProductGameRoot -GameRoot $GameRoot
$stagePlan = Get-FnvxrProductStagePlan `
    -Root $root `
    -Configuration $Configuration `
    -GameRoot $game.root
$artifactSnapshot = Get-FnvxrProductArtifactSnapshot -Descriptors (
    Get-FnvxrProductArtifactDescriptors -Root $root -Configuration $Configuration)
$attestationIdentity = Get-FnvxrProductFileIdentity -Path $attestationPath

if ($ValidateOnly) {
    [pscustomobject][ordered]@{
        valid = $true
        liveActionsTaken = $false
        product = "fnvxr-v5-exact-retail"
        gameRoot = $game.root
        falloutIdentity = $game.fallout
        nvseLoaderIdentity = $game.nvseLoader
        compatibilityModules = $game.compatibilityModules
        buildAttestation = $attestationIdentity
        sourceSha256 = $attestation.source.sha256
        artifactSetSha256 = $artifactSnapshot.sha256
        testCatalogSha256 = $attestation.tests.sha256
        stagePlan = $stagePlan
        minimalEnvironmentKeys = @(Get-FnvxrProductMinimalEnvironment `
            -RunId "validate-only" `
            -RunDirectory "validate-only" `
            -OpenXrLoaderPath $stagedLoaderPath).Keys
    } | ConvertTo-Json -Depth 8
    return
}

$existing = @(Get-Process FalloutNV,nvse_loader,fnvxr_openxr_pose_host -ErrorAction SilentlyContinue)
if ($existing.Count -ne 0) {
    $summary = @($existing | ForEach-Object { "$($_.ProcessName):$($_.Id)" }) -join ", "
    throw "Product launch refused because an existing runtime process is not owned by this supervisor: $summary"
}

$runId = "{0}-{1}" -f (Get-Date -Format "yyyyMMdd-HHmmss-fff"), [Guid]::NewGuid().ToString("N").Substring(0, 12)
$runDirectory = Join-Path $root "local\product-runs\$runId"
$backupRoot = Join-Path $runDirectory "backups"
$manifestPath = Join-Path $runDirectory "manifest.json"
$completionHashPath = Join-Path $runDirectory "completion.sha256"
$hostOut = Join-Path $runDirectory "host.stdout.log"
$hostErr = Join-Path $runDirectory "host.stderr.log"
$probeLog = Join-Path $runDirectory "readiness-probe.log"
$launcherLog = Join-Path $runDirectory "supervisor.log"
New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null

$manifest = [ordered]@{
    schema = 1
    product = "fnvxr-v5-exact-retail"
    acceptanceScope = "stereo-visual-trial-only"
    runId = $runId
    supervisorProcessId = $PID
    startedAtUtc = [DateTime]::UtcNow.ToString("o")
    state = "initializing"
    accepted = $false
    trialReady = $false
    fullProductAccepted = $false
    controllerMutationAuthorized = $false
    trackedWeaponAuthorized = $false
    completion = $null
    error = $null
    game = $game
    build = [ordered]@{
        attestation = $attestationIdentity
        nonce = $attestation.nonce
        sourceSha256 = $attestation.source.sha256
        artifactSetSha256 = $artifactSnapshot.sha256
        testCatalogSha256 = $attestation.tests.sha256
        testCount = $attestation.tests.count
    }
    environment = $null
    staged = @()
    processes = [ordered]@{}
    readiness = [ordered]@{
        hostPose = $false
        retailRuntimeAndPose = $false
        exactModules = $false
    }
    cleanup = [ordered]@{
        falloutStopped = $false
        nvseLoaderStopped = $false
        hostStopped = $false
        failedStageRolledBack = $false
    }
    logs = [ordered]@{
        supervisor = $launcherLog
        hostStdout = $hostOut
        hostStderr = $hostErr
        readinessProbe = $probeLog
    }
}
Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath

$savedEnvironment = @{}
foreach ($entry in Get-ChildItem Env: | Where-Object { $_.Name -like "FNVXR_*" }) {
    $savedEnvironment[$entry.Name] = $entry.Value
}
$hostProcess = $null
$nvse = $null
$fallout = $null
$staged = @()
$runtimeReady = $false
$normalCompletion = $false

function Write-SupervisorLog {
    param([string]$Message)
    Add-Content -LiteralPath $launcherLog -Value ("{0} {1}" -f [DateTime]::UtcNow.ToString("o"), $Message) -Encoding UTF8
}

function Get-ExactFalloutProcess {
    param([Parameter(Mandatory = $true)][string]$ExpectedPath)

    foreach ($candidate in @(Get-Process FalloutNV -ErrorAction SilentlyContinue)) {
        try {
            if ([string]::Equals($candidate.Path, $ExpectedPath, [System.StringComparison]::OrdinalIgnoreCase)) {
                return $candidate
            }
        } catch {}
    }
    return $null
}

function Wait-ExactLoadedModule {
    param(
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$Process,
        [Parameter(Mandatory = $true)][string]$ExpectedPath,
        [Parameter(Mandatory = $true)][string]$ExpectedSha256,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $Process.Refresh()
        if ($Process.HasExited) { throw "Fallout exited before loading $ExpectedPath." }
        try {
            foreach ($module in $Process.Modules) {
                if ([string]::Equals($module.FileName, $ExpectedPath, [System.StringComparison]::OrdinalIgnoreCase)) {
                    $identity = Get-FnvxrProductFileIdentity -Path $module.FileName -RequirePe
                    if ($identity.sha256 -cne $ExpectedSha256) {
                        throw "Loaded module hash differs from staged module: $ExpectedPath"
                    }
                    return $identity
                }
            }
        } catch {
            if ($_.Exception.Message -like "Loaded module hash differs*") { throw }
        }
        Start-Sleep -Milliseconds 200
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Timed out waiting for exact loaded module: $ExpectedPath"
}

try {
    $environment = Get-FnvxrProductMinimalEnvironment `
        -RunId $runId `
        -RunDirectory $runDirectory `
        -OpenXrLoaderPath $stagedLoaderPath
    Set-FnvxrProductMinimalEnvironment -Environment $environment
    $manifest.environment = $environment
    $manifest.state = "starting-host"
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
    Write-SupervisorLog "starting attested x64 host before any game staging"

    $hostProcess = Start-Process `
        -FilePath $hostPath `
        -ArgumentList ([string]$HostFrames) `
        -WorkingDirectory $x64Output `
        -RedirectStandardOutput $hostOut `
        -RedirectStandardError $hostErr `
        -WindowStyle Hidden `
        -PassThru
    $manifest.processes.host = [ordered]@{
        processId = $hostProcess.Id
        path = $hostPath
        startedAtUtc = $hostProcess.StartTime.ToUniversalTime().ToString("o")
        frameLimit = $HostFrames
    }
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath

    Wait-FnvxrProductProbeReady `
        -ProbePath $probePath `
        -Arguments @("--require-pose", "--require-advancing", "--sample-delay-ms", "100") `
        -RequiredProcess $hostProcess `
        -TimeoutSeconds $HostReadyTimeoutSeconds `
        -LogPath $probeLog `
        -Description "advancing OpenXR v8 pose publication"
    $manifest.readiness.hostPose = $true
    $manifest.state = "staging-attested-retail-artifacts"
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
    Write-SupervisorLog "host pose publication is advancing; staging exact Win32 product set"

    $staged = @(Install-FnvxrProductArtifactSet `
        -Plan $stagePlan `
        -BackupRoot $backupRoot `
        -RunId $runId)
    $manifest.staged = $staged
    $manifest.state = "starting-retail"
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath

    $nvse = Start-Process `
        -FilePath $game.nvseLoader.path `
        -WorkingDirectory $game.root `
        -PassThru
    $manifest.processes.nvseLoader = [ordered]@{
        processId = $nvse.Id
        path = $game.nvseLoader.path
        startedAtUtc = $nvse.StartTime.ToUniversalTime().ToString("o")
    }
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath

    $retailDeadline = [DateTime]::UtcNow.AddSeconds($RetailReadyTimeoutSeconds)
    do {
        $hostProcess.Refresh()
        if ($hostProcess.HasExited) { throw "OpenXR host exited before Fallout startup with code $($hostProcess.ExitCode)." }
        $fallout = Get-ExactFalloutProcess -ExpectedPath $game.fallout.path
        if ($fallout) { break }
        Start-Sleep -Milliseconds 200
    } while ([DateTime]::UtcNow -lt $retailDeadline)
    if (-not $fallout) { throw "Timed out waiting for exact FalloutNV.exe process." }

    $manifest.processes.fallout = [ordered]@{
        processId = $fallout.Id
        path = $game.fallout.path
        startedAtUtc = $fallout.StartTime.ToUniversalTime().ToString("o")
        sha256 = $game.fallout.sha256
    }
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath

    $remainingReadySeconds = [Math]::Max(5, [int]($retailDeadline - [DateTime]::UtcNow).TotalSeconds)
    Wait-FnvxrProductProbeReady `
        -ProbePath $probePath `
        -Arguments @("--require-pose", "--require-runtime", "--require-advancing", "--sample-delay-ms", "100") `
        -RequiredProcess $fallout `
        -TimeoutSeconds $remainingReadySeconds `
        -LogPath $probeLog `
        -Description "advancing retail runtime plus OpenXR pose publication"
    $runtimeReady = $true
    $manifest.readiness.retailRuntimeAndPose = $true

    $d3d9Record = @($staged | Where-Object { $_.key -eq "x86/d3d9.dll" })[0]
    $pluginRecord = @($staged | Where-Object { $_.key -eq "x86/nvse_fnvxr.dll" })[0]
    $loadedD3d9 = Wait-ExactLoadedModule `
        -Process $fallout `
        -ExpectedPath $d3d9Record.destination.path `
        -ExpectedSha256 $d3d9Record.destination.sha256 `
        -TimeoutSeconds 10
    $loadedPlugin = Wait-ExactLoadedModule `
        -Process $fallout `
        -ExpectedPath $pluginRecord.destination.path `
        -ExpectedSha256 $pluginRecord.destination.sha256 `
        -TimeoutSeconds 10
    $manifest.readiness.exactModules = $true
    $manifest.processes.fallout.loadedProductModules = @($loadedD3d9, $loadedPlugin)
    $manifest.state = "ready-and-supervised"
    $manifest.trialReady = $true
    $manifest.readyAtUtc = [DateTime]::UtcNow.ToString("o")
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
    Write-SupervisorLog "stereo visual trial ready: retail runtime, pose, d3d9 bridge, and NVSE plugin identities are live; controller/weapon product gates remain closed"

    $deadline = [DateTime]::UtcNow.AddSeconds($MaximumRunSeconds)
    $healthFailures = 0
    $nextHealthCheck = [DateTime]::UtcNow
    $completion = $null
    do {
        $hostProcess.Refresh()
        $fallout.Refresh()
        if ($fallout.HasExited) { $completion = "retail-exited"; break }
        if ($hostProcess.HasExited) {
            $completion = if ($hostProcess.ExitCode -eq 0) { "host-frame-limit" } else { "host-failed" }
            break
        }
        if ([DateTime]::UtcNow -ge $nextHealthCheck) {
            if (Test-FnvxrProductProbeReady `
                -ProbePath $probePath `
                -Arguments @("--require-pose", "--require-runtime", "--require-advancing", "--sample-delay-ms", "100") `
                -LogPath $probeLog) {
                $healthFailures = 0
            } else {
                ++$healthFailures
                if ($healthFailures -ge 3) { $completion = "shared-state-health-lost"; break }
            }
            $nextHealthCheck = [DateTime]::UtcNow.AddSeconds(2)
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    if (-not $completion) { $completion = "supervised-time-limit" }

    $normalCompletion = $completion -in @("retail-exited", "host-frame-limit", "supervised-time-limit")
    if (-not $normalCompletion) { throw "Product supervision failed: $completion" }
    $manifest.completion = $completion
    $manifest.state = "cleaning-up"
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
} catch {
    $manifest.error = $_.Exception.Message
    $manifest.state = "failed"
    Write-SupervisorLog ("ERROR " + $_.Exception.Message)
} finally {
    Stop-FnvxrOwnedProcess -Process $fallout
    if ($fallout) {
        try { $fallout.Refresh(); $manifest.cleanup.falloutStopped = $fallout.HasExited } catch {}
    } else { $manifest.cleanup.falloutStopped = $true }
    Stop-FnvxrOwnedProcess -Process $nvse
    if ($nvse) {
        try { $nvse.Refresh(); $manifest.cleanup.nvseLoaderStopped = $nvse.HasExited } catch {}
    } else { $manifest.cleanup.nvseLoaderStopped = $true }
    Stop-FnvxrOwnedProcess -Process $hostProcess
    if ($hostProcess) {
        try {
            $hostProcess.Refresh()
            $manifest.cleanup.hostStopped = $hostProcess.HasExited
            if ($hostProcess.HasExited) { $manifest.processes.host.exitCode = $hostProcess.ExitCode }
        } catch {}
    } else { $manifest.cleanup.hostStopped = $true }

    if ($manifest.error -and $staged.Count -gt 0 -and -not $runtimeReady) {
        try {
            Restore-FnvxrProductArtifactSet -Records $staged
            $manifest.cleanup.failedStageRolledBack = $true
        } catch {
            $manifest.cleanup.rollbackError = $_.Exception.Message
        }
    }

    Get-ChildItem Env: | Where-Object { $_.Name -like "FNVXR_*" } | ForEach-Object {
        Remove-Item -LiteralPath ("Env:{0}" -f $_.Name) -ErrorAction SilentlyContinue
    }
    foreach ($key in $savedEnvironment.Keys) {
        Set-Item -LiteralPath ("Env:{0}" -f $key) -Value ([string]$savedEnvironment[$key])
    }
    $manifest.completedAtUtc = [DateTime]::UtcNow.ToString("o")
    if (-not $manifest.error -and $normalCompletion) { $manifest.state = "complete" }
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
    $completionIdentity = Get-FnvxrProductFileIdentity -Path $manifestPath
    ("{0}  {1}" -f $completionIdentity.sha256, [System.IO.Path]::GetFileName($manifestPath)) |
        Set-Content -LiteralPath $completionHashPath -Encoding ASCII
}

if ($manifest.error) { throw $manifest.error }
$manifest | ConvertTo-Json -Depth 10
