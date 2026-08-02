param(
    [string]$SandboxRoot = "",
    [string]$RetailSourceRoot = "D:\SteamLibrary\steamapps\common\Fallout New Vegas",
    [string]$TtwDataRoot = "D:\Modlists\fnv\mods\Tale of Two Wastelands - OpenMW",
    # Add only missing official retail Data assets to an already-owned
    # sandbox. It never overwrites or deletes existing files.
    [switch]$RepairMissingRetailData,
    [switch]$ValidateOnly
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

# This script creates a disposable FNV root under this repository.  It never
# stages, launches, controls, or stops a process, and it never touches the
# retail game root, the user's mod manager, LocalAppData, or Documents saves.
#
# Large game and TTW assets are NTFS hard links, so the sandbox has its own
# names and staging locations without making another 17+ GB copy.  Mutable
# configuration files are copied instead of linked.  The product supervisor
# may later stage its own files into this sandbox only.

. (Join-Path $PSScriptRoot "fnvxr-product-common.ps1")

$repositoryRoot = (Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$localRoot = [System.IO.Path]::GetFullPath((Join-Path $repositoryRoot "local"))
if ([string]::IsNullOrWhiteSpace($SandboxRoot)) {
    $SandboxRoot = Join-Path $localRoot "ttw-retail-sandbox"
}

function Resolve-FnvxrTtwSandboxPath {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Description
    )

    if ([string]::IsNullOrWhiteSpace($Path) -or
        -not [System.IO.Path]::IsPathRooted($Path)) {
        throw "$Description must be an absolute path."
    }
    return [System.IO.Path]::GetFullPath($Path)
}

function Test-FnvxrTtwPathIsWithin {
    param(
        [Parameter(Mandatory = $true)][string]$Child,
        [Parameter(Mandatory = $true)][string]$Parent
    )

    $normalizedParent = $Parent.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    return $Child.StartsWith(
        $normalizedParent,
        [System.StringComparison]::OrdinalIgnoreCase)
}

function Assert-FnvxrTtwRequiredFile {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Description
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Description is missing: $Path"
    }
}

