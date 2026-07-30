param(
    [ValidateSet("Release")][string]$Configuration = "Release",
    [string]$GameRoot = "D:\SteamLibrary\steamapps\common\Fallout New Vegas",
    [string]$OpenXrLoaderPath = "",
    [string]$HeadlessSimulatorManifest = "",
    # Process-local runtime selection for a physical headset. Unlike the
    # simulator option, this sets only XR_RUNTIME_JSON.
    [string]$PhysicalRuntimeManifest = "",
    [switch]$PhysicalHeadsetPlay,
    # Temporary per-eye retail source resolution for interactive headset play.
    # The user's Fallout INIs are backed up and restored byte-for-byte.
    [ValidateRange(1280, 4096)][int]$PhysicalGameWidth = 1920,
    [ValidateRange(720, 2560)][int]$PhysicalGameHeight = 1200,
    # Optional workspace-staged Meta XR Operator API layer. It is observed
    # only: FNVXR neither starts its MCP proxy nor invokes pose/controller tools.
    [string]$MetaXrOperatorLayerDirectory = "",
    [ValidateRange(1, 7200)][int]$HostFrames = 7200,
    [ValidateRange(5, 900)][int]$MaximumRunSeconds = 40,
    [ValidateRange(5, 120)][int]$HostReadyTimeoutSeconds = 45,
    [ValidateRange(5, 900)][int]$RetailReadyTimeoutSeconds = 60,
    [switch]$UseAttestedBuild,
    [switch]$AutomateInGame,
    # A separate opt-in for at most one native acknowledgement of each exact
    # official pre-order-pack notification observed after the verified save
    # loads. It never sends desktop, keyboard, mouse, controller, or
    # simulator input.
    [switch]$AcknowledgeTribalPackPopup,
    # Opt in to a temporary, exact-official retail plugins.txt profile. The
    # supervisor hashes and restores the user's original profile after its
    # owned game processes stop; save files are never staged or written.
    [switch]$UseRetailPluginProfile,
    [switch]$StartNewCharacter,
    # Create or load an owned, disposable level-one retail fixture. This is
    # independent of the historical FNVXR_StereoTest save and always uses the
    # minimal FalloutNV.esm-only profile while the supervisor owns the run.
    [ValidateSet("Disabled", "Create", "Load", "Ensure")]
    [string]$RetailFixtureAction = "Disabled",
    # Selects the exact TTW core profile in an isolated workspace sandbox.
    # It is valid only for the owned FNVXR_AutoTTW fixture lineage.
    [switch]$TtwCore,
    [ValidateSet(
        "None", "BuiltToDestroy", "FastShot", "FourEyes", "GoodNatured",
        "HeavyHanded", "Kamikaze", "SmallFrame", "TriggerDiscipline",
        "WildWasteland")]
    [string]$RetailFixtureTraitOne = "None",
    [ValidateSet(
        "None", "BuiltToDestroy", "FastShot", "FourEyes", "GoodNatured",
        "HeavyHanded", "Kamikaze", "SmallFrame", "TriggerDiscipline",
        "WildWasteland")]
    [string]$RetailFixtureTraitTwo = "None",
    # Writes a bounded image sequence from the final OpenXR eye swapchains.
    # This is a command-line-only simulator mirror; it never drives the game
    # window, a controller, or simulator UI.
    [switch]$CaptureHeadsetMirror,
    [ValidateRange(1, 600)][int]$HeadsetMirrorCaptureEveryFrames = 6,
    [ValidateRange(1, 3600)][int]$HeadsetMirrorCaptureMaxPairs = 180,
    # Opt in to loading an owned FNVXR_AutoRetail fixture through the visual
    # trial, then showing its Pip-Boy via two fixed in-game Tab events. This
    # is only valid alongside the final headset mirror capture.
    [switch]$HeadsetDemoFixture,
    # Loads the same owned fixture into the headless OpenXR visual trial but
    # keeps it in gameplay for a sustained world-only stereo recording. This
    # mode never enables the demo's fixed Pip-Boy input events.
    [switch]$HeadsetWorldOnlyCapture,
    # Drives a deterministic, bounded six-axis HMD pose sweep through the
    # headless runtime's per-run file IPC while world-only capture is active.
    # The runtime keeps both controllers in their tracked head-relative poses.
    [switch]$HeadsetPoseSweep,
    [ValidateRange(2, 120)][int]$HeadsetPoseSweepSeconds = 12,
    # Keeps the default demo behavior while allowing a bounded diagnostic run
    # to prove whether a fault precedes its first fixed in-game Pip-Boy tap.
    [ValidateRange(1, 1200)]
    [int]$HeadsetDemoGameplayWarmupFrames = 90,
    # Narrows the exact AccumulateScene integration for crash localization.
    # RelayOnly runs only the stack-preserving stock relay; CaptureOnly also
    # tees and seals the stock culler, while neither mode renders private eyes
    # or publishes world stereo.
    [ValidateSet(
        "Full",
        "RelayOnly",
        "CaptureOnly",
        "PrepareOnly",
        "RenderNoPublish",
        "SnapshotOnly",
        "CollectOnly",
        "BindOnly",
        "CameraOnly",
        "PopulateOnly",
        "RenderOnly",
        "FinalizeOnly",
        "LeftEyeOnly")]
    [string]$RetailVrAccumulationDiagnosticMode = "Full",
    [Alias("RecoverySaveName")]
    [ValidateSet("FNVXR_StereoTest")]
    [string]$RetailSaveName = "FNVXR_StereoTest",
    [switch]$ValidateOnly
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

. (Join-Path $PSScriptRoot "fnvxr-product-common.ps1")

$headsetFixtureVisualTrial =
    [bool]($HeadsetDemoFixture -or $HeadsetWorldOnlyCapture)
$headsetFixtureOpenXrRun =
    [bool]($headsetFixtureVisualTrial -or $PhysicalHeadsetPlay)
if ($HeadsetDemoFixture -and $HeadsetWorldOnlyCapture) {
    throw "-HeadsetDemoFixture and -HeadsetWorldOnlyCapture are mutually exclusive."
}
if ($HeadsetPoseSweep -and -not $HeadsetWorldOnlyCapture) {
    throw "-HeadsetPoseSweep requires -HeadsetWorldOnlyCapture."
}
if (-not $HeadsetPoseSweep -and $HeadsetPoseSweepSeconds -ne 12) {
    throw "-HeadsetPoseSweepSeconds requires -HeadsetPoseSweep."
}
if ($AutomateInGame -and $StartNewCharacter) {
    throw "-AutomateInGame and -StartNewCharacter are mutually exclusive."
}
if ($RetailFixtureAction -ne "Disabled" -and
    ($AutomateInGame -or $StartNewCharacter)) {
    throw "-RetailFixtureAction is mutually exclusive with -AutomateInGame and -StartNewCharacter."
}
if ($TtwCore -and $RetailFixtureAction -eq "Disabled") {
    throw "-TtwCore requires -RetailFixtureAction Create, Load, or Ensure for the owned FNVXR_AutoTTW fixture lineage."
}
if ($TtwCore -and ($AutomateInGame -or $StartNewCharacter)) {
    throw "-TtwCore is mutually exclusive with -AutomateInGame and -StartNewCharacter."
}
if ($AcknowledgeTribalPackPopup -and -not $AutomateInGame) {
    throw "-AcknowledgeTribalPackPopup requires -AutomateInGame."
}
if ($CaptureHeadsetMirror -and [string]::IsNullOrWhiteSpace($HeadlessSimulatorManifest)) {
    throw "-CaptureHeadsetMirror requires -HeadlessSimulatorManifest so the recording is from the command-line headless simulator."
}
if (-not [string]::IsNullOrWhiteSpace($HeadlessSimulatorManifest) -and
    -not [string]::IsNullOrWhiteSpace($PhysicalRuntimeManifest)) {
    throw "-HeadlessSimulatorManifest and -PhysicalRuntimeManifest are mutually exclusive."
}
if ($PhysicalHeadsetPlay -and
    [string]::IsNullOrWhiteSpace($PhysicalRuntimeManifest)) {
    throw "-PhysicalHeadsetPlay requires -PhysicalRuntimeManifest."
}
if (-not $PhysicalHeadsetPlay -and
    -not [string]::IsNullOrWhiteSpace($PhysicalRuntimeManifest)) {
    throw "-PhysicalRuntimeManifest is valid only with -PhysicalHeadsetPlay."
}
if ($PhysicalHeadsetPlay -and $headsetFixtureVisualTrial) {
    throw "-PhysicalHeadsetPlay is mutually exclusive with headless headset demo/world-capture modes."
}
if ($PhysicalHeadsetPlay -and $CaptureHeadsetMirror) {
    throw "-PhysicalHeadsetPlay does not permit diagnostic mirror capture; the capture stalls the physical compositor."
}
if ($PhysicalHeadsetPlay -and $HeadsetPoseSweep) {
    throw "-PhysicalHeadsetPlay cannot use the headless simulator pose sweep."
}
if ($PhysicalHeadsetPlay -and
    -not [string]::IsNullOrWhiteSpace($MetaXrOperatorLayerDirectory)) {
    throw "-PhysicalHeadsetPlay does not load the simulator-only Meta XR Operator layer."
}
if ($PhysicalHeadsetPlay -and $RetailFixtureAction -cne "Load") {
    throw "-PhysicalHeadsetPlay requires -RetailFixtureAction Load for an existing owned fixture."
}
$physicalDisplaySize = Assert-FnvxrProductPhysicalDisplaySize `
    -Width $PhysicalGameWidth `
    -Height $PhysicalGameHeight
if (-not [string]::IsNullOrWhiteSpace($MetaXrOperatorLayerDirectory) -and
    [string]::IsNullOrWhiteSpace($HeadlessSimulatorManifest)) {
    throw "-MetaXrOperatorLayerDirectory requires -HeadlessSimulatorManifest; interactive runtime selection is not authorized."
}
if ($headsetFixtureVisualTrial -and $RetailFixtureAction -eq "Disabled") {
    throw "-HeadsetDemoFixture and -HeadsetWorldOnlyCapture require -RetailFixtureAction Create, Load, or Ensure."
}
if ($headsetFixtureVisualTrial -and -not $CaptureHeadsetMirror) {
    throw "-HeadsetDemoFixture and -HeadsetWorldOnlyCapture require -CaptureHeadsetMirror so final headset output is recorded."
}
if (-not $HeadsetDemoFixture -and
    $HeadsetDemoGameplayWarmupFrames -ne 90) {
    throw "-HeadsetDemoGameplayWarmupFrames requires -HeadsetDemoFixture."
}
if (-not $HeadsetDemoFixture -and
    $RetailVrAccumulationDiagnosticMode -cne "Full") {
    throw "-RetailVrAccumulationDiagnosticMode requires -HeadsetDemoFixture."
}
if ($CaptureHeadsetMirror -and $RetailFixtureAction -ne "Disabled" -and
    -not $headsetFixtureVisualTrial) {
    throw "-CaptureHeadsetMirror cannot use the isolated retail-fixture runner; it has no OpenXR or simulator path."
}
if ($UseRetailPluginProfile) {
    throw "-UseRetailPluginProfile is retired because its DLC-only profile mismatches TTW and fixture saves. Use -RetailFixtureAction instead."
}

if (Test-FnvxrProductProcessElevated) {
    throw "Product launcher refuses to run elevated; the game, host, and process-local OpenXR runtime must remain at normal user integrity."
}
if ($RetailFixtureAction -ne "Disabled" -and -not $headsetFixtureOpenXrRun) {
    # Never let the general VR supervisor start a host or runtime for a save
    # fixture. The dedicated runner stages only the xNVSE plugin and has no
    # OpenXR/simulator path at all.
    if (-not [string]::IsNullOrWhiteSpace($HeadlessSimulatorManifest) -or
        -not [string]::IsNullOrWhiteSpace($PhysicalRuntimeManifest) -or
        -not [string]::IsNullOrWhiteSpace($OpenXrLoaderPath) -or
        -not [string]::IsNullOrWhiteSpace($MetaXrOperatorLayerDirectory)) {
        throw "Retail fixtures do not accept an OpenXR loader or simulator manifest."
    }
    # A PowerShell script invoked with an array splat receives those strings as
    # positional arguments; it does not reinterpret "-Configuration" as a
    # named parameter. Keep this boundary as a hashtable splat so every value
    # reaches the dedicated fixture runner under its declared parameter name.
    $fixtureArguments = @{
        Configuration = $Configuration
        GameRoot = $GameRoot
        Action = $RetailFixtureAction
        TraitOne = $RetailFixtureTraitOne
        TraitTwo = $RetailFixtureTraitTwo
        ReadyTimeoutSeconds = $RetailReadyTimeoutSeconds
    }
    if ($TtwCore) { $fixtureArguments["TtwCore"] = $true }
    if ($UseAttestedBuild) { $fixtureArguments["UseAttestedBuild"] = $true }
    if ($ValidateOnly) { $fixtureArguments["ValidateOnly"] = $true }
    & (Join-Path $PSScriptRoot "start-fnvxr-retail-fixture.ps1") @fixtureArguments
    return
}

$root = Get-FnvxrProductRoot
$fixtureFamily = if ($TtwCore) { "ttw" } else { "retail" }
$validateHeadsetMirrorDirectory = if ($CaptureHeadsetMirror) {
    Join-Path $root "local\product-runs\validate-only-headset-mirror"
} else {
    ""
}
$attestationPath = Join-Path $root "local\product-build\fnvxr-product-$Configuration.json"
$x64Output = Join-Path $root "build-product-x64\$Configuration"
$hostPath = Join-Path $x64Output "fnvxr_openxr_pose_host.exe"
$probePath = Join-Path $x64Output "fnvxr_shared_state_probe.exe"
$commandPath = Join-Path $x64Output "fnvxr_command.exe"
$stagedLoaderPath = Join-Path $x64Output "openxr_loader.dll"
$fixedRecoverySaveName = $RetailSaveName
$fixedRecoveryLoadCommand =
    Get-FnvxrProductApprovedRetailSaveLoadCommand -RetailSaveName $RetailSaveName
$fixedRecoveryLoadWaitMilliseconds = [int]($RetailReadyTimeoutSeconds * 1000)
$freshCharacterName = "FNVXR_StereoTest"
$freshCharacterStartCommand = Get-FnvxrProductFreshCharacterStartCommand
$saveRoot = Join-Path (Get-FnvxrProductDocumentsPath) "My Games\FalloutNV\Saves"
$fixedRecoverySavePath = Join-Path $saveRoot ($fixedRecoverySaveName + ".fos")
$fixedRecoveryNvsePath = Join-Path $saveRoot ($fixedRecoverySaveName + ".nvse")
$freshCharacterSavePath = Join-Path $saveRoot ($freshCharacterName + ".fos")
$freshCharacterNvsePath = Join-Path $saveRoot ($freshCharacterName + ".nvse")
$retailFixtureRequested = $RetailFixtureAction -cne "Disabled"
$retailFixtureTraits = if ($retailFixtureRequested) {
    Resolve-FnvxrProductRetailFixtureTraits `
        -TraitOne $RetailFixtureTraitOne `
        -TraitTwo $RetailFixtureTraitTwo
} else {
    $null
}
$retailFixtureSaveName = if ($retailFixtureRequested) {
    if ($TtwCore) {
        Get-FnvxrProductTtwFixtureSaveName `
            -TraitOne $retailFixtureTraits.first `
            -TraitTwo $retailFixtureTraits.second
    } else {
        Get-FnvxrProductRetailFixtureSaveName `
            -TraitOne $retailFixtureTraits.first `
            -TraitTwo $retailFixtureTraits.second
    }
} else {
    ""
}
$retailFixtureSavePath = if ($retailFixtureRequested) {
    Join-Path $saveRoot ($retailFixtureSaveName + ".fos")
} else {
    ""
}
$retailFixtureNvsePath = if ($retailFixtureRequested) {
    Join-Path $saveRoot ($retailFixtureSaveName + ".nvse")
} else {
    ""
}
$resolvedRetailFixtureAction = "disabled"
if ($retailFixtureRequested) {
    $fixtureFosExists = Test-Path -LiteralPath $retailFixtureSavePath -PathType Leaf
    $fixtureNvseExists = Test-Path -LiteralPath $retailFixtureNvsePath -PathType Leaf
    if ($fixtureFosExists -ne $fixtureNvseExists) {
        throw "Retail fixture has an incomplete save pair; refusing to overwrite or load it: $retailFixtureSavePath"
    }
    switch ($RetailFixtureAction) {
        "Create" {
            if ($fixtureFosExists) {
                throw "Retail fixture already exists; use -RetailFixtureAction Load or Ensure: $retailFixtureSavePath"
            }
            $resolvedRetailFixtureAction = "create"
        }
        "Load" {
            if (-not $fixtureFosExists) {
                throw "Retail fixture does not exist yet; use -RetailFixtureAction Create or Ensure: $retailFixtureSavePath"
            }
            $resolvedRetailFixtureAction = "load"
        }
        "Ensure" {
            $resolvedRetailFixtureAction = if ($fixtureFosExists) { "load" } else { "create" }
        }
    }
}
if ($headsetFixtureVisualTrial -and $retailFixtureRequested -and
    $resolvedRetailFixtureAction -cne "load") {
    throw "Headset fixture visual trials require an existing owned fixture load. Create it first with the isolated fixture runner, then use -RetailFixtureAction Load."
}
$automationRequested = [bool]($AutomateInGame -or $StartNewCharacter -or $retailFixtureRequested)
$creatingRetailFixture = $retailFixtureRequested -and
    $resolvedRetailFixtureAction -ceq "create"
$loadingRetailFixture = $retailFixtureRequested -and
    $resolvedRetailFixtureAction -ceq "load"
$creatingFreshCharacter = $StartNewCharacter -or $creatingRetailFixture
$runtimeRegistryBefore = $null
$headlessRuntimeIdentity = $null
$headlessRuntimeManifestPath = ""
$physicalRuntimeIdentity = $null
$physicalRuntimeManifestPath = ""
if (-not [string]::IsNullOrWhiteSpace($HeadlessSimulatorManifest)) {
    $runtimeRegistryBefore = Get-FnvxrProductHklmActiveRuntimeSnapshot
    $headlessRuntimeIdentity =
        Resolve-FnvxrProductHeadlessRuntimeManifest `
            -ManifestPath $HeadlessSimulatorManifest
    $headlessRuntimeManifestPath = [string]$headlessRuntimeIdentity.manifest.path
}
if (-not [string]::IsNullOrWhiteSpace($PhysicalRuntimeManifest)) {
    $runtimeRegistryBefore = Get-FnvxrProductHklmActiveRuntimeSnapshot
    $physicalRuntimeIdentity =
        Resolve-FnvxrProductPhysicalRuntimeManifest `
            -ManifestPath $PhysicalRuntimeManifest
    $physicalRuntimeManifestPath =
        [string]$physicalRuntimeIdentity.manifest.path
}
$selectedRuntimeIdentity = if ($headlessRuntimeIdentity) {
    $headlessRuntimeIdentity
} else {
    $physicalRuntimeIdentity
}
$metaXrOperatorIdentity = $null
if (-not [string]::IsNullOrWhiteSpace($MetaXrOperatorLayerDirectory)) {
    $metaXrOperatorIdentity = Resolve-FnvxrProductMetaXrOperatorLayer `
        -Root $root `
        -LayerDirectory $MetaXrOperatorLayerDirectory
}

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
$ttwCoreProfilePlan = [ordered]@{
    requested = [bool]$TtwCore
    fixtureFamily = $fixtureFamily
    sandboxManifest = $null
    pluginEntries = @()
    dataVerified = $false
    status = if ($TtwCore) { "validated-not-staged" } else { "disabled" }
    before = $null
    staged = $null
    after = $null
    restored = $false
}
if ($TtwCore) {
    $allowedSandboxParent = [System.IO.Path]::GetFullPath((Join-Path $root "local"))
    $sandboxPrefix = $allowedSandboxParent.TrimEnd('\', '/') +
        [System.IO.Path]::DirectorySeparatorChar
    if (-not $game.root.StartsWith(
            $sandboxPrefix,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "TTW headset proof GameRoot must be an isolated workspace sandbox below: $allowedSandboxParent"
    }
    Assert-FnvxrProductTtwBaselinePluginData -GameRoot $game.root
    $ttwSandboxManifestPath = Join-Path $game.root "fnvxr-ttw-sandbox-manifest.json"
    if (-not (Test-Path -LiteralPath $ttwSandboxManifestPath -PathType Leaf)) {
        throw "TTW headset proof requires the owned sandbox manifest: $ttwSandboxManifestPath"
    }
    $ttwSandboxManifest = Get-Content -LiteralPath $ttwSandboxManifestPath -Raw |
        ConvertFrom-Json
    if ($ttwSandboxManifest.plan.schema -cne "fnvxr-ttw-retail-sandbox-v1" -or
        $ttwSandboxManifest.plan.profile.name -cne "ttw-core-baseline-v1") {
        throw "TTW headset proof sandbox manifest is not the exact core profile: $ttwSandboxManifestPath"
    }
    $expectedTtwPlugins = @(Get-FnvxrProductTtwBaselinePluginNames)
    if ((@($ttwSandboxManifest.plan.profile.plugins) -join "|") -cne
        ($expectedTtwPlugins -join "|")) {
        throw "TTW headset proof sandbox manifest plugin order differs from the exact core profile."
    }
    $ttwCoreProfilePlan.sandboxManifest = $ttwSandboxManifestPath
    $ttwCoreProfilePlan.pluginEntries = $expectedTtwPlugins
    $ttwCoreProfilePlan.dataVerified = $true
}
$retailPluginProfilePath = Get-FnvxrProductRetailVisualTrialPluginsPath
$retailPluginProfilePlan = [ordered]@{
    requested = [bool]$UseRetailPluginProfile
    scope = "temporary exact-official retail plugins.txt only; no save, NVSE sidecar, or game-data mutation"
    path = $retailPluginProfilePath
    entries = @(Get-FnvxrProductRetailVisualTrialPluginNames)
    dataVerified = $false
    status = if ($UseRetailPluginProfile) { "validated-not-staged" } else { "disabled" }
    before = $null
    staged = $null
    after = $null
    restored = $false
}
if ($UseRetailPluginProfile) {
    Assert-FnvxrProductRetailVisualTrialPluginData -GameRoot $game.root
    $retailPluginProfilePlan.dataVerified = $true
    if (Test-Path -LiteralPath $retailPluginProfilePath -PathType Leaf) {
        $retailPluginProfilePlan.before =
            Get-FnvxrProductFileIdentity -Path $retailPluginProfilePath
    }
}
$retailFixturePluginProfilePath = Get-FnvxrProductRetailVisualTrialPluginsPath
$retailFixturePluginProfilePlan = [ordered]@{
    requested = [bool]$retailFixtureRequested
    fixtureFamily = $fixtureFamily
    scope = if ($TtwCore) {
        "temporary exact TTW core plugins.txt profile in the isolated workspace sandbox only; no personal save, NVSE sidecar, live retail, or game-data mutation"
    } else {
        "temporary FalloutNV.esm-only fixture plugins.txt profile; no personal save, NVSE sidecar, TTW, or game-data mutation"
    }
    path = $retailFixturePluginProfilePath
    entries = if ($TtwCore) {
        @(Get-FnvxrProductTtwBaselinePluginNames)
    } else {
        @(Get-FnvxrProductRetailFixturePluginNames)
    }
    dataVerified = $false
    status = if ($retailFixtureRequested) { "validated-not-staged" } else { "disabled" }
    before = $null
    staged = $null
    after = $null
    restored = $false
}
if ($retailFixtureRequested) {
    if ($TtwCore) {
        Assert-FnvxrProductTtwBaselinePluginData -GameRoot $game.root
    } else {
        Assert-FnvxrProductRetailFixturePluginData -GameRoot $game.root
    }
    $retailFixturePluginProfilePlan.dataVerified = $true
    if (Test-Path -LiteralPath $retailFixturePluginProfilePath -PathType Leaf) {
        $retailFixturePluginProfilePlan.before =
            Get-FnvxrProductFileIdentity -Path $retailFixturePluginProfilePath
    }
}
$physicalDisplayProfilePlan = [ordered]@{
    requested = [bool]$PhysicalHeadsetPlay
    scope = "temporary 1920x1200-class retail source profile; both Fallout INIs are restored byte-for-byte after owned processes stop"
    size = $physicalDisplaySize
    paths = @(Get-FnvxrProductPhysicalDisplayIniPaths)
    status = if ($PhysicalHeadsetPlay) {
        "validated-not-staged"
    } else {
        "disabled"
    }
    before = @()
    staged = $null
    outputProof = $null
    after = $null
    restored = $false
}
if ($PhysicalHeadsetPlay) {
    foreach ($iniPath in @($physicalDisplayProfilePlan.paths)) {
        if (-not (Test-Path -LiteralPath $iniPath -PathType Leaf)) {
            throw "Physical headset play requires an existing Fallout INI: $iniPath"
        }
        $physicalDisplayProfilePlan.before +=
            Get-FnvxrProductFileIdentity -Path $iniPath
    }
}
$stagePlan = Get-FnvxrProductStagePlan `
    -Root $root `
    -Configuration $Configuration `
    -GameRoot $game.root
$artifactSnapshot = Get-FnvxrProductArtifactSnapshot -Descriptors (
    Get-FnvxrProductArtifactDescriptors -Root $root -Configuration $Configuration)
$attestationIdentity = Get-FnvxrProductFileIdentity -Path $attestationPath
$automationKind = if ($StartNewCharacter) {
    "fresh-character"
} elseif ($retailFixtureRequested) {
    if ($PhysicalHeadsetPlay) {
        "physical-headset-play-$fixtureFamily-fixture-$resolvedRetailFixtureAction"
    } elseif ($HeadsetDemoFixture) {
        "headset-demo-$fixtureFamily-fixture-$resolvedRetailFixtureAction"
    } elseif ($HeadsetWorldOnlyCapture) {
        "headset-world-$fixtureFamily-fixture-$resolvedRetailFixtureAction"
    } else {
        "$fixtureFamily-fixture-$resolvedRetailFixtureAction"
    }
} elseif ($AutomateInGame) {
    "recovery-load"
} else {
    "disabled"
}
$automationSaveName = if ($StartNewCharacter) {
    $freshCharacterName
} elseif ($retailFixtureRequested) {
    $retailFixtureSaveName
} else {
    $fixedRecoverySaveName
}
$automationFixedCommand = if ($StartNewCharacter) {
    $freshCharacterStartCommand
} elseif ($retailFixtureRequested -and $resolvedRetailFixtureAction -ceq "create") {
    $freshCharacterStartCommand
} elseif ($retailFixtureRequested) {
    "load $retailFixtureSaveName"
} else {
    $fixedRecoveryLoadCommand
}
$automationPlan = [ordered]@{
    requested = $automationRequested
    kind = $automationKind
    status = if ($automationRequested) { "validated-not-started" } else { "disabled" }
    scope = if ($StartNewCharacter) {
        "one fixed no-save COC, fixed name, and new disposable save only; no menu, input, controller, camera, rig, weapon, or general command authority"
    } elseif ($retailFixtureRequested) {
        if ($PhysicalHeadsetPlay) {
            "one existing owned $fixtureFamily fixture load under the process-local physical OpenXR runtime; exact-retail xNVSE consumes authenticated Quest controller state with explicit menu/game routing; no desktop or simulator control, personal-save mutation, tracked weapon, projectile, or unverified camera/rig authority"
        } elseif ($HeadsetDemoFixture) {
            if ($TtwCore) {
                "one owned FNVXR_AutoTTW level-one fixture in the isolated TTW workspace sandbox and process-local headless OpenXR visual trial; temporary exact TTW core profile; exact title/body/unique-OK official-pack acknowledgement if presented; exactly two fixed in-game Pip-Boy Tab events after verified gameplay; no personal save, live retail, desktop, keyboard, mouse, controller, camera, rig, weapon, simulator UI, or general command authority"
            } else {
                "one owned FNVXR_AutoRetail level-one fixture in the process-local headless OpenXR visual trial; temporary FalloutNV.esm-only profile; exact title/body/unique-OK official-pack acknowledgement if presented; exactly two fixed in-game Pip-Boy Tab events after verified gameplay; no personal save, TTW, desktop, keyboard, mouse, controller, camera, rig, weapon, simulator UI, or general command authority"
            }
        } elseif ($HeadsetWorldOnlyCapture) {
            if ($TtwCore) {
                "one owned FNVXR_AutoTTW level-one fixture in the isolated TTW workspace sandbox and process-local headless OpenXR visual trial; temporary exact TTW core profile; exact title/body/unique-OK official-pack acknowledgement if presented; sustained world-only eye capture and optional bounded per-run simulator HMD pose sweep with no Pip-Boy or game-input events; no personal save, live retail, desktop, keyboard, mouse, controller mutation, camera hook, rig, weapon, simulator UI, or general command authority"
            } else {
                "one owned FNVXR_AutoRetail level-one fixture in the process-local headless OpenXR visual trial; temporary FalloutNV.esm-only profile; exact title/body/unique-OK official-pack acknowledgement if presented; sustained world-only eye capture and optional bounded per-run simulator HMD pose sweep with no Pip-Boy or game-input events; no personal save, TTW, desktop, keyboard, mouse, controller mutation, camera hook, rig, weapon, simulator UI, or general command authority"
            }
        } else {
            if ($TtwCore) {
                "one owned FNVXR_AutoTTW level-one fixture in the isolated TTW workspace sandbox with zero, one, or two allowlisted base-game traits; temporary exact TTW core profile; exact title/body/unique-OK official-pack acknowledgement if presented; no personal save, live retail, desktop, keyboard, mouse, controller, camera, rig, weapon, simulator, or general command authority"
            } else {
                "one owned FNVXR_AutoRetail level-one fixture with zero, one, or two allowlisted base-game traits; temporary FalloutNV.esm-only profile; exact title/body/unique-OK official-pack acknowledgement if presented; no personal save, TTW, desktop, keyboard, mouse, controller, camera, rig, weapon, simulator, or general command authority"
            }
        }
    } elseif ($AcknowledgeTribalPackPopup) {
        "one exact approved retail-save load plus at most one native acknowledgement of each of four exact official pre-order-pack title/body notifications and their unique first-button OK tiles; no desktop, keyboard, mouse, controller, simulator, camera, rig, weapon, or general command authority"
    } else {
        "one exact approved retail-save load only; no menu, name, character, controller, camera, rig, weapon, or general command authority"
    }
    saveName = $automationSaveName
    fixedCommand = $automationFixedCommand
    commandArguments = if ($StartNewCharacter) {
        @("fresh-character", "--wait-ms", [string]$fixedRecoveryLoadWaitMilliseconds)
    } elseif ($retailFixtureRequested -and $resolvedRetailFixtureAction -ceq "create") {
        @("fresh-character", "--wait-ms", [string]$fixedRecoveryLoadWaitMilliseconds)
    } elseif ($retailFixtureRequested) {
        @("console", "load $retailFixtureSaveName", "--wait-ms", [string]$fixedRecoveryLoadWaitMilliseconds)
    } else {
        @("console", $fixedRecoveryLoadCommand, "--wait-ms", [string]$fixedRecoveryLoadWaitMilliseconds)
    }
    acknowledgeOfficialPackPopupsRequested =
        [bool]($AcknowledgeTribalPackPopup -or $retailFixtureRequested)
    controllerMutationRequested = [bool]$PhysicalHeadsetPlay
    controllerMutationAuthorized = $false
    trackedWeaponAuthorized = $false
    startMenuEvidence = $null
    commandInvocation = $null
    gameplayEvidence = $null
    createdSave = $null
    saveAfter = $null
    nvseAfter = $null
    loadOnlySaveUnchanged = $null
    loadOnlyNvseUnchanged = $null
    fixture = if ($retailFixtureRequested) {
        [ordered]@{
            action = $resolvedRetailFixtureAction
            saveName = $retailFixtureSaveName
            traitOne = $retailFixtureTraits.first
            traitTwo = $retailFixtureTraits.second
            savePath = $retailFixtureSavePath
            nvsePath = $retailFixtureNvsePath
        }
    } else {
        $null
    }
}
if ($AutomateInGame) {
    if (-not (Test-Path -LiteralPath $fixedRecoverySavePath -PathType Leaf) -or
        -not (Test-Path -LiteralPath $fixedRecoveryNvsePath -PathType Leaf)) {
        throw (
            "-AutomateInGame requires the verified load-only save and NVSE sidecar: {0}, {1}" -f
            $fixedRecoverySavePath,
            $fixedRecoveryNvsePath)
    }
}
if ($StartNewCharacter) {
    if ((Test-Path -LiteralPath $freshCharacterSavePath -PathType Leaf) -or
        (Test-Path -LiteralPath $freshCharacterNvsePath -PathType Leaf)) {
        throw (
            "-StartNewCharacter refuses to overwrite an existing disposable save or NVSE sidecar: {0}, {1}" -f
            $freshCharacterSavePath,
            $freshCharacterNvsePath)
    }
}
if ($retailFixtureRequested -and $resolvedRetailFixtureAction -ceq "load") {
    if (-not (Test-Path -LiteralPath $retailFixtureSavePath -PathType Leaf) -or
        -not (Test-Path -LiteralPath $retailFixtureNvsePath -PathType Leaf)) {
        throw "Retail fixture load requires its owned save and NVSE sidecar: $retailFixtureSavePath, $retailFixtureNvsePath"
    }
}
if ($automationRequested) {
    $commandIdentity = Get-FnvxrProductFileIdentity -Path $commandPath -RequirePe
    if ($commandIdentity.peMachine -cne "0x8664") {
        throw "The fixed visual-trial command helper is not x64: $commandPath"
    }
    $attestedCommand = @($artifactSnapshot.records | Where-Object {
        $_.key -ceq "x64/fnvxr_command.exe"
    })
    if ($attestedCommand.Count -ne 1 -or
        $attestedCommand[0].sha256 -cne $commandIdentity.sha256) {
        throw "The fixed visual-trial command helper is not part of the attested artifact set."
    }
    if ($AutomateInGame) {
        $automationPlan.save = Get-FnvxrProductFileIdentity -Path $fixedRecoverySavePath
        $automationPlan.nvse = Get-FnvxrProductFileIdentity -Path $fixedRecoveryNvsePath
    }
    if ($retailFixtureRequested -and $resolvedRetailFixtureAction -ceq "load") {
        $automationPlan.save = Get-FnvxrProductFileIdentity -Path $retailFixtureSavePath
        $automationPlan.nvse = Get-FnvxrProductFileIdentity -Path $retailFixtureNvsePath
    }
    $automationPlan.commandHelper = $commandIdentity
}
$openXrRuntimePlan = [ordered]@{
    requested = [bool]$selectedRuntimeIdentity
    status = if ($selectedRuntimeIdentity) {
        "validated-not-started"
    } else {
        "inherited-not-overridden"
    }
    selection = if ($selectedRuntimeIdentity) {
        "process-local-environment-only"
    } else {
        "inherited"
    }
    headless = [bool]$headlessRuntimeIdentity
    physicalHeadset = [bool]$physicalRuntimeIdentity
    registryMutationAuthorized = $false
    identity = $selectedRuntimeIdentity
    hklmActiveRuntimeBefore = $runtimeRegistryBefore
    hklmActiveRuntimeAfter = $null
    hklmActiveRuntimeUnchanged = $null
    processLocalEnvironmentRestored = $null
    runtimeLogIdentity = $null
    metaXrOperator = [ordered]@{
        requested = [bool]$metaXrOperatorIdentity
        status = if ($metaXrOperatorIdentity) {
            "validated-not-started"
        } else {
            "disabled"
        }
        identity = $metaXrOperatorIdentity
        expectedMcpPort = if ($metaXrOperatorIdentity) { 8720 } else { $null }
        listenerBefore = $null
        hostOwnedListener = $null
        mcpProxyLaunched = $false
        controllerOrPoseOverrideAuthorized = $false
        processLocalEnvironmentRestored = $null
    }
}

if ($ValidateOnly) {
    if ($runtimeRegistryBefore) {
        $openXrRuntimePlan.hklmActiveRuntimeAfter =
            Assert-FnvxrProductHklmActiveRuntimeUnchanged `
                -Before $runtimeRegistryBefore
        $openXrRuntimePlan.hklmActiveRuntimeUnchanged = $true
    }
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
        ttwCoreProfile = $ttwCoreProfilePlan
        retailPluginProfile = $retailPluginProfilePlan
        retailFixturePluginProfile = $retailFixturePluginProfilePlan
        physicalDisplayProfile = $physicalDisplayProfilePlan
        automation = $automationPlan
        openXrRuntime = $openXrRuntimePlan
        minimalEnvironmentKeys = @(Get-FnvxrProductMinimalEnvironment `
            -RunId "validate-only" `
            -RunDirectory "validate-only" `
            -OpenXrLoaderPath $stagedLoaderPath `
            -SessionReadyTimeoutSeconds $RetailReadyTimeoutSeconds `
            -AutomateRecoveryLoad:$AutomateInGame `
            -AutomateFreshCharacter:$StartNewCharacter `
            -AutomateRetailFixture:$retailFixtureRequested `
            -TtwFixture:$TtwCore `
            -HeadsetDemoFixture:$HeadsetDemoFixture `
            -HeadsetWorldOnlyCapture:$HeadsetWorldOnlyCapture `
            -PhysicalHeadsetPlay:$PhysicalHeadsetPlay `
            -PhysicalGameWidth $PhysicalGameWidth `
            -PhysicalGameHeight $PhysicalGameHeight `
            -HeadsetDemoGameplayWarmupFrames $HeadsetDemoGameplayWarmupFrames `
            -RetailVrAccumulationDiagnosticMode $RetailVrAccumulationDiagnosticMode `
            -RetailFixtureAction $resolvedRetailFixtureAction `
            -RetailFixtureSaveName $retailFixtureSaveName `
            -RetailFixtureTraitOne $(if ($retailFixtureRequested) { $retailFixtureTraits.first } else { "None" }) `
            -RetailFixtureTraitTwo $(if ($retailFixtureRequested) { $retailFixtureTraits.second } else { "None" }) `
            -AcknowledgeTribalPackPopup:$AcknowledgeTribalPackPopup `
            -AutomateRecoverySaveName $RetailSaveName `
            -HeadlessRuntimeManifest $headlessRuntimeManifestPath `
            -PhysicalRuntimeManifest $physicalRuntimeManifestPath `
            -HeadsetMirrorCaptureDirectory $validateHeadsetMirrorDirectory `
            -HeadsetMirrorCaptureEveryFrames $HeadsetMirrorCaptureEveryFrames `
            -HeadsetMirrorCaptureMaxPairs $HeadsetMirrorCaptureMaxPairs `
            -MetaXrOperatorLayerDirectory $(if ($metaXrOperatorIdentity) {
                $metaXrOperatorIdentity.directory
            } else { "" })).Keys
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
$retailVrLog = Join-Path $runDirectory "fnvxr_retail_vr.log"
$headsetDemoInputTelemetryLog = Join-Path $runDirectory "fnvxr_input_telemetry.log"
$automationCommandLog = Join-Path $runDirectory "automation-command.log"
$openXrSimulatorDataDirectory = Join-Path $runDirectory "openxr-simulator"
$openXrSimulatorLog = Join-Path $runDirectory "openxr-simulator.log"
$launcherLog = Join-Path $runDirectory "supervisor.log"
$headsetMirrorDirectory = if ($CaptureHeadsetMirror) {
    Join-Path $runDirectory "headset-mirror"
} else {
    ""
}
New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
if ($CaptureHeadsetMirror) {
    New-Item -ItemType Directory -Path $headsetMirrorDirectory -Force | Out-Null
}
if ($headlessRuntimeIdentity) {
    $openXrRuntimePlan.dataDirectory = $openXrSimulatorDataDirectory
    $openXrRuntimePlan.logPath = $openXrSimulatorLog
}

$manifest = [ordered]@{
    schema = 1
    product = "fnvxr-v5-exact-retail"
    acceptanceScope = if ($PhysicalHeadsetPlay) {
        "physical-headset-interactive-play"
    } else {
        "stereo-visual-trial-only"
    }
    runId = $runId
    supervisorProcessId = $PID
    startedAtUtc = [DateTime]::UtcNow.ToString("o")
    state = "initializing"
    accepted = $false
    trialReady = $false
    fullProductAccepted = $false
    controllerMutationRequested = [bool]$PhysicalHeadsetPlay
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
    automation = $automationPlan
    openXrRuntime = $openXrRuntimePlan
    physicalHeadsetPlay = [ordered]@{
        requested = [bool]$PhysicalHeadsetPlay
        profile = if ($PhysicalHeadsetPlay) {
            "retail-vr-play-v1"
        } else {
            "disabled"
        }
        controllerConsumerAcknowledged = $false
        controllerMode = "unknown"
        status = if ($PhysicalHeadsetPlay) {
            "pending-exact-retail-controller-ack"
        } else {
            "disabled"
        }
    }
    physicalDisplayProfile = $physicalDisplayProfilePlan
    headsetMirror = [ordered]@{
        requested = [bool]$CaptureHeadsetMirror
        scope = "bounded final OpenXR eye-swapchain image sequence only; no game, desktop, controller, or simulator UI input"
        directory = $headsetMirrorDirectory
        everyFrames = $HeadsetMirrorCaptureEveryFrames
        maximumPairs = $HeadsetMirrorCaptureMaxPairs
        status = if ($CaptureHeadsetMirror) { "directory-created" } else { "disabled" }
    }
    headsetDemo = [ordered]@{
        requested = [bool]$HeadsetDemoFixture
        fixtureFamily = $fixtureFamily
        scope = if ($TtwCore) {
            "owned FNVXR_AutoTTW fixture plus exactly two in-game Pip-Boy Tab events; no OS, desktop, controller, or simulator input"
        } else {
            "owned FNVXR_AutoRetail fixture plus exactly two in-game Pip-Boy Tab events; no OS, desktop, controller, or simulator input"
        }
        status = if ($HeadsetDemoFixture) { "pending-owned-fixture-load" } else { "disabled" }
        inputTelemetry = $headsetDemoInputTelemetryLog
        pipBoyOutputProof = $null
        inputProof = $null
    }
    headsetWorldCapture = [ordered]@{
        requested = [bool]$HeadsetWorldOnlyCapture
        fixtureFamily = $fixtureFamily
        scope = "owned fixture world-only final OpenXR eye-swapchain capture; no Pip-Boy, OS, desktop, keyboard, mouse, controller, weapon, or simulator input"
        status = if ($HeadsetWorldOnlyCapture) {
            "pending-owned-fixture-load"
        } else {
            "disabled"
        }
        continuityProof = $null
    }
    headsetPoseSweep = [ordered]@{
        requested = [bool]$HeadsetPoseSweep
        scope = "bounded per-run headless-runtime HMD translation and rotation commands only; controllers retain tracked head-relative poses; no game, xNVSE input, desktop, registry, or simulator GUI control"
        durationSeconds = $HeadsetPoseSweepSeconds
        status = if ($HeadsetPoseSweep) { "pending-gameplay" } else { "disabled" }
        evidence = $null
    }
    retailPluginProfile = $retailPluginProfilePlan
    ttwCoreProfile = $ttwCoreProfilePlan
    retailFixturePluginProfile = $retailFixturePluginProfilePlan
    staged = @()
    processes = [ordered]@{}
    readiness = [ordered]@{
        hostBridge = $false
        hostPose = $false
        retailRuntimeAndPose = $false
        exactModules = $false
        retailVrBridge = $false
        automatedGameplay = $false
        automatedFreshCharacter = $false
        automatedRetailFixture = $false
        stereoOutput = $false
        headsetDemoPipBoyUi = $false
    }
    cleanup = [ordered]@{
        falloutStopped = $false
        nvseLoaderStopped = $false
        hostStopped = $false
        stageRestorationRequired = $false
        stagedArtifactsRestored = $false
        failedStageRolledBack = $false
    }
    logs = [ordered]@{
        supervisor = $launcherLog
        hostStdout = $hostOut
        hostStderr = $hostErr
        readinessProbe = $probeLog
        retailVrBridge = $retailVrLog
        headsetDemoInputTelemetry = $headsetDemoInputTelemetryLog
        automationCommand = $automationCommandLog
        openXrSimulator = $openXrSimulatorLog
    }
}
Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath

$savedEnvironment = @{}
foreach ($entry in @(Get-FnvxrProductProcessEnvironmentEntries -Prefix "FNVXR_")) {
    $savedEnvironment[$entry.Name] = $entry.Value
}
$processLocalRuntimeEnvironmentNames = @(
    "XR_RUNTIME_JSON",
    "XR_API_LAYER_PATH",
    "XR_ENABLE_API_LAYERS",
    "OPENXR_SIMULATOR_HEADLESS",
    "OPENXR_SIMULATOR_DATA_DIR",
    "OPENXR_SIMULATOR_LOG_PATH")
foreach ($name in $processLocalRuntimeEnvironmentNames) {
    $value = [Environment]::GetEnvironmentVariable(
        $name,
        [EnvironmentVariableTarget]::Process)
    if ($null -ne $value) { $savedEnvironment[$name] = $value }
}
$hostProcess = $null
$nvse = $null
$fallout = $null
$staged = @()
$retailPluginProfileRecord = $null
$retailFixturePluginProfileRecord = $null
$physicalDisplayProfileRecord = $null
$normalCompletion = $false

function Write-SupervisorLog {
    param([string]$Message)
    Add-Content -LiteralPath $launcherLog -Value ("{0} {1}" -f [DateTime]::UtcNow.ToString("o"), $Message) -Encoding UTF8
}

function Get-FnvxrProductMetaXrOperatorListeners {
    try {
        return @(
            Get-NetTCPConnection -State Listen -LocalPort 8720 -ErrorAction Stop |
                Select-Object LocalAddress,LocalPort,OwningProcess)
    } catch {
        throw "Cannot read the Meta XR Operator observation port 8720. $($_.Exception.Message)"
    }
}

function Wait-FnvxrProductMetaXrOperatorHostListener {
    param(
        [Parameter(Mandatory = $true)][uint32]$ExpectedProcessId,
        [ValidateRange(1, 60)][int]$TimeoutSeconds = 15
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        foreach ($listener in @(Get-FnvxrProductMetaXrOperatorListeners)) {
            if ([uint32]$listener.OwningProcess -eq $ExpectedProcessId) {
                return [pscustomobject][ordered]@{
                    localAddress = [string]$listener.LocalAddress
                    localPort = [uint16]$listener.LocalPort
                    owningProcessId = [uint32]$listener.OwningProcess
                    observedAtUtc = [DateTime]::UtcNow.ToString("o")
                }
            }
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)

    throw "Meta XR Operator did not open its read-only MCP observation listener for the owned OpenXR host within $TimeoutSeconds seconds."
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

    $expectedFullPath = [System.IO.Path]::GetFullPath($ExpectedPath)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $Process.Refresh()
        if ($Process.HasExited) { throw "Fallout exited before loading $ExpectedPath." }
        foreach ($module in @(Get-FnvxrProductLoadedModuleCensus -ProcessId ([uint32]$Process.Id))) {
            if ([string]::Equals($module.path, $expectedFullPath, [System.StringComparison]::OrdinalIgnoreCase)) {
                $identity = Get-FnvxrProductFileIdentity -Path $module.path -RequirePe
                if ($identity.sha256 -cne $ExpectedSha256) {
                    throw "Loaded module hash differs from staged module: $ExpectedPath"
                }
                $identity | Add-Member -NotePropertyName loadedModuleBaseAddress `
                    -NotePropertyValue ("0x{0:x}" -f [uint64]$module.baseAddress)
                $identity | Add-Member -NotePropertyName loadedModuleImageSize `
                    -NotePropertyValue ([uint32]$module.imageSize)
                $identity | Add-Member -NotePropertyName loadedModuleCensus `
                    -NotePropertyValue ([string]$module.census)
                return $identity
            }
        }
        Start-Sleep -Milliseconds 200
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Timed out waiting for exact loaded module: $ExpectedPath"
}

