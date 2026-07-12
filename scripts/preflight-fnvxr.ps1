$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$LocalDir = Join-Path $Root "local"
New-Item -ItemType Directory -Force -Path $LocalDir | Out-Null
$stepPath = Join-Path $LocalDir "preflight-step.txt"
Remove-Item -LiteralPath $stepPath -Force -ErrorAction SilentlyContinue
function Write-Step {
    param([string]$Step)
    Add-Content -LiteralPath $stepPath -Value $Step
}

Write-Step "start"
& (Join-Path $PSScriptRoot "probe-local-fnv.ps1") | Out-Null
Write-Step "after-fnv-probe"

$depsManifestPath = Join-Path $Root "deps\manifest.json"
$fnvProbePath = Join-Path $LocalDir "fnv-probe.json"
$win32Plugin = Join-Path $Root "build-win32\Debug\nvse_fnvxr.dll"
$x64Plugin = Join-Path $Root "build\Debug\nvse_fnvxr.dll"
$openXrStep = Join-Path $LocalDir "openxr-probe-step.txt"

Write-Step "before-registry"
$openXrRegistry = Get-ItemProperty -Path "HKLM:\SOFTWARE\Khronos\OpenXR\1","HKCU:\SOFTWARE\Khronos\OpenXR\1" -ErrorAction SilentlyContinue
$openXrActiveRuntime = @($openXrRegistry | ForEach-Object { $_.ActiveRuntime } | Where-Object { $_ } | Select-Object -First 1)
Write-Step "after-registry"
$availableRuntimeNames = @()
foreach ($runtimeKey in @("HKLM:\SOFTWARE\Khronos\OpenXR\1\AvailableRuntimes", "HKCU:\SOFTWARE\Khronos\OpenXR\1\AvailableRuntimes")) {
    if (Test-Path $runtimeKey) {
        $item = Get-Item -Path $runtimeKey
        $availableRuntimeNames += $item.GetValueNames()
    }
}
Write-Step "after-available-runtimes"

$openXrProbeSteps = if (Test-Path -LiteralPath $openXrStep) { @(Get-Content -LiteralPath $openXrStep) } else { @() }
Write-Step "after-files"

function JsonString {
    param([AllowNull()][string]$Value)
    if ($null -eq $Value) { return "null" }
    $escaped = $Value.Replace("\", "\\").Replace('"', '\"').Replace("`r", "").Replace("`n", "\n")
    return '"' + $escaped + '"'
}

function JsonBool {
    param([bool]$Value)
    if ($Value) { return "true" }
    return "false"
}

function JsonArray {
    param([string[]]$Values)
    return "[" + (($Values | ForEach-Object { JsonString $_ }) -join ", ") + "]"
}

$activeRuntime = if ($openXrActiveRuntime.Count -gt 0) { $openXrActiveRuntime[0] } else { $null }
Write-Step "after-object"

$outPath = Join-Path $LocalDir "preflight-fnvxr.json"
$json = @"
{
  "checkedAt": $(JsonString ((Get-Date).ToString("o"))),
  "depsManifestExists": $(JsonBool (Test-Path -LiteralPath $depsManifestPath)),
  "depsManifestPath": $(JsonString $depsManifestPath),
  "fnvProbePath": $(JsonString $fnvProbePath),
  "win32PluginExists": $(JsonBool (Test-Path -LiteralPath $win32Plugin)),
  "win32Plugin": $(JsonString $win32Plugin),
  "x64PluginExists": $(JsonBool (Test-Path -LiteralPath $x64Plugin)),
  "x64Plugin": $(JsonString $x64Plugin),
  "openXrActiveRuntime": $(JsonString $activeRuntime),
  "openXrAvailableRuntimeKeys": $(JsonArray @($availableRuntimeNames)),
  "openXrProbeSteps": $(JsonArray @($openXrProbeSteps))
}
"@

$json | Set-Content -LiteralPath $outPath -Encoding UTF8
Write-Step "after-write"
$json
Write-Step "after-print"
