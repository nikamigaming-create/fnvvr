param(
    [string]$ZipPath = "C:\Users\nbrys\Downloads\meta-xr-operator-standalone-public.zip",
    [string]$DestinationRoot = "",
    [switch]$ValidateOnly
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

# This stages Meta's standalone OpenXR API layer as an application-local,
# inspectable workspace dependency. It never launches or controls the game,
# simulator, desktop, headset, controller, or Operator MCP endpoint.

. (Join-Path $PSScriptRoot "fnvxr-product-common.ps1")

$root = Get-FnvxrProductRoot
$allowedRoot = [System.IO.Path]::GetFullPath((Join-Path $root "local\meta-xr-operator"))
if ([string]::IsNullOrWhiteSpace($DestinationRoot)) {
    $DestinationRoot = Join-Path $allowedRoot "standalone-public-v1"
}
if (-not [System.IO.Path]::IsPathRooted($DestinationRoot)) {
    throw "Operator DestinationRoot must be an absolute workspace path."
}
$destination = [System.IO.Path]::GetFullPath($DestinationRoot)
$allowedPrefix = $allowedRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
if (-not $destination.StartsWith($allowedPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Operator DestinationRoot must remain below the workspace-owned local dependency root: $allowedRoot"
}
if ([string]::Equals($destination, $allowedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Operator DestinationRoot cannot be the dependency root itself."
}
if (-not (Test-Path -LiteralPath $ZipPath -PathType Leaf)) {
    throw "Meta XR Operator archive is missing: $ZipPath"
}

$expectedEntries = [ordered]@{
    manifest = "meta-xr-operator-standalone-public/windows/XrApiLayer_METAX_operator.json"
    library = "meta-xr-operator-standalone-public/windows/XrApiLayer_METAX_operator.dll"
    proxy = "meta-xr-operator-standalone-public/windows/meta-xr-operator-mcp-proxy.exe"
}

function Get-FnvxrOperatorArchiveEntryText {
    param([Parameter(Mandatory = $true)]$Entry)

    $reader = [System.IO.StreamReader]::new($Entry.Open())
    try {
        return $reader.ReadToEnd()
    } finally {
        $reader.Dispose()
    }
}

function Get-FnvxrOperatorStagePlan {
    param([Parameter(Mandatory = $true)][string]$ArchivePath)

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [System.IO.Compression.ZipFile]::OpenRead($ArchivePath)
    try {
        $entries = [ordered]@{}
        foreach ($key in $expectedEntries.Keys) {
            $entry = @($archive.Entries | Where-Object { $_.FullName -ceq $expectedEntries[$key] })
            if ($entry.Count -ne 1) {
                throw "Meta XR Operator archive must contain exactly one $key entry: $($expectedEntries[$key])"
            }
            if ($entry[0].Length -le 0) {
                throw "Meta XR Operator archive entry is empty: $($expectedEntries[$key])"
            }
            $entries[$key] = [pscustomobject][ordered]@{
                fullName = $entry[0].FullName
                length = $entry[0].Length
            }
        }
        try {
        $manifestEntry = @($archive.Entries | Where-Object {
            $_.FullName -ceq $entries.manifest.fullName
        })[0]
        $document = Get-FnvxrOperatorArchiveEntryText -Entry $manifestEntry | ConvertFrom-Json
        } catch {
            throw "Meta XR Operator API-layer manifest is not valid JSON. $($_.Exception.Message)"
        }
        if ($document.file_format_version -cne "1.0.0" -or
            $document.api_layer.name -cne "XR_APILAYER_METAX_operator" -or
            $document.api_layer.library_path -cne "./XrApiLayer_METAX_operator.dll") {
            throw "Meta XR Operator archive does not contain the expected Windows API-layer identity."
        }
        return [pscustomobject][ordered]@{
            archive = Get-FnvxrProductFileIdentity -Path $ArchivePath
            manifestDocument = $document
            entries = $entries
        }
    } finally {
        $archive.Dispose()
    }
}

function Get-FnvxrOperatorStagedIdentity {
    param([Parameter(Mandatory = $true)][string]$Root)

    $operatorDirectory = Join-Path $Root "windows"
    $manifestPath = Join-Path $operatorDirectory "XrApiLayer_METAX_operator.json"
    $libraryPath = Join-Path $operatorDirectory "XrApiLayer_METAX_operator.dll"
    $proxyPath = Join-Path $operatorDirectory "meta-xr-operator-mcp-proxy.exe"
    foreach ($path in @($manifestPath, $libraryPath, $proxyPath)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Meta XR Operator staged dependency is incomplete: $path"
        }
    }
    try {
        $document = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    } catch {
        throw "Staged Meta XR Operator API-layer manifest is not valid JSON: $manifestPath"
    }
    if ($document.file_format_version -cne "1.0.0" -or
        $document.api_layer.name -cne "XR_APILAYER_METAX_operator" -or
        $document.api_layer.library_path -cne "./XrApiLayer_METAX_operator.dll") {
        throw "Staged Meta XR Operator API-layer identity is not exact: $manifestPath"
    }
    return [pscustomobject][ordered]@{
        directory = $operatorDirectory
        apiLayerName = [string]$document.api_layer.name
        manifest = Get-FnvxrProductFileIdentity -Path $manifestPath
        library = Get-FnvxrProductFileIdentity -Path $libraryPath -RequirePe
        proxy = Get-FnvxrProductFileIdentity -Path $proxyPath -RequirePe
    }
}

$archivePlan = Get-FnvxrOperatorStagePlan -ArchivePath $ZipPath
if ($ValidateOnly) {
    [pscustomobject][ordered]@{
        schema = "fnvxr-meta-xr-operator-stage-v1"
        valid = $true
        liveActionsTaken = $false
        archive = $archivePlan.archive
        destinationRoot = $destination
        apiLayerName = [string]$archivePlan.manifestDocument.api_layer.name
        processOrUiControl = $false
        inputOrSimulatorControl = $false
    } | ConvertTo-Json -Depth 8
    return
}

$runtime = @(
    Get-Process -Name FalloutNV,nvse_loader,fnvxr_openxr_pose_host `
        -ErrorAction SilentlyContinue | Select-Object ProcessName,Id,StartTime)
if ($runtime.Count -ne 0) {
    throw "Meta XR Operator staging refuses to change dependencies while a runtime exists."
}

$stageManifestPath = Join-Path $destination "fnvxr-meta-xr-operator-stage.json"
if (Test-Path -LiteralPath $destination -PathType Container) {
    if (-not (Test-Path -LiteralPath $stageManifestPath -PathType Leaf)) {
        throw "Meta XR Operator destination already exists without its owned stage manifest; refusing to overwrite, merge, or delete it: $destination"
    }
    $existing = Get-Content -LiteralPath $stageManifestPath -Raw | ConvertFrom-Json
    if ($existing.schema -cne "fnvxr-meta-xr-operator-stage-v1" -or
        $existing.destinationRoot -cne $destination -or
        $existing.archive.sha256 -cne $archivePlan.archive.sha256) {
        throw "Meta XR Operator destination does not match this exact archive; refusing to overwrite, merge, or delete it: $destination"
    }
    $staged = Get-FnvxrOperatorStagedIdentity -Root $destination
    [pscustomobject][ordered]@{
        schema = "fnvxr-meta-xr-operator-stage-v1"
        reused = $true
        destinationRoot = $destination
        operatorLayerDirectory = $staged.directory
        apiLayerName = $staged.apiLayerName
        archive = $archivePlan.archive
        staged = $staged
        processOrUiControl = $false
        inputOrSimulatorControl = $false
    } | ConvertTo-Json -Depth 10
    return
}

New-Item -ItemType Directory -Path (Join-Path $destination "windows") -ErrorAction Stop | Out-Null
try {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [System.IO.Compression.ZipFile]::OpenRead($ZipPath)
    try {
        foreach ($key in $expectedEntries.Keys) {
            $entry = @($archive.Entries | Where-Object {
                $_.FullName -ceq $archivePlan.entries[$key].fullName
            })[0]
            if ($null -eq $entry) {
                throw "Meta XR Operator archive changed after validation: $($archivePlan.entries[$key].fullName)"
            }
            $target = Join-Path (Join-Path $destination "windows") (Split-Path -Leaf $entry.FullName)
            # API layer manifest and DLL must stay side by side. The no-overwrite
            # extraction flag makes a partial failure inspectable and non-destructive.
            [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $target, $false)
        }
    } finally {
        $archive.Dispose()
    }
    $staged = Get-FnvxrOperatorStagedIdentity -Root $destination
    if ($staged.library.peMachine -cne "0x8664" -or $staged.proxy.peMachine -cne "0x8664") {
        throw "Meta XR Operator staged Windows binaries are not x64."
    }
    $record = [ordered]@{
        schema = "fnvxr-meta-xr-operator-stage-v1"
        createdAtUtc = [DateTime]::UtcNow.ToString("o")
        destinationRoot = $destination
        archive = $archivePlan.archive
        apiLayerName = $staged.apiLayerName
        operatorLayerDirectory = $staged.directory
        staged = $staged
        overwriteOrDelete = $false
        processOrUiControl = $false
        inputOrSimulatorControl = $false
    }
    [System.IO.File]::WriteAllText(
        $stageManifestPath,
        ($record | ConvertTo-Json -Depth 12),
        [System.Text.UTF8Encoding]::new($false))
} catch {
    # Keep only the workspace-owned partial output for explicit inspection.
    # Automatic removal is deliberately absent.
    throw "Meta XR Operator staging stopped without touching the archive or any runtime. Inspect the workspace-only partial target: $destination. $($_.Exception.Message)"
}

[pscustomobject][ordered]@{
    schema = "fnvxr-meta-xr-operator-stage-v1"
    reused = $false
    destinationRoot = $destination
    operatorLayerDirectory = $staged.directory
    apiLayerName = $staged.apiLayerName
    archive = $archivePlan.archive
    staged = $staged
    processOrUiControl = $false
    inputOrSimulatorControl = $false
} | ConvertTo-Json -Depth 10