function Wait-FnvxrProductHostBridgeReady {
    param(
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$Process,
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds
    )

    $pattern = "fnvxrHostBridgeReady xrSessionCreated=1 sharedMappingsReady=1"
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $Process.Refresh()
        if ($Process.HasExited) { return $false }
        if (Test-Path -LiteralPath $LogPath -PathType Leaf) {
            $ready = Select-String `
                -LiteralPath $LogPath `
                -Pattern $pattern `
                -SimpleMatch `
                -Quiet `
                -ErrorAction SilentlyContinue
            if ($ready) {
                $Process.Refresh()
                return -not $Process.HasExited
            }
        }
        Start-Sleep -Milliseconds 200
    } while ([DateTime]::UtcNow -lt $deadline)
    return $false
}

function Wait-FnvxrProductLogPattern {
    param(
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$Process,
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $Process.Refresh()
        if ($Process.HasExited) { return $false }
        if ((Test-Path -LiteralPath $LogPath -PathType Leaf) -and
            (Select-String -LiteralPath $LogPath -Pattern $Pattern `
                -SimpleMatch -Quiet -ErrorAction SilentlyContinue)) {
            return $true
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    return $false
}

function Get-FnvxrProductProbeSnapshot {
    param(
        [Parameter(Mandatory = $true)][string]$ProbePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$LogPath
    )

    $output = & $ProbePath @Arguments 2>&1 | Out-String
    $exitCode = $LASTEXITCODE
    Add-Content -LiteralPath $LogPath -Value $output -Encoding UTF8
    $json = $null
    try {
        $json = $output | ConvertFrom-Json -ErrorAction Stop
    } catch {}
    return [pscustomobject][ordered]@{
        exitCode = $exitCode
        json = $json
    }
}

