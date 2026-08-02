param(
    [Parameter(Mandatory = $true)][string]$SourceRoot
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

. (Join-Path $SourceRoot "scripts\fnvxr-product-common.ps1")

$launcherPath = Join-Path $SourceRoot "scripts\start-fnvxr-retail-fixture.ps1"
$productLauncherPath = Join-Path $SourceRoot "scripts\start-fnvxr-product.ps1"
$commonPath = Join-Path $SourceRoot "scripts\fnvxr-product-common.ps1"
$pluginPath = Join-Path $SourceRoot "plugin\fnvxr_nvse_plugin.cpp"
$authorityPath = Join-Path $SourceRoot "runtime\fnvxr_retail_fixture_automation_authority.h"
foreach ($path in @($launcherPath, $productLauncherPath, $commonPath, $pluginPath, $authorityPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "TTW fixture contract path is missing: $path"
    }
}

foreach ($path in @($launcherPath, $productLauncherPath, $commonPath)) {
    $tokens = $null
    $parseErrors = $null
    [System.Management.Automation.Language.Parser]::ParseFile(
        $path,
        [ref]$tokens,
        [ref]$parseErrors) | Out-Null
    if ($parseErrors.Count -ne 0) {
        throw "TTW fixture PowerShell contract has parse errors: $path"
    }
}

$launcher = Get-Content -LiteralPath $launcherPath -Raw
$productLauncher = Get-Content -LiteralPath $productLauncherPath -Raw
$common = Get-Content -LiteralPath $commonPath -Raw
$plugin = Get-Content -LiteralPath $pluginPath -Raw
$authority = Get-Content -LiteralPath $authorityPath -Raw
foreach ($required in @(
    '[switch]$TtwCore',
    'TTW fixture GameRoot must be an isolated workspace sandbox below:',
    'fnvxr-ttw-retail-sandbox-v1',
    'fnvxr-ttw-fixture-v1',
    'FNVXR_AutoTTW',
    'Get-FnvxrProductTtwFixtureSaveName',
    'Assert-FnvxrProductTtwBaselinePluginData',
    'Install-FnvxrProductTtwBaselinePluginProfile',
    'Restore-FnvxrProductTtwBaselinePluginProfile',
    '-TtwFixture:$TtwCore',
    '-WindowStyle Hidden',
    'A pre-existing runtime is present; refusing to attach to, control, or stop it.',
    'noOpenXrOrSimulator = $true')) {
    if (-not $launcher.Contains($required)) {
        throw "TTW fixture launcher lost required isolation contract: $required"
    }
}
foreach ($required in @(
    'function Get-FnvxrProductTtwFixtureSaveName',
    'function Assert-FnvxrProductTtwFixtureSaveName',
    'FNVXR_RETAIL_FIXTURE_ACK_OFFICIAL_PACK_POPUP = "1"',
    'FNVXR_RETAIL_FIXTURE_ACK_TTW_STEWIE_DEPENDENCY_WARNING')) {
    if (-not $common.Contains($required)) {
        throw "TTW fixture common helper is incomplete: $required"
    }
}
foreach ($forbidden in @(
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
    if ($launcher.Contains($forbidden)) {
        throw "TTW fixture launcher must remain OpenXR/proxy/OS-input free: $forbidden"
    }
}

if ((Get-FnvxrProductTtwFixtureSaveName `
        -TraitOne "FastShot" `
        -TraitTwo "WildWasteland") -cne
    "FNVXR_AutoTTW_L1_FastShot_WildWasteland") {
    throw "TTW fixture save helper did not construct the separate owned save lineage."
}
if ((Assert-FnvxrProductTtwFixtureSaveName `
        -SaveName "FNVXR_AutoTTW_L1_FastShot_WildWasteland") -cne
    "FNVXR_AutoTTW_L1_FastShot_WildWasteland") {
    throw "TTW fixture save-name authority rejected its owned fixture."
}
try {
    Assert-FnvxrProductTtwFixtureSaveName -SaveName "FNVXR_StereoTest" | Out-Null
    throw "TTW fixture save-name authority accepted the historical save."
} catch {
    if (-not $_.Exception.Message.Contains("not owned")) {
        throw
    }
}

foreach ($required in @(
    'inline constexpr std::string_view TtwSaveNamePrefix',
    'FNVXR_AutoTTW_',
    'TtwStewieDependencyWarningTitle',
    'TtwStewieDependencyWarningBody',
    'exactTtwStewieDependencyAcknowledgementAuthorized')) {
    if (-not $authority.Contains($required)) {
        throw "Fixture authority lost TTW save-lineage contract: $required"
    }
}
if (-not $plugin.Contains(
        'return runProfileIs("retail-fixture-v1") || runProfileIs("ttw-fixture-v1");')) {
    throw "Plugin lost the TTW fixture profile contract."
}
foreach ($required in @(
    'retailFixtureTtwStewieDependencyAcknowledgementRequested',
    'FNVXR_RETAIL_FIXTURE_ACK_TTW_STEWIE_DEPENDENCY_WARNING',
    'findExactTtwStewieDependencyMessageMenuTarget',
    'acknowledgeExactTtwStewieDependencyMessageMenu',
    'closeExactRetailFixtureTtwStewieDependencyMessageMenu',
    'MaxExactTtwStewieDependencyCloseAttemptsPerRun')) {
    if (-not $plugin.Contains($required)) {
        throw "Plugin lost exact TTW dependency-warning acknowledgement: $required"
    }
}
foreach ($required in @(
    'bool physicalHeadsetFixtureMessageAcknowledgementRequested()',
    'FNVXR_PHYSICAL_HEADSET_PLAY',
    'bool ownedRetailFixtureMessageAcknowledgementRequested()',
    'void processOwnedRetailFixtureMessageMenuAcknowledgements(',
    'processOwnedRetailFixtureMessageMenuAcknowledgements(observation);')) {
    if (-not $plugin.Contains($required)) {
        throw "Physical fixture run lost its bounded TTW warning acknowledgement: $required"
    }
}
$physicalAcknowledgementBlock = [regex]::Match(
    $plugin,
    '(?s)if \(physicalHeadsetPlayProfileSelected\(\)\s*&&\s*physicalHeadsetFixtureMessageAcknowledgementRequested\(\)\)\s*\{.*?processOwnedRetailFixtureMessageMenuAcknowledgements\(observation\);.*?\}').Value
if ([string]::IsNullOrWhiteSpace($physicalAcknowledgementBlock) -or
    $physicalAcknowledgementBlock.Contains('processMainGameLoop(') -or
    $physicalAcknowledgementBlock.Contains('consumeSharedCommand(') -or
    $physicalAcknowledgementBlock.Contains('SendInput(') -or
    $physicalAcknowledgementBlock.Contains('dispatchMenuClick(')) {
    throw "Physical fixture acknowledgement must remain an exact native-message handler only."
}

# The visual headset route has a separate fixture-load helper. It must select
# the AutoTTW authority after -TtwCore has constrained the root/profile;
# otherwise a valid owned TTW fixture is rejected before the game starts.
foreach ($required in @(
    'function Invoke-FnvxrProductRetailFixtureLoad',
    '[switch]$TtwFixture',
    'Assert-FnvxrProductTtwFixtureSaveName -SaveName $SaveName',
    '-TtwFixture:$TtwCore')) {
    if (-not $productLauncher.Contains($required)) {
        throw "TTW headset fixture handoff lost required save-lineage contract: $required"
    }
}

Write-Output "fnvxr_ttw_fixture_launcher_test passed"
