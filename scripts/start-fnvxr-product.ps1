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
    # Match the attached Quest runtime's reported per-eye recommendation.
    # VR projection views are headset-shaped (near-square/portrait), not a
    # desktop 16:9 backbuffer.
    [ValidateRange(1280, 4096)][int]$PhysicalGameWidth = 1872,
    [ValidateRange(720, 2560)][int]$PhysicalGameHeight = 2016,
    # Optional workspace-staged Meta XR Operator API layer. It is observed
    # only: FNVXR neither starts its MCP proxy nor invokes pose/controller tools.
    [string]$MetaXrOperatorLayerDirectory = "",
    [ValidateRange(1, 2000000000)][int]$HostFrames = 60000,
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
    # A fixed stock weapon is saved only into a new owned fixture lineage.
    # This is a visibility/loadout selector, not a general inventory command.
    [ValidateSet(
        "None", "Pistol", "RifleSingleHand", "RifleTwoHand", "Minigun",
        "FragGrenade", "Knife", "ThrowingKnife")]
    [string]$RetailFixtureWeapon = "None",
    # Writes a bounded image sequence from the final OpenXR eye swapchains.
    # This is a command-line-only simulator mirror; it never drives the game
    # window, a controller, or simulator UI.
    [switch]$CaptureHeadsetMirror,
    [ValidateRange(1, 600)][int]$HeadsetMirrorCaptureEveryFrames = 6,
    [ValidateRange(1, 3600)][int]$HeadsetMirrorCaptureMaxPairs = 180,
    # Records the simulator's own native side-by-side preview window across
    # the controller and HMD sweeps after a ready weapon. This is a video
    # capture of the final OpenXR display, not a retail render path.
    [switch]$RecordSimulatorSbs,
    [ValidateRange(3, 120)][int]$SimulatorSbsRecordSeconds = 18,
    # Opt in to loading an owned FNVXR_AutoRetail fixture through the visual
    # trial, then showing its Pip-Boy via two fixed in-game Tab events. This
    # is only valid alongside the final headset mirror capture.
    [switch]$HeadsetDemoFixture,
    # Loads the same owned fixture into the headless OpenXR visual trial but
    # keeps it in gameplay for a sustained world-only stereo recording. This
    # mode never enables the demo's fixed Pip-Boy input events.
    [switch]$HeadsetWorldOnlyCapture,
    # Finalizes the loaded owned fixture after stock notices have cleared, then
    # submits JIP's fixed SetWeaponOut command for its named equipped weapon.
    # It is a visibility/draw check, not general keyboard, controller, firing,
    # desktop, or simulator control.
    [switch]$HeadsetFixtureWeaponDraw,
    # Select exactly one audited outer RenderFirstPerson caller for the
    # private proof route. None keeps every caller stock-only until a caller
    # has been trace-proven.
    [ValidateSet("None", "Primary", "Alternate", "Third")]
    [string]$RetailVrFirstPersonPrivateCaller = "None",
    # Enables only controller-driven visual hand/weapon transforms for the
    # named stock weapon in the owned, headless world-only fixture. The host
    # still owns OpenXR poses and final eye submission; no input, fire,
    # projectile, hit, camera-hook, or physical-headset path is enabled.
    [switch]$HeadsetControllerRigVisualTrial,
    # Leaves the isolated simulator controller stream manual for authentic
    # Pip-Boy navigation and inventory equip proof.
    [switch]$HeadsetInventoryVisualTrial,
    # Diagnostic-only selection of authenticated first-person roots:
    # weapon=1, upper-body=2, left-hand=4, right-hand=8, Pip-Boy=16.
    [ValidateRange(1, 31)][int]$FirstPersonRootMask = 1,
    # In the owned headless fixture only, authorizes a bounded normal-input
    # sequence: right trigger fires and left-controller X reloads.
    [switch]$HeadsetCombatVisualTrial,
    # Drives a deterministic, bounded six-axis HMD pose sweep through the
    # headless runtime's per-run file IPC while world-only capture is active.
    # The runtime keeps both controllers in their tracked head-relative poses.
    [switch]$HeadsetPoseSweep,
    [ValidateRange(2, 120)][int]$HeadsetPoseSweepSeconds = 12,
    # Drives only the right OpenXR grip/aim controller through translation and
    # yaw/pitch/roll while the HMD stays fixed. It is valid only for the
    # headless visual-rig fixture trial.
    [switch]$ControllerPoseSweep,
    [ValidateRange(2, 120)][int]$ControllerPoseSweepSeconds = 14,
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

