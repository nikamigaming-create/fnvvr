param(
    [Parameter(Mandatory = $true)][string]$SourceRoot
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

. (Join-Path $SourceRoot "scripts\fnvxr-product-common.ps1")

function Require-Throws {
    param(
        [Parameter(Mandatory = $true)][scriptblock]$Action,
        [Parameter(Mandatory = $true)][string]$Fragment
    )
    try { & $Action | Out-Null }
    catch {
        if (-not $_.Exception.Message.Contains($Fragment)) {
            throw "Expected failure containing '$Fragment', got '$($_.Exception.Message)'."
        }
        return
    }
    throw "Expected failure containing '$Fragment'."
}

$launcherPath = Join-Path $SourceRoot "scripts\start-fnvxr-product.ps1"
$fixtureLauncherPath = Join-Path $SourceRoot "scripts\start-fnvxr-retail-fixture.ps1"
$buildPath = Join-Path $SourceRoot "scripts\build-fnvxr-product.ps1"
$commonPath = Join-Path $SourceRoot "scripts\fnvxr-product-common.ps1"
$hostSourcePath = Join-Path $SourceRoot "host\fnvxr_openxr_pose_host.cpp"
$pluginSourcePath = Join-Path $SourceRoot "plugin\fnvxr_nvse_plugin.cpp"
$d3d9SourcePath = Join-Path $SourceRoot "renderhook\fnvxr_d3d9_proxy.cpp"
$launcher = Get-Content -LiteralPath $launcherPath -Raw
$fixtureLauncher = Get-Content -LiteralPath $fixtureLauncherPath -Raw
$builder = Get-Content -LiteralPath $buildPath -Raw
$common = Get-Content -LiteralPath $commonPath -Raw
$hostSource = Get-Content -LiteralPath $hostSourcePath -Raw
$pluginSource = Get-Content -LiteralPath $pluginSourcePath -Raw
$d3d9Source = Get-Content -LiteralPath $d3d9SourcePath -Raw

$documentsPath = Get-FnvxrProductDocumentsPath
if ([string]::IsNullOrWhiteSpace($documentsPath) -or
    -not [System.IO.Path]::IsPathRooted($documentsPath)) {
    throw "Product Documents known-folder resolver did not return a rooted path."
}
$userShellFolders = $null
try {
    $userShellFolders = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey(
        "Software\Microsoft\Windows\CurrentVersion\Explorer\User Shell Folders",
        $false)
    if ($userShellFolders) {
        $rawDocumentsPath = [string]$userShellFolders.GetValue(
            "Personal",
            $null,
            [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
        if (-not [string]::IsNullOrWhiteSpace($rawDocumentsPath)) {
            $expectedDocumentsPath = [System.IO.Path]::GetFullPath(
                [Environment]::ExpandEnvironmentVariables($rawDocumentsPath))
            if (-not [string]::Equals(
                    $documentsPath,
                    $expectedDocumentsPath,
                    [System.StringComparison]::OrdinalIgnoreCase)) {
                throw "Product Documents resolver ignored the current user's known-folder redirection."
            }
        }
    }
} finally {
    if ($userShellFolders) { $userShellFolders.Dispose() }
}
if (-not $launcher.Contains(
        '$saveRoot = Join-Path (Get-FnvxrProductDocumentsPath) "My Games\FalloutNV\Saves"')) {
    throw "Product launcher must resolve the fixed save through the redirected Documents known folder."
}
if ((Get-FnvxrProductApprovedRetailSaveLoadCommand `
        -RetailSaveName "FNVXR_StereoTest") -cne
        "load FNVXR_StereoTest") {
    throw "Verified Goodsprings save did not map to its fixed load-only command."
}
Require-Throws {
    Get-FnvxrProductApprovedRetailSaveLoadCommand `
        -RetailSaveName "FNVXR_HostExitRecovery"
} "Cannot validate argument"

$expectedRetailVisualTrialPlugins = @(
    "FalloutNV.esm",
    "DeadMoney.esm",
    "HonestHearts.esm",
    "OldWorldBlues.esm",
    "LonesomeRoad.esm",
    "TribalPack.esm",
    "MercenaryPack.esm",
    "ClassicPack.esm",
    "CaravanPack.esm",
    "GunRunnersArsenal.esm")
$actualRetailVisualTrialPlugins = @(Get-FnvxrProductRetailVisualTrialPluginNames)
if (($actualRetailVisualTrialPlugins -join "|") -cne
    ($expectedRetailVisualTrialPlugins -join "|")) {
    throw "Retail visual-trial profile no longer matches the approved save's exact official-master order."
}
foreach ($retailProfileContract in @(
    'function Get-FnvxrProductRetailVisualTrialPluginNames',
    'function Get-FnvxrProductRetailVisualTrialPluginsPath',
    'function Assert-FnvxrProductRetailVisualTrialPluginData',
    'function Install-FnvxrProductRetailVisualTrialPluginProfile',
    'function Restore-FnvxrProductRetailVisualTrialPluginProfile',
    'Retail plugin-profile restore hash mismatch')) {
    if (-not $common.Contains($retailProfileContract)) {
        throw "Product common helpers lost the temporary retail plugin-profile contract: $retailProfileContract"
    }
}
if (-not $launcher.Contains('[switch]$UseRetailPluginProfile') -or
    -not $launcher.Contains('temporary exact-official retail plugins.txt only') -or
    -not $launcher.Contains('staging-temporary-retail-plugin-profile')) {
    throw "Product launcher lost the explicit temporary retail plugin-profile opt-in."
}
if (-not $launcher.Contains('[ValidateSet("Disabled", "Create", "Load", "Ensure")]') -or
    -not $launcher.Contains('RetailFixtureAction is mutually exclusive') -or
    -not $launcher.Contains('Use -RetailFixtureAction instead.') -or
    -not $launcher.Contains('start-fnvxr-retail-fixture.ps1') -or
    -not $launcher.Contains('FalloutNV.esm-only fixture plugins.txt profile')) {
    throw "Product launcher lost the owned retail fixture route or its retired unsafe profile guard."
}
if (-not $launcher.Contains('$fixtureArguments = @{') -or
    $launcher.Contains('$fixtureArguments = @(') -or
    -not $launcher.Contains('ReadyTimeoutSeconds = $RetailReadyTimeoutSeconds') -or
    -not $launcher.Contains('$fixtureArguments["TtwCore"] = $true') -or
    -not $launcher.Contains('$fixtureArguments["UseAttestedBuild"] = $true')) {
    throw "Product launcher must forward the dedicated fixture runner through a named-parameter hashtable splat."
}
foreach ($fixtureContract in @(
    'schema = if ($TtwCore) { "fnvxr-ttw-fixture-v1" } else { "fnvxr-retail-fixture-v1" }',
    'Get-FnvxrProductRetailFixtureStagePlan',
    'Install-FnvxrProductRetailFixturePluginProfile',
    'Restore-FnvxrProductRetailFixturePluginProfile',
    'Wait-FnvxrProductRetailFixtureStartMenu',
    'Wait-FnvxrProductRetailFixtureGameplay',
    'Wait-FnvxrProductRetailFixtureSavePair',
    'sourceSha256 = $attestation.source.sha256',
    'artifactSha256 = $attestation.artifacts.sha256',
    'testCount = $attestation.tests.count',
    'retail-fixture-v1',
    'noOpenXrOrSimulator = $true',
    'A pre-existing runtime is present; refusing to attach to, control, or stop it.')) {
    if (-not $fixtureLauncher.Contains($fixtureContract)) {
        throw "Dedicated retail fixture runner lost required contract: $fixtureContract"
    }
}
foreach ($ttwFixtureContract in @(
    '[switch]$TtwCore',
    'FNVXR_AutoTTW fixture lineage',
    'TTW headset proof GameRoot must be an isolated workspace sandbox below:',
    'fnvxr-ttw-retail-sandbox-v1',
    'Get-FnvxrProductTtwFixtureSaveName',
    'Install-FnvxrProductTtwBaselinePluginProfile',
    'Restore-FnvxrProductTtwBaselinePluginProfile',
    '-TtwFixture:$TtwCore')) {
    if (-not ($launcher.Contains($ttwFixtureContract) -or
            $fixtureLauncher.Contains($ttwFixtureContract) -or
            $common.Contains($ttwFixtureContract))) {
        throw "TTW fixture/headset contract is missing: $ttwFixtureContract"
    }
}
foreach ($forbiddenFixtureLauncherText in @(
    'fnvxr_openxr_pose_host.exe',
    'HeadlessSimulatorManifest',
    'd3d9.dll',
    'dinput8.dll',
    'xinput1_3.dll',
    'SendInput',
    'SendKeys',
    'keybd_event',
    'mouse_event',
    'SetForegroundWindow',
    'SetCursorPos')) {
    if ($fixtureLauncher.Contains($forbiddenFixtureLauncherText)) {
        throw "Dedicated retail fixture runner must not contain an OpenXR, proxy, or OS-input path: $forbiddenFixtureLauncherText"
    }
}

$fixtureGameplayFunction = [regex]::Match(
    $common,
    '(?s)function Wait-FnvxrProductRetailFixtureGameplay \{.*?(?=function Wait-FnvxrProductRetailFixtureSavePair)').Value
if ([string]::IsNullOrWhiteSpace($fixtureGameplayFunction)) {
    throw "Could not isolate the dedicated retail-fixture gameplay verifier."
}
foreach ($requiredFixtureGameplayText in @(
    '"--require-runtime"',
    '"--require-advancing"',
    '[uint32]$snapshot.runtime.phase -eq 3',
    '([uint32]$snapshot.runtime.menuBits -band 0xFE) -eq 0',
    '-not [bool]$snapshot.runtime.uiInputAllowed',
    '[bool]$snapshot.runtime.cameraActive',
    '-not [bool]$snapshot.runtime.showroomActive')) {
    if (-not $fixtureGameplayFunction.Contains($requiredFixtureGameplayText)) {
        throw "Retail-fixture gameplay verifier lost runtime requirement: $requiredFixtureGameplayText"
    }
}
foreach ($forbiddenFixtureGameplayText in @(
    '"--require-player"',
    'snapshot.player.')) {
    if ($fixtureGameplayFunction.Contains($forbiddenFixtureGameplayText)) {
        throw "Retail-fixture gameplay verifier must not require an intentionally absent player bridge: $forbiddenFixtureGameplayText"
    }
}
foreach ($forbiddenFixtureEvidenceText in @(
    'playerCellFormId =',
    'playerAddress =')) {
    if ($fixtureLauncher.Contains($forbiddenFixtureEvidenceText)) {
        throw "Retail-fixture manifest must not report unavailable player-bridge data: $forbiddenFixtureEvidenceText"
    }
}
foreach ($requiredFixtureEvidenceText in @(
    'cameraActive = [bool]$gameplay.json.runtime.cameraActive',
    'showroomActive = [bool]$gameplay.json.runtime.showroomActive')) {
    if (-not $fixtureLauncher.Contains($requiredFixtureEvidenceText)) {
        throw "Retail-fixture manifest lost runtime gameplay evidence: $requiredFixtureEvidenceText"
    }
}

$expectedRetailFixtureTraits = @(
    "None",
    "BuiltToDestroy",
    "FastShot",
    "FourEyes",
    "GoodNatured",
    "HeavyHanded",
    "Kamikaze",
    "SmallFrame",
    "TriggerDiscipline",
    "WildWasteland")
if ((@(Get-FnvxrProductRetailFixtureTraitNames) -join "|") -cne
    ($expectedRetailFixtureTraits -join "|")) {
    throw "Retail fixture trait list no longer matches the supported base-retail trait set."
}
$expectedRetailFixtureWeapons = @(
    "None",
    "Pistol",
    "RifleSingleHand",
    "RifleTwoHand",
    "Minigun",
    "FragGrenade",
    "Knife",
    "ThrowingKnife")
if ((@(Get-FnvxrProductRetailFixtureWeaponNames) -join "|") -cne
    ($expectedRetailFixtureWeapons -join "|")) {
    throw "Retail fixture weapon list no longer matches the fixed stock visibility set."
}
if ((Resolve-FnvxrProductRetailFixtureWeapon -Weapon "pistol") -cne "Pistol") {
    throw "Retail fixture weapon resolver did not canonicalize the selected stock weapon."
}
Require-Throws -Fragment "Unsupported retail fixture weapon" -Action {
    Resolve-FnvxrProductRetailFixtureWeapon -Weapon "NotARetailWeapon"
}
$fixtureTraits = Resolve-FnvxrProductRetailFixtureTraits `
    -TraitOne "goodnatured" `
    -TraitTwo "builtToDestroy"
if ($fixtureTraits.first -cne "GoodNatured" -or
    $fixtureTraits.second -cne "BuiltToDestroy") {
    throw "Retail fixture trait resolver did not canonicalize the selected traits."
}
Require-Throws -Fragment "distinct" -Action {
    Resolve-FnvxrProductRetailFixtureTraits `
        -TraitOne "FastShot" `
        -TraitTwo "FastShot"
}
Require-Throws -Fragment "Unsupported retail fixture trait" -Action {
    Resolve-FnvxrProductRetailFixtureTrait -Trait "NotARetailTrait"
}
if ((Get-FnvxrProductRetailFixtureSaveName -TraitOne "None" -TraitTwo "None") -cne
    "FNVXR_AutoRetail_L1_Base") {
    throw "Base retail fixture name is not deterministic."
}
if ((Get-FnvxrProductRetailFixtureSaveName `
        -TraitOne "GoodNatured" `
        -TraitTwo "BuiltToDestroy") -cne
    "FNVXR_AutoRetail_L1_BuiltToDestroy_GoodNatured") {
    throw "Two-trait retail fixture name is not canonical and deterministic."
}
if ((Get-FnvxrProductRetailFixtureSaveName `
        -TraitOne "None" `
        -TraitTwo "None" `
        -Weapon "Pistol") -cne "FNVXR_AutoRetail_L1_Pistol") {
    throw "Weapon fixture name is not isolated and deterministic."
}
if ((Assert-FnvxrProductRetailFixtureSaveName `
        -SaveName "FNVXR_AutoRetail_L1_BuiltToDestroy_GoodNatured") -cne
    "FNVXR_AutoRetail_L1_BuiltToDestroy_GoodNatured") {
    throw "Owned retail fixture save-name gate rejected its canonical name."
}
Require-Throws -Fragment "not owned" -Action {
    Assert-FnvxrProductRetailFixtureSaveName -SaveName "FNVXR_StereoTest"
}
if ((@(Get-FnvxrProductRetailFixturePluginNames) -join "|") -cne "FalloutNV.esm") {
    throw "Retail fixture profile must activate only FalloutNV.esm."
}

$defaultEnvironment = Get-FnvxrProductMinimalEnvironment `
    -RunId "default-contract" `
    -RunDirectory "C:\fnvxr-default-contract" `
    -OpenXrLoaderPath "C:\fnvxr-default-contract\openxr_loader.dll" `
    -SessionReadyTimeoutSeconds 60
if ([string]$defaultEnvironment.FNVXR_RUN_PROFILE -cne "stereo-visual-trial-v5" -or
    [string]$defaultEnvironment.FNVXR_ENABLE_ENGINE_CENTER_STEREO -cne "1" -or
    [string]$defaultEnvironment.FNVXR_OPENXR_LOADER_HINT -cne
        "C:\fnvxr-default-contract\openxr_loader.dll") {
    throw "Default visual-trial environment lost its explicit normal VR profile."
}
foreach ($defaultForbiddenKey in @(
    "FNVXR_STEREO_VISUAL_TRIAL_AUTOMATE_RECOVERY_LOAD",
    "FNVXR_STEREO_VISUAL_TRIAL_AUTOMATE_RECOVERY_SAVE_NAME",
    "FNVXR_STEREO_VISUAL_TRIAL_AUTOMATE_FRESH_CHARACTER",
    "FNVXR_STEREO_VISUAL_TRIAL_ACK_TRIBAL_PACK_POPUP",
    "FNVXR_RETAIL_FIXTURE_AUTOMATION",
    "FNVXR_RETAIL_FIXTURE_ACTION",
    "FNVXR_RETAIL_FIXTURE_SAVE_NAME",
    "FNVXR_RETAIL_FIXTURE_TRAIT_ONE",
    "FNVXR_RETAIL_FIXTURE_TRAIT_TWO",
    "FNVXR_RETAIL_FIXTURE_WEAPON",
    "FNVXR_HEADSET_FINAL_STOCK_FRAME_CAPTURE",
    "FNVXR_RETAIL_FIXTURE_ACK_OFFICIAL_PACK_POPUP",
    "FNVXR_RETAIL_FIXTURE_ACK_TTW_STEWIE_DEPENDENCY_WARNING",
    "FNVXR_HEADSET_DEMO_FIXTURE",
    "FNVXR_HEADSET_DEMO_GAMEPLAY_WARMUP_FRAMES",
    "FNVXR_HEADSET_DEMO_PIPBOY_HOLD_FRAMES",
    "FNVXR_RETAIL_VR_ACCUMULATION_DIAGNOSTIC_MODE",
    "FNVXR_HMD_MIRROR_CAPTURE_DIR",
    "FNVXR_HMD_MIRROR_CAPTURE_EVERY_N_FRAMES",
    "FNVXR_HMD_MIRROR_CAPTURE_MAX_PAIRS",
    "XR_RUNTIME_JSON",
    "XR_API_LAYER_PATH",
    "XR_ENABLE_API_LAYERS",
    "OPENXR_SIMULATOR_HEADLESS",
    "OPENXR_SIMULATOR_DATA_DIR",
    "OPENXR_SIMULATOR_LOG_PATH")) {
    if ($defaultEnvironment.Keys -ccontains $defaultForbiddenKey) {
        throw "Default product environment unexpectedly enables CLI automation/runtime override: $defaultForbiddenKey"
    }
}
$optInEnvironment = Get-FnvxrProductMinimalEnvironment `
    -RunId "opt-in-contract" `
    -RunDirectory "C:\fnvxr-opt-in-contract" `
    -OpenXrLoaderPath "C:\fnvxr-opt-in-contract\openxr_loader.dll" `
    -SessionReadyTimeoutSeconds 60 `
    -AutomateRecoveryLoad `
    -AutomateRecoverySaveName "FNVXR_StereoTest" `
    -HeadlessRuntimeManifest "C:\fnvxr-opt-in-contract\openxr_simulator.json"
$expectedOptInEnvironment = [ordered]@{
    FNVXR_STEREO_VISUAL_TRIAL_AUTOMATE_RECOVERY_LOAD = "1"
    FNVXR_STEREO_VISUAL_TRIAL_AUTOMATE_RECOVERY_SAVE_NAME = "FNVXR_StereoTest"
    XR_RUNTIME_JSON = "C:\fnvxr-opt-in-contract\openxr_simulator.json"
    OPENXR_SIMULATOR_HEADLESS = "1"
    OPENXR_SIMULATOR_DATA_DIR = "C:\fnvxr-opt-in-contract\openxr-simulator"
    OPENXR_SIMULATOR_LOG_PATH = "C:\fnvxr-opt-in-contract\openxr-simulator.log"
}
foreach ($key in $expectedOptInEnvironment.Keys) {
    if (-not ($optInEnvironment.Keys -ccontains $key) -or
        [string]$optInEnvironment[$key] -cne [string]$expectedOptInEnvironment[$key]) {
        throw "Opt-in product environment lost exact CLI automation/runtime value: $key"
    }
}
foreach ($forbiddenAuthorityKey in @(
    "FNVXR_DESKTOP_ASSIST_ENABLE",
    "FNVXR_ENABLE_CONTROLLER_BRIDGE",
    "FNVXR_ENABLE_TRACKED_WEAPON")) {
    if ($optInEnvironment.Keys -ccontains $forbiddenAuthorityKey) {
        throw "Fixed recovery automation broadened authority through: $forbiddenAuthorityKey"
    }
}

$operatorEnvironment = Get-FnvxrProductMinimalEnvironment `
    -RunId "operator-contract" `
    -RunDirectory "C:\fnvxr-operator-contract" `
    -OpenXrLoaderPath "C:\fnvxr-operator-contract\openxr_loader.dll" `
    -SessionReadyTimeoutSeconds 60 `
    -HeadlessRuntimeManifest "C:\fnvxr-operator-contract\openxr_simulator.json" `
    -MetaXrOperatorLayerDirectory "C:\fnvxr-operator-contract\meta-xr-operator\windows"
if ([string]$operatorEnvironment.XR_API_LAYER_PATH -cne
        "C:\fnvxr-operator-contract\meta-xr-operator\windows" -or
    [string]$operatorEnvironment.XR_ENABLE_API_LAYERS -cne
        "XR_APILAYER_METAX_operator") {
    throw "Meta XR Operator observation opt-in lost its exact process-local API-layer environment."
}
foreach ($forbiddenOperatorAuthorityKey in @(
    "FNVXR_DESKTOP_ASSIST_ENABLE",
    "FNVXR_ENABLE_CONTROLLER_BRIDGE",
    "FNVXR_ENABLE_TRACKED_WEAPON")) {
    if ($operatorEnvironment.Keys -ccontains $forbiddenOperatorAuthorityKey) {
        throw "Meta XR Operator observation opt-in broadened authority through: $forbiddenOperatorAuthorityKey"
    }
}

$acknowledgedEnvironment = Get-FnvxrProductMinimalEnvironment `
    -RunId "acknowledgement-contract" `
    -RunDirectory "C:\fnvxr-acknowledgement-contract" `
    -OpenXrLoaderPath "C:\fnvxr-acknowledgement-contract\openxr_loader.dll" `
    -SessionReadyTimeoutSeconds 60 `
    -AutomateRecoveryLoad `
    -AcknowledgeTribalPackPopup
if ([string]$acknowledgedEnvironment[
        "FNVXR_STEREO_VISUAL_TRIAL_ACK_TRIBAL_PACK_POPUP"] -cne "1") {
    throw "Exact official-pack acknowledgement opt-in did not expose its narrow environment gate."
}
foreach ($forbiddenAcknowledgementKey in @(
    "FNVXR_STEREO_VISUAL_TRIAL_AUTOMATE_FRESH_CHARACTER",
    "FNVXR_DESKTOP_ASSIST_ENABLE",
    "FNVXR_ENABLE_CONTROLLER_BRIDGE",
    "FNVXR_ENABLE_TRACKED_WEAPON")) {
    if ($acknowledgedEnvironment.Keys -ccontains $forbiddenAcknowledgementKey) {
        throw "Exact official-pack acknowledgement broadened authority through: $forbiddenAcknowledgementKey"
    }
}
Require-Throws {
    Get-FnvxrProductMinimalEnvironment `
        -RunId "invalid-acknowledgement-contract" `
        -RunDirectory "C:\fnvxr-invalid-acknowledgement-contract" `
        -OpenXrLoaderPath "C:\fnvxr-invalid-acknowledgement-contract\openxr_loader.dll" `
        -SessionReadyTimeoutSeconds 60 `
        -AcknowledgeTribalPackPopup
} "requires recovery-load automation"

$freshCharacterEnvironment = Get-FnvxrProductMinimalEnvironment `
    -RunId "fresh-character-contract" `
    -RunDirectory "C:\fnvxr-fresh-character-contract" `
    -OpenXrLoaderPath "C:\fnvxr-fresh-character-contract\openxr_loader.dll" `
    -SessionReadyTimeoutSeconds 60 `
    -AutomateFreshCharacter
if ([string]$freshCharacterEnvironment[
        "FNVXR_STEREO_VISUAL_TRIAL_AUTOMATE_FRESH_CHARACTER"] -cne "1") {
    throw "Fresh-character opt-in did not expose its fixed xNVSE workflow gate."
}
foreach ($forbiddenFreshCharacterKey in @(
    "FNVXR_STEREO_VISUAL_TRIAL_AUTOMATE_RECOVERY_LOAD",
    "FNVXR_STEREO_VISUAL_TRIAL_AUTOMATE_RECOVERY_SAVE_NAME",
    "FNVXR_ENABLE_CONTROLLER_BRIDGE",
    "FNVXR_ENABLE_TRACKED_WEAPON")) {
    if ($freshCharacterEnvironment.Keys -ccontains $forbiddenFreshCharacterKey) {
        throw "Fresh-character automation broadened authority through: $forbiddenFreshCharacterKey"
    }
}
Require-Throws {
    Get-FnvxrProductMinimalEnvironment `
        -RunId "ambiguous-automation-contract" `
        -RunDirectory "C:\fnvxr-ambiguous-automation-contract" `
        -OpenXrLoaderPath "C:\fnvxr-ambiguous-automation-contract\openxr_loader.dll" `
        -SessionReadyTimeoutSeconds 60 `
        -AutomateRecoveryLoad `
        -AutomateFreshCharacter
} "mutually exclusive"

$fixtureEnvironment = Get-FnvxrProductMinimalEnvironment `
    -RunId "retail-fixture-contract" `
    -RunDirectory "C:\fnvxr-retail-fixture-contract" `
    -OpenXrLoaderPath "" `
    -SessionReadyTimeoutSeconds 60 `
    -AutomateRetailFixture `
    -RetailFixtureAction "create" `
    -RetailFixtureSaveName "FNVXR_AutoRetail_L1_BuiltToDestroy_GoodNatured" `
    -RetailFixtureTraitOne "BuiltToDestroy" `
    -RetailFixtureTraitTwo "GoodNatured" `
    -RetailFixtureWeapon "None"
$expectedFixtureEnvironment = [ordered]@{
    FNVXR_RETAIL_FIXTURE_AUTOMATION = "1"
    FNVXR_RETAIL_FIXTURE_ACTION = "create"
    FNVXR_RETAIL_FIXTURE_SAVE_NAME = "FNVXR_AutoRetail_L1_BuiltToDestroy_GoodNatured"
    FNVXR_RETAIL_FIXTURE_TRAIT_ONE = "BuiltToDestroy"
    FNVXR_RETAIL_FIXTURE_TRAIT_TWO = "GoodNatured"
    FNVXR_RETAIL_FIXTURE_WEAPON = "None"
    FNVXR_RETAIL_FIXTURE_ACK_OFFICIAL_PACK_POPUP = "1"
}
foreach ($key in $expectedFixtureEnvironment.Keys) {
    if (-not ($fixtureEnvironment.Keys -ccontains $key) -or
        [string]$fixtureEnvironment[$key] -cne [string]$expectedFixtureEnvironment[$key]) {
        throw "Retail fixture environment lost exact isolated value: $key"
    }
}
if ([string]$fixtureEnvironment.FNVXR_RUN_PROFILE -cne "retail-fixture-v1" -or
    [string]$fixtureEnvironment.FNVXR_HOST_MODE -cne "retail-fixture" -or
    [string]$fixtureEnvironment.FNVXR_ENABLE_ENGINE_CENTER_STEREO -cne "0") {
    throw "Retail fixture environment must select the dedicated non-OpenXR profile."
}
$pistolFixtureEnvironment = Get-FnvxrProductMinimalEnvironment `
    -RunId "retail-pistol-fixture-contract" `
    -RunDirectory "C:\fnvxr-retail-pistol-fixture-contract" `
    -OpenXrLoaderPath "" `
    -SessionReadyTimeoutSeconds 60 `
    -AutomateRetailFixture `
    -RetailFixtureAction "create" `
    -RetailFixtureSaveName "FNVXR_AutoRetail_L1_Pistol" `
    -RetailFixtureWeapon "Pistol"
if ([string]$pistolFixtureEnvironment.FNVXR_RETAIL_FIXTURE_WEAPON -cne "Pistol" -or
    [string]$pistolFixtureEnvironment.FNVXR_RUN_PROFILE -cne "retail-fixture-v1") {
    throw "Pistol fixture did not retain the bounded owned loadout selector."
}
$ttwFixtureEnvironment = Get-FnvxrProductMinimalEnvironment `
    -RunId "ttw-fixture-contract" `
    -RunDirectory "C:\fnvxr-ttw-fixture-contract" `
    -OpenXrLoaderPath "" `
    -SessionReadyTimeoutSeconds 60 `
    -AutomateRetailFixture `
    -TtwFixture `
    -RetailFixtureAction "load" `
    -RetailFixtureSaveName "FNVXR_AutoTTW_L1_FastShot_WildWasteland" `
    -RetailFixtureTraitOne "FastShot" `
    -RetailFixtureTraitTwo "WildWasteland"
if ([string]$ttwFixtureEnvironment.FNVXR_RUN_PROFILE -cne "ttw-fixture-v1" -or
    [string]$ttwFixtureEnvironment.FNVXR_HOST_MODE -cne "ttw-fixture" -or
    [string]$ttwFixtureEnvironment.FNVXR_ENABLE_ENGINE_CENTER_STEREO -cne "0" -or
    [string]$ttwFixtureEnvironment.FNVXR_RETAIL_FIXTURE_SAVE_NAME -cne
        "FNVXR_AutoTTW_L1_FastShot_WildWasteland" -or
    [string]$ttwFixtureEnvironment.FNVXR_RETAIL_FIXTURE_ACK_OFFICIAL_PACK_POPUP -cne
        "1" -or
    [string]$ttwFixtureEnvironment.FNVXR_RETAIL_FIXTURE_ACK_TTW_STEWIE_DEPENDENCY_WARNING -cne
        "1") {
    throw "TTW fixture environment must select the owned non-OpenXR TTW profile."
}
if ($fixtureEnvironment.Keys -ccontains
        "FNVXR_RETAIL_FIXTURE_ACK_TTW_STEWIE_DEPENDENCY_WARNING") {
    throw "Base-retail fixture must not receive TTW dependency-warning authority."
}
foreach ($forbiddenTtwFixtureKey in @(
    "FNVXR_OPENXR_LOADER_HINT",
    "XR_RUNTIME_JSON",
    "OPENXR_SIMULATOR_HEADLESS")) {
    if ($ttwFixtureEnvironment.Keys -ccontains $forbiddenTtwFixtureKey) {
        throw "TTW fixture environment broadened authority through: $forbiddenTtwFixtureKey"
    }
}
if ((Get-FnvxrProductTtwFixtureSaveName `
        -TraitOne "FastShot" `
        -TraitTwo "WildWasteland") -cne
    "FNVXR_AutoTTW_L1_FastShot_WildWasteland") {
    throw "TTW fixture save helper did not produce the separate owned lineage."
}
Require-Throws -Fragment "not owned" -Action {
    Assert-FnvxrProductTtwFixtureSaveName -SaveName "FNVXR_StereoTest"
}
foreach ($forbiddenFixtureAuthorityKey in @(
    "FNVXR_STEREO_VISUAL_TRIAL_AUTOMATE_RECOVERY_LOAD",
    "FNVXR_STEREO_VISUAL_TRIAL_AUTOMATE_FRESH_CHARACTER",
    "FNVXR_OPENXR_LOADER_HINT",
    "XR_RUNTIME_JSON",
    "OPENXR_SIMULATOR_HEADLESS",
    "FNVXR_DESKTOP_ASSIST_ENABLE",
    "FNVXR_ENABLE_CONTROLLER_BRIDGE",
    "FNVXR_ENABLE_TRACKED_WEAPON")) {
    if ($fixtureEnvironment.Keys -ccontains $forbiddenFixtureAuthorityKey) {
        throw "Retail fixture automation broadened authority through: $forbiddenFixtureAuthorityKey"
    }
}
Require-Throws -Fragment "mutually exclusive" -Action {
    Get-FnvxrProductMinimalEnvironment `
        -RunId "ambiguous-retail-fixture-contract" `
        -RunDirectory "C:\fnvxr-ambiguous-retail-fixture-contract" `
        -OpenXrLoaderPath "C:\fnvxr-ambiguous-retail-fixture-contract\openxr_loader.dll" `
        -SessionReadyTimeoutSeconds 60 `
        -AutomateFreshCharacter `
        -AutomateRetailFixture `
        -RetailFixtureAction "create" `
        -RetailFixtureSaveName "FNVXR_AutoRetail_L1_Base"
}

Require-Throws -Fragment "requires the owned retail-fixture automation" -Action {
    Get-FnvxrProductMinimalEnvironment `
        -RunId "invalid-headset-demo-contract" `
        -RunDirectory "C:\fnvxr-invalid-headset-demo-contract" `
        -OpenXrLoaderPath "C:\fnvxr-invalid-headset-demo-contract\openxr_loader.dll" `
        -SessionReadyTimeoutSeconds 60 `
        -HeadsetDemoFixture
}

$headsetDemoEnvironment = Get-FnvxrProductMinimalEnvironment `
    -RunId "headset-demo-contract" `
    -RunDirectory "C:\fnvxr-headset-demo-contract" `
    -OpenXrLoaderPath "C:\fnvxr-headset-demo-contract\openxr_loader.dll" `
    -SessionReadyTimeoutSeconds 60 `
    -AutomateRetailFixture `
    -HeadsetDemoFixture `
    -RetailFixtureAction "load" `
    -RetailFixtureSaveName "FNVXR_AutoRetail_L1_FastShot_WildWasteland" `
    -RetailFixtureTraitOne "FastShot" `
    -RetailFixtureTraitTwo "WildWasteland" `
    -HeadlessRuntimeManifest "C:\fnvxr-headset-demo-contract\openxr_simulator.json" `
    -HeadsetMirrorCaptureDirectory "C:\fnvxr-headset-demo-contract\headset-mirror" `
    -HeadsetMirrorCaptureEveryFrames 6 `
    -HeadsetMirrorCaptureMaxPairs 300
if ([string]$headsetDemoEnvironment.FNVXR_RUN_PROFILE -cne "stereo-visual-trial-v5" -or
    [string]$headsetDemoEnvironment.FNVXR_HOST_MODE -cne "vr" -or
    [string]$headsetDemoEnvironment.FNVXR_ENABLE_ENGINE_CENTER_STEREO -cne "1" -or
    [string]$headsetDemoEnvironment.FNVXR_OPENXR_LOADER_HINT -cne
        "C:\fnvxr-headset-demo-contract\openxr_loader.dll") {
    throw "Headset demo environment must retain the normal visual-trial OpenXR route."
}
$expectedHeadsetDemoEnvironment = [ordered]@{
    FNVXR_RETAIL_FIXTURE_AUTOMATION = "1"
    FNVXR_RETAIL_FIXTURE_ACTION = "load"
    FNVXR_RETAIL_FIXTURE_SAVE_NAME = "FNVXR_AutoRetail_L1_FastShot_WildWasteland"
    FNVXR_RETAIL_FIXTURE_TRAIT_ONE = "FastShot"
    FNVXR_RETAIL_FIXTURE_TRAIT_TWO = "WildWasteland"
    FNVXR_HEADSET_DEMO_FIXTURE = "1"
    FNVXR_HEADSET_DEMO_GAMEPLAY_WARMUP_FRAMES = "90"
    FNVXR_HEADSET_DEMO_PIPBOY_HOLD_FRAMES = "240"
    FNVXR_HMD_MIRROR_CAPTURE_DIR = "C:\fnvxr-headset-demo-contract\headset-mirror"
    FNVXR_HMD_MIRROR_CAPTURE_EVERY_N_FRAMES = "6"
    FNVXR_HMD_MIRROR_CAPTURE_MAX_PAIRS = "300"
    XR_RUNTIME_JSON = "C:\fnvxr-headset-demo-contract\openxr_simulator.json"
    OPENXR_SIMULATOR_HEADLESS = "1"
}
foreach ($key in $expectedHeadsetDemoEnvironment.Keys) {
    if (-not ($headsetDemoEnvironment.Keys -ccontains $key) -or
        [string]$headsetDemoEnvironment[$key] -cne
            [string]$expectedHeadsetDemoEnvironment[$key]) {
        throw "Headset demo environment lost exact bounded recording value: $key"
    }
}
$headsetWorldEnvironment = Get-FnvxrProductMinimalEnvironment `
    -RunId "headset-world-contract" `
    -RunDirectory "C:\fnvxr-headset-world-contract" `
    -OpenXrLoaderPath "C:\fnvxr-headset-world-contract\openxr_loader.dll" `
    -SessionReadyTimeoutSeconds 60 `
    -AutomateRetailFixture `
    -HeadsetWorldOnlyCapture `
    -RetailFixtureAction "load" `
    -RetailFixtureSaveName "FNVXR_AutoRetail_L1_FastShot_WildWasteland" `
    -RetailFixtureTraitOne "FastShot" `
    -RetailFixtureTraitTwo "WildWasteland" `
    -HeadlessRuntimeManifest "C:\fnvxr-headset-world-contract\openxr_simulator.json" `
    -HeadsetMirrorCaptureDirectory "C:\fnvxr-headset-world-contract\headset-mirror" `
    -HeadsetMirrorCaptureEveryFrames 1 `
    -HeadsetMirrorCaptureMaxPairs 600
if ([string]$headsetWorldEnvironment.FNVXR_RUN_PROFILE -cne
        "stereo-visual-trial-v5" -or
    [string]$headsetWorldEnvironment.FNVXR_HOST_MODE -cne "vr" -or
    [string]$headsetWorldEnvironment.FNVXR_ENABLE_ENGINE_CENTER_STEREO -cne
        "1" -or
    [string]$headsetWorldEnvironment.FNVXR_HEADSET_DEMO_FIXTURE -cne "1" -or
    [string]$headsetWorldEnvironment.FNVXR_HEADSET_WORLD_ONLY_CAPTURE -cne
        "1") {
    throw "Headset world-only capture lost its exact owned-fixture OpenXR route."
}
foreach ($forbiddenHeadsetWorldInputKey in @(
    "FNVXR_HEADSET_DEMO_GAMEPLAY_WARMUP_FRAMES",
    "FNVXR_HEADSET_DEMO_PIPBOY_HOLD_FRAMES",
    "FNVXR_DESKTOP_ASSIST_ENABLE",
    "FNVXR_ENABLE_CONTROLLER_BRIDGE",
    "FNVXR_ENABLE_TRACKED_WEAPON")) {
    if ($headsetWorldEnvironment.Keys -ccontains
        $forbiddenHeadsetWorldInputKey) {
        throw "Headset world-only capture broadened input authority through: $forbiddenHeadsetWorldInputKey"
    }
}
$headsetWorldWeaponDrawEnvironment = Get-FnvxrProductMinimalEnvironment `
    -RunId "headset-world-weapon-draw-contract" `
    -RunDirectory "C:\fnvxr-headset-world-weapon-draw-contract" `
    -OpenXrLoaderPath "C:\fnvxr-headset-world-weapon-draw-contract\openxr_loader.dll" `
    -SessionReadyTimeoutSeconds 60 `
    -AutomateRetailFixture `
    -HeadsetWorldOnlyCapture `
    -HeadsetFixtureWeaponDraw `
    -RetailVrFirstPersonPrivateCaller Primary `
    -RetailFixtureAction "load" `
    -RetailFixtureSaveName "FNVXR_AutoRetail_L1_Pistol" `
    -RetailFixtureWeapon "Pistol" `
    -HeadlessRuntimeManifest "C:\fnvxr-headset-world-weapon-draw-contract\openxr_simulator.json" `
    -HeadsetMirrorCaptureDirectory "C:\fnvxr-headset-world-weapon-draw-contract\headset-mirror"
if ([string]$headsetWorldWeaponDrawEnvironment.FNVXR_HEADSET_DEMO_FIXTURE -cne "1" -or
    [string]$headsetWorldWeaponDrawEnvironment.FNVXR_HEADSET_WORLD_ONLY_CAPTURE -cne "1" -or
    [string]$headsetWorldWeaponDrawEnvironment.FNVXR_HEADSET_FIXTURE_DRAW_WEAPON -cne "1" -or
    [string]$headsetWorldWeaponDrawEnvironment.FNVXR_HEADSET_FINAL_STOCK_FRAME_CAPTURE -cne "0" -or
    [string]$headsetWorldWeaponDrawEnvironment.FNVXR_RETAIL_FIRST_PERSON_RAW_EYE_CAPTURE -cne "1" -or
    [string]$headsetWorldWeaponDrawEnvironment.FNVXR_RETAIL_FIRST_PERSON_DRAW_TRACE_LIMIT -cne "4096") {
    throw "Headset world weapon draw lost its fail-closed first-person evidence environment gate."
}
if ([string]$headsetWorldWeaponDrawEnvironment.FNVXR_RETAIL_VR_FIRST_PERSON_PRIVATE_CALLER -cne "primary") {
    throw "Headset world weapon draw did not carry the selected primary first-person caller proof route."
}
$headsetControllerRigEnvironment = Get-FnvxrProductMinimalEnvironment `
    -RunId "headset-controller-rig-contract" `
    -RunDirectory "C:\fnvxr-headset-controller-rig-contract" `
    -OpenXrLoaderPath "C:\fnvxr-headset-controller-rig-contract\openxr_loader.dll" `
    -SessionReadyTimeoutSeconds 60 `
    -AutomateRetailFixture `
    -HeadsetWorldOnlyCapture `
    -HeadsetFixtureWeaponDraw `
    -HeadsetControllerRigVisualTrial `
    -RetailFixtureAction "load" `
    -RetailFixtureSaveName "FNVXR_AutoRetail_L1_Pistol" `
    -RetailFixtureWeapon "Pistol" `
    -HeadlessRuntimeManifest "C:\fnvxr-headset-controller-rig-contract\openxr_simulator.json"
if ([string]$headsetControllerRigEnvironment.FNVXR_RETAIL_VR_FIRST_PERSON_PRIVATE_CALLER -cne "third" -or
    [string]$headsetControllerRigEnvironment.FNVXR_RETAIL_CENTER_INTEGRATED_FIRST_PERSON -cne "1") {
    throw "The controller-rig visual trial must publish at the observed outer seam through the zero-private-eye center-integrated branch."
}
$headsetCombatEnvironment = Get-FnvxrProductMinimalEnvironment `
    -RunId "headset-combat-contract" `
    -RunDirectory "C:\fnvxr-headset-combat-contract" `
    -OpenXrLoaderPath "C:\fnvxr-headset-combat-contract\openxr_loader.dll" `
    -SessionReadyTimeoutSeconds 60 `
    -AutomateRetailFixture `
    -HeadsetWorldOnlyCapture `
    -HeadsetFixtureWeaponDraw `
    -HeadsetControllerRigVisualTrial `
    -HeadsetCombatVisualTrial `
    -RetailFixtureAction "load" `
    -RetailFixtureSaveName "FNVXR_AutoRetail_L1_Pistol" `
    -RetailFixtureWeapon "Pistol" `
    -HeadlessRuntimeManifest "C:\fnvxr-headset-combat-contract\openxr_simulator.json"
if ([string]$headsetCombatEnvironment.FNVXR_HEADSET_COMBAT_VISUAL_TRIAL -cne "1" -or
    [string]$headsetCombatEnvironment.FNVXR_EXTERNAL_XINPUT_WRITER -cne "0" -or
    [string]$headsetCombatEnvironment.FNVXR_EXTERNAL_DINPUT_WRITER -cne "0" -or
    [string]$headsetCombatEnvironment.FNVXR_PLUGIN_GAMEPLAY_KEYBOARD_FALLBACK -cne "1") {
    throw "The bounded headless combat trial must keep the generic writer flags closed while retaining its explicit fire/reload lease."
}
$physicalHeadsetEnvironment = Get-FnvxrProductMinimalEnvironment `
    -RunId "physical-headset-play-contract" `
    -RunDirectory "C:\fnvxr-physical-headset-play-contract" `
    -OpenXrLoaderPath "C:\fnvxr-physical-headset-play-contract\openxr_loader.dll" `
    -SessionReadyTimeoutSeconds 60 `
    -AutomateRetailFixture `
    -RetailFixtureAction "load" `
    -RetailFixtureSaveName "FNVXR_AutoRetail_L1_Pistol" `
    -RetailFixtureWeapon "Pistol" `
    -PhysicalHeadsetPlay `
    -PhysicalRuntimeManifest "C:\fnvxr-physical-headset-play-contract\oculus_openxr_64.json"
if ([string]$physicalHeadsetEnvironment.FNVXR_RETAIL_VR_FIRST_PERSON_PRIVATE_CALLER -cne "third" -or
    [string]$physicalHeadsetEnvironment.FNVXR_PHYSICAL_HEADSET_PLAY -cne "1" -or
    [string]$physicalHeadsetEnvironment.FNVXR_RETAIL_CENTER_INTEGRATED_FIRST_PERSON -cne "1") {
    throw "Physical headset play must select one product mode and the observed center-integrated publication seam."
}
if (-not $pluginSource.Contains(
        'calibration.controllerToWristBodyLocal = configuredControllerToWristOffset(left);') -or
    $pluginSource.Contains(
        'calibration.controllerToWristBodyLocal = transformVec3(')) {
    throw "Controller-owned wrist attachment must use the absolute tracked pose, not preserve the initial animated-hand gap."
}
if (-not $pluginSource.Contains(
        '? weaponWorldPosition') -or
    -not $pluginSource.Contains(
        'pose.handLocalTranslation = readVec3(') -or
    -not $pluginSource.Contains(
        'pose.handLocalTranslation)')) {
    throw "Physical weapon continuity must retain the tracked hand translation and keep the weapon attached as its child."
}
foreach ($retiredPhysicalSubfeatureFlag in @(
    "FNVXR_RETAIL_RIG_ENABLE",
    "FNVXR_RETAIL_RIG_APPLY",
    "FNVXR_RETAIL_WEAPON_APPLY",
    "FNVXR_RIGHT_STICK_SNAP_TURN_ENABLE",
    "FNVXR_RIGHT_STICK_SNAP_DEGREES",
    "FNVXR_GAMEPLAY_ANALOG_RUN_ENABLE",
    "FNVXR_HEAD_RELATIVE_LOCOMOTION_ENABLE")) {
    if ($physicalHeadsetEnvironment.Keys -ccontains $retiredPhysicalSubfeatureFlag) {
        throw "Physical product behavior must not depend on subfeature flag: $retiredPhysicalSubfeatureFlag"
    }
}
foreach ($forbiddenWeaponDrawAuthorityKey in @(
    "FNVXR_DESKTOP_ASSIST_ENABLE",
    "FNVXR_ENABLE_CONTROLLER_BRIDGE",
    "FNVXR_ENABLE_TRACKED_WEAPON",
    "FNVXR_PLUGIN_KEYBOARD_MOVEMENT_ENABLE")) {
    if ($headsetWorldWeaponDrawEnvironment.Keys -ccontains
        $forbiddenWeaponDrawAuthorityKey) {
        throw "Headset weapon draw broadened authority through: $forbiddenWeaponDrawAuthorityKey"
    }
}
Require-Throws -Fragment "requires world-only capture" -Action {
    Get-FnvxrProductMinimalEnvironment `
        -RunId "invalid-headset-weapon-draw-world-contract" `
        -RunDirectory "C:\fnvxr-invalid-headset-weapon-draw-world-contract" `
        -OpenXrLoaderPath "C:\fnvxr-invalid-headset-weapon-draw-world-contract\openxr_loader.dll" `
        -SessionReadyTimeoutSeconds 60 `
        -HeadsetFixtureWeaponDraw
}
Require-Throws -Fragment "requires an existing owned fixture load" -Action {
    Get-FnvxrProductMinimalEnvironment `
        -RunId "invalid-headset-weapon-draw-create-contract" `
        -RunDirectory "C:\fnvxr-invalid-headset-weapon-draw-create-contract" `
        -OpenXrLoaderPath "C:\fnvxr-invalid-headset-weapon-draw-create-contract\openxr_loader.dll" `
        -SessionReadyTimeoutSeconds 60 `
        -AutomateRetailFixture `
        -HeadsetWorldOnlyCapture `
        -HeadsetFixtureWeaponDraw `
        -RetailFixtureAction "create" `
        -RetailFixtureSaveName "FNVXR_AutoRetail_L1_Pistol" `
        -RetailFixtureWeapon "Pistol"
}
Require-Throws -Fragment "mutually exclusive" -Action {
    Get-FnvxrProductMinimalEnvironment `
        -RunId "ambiguous-headset-capture-contract" `
        -RunDirectory "C:\fnvxr-ambiguous-headset-capture-contract" `
        -OpenXrLoaderPath "C:\fnvxr-ambiguous-headset-capture-contract\openxr_loader.dll" `
        -SessionReadyTimeoutSeconds 60 `
        -AutomateRetailFixture `
        -HeadsetDemoFixture `
        -HeadsetWorldOnlyCapture `
        -RetailFixtureAction "load" `
        -RetailFixtureSaveName "FNVXR_AutoRetail_L1_FastShot_WildWasteland"
}
$delayedHeadsetDemoEnvironment = Get-FnvxrProductMinimalEnvironment `
    -RunId "delayed-headset-demo-contract" `
    -RunDirectory "C:\fnvxr-delayed-headset-demo-contract" `
    -OpenXrLoaderPath "C:\fnvxr-delayed-headset-demo-contract\openxr_loader.dll" `
    -SessionReadyTimeoutSeconds 60 `
    -AutomateRetailFixture `
    -HeadsetDemoFixture `
    -HeadsetDemoGameplayWarmupFrames 1200 `
    -RetailFixtureAction "load" `
    -RetailFixtureSaveName "FNVXR_AutoRetail_L1_FastShot_WildWasteland" `
    -RetailFixtureTraitOne "FastShot" `
    -RetailFixtureTraitTwo "WildWasteland"
if ([string]$delayedHeadsetDemoEnvironment.FNVXR_HEADSET_DEMO_GAMEPLAY_WARMUP_FRAMES -cne
    "1200") {
    throw "Headset demo diagnostic warmup did not remain bounded and explicit."
}
$captureOnlyHeadsetDemoEnvironment = Get-FnvxrProductMinimalEnvironment `
    -RunId "capture-only-headset-demo-contract" `
    -RunDirectory "C:\fnvxr-capture-only-headset-demo-contract" `
    -OpenXrLoaderPath "C:\fnvxr-capture-only-headset-demo-contract\openxr_loader.dll" `
    -SessionReadyTimeoutSeconds 60 `
    -AutomateRetailFixture `
    -HeadsetDemoFixture `
    -RetailVrAccumulationDiagnosticMode "CaptureOnly" `
    -RetailFixtureAction "load" `
    -RetailFixtureSaveName "FNVXR_AutoRetail_L1_FastShot_WildWasteland" `
    -RetailFixtureTraitOne "FastShot" `
    -RetailFixtureTraitTwo "WildWasteland"
if ([string]$captureOnlyHeadsetDemoEnvironment.FNVXR_RETAIL_VR_ACCUMULATION_DIAGNOSTIC_MODE -cne
    "capture-only") {
    throw "Capture-only accumulation diagnostic did not remain narrowly and explicitly selected."
}
$renderNoPublishHeadsetDemoEnvironment = Get-FnvxrProductMinimalEnvironment `
    -RunId "render-no-publish-headset-demo-contract" `
    -RunDirectory "C:\fnvxr-render-no-publish-headset-demo-contract" `
    -OpenXrLoaderPath "C:\fnvxr-render-no-publish-headset-demo-contract\openxr_loader.dll" `
    -SessionReadyTimeoutSeconds 60 `
    -AutomateRetailFixture `
    -HeadsetDemoFixture `
    -RetailVrAccumulationDiagnosticMode "RenderNoPublish" `
    -RetailFixtureAction "load" `
    -RetailFixtureSaveName "FNVXR_AutoRetail_L1_FastShot_WildWasteland" `
    -RetailFixtureTraitOne "FastShot" `
    -RetailFixtureTraitTwo "WildWasteland"
if ([string]$renderNoPublishHeadsetDemoEnvironment.FNVXR_RETAIL_VR_ACCUMULATION_DIAGNOSTIC_MODE -cne
    "render-no-publish") {
    throw "Render-without-publication diagnostic did not remain narrowly and explicitly selected."
}
foreach ($rendererStageDiagnostic in ([ordered]@{
    SnapshotOnly = "snapshot-only"
    CollectOnly = "collect-only"
    BindOnly = "bind-only"
    CameraOnly = "camera-only"
    PopulateOnly = "populate-only"
    RenderOnly = "render-only"
    FinalizeOnly = "finalize-only"
    LeftEyeOnly = "left-eye-only"
}).GetEnumerator()) {
    $rendererStageEnvironment = Get-FnvxrProductMinimalEnvironment `
        -RunId "$($rendererStageDiagnostic.Value)-headset-demo-contract" `
        -RunDirectory "C:\fnvxr-$($rendererStageDiagnostic.Value)-headset-demo-contract" `
        -OpenXrLoaderPath "C:\fnvxr-$($rendererStageDiagnostic.Value)-headset-demo-contract\openxr_loader.dll" `
        -SessionReadyTimeoutSeconds 60 `
        -AutomateRetailFixture `
        -HeadsetDemoFixture `
        -RetailVrAccumulationDiagnosticMode $rendererStageDiagnostic.Key `
        -RetailFixtureAction "load" `
        -RetailFixtureSaveName "FNVXR_AutoRetail_L1_FastShot_WildWasteland" `
        -RetailFixtureTraitOne "FastShot" `
        -RetailFixtureTraitTwo "WildWasteland"
    if ([string]$rendererStageEnvironment.FNVXR_RETAIL_VR_ACCUMULATION_DIAGNOSTIC_MODE -cne
        [string]$rendererStageDiagnostic.Value) {
        throw "Renderer-stage accumulation diagnostic did not remain exact: $($rendererStageDiagnostic.Key)"
    }
}
Require-Throws -Fragment "requires the bounded headset demo" -Action {
    Get-FnvxrProductMinimalEnvironment `
        -RunId "invalid-accumulation-diagnostic-contract" `
        -RunDirectory "C:\fnvxr-invalid-accumulation-diagnostic-contract" `
        -OpenXrLoaderPath "C:\fnvxr-invalid-accumulation-diagnostic-contract\openxr_loader.dll" `
        -SessionReadyTimeoutSeconds 60 `
        -RetailVrAccumulationDiagnosticMode "RelayOnly"
}
foreach ($forbiddenHeadsetDemoAuthorityKey in @(
    "FNVXR_DESKTOP_ASSIST_ENABLE",
    "FNVXR_ENABLE_CONTROLLER_BRIDGE",
    "FNVXR_ENABLE_TRACKED_WEAPON")) {
    if ($headsetDemoEnvironment.Keys -ccontains $forbiddenHeadsetDemoAuthorityKey) {
        throw "Headset demo broadened authority through: $forbiddenHeadsetDemoAuthorityKey"
    }
}

if ($launcher.Contains('$Process.Modules')) {
    throw "Product launcher must not use the architecture-dependent Process.Modules census for Win32 Fallout."
}
foreach ($moduleCensusContract in @(
    'Get-FnvxrProductLoadedModuleCensus',
    'TH32CS_SNAPMODULE32',
    'CreateToolhelp32Snapshot',
    'Module32FirstW',
    'Module32NextW')) {
    if (-not ($launcher.Contains($moduleCensusContract) -or $common.Contains($moduleCensusContract))) {
        throw "Product launcher lost the native Win32 module census contract: $moduleCensusContract"
    }
}

$readOnlyProbeStart = $common.IndexOf('function Invoke-FnvxrProductReadOnlyRetailRuntimeProbe')
$readOnlyProbeEnd = $common.IndexOf('function Wait-FnvxrProductProbeReady', $readOnlyProbeStart)
if ($readOnlyProbeStart -lt 0 -or $readOnlyProbeEnd -le $readOnlyProbeStart) {
    throw "Product common helpers lost the bounded read-only retail runtime probe wrapper."
}
$readOnlyProbeBody = $common.Substring($readOnlyProbeStart, $readOnlyProbeEnd - $readOnlyProbeStart)
foreach ($readOnlyContract in @(
    '"--pid"',
    '"--wait-ms"',
    'probeReportedEngineCapability = ($exitCode -eq 0)',
    'external read-only loaded-process observation')) {
    if (-not $readOnlyProbeBody.Contains($readOnlyContract)) {
        throw "Read-only retail runtime probe wrapper lost required contract: $readOnlyContract"
    }
}
foreach ($forbiddenReadOnlyAction in @(
    'Set-Item',
    'Start-Process',
    'Stop-Process',
    'Copy-Item',
    'Move-Item',
    'Remove-Item',
    'FNVXR_ENABLE_ENGINE_CENTER_STEREO')) {
    if ($readOnlyProbeBody.Contains($forbiddenReadOnlyAction)) {
        throw "Read-only retail runtime probe wrapper must not perform: $forbiddenReadOnlyAction"
    }
}

if (-not $launcher.Contains('[ValidateRange(5, 900)][int]$MaximumRunSeconds = 40')) {
    throw "Product launcher must allow a bounded lunch/headset wait of up to 900 seconds."
}
if (-not $launcher.Contains('[ValidateRange(5, 900)][int]$RetailReadyTimeoutSeconds = 60')) {
    throw "Product launcher must allow the combined pose/runtime readiness wait to remain bounded at up to 900 seconds."
}
if (-not $launcher.Contains(
        '$fixedRecoveryLoadWaitMilliseconds = [int]($RetailReadyTimeoutSeconds * 1000)') -or
    -not $launcher.Contains('[ValidateRange(5000, 900000)]') -or
    -not $launcher.Contains('-WaitMilliseconds $fixedRecoveryLoadWaitMilliseconds')) {
    throw "Fixed retail-save load must wait only within the bounded retail-ready timeout."
}
if ($launcher.Contains('--wait-ms 15000')) {
    throw "Fixed retail-save load must not cut off a valid retail load at the obsolete 15-second limit."
}
if (-not $hostSource.Contains('envInt("FNVXR_SESSION_READY_TIMEOUT_SECONDS", 45), 1, 900)')) {
    throw "OpenXR host must support the launcher's bounded 900-second off-face session-ready wait."
}

foreach ($cliOnlyContract in @(
    '[string]$HeadlessSimulatorManifest = ""',
    '[string]$PhysicalRuntimeManifest = ""',
    '[switch]$PhysicalHeadsetPlay',
    'if (Test-FnvxrProductProcessElevated)',
    'Resolve-FnvxrProductHeadlessRuntimeManifest',
    'Resolve-FnvxrProductPhysicalRuntimeManifest',
    'selection = "process-local-environment-only"',
    'registryMutationAuthorized = $false',
    'Assert-FnvxrProductHklmActiveRuntimeUnchanged',
    'XR_RUNTIME_JSON',
    'OPENXR_SIMULATOR_HEADLESS',
    'OPENXR_SIMULATOR_DATA_DIR',
    'OPENXR_SIMULATOR_LOG_PATH',
    '[string]$MetaXrOperatorLayerDirectory = ""',
    'Resolve-FnvxrProductMetaXrOperatorLayer',
    'XR_APILAYER_METAX_operator',
    'XR_API_LAYER_PATH',
    'XR_ENABLE_API_LAYERS',
    'Get-NetTCPConnection -State Listen -LocalPort 8720',
    'mcpProxyLaunched = $false',
    'controllerOrPoseOverrideAuthorized = $false',
    'interactive runtime selection is not authorized',
    'observation port 8720 is already owned by another process',
    '$processLocalRuntimeEnvironmentNames',
    'processLocalEnvironmentRestored = $true',
    'hklmActiveRuntimeUnchanged = $true',
    'runtimeLogIdentity',
    'Headless OpenXR runtime manifest or DLL changed after validation.',
    'Physical OpenXR runtime manifest or DLL changed after validation.',
    'function Wait-FnvxrProductStartMenuForRecoveryLoad',
    'function Wait-FnvxrProductLoadedGameplay',
    'function Invoke-FnvxrProductFreshCharacter',
    'function Wait-FnvxrProductFreshCharacterSave',
    'Get-FnvxrProductApprovedRetailSaveLoadCommand -RetailSaveName $RetailSaveName',
    'Get-FnvxrProductFreshCharacterStartCommand',
    '[ValidateSet("FNVXR_StereoTest")]',
    'FNVXR_STEREO_VISUAL_TRIAL_AUTOMATE_RECOVERY_SAVE_NAME',
    'FNVXR_STEREO_VISUAL_TRIAL_AUTOMATE_FRESH_CHARACTER',
    'FNVXR_STEREO_VISUAL_TRIAL_ACK_TRIBAL_PACK_POPUP',
    '[switch]$AcknowledgeTribalPackPopup',
    'official pre-order-pack title/body notifications',
    '[switch]$StartNewCharacter',
    'FNVXR_StereoTest',
    'fresh-character',
    'refuses to overwrite an existing disposable save',
    '[uint32]$snapshot.runtime.phase -eq 1',
    '([uint32]$snapshot.runtime.menuBits -band 2)',
    '[uint32]$snapshot.runtime.phase -eq 3',
    '([uint32]$snapshot.runtime.menuBits -band 0xFE) -eq 0',
    '-not [bool]$snapshot.runtime.uiInputAllowed',
    '[bool]$snapshot.runtime.cameraActive',
    '"--require-player"',
    '[bool]$snapshot.player.usable',
    '[bool]$snapshot.player.cameraValid',
    '[bool]$snapshot.player.cellKnown',
    '[bool]$snapshot.player.gameplay',
    '[uint32]$snapshot.player.currentCellFormId -ne 0',
    'function Test-FnvxrProductLoadOnlySaveUnchanged',
    'loadOnlySaveUnchanged',
    'loadOnlyNvseUnchanged',
    'controllerMutationAuthorized = $false',
    'trackedWeaponAuthorized = $false')) {
    if (-not ($launcher.Contains($cliOnlyContract) -or $common.Contains($cliOnlyContract))) {
        throw "Product launcher lost command-line-only simulator/automation contract: $cliOnlyContract"
     }
}

foreach ($partialProbeContract in @(
    'function Get-FnvxrProductProbeField',
    '$Object.PSObject.Properties[$Name]',
    '$accepted = [bool](& $Accept $sample.json)',
    'Missing fields mean "not ready"',
    'Get-FnvxrProductProbeField -Object $runtime -Name "stable" -DefaultValue $false',
    'Get-FnvxrProductProbeField -Object $player -Name "stable" -DefaultValue $false')) {
    if (-not $launcher.Contains($partialProbeContract)) {
        throw "Product launcher lost fail-closed partial-probe handling: $partialProbeContract"
    }
}

$elevationGate = $launcher.IndexOf('if (Test-FnvxrProductProcessElevated)')
$attestedBuildDecision = $launcher.IndexOf('if ($UseAttestedBuild)')
if ($elevationGate -lt 0 -or $elevationGate -ge $attestedBuildDecision) {
    throw "Product launcher must reject elevation before building, staging, or launching anything."
}
if ([regex]::Matches(
        $launcher,
        [regex]::Escape('-HeadlessRuntimeManifest $headlessRuntimeManifestPath')).Count -ne 2) {
    throw "Validated process-local runtime manifest must feed validation metadata and the live child environment."
}
if ([regex]::Matches(
        $launcher,
        [regex]::Escape('-PhysicalRuntimeManifest $physicalRuntimeManifestPath')).Count -ne 2) {
    throw "Validated physical runtime manifest must feed validation metadata and the live child environment."
}
$recoveryAutomationSubmit = $launcher.LastIndexOf('Invoke-FnvxrProductFixedRecoveryLoad `')
$freshCharacterAutomationSubmit = $launcher.LastIndexOf('Invoke-FnvxrProductFreshCharacter `')
$automationGameplayGate = $launcher.LastIndexOf('Wait-FnvxrProductLoadedGameplay `')
if ($recoveryAutomationSubmit -lt 0 -or $freshCharacterAutomationSubmit -lt 0 -or
    $automationGameplayGate -le $recoveryAutomationSubmit -or
    $automationGameplayGate -le $freshCharacterAutomationSubmit) {
    throw "Fixed recovery and fresh-character automation must submit their exact commands before accepting loaded gameplay."
}
foreach ($forbiddenAutomationMechanism in @(
    "SendInput",
    "SendKeys",
    "keybd_event",
    "mouse_event",
    "SetForegroundWindow",
    "SetCursorPos")) {
    if ($launcher.Contains($forbiddenAutomationMechanism)) {
        throw "Product launcher must not control the simulator/game through OS input: $forbiddenAutomationMechanism"
    }
}

foreach ($headsetDemoContract in @(
    '[switch]$HeadsetDemoFixture',
    '[switch]$HeadsetWorldOnlyCapture',
    '[switch]$HeadsetFixtureWeaponDraw',
    '[switch]$HeadsetPoseSweep',
    'Get-FnvxrProductPipBoyOutputProof',
    'Get-FnvxrProductHeadsetDemoInputProof',
    'Get-FnvxrProductHeadsetMirrorCaptureProof',
    '$left.Name -replace ''_left\.png$'', ''_right.png''',
    'Get-FnvxrProductStereoContinuityProof',
    'rejectedGameplaySubmitFrames',
    '$transactions = [ordered]@{}',
    '$gameplaySubmitFrames = 0',
    'A rejected startup submit must',
    'FNVXR_STEREO_RETAIN_LAST_VALID_ON_REJECT = "1"',
    'FNVXR_STEREO_STALE_FRAME_LIMIT = "30"',
    'FNVXR_CPU_STEREO_MAX_SOURCE_POSE_AGE_MS = "250"',
    'Get-FnvxrProductRetailCameraPoseSweepProof',
    'invoke-openxr-simulator-head-sweep.ps1',
    '"rendered-six-dof-cardinal-proven-centered"',
    'pitchCameraResponseProven',
    'Wait-FnvxrProductRetailFixtureGameplay',
    'retailFixtureRequested -and -not $headsetFixtureOpenXrRun',
    'No final-headset Pip-Boy UI frame reached OpenXR',
    'FNVXR_HEADSET_DEMO_FIXTURE',
    'FNVXR_HEADSET_WORLD_ONLY_CAPTURE',
    'FNVXR_HEADSET_FIXTURE_DRAW_WEAPON',
    'Get-FnvxrProductHeadsetFixtureWeaponDrawProof')) {
    if (-not ($launcher.Contains($headsetDemoContract) -or
            $common.Contains($headsetDemoContract))) {
        throw "Product launcher lost headset demo/recording contract: $headsetDemoContract"
    }
}
if (-not (Get-Content -LiteralPath (Join-Path $SourceRoot `
            "scripts\invoke-openxr-simulator-combat-demo.ps1") -Raw).Contains(
        '$ticksPerShot = 15')) {
    throw "The simulator combat harness must leave a full semi-auto animation/recovery window between confirmed shots."
}
foreach ($physicalPlayContract in @(
    'retail-vr-play-v1',
    'physical-headset-interactive-play',
    '[ValidateRange(1280, 4096)][int]$PhysicalGameWidth = 1600',
    '[ValidateRange(720, 2560)][int]$PhysicalGameHeight = 1728',
    'FNVXR_PHYSICAL_HEADSET_PLAY = "1"',
    'FNVXR_PHASE1_TRACE_TELEMETRY = "1"',
    'FNVXR_PLUGIN_KEYBOARD_MOVEMENT_ENABLE = "1"',
    'FNVXR_PLUGIN_MENU_KEYBOARD_FALLBACK = "1"',
    'FNVXR_PLUGIN_GAMEPLAY_KEYBOARD_FALLBACK = "1"',
    'FNVXR_UI_INPUT_WIDTH',
    'FNVXR_UI_INPUT_HEIGHT',
    'FNVXR_D3D9_NATIVE_APPLY_HEAD_ROTATION = "1"',
    'FNVXR_HEADSPACE_LOOK_ENABLE = "0"',
    'Install-FnvxrProductPhysicalDisplayProfile',
    'Restore-FnvxrProductPhysicalDisplayProfile',
    'Get-FnvxrProductPhysicalDisplayOutputProof',
    '"fnvxrRetailEngineCenterCpuStereo"',
    'live-source-resolution-proven',
    'Get-FnvxrProductControllerAuthorizationProof',
    'controllerConsumerAcknowledged',
    'runtimeControllerMode(',
    'controller mode transition',
    'releaseBeforePress=1',
    'StereoCompletionProofInterval = 24u',
    'periodicStereoCompletion',
    '$manifest.controllerMutationAuthorized = $true',
    'The exact-retail xNVSE controller consumer never acknowledged')) {
    if (-not ($launcher.Contains($physicalPlayContract) -or
            $common.Contains($physicalPlayContract) -or
            $pluginSource.Contains($physicalPlayContract) -or
            $hostSource.Contains($physicalPlayContract) -or
            $d3d9Source.Contains($physicalPlayContract))) {
        throw "Product launcher lost physical-headset play contract: $physicalPlayContract"
    }
}
$physicalDisplayIni = @"
[General]
iSize W=640
[Display]
iSize W=800
iSize H=600
[Other]
iSize H=1
"@
$physicalDisplayIni = ConvertTo-FnvxrProductPhysicalDisplayIniText `
    -Text $physicalDisplayIni `
    -Width 1920 `
    -Height 1200
if (([regex]::Matches(
        $physicalDisplayIni,
        '(?im)^\s*iSize W\s*=')).Count -ne 1 -or
    ([regex]::Matches(
        $physicalDisplayIni,
        '(?im)^\s*iSize H\s*=')).Count -ne 1 -or
    $physicalDisplayIni -notmatch '(?im)^\s*iSize W\s*=\s*1920\s*$' -or
    $physicalDisplayIni -notmatch '(?im)^\s*iSize H\s*=\s*1200\s*$') {
    throw "Physical display-profile INI conversion did not produce one exact staged resolution."
}
Require-Throws -Fragment "aspect" -Action {
    Assert-FnvxrProductPhysicalDisplaySize `
        -Width 1280 `
        -Height 2000
}
$poseProofLog = [System.IO.Path]::GetTempFileName()
try {
    $poseProofFunction = [regex]::Match(
        $launcher,
        '(?s)function Get-FnvxrProductRetailCameraPoseSweepProof \{.*?(?=function Get-FnvxrProductStereoContinuityProof)').Value
    if ([string]::IsNullOrWhiteSpace($poseProofFunction)) {
        throw "Could not isolate the rendered retail camera sweep verifier."
    }
    . ([scriptblock]::Create($poseProofFunction))
    $poseProofLines = @(
        [ordered]@{ event = "fnvxrRetailEngineCenterFrame"; transaction = 1; delivered = $true; cameraPoseValid = $true; hmdPos = @(-0.10, 1.64, -0.08); centerForward = @(-0.98, -0.20, -0.10); centerUp = @(-0.06, -0.03, 0.99); centerOffsetFromStock = @(-7.0, -5.0, -4.0) },
        [ordered]@{ event = "fnvxrRetailEngineCenterFrame"; transaction = 2; delivered = $true; cameraPoseValid = $true; hmdPos = @(0.10, 1.76, 0.08); centerForward = @(-0.98, 0.20, 0.10); centerUp = @(0.06, 0.03, 0.99); centerOffsetFromStock = @(7.0, 5.0, 4.0) },
        [ordered]@{ event = "fnvxrRetailEngineCenterFrame"; transaction = 3; delivered = $true; cameraPoseValid = $true; hmdPos = @(-0.08, 1.65, 0.07); centerForward = @(-0.99, -0.18, 0.09); centerUp = @(0.05, 0.02, 0.99); centerOffsetFromStock = @(-5.0, 4.0, 3.0) },
        [ordered]@{ event = "fnvxrRetailEngineCenterFrame"; transaction = 4; delivered = $true; cameraPoseValid = $true; hmdPos = @(0.08, 1.75, -0.07); centerForward = @(-0.99, 0.18, -0.09); centerUp = @(-0.05, -0.02, 0.99); centerOffsetFromStock = @(5.0, -4.0, -3.0) }
    ) | ForEach-Object {
        $_ | ConvertTo-Json -Compress
    }
    [System.IO.File]::WriteAllLines(
        $poseProofLog,
        $poseProofLines,
        [System.Text.UTF8Encoding]::new($false))
    $poseProof =
        Get-FnvxrProductRetailCameraPoseSweepProof `
            -LogPath $poseProofLog
    if (-not $poseProof -or
        -not [bool]$poseProof.sixDofCameraResponseProven -or
        -not [bool]$poseProof.pitchCameraResponseProven) {
        throw "Rendered retail camera sweep proof did not recognize six-axis motion."
    }
} finally {
    Remove-Item -LiteralPath $poseProofLog -Force -ErrorAction SilentlyContinue
}
$headsetDemoFunction = [regex]::Match(
    $pluginSource,
    '(?s)void processHeadsetDemoFixtureUi\(.*?(?=void processStereoVisualTrialFixedSaveAutomation)').Value
if ([string]::IsNullOrWhiteSpace($headsetDemoFunction)) {
    throw "Could not isolate the bounded headset demo input function."
}
foreach ($requiredHeadsetDemoInputText in @(
    'headsetDemoUiProfileSelected()',
    'tapDirectInputKey(DIK_TAB)',
    'fnvxrHeadsetDemoPipBoyTap',
    'fnvxrHeadsetDemoPipBoyStage',
    'g_headsetDemoFixtureReady')) {
    if (-not $headsetDemoFunction.Contains($requiredHeadsetDemoInputText)) {
        throw "Bounded headset demo lost required in-game evidence: $requiredHeadsetDemoInputText"
    }
}
foreach ($forbiddenHeadsetDemoInputText in @(
    'SendInput',
    'SendKeys',
    'PostMessage',
    'SetForegroundWindow',
    'SetCursorPos',
    'XInput')) {
    if ($headsetDemoFunction.Contains($forbiddenHeadsetDemoInputText)) {
        throw "Bounded headset demo must not use desktop/controller/simulator input: $forbiddenHeadsetDemoInputText"
    }
}
$headsetFixtureWeaponDrawFunction = [regex]::Match(
    $pluginSource,
    '(?s)void processHeadsetWorldOnlyFixtureWeaponDraw\(.*?(?=// The retail GUI normally assigns)').Value
if ([string]::IsNullOrWhiteSpace($headsetFixtureWeaponDrawFunction)) {
    throw "Could not isolate the bounded headset fixture weapon-draw function."
}
foreach ($requiredHeadsetWeaponDrawText in @(
    'headsetWorldOnlyFixtureWeaponDrawRequested()',
    'fixture::headsetWorldOnlyFixturePreparationSaveAuthorized(',
    'fixture::headsetWorldOnlyFixtureWeaponDrawAuthorized(',
    'fnvxrHeadsetFixtureFinalizeSave',
    'fixture::SetFixtureWeaponOutCommand.data()',
    'fnvxrHeadsetFixtureWeaponDraw',
    'fnvxrHeadsetFixtureWeaponDrawResult',
    'FixtureInitialOfficialNoticeDrainFrames = 900u',
    'FixturePersistedCleanSaveDrainFrames = 120u',
    'FixtureQuietGameplayFrames = 180u',
    'FixturePersistedCleanSaveQuietGameplayFrames = 60u',
    'FixtureSaveSettlingFrames = 60u',
    'DrawResultSettlingFrames = 60u',
    'persistedCleanWeaponOut = weaponOut')) {
    if (-not $headsetFixtureWeaponDrawFunction.Contains($requiredHeadsetWeaponDrawText)) {
        throw "Bounded headset weapon draw lost required evidence: $requiredHeadsetWeaponDrawText"
    }
}
foreach ($forbiddenHeadsetWeaponDrawText in @(
    'SendInput',
    'SendKeys',
    'PostMessage',
    'SetForegroundWindow',
    'SetCursorPos',
    'XInput',
    'tapDirectInputKey(DIK_R)')) {
    if ($headsetFixtureWeaponDrawFunction.Contains($forbiddenHeadsetWeaponDrawText)) {
        throw "Bounded headset weapon draw must not use desktop/controller/simulator input: $forbiddenHeadsetWeaponDrawText"
    }
}
$retailFixtureAutomationFunction = [regex]::Match(
    $pluginSource,
    '(?s)void processRetailFixtureAutomation\(.*?(?=void processHeadsetDemoFixtureUi)').Value
$retailFixtureMessageHandler = [regex]::Match(
    $pluginSource,
    '(?s)void processOwnedRetailFixtureMessageMenuAcknowledgements\(.*?(?=enum class RetailFixtureAutomationStage)').Value
if ([string]::IsNullOrWhiteSpace($retailFixtureAutomationFunction) -or
    -not $retailFixtureAutomationFunction.Contains(
        'processOwnedRetailFixtureMessageMenuAcknowledgements(observation);') -or
    [string]::IsNullOrWhiteSpace($retailFixtureMessageHandler) -or
    -not $retailFixtureMessageHandler.Contains(
        'acknowledgeExactOfficialPackMessageMenu(observation);') -or
    -not $retailFixtureMessageHandler.Contains(
        'closeExactRetailFixtureOfficialPackMessageMenu(observation);') -or
    $retailFixtureAutomationFunction.Contains(
        '!headsetDemoFixtureProfileSelected() || !g_headsetDemoFixtureReady')) {
    throw "Headset demo must retain only the exact official-pack popup handler after its owned save is ready."
}
foreach ($forbiddenRegistryWrite in @(
    "Set-ItemProperty",
    "New-ItemProperty",
    "Remove-ItemProperty",
    ".SetValue(",
    "reg.exe",
    "reg add")) {
    if ($launcher.Contains($forbiddenRegistryWrite) -or $common.Contains($forbiddenRegistryWrite)) {
        throw "Product launcher may observe but never write OpenXR registry state: $forbiddenRegistryWrite"
    }
}

foreach ($forbidden in @(
    '$host =',
    "fnvxr-sidecar-common.ps1",
    "start-openxr-retail-sidecar.ps1",
    "FNVXR_D3D9_STEREO_REPLAY",
    "FNVXR_D3D9_STEREO_READBACK",
    'FNVXR_ENABLE_LEGACY_IMAGE_DIAGNOSTICS = "1"',
    'FNVXR_ALLOW_STEREO_WORLD_2D_FALLBACK = "1"')) {
    if ($launcher.Contains($forbidden) -or $builder.Contains($forbidden)) {
        throw "Product scripts contain retired harness text: $forbidden"
    }
}
foreach ($required in @(
    '"stereo-visual-trial-v5"',
    'FNVXR_ENABLE_LEGACY_IMAGE_DIAGNOSTICS = "0"',
    'FNVXR_ENABLE_ENGINE_CENTER_STEREO = $engineCenterStereo',
    'FNVXR_ALLOW_STEREO_WORLD_2D_FALLBACK = "0"',
    'FNVXR_SESSION_READY_TIMEOUT_SECONDS',
    '-WindowStyle Hidden',
    'Get-FnvxrProductProcessEnvironmentEntries',
    'Clear-FnvxrProductProcessEnvironmentVariables',
    'Get-FnvxrProductSourceSnapshot',
    'ctest --test-dir $win32Build',
    'ctest --test-dir $x64Build',
    '--clean-first',
    '[ValidateRange(1, 4)][int]$Parallelism = 2',
    '--parallel $Parallelism')) {
    if (-not ($launcher.Contains($required) -or $builder.Contains($required) -or
        (Get-Content -LiteralPath (Join-Path $SourceRoot "scripts\fnvxr-product-common.ps1") -Raw).Contains($required))) {
        throw "Product scripts are missing required contract text: $required"
    }
}
foreach ($environmentProviderConsumer in @(
    [pscustomobject]@{ name = "product launcher"; content = $launcher },
    [pscustomobject]@{ name = "retail fixture launcher"; content = $fixtureLauncher },
    [pscustomobject]@{ name = "product common"; content = $common })) {
    if ($environmentProviderConsumer.content.Contains("Get-ChildItem Env:")) {
        throw "$($environmentProviderConsumer.name) must tolerate case-duplicate native environment keys."
    }
}
$sessionTimeoutBinding = '-SessionReadyTimeoutSeconds $RetailReadyTimeoutSeconds'
if ([regex]::Matches($launcher, [regex]::Escape($sessionTimeoutBinding)).Count -ne 2) {
    throw "Product launcher must bind the validated readiness timeout into both validation metadata and the live host environment."
}
foreach ($trialBoundary in @(
    '"stereo-visual-trial-only"',
    'fullProductAccepted = $false',
    'controllerMutationAuthorized = $false',
    'trackedWeaponAuthorized = $false',
    '$manifest.trialReady = $true',
    '$manifest.readiness.retailVrBridge = $true',
    '$manifest.readiness.stereoOutput = $true',
    'Get-FnvxrProductStereoOutputProof',
    'Get-FnvxrProductStereoContinuityProof')) {
    if (-not $launcher.Contains($trialBoundary)) {
        throw "Product launcher lost its honest visual-trial boundary: $trialBoundary"
    }
}
if ($launcher.Contains('$manifest.accepted = $true')) {
    throw "Visual trial must not represent itself as full product acceptance."
}
if (-not $launcher.Contains(
    'retail VR bridge initialized: exact AccumulateScene callsite hook, ordinary-D3D9 CPU-v8 stereo transport, and deferred Present bootstrap ready')) {
    throw "Visual trial must prove bridge initialization instead of accepting a merely loaded proxy."
}
if (-not $launcher.Contains(
    'No proven binocular engine-stereo frame reached OpenXR')) {
    throw "Visual trial must fail when no binocular engine-stereo output was observed."
}

$hostStart = $launcher.IndexOf('$hostProcess = Start-Process')
$hostReady = $launcher.IndexOf('$hostBridgeReady = Wait-FnvxrProductHostBridgeReady')
$stage = $launcher.IndexOf('$staged = @(Install-FnvxrProductArtifactSet')
$retailProfileStage = $launcher.IndexOf('Install-FnvxrProductRetailVisualTrialPluginProfile')
$retailStart = $launcher.IndexOf('$nvse = Start-Process')
$runtimeReady = $launcher.IndexOf('-Description "advancing retail runtime plus OpenXR pose publication"')
if ($hostStart -lt 0 -or $hostReady -le $hostStart -or
    $retailProfileStage -le $hostReady -or $retailProfileStage -ge $stage -or
    $stage -le $hostReady -or
    $retailStart -le $stage -or $runtimeReady -le $retailStart) {
    throw "Product launcher ordering is not host -> authoritative bridge -> temporary retail profile -> stage -> NVSE -> advancing pose/runtime."
}
$stagedToRetail = $launcher.Substring($stage, $retailStart - $stage)
if (-not $stagedToRetail.Contains('OpenXR host exited after bridge readiness but before retail launch.')) {
    throw "Product launcher must recheck host liveness after staging and before launching retail."
}
foreach ($bridgeContract in @(
    'function Wait-FnvxrProductHostBridgeReady',
    'fnvxrHostBridgeReady xrSessionCreated=1 sharedMappingsReady=1',
    '$manifest.readiness.hostBridge = $true')) {
    if (-not $launcher.Contains($bridgeContract)) {
        throw "Product launcher is missing the pre-retail host bridge contract: $bridgeContract"
    }
}
$bridgeWaitStart = $launcher.IndexOf('function Wait-FnvxrProductHostBridgeReady')
$bridgeWaitEnd = $launcher.IndexOf("`ntry {", $bridgeWaitStart)
if ($bridgeWaitEnd -le $bridgeWaitStart) {
    throw "Product launcher bridge wait function has no bounded implementation body."
}
$bridgeWaitBody = $launcher.Substring($bridgeWaitStart, $bridgeWaitEnd - $bridgeWaitStart)
foreach ($livenessContract in @(
    '$Process.Refresh()',
    'if ($Process.HasExited) { return $false }',
    'return -not $Process.HasExited',
    '[DateTime]::UtcNow.AddSeconds($TimeoutSeconds)')) {
    if (-not $bridgeWaitBody.Contains($livenessContract)) {
        throw "Product launcher bridge wait does not fail closed on host lifetime/timeout: $livenessContract"
    }
}
if ($launcher.Contains('-Description "advancing OpenXR v8 pose publication"')) {
    throw "Product launcher must not require an advancing pose before Fallout is staged and launched."
}
$hostPoseAccepted = $launcher.IndexOf('$manifest.readiness.hostPose = $true')
if ($hostPoseAccepted -le $runtimeReady) {
    throw "Product launcher must record pose readiness only after the combined post-retail pose/runtime gate."
}

$finallyStart = $launcher.IndexOf('} finally {')
$finallyEnd = $launcher.IndexOf('if ($manifest.error) { throw $manifest.error }', $finallyStart)
if ($finallyStart -lt 0 -or $finallyEnd -le $finallyStart) {
    throw "Product launcher has no inspectable finally cleanup body."
}
$finallyBody = $launcher.Substring($finallyStart, $finallyEnd - $finallyStart)
$restoreCall = $finallyBody.IndexOf('Restore-FnvxrProductArtifactSet -Records $staged')
$retailProfileRestoreCall = $finallyBody.IndexOf('Restore-FnvxrProductRetailVisualTrialPluginProfile')
$lastOwnedStop = $finallyBody.LastIndexOf('Stop-FnvxrOwnedProcess')
if ($restoreCall -le $lastOwnedStop -or $retailProfileRestoreCall -le $lastOwnedStop) {
    throw "Product launcher must restore the temporary stage only after every owned process is stopped."
}
foreach ($restorationContract in @(
    'if ($staged.Count -gt 0)',
    '$manifest.cleanup.stageRestorationRequired = $true',
    '$manifest.cleanup.stagedArtifactsRestored = $true')) {
    if (-not $finallyBody.Contains($restorationContract)) {
        throw "Product launcher lost unconditional temporary-stage restoration: $restorationContract"
    }
}
foreach ($forbiddenCleanupGate in @(
    '-not $runtimeReady',
    'if ($manifest.error -and $staged.Count')) {
    if ($finallyBody.Contains($forbiddenCleanupGate)) {
        throw "Product launcher may not retain temporary game files after readiness or successful supervision: $forbiddenCleanupGate"
    }
}

$root = (Resolve-Path -LiteralPath $SourceRoot).Path
$fixtureRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("fnvxr-product-launcher-" + [Guid]::NewGuid().ToString("N"))
$tempPrefix = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\') + '\'
$resolvedFixture = [System.IO.Path]::GetFullPath($fixtureRoot)
if (-not $resolvedFixture.StartsWith($tempPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Unsafe product launcher fixture path: $resolvedFixture"
}

try {
    $selfCensus = @(Get-FnvxrProductLoadedModuleCensus -ProcessId ([uint32]$PID))
    $selfPath = (Get-Process -Id $PID).Path
    if (-not @($selfCensus | Where-Object {
        [string]::Equals($_.path, $selfPath, [System.StringComparison]::OrdinalIgnoreCase)
    })) {
        throw "Native module census did not report its own exact executable path."
    }

    if ([IntPtr]::Size -eq 8) {
        $win32PowerShell = Join-Path $env:SystemRoot "SysWOW64\WindowsPowerShell\v1.0\powershell.exe"
        if (-not (Test-Path -LiteralPath $win32PowerShell -PathType Leaf)) {
            throw "Cross-bitness module-census regression requires Win32 PowerShell: $win32PowerShell"
        }
        $win32Fixture = $null
        try {
            $win32Fixture = Start-Process `
                -FilePath $win32PowerShell `
                -ArgumentList @("-NoLogo", "-NoProfile", "-NonInteractive", "-Command", "Start-Sleep -Seconds 30") `
                -WindowStyle Hidden `
                -PassThru
            $fixtureDeadline = [DateTime]::UtcNow.AddSeconds(5)
            do {
                $win32Fixture.Refresh()
                if ($win32Fixture.HasExited) { throw "Win32 module-census fixture exited before inspection." }
                $win32Census = @(Get-FnvxrProductLoadedModuleCensus -ProcessId ([uint32]$win32Fixture.Id))
                $win32Kernel = @($win32Census | Where-Object {
                    [System.IO.Path]::GetFileName([string]$_.path) -ieq "kernel32.dll" -and
                    [uint64]$_.baseAddress -lt 0x100000000L
                })
                if ($win32Kernel.Count -gt 0) { break }
                Start-Sleep -Milliseconds 50
            } while ([DateTime]::UtcNow -lt $fixtureDeadline)
            if ($win32Kernel.Count -eq 0) {
                throw "Native module census did not expose the Win32 loader list from an x64 supervisor."
            }
        } finally {
            if ($win32Fixture) {
                Stop-Process -Id $win32Fixture.Id -Force -ErrorAction SilentlyContinue
                $win32Fixture.WaitForExit(5000) | Out-Null
            }
        }
    }

    New-Item -ItemType Directory -Path $fixtureRoot -Force | Out-Null
    $x64Pe = Get-FnvxrProductFileIdentity -Path $selfPath -RequirePe
    if ($x64Pe.peMachine -cne "0x8664") {
        throw "Headless runtime manifest regression requires an x64 PowerShell fixture."
    }
    $runtimeFixtureDll = Join-Path $fixtureRoot "openxr_simulator.dll"
    Copy-Item -LiteralPath $x64Pe.path -Destination $runtimeFixtureDll
    $runtimeManifestPath = Join-Path $fixtureRoot "openxr_simulator.json"
    $runtimeManifestJson = [ordered]@{
        file_format_version = "1.0.0"
        runtime = [ordered]@{
            library_path = "openxr_simulator.dll"
        }
    } | ConvertTo-Json -Depth 4
    [System.IO.File]::WriteAllText($runtimeManifestPath, $runtimeManifestJson)
    $runtimeManifestIdentity =
        Resolve-FnvxrProductHeadlessRuntimeManifest `
            -ManifestPath $runtimeManifestPath
    if ($runtimeManifestIdentity.selection -cne "process-local-environment-only" -or
        -not $runtimeManifestIdentity.headless -or
        $runtimeManifestIdentity.registryMutationAuthorized -or
        $runtimeManifestIdentity.runtimeDll.peMachine -cne "0x8664" -or
        $runtimeManifestIdentity.runtimeDll.sha256 -cne $x64Pe.sha256) {
        throw "Headless runtime manifest did not preserve its validated x64 process-local identity."
    }
    $registryBefore = Get-FnvxrProductHklmActiveRuntimeSnapshot
    $registryAfter =
        Assert-FnvxrProductHklmActiveRuntimeUnchanged -Before $registryBefore
    if (($registryBefore | ConvertTo-Json -Depth 6 -Compress) -cne
        ($registryAfter | ConvertTo-Json -Depth 6 -Compress)) {
        throw "Read-only ActiveRuntime assertion did not return the exact unchanged HKLM snapshot."
    }

    $peCandidates = @(
        (Join-Path $env:SystemRoot "SysWOW64\notepad.exe"),
        (Join-Path $env:SystemRoot "SysWOW64\WindowsPowerShell\v1.0\powershell.exe"),
        (Join-Path $root "build-win32\Release\fnvxr_shared_state_probe.exe"))
    $win32Pe = $null
    foreach ($candidate in $peCandidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            $candidateIdentity = Get-FnvxrProductFileIdentity -Path $candidate -RequirePe
            if ($candidateIdentity.peMachine -eq "0x014c") {
                $win32Pe = $candidateIdentity
                break
            }
        }
    }
    if (-not $win32Pe) { throw "Test fixture could not locate a Win32 PE image." }
    $win32RuntimeManifestPath = Join-Path $fixtureRoot "win32-openxr-simulator.json"
    $win32RuntimeManifestJson = [ordered]@{
        file_format_version = "1.0.0"
        runtime = [ordered]@{
            library_path = $win32Pe.path
        }
    } | ConvertTo-Json -Depth 4
    [System.IO.File]::WriteAllText(
        $win32RuntimeManifestPath,
        $win32RuntimeManifestJson)
    Require-Throws -Fragment "runtime DLL is not x64" -Action {
        Resolve-FnvxrProductHeadlessRuntimeManifest `
            -ManifestPath $win32RuntimeManifestPath
    }

    $destinationRoot = Join-Path $fixtureRoot "game"
    $pluginRoot = Join-Path $destinationRoot "Data\NVSE\Plugins"
    New-Item -ItemType Directory -Path $pluginRoot -Force | Out-Null

    # The retail-profile stage is deliberately independent of the save
    # directory. It may alter only this temporary plugins.txt fixture, must
    # use the exact official master list, and must restore prior bytes.
    $retailDataRoot = Join-Path $destinationRoot "Data"
    foreach ($plugin in $expectedRetailVisualTrialPlugins) {
        [System.IO.File]::WriteAllText(
            (Join-Path $retailDataRoot $plugin),
            "retail-master-fixture:$plugin")
    }
    $retailProfileDirectory = Join-Path $fixtureRoot "retail-profile"
    New-Item -ItemType Directory -Path $retailProfileDirectory -Force | Out-Null
    $retailProfilePath = Join-Path $retailProfileDirectory "plugins.txt"
    [System.IO.File]::WriteAllText(
        $retailProfilePath,
        "FalloutNV.esm`r`nFallout3.esm`r`nTaleOfTwoWastelands.esm`r`n")
    $retailProfileBefore = Get-FnvxrProductFileIdentity -Path $retailProfilePath
    $retailProfileRecord = Install-FnvxrProductRetailVisualTrialPluginProfile `
        -Path $retailProfilePath `
        -BackupRoot (Join-Path $fixtureRoot "retail-profile-backup") `
        -RunId "retail-profile-contract" `
        -GameRoot $destinationRoot
    if (-not $retailProfileRecord.changed -or
        (Get-Content -LiteralPath $retailProfilePath -Raw) -cne
            (Get-FnvxrProductRetailVisualTrialPluginProfileText)) {
        throw "Temporary retail plugin profile did not replace the fixture with the exact approved master list."
    }
    $alreadyExactRetailProfile = Install-FnvxrProductRetailVisualTrialPluginProfile `
        -Path $retailProfilePath `
        -BackupRoot (Join-Path $fixtureRoot "retail-profile-backup") `
        -RunId "retail-profile-contract-repeat" `
        -GameRoot $destinationRoot
    if ($alreadyExactRetailProfile.changed) {
        throw "Exact retail plugin profile should not be rewritten."
    }
    [void](Restore-FnvxrProductRetailVisualTrialPluginProfile `
        -Record $alreadyExactRetailProfile)
    [void](Restore-FnvxrProductRetailVisualTrialPluginProfile `
        -Record $retailProfileRecord)
    if ((Get-FnvxrProductFileIdentity -Path $retailProfilePath).sha256 -cne
        $retailProfileBefore.sha256) {
        throw "Temporary retail plugin profile did not restore the user's original fixture bytes."
    }

    # Fixture creation deliberately stages a different, base-game-only profile.
    # It must still be byte-for-byte reversible and must never re-use the
    # historical visual-trial master list.
    $retailFixtureProfilePath = Join-Path $retailProfileDirectory "fixture-plugins.txt"
    [System.IO.File]::WriteAllText(
        $retailFixtureProfilePath,
        "FalloutNV.esm`r`nFallout3.esm`r`nTaleOfTwoWastelands.esm`r`n")
    $retailFixtureProfileBefore =
        Get-FnvxrProductFileIdentity -Path $retailFixtureProfilePath
    $retailFixtureProfileRecord =
        Install-FnvxrProductRetailFixturePluginProfile `
            -Path $retailFixtureProfilePath `
            -BackupRoot (Join-Path $fixtureRoot "retail-fixture-profile-backup") `
            -RunId "retail-fixture-profile-contract" `
            -GameRoot $destinationRoot
    if (-not $retailFixtureProfileRecord.changed -or
        (Get-Content -LiteralPath $retailFixtureProfilePath -Raw) -cne
            (Get-FnvxrProductRetailFixturePluginProfileText) -or
        ((@($retailFixtureProfileRecord.entries) -join "|") -cne "FalloutNV.esm")) {
        throw "Retail fixture plugin profile did not stage the exact base-game-only master list."
    }
    $alreadyExactRetailFixtureProfile =
        Install-FnvxrProductRetailFixturePluginProfile `
            -Path $retailFixtureProfilePath `
            -BackupRoot (Join-Path $fixtureRoot "retail-fixture-profile-backup") `
            -RunId "retail-fixture-profile-contract-repeat" `
            -GameRoot $destinationRoot
    if ($alreadyExactRetailFixtureProfile.changed) {
        throw "Exact retail fixture plugin profile should not be rewritten."
    }
    [void](Restore-FnvxrProductRetailFixturePluginProfile `
        -Record $alreadyExactRetailFixtureProfile)
    [void](Restore-FnvxrProductRetailFixturePluginProfile `
        -Record $retailFixtureProfileRecord)
    if ((Get-FnvxrProductFileIdentity -Path $retailFixtureProfilePath).sha256 -cne
        $retailFixtureProfileBefore.sha256) {
        throw "Retail fixture plugin profile did not restore the user's original fixture bytes."
    }

    $ownedFixtureSavePath = Join-Path $fixtureRoot "FNVXR_AutoRetail_L1_Base.fos"
    $ownedFixtureNvsePath = Join-Path $fixtureRoot "FNVXR_AutoRetail_L1_Base.nvse"
    [System.IO.File]::WriteAllText($ownedFixtureSavePath, "owned-retail-fixture-save")
    [System.IO.File]::WriteAllText($ownedFixtureNvsePath, "owned-retail-fixture-nvse")
    $settledFixturePair = Wait-FnvxrProductRetailFixtureSavePair `
        -SavePath $ownedFixtureSavePath `
        -NvsePath $ownedFixtureNvsePath `
        -RequiredProcess (Get-Process -Id $PID) `
        -TimeoutSeconds 5
    if (-not [bool]$settledFixturePair.settled -or
        $settledFixturePair.save.sha256 -ne
            (Get-FnvxrProductFileIdentity -Path $ownedFixtureSavePath).sha256 -or
        $settledFixturePair.nvse.sha256 -ne
            (Get-FnvxrProductFileIdentity -Path $ownedFixtureNvsePath).sha256) {
        throw "Retail fixture creation did not require a stable .fos/.nvse save pair."
    }

    $existingDestination = Join-Path $destinationRoot "d3d9.dll"
    [System.IO.File]::WriteAllText($existingDestination, "previous-user-file")
    $previousHash = (Get-FnvxrProductFileIdentity -Path $existingDestination).sha256
    $plan = @(
        [pscustomobject]@{ key = "x86/d3d9.dll"; source = $win32Pe.path; destination = $existingDestination; machine = "0x014c" },
        [pscustomobject]@{ key = "x86/nvse_fnvxr.dll"; source = $win32Pe.path; destination = Join-Path $pluginRoot "nvse_fnvxr.dll"; machine = "0x014c" },
        [pscustomobject]@{ key = "x86/dinput8.dll"; source = $win32Pe.path; destination = Join-Path $destinationRoot "dinput8.dll"; machine = "0x014c" },
        [pscustomobject]@{ key = "x86/xinput1_3.dll"; source = $win32Pe.path; destination = Join-Path $destinationRoot "xinput1_3.dll"; machine = "0x014c" })
    $records = @(Install-FnvxrProductArtifactSet `
        -Plan $plan `
        -BackupRoot (Join-Path $fixtureRoot "backup") `
        -RunId "contract-test")
    if ($records.Count -ne 4) { throw "Stage transaction did not install exactly four artifacts." }
    foreach ($record in $records) {
        if ($record.destination.sha256 -cne $win32Pe.sha256) {
            throw "Stage transaction did not preserve source bytes."
        }
    }
    foreach ($forbiddenAlias in @("xinput9_1_0.dll", "xinput1_4.dll", "xinput1_2.dll", "xinput1_1.dll")) {
        if (Test-Path -LiteralPath (Join-Path $destinationRoot $forbiddenAlias)) {
            throw "Narrow product staging created an unrequested XInput alias: $forbiddenAlias"
        }
    }
    Restore-FnvxrProductArtifactSet -Records $records
    if ((Get-FnvxrProductFileIdentity -Path $existingDestination).sha256 -cne $previousHash) {
        throw "Stage rollback did not restore the pre-existing destination."
    }
    foreach ($newPath in @(
        (Join-Path $pluginRoot "nvse_fnvxr.dll"),
        (Join-Path $destinationRoot "dinput8.dll"),
        (Join-Path $destinationRoot "xinput1_3.dll"))) {
        if (Test-Path -LiteralPath $newPath) { throw "Stage rollback left a newly-created artifact: $newPath" }
    }

    # Windows Error Reporting can retain a just-exited Fallout DLL briefly.
    # The restore helper must retry exactly the Win32 sharing/lock codes, while
    # keeping all unrelated failures fail-closed.
    $sharingRetryState = [pscustomobject]@{ attempts = 0 }
    Invoke-FnvxrProductSharingViolationRetry -TimeoutMilliseconds 1000 -RetryMilliseconds 10 -Action {
        ++$sharingRetryState.attempts
        if ($sharingRetryState.attempts -lt 3) {
            throw [System.IO.IOException]::new(
                "fixture sharing violation",
                [int]0x80070020)
        }
    }
    if ($sharingRetryState.attempts -ne 3) {
        throw "Sharing-violation restoration retry did not converge exactly."
    }
    $nonSharingRetryState = [pscustomobject]@{ attempts = 0 }
    Require-Throws -Fragment "fixture non-sharing failure" -Action {
        Invoke-FnvxrProductSharingViolationRetry -TimeoutMilliseconds 1000 -RetryMilliseconds 10 -Action {
            ++$nonSharingRetryState.attempts
            throw [System.IO.IOException]::new(
                "fixture non-sharing failure",
                [int]0x80070002)
        }
    }
    if ($nonSharingRetryState.attempts -ne 1) {
        throw "Non-sharing restoration failure was retried instead of failing closed."
    }

    # A stage failure after the first file has been written must restore that
    # first file before the error escapes. This is the exact failure shape the
    # desktop-assist supervisor needs to survive without changing the game.
    $partialDestination = Join-Path $destinationRoot "partial-stage.dll"
    [System.IO.File]::WriteAllText($partialDestination, "partial-stage-previous-user-file")
    $partialPreviousHash = (Get-FnvxrProductFileIdentity -Path $partialDestination).sha256
    $partialPlan = @(
        [pscustomobject]@{
            key = "z/partial-stage.dll"
            source = $win32Pe.path
            destination = $partialDestination
            machine = "0x014c"
        },
        [pscustomobject]@{
            key = "a/missing-stage.dll"
            source = Join-Path $fixtureRoot "does-not-exist.dll"
            destination = Join-Path $destinationRoot "missing-stage.dll"
            machine = "0x014c"
        })
    Require-Throws -Fragment "Required file is missing" -Action {
        Install-FnvxrProductArtifactSet `
            -Plan $partialPlan `
            -BackupRoot (Join-Path $fixtureRoot "partial-backup") `
            -RunId "partial-stage"
    }
    if ((Get-FnvxrProductFileIdentity -Path $partialDestination).sha256 -cne $partialPreviousHash) {
        throw "A failed later stage left the earlier destination changed."
    }

    # A damaged backup for one record must still leave the other staged record
    # restored. The rollback error must make that partial recovery explicit.
    $firstRestoreDestination = Join-Path $destinationRoot "first-restore.dll"
    $secondRestoreDestination = Join-Path $destinationRoot "second-restore.dll"
    [System.IO.File]::WriteAllText($firstRestoreDestination, "first-restore-previous-user-file")
    [System.IO.File]::WriteAllText($secondRestoreDestination, "second-restore-previous-user-file")
    $firstRestoreHash = (Get-FnvxrProductFileIdentity -Path $firstRestoreDestination).sha256
    $secondRestoreHash = (Get-FnvxrProductFileIdentity -Path $secondRestoreDestination).sha256
    $restorePlan = @(
        [pscustomobject]@{
            key = "a/first-restore.dll"
            source = $win32Pe.path
            destination = $firstRestoreDestination
            machine = "0x014c"
        },
        [pscustomobject]@{
            key = "z/second-restore.dll"
            source = $win32Pe.path
            destination = $secondRestoreDestination
            machine = "0x014c"
        })
    $restoreRecords = @(Install-FnvxrProductArtifactSet `
        -Plan $restorePlan `
        -BackupRoot (Join-Path $fixtureRoot "damaged-backup") `
        -RunId "damaged-backup")
    $damagedRecord = @($restoreRecords | Where-Object { $_.key -eq "z/second-restore.dll" })[0]
    Remove-Item -LiteralPath $damagedRecord.backup -Force
    Require-Throws -Fragment "attempted every record" -Action {
        Restore-FnvxrProductArtifactSet -Records $restoreRecords
    }
    if ((Get-FnvxrProductFileIdentity -Path $firstRestoreDestination).sha256 -cne $firstRestoreHash) {
        throw "Rollback stopped at a damaged backup instead of restoring the remaining artifact."
    }
    if ((Get-FnvxrProductFileIdentity -Path $secondRestoreDestination).sha256 -ne $win32Pe.sha256) {
        throw "The damaged-backup fixture no longer proves the failed record remained staged."
    }

    # A read-only engine observation uses a nonzero exit for the normal
    # incomplete-production-proof state. The desktop supervisor must retain
    # that evidence without treating it as authorization or a process error.
    $readOnlyProbe = Join-Path $fixtureRoot "read-only-runtime-probe.cmd"
    $readOnlyLog = Join-Path $fixtureRoot "read-only-runtime-probe.log"
    [System.IO.File]::WriteAllText(
        $readOnlyProbe,
        "@echo off`r`n" +
        "echo scene_graph non_null_pointer_observation=MATCH`r`n" +
        "echo loaded_pe timestamp=0x4e0d50ed proof=MATCH`r`n" +
        "echo retail_engine_capability_proof=FAIL`r`n" +
        "exit /b 1`r`n")
    $readOnlyObservation = Invoke-FnvxrProductReadOnlyRetailRuntimeProbe `
        -ProbePath $readOnlyProbe `
        -ProcessId ([uint32]$PID) `
        -WaitMilliseconds 0 `
        -LogPath $readOnlyLog
    if (-not $readOnlyObservation.captured `
        -or $readOnlyObservation.exitCode -ne 1 `
        -or $readOnlyObservation.probeReportedEngineCapability `
        -or -not $readOnlyObservation.sceneGraphObserved `
        -or -not $readOnlyObservation.loadedIdentityMatched `
        -or $readOnlyObservation.capabilityProofObserved) {
        throw "Read-only engine observation did not preserve an incomplete proof as evidence only."
    }
    if (-not (Get-Content -LiteralPath $readOnlyLog -Raw).Contains(
            "retail_engine_capability_proof=FAIL")) {
        throw "Read-only engine observation did not preserve the probe output."
    }
    Require-Throws -Fragment "Read-only retail runtime probe is missing" -Action {
        Invoke-FnvxrProductReadOnlyRetailRuntimeProbe `
            -ProbePath (Join-Path $fixtureRoot "missing-runtime-probe.exe") `
            -ProcessId ([uint32]$PID) `
            -WaitMilliseconds 0 `
            -LogPath $readOnlyLog
    }

    $badPlan = @([pscustomobject]@{
        key = "bad"
        source = $win32Pe.path
        destination = Join-Path $destinationRoot "bad.dll"
        machine = "0x8664"
    })
    Require-Throws -Fragment "wrong architecture" -Action {
        Install-FnvxrProductArtifactSet -Plan $badPlan -BackupRoot (Join-Path $fixtureRoot "bad-backup") -RunId "bad"
    }

    $stagePlan = Get-FnvxrProductStagePlan -Root $root -Configuration Release -GameRoot $destinationRoot
    $expectedKeys = @("x86/d3d9.dll", "x86/nvse_fnvxr.dll", "x86/dinput8.dll", "x86/xinput1_3.dll")
    if (@($stagePlan.key).Count -ne $expectedKeys.Count -or
        (Compare-Object -ReferenceObject $expectedKeys -DifferenceObject @($stagePlan.key))) {
        throw "Product stage plan is not the exact four-file Win32 set."
    }
    $fixtureStagePlan = @(Get-FnvxrProductRetailFixtureStagePlan `
        -Root $root `
        -Configuration Release `
        -GameRoot $destinationRoot)
    if ($fixtureStagePlan.Count -ne 1 -or
        $fixtureStagePlan[0].key -cne "x86/nvse_fnvxr.dll" -or
        $fixtureStagePlan[0].destination -cne
            (Join-Path $destinationRoot "Data\NVSE\Plugins\nvse_fnvxr.dll")) {
        throw "Retail fixture stage plan must contain only the xNVSE plugin."
    }
} finally {
    if ((Test-Path -LiteralPath $resolvedFixture -PathType Container) -and
        $resolvedFixture.StartsWith($tempPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        Remove-Item -LiteralPath $resolvedFixture -Recurse -Force
    }
}
