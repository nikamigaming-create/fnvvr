param(
    [ValidateSet("Release")][string]$Configuration = "Release",
    [string]$GameRoot = "D:\SteamLibrary\steamapps\common\Fallout New Vegas",
    # Uses an owned TTW sandbox and the separate FNVXR_AutoTTW_ save lineage.
    # It never targets the live retail game root.
    [switch]$TtwCore,
    [ValidateSet("Create", "Load", "Ensure")]
    [string]$Action = "Ensure",
    [ValidateSet(
        "None", "BuiltToDestroy", "FastShot", "FourEyes", "GoodNatured",
        "HeavyHanded", "Kamikaze", "SmallFrame", "TriggerDiscipline",
        "WildWasteland")]
    [string]$TraitOne = "None",
    [ValidateSet(
        "None", "BuiltToDestroy", "FastShot", "FourEyes", "GoodNatured",
        "HeavyHanded", "Kamikaze", "SmallFrame", "TriggerDiscipline",
        "WildWasteland")]
    [string]$TraitTwo = "None",
    # Fixed base FalloutNV.esm visibility loadout for this owned save. It is
    # selected before process launch and cannot carry an arbitrary command.
    [ValidateSet(
        "None", "Pistol", "RifleSingleHand", "RifleTwoHand", "Minigun",
        "FragGrenade", "Knife", "ThrowingKnife")]
    [string]$Weapon = "None",
    [ValidateRange(5, 900)][int]$ReadyTimeoutSeconds = 90,
    [switch]$UseAttestedBuild,
    [switch]$ValidateOnly
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

. (Join-Path $PSScriptRoot "fnvxr-product-common.ps1")

if (Test-FnvxrProductProcessElevated) {
    throw "Retail fixture runner refuses to run elevated."
}
if ($ValidateOnly -and -not $UseAttestedBuild) {
    throw "-ValidateOnly is read-only and requires -UseAttestedBuild."
}

$root = Get-FnvxrProductRoot
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
        throw "Retail fixture build attestation failed with exit code $LASTEXITCODE."
    }
    $attestation = Assert-FnvxrProductBuildAttestation `
        -Path $attestationPath `
        -Root $root `
        -Configuration $Configuration
}

$game = Assert-FnvxrProductGameRoot -GameRoot $GameRoot
$ttwSandboxManifest = $null
if ($TtwCore) {
    $allowedSandboxParent = [System.IO.Path]::GetFullPath((Join-Path $root "local"))
    $sandboxPrefix = $allowedSandboxParent.TrimEnd('\', '/') +
        [System.IO.Path]::DirectorySeparatorChar
    if (-not $game.root.StartsWith(
            $sandboxPrefix,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "TTW fixture GameRoot must be an isolated workspace sandbox below: $allowedSandboxParent"
    }
    Assert-FnvxrProductTtwBaselinePluginData -GameRoot $game.root
    $ttwSandboxManifestPath = Join-Path $game.root "fnvxr-ttw-sandbox-manifest.json"
    if (-not (Test-Path -LiteralPath $ttwSandboxManifestPath -PathType Leaf)) {
        throw "TTW fixture requires the owned sandbox manifest: $ttwSandboxManifestPath"
    }
    $ttwSandboxManifest = Get-Content -LiteralPath $ttwSandboxManifestPath -Raw | ConvertFrom-Json
    if ($ttwSandboxManifest.plan.schema -cne "fnvxr-ttw-retail-sandbox-v1" -or
        $ttwSandboxManifest.plan.profile.name -cne "ttw-core-baseline-v1") {
        throw "TTW fixture sandbox manifest is not the exact core profile: $ttwSandboxManifestPath"
    }
    $expectedTtwPlugins = @(Get-FnvxrProductTtwBaselinePluginNames)
    if ((@($ttwSandboxManifest.plan.profile.plugins) -join "|") -cne
        ($expectedTtwPlugins -join "|")) {
        throw "TTW fixture sandbox manifest plugin order differs from the exact core profile."
    }
}
$fixtureFamily = if ($TtwCore) { "ttw" } else { "retail" }
$traits = Resolve-FnvxrProductRetailFixtureTraits `
    -TraitOne $TraitOne `
    -TraitTwo $TraitTwo
$weapon = Resolve-FnvxrProductRetailFixtureWeapon -Weapon $Weapon
$saveName = if ($TtwCore) {
    Get-FnvxrProductTtwFixtureSaveName `
        -TraitOne $traits.first `
        -TraitTwo $traits.second `
        -Weapon $weapon
} else {
    Get-FnvxrProductRetailFixtureSaveName `
        -TraitOne $traits.first `
        -TraitTwo $traits.second `
        -Weapon $weapon
}
$saveRoot = Join-Path (Get-FnvxrProductDocumentsPath) "My Games\FalloutNV\Saves"
$savePath = Join-Path $saveRoot ($saveName + ".fos")
$nvsePath = Join-Path $saveRoot ($saveName + ".nvse")
$saveExists = Test-Path -LiteralPath $savePath -PathType Leaf
$nvseExists = Test-Path -LiteralPath $nvsePath -PathType Leaf
if ($saveExists -ne $nvseExists) {
    throw "Owned retail fixture has an incomplete save pair; refusing to overwrite or load it: $savePath"
}

$resolvedAction = switch ($Action) {
    "Create" {
        if ($saveExists) {
            throw "Owned retail fixture already exists; use -Action Load or Ensure: $savePath"
        }
        "create"
    }
    "Load" {
        if (-not $saveExists) {
            throw "Owned retail fixture does not exist; use -Action Create or Ensure: $savePath"
        }
        "load"
    }
    "Ensure" { if ($saveExists) { "load" } else { "create" } }
}

$stagePlan = @(Get-FnvxrProductRetailFixtureStagePlan `
    -Root $root `
    -Configuration $Configuration `
    -GameRoot $game.root)