function Wait-FnvxrProductProbeCondition {
    param(
        [Parameter(Mandatory = $true)][string]$ProbePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$RequiredProcess,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds,
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string]$Description,
        [Parameter(Mandatory = $true)][scriptblock]$Accept
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $RequiredProcess.Refresh()
        if ($RequiredProcess.HasExited) {
            $exitCode = try { [string]$RequiredProcess.ExitCode } catch { "unknown" }
            throw "$Description failed because FalloutNV:$($RequiredProcess.Id) exited with code $exitCode."
        }
        $sample = Get-FnvxrProductProbeSnapshot `
            -ProbePath $ProbePath `
            -Arguments $Arguments `
            -LogPath $LogPath
        $accepted = $false
        if ($sample.exitCode -eq 0 -and $sample.json) {
            try {
                $accepted = [bool](& $Accept $sample.json)
            } catch {
                # A probe can legitimately publish a partial object while its
                # mappings are coming online.  Missing fields mean "not ready";
                # they must not abort the bounded supervisor under StrictMode.
                $accepted = $false
            }
        }
        if ($accepted) {
            return $sample
        }
        Start-Sleep -Milliseconds 200
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Timed out after $TimeoutSeconds seconds waiting for $Description."
}

function Get-FnvxrProductProbeField {
    param(
        [AllowNull()]$Object,
        [Parameter(Mandatory = $true)][string]$Name,
        [AllowNull()]$DefaultValue = $null
    )

    if ($null -eq $Object) {
        return $DefaultValue
    }
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        return $DefaultValue
    }
    return $property.Value
}

function ConvertTo-FnvxrProductRuntimeEvidence {
    param([Parameter(Mandatory = $true)]$Sample)

    $runtime = $Sample.json.runtime
    $player = $Sample.json.player
    return [ordered]@{
        capturedAtUtc = [DateTime]::UtcNow.ToString("o")
        probeExitCode = [int]$Sample.exitCode
        present = [bool](Get-FnvxrProductProbeField -Object $runtime -Name "present" -DefaultValue $false)
        stable = [bool](Get-FnvxrProductProbeField -Object $runtime -Name "stable" -DefaultValue $false)
        usable = [bool](Get-FnvxrProductProbeField -Object $runtime -Name "usable" -DefaultValue $false)
        sequenceBefore = [int64](Get-FnvxrProductProbeField -Object $runtime -Name "sequenceBefore" -DefaultValue 0)
        sequenceAfter = [int64](Get-FnvxrProductProbeField -Object $runtime -Name "sequenceAfter" -DefaultValue 0)
        frame = [uint64](Get-FnvxrProductProbeField -Object $runtime -Name "frame" -DefaultValue 0)
        phase = [uint32](Get-FnvxrProductProbeField -Object $runtime -Name "phase" -DefaultValue 0)
        menuBits = [uint32](Get-FnvxrProductProbeField -Object $runtime -Name "menuBits" -DefaultValue 0)
        uiInputAllowed = [bool](Get-FnvxrProductProbeField -Object $runtime -Name "uiInputAllowed" -DefaultValue $false)
        cameraActive = [bool](Get-FnvxrProductProbeField -Object $runtime -Name "cameraActive" -DefaultValue $false)
        showroomActive = [bool](Get-FnvxrProductProbeField -Object $runtime -Name "showroomActive" -DefaultValue $false)
        scene = [ordered]@{
            playerPresent = [bool](Get-FnvxrProductProbeField -Object $player -Name "present" -DefaultValue $false)
            playerStable = [bool](Get-FnvxrProductProbeField -Object $player -Name "stable" -DefaultValue $false)
            playerUsable = [bool](Get-FnvxrProductProbeField -Object $player -Name "usable" -DefaultValue $false)
            playerNodeValid = [bool](Get-FnvxrProductProbeField -Object $player -Name "playerNodeValid" -DefaultValue $false)
            cameraNodeValid = [bool](Get-FnvxrProductProbeField -Object $player -Name "cameraValid" -DefaultValue $false)
            cellKnown = [bool](Get-FnvxrProductProbeField -Object $player -Name "cellKnown" -DefaultValue $false)
            gameplay = [bool](Get-FnvxrProductProbeField -Object $player -Name "gameplay" -DefaultValue $false)
            currentCellFormId = [uint32](Get-FnvxrProductProbeField -Object $player -Name "currentCellFormId" -DefaultValue 0)
            playerAddress = [uint64](Get-FnvxrProductProbeField -Object $player -Name "playerAddress" -DefaultValue 0)
            playerNodeAddress = [uint64](Get-FnvxrProductProbeField -Object $player -Name "playerNodeAddress" -DefaultValue 0)
            cameraNodeAddress = [uint64](Get-FnvxrProductProbeField -Object $player -Name "cameraNodeAddress" -DefaultValue 0)
        }
    }
}

function Test-FnvxrProductLoadOnlySaveUnchanged {
    param(
        [Parameter(Mandatory = $true)]$Before,
        [Parameter(Mandatory = $true)]$After
    )

    return [string]::Equals(
            [string]$Before.path,
            [string]$After.path,
            [System.StringComparison]::OrdinalIgnoreCase) `
        -and [uint64]$Before.length -eq [uint64]$After.length `
        -and [string]$Before.sha256 -ceq [string]$After.sha256
}

function Wait-FnvxrProductStartMenuForRecoveryLoad {
    param(
        [Parameter(Mandatory = $true)][string]$ProbePath,
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$RequiredProcess,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds,
        [Parameter(Mandatory = $true)][string]$LogPath
    )

    return Wait-FnvxrProductProbeCondition `
        -ProbePath $ProbePath `
        -Arguments @("--require-runtime", "--require-advancing", "--sample-delay-ms", "100") `
        -RequiredProcess $RequiredProcess `
        -TimeoutSeconds $TimeoutSeconds `
        -LogPath $LogPath `
        -Description "fresh real Fallout Start Menu evidence before the fixed visual-trial command" `
        -Accept {
            param($snapshot)
            return [bool]$snapshot.runtime.present `
                -and [bool]$snapshot.runtime.stable `
                -and [bool]$snapshot.runtime.usable `
                -and [uint64]$snapshot.runtime.frame -gt 0 `
                -and [uint32]$snapshot.runtime.phase -eq 1 `
                -and (([uint32]$snapshot.runtime.menuBits -band 2) -ne 0) `
                -and [bool]$snapshot.runtime.uiInputAllowed `
                -and -not [bool]$snapshot.runtime.showroomActive
        }
}

function Wait-FnvxrProductLoadedGameplay {
    param(
        [Parameter(Mandatory = $true)][string]$ProbePath,
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$RequiredProcess,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds,
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][uint64]$MinimumFrame
    )

    return Wait-FnvxrProductProbeCondition `
        -ProbePath $ProbePath `
        -Arguments @("--require-player", "--require-runtime", "--require-advancing", "--sample-delay-ms", "100") `
        -RequiredProcess $RequiredProcess `
        -TimeoutSeconds $TimeoutSeconds `
        -LogPath $LogPath `
        -Description "fresh loaded real gameplay scene with a player cell, active camera, and UI closed" `
        -Accept {
            param($snapshot)
            return [bool]$snapshot.runtime.present `
                -and [bool]$snapshot.runtime.stable `
                -and [bool]$snapshot.runtime.usable `
                -and [uint64]$snapshot.runtime.frame -gt $MinimumFrame `
                -and [uint32]$snapshot.runtime.phase -eq 3 `
                # RuntimeMenuModeBit (0x01) is diagnostic only and the shared
                # protocol explicitly excludes it from RuntimeBlockingMenuBits.
                -and (([uint32]$snapshot.runtime.menuBits -band 0xFE) -eq 0) `
                -and -not [bool]$snapshot.runtime.uiInputAllowed `
                -and [bool]$snapshot.runtime.cameraActive `
                -and -not [bool]$snapshot.runtime.showroomActive `
                -and [bool]$snapshot.player.present `
                -and [bool]$snapshot.player.stable `
                -and [bool]$snapshot.player.usable `
                -and [bool]$snapshot.player.playerNodeValid `
                -and [bool]$snapshot.player.cameraValid `
                -and [bool]$snapshot.player.cellKnown `
                -and [bool]$snapshot.player.gameplay `
                -and [uint32]$snapshot.player.currentCellFormId -ne 0 `
                -and [uint64]$snapshot.player.playerAddress -ne 0 `
                -and [uint64]$snapshot.player.playerNodeAddress -ne 0 `
                -and [uint64]$snapshot.player.cameraNodeAddress -ne 0
        }
}

function Invoke-FnvxrProductFixedRecoveryLoad {
    param(
        [Parameter(Mandatory = $true)][string]$CommandPath,
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)]
        [ValidateSet("FNVXR_StereoTest")]
        [string]$RetailSaveName,
        [Parameter(Mandatory = $true)]
        [ValidateRange(5000, 900000)]
        [int]$WaitMilliseconds
    )

    # This is the only command submitted by the visual-trial launcher. The
    # plugin's opt-in publication path rejects all other mailbox actions.
    $fixedCommand =
        Get-FnvxrProductApprovedRetailSaveLoadCommand -RetailSaveName $RetailSaveName
    $output = & $CommandPath console $fixedCommand --wait-ms $WaitMilliseconds 2>&1 |
        Out-String
    $exitCode = $LASTEXITCODE
    Add-Content -LiteralPath $LogPath -Value $output -Encoding UTF8
    return [ordered]@{
        invokedAtUtc = [DateTime]::UtcNow.ToString("o")
        executable = $CommandPath
        arguments = @("console", $fixedCommand, "--wait-ms", [string]$WaitMilliseconds)
        fixedCommand = $fixedCommand
        waitMilliseconds = $WaitMilliseconds
        exitCode = $exitCode
        completed = $exitCode -eq 0
        output = $output
    }
}

function Invoke-FnvxrProductFreshCharacter {
    param(
        [Parameter(Mandatory = $true)][string]$CommandPath,
        [Parameter(Mandatory = $true)][string]$LogPath,
        [string]$FixtureSaveName = "",
        [Parameter(Mandatory = $true)]
        [ValidateRange(5000, 900000)]
        [int]$WaitMilliseconds
    )

    # The launcher submits one argument-free helper verb. The xNVSE plugin
    # accepts only its exact COC payload, then owns the fixed name/save steps.
    $output = & $CommandPath fresh-character --wait-ms $WaitMilliseconds 2>&1 |
        Out-String
    $exitCode = $LASTEXITCODE
    Add-Content -LiteralPath $LogPath -Value $output -Encoding UTF8
    $reportedSaveName = if ([string]::IsNullOrWhiteSpace($FixtureSaveName)) {
        "FNVXR_StereoTest"
    } else {
        Assert-FnvxrProductRetailFixtureSaveName -SaveName $FixtureSaveName
    }
    return [ordered]@{
        invokedAtUtc = [DateTime]::UtcNow.ToString("o")
        executable = $CommandPath
        arguments = @("fresh-character", "--wait-ms", [string]$WaitMilliseconds)
        fixedCommand = Get-FnvxrProductFreshCharacterStartCommand
        fixedName = if ($reportedSaveName -ceq "FNVXR_StereoTest") {
            "FNVXR_StereoTest"
        } else {
            "FNVXR_AutoRetail"
        }
        fixedSaveName = $reportedSaveName
        waitMilliseconds = $WaitMilliseconds
        exitCode = $exitCode
        completed = $exitCode -eq 0
        output = $output
    }
}

function Invoke-FnvxrProductRetailFixtureLoad {
    param(
        [Parameter(Mandatory = $true)][string]$CommandPath,
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string]$SaveName,
        # The visual trial may load the separate AutoTTW lineage only after
        # -TtwCore has selected the isolated sandbox and exact TTW profile.
        # Keep the retail default strict for every other caller.
        [switch]$TtwFixture,
        [Parameter(Mandatory = $true)]
        [ValidateRange(5000, 900000)]
        [int]$WaitMilliseconds
    )

    $ownedSaveName = if ($TtwFixture) {
        Assert-FnvxrProductTtwFixtureSaveName -SaveName $SaveName
    } else {
        Assert-FnvxrProductRetailFixtureSaveName -SaveName $SaveName
    }
    $fixedCommand = "load $ownedSaveName"
    $output = & $CommandPath console $fixedCommand --wait-ms $WaitMilliseconds 2>&1 |
        Out-String
    $exitCode = $LASTEXITCODE
    Add-Content -LiteralPath $LogPath -Value $output -Encoding UTF8
    return [ordered]@{
        invokedAtUtc = [DateTime]::UtcNow.ToString("o")
        executable = $CommandPath
        arguments = @("console", $fixedCommand, "--wait-ms", [string]$WaitMilliseconds)
        fixedCommand = $fixedCommand
        fixtureSaveName = $ownedSaveName
        waitMilliseconds = $WaitMilliseconds
        exitCode = $exitCode
        completed = $exitCode -eq 0
        output = $output
    }
}

function Wait-FnvxrProductFreshCharacterSave {
    param(
        [Parameter(Mandatory = $true)][string]$SavePath,
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$RequiredProcess,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $RequiredProcess.Refresh()
        if ($RequiredProcess.HasExited) {
            throw "Fallout exited before the fresh disposable retail save appeared: $SavePath"
        }
        if (Test-Path -LiteralPath $SavePath -PathType Leaf) {
            return Get-FnvxrProductFileIdentity -Path $SavePath
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)

    throw "Timed out waiting for the fresh disposable retail save: $SavePath"
}

function ConvertTo-FnvxrProductStereoOutputProof {
    param([Parameter(Mandatory = $true)]$Frame)

    $gpuProof =
        [bool]$Frame.stereoVisualTrialActive -and
        [uint64]$Frame.gpuV5Transaction -gt 0 -and
        [uint64]$Frame.gpuV5SourceFrame -gt 0 -and
        [uint64]$Frame.gpuV5PoseSequence -gt 0 -and
        [bool]$Frame.gpuV5RuntimeLineage -and
        [bool]$Frame.gpuV5ExactSourceView
    $cpuProof =
        [bool]$Frame.cpuEngineStereoActive -and
        [int]$Frame.cpuEngineProducerMode -eq 4 -and
        [uint64]$Frame.cpuEngineTransaction -gt 0 -and
        [bool]$Frame.cpuEngineRuntimeLineage -and
        [uint64]$Frame.cpuEngineSourceRuntimeSample -gt 0 -and
        [uint64]$Frame.sourceRenderPairSequence -gt 0 -and
        [uint64]$Frame.sourcePoseSequence -gt 0 -and
        [uint64]$Frame.sourceReferenceSpaceGeneration -gt 0 -and
        [string]$Frame.sourcePoseProducerEpoch -ne "0" -and
        [string]$Frame.sourceRendererProducerEpoch -ne "0" -and
        [uint32]$Frame.sourceProducerProcessId -gt 0 -and
        [bool]$Frame.sourcePoseAgeValid -and
        [int]$Frame.nonBlackSamples -gt 0 -and
        [int]$Frame.meaningfulDifferentSamples -gt 0 -and
        [int]$Frame.leftActiveTiles -gt 0 -and
        [int]$Frame.rightActiveTiles -gt 0 -and
        [int]$Frame.differentTiles -gt 0 -and
        [string]$Frame.leftHash -ne "0x0" -and
        [string]$Frame.rightHash -ne "0x0" -and
        [string]$Frame.leftHash -ne [string]$Frame.rightHash
    if (-not (($gpuProof -or $cpuProof) -and
            [bool]$Frame.stereoFullscreen -and
            [bool]$Frame.runtimeGameplay -and
            [bool]$Frame.runtimeShouldRender -and
            [bool]$Frame.projectionLayerSubmitted -and
            [int]$Frame.layerCount -eq 1 -and
            [string]$Frame.xrEndFrame -eq "XR_SUCCESS" -and
            [bool]$Frame.leftOutputProof -and
            [bool]$Frame.rightOutputProof -and
            [int]$Frame.leftOutputNonBlackSamples -gt 0 -and
            [int]$Frame.rightOutputNonBlackSamples -gt 0 -and
            [int]$Frame.leftOutputVariedSamples -gt 0 -and
            [int]$Frame.rightOutputVariedSamples -gt 0 -and
            [string]$Frame.leftOutputHash -ne "0x0" -and
            [string]$Frame.rightOutputHash -ne "0x0" -and
            [string]$Frame.leftOutputHash -ne
                [string]$Frame.rightOutputHash)) {
        return $null
    }

    $transport = if ($cpuProof) { "cpu-engine-v8" } else { "gpu-v5" }
    $transaction = if ($cpuProof) {
        [uint64]$Frame.cpuEngineTransaction
    } else {
        [uint64]$Frame.gpuV5Transaction
    }
    $sourceFrame = if ($cpuProof) {
        [uint64]$Frame.sourcePoseSequence
    } else {
        [uint64]$Frame.gpuV5SourceFrame
    }
    $poseSequence = if ($cpuProof) {
        [uint64]$Frame.sourcePoseSequence
    } else {
        [uint64]$Frame.gpuV5PoseSequence
    }
    return [ordered]@{
        frame = [uint64]$Frame.frame
        hostWallClockUnixMilliseconds =
            [uint64]$Frame.hostWallClockUnixMilliseconds
        transport = $transport
        transaction = $transaction
        sourceFrame = $sourceFrame
        poseSequence = $poseSequence
        leftOutputHash = [string]$Frame.leftOutputHash
        rightOutputHash = [string]$Frame.rightOutputHash
        observedAtUtc = [DateTime]::UtcNow.ToString("o")
    }
}

function Get-FnvxrProductStereoOutputProof {
    param([Parameter(Mandatory = $true)][string]$LogPath)

    if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
        return $null
    }
    $lines = @(Get-Content -LiteralPath $LogPath -Tail 300)
    [array]::Reverse($lines)
    foreach ($line in $lines) {
        if (-not $line.StartsWith('{"event":"fnvxrOpenXrSubmit"')) {
            continue
        }
        try {
            $frame = $line | ConvertFrom-Json -ErrorAction Stop
            $proof = ConvertTo-FnvxrProductStereoOutputProof -Frame $frame
            if ($proof) { return $proof }
        } catch {
            continue
        }
    }
    return $null
}

function Get-FnvxrProductControllerAuthorizationProof {
    param([Parameter(Mandatory = $true)][string]$LogPath)

    if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
        return $null
    }
    $lines = @(Get-Content -LiteralPath $LogPath -Tail 600)
    [array]::Reverse($lines)
    foreach ($line in $lines) {
        if (-not $line.StartsWith('{"event":"fnvxrOpenXrSubmit"')) {
            continue
        }
        try {
            $frame = $line | ConvertFrom-Json -ErrorAction Stop
        } catch {
            continue
        }
        if ([bool]$frame.physicalHeadsetPlayRequested -and
            [bool]$frame.controllerConsumerAcknowledged -and
            [bool]$frame.controllerMutationAuthorized -and
            [string]$frame.controllerMode -in @("ui", "gameplay")) {
            return [ordered]@{
                frame = [uint64]$frame.frame
                mode = [string]$frame.controllerMode
                presentationMode = [string]$frame.cpuPresentationMode
                runtimeStateSample =
                    [uint64]$frame.controllerRuntimeStateSample
                consumerAcknowledged = $true
                mutationAuthorized = $true
                observedAtUtc = [DateTime]::UtcNow.ToString("o")
            }
        }
    }
    return $null
}

function Get-FnvxrProductPhysicalDisplayOutputProof {
    param(
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][int]$ExpectedWidth,
        [Parameter(Mandatory = $true)][int]$ExpectedHeight
    )

    if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
        return $null
    }
    $lines = @(Get-Content -LiteralPath $LogPath -Tail 1200)
    [array]::Reverse($lines)
    foreach ($line in $lines) {
        $jsonStart = $line.IndexOf('{"event":"fnvxrRetailEngineCenterCpu')
        if ($jsonStart -lt 0) {
            continue
        }
        try {
            $frame = $line.Substring($jsonStart) |
                ConvertFrom-Json -ErrorAction Stop
        } catch {
            continue
        }
        # The world eye targets are created from the actual retail backbuffer.
        # A fixed-size menu transport is not proof of the gameplay source
        # resolution, even when it happens to share the same dimensions.
        if ([string]$frame.event -cne
            "fnvxrRetailEngineCenterCpuStereo") {
            continue
        }
        $actualWidth = [int]$frame.width
        $actualHeight = [int]$frame.height
        return [ordered]@{
            event = [string]$frame.event
            transaction = [uint64]$frame.transaction
            width = $actualWidth
            height = $actualHeight
            expectedWidth = $ExpectedWidth
            expectedHeight = $ExpectedHeight
            matched = $actualWidth -eq $ExpectedWidth -and
                $actualHeight -eq $ExpectedHeight
            observedAtUtc = [DateTime]::UtcNow.ToString("o")
        }
    }
    return $null
}

function Get-FnvxrProductRetailCameraPoseSweepProof {
    param([Parameter(Mandatory = $true)][string]$LogPath)

    if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
        return $null
    }
    $samples = @()
    foreach ($line in @(Get-Content -LiteralPath $LogPath)) {
        $jsonStart = $line.IndexOf(
            '{"event":"fnvxrRetailEngineCenterFrame"')
        if ($jsonStart -lt 0) {
            continue
        }
        try {
            $frame = $line.Substring($jsonStart) |
                ConvertFrom-Json -ErrorAction Stop
        } catch {
            continue
        }
        if (-not [bool]$frame.delivered -or
            -not [bool]$frame.cameraPoseValid -or
            @($frame.hmdPos).Count -ne 3 -or
            @($frame.centerForward).Count -ne 3 -or
            @($frame.centerUp).Count -ne 3 -or
            @($frame.centerOffsetFromStock).Count -ne 3) {
            continue
        }
        $samples += $frame
    }
    if ($samples.Count -lt 4) {
        return $null
    }

    function Get-Range {
        param(
            [Parameter(Mandatory = $true)]$Values,
            [Parameter(Mandatory = $true)][int]$Index
        )
        $axis = @($Values | ForEach-Object {
            [double]$_[$Index]
        })
        $minimum = [double]($axis | Measure-Object -Minimum).Minimum
        $maximum = [double]($axis | Measure-Object -Maximum).Maximum
        return [ordered]@{
            minimum = $minimum
            maximum = $maximum
            span = $maximum - $minimum
        }
    }

    $hmdPosition = @($samples | ForEach-Object {
        ,@($_.hmdPos)
    })
    $centerForward = @($samples | ForEach-Object {
        ,@($_.centerForward)
    })
    $centerUp = @($samples | ForEach-Object {
        ,@($_.centerUp)
    })
    $centerOffset = @($samples | ForEach-Object {
        ,@($_.centerOffsetFromStock)
    })
    $hmdX = Get-Range -Values $hmdPosition -Index 0
    $hmdY = Get-Range -Values $hmdPosition -Index 1
    $hmdZ = Get-Range -Values $hmdPosition -Index 2
    $forwardX = Get-Range -Values $centerForward -Index 0
    $forwardY = Get-Range -Values $centerForward -Index 1
    $forwardZ = Get-Range -Values $centerForward -Index 2
    $upX = Get-Range -Values $centerUp -Index 0
    $upY = Get-Range -Values $centerUp -Index 1
    $offsetX = Get-Range -Values $centerOffset -Index 0
    $offsetY = Get-Range -Values $centerOffset -Index 1
    $offsetZ = Get-Range -Values $centerOffset -Index 2
    $translationResponseMagnitude = [Math]::Sqrt(
        $offsetX.span * $offsetX.span +
        $offsetY.span * $offsetY.span +
        $offsetZ.span * $offsetZ.span)
    # Fallout's stock camera heading is not pinned to one world-horizontal
    # component. Measure yaw and roll in the full horizontal plane so a valid
    # camera facing -X is not rejected merely because yaw appears in Y.
    $horizontalHeadingResponseMagnitude = [Math]::Sqrt(
        $forwardX.span * $forwardX.span +
        $forwardY.span * $forwardY.span)
    $horizontalUpResponseMagnitude = [Math]::Sqrt(
        $upX.span * $upX.span +
        $upY.span * $upY.span)
    $translationInputProven =
        $hmdX.span -ge 0.05 -and
        $hmdY.span -ge 0.03 -and
        $hmdZ.span -ge 0.04
    $translationCameraResponseProven =
        $translationResponseMagnitude -ge 3.0
    $yawCameraResponseProven =
        $horizontalHeadingResponseMagnitude -ge 0.12
    $pitchCameraResponseProven = $forwardZ.span -ge 0.08
    $rollCameraResponseProven =
        $horizontalUpResponseMagnitude -ge 0.04
    return [ordered]@{
        sampleCount = $samples.Count
        firstTransaction = [uint64]$samples[0].transaction
        lastTransaction =
            [uint64]$samples[$samples.Count - 1].transaction
        hmdPosition = [ordered]@{
            x = $hmdX
            y = $hmdY
            z = $hmdZ
        }
        centerForward = [ordered]@{
            x = $forwardX
            y = $forwardY
            z = $forwardZ
            horizontalResponseMagnitude =
                $horizontalHeadingResponseMagnitude
        }
        centerUp = [ordered]@{
            x = $upX
            y = $upY
            horizontalResponseMagnitude =
                $horizontalUpResponseMagnitude
        }
        centerOffsetFromStock = [ordered]@{
            x = $offsetX
            y = $offsetY
            z = $offsetZ
            responseMagnitude = $translationResponseMagnitude
        }
        translationInputProven = $translationInputProven
        translationCameraResponseProven =
            $translationCameraResponseProven
        yawCameraResponseProven = $yawCameraResponseProven
        pitchCameraResponseProven = $pitchCameraResponseProven
        rollCameraResponseProven = $rollCameraResponseProven
        sixDofCameraResponseProven =
            $translationInputProven -and
            $translationCameraResponseProven -and
            $yawCameraResponseProven -and
            $pitchCameraResponseProven -and
            $rollCameraResponseProven
        observedAtUtc = [DateTime]::UtcNow.ToString("o")
    }
}

function Get-FnvxrProductStereoContinuityProof {
    param(
        [Parameter(Mandatory = $true)][string]$LogPath,
        [ValidateRange(2, 600)][int]$MinimumTransactions = 60,
        [ValidateRange(100, 30000)][int]$MinimumDurationMilliseconds = 2000
    )

    if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
        return $null
    }
    $transactions = [ordered]@{}
    $poseSequences = @{}
    foreach ($line in @(Get-Content -LiteralPath $LogPath -Tail 5000)) {
        if (-not $line.StartsWith('{"event":"fnvxrOpenXrSubmit"')) {
            continue
        }
        try {
            $frame = $line | ConvertFrom-Json -ErrorAction Stop
            $proof = ConvertTo-FnvxrProductStereoOutputProof -Frame $frame
            if (-not $proof) { continue }
            $transactionKey = "{0}:{1}" -f
                $proof.transport,
                $proof.transaction
            if (-not $transactions.Contains($transactionKey)) {
                $transactions[$transactionKey] = $proof
            }
            $poseSequences[[string]$proof.poseSequence] = $true
        } catch {
            continue
        }
    }
    if ($transactions.Count -lt $MinimumTransactions -or
        $poseSequences.Count -lt $MinimumTransactions) {
        return $null
    }
    $proofs = @($transactions.Values)
    $first = $proofs[0]
    $last = $proofs[$proofs.Count - 1]
    $durationMilliseconds =
        [uint64]$last.hostWallClockUnixMilliseconds -
        [uint64]$first.hostWallClockUnixMilliseconds
    if ($durationMilliseconds -lt $MinimumDurationMilliseconds -or
        [uint64]$last.frame -le [uint64]$first.frame) {
        return $null
    }
    return [ordered]@{
        uniqueTransactions = $transactions.Count
        uniquePoseSequences = $poseSequences.Count
        durationMilliseconds = $durationMilliseconds
        first = $first
        last = $last
        observedAtUtc = [DateTime]::UtcNow.ToString("o")
    }
}

function Get-FnvxrProductPipBoyOutputProof {
    param([Parameter(Mandatory = $true)][string]$LogPath)

    if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
        return $null
    }
    $lines = @(Get-Content -LiteralPath $LogPath -Tail 600)
    [array]::Reverse($lines)
    foreach ($line in $lines) {
        if (-not $line.StartsWith('{"event":"fnvxrOpenXrSubmit"')) {
            continue
        }
        try {
            $frame = $line | ConvertFrom-Json -ErrorAction Stop
        } catch {
            continue
        }
        $pipBoyVisible = (([uint32]$frame.runtimeMenuBits -band 64) -ne 0)
        if ([bool]$frame.runtimeUi -and
            [uint32]$frame.runtimePhase -eq 1 -and
            $pipBoyVisible -and
            [bool]$frame.uiQuadVisible -and
            [uint64]$frame.productUiSourceFrame -gt 0 -and
            [bool]$frame.runtimeShouldRender -and
            [bool]$frame.projectionLayerSubmitted -and
            [int]$frame.layerCount -eq 1 -and
            [string]$frame.xrEndFrame -eq "XR_SUCCESS" -and
            [bool]$frame.leftOutputProof -and
            [bool]$frame.rightOutputProof -and
            [int]$frame.leftOutputNonBlackSamples -gt 0 -and
            [int]$frame.rightOutputNonBlackSamples -gt 0 -and
            [int]$frame.leftOutputVariedSamples -gt 0 -and
            [int]$frame.rightOutputVariedSamples -gt 0 -and
            [string]$frame.leftOutputHash -ne "0x0" -and
            [string]$frame.rightOutputHash -ne "0x0") {
            return [ordered]@{
                frame = [uint64]$frame.frame
                runtimeState = "pipboy-ui"
                menuBits = [uint32]$frame.runtimeMenuBits
                uiSourceFrame = [uint64]$frame.productUiSourceFrame
                leftOutputHash = [string]$frame.leftOutputHash
                rightOutputHash = [string]$frame.rightOutputHash
                observedAtUtc = [DateTime]::UtcNow.ToString("o")
            }
        }
    }
    return $null
}

function Get-FnvxrProductHeadsetDemoInputProof {
    param([Parameter(Mandatory = $true)][string]$LogPath)

    if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
        return $null
    }
    $openTap = $null
    $closeTap = $null
    $completion = $null
    foreach ($line in @(Get-Content -LiteralPath $LogPath -Tail 600)) {
        if (-not $line.StartsWith('{"event":"fnvxrHeadsetDemoPipBoy')) {
            continue
        }
        try {
            $event = $line | ConvertFrom-Json -ErrorAction Stop
        } catch {
            continue
        }
        if ($event.event -eq "fnvxrHeadsetDemoPipBoyTap" -and
            [bool]$event.tapped) {
            if ($event.action -eq "open") {
                $openTap = $event
            } elseif ($event.action -eq "close") {
                $closeTap = $event
            }
        } elseif ($event.event -eq "fnvxrHeadsetDemoPipBoyStage" -and
            [uint32]$event.stage -eq 5 -and
            [bool]$event.gameplay -and -not [bool]$event.pipBoyVisible) {
            $completion = $event
        }
    }
    if (-not $openTap -or -not $closeTap -or -not $completion) {
        return $null
    }
    return [ordered]@{
        openFrame = [uint64]$openTap.frame
        closeFrame = [uint64]$closeTap.frame
        completionFrame = [uint64]$completion.frame
        observedAtUtc = [DateTime]::UtcNow.ToString("o")
    }
}

function Get-FnvxrProductHeadsetMirrorCaptureProof {
    param([Parameter(Mandatory = $true)][string]$Directory)

    if (-not (Test-Path -LiteralPath $Directory -PathType Container)) {
        return $null
    }
    $pairs = @()
    foreach ($left in @(Get-ChildItem -LiteralPath $Directory -Filter "pair_*_left.png" -File)) {
        $rightName = $left.Name -replace '_left\.png$', '_right.png'
        $rightPath = Join-Path $Directory $rightName
        if (Test-Path -LiteralPath $rightPath -PathType Leaf) {
            $pairs += [ordered]@{
                ordinal = $left.BaseName -replace "^pair_", "" -replace "_left$", ""
                left = $left.FullName
                right = $rightPath
            }
        }
    }
    if ($pairs.Count -eq 0) {
        return $null
    }
    return [ordered]@{
        pairCount = $pairs.Count
        firstPair = $pairs[0]
        lastPair = $pairs[$pairs.Count - 1]
        observedAtUtc = [DateTime]::UtcNow.ToString("o")
    }
}

try {
    if ($metaXrOperatorIdentity) {
        $operatorListeners = @(Get-FnvxrProductMetaXrOperatorListeners)
        if ($operatorListeners.Count -ne 0) {
            throw "Meta XR Operator observation port 8720 is already owned by another process; refusing to attach to or disturb it."
        }
        $openXrRuntimePlan.metaXrOperator.listenerBefore = @($operatorListeners)
    }
    if ($headlessRuntimeIdentity) {
        $preLaunchRuntimeIdentity =
            Resolve-FnvxrProductHeadlessRuntimeManifest `
                -ManifestPath $headlessRuntimeManifestPath
        if ($preLaunchRuntimeIdentity.manifest.sha256 -cne
                $headlessRuntimeIdentity.manifest.sha256 -or
            $preLaunchRuntimeIdentity.runtimeDll.sha256 -cne
                $headlessRuntimeIdentity.runtimeDll.sha256) {
            throw "Headless OpenXR runtime manifest or DLL changed after validation."
        }
        $openXrRuntimePlan.preLaunchIdentity = $preLaunchRuntimeIdentity
        New-Item `
            -ItemType Directory `
            -Path $openXrSimulatorDataDirectory `
            -Force | Out-Null
    }
    if ($physicalRuntimeIdentity) {
        $preLaunchRuntimeIdentity =
            Resolve-FnvxrProductPhysicalRuntimeManifest `
                -ManifestPath $physicalRuntimeManifestPath
        if ($preLaunchRuntimeIdentity.manifest.sha256 -cne
                $physicalRuntimeIdentity.manifest.sha256 -or
            $preLaunchRuntimeIdentity.runtimeDll.sha256 -cne
                $physicalRuntimeIdentity.runtimeDll.sha256) {
            throw "Physical OpenXR runtime manifest or DLL changed after validation."
        }
        $openXrRuntimePlan.preLaunchIdentity = $preLaunchRuntimeIdentity
    }
    foreach ($name in $processLocalRuntimeEnvironmentNames) {
        Remove-Item -LiteralPath ("Env:{0}" -f $name) -ErrorAction SilentlyContinue
    }
    $environment = Get-FnvxrProductMinimalEnvironment `
        -RunId $runId `
        -RunDirectory $runDirectory `
        -OpenXrLoaderPath $stagedLoaderPath `
        -SessionReadyTimeoutSeconds $RetailReadyTimeoutSeconds `
        -AutomateRecoveryLoad:$AutomateInGame `
        -AutomateFreshCharacter:$StartNewCharacter `
        -AutomateRetailFixture:$retailFixtureRequested `
        -TtwFixture:$TtwCore `
        -HeadsetDemoFixture:$HeadsetDemoFixture `
        -HeadsetWorldOnlyCapture:$HeadsetWorldOnlyCapture `
        -PhysicalHeadsetPlay:$PhysicalHeadsetPlay `
        -PhysicalGameWidth $PhysicalGameWidth `
        -PhysicalGameHeight $PhysicalGameHeight `
        -HeadsetDemoGameplayWarmupFrames $HeadsetDemoGameplayWarmupFrames `
        -RetailVrAccumulationDiagnosticMode $RetailVrAccumulationDiagnosticMode `
        -RetailFixtureAction $resolvedRetailFixtureAction `
        -RetailFixtureSaveName $retailFixtureSaveName `
        -RetailFixtureTraitOne $(if ($retailFixtureRequested) { $retailFixtureTraits.first } else { "None" }) `
        -RetailFixtureTraitTwo $(if ($retailFixtureRequested) { $retailFixtureTraits.second } else { "None" }) `
        -AcknowledgeTribalPackPopup:$AcknowledgeTribalPackPopup `
        -AutomateRecoverySaveName $RetailSaveName `
        -HeadlessRuntimeManifest $headlessRuntimeManifestPath `
        -PhysicalRuntimeManifest $physicalRuntimeManifestPath `
        -HeadsetMirrorCaptureDirectory $headsetMirrorDirectory `
        -HeadsetMirrorCaptureEveryFrames $HeadsetMirrorCaptureEveryFrames `
        -HeadsetMirrorCaptureMaxPairs $HeadsetMirrorCaptureMaxPairs `
        -MetaXrOperatorLayerDirectory $(if ($metaXrOperatorIdentity) {
            $metaXrOperatorIdentity.directory
        } else { "" })
    Set-FnvxrProductMinimalEnvironment -Environment $environment
    if ($selectedRuntimeIdentity) {
        $openXrRuntimePlan.status = "process-local-environment-applied"
    }
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

    $hostBridgeReady = Wait-FnvxrProductHostBridgeReady `
        -Process $hostProcess `
        -LogPath $hostOut `
        -TimeoutSeconds $HostReadyTimeoutSeconds
    if (-not $hostBridgeReady) {
        $hostProcess.Refresh()
        if ($hostProcess.HasExited) {
            throw "OpenXR host exited before publishing its authoritative session/shared-mapping bridge handshake (exit code $($hostProcess.ExitCode))."
        }
        throw "Timed out after $HostReadyTimeoutSeconds seconds waiting for the authoritative OpenXR session/shared-mapping bridge handshake."
    }
    $manifest.readiness.hostBridge = $true
    if ($selectedRuntimeIdentity) {
        $openXrRuntimePlan.status = "host-bridge-ready"
    }
    if ($metaXrOperatorIdentity) {
        $openXrRuntimePlan.metaXrOperator.hostOwnedListener =
            Wait-FnvxrProductMetaXrOperatorHostListener `
                -ExpectedProcessId ([uint32]$hostProcess.Id) `
                -TimeoutSeconds ([Math]::Min($HostReadyTimeoutSeconds, 15))
        $openXrRuntimePlan.metaXrOperator.status = "host-owned-observation-listener-ready"
    }
    if ($PhysicalHeadsetPlay) {
        $manifest.state = "staging-temporary-physical-display-profile"
        $physicalDisplayProfilePlan.status = "staging"
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        Write-SupervisorLog (
            "staging temporary physical-play source resolution {0}x{1}; both Fallout INIs will be restored byte-for-byte" -f
            $PhysicalGameWidth,
            $PhysicalGameHeight)
        $physicalDisplayProfileRecord =
            Install-FnvxrProductPhysicalDisplayProfile `
                -BackupRoot $backupRoot `
                -RunId $runId `
                -Width $PhysicalGameWidth `
                -Height $PhysicalGameHeight
        $physicalDisplayProfilePlan.staged =
            @($physicalDisplayProfileRecord.records | ForEach-Object {
                $_.staged
            })
        $physicalDisplayProfilePlan.status = "staged-temporary"
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
    }
    if ($UseRetailPluginProfile) {
        $manifest.state = "staging-temporary-retail-plugin-profile"
        $retailPluginProfilePlan.status = "staging"
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        Write-SupervisorLog "staging exact-official retail plugins.txt profile; save files remain read-only"
        $retailPluginProfileRecord =
            Install-FnvxrProductRetailVisualTrialPluginProfile `
                -Path $retailPluginProfilePath `
                -BackupRoot $backupRoot `
                -RunId $runId `
                -GameRoot $game.root
        $retailPluginProfilePlan.staged = $retailPluginProfileRecord.staged
        $retailPluginProfilePlan.status = if ($retailPluginProfileRecord.changed) {
            "staged-temporary"
        } else {
            "already-exact-retail"
        }
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
    }
    if ($retailFixtureRequested) {
        $manifest.state = if ($TtwCore) {
            "staging-temporary-ttw-core-fixture-plugin-profile"
        } else {
            "staging-temporary-retail-fixture-plugin-profile"
        }
        $retailFixturePluginProfilePlan.status = "staging"
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        if ($TtwCore) {
            Write-SupervisorLog "staging exact TTW core fixture plugins.txt profile in the isolated workspace sandbox; personal saves remain untouched"
            $retailFixturePluginProfileRecord =
                Install-FnvxrProductTtwBaselinePluginProfile `
                    -Path $retailFixturePluginProfilePath `
                    -BackupRoot $backupRoot `
                    -RunId $runId `
                    -GameRoot $game.root
        } else {
            Write-SupervisorLog "staging FalloutNV.esm-only retail fixture plugins.txt profile; personal saves remain untouched"
            $retailFixturePluginProfileRecord =
                Install-FnvxrProductRetailFixturePluginProfile `
                    -Path $retailFixturePluginProfilePath `
                    -BackupRoot $backupRoot `
                    -RunId $runId `
                    -GameRoot $game.root
        }
        $retailFixturePluginProfilePlan.staged =
            $retailFixturePluginProfileRecord.staged
        $retailFixturePluginProfilePlan.status = if ($retailFixturePluginProfileRecord.changed) {
            "staged-temporary"
        } else {
            "already-exact-fixture"
        }
        if ($TtwCore) {
            $ttwCoreProfilePlan.staged = $retailFixturePluginProfileRecord.staged
            $ttwCoreProfilePlan.status = if ($retailFixturePluginProfileRecord.changed) {
                "staged-temporary"
            } else {
                "already-exact-ttw-core"
            }
        }
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
    }
    $manifest.state = "staging-attested-retail-artifacts"
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
    Write-SupervisorLog "host OpenXR session/shared mappings are authoritative and host is alive; staging exact Win32 product set"

    $staged = @(Install-FnvxrProductArtifactSet `
        -Plan $stagePlan `
        -BackupRoot $backupRoot `
        -RunId $runId)
    $manifest.staged = $staged
    $hostProcess.Refresh()
    if ($hostProcess.HasExited) {
        throw "OpenXR host exited after bridge readiness but before retail launch."
    }
    $manifest.state = "starting-retail"
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath

    $nvse = Start-Process `
        -FilePath $game.nvseLoader.path `
        -WorkingDirectory $game.root `
        -WindowStyle Hidden `
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
    $manifest.readiness.hostPose = $true
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
    if (-not (Wait-FnvxrProductLogPattern `
        -Process $fallout `
        -LogPath $retailVrLog `
        -Pattern "retail VR bridge initialized: exact AccumulateScene callsite hook, ordinary-D3D9 CPU-v8 stereo transport, and deferred Present bootstrap ready" `
        -TimeoutSeconds 15)) {
        throw "The exact D3D9 module loaded, but its ordinary-D3D9 retail VR bridge never initialized. See $retailVrLog"
    }
    $manifest.readiness.retailVrBridge = $true
    if ($automationRequested) {
        $automationPlan.status = "waiting-for-start-menu"
        $manifest.state = if ($retailFixtureRequested) {
            "waiting-for-retail-fixture-gate"
        } elseif ($StartNewCharacter) {
            "waiting-for-fixed-fresh-character-gate"
        } else {
            "waiting-for-fixed-recovery-load-gate"
        }
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        $startMenuSample = Wait-FnvxrProductStartMenuForRecoveryLoad `
            -ProbePath $probePath `
            -RequiredProcess $fallout `
            -TimeoutSeconds $RetailReadyTimeoutSeconds `
            -LogPath $probeLog
        $automationPlan.startMenuEvidence =
            ConvertTo-FnvxrProductRuntimeEvidence -Sample $startMenuSample
        $automationPlan.status = if ($retailFixtureRequested) {
            "submitting-retail-fixture-$resolvedRetailFixtureAction"
        } elseif ($StartNewCharacter) {
            "submitting-fixed-fresh-character"
        } else {
            "submitting-fixed-recovery-load"
        }
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath

        if ($creatingFreshCharacter) {
            $automationPlan.commandInvocation =
                Invoke-FnvxrProductFreshCharacter `
                    -CommandPath $commandPath `
                    -LogPath $automationCommandLog `
                    -FixtureSaveName $(if ($retailFixtureRequested) { $retailFixtureSaveName } else { "" }) `
                    -WaitMilliseconds $fixedRecoveryLoadWaitMilliseconds
        } elseif ($loadingRetailFixture) {
            $automationPlan.commandInvocation =
                Invoke-FnvxrProductRetailFixtureLoad `
                    -CommandPath $commandPath `
                    -LogPath $automationCommandLog `
                    -SaveName $retailFixtureSaveName `
                    -TtwFixture:$TtwCore `
                    -WaitMilliseconds $fixedRecoveryLoadWaitMilliseconds
        } else {
            $automationPlan.commandInvocation =
                Invoke-FnvxrProductFixedRecoveryLoad `
                    -CommandPath $commandPath `
                    -LogPath $automationCommandLog `
                    -RetailSaveName $RetailSaveName `
                    -WaitMilliseconds $fixedRecoveryLoadWaitMilliseconds
        }
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        if (-not $automationPlan.commandInvocation.completed) {
            $automationDescription = if ($retailFixtureRequested) {
                "retail fixture $resolvedRetailFixtureAction command"
            } elseif ($StartNewCharacter) {
                "fresh-character command"
            } else {
                "fixed recovery-load command"
            }
            throw (
                "The exact {0} was rejected or timed out with exit code {1}. See {2}" -f
                $automationDescription,
                $automationPlan.commandInvocation.exitCode,
                $automationCommandLog)
        }

        $automationPlan.status = "waiting-for-loaded-gameplay"
        $gameplaySample = if ($retailFixtureRequested) {
            # An owned FNVXR_AutoRetail fixture proves its real player/cell
            # state in-process and intentionally publishes no player bridge.
            # Use its matching outer verifier rather than rejecting a valid
            # gameplay scene solely because that optional bridge is absent.
            Wait-FnvxrProductRetailFixtureGameplay `
                -ProbePath $probePath `
                -RequiredProcess $fallout `
                -TimeoutSeconds $RetailReadyTimeoutSeconds `
                -LogPath $probeLog `
                -MinimumFrame ([uint64]$automationPlan.startMenuEvidence.frame)
        } else {
            Wait-FnvxrProductLoadedGameplay `
                -ProbePath $probePath `
                -RequiredProcess $fallout `
                -TimeoutSeconds $RetailReadyTimeoutSeconds `
                -LogPath $probeLog `
                -MinimumFrame ([uint64]$automationPlan.startMenuEvidence.frame)
        }
        $automationPlan.gameplayEvidence =
            ConvertTo-FnvxrProductRuntimeEvidence -Sample $gameplaySample
        if ($creatingFreshCharacter) {
            $automationPlan.status = "waiting-for-fresh-save"
            Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
            $createdSavePath = if ($retailFixtureRequested) {
                $retailFixtureSavePath
            } else {
                $freshCharacterSavePath
            }
            if ($retailFixtureRequested) {
                $automationPlan.createdSave =
                    Wait-FnvxrProductRetailFixtureSavePair `
                        -SavePath $createdSavePath `
                        -NvsePath $retailFixtureNvsePath `
                        -RequiredProcess $fallout `
                        -TimeoutSeconds $RetailReadyTimeoutSeconds
            } else {
                $automationPlan.createdSave =
                    Wait-FnvxrProductFreshCharacterSave `
                        -SavePath $createdSavePath `
                        -RequiredProcess $fallout `
                        -TimeoutSeconds $RetailReadyTimeoutSeconds
            }
            $manifest.readiness.automatedFreshCharacter = $true
            if ($retailFixtureRequested) {
                $manifest.readiness.automatedRetailFixture = $true
            }
        } elseif ($loadingRetailFixture) {
            $manifest.readiness.automatedRetailFixture = $true
        }
        $automationPlan.status = "complete"
        $manifest.readiness.automatedGameplay = $true
        if ($HeadsetDemoFixture) {
            $manifest.headsetDemo.status = "owned-fixture-gameplay-proven"
        }
        if ($HeadsetWorldOnlyCapture) {
            $manifest.headsetWorldCapture.status =
                "owned-fixture-gameplay-proven"
        }
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        if ($retailFixtureRequested) {
            Write-SupervisorLog (
                "retail fixture proven by shared state: action={0} save={1} traits={2},{3} startFrame={4} gameplayFrame={5} phase=3 cameraActive=1 blockingMenuBits=0 menuBits=0x{6:X2} uiInputAllowed=0" -f
                $resolvedRetailFixtureAction,
                $retailFixtureSavePath,
                $retailFixtureTraits.first,
                $retailFixtureTraits.second,
                $automationPlan.startMenuEvidence.frame,
                $automationPlan.gameplayEvidence.frame,
                [uint32]$automationPlan.gameplayEvidence.menuBits)
        } elseif ($StartNewCharacter) {
            Write-SupervisorLog (
                "fixed fresh character proven by shared state and new save: name={0} save={1} startFrame={2} gameplayFrame={3} phase=3 cameraActive=1 blockingMenuBits=0 menuBits=0x{4:X2} uiInputAllowed=0" -f
                $freshCharacterName,
                $freshCharacterSavePath,
                $automationPlan.startMenuEvidence.frame,
                $automationPlan.gameplayEvidence.frame,
                [uint32]$automationPlan.gameplayEvidence.menuBits)
        } else {
            Write-SupervisorLog (
                "verified load-only save proven by a real gameplay scene: startFrame={0} gameplayFrame={1} phase=3 cameraActive=1 blockingMenuBits=0 menuBits=0x{5:X2} uiInputAllowed=0 cell=0x{2:X8} player=0x{3:X8} camera=0x{4:X8}" -f
                $automationPlan.startMenuEvidence.frame,
                $automationPlan.gameplayEvidence.frame,
                [uint32]$automationPlan.gameplayEvidence.scene.currentCellFormId,
                [uint64]$automationPlan.gameplayEvidence.scene.playerAddress,
                [uint64]$automationPlan.gameplayEvidence.scene.cameraNodeAddress,
                [uint32]$automationPlan.gameplayEvidence.menuBits)
        }
    }
    if ($HeadsetPoseSweep) {
        $manifest.headsetPoseSweep.status = "running"
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        Write-SupervisorLog (
            "starting bounded six-axis headless-runtime HMD pose sweep for {0} seconds; xNVSE controller/weapon mutation gates remain closed" -f
            $HeadsetPoseSweepSeconds)
        $headsetPoseSweepOutput = @(
            & (Join-Path $PSScriptRoot "invoke-openxr-simulator-head-sweep.ps1") `
                -DataDirectory $openXrSimulatorDataDirectory `
                -DurationSeconds $HeadsetPoseSweepSeconds
        ) -join [Environment]::NewLine
        $headsetPoseSweepEvidence =
            $headsetPoseSweepOutput | ConvertFrom-Json -ErrorAction Stop
        if (-not [bool]$headsetPoseSweepEvidence.centerRestored -or
            [int]$headsetPoseSweepEvidence.commandCount -lt 2) {
            throw "The headless-runtime HMD pose sweep did not prove bounded motion and center restoration."
        }
        $headsetPoseSweepRenderedProof =
            Get-FnvxrProductRetailCameraPoseSweepProof `
                -LogPath $retailVrLog
        if (-not $headsetPoseSweepRenderedProof -or
            -not [bool]$headsetPoseSweepRenderedProof.sixDofCameraResponseProven) {
            throw "The commanded headless HMD sweep did not prove yaw, pitch, roll, and translation in the actual retail NiCamera transactions."
        }
        $manifest.headsetPoseSweep.status =
            "rendered-six-dof-proven-centered"
        $manifest.headsetPoseSweep.evidence = $headsetPoseSweepEvidence
        $manifest.headsetPoseSweep.renderedCameraProof =
            $headsetPoseSweepRenderedProof
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        Write-SupervisorLog (
            "completed six-axis HMD pose sweep commands={0} x=[{1:N3},{2:N3}] y=[{3:N3},{4:N3}] z=[{5:N3},{6:N3}] renderedSamples={7} yawSpan={8:N3} pitchSpan={9:N3} rollSpan={10:N3} translationResponse={11:N3}; simulator center restored" -f
            $headsetPoseSweepEvidence.commandCount,
            $headsetPoseSweepEvidence.commandedMinimum.x,
            $headsetPoseSweepEvidence.commandedMaximum.x,
            $headsetPoseSweepEvidence.commandedMinimum.y,
            $headsetPoseSweepEvidence.commandedMaximum.y,
            $headsetPoseSweepEvidence.commandedMinimum.z,
            $headsetPoseSweepEvidence.commandedMaximum.z,
            $headsetPoseSweepRenderedProof.sampleCount,
            $headsetPoseSweepRenderedProof.centerForward.horizontalResponseMagnitude,
            $headsetPoseSweepRenderedProof.centerForward.z.span,
            $headsetPoseSweepRenderedProof.centerUp.x.span,
            $headsetPoseSweepRenderedProof.centerOffsetFromStock.responseMagnitude)
    }
    if ($retailFixtureRequested -and -not $headsetFixtureOpenXrRun) {
        # Fixture automation is a deterministic game-state harness, not a
        # stereo acceptance run. Stop once the owned save/load and real scene
        # proof succeed so a missing binocular proof cannot hide a successful
        # character fixture behind an unrelated VR result.
        $manifest.fixtureOnly = $true
        $manifest.completion = "fixture-ready"
        $manifest.state = "fixture-ready"
        $normalCompletion = $true
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        Write-SupervisorLog "retail fixture is ready; ending fixture-only run before stereo acceptance"
    } else {
        $manifest.state = "ready-and-supervised"
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        Write-SupervisorLog "retail runtime, pose, exact modules, and the CPU-v8 engine bridge are live; waiting for proven binocular headset output"

    $deadline = [DateTime]::UtcNow.AddSeconds($MaximumRunSeconds)
    $healthFailures = 0
    $nextHealthCheck = [DateTime]::UtcNow
    $completion = $null
    $stereoOutputProof = $null
    $stereoContinuityProof = $null
    $pipBoyOutputProof = $null
    $headsetDemoInputProof = $null
    $controllerAuthorizationProof = $null
    $physicalDisplayOutputProof = $null
    do {
        $hostProcess.Refresh()
        $fallout.Refresh()
        if ($fallout.HasExited) { $completion = "retail-exited"; break }
        if ($hostProcess.HasExited) {
            $completion = if ($hostProcess.ExitCode -eq 0) { "host-frame-limit" } else { "host-failed" }
            break
        }
        if ([DateTime]::UtcNow -ge $nextHealthCheck) {
            if ($PhysicalHeadsetPlay -and
                -not $physicalDisplayOutputProof) {
                $physicalDisplayObservation =
                    Get-FnvxrProductPhysicalDisplayOutputProof `
                        -LogPath $retailVrLog `
                        -ExpectedWidth $PhysicalGameWidth `
                        -ExpectedHeight $PhysicalGameHeight
                if ($physicalDisplayObservation -and
                    -not [bool]$physicalDisplayObservation.matched) {
                    throw (
                        "Retail ignored the staged physical-play source resolution: expected {0}x{1}, observed {2}x{3}." -f
                            $PhysicalGameWidth,
                            $PhysicalGameHeight,
                            $physicalDisplayObservation.width,
                            $physicalDisplayObservation.height)
                }
                if ($physicalDisplayObservation) {
                    $physicalDisplayOutputProof =
                        $physicalDisplayObservation
                    $physicalDisplayProfilePlan.outputProof =
                        $physicalDisplayOutputProof
                    $physicalDisplayProfilePlan.status =
                        "live-source-resolution-proven"
                    Write-FnvxrProductJsonAtomic `
                        -Value $manifest `
                        -Path $manifestPath
                    Write-SupervisorLog (
                        "proven physical-play retail source resolution {0}x{1} transaction={2} event={3}" -f
                            $physicalDisplayOutputProof.width,
                            $physicalDisplayOutputProof.height,
                            $physicalDisplayOutputProof.transaction,
                            $physicalDisplayOutputProof.event)
                }
            }
            if ($PhysicalHeadsetPlay -and
                -not $controllerAuthorizationProof) {
                $controllerAuthorizationProof =
                    Get-FnvxrProductControllerAuthorizationProof `
                        -LogPath $hostOut
                if ($controllerAuthorizationProof) {
                    $manifest.controllerMutationAuthorized = $true
                    $automationPlan.controllerMutationAuthorized = $true
                    $manifest.physicalHeadsetPlay.controllerConsumerAcknowledged =
                        $true
                    $manifest.physicalHeadsetPlay.controllerMode =
                        $controllerAuthorizationProof.mode
                    $manifest.physicalHeadsetPlay.status =
                        "exact-retail-controller-route-live"
                    $manifest.physicalHeadsetPlay.authorizationProof =
                        $controllerAuthorizationProof
                    Write-FnvxrProductJsonAtomic `
                        -Value $manifest `
                        -Path $manifestPath
                    Write-SupervisorLog (
                        "physical controller route authorized by exact-retail xNVSE consumer frame={0} mode={1} runtimeSample={2}" -f
                        $controllerAuthorizationProof.frame,
                        $controllerAuthorizationProof.mode,
                        $controllerAuthorizationProof.runtimeStateSample)
                }
            }
            if (-not $stereoOutputProof) {
                if ($HeadsetWorldOnlyCapture) {
                    $stereoContinuityProof =
                        Get-FnvxrProductStereoContinuityProof `
                            -LogPath $hostOut
                    if ($stereoContinuityProof) {
                        $stereoOutputProof = $stereoContinuityProof.last
                    }
                } else {
                    $stereoOutputProof =
                        Get-FnvxrProductStereoOutputProof -LogPath $hostOut
                }
                if ($stereoOutputProof) {
                    $manifest.readiness.stereoOutput = $true
                    $manifest.stereoOutputProof = $stereoOutputProof
                    if ($stereoContinuityProof) {
                        $manifest.headsetWorldCapture.status =
                            "sustained-world-stereo-proven"
                        $manifest.headsetWorldCapture.continuityProof =
                            $stereoContinuityProof
                    }
                    $manifest.trialReady = $true
                    $manifest.readyAtUtc = [DateTime]::UtcNow.ToString("o")
                    Write-FnvxrProductJsonAtomic `
                        -Value $manifest `
                        -Path $manifestPath
                    if ($stereoContinuityProof) {
                        Write-SupervisorLog (
                            "proven sustained binocular world output transactions={0} poseSequences={1} durationMs={2} firstFrame={3} lastFrame={4}; controller/weapon product gates remain closed" -f
                            $stereoContinuityProof.uniqueTransactions,
                            $stereoContinuityProof.uniquePoseSequences,
                            $stereoContinuityProof.durationMilliseconds,
                            $stereoContinuityProof.first.frame,
                            $stereoContinuityProof.last.frame)
                    } else {
                        Write-SupervisorLog (
                            "proven binocular output transaction={0} frame={1} left={2} right={3}; controllerAuthorized={4} trackedWeaponAuthorized=0" -f
                            $stereoOutputProof.transaction,
                            $stereoOutputProof.frame,
                            $stereoOutputProof.leftOutputHash,
                            $stereoOutputProof.rightOutputHash,
                            [bool]$manifest.controllerMutationAuthorized)
                    }
                }
            }
            if ($HeadsetDemoFixture -and -not $pipBoyOutputProof) {
                $pipBoyOutputProof =
                    Get-FnvxrProductPipBoyOutputProof -LogPath $hostOut
                if ($pipBoyOutputProof) {
                    $manifest.readiness.headsetDemoPipBoyUi = $true
                    $manifest.headsetDemo.status = "pipboy-ui-proven"
                    $manifest.headsetDemo.pipBoyOutputProof = $pipBoyOutputProof
                    Write-FnvxrProductJsonAtomic `
                        -Value $manifest `
                        -Path $manifestPath
                    Write-SupervisorLog (
                        "proven final-headset Pip-Boy UI frame={0} menuBits=0x{1:X2} left={2} right={3}" -f
                        $pipBoyOutputProof.frame,
                        $pipBoyOutputProof.menuBits,
                        $pipBoyOutputProof.leftOutputHash,
                        $pipBoyOutputProof.rightOutputHash)
                }
            }
            if ($HeadsetDemoFixture -and -not $headsetDemoInputProof) {
                $headsetDemoInputProof =
                    Get-FnvxrProductHeadsetDemoInputProof `
                        -LogPath $headsetDemoInputTelemetryLog
                if ($headsetDemoInputProof) {
                    $manifest.headsetDemo.status = if ($pipBoyOutputProof) {
                        "pipboy-ui-proven-and-returned-to-gameplay"
                    } else {
                        "pipboy-sequence-complete-awaiting-final-eye-ui-proof"
                    }
                    $manifest.headsetDemo.inputProof = $headsetDemoInputProof
                    Write-FnvxrProductJsonAtomic `
                        -Value $manifest `
                        -Path $manifestPath
                    Write-SupervisorLog (
                        "bounded in-game Pip-Boy sequence completed: openFrame={0} closeFrame={1} gameplayFrame={2}" -f
                        $headsetDemoInputProof.openFrame,
                        $headsetDemoInputProof.closeFrame,
                        $headsetDemoInputProof.completionFrame)
                }
            }
            # A private two-eye stock traversal can legitimately publish
            # below the game's main-loop cadence. Sample across one full
            # second here; the independent three-strike watchdog still fails
            # a genuinely stalled producer.
            if (Test-FnvxrProductProbeReady `
                -ProbePath $probePath `
                -Arguments @("--require-pose", "--require-runtime", "--require-advancing", "--sample-delay-ms", "1000") `
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

    if (-not $stereoOutputProof) {
        if ($HeadsetWorldOnlyCapture) {
            throw "No sustained sequence of at least 60 advancing binocular engine-stereo transactions over two seconds reached OpenXR before '$completion'; evidence is in $runDirectory"
        }
        throw "No proven binocular engine-stereo frame reached OpenXR before '$completion'. Enter a loaded gameplay world before the bounded visual trial expires; evidence is in $runDirectory"
    }
    if ($PhysicalHeadsetPlay -and -not $controllerAuthorizationProof) {
        throw "The exact-retail xNVSE controller consumer never acknowledged the physical-play route before '$completion'; controller input remains unauthorized. Evidence is in $runDirectory"
    }
    if ($PhysicalHeadsetPlay -and -not $physicalDisplayOutputProof) {
        throw "No retail frame proved the requested physical-play source resolution ${PhysicalGameWidth}x${PhysicalGameHeight} before '$completion'. Evidence is in $runDirectory"
    }
    if ($HeadsetDemoFixture -and -not $pipBoyOutputProof) {
        throw "No final-headset Pip-Boy UI frame reached OpenXR before '$completion'. The bounded demo does not claim UI proof without an actual projected UI frame; evidence is in $runDirectory"
    }
    if ($HeadsetDemoFixture -and -not $headsetDemoInputProof) {
        throw "The bounded in-game Pip-Boy open/close sequence did not return to gameplay before '$completion'; no partial UI interaction is accepted as a completed demo. Evidence is in $runDirectory"
    }
    if ($CaptureHeadsetMirror) {
        $headsetMirrorCaptureProof =
            Get-FnvxrProductHeadsetMirrorCaptureProof `
                -Directory $headsetMirrorDirectory
        if (-not $headsetMirrorCaptureProof) {
            throw "No complete final-headset eye-image pair was captured before '$completion'; no recording is claimed. Evidence is in $runDirectory"
        }
        $manifest.headsetMirror.status = "captured"
        $manifest.headsetMirror.captureProof = $headsetMirrorCaptureProof
    }
    $normalCompletion = $completion -in @("retail-exited", "host-frame-limit", "supervised-time-limit")
    if (-not $normalCompletion) { throw "Product supervision failed: $completion" }
    $manifest.completion = $completion
        $manifest.state = "cleaning-up"
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
    }
} catch {
    if ($automationRequested -and $automationPlan.status -cne "complete") {
        $automationPlan.failedStage = [string]$automationPlan.status
        $automationPlan.status = "failed"
        $automationPlan.error = $_.Exception.Message
    }
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

    if (($AutomateInGame -or $loadingRetailFixture) -and
        $automationPlan.save -and $automationPlan.nvse) {
        try {
            $loadOnlySavePath = if ($loadingRetailFixture) {
                $retailFixtureSavePath
            } else {
                $fixedRecoverySavePath
            }
            $loadOnlyNvsePath = if ($loadingRetailFixture) {
                $retailFixtureNvsePath
            } else {
                $fixedRecoveryNvsePath
            }
            $automationPlan.saveAfter =
                Get-FnvxrProductFileIdentity -Path $loadOnlySavePath
            $automationPlan.nvseAfter =
                Get-FnvxrProductFileIdentity -Path $loadOnlyNvsePath
            $automationPlan.loadOnlySaveUnchanged =
                Test-FnvxrProductLoadOnlySaveUnchanged `
                    -Before $automationPlan.save `
                    -After $automationPlan.saveAfter
            $automationPlan.loadOnlyNvseUnchanged =
                Test-FnvxrProductLoadOnlySaveUnchanged `
                    -Before $automationPlan.nvse `
                    -After $automationPlan.nvseAfter
            if (-not $automationPlan.loadOnlySaveUnchanged -or
                -not $automationPlan.loadOnlyNvseUnchanged) {
                throw "The load-only visual-trial route changed the verified save or NVSE sidecar. No save replacement or mutation is permitted."
            }
        } catch {
            $saveIdentityError = "Verified load-only save identity check failed: $($_.Exception.Message)"
            $automationPlan.loadOnlyIdentityError = $saveIdentityError
            if ($manifest.error) {
                $manifest.error = "{0} Save identity check also failed: {1}" -f
                    $manifest.error,
                    $saveIdentityError
            } else {
                $manifest.error = $saveIdentityError
            }
            $manifest.state = "failed"
            Write-SupervisorLog ("ERROR " + $saveIdentityError)
        }
    }

    if ($physicalDisplayProfileRecord) {
        try {
            $physicalDisplayProfilePlan.after =
                @(Restore-FnvxrProductPhysicalDisplayProfile `
                    -Record $physicalDisplayProfileRecord)
            $physicalDisplayProfilePlan.restored = $true
            $physicalDisplayProfilePlan.status = "restored-exactly"
        } catch {
            $physicalDisplayProfilePlan.restoreError =
                $_.Exception.Message
            $physicalDisplayProfilePlan.status = "restore-failed"
            if ($manifest.error) {
                $manifest.error =
                    "{0} Physical display-profile restore also failed: {1}" -f
                        $manifest.error,
                        $_.Exception.Message
            } else {
                $manifest.error =
                    "Physical display-profile restore failed: $($_.Exception.Message)"
            }
            $manifest.state = "failed"
            Write-SupervisorLog ("ERROR " + $manifest.error)
        }
    }

    if ($retailPluginProfileRecord) {
        try {
            $retailPluginProfilePlan.after =
                Restore-FnvxrProductRetailVisualTrialPluginProfile `
                    -Record $retailPluginProfileRecord
            $retailPluginProfilePlan.restored = $true
            $retailPluginProfilePlan.status = "restored-exactly"
        } catch {
            $retailPluginProfilePlan.restoreError = $_.Exception.Message
            $retailPluginProfilePlan.status = "restore-failed"
            if ($manifest.error) {
                $manifest.error = "{0} Retail plugin-profile restore also failed: {1}" -f
                    $manifest.error,
                    $_.Exception.Message
            } else {
                $manifest.error = "Retail plugin-profile restore failed: $($_.Exception.Message)"
            }
            $manifest.state = "failed"
            Write-SupervisorLog ("ERROR " + $manifest.error)
        }
    }
    if ($retailFixturePluginProfileRecord) {
        try {
            $retailFixturePluginProfilePlan.after = if ($TtwCore) {
                Restore-FnvxrProductTtwBaselinePluginProfile `
                    -Record $retailFixturePluginProfileRecord
            } else {
                Restore-FnvxrProductRetailFixturePluginProfile `
                    -Record $retailFixturePluginProfileRecord
            }
            $retailFixturePluginProfilePlan.restored = $true
            $retailFixturePluginProfilePlan.status = "restored-exactly"
            if ($TtwCore) {
                $ttwCoreProfilePlan.after = $retailFixturePluginProfilePlan.after
                $ttwCoreProfilePlan.restored = $true
                $ttwCoreProfilePlan.status = "restored-exactly"
            }
        } catch {
            $retailFixturePluginProfilePlan.restoreError = $_.Exception.Message
            $retailFixturePluginProfilePlan.status = "restore-failed"
            if ($TtwCore) {
                $ttwCoreProfilePlan.restoreError = $_.Exception.Message
                $ttwCoreProfilePlan.status = "restore-failed"
            }
            if ($manifest.error) {
                $manifest.error = "{0} Retail fixture plugin-profile restore also failed: {1}" -f
                    $manifest.error,
                    $_.Exception.Message
            } else {
                $manifest.error = "Retail fixture plugin-profile restore failed: $($_.Exception.Message)"
            }
            $manifest.state = "failed"
            Write-SupervisorLog ("ERROR " + $manifest.error)
        }
    }

    if ($staged.Count -gt 0) {
        $manifest.cleanup.stageRestorationRequired = $true
        try {
            Restore-FnvxrProductArtifactSet -Records $staged
            $manifest.cleanup.stagedArtifactsRestored = $true
            if ($manifest.error) {
                $manifest.cleanup.failedStageRolledBack = $true
            }
        } catch {
            $manifest.cleanup.rollbackError = $_.Exception.Message
            if (-not $manifest.error) {
                $manifest.error = "Staged artifact restoration failed: $($_.Exception.Message)"
                $manifest.state = "failed"
                Write-SupervisorLog ("ERROR " + $manifest.error)
            }
        }
    }

    Clear-FnvxrProductProcessEnvironmentVariables
    foreach ($name in $processLocalRuntimeEnvironmentNames) {
        [Environment]::SetEnvironmentVariable(
            $name,
            $null,
            [EnvironmentVariableTarget]::Process)
    }
    foreach ($key in $savedEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable(
            [string]$key,
            [string]$savedEnvironment[$key],
            [EnvironmentVariableTarget]::Process)
    }
    if ($selectedRuntimeIdentity -or $metaXrOperatorIdentity) {
        $openXrRuntimePlan.processLocalEnvironmentRestored = $true
        if ($selectedRuntimeIdentity) {
            $openXrRuntimePlan.status = "process-local-environment-restored"
        }
        if ($metaXrOperatorIdentity) {
            $openXrRuntimePlan.metaXrOperator.processLocalEnvironmentRestored = $true
        }
        if (Test-Path -LiteralPath $openXrSimulatorLog -PathType Leaf) {
            try {
                $openXrRuntimePlan.runtimeLogIdentity =
                    Get-FnvxrProductFileIdentity -Path $openXrSimulatorLog
            } catch {
                $openXrRuntimePlan.runtimeLogIdentityError = $_.Exception.Message
            }
        }
    }
    if ($runtimeRegistryBefore) {
        try {
            $openXrRuntimePlan.hklmActiveRuntimeAfter =
                Assert-FnvxrProductHklmActiveRuntimeUnchanged `
                    -Before $runtimeRegistryBefore
            $openXrRuntimePlan.hklmActiveRuntimeUnchanged = $true
        } catch {
            $openXrRuntimePlan.hklmActiveRuntimeAfter =
                Get-FnvxrProductHklmActiveRuntimeSnapshot
            $openXrRuntimePlan.hklmActiveRuntimeUnchanged = $false
            $openXrRuntimePlan.status = "failed-registry-assertion"
            $openXrRuntimePlan.registryAssertionError = $_.Exception.Message
            if ($manifest.error) {
                $manifest.error = "{0} Registry assertion also failed: {1}" -f
                    $manifest.error,
                    $_.Exception.Message
            } else {
                $manifest.error = $_.Exception.Message
            }
            $manifest.state = "failed"
            Write-SupervisorLog ("ERROR " + $_.Exception.Message)
        }
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
