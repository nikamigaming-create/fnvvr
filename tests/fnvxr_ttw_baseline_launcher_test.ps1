param(
    [Parameter(Mandatory = $true)][string]$SourceRoot
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

. (Join-Path $SourceRoot "scripts\fnvxr-product-common.ps1")

$launcherPath = Join-Path $SourceRoot "scripts\start-fnvxr-ttw-baseline.ps1"
$commonPath = Join-Path $SourceRoot "scripts\fnvxr-product-common.ps1"
$pluginPath = Join-Path $SourceRoot "plugin\fnvxr_nvse_plugin.cpp"
foreach ($path in @($launcherPath, $commonPath, $pluginPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "TTW baseline contract path is missing: $path"
    }
}

foreach ($path in @($launcherPath, $commonPath)) {
    $tokens = $null
    $parseErrors = $null
    [System.Management.Automation.Language.Parser]::ParseFile(
        $path,
        [ref]$tokens,
        [ref]$parseErrors) | Out-Null
    if ($parseErrors.Count -ne 0) {
        throw "TTW baseline PowerShell contract has parse errors: $path"
    }
}

$expectedPlugins = @(
    "FalloutNV.esm",
    "DeadMoney.esm",
    "HonestHearts.esm",
    "OldWorldBlues.esm",
    "LonesomeRoad.esm",
    "GunRunnersArsenal.esm",
    "Fallout3.esm",
    "Anchorage.esm",
    "ThePitt.esm",
    "BrokenSteel.esm",
    "PointLookout.esm",
    "Zeta.esm",
    "CaravanPack.esm",
    "ClassicPack.esm",
    "MercenaryPack.esm",
    "TribalPack.esm",
    "TaleOfTwoWastelands.esm",
    "YUPTTW.esm")
if ((@(Get-FnvxrProductTtwBaselinePluginNames) -join "|") -cne
    ($expectedPlugins -join "|")) {
    throw "TTW baseline plugin order does not match the exact core profile."
}

$launcher = Get-Content -LiteralPath $launcherPath -Raw
$common = Get-Content -LiteralPath $commonPath -Raw
$plugin = Get-Content -LiteralPath $pluginPath -Raw
foreach ($required in @(
    'schema = "fnvxr-ttw-baseline-v1"',
    'TTW baseline GameRoot must be an isolated workspace sandbox below:',
    'A pre-existing runtime is present; refusing to attach to, control, or stop it.',
    'Install-FnvxrProductTtwBaselinePluginProfile',
    'Restore-FnvxrProductTtwBaselinePluginProfile',
    'Get-FnvxrProductRetailFixtureStagePlan',
    'Wait-FnvxrProductRetailFixtureStartMenu',
    'FNVXR_RUN_PROFILE = "ttw-baseline-v1"',
    'FNVXR_DISABLE_BRIDGE = "1"',
    'FNVXR_DISABLE_STEREO_WORLD = "1"',
    'FNVXR_ENABLE_ENGINE_CENTER_STEREO = "0"',
    '-WindowStyle Hidden',
    'XR_API_LAYER_PATH',
    'XR_ENABLE_API_LAYERS',
    'saveAction = "none"',
    'noOpenXrOrSimulator = $true')) {
    if (-not $launcher.Contains($required)) {
        throw "TTW baseline launcher lost required isolation contract: $required"
    }
}
foreach ($required in @(
    'function Get-FnvxrProductTtwBaselinePluginNames',
    'function Assert-FnvxrProductTtwBaselinePluginData',
    'function Install-FnvxrProductTtwBaselinePluginProfile',
    'function Restore-FnvxrProductTtwBaselinePluginProfile',
    'TTW baseline plugin-profile restore hash mismatch')) {
    if (-not $common.Contains($required)) {
        throw "TTW baseline common helper is incomplete: $required"
    }
}
foreach ($forbidden in @(
    'fnvxr_command.exe',
    'HeadlessSimulatorManifest',
    'fnvxr_openxr_pose_host.exe',
    'd3d9.dll',
    'dinput8.dll',
    'xinput1_3.dll',
    'SendInput',
    'SendKeys',
    'keybd_event',
    'mouse_event',
    'SetForegroundWindow',
    'SetCursorPos',
    'Get-FnvxrProductDocumentsPath')) {
    if ($launcher.Contains($forbidden)) {
        throw "TTW baseline launcher must remain menu-observation-only: $forbidden"
    }
}

foreach ($required in @(
    'bool ttwBaselineProfileSelected()',
    'runProfileIs("ttw-baseline-v1")',
    'TTW baseline runtime publication ready; menu observation only',
    'TTW baseline profile selected: Start Menu observation only')) {
    if (-not $plugin.Contains($required)) {
        throw "Plugin lost TTW baseline publication-only contract: $required"
    }
}
$baselineBranch = [regex]::Match(
    $plugin,
    '(?s)if \(ttwBaselineProfileSelected\(\)\)\s*\{.*?(?=\s*if \(retailFixtureProfileSelected\(\))').Value
if ([string]::IsNullOrWhiteSpace($baselineBranch)) {
    throw "Could not isolate the TTW baseline publication-only main-loop branch."
}
foreach ($forbidden in @(
    'processRetailFixtureAutomation(',
    'ensureAuthorizedSharedBridgeStarted(',
    'ensureAuthorizedDesktopAssistBridgeStarted(',
    'ensureAuthorizedTrackedPropAssistBridgeStarted(',
    'processMainGameLoop(',
    'SendInput(',
    'tapKey(',
    'installCameraHook(',
    'installRetailRigHook(')) {
    if ($baselineBranch.Contains($forbidden)) {
        throw "TTW baseline publication branch gained prohibited authority: $forbidden"
    }
}
if (-not $baselineBranch.Contains('return;')) {
    throw "TTW baseline publication branch must return before generic bridge work."
}

Write-Output "fnvxr_ttw_baseline_launcher_test passed"
