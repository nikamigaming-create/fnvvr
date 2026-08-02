param(
    [Parameter(Mandatory = $true)][string]$SourceRoot
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

$scriptPath = Join-Path $SourceRoot "scripts\stage-meta-xr-operator.ps1"
if (-not (Test-Path -LiteralPath $scriptPath -PathType Leaf)) {
    throw "Meta XR Operator staging script is missing: $scriptPath"
}

$tokens = $null
$parseErrors = $null
[System.Management.Automation.Language.Parser]::ParseFile(
    $scriptPath,
    [ref]$tokens,
    [ref]$parseErrors) | Out-Null
if ($parseErrors.Count -ne 0) {
    throw "Meta XR Operator staging script has PowerShell parse errors."
}

$script = Get-Content -LiteralPath $scriptPath -Raw
foreach ($required in @(
    'XR_APILAYER_METAX_operator',
    'XrApiLayer_METAX_operator.dll',
    'meta-xr-operator-mcp-proxy.exe',
    'API layer manifest and DLL must stay side by side',
    'Meta XR Operator staging refuses to change dependencies while a runtime exists.',
    'Operator DestinationRoot must remain below the workspace-owned local dependency root',
    'processOrUiControl = $false',
    'inputOrSimulatorControl = $false')) {
    if (-not $script.Contains($required)) {
        throw "Meta XR Operator staging script lost required contract: $required"
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
    'XR_ENABLE_API_LAYERS',
    'XR_API_LAYER_PATH')) {
    if ($script.Contains($forbidden)) {
        throw "Meta XR Operator staging script must remain dependency-only: $forbidden"
    }
}

Write-Output "fnvxr_meta_xr_operator_staging_test passed"
