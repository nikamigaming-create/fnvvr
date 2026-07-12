param(
    [string]$PinnedXnvseTag = "6.4.8",
    [string]$PinnedXnvseCommit = "062bccb15abd0397aaeb0a2cf58d7c3ca6140618",
    [string]$PinnedXnvseRuntimeSha256 = "80608d0fd437b56d2670ce47ecc749e6dcd8b69db263a2e6c2de5095b1a42534",
    [string]$PinnedOpenXrSdkTag = "release-1.1.60",
    [string]$PinnedOpenXrSdkCommit = "64f2b37c8c6da3d83c9b4d11865ba1fb752cb8ec",
    [string]$PinnedOpenXrSourceTag = "release-1.1.60",
    [string]$PinnedOpenXrSourceCommit = "c07ad64839653712190e05dbd8cf460e1d239513",
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$Deps = Join-Path $Root "deps"
$Downloads = Join-Path $Deps "downloads"
$Sources = Join-Path $Deps "sources"
$Tools = Join-Path $Deps "tools"

New-Item -ItemType Directory -Force -Path $Downloads, $Sources, $Tools | Out-Null

function Invoke-GitHubApi {
    param([string]$Uri)

    Invoke-RestMethod -Uri $Uri -Headers @{
        "User-Agent" = "fnvvr"
        "Accept" = "application/vnd.github+json"
    }
}

function Save-Url {
    param(
        [string]$Uri,
        [string]$OutFile
    )

    if ((Test-Path -LiteralPath $OutFile) -and -not $Force) {
        Write-Host "Already downloaded: $OutFile"
        return
    }

    Write-Host "Downloading $Uri"
    Invoke-WebRequest -Uri $Uri -OutFile $OutFile -Headers @{
        "User-Agent" = "fnvvr"
    }
}

function Resolve-GitHubTagCommit {
    param(
        [string]$Repository,
        [string]$Tag
    )

    $encodedTag = [Uri]::EscapeDataString($Tag)
    $ref = Invoke-GitHubApi "https://api.github.com/repos/$Repository/git/ref/tags/$encodedTag"
    $objectType = [string]$ref.object.type
    $objectSha = [string]$ref.object.sha
    if ($objectType -eq "tag") {
        $tagObject = Invoke-GitHubApi "https://api.github.com/repos/$Repository/git/tags/$objectSha"
        $objectType = [string]$tagObject.object.type
        $objectSha = [string]$tagObject.object.sha
    }
    if ($objectType -ne "commit" -or -not $objectSha) {
        throw "Tag $Repository@$Tag did not resolve to a commit."
    }
    return $objectSha.ToLowerInvariant()
}

function Assert-PinnedTagCommit {
    param(
        [string]$Repository,
        [string]$Tag,
        [string]$ExpectedCommit
    )

    $actualCommit = Resolve-GitHubTagCommit -Repository $Repository -Tag $Tag
    if ($actualCommit -ne $ExpectedCommit.ToLowerInvariant()) {
        throw "Pinned tag moved: $Repository@$Tag expected $ExpectedCommit, received $actualCommit"
    }
}

function Assert-Sha256 {
    param(
        [string]$Path,
        [string]$Expected
    )

    $actual = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $Expected.ToLowerInvariant()) {
        throw "SHA-256 mismatch for $Path. Expected $Expected, received $actual"
    }
}

function Expand-Zip {
    param(
        [string]$ZipPath,
        [string]$Destination
    )

    if ((Test-Path -LiteralPath $Destination) -and -not $Force) {
        Write-Host "Already extracted: $Destination"
        return
    }

    if (Test-Path -LiteralPath $Destination) {
        Remove-Item -LiteralPath $Destination -Recurse -Force
    }

    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    tar -xf $ZipPath -C $Destination
    if ($LASTEXITCODE -ne 0) {
        throw "tar failed extracting $ZipPath"
    }
}

function Expand-ArchiveWithTar {
    param(
        [string]$ArchivePath,
        [string]$Destination
    )

    if ((Test-Path -LiteralPath $Destination) -and -not $Force) {
        Write-Host "Already extracted: $Destination"
        return
    }

    if (Test-Path -LiteralPath $Destination) {
        Remove-Item -LiteralPath $Destination -Recurse -Force
    }

    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    tar -xf $ArchivePath -C $Destination
    if ($LASTEXITCODE -ne 0) {
        throw "tar failed extracting $ArchivePath"
    }
}

$manifest = [ordered]@{
    fetchedAt = (Get-Date).ToString("o")
    dependencies = @()
}

Assert-PinnedTagCommit -Repository "xNVSE/NVSE" -Tag $PinnedXnvseTag -ExpectedCommit $PinnedXnvseCommit
$xnvseRelease = Invoke-GitHubApi ("https://api.github.com/repos/xNVSE/NVSE/releases/tags/{0}" -f [Uri]::EscapeDataString($PinnedXnvseTag))
$xnvseTag = $xnvseRelease.tag_name
if ($xnvseTag -ne $PinnedXnvseTag) {
    throw "xNVSE release tag mismatch: requested $PinnedXnvseTag, received $xnvseTag"
}
$xnvseRuntimeAsset = $xnvseRelease.assets | Where-Object { $_.name -match '^nvse_.*\.7z$' } | Select-Object -First 1
if (-not $xnvseRuntimeAsset) {
    throw "Could not find xNVSE runtime .7z asset in pinned release $xnvseTag"
}

