$ErrorActionPreference = "Stop"

if (-not ("FnvxrSidecarWindow" -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class FnvxrSidecarWindow {
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
}
'@
}

function Write-FnvxrCheckpoint {
    param(
        [string]$Path,
        [string]$Message
    )

    Add-Content -LiteralPath $Path -Value ("{0:o} {1}" -f (Get-Date), $Message) -Encoding UTF8
}

function Test-FnvxrRetailGameRoot {
    param([string]$Path)

    if (-not $Path) {
        return $false
    }

    return (
        (Test-Path -LiteralPath $Path) -and
        (Test-Path -LiteralPath (Join-Path $Path "FalloutNV.exe")) -and
        (Test-Path -LiteralPath (Join-Path $Path "nvse_loader.exe")) -and
        (Test-Path -LiteralPath (Join-Path $Path "Data\FalloutNV.esm"))
    )
}

function Resolve-FnvxrGameRoot {
    param(
        [string]$GameRoot,
        [string]$DebugLog = ""
    )

    $candidates = New-Object System.Collections.Generic.List[string]

    function Add-Candidate {
        param([string]$Path)

        if (-not $Path) {
            return
        }
        $trimmed = $Path.Trim().Trim('"')
        if ($trimmed) {
            $candidates.Add($trimmed)
        }
    }

    Add-Candidate $GameRoot
    Add-Candidate $env:FNVXR_GAME_ROOT

    $repoRoot = Split-Path -Parent $PSScriptRoot
    $searchRoots = New-Object System.Collections.Generic.List[string]
    $searchRoots.Add($repoRoot)
    if ($env:FNVXR_SEARCH_ROOTS) {
        foreach ($root in ($env:FNVXR_SEARCH_ROOTS -split ';')) {
            $trimmedRoot = $root.Trim().Trim('"')
            if ($trimmedRoot) {
                $searchRoots.Add($trimmedRoot)
            }
        }
    }

    foreach ($root in ($searchRoots | Select-Object -Unique)) {
        if (-not $root) {
            continue
        }
        Add-Candidate (Join-Path $root "mod-library\america-rebuilt\toolspace\runtime_sandbox\Fallout New Vegas ARQA")
        Add-Candidate (Join-Path $root "mod-library\america-rebuilt\toolspace\runtime_sandbox\Fallout New Vegas")
        Add-Candidate (Join-Path $root "Fallout New Vegas ARQA")
        Add-Candidate (Join-Path $root "Fallout New Vegas")
    }

    Add-Candidate "C:\Program Files (x86)\Steam\steamapps\common\Fallout New Vegas"
    Add-Candidate "C:\GOG Games\Fallout New Vegas"

    $configPath = if ($env:FNVXR_OPENMW_CONFIG) {
        $env:FNVXR_OPENMW_CONFIG
    } elseif ($env:FNVXR_MODLIST_ROOT) {
        Join-Path $env:FNVXR_MODLIST_ROOT "openmw-config\openmw.cfg"
    } else {
        ""
    }
    if ($configPath -and (Test-Path -LiteralPath $configPath)) {
        Get-Content -LiteralPath $configPath | ForEach-Object {
            if ($_ -match '^data=(.+)$') {
                $dataPath = $Matches[1] -replace '/', '\'
                if ($dataPath.EndsWith('\Data', [System.StringComparison]::OrdinalIgnoreCase)) {
                    Add-Candidate (Split-Path -Parent $dataPath)
                }
            }
        }
    }

    $checked = New-Object System.Collections.Generic.List[string]
    foreach ($candidate in ($candidates | Select-Object -Unique)) {
        $checked.Add($candidate)
        if (Test-FnvxrRetailGameRoot -Path $candidate) {
            $resolved = (Resolve-Path -LiteralPath $candidate).Path
            if ($DebugLog) {
                Write-FnvxrCheckpoint -Path $DebugLog -Message ("gameRoot resolved '{0}'" -f $resolved)
            }
            $env:FNVXR_GAME_ROOT = $resolved
            return $resolved
        }
    }

    throw ("Missing Fallout New Vegas retail root with FalloutNV.exe, nvse_loader.exe, and Data\FalloutNV.esm. Checked: {0}" -f (($checked | Select-Object -Unique) -join "; "))
}

function Resolve-FnvxrOpenXrLoader {
    param([string]$DebugLog = "")

    $candidates = New-Object System.Collections.Generic.List[string]

    function Add-LoaderCandidate {
        param([string]$Path)

        if (-not $Path) {
            return
        }
        $trimmed = $Path.Trim().Trim('"')
        if ($trimmed) {
            $candidates.Add($trimmed)
        }
    }

    Add-LoaderCandidate $env:FNVXR_OPENXR_LOADER_HINT
    Add-LoaderCandidate $env:FNVXR_OPENXR_LOADER
    Add-LoaderCandidate "C:\Program Files (x86)\Steam\steamapps\common\SteamVR\bin\win64\openxr_loader.dll"
    Add-LoaderCandidate "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\ThirdParty\OpenXR\win64\openxr_loader.dll"
    if ($env:VCPKG_ROOT) {
        Add-LoaderCandidate (Join-Path $env:VCPKG_ROOT "installed\x64-windows\bin\openxr_loader.dll")
        Add-LoaderCandidate (Join-Path $env:VCPKG_ROOT "packages\openxr-loader_x64-windows\bin\openxr_loader.dll")
    }

    $checked = New-Object System.Collections.Generic.List[string]
    foreach ($candidate in ($candidates | Select-Object -Unique)) {
        $checked.Add($candidate)
        if (Test-Path -LiteralPath $candidate) {
            $resolved = (Resolve-Path -LiteralPath $candidate).Path
            if ($DebugLog) {
                Write-FnvxrCheckpoint -Path $DebugLog -Message ("openxr_loader resolved '{0}'" -f $resolved)
            }
            return $resolved
        }
    }

    throw ("Missing openxr_loader.dll. Checked: {0}" -f (($checked | Select-Object -Unique) -join "; "))
}

function Copy-FnvxrOpenXrLoader {
    param(
        [string]$HostExe,
        [string]$DebugLog = ""
    )

    if (-not (Test-Path -LiteralPath $HostExe)) {
        throw "Missing OpenXR pose host before loader staging: $HostExe"
    }

    $source = Resolve-FnvxrOpenXrLoader -DebugLog $DebugLog
    $destination = Join-Path (Split-Path -Parent $HostExe) "openxr_loader.dll"
    $copy = $true
    if (Test-Path -LiteralPath $destination) {
        $sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
        $destinationHash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
        $copy = ($sourceHash -ne $destinationHash)
    }

    if ($copy) {
        Copy-Item -LiteralPath $source -Destination $destination -Force
    }

    if ($DebugLog) {
        Write-FnvxrCheckpoint -Path $DebugLog -Message ("openxr_loader staged source='{0}' destination='{1}' copied={2}" -f $source, $destination, $copy)
    }
    return $destination
}

function Assert-FnvxrDimensions {
    param(
        [int]$GameBackbufferWidth,
        [int]$GameBackbufferHeight,
        [int]$HostGameTextureWidth,
        [int]$HostGameTextureHeight,
        [int]$UiWidth,
        [int]$UiHeight
    )

    if ($GameBackbufferWidth -lt 1 -or $GameBackbufferHeight -lt 1) {
        throw "Invalid retail backbuffer size: ${GameBackbufferWidth}x${GameBackbufferHeight}"
    }
    if ($GameBackbufferWidth -gt 4096 -or $GameBackbufferHeight -gt 2560) {
        throw "Retail backbuffer ${GameBackbufferWidth}x${GameBackbufferHeight} exceeds Local\FNVXR_D3D9_Frame_v1 ABI max 4096x2560."
    }
    if ($HostGameTextureWidth -lt 1 -or $HostGameTextureHeight -lt 1) {
        throw "Invalid host game texture size: ${HostGameTextureWidth}x${HostGameTextureHeight}"
    }
    if ($UiWidth -ne 1280 -or $UiHeight -ne 720) {
        throw "Sidecar UI grid must stay 1280x720. Retail menu tiles, OpenMW pointer packets, and NVSE click dispatch all use this canonical grid."
    }

    $retailAspect = [double]$GameBackbufferWidth / [double]$GameBackbufferHeight
    $hostAspect = [double]$HostGameTextureWidth / [double]$HostGameTextureHeight
    if ([Math]::Abs($retailAspect - $hostAspect) -gt 0.001) {
        throw "Retail backbuffer aspect $retailAspect does not match host texture aspect $hostAspect."
    }
}

function Set-FnvxrIniValue {
    param(
        [string]$Path,
        [string]$Key,
        [string]$Value,
        [string]$DebugLog
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }

    $lines = @(Get-Content -LiteralPath $Path -ErrorAction Stop)
    $pattern = "^\s*$([regex]::Escape($Key))\s*="
    $updated = $false
    for ($i = 0; $i -lt $lines.Count; ++$i) {
        if ($lines[$i] -match $pattern) {
            $lines[$i] = "$Key=$Value"
            $updated = $true
        }
    }
    if (-not $updated) {
        $lines += "$Key=$Value"
    }

    Set-Content -LiteralPath $Path -Value $lines -Encoding ASCII
    if ($DebugLog) {
        Write-FnvxrCheckpoint -Path $DebugLog -Message ("ini {0} {1}={2}" -f $Path, $Key, $Value)
    }
}

function Set-FnvxrIniSectionValue {
    param(
        [string]$Path,
        [string]$Section,
        [string]$Key,
        [string]$Value,
        [string]$DebugLog
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }

    $lines = [System.Collections.Generic.List[string]]::new()
    foreach ($line in (Get-Content -LiteralPath $Path -ErrorAction Stop)) {
        $lines.Add([string]$line)
    }

    $sectionPattern = "^\s*\[$([regex]::Escape($Section))\]\s*$"
    $keyPattern = "^\s*$([regex]::Escape($Key))\s*="
    $sectionStart = -1
    $sectionEnd = $lines.Count
    for ($i = 0; $i -lt $lines.Count; ++$i) {
        if ($lines[$i] -match $sectionPattern) {
            $sectionStart = $i
            for ($j = $i + 1; $j -lt $lines.Count; ++$j) {
                if ($lines[$j] -match '^\s*\[.+\]\s*$') {
                    $sectionEnd = $j
                    break
                }
            }
            break
        }
    }

    if ($sectionStart -lt 0) {
        if ($lines.Count -gt 0 -and $lines[$lines.Count - 1].Trim().Length -ne 0) {
            $lines.Add("")
        }
        $lines.Add("[$Section]")
        $lines.Add("$Key=$Value")
    } else {
        $updated = $false
        for ($i = $sectionStart + 1; $i -lt $sectionEnd; ++$i) {
            if ($lines[$i] -match $keyPattern) {
                $lines[$i] = "$Key=$Value"
                $updated = $true
            }
        }
        if (-not $updated) {
            $lines.Insert($sectionEnd, "$Key=$Value")
        }
    }

    Set-Content -LiteralPath $Path -Value $lines -Encoding ASCII
    if ($DebugLog) {
        Write-FnvxrCheckpoint -Path $DebugLog -Message ("ini {0} [{1}] {2}={3}" -f $Path, $Section, $Key, $Value)
    }
}

function Set-FnvxrFalloutIni {
    param(
        [int]$Width,
        [int]$Height,
        [double]$DefaultFov = 0,
        [double]$FirstPersonFov = 0,
        [double]$PipboyFov = 0,
        [switch]$DisableMultisampling,
        [string]$DebugLog
    )

    $iniRoots = @(
        (Join-Path $env:USERPROFILE "Documents\My Games\FalloutNV"),
        (Join-Path $env:USERPROFILE "OneDrive\Documents\My Games\FalloutNV")
    )

    foreach ($root in $iniRoots) {
        if (-not (Test-Path -LiteralPath $root)) {
            continue
        }
        foreach ($iniName in @("Fallout.ini", "FalloutPrefs.ini")) {
            $iniPath = Join-Path $root $iniName
            Set-FnvxrIniSectionValue -Path $iniPath -Section "General" -Key "bDisableAutoVanityMode" -Value "1" -DebugLog $DebugLog
            Set-FnvxrIniValue -Path $iniPath -Key "bAlwaysActive" -Value "1" -DebugLog $DebugLog
            Set-FnvxrIniValue -Path $iniPath -Key "bBackground Mouse" -Value "1" -DebugLog $DebugLog
            Set-FnvxrIniValue -Path $iniPath -Key "bBackground Keyboard" -Value "1" -DebugLog $DebugLog
            Set-FnvxrIniValue -Path $iniPath -Key "bDisable360Controller" -Value "0" -DebugLog $DebugLog
            Set-FnvxrIniValue -Path $iniPath -Key "bGamepadEnable" -Value "1" -DebugLog $DebugLog
            Set-FnvxrIniValue -Path $iniPath -Key "bFull Screen" -Value "0" -DebugLog $DebugLog
            Set-FnvxrIniValue -Path $iniPath -Key "bBorderless" -Value "1" -DebugLog $DebugLog
            Set-FnvxrIniValue -Path $iniPath -Key "iPresentInterval" -Value "0" -DebugLog $DebugLog
            Set-FnvxrIniValue -Path $iniPath -Key "iSize W" -Value ([string]$Width) -DebugLog $DebugLog
            Set-FnvxrIniValue -Path $iniPath -Key "iSize H" -Value ([string]$Height) -DebugLog $DebugLog
            if ($DisableMultisampling) {
                Set-FnvxrIniValue -Path $iniPath -Key "iMultiSample" -Value "0" -DebugLog $DebugLog
                Set-FnvxrIniValue -Path $iniPath -Key "bTransparencyMultisampling" -Value "0" -DebugLog $DebugLog
            }
            Set-FnvxrIniValue -Path $iniPath -Key "iSystemColorHUDMainRed" -Value "255" -DebugLog $DebugLog
            Set-FnvxrIniValue -Path $iniPath -Key "iSystemColorHUDMainGreen" -Value "182" -DebugLog $DebugLog
            Set-FnvxrIniValue -Path $iniPath -Key "iSystemColorHUDMainBlue" -Value "0" -DebugLog $DebugLog
            Set-FnvxrIniValue -Path $iniPath -Key "iSystemColorHUDAltRed" -Value "255" -DebugLog $DebugLog
            Set-FnvxrIniValue -Path $iniPath -Key "iSystemColorHUDAltGreen" -Value "182" -DebugLog $DebugLog
            Set-FnvxrIniValue -Path $iniPath -Key "iSystemColorHUDAltBlue" -Value "0" -DebugLog $DebugLog
            if ($DefaultFov -gt 0) {
                Set-FnvxrIniValue -Path $iniPath -Key "fDefaultFOV" -Value ($DefaultFov.ToString("0.0000", [Globalization.CultureInfo]::InvariantCulture)) -DebugLog $DebugLog
            }
            if ($FirstPersonFov -gt 0) {
                Set-FnvxrIniValue -Path $iniPath -Key "fDefault1stPersonFOV" -Value ($FirstPersonFov.ToString("0.0000", [Globalization.CultureInfo]::InvariantCulture)) -DebugLog $DebugLog
            }
            if ($PipboyFov -gt 0) {
                Set-FnvxrIniValue -Path $iniPath -Key "fPipboy1stPersonFOV" -Value ($PipboyFov.ToString("0.0000", [Globalization.CultureInfo]::InvariantCulture)) -DebugLog $DebugLog
            }
        }
    }
}

function Invoke-FnvxrRuntimeConsoleCommand {
    param(
        [string]$CommandExe,
        [string]$Line,
        [string]$DebugLog,
        [string]$Label = "runtime-console",
        [int]$WaitMs = 5000
    )

    if (-not $Line) {
        Write-FnvxrCheckpoint -Path $DebugLog -Message ("{0} skipped empty command" -f $Label)
        return $false
    }
    if (-not $CommandExe -or -not (Test-Path -LiteralPath $CommandExe)) {
        Write-FnvxrCheckpoint -Path $DebugLog -Message ("{0} skipped missing command helper '{1}' line='{2}'" -f $Label, $CommandExe, $Line)
        return $false
    }

    try {
        $output = @(& $CommandExe console $Line --wait-ms $WaitMs 2>&1)
        $exit = $LASTEXITCODE
        $joined = ($output | Out-String).Trim()
        if ($joined.Length -gt 360) {
            $joined = $joined.Substring(0, 360) + "..."
        }
        Write-FnvxrCheckpoint -Path $DebugLog -Message ("{0} exit={1} line='{2}' output='{3}'" -f $Label, $exit, $Line, $joined)
        return ($exit -eq 0)
    } catch {
        Write-FnvxrCheckpoint -Path $DebugLog -Message ("{0} failed line='{1}' error='{2}'" -f $Label, $Line, $_.Exception.Message)
        return $false
    }
}

function Invoke-FnvxrTestLoadout {
    param(
        [string]$CommandExe,
        [string]$DebugLog,
        [int]$WaitMs = 5000
    )

    $commands = @(
        @{ Label = "loadout add slot1 stimpak"; Line = "player.additem 00015169 25"; Slot = 1; Item = "Stimpak"; FormId = "00015169" },
        @{ Label = "loadout add slot3 frag-grenade"; Line = "player.additem 00004330 12"; Slot = 3; Item = "Frag Grenade"; FormId = "00004330" },
        @{ Label = "loadout add slot4 power-fist"; Line = "player.additem 00004347 1"; Slot = 4; Item = "Power Fist"; FormId = "00004347" },
        @{ Label = "loadout add slot5 9mm-pistol"; Line = "player.additem 000E3778 1"; Slot = 5; Item = "9mm Pistol"; FormId = "000E3778" },
        @{ Label = "loadout add slot6 service-rifle"; Line = "player.additem 000E9C3B 1"; Slot = 6; Item = "Service Rifle"; FormId = "000E9C3B" },
        @{ Label = "loadout add slot7 caravan-shotgun"; Line = "player.additem 000CD53A 1"; Slot = 7; Item = "Caravan Shotgun"; FormId = "000CD53A" },
        @{ Label = "loadout add slot8 cowboy-repeater"; Line = "player.additem 0008F21A 1"; Slot = 8; Item = "Cowboy Repeater"; FormId = "0008F21A" },
        @{ Label = "loadout add ammo 9mm"; Line = "player.additem 0008ED03 250"; Slot = $null; Item = "9mm Round"; FormId = "0008ED03" },
        @{ Label = "loadout add ammo 556"; Line = "player.additem 00004240 300"; Slot = $null; Item = "5.56mm Round"; FormId = "00004240" },
        @{ Label = "loadout add ammo 20g"; Line = "player.additem 000E86F2 80"; Slot = $null; Item = "20 Gauge Shell"; FormId = "000E86F2" },
        @{ Label = "loadout add ammo energy-cell"; Line = "player.additem 00020772 180"; Slot = $null; Item = "Energy Cell"; FormId = "00020772" },
        @{ Label = "loadout add ammo 357"; Line = "player.additem 0008ED02 160"; Slot = $null; Item = ".357 Magnum Round"; FormId = "0008ED02" },
        @{ Label = "loadout bind slot1 stimpak"; Line = "SetHotkeyItem 1 00015169"; Slot = 1; Item = "Stimpak"; FormId = "00015169" },
        @{ Label = "loadout bind slot3 frag-grenade"; Line = "SetHotkeyItem 3 00004330"; Slot = 3; Item = "Frag Grenade"; FormId = "00004330" },
        @{ Label = "loadout bind slot4 power-fist"; Line = "SetHotkeyItem 4 00004347"; Slot = 4; Item = "Power Fist"; FormId = "00004347" },
        @{ Label = "loadout bind slot5 9mm-pistol"; Line = "SetHotkeyItem 5 000E3778"; Slot = 5; Item = "9mm Pistol"; FormId = "000E3778" },
        @{ Label = "loadout bind slot6 service-rifle"; Line = "SetHotkeyItem 6 000E9C3B"; Slot = 6; Item = "Service Rifle"; FormId = "000E9C3B" },
        @{ Label = "loadout bind slot7 caravan-shotgun"; Line = "SetHotkeyItem 7 000CD53A"; Slot = 7; Item = "Caravan Shotgun"; FormId = "000CD53A" },
        @{ Label = "loadout bind slot8 cowboy-repeater"; Line = "SetHotkeyItem 8 0008F21A"; Slot = 8; Item = "Cowboy Repeater"; FormId = "0008F21A" }
    )

    $results = @()
    $success = $true
    foreach ($command in $commands) {
        $ok = Invoke-FnvxrRuntimeConsoleCommand `
            -CommandExe $CommandExe `
            -Line $command.Line `
            -DebugLog $DebugLog `
            -Label $command.Label `
            -WaitMs $WaitMs
        if (-not $ok) {
            $success = $false
        }
        $results += [ordered]@{
            label = $command.Label
            line = $command.Line
            slot = $command.Slot
            item = $command.Item
            formId = $command.FormId
            success = [bool]$ok
        }
        Start-Sleep -Milliseconds 75
    }

    return [ordered]@{
        requested = $true
        applied = [bool]$success
        slot2ReservedForAmmoSwap = $true
        combatSlots = @(5, 6, 7, 8)
        utilitySlots = @(1, 3, 4)
        commands = $results
    }
}

function Get-FnvxrArtifactInfo {
    param([string]$Path)

    $exists = Test-Path -LiteralPath $Path
    $info = [ordered]@{
        path = $Path
        exists = $exists
        length = 0
        sha256 = $null
    }
    if ($exists) {
        $item = Get-Item -LiteralPath $Path
        $info.length = $item.Length
        $info.sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    return $info
}

function Copy-FnvxrStageArtifact {
    param(
        [hashtable]$Item,
        [bool]$Copy
    )

    $optional = [bool]$Item.Optional
    $sourceInfo = Get-FnvxrArtifactInfo -Path $Item.Source
    if (-not $sourceInfo.exists) {
        if ($optional) {
            return [ordered]@{
                source = $sourceInfo
                destination = Get-FnvxrArtifactInfo -Path $Item.Destination
                optional = $true
                copied = $false
                verified = $false
                skipped = "missing optional source"
            }
        }
        throw "Missing build output: $($Item.Source)"
    }

    if ($Copy) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Item.Destination) | Out-Null
        Copy-Item -LiteralPath $Item.Source -Destination $Item.Destination -Force
    }

    $destinationInfo = Get-FnvxrArtifactInfo -Path $Item.Destination
    $verified = $destinationInfo.exists -and $sourceInfo.sha256 -and ($sourceInfo.sha256 -eq $destinationInfo.sha256)
    if ($Copy -and -not $verified) {
        throw "Staged artifact hash mismatch: $($Item.Source) -> $($Item.Destination)"
    }

    return [ordered]@{
        source = $sourceInfo
        destination = $destinationInfo
        optional = $optional
        copied = $Copy
        verified = $verified
        skipped = if ($Copy) { $null } else { "validate only" }
    }
}

function Copy-FnvxrRetailArtifacts {
    param(
        [string]$Root,
        [string]$Configuration,
        [string]$GameRoot,
        [bool]$Copy
    )

    $buildWin32 = Join-Path $Root "build-win32\$Configuration"
    $pluginDir = Join-Path $GameRoot "Data\NVSE\Plugins"
    $stageMap = @(
        @{ Source = Join-Path $buildWin32 "nvse_fnvxr.dll"; Destination = Join-Path $pluginDir "nvse_fnvxr.dll" },
        @{ Source = Join-Path $buildWin32 "nvse_fnvxr.pdb"; Destination = Join-Path $pluginDir "nvse_fnvxr.pdb"; Optional = $true },
        @{ Source = Join-Path $buildWin32 "d3d9.dll"; Destination = Join-Path $GameRoot "d3d9.dll" },
        @{ Source = Join-Path $buildWin32 "d3d9.pdb"; Destination = Join-Path $GameRoot "d3d9.pdb"; Optional = $true },
        @{ Source = Join-Path $buildWin32 "dinput8.dll"; Destination = Join-Path $GameRoot "dinput8.dll" },
        @{ Source = Join-Path $buildWin32 "dinput8.pdb"; Destination = Join-Path $GameRoot "dinput8.pdb"; Optional = $true },
        @{ Source = Join-Path $buildWin32 "xinput1_3.dll"; Destination = Join-Path $GameRoot "xinput1_3.dll" },
        @{ Source = Join-Path $buildWin32 "xinput1_3.dll"; Destination = Join-Path $GameRoot "xinput9_1_0.dll" },
        @{ Source = Join-Path $buildWin32 "xinput1_3.dll"; Destination = Join-Path $GameRoot "xinput1_4.dll" },
        @{ Source = Join-Path $buildWin32 "xinput1_3.dll"; Destination = Join-Path $GameRoot "xinput1_2.dll" },
        @{ Source = Join-Path $buildWin32 "xinput1_3.dll"; Destination = Join-Path $GameRoot "xinput1_1.dll" },
        @{ Source = Join-Path $buildWin32 "xinput1_3.pdb"; Destination = Join-Path $GameRoot "xinput1_3.pdb"; Optional = $true }
    )

    $staged = @()
    foreach ($item in $stageMap) {
        $staged += Copy-FnvxrStageArtifact -Item $item -Copy $Copy
    }
    return $staged
}

function Clear-FnvxrEnvironment {
    Get-ChildItem Env: | Where-Object { $_.Name -like "FNVXR_*" } | ForEach-Object {
        Remove-Item ("Env:{0}" -f $_.Name) -ErrorAction SilentlyContinue
    }
}

function Set-FnvxrSidecarEnvironment {
    param(
        [ValidateSet("openxr-sidecar", "retail-sidecar")]
        [string]$Profile,
        [int]$UiWidth = 1280,
        [int]$UiHeight = 720,
        [int]$InputWidth = 1280,
        [int]$InputHeight = 720,
        [int]$HostGameTextureWidth = 3072,
        [int]$HostGameTextureHeight = 1920,
        [string]$RunDir = "",
        [string]$RunId = ""
    )

    Clear-FnvxrEnvironment
    # The retail oracle is an intentionally expensive animation/environment
    # scanner for offline OpenMW capture work.  A cooperating oracle build
    # treats this as a clean no-op so VR launches do not scan every game loop.
    $env:NIKAMI_ORACLE_ENABLE = "0"
    $env:FNVXR_RUN_PROFILE = $Profile
    if ($RunDir) {
        $env:FNVXR_RUN_LOG_DIR = $RunDir
    }
    if ($RunId) {
        $env:FNVXR_RUN_ID = $RunId
    }
    $env:FNVXR_TELEMETRY_HAMMER = "1"
    $env:FNVXR_D3D9_TELEMETRY_HAMMER = $env:FNVXR_TELEMETRY_HAMMER
    $env:FNVXR_D3D9_TELEMETRY_HAMMER_WARMUP = "240"
    $env:FNVXR_D3D9_REPLAY_DRAW_TELEMETRY_STRIDE = "1"
    $env:FNVXR_D3D9_WVP_TELEMETRY_STRIDE = "1"
    $env:FNVXR_D3D9_CLEAR_TELEMETRY_STRIDE = "1"
    $env:FNVXR_D3D9_REPLAY_TARGET_TELEMETRY_STRIDE = "1"
    $env:FNVXR_D3D9_STATEBLOCK_TELEMETRY_STRIDE = "1"
    $env:FNVXR_D3D9_CULL_TELEMETRY_STRIDE = "120"
    $env:FNVXR_UI_SHARED_WIDTH = [string]$UiWidth
    $env:FNVXR_UI_SHARED_HEIGHT = [string]$UiHeight
    $env:FNVXR_UI_INPUT_WIDTH = [string]$InputWidth
    $env:FNVXR_UI_INPUT_HEIGHT = [string]$InputHeight
    $env:FNVXR_MENU_TILE_WIDTH = "640"
    $env:FNVXR_MENU_TILE_HEIGHT = "480"
    $env:FNVXR_GAME_TEXTURE_WIDTH = [string]$HostGameTextureWidth
    $env:FNVXR_GAME_TEXTURE_HEIGHT = [string]$HostGameTextureHeight
    $env:FNVXR_GAME_PLANE_WIDTH = "3.15"
    $env:FNVXR_GAME_PLANE_HEIGHT = "2.15"
    $env:FNVXR_GAME_PLANE_OFFSET_Z = "-3.35"
    $env:FNVXR_GAMEPLAY_PLANE_WIDTH = "5.60"
    $env:FNVXR_GAMEPLAY_PLANE_HEIGHT = "3.50"
    $env:FNVXR_GAMEPLAY_PLANE_OFFSET_Z = "-3.55"
    $env:FNVXR_GAME_PLANE_CURVE_ENABLE = "1"
    $env:FNVXR_GAME_PLANE_CURVE_SEGMENTS_X = "32"
    $env:FNVXR_GAME_PLANE_CURVE_SEGMENTS_Y = "18"
    $env:FNVXR_GAME_PLANE_CURVE_DEPTH_X = "0.22"
    $env:FNVXR_GAME_PLANE_CURVE_DEPTH_Y = "0.08"
    $env:FNVXR_GAME_PLANE_CURVE_CORNER_DEPTH = "0.03"
    $env:FNVXR_GAME_PLANE_CURVE_SIGN = "1"
    $env:FNVXR_GAME_PLANE_CENTER_UV_LEFT = "0"
    $env:FNVXR_GAME_PLANE_CENTER_UV_RIGHT = "1"
    $env:FNVXR_GAME_PLANE_CENTER_UV_TOP = "0"
    $env:FNVXR_GAME_PLANE_CENTER_UV_BOTTOM = "1"
    $env:FNVXR_GAME_PLANE_MODE = "center2d"
    $env:FNVXR_DIALOG_USES_GAMEPLAY_PLANE = "1"
    $env:FNVXR_PIPBOY_SCALE = "0.88"
    $env:FNVXR_LEFT_GRIP_PIPBOY_MODE = "1"
    $env:FNVXR_PIPBOY_GRIP_THRESHOLD = "0.55"
    $env:FNVXR_RIGHT_GRIP_MENU_MODE = "0"
    $env:FNVXR_MENU_GRIP_THRESHOLD = "0.55"
    $env:FNVXR_XINPUT_LEFT_GRIP_PIPBOY_ENABLE = "0"
    $env:FNVXR_XINPUT_RIGHT_GRIP_MENU_ENABLE = "0"
    $env:FNVXR_XINPUT_RIGHT_GRIP_MENU_CLOSE_ON_RELEASE = "0"
    $env:FNVXR_XINPUT_GRIP_MENU_THRESHOLD = "0.55"
    $env:FNVXR_XINPUT_PHYSICAL_MENU_BUTTONS_ENABLE = "1"
    $env:FNVXR_XINPUT_MASK_X = "1"
    $env:FNVXR_XINPUT_MASK_B = "1"
    $env:FNVXR_XINPUT_MASK_Y = "1"
    $env:FNVXR_DIRECT_UI_CLICK = "0"
    $env:FNVXR_POINTER_TILE_FALLBACK = "0"
    $env:FNVXR_QUEUE_ACCEPT_CLICK = "0"
    $env:FNVXR_PLUGIN_ACCEPT_ON_EXTERNAL_DINPUT_CLICK = "0"
    $env:FNVXR_CLICK_CLEAR_CLIP = "1"
    $env:FNVXR_CURSOR_TRACK_POINTER = "0"
    $env:FNVXR_CURSOR_FOCUS = "0"
    $env:FNVXR_CLICK_FOCUS_ON_CLICK = "0"
    $env:FNVXR_CLICK_SENDINPUT_MOUSE = "0"
    $env:FNVXR_PLUGIN_SENDINPUT_CLICK = "0"
    $env:FNVXR_POST_MENU_KEYS = "0"
    $env:FNVXR_POST_WINDOW_MOUSE_FALLBACK = "0"
    $env:FNVXR_IMMEDIATE_OS_CLICK = "0"
    $env:FNVXR_CLICK_LEGACY_FALLBACK_AFTER_DIRECT = "0"
    $env:FNVXR_ACCEPT_REPEAT = "0"
    $env:FNVXR_CAMERA_APPLY = "0"
    $env:FNVXR_NVSE_WRITES_VR_POSE = "0"
    $env:FNVXR_HOST_CURSOR_CLICK_ENABLED = "0"
    $env:FNVXR_HOST_CURSOR_SET_POS = "0"
    $env:FNVXR_HOST_CURSOR_ABSOLUTE_MOVE = "0"
    $env:FNVXR_HOST_CURSOR_TRACK_POINTER = "0"
    $env:FNVXR_HOST_CURSOR_FOCUS = "0"
    $env:FNVXR_HOST_SENDINPUT_CLICK = "0"
    $env:FNVXR_HOST_CURSOR_CLICK_FALLBACK = "0"
    $env:FNVXR_DINPUT_FORCE_BACKGROUND = "1"
    $env:FNVXR_DINPUT_KEYBOARD_MOVEMENT_ENABLE = "auto"
    $env:FNVXR_PLUGIN_KEYBOARD_MOVEMENT_ENABLE = "0"
    $env:FNVXR_PLUGIN_MENU_KEYBOARD_FALLBACK = "1"
    $env:FNVXR_PLUGIN_GAMEPLAY_KEYBOARD_FALLBACK = "1"
    $env:FNVXR_DINPUT_FORWARD_AXIS_SIGN = "1"
    $env:FNVXR_DINPUT_WALK_THRESHOLD = "12000"
    $env:FNVXR_DINPUT_NORMAL_THRESHOLD = "17000"
    $env:FNVXR_DINPUT_RUN_THRESHOLD = "26000"
    $env:FNVXR_DINPUT_SLOW_PULSE_ENABLE = "0"
    $env:FNVXR_DINPUT_SLOW_PULSE_MS = "180"
    $env:FNVXR_DINPUT_SLOW_DUTY_PERCENT = "42"
    $env:FNVXR_DINPUT_WALK_MODIFIER_DIK = "0"
    $env:FNVXR_DINPUT_RUN_MODIFIER_DIK = "0"
    $env:FNVXR_DINPUT_AUTO_RUN_LEFT_THUMB_ENABLE = "1"
    $env:FNVXR_GAMEPLAY_RUN_BUTTON_ENABLE = "1"
    $env:FNVXR_GAMEPLAY_RUN_MODIFIER_DIK = "42"
    $env:FNVXR_GAMEPLAY_ANALOG_RUN_ENABLE = "0"
    $env:FNVXR_GAMEPLAY_ANALOG_RUN_THRESHOLD = "0.92"
    $env:FNVXR_GAMEPLAY_COMBAT_CHORDS_ENABLE = "1"
    $env:FNVXR_GAMEPLAY_COMBAT_CHORD_SUPPRESS_AIM_MOUSE = "1"
    $env:FNVXR_GAMEPLAY_HOTKEY_RELEASE_AIM_MOUSE = "1"
    $env:FNVXR_GAMEPLAY_VATS_CHORD_ENABLE = "1"
    $env:FNVXR_GAMEPLAY_VATS_DIK = "47"
    $env:FNVXR_GAMEPLAY_WAIT_CHORD_ENABLE = "1"
    $env:FNVXR_GAMEPLAY_WAIT_DIK = "20"
    $env:FNVXR_GAMEPLAY_RIGHT_GRIP_GRAB_ENABLE = "1"
    $env:FNVXR_GAMEPLAY_GRAB_DIK = "44"
    $env:FNVXR_LEFT_GRIP_COMBAT_CHORD_SUPPRESSES_PIPBOY = "1"
    $env:FNVXR_RIGHT_GRIP_COMBAT_CHORD_SUPPRESSES_MENU = "1"
    $env:FNVXR_GAMEPLAY_STIMPAK_DIK = "2"
    $env:FNVXR_GAMEPLAY_AMMO_SWAP_DIK = "3"
    $env:FNVXR_GAMEPLAY_GRENADE_DIK = "4"
    $env:FNVXR_GAMEPLAY_BACKUP_DIK = "5"
    $env:FNVXR_GAMEPLAY_COMBAT_A_DIK = "6"
    $env:FNVXR_GAMEPLAY_COMBAT_B_DIK = "7"
    $env:FNVXR_GAMEPLAY_COMBAT_X_DIK = "8"
    $env:FNVXR_GAMEPLAY_COMBAT_Y_DIK = "9"
    $env:FNVXR_XINPUT_MASK_THUMBSTICK_CLICKS = "1"
    $env:FNVXR_XINPUT_RIGHT_STICK_Y_ENABLE = "1"
    $env:FNVXR_XINPUT_RIGHT_STICK_SCALE = "1.12"
    $env:FNVXR_XINPUT_AUTO_RUN_LEFT_THUMB_ENABLE = "1"
    $env:FNVXR_FORCE_FIRST_PERSON = "1"
    $env:FNVXR_FORCE_FIRST_PERSON_RETRY_MS = "250"
    $env:FNVXR_DISABLE_AUTO_VANITY_CAMERA = "1"
    $env:FNVXR_THIRD_PERSON_L3_ENABLE = "0"
    $env:FNVXR_THIRD_PERSON_ZOOM_DEADZONE = "9000"
    $env:FNVXR_THIRD_PERSON_ZOOM_REPEAT_MS = "80"
    $env:FNVXR_THIRD_PERSON_ZOOM_WHEEL_DELTA = "120"
    $env:FNVXR_THIRD_PERSON_TOGGLE_GUARD_MS = "350"
    $env:FNVXR_THIRD_PERSON_TAP_MAX_MS = "650"
    $env:FNVXR_XINPUT_MENU_STICK_TO_DPAD = "0"
    $env:FNVXR_XINPUT_MENU_SUPPRESS_ANALOG = "1"
    $env:FNVXR_XINPUT_MENU_DPAD_DEADZONE = "0.45"
    $env:FNVXR_XINPUT_MOVEMENT_MODE_SCALE_ENABLE = "0"
    $env:FNVXR_XINPUT_WALK_SCALE = "0.45"
    $env:FNVXR_XINPUT_RUN_FLOOR = "0.75"
    $env:FNVXR_XINPUT_MOVEMENT_MODE_DEADZONE = "4500"
    $env:FNVXR_DINPUT_RIGHT_STICK_PITCH_ENABLE = "1"
    $env:FNVXR_DINPUT_LOOK_SCALE = "11.2"
    $env:FNVXR_UI_NAV_STICK = "left"
    $env:FNVXR_UI_NAV_DEADZONE = "16000"
    $env:FNVXR_UI_GENERIC_BACK_USES_TAB = "1"
    $env:FNVXR_UI_MAP_ZOOM_ENABLE = "1"
    $env:FNVXR_UI_MAP_ZOOM_TRIGGERS_ENABLE = "0"
    $env:FNVXR_UI_MAP_ZOOM_RIGHT_STICK_ENABLE = "1"
    $env:FNVXR_UI_MAP_ZOOM_WHEEL_DELTA = "120"
    $env:FNVXR_UI_MAP_ZOOM_REPEAT_MS = "120"
    $env:FNVXR_UI_MAP_ZOOM_STICK_DEADZONE = "16000"
    $env:FNVXR_PIPBOY_RIGHT_STICK_NAV = "0"
    $env:FNVXR_PIPBOY_SPLIT_STICK_NAV = "0"
    $env:FNVXR_PIPBOY_POINTER_ONLY = "1"
    $env:FNVXR_PIPBOY_POINTER_ONLY_STICK_NAV_SUPPRESS = "0"
    $env:FNVXR_PIPBOY_B_USES_TAB = "1"
    $env:FNVXR_PIPBOY_POINTER_CANONICAL_GRID = "0"
    $env:FNVXR_PIPBOY_Y_ASSIGN_FAVORITE = "1"
    $env:FNVXR_UI_FAVORITE_ASSIGN_HOLD_MS = "900"
    $env:FNVXR_UI_FAVORITE_ASSIGN_CLICK_DELAY_MS = "75"
    $env:FNVXR_EXTERNAL_DINPUT_WRITER = "1"
    $env:FNVXR_HEADSPACE_LOOK_ENABLE = "1"
    $env:FNVXR_HEADSPACE_LOOK_DEADZONE_DEGREES = "0.08"
    $env:FNVXR_HEADSPACE_LOOK_NORMAL_DEADZONE_DEGREES = "0.08"
    $env:FNVXR_HEADSPACE_LOOK_AIM_TRIGGER = "0.35"
    $env:FNVXR_HEADSPACE_LOOK_AIM_DEADZONE_DEGREES = "0.035"
    $env:FNVXR_HEADSPACE_LOOK_MAX_DELTA_DEGREES = "3.0"
    $env:FNVXR_HEADSPACE_LOOK_NORMAL_MAX_DELTA_DEGREES = "3.0"
    $env:FNVXR_HEADSPACE_LOOK_AIM_MAX_DELTA_DEGREES = "2.0"
    $env:FNVXR_HEADSPACE_LOOK_GAMEPLAY_WARMUP_FRAMES = "45"
    $env:FNVXR_HANDSPACE_LOOK_ENABLE = "1"
    $env:FNVXR_HANDSPACE_LOOK_REQUIRES_WEAPON_OUT = "1"
    $env:FNVXR_HANDSPACE_LOOK_DEADZONE = "0.0005"
    $env:FNVXR_HANDSPACE_LOOK_MAX_DELTA = "0.025"
    $env:FNVXR_HANDSPACE_LOOK_HORIZONTAL_DEGREES = "75"
    $env:FNVXR_HANDSPACE_LOOK_VERTICAL_DEGREES = "50"
    $env:FNVXR_GYRO_AIM_ENABLE = "1"
    $env:FNVXR_GYRO_AIM_DEADZONE_DEGREES = "0.04"
    $env:FNVXR_GYRO_AIM_MAX_DELTA_DEGREES = "2.0"
    $env:FNVXR_DINPUT_HEAD_LOOK_ENABLE = "1"
    $env:FNVXR_DINPUT_HEAD_LOOK_SCALE = "560"
    $env:FNVXR_DINPUT_HEAD_LOOK_NORMAL_SCALE = "560"
    $env:FNVXR_DINPUT_HEAD_LOOK_AIM_ENABLE = "1"
    $env:FNVXR_DINPUT_HEAD_LOOK_AIM_SCALE = "950"
    $env:FNVXR_DINPUT_HANDSPACE_LOOK_ENABLE = "1"
    $env:FNVXR_DINPUT_HANDSPACE_LOOK_SCALE = "720"
    $env:FNVXR_DINPUT_HANDSPACE_LOOK_SUPPRESS_HEAD = "0"
    $env:FNVXR_DINPUT_GYRO_AIM_ENABLE = "1"
    $env:FNVXR_DINPUT_GYRO_AIM_SCALE = "920"
    $env:FNVXR_DINPUT_GYRO_AIM_SUPPRESS_HEAD = "1"
    $env:FNVXR_MENU_POINTER_HAND = "right"
    $env:FNVXR_MENU_POINTER_HEAD_FALLBACK = "1"
    $env:FNVXR_GAME_PLANE_CAPTURE_HZ = "0"
    $env:FNVXR_D3D9_SHARED_VIDEO_CAPTURE_HZ = "0"
    $env:FNVXR_D3D9_FORCE_PRESENT_IMMEDIATE = "1"
    $env:FNVXR_D3D9_SHARED_VIDEO_SLOW_MS = "4.0"
    $env:FNVXR_HOST_GAME_TEXTURE_SLOW_MS = "2.0"
    $env:FNVXR_GAME_PLANE_AUTO_CENTER_FRAMES = "0"
    $env:FNVXR_GAME_PLANE_LOCK_TO_HEAD = "0"
    $env:FNVXR_GAME_PLANE_RECENTER_ON_FOCUS = "1"
    $env:FNVXR_MENU_POINTER_SMOOTHING = "0.12"
    $env:FNVXR_SHOW_PAUSE_SCENE = "0"
    $env:FNVXR_GAME_WORLD_BEHIND_MENU = "0"
    $env:FNVXR_RUNTIME_UI_STUCK_FORCE_WORLD = "0"
    $env:FNVXR_FORCE_GAMEPLAY = "0"
    $env:FNVXR_DISABLE_STEREO_WORLD = "1"
    $env:FNVXR_SHOW_GAME_PLANE_IN_GAME = "1"
    $env:FNVXR_STEREO_FALLBACK_MONO_FULLSCREEN = "0"
    $env:FNVXR_SHOW_GAME_PLANE_ON_STEREO_LOSS = "0"
    $env:FNVXR_SHOW_WORLD_PROPS = "0"
    $env:FNVXR_SHOW_BODY_RIG = "0"
    $env:FNVXR_SHOW_HAND_FINGERS = "0"
    $env:FNVXR_SHOW_LEFT_AIM_RAY = "0"
    $env:FNVXR_SHOW_RIGHT_AIM_RAY = "0"
    $env:FNVXR_D3D9_CAPTURE_WORLD_DURING_UI = "0"
    $env:FNVXR_D3D9_REQUIRE_SHARED_CAMERA_FOR_WORLD = "1"
    $env:FNVXR_D3D9_USE_SHARED_CAMERA_VIEW = "1"
    $env:FNVXR_D3D9_APPLY_HMD_POSE = "0"
    $env:FNVXR_D3D9_POSE_AXIS_MODE = "gamebryo"
    $env:FNVXR_D3D9_GAME_UNITS_PER_METER = "39.3701"
    $env:FNVXR_D3D9_HEAD_POSITION_SCALE = "1"
    $env:FNVXR_D3D9_STEREO_SCALE = "1"
    # Retail arm IK is injected only by the 32-bit NVSE plugin. Keep it inert
    # for ordinary quad launches; focused stereo tests opt in below.
    $env:FNVXR_RETAIL_RIG_ENABLE = "0"
    $env:FNVXR_RETAIL_RIG_APPLY = "0"
    $env:FNVXR_RETAIL_RIG_DUMP_NODES = "1"
    $env:FNVXR_RETAIL_RIG_POSITION_SCALE = "1"
    $env:FNVXR_RETAIL_RIG_FABRIK_ITERATIONS = "12"
    $env:FNVXR_RETAIL_RIG_FABRIK_TOLERANCE = "0.05"
    $env:FNVXR_RETAIL_RIG_ELBOW_POLE_WEIGHT = "1"
    $env:FNVXR_RETAIL_RIG_AUTO_CALIBRATE_POSITION = "0"
    $env:FNVXR_D3D9_MAX_LOCAL_VIEW_OFFSET_UNITS = "400"
    $env:FNVXR_D3D9_SHADER_STEREO = "0"
    $env:FNVXR_D3D9_SHADER_MATRIX_DELTA = "0"
    $env:FNVXR_D3D9_SHADER_MATRIX_ORDER = "column"
    $env:FNVXR_D3D9_SHADER_MATRIX_REQUIRE_SHARED_CAMERA = "1"
    $env:FNVXR_D3D9_SHADER_MATRIX_ALIGNMENT = "4"
    $env:FNVXR_D3D9_SHADER_MATRIX_MAX_CANDIDATES = "12"
    $env:FNVXR_D3D9_SHADER_PATCH_START_REGISTER = "0"
    $env:FNVXR_D3D9_SHADER_STEREO_ALLOW_VERTEX_HASHES = ""
    $env:FNVXR_D3D9_SHADER_MATRIX_MAX_ABS = "1000000000"
    $env:FNVXR_D3D9_SHARED_STEREO = "1"
    $env:FNVXR_D3D9_NATIVE_MULTIPASS = "0"
    $env:FNVXR_D3D9_STEREO_REPLAY = "1"
    $env:FNVXR_D3D9_RESOURCE_GRAPH_TELEMETRY = "0"
    $env:FNVXR_D3D9_STEREO_BEST_SNAPSHOT_AS_WORLD = "0"
    $env:FNVXR_D3D9_STEREO_COLLAPSE_AUDIT = "0"
    $env:FNVXR_D3D9_STEREO_AUTO_SKIP_COLLAPSE_SHADER_PAIRS = "0"
    $env:FNVXR_D3D9_STEREO_SKIP_SHADER_HASH_PAIRS = ""
    $env:FNVXR_D3D9_STEREO_TARGET_DIFF_PROBE = "0"
    $env:FNVXR_D3D9_STEREO_VISUAL_COVERAGE_GATE = "1"
    $env:FNVXR_D3D9_STEREO_VISUAL_STABLE_FRAMES = "60"
    $env:FNVXR_D3D9_STEREO_MIN_ACTIVE_FRACTION = "0.12"
    $env:FNVXR_D3D9_STEREO_MIN_ACTIVE_SPAN_X = "0.35"
    $env:FNVXR_D3D9_STEREO_MIN_ACTIVE_SPAN_Y = "0.35"
    $env:FNVXR_D3D9_STEREO_RETAIN_LAST_VALID_ON_INVALID = "0"
    $env:FNVXR_D3D9_STEREO_CLEAR_ON_UI_INVALID = "0"
    $env:FNVXR_D3D9_STEREO_STRICT_TARGET_GATE = "1"
    $env:FNVXR_D3D9_STEREO_AUTO_ACTIVATE_TARGET_ON_DRAW = "1"
    $env:FNVXR_D3D9_STEREO_READBACK_ON_SWAP_AWAY = "0"
    $env:FNVXR_D3D9_SKIP_SCREENSPACE_STEREO_DRAWS = "1"
    $env:FNVXR_D3D9_SKIP_SCREENSPACE_BY_PROJECTION_ONLY = "1"
    $env:FNVXR_D3D9_STEREO_REPLAY_UP_DRAWS = "0"
    $env:FNVXR_D3D9_STEREO_READBACK = "1"
    $env:FNVXR_D3D9_STEREO_IDENTICAL_DISARM_FRAMES = "120"
    $env:FNVXR_D3D9_SHARED_VIDEO = "1"
    $env:FNVXR_REQUIRE_WORLD_STEREO = "0"
    $env:FNVXR_GAME_FULLSCREEN_IN_XR = "0"
    $env:FNVXR_USE_STEREO_GAME_TEXTURES = "0"
    $env:FNVXR_SHOW_GAME_PLANE = "1"
}

function Set-FnvxrStereoWorldRuntimeEnvironment {
    $env:FNVXR_DISABLE_STEREO_WORLD = "0"
    $env:FNVXR_TELEMETRY_HAMMER = "0"
    $env:FNVXR_D3D9_TELEMETRY_HAMMER = "0"
    $env:FNVXR_D3D9_NATIVE_MULTIPASS = "1"
    $env:FNVXR_D3D9_NATIVE_PIPELINE_TRACE = "0"
    $env:FNVXR_D3D9_NATIVE_TRACE_ALL_DRAWS = "0"
    $env:FNVXR_D3D9_NATIVE_REQUIRE_FIRST_PERSON = "1"
    $env:FNVXR_D3D9_NATIVE_RECENTER_ON_FIRST_GAMEPLAY = "1"
    $env:FNVXR_D3D9_ALLOW_THIRD_PERSON_STEREO = "0"
    $env:FNVXR_D3D9_NATIVE_APPLY_HEAD_ROTATION = "1"
    $env:FNVXR_D3D9_NATIVE_HEAD_AXIS_MODE = "retail-camera"
    $env:FNVXR_D3D9_NATIVE_ASYMMETRIC_FOV = "1"
    $env:FNVXR_REQUIRE_NATIVE_STEREO = "1"
    $env:FNVXR_D3D9_APPLY_HMD_POSE = "0"
    $env:FNVXR_HEADSPACE_LOOK_ENABLE = "0"
    $env:FNVXR_HANDSPACE_LOOK_ENABLE = "0"
    $env:FNVXR_DINPUT_HEAD_LOOK_ENABLE = "0"
    $env:FNVXR_DINPUT_HANDSPACE_LOOK_ENABLE = "0"
    $env:FNVXR_D3D9_SHADER_STEREO = "0"
    $env:FNVXR_D3D9_SHADER_MATRIX_DELTA = "0"
    $env:FNVXR_D3D9_SHADER_MATRIX_ORDER = "column"
    $env:FNVXR_D3D9_SHADER_MATRIX_REQUIRE_SHARED_CAMERA = "1"
    $env:FNVXR_D3D9_SHADER_WVP_REPLAY = "0"
    $env:FNVXR_D3D9_SHADER_MATRIX_MAX_CANDIDATES = "1"
    $env:FNVXR_D3D9_SHADER_PATCH_START_REGISTER = "0"
    $env:FNVXR_D3D9_SHADER_STEREO_ALLOW_VERTEX_HASHES = ""
    $env:FNVXR_D3D9_SHADER_SANITY_OFFSET = "0"
    $env:FNVXR_D3D9_SHADER_SANITY_SLOT = "c1w"
    $env:FNVXR_D3D9_SHADER_SANITY_START_REGISTER = "0"
    $env:FNVXR_D3D9_SHADER_IPD = "2.5"
    $env:FNVXR_D3D9_SHADER_DEPTH = "1.0"
    $env:FNVXR_D3D9_SHADER_NUDGE_SLOT = "03"
    $env:FNVXR_D3D9_DEBUG_COMPARE_REPLAY_DRAWS = "0"
    $env:FNVXR_D3D9_STEREO_PUBLISH_BEST_SNAPSHOT = "0"
    $env:FNVXR_D3D9_STEREO_BEST_SNAPSHOT_AS_WORLD = "0"
    $env:FNVXR_D3D9_STEREO_COLLAPSE_AUDIT = "0"
    $env:FNVXR_D3D9_STEREO_AUTO_SKIP_COLLAPSE_SHADER_PAIRS = "0"
    $env:FNVXR_D3D9_STEREO_SKIP_SHADER_HASH_PAIRS = ""
    $env:FNVXR_D3D9_STEREO_TARGET_DIFF_PROBE = "0"
    $env:FNVXR_D3D9_STEREO_VISUAL_COVERAGE_GATE = "0"
    $env:FNVXR_D3D9_STEREO_VISUAL_STABLE_FRAMES = "1"
    $env:FNVXR_D3D9_STEREO_MIN_ACTIVE_FRACTION = "0.12"
    $env:FNVXR_D3D9_STEREO_MIN_ACTIVE_SPAN_X = "0.35"
    $env:FNVXR_D3D9_STEREO_MIN_ACTIVE_SPAN_Y = "0.35"
    $env:FNVXR_D3D9_STEREO_RETAIN_LAST_VALID_ON_INVALID = "0"
    $env:FNVXR_D3D9_STEREO_CLEAR_ON_UI_INVALID = "0"
    $env:FNVXR_D3D9_STEREO_STRICT_TARGET_GATE = "1"
    $env:FNVXR_D3D9_STEREO_AUTO_ACTIVATE_TARGET_ON_DRAW = "1"
    $env:FNVXR_D3D9_STEREO_READBACK_ON_SWAP_AWAY = "0"
    $env:FNVXR_D3D9_STEREO_SNAPSHOT_REPLAY_DRAWS = "512"
    $env:FNVXR_D3D9_STEREO_SNAPSHOT_FIXED_DRAW = "512"
    $env:FNVXR_D3D9_STEREO_SNAPSHOT_DRAW_STRIDE = "16"
    $env:FNVXR_D3D9_RESOURCE_GRAPH_TELEMETRY = "0"
    $env:FNVXR_D3D9_SHARED_STEREO = "1"
    $env:FNVXR_D3D9_STEREO_REPLAY = "0"
    $env:FNVXR_D3D9_STEREO_READBACK = "1"
    $env:FNVXR_D3D9_STEREO_IDENTICAL_DISARM_FRAMES = "120"
    $env:FNVXR_REQUIRE_WORLD_STEREO = "0"
    $env:FNVXR_GAME_FULLSCREEN_IN_XR = "1"
    $env:FNVXR_USE_STEREO_GAME_TEXTURES = "1"
    $env:FNVXR_STEREO_FALLBACK_MONO_FULLSCREEN = "0"
    $env:FNVXR_STEREO_STABLE_HANDOFF_FRAMES = "2"
    $env:FNVXR_STEREO_STALE_FRAME_LIMIT = "180"
    # OpenXR polls faster than the retail producer.  Hold the last complete
    # pair across ordinary no-new-sequence polls, but only for the short stale
    # window above so ordinary retail hitches do not flash the compositor.
    $env:FNVXR_STEREO_RETAIN_LAST_VALID_ON_REJECT = "1"
}

function Get-FnvxrCurrentProcessExclusionIds {
    $exclude = New-Object 'System.Collections.Generic.HashSet[int]'
    $current = Get-CimInstance Win32_Process -Filter ("ProcessId={0}" -f $PID) -ErrorAction SilentlyContinue
    while ($current) {
        [void]$exclude.Add([int]$current.ProcessId)
        if (-not $current.ParentProcessId -or $current.ParentProcessId -eq $current.ProcessId) {
            break
        }
        $current = Get-CimInstance Win32_Process -Filter ("ProcessId={0}" -f $current.ParentProcessId) -ErrorAction SilentlyContinue
    }
    return ,$exclude
}

function Get-FnvxrControlProcess {
    $all = @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue)
    $exclude = Get-FnvxrCurrentProcessExclusionIds
    $rootIds = New-Object 'System.Collections.Generic.HashSet[int]'
    $scriptPattern = '(?i)(start-openxr-retail-sidecar|start-retail-surface-producer|start-rock-solid|start-retail-pcvr-max|start-openmw-fnv-sidecar|watch-openmw-left-grip-launch-retail|probe-shared-state)\.ps1'

    foreach ($process in $all) {
        $id = [int]$process.ProcessId
        if ($exclude.Contains($id)) {
            continue
        }
        $commandLine = [string]$process.CommandLine
        if (-not $commandLine) {
            continue
        }
        if ($commandLine -match $scriptPattern -or $commandLine -match '(?i)RequireWorldStereo|RequireStereo') {
            [void]$rootIds.Add($id)
        }
    }

    $treeIds = New-Object 'System.Collections.Generic.HashSet[int]'
    $changed = $true
    while ($changed) {
        $changed = $false
        foreach ($process in $all) {
            $id = [int]$process.ProcessId
            if ($exclude.Contains($id) -or $treeIds.Contains($id)) {
                continue
            }
            $parentId = [int]$process.ParentProcessId
            if ($rootIds.Contains($id) -or $treeIds.Contains($parentId)) {
                [void]$treeIds.Add($id)
                $changed = $true
            }
        }
    }

    foreach ($process in $all) {
        $id = [int]$process.ProcessId
        if ($treeIds.Contains($id)) {
            [pscustomobject]@{
                Id = $id
                ParentProcessId = [int]$process.ParentProcessId
                ProcessName = [string]$process.Name
                CommandLine = [string]$process.CommandLine
            }
        }
    }
}

function Get-FnvxrLaunchProcess {
    $runtime = @(Get-Process FalloutNV,nvse_loader,fnvxr_openxr_pose_host -ErrorAction SilentlyContinue)
    $control = @(Get-FnvxrControlProcess | ForEach-Object {
        Get-Process -Id $_.Id -ErrorAction SilentlyContinue
    })
    @($runtime + $control)
}

function Stop-FnvxrLaunchProcess {
    param([string]$DebugLog)

    for ($pass = 1; $pass -le 3; ++$pass) {
        $existing = @(Get-FnvxrLaunchProcess)
        if ($existing.Count -eq 0) {
            return
        }

        if ($DebugLog) {
            Write-FnvxrCheckpoint -Path $DebugLog -Message ("stopping existing launch processes pass={0} count={1}" -f $pass, $existing.Count)
        }
        foreach ($process in $existing) {
            try {
                if ($DebugLog) {
                    Write-FnvxrCheckpoint -Path $DebugLog -Message ("stopping pid={0} name={1}" -f $process.Id, $process.ProcessName)
                }
                Stop-Process -Id $process.Id -Force -ErrorAction Stop
            } catch {
                if ($DebugLog) {
                    Write-FnvxrCheckpoint -Path $DebugLog -Message ("stop skipped pid={0} error={1}" -f $process.Id, $_.Exception.Message)
                }
            }
        }
        Start-Sleep -Seconds 1
    }
}

function Find-FnvxrFalloutWindow {
    param(
        [System.Diagnostics.Process]$Process,
        [bool]$Focus
    )

    for ($i = 0; $i -lt 80; ++$i) {
        $candidates = @()
        if ($Process -and -not $Process.HasExited) {
            $candidates += $Process
        }
        $candidates += @(Get-Process FalloutNV,nvse_loader -ErrorAction SilentlyContinue)

        foreach ($candidate in $candidates) {
            try {
                $candidate.Refresh()
                $handle = [IntPtr]$candidate.MainWindowHandle
            } catch {
                continue
            }
            if ($handle -ne [IntPtr]::Zero) {
                if ($Focus) {
                    Set-FnvxrWindowForeground -Handle $handle | Out-Null
                }
                return $handle
            }
        }
        Start-Sleep -Milliseconds 250
    }

    return [IntPtr]::Zero
}

function Set-FnvxrWindowForeground {
    param(
        [IntPtr]$Handle,
        [string]$DebugLog = ""
    )

    if ($Handle -eq [IntPtr]::Zero) {
        return $false
    }

    [FnvxrSidecarWindow]::ShowWindow($Handle, 9) | Out-Null
    Start-Sleep -Milliseconds 50
    $focused = [FnvxrSidecarWindow]::SetForegroundWindow($Handle)
    if ($DebugLog) {
        Write-FnvxrCheckpoint -Path $DebugLog -Message ("retail foreground repair hwnd={0} result={1}" -f $Handle, $focused)
    }
    return $focused
}

function Wait-FnvxrLogPattern {
    param(
        [string]$Path,
        [string]$Pattern,
        [int]$TimeoutSeconds,
        [System.Diagnostics.Process]$Process = $null
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        if ($Process) {
            $Process.Refresh()
            if ($Process.HasExited) {
                return $false
            }
        }
        if (Test-Path -LiteralPath $Path) {
            $hit = Select-String -LiteralPath $Path -Pattern $Pattern -SimpleMatch -Quiet -ErrorAction SilentlyContinue
            if ($hit) {
                return $true
            }
        }
        Start-Sleep -Milliseconds 250
    } while ((Get-Date) -lt $deadline)

    return $false
}
