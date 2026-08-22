$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

# System.Diagnostics.Process.Modules is not a complete module census when an
# x64 PowerShell supervisor observes a Win32 process. In that configuration it
# can expose only the executable and the WOW64 support modules, omitting the
# target's Win32 loader list. Use the native Toolhelp contract with the
# explicit TH32CS_SNAPMODULE32 flag so exact staged-module evidence is not
# architecture-dependent.
if (-not ("Fnvxr.Product.NativeModuleCensus" -as [type])) {
    Add-Type -Language CSharp -TypeDefinition @"
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Threading;

namespace Fnvxr.Product
{
    public sealed class NativeModuleRecord
    {
        public string Path;
        public string Name;
        public ulong BaseAddress;
        public uint ImageSize;
    }

    public static class NativeModuleCensus
    {
        private const uint TH32CS_SNAPMODULE = 0x00000008;
        private const uint TH32CS_SNAPMODULE32 = 0x00000010;
        private const int ERROR_BAD_LENGTH = 24;
        private const int ERROR_NO_MORE_FILES = 18;
        private const int ERROR_PARTIAL_COPY = 299;

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct MODULEENTRY32W
        {
            public uint dwSize;
            public uint th32ModuleID;
            public uint th32ProcessID;
            public uint GlblcntUsage;
            public uint ProccntUsage;
            public IntPtr modBaseAddr;
            public uint modBaseSize;
            public IntPtr hModule;

            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
            public string szModule;

            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
            public string szExePath;
        }

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr CreateToolhelp32Snapshot(uint flags, uint processId);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool Module32FirstW(IntPtr snapshot, ref MODULEENTRY32W entry);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool Module32NextW(IntPtr snapshot, ref MODULEENTRY32W entry);

        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool CloseHandle(IntPtr handle);

        private static IntPtr OpenSnapshot(uint processId)
        {
            int lastError = 0;
            for (int attempt = 0; attempt != 32; ++attempt)
            {
                IntPtr snapshot = CreateToolhelp32Snapshot(
                    TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
                    processId);
                if (snapshot != new IntPtr(-1))
                    return snapshot;

                lastError = Marshal.GetLastWin32Error();
                if (lastError != ERROR_BAD_LENGTH && lastError != ERROR_PARTIAL_COPY)
                    throw new Win32Exception(lastError);
                Thread.Sleep(1);
            }
            throw new Win32Exception(lastError);
        }

        public static NativeModuleRecord[] Enumerate(uint processId)
        {
            IntPtr snapshot = OpenSnapshot(processId);
            try
            {
                List<NativeModuleRecord> records = new List<NativeModuleRecord>();
                MODULEENTRY32W entry = new MODULEENTRY32W();
                entry.dwSize = (uint)Marshal.SizeOf(typeof(MODULEENTRY32W));
                if (!Module32FirstW(snapshot, ref entry))
                {
                    int firstError = Marshal.GetLastWin32Error();
                    if (firstError == ERROR_NO_MORE_FILES)
                        return records.ToArray();
                    throw new Win32Exception(firstError, "Module32FirstW failed");
                }

                while (true)
                {
                    NativeModuleRecord record = new NativeModuleRecord();
                    record.Path = entry.szExePath;
                    record.Name = entry.szModule;
                    record.BaseAddress = unchecked((ulong)entry.modBaseAddr.ToInt64());
                    record.ImageSize = entry.modBaseSize;
                    records.Add(record);

                    entry.dwSize = (uint)Marshal.SizeOf(typeof(MODULEENTRY32W));
                    if (!Module32NextW(snapshot, ref entry))
                    {
                        int nextError = Marshal.GetLastWin32Error();
                        if (nextError != ERROR_NO_MORE_FILES)
                            throw new Win32Exception(nextError, "Module32NextW failed");
                        break;
                    }
                }
                return records.ToArray();
            }
            finally
            {
                CloseHandle(snapshot);
            }
        }
    }
}
"@
}

function Get-FnvxrProductLoadedModuleCensus {
    param([Parameter(Mandatory = $true)][uint32]$ProcessId)

    if ($ProcessId -eq 0) { throw "Native module census requires a nonzero process id." }
    try {
        foreach ($module in [Fnvxr.Product.NativeModuleCensus]::Enumerate($ProcessId)) {
            if ([string]::IsNullOrWhiteSpace($module.Path)) { continue }
            [pscustomobject][ordered]@{
                path = [System.IO.Path]::GetFullPath([string]$module.Path)
                name = [string]$module.Name
                baseAddress = [uint64]$module.BaseAddress
                imageSize = [uint32]$module.ImageSize
                census = "CreateToolhelp32Snapshot(SNAPMODULE|SNAPMODULE32)"
            }
        }
    } catch {
        throw "Native module census failed for process $ProcessId`: $($_.Exception.Message)"
    }
}

function Get-FnvxrProductRoot {
    return (Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
}

function Get-FnvxrProductApprovedRetailSaveLoadCommand {
    param(
        [Parameter(Mandatory = $true)]
        [ValidateSet("FNVXR_StereoTest")]
        [string]$RetailSaveName
    )

    switch ($RetailSaveName) {
        # The load-only visual-trial route uses only the clean Goodsprings
        # save created by the fixed fresh-character workflow. Suspect legacy
        # saves remain untouched but cannot be selected here.
        "FNVXR_StereoTest" { return "load FNVXR_StereoTest" }
    }
    throw "Unsupported approved retail save name: $RetailSaveName"
}

function Get-FnvxrProductFreshCharacterStartCommand {
    # xNVSE treats CenterOnCell from the start menu as a new no-save game.
    # Keep this exact command owned by the product; the plugin rejects every
    # other fresh-character command and performs the fixed name/save steps.
    return "coc Goodsprings"
}

function Get-FnvxrProductRetailFixtureTraitNames {
    # These are the base FalloutNV.esm trait editor IDs used by the game's own
    # trait script. The fixture path deliberately has no DLC or TTW traits:
    # each fixture is created under a minimal, reproducible retail profile.
    return @(
        "None",
        "BuiltToDestroy",
        "FastShot",
        "FourEyes",
        "GoodNatured",
        "HeavyHanded",
        "Kamikaze",
        "SmallFrame",
        "TriggerDiscipline",
        "WildWasteland")
}

function Resolve-FnvxrProductRetailFixtureTrait {
    param([Parameter(Mandatory = $true)][string]$Trait)

    foreach ($candidate in @(Get-FnvxrProductRetailFixtureTraitNames)) {
        if ([string]::Equals(
                $candidate,
                $Trait,
                [System.StringComparison]::OrdinalIgnoreCase)) {
            return $candidate
        }
    }
    throw "Unsupported retail fixture trait: $Trait"
}

function Resolve-FnvxrProductRetailFixtureTraits {
    param(
        [Parameter(Mandatory = $true)][string]$TraitOne,
        [Parameter(Mandatory = $true)][string]$TraitTwo
    )

    $first = Resolve-FnvxrProductRetailFixtureTrait -Trait $TraitOne
    $second = Resolve-FnvxrProductRetailFixtureTrait -Trait $TraitTwo
    if ($first -cne "None" -and $first -ceq $second) {
        throw "Retail fixture traits must be distinct when both slots are used: $first"
    }
    return [pscustomobject][ordered]@{
        first = $first
        second = $second
    }
}

function Get-FnvxrProductRetailFixtureWeaponNames {
    # Finite stock FalloutNV.esm loadouts only. This selector is a fixture
    # creation input, never a generic console-command or user-save input.
    return @(
        "None",
        "Pistol",
        "RifleSingleHand",
        "RifleTwoHand",
        "Minigun",
        "FragGrenade",
        "Knife",
        "ThrowingKnife")
}

function Resolve-FnvxrProductRetailFixtureWeapon {
    param([Parameter(Mandatory = $true)][string]$Weapon)

    foreach ($candidate in @(Get-FnvxrProductRetailFixtureWeaponNames)) {
        if ([string]::Equals(
                $candidate,
                $Weapon,
                [System.StringComparison]::OrdinalIgnoreCase)) {
            return $candidate
        }
    }
    throw "Unsupported retail fixture weapon: $Weapon"
}

function Get-FnvxrProductRetailFixtureSaveName {
    param(
        [Parameter(Mandatory = $true)][string]$TraitOne,
        [Parameter(Mandatory = $true)][string]$TraitTwo,
        [string]$Weapon = "None"
    )

    $traits = Resolve-FnvxrProductRetailFixtureTraits `
        -TraitOne $TraitOne `
        -TraitTwo $TraitTwo
    $selected = @(
        @($traits.first, $traits.second) |
            Where-Object { $_ -cne "None" } |
            Sort-Object -CaseSensitive)
    $selectedWeapon = Resolve-FnvxrProductRetailFixtureWeapon -Weapon $Weapon
    if ($selected.Count -eq 0) {
        if ($selectedWeapon -ceq "None") {
            return "FNVXR_AutoRetail_L1_Base"
        }
        return "FNVXR_AutoRetail_L1_$selectedWeapon"
    }
    $weaponPrefix = if ($selectedWeapon -ceq "None") { "" } else { $selectedWeapon + "_" }
    return "FNVXR_AutoRetail_L1_" + $weaponPrefix + ($selected -join "_")
}

function Assert-FnvxrProductRetailFixtureSaveName {
    param([Parameter(Mandatory = $true)][string]$SaveName)

    if ($SaveName -notmatch '^FNVXR_AutoRetail_[A-Za-z0-9_]+$' -or
        $SaveName.Length -ge 64) {
        throw "Retail fixture save name is not owned or is too long: $SaveName"
    }
    return $SaveName
}

function Get-FnvxrProductTtwFixtureSaveName {
    param(
        [Parameter(Mandatory = $true)][string]$TraitOne,
        [Parameter(Mandatory = $true)][string]$TraitTwo,
        [string]$Weapon = "None"
    )

    $traits = Resolve-FnvxrProductRetailFixtureTraits `
        -TraitOne $TraitOne `
        -TraitTwo $TraitTwo
    $selected = @(
        @($traits.first, $traits.second) |
            Where-Object { $_ -cne "None" } |
            Sort-Object -CaseSensitive)
    $selectedWeapon = Resolve-FnvxrProductRetailFixtureWeapon -Weapon $Weapon
    if ($selected.Count -eq 0) {
        if ($selectedWeapon -ceq "None") {
            return "FNVXR_AutoTTW_L1_Base"
        }
        return "FNVXR_AutoTTW_L1_$selectedWeapon"
    }
    $weaponPrefix = if ($selectedWeapon -ceq "None") { "" } else { $selectedWeapon + "_" }
    return "FNVXR_AutoTTW_L1_" + $weaponPrefix + ($selected -join "_")
}

function Assert-FnvxrProductTtwFixtureSaveName {
    param([Parameter(Mandatory = $true)][string]$SaveName)

    if ($SaveName -notmatch '^FNVXR_AutoTTW_[A-Za-z0-9_]+$' -or
        $SaveName.Length -ge 64) {
        throw "TTW fixture save name is not owned or is too long: $SaveName"
    }
    return $SaveName
}

function Get-FnvxrProductDocumentsPath {
    # Environment.GetFolderPath can return the unredirected profile default
    # during the first call in a fresh non-interactive PowerShell process.
    # Read the per-user known-folder value directly first so OneDrive and other
    # Windows Documents redirections are honored deterministically.
    $userKey = $null
    $userShellFolders = $null
    try {
        $userKey = [Microsoft.Win32.RegistryKey]::OpenBaseKey(
            [Microsoft.Win32.RegistryHive]::CurrentUser,
            [Microsoft.Win32.RegistryView]::Default)
        $userShellFolders = $userKey.OpenSubKey(
            "Software\Microsoft\Windows\CurrentVersion\Explorer\User Shell Folders",
            $false)
        if ($userShellFolders) {
            $rawPath = [string]$userShellFolders.GetValue(
                "Personal",
                $null,
                [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
            if (-not [string]::IsNullOrWhiteSpace($rawPath)) {
                $expandedPath = [Environment]::ExpandEnvironmentVariables($rawPath)
                if ([System.IO.Path]::IsPathRooted($expandedPath)) {
                    return [System.IO.Path]::GetFullPath($expandedPath)
                }
            }
        }
    } finally {
        if ($userShellFolders) { $userShellFolders.Dispose() }
        if ($userKey) { $userKey.Dispose() }
    }

    $fallbackPath = [Environment]::GetFolderPath("MyDocuments")
    if ([string]::IsNullOrWhiteSpace($fallbackPath) -or
        -not [System.IO.Path]::IsPathRooted($fallbackPath)) {
        throw "Windows did not provide a rooted Documents known-folder path."
    }
    return [System.IO.Path]::GetFullPath($fallbackPath)
}

function Assert-FnvxrProductPhysicalDisplaySize {
    param(
        [ValidateRange(1280, 4096)][int]$Width,
        [ValidateRange(720, 2560)][int]$Height
    )

    $aspect = [double]$Width / [double]$Height
    # Headset projection views are close to square (the attached Quest run
    # reported 1872x2016 per eye).  Accept headset-shaped sources as well as
    # desktop-shaped diagnostic sources; reject only extreme aspect ratios.
    if ($aspect -lt 0.80 -or $aspect -gt 2.0) {
        throw "Physical-play source aspect must be between 0.80 and 2.0: ${Width}x${Height}"
    }
    return [pscustomobject][ordered]@{
        width = $Width
        height = $Height
        aspect = $aspect
        pixelsPerEye = [uint64]$Width * [uint64]$Height
        transportMaximum = "4096x2560"
    }
}

function Get-FnvxrProductPhysicalDisplayIniPaths {
    $falloutRoot = Join-Path (
        Get-FnvxrProductDocumentsPath) "My Games\FalloutNV"
    return @(
        (Join-Path $falloutRoot "Fallout.ini"),
        (Join-Path $falloutRoot "FalloutPrefs.ini"))
}

function ConvertTo-FnvxrProductPhysicalDisplayIniText {
    param(
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Text,
        [ValidateRange(1280, 4096)][int]$Width,
        [ValidateRange(720, 2560)][int]$Height
    )

    Assert-FnvxrProductPhysicalDisplaySize `
        -Width $Width `
        -Height $Height | Out-Null
    $settings = [ordered]@{
        "bFull Screen" = "0"
        "bBorderless" = "1"
        "iPresentInterval" = "0"
        "iSize W" = [string]$Width
        "iSize H" = [string]$Height
        "iMultiSample" = "0"
        "bTransparencyMultisampling" = "0"
        # A desktop-default 75-degree source projection reads as a small
        # window inside the headset.  This temporary physical profile is
        # restored byte-for-byte after every run, along with the resolution.
        "fDefaultFOV" = "110.0000"
        "fDefault1stPersonFOV" = "110.0000"
        "fPipboy1stPersonFOV" = "110.0000"
    }
    $lines = [System.Collections.Generic.List[string]]::new()
    foreach ($line in [regex]::Split($Text, "`r`n|`n|`r")) {
        $remove = $false
        foreach ($key in $settings.Keys) {
            if ($line -match ("^\s*{0}\s*=" -f [regex]::Escape($key))) {
                $remove = $true
                break
            }
        }
        if (-not $remove) {
            $lines.Add([string]$line)
        }
    }

    $displayStart = -1
    $displayEnd = $lines.Count
    for ($index = 0; $index -lt $lines.Count; ++$index) {
        if ($lines[$index] -match '^\s*\[Display\]\s*$') {
            $displayStart = $index
            for ($next = $index + 1; $next -lt $lines.Count; ++$next) {
                if ($lines[$next] -match '^\s*\[.+\]\s*$') {
                    $displayEnd = $next
                    break
                }
            }
            break
        }
    }
    if ($displayStart -lt 0) {
        if ($lines.Count -gt 0 -and
            -not [string]::IsNullOrWhiteSpace($lines[$lines.Count - 1])) {
            $lines.Add("")
        }
        $lines.Add("[Display]")
        $displayEnd = $lines.Count
    }
    $offset = 0
    foreach ($key in $settings.Keys) {
        $lines.Insert(
            $displayEnd + $offset,
            ("{0}={1}" -f $key, $settings[$key]))
        ++$offset
    }

    $result = [string]::Join("`r`n", $lines)
    if (-not $result.EndsWith("`r`n")) {
        $result += "`r`n"
    }
    return $result
}

function Install-FnvxrProductPhysicalDisplayProfile {
    param(
        [Parameter(Mandatory = $true)][string]$BackupRoot,
        [Parameter(Mandatory = $true)][string]$RunId,
        [ValidateRange(1280, 4096)][int]$Width,
        [ValidateRange(720, 2560)][int]$Height
    )

    $size = Assert-FnvxrProductPhysicalDisplaySize `
        -Width $Width `
        -Height $Height
    $paths = @(Get-FnvxrProductPhysicalDisplayIniPaths)
    foreach ($path in $paths) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Physical-play display profile requires an existing Fallout INI: $path"
        }
    }

    $backupDirectory = Join-Path $BackupRoot "physical-display-profile"
    New-Item -ItemType Directory -Path $backupDirectory -Force | Out-Null
    $records = @()
    foreach ($path in $paths) {
        $previous = Get-FnvxrProductFileIdentity -Path $path
        $backup = Join-Path $backupDirectory (
            Split-Path -Leaf $path)
        Copy-Item -LiteralPath $path -Destination $backup -Force
        $backupIdentity = Get-FnvxrProductFileIdentity -Path $backup
        if ($backupIdentity.sha256 -cne $previous.sha256) {
            throw "Physical display-profile backup hash mismatch: $path -> $backup"
        }
        $records += [pscustomobject][ordered]@{
            path = [System.IO.Path]::GetFullPath($path)
            previous = $previous
            backup = $backup
            staged = $null
            changed = $false
        }
    }

    try {
        foreach ($record in $records) {
            $sourceText = [System.IO.File]::ReadAllText(
                [string]$record.path)
            $expectedText =
                ConvertTo-FnvxrProductPhysicalDisplayIniText `
                    -Text $sourceText `
                    -Width $Width `
                    -Height $Height
            $expectedSha256 =
                Get-FnvxrProductStringSha256 -Value $expectedText
            $temporary = "{0}.fnvxr-display-{1}" -f
                $record.path,
                $RunId
            try {
                if (Test-Path -LiteralPath $temporary -PathType Leaf) {
                    Remove-Item -LiteralPath $temporary -Force
                }
                [System.IO.File]::WriteAllText(
                    $temporary,
                    $expectedText,
                    [System.Text.ASCIIEncoding]::new())
                $temporaryIdentity =
                    Get-FnvxrProductFileIdentity -Path $temporary
                if ($temporaryIdentity.sha256 -cne $expectedSha256) {
                    throw "Physical display-profile temporary hash mismatch: $temporary"
                }
                Move-Item `
                    -LiteralPath $temporary `
                    -Destination ([string]$record.path) `
                    -Force
            } finally {
                if (Test-Path -LiteralPath $temporary -PathType Leaf) {
                    Remove-Item -LiteralPath $temporary -Force
                }
            }
            $record.staged = Get-FnvxrProductFileIdentity `
                -Path ([string]$record.path)
            if ($record.staged.sha256 -cne $expectedSha256) {
                throw "Physical display-profile staged hash mismatch: $($record.path)"
            }
            $record.changed =
                $record.previous.sha256 -cne $record.staged.sha256
        }
    } catch {
        foreach ($record in $records) {
            if ($record.backup -and
                (Test-Path -LiteralPath $record.backup -PathType Leaf)) {
                Copy-Item `
                    -LiteralPath $record.backup `
                    -Destination ([string]$record.path) `
                    -Force
            }
        }
        throw
    }

    return [pscustomobject][ordered]@{
        key = "physical-display-profile"
        size = $size
        records = $records
        stagedAtUtc = [DateTime]::UtcNow.ToString("o")
    }
}

