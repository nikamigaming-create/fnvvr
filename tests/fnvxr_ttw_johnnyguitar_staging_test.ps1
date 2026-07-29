param(
    [Parameter(Mandatory = $true)][string]$SourceRoot
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

$scriptPath = Join-Path $SourceRoot "scripts\stage-ttw-johnnyguitar.ps1"
if (-not (Test-Path -LiteralPath $scriptPath -PathType Leaf)) {
    throw "TTW JohnnyGuitar staging script is missing."
}
$tokens = $null
$parseErrors = $null
[System.Management.Automation.Language.Parser]::ParseFile(
    $scriptPath,
    [ref]$tokens,
    [ref]$parseErrors) | Out-Null
if ($parseErrors.Count -ne 0) {
    throw "TTW JohnnyGuitar staging script has parse errors."
}

$script = Get-Content -LiteralPath $scriptPath -Raw
foreach ($required in @(
    'JohnnyGuitarNVSE-5.28.zip',
    '5ab249afd70bfbc727659a81c2916c63508664b748f336289b2339894be830bc',
    'https://github.com/carxt/JohnnyGuitarNVSE/releases/download/5.28/JohnnyGuitarNVSE-5.28.zip',
    'Data\NVSE\Plugins\johnnyguitar.dll',
    'Data\NVSE\Plugins\JohnnyGuitar.ini',
    'A runtime is active; refusing to stage TTW dependencies into an in-use sandbox.',
    'Refusing to overwrite a different sandbox dependency')) {
    if (-not $script.Contains($required)) {
        throw "TTW JohnnyGuitar staging contract is incomplete: $required"
    }
}
foreach ($forbidden in @(
    'Start-Process',
    'Stop-Process',
    'SendInput',
    'SendKeys',
    'SetForegroundWindow',
    'SetCursorPos',
    'Documents\My Games\FalloutNV\Saves')) {
    if ($script.Contains($forbidden)) {
        throw "TTW JohnnyGuitar staging script exceeded its workspace-only authority: $forbidden"
    }
}

Write-Output "fnvxr_ttw_johnnyguitar_staging_test passed"