if ([Environment]::GetEnvironmentVariable(
        "FNVXR_HEADSET_FINAL_STOCK_FRAME_CAPTURE",
        [EnvironmentVariableTarget]::Process) -eq "1") {
    throw "FNVXR_HEADSET_FINAL_STOCK_FRAME_CAPTURE is forbidden: final-stock backbuffer copies are not an engine-rendered stereo proof source."
}

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
if ($HeadsetFixtureWeaponDraw -and -not $HeadsetWorldOnlyCapture) {
    throw "-HeadsetFixtureWeaponDraw requires -HeadsetWorldOnlyCapture."
}
if ($HeadsetFixtureWeaponDraw -and $RetailFixtureAction -cne "Load") {
    throw "-HeadsetFixtureWeaponDraw requires -RetailFixtureAction Load for an existing owned fixture."
}
if ($HeadsetFixtureWeaponDraw -and $RetailFixtureWeapon -ceq "None") {
    throw "-HeadsetFixtureWeaponDraw requires a named -RetailFixtureWeapon."
}
if (-not $HeadsetFixtureWeaponDraw -and $RetailVrFirstPersonPrivateCaller -cne "None") {
    throw "-RetailVrFirstPersonPrivateCaller requires -HeadsetFixtureWeaponDraw."
}
if ($HeadsetControllerRigVisualTrial -and -not $HeadsetFixtureWeaponDraw) {
    throw "-HeadsetControllerRigVisualTrial requires -HeadsetFixtureWeaponDraw."
}
if ($HeadsetControllerRigVisualTrial -and
    [string]::IsNullOrWhiteSpace($HeadlessSimulatorManifest)) {
    throw "-HeadsetControllerRigVisualTrial requires -HeadlessSimulatorManifest."
}
if ($ControllerPoseSweep -and -not $HeadsetControllerRigVisualTrial) {
    throw "-ControllerPoseSweep requires -HeadsetControllerRigVisualTrial."
}
if ($HeadsetCombatVisualTrial -and -not $HeadsetControllerRigVisualTrial) {
    throw "-HeadsetCombatVisualTrial requires -HeadsetControllerRigVisualTrial."
}
if ($HeadsetInventoryVisualTrial -and -not $HeadsetControllerRigVisualTrial) {
    throw "-HeadsetInventoryVisualTrial requires -HeadsetControllerRigVisualTrial."
}
if ($HeadsetInventoryVisualTrial -and $HeadsetCombatVisualTrial) {
    throw "-HeadsetInventoryVisualTrial and -HeadsetCombatVisualTrial are mutually exclusive."
}
if ($HeadsetCombatVisualTrial -and
    -not ($RecordSimulatorSbs -or $CaptureHeadsetMirror)) {
    throw "-HeadsetCombatVisualTrial requires -RecordSimulatorSbs or -CaptureHeadsetMirror so firing and reload are recorded."
}
if (-not $ControllerPoseSweep -and $ControllerPoseSweepSeconds -ne 14) {
    throw "-ControllerPoseSweepSeconds requires -ControllerPoseSweep."
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
if ($RetailFixtureAction -eq "Disabled" -and
    $RetailFixtureWeapon -cne "None") {
    throw "-RetailFixtureWeapon requires -RetailFixtureAction Create, Load, or Ensure."
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
if ($RecordSimulatorSbs -and [string]::IsNullOrWhiteSpace($HeadlessSimulatorManifest)) {
    throw "-RecordSimulatorSbs requires -HeadlessSimulatorManifest for the simulator's native SBS preview."
}
if ($RecordSimulatorSbs -and -not $HeadsetWorldOnlyCapture) {
    throw "-RecordSimulatorSbs requires -HeadsetWorldOnlyCapture."
}
if ($RecordSimulatorSbs -and -not $HeadsetFixtureWeaponDraw) {
    throw "-RecordSimulatorSbs requires -HeadsetFixtureWeaponDraw so the video has a proven weapon-out state."
}
if ($RecordSimulatorSbs -and -not $HeadsetPoseSweep) {
    throw "-RecordSimulatorSbs requires -HeadsetPoseSweep so the video includes bounded full head motion."
}
if ($RecordSimulatorSbs -and -not $ControllerPoseSweep) {
    throw "-RecordSimulatorSbs requires -ControllerPoseSweep so the run has both independent 6DoF sweep proofs."
}
if ($RecordSimulatorSbs -and $RetailVrFirstPersonPrivateCaller -cne "None") {
    throw "-RecordSimulatorSbs requires -RetailVrFirstPersonPrivateCaller None so the center-integrated stock weapon is the only first-person weapon rendered."
}
if ($RecordSimulatorSbs -and
    $SimulatorSbsRecordSeconds -lt
        ($ControllerPoseSweepSeconds + $HeadsetPoseSweepSeconds)) {
    throw "-SimulatorSbsRecordSeconds must cover the complete controller and HMD sweep intervals."
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
if ($PhysicalHeadsetPlay -and ($HeadsetControllerRigVisualTrial -or $ControllerPoseSweep)) {
    throw "-PhysicalHeadsetPlay cannot use the headless controller visual-rig trial or controller sweep."
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
if ($headsetFixtureVisualTrial -and
    -not ($CaptureHeadsetMirror -or $RecordSimulatorSbs)) {
    throw "-HeadsetDemoFixture and -HeadsetWorldOnlyCapture require -CaptureHeadsetMirror or -RecordSimulatorSbs so final headset output is recorded."
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
        Weapon = $RetailFixtureWeapon
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
$retailFixtureWeapon = if ($retailFixtureRequested) {
    Resolve-FnvxrProductRetailFixtureWeapon -Weapon $RetailFixtureWeapon
} else {
    "None"
}
$retailFixtureSaveName = if ($retailFixtureRequested) {
    if ($TtwCore) {
        Get-FnvxrProductTtwFixtureSaveName `
            -TraitOne $retailFixtureTraits.first `
            -TraitTwo $retailFixtureTraits.second `
            -Weapon $retailFixtureWeapon
    } else {
        Get-FnvxrProductRetailFixtureSaveName `
            -TraitOne $retailFixtureTraits.first `
            -TraitTwo $retailFixtureTraits.second `
            -Weapon $retailFixtureWeapon
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
$finalizingRetailFixture = $loadingRetailFixture -and
    [bool]$HeadsetFixtureWeaponDraw
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
    scope = "temporary headset-aspect retail source profile; both Fallout INIs are restored byte-for-byte after owned processes stop"
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
        if ($HeadsetFixtureWeaponDraw) {
            "headset-world-weapon-draw-$fixtureFamily-fixture-$resolvedRetailFixtureAction"
        } else {
            "headset-world-$fixtureFamily-fixture-$resolvedRetailFixtureAction"
        }
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
            if ($HeadsetFixtureWeaponDraw) {
                if ($TtwCore) {
                    "one owned FNVXR_AutoTTW level-one fixture in the isolated TTW workspace sandbox and process-local headless OpenXR visual trial; temporary exact TTW core profile; exact title/body/unique-OK official-pack acknowledgement if presented; one exact save back to that same owned fixture after clean gameplay, then one fixed JIP SetWeaponOut command for the named holstered stock weapon; sustained world-only eye capture; no personal save, live retail, desktop, keyboard, mouse, controller mutation, firing, camera hook, rig, simulator UI, or general command authority"
                } else {
                    "one owned FNVXR_AutoRetail level-one fixture in the process-local headless OpenXR visual trial; temporary FalloutNV.esm-only profile; exact title/body/unique-OK official-pack acknowledgement if presented; one exact save back to that same owned fixture after clean gameplay, then one fixed JIP SetWeaponOut command for the named holstered stock weapon; sustained world-only eye capture; no personal save, TTW, desktop, keyboard, mouse, controller mutation, firing, camera hook, rig, simulator UI, or general command authority"
                }
            } elseif ($TtwCore) {
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
    fixtureFinalizationSaveRequested = [bool]$finalizingRetailFixture
    fixtureFinalizationSaveChanged = $null
    fixtureFinalizationNvseChanged = $null
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
            -HeadsetFixtureWeaponDraw:$HeadsetFixtureWeaponDraw `
            -RetailVrFirstPersonPrivateCaller $RetailVrFirstPersonPrivateCaller `
            -HeadsetControllerRigVisualTrial:$HeadsetControllerRigVisualTrial `
            -HeadsetInventoryVisualTrial:$HeadsetInventoryVisualTrial `
            -HeadsetCombatVisualTrial:$HeadsetCombatVisualTrial `
            -PhysicalHeadsetPlay:$PhysicalHeadsetPlay `
            -PhysicalGameWidth $PhysicalGameWidth `
            -PhysicalGameHeight $PhysicalGameHeight `
            -HeadsetDemoGameplayWarmupFrames $HeadsetDemoGameplayWarmupFrames `
            -RetailVrAccumulationDiagnosticMode $RetailVrAccumulationDiagnosticMode `
            -RetailFixtureAction $resolvedRetailFixtureAction `
            -RetailFixtureSaveName $retailFixtureSaveName `
            -RetailFixtureTraitOne $(if ($retailFixtureRequested) { $retailFixtureTraits.first } else { "None" }) `
            -RetailFixtureTraitTwo $(if ($retailFixtureRequested) { $retailFixtureTraits.second } else { "None" }) `
            -RetailFixtureWeapon $retailFixtureWeapon `
            -AcknowledgeTribalPackPopup:$AcknowledgeTribalPackPopup `
            -AutomateRecoverySaveName $RetailSaveName `
            -HeadlessRuntimeManifest $headlessRuntimeManifestPath `
            -SimulatorDesktopPreview:$RecordSimulatorSbs `
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
$simulatorSbsVideoPath = if ($RecordSimulatorSbs) {
    Join-Path $runDirectory "simulator-native-sbs.mp4"
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
        cardinalScript = if ($PhysicalHeadsetPlay) {
            Join-Path $runDirectory "phase1-physical-cardinal-script.json"
        } else {
            $null
        }
        cardinalScriptStatus = if ($PhysicalHeadsetPlay) {
            "pending-retention"
        } else {
            "disabled"
        }
        cardinalScriptEvidence = $null
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
    simulatorSbsVideo = [ordered]@{
        requested = [bool]$RecordSimulatorSbs
        source = "native OpenXR Simulator SBS preview client area"
        firstPersonPrivateCaller = $RetailVrFirstPersonPrivateCaller
        sourceSafety = "single center-integrated stock first-person weapon; private per-eye calls disabled"
        path = $simulatorSbsVideoPath
        durationSeconds = $SimulatorSbsRecordSeconds
        frameRate = if ($RecordSimulatorSbs) { 30 } else { $null }
        status = if ($RecordSimulatorSbs) { "pending-ready-weapon-and-head-sweep" } else { "disabled" }
        proof = $null
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
        weaponDrawRequested = [bool]$HeadsetFixtureWeaponDraw
        fixtureFamily = $fixtureFamily
        scope = if ($HeadsetFixtureWeaponDraw) {
            "owned fixture world-only final OpenXR eye-swapchain capture plus one save to the same owned fixture after its exact stock notices clear and one fixed JIP SetWeaponOut command for a named holstered stock weapon; no Pip-Boy, OS, desktop, keyboard, mouse, controller, firing, or simulator input"
        } else {
            "owned fixture world-only final OpenXR eye-swapchain capture; no Pip-Boy, OS, desktop, keyboard, mouse, controller, weapon, or simulator input"
        }
        status = if ($HeadsetWorldOnlyCapture) {
            "pending-owned-fixture-load"
        } else {
            "disabled"
        }
        inputTelemetry = $headsetDemoInputTelemetryLog
        weaponDrawProof = $null
        firstPersonRenderProof = $null
        centerIntegratedFirstPersonProof = $null
        continuityProof = $null
    }
    headsetControllerRigVisualTrial = [ordered]@{
        requested = [bool]$HeadsetControllerRigVisualTrial
        scope = if ($HeadsetCombatVisualTrial) {
            "headless owned-fixture controller proof: tracked hands/weapon plus native gameplay, Pip-Boy, and menu controls while engine-center stereo retains final-eye authority; no desktop, window, mouse, simulator GUI, camera hook, replay, or physical-headset route"
        } elseif ($HeadsetInventoryVisualTrial) {
            "headless owned-fixture live-prop proof: one-to-one tracked hands, stock weapon on the right hand, and the real Pip-Boy on the opposite wrist with a bounded right-ray focus gesture; no desktop, window, mouse, simulator GUI, camera hook, replay, firing, or physical-headset route"
        } else {
            "headless owned-fixture visual rig only: right OpenXR grip/aim may drive stock first-person hand/weapon transforms while engine-center stereo retains final-eye authority; no input, firing, projectile, hit, camera hook, replay, UI, or physical-headset route"
        }
        status = if ($HeadsetControllerRigVisualTrial) {
            "pending-owned-fixture-weapon-and-rig-proof"
        } else {
            "disabled"
        }
        evidence = $null
        livePipBoyProof = $null
    }
    headsetCombatVisualTrial = [ordered]@{
        requested = [bool]$HeadsetCombatVisualTrial
        scope = "owned headless fixture only: sticks, native Pip-Boy/menu selection, smooth multi-target controller motion, firing, and reload through per-run OpenXR IPC; no desktop, window, mouse, or simulator UI control"
        status = if ($HeadsetCombatVisualTrial) { "pending-ready-weapon" } else { "disabled" }
        evidence = $null
    }
    headsetPoseSweep = [ordered]@{
        requested = [bool]$HeadsetPoseSweep
        scope = "bounded per-run headless-runtime cardinal HMD commands: +/-100 mm X/Y/Z and +/-15 degrees yaw/pitch/roll, one signed axis at a time; controllers retain tracked head-relative poses; no game, xNVSE input, desktop, registry, or simulator GUI control"
        durationSeconds = $HeadsetPoseSweepSeconds
        status = if ($HeadsetPoseSweep) { "pending-gameplay" } else { "disabled" }
        evidence = $null
    }
    controllerPoseSweep = [ordered]@{
        requested = [bool]$ControllerPoseSweep
        scope = "bounded right-controller LOCAL-space translations plus yaw/pitch/roll with a static HMD; final proof requires retail rig telemetry showing controller-only hand and stock-weapon motion"
        durationSeconds = $ControllerPoseSweepSeconds
        status = if ($ControllerPoseSweep) { "pending-gameplay" } else { "disabled" }
        evidence = $null
        renderedRigProof = $null
    }
    phase1 = [ordered]@{
        schema = "fnvxr-phase1-6dof-evidence-v1"
        status = "pending-run-analysis"
        evidencePath = Join-Path $runDirectory "phase1-6dof-evidence.json"
        report = $null
        physicalHeadsetGateAccepted = $false
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
        simulatorSbsVideo = $simulatorSbsVideoPath
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
$simulatorSbsRecorderProcess = $null
$simulatorSbsVideoProof = $null
$centerIntegratedFirstPersonProof = $null
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

function Initialize-FnvxrSimulatorPreviewNative {
    if (-not ("FnvxrSimulatorPreviewNative" -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class FnvxrSimulatorPreviewNative
{
    private static System.Threading.Timer isolationTimer;
    private static uint isolatedProcessId;
    private delegate bool EnumWindowsProc(IntPtr window, IntPtr parameter);

    [StructLayout(LayoutKind.Sequential)]
    private struct RECT { public int left; public int top; public int right; public int bottom; }

    [StructLayout(LayoutKind.Sequential)]
    private struct POINT { public int x; public int y; }

    [DllImport("user32.dll")]
    private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetClassName(IntPtr window, StringBuilder name, int capacity);
    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll")]
    private static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")]
    private static extern bool ShowWindow(IntPtr window, int command);
    [DllImport("user32.dll")]
    private static extern bool GetClientRect(IntPtr window, out RECT rectangle);
    [DllImport("user32.dll")]
    private static extern bool ClientToScreen(IntPtr window, ref POINT point);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetWindowText(IntPtr window, StringBuilder title, int capacity);

    private static IntPtr Find(uint expectedProcessId)
    {
        IntPtr[] found = new IntPtr[] { IntPtr.Zero };
        EnumWindows(delegate(IntPtr window, IntPtr parameter) {
            uint processId;
            GetWindowThreadProcessId(window, out processId);
            if (processId != expectedProcessId || !IsWindowVisible(window)) return true;
            StringBuilder className = new StringBuilder(128);
            GetClassName(window, className, className.Capacity);
            if (!String.Equals(className.ToString(), "OpenXR Simulator", StringComparison.Ordinal)) return true;
            found[0] = window;
            return false;
        }, IntPtr.Zero);
        return found[0];
    }

    public static int[] FindClientBounds(uint expectedProcessId)
    {
        IntPtr window = Find(expectedProcessId);
        if (window == IntPtr.Zero) return new int[0];
        RECT rectangle;
        POINT origin = new POINT { x = 0, y = 0 };
        if (!GetClientRect(window, out rectangle) || !ClientToScreen(window, ref origin)) return new int[0];
        int width = rectangle.right - rectangle.left;
        int height = rectangle.bottom - rectangle.top;
        if (width <= 0 || height <= 0) return new int[0];
        return new int[] { origin.x, origin.y, width, height };
    }

    public static string FindTitle(uint expectedProcessId)
    {
        IntPtr window = Find(expectedProcessId);
        if (window == IntPtr.Zero) return String.Empty;
        StringBuilder title = new StringBuilder(768);
        GetWindowText(window, title, title.Capacity);
        return title.ToString();
    }

    public static int HideProcessWindows(uint expectedProcessId)
    {
        int[] hidden = new int[] { 0 };
        EnumWindows(delegate(IntPtr window, IntPtr parameter) {
            uint processId;
            GetWindowThreadProcessId(window, out processId);
            if (processId != expectedProcessId) return true;
            if (IsWindowVisible(window) && ShowWindow(window, 0)) hidden[0]++;
            return true;
        }, IntPtr.Zero);
        return hidden[0];
    }

    public static void StartWindowIsolation(uint expectedProcessId)
    {
        StopWindowIsolation();
        isolatedProcessId = expectedProcessId;
        isolationTimer = new System.Threading.Timer(
            delegate(object state) { HideProcessWindows(isolatedProcessId); },
            null,
            0,
            25);
    }

    public static void StopWindowIsolation()
    {
        System.Threading.Timer timer = isolationTimer;
        isolationTimer = null;
        isolatedProcessId = 0;
        if (timer != null) timer.Dispose();
    }
}
'@
    }
}

function Get-FnvxrSimulatorPreviewClientBounds {
    param([Parameter(Mandatory = $true)][uint32]$ProcessId)

    Initialize-FnvxrSimulatorPreviewNative

    $bounds = [FnvxrSimulatorPreviewNative]::FindClientBounds($ProcessId)
    if ($null -eq $bounds -or $bounds.Length -ne 4) {
        return $null
    }
    return [pscustomobject][ordered]@{
        x = [int]$bounds[0]
        y = [int]$bounds[1]
        width = [int]$bounds[2]
        height = [int]$bounds[3]
        title = [FnvxrSimulatorPreviewNative]::FindTitle($ProcessId)
    }
}

function Wait-FnvxrSimulatorPreviewClientBounds {
    param(
        [Parameter(Mandatory = $true)][uint32]$ProcessId,
        [ValidateRange(1, 30)][int]$TimeoutSeconds = 15
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $bounds = Get-FnvxrSimulatorPreviewClientBounds -ProcessId $ProcessId
        if ($bounds) { return $bounds }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)

    throw "Timed out waiting for the owned OpenXR Simulator SBS preview window."
}

function Start-FnvxrSimulatorSbsRecorder {
    param(
        [Parameter(Mandatory = $true)][uint32]$HostProcessId,
        [Parameter(Mandatory = $true)][string]$OutputPath,
        [Parameter(Mandatory = $true)][int]$DurationSeconds
    )

    $bounds = Wait-FnvxrSimulatorPreviewClientBounds -ProcessId $HostProcessId
    $perEyeAspect = ([double]$bounds.width / 2.0) / [double]$bounds.height
    if ([Math]::Abs($perEyeAspect - (16.0 / 9.0)) -gt 0.02) {
        throw (
            "OpenXR Simulator preview has an invalid SBS client aspect {0}x{1}; expected two 16:9 eyes." -f
            $bounds.width, $bounds.height)
    }
    if ([string]::IsNullOrWhiteSpace($bounds.title)) {
        throw "OpenXR Simulator preview has no readable window title for native SBS recording."
    }
    $ffmpeg = Get-Command ffmpeg -ErrorAction Stop
    $arguments = @(
        "-hide_banner", "-loglevel", "error",
        "-f", "gdigrab", "-draw_mouse", "0", "-framerate", "30",
        "-i", ("title={0}" -f $bounds.title),
        "-t", [string]$DurationSeconds,
        "-c:v", "libx264", "-preset", "medium", "-crf", "17",
        "-pix_fmt", "yuv420p", "-movflags", "+faststart", "-y", $OutputPath)
    $process = Start-Process `
        -FilePath $ffmpeg.Source `
        -ArgumentList $arguments `
        -WindowStyle Hidden `
        -PassThru
    return [pscustomobject][ordered]@{
        process = $process
        bounds = $bounds
    }
}

function Complete-FnvxrSimulatorSbsRecorder {
    param(
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$Process,
        [Parameter(Mandatory = $true)][string]$OutputPath,
        [Parameter(Mandatory = $true)][int]$ExpectedDurationSeconds
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($ExpectedDurationSeconds + 15)
    do {
        $Process.Refresh()
        if ($Process.HasExited) { break }
        Start-Sleep -Milliseconds 200
    } while ([DateTime]::UtcNow -lt $deadline)
    $Process.Refresh()
    if (-not $Process.HasExited) {
        Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
        throw "Simulator SBS recorder did not complete in its bounded duration."
    }
    if ($Process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $OutputPath -PathType Leaf)) {
        throw "Simulator SBS recorder failed with exit code $($Process.ExitCode)."
    }
    $ffprobe = Get-Command ffprobe -ErrorAction Stop
    $probeOutput = @(
        & $ffprobe.Source -v error -select_streams v:0 `
            -show_entries stream=width,height,avg_frame_rate,duration `
            -of json $OutputPath
    ) -join [Environment]::NewLine
    $probe = $probeOutput | ConvertFrom-Json -ErrorAction Stop
    $stream = @($probe.streams)[0]
    if (-not $stream -or [int]$stream.width -le 0 -or [int]$stream.height -le 0) {
        throw "Simulator SBS video has no readable video stream."
    }
    $perEyeAspect = (([double]$stream.width / 2.0) / [double]$stream.height)
    if (($stream.width % 2) -ne 0 -or
        [Math]::Abs($perEyeAspect - (16.0 / 9.0)) -gt 0.02) {
        throw "Simulator SBS video did not retain two un-stretched 16:9 eye images."
    }
    $identity = Get-FnvxrProductFileIdentity -Path $OutputPath
    return [ordered]@{
        path = $OutputPath
        length = $identity.length
        sha256 = $identity.sha256
        width = [int]$stream.width
        height = [int]$stream.height
        durationSeconds = [double]$stream.duration
        averageFrameRate = [string]$stream.avg_frame_rate
        perEyeAspect = $perEyeAspect
    }
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
    # Certify a contiguous recovered window. A rejected startup submit must
    # invalidate the candidate that contains it, but it must not poison every
    # clean transaction that follows for the remainder of the process.
    $transactions = [ordered]@{}
    $poseSequences = @{}
    $first = $null
    $last = $null
    $gameplaySubmitFrames = 0
    $qualifiedProof = $null
    foreach ($line in @(Get-Content -LiteralPath $LogPath -Tail 5000)) {
        if (-not $line.StartsWith('{"event":"fnvxrOpenXrSubmit"')) {
            continue
        }
        try {
            $frame = $line | ConvertFrom-Json -ErrorAction Stop
            if (-not ([bool]$frame.runtimeGameplay -and
                    [bool]$frame.runtimeShouldRender)) {
                continue
            }
            if (-not [bool]$frame.projectionLayerSubmitted -or
                [int]$frame.layerCount -ne 1 -or
                [string]$frame.xrEndFrame -ne "XR_SUCCESS") {
                $transactions = [ordered]@{}
                $poseSequences = @{}
                $first = $null
                $last = $null
                $gameplaySubmitFrames = 0
                $qualifiedProof = $null
                continue
            }
            ++$gameplaySubmitFrames
            $proof = ConvertTo-FnvxrProductStereoOutputProof -Frame $frame
            if (-not $proof) { continue }
            $transactionKey = "{0}:{1}" -f
                $proof.transport,
                $proof.transaction
            if (-not $transactions.Contains($transactionKey)) {
                $transactions[$transactionKey] = $proof
            }
            $poseSequences[[string]$proof.poseSequence] = $true
            if (-not $first) { $first = $proof }
            $last = $proof
            if ($transactions.Count -lt $MinimumTransactions -or
                $poseSequences.Count -lt $MinimumTransactions) {
                continue
            }
            $durationMilliseconds =
                [uint64]$last.hostWallClockUnixMilliseconds -
                [uint64]$first.hostWallClockUnixMilliseconds
            if ($durationMilliseconds -lt $MinimumDurationMilliseconds -or
                [uint64]$last.frame -le [uint64]$first.frame -or
                $gameplaySubmitFrames -lt $MinimumTransactions) {
                continue
            }
            $qualifiedProof = [ordered]@{
                uniqueTransactions = $transactions.Count
                uniquePoseSequences = $poseSequences.Count
                durationMilliseconds = $durationMilliseconds
                gameplaySubmitFrames = $gameplaySubmitFrames
                rejectedGameplaySubmitFrames = 0
                first = $first
                last = $last
                observedAtUtc = [DateTime]::UtcNow.ToString("o")
            }
        } catch {
            continue
        }
    }
    return $qualifiedProof
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
            [bool]$frame.livePipBoy -and
            -not [bool]$frame.uiQuadVisible -and
            [bool]$frame.cpuEngineStereoActive -and
            [string]$frame.cpuPresentationMode -ceq "gameplay" -and
            [uint32]$frame.cpuEngineProducerMode -eq 4 -and
            [uint64]$frame.sourceRenderPairSequence -gt 0 -and
            [uint32]$frame.sourcePoseSequence -gt 0 -and
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
            [string]$frame.rightOutputHash -ne "0x0" -and
            [string]$frame.leftOutputHash -cne
                [string]$frame.rightOutputHash) {
            return [ordered]@{
                frame = [uint64]$frame.frame
                runtimeState = "live-pipboy-world"
                menuBits = [uint32]$frame.runtimeMenuBits
                transaction = [uint64]$frame.cpuEngineTransaction
                poseSequence = [uint32]$frame.sourcePoseSequence
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

function Get-FnvxrProductHeadsetFixtureWeaponDrawProof {
    param([Parameter(Mandatory = $true)][string]$LogPath)

    if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
        return $null
    }
    $fixtureSave = $null
    $issuedDraw = $null
    $result = $null
    foreach ($line in @(Get-Content -LiteralPath $LogPath -Tail 1200)) {
        if (-not ($line.StartsWith('{"event":"fnvxrHeadsetFixtureFinalizeSave"') -or
                $line.StartsWith('{"event":"fnvxrHeadsetFixtureWeaponDraw"') -or
                $line.StartsWith('{"event":"fnvxrHeadsetFixtureWeaponDrawResult"'))) {
            continue
        }
        try {
            $event = $line | ConvertFrom-Json -ErrorAction Stop
        } catch {
            continue
        }
        # runPluginConsoleCommand emits a compact command echo with the same
        # event name as the structured proof record.  Accept only the latter;
        # a partial echo must never terminate supervision under StrictMode.
        $hasFixtureSaveShape =
            $null -ne $event.PSObject.Properties['commandBuilt'] -and
            $null -ne $event.PSObject.Properties['submitted']
        $hasDrawShape =
            $null -ne $event.PSObject.Properties['action'] -and
            $null -ne $event.PSObject.Properties['source'] -and
            $null -ne $event.PSObject.Properties['submitted']
        if ($event.event -eq "fnvxrHeadsetFixtureFinalizeSave" -and
            $hasFixtureSaveShape -and
            [bool]$event.commandBuilt -and [bool]$event.submitted) {
            $fixtureSave = $event
        } elseif ($event.event -eq "fnvxrHeadsetFixtureWeaponDraw" -and
            $hasDrawShape -and
            [bool]$event.submitted -and
            $event.action -eq "set-weapon-out" -and
            $event.source -eq "JIP") {
            $issuedDraw = $event
        } elseif ($event.event -eq "fnvxrHeadsetFixtureWeaponDrawResult" -and
            [bool]$event.success -and (
                [bool]$event.alreadyReady -or (
                    $issuedDraw -and
                    [string]$event.weapon -ceq [string]$issuedDraw.weapon))) {
            $result = $event
        }
    }
    if (-not $fixtureSave -or -not $result) {
        return $null
    }
    return [ordered]@{
        weapon = [string]$result.weapon
        saveName = [string]$fixtureSave.saveName
        saveFrame = [uint64]$fixtureSave.frame
        submitted = [bool]$issuedDraw
        alreadyReady = [bool]$result.alreadyReady
        issuedFrame = if ($issuedDraw) { [uint64]$issuedDraw.frame } else { $null }
        resultFrame = [uint64]$result.frame
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
            $right = Get-Item -LiteralPath $rightPath -Force
            # The host publishes each eye PNG by atomic rename. Keep this
            # defensive size check as well, so an interrupted older capture
            # can never be mistaken for a complete recording pair.
            if ($left.Length -le 0 -or $right.Length -le 0) {
                continue
            }
            $pairs += [ordered]@{
                ordinal = $left.BaseName -replace "^pair_", "" -replace "_left$", ""
                left = $left.FullName
                right = $right.FullName
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

function Get-FnvxrProductCenterIntegratedFirstPersonProof {
    param([Parameter(Mandatory = $true)][string]$RetailVrLogPath)

    if (-not (Test-Path -LiteralPath $RetailVrLogPath -PathType Leaf)) {
        return $null
    }
    $events = @()
    foreach ($line in @(Get-Content -LiteralPath $RetailVrLogPath -Tail 12000)) {
        $jsonStart = $line.IndexOf(
            '{"event":"fnvxrRetailCenterIntegratedFirstPerson"')
        if ($jsonStart -lt 0) { continue }
        try {
            $event = $line.Substring($jsonStart) |
                ConvertFrom-Json -ErrorAction Stop
        } catch {
            continue
        }
        if ([string]$event.event -ceq
                "fnvxrRetailCenterIntegratedFirstPerson" -and
            [bool]$event.weaponFrameConsumed -and
            [uint64]$event.weaponFrameCommitId -gt 0 -and
            [uint32]$event.weaponFrameFailure -eq 0 -and
            [bool]$event.published -and
            [int]$event.privateEyeCalls -eq 0) {
            $events += $event
        }
    }
    if ($events.Count -eq 0) { return $null }
    $proof = $events[$events.Count - 1]
    return [ordered]@{
        callerId = [uint32]$proof.callerId
        callerAddress = [string]$proof.callerAddress
        callerOrdinal = [long]$proof.callerOrdinal
        poseSequence = [uint32]$proof.poseSequence
        poseFrame = [uint64]$proof.poseFrame
        weaponFrameConsumed = [bool]$proof.weaponFrameConsumed
        weaponFrameCommitId = [uint64]$proof.weaponFrameCommitId
        published = [bool]$proof.published
        privateEyeCalls = [int]$proof.privateEyeCalls
        renderPath = "center-integrated-stock-first-person"
        observedAtUtc = [DateTime]::UtcNow.ToString("o")
    }
}

function Get-FnvxrProductRetailFirstPersonRenderProof {
    param(
        [Parameter(Mandatory = $true)][string]$RetailVrLogPath,
        [Parameter(Mandatory = $true)][string]$WeaponTelemetryLogPath,
        [Parameter(Mandatory = $true)]
        [ValidateSet("Primary", "Alternate", "Third")]
        [string]$RequiredCaller
    )

    if (-not (Test-Path -LiteralPath $RetailVrLogPath -PathType Leaf)) {
        return $null
    }
    $callerId = switch ($RequiredCaller) {
        "Primary" { 1 }
        "Alternate" { 2 }
        "Third" { 3 }
    }
    $firstPersonEvents = @()
    try {
        $retailVrLines = @(
            Get-Content -LiteralPath $RetailVrLogPath -Tail 12000)
    } catch [System.IO.IOException] {
        # The D3D9 writer briefly rotates/flushed this live evidence stream
        # with an exclusive handle. Supervision polls again; a transient share
        # violation is not a product failure after the render remains live.
        return $null
    }
    foreach ($line in $retailVrLines) {
        $jsonStart = $line.IndexOf('{"event":"fnvxrRetailFirstPerson')
        if ($jsonStart -lt 0) { continue }
        try {
            $event = $line.Substring($jsonStart) |
                ConvertFrom-Json -ErrorAction Stop
        } catch {
            continue
        }
        $firstPersonEvents += $event
    }
    $stereo = @($firstPersonEvents | Where-Object {
        [string]$_.event -ceq "fnvxrRetailFirstPersonStereo" -and
        [int]$_.callerId -eq $callerId -and
        [bool]$_.leftRendered -and
        [bool]$_.rightRendered -and
        [bool]$_.targetsRestored -and
        [bool]$_.interEyeD3dStateRestored -and
        [bool]$_.privatePairReady -and
        [bool]$_.desktopFinalizerCompleted -and
        [bool]$_.leftAccumulatorTransactionEntered -and
        [bool]$_.rightAccumulatorTransactionEntered -and
        [bool]$_.leftAccumulatorRestored -and
        [bool]$_.rightAccumulatorRestored -and
        [bool]$_.accumulatorSchedule -and
        [int]$_.privateAccumulatorCalls -eq 2 -and
        [int]$_.desktopAccumulatorCalls -eq 1 -and
        [bool]$_.published -and
        [bool]$_.d3dEvidence
    })
    if ($stereo.Count -eq 0) { return $null }

    $ledger = @($firstPersonEvents | Where-Object {
        [string]$_.event -ceq "fnvxrRetailFirstPersonD3dLedger"
    })
    $accumulator = @($firstPersonEvents | Where-Object {
        [string]$_.event -ceq "fnvxrRetailFirstPersonAccumulatorCall"
    })
    $rig = @()
    if (Test-Path -LiteralPath $WeaponTelemetryLogPath -PathType Leaf) {
        try {
            $weaponTelemetryLines = @(
                Get-Content -LiteralPath $WeaponTelemetryLogPath -Tail 12000)
        } catch [System.IO.IOException] {
            return $null
        }
        foreach ($line in $weaponTelemetryLines) {
            $jsonStart = $line.IndexOf('{"event":"fnvxrRigIndependence"')
            if ($jsonStart -lt 0) { continue }
            try {
                $event = $line.Substring($jsonStart) |
                    ConvertFrom-Json -ErrorAction Stop
            } catch {
                continue
            }
            if ([string]$event.event -ceq "fnvxrRigIndependence") {
                $rig += $event
            }
        }
    }

    [array]::Reverse($stereo)
    foreach ($frame in $stereo) {
        $transaction = [uint64]$frame.transaction
        $poseFrame = [uint64]$frame.poseFrame
        $matchingLedger = @($ledger | Where-Object {
            [uint64]$_.transaction -eq $transaction -and
            [uint64]$_.poseFrame -eq $poseFrame
        })
        $left = @($matchingLedger | Where-Object { [int]$_.eye -eq 0 } |
            Select-Object -Last 1)
        $right = @($matchingLedger | Where-Object { [int]$_.eye -eq 1 } |
            Select-Object -Last 1)
        if ($left.Count -eq 0 -or $right.Count -eq 0) { continue }
        $leftLedger = $left[0]
        $rightLedger = $right[0]
        if ([int]$leftLedger.draws -le 0 -or
            [int]$rightLedger.draws -le 0 -or
            [int]$leftLedger.eyeBoundDraws -le 0 -or
            [int]$rightLedger.eyeBoundDraws -le 0 -or
            [int]$leftLedger.wrongTargetDraws -ne 0 -or
            [int]$rightLedger.wrongTargetDraws -ne 0 -or
            [int]$leftLedger.wrongDepthDraws -ne 0 -or
            [int]$rightLedger.wrongDepthDraws -ne 0 -or
            [int]$leftLedger.viewportMismatches -ne 0 -or
            [int]$rightLedger.viewportMismatches -ne 0 -or
            -not [bool]$leftLedger.rawCaptured -or
            -not [bool]$rightLedger.rawCaptured -or
            [string]$rightLedger.leftHash -eq "0x00000000" -or
            [string]$rightLedger.rightHash -eq "0x00000000" -or
            [string]$rightLedger.leftHash -eq [string]$rightLedger.rightHash) {
            continue
        }
        $privateAccumulatorCalls = @($accumulator | Where-Object {
            [uint64]$_.transaction -eq $transaction -and
            [string]$_.phase -ceq "private-render"
        })
        $desktopAccumulatorCalls = @($accumulator | Where-Object {
            [uint64]$_.transaction -eq $transaction -and
            [string]$_.phase -ceq "desktop-finalize"
        })
        if ($privateAccumulatorCalls.Count -lt 2 -or
            $desktopAccumulatorCalls.Count -lt 1) { continue }

        # xNVSE proves the authenticated Weapon node/descendant and its
        # muzzle alignment. Join that proof to the same tracked pose frame as
        # the D3D ledger; a separate ready-weapon event is not enough.
        $weaponRig = @($rig | Where-Object {
            [uint64]$_.poseFrame -eq $poseFrame -and
            [string]$_.originSource -ceq "headless-stereo-rig-body" -and
            [bool]$_.apply -and
            [bool]$_.rightSolved -and
            [bool]$_.weaponAligned -and
            [bool]$_.weaponWriteApplied -and
            [bool]$_.muzzleMeasured -and
            [bool]$_.muzzleInWeaponBranch
        } | Select-Object -Last 1)
        if ($weaponRig.Count -eq 0) { continue }

        return [ordered]@{
            caller = $RequiredCaller
            callerId = $callerId
            transaction = $transaction
            poseFrame = $poseFrame
            generation = [uint64]$frame.generation
            left = $leftLedger
            right = $rightLedger
            privateAccumulatorCalls = $privateAccumulatorCalls.Count
            desktopAccumulatorCalls = $desktopAccumulatorCalls.Count
            weaponRig = $weaponRig[0]
            observedAtUtc = [DateTime]::UtcNow.ToString("o")
        }
    }
    return $null
}

try {
    if ($PhysicalHeadsetPlay) {
        # This produces an operator-only checklist.  It intentionally has no
        # route to XR, retail, controller, desktop, or simulator input and is
        # never itself an acceptance proof.
        $physicalCardinalOutput = @(
            & (Join-Path $PSScriptRoot "new-fnvxr-phase1-physical-cardinal-script.ps1") `
                -RunDirectory $runDirectory
        ) -join [Environment]::NewLine
        $physicalCardinalScript =
            $physicalCardinalOutput | ConvertFrom-Json -ErrorAction Stop
        if ([string]$physicalCardinalScript.schema -cne
                "fnvxr-phase1-physical-cardinal-script-v1" -or
            @($physicalCardinalScript.steps).Count -ne 12) {
            throw "The Phase 1 physical cardinal script did not retain the complete signed-axis checklist."
        }
        foreach ($axis in @(
            "translationX", "translationY", "translationZ", "yaw", "pitch", "roll")) {
            foreach ($direction in @(-1, 1)) {
                if (@($physicalCardinalScript.steps | Where-Object {
                        [string]$_.axis -ceq $axis -and
                        [int]$_.direction -eq $direction
                    }).Count -ne 1) {
                    throw "The Phase 1 physical cardinal script omitted $axis/$direction."
                }
            }
        }
        $manifest.physicalHeadsetPlay.cardinalScriptStatus =
            "retained-pending-operator-execution"
        $manifest.physicalHeadsetPlay.cardinalScriptEvidence = [ordered]@{
            schema = $physicalCardinalScript.schema
            stepCount = @($physicalCardinalScript.steps).Count
            generatedAtUtc = $physicalCardinalScript.generatedAtUtc
            acceptance = "operator-protocol-only"
        }
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        Write-SupervisorLog (
            "retained input-free physical HMD cardinal protocol steps={0}; operator evidence is still required for Phase 1" -f
            @($physicalCardinalScript.steps).Count)
    }
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
        -HeadsetFixtureWeaponDraw:$HeadsetFixtureWeaponDraw `
        -RetailVrFirstPersonPrivateCaller $RetailVrFirstPersonPrivateCaller `
        -HeadsetControllerRigVisualTrial:$HeadsetControllerRigVisualTrial `
        -HeadsetInventoryVisualTrial:$HeadsetInventoryVisualTrial `
        -HeadsetCombatVisualTrial:$HeadsetCombatVisualTrial `
        -PhysicalHeadsetPlay:$PhysicalHeadsetPlay `
        -PhysicalGameWidth $PhysicalGameWidth `
        -PhysicalGameHeight $PhysicalGameHeight `
        -HeadsetDemoGameplayWarmupFrames $HeadsetDemoGameplayWarmupFrames `
        -RetailVrAccumulationDiagnosticMode $RetailVrAccumulationDiagnosticMode `
        -RetailFixtureAction $resolvedRetailFixtureAction `
        -RetailFixtureSaveName $retailFixtureSaveName `
        -RetailFixtureTraitOne $(if ($retailFixtureRequested) { $retailFixtureTraits.first } else { "None" }) `
        -RetailFixtureTraitTwo $(if ($retailFixtureRequested) { $retailFixtureTraits.second } else { "None" }) `
        -RetailFixtureWeapon $retailFixtureWeapon `
        -AcknowledgeTribalPackPopup:$AcknowledgeTribalPackPopup `
        -AutomateRecoverySaveName $RetailSaveName `
        -HeadlessRuntimeManifest $headlessRuntimeManifestPath `
        -SimulatorDesktopPreview:$RecordSimulatorSbs `
        -PhysicalRuntimeManifest $physicalRuntimeManifestPath `
        -HeadsetMirrorCaptureDirectory $headsetMirrorDirectory `
        -HeadsetMirrorCaptureEveryFrames $HeadsetMirrorCaptureEveryFrames `
        -HeadsetMirrorCaptureMaxPairs $HeadsetMirrorCaptureMaxPairs `
        -MetaXrOperatorLayerDirectory $(if ($metaXrOperatorIdentity) {
            $metaXrOperatorIdentity.directory
        } else { "" })
    $effectiveFirstPersonRootMask = if ($PhysicalHeadsetPlay -or
        $headsetFixtureVisualTrial) { 31 } else { $FirstPersonRootMask }
    $environment.FNVXR_FIRST_PERSON_WEAPON_ROOT =
        $(if (($effectiveFirstPersonRootMask -band 1) -ne 0) { "1" } else { "0" })
    $environment.FNVXR_FIRST_PERSON_UPPER_BODY_ROOT =
        $(if (($effectiveFirstPersonRootMask -band 2) -ne 0) { "1" } else { "0" })
    $environment.FNVXR_FIRST_PERSON_LEFT_HAND_ROOT =
        $(if (($effectiveFirstPersonRootMask -band 4) -ne 0) { "1" } else { "0" })
    $environment.FNVXR_FIRST_PERSON_RIGHT_HAND_ROOT =
        $(if (($effectiveFirstPersonRootMask -band 8) -ne 0) { "1" } else { "0" })
    $environment.FNVXR_FIRST_PERSON_PIPBOY_ROOT =
        $(if (($effectiveFirstPersonRootMask -band 16) -ne 0) { "1" } else { "0" })
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
        Start-Sleep -Milliseconds $(if ($headlessRuntimeIdentity) { 25 } else { 200 })
    } while ([DateTime]::UtcNow -lt $retailDeadline)
    if (-not $fallout) { throw "Timed out waiting for exact FalloutNV.exe process." }

    if ($headlessRuntimeIdentity) {
        Initialize-FnvxrSimulatorPreviewNative
        [FnvxrSimulatorPreviewNative]::StartWindowIsolation(
            [uint32]$fallout.Id)
        $hiddenWindowCount = [FnvxrSimulatorPreviewNative]::HideProcessWindows(
            [uint32]$fallout.Id)
        Write-SupervisorLog (
            "headless retail window isolation armed process={0} initiallyHidden={1}; desktop foreground/input remains user-owned" -f
                $fallout.Id,
                $hiddenWindowCount)
    }

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
    # A motion demo is only meaningful once the requested pistol is actually
    # visible.  Wait for the exact post-notice weapon-out/save proof before
    # issuing either HMD or controller pose commands, so both independent
    # streams run downstream of one clean, known stock-weapon state.
    $headsetFixtureWeaponDrawProof = $null
    $controllerPoseSweepRenderedProof = $null
    if ($HeadsetFixtureWeaponDraw -and
        ($HeadsetPoseSweep -or $ControllerPoseSweep)) {
        $manifest.headsetWorldCapture.status =
            "waiting-for-weapon-draw-before-motion-sweeps"
        if ($HeadsetControllerRigVisualTrial) {
            $manifest.headsetControllerRigVisualTrial.status =
                "waiting-for-weapon-draw"
        }
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        Write-SupervisorLog (
            "waiting for the owned fixture final save and ready weapon before bounded HMD/controller motion sweeps")
        $weaponDrawDeadline = [DateTime]::UtcNow.AddSeconds(50)
        do {
            $headsetFixtureWeaponDrawProof =
                Get-FnvxrProductHeadsetFixtureWeaponDrawProof `
                    -LogPath $headsetDemoInputTelemetryLog
            if ($headsetFixtureWeaponDrawProof) { break }
            $hostProcess.Refresh()
            if ($hostProcess.HasExited) {
                throw "OpenXR host exited before the fixture weapon-out proof was ready."
            }
            $fallout.Refresh()
            if ($fallout.HasExited) {
                throw "Fallout exited before the fixture weapon-out proof was ready."
            }
            Start-Sleep -Milliseconds 250
        } while ([DateTime]::UtcNow -lt $weaponDrawDeadline)
        if (-not $headsetFixtureWeaponDrawProof) {
            throw "The owned fixture weapon draw did not become ready before the bounded motion sweeps."
        }
        $manifest.headsetWorldCapture.weaponDrawProof =
            $headsetFixtureWeaponDrawProof
        $manifest.headsetWorldCapture.status =
            "weapon-draw-proven-running-motion-sweeps"
        if ($HeadsetControllerRigVisualTrial) {
            $manifest.headsetControllerRigVisualTrial.status =
                "weapon-draw-proven-awaiting-rig-motion-proof"
        }
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        Write-SupervisorLog (
            "owned fixture final save and ready weapon proven before the motion sweeps: weapon={0} saveFrame={1} resultFrame={2}" -f
                $headsetFixtureWeaponDrawProof.weapon,
                $headsetFixtureWeaponDrawProof.saveFrame,
                $headsetFixtureWeaponDrawProof.resultFrame)
    }
    # Start the native SBS recorder before either commanded motion stream.
    # Starting it after the controller proof produced a technically valid
    # video that omitted the controller-driven weapon movement it was meant
    # to demonstrate.
    if ($RecordSimulatorSbs) {
        $centerProofDeadline = [DateTime]::UtcNow.AddSeconds(10)
        do {
            $centerIntegratedFirstPersonProof =
                Get-FnvxrProductCenterIntegratedFirstPersonProof `
                    -RetailVrLogPath $retailVrLog
            if ($centerIntegratedFirstPersonProof) { break }
            Start-Sleep -Milliseconds 100
        } while ([DateTime]::UtcNow -lt $centerProofDeadline)
        if (-not $centerIntegratedFirstPersonProof) {
            throw "The single center-integrated stock first-person path did not publish with privateEyeCalls=0 before the SBS recorder could start. Evidence is in $runDirectory"
        }
        $manifest.headsetWorldCapture.centerIntegratedFirstPersonProof =
            $centerIntegratedFirstPersonProof
        $manifest.simulatorSbsVideo.status = "starting-native-sbs-recorder"
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        $recorder = Start-FnvxrSimulatorSbsRecorder `
            -HostProcessId ([uint32]$hostProcess.Id) `
            -OutputPath $simulatorSbsVideoPath `
            -DurationSeconds $SimulatorSbsRecordSeconds
        $simulatorSbsRecorderProcess = $recorder.process
        $manifest.simulatorSbsVideo.status = "recording-native-sbs"
        $manifest.simulatorSbsVideo.previewClientBounds = $recorder.bounds
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        Write-SupervisorLog (
            "recording native OpenXR Simulator SBS client area {0}x{1} at 30 fps for {2} seconds across controller and HMD sweeps" -f
                $recorder.bounds.width,
                $recorder.bounds.height,
                $SimulatorSbsRecordSeconds)
        # Give gdigrab a deterministic pre-roll before the first trigger edge.
        # This also makes the neutral-to-motion transition obvious on video.
        Start-Sleep -Milliseconds 1000
    }
    # Establish the controller in stage-local space first.  The following HMD
    # sweep can then prove the hand target remains fixed while the head moves;
    # this is the exact inverse of the controller-only proof below.
    if ($ControllerPoseSweep -and -not $HeadsetCombatVisualTrial) {
        $manifest.controllerPoseSweep.status = "running"
        $manifest.headsetControllerRigVisualTrial.status =
            "running-controller-only-rig-sweep"
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        Write-SupervisorLog (
            "starting bounded LOCAL right-controller six-axis sweep for {0} seconds with the HMD held fixed; final acceptance requires retail hand and stock-weapon transform evidence" -f
            $ControllerPoseSweepSeconds)
        $controllerPoseSweepOutput = @(
            & (Join-Path $PSScriptRoot "invoke-openxr-simulator-controller-sweep.ps1") `
                -DataDirectory $openXrSimulatorDataDirectory `
                -Hand right `
                -LivePipBoyFocus:$HeadsetInventoryVisualTrial `
                -DurationSeconds $ControllerPoseSweepSeconds
        ) -join [Environment]::NewLine
        $controllerPoseSweepEvidence =
            $controllerPoseSweepOutput | ConvertFrom-Json -ErrorAction Stop
        if ([string]$controllerPoseSweepEvidence.poseSpace -cne "local" -or
            -not [bool]$controllerPoseSweepEvidence.centerRestored -or
            [bool]$controllerPoseSweepEvidence.headPoseMutated -or
            @($controllerPoseSweepEvidence.commands).Count -lt 14) {
            throw "The headless runtime controller script did not retain all LOCAL six-axis commands with a static HMD and centered restoration."
        }
        if ($HeadsetInventoryVisualTrial -and
            (-not [bool]$controllerPoseSweepEvidence.livePipBoyFocusRequested -or
             [int]$controllerPoseSweepEvidence.livePipBoyFocusCommandCount -lt 4)) {
            throw "The live Pip-Boy visual trial did not retain its opposite-wrist pointing gesture."
        }

        # Accept the clear live path directly: a current headless-rig event
        # must show the right hand solved and the stock weapon write applied.
        # No motion classifier or reciprocal independence heuristic sits in
        # front of controller movement, firing, reload, or video completion.
        $directRigEvents = @(Select-String `
            -LiteralPath $headsetDemoInputTelemetryLog `
            -Pattern '"originSource":"headless-stereo-rig-body".*"apply":true.*"leftSolved":true.*"rightSolved":true.*"pipBoyTracked":true.*"weaponWriteApplied":true' `
            -ErrorAction SilentlyContinue)
        if ($directRigEvents.Count -lt 1) {
            throw "The direct controller-to-stock-weapon rig path did not apply during the commanded sweep. Evidence is in $runDirectory"
        }
        $directRigProof = [ordered]@{
            appliedEventCount = $directRigEvents.Count
            path = "OpenXR right controller -> post-animation right hand -> stock weapon"
            centerRestored = [bool]$controllerPoseSweepEvidence.centerRestored
            observedAtUtc = [DateTime]::UtcNow.ToString("o")
        }
        $manifest.controllerPoseSweep.status = "direct-rig-path-applied-centered"
        $manifest.controllerPoseSweep.evidence = $controllerPoseSweepEvidence
        $manifest.controllerPoseSweep.renderedRigProof = $directRigProof
        $manifest.headsetControllerRigVisualTrial.status =
            "direct-controller-weapon-path-applied"
        $manifest.headsetControllerRigVisualTrial.evidence = $directRigProof
        if ($HeadsetInventoryVisualTrial) {
            $livePipBoyDeadline = [DateTime]::UtcNow.AddSeconds(8)
            $livePipBoyOpenLine = $null
            $livePipBoyHostLine = $null
            do {
                $livePipBoyOpenLine = Select-String `
                    -LiteralPath $headsetDemoInputTelemetryLog `
                    -Pattern 'enginePipBoy open .*source=live-pipboy-focus' `
                    -ErrorAction SilentlyContinue | Select-Object -Last 1
                $livePipBoyHostLine = Select-String `
                    -LiteralPath $hostOut `
                    -Pattern '"livePipBoy":true.*"livePipBoyScale":1\.[1-9][0-9]*.*"uiQuadVisible":false' `
                    -ErrorAction SilentlyContinue | Select-Object -Last 1
                if ($livePipBoyOpenLine -and $livePipBoyHostLine) { break }
                Start-Sleep -Milliseconds 100
            } while ([DateTime]::UtcNow -lt $livePipBoyDeadline)
            if (-not $livePipBoyOpenLine -or -not $livePipBoyHostLine) {
                throw "The opposite-wrist pointing gesture did not prove a scaled live Pip-Boy with the old UI quad absent. Evidence is in $runDirectory"
            }
            $livePipBoyProof = [ordered]@{
                pointingGesture = "right tracked aim ray -> opposite tracked wrist"
                engineOpen = $livePipBoyOpenLine.Line
                hostSubmission = $livePipBoyHostLine.Line
                oldPipBoyQuadVisible = $false
                observedAtUtc = [DateTime]::UtcNow.ToString("o")
            }
            $manifest.headsetControllerRigVisualTrial.livePipBoyProof =
                $livePipBoyProof
            Write-SupervisorLog (
                "opposite-wrist live Pip-Boy focus proven: engine opened from tracked dwell, scale exceeded 1.0, old UI quad absent")
        }
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        Write-SupervisorLog (
            "completed direct LOCAL right-controller sweep commands={0} appliedRigEvents={1}; no classifier gate" -f
            $controllerPoseSweepEvidence.commandCount,
            $directRigEvents.Count)
    }
    if ($HeadsetCombatVisualTrial) {
        $playerStateBeforeCombat = @(Get-Content `
            -LiteralPath $headsetDemoInputTelemetryLog `
            -ErrorAction SilentlyContinue | Where-Object {
                $_ -match '"event":"fnvxrPlayerState"'
            } | Select-Object -Last 1)
        $manifest.controllerPoseSweep.status = "running-smooth-combat-path"
        $manifest.headsetPoseSweep.status = "running-gentle-concurrent-head-look"
        $manifest.headsetControllerRigVisualTrial.status =
            "running-smooth-multi-target-controller-path"
        $manifest.headsetCombatVisualTrial.status =
            "running-empty-reload-confirm"
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        Write-SupervisorLog (
            "starting clean combined harness: smooth multi-target 6DoF weapon path plus gentle independent head look; empty pistol, X reload, two confirmation shots")

        $combatOutput = @(
            & (Join-Path $PSScriptRoot `
                "invoke-openxr-simulator-combat-demo.ps1") `
                -DataDirectory $openXrSimulatorDataDirectory `
                -ShotsToEmpty 13 `
                -ShotsAfterReload 2 `
                -UpdatesPerSecond 12 `
                -ConsumeTimeoutMilliseconds 5000
        ) -join [Environment]::NewLine
        $combatEvidence =
            $combatOutput | ConvertFrom-Json -ErrorAction Stop
        $headsetPoseSweepEvidence = $combatEvidence.headMotion
        if (-not [bool]$combatEvidence.centerRestored -or
            -not [bool]$combatEvidence.controlsReleased -or
            -not [bool]$combatEvidence.locomotion.neutralRestored -or
            [int]$combatEvidence.triggerPresses -lt 15 -or
            -not [bool]$headsetPoseSweepEvidence.centerRestored -or
            [string]$headsetPoseSweepEvidence.pattern -cne
                "gentle-sinusoidal-v1") {
            throw "The combined combat/head harness did not finish and restore neutral tracked state."
        }

        $attackEvents = @(Select-String `
            -LiteralPath $headsetDemoInputTelemetryLog `
            -Pattern 'primaryAttack edge .*held=true applied=true finalConsumer=HighProcess::forceFireWeapon' `
            -ErrorAction SilentlyContinue)
        $reloadEvents = @(Select-String `
            -LiteralPath $headsetDemoInputTelemetryLog `
            -Pattern 'buttonX reloadEdge .*held=true applied=(true|false) finalConsumer=PlayerCharacter::Reload' `
            -ErrorAction SilentlyContinue)
        $loadedBefore = @($attackEvents | ForEach-Object {
            if ($_.Line -match 'loadedBefore=(\d+)') { [int]$Matches[1] }
        })
        $emptyingPattern = ($loadedBefore | Select-Object -First 13) -join ','
        $confirmationPattern = ($loadedBefore | Select-Object -Last 2) -join ','
        if ($attackEvents.Count -ne 15 -or $reloadEvents.Count -ne 2 -or
            $emptyingPattern -cne '13,12,11,10,9,8,7,6,5,4,3,2,1' -or
            $confirmationPattern -cne '13,12') {
            throw "The bounded combat trial did not prove a full 13-round magazine discharge, engine reload back to 13, and two post-reload shots from the simulator controllers. Evidence is in $runDirectory"
        }
        $directRigEvents = @(Select-String `
            -LiteralPath $headsetDemoInputTelemetryLog `
            -Pattern '"originSource":"headless-stereo-rig-body".*"apply":true.*"leftSolved":true.*"rightSolved":true.*"pipBoyTracked":true.*"weaponWriteApplied":true' `
            -ErrorAction SilentlyContinue)
        if ($directRigEvents.Count -lt 1) {
            throw "The direct controller-to-stock-weapon path did not apply during the combined harness."
        }
        $playerStateAfterCombat = @(Get-Content `
            -LiteralPath $headsetDemoInputTelemetryLog `
            -ErrorAction SilentlyContinue | Where-Object {
                $_ -match '"event":"fnvxrPlayerState"'
            } | Select-Object -Last 1)
        if ($playerStateBeforeCombat.Count -ne 1 -or
            $playerStateAfterCombat.Count -ne 1) {
            throw "The simulator locomotion proof did not capture player-state samples on both sides of the commanded left-stick interval."
        }
        $beforePlayer = $playerStateBeforeCombat[0] | ConvertFrom-Json
        $afterPlayer = $playerStateAfterCombat[0] | ConvertFrom-Json
        $dx = [double]$afterPlayer.playerPosition[0] -
            [double]$beforePlayer.playerPosition[0]
        $dy = [double]$afterPlayer.playerPosition[1] -
            [double]$beforePlayer.playerPosition[1]
        $dz = [double]$afterPlayer.playerPosition[2] -
            [double]$beforePlayer.playerPosition[2]
        $playerPositionDelta = [Math]::Sqrt(
            $dx * $dx + $dy * $dy + $dz * $dz)
        if ([double]::IsNaN($playerPositionDelta) -or
            [double]::IsInfinity($playerPositionDelta) -or
            $playerPositionDelta -lt 1.0) {
            throw ("Simulator left-stick input did not move the real Fallout player: world-position delta={0:N4} units. Evidence is in {1}" -f
                $playerPositionDelta,
                $runDirectory)
        }
        $combatProof = [ordered]@{
            shotsToEmpty = 13
            preparationReloads = 1
            reloads = 1
            shotsAfterReload = 2
            commandedShots = 15
            primaryAttackHoldEvents = $attackEvents.Count
            reloadHoldEvents = $reloadEvents.Count
            loadedRoundsBeforeAttacks = $loadedBefore
            fireBinding = "OpenXR right trigger -> retail primary attack"
            reloadBinding = "retail empty-magazine automatic reload; OpenXR left X reload edge also delivered"
            controllerMotion = $combatEvidence
            headMotion = $headsetPoseSweepEvidence
            appliedRigEvents = $directRigEvents.Count
            locomotion = [ordered]@{
                binding = "OpenXR left thumbstick -> PlayerMover::SetMovementFlags"
                beforePlayerPosition = @($beforePlayer.playerPosition)
                afterPlayerPosition = @($afterPlayer.playerPosition)
                worldPositionDeltaUnits = $playerPositionDelta
                thresholdUnits = 1.0
                neutralRestored = [bool]$combatEvidence.locomotion.neutralRestored
            }
            observedAtUtc = [DateTime]::UtcNow.ToString("o")
        }
        $manifest.controllerPoseSweep.status =
            "smooth-multi-target-direct-rig-path-complete"
        $manifest.controllerPoseSweep.evidence = $combatEvidence
        $manifest.controllerPoseSweep.renderedRigProof = [ordered]@{
            appliedEventCount = $directRigEvents.Count
            path = "OpenXR right controller -> post-animation right hand -> stock weapon"
        }
        $manifest.headsetPoseSweep.status =
            "gentle-concurrent-head-look-complete-centered"
        $manifest.headsetPoseSweep.evidence = $headsetPoseSweepEvidence
        $manifest.headsetControllerRigVisualTrial.status =
            "smooth-direct-controller-weapon-path-applied"
        $manifest.headsetControllerRigVisualTrial.evidence =
            $manifest.controllerPoseSweep.renderedRigProof
        $manifest.headsetCombatVisualTrial.status =
            "empty-reload-two-shots-proven"
        $manifest.headsetCombatVisualTrial.evidence = $combatProof
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        Write-SupervisorLog (
            "completed combined harness shots={0} attackEvents={1} reloadEvents={2} rigEvents={3} playerDeltaUnits={4:N3}" -f
                $combatProof.commandedShots,
                $combatProof.primaryAttackHoldEvents,
                $combatProof.reloadHoldEvents,
                $combatProof.appliedRigEvents,
                $combatProof.locomotion.worldPositionDeltaUnits)
    }
    if ($HeadsetPoseSweep -and -not $HeadsetCombatVisualTrial) {
        $manifest.headsetPoseSweep.status = "running"
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        Write-SupervisorLog (
            "starting bounded six-axis headless-runtime HMD pose sweep for {0} seconds; the stage-local controller remains fixed and no game input, firing, projectile, hit, camera-hook, replay, UI, or physical-headset path is enabled" -f
            $HeadsetPoseSweepSeconds)
        $headsetPoseSweepOutput = @(
            & (Join-Path $PSScriptRoot "invoke-openxr-simulator-head-sweep.ps1") `
                -DataDirectory $openXrSimulatorDataDirectory `
                -Pattern Cardinal `
                -DurationSeconds $HeadsetPoseSweepSeconds
        ) -join [Environment]::NewLine
        $headsetPoseSweepEvidence =
            $headsetPoseSweepOutput | ConvertFrom-Json -ErrorAction Stop
        if (-not [bool]$headsetPoseSweepEvidence.centerRestored -or
            [string]$headsetPoseSweepEvidence.pattern -cne "cardinal-v1" -or
            @($headsetPoseSweepEvidence.commands).Count -lt 13) {
            throw "The headless-runtime HMD cardinal script did not retain every signed axis and centered restoration."
        }
        $headsetPoseSweepRenderedProof =
            Get-FnvxrProductRetailCameraPoseSweepProof `
                -LogPath $retailVrLog
        if (-not $headsetPoseSweepRenderedProof -or
            -not [bool]$headsetPoseSweepRenderedProof.sixDofCameraResponseProven) {
            throw "The commanded headless HMD sweep did not prove yaw, pitch, roll, and translation in the actual retail NiCamera transactions."
        }
        $manifest.headsetPoseSweep.status =
            "rendered-six-dof-cardinal-proven-centered"
        $manifest.headsetPoseSweep.evidence = $headsetPoseSweepEvidence
        $manifest.headsetPoseSweep.renderedCameraProof =
            $headsetPoseSweepRenderedProof
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        Write-SupervisorLog (
            "completed cardinal six-axis HMD pose script commands={0} x=[{1:N3},{2:N3}] y=[{3:N3},{4:N3}] z=[{5:N3},{6:N3}] renderedSamples={7} yawSpan={8:N3} pitchSpan={9:N3} rollSpan={10:N3} translationResponse={11:N3}; simulator center restored" -f
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
    # A combined weapon/sweep run established this proof before the sweep.
    # Preserve it through supervision; ordinary world-only capture discovers
    # it lazily below as before.
    $controllerAuthorizationProof = $null
    $retailFirstPersonRenderProof = $null
    $physicalDisplayOutputProof = $null
    do {
        $hostProcess.Refresh()
        $fallout.Refresh()
        if ($fallout.HasExited) { $completion = "retail-exited"; break }
        if ($hostProcess.HasExited) {
            $hostExitCode = try { [int]$hostProcess.ExitCode } catch { $null }
            $manifest.processes.host.exitCode = $hostExitCode
            $completion = if ($hostExitCode -eq 0) { "host-frame-limit" } else { "host-failed" }
            Write-SupervisorLog (
                "OpenXR host exited during supervision code={0} completion={1}" -f
                    $(if ($null -eq $hostExitCode) { "unavailable" } else { $hostExitCode }),
                    $completion)
            Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
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
                            if ($HeadsetFixtureWeaponDraw) {
                                if ($headsetFixtureWeaponDrawProof) {
                                    "sustained-world-stereo-and-weapon-draw-proven"
                                } else {
                                    "sustained-world-stereo-proven-awaiting-weapon-draw-result"
                                }
                            } else {
                                "sustained-world-stereo-proven"
                            }
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
            if ($HeadsetFixtureWeaponDraw -and
                -not $headsetFixtureWeaponDrawProof) {
                $headsetFixtureWeaponDrawProof =
                    Get-FnvxrProductHeadsetFixtureWeaponDrawProof `
                        -LogPath $headsetDemoInputTelemetryLog
                if ($headsetFixtureWeaponDrawProof) {
                    $manifest.headsetWorldCapture.weaponDrawProof =
                        $headsetFixtureWeaponDrawProof
                    $manifest.headsetWorldCapture.status =
                        if ($stereoContinuityProof) {
                            "sustained-world-stereo-and-weapon-draw-proven"
                        } else {
                            "weapon-draw-proven-awaiting-sustained-world-stereo"
                        }
                    Write-FnvxrProductJsonAtomic `
                        -Value $manifest `
                        -Path $manifestPath
                    Write-SupervisorLog (
                        "owned fixture final save and JIP weapon draw proven: save={0} saveFrame={1} weapon={2} submitted={3} alreadyReady={4} issuedFrame={5} resultFrame={6}" -f
                        $headsetFixtureWeaponDrawProof.saveName,
                        $headsetFixtureWeaponDrawProof.saveFrame,
                        $headsetFixtureWeaponDrawProof.weapon,
                        $headsetFixtureWeaponDrawProof.submitted,
                        $headsetFixtureWeaponDrawProof.alreadyReady,
                        $(if ($null -eq $headsetFixtureWeaponDrawProof.issuedFrame) { "none" } else { $headsetFixtureWeaponDrawProof.issuedFrame }),
                        $headsetFixtureWeaponDrawProof.resultFrame)
                }
            }
            if ($HeadsetFixtureWeaponDraw -and
                $RetailVrFirstPersonPrivateCaller -cne "None" -and
                -not $retailFirstPersonRenderProof) {
                $retailFirstPersonRenderProof =
                    Get-FnvxrProductRetailFirstPersonRenderProof `
                        -RetailVrLogPath $retailVrLog `
                        -WeaponTelemetryLogPath $headsetDemoInputTelemetryLog `
                        -RequiredCaller $RetailVrFirstPersonPrivateCaller
                if ($retailFirstPersonRenderProof) {
                    $manifest.headsetWorldCapture.firstPersonRenderProof =
                        $retailFirstPersonRenderProof
                    Write-FnvxrProductJsonAtomic `
                        -Value $manifest `
                        -Path $manifestPath
                    Write-SupervisorLog (
                        "selected first-person caller {0} proved an engine-rendered two-eye weapon transaction={1} poseFrame={2}; stock backbuffer route remains disabled" -f
                        $RetailVrFirstPersonPrivateCaller,
                        $retailFirstPersonRenderProof.transaction,
                        $retailFirstPersonRenderProof.poseFrame)
                }
            }
            if ($HeadsetDemoFixture -and -not $pipBoyOutputProof) {
                $pipBoyOutputProof =
                    Get-FnvxrProductPipBoyOutputProof -LogPath $hostOut
                if ($pipBoyOutputProof) {
                    $manifest.readiness.headsetDemoPipBoyUi = $true
                    $manifest.headsetDemo.status = "live-pipboy-proven"
                    $manifest.headsetDemo.pipBoyOutputProof = $pipBoyOutputProof
                    Write-FnvxrProductJsonAtomic `
                        -Value $manifest `
                        -Path $manifestPath
                    Write-SupervisorLog (
                        "proven live wrist Pip-Boy frame={0} menuBits=0x{1:X2} left={2} right={3}" -f
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
                        "live-pipboy-proven-and-returned-to-gameplay"
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
        } else {
            throw "No proven binocular engine-stereo frame reached OpenXR before '$completion'. Enter a loaded gameplay world before the bounded visual trial expires; evidence is in $runDirectory"
        }
    }
    if ($PhysicalHeadsetPlay -and -not $controllerAuthorizationProof) {
        throw "The exact-retail xNVSE controller consumer never acknowledged the physical-play route before '$completion'; controller input remains unauthorized. Evidence is in $runDirectory"
    }
    if ($PhysicalHeadsetPlay -and -not $physicalDisplayOutputProof) {
        throw "No retail frame proved the requested physical-play source resolution ${PhysicalGameWidth}x${PhysicalGameHeight} before '$completion'. Evidence is in $runDirectory"
    }
    if ($HeadsetDemoFixture -and -not $pipBoyOutputProof) {
        throw "No live wrist Pip-Boy frame reached OpenXR before '$completion'. The bounded demo requires a fresh binocular retail Pip-Boy transaction with zero UI quad; evidence is in $runDirectory"
    }
    if ($HeadsetDemoFixture -and -not $headsetDemoInputProof) {
        throw "The bounded in-game Pip-Boy open/close sequence did not return to gameplay before '$completion'; no partial UI interaction is accepted as a completed demo. Evidence is in $runDirectory"
    }
    if ($HeadsetFixtureWeaponDraw -and -not $headsetFixtureWeaponDrawProof) {
        throw "The owned fixture weapon draw did not produce a ready weapon before '$completion'; no weapon video is claimed. Evidence is in $runDirectory"
    }
    if ($HeadsetFixtureWeaponDraw -and
        $RetailVrFirstPersonPrivateCaller -ne "None" -and
        -not $retailFirstPersonRenderProof) {
        throw "The selected outer RenderFirstPerson caller did not prove a published two-eye engine-rendered weapon transaction; no weapon video is claimed. Evidence is in $runDirectory"
    }
    if ($RecordSimulatorSbs -and
        (-not $centerIntegratedFirstPersonProof -or
         -not $ControllerPoseSweep -or
         -not $HeadsetPoseSweep)) {
        throw "The SBS deliverable requires the single center-integrated first-person proof with privateEyeCalls=0 plus both completed controller and HMD pose sweeps. Evidence is in $runDirectory"
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
    if ($RecordSimulatorSbs) {
        if (-not $simulatorSbsRecorderProcess) {
            throw "The native OpenXR Simulator SBS recorder never started."
        }
        $simulatorSbsVideoProof = Complete-FnvxrSimulatorSbsRecorder `
            -Process $simulatorSbsRecorderProcess `
            -OutputPath $simulatorSbsVideoPath `
            -ExpectedDurationSeconds $SimulatorSbsRecordSeconds
        $manifest.simulatorSbsVideo.status = "captured-and-verified"
        $manifest.simulatorSbsVideo.proof = $simulatorSbsVideoProof
        Write-SupervisorLog (
            "native OpenXR Simulator SBS video verified path={0} size={1}x{2} duration={3:N2}s" -f
                $simulatorSbsVideoProof.path,
                $simulatorSbsVideoProof.width,
                $simulatorSbsVideoProof.height,
                $simulatorSbsVideoProof.durationSeconds)
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
    if ("FnvxrSimulatorPreviewNative" -as [type]) {
        [FnvxrSimulatorPreviewNative]::StopWindowIsolation()
    }
    if ($simulatorSbsRecorderProcess) {
        try {
            $simulatorSbsRecorderProcess.Refresh()
            if (-not $simulatorSbsRecorderProcess.HasExited) {
                Stop-Process -Id $simulatorSbsRecorderProcess.Id -Force -ErrorAction SilentlyContinue
            }
        } catch {}
    }
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
            if ($finalizingRetailFixture) {
                # The explicitly requested world-only weapon pass is the one
                # exception to the otherwise load-only route: it saves only
                # the same owned fixture after its exact stock notices have
                # settled.  Require that owned .fos to have actually changed;
                # the paired NVSE sidecar may or may not change with the
                # retail save implementation, but it remains bounded to this
                # same owned fixture pair.
                $automationPlan.fixtureFinalizationSaveChanged =
                    -not [bool]$automationPlan.loadOnlySaveUnchanged
                $automationPlan.fixtureFinalizationNvseChanged =
                    -not [bool]$automationPlan.loadOnlyNvseUnchanged
                if (-not $automationPlan.fixtureFinalizationSaveChanged) {
                    throw "The requested owned-fixture finalization did not update its .fos save; no clean post-notice fixture was produced."
                }
            } elseif (-not $automationPlan.loadOnlySaveUnchanged -or
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
    # Persist the terminal state before analyzing it. The analyzer is a
    # read-only evidence consumer: a missing or incomplete Phase 1 proof must
    # never hide the launcher's own failure classification or alter product
    # authority.
    $manifest.phase1.status = "analyzing"
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
    try {
        $phase1Output = @(
            & (Join-Path $PSScriptRoot "verify-fnvxr-phase1-6dof.ps1") `
                -RunDirectory $runDirectory
        ) -join [Environment]::NewLine
        $phase1Report = $phase1Output | ConvertFrom-Json -ErrorAction Stop
        $manifest.phase1.status = [string]$phase1Report.status
        $manifest.phase1.report = $phase1Report
        $manifest.phase1.physicalHeadsetGateAccepted = $false
    } catch {
        $manifest.phase1.status = "analysis-failed"
        $manifest.phase1.analysisError = $_.Exception.Message
        $manifest.phase1.physicalHeadsetGateAccepted = $false
        Write-SupervisorLog (
            "Phase 1 evidence analysis failed without changing launch authority: " +
            $_.Exception.Message)
    }
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
    $completionIdentity = Get-FnvxrProductFileIdentity -Path $manifestPath
    ("{0}  {1}" -f $completionIdentity.sha256, [System.IO.Path]::GetFileName($manifestPath)) |
        Set-Content -LiteralPath $completionHashPath -Encoding ASCII
}

if ($manifest.error) { throw $manifest.error }
$manifest | ConvertTo-Json -Depth 10