function Restore-FnvxrProductPhysicalDisplayProfile {
    param([Parameter(Mandatory = $true)]$Record)

    $restored = @()
    foreach ($item in @($Record.records)) {
        if (-not $item.backup -or
            -not (Test-Path -LiteralPath ([string]$item.backup) -PathType Leaf)) {
            throw "Physical display-profile backup is missing: $($item.path)"
        }
        $backupIdentity =
            Get-FnvxrProductFileIdentity -Path ([string]$item.backup)
        if ($backupIdentity.sha256 -cne $item.previous.sha256) {
            throw "Physical display-profile backup changed before restore: $($item.backup)"
        }
        Copy-Item `
            -LiteralPath ([string]$item.backup) `
            -Destination ([string]$item.path) `
            -Force
        $identity =
            Get-FnvxrProductFileIdentity -Path ([string]$item.path)
        if ($identity.sha256 -cne $item.previous.sha256) {
            throw "Physical display-profile restore hash mismatch: $($item.path)"
        }
        $restored += $identity
    }
    return $restored
}

function Get-FnvxrProductRetailVisualTrialPluginNames {
    # This is the exact official-master order recorded in the approved
    # FNVXR_StereoTest NVSE sidecar.  It intentionally excludes TTW/Fallout 3
    # content: the supervised product target is the installed retail New Vegas
    # game root, not a mod-manager or TTW profile.
    return @(
        "FalloutNV.esm",
        "DeadMoney.esm",
        "HonestHearts.esm",
        "OldWorldBlues.esm",
        "LonesomeRoad.esm",
        "TribalPack.esm",
        "MercenaryPack.esm",
        "ClassicPack.esm",
        "CaravanPack.esm",
        "GunRunnersArsenal.esm")
}

function Get-FnvxrProductRetailFixturePluginNames {
    # A brand-new fixture must not activate DLC pre-order packs. That removes
    # their modal first-load notifications from the automated path entirely.
    return @("FalloutNV.esm")
}

function Get-FnvxrProductRetailVisualTrialPluginsPath {
    $localAppData = [Environment]::GetFolderPath("LocalApplicationData")
    if ([string]::IsNullOrWhiteSpace($localAppData) -or
        -not [System.IO.Path]::IsPathRooted($localAppData)) {
        throw "Windows did not provide a rooted LocalAppData known-folder path."
    }
    return Join-Path ([System.IO.Path]::GetFullPath($localAppData)) "FalloutNV\plugins.txt"
}

function Assert-FnvxrProductRetailVisualTrialPluginData {
    param([Parameter(Mandatory = $true)][string]$GameRoot)

    $dataRoot = Join-Path ([System.IO.Path]::GetFullPath($GameRoot)) "Data"
    foreach ($plugin in @(Get-FnvxrProductRetailVisualTrialPluginNames)) {
        $path = Join-Path $dataRoot $plugin
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Retail visual-trial plugin is missing from the retail game root: $path"
        }
    }
}

function Get-FnvxrProductRetailVisualTrialPluginProfileText {
    return ((Get-FnvxrProductRetailVisualTrialPluginNames) -join "`r`n") + "`r`n"
}

function Install-FnvxrProductRetailVisualTrialPluginProfile {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$BackupRoot,
        [Parameter(Mandatory = $true)][string]$RunId,
        [Parameter(Mandatory = $true)][string]$GameRoot
    )

    # The profile is staged only after the launcher has confirmed that no
    # unrelated Fallout/NVSE process is alive.  It contains only the exact
    # retail masters from the approved save and is restored byte-for-byte in
    # the supervisor's finally block.  No save, NVSE sidecar, or game data
    # record is ever included in this operation.
    Assert-FnvxrProductRetailVisualTrialPluginData -GameRoot $GameRoot
    $destination = [System.IO.Path]::GetFullPath($Path)
    $directory = Split-Path -Parent $destination
    if (-not (Test-Path -LiteralPath $directory -PathType Container)) {
        throw "Retail plugin-profile directory is missing: $directory"
    }
    $expectedText = Get-FnvxrProductRetailVisualTrialPluginProfileText
    $expectedSha256 = Get-FnvxrProductStringSha256 -Value $expectedText
    $previousExists = Test-Path -LiteralPath $destination -PathType Leaf
    $previous = if ($previousExists) {
        Get-FnvxrProductFileIdentity -Path $destination
    } else {
        $null
    }
    if ($previous -and $previous.sha256 -ceq $expectedSha256) {
        return [pscustomobject][ordered]@{
            key = "retail-plugin-profile"
            path = $destination
            entries = @(Get-FnvxrProductRetailVisualTrialPluginNames)
            expectedSha256 = $expectedSha256
            previousExists = $true
            previous = $previous
            backup = $null
            changed = $false
            staged = $previous
        }
    }

    $backup = $null
    if ($previousExists) {
        $backup = Join-Path $BackupRoot "retail-plugin-profile\plugins.txt"
        New-Item -ItemType Directory -Path (Split-Path -Parent $backup) -Force | Out-Null
        Copy-Item -LiteralPath $destination -Destination $backup -Force
        $backupIdentity = Get-FnvxrProductFileIdentity -Path $backup
        if ($backupIdentity.sha256 -cne $previous.sha256) {
            throw "Retail plugin-profile backup hash mismatch: $destination -> $backup"
        }
    }

    $temporary = "$destination.fnvxr-profile-$RunId"
    try {
        if (Test-Path -LiteralPath $temporary -PathType Leaf) {
            Remove-Item -LiteralPath $temporary -Force
        }
        [System.IO.File]::WriteAllText(
            $temporary,
            $expectedText,
            [System.Text.UTF8Encoding]::new($false))
        $temporaryIdentity = Get-FnvxrProductFileIdentity -Path $temporary
        if ($temporaryIdentity.sha256 -cne $expectedSha256) {
            throw "Retail plugin-profile temporary hash mismatch: $temporary"
        }
        Move-Item -LiteralPath $temporary -Destination $destination -Force
    } finally {
        if (Test-Path -LiteralPath $temporary -PathType Leaf) {
            Remove-Item -LiteralPath $temporary -Force
        }
    }

    $staged = Get-FnvxrProductFileIdentity -Path $destination
    if ($staged.sha256 -cne $expectedSha256) {
        throw "Retail plugin-profile staged hash mismatch: $destination"
    }
    return [pscustomobject][ordered]@{
        key = "retail-plugin-profile"
        path = $destination
        entries = @(Get-FnvxrProductRetailVisualTrialPluginNames)
        expectedSha256 = $expectedSha256
        previousExists = [bool]$previousExists
        previous = $previous
        backup = $backup
        changed = $true
        staged = $staged
    }
}

function Restore-FnvxrProductRetailVisualTrialPluginProfile {
    param([Parameter(Mandatory = $true)]$Record)

    if (-not [bool]$Record.changed) {
        return Get-FnvxrProductFileIdentity -Path ([string]$Record.path)
    }

    $destination = [string]$Record.path
    if ([bool]$Record.previousExists) {
        if (-not $Record.backup -or
            -not (Test-Path -LiteralPath ([string]$Record.backup) -PathType Leaf)) {
            throw "Retail plugin-profile backup is missing: $destination"
        }
        Copy-Item -LiteralPath ([string]$Record.backup) -Destination $destination -Force
        $restored = Get-FnvxrProductFileIdentity -Path $destination
        if ($restored.sha256 -cne $Record.previous.sha256) {
            throw "Retail plugin-profile restore hash mismatch: $destination"
        }
        return $restored
    }

    if (Test-Path -LiteralPath $destination -PathType Leaf) {
        Remove-Item -LiteralPath $destination -Force
    }
    if (Test-Path -LiteralPath $destination -PathType Leaf) {
        throw "Retail plugin-profile remains after removal: $destination"
    }
    return $null
}

function Assert-FnvxrProductRetailFixturePluginData {
    param([Parameter(Mandatory = $true)][string]$GameRoot)

    $dataRoot = Join-Path ([System.IO.Path]::GetFullPath($GameRoot)) "Data"
    foreach ($plugin in @(Get-FnvxrProductRetailFixturePluginNames)) {
        $path = Join-Path $dataRoot $plugin
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Retail fixture plugin is missing from the retail game root: $path"
        }
    }
}

function Get-FnvxrProductRetailFixturePluginProfileText {
    return ((Get-FnvxrProductRetailFixturePluginNames) -join "`r`n") + "`r`n"
}

function Install-FnvxrProductRetailFixturePluginProfile {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$BackupRoot,
        [Parameter(Mandatory = $true)][string]$RunId,
        [Parameter(Mandatory = $true)][string]$GameRoot
    )

    Assert-FnvxrProductRetailFixturePluginData -GameRoot $GameRoot
    $destination = [System.IO.Path]::GetFullPath($Path)
    $directory = Split-Path -Parent $destination
    if (-not (Test-Path -LiteralPath $directory -PathType Container)) {
        throw "Retail fixture plugin-profile directory is missing: $directory"
    }
    $expectedText = Get-FnvxrProductRetailFixturePluginProfileText
    $expectedSha256 = Get-FnvxrProductStringSha256 -Value $expectedText
    $previousExists = Test-Path -LiteralPath $destination -PathType Leaf
    $previous = if ($previousExists) {
        Get-FnvxrProductFileIdentity -Path $destination
    } else {
        $null
    }
    if ($previous -and $previous.sha256 -ceq $expectedSha256) {
        return [pscustomobject][ordered]@{
            key = "retail-fixture-plugin-profile"
            path = $destination
            entries = @(Get-FnvxrProductRetailFixturePluginNames)
            expectedSha256 = $expectedSha256
            previousExists = $true
            previous = $previous
            backup = $null
            changed = $false
            staged = $previous
        }
    }

    $backup = $null
    if ($previousExists) {
        $backup = Join-Path $BackupRoot "retail-fixture-plugin-profile\plugins.txt"
        New-Item -ItemType Directory -Path (Split-Path -Parent $backup) -Force | Out-Null
        Copy-Item -LiteralPath $destination -Destination $backup -Force
        $backupIdentity = Get-FnvxrProductFileIdentity -Path $backup
        if ($backupIdentity.sha256 -cne $previous.sha256) {
            throw "Retail fixture plugin-profile backup hash mismatch: $destination -> $backup"
        }
    }

    $temporary = "$destination.fnvxr-fixture-$RunId"
    try {
        if (Test-Path -LiteralPath $temporary -PathType Leaf) {
            Remove-Item -LiteralPath $temporary -Force
        }
        [System.IO.File]::WriteAllText(
            $temporary,
            $expectedText,
            [System.Text.UTF8Encoding]::new($false))
        $temporaryIdentity = Get-FnvxrProductFileIdentity -Path $temporary
        if ($temporaryIdentity.sha256 -cne $expectedSha256) {
            throw "Retail fixture plugin-profile temporary hash mismatch: $temporary"
        }
        Move-Item -LiteralPath $temporary -Destination $destination -Force
    } finally {
        if (Test-Path -LiteralPath $temporary -PathType Leaf) {
            Remove-Item -LiteralPath $temporary -Force
        }
    }

    $staged = Get-FnvxrProductFileIdentity -Path $destination
    if ($staged.sha256 -cne $expectedSha256) {
        throw "Retail fixture plugin-profile staged hash mismatch: $destination"
    }
    return [pscustomobject][ordered]@{
        key = "retail-fixture-plugin-profile"
        path = $destination
        entries = @(Get-FnvxrProductRetailFixturePluginNames)
        expectedSha256 = $expectedSha256
        previousExists = [bool]$previousExists
        previous = $previous
        backup = $backup
        changed = $true
        staged = $staged
    }
}

function Restore-FnvxrProductRetailFixturePluginProfile {
    param([Parameter(Mandatory = $true)]$Record)

    if (-not [bool]$Record.changed) {
        return Get-FnvxrProductFileIdentity -Path ([string]$Record.path)
    }

    $destination = [string]$Record.path
    if ([bool]$Record.previousExists) {
        if (-not $Record.backup -or
            -not (Test-Path -LiteralPath ([string]$Record.backup) -PathType Leaf)) {
            throw "Retail fixture plugin-profile backup is missing: $destination"
        }
        Copy-Item -LiteralPath ([string]$Record.backup) -Destination $destination -Force
        $restored = Get-FnvxrProductFileIdentity -Path $destination
        if ($restored.sha256 -cne $Record.previous.sha256) {
            throw "Retail fixture plugin-profile restore hash mismatch: $destination"
        }
        return $restored
    }

    if (Test-Path -LiteralPath $destination -PathType Leaf) {
        Remove-Item -LiteralPath $destination -Force
    }
    if (Test-Path -LiteralPath $destination -PathType Leaf) {
        throw "Retail fixture plugin-profile remains after removal: $destination"
    }
    return $null
}

function Get-FnvxrProductTtwBaselinePluginNames {
    # Current TTW core order from Wasteland Survival Guide.  This is a
    # deliberately minimal verification profile: it excludes all gameplay,
    # UI, camera, and compatibility additions until core TTW reaches the real
    # Start Menu on its own.
    return @(
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
}

function Assert-FnvxrProductTtwBaselinePluginData {
    param([Parameter(Mandatory = $true)][string]$GameRoot)

    $dataRoot = Join-Path ([System.IO.Path]::GetFullPath($GameRoot)) "Data"
    foreach ($plugin in @(Get-FnvxrProductTtwBaselinePluginNames)) {
        $path = Join-Path $dataRoot $plugin
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "TTW baseline plugin is missing from the isolated game root: $path"
        }
    }
}

function Get-FnvxrProductTtwBaselinePluginProfileText {
    return ((Get-FnvxrProductTtwBaselinePluginNames) -join "`r`n") + "`r`n"
}

function Install-FnvxrProductTtwBaselinePluginProfile {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$BackupRoot,
        [Parameter(Mandatory = $true)][string]$RunId,
        [Parameter(Mandatory = $true)][string]$GameRoot
    )

    # plugins.txt is the engine's load-order selector.  The supervisor stages
    # this exact baseline only after it has established that no pre-existing
    # runtime exists, then restores any prior bytes in its finally path.
    # No save, NVSE sidecar, or source game data is included in this operation.
    Assert-FnvxrProductTtwBaselinePluginData -GameRoot $GameRoot
    $destination = [System.IO.Path]::GetFullPath($Path)
    $directory = Split-Path -Parent $destination
    if (-not (Test-Path -LiteralPath $directory -PathType Container)) {
        throw "TTW baseline plugin-profile directory is missing: $directory"
    }
    $expectedText = Get-FnvxrProductTtwBaselinePluginProfileText
    $expectedSha256 = Get-FnvxrProductStringSha256 -Value $expectedText
    $previousExists = Test-Path -LiteralPath $destination -PathType Leaf
    $previous = if ($previousExists) {
        Get-FnvxrProductFileIdentity -Path $destination
    } else {
        $null
    }
    if ($previous -and $previous.sha256 -ceq $expectedSha256) {
        return [pscustomobject][ordered]@{
            key = "ttw-baseline-plugin-profile"
            path = $destination
            entries = @(Get-FnvxrProductTtwBaselinePluginNames)
            expectedSha256 = $expectedSha256
            previousExists = $true
            previous = $previous
            backup = $null
            changed = $false
            staged = $previous
        }
    }

    $backup = $null
    if ($previousExists) {
        $backup = Join-Path $BackupRoot "ttw-baseline-plugin-profile\plugins.txt"
        New-Item -ItemType Directory -Path (Split-Path -Parent $backup) -Force | Out-Null
        Copy-Item -LiteralPath $destination -Destination $backup -Force
        $backupIdentity = Get-FnvxrProductFileIdentity -Path $backup
        if ($backupIdentity.sha256 -cne $previous.sha256) {
            throw "TTW baseline plugin-profile backup hash mismatch: $destination -> $backup"
        }
    }

    $temporary = "$destination.fnvxr-ttw-baseline-$RunId"
    try {
        if (Test-Path -LiteralPath $temporary -PathType Leaf) {
            Remove-Item -LiteralPath $temporary -Force
        }
        [System.IO.File]::WriteAllText(
            $temporary,
            $expectedText,
            [System.Text.UTF8Encoding]::new($false))
        $temporaryIdentity = Get-FnvxrProductFileIdentity -Path $temporary
        if ($temporaryIdentity.sha256 -cne $expectedSha256) {
            throw "TTW baseline plugin-profile temporary hash mismatch: $temporary"
        }
        Move-Item -LiteralPath $temporary -Destination $destination -Force
    } finally {
        if (Test-Path -LiteralPath $temporary -PathType Leaf) {
            Remove-Item -LiteralPath $temporary -Force
        }
    }

    $staged = Get-FnvxrProductFileIdentity -Path $destination
    if ($staged.sha256 -cne $expectedSha256) {
        throw "TTW baseline plugin-profile staged hash mismatch: $destination"
    }
    return [pscustomobject][ordered]@{
        key = "ttw-baseline-plugin-profile"
        path = $destination
        entries = @(Get-FnvxrProductTtwBaselinePluginNames)
        expectedSha256 = $expectedSha256
        previousExists = [bool]$previousExists
        previous = $previous
        backup = $backup
        changed = $true
        staged = $staged
    }
}

function Restore-FnvxrProductTtwBaselinePluginProfile {
    param([Parameter(Mandatory = $true)]$Record)

    if (-not [bool]$Record.changed) {
        return Get-FnvxrProductFileIdentity -Path ([string]$Record.path)
    }

    $destination = [string]$Record.path
    if ([bool]$Record.previousExists) {
        if (-not $Record.backup -or
            -not (Test-Path -LiteralPath ([string]$Record.backup) -PathType Leaf)) {
            throw "TTW baseline plugin-profile backup is missing: $destination"
        }
        Copy-Item -LiteralPath ([string]$Record.backup) -Destination $destination -Force
        $restored = Get-FnvxrProductFileIdentity -Path $destination
        if ($restored.sha256 -cne $Record.previous.sha256) {
            throw "TTW baseline plugin-profile restore hash mismatch: $destination"
        }
        return $restored
    }

    if (Test-Path -LiteralPath $destination -PathType Leaf) {
        Remove-Item -LiteralPath $destination -Force
    }
    if (Test-Path -LiteralPath $destination -PathType Leaf) {
        throw "TTW baseline plugin-profile remains after removal: $destination"
    }
    return $null
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
    # The exit watcher may be reading or replacing the same manifest while the
    # launcher records a terminal state.  Retain atomic replacement, but treat
    # a transient Windows handle conflict as a bounded retry rather than
    # turning a completed retail run into a failed one.
    $json = $Value | ConvertTo-Json -Depth $Depth
    $lastLockError = $null
    for ($attempt = 1; $attempt -le 6; ++$attempt) {
        $temporary = Join-Path $directory (".{0}.{1}.tmp" -f
            [System.IO.Path]::GetFileName($fullPath), [Guid]::NewGuid().ToString("N"))
        try {
            $json | Set-Content -LiteralPath $temporary -Encoding UTF8 -ErrorAction Stop
            Move-Item -LiteralPath $temporary -Destination $fullPath -Force -ErrorAction Stop
            return
        } catch [System.IO.IOException] {
            $lastLockError = $_
            if ($attempt -eq 6) { throw }
            Start-Sleep -Milliseconds (50 * $attempt)
        } finally {
            if (Test-Path -LiteralPath $temporary -PathType Leaf) {
                Remove-Item -LiteralPath $temporary -Force -ErrorAction SilentlyContinue
            }
        }
    }
    throw $lastLockError
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
    $paths.Add((Join-Path $resolvedRoot "README.md"))
    foreach ($name in @("docs", "host", "plugin", "protocol", "renderhook", "runtime", "scripts", "tests", "tools")) {
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
        [pscustomobject]@{ key = "x64/fnvxr_command.exe"; path = Join-Path $x64 "fnvxr_command.exe"; machine = "0x8664" },
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

function Test-FnvxrProductProcessElevated {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Get-FnvxrProductHklmActiveRuntimeSnapshot {
    $records = @()
    foreach ($view in @(
        [Microsoft.Win32.RegistryView]::Registry64,
        [Microsoft.Win32.RegistryView]::Registry32)) {
        $baseKey = $null
        $openXrKey = $null
        try {
            $baseKey = [Microsoft.Win32.RegistryKey]::OpenBaseKey(
                [Microsoft.Win32.RegistryHive]::LocalMachine,
                $view)
            $openXrKey = $baseKey.OpenSubKey(
                "SOFTWARE\Khronos\OpenXR\1",
                $false)
            $present = $false
            $value = $null
            if ($openXrKey) {
                $present = @($openXrKey.GetValueNames()) -contains "ActiveRuntime"
                if ($present) {
                    $value = [string]$openXrKey.GetValue(
                        "ActiveRuntime",
                        $null,
                        [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
                }
            }
            $records += [pscustomobject][ordered]@{
                hive = "HKLM"
                view = [string]$view
                key = "SOFTWARE\Khronos\OpenXR\1"
                valueName = "ActiveRuntime"
                present = [bool]$present
                value = $value
            }
        } finally {
            if ($openXrKey) { $openXrKey.Dispose() }
            if ($baseKey) { $baseKey.Dispose() }
        }
    }
    return [pscustomobject][ordered]@{
        schema = "fnvxr-hklm-openxr-active-runtime-v1"
        records = $records
    }
}

function Assert-FnvxrProductHklmActiveRuntimeUnchanged {
    param([Parameter(Mandatory = $true)]$Before)

    $after = Get-FnvxrProductHklmActiveRuntimeSnapshot
    $beforeJson = $Before | ConvertTo-Json -Depth 6 -Compress
    $afterJson = $after | ConvertTo-Json -Depth 6 -Compress
    if (-not [string]::Equals(
            $beforeJson,
            $afterJson,
            [System.StringComparison]::Ordinal)) {
        throw "HKLM OpenXR ActiveRuntime changed during a process-local product run; no registry mutation is authorized."
    }
    return $after
}

function Resolve-FnvxrProductHeadlessRuntimeManifest {
    param([Parameter(Mandatory = $true)][string]$ManifestPath)

    if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
        throw "Headless OpenXR runtime manifest is missing: $ManifestPath"
    }
    $resolvedManifestPath = (Resolve-Path -LiteralPath $ManifestPath).Path
    try {
        $document = Get-Content -LiteralPath $resolvedManifestPath -Raw |
            ConvertFrom-Json -ErrorAction Stop
    } catch {
        throw "Headless OpenXR runtime manifest is not valid JSON: $resolvedManifestPath"
    }
    if (-not ($document.PSObject.Properties.Name -ccontains "file_format_version") -or
        [string]$document.file_format_version -cne "1.0.0") {
        throw "Headless OpenXR runtime manifest must use file_format_version 1.0.0: $resolvedManifestPath"
    }
    if (-not ($document.PSObject.Properties.Name -ccontains "runtime") -or
        -not $document.runtime -or
        -not ($document.runtime.PSObject.Properties.Name -ccontains "library_path") -or
        [string]::IsNullOrWhiteSpace([string]$document.runtime.library_path)) {
        throw "Headless OpenXR runtime manifest has no runtime.library_path: $resolvedManifestPath"
    }

    $declaredLibraryPath = [string]$document.runtime.library_path
    $libraryPath = if ([System.IO.Path]::IsPathRooted($declaredLibraryPath)) {
        $declaredLibraryPath
    } else {
        Join-Path (Split-Path -Parent $resolvedManifestPath) $declaredLibraryPath
    }
    if (-not (Test-Path -LiteralPath $libraryPath -PathType Leaf)) {
        throw "Headless OpenXR runtime DLL is missing: $libraryPath"
    }
    $runtimeIdentity = Get-FnvxrProductFileIdentity -Path $libraryPath -RequirePe
    if ($runtimeIdentity.peMachine -cne "0x8664") {
        throw "Headless OpenXR runtime DLL is not x64: $($runtimeIdentity.path)"
    }

    return [pscustomobject][ordered]@{
        selection = "process-local-environment-only"
        headless = $true
        registryMutationAuthorized = $false
        manifest = Get-FnvxrProductFileIdentity -Path $resolvedManifestPath
        declaredLibraryPath = $declaredLibraryPath
        runtimeDll = $runtimeIdentity
    }
}

function Resolve-FnvxrProductPhysicalRuntimeManifest {
    param([Parameter(Mandatory = $true)][string]$ManifestPath)

    if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
        throw "Physical OpenXR runtime manifest is missing: $ManifestPath"
    }
    $resolvedManifestPath = (Resolve-Path -LiteralPath $ManifestPath).Path
    try {
        $document = Get-Content -LiteralPath $resolvedManifestPath -Raw |
            ConvertFrom-Json -ErrorAction Stop
    } catch {
        throw "Physical OpenXR runtime manifest is not valid JSON: $resolvedManifestPath"
    }
    if (-not ($document.PSObject.Properties.Name -ccontains "file_format_version") -or
        [string]$document.file_format_version -cne "1.0.0") {
        throw "Physical OpenXR runtime manifest must use file_format_version 1.0.0: $resolvedManifestPath"
    }
    if (-not ($document.PSObject.Properties.Name -ccontains "runtime") -or
        -not $document.runtime -or
        -not ($document.runtime.PSObject.Properties.Name -ccontains "library_path") -or
        [string]::IsNullOrWhiteSpace([string]$document.runtime.library_path)) {
        throw "Physical OpenXR runtime manifest has no runtime.library_path: $resolvedManifestPath"
    }

    $declaredLibraryPath = [string]$document.runtime.library_path
    $libraryPath = if ([System.IO.Path]::IsPathRooted($declaredLibraryPath)) {
        $declaredLibraryPath
    } else {
        Join-Path (Split-Path -Parent $resolvedManifestPath) $declaredLibraryPath
    }
    if (-not (Test-Path -LiteralPath $libraryPath -PathType Leaf)) {
        throw "Physical OpenXR runtime DLL is missing: $libraryPath"
    }
    $runtimeIdentity = Get-FnvxrProductFileIdentity -Path $libraryPath -RequirePe
    if ($runtimeIdentity.peMachine -cne "0x8664") {
        throw "Physical OpenXR runtime DLL is not x64: $($runtimeIdentity.path)"
    }

    return [pscustomobject][ordered]@{
        selection = "process-local-environment-only"
        headless = $false
        registryMutationAuthorized = $false
        manifest = Get-FnvxrProductFileIdentity -Path $resolvedManifestPath
        declaredLibraryPath = $declaredLibraryPath
        runtimeDll = $runtimeIdentity
    }
}

function Resolve-FnvxrProductMetaXrOperatorLayer {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$LayerDirectory
    )

    if ([string]::IsNullOrWhiteSpace($LayerDirectory) -or
        -not [System.IO.Path]::IsPathRooted($LayerDirectory)) {
        throw "Meta XR Operator layer directory must be an absolute workspace path."
    }
    $resolvedRoot = [System.IO.Path]::GetFullPath($Root)
    $allowedRoot = [System.IO.Path]::GetFullPath(
        (Join-Path $resolvedRoot "local\meta-xr-operator"))
    $resolvedLayerDirectory = [System.IO.Path]::GetFullPath($LayerDirectory)
    $allowedPrefix = $allowedRoot.TrimEnd('\', '/') +
        [System.IO.Path]::DirectorySeparatorChar
    if (-not $resolvedLayerDirectory.StartsWith(
            $allowedPrefix,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Meta XR Operator layer directory must remain in the workspace-owned dependency root: $allowedRoot"
    }

    $stageRoot = Split-Path -Parent $resolvedLayerDirectory
    $stageManifestPath = Join-Path $stageRoot "fnvxr-meta-xr-operator-stage.json"
    if (-not (Test-Path -LiteralPath $stageManifestPath -PathType Leaf)) {
        throw "Meta XR Operator layer requires its owned stage manifest: $stageManifestPath"
    }
    try {
        $stageManifest = Get-Content -LiteralPath $stageManifestPath -Raw |
            ConvertFrom-Json -ErrorAction Stop
    } catch {
        throw "Meta XR Operator stage manifest is not valid JSON: $stageManifestPath"
    }
    if ($stageManifest.schema -cne "fnvxr-meta-xr-operator-stage-v1" -or
        $stageManifest.operatorLayerDirectory -cne $resolvedLayerDirectory -or
        $stageManifest.apiLayerName -cne "XR_APILAYER_METAX_operator") {
        throw "Meta XR Operator stage manifest does not authorize this exact layer directory: $stageManifestPath"
    }

    $manifestPath = Join-Path $resolvedLayerDirectory "XrApiLayer_METAX_operator.json"
    $libraryPath = Join-Path $resolvedLayerDirectory "XrApiLayer_METAX_operator.dll"
    $proxyPath = Join-Path $resolvedLayerDirectory "meta-xr-operator-mcp-proxy.exe"
    foreach ($path in @($manifestPath, $libraryPath, $proxyPath)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Meta XR Operator staged layer is incomplete: $path"
        }
    }
    try {
        $document = Get-Content -LiteralPath $manifestPath -Raw |
            ConvertFrom-Json -ErrorAction Stop
    } catch {
        throw "Meta XR Operator API-layer manifest is not valid JSON: $manifestPath"
    }
    if ($document.file_format_version -cne "1.0.0" -or
        $document.api_layer.name -cne "XR_APILAYER_METAX_operator" -or
        $document.api_layer.library_path -cne "./XrApiLayer_METAX_operator.dll") {
        throw "Meta XR Operator API-layer manifest identity is not exact: $manifestPath"
    }

    $manifestIdentity = Get-FnvxrProductFileIdentity -Path $manifestPath
    $libraryIdentity = Get-FnvxrProductFileIdentity -Path $libraryPath -RequirePe
    $proxyIdentity = Get-FnvxrProductFileIdentity -Path $proxyPath -RequirePe
    if ($libraryIdentity.peMachine -cne "0x8664" -or
        $proxyIdentity.peMachine -cne "0x8664") {
        throw "Meta XR Operator staged layer must contain x64 Windows binaries."
    }
    if ($stageManifest.staged.manifest.sha256 -cne $manifestIdentity.sha256 -or
        $stageManifest.staged.library.sha256 -cne $libraryIdentity.sha256 -or
        $stageManifest.staged.proxy.sha256 -cne $proxyIdentity.sha256) {
        throw "Meta XR Operator staged layer no longer matches its owned stage manifest."
    }

    return [pscustomobject][ordered]@{
        schema = "fnvxr-meta-xr-operator-layer-v1"
        directory = $resolvedLayerDirectory
        apiLayerName = "XR_APILAYER_METAX_operator"
        stageManifest = Get-FnvxrProductFileIdentity -Path $stageManifestPath
        manifest = $manifestIdentity
        library = $libraryIdentity
        proxy = $proxyIdentity
        observationOnly = $true
        mcpProxyLaunched = $false
        controllerOrPoseOverrideAuthorized = $false
    }
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

    $sandbox = $null
    $sandboxManifestPath = Join-Path $resolved "fnvxr-retail-sandbox-manifest.json"
    if (Test-Path -LiteralPath $sandboxManifestPath -PathType Leaf) {
        $sandboxManifest = Get-Content -LiteralPath $sandboxManifestPath -Raw |
            ConvertFrom-Json -ErrorAction Stop
        if ([string]$sandboxManifest.schema -cne "fnvxr-retail-sandbox/v1" -or
            -not [string]::Equals(
                [System.IO.Path]::GetFullPath([string]$sandboxManifest.sandboxRoot),
                $resolved,
                [System.StringComparison]::OrdinalIgnoreCase) -or
            [bool]$sandboxManifest.sourceRootsMutated -or
            [bool]$sandboxManifest.processOrUiControl) {
            throw "Retail sandbox manifest does not retain its isolated/no-control contract: $sandboxManifestPath"
        }
        $sourceRoot = [System.IO.Path]::GetFullPath(
            [string]$sandboxManifest.sourceRoot)
        if ([string]::Equals(
                $sourceRoot,
                $resolved,
                [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Retail sandbox source and destination roots must be distinct: $resolved"
        }
        $steamAppIdPath = Join-Path $resolved "steam_appid.txt"
        if (-not (Test-Path -LiteralPath $steamAppIdPath -PathType Leaf) -or
            (Get-Content -LiteralPath $steamAppIdPath -Raw).Trim() -cne "22380") {
            throw "Retail sandbox must contain steam_appid.txt with only AppID 22380 so Steam cannot redirect the run into the installed Library copy: $steamAppIdPath"
        }
        if ([string]$sandboxManifest.falloutSha256 -cne $fallout.sha256) {
            throw "Retail sandbox Fallout hash no longer matches its manifest."
        }
        $sandbox = [pscustomobject][ordered]@{
            manifest = Get-FnvxrProductFileIdentity -Path $sandboxManifestPath
            sourceRoot = $sourceRoot
            steamAppId = "22380"
            steamAppIdFile = Get-FnvxrProductFileIdentity -Path $steamAppIdPath
            steamLibraryRedirectForbidden = $true
            sourceRootsMutated = $false
            processOrUiControl = $false
        }
    }

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
        sandbox = $sandbox
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

function Get-FnvxrProductRetailFixtureStagePlan {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$Configuration,
        [Parameter(Mandatory = $true)][string]$GameRoot
    )

    # A command-line fixture needs only the xNVSE plugin. Do not stage the
    # D3D9, DirectInput, or XInput proxies: that keeps OpenXR, rendering, and
    # input systems completely out of the fixture process.
    $x86 = Join-Path $Root "build-product-win32\$Configuration"
    $pluginRoot = Join-Path $GameRoot "Data\NVSE\Plugins"
    return @(
        [pscustomobject]@{
            key = "x86/nvse_fnvxr.dll"
            source = Join-Path $x86 "nvse_fnvxr.dll"
            destination = Join-Path $pluginRoot "nvse_fnvxr.dll"
            machine = "0x014c"
        })
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
        $stageFailure = $_.Exception.Message
        if ($records.Count -gt 0) {
            try {
                Restore-FnvxrProductArtifactSet -Records $records
            } catch {
                throw (
                    "Artifact staging failed: {0}. Rollback also failed after attempting every staged record: {1}" -f
                    $stageFailure,
                    $_.Exception.Message)
            }
        }
        throw
    }
}

function Restore-FnvxrProductArtifactSet {
    param([Parameter(Mandatory = $true)][object[]]$Records)

    # A broken backup must never prevent cleanup of the remaining staged files.
    # Attempt every record in reverse stage order, then report the complete set
    # of failures so a supervisor cannot mistake a partial rollback for a pass.
    # Fallout can exit before Windows Error Reporting releases its mapped DLL
    # handles. Retry only the two Win32 sharing-lock failures for a bounded
    # interval; every other restore error remains immediate and fail-closed.
    $failures = @()
    foreach ($record in @($Records | Select-Object -Last 999 | Sort-Object key -Descending)) {
        try {
            $destination = [string]$record.destination.path
            if ($record.previousExists) {
                if (-not $record.backup -or -not (Test-Path -LiteralPath $record.backup -PathType Leaf)) {
                    throw "Cannot restore staged artifact; backup is missing: $destination"
                }
                Invoke-FnvxrProductSharingViolationRetry -Action {
                    Copy-Item -LiteralPath $record.backup -Destination $destination -Force
                }
                $restored = Get-FnvxrProductFileIdentity -Path $destination
                if ($restored.sha256 -cne $record.previous.sha256) {
                    throw "Restored artifact hash mismatch: $destination"
                }
            } elseif (Test-Path -LiteralPath $destination -PathType Leaf) {
                Invoke-FnvxrProductSharingViolationRetry -Action {
                    Remove-Item -LiteralPath $destination -Force
                }
                if (Test-Path -LiteralPath $destination -PathType Leaf) {
                    throw "New staged artifact remains after removal: $destination"
                }
            }
        } catch {
            $failures += ("{0}: {1}" -f [string]$record.key, $_.Exception.Message)
        }
    }
    if ($failures.Count -gt 0) {
        throw (
            "Failed to restore {0} staged artifact(s); attempted every record. Failures: {1}" -f
            $failures.Count,
            ($failures -join " | "))
    }
}

function Invoke-FnvxrProductSharingViolationRetry {
    param(
        [Parameter(Mandatory = $true)][scriptblock]$Action,
        [ValidateRange(0, 10000)][int]$TimeoutMilliseconds = 5000,
        [ValidateRange(10, 1000)][int]$RetryMilliseconds = 100
    )

    $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMilliseconds)
    do {
        try {
            & $Action
            return
        } catch {
            $exception = $_.Exception
            $sharingViolation = $false
            while ($exception) {
                $win32Code = $exception.HResult -band 0xffff
                if ($exception -is [System.IO.IOException] -and
                    ($win32Code -eq 32 -or $win32Code -eq 33)) {
                    $sharingViolation = $true
                    break
                }
                $exception = $exception.InnerException
            }
            if (-not $sharingViolation -or
                [DateTime]::UtcNow -ge $deadline) {
                throw
            }
            Start-Sleep -Milliseconds $RetryMilliseconds
        }
    } while ($true)
}

function Get-FnvxrProductExactFalloutProcess {
    param([Parameter(Mandatory = $true)][string]$ExpectedPath)

    foreach ($candidate in @(Get-Process FalloutNV -ErrorAction SilentlyContinue)) {
        try {
            if ([string]::Equals(
                    $candidate.Path,
                    $ExpectedPath,
                    [System.StringComparison]::OrdinalIgnoreCase)) {
                return $candidate
            }
        } catch {}
    }
    return $null
}

function Wait-FnvxrProductExactLoadedModule {
    param(
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$Process,
        [Parameter(Mandatory = $true)][string]$ExpectedPath,
        [Parameter(Mandatory = $true)][string]$ExpectedSha256,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds
    )

    $expectedFullPath = [System.IO.Path]::GetFullPath($ExpectedPath)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $Process.Refresh()
        if ($Process.HasExited) {
            throw "Fallout exited before loading the owned retail-fixture module: $ExpectedPath"
        }
        foreach ($module in @(Get-FnvxrProductLoadedModuleCensus -ProcessId ([uint32]$Process.Id))) {
            if ([string]::Equals(
                    $module.path,
                    $expectedFullPath,
                    [System.StringComparison]::OrdinalIgnoreCase)) {
                $identity = Get-FnvxrProductFileIdentity -Path $module.path -RequirePe
                if ($identity.sha256 -cne $ExpectedSha256) {
                    throw "Loaded retail-fixture module hash differs from the staged module: $ExpectedPath"
                }
                return $identity
            }
        }
        Start-Sleep -Milliseconds 200
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Timed out waiting for the exact owned retail-fixture module: $ExpectedPath"
}

function Get-FnvxrProductProbeSnapshot {
    param(
        [Parameter(Mandatory = $true)][string]$ProbePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$LogPath
    )

    $output = & $ProbePath @Arguments 2>&1 | Out-String
    $exitCode = $LASTEXITCODE
    Add-Content -LiteralPath $LogPath -Value $output -Encoding UTF8
    $json = $null
    try { $json = $output | ConvertFrom-Json -ErrorAction Stop } catch {}
    return [pscustomobject][ordered]@{
        exitCode = $exitCode
        json = $json
    }
}

function Wait-FnvxrProductProbeCondition {
    param(
        [Parameter(Mandatory = $true)][string]$ProbePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$RequiredProcess,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds,
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string]$Description,
        [Parameter(Mandatory = $true)][scriptblock]$Accept
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $RequiredProcess.Refresh()
        if ($RequiredProcess.HasExited) {
            $exitCode = try { [string]$RequiredProcess.ExitCode } catch { "unknown" }
            throw "$Description failed because FalloutNV:$($RequiredProcess.Id) exited with code $exitCode."
        }
        $sample = Get-FnvxrProductProbeSnapshot `
            -ProbePath $ProbePath `
            -Arguments $Arguments `
            -LogPath $LogPath
        if ($sample.exitCode -eq 0 -and $sample.json -and (& $Accept $sample.json)) {
            return $sample
        }
        Start-Sleep -Milliseconds 200
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Timed out after $TimeoutSeconds seconds waiting for $Description."
}

function Wait-FnvxrProductRetailFixtureStartMenu {
    param(
        [Parameter(Mandatory = $true)][string]$ProbePath,
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$RequiredProcess,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds,
        [Parameter(Mandatory = $true)][string]$LogPath
    )

    return Wait-FnvxrProductProbeCondition `
        -ProbePath $ProbePath `
        -Arguments @("--require-runtime", "--require-advancing", "--sample-delay-ms", "100") `
        -RequiredProcess $RequiredProcess `
        -TimeoutSeconds $TimeoutSeconds `
        -LogPath $LogPath `
        -Description "a real retail Start Menu before the owned fixture command" `
        -Accept {
            param($snapshot)
            return [bool]$snapshot.runtime.present `
                -and [bool]$snapshot.runtime.stable `
                -and [bool]$snapshot.runtime.usable `
                -and [uint64]$snapshot.runtime.frame -gt 0 `
                -and [uint32]$snapshot.runtime.phase -eq 1 `
                -and (([uint32]$snapshot.runtime.menuBits -band 2) -ne 0) `
                -and [bool]$snapshot.runtime.uiInputAllowed `
                -and -not [bool]$snapshot.runtime.showroomActive
        }
}