function New-FnvxrTtwHardLink {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    $sourceRoot = [System.IO.Path]::GetPathRoot($Source)
    $destinationRoot = [System.IO.Path]::GetPathRoot($Destination)
    if (-not [string]::Equals(
            $sourceRoot,
            $destinationRoot,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing a copy fallback: hard-link source and destination are on different volumes: $Source -> $Destination"
    }
    $directory = Split-Path -Parent $Destination
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    New-Item -ItemType HardLink -Path $Destination -Target $Source -ErrorAction Stop | Out-Null
    $sourceLength = (Get-Item -LiteralPath $Source -Force).Length
    $destinationLength = (Get-Item -LiteralPath $Destination -Force).Length
    if ($sourceLength -ne $destinationLength) {
        throw "Hard-link length mismatch: $Source -> $Destination"
    }
}

function Copy-FnvxrTtwMutableFile {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    $directory = Split-Path -Parent $Destination
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    Copy-Item -LiteralPath $Source -Destination $Destination -ErrorAction Stop
    $sourceLength = (Get-Item -LiteralPath $Source -Force).Length
    $destinationLength = (Get-Item -LiteralPath $Destination -Force).Length
    if ($sourceLength -ne $destinationLength) {
        throw "Mutable-file copy length mismatch: $Source -> $Destination"
    }
}

$sandbox = Resolve-FnvxrTtwSandboxPath -Path $SandboxRoot -Description "SandboxRoot"
$retailSource = Resolve-FnvxrTtwSandboxPath -Path $RetailSourceRoot -Description "RetailSourceRoot"
$ttwSource = Resolve-FnvxrTtwSandboxPath -Path $TtwDataRoot -Description "TtwDataRoot"

if (-not (Test-FnvxrTtwPathIsWithin -Child $sandbox -Parent $localRoot)) {
    throw "SandboxRoot must remain inside the repository local directory: $localRoot"
}
if ([string]::Equals($sandbox, $localRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "SandboxRoot cannot be the repository local directory itself."
}
foreach ($source in @($retailSource, $ttwSource)) {
    if (Test-FnvxrTtwPathIsWithin -Child $source -Parent $sandbox) {
        throw "A sandbox source cannot live inside the sandbox target: $source"
    }
}
if (-not (Test-Path -LiteralPath $retailSource -PathType Container)) {
    throw "Retail FNV source root is missing: $retailSource"
}
if (-not (Test-Path -LiteralPath $ttwSource -PathType Container)) {
    throw "TTW data source root is missing: $ttwSource"
}

# Keep the source identity gate used by the product launcher.  This protects
# against building a sandbox from an arbitrary or unsupported executable.
$retailGame = Assert-FnvxrProductGameRoot -GameRoot $retailSource

$coreRetailFiles = @(
    "atimgpud.dll",
    "binkw32.dll",
    "FalloutNV.exe",
    "Fallout_default.ini",
    "GDFFalloutNV.dll",
    "libvorbis.dll",
    "libvorbisfile.dll",
    "MainTitle.wav",
    "nvse_1_4.dll",
    "nvse_loader.exe",
    "nvse_steam_loader.dll",
    "steam_api.dll",
    "xinput1_1.dll",
    "xinput1_2.dll",
    "xinput1_3.dll",
    "xinput1_4.dll",
    "xinput9_1_0.dll")
$mutableRetailFiles = @("Fallout_default.ini")
$compatibilityFiles = @(
    "Data\NVSE\Plugins\jip_nvse.dll",
    "Data\NVSE\Plugins\jip_nvse.ini",
    "Data\NVSE\Plugins\ShowOffNVSE.dll")
$mutableCompatibilityFiles = @("Data\NVSE\Plugins\jip_nvse.ini")

foreach ($relative in @($coreRetailFiles + $compatibilityFiles)) {
    Assert-FnvxrTtwRequiredFile `
        -Path (Join-Path $retailSource $relative) `
        -Description "Required clean retail compatibility file"
}

# The order is the current TTW baseline from Wasteland Survival Guide.  It is
# deliberately only the base TTW/YUPTTW profile; JAM, BHY, and Enhanced Camera
# are a later explicit stability gate rather than hidden variables here.
$ttwBaselinePlugins = @(
    "FalloutNV.esm",
    "DeadMoney.esm",
    "HonestHearts.esm",
    "OldWorldBlues.esm",
    "LonesomeRoad.esm",
    "GunRunnersArsenal.esm",
    "Fallout3.esm",
    "Anchorage.esm",
    "ThePitt.esm",
    "BrokenSteel.esm",
    "PointLookout.esm",
    "Zeta.esm",
    "CaravanPack.esm",
    "ClassicPack.esm",
    "MercenaryPack.esm",
    "TribalPack.esm",
    "TaleOfTwoWastelands.esm",
    "YUPTTW.esm")
foreach ($plugin in $ttwBaselinePlugins) {
    Assert-FnvxrTtwRequiredFile `
        -Path (Join-Path $ttwSource $plugin) `
        -Description "Required TTW baseline plugin"
}

$ttwFiles = @(
    Get-ChildItem -LiteralPath $ttwSource -File -Recurse | Sort-Object FullName)
if ($ttwFiles.Count -eq 0) {
    throw "TTW data source contains no files: $ttwSource"
}

# TTW installer output is an MO2 overlay, not a complete standalone FNV Data
# directory. Retain the base retail Data assets which it does not replace,
# while excluding NVSE and root ESP files from the user's active mod stack. The TTW
# layer is linked first, so it always wins on duplicate relative paths.
$retailDataRoot = Join-Path $retailSource "Data"
$retailDataOverlayFiles = @(
    Get-ChildItem -LiteralPath $retailDataRoot -File -Recurse |
        Where-Object {
            $relative = $_.FullName.Substring($retailDataRoot.Length).TrimStart('\', '/')
            if ($relative -match '^(?i:NVSE)[\\/]') { return $false }
            if ($relative -notmatch '[\\/]') {
                return $_.Extension -in @('.bsa', '.nam', '.esm')
            }
            return $relative -match '^(?i:Music|Sound|Shaders|Video)[\\/]'
        } |
        Sort-Object FullName)
if ($retailDataOverlayFiles.Count -eq 0) {
    throw "Retail source did not provide a base Data overlay candidate set: $retailDataRoot"
}
$requiredRetailDataAssets = @(
    "Fallout - Voices1.bsa",
    "Update.bsa")
foreach ($relative in $requiredRetailDataAssets) {
    Assert-FnvxrTtwRequiredFile `
        -Path (Join-Path $retailDataRoot $relative) `
        -Description "Required base retail Data asset"
}

$plan = [ordered]@{
    schema = "fnvxr-ttw-retail-sandbox-v1"
    sandboxRoot = $sandbox
    source = [ordered]@{
        retailRoot = $retailGame.root
        retailFallout = $retailGame.fallout
        ttwDataRoot = $ttwSource
    }
    scope = "workspace-owned isolated retail FNV TTW baseline; no process, UI, input, simulator, LocalAppData, Documents-save, or source-root mutation"
    profile = [ordered]@{
        name = "ttw-core-baseline-v1"
        plugins = @($ttwBaselinePlugins)
        laterStabilityGates = @("JAM", "Benny Humbles You", "Enhanced Camera")
    }
    transfer = [ordered]@{
        hardLinkFiles = [int]($coreRetailFiles.Count - $mutableRetailFiles.Count +
            $compatibilityFiles.Count - $mutableCompatibilityFiles.Count +
            $ttwFiles.Count + $retailDataOverlayFiles.Count)
        copiedMutableFiles = [int]($mutableRetailFiles.Count + $mutableCompatibilityFiles.Count)
        noCopyFallback = $true
        retailDataOverlayCandidates = $retailDataOverlayFiles.Count
    }
}

if ($ValidateOnly) {
    $plan | ConvertTo-Json -Depth 10
    return
}

if ((Test-Path -LiteralPath $sandbox) -and -not $RepairMissingRetailData) {
    throw "Sandbox target already exists; refusing to overwrite, merge, or delete it: $sandbox"
}
if ($RepairMissingRetailData -and -not (Test-Path -LiteralPath $sandbox -PathType Container)) {
    throw "Retail-data repair requires an existing owned sandbox; refusing to create a different target: $sandbox"
}

if ($RepairMissingRetailData) {
    $manifestPath = Join-Path $sandbox "fnvxr-ttw-sandbox-manifest.json"
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        throw "Retail-data repair requires the owned sandbox manifest: $manifestPath"
    }
    $existingManifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ($existingManifest.plan.schema -cne "fnvxr-ttw-retail-sandbox-v1" -or
        $existingManifest.plan.sandboxRoot -cne $sandbox) {
        throw "Retail-data repair refuses a sandbox without the exact owned manifest: $sandbox"
    }
    $runtime = @(
        Get-Process -Name FalloutNV,nvse_loader,fnvxr_openxr_pose_host `
            -ErrorAction SilentlyContinue | Select-Object ProcessName,Id,StartTime)
    if ($runtime.Count -ne 0) {
        throw "Retail-data repair refuses to modify the sandbox while a runtime exists."
    }

    $added = @()
    foreach ($file in $retailDataOverlayFiles) {
        $relative = $file.FullName.Substring($retailDataRoot.Length).TrimStart('\', '/')
        $destination = Join-Path (Join-Path $sandbox "Data") $relative
        if (-not (Test-Path -LiteralPath $destination -PathType Leaf)) {
            New-FnvxrTtwHardLink -Source $file.FullName -Destination $destination
            $added += $relative
        }
    }
    $existingManifest | Add-Member -NotePropertyName retailDataRepair -NotePropertyValue ([ordered]@{
        completedAtUtc = [DateTime]::UtcNow.ToString("o")
        addedFiles = @($added)
        addedCount = $added.Count
        overwriteOrDelete = $false
    }) -Force
    [System.IO.File]::WriteAllText(
        $manifestPath,
        ($existingManifest | ConvertTo-Json -Depth 14),
        [System.Text.UTF8Encoding]::new($false))
    [pscustomobject][ordered]@{
        sandboxRoot = $sandbox
        manifestPath = $manifestPath
        profile = "ttw-core-baseline-v1"
        retailDataFilesAdded = $added.Count
        sourceRootsMutated = $false
        processOrUiControl = $false
    } | ConvertTo-Json -Depth 8
    return
}

New-Item -ItemType Directory -Path $sandbox -ErrorAction Stop | Out-Null

try {
    foreach ($relative in $coreRetailFiles) {
        $source = Join-Path $retailSource $relative
        $destination = Join-Path $sandbox $relative
        if ($mutableRetailFiles -ccontains $relative) {
            Copy-FnvxrTtwMutableFile -Source $source -Destination $destination
        } else {
            New-FnvxrTtwHardLink -Source $source -Destination $destination
        }
    }
    foreach ($relative in $compatibilityFiles) {
        $source = Join-Path $retailSource $relative
        $destination = Join-Path $sandbox $relative
        if ($mutableCompatibilityFiles -ccontains $relative) {
            Copy-FnvxrTtwMutableFile -Source $source -Destination $destination
        } else {
            New-FnvxrTtwHardLink -Source $source -Destination $destination
        }
    }
    foreach ($file in $ttwFiles) {
        $relative = $file.FullName.Substring($ttwSource.Length).TrimStart('\', '/')
        New-FnvxrTtwHardLink `
            -Source $file.FullName `
            -Destination (Join-Path (Join-Path $sandbox "Data") $relative)
    }
    $retailDataAdded = @()
    foreach ($file in $retailDataOverlayFiles) {
        $relative = $file.FullName.Substring($retailDataRoot.Length).TrimStart('\', '/')
        $destination = Join-Path (Join-Path $sandbox "Data") $relative
        # The TTW overlay is always linked first. A base retail asset fills
        # only a gap; it never replaces TTW content.
        if (-not (Test-Path -LiteralPath $destination -PathType Leaf)) {
            New-FnvxrTtwHardLink -Source $file.FullName -Destination $destination
            $retailDataAdded += $relative
        }
    }

    $manifestPath = Join-Path $sandbox "fnvxr-ttw-sandbox-manifest.json"
    $manifest = [ordered]@{
        plan = $plan
        createdAtUtc = [DateTime]::UtcNow.ToString("o")
        completed = $true
    }
    [System.IO.File]::WriteAllText(
        $manifestPath,
        ($manifest | ConvertTo-Json -Depth 12),
        [System.Text.UTF8Encoding]::new($false))
} catch {
    # Preserve the partial workspace-only tree for inspection.  Automatic
    # cleanup would be a destructive operation and is intentionally absent.
    throw "TTW retail sandbox creation stopped without touching either source root. Inspect the workspace-only partial target: $sandbox. $($_.Exception.Message)"
}

[pscustomobject][ordered]@{
    sandboxRoot = $sandbox
    manifestPath = (Join-Path $sandbox "fnvxr-ttw-sandbox-manifest.json")
    profile = "ttw-core-baseline-v1"
    ttwFilesLinked = $ttwFiles.Count
    retailDataFilesLinked = $retailDataAdded.Count
    sourceRootsMutated = $false
    processOrUiControl = $false
} | ConvertTo-Json -Depth 8
