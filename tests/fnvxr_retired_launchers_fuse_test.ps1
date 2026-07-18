param(
    [Parameter(Mandatory = $true)][string]$SourceRoot
)

$ErrorActionPreference = "Stop"

$powerShellExe = Join-Path $PSHOME "powershell.exe"
$retailLauncher = Join-Path $SourceRoot "scripts\start-retail-surface-producer.ps1"
$pluginStage = Join-Path $SourceRoot "scripts\stage-plugin.ps1"
$retailRunRoot = Join-Path $SourceRoot "local\retail-sidecar-runs"
$openXrRunRoot = Join-Path $SourceRoot "local\openxr-retail-sidecar-runs"

$beforeRunCount = @(
    Get-ChildItem -LiteralPath $retailRunRoot -Directory -ErrorAction SilentlyContinue
).Count
$savedErrorActionPreference = $ErrorActionPreference
$ErrorActionPreference = "Continue"
$retailOutput = (& $powerShellExe `
    -NoProfile `
    -ExecutionPolicy Bypass `
    -File $retailLauncher 2>&1 | Out-String)
$retailExit = $LASTEXITCODE
$ErrorActionPreference = $savedErrorActionPreference
$afterRunCount = @(
    Get-ChildItem -LiteralPath $retailRunRoot -Directory -ErrorAction SilentlyContinue
).Count
if ($retailExit -ne 1 -or
    -not $retailOutput.Contains("Legacy flat retail surface producer is retired.") -or
    $afterRunCount -ne $beforeRunCount) {
    throw "Legacy retail fuse failed: exit=$retailExit before=$beforeRunCount after=$afterRunCount output=$retailOutput"
}

$ErrorActionPreference = "Continue"
$pluginOutput = (& $powerShellExe `
    -NoProfile `
    -ExecutionPolicy Bypass `
    -File $pluginStage `
    -InstallToGame 2>&1 | Out-String)
$pluginExit = $LASTEXITCODE
$ErrorActionPreference = $savedErrorActionPreference
if ($pluginExit -ne 1 -or
    -not $pluginOutput.Contains("Direct plugin-only game installation is retired.")) {
    throw "Direct plugin-install fuse failed: exit=$pluginExit output=$pluginOutput"
}

$beforeOpenXrRunCount = @(
    Get-ChildItem -LiteralPath $openXrRunRoot -Directory -ErrorAction SilentlyContinue
).Count
$ErrorActionPreference = "Continue"
$validateOutput = (& $powerShellExe `
    -NoProfile `
    -ExecutionPolicy Bypass `
    -File (Join-Path $SourceRoot "scripts\start-openxr-retail-sidecar.ps1") `
    -ValidateOnly `
    -StopExisting 2>&1 | Out-String)
$validateExit = $LASTEXITCODE
$ErrorActionPreference = $savedErrorActionPreference
$afterOpenXrRunCount = @(
    Get-ChildItem -LiteralPath $openXrRunRoot -Directory -ErrorAction SilentlyContinue
).Count
if ($validateExit -ne 1 -or
    -not $validateOutput.Contains("-ValidateOnly is read-only with respect to running processes") -or
    $afterOpenXrRunCount -ne $beforeOpenXrRunCount) {
    throw "Validate-only process fuse failed: exit=$validateExit before=$beforeOpenXrRunCount after=$afterOpenXrRunCount output=$validateOutput"
}