function Wait-FnvxrProductRetailFixtureGameplay {
    param(
        [Parameter(Mandatory = $true)][string]$ProbePath,
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$RequiredProcess,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds,
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][uint64]$MinimumFrame
    )

    # The fixture profile intentionally publishes no player bridge. The
    # native fixture authority verifies player/cell/camera state in-process
    # before saving; this outer guard proves stable, advancing gameplay.
    return Wait-FnvxrProductProbeCondition `
        -ProbePath $ProbePath `
        -Arguments @("--require-runtime", "--require-advancing", "--sample-delay-ms", "100") `
        -RequiredProcess $RequiredProcess `
        -TimeoutSeconds $TimeoutSeconds `
        -LogPath $LogPath `
        -Description "a real owned retail-fixture gameplay scene" `
        -Accept {
            param($snapshot)
            return [bool]$snapshot.runtime.present `
                -and [bool]$snapshot.runtime.stable `
                -and [bool]$snapshot.runtime.usable `
                -and [uint64]$snapshot.runtime.frame -gt $MinimumFrame `
                -and [uint32]$snapshot.runtime.phase -eq 3 `
                # RuntimeMenuModeBit (0x01) is diagnostic only and the shared
                # protocol explicitly excludes it from RuntimeBlockingMenuBits.
                -and (([uint32]$snapshot.runtime.menuBits -band 0xFE) -eq 0) `
                -and -not [bool]$snapshot.runtime.uiInputAllowed `
                -and [bool]$snapshot.runtime.cameraActive `
                -and -not [bool]$snapshot.runtime.showroomActive
        }
}

function Wait-FnvxrProductRetailFixtureSavePair {
    param(
        [Parameter(Mandatory = $true)][string]$SavePath,
        [Parameter(Mandatory = $true)][string]$NvsePath,
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$RequiredProcess,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds
    )

    # A loadable xNVSE fixture is a pair, not merely a .fos that happened to
    # appear while the engine was still writing. Require both files to be
    # nonempty and unchanged across a short second sample before ending the
    # owned fixture process. Historical saves are never touched here.
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $RequiredProcess.Refresh()
        if ($RequiredProcess.HasExited) {
            throw "Fallout exited before the owned retail fixture save pair appeared: $SavePath, $NvsePath"
        }
        if ((Test-Path -LiteralPath $SavePath -PathType Leaf) -and
            (Test-Path -LiteralPath $NvsePath -PathType Leaf)) {
            $saveBefore = Get-FnvxrProductFileIdentity -Path $SavePath
            $nvseBefore = Get-FnvxrProductFileIdentity -Path $NvsePath
            if ($saveBefore.length -gt 0 -and $nvseBefore.length -gt 0) {
                Start-Sleep -Milliseconds 500
                $RequiredProcess.Refresh()
                if ($RequiredProcess.HasExited) {
                    throw "Fallout exited while the owned retail fixture save pair was settling: $SavePath, $NvsePath"
                }
                if ((Test-Path -LiteralPath $SavePath -PathType Leaf) -and
                    (Test-Path -LiteralPath $NvsePath -PathType Leaf)) {
                    $saveAfter = Get-FnvxrProductFileIdentity -Path $SavePath
                    $nvseAfter = Get-FnvxrProductFileIdentity -Path $NvsePath
                    if ($saveBefore.sha256 -ceq $saveAfter.sha256 -and
                        $saveBefore.length -eq $saveAfter.length -and
                        $nvseBefore.sha256 -ceq $nvseAfter.sha256 -and
                        $nvseBefore.length -eq $nvseAfter.length) {
                        return [ordered]@{
                            save = $saveAfter
                            nvse = $nvseAfter
                            settled = $true
                        }
                    }
                }
            }
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)

    throw "Timed out waiting for a stable owned retail fixture save and NVSE sidecar: $SavePath, $NvsePath"
}

function Get-FnvxrProductMinimalEnvironment {
    param(
        [Parameter(Mandatory = $true)][string]$RunId,
        [Parameter(Mandatory = $true)][string]$RunDirectory,
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$OpenXrLoaderPath,
        [Parameter(Mandatory = $true)]
        [ValidateRange(5, 900)]
        [int]$SessionReadyTimeoutSeconds,
        [switch]$AutomateRecoveryLoad,
        [switch]$AutomateFreshCharacter,
        [switch]$AutomateRetailFixture,
        [switch]$TtwFixture,
        [switch]$HeadsetDemoFixture,
        [switch]$HeadsetWorldOnlyCapture,
        [switch]$HeadsetFixtureWeaponDraw,
        [ValidateSet("None", "Primary", "Alternate", "Third")]
        [string]$RetailVrFirstPersonPrivateCaller = "None",
        [switch]$StockFirstPersonBaseline,
        [switch]$HeadsetControllerRigVisualTrial,
        [switch]$HeadsetInventoryVisualTrial,
        [switch]$HeadsetCombatVisualTrial,
        [switch]$PhysicalHeadsetPlay,
        [ValidateRange(1280, 4096)][int]$PhysicalGameWidth = 1872,
        [ValidateRange(720, 2560)][int]$PhysicalGameHeight = 2016,
        [ValidateRange(1, 1200)]
        [int]$HeadsetDemoGameplayWarmupFrames = 90,
        [ValidateSet(
            "Full",
            "RelayOnly",
            "CaptureOnly",
            "PrepareOnly",
            "RenderNoPublish",
            "SnapshotOnly",
            "CollectOnly",
            "BindOnly",
            "CameraOnly",
            "PopulateOnly",
            "RenderOnly",
            "FinalizeOnly",
            "LeftEyeOnly")]
        [string]$RetailVrAccumulationDiagnosticMode = "Full",
        [ValidateSet("disabled", "create", "load")]
        [string]$RetailFixtureAction = "disabled",
        [string]$RetailFixtureSaveName = "",
        [string]$RetailFixtureTraitOne = "None",
        [string]$RetailFixtureTraitTwo = "None",
        [ValidateSet(
            "None", "Pistol", "RifleSingleHand", "RifleTwoHand", "Minigun",
            "FragGrenade", "Knife", "ThrowingKnife")]
        [string]$RetailFixtureWeapon = "None",
        [switch]$AcknowledgeTribalPackPopup,
        [ValidateSet("FNVXR_StereoTest")]
        [string]$AutomateRecoverySaveName = "FNVXR_StereoTest",
        [string]$HeadlessRuntimeManifest = "",
        # Uses the simulator runtime's native SBS desktop preview while
        # retaining the same process-local runtime manifest and file IPC.
        # It never changes the retail renderer, assets, or simulator menus.
        [switch]$SimulatorDesktopPreview,
        [string]$PhysicalRuntimeManifest = "",
        [string]$HeadsetMirrorCaptureDirectory = "",
        [ValidateRange(1, 600)][int]$HeadsetMirrorCaptureEveryFrames = 6,
        [ValidateRange(1, 3600)][int]$HeadsetMirrorCaptureMaxPairs = 180,
        [string]$MetaXrOperatorLayerDirectory = ""
    )

    $automationModes = @()
    if ($AutomateRecoveryLoad) { $automationModes += "recovery-load" }
    if ($AutomateFreshCharacter) { $automationModes += "fresh-character" }
    if ($AutomateRetailFixture) { $automationModes += "retail-fixture" }
    if ($automationModes.Count -gt 1) {
        throw "Recovery-load, fresh-character, and retail-fixture automation are mutually exclusive."
    }
    if ($AcknowledgeTribalPackPopup -and -not $AutomateRecoveryLoad) {
        throw "The exact official-pack acknowledgement requires recovery-load automation."
    }
    $headsetFixtureVisualTrial =
        [bool]($HeadsetDemoFixture -or $HeadsetWorldOnlyCapture)
    if ($HeadsetDemoFixture -and $HeadsetWorldOnlyCapture) {
        throw "The headset demo and world-only capture are mutually exclusive."
    }
    if ($HeadsetFixtureWeaponDraw -and -not $HeadsetWorldOnlyCapture) {
        throw "The headset fixture weapon draw requires world-only capture."
    }
    if ($headsetFixtureVisualTrial -and -not $AutomateRetailFixture) {
        throw "The headset fixture visual trial requires the owned retail-fixture automation."
    }
    if ($HeadsetFixtureWeaponDraw -and -not $AutomateRetailFixture) {
        throw "The headset fixture weapon draw requires the owned retail-fixture automation."
    }
    if ($HeadsetControllerRigVisualTrial -and -not $HeadsetFixtureWeaponDraw) {
        throw "The headless controller visual-rig trial requires the owned fixture weapon draw."
    }
    if ($HeadsetControllerRigVisualTrial -and
        [string]::IsNullOrWhiteSpace($HeadlessRuntimeManifest)) {
        throw "The controller visual-rig trial requires the process-local headless runtime."
    }
    if ($HeadsetControllerRigVisualTrial -and $PhysicalHeadsetPlay) {
        throw "The controller visual-rig trial is headless-only and cannot combine with physical headset play."
    }
    if ($HeadsetCombatVisualTrial -and -not $HeadsetControllerRigVisualTrial) {
        throw "The headless combat visual trial requires the controller visual-rig trial."
    }
    if ($HeadsetInventoryVisualTrial -and -not $HeadsetControllerRigVisualTrial) {
        throw "The headless inventory visual trial requires the controller visual-rig trial."
    }
    if ($HeadsetInventoryVisualTrial -and $HeadsetCombatVisualTrial) {
        throw "The headless inventory and automated combat visual trials are mutually exclusive."
    }
    if ($HeadsetFixtureWeaponDraw -and $RetailFixtureAction -cne "load") {
        throw "The headset fixture weapon draw requires an existing owned fixture load."
    }
    if ($HeadsetFixtureWeaponDraw -and $RetailFixtureWeapon -ceq "None") {
        throw "The headset fixture weapon draw requires a named stock fixture weapon."
    }
    if (-not $HeadsetFixtureWeaponDraw -and $RetailVrFirstPersonPrivateCaller -cne "None") {
        throw "The private first-person caller selector requires the headset fixture weapon draw."
    }
    if ($StockFirstPersonBaseline -and
        (-not $HeadsetWorldOnlyCapture -or
         -not $HeadsetFixtureWeaponDraw -or
         -not $AutomateRetailFixture -or
         [string]::IsNullOrWhiteSpace($HeadlessRuntimeManifest))) {
        throw "The stock first-person baseline requires the headless world-only fixture weapon capture."
    }
    if ($StockFirstPersonBaseline -and
        ($RetailVrFirstPersonPrivateCaller -cne "Third" -or
         $HeadsetControllerRigVisualTrial -or $PhysicalHeadsetPlay)) {
        throw "The stock first-person baseline requires the Third caller and forbids controller-rig/physical mutation."
    }
    if ($PhysicalHeadsetPlay -and -not $AutomateRetailFixture) {
        throw "Physical headset play requires an owned retail-fixture load."
    }
    if ($PhysicalHeadsetPlay -and $headsetFixtureVisualTrial) {
        throw "Physical headset play and headless headset visual trials are mutually exclusive."
    }
    if ($PhysicalHeadsetPlay -and
        [string]::IsNullOrWhiteSpace($PhysicalRuntimeManifest)) {
        throw "Physical headset play requires a process-local physical OpenXR runtime manifest."
    }
    if ($PhysicalHeadsetPlay) {
        Assert-FnvxrProductPhysicalDisplaySize `
            -Width $PhysicalGameWidth `
            -Height $PhysicalGameHeight | Out-Null
    }
    if (-not [string]::IsNullOrWhiteSpace($HeadlessRuntimeManifest) -and
        -not [string]::IsNullOrWhiteSpace($PhysicalRuntimeManifest)) {
        throw "Headless and physical OpenXR runtime manifests are mutually exclusive."
    }
    if ($SimulatorDesktopPreview -and
        [string]::IsNullOrWhiteSpace($HeadlessRuntimeManifest)) {
        throw "The simulator desktop preview requires the simulator runtime manifest."
    }
    if ($RetailVrAccumulationDiagnosticMode -cne "Full" -and
        -not $HeadsetDemoFixture) {
        throw "The retail VR accumulation diagnostic mode requires the bounded headset demo."
    }
    if ($TtwFixture -and -not $AutomateRetailFixture) {
        throw "The TTW fixture family requires the owned fixture automation."
    }
    if ($AutomateRetailFixture -and -not $headsetFixtureVisualTrial -and
        -not $PhysicalHeadsetPlay -and
        -not [string]::IsNullOrWhiteSpace($HeadsetMirrorCaptureDirectory)) {
        throw "The isolated retail fixture profile cannot capture a headset mirror."
    }

    $runProfile = if ($AutomateRetailFixture -and
        -not $headsetFixtureVisualTrial -and
        -not $PhysicalHeadsetPlay) {
        if ($TtwFixture) { "ttw-fixture-v1" } else { "retail-fixture-v1" }
    } elseif ($PhysicalHeadsetPlay) {
        "retail-vr-play-v1"
    } else {
        "stereo-visual-trial-v5"
    }
    $hostMode = if ($AutomateRetailFixture -and
        -not $headsetFixtureVisualTrial -and
        -not $PhysicalHeadsetPlay) {
        if ($TtwFixture) { "ttw-fixture" } else { "retail-fixture" }
    } else {
        "vr"
    }
    $engineCenterStereo = if ($AutomateRetailFixture -and
        -not $headsetFixtureVisualTrial -and
        -not $PhysicalHeadsetPlay) {
        "0"
    } else {
        "1"
    }
    $environment = [ordered]@{
        FNVXR_RUN_PROFILE = $runProfile
        FNVXR_HOST_MODE = $hostMode
        FNVXR_RUN_ID = $RunId
        FNVXR_RUN_LOG_DIR = $RunDirectory
        # Product input is delivered through OpenXR/shared memory and the
        # background-capable DirectInput/XInput bridges.  This fuse is checked
        # again inside both native processes before any OS window activation,
        # cursor movement, SendInput, or PostMessage fallback.
        FNVXR_WINDOWS_FOREGROUND_INPUT_FORBIDDEN = "1"
        FNVXR_DINPUT_FORCE_BACKGROUND = "1"
        FNVXR_DINPUT_VIRTUAL_OWNER = "1"
        FNVXR_CURSOR_TRACK_POINTER = "0"
        FNVXR_CURSOR_FOCUS = "0"
        FNVXR_CLICK_FOCUS_ON_CLICK = "0"
        FNVXR_CLICK_SENDINPUT_MOUSE = "0"
        FNVXR_PLUGIN_SENDINPUT_CLICK = "0"
        FNVXR_POST_MENU_KEYS = "0"
        FNVXR_POST_WINDOW_MOUSE_FALLBACK = "0"
        FNVXR_IMMEDIATE_OS_CLICK = "0"
        FNVXR_HOST_CURSOR_CLICK_ENABLED = "0"
        FNVXR_HOST_CURSOR_SET_POS = "0"
        FNVXR_HOST_CURSOR_ABSOLUTE_MOVE = "0"
        FNVXR_HOST_CURSOR_TRACK_POINTER = "0"
        FNVXR_HOST_CURSOR_FOCUS = "0"
        FNVXR_HOST_SENDINPUT_CLICK = "0"
        FNVXR_HOST_CURSOR_CLICK_FALLBACK = "0"
        FNVXR_SESSION_READY_TIMEOUT_SECONDS = [string]$SessionReadyTimeoutSeconds
        FNVXR_ENABLE_LEGACY_IMAGE_DIAGNOSTICS = "0"
        FNVXR_ENABLE_ENGINE_CENTER_STEREO = $engineCenterStereo
        FNVXR_ALLOW_STEREO_WORLD_2D_FALLBACK = "0"
        FNVXR_SHOW_GAME_PLANE_ON_STEREO_LOSS = "0"
        FNVXR_STEREO_FALLBACK_MONO_FULLSCREEN = "0"
        # Bilinear final-eye sampling prevents terrain texture shimmer when a
        # runtime rounds or otherwise adjusts its recommended eye extent.
        # Point sampling remains available only as an explicit diagnostic.
        FNVXR_GAME_PLANE_SHARP_FILTER = "0"
        # The CPU producer replaces a sequenced stereo pair while the OpenXR
        # host polls independently. A read that lands inside that atomic
        # replacement is not stereo loss: retain the last identity-validated
        # pair until the bounded pose-age gate expires. Clearing it caused the
        # observed alternating gun/HUD and zero-layer pulse.
        FNVXR_STEREO_RETAIN_LAST_VALID_ON_REJECT = "1"
        FNVXR_STEREO_STALE_FRAME_LIMIT = "30"
        FNVXR_CPU_STEREO_MAX_SOURCE_POSE_AGE_MS = "250"
        FNVXR_TELEMETRY_HAMMER = "0"
        FNVXR_D3D9_TELEMETRY_HAMMER = "0"
    }
    # The isolated fixture runner has no OpenXR route.  The separately opted-in
    # headset demo still uses the normal visual-trial host, so retain the
    # loader hint for that one bounded recording route.
    if (-not $AutomateRetailFixture -or
        $headsetFixtureVisualTrial -or
        $PhysicalHeadsetPlay) {
        $environment.FNVXR_OPENXR_LOADER_HINT = $OpenXrLoaderPath
    }
    if ($PhysicalHeadsetPlay) {
        # Interactive input is consumed directly by the exact-retail xNVSE
        # game-thread bridge. The shipping XInput/DirectInput DLL shims remain
        # transparent and are not granted production mutation authority.
        $environment.FNVXR_PHYSICAL_HEADSET_PLAY = "1"
        # Physical retail reaches the authentic first-person weapon pass via
        # the third audited outer caller (0x00870F74). Hold the world pair for
        # that pass so the CPU-v8 producer can publish a complete world plus
        # weapon image instead of leaving the OpenXR host with zero layers.
        $environment.FNVXR_RETAIL_VR_FIRST_PERSON_PRIVATE_CALLER = "third"
        # The physical profile owns the full compatibility-checked game-thread
        # bridge. Its post-animation rig consumes live controller poses while
        # the engine-center camera independently consumes the HMD pose.
        $environment.FNVXR_RETAIL_CENTER_INTEGRATED_FIRST_PERSON = "1"
        # Keep one post-xrEndFrame engine-center submit per transaction for
        # the Phase 1 physical evidence verifier. This is deliberately
        # narrower than the general telemetry hammer.
        $environment.FNVXR_PHASE1_TRACE_TELEMETRY = "1"
        $environment.FNVXR_EXTERNAL_XINPUT_WRITER = "1"
        $environment.FNVXR_EXTERNAL_DINPUT_WRITER = "1"
        # Let Fallout consume the Quest left stick through its native analog
        # XInput lane. The proxy still masks plugin-owned combat controls.
        $environment.FNVXR_XINPUT_NATIVE_LOCOMOTION = "1"
        $environment.FNVXR_XINPUT_MASK_PLUGIN_OWNED_TRIGGERS = "1"
        # CPU-v8 frames can arrive one game-frame late during controller and
        # weapon animation bursts. Retain that proven stereo pair briefly so
        # the compositor does not alternate between stereo and zero layers.
        $environment.FNVXR_CPU_STEREO_MAX_SOURCE_POSE_AGE_MS = "250"
        $environment.FNVXR_PLUGIN_KEYBOARD_MOVEMENT_ENABLE = "1"
        # The live Quest sample was 0.25 forward (8,257 in XInput units),
        # below the former 9,000 cutoff.  Six thousand still excludes normal
        # stick drift while accepting deliberate walking input.
        $environment.FNVXR_PLUGIN_MOVEMENT_DEADZONE = "6000"
        $environment.FNVXR_PLUGIN_MENU_KEYBOARD_FALLBACK = "1"
        $environment.FNVXR_PLUGIN_GAMEPLAY_KEYBOARD_FALLBACK = "1"
        $environment.FNVXR_PLUGIN_ACCEPT_ON_EXTERNAL_DINPUT_CLICK = "1"
        $environment.FNVXR_XINPUT_PHYSICAL_MENU_BUTTONS_ENABLE = "1"
        # One left-controller Menu press opens the real Inventory Pip-Boy;
        # the same button closes it while the Pip-Boy is active.
        $environment.FNVXR_PHYSICAL_LEFT_MENU_PIPBOY_ENABLE = "1"
        $environment.FNVXR_L3_MENU_FALLBACK = "1"
        # Physical gameplay owns discrete comfort turning: one exact 30-degree
        # actor-heading change per deflection, with neutral required to rearm.
        $environment.FNVXR_RIGHT_STICK_KEY_TURN = "0"
        # Full stick magnitude selects Fallout's native running movement flag
        # in every direction; shallow input remains walking speed.
        $environment.FNVXR_GAMEPLAY_RIGHT_GRIP_GRAB_ENABLE = "1"
        # The host already publishes a tracked right-hand pointer into the
        # exact retail UI lane.  Keep its hover and native accept path live.
        $environment.FNVXR_DIRECT_UI_CLICK = "1"
        # Keep the authored stock weapon and world in the engine binocular
        # transaction. The host-owned tracked categories above replace the
        # unstable arm/hand/Pip-Boy collector roots and spatialize the live
        # retail screen crop directly on the opposite wrist.
        $environment.FNVXR_LIVE_PIPBOY_FOCUS_FRAMES = "12"
        $environment.FNVXR_WEAPON_ORBIT_GRIP_THRESHOLD = "0.55"
        $environment.FNVXR_WEAPON_ORBIT_DEADZONE = "0.35"
        $environment.FNVXR_UI_SHARED_WIDTH = "1280"
        $environment.FNVXR_UI_SHARED_HEIGHT = "720"
        $environment.FNVXR_UI_INPUT_WIDTH =
            [string]$PhysicalGameWidth
        $environment.FNVXR_UI_INPUT_HEIGHT =
            [string]$PhysicalGameHeight
        $environment.FNVXR_GAME_TEXTURE_WIDTH =
            [string]$PhysicalGameWidth
        $environment.FNVXR_GAME_TEXTURE_HEIGHT =
            [string]$PhysicalGameHeight

        # The engine-center camera transaction owns the complete OpenXR
        # yaw/pitch/roll plus translation. Disable every 2D mouse-look lane so
        # pitch cannot be duplicated, filtered away, or applied on another
        # frame.
        $environment.FNVXR_D3D9_NATIVE_APPLY_HEAD_ROTATION = "1"
        $environment.FNVXR_D3D9_NATIVE_HEAD_AXIS_MODE =
            "openxr-to-ni-camera"
        $environment.FNVXR_D3D9_USE_SHARED_CAMERA_VIEW = "0"
        $environment.FNVXR_D3D9_APPLY_HMD_POSE = "0"
        $environment.FNVXR_HEADSPACE_LOOK_ENABLE = "0"
        $environment.FNVXR_HANDSPACE_LOOK_ENABLE = "0"
        $environment.FNVXR_GYRO_AIM_ENABLE = "0"
        $environment.FNVXR_DINPUT_HEAD_LOOK_ENABLE = "0"
        $environment.FNVXR_DINPUT_HANDSPACE_LOOK_ENABLE = "0"
        $environment.FNVXR_DINPUT_GYRO_AIM_ENABLE = "0"
        $environment.FNVXR_DINPUT_RIGHT_STICK_LOOK_ENABLE = "0"
        $environment.FNVXR_DINPUT_RIGHT_STICK_PITCH_ENABLE = "0"
        $environment.FNVXR_XINPUT_RIGHT_STICK_Y_ENABLE = "0"
    }
    if (($headsetFixtureVisualTrial -and -not $StockFirstPersonBaseline) -or
        $PhysicalHeadsetPlay) {
        # The manual center collector can preserve the stock weapon root, but
        # its separately skinned arm/hand/Pip-Boy categories have invalid
        # transient skinning state and visibly blink or stretch. Replace only
        # those failed categories in the final OpenXR eye pass. The overlay is
        # driven directly from tracked OpenXR poses and samples the live retail
        # Pip-Boy screen crop; it has no window, focus, cursor, or OS-input lane.
        $environment.FNVXR_SPATIAL_HANDS_OVERLAY = "1"
        $environment.FNVXR_SHOW_WORLD_PROPS = "1"
        $environment.FNVXR_SHOW_BODY_RIG = "1"
        $environment.FNVXR_SHOW_FULL_ARMS = "0"
        $environment.FNVXR_SHOW_HAND_FINGERS = "1"
        $environment.FNVXR_SHOW_PIPBOY_RIG = "1"
        $environment.FNVXR_SHOW_LEFT_AIM_RAY = "0"
        $environment.FNVXR_SHOW_RIGHT_AIM_RAY = "0"
        $environment.FNVXR_DEBUG_AXES = "0"
        $environment.FNVXR_DEBUG_LEFT_AXES = "0"
        $environment.FNVXR_PIPBOY_WRIST_UI_WIDTH = "0.12"
        $environment.FNVXR_PIPBOY_WRIST_UI_ROT_X = "0"
        $environment.FNVXR_PIPBOY_WRIST_UI_ROT_Y = "0"
        $environment.FNVXR_PIPBOY_WRIST_UI_ROT_Z = "0"
        $environment.FNVXR_PIPBOY_WRIST_UI_OFFSET_X = "-0.11"
        $environment.FNVXR_PIPBOY_WRIST_UI_OFFSET_Y = "0.065"
        $environment.FNVXR_PIPBOY_WRIST_UI_OFFSET_Z = "-0.025"
        $retailHandAssetRoot = Join-Path `
            (Get-FnvxrProductRoot) `
            "local\retail-assets"
        $retailLeftHandMesh = Join-Path `
            $retailHandAssetRoot `
            "lefthandpipboy-hand.fhm"
        $retailRightHandMesh = Join-Path `
            $retailHandAssetRoot `
            "righthand1st-grip.fhm"
        $retailHandTexture = Join-Path `
            $retailHandAssetRoot `
            "HandMale.dds"
        $retailLeftCuffMesh = Join-Path `
            $retailHandAssetRoot `
            "lefthandpipboy-cuff.fhm"
        $retailLeftCuffTexture = Join-Path `
            $retailHandAssetRoot `
            "PipBoyGlove01.dds"
        $retailPipBoyMesh = Join-Path `
            $retailHandAssetRoot `
            "pipboyarm.fpm"
        $retailPipBoyScreenMesh = Join-Path `
            $retailHandAssetRoot `
            "pipboyscreen.fps"
        $retailPipBoyTexture = Join-Path `
            $retailHandAssetRoot `
            "PipBoyArm01.dds"
        if ((Test-Path -LiteralPath $retailLeftHandMesh -PathType Leaf) -and
            (Test-Path -LiteralPath $retailRightHandMesh -PathType Leaf) -and
            (Test-Path -LiteralPath $retailLeftCuffMesh -PathType Leaf) -and
            (Test-Path -LiteralPath $retailLeftCuffTexture -PathType Leaf)) {
            # These are local derived meshes generated from the user's own
            # installed retail BSA. They are never staged into the game or
            # distributed by the source tree; the host validates their exact
            # binary schema before creating immutable vertex buffers.
            $environment.FNVXR_RETAIL_LEFT_HAND_MESH_PATH =
                [System.IO.Path]::GetFullPath($retailLeftHandMesh)
            $environment.FNVXR_RETAIL_RIGHT_HAND_MESH_PATH =
                [System.IO.Path]::GetFullPath($retailRightHandMesh)
            $environment.FNVXR_RETAIL_LEFT_CUFF_MESH_PATH =
                [System.IO.Path]::GetFullPath($retailLeftCuffMesh)
            $environment.FNVXR_RETAIL_LEFT_CUFF_TEXTURE_PATH =
                [System.IO.Path]::GetFullPath($retailLeftCuffTexture)
            if (Test-Path -LiteralPath $retailHandTexture -PathType Leaf) {
                $environment.FNVXR_RETAIL_HAND_TEXTURE_PATH =
                    [System.IO.Path]::GetFullPath($retailHandTexture)
            }
        }
        if ((Test-Path -LiteralPath $retailPipBoyMesh -PathType Leaf) -and
            (Test-Path -LiteralPath $retailPipBoyScreenMesh -PathType Leaf) -and
            (Test-Path -LiteralPath $retailPipBoyTexture -PathType Leaf)) {
            $environment.FNVXR_RETAIL_PIPBOY_MESH_PATH =
                [System.IO.Path]::GetFullPath($retailPipBoyMesh)
            $environment.FNVXR_RETAIL_PIPBOY_SCREEN_MESH_PATH =
                [System.IO.Path]::GetFullPath($retailPipBoyScreenMesh)
            $environment.FNVXR_RETAIL_PIPBOY_TEXTURE_PATH =
                [System.IO.Path]::GetFullPath($retailPipBoyTexture)
        }
        $environment.FNVXR_RETAIL_LEFT_HAND_OFFSET_X = "0"
        $environment.FNVXR_RETAIL_LEFT_HAND_OFFSET_Y = "0"
        $environment.FNVXR_RETAIL_LEFT_HAND_OFFSET_Z = "0"
        $environment.FNVXR_RETAIL_RIGHT_HAND_OFFSET_X = "-0.040"
        $environment.FNVXR_RETAIL_RIGHT_HAND_OFFSET_Y = "0.026"
        $environment.FNVXR_RETAIL_RIGHT_HAND_OFFSET_Z = "0"
    }
    if ($AutomateRecoveryLoad) {
        # This opt-in does not enable the general command or input bridge.
        # The publication-only plugin path accepts only a compile-time
        # approved retail save load.  A post-load acknowledgement remains off
        # unless the separate, exact official-pack opt-in is also supplied.
        $environment.FNVXR_STEREO_VISUAL_TRIAL_AUTOMATE_RECOVERY_LOAD = "1"
        $environment.FNVXR_STEREO_VISUAL_TRIAL_AUTOMATE_RECOVERY_SAVE_NAME =
            $AutomateRecoverySaveName
    }
    if ($AcknowledgeTribalPackPopup) {
        # This legacy-named switch authorizes only the native acknowledgement
        # of each exact observed official pre-order pack notification and its
        # unique native first-button OK tile.
        # It does not enable desktop, keyboard, mouse, controller, or
        # simulator input.
        $environment.FNVXR_STEREO_VISUAL_TRIAL_ACK_TRIBAL_PACK_POPUP = "1"
    }
    if ($AutomateFreshCharacter) {
        # This opt-in maps the same narrow mailbox, but the visual-trial
        # plugin accepts only the fixed start command and owns its fixed
        # no-menu COC/name/save sequence. It does not enable input, camera,
        # rig, weapon, or general console authority.
        $environment.FNVXR_STEREO_VISUAL_TRIAL_AUTOMATE_FRESH_CHARACTER = "1"
    }
    if ($AutomateRetailFixture) {
        $fixtureTraits = Resolve-FnvxrProductRetailFixtureTraits `
            -TraitOne $RetailFixtureTraitOne `
            -TraitTwo $RetailFixtureTraitTwo
        $fixtureWeapon = Resolve-FnvxrProductRetailFixtureWeapon `
            -Weapon $RetailFixtureWeapon
        $fixtureSaveName = if ($TtwFixture) {
            Assert-FnvxrProductTtwFixtureSaveName -SaveName $RetailFixtureSaveName
        } else {
            Assert-FnvxrProductRetailFixtureSaveName -SaveName $RetailFixtureSaveName
        }
        $environment.FNVXR_RETAIL_FIXTURE_AUTOMATION = "1"
        $environment.FNVXR_RETAIL_FIXTURE_ACTION = $RetailFixtureAction
        $environment.FNVXR_RETAIL_FIXTURE_SAVE_NAME = $fixtureSaveName
        $environment.FNVXR_RETAIL_FIXTURE_TRAIT_ONE = $fixtureTraits.first
        $environment.FNVXR_RETAIL_FIXTURE_TRAIT_TWO = $fixtureTraits.second
        $environment.FNVXR_RETAIL_FIXTURE_WEAPON = $fixtureWeapon
        # Both the base-game and exact TTW-core fixture profiles can present
        # one of four known stock pre-order-pack notices after an owned load.
        # The plugin accepts only an exact title/body pair and its unique
        # native first-button OK target; it does not send an OS, controller,
        # keyboard, mouse, or simulator input event.
        $environment.FNVXR_RETAIL_FIXTURE_ACK_OFFICIAL_PACK_POPUP = "1"
        if ($TtwFixture) {
            # TTW can present one exact, versioned dependency warning after an
            # owned load. The plugin must independently match its complete
            # title/body and unique native first-button OK tile. This grants
            # no general MessageMenu, console, OS, or device-input authority.
            $environment.FNVXR_RETAIL_FIXTURE_ACK_TTW_STEWIE_DEPENDENCY_WARNING =
                "1"
        }
        if ($headsetFixtureVisualTrial) {
            $environment.FNVXR_HEADSET_DEMO_FIXTURE = "1"
        }
        if ($HeadsetDemoFixture) {
            # Exercise the same no-quad source contract under the isolated
            # process-local simulator before asking a physical headset to
            # accept it. The demo still owns only its two fixed native
            # InterfaceManager open/close actions.
            $environment.FNVXR_LIVE_PIPBOY_FOCUS_FRAMES = "12"
            # Publish the already-rendered world plus authentic first-person
            # roots at the same audited third outer caller used by physical
            # play. This is presentation only; the demo still has no combat
            # or general controller authority.
            $environment.FNVXR_RETAIL_VR_FIRST_PERSON_PRIVATE_CALLER =
                "third"
            $environment.FNVXR_RETAIL_CENTER_INTEGRATED_FIRST_PERSON = "1"
            $environment.FNVXR_HEADSET_DEMO_GAMEPLAY_WARMUP_FRAMES =
                [string]$HeadsetDemoGameplayWarmupFrames
            $environment.FNVXR_HEADSET_DEMO_PIPBOY_HOLD_FRAMES = "240"
            if ($RetailVrAccumulationDiagnosticMode -cne "Full") {
                $environment.FNVXR_RETAIL_VR_ACCUMULATION_DIAGNOSTIC_MODE =
                    switch ($RetailVrAccumulationDiagnosticMode) {
                        "RelayOnly" { "relay-only" }
                        "CaptureOnly" { "capture-only" }
                        "PrepareOnly" { "prepare-only" }
                        "RenderNoPublish" { "render-no-publish" }
                        "SnapshotOnly" { "snapshot-only" }
                        "CollectOnly" { "collect-only" }
                        "BindOnly" { "bind-only" }
                        "CameraOnly" { "camera-only" }
                        "PopulateOnly" { "populate-only" }
                        "RenderOnly" { "render-only" }
                        "FinalizeOnly" { "finalize-only" }
                        "LeftEyeOnly" { "left-eye-only" }
                    }
            }
        }
        if ($HeadsetWorldOnlyCapture) {
            # This second key closes the demo UI gate inside xNVSE while
            # retaining the exact owned-fixture/OpenXR route above.
            $environment.FNVXR_HEADSET_WORLD_ONLY_CAPTURE = "1"
        }
        if ($HeadsetFixtureWeaponDraw) {
            # This is consumed only by the post-load world-only fixture
            # finisher. It may save once to the same verified owned Load after
            # clean gameplay, then issue JIP's one fixed SetWeaponOut command
            # for its named, currently holstered fixture weapon.
            $environment.FNVXR_HEADSET_FIXTURE_DRAW_WEAPON = "1"
            # A completed stock backbuffer is not a weapon-bearing engine
            # stereo proof. The renderer hard-fuses this producer off; keep
            # an explicit zero in the weapon route so inherited user
            # environment state cannot revive it.
            $environment.FNVXR_HEADSET_FINAL_STOCK_FRAME_CAPTURE = "0"
            $environment.FNVXR_RETAIL_FIRST_PERSON_RAW_EYE_CAPTURE = "1"
            $environment.FNVXR_RETAIL_FIRST_PERSON_DRAW_TRACE_LIMIT = "4096"
            if ($RetailVrFirstPersonPrivateCaller -cne "None") {
                $environment.FNVXR_RETAIL_VR_FIRST_PERSON_PRIVATE_CALLER =
                    $RetailVrFirstPersonPrivateCaller.ToLowerInvariant()
            }
        }
        if ($StockFirstPersonBaseline) {
            $environment.FNVXR_STOCK_FIRST_PERSON_BASELINE = "1"
            $environment.FNVXR_RETAIL_VR_FIRST_PERSON_PRIVATE_CALLER = "third"
            $environment.FNVXR_RETAIL_CENTER_INTEGRATED_FIRST_PERSON = "0"
            $environment.FNVXR_RETAIL_RIG_ENABLE = "0"
            $environment.FNVXR_RETAIL_RIG_APPLY = "0"
            $environment.FNVXR_RETAIL_WEAPON_APPLY = "0"
        }
        if ($HeadsetControllerRigVisualTrial) {
            # A fixture-only visual lease. The OpenXR host still owns pose
            # sampling and final presentation; the D3D bridge owns stereo
            # cameras. The host owns its retail-derived hands/Pip-Boy, while
            # xNVSE may restore only the stock weapon from current aim poses.
            $environment.FNVXR_HEADSET_CONTROLLER_RIG_VISUAL_TRIAL = "1"
            # Select the observed outer seam so the already accumulated
            # engine-center eye pair can be published there. The center-
            # integrated branch returns before the legacy private-eye fanout,
            # so this selector produces privateEyeCalls=0 and one stock gun.
            $environment.FNVXR_RETAIL_VR_FIRST_PERSON_PRIVATE_CALLER = "third"
            $environment.FNVXR_RETAIL_CENTER_INTEGRATED_FIRST_PERSON = "1"
            $environment.FNVXR_INSTALL_CAMERA_HOOK = "0"
            $environment.FNVXR_CAMERA_HOOK = "0"
            $environment.FNVXR_CAMERA_APPLY = "0"
            $environment.FNVXR_RETAIL_RIG_ENABLE = "1"
            $environment.FNVXR_RETAIL_RIG_APPLY = "1"
            $environment.FNVXR_RETAIL_WEAPON_APPLY = "1"
            $environment.FNVXR_RETAIL_PROJECTILE_NODE_HOOK = "0"
            $environment.FNVXR_TRACKED_PROP_ASSIST_PROJECTILE_OR_HIT_MUTATION = "0"
            $environment.FNVXR_NVSE_WRITES_VR_POSE = "0"
            $environment.FNVXR_CLICK_SENDINPUT_MOUSE = "0"
            $environment.FNVXR_PLUGIN_SENDINPUT_CLICK = "0"
            $environment.FNVXR_EXTERNAL_XINPUT_WRITER = "0"
            $environment.FNVXR_EXTERNAL_DINPUT_WRITER = "0"
            $environment.FNVXR_DESKTOP_ASSIST_AUTOMATION = "0"
            $environment.FNVXR_D3D9_STEREO_REPLAY = "0"
            $environment.FNVXR_D3D9_NATIVE_SINGLE_TRAVERSAL_REPLAY = "0"
            $environment.FNVXR_D3D9_WIDE_WORLD_REPLAY = "0"
            $environment.FNVXR_DESKTOP_ASSIST_UI_CAPTURE = "0"
            if ($HeadsetInventoryVisualTrial) {
                # This lease leaves the per-run simulator stream manual so an
                # authentic Pip-Boy inventory selection can be observed.
                $environment.FNVXR_HEADSET_INVENTORY_VISUAL_TRIAL = "1"
                $environment.FNVXR_LEFT_GRIP_PIPBOY_MODE = "0"
                $environment.FNVXR_XINPUT_LEFT_GRIP_PIPBOY_ENABLE = "0"
                $environment.FNVXR_XINPUT_PHYSICAL_MENU_BUTTONS_ENABLE = "1"
                $environment.FNVXR_PLUGIN_KEYBOARD_MOVEMENT_ENABLE = "1"
                $environment.FNVXR_PLUGIN_MENU_KEYBOARD_FALLBACK = "1"
                $environment.FNVXR_PLUGIN_GAMEPLAY_KEYBOARD_FALLBACK = "1"
                $environment.FNVXR_PLUGIN_ACCEPT_ON_EXTERNAL_DINPUT_CLICK = "1"
                $environment.FNVXR_DIRECT_UI_CLICK = "1"
                $environment.FNVXR_BUFFERED_DIRECTINPUT_CALL = "1"
            }
            if ($HeadsetCombatVisualTrial) {
                # A bounded owned-fixture input lease. The OpenXR host remains
                # the sole controller payload producer; xNVSE translates RT
                # and X through Fallout's normal attack/reload key path. Do
                # not set the generic external-writer flags here: they are
                # deliberately incompatible with the base visual-rig lease.
                $environment.FNVXR_HEADSET_COMBAT_VISUAL_TRIAL = "1"
                $environment.FNVXR_PLUGIN_KEYBOARD_MOVEMENT_ENABLE = "1"
                $environment.FNVXR_PLUGIN_MENU_KEYBOARD_FALLBACK = "1"
                $environment.FNVXR_PLUGIN_GAMEPLAY_KEYBOARD_FALLBACK = "1"
            }
        }
    }
    if (-not [string]::IsNullOrWhiteSpace($HeadlessRuntimeManifest)) {
        $environment.XR_RUNTIME_JSON = $HeadlessRuntimeManifest
        # Keep the simulator logically headless/command-driven even when its
        # SBS preview is visible for recording. Logical XR focus, tracked pose
        # publication, and controller IPC must not depend on whether the
        # diagnostic preview window is shown.
        $environment.OPENXR_SIMULATOR_HEADLESS = "1"
        if ($SimulatorDesktopPreview) {
            $environment.OPENXR_SIMULATOR_DESKTOP_PREVIEW = "1"
        }
        $environment.OPENXR_SIMULATOR_DATA_DIR =
            Join-Path $RunDirectory "openxr-simulator"
        $environment.OPENXR_SIMULATOR_LOG_PATH =
            Join-Path $RunDirectory "openxr-simulator.log"
    }
    if (-not [string]::IsNullOrWhiteSpace($PhysicalRuntimeManifest)) {
        # A physical runtime receives only the standard process-local loader
        # override. Simulator-only headless/data/log variables are absent.
        $environment.XR_RUNTIME_JSON = $PhysicalRuntimeManifest
    }
    if (-not [string]::IsNullOrWhiteSpace($HeadsetMirrorCaptureDirectory)) {
        $environment.FNVXR_HMD_MIRROR_CAPTURE_DIR = $HeadsetMirrorCaptureDirectory
        $environment.FNVXR_HMD_MIRROR_CAPTURE_EVERY_N_FRAMES =
            [string]$HeadsetMirrorCaptureEveryFrames
        $environment.FNVXR_HMD_MIRROR_CAPTURE_MAX_PAIRS =
            [string]$HeadsetMirrorCaptureMaxPairs
    }
    if (-not [string]::IsNullOrWhiteSpace($MetaXrOperatorLayerDirectory)) {
        # This API layer is observation-only in FNVXR: the launcher never
        # starts its MCP proxy or calls pose/controller automation tools.
        $environment.XR_API_LAYER_PATH = $MetaXrOperatorLayerDirectory
        $environment.XR_ENABLE_API_LAYERS = "XR_APILAYER_METAX_operator"
    }
    return $environment
}

