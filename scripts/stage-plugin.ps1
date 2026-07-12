param(
    [string]$Configuration = "Debug",
    [switch]$InstallToGame
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $Root "build-win32"
$PluginDll = Join-Path $BuildDir "$Configuration\nvse_fnvxr.dll"
$PluginPdb = Join-Path $BuildDir "$Configuration\nvse_fnvxr.pdb"

if (-not (Test-Path -LiteralPath $PluginDll)) {
    & (Join-Path $PSScriptRoot "build-win32.ps1") -Configuration $Configuration
}

if (-not (Test-Path -LiteralPath $PluginDll)) {
    throw "Missing plugin DLL after build: $PluginDll"
}

$probePath = Join-Path $Root "local\fnv-probe.json"
if (-not (Test-Path -LiteralPath $probePath)) {
    & (Join-Path $PSScriptRoot "probe-local-fnv.ps1") | Out-Null
}

$probe = Get-Content -Raw -LiteralPath $probePath | ConvertFrom-Json
$game = $probe.candidates | Where-Object {
    $_.falloutNVExeExists -and $_.dataDirExists -and $_.falloutNVESMExists
} | Select-Object -First 1

$StageRoot = Join-Path $Root "local\fnv-plugin-stage"
$StagePluginDir = Join-Path $StageRoot "Data\NVSE\Plugins"
New-Item -ItemType Directory -Force -Path $StagePluginDir | Out-Null

Copy-Item -LiteralPath $PluginDll -Destination (Join-Path $StagePluginDir "nvse_fnvxr.dll") -Force
if (Test-Path -LiteralPath $PluginPdb) {
    Copy-Item -LiteralPath $PluginPdb -Destination (Join-Path $StagePluginDir "nvse_fnvxr.pdb") -Force
}

$installedToGame = $false
if ($InstallToGame) {
    if (-not $game) {
        throw "No valid FNV game root found; cannot install"
    }

    $livePluginDir = Join-Path $game.root "Data\NVSE\Plugins"
    New-Item -ItemType Directory -Force -Path $livePluginDir | Out-Null
    Copy-Item -LiteralPath $PluginDll -Destination (Join-Path $livePluginDir "nvse_fnvxr.dll") -Force
    if (Test-Path -LiteralPath $PluginPdb) {
        Copy-Item -LiteralPath $PluginPdb -Destination (Join-Path $livePluginDir "nvse_fnvxr.pdb") -Force
    }
    $installedToGame = $true
}

$manifest = [ordered]@{
    stagedAt = (Get-Date).ToString("o")
    configuration = $Configuration
    pluginDll = $PluginDll
    stagedPluginDir = $StagePluginDir
    detectedGameRoot = if ($game) { $game.root } else { $null }
    installedToGame = $installedToGame
}

$manifestPath = Join-Path $StageRoot "stage-manifest.json"
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
$manifest | ConvertTo-Json -Depth 5