if ($stagePlan.Count -ne 1 -or $stagePlan[0].key -cne "x86/nvse_fnvxr.dll") {
    throw "Retail fixture runner must stage exactly the xNVSE plugin and no graphics/input proxy."
}

$x64Output = Join-Path $root "build-product-x64\$Configuration"
$probePath = Join-Path $x64Output "fnvxr_shared_state_probe.exe"
$commandPath = Join-Path $x64Output "fnvxr_command.exe"
foreach ($requiredPath in @($probePath, $commandPath)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Attested retail fixture helper is missing: $requiredPath"
    }
}

$plan = [ordered]@{
    schema = if ($TtwCore) { "fnvxr-ttw-fixture-v1" } else { "fnvxr-retail-fixture-v1" }
    fixtureFamily = $fixtureFamily
    scope = if ($TtwCore) {
        "owned level-one TTW fixture in the isolated workspace sandbox only; fixed stock weapon loadout selected before launch; no official-pack acknowledgement, OpenXR host, simulator, D3D9, DirectInput, XInput, desktop input, controller, camera, rig, projectile, hit, or historical-user-save mutation"
    } else {
        "owned level-one base-retail fixture only; fixed stock weapon loadout selected before launch; exact native acknowledgement of only the four known official-pack notices when shown; no OpenXR host, simulator, D3D9, DirectInput, XInput, desktop input, controller, camera, rig, projectile, hit, or historical-user-save mutation"
    }
    action = $resolvedAction
    saveName = $saveName
    savePath = $savePath
    nvsePath = $nvsePath
    traits = [ordered]@{ first = $traits.first; second = $traits.second }
    weapon = $weapon
    runProfile = if ($TtwCore) { "ttw-fixture-v1" } else { "retail-fixture-v1" }
    stagedArtifacts = @($stagePlan | ForEach-Object { $_.key })
    fixturePlugins = if ($TtwCore) {
        @(Get-FnvxrProductTtwBaselinePluginNames)
    } else {
        @(Get-FnvxrProductRetailFixturePluginNames)
    }
    officialPackAcknowledgement = if ($TtwCore) {
        "disabled"
    } else {
        "only exact known title/body plus unique native first-button OK"
    }
    attestation = [ordered]@{
        path = $attestationPath
        sourceSha256 = $attestation.source.sha256
        artifactSha256 = $attestation.artifacts.sha256
        testCount = $attestation.tests.count
    }
}
if ($ValidateOnly) {
    $plan | ConvertTo-Json -Depth 8
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
$runDirectory = if ($TtwCore) {
    Join-Path $root "local\ttw-fixture-runs\$runId"
} else {
    Join-Path $root "local\retail-fixture-runs\$runId"
}
$backupRoot = Join-Path $runDirectory "backup"
New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
$launcherLog = Join-Path $runDirectory "fixture-supervisor.log"
$probeLog = Join-Path $runDirectory "runtime-probe.log"
$commandLog = Join-Path $runDirectory "fixture-command.log"
$manifestPath = Join-Path $runDirectory "fixture-manifest.json"

function Write-FixtureLog {
    param([Parameter(Mandatory = $true)][string]$Message)
    Add-Content -LiteralPath $launcherLog -Encoding UTF8 -Value (
        "{0} {1}" -f [DateTime]::UtcNow.ToString("o"), $Message)
}

function Invoke-FixtureMailboxCommand {
    param(
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$Description
    )

    $output = & $commandPath @Arguments 2>&1 | Out-String
    $exitCode = $LASTEXITCODE
    Add-Content -LiteralPath $commandLog -Encoding UTF8 -Value $output
    return [ordered]@{
        description = $Description
        arguments = @($Arguments)
        exitCode = $exitCode
        completed = ($exitCode -eq 0)
        output = $output
    }
}

$previousFnvxrEnvironment = @{}
Get-FnvxrProductProcessEnvironmentEntries -Prefix "FNVXR_" | ForEach-Object {
    $previousFnvxrEnvironment[$_.Name] = $_.Value
}
$manifest = [ordered]@{
    schema = if ($TtwCore) { "fnvxr-ttw-fixture-run-v1" } else { "fnvxr-retail-fixture-run-v1" }
    runId = $runId
    startedAtUtc = [DateTime]::UtcNow.ToString("o")
    state = "preflight"
    plan = $plan
    process = $null
    readiness = [ordered]@{
        exactPlugin = $false
        runtime = $false
        startMenu = $false
        gameplay = $false
        stableSavePair = $false
        loadPairUnchanged = $null
    }
    profiles = [ordered]@{
        fixture = $null
        after = $null
        restored = $false
    }
    staging = [ordered]@{
        records = @()
        restored = $false
    }
    loadedPlugin = $null
    command = $null
    gameplayEvidence = $null
    savePair = $null
    completion = $null
    error = $null
    endedAtUtc = $null
    normalCompletion = $false
}
$staged = @()
$fixtureProfileRecord = $null
$nvse = $null
$fallout = $null
$loadBefore = $null
$normalCompletion = $false

try {
    if ($TtwCore) {
        Assert-FnvxrProductTtwBaselinePluginData -GameRoot $game.root
    } else {
        Assert-FnvxrProductRetailFixturePluginData -GameRoot $game.root
    }
    $manifest.state = if ($TtwCore) {
        "staging-temporary-ttw-core-profile"
    } else {
        "staging-temporary-base-retail-profile"
    }
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
    if ($TtwCore) {
        Write-FixtureLog "staging temporary exact TTW core plugins.txt profile; historical saves are not touched"
        $fixtureProfileRecord = Install-FnvxrProductTtwBaselinePluginProfile `
            -Path (Get-FnvxrProductRetailVisualTrialPluginsPath) `
            -BackupRoot $backupRoot `
            -RunId $runId `
            -GameRoot $game.root
    } else {
        Write-FixtureLog "staging temporary FalloutNV.esm-only fixture plugins.txt profile; historical saves are not touched"
        $fixtureProfileRecord = Install-FnvxrProductRetailFixturePluginProfile `
            -Path (Get-FnvxrProductRetailVisualTrialPluginsPath) `
            -BackupRoot $backupRoot `
            -RunId $runId `
            -GameRoot $game.root
    }
    $manifest.profiles.fixture = $fixtureProfileRecord

    if ($resolvedAction -ceq "load") {
        $loadBefore = [ordered]@{
            save = Get-FnvxrProductFileIdentity -Path $savePath
            nvse = Get-FnvxrProductFileIdentity -Path $nvsePath
        }
    }

    $environment = Get-FnvxrProductMinimalEnvironment `
        -RunId $runId `
        -RunDirectory $runDirectory `
        -OpenXrLoaderPath "" `
        -SessionReadyTimeoutSeconds $ReadyTimeoutSeconds `
        -AutomateRetailFixture `
        -TtwFixture:$TtwCore `
        -RetailFixtureAction $resolvedAction `
        -RetailFixtureSaveName $saveName `
        -RetailFixtureTraitOne $traits.first `
        -RetailFixtureTraitTwo $traits.second `
        -RetailFixtureWeapon $weapon
    if ($environment.Contains("FNVXR_OPENXR_LOADER_HINT") -or
        $environment.Contains("XR_RUNTIME_JSON") -or
        $environment.Contains("OPENXR_SIMULATOR_HEADLESS")) {
        throw "Retail fixture environment unexpectedly contains an OpenXR or simulator setting."
    }
    Set-FnvxrProductMinimalEnvironment -Environment $environment

    $manifest.state = "staging-owned-xnvse-plugin-only"
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
    $staged = @(Install-FnvxrProductArtifactSet `
        -Plan $stagePlan `
        -BackupRoot $backupRoot `
        -RunId $runId)
    $manifest.staging.records = $staged

    $manifest.state = if ($TtwCore) {
        "starting-owned-ttw-fixture"
    } else {
        "starting-owned-retail-fixture"
    }
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
        throw "Timed out waiting for the owned exact FalloutNV.exe process."
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
        -Description "retail-fixture runtime publication with no OpenXR host"
    $manifest.readiness.runtime = $true

    $startMenu = Wait-FnvxrProductRetailFixtureStartMenu `
        -ProbePath $probePath `
        -RequiredProcess $fallout `
        -TimeoutSeconds $ReadyTimeoutSeconds `
        -LogPath $probeLog
    $manifest.readiness.startMenu = $true
    $commandWaitMilliseconds = [int]($ReadyTimeoutSeconds * 1000)
    $manifest.command = if ($resolvedAction -ceq "create") {
        Invoke-FixtureMailboxCommand `
            -Arguments @("fresh-character", "--wait-ms", [string]$commandWaitMilliseconds) `
            -Description "owned exact Goodsprings/create/name/trait/save fixture lifecycle"
    } else {
        Invoke-FixtureMailboxCommand `
            -Arguments @("console", "load $saveName", "--wait-ms", [string]$commandWaitMilliseconds) `
            -Description "owned exact fixture load"
    }
    if (-not $manifest.command.completed) {
        throw "Owned retail fixture $resolvedAction command was rejected or timed out. See $commandLog"
    }

    $gameplay = Wait-FnvxrProductRetailFixtureGameplay `
        -ProbePath $probePath `
        -RequiredProcess $fallout `
        -TimeoutSeconds $ReadyTimeoutSeconds `
        -LogPath $probeLog `
        -MinimumFrame ([uint64]$startMenu.json.runtime.frame)
    $manifest.readiness.gameplay = $true
    $manifest.gameplayEvidence = [ordered]@{
        frame = [uint64]$gameplay.json.runtime.frame
        phase = [uint32]$gameplay.json.runtime.phase
        menuBits = [uint32]$gameplay.json.runtime.menuBits
        stable = [bool]$gameplay.json.runtime.stable
        usable = [bool]$gameplay.json.runtime.usable
        uiInputAllowed = [bool]$gameplay.json.runtime.uiInputAllowed
        cameraActive = [bool]$gameplay.json.runtime.cameraActive
        showroomActive = [bool]$gameplay.json.runtime.showroomActive
    }

    if ($resolvedAction -ceq "create") {
        $manifest.savePair = Wait-FnvxrProductRetailFixtureSavePair `
            -SavePath $savePath `
            -NvsePath $nvsePath `
            -RequiredProcess $fallout `
            -TimeoutSeconds $ReadyTimeoutSeconds
        $manifest.readiness.stableSavePair = $true
    } else {
        $loadAfter = [ordered]@{
            save = Get-FnvxrProductFileIdentity -Path $savePath
            nvse = Get-FnvxrProductFileIdentity -Path $nvsePath
        }
        $unchanged = $loadBefore.save.sha256 -ceq $loadAfter.save.sha256 -and
            $loadBefore.save.length -eq $loadAfter.save.length -and
            $loadBefore.nvse.sha256 -ceq $loadAfter.nvse.sha256 -and
            $loadBefore.nvse.length -eq $loadAfter.nvse.length
        $manifest.readiness.loadPairUnchanged = $unchanged
        if (-not $unchanged) {
            throw "Owned retail fixture load changed its save pair; no load-only mutation is permitted."
        }
    }

    $manifest.state = "fixture-ready"
    $manifest.completion = "fixture-ready"
    $normalCompletion = $true
    Write-FixtureLog (
        "fixture ready action={0} save={1} traits={2},{3} gameplayFrame={4}; no OpenXR host or simulator was started" -f
        $resolvedAction, $saveName, $traits.first, $traits.second,
        $manifest.gameplayEvidence.frame)
} catch {
    $manifest.error = $_.Exception.Message
    $manifest.state = "failed"
    Write-FixtureLog ("ERROR " + $manifest.error)
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
    if ($fixtureProfileRecord) {
        try {
            $manifest.profiles.after = if ($TtwCore) {
                Restore-FnvxrProductTtwBaselinePluginProfile `
                    -Record $fixtureProfileRecord
            } else {
                Restore-FnvxrProductRetailFixturePluginProfile `
                    -Record $fixtureProfileRecord
            }
            $manifest.profiles.restored = $true
        } catch {
            $manifest.error = if ($manifest.error) {
                "$($manifest.error) Fixture profile restore also failed: $($_.Exception.Message)"
            } else {
                "Fixture profile restore failed: $($_.Exception.Message)"
            }
            $manifest.state = "failed"
        }
    }

    Clear-FnvxrProductProcessEnvironmentVariables
    foreach ($name in $previousFnvxrEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable(
            [string]$name,
            [string]$previousFnvxrEnvironment[$name],
            [EnvironmentVariableTarget]::Process)
    }
    $manifest.endedAtUtc = [DateTime]::UtcNow.ToString("o")
    $manifest.normalCompletion = $normalCompletion -and -not $manifest.error
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
}

if ($manifest.error) {
    throw "$($manifest.error) See $manifestPath"
}

[pscustomobject][ordered]@{
    fixtureFamily = $fixtureFamily
    saveName = $saveName
    action = $resolvedAction
    traits = $traits
    manifestPath = $manifestPath
    runDirectory = $runDirectory
    noOpenXrOrSimulator = $true
} | ConvertTo-Json -Depth 8