$xnvseCommitShort = $PinnedXnvseCommit.Substring(0, 12)
$xnvseRuntimeArchive = Join-Path $Downloads $xnvseRuntimeAsset.name
$xnvseSourceZip = Join-Path $Downloads "xnvse-NVSE-$xnvseTag-$xnvseCommitShort-source.zip"

Save-Url $xnvseRuntimeAsset.browser_download_url $xnvseRuntimeArchive
Assert-Sha256 -Path $xnvseRuntimeArchive -Expected $PinnedXnvseRuntimeSha256
Save-Url "https://api.github.com/repos/xNVSE/NVSE/zipball/$PinnedXnvseCommit" $xnvseSourceZip
Expand-Zip $xnvseSourceZip (Join-Path $Sources "xnvse-source-$xnvseTag-$xnvseCommitShort")

$xnvseRuntimeDir = Join-Path $Tools "xnvse-runtime-$xnvseTag-$xnvseCommitShort"
Expand-ArchiveWithTar $xnvseRuntimeArchive $xnvseRuntimeDir

$manifest.dependencies += [ordered]@{
    name = "xNVSE"
    tag = $xnvseTag
    commit = $PinnedXnvseCommit
    releaseUrl = $xnvseRelease.html_url
    runtimeArchive = $xnvseRuntimeArchive
    runtimeSha256 = $PinnedXnvseRuntimeSha256
    runtimeDir = $xnvseRuntimeDir
    sourceZip = $xnvseSourceZip
    sourceDir = (Join-Path $Sources "xnvse-source-$xnvseTag-$xnvseCommitShort")
}

Assert-PinnedTagCommit -Repository "KhronosGroup/OpenXR-SDK" -Tag $PinnedOpenXrSdkTag -ExpectedCommit $PinnedOpenXrSdkCommit
$openxrSdkRelease = Invoke-GitHubApi ("https://api.github.com/repos/KhronosGroup/OpenXR-SDK/releases/tags/{0}" -f [Uri]::EscapeDataString($PinnedOpenXrSdkTag))
$openxrSdkTag = $openxrSdkRelease.tag_name
if ($openxrSdkTag -ne $PinnedOpenXrSdkTag) {
    throw "OpenXR SDK release tag mismatch: requested $PinnedOpenXrSdkTag, received $openxrSdkTag"
}
$openxrSdkCommitShort = $PinnedOpenXrSdkCommit.Substring(0, 12)
$openxrSdkZip = Join-Path $Downloads "OpenXR-SDK-$openxrSdkTag-$openxrSdkCommitShort-source.zip"
$openxrSdkDir = Join-Path $Sources "OpenXR-SDK-$openxrSdkTag-$openxrSdkCommitShort"
Save-Url "https://api.github.com/repos/KhronosGroup/OpenXR-SDK/zipball/$PinnedOpenXrSdkCommit" $openxrSdkZip
Expand-Zip $openxrSdkZip $openxrSdkDir

$manifest.dependencies += [ordered]@{
    name = "OpenXR-SDK"
    tag = $openxrSdkTag
    commit = $PinnedOpenXrSdkCommit
    releaseUrl = $openxrSdkRelease.html_url
    sourceZip = $openxrSdkZip
    sourceDir = $openxrSdkDir
}

Assert-PinnedTagCommit -Repository "KhronosGroup/OpenXR-SDK-Source" -Tag $PinnedOpenXrSourceTag -ExpectedCommit $PinnedOpenXrSourceCommit
$openxrSourceRelease = Invoke-GitHubApi ("https://api.github.com/repos/KhronosGroup/OpenXR-SDK-Source/releases/tags/{0}" -f [Uri]::EscapeDataString($PinnedOpenXrSourceTag))
$openxrSourceTag = $openxrSourceRelease.tag_name
if ($openxrSourceTag -ne $PinnedOpenXrSourceTag) {
    throw "OpenXR SDK Source release tag mismatch: requested $PinnedOpenXrSourceTag, received $openxrSourceTag"
}
$openxrSourceCommitShort = $PinnedOpenXrSourceCommit.Substring(0, 12)
$openxrSourceZip = Join-Path $Downloads "OpenXR-SDK-Source-$openxrSourceTag-$openxrSourceCommitShort-source.zip"
$openxrSourceDir = Join-Path $Sources "OpenXR-SDK-Source-$openxrSourceTag-$openxrSourceCommitShort"
Save-Url "https://api.github.com/repos/KhronosGroup/OpenXR-SDK-Source/zipball/$PinnedOpenXrSourceCommit" $openxrSourceZip
Expand-Zip $openxrSourceZip $openxrSourceDir

$manifest.dependencies += [ordered]@{
    name = "OpenXR-SDK-Source"
    tag = $openxrSourceTag
    commit = $PinnedOpenXrSourceCommit
    releaseUrl = $openxrSourceRelease.html_url
    sourceZip = $openxrSourceZip
    sourceDir = $openxrSourceDir
}

$manifestPath = Join-Path $Deps "manifest.json"
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

Write-Host "Wrote $manifestPath"
