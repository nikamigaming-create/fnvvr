$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot "fnvxr-sidecar-common.ps1")
$SourceDirectories = @("host", "plugin", "protocol", "renderhook", "runtime", "scripts", "tests", "tools")
$src = Get-FnvxrBuildSourceSnapshot -Root $Root -SourceDirectories $SourceDirectories
$artDesc = Get-FnvxrRetailRuntimeArtifactDescriptors -Root $Root -Configuration "Release"
$art = Get-FnvxrArtifactContentSnapshot -Descriptors $artDesc
$tDesc = @(
    [ordered]@{ key = "x64"; buildDirectory = (Join-Path $Root "build"); configuration = "Release" },
    [ordered]@{ key = "x86"; buildDirectory = (Join-Path $Root "build-win32"); configuration = "Release" }
)
$tCat = Get-FnvxrCtestCatalogSnapshot -BuildDescriptors $tDesc
$nonce = [Guid]::NewGuid().ToString("N")
$attPath = Join-Path $Root "build\fnvxr-retail-build-attestation-Release.json"
Write-FnvxrBuildAttestation -Path $attPath -Root $Root -Configuration "Release" -Nonce $nonce -SourceSnapshot $src -ArtifactSnapshot $art -TestCatalogSnapshot $tCat
Write-Host "Attestation written successfully to $attPath"