function Get-FnvxrProductProcessEnvironmentEntries {
    param([string]$Prefix = "")

    # The native process environment can legally contain differently-cased
    # duplicates (for example Path and PATH). PowerShell's Env: provider tries
    # to copy that block into a case-insensitive dictionary and throws before
    # filtering it. The .NET process-environment table remains enumerable in
    # that state, so use it for every FNVXR-scoped save/clear operation.
    foreach ($entry in [Environment]::GetEnvironmentVariables(
            [EnvironmentVariableTarget]::Process).GetEnumerator()) {
        $name = [string]$entry.Key
        if ([string]::IsNullOrEmpty($Prefix) -or
            $name.StartsWith($Prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            [pscustomobject][ordered]@{
                Name = $name
                Value = [string]$entry.Value
            }
        }
    }
}

function Clear-FnvxrProductProcessEnvironmentVariables {
    param([string]$Prefix = "FNVXR_")

    foreach ($entry in @(Get-FnvxrProductProcessEnvironmentEntries -Prefix $Prefix)) {
        [Environment]::SetEnvironmentVariable(
            $entry.Name,
            $null,
            [EnvironmentVariableTarget]::Process)
    }
}

function Set-FnvxrProductMinimalEnvironment {
    param([Parameter(Mandatory = $true)]$Environment)

    Clear-FnvxrProductProcessEnvironmentVariables
    foreach ($key in $Environment.Keys) {
        [Environment]::SetEnvironmentVariable(
            [string]$key,
            [string]$Environment[$key],
            [EnvironmentVariableTarget]::Process)
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

function Invoke-FnvxrProductReadOnlyRetailRuntimeProbe {
    param(
        [Parameter(Mandatory = $true)][string]$ProbePath,
        [Parameter(Mandatory = $true)][uint32]$ProcessId,
        [ValidateRange(0, 60000)][int]$WaitMilliseconds = 1000,
        [Parameter(Mandatory = $true)][string]$LogPath
    )

    # The retail runtime probe opens the target with QUERY_INFORMATION and
    # VM_READ only. Its normal incomplete-proof exit is useful evidence, not
    # an authorization failure or a reason to mutate the game process.
    if (-not (Test-Path -LiteralPath $ProbePath -PathType Leaf)) {
        throw "Read-only retail runtime probe is missing: $ProbePath"
    }
    $output = & $ProbePath `
        "--pid" ([string]$ProcessId) `
        "--wait-ms" ([string]$WaitMilliseconds) 2>&1 | Out-String
    $exitCode = $LASTEXITCODE
    Add-Content -LiteralPath $LogPath -Value $output -Encoding UTF8

    return [pscustomobject][ordered]@{
        schema = "fnvxr-read-only-retail-runtime-probe-v1"
        scope = "external read-only loaded-process observation; no patch, suspend, resume, input, OpenXR, or game-tree mutation"
        probePath = (Resolve-Path -LiteralPath $ProbePath).Path
        processId = $ProcessId
        waitMilliseconds = $WaitMilliseconds
        captured = $true
        exitCode = $exitCode
        probeReportedEngineCapability = ($exitCode -eq 0)
        sceneGraphObserved = $output -match "non_null_pointer_observation=MATCH"
        loadedIdentityMatched = $output -match "loaded_pe .*proof=MATCH"
        capabilityProofObserved = $output -match "retail_engine_capability_proof=PASS"
        logPath = $LogPath
    }
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

    $requiredProcessId = $RequiredProcess.Id
    $requiredProcessName = try {
        [string]$RequiredProcess.ProcessName
    } catch {
        "process"
    }
    if ([string]::IsNullOrWhiteSpace($requiredProcessName)) {
        $requiredProcessName = "process"
    }
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $RequiredProcess.Refresh()
        if ($RequiredProcess.HasExited) {
            $exitCode = try { [string]$RequiredProcess.ExitCode } catch { "unknown" }
            throw "$Description failed because $requiredProcessName`:$requiredProcessId exited with code $exitCode."
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
