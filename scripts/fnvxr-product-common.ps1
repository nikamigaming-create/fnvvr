$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

function Get-FnvxrProductRoot {
    return (Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
}

function Write-FnvxrProductJsonAtomic {
    param(
        [Parameter(Mandatory = $true)]$Value,
        [Parameter(Mandatory = $true)][string]$Path,
        [int]$Depth = 12
    )

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $directory = Split-Path -Parent $fullPath
    if (-not (Test-Path -LiteralPath $directory -PathType Container)) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }
    $temporary = Join-Path $directory (".{0}.{1}.tmp" -f
        [System.IO.Path]::GetFileName($fullPath), [Guid]::NewGuid().ToString("N"))
    try {
        $Value | ConvertTo-Json -Depth $Depth | Set-Content -LiteralPath $temporary -Encoding UTF8
        Move-Item -LiteralPath $temporary -Destination $fullPath -Force
    } finally {
        if (Test-Path -LiteralPath $temporary -PathType Leaf) {
            Remove-Item -LiteralPath $temporary -Force
        }
    }
}

function Get-FnvxrProductFileIdentity {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [switch]$RequirePe
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required file is missing: $Path"
    }
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    $file = Get-Item -LiteralPath $resolved -Force
    if (($file.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Refusing reparse-point file: $resolved"
    }

    $stream = $null
    $hasher = $null
    try {
        $stream = New-Object System.IO.FileStream(
            $resolved,
            [System.IO.FileMode]::Open,
            [System.IO.FileAccess]::Read,
            [System.IO.FileShare]::Read)
        $hasher = [System.Security.Cryptography.SHA256]::Create()
        $digest = $hasher.ComputeHash($stream)
        $sha256 = ([System.BitConverter]::ToString($digest)).Replace("-", "").ToLowerInvariant()

        $machine = $null
        $timeDateStamp = $null
        $optionalMagic = $null
        $imageBase = $null
        $sizeOfImage = $null
        $checksum = $null
        if ($stream.Length -ge 64) {
            $reader = New-Object System.IO.BinaryReader($stream, [System.Text.Encoding]::ASCII, $true)
            $stream.Position = 0
            $dosMagic = $reader.ReadUInt16()
            $stream.Position = 0x3c
            $peOffset = $reader.ReadInt32()
            if ($dosMagic -eq 0x5a4d -and $peOffset -ge 64 -and
                [int64]$peOffset + 24 -le $stream.Length) {
                $stream.Position = $peOffset
                $signature = $reader.ReadUInt32()
                if ($signature -eq 0x00004550) {
                    $machineValue = $reader.ReadUInt16()
                    [void]$reader.ReadUInt16()
                    $timeDateStampValue = $reader.ReadUInt32()
                    $stream.Position = $peOffset + 20
                    $optionalBytes = $reader.ReadUInt16()
                    [void]$reader.ReadUInt16()
                    $optionalOffset = $peOffset + 24
                    if ($optionalBytes -ge 68 -and
                        [int64]$optionalOffset + $optionalBytes -le $stream.Length) {
                        $stream.Position = $optionalOffset
                        $optionalMagicValue = $reader.ReadUInt16()
                        if ($optionalMagicValue -eq 0x010b -or $optionalMagicValue -eq 0x020b) {
                            $stream.Position = $optionalOffset + 56
                            $sizeOfImageValue = $reader.ReadUInt32()
                            $stream.Position = $optionalOffset + 64
                            $checksumValue = $reader.ReadUInt32()
                            if ($optionalMagicValue -eq 0x010b) {
                                $stream.Position = $optionalOffset + 28
                                $imageBaseValue = [uint64]$reader.ReadUInt32()
                            } else {
                                $stream.Position = $optionalOffset + 24
                                $imageBaseValue = $reader.ReadUInt64()
                            }
                            $machine = ("0x{0:x4}" -f $machineValue)
                            $timeDateStamp = ("0x{0:x8}" -f $timeDateStampValue)
                            $optionalMagic = ("0x{0:x4}" -f $optionalMagicValue)
                            $imageBase = ("0x{0:x}" -f $imageBaseValue)
                            $sizeOfImage = ("0x{0:x8}" -f $sizeOfImageValue)
                            $checksum = ("0x{0:x8}" -f $checksumValue)
                        }
                    }
                }
            }
        }
        if ($RequirePe -and -not $machine) {
            throw "Required PE identity is unavailable: $resolved"
        }

        return [pscustomobject][ordered]@{
            path = $resolved
            length = [uint64]$stream.Length
            sha256 = $sha256
            peMachine = $machine
            peTimeDateStamp = $timeDateStamp
            peOptionalMagic = $optionalMagic
            peImageBase = $imageBase
            peSizeOfImage = $sizeOfImage
            peChecksum = $checksum
        }
    } finally {
        if ($hasher) { $hasher.Dispose() }
        if ($stream) { $stream.Dispose() }
    }
}

