param(
    [string]$GameRoot = "D:\SteamLibrary\steamapps\common\Fallout New Vegas",
    [ValidateRange(15, 75)][int]$TrialSeconds = 75,
    [switch]$UseAttestedBuild
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

$dash = @(Get-Process OculusDash -ErrorAction SilentlyContinue |
    Where-Object { -not $_.HasExited })
if ($dash.Count -eq 0) {
    throw "Quest Link is not active. In the headset, open Quest Link and press Launch before starting FNVXR."
}

Write-Host "Put on the headset and enter a loaded gameplay world. The run only passes after a real two-eye OpenXR frame is proven."

& (Join-Path $PSScriptRoot "start-fnvxr-product.ps1") `
    -Configuration Release `
    -GameRoot $GameRoot `
    -HostFrames 7200 `
    -MaximumRunSeconds $TrialSeconds `
    -RetailReadyTimeoutSeconds 60 `
    -UseAttestedBuild:$UseAttestedBuild
