param(
    [string]$SourceRoot = "D:\SteamLibrary\steamapps\common\Fallout New Vegas",
    [string]$SandboxRoot = "",
    [switch]$ValidateOnly
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

. (Join-Path $PSScriptRoot "fnvxr-product-common.ps1")

$root = Get-FnvxrProductRoot
if ([string]::IsNullOrWhiteSpace($SandboxRoot)) {
    $SandboxRoot = Join-Path $root "local\retail-sandbox-v1"
}
$source = [System.IO.Path]::GetFullPath($SourceRoot)
$sandbox = [System.IO.Path]::GetFullPath($SandboxRoot)
$allowedParent = [System.IO.Path]::GetFullPath((Join-Path $root "local"))
$allowedPrefix = $allowedParent.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
if (-not $sandbox.StartsWith(
        $allowedPrefix,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Retail sandbox must stay below the workspace local directory: $allowedParent"
}
if ([string]::Equals(
        $source,
        $sandbox,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Retail sandbox source and destination must be distinct."
}
if (-not (Test-Path -LiteralPath $source -PathType Container)) {
    throw "Retail source root is missing: $source"
}

$copiedFiles = @(
    "FalloutNV.exe",
    "nvse_loader.exe",
    "nvse_1_4.dll",
    "nvse_steam_loader.dll",
    "steam_api.dll",
    "binkw32.dll",
    "atimgpud.dll",
    "GDFFalloutNV.dll",
    "libvorbis.dll",
    "libvorbisfile.dll",
    "Fallout_default.ini",
    "MainTitle.wav",
    "Data\NVSE\Plugins\jip_nvse.dll",
    "Data\NVSE\Plugins\jip_nvse.ini",
    "Data\NVSE\Plugins\ShowOffNVSE.dll"
)
$hardLinkedDataFiles = @(
    "Data\CaravanPack - Main.bsa",
    "Data\CaravanPack.esm",
    "Data\CaravanPack.nam",
    "Data\ClassicPack - Main.bsa",
    "Data\ClassicPack.esm",
    "Data\ClassicPack.nam",
    "Data\DeadMoney - Main.bsa",
    "Data\DeadMoney - Sounds.bsa",
    "Data\DeadMoney.esm",
    "Data\DEADMONEY.NAM",
    "Data\Fallout - Meshes.bsa",
    "Data\Fallout - Misc.bsa",
    "Data\Fallout - Sound.bsa",
    "Data\Fallout - Textures.bsa",
    "Data\Fallout - Textures2.bsa",
    "Data\Fallout - Voices1.bsa",
    "Data\FalloutNV.esm",
    "Data\GunRunnersArsenal - Main.bsa",
    "Data\GunRunnersArsenal - Sounds.bsa",
    "Data\GunRunnersArsenal.esm",
    "Data\GUNRUNNERSARSENAL.NAM",
    "Data\HonestHearts - Main.bsa",
    "Data\HonestHearts - Sounds.bsa",
    "Data\HonestHearts.esm",
    "Data\HONESTHEARTS.NAM",
    "Data\LonesomeRoad - Main.bsa",
    "Data\LonesomeRoad - Sounds.bsa",
    "Data\LonesomeRoad.esm",
    "Data\LONESOMEROAD.NAM",
    "Data\MercenaryPack - Main.bsa",
    "Data\MercenaryPack.esm",
    "Data\MercenaryPack.nam",
    "Data\OldWorldBlues - Main.bsa",
    "Data\OldWorldBlues - Sounds.bsa",
    "Data\OldWorldBlues.esm",
    "Data\OLDWORLDBLUES.NAM",
    "Data\Shaders\shaderpackage002.sdp",
    "Data\Shaders\shaderpackage003.sdp",
    "Data\Shaders\shaderpackage004.sdp",
    "Data\Shaders\shaderpackage006.sdp",
    "Data\Shaders\shaderpackage007.sdp",
    "Data\Shaders\shaderpackage009.sdp",
    "Data\Shaders\shaderpackage010.sdp",
    "Data\Shaders\shaderpackage011.sdp",
    "Data\Shaders\shaderpackage012.sdp",
    "Data\Shaders\shaderpackage013.sdp",
    "Data\Shaders\shaderpackage014.sdp",
    "Data\Shaders\shaderpackage015.sdp",
    "Data\Shaders\shaderpackage016.sdp",
    "Data\Shaders\shaderpackage017.sdp",
    "Data\Shaders\shaderpackage018.sdp",
    "Data\Shaders\shaderpackage019.sdp",
    "Data\TribalPack - Main.bsa",
    "Data\TribalPack.esm",
    "Data\TribalPack.nam",
    "Data\Update.bsa",
    "Data\Video\FNVIntro.bik"
)

function Assert-SourceFile {
    param([Parameter(Mandatory = $true)][string]$RelativePath)
    $path = Join-Path $source $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required retail source file is missing: $path"
    }
    return $path
}

if (-not $ValidateOnly -and
    -not (Test-Path -LiteralPath $sandbox -PathType Container)) {
    New-Item -ItemType Directory -Path $sandbox -Force | Out-Null
}
if (-not (Test-Path -LiteralPath $sandbox -PathType Container)) {
    throw "Retail sandbox is missing: $sandbox"
}

foreach ($relative in $copiedFiles) {
    $sourcePath = Assert-SourceFile -RelativePath $relative
    $destinationPath = Join-Path $sandbox $relative
    if (-not (Test-Path -LiteralPath $destinationPath -PathType Leaf)) {
        if ($ValidateOnly) {
            throw "Copied retail sandbox file is missing: $destinationPath"
        }
        New-Item -ItemType Directory -Path (Split-Path -Parent $destinationPath) `
            -Force | Out-Null
        Copy-Item -LiteralPath $sourcePath -Destination $destinationPath
    }
    $sourceIdentity = Get-FnvxrProductFileIdentity -Path $sourcePath
    $destinationIdentity = Get-FnvxrProductFileIdentity -Path $destinationPath
    if ($sourceIdentity.length -ne $destinationIdentity.length -or
        $sourceIdentity.sha256 -cne $destinationIdentity.sha256) {
        throw "Copied retail sandbox file differs from its source: $relative"
    }
}

foreach ($relative in $hardLinkedDataFiles) {
    $sourcePath = Assert-SourceFile -RelativePath $relative
    $destinationPath = Join-Path $sandbox $relative
    if (-not (Test-Path -LiteralPath $destinationPath -PathType Leaf)) {
        if ($ValidateOnly) {
            throw "Hard-linked retail sandbox file is missing: $destinationPath"
        }
        New-Item -ItemType Directory -Path (Split-Path -Parent $destinationPath) `
            -Force | Out-Null
        New-Item -ItemType HardLink -Path $destinationPath -Target $sourcePath |
            Out-Null
    }
    $sourceItem = Get-Item -LiteralPath $sourcePath
    $destinationItem = Get-Item -LiteralPath $destinationPath
    if ($sourceItem.Length -ne $destinationItem.Length -or
        [string]$destinationItem.LinkType -cne "HardLink") {
        throw "Retail data file is not the expected source-backed hard link: $relative"
    }
}

$steamAppIdPath = Join-Path $sandbox "steam_appid.txt"
if (-not (Test-Path -LiteralPath $steamAppIdPath -PathType Leaf)) {
    if ($ValidateOnly) {
        throw "Steam redirect suppression file is missing: $steamAppIdPath"
    }
    [System.IO.File]::WriteAllText(
        $steamAppIdPath,
        "22380`n",
        [System.Text.UTF8Encoding]::new($false))
}
if ((Get-Content -LiteralPath $steamAppIdPath -Raw).Trim() -cne "22380") {
    throw "steam_appid.txt must contain only Fallout: New Vegas AppID 22380."
}

$manifestPath = Join-Path $sandbox "fnvxr-retail-sandbox-manifest.json"
if (-not $ValidateOnly) {
    $fallout = Get-FnvxrProductFileIdentity `
        -Path (Join-Path $sandbox "FalloutNV.exe") `
        -RequirePe
    $manifest = [ordered]@{
        schema = "fnvxr-retail-sandbox/v1"
        createdAtUtc = [DateTime]::UtcNow.ToString("o")
        sandboxRoot = $sandbox
        sourceRoot = $source
        scope = "workspace-owned retail executable/runtime copies plus source-backed official Data hard links; FNVXR stages only inside the sandbox"
        copiedMutableFiles = $copiedFiles.Count
        hardLinkedReadOnlyDataFiles = $hardLinkedDataFiles.Count
        steamAppId = "22380"
        steamLibraryRedirectForbidden = $true
        sourceRootsMutated = $false
        processOrUiControl = $false
        falloutSha256 = $fallout.sha256
    }
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
}
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Retail sandbox manifest is missing: $manifestPath"
}

$validated = Assert-FnvxrProductGameRoot -GameRoot $sandbox
[pscustomobject][ordered]@{
    schema = "fnvxr-retail-sandbox-preparation/v1"
    validated = $true
    validateOnly = [bool]$ValidateOnly
    sandboxRoot = $validated.root
    sourceRoot = $validated.sandbox.sourceRoot
    copiedMutableFiles = $copiedFiles.Count
    hardLinkedReadOnlyDataFiles = $hardLinkedDataFiles.Count
    steamAppId = $validated.sandbox.steamAppId
    steamLibraryRedirectForbidden = $true
    sourceRootsMutated = $false
    processOrUiControl = $false
} | ConvertTo-Json -Depth 5
