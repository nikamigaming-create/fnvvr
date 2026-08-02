param(
    [string]$SandboxRoot = "",
    [string]$PackagePath = "",
    [switch]$ValidateOnly
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

# Stage the one runtime dependency that TTW itself reported as missing during
# the owned headless fixture load. This is deliberately a workspace-only
# operation: it neither launches nor controls Fallout, and it never writes to
# the live retail root, a mod manager, or Documents saves.

$repositoryRoot = (Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$localRoot = [System.IO.Path]::GetFullPath((Join-Path $repositoryRoot "local"))
if ([string]::IsNullOrWhiteSpace($SandboxRoot)) {
    $SandboxRoot = Join-Path $localRoot "ttw-retail-sandbox"
}
if ([string]::IsNullOrWhiteSpace($PackagePath)) {
    $PackagePath = Join-Path $localRoot "ttw-dependencies\johnnyguitar-5.28\JohnnyGuitarNVSE-5.28.zip"
}

function Resolve-FnvxrTtwJohnnyPath {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Description
    )

    if ([string]::IsNullOrWhiteSpace($Path) -or -not [IO.Path]::IsPathRooted($Path)) {
        throw "$Description must be an absolute path."
    }
    return [IO.Path]::GetFullPath($Path)
}

function Test-FnvxrTtwJohnnyPathWithin {
    param(
        [Parameter(Mandatory = $true)][string]$Child,
        [Parameter(Mandatory = $true)][string]$Parent
    )

    $prefix = $Parent.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    return $Child.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)
}

function Get-FnvxrTtwJohnnyIdentity {
    param([Parameter(Mandatory = $true)][string]$Path)

    $item = Get-Item -LiteralPath $Path -Force
    return [ordered]@{
        path = [IO.Path]::GetFullPath($Path)
        length = [int64]$item.Length
        sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

function Copy-FnvxrTtwJohnnyExactFile {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    $sourceIdentity = Get-FnvxrTtwJohnnyIdentity -Path $Source
    if (Test-Path -LiteralPath $Destination -PathType Leaf) {
        $destinationIdentity = Get-FnvxrTtwJohnnyIdentity -Path $Destination
        if ($destinationIdentity.sha256 -cne $sourceIdentity.sha256) {
            throw "Refusing to overwrite a different sandbox dependency: $Destination"
        }
        return [ordered]@{ state = "already-present"; source = $sourceIdentity; destination = $destinationIdentity }
    }

    New-Item -ItemType Directory -Path (Split-Path -Parent $Destination) -Force | Out-Null
    Copy-Item -LiteralPath $Source -Destination $Destination -ErrorAction Stop
    $destinationIdentity = Get-FnvxrTtwJohnnyIdentity -Path $Destination
    if ($destinationIdentity.sha256 -cne $sourceIdentity.sha256) {
        throw "JohnnyGuitar sandbox copy hash mismatch: $Source -> $Destination"
    }
    return [ordered]@{ state = "copied"; source = $sourceIdentity; destination = $destinationIdentity }
}

$sandbox = Resolve-FnvxrTtwJohnnyPath -Path $SandboxRoot -Description "SandboxRoot"
$package = Resolve-FnvxrTtwJohnnyPath -Path $PackagePath -Description "PackagePath"
if (-not (Test-FnvxrTtwJohnnyPathWithin -Child $sandbox -Parent $localRoot)) {
    throw "SandboxRoot must remain inside the workspace local directory: $localRoot"
}
if (-not (Test-Path -LiteralPath (Join-Path $sandbox "FalloutNV.exe") -PathType Leaf)) {
    throw "Owned TTW sandbox FalloutNV.exe is missing: $sandbox"
}
if (-not (Test-Path -LiteralPath $package -PathType Leaf)) {
    throw "Official JohnnyGuitar 5.28 package is missing: $package"
}

$expectedPackageSha256 = "5ab249afd70bfbc727659a81c2916c63508664b748f336289b2339894be830bc"
$packageIdentity = Get-FnvxrTtwJohnnyIdentity -Path $package
if ($packageIdentity.sha256 -cne $expectedPackageSha256) {
    throw "JohnnyGuitar 5.28 package hash does not match the pinned official asset."
}

$cacheRoot = Join-Path $localRoot "ttw-dependencies\johnnyguitar-5.28"
$extractionRoot = Join-Path $cacheRoot "extracted"
$sourceFiles = [ordered]@{
    "nvse\plugins\johnnyguitar.dll" = "Data\NVSE\Plugins\johnnyguitar.dll"
    "nvse\plugins\JohnnyGuitar.ini" = "Data\NVSE\Plugins\JohnnyGuitar.ini"
}
$plan = [ordered]@{
    schema = "fnvxr-ttw-johnnyguitar-stage-v1"
    scope = "workspace-owned TTW sandbox only; no process launch/control, simulator/UI/input, live-retail root, mod-manager, or Documents-save mutation"
    release = [ordered]@{
        name = "JohnnyGuitar NVSE"
        version = "5.28"
        source = "https://github.com/carxt/JohnnyGuitarNVSE/releases/download/5.28/JohnnyGuitarNVSE-5.28.zip"
        package = $packageIdentity
        pinnedSha256 = $expectedPackageSha256
    }
    sandboxRoot = $sandbox
    files = @($sourceFiles.GetEnumerator() | ForEach-Object {
        [ordered]@{
            source = Join-Path $extractionRoot $_.Key
            destination = Join-Path $sandbox $_.Value
        }
    })
}

if ($ValidateOnly) {
    $plan | ConvertTo-Json -Depth 8
    return
}

$existingRuntime = @(
    Get-Process -Name FalloutNV,nvse_loader,fnvxr_openxr_pose_host -ErrorAction SilentlyContinue |
        Select-Object ProcessName,Id,StartTime)
if ($existingRuntime.Count -ne 0) {
    throw "A runtime is active; refusing to stage TTW dependencies into an in-use sandbox."
}

if (-not (Test-Path -LiteralPath $extractionRoot -PathType Container)) {
    New-Item -ItemType Directory -Path $extractionRoot -Force | Out-Null
    Expand-Archive -LiteralPath $package -DestinationPath $extractionRoot -ErrorAction Stop
}

foreach ($relative in $sourceFiles.Keys) {
    $source = Join-Path $extractionRoot $relative
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Pinned JohnnyGuitar archive has an unexpected layout; required file is missing: $source"
    }
}

$staged = @()
foreach ($relative in $sourceFiles.Keys) {
    $staged += Copy-FnvxrTtwJohnnyExactFile `
        -Source (Join-Path $extractionRoot $relative) `
        -Destination (Join-Path $sandbox $sourceFiles[$relative])
}

$manifest = [ordered]@{
    schema = "fnvxr-ttw-johnnyguitar-stage-v1"
    stagedAtUtc = [DateTime]::UtcNow.ToString("o")
    plan = $plan
    staged = @($staged)
    complete = $true
}
$manifestPath = Join-Path $cacheRoot "johnnyguitar-stage-manifest.json"
$temporaryManifestPath = $manifestPath + ".tmp-" + [Guid]::NewGuid().ToString("N")
$manifest | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $temporaryManifestPath -Encoding UTF8 -NoNewline
Move-Item -LiteralPath $temporaryManifestPath -Destination $manifestPath -Force
$manifest | ConvertTo-Json -Depth 12
