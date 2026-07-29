param(
    [Parameter(Mandatory = $true)][string]$SourceRoot
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

$scriptPath = Join-Path $SourceRoot "scripts\prepare-ttw-retail-sandbox.ps1"
if (-not (Test-Path -LiteralPath $scriptPath -PathType Leaf)) {
    throw "TTW retail sandbox preparation script is missing: $scriptPath"
}

$tokens = $null
$parseErrors = $null
[System.Management.Automation.Language.Parser]::ParseFile(
    $scriptPath,
    [ref]$tokens,
    [ref]$parseErrors) | Out-Null
if ($parseErrors.Count -ne 0) {
    throw "TTW retail sandbox preparation script has PowerShell parse errors."
}

$script = Get-Content -LiteralPath $scriptPath -Raw
foreach ($required in @(
    'SandboxRoot must remain inside the repository local directory',
    'Assert-FnvxrProductGameRoot -GameRoot $retailSource',
    'ttw-core-baseline-v1',
    'TaleOfTwoWastelands.esm',
    'YUPTTW.esm',
    'New-Item -ItemType HardLink',
    'Refusing a copy fallback',
    'Sandbox target already exists; refusing to overwrite, merge, or delete it',
    'RepairMissingRetailData',
    'Retail-data repair refuses to modify the sandbox while a runtime exists.',
    'Fallout - Voices1.bsa',
    'Update.bsa',
    'sourceRootsMutated = $false',
    'processOrUiControl = $false')) {
    if (-not $script.Contains($required)) {
        throw "TTW sandbox script lost required isolation contract: $required"
    }
}

foreach ($forbidden in @(
    'Start-Process',
    'Stop-Process',
    'SendInput',
    'SendKeys',
    'keybd_event',
    'mouse_event',
    'SetForegroundWindow',
    'SetCursorPos',
    'Remove-Item',
    'Get-FnvxrProductDocumentsPath',
    'Get-FnvxrProductRetailVisualTrialPluginsPath')) {
    if ($script.Contains($forbidden)) {
        throw "TTW sandbox script must not gain process, UI, input, destructive cleanup, or user-profile authority: $forbidden"
    }
}

if ($script.Contains('FNVR.esp')) {
    throw "TTW sandbox script must not bring an active root ESP into the isolated baseline."
}

Write-Output "fnvxr_ttw_retail_sandbox_script_test passed"