function Get-FnvxrProductStringSha256 {
    param([Parameter(Mandatory = $true)][string]$Value)

    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($Value)
        return ([System.BitConverter]::ToString($sha.ComputeHash($bytes))).Replace("-", "").ToLowerInvariant()
    } finally {
        $sha.Dispose()
    }
}

function Get-FnvxrProductRecordDigest {
    param([Parameter(Mandatory = $true)][object[]]$Records)

    $lines = @($Records | Sort-Object key | ForEach-Object {
        "{0}|{1}|{2}" -f ([string]$_.key).ToLowerInvariant(), [uint64]$_.length,
            ([string]$_.sha256).ToLowerInvariant()
    })
    return Get-FnvxrProductStringSha256 -Value ($lines -join "`n")
}

function Get-FnvxrProductSourceSnapshot {
    param([Parameter(Mandatory = $true)][string]$Root)

    $resolvedRoot = (Resolve-Path -LiteralPath $Root).Path.TrimEnd('\')
    $rootPrefix = $resolvedRoot + '\'
    $paths = New-Object 'System.Collections.Generic.List[string]'
    $paths.Add((Join-Path $resolvedRoot "CMakeLists.txt"))
    foreach ($name in @("host", "plugin", "protocol", "renderhook", "runtime", "scripts", "tests", "tools")) {
        $directory = Join-Path $resolvedRoot $name
        if (-not (Test-Path -LiteralPath $directory -PathType Container)) {
            throw "Build source directory is missing: $directory"
        }
        $directoryInfo = Get-Item -LiteralPath $directory -Force
        if (($directoryInfo.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Build source snapshot refuses a reparse-point directory: $directory"
        }
        foreach ($source in Get-ChildItem -LiteralPath $directory -File -Recurse -Force) {
            if (($source.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Build source snapshot refuses a reparse-point file: $($source.FullName)"
            }
            $paths.Add($source.FullName)
        }
    }

    $records = @()
    foreach ($path in @($paths | Sort-Object -Unique)) {
        $identity = Get-FnvxrProductFileIdentity -Path $path
        if (-not $identity.path.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Build source escaped repository root: $($identity.path)"
        }
        $records += [pscustomobject][ordered]@{
            key = $identity.path.Substring($rootPrefix.Length).Replace('\', '/').ToLowerInvariant()
            length = $identity.length
            sha256 = $identity.sha256
        }
    }
    if ($records.Count -eq 0) { throw "Build source snapshot is empty." }
    $records = @($records | Sort-Object key)
    return [pscustomobject][ordered]@{
        count = $records.Count
        sha256 = Get-FnvxrProductRecordDigest -Records $records
        records = $records
    }
}

function Get-FnvxrProductArtifactDescriptors {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$Configuration
    )

    $x64 = Join-Path $Root "build-product-x64\$Configuration"
    $x86 = Join-Path $Root "build-product-win32\$Configuration"
    return @(
        [pscustomobject]@{ key = "x64/fnvxr_openxr_pose_host.exe"; path = Join-Path $x64 "fnvxr_openxr_pose_host.exe"; machine = "0x8664" },
        [pscustomobject]@{ key = "x64/fnvxr_shared_state_probe.exe"; path = Join-Path $x64 "fnvxr_shared_state_probe.exe"; machine = "0x8664" },
        [pscustomobject]@{ key = "x64/openxr_loader.dll"; path = Join-Path $x64 "openxr_loader.dll"; machine = "0x8664" },
        [pscustomobject]@{ key = "x86/nvse_fnvxr.dll"; path = Join-Path $x86 "nvse_fnvxr.dll"; machine = "0x014c" },
        [pscustomobject]@{ key = "x86/d3d9.dll"; path = Join-Path $x86 "d3d9.dll"; machine = "0x014c" },
        [pscustomobject]@{ key = "x86/dinput8.dll"; path = Join-Path $x86 "dinput8.dll"; machine = "0x014c" },
        [pscustomobject]@{ key = "x86/xinput1_3.dll"; path = Join-Path $x86 "xinput1_3.dll"; machine = "0x014c" },
        [pscustomobject]@{ key = "build/x64-cmake-cache"; path = Join-Path $Root "build-product-x64\CMakeCache.txt"; machine = "" },
        [pscustomobject]@{ key = "build/x86-cmake-cache"; path = Join-Path $Root "build-product-win32\CMakeCache.txt"; machine = "" }
    )
}

function Get-FnvxrProductArtifactSnapshot {
    param([Parameter(Mandatory = $true)][object[]]$Descriptors)

    $records = @()
    foreach ($descriptor in @($Descriptors | Sort-Object key)) {
        $identity = Get-FnvxrProductFileIdentity -Path ([string]$descriptor.path) -RequirePe:([bool]$descriptor.machine)
        if ($descriptor.machine -and $identity.peMachine -cne ([string]$descriptor.machine).ToLowerInvariant()) {
            throw "Artifact has wrong PE architecture: $($identity.path) expected=$($descriptor.machine) actual=$($identity.peMachine)"
        }
        $records += [pscustomobject][ordered]@{
            key = ([string]$descriptor.key).ToLowerInvariant()
            path = $identity.path
            length = $identity.length
            sha256 = $identity.sha256
            peMachine = $identity.peMachine
        }
    }
    $records = @($records | Sort-Object key)
    return [pscustomobject][ordered]@{
        count = $records.Count
        sha256 = Get-FnvxrProductRecordDigest -Records $records
        records = $records
    }
}

function Get-FnvxrProductCtestSnapshot {
    param(
        [Parameter(Mandatory = $true)][string]$X64BuildDirectory,
        [Parameter(Mandatory = $true)][string]$Win32BuildDirectory,
        [Parameter(Mandatory = $true)][string]$Configuration
    )

    $records = @()
    foreach ($item in @(
        [pscustomobject]@{ key = "x64"; path = $X64BuildDirectory },
        [pscustomobject]@{ key = "x86"; path = $Win32BuildDirectory })) {
        $output = & ctest --test-dir $item.path -C $Configuration -N 2>&1
        if ($LASTEXITCODE -ne 0) {
            throw "Unable to enumerate $($item.key) CTest catalog: $($output -join [Environment]::NewLine)"
        }
        $names = @($output | ForEach-Object {
            if ([string]$_ -match '^\s*Test\s+#[0-9]+:\s+(.+?)\s*$') { $Matches[1] }
        } | Where-Object { $_ } | Sort-Object -Unique)
        if ($names.Count -eq 0) { throw "$($item.key) CTest catalog is empty." }
        foreach ($name in $names) {
            $key = "$($item.key)/$name"
            $records += [pscustomobject][ordered]@{
                key = $key
                length = 0
                sha256 = Get-FnvxrProductStringSha256 -Value $key
            }
        }
    }
    $records = @($records | Sort-Object key)
    return [pscustomobject][ordered]@{
        count = $records.Count
        sha256 = Get-FnvxrProductRecordDigest -Records $records
        records = $records
    }
}

function Write-FnvxrProductBuildAttestation {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$Configuration,
        [Parameter(Mandatory = $true)][string]$Nonce,
        [Parameter(Mandatory = $true)]$Source,
        [Parameter(Mandatory = $true)]$Artifacts,
        [Parameter(Mandatory = $true)]$Tests
    )

    $value = [ordered]@{
        schema = 2
        product = "fnvxr-v5-exact-retail"
        nonce = $Nonce
        repositoryRoot = (Resolve-Path -LiteralPath $Root).Path
        configuration = $Configuration
        createdAtUtc = [DateTime]::UtcNow.ToString("o")
        source = $Source
        artifacts = $Artifacts
        tests = [ordered]@{
            passed = $true
            count = $Tests.count
            sha256 = $Tests.sha256
            records = $Tests.records
        }
    }
    Write-FnvxrProductJsonAtomic -Value $value -Path $Path
}

function Assert-FnvxrProductBuildAttestation {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$Configuration
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Product build attestation is missing: $Path"
    }
    try { $attestation = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json }
    catch { throw "Product build attestation is unreadable: $Path`: $($_.Exception.Message)" }
    $resolvedRoot = (Resolve-Path -LiteralPath $Root).Path
    if ($attestation.schema -ne 2 -or $attestation.product -cne "fnvxr-v5-exact-retail" -or
        -not $attestation.tests.passed -or
        -not [string]::Equals([string]$attestation.repositoryRoot, $resolvedRoot,
            [System.StringComparison]::OrdinalIgnoreCase) -or
        [string]$attestation.configuration -cne $Configuration -or
        [string]::IsNullOrWhiteSpace([string]$attestation.nonce)) {
        throw "Product build attestation context mismatch: $Path"
    }
    $source = Get-FnvxrProductSourceSnapshot -Root $Root
    if ([uint64]$source.count -ne [uint64]$attestation.source.count -or
        [string]$source.sha256 -cne [string]$attestation.source.sha256) {
        throw "Product build attestation source digest mismatch: $Path"
    }
    $tests = Get-FnvxrProductCtestSnapshot `
        -X64BuildDirectory (Join-Path $Root "build-product-x64") `
        -Win32BuildDirectory (Join-Path $Root "build-product-win32") `
        -Configuration $Configuration
    if ([uint64]$tests.count -ne [uint64]$attestation.tests.count -or
        [string]$tests.sha256 -cne [string]$attestation.tests.sha256) {
        throw "Product build attestation test catalog mismatch: $Path"
    }
    $artifacts = Get-FnvxrProductArtifactSnapshot -Descriptors (
        Get-FnvxrProductArtifactDescriptors -Root $Root -Configuration $Configuration)
    if ([uint64]$artifacts.count -ne [uint64]$attestation.artifacts.count -or
        [string]$artifacts.sha256 -cne [string]$attestation.artifacts.sha256) {
        throw "Product build attestation artifact digest mismatch: $Path"
    }
    return $attestation
}

function Resolve-FnvxrProductOpenXrLoader {
    param([string]$ExplicitPath = "")

    $candidates = @(
        $ExplicitPath,
        $env:FNVXR_OPENXR_LOADER_HINT,
        "C:\Program Files (x86)\Steam\steamapps\common\SteamVR\bin\win64\openxr_loader.dll",
        "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\ThirdParty\OpenXR\win64\openxr_loader.dll")
    foreach ($candidate in @($candidates | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique)) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            $identity = Get-FnvxrProductFileIdentity -Path $candidate -RequirePe
            if ($identity.peMachine -cne "0x8664") {
                throw "OpenXR loader is not x64: $($identity.path)"
            }
            return $identity.path
        }
    }
    throw "No x64 openxr_loader.dll was found. Pass -OpenXrLoaderPath explicitly."
}

function Assert-FnvxrProductGameRoot {
    param([Parameter(Mandatory = $true)][string]$GameRoot)

    if (-not (Test-Path -LiteralPath $GameRoot -PathType Container)) {
        throw "Fallout New Vegas root is missing: $GameRoot"
    }
    $resolved = (Resolve-Path -LiteralPath $GameRoot).Path
    foreach ($relative in @("FalloutNV.exe", "nvse_loader.exe", "Data\FalloutNV.esm")) {
        if (-not (Test-Path -LiteralPath (Join-Path $resolved $relative) -PathType Leaf)) {
            throw "Fallout New Vegas root is incomplete; missing $relative in $resolved"
        }
    }
    $fallout = Get-FnvxrProductFileIdentity -Path (Join-Path $resolved "FalloutNV.exe") -RequirePe
    $expected = [ordered]@{
        peMachine = "0x014c"
        peTimeDateStamp = "0x4e0d50ed"
        peOptionalMagic = "0x010b"
        peImageBase = "0x400000"
        peSizeOfImage = "0x0107b000"
        peChecksum = "0x00fcf93e"
    }
    foreach ($property in $expected.Keys) {
        if ([string]$fallout.$property -cne [string]$expected[$property]) {
            throw "Unsupported FalloutNV.exe PE identity: $property expected=$($expected[$property]) actual=$($fallout.$property)"
        }
    }
    $loader = Get-FnvxrProductFileIdentity -Path (Join-Path $resolved "nvse_loader.exe") -RequirePe
    if ($loader.peMachine -cne "0x014c") { throw "nvse_loader.exe is not Win32: $($loader.path)" }

    $pluginRoot = Join-Path $resolved "Data\NVSE\Plugins"
    $compatibility = @(
        [pscustomobject]@{ name = "jip_nvse.dll"; length = 502272; sha256 = "9d2779647ed0ce63043390f47fc978e3234af8e558dc6cb6bcb231478a2d74d4" },
        [pscustomobject]@{ name = "ShowOffNVSE.dll"; length = 1091584; sha256 = "37cb22c5288fedd0d57196c8c2f6bbaba5a1dafd9ce58f14dac9410dbee7ef3e" })
    $modules = @()
    foreach ($contract in $compatibility) {
        $path = Join-Path $pluginRoot $contract.name
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            $identity = Get-FnvxrProductFileIdentity -Path $path -RequirePe
            if ($identity.length -ne $contract.length -or $identity.sha256 -cne $contract.sha256) {
                throw "Installed $($contract.name) does not match the exact authorized retail contract."
            }
            $modules += $identity
        }
    }
    return [pscustomobject][ordered]@{
        root = $resolved
        fallout = $fallout
        nvseLoader = $loader
        compatibilityModules = $modules
    }
}

function Get-FnvxrProductStagePlan {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$Configuration,
        [Parameter(Mandatory = $true)][string]$GameRoot
    )

    $x86 = Join-Path $Root "build-product-win32\$Configuration"
    $pluginRoot = Join-Path $GameRoot "Data\NVSE\Plugins"
    return @(
        [pscustomobject]@{ key = "x86/d3d9.dll"; source = Join-Path $x86 "d3d9.dll"; destination = Join-Path $GameRoot "d3d9.dll"; machine = "0x014c" },
        [pscustomobject]@{ key = "x86/nvse_fnvxr.dll"; source = Join-Path $x86 "nvse_fnvxr.dll"; destination = Join-Path $pluginRoot "nvse_fnvxr.dll"; machine = "0x014c" },
        [pscustomobject]@{ key = "x86/dinput8.dll"; source = Join-Path $x86 "dinput8.dll"; destination = Join-Path $GameRoot "dinput8.dll"; machine = "0x014c" },
        [pscustomobject]@{ key = "x86/xinput1_3.dll"; source = Join-Path $x86 "xinput1_3.dll"; destination = Join-Path $GameRoot "xinput1_3.dll"; machine = "0x014c" }
    )
}

function Install-FnvxrProductArtifactSet {
    param(
        [Parameter(Mandatory = $true)][object[]]$Plan,
        [Parameter(Mandatory = $true)][string]$BackupRoot,
        [Parameter(Mandatory = $true)][string]$RunId
    )

    New-Item -ItemType Directory -Path $BackupRoot -Force | Out-Null
    $records = @()
    try {
        foreach ($item in $Plan) {
            $source = Get-FnvxrProductFileIdentity -Path $item.source -RequirePe
            if ($source.peMachine -cne $item.machine) {
                throw "Stage source has wrong architecture: $($source.path)"
            }
            $destination = [System.IO.Path]::GetFullPath([string]$item.destination)
            $destinationDirectory = Split-Path -Parent $destination
            if (-not (Test-Path -LiteralPath $destinationDirectory -PathType Container)) {
                New-Item -ItemType Directory -Path $destinationDirectory -Force | Out-Null
            }
            $existed = Test-Path -LiteralPath $destination -PathType Leaf
            $previous = $null
            $backup = $null
            if ($existed) {
                $previous = Get-FnvxrProductFileIdentity -Path $destination
                $backup = Join-Path $BackupRoot (([string]$item.key).Replace('/', '\'))
                New-Item -ItemType Directory -Path (Split-Path -Parent $backup) -Force | Out-Null
                Copy-Item -LiteralPath $destination -Destination $backup -Force
                $backupIdentity = Get-FnvxrProductFileIdentity -Path $backup
                if ($backupIdentity.sha256 -cne $previous.sha256) {
                    throw "Artifact backup hash mismatch: $destination -> $backup"
                }
            }
            $record = [pscustomobject][ordered]@{
                key = $item.key
                source = $source
                destination = [pscustomobject]@{ path = $destination }
                previousExists = [bool]$existed
                previous = $previous
                backup = $backup
            }
            $records += $record
            $temporary = "$destination.fnvxr-new-$RunId"
            try {
                if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force }
                Copy-Item -LiteralPath $source.path -Destination $temporary -Force
                $temporaryIdentity = Get-FnvxrProductFileIdentity -Path $temporary -RequirePe
                if ($temporaryIdentity.sha256 -cne $source.sha256 -or
                    $temporaryIdentity.peMachine -cne $item.machine) {
                    throw "Temporary staged artifact failed identity check: $temporary"
                }
                Move-Item -LiteralPath $temporary -Destination $destination -Force
            } finally {
                if (Test-Path -LiteralPath $temporary -PathType Leaf) {
                    Remove-Item -LiteralPath $temporary -Force
                }
            }
            $installed = Get-FnvxrProductFileIdentity -Path $destination -RequirePe
            if ($installed.sha256 -cne $source.sha256 -or $installed.peMachine -cne $item.machine) {
                throw "Installed artifact failed identity check: $destination"
            }
            $record.destination = $installed
        }
        return $records
    } catch {
        if ($records.Count -gt 0) {
            Restore-FnvxrProductArtifactSet -Records $records
        }
        throw
    }
}

function Restore-FnvxrProductArtifactSet {
    param([Parameter(Mandatory = $true)][object[]]$Records)

    foreach ($record in @($Records | Select-Object -Last 999 | Sort-Object key -Descending)) {
        $destination = [string]$record.destination.path
        if ($record.previousExists) {
            if (-not $record.backup -or -not (Test-Path -LiteralPath $record.backup -PathType Leaf)) {
                throw "Cannot restore staged artifact; backup is missing: $destination"
            }
            Copy-Item -LiteralPath $record.backup -Destination $destination -Force
            $restored = Get-FnvxrProductFileIdentity -Path $destination
            if ($restored.sha256 -cne $record.previous.sha256) {
                throw "Restored artifact hash mismatch: $destination"
            }
        } elseif (Test-Path -LiteralPath $destination -PathType Leaf) {
            Remove-Item -LiteralPath $destination -Force
        }
    }
}

function Get-FnvxrProductMinimalEnvironment {
    param(
        [Parameter(Mandatory = $true)][string]$RunId,
        [Parameter(Mandatory = $true)][string]$RunDirectory,
        [Parameter(Mandatory = $true)][string]$OpenXrLoaderPath
    )

    return [ordered]@{
        FNVXR_RUN_PROFILE = "stereo-visual-trial-v5"
        FNVXR_HOST_MODE = "vr"
        FNVXR_RUN_ID = $RunId
        FNVXR_RUN_LOG_DIR = $RunDirectory
        FNVXR_OPENXR_LOADER_HINT = $OpenXrLoaderPath
        FNVXR_ENABLE_LEGACY_IMAGE_DIAGNOSTICS = "0"
        FNVXR_ALLOW_STEREO_WORLD_2D_FALLBACK = "0"
        FNVXR_SHOW_GAME_PLANE_ON_STEREO_LOSS = "0"
        FNVXR_STEREO_FALLBACK_MONO_FULLSCREEN = "0"
        FNVXR_TELEMETRY_HAMMER = "0"
        FNVXR_D3D9_TELEMETRY_HAMMER = "0"
    }
}

function Set-FnvxrProductMinimalEnvironment {
    param([Parameter(Mandatory = $true)]$Environment)

    Get-ChildItem Env: | Where-Object { $_.Name -like "FNVXR_*" } | ForEach-Object {
        Remove-Item -LiteralPath ("Env:{0}" -f $_.Name) -ErrorAction SilentlyContinue
    }
    foreach ($key in $Environment.Keys) {
        Set-Item -LiteralPath ("Env:{0}" -f $key) -Value ([string]$Environment[$key])
    }
}

function Test-FnvxrProductProbeReady {
    param(
        [Parameter(Mandatory = $true)][string]$ProbePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$LogPath
    )

    $output = & $ProbePath @Arguments 2>&1 | Out-String
    $exitCode = $LASTEXITCODE
    Add-Content -LiteralPath $LogPath -Value $output -Encoding UTF8
    return $exitCode -eq 0
}

function Wait-FnvxrProductProbeReady {
    param(
        [Parameter(Mandatory = $true)][string]$ProbePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$RequiredProcess,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds,
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string]$Description
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $RequiredProcess.Refresh()
        if ($RequiredProcess.HasExited) {
            throw "$Description failed because $($RequiredProcess.ProcessName) exited with code $($RequiredProcess.ExitCode)."
        }
        if (Test-FnvxrProductProbeReady -ProbePath $ProbePath -Arguments $Arguments -LogPath $LogPath) {
            return
        }
        Start-Sleep -Milliseconds 200
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Timed out after $TimeoutSeconds seconds waiting for $Description."
}

function Stop-FnvxrOwnedProcess {
    param([System.Diagnostics.Process]$Process)

    if (-not $Process) { return }
    try { $Process.Refresh() } catch { return }
    if ($Process.HasExited) { return }
    try {
        if ($Process.MainWindowHandle -ne [IntPtr]::Zero) {
            [void]$Process.CloseMainWindow()
            if ($Process.WaitForExit(3000)) { return }
        }
    } catch {}
    Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
    try { [void]$Process.WaitForExit(3000) } catch {}
}
