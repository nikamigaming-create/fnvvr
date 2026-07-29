param(
    [ValidateSet("Release")]
    [string]$Configuration = "Release",
    [string]$GameRoot = $(if ($env:FNVXR_GAME_ROOT) {
        $env:FNVXR_GAME_ROOT
    } elseif (Test-Path -LiteralPath "D:\SteamLibrary\steamapps\common\Fallout New Vegas\FalloutNV.exe" -PathType Leaf) {
        "D:\SteamLibrary\steamapps\common\Fallout New Vegas"
    } else {
        "C:\Program Files (x86)\Steam\steamapps\common\Fallout New Vegas"
    }),
    [ValidateRange(15, 1800)]
    [int]$MaximumRunSeconds = 900,
    [ValidateRange(5, 120)]
    [int]$StartupTimeoutSeconds = 60,
    [ValidateRange(1, 8)]
    [int]$AcceptanceTrialCycles = 2,
    [ValidateRange(1000, 15000)]
    [int]$AcceptanceStepMilliseconds = 5000,
    [ValidateRange(10, 300)]
    [int]$AcceptanceReadinessTimeoutSeconds = 120,
    [ValidateRange(0, 60000)]
    [int]$EngineEvidenceWaitMilliseconds = 1000,
    [switch]$ValidateOnly,
    [switch]$RunAcceptanceTrial,
    [switch]$AutomateAcceptance,
    [switch]$CollectEngineEvidence,
    [switch]$TrackedPropVisualTrial,
    [switch]$ApproveStageAndLaunch
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

# This launcher is the narrow exception to the retired direct-install paths.
# It is deliberately not an OpenXR/stereo launcher: it stages only the Win32
# plugin and D3D9 proxy for the camera-local, desktop-assist experiment, then
# restores both exact prior files after the owned desktop game process exits.
if (-not $ValidateOnly -and -not $ApproveStageAndLaunch) {
    throw "Desktop-assist staging and launch require -ApproveStageAndLaunch. Use -ValidateOnly for a read-only plan."
}
if ($ValidateOnly -and $ApproveStageAndLaunch) {
    throw "-ValidateOnly is read-only and cannot be combined with -ApproveStageAndLaunch."
}
if ($ValidateOnly -and $RunAcceptanceTrial) {
    throw "-RunAcceptanceTrial requires a live approved desktop-assist session and cannot be combined with -ValidateOnly."
}
if ($ValidateOnly -and $AutomateAcceptance) {
    throw "-AutomateAcceptance requires a live approved desktop-assist session and cannot be combined with -ValidateOnly."
}
if ($AutomateAcceptance -and -not $RunAcceptanceTrial) {
    throw "-AutomateAcceptance requires -RunAcceptanceTrial so its recovery load and two Escape taps are evidence-bound."
}
if ($ValidateOnly -and $CollectEngineEvidence) {
    throw "-CollectEngineEvidence requires a live approved desktop-assist session and cannot be combined with -ValidateOnly."
}
if ($TrackedPropVisualTrial -and ($RunAcceptanceTrial -or $AutomateAcceptance -or $CollectEngineEvidence)) {
    throw "-TrackedPropVisualTrial is a manual visual rig trial and cannot be combined with acceptance automation or engine-evidence collection."
}
$acceptanceTrialDurationSeconds = [Math]::Ceiling(
    (11.0 * [double]$AcceptanceTrialCycles * [double]$AcceptanceStepMilliseconds) / 1000.0)
if ($RunAcceptanceTrial -and $acceptanceTrialDurationSeconds -gt $MaximumRunSeconds) {
    throw "The requested desktop-assist acceptance trace takes $acceptanceTrialDurationSeconds seconds, exceeding -MaximumRunSeconds $MaximumRunSeconds."
}

. (Join-Path $PSScriptRoot "fnvxr-product-common.ps1")

function Get-DesktopAssistStagePlan {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$Configuration,
        [Parameter(Mandatory = $true)][string]$GameRoot
    )

    $win32Build = Join-Path $Root "build-product-win32\$Configuration"
    $pluginRoot = Join-Path $GameRoot "Data\NVSE\Plugins"
    return @(
        [pscustomobject]@{
            key = "x86/d3d9.dll"
            source = Join-Path $win32Build "d3d9.dll"
            destination = Join-Path $GameRoot "d3d9.dll"
            machine = "0x014c"
        },
        [pscustomobject]@{
            key = "x86/nvse_fnvxr.dll"
            source = Join-Path $win32Build "nvse_fnvxr.dll"
            destination = Join-Path $pluginRoot "nvse_fnvxr.dll"
            machine = "0x014c"
        }
    )
}

function Get-DesktopAssistBuildPlan {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$Configuration
    )

    return @(
        [pscustomobject]@{
            key = "win32-stage-artifacts"
            buildDirectory = Join-Path $Root "build-product-win32"
            targets = @("fnvxr_d3d9_proxy", "nvse_fnvxr")
            artifacts = @("d3d9.dll", "nvse_fnvxr.dll")
        },
        [pscustomobject]@{
            key = "x64-assist-evidence-tools"
            buildDirectory = Join-Path $Root "build-product-x64"
            targets = @(
                "fnvxr_assist",
                "fnvxr_shared_state_probe",
                "fnvxr_retail_runtime_probe",
                "fnvxr_command")
            artifacts = @(
                "fnvxr_assist.exe",
                "fnvxr_shared_state_probe.exe",
                "fnvxr_retail_runtime_probe.exe",
                "fnvxr_command.exe")
        }
    )
}

function Invoke-DesktopAssistBuildPlan {
    param(
        [Parameter(Mandatory = $true)][object[]]$Plan,
        [Parameter(Mandatory = $true)][string]$Configuration
    )

    $records = @()
    foreach ($item in $Plan) {
        $cache = Join-Path $item.buildDirectory "CMakeCache.txt"
        if (-not (Test-Path -LiteralPath $cache -PathType Leaf)) {
            throw "Desktop-assist build directory is not configured: $($item.buildDirectory)"
        }
        $arguments = @(
            "--build",
            $item.buildDirectory,
            "--config",
            $Configuration,
            "--target") + @($item.targets)
        $output = & cmake @arguments 2>&1 | Out-String
        $exitCode = $LASTEXITCODE
        $records += [pscustomobject][ordered]@{
            key = $item.key
            buildDirectory = $item.buildDirectory
            targets = @($item.targets)
            artifacts = @($item.artifacts)
            exitCode = $exitCode
            output = $output
        }
        if ($exitCode -ne 0) {
            throw "Desktop-assist build failed for $($item.key) with exit code $exitCode`: $output"
        }
    }
    return @($records)
}

function Get-DesktopAssistEnvironment {
    param(
        [Parameter(Mandatory = $true)][string]$RunId,
        [Parameter(Mandatory = $true)][string]$RunDirectory,
        [bool]$AutomateAcceptance = $false,
        [bool]$TrackedPropVisualTrial = $false
    )

    $environment = [ordered]@{
        FNVXR_RUN_PROFILE = "desktop-assist"
        FNVXR_RUN_ID = $RunId
        FNVXR_RUN_LOG_DIR = $RunDirectory
        FNVXR_DESKTOP_ASSIST_CAMERA_ONLY = "1"
        FNVXR_DESKTOP_ASSIST_UI_CAPTURE = "1"
        FNVXR_INSTALL_CAMERA_HOOK = "1"
        FNVXR_CAMERA_HOOK = "1"
        FNVXR_CAMERA_APPLY = "1"
        FNVXR_CAMERA_APPLY_ROTATION = "1"
        FNVXR_CAMERA_YAW_ONLY = "0"
        FNVXR_CAMERA_APPLY_TRANSLATION = "0"
        FNVXR_CAMERA_POSITION_SCALE = "0"
        FNVXR_CAMERA_WRITE_WORLD = "0"
        FNVXR_CAMERA_UPDATE_TRANSFORM = "0"
        FNVXR_RETAIL_RIG_ENABLE = "0"
        FNVXR_RETAIL_RIG_APPLY = "0"
        FNVXR_RETAIL_WEAPON_APPLY = "0"
        FNVXR_RETAIL_PROJECTILE_NODE_HOOK = "0"
        FNVXR_ENABLE_ENGINE_CENTER_STEREO = "0"
        FNVXR_ENABLE_LEGACY_IMAGE_DIAGNOSTICS = "0"
        FNVXR_DISABLE_STEREO_WORLD = "1"
        FNVXR_D3D9_STEREO_REPLAY = "0"
        FNVXR_D3D9_NATIVE_SINGLE_TRAVERSAL_REPLAY = "0"
        FNVXR_D3D9_WIDE_WORLD_REPLAY = "0"
    }
    if ($TrackedPropVisualTrial) {
        # This is intentionally not a firing or renderer experiment. The
        # plugin's narrow authority checks every one of these false routes at
        # the hook decision point before it can write the visual rig.
        $environment.FNVXR_RUN_PROFILE = "tracked-prop-assist"
        $environment.FNVXR_DESKTOP_ASSIST_CAMERA_ONLY = "0"
        $environment.FNVXR_TRACKED_PROP_ASSIST_VISUAL_ONLY = "1"
        $environment.FNVXR_DESKTOP_ASSIST_UI_CAPTURE = "0"
        $environment.FNVXR_RETAIL_RIG_ENABLE = "1"
        $environment.FNVXR_RETAIL_RIG_APPLY = "1"
        $environment.FNVXR_RETAIL_WEAPON_APPLY = "1"
        $environment.FNVXR_RETAIL_PROJECTILE_NODE_HOOK = "0"
        $environment.FNVXR_TRACKED_PROP_ASSIST_PROJECTILE_OR_HIT_MUTATION = "0"
        $environment.FNVXR_TRACKED_PROP_ASSIST_OPENXR_PRESENTATION = "0"
        $environment.FNVXR_NVSE_WRITES_VR_POSE = "0"
        $environment.FNVXR_CLICK_SENDINPUT_MOUSE = "0"
        $environment.FNVXR_PLUGIN_SENDINPUT_CLICK = "0"
        $environment.FNVXR_EXTERNAL_XINPUT_WRITER = "0"
        $environment.FNVXR_EXTERNAL_DINPUT_WRITER = "0"
        $environment.FNVXR_DESKTOP_ASSIST_AUTOMATION = "0"
    }
    if ($AutomateAcceptance) {
        # The plugin rejects every command except the exact fixed recovery
        # load, and the supervisor itself sends only two verified Escape taps.
        $environment.FNVXR_DESKTOP_ASSIST_AUTOMATION = "1"
    }
    return $environment
}

function Get-DesktopAssistAcceptanceReportEvidence {
    param($Report)

    $result = [ordered]@{
        complete = $false
        cameraEvidenceSource = ""
        requestedChecksPass = $false
        desktopAssistHeadBodyDecoupled = $false
        headTranslationBodyDecoupled = $false
        runtimeUiTransitionObserved = $false
        desktopAssistUiQuadPixelsVerifiedObserved = $false
        desktopAssistUiQuadInvalidatedAfterUiObserved = $false
        desktopAssistUiQuadTransitionObserved = $false
        bodyPositionToleranceUnits = $null
    }
    if ($null -eq $Report) {
        return [pscustomobject]$result
    }

    $readProperty = {
        param($Object, [string]$Name)
        if ($null -eq $Object) { return $null }
        $property = $Object.PSObject.Properties[$Name]
        if ($null -eq $property) { return $null }
        return $property.Value
    }
    $result.cameraEvidenceSource = [string](& $readProperty $Report "cameraEvidenceSource")
    foreach ($name in @(
            "requestedChecksPass",
            "desktopAssistHeadBodyDecoupled",
            "headTranslationBodyDecoupled",
            "runtimeUiTransitionObserved",
            "desktopAssistUiQuadPixelsVerifiedObserved",
            "desktopAssistUiQuadInvalidatedAfterUiObserved",
            "desktopAssistUiQuadTransitionObserved")) {
        $result[$name] = (& $readProperty $Report $name) -eq $true
    }

    try {
        $thresholds = & $readProperty $Report "thresholds"
        $result.bodyPositionToleranceUnits = [double](& $readProperty $thresholds "bodyPositionUnits")
    } catch {
        $result.bodyPositionToleranceUnits = $null
    }
    $positionToleranceValid = $null -ne $result.bodyPositionToleranceUnits `
        -and -not [double]::IsNaN([double]$result.bodyPositionToleranceUnits) `
        -and -not [double]::IsInfinity([double]$result.bodyPositionToleranceUnits) `
        -and [double]$result.bodyPositionToleranceUnits -gt 0.0 `
        -and [double]$result.bodyPositionToleranceUnits -le 0.25
    $result.complete = $result.cameraEvidenceSource -eq "desktop-assist-local-camera" `
        -and $result.requestedChecksPass `
        -and $result.desktopAssistHeadBodyDecoupled `
        -and $result.headTranslationBodyDecoupled `
        -and $result.runtimeUiTransitionObserved `
        -and $result.desktopAssistUiQuadPixelsVerifiedObserved `
        -and $result.desktopAssistUiQuadInvalidatedAfterUiObserved `
        -and $result.desktopAssistUiQuadTransitionObserved `
        -and $positionToleranceValid
    return [pscustomobject]$result
}

function Get-ExactDesktopAssistFalloutProcess {
    param([Parameter(Mandatory = $true)][string]$ExpectedPath)

    $expectedFullPath = [System.IO.Path]::GetFullPath($ExpectedPath)
    foreach ($candidate in @(Get-Process FalloutNV -ErrorAction SilentlyContinue)) {
        try {
            if ([string]::Equals(
                    [System.IO.Path]::GetFullPath($candidate.Path),
                    $expectedFullPath,
                    [System.StringComparison]::OrdinalIgnoreCase)) {
                return $candidate
            }
        } catch {}
    }
    return $null
}

function Wait-DesktopAssistLoadedModule {
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
            throw "Fallout exited before loading $ExpectedPath."
        }
        foreach ($module in @(Get-FnvxrProductLoadedModuleCensus -ProcessId ([uint32]$Process.Id))) {
            if (-not [string]::Equals(
                    $module.path,
                    $expectedFullPath,
                    [System.StringComparison]::OrdinalIgnoreCase)) {
                continue
            }
            $identity = Get-FnvxrProductFileIdentity -Path $module.path -RequirePe
            if ($identity.sha256 -cne $ExpectedSha256) {
                throw "Loaded module hash differs from the temporary desktop-assist stage: $ExpectedPath"
            }
            $identity | Add-Member -NotePropertyName loadedModuleBaseAddress `
                -NotePropertyValue ("0x{0:x}" -f [uint64]$module.baseAddress)
            $identity | Add-Member -NotePropertyName loadedModuleImageSize `
                -NotePropertyValue ([uint32]$module.imageSize)
            $identity | Add-Member -NotePropertyName loadedModuleCensus `
                -NotePropertyValue ([string]$module.census)
            return $identity
        }
        Start-Sleep -Milliseconds 200
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Timed out waiting for the exact temporary desktop-assist module: $ExpectedPath"
}

function Get-DesktopAssistRecoverySavePath {
    $saveRoot = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "My Games\FalloutNV\Saves"
    return Join-Path $saveRoot "FNVXR_HostExitRecovery.fos"
}

function Get-DesktopAssistProbeSnapshot {
    param(
        [Parameter(Mandatory = $true)][string]$ProbePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$LogPath
    )

    $output = & $ProbePath @Arguments 2>&1 | Out-String
    $exitCode = $LASTEXITCODE
    Add-Content -LiteralPath $LogPath -Value $output -Encoding UTF8
    $json = $null
    try {
        $json = $output | ConvertFrom-Json -ErrorAction Stop
    } catch {}
    return [pscustomobject][ordered]@{
        exitCode = $exitCode
        output = $output
        json = $json
    }
}

function Wait-DesktopAssistProbeCondition {
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
        $sample = Get-DesktopAssistProbeSnapshot `
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

function Wait-DesktopAssistStartMenu {
    param(
        [Parameter(Mandatory = $true)][string]$ProbePath,
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$RequiredProcess,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds,
        [Parameter(Mandatory = $true)][string]$LogPath
    )

    return Wait-DesktopAssistProbeCondition `
        -ProbePath $ProbePath `
        -Arguments @("--require-runtime", "--require-advancing", "--sample-delay-ms", "100") `
        -RequiredProcess $RequiredProcess `
        -TimeoutSeconds $TimeoutSeconds `
        -LogPath $LogPath `
        -Description "the real Fallout Start Menu before recovery load" `
        -Accept {
            param($snapshot)
            return [uint32]$snapshot.runtime.phase -eq 1 `
                -and (([uint32]$snapshot.runtime.menuBits -band 2) -ne 0) `
                -and [bool]$snapshot.runtime.uiInputAllowed
        }
}

function Wait-DesktopAssistGameplay {
    param(
        [Parameter(Mandatory = $true)][string]$ProbePath,
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$RequiredProcess,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds,
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string]$Description
    )

    return Wait-DesktopAssistProbeCondition `
        -ProbePath $ProbePath `
        -Arguments @("--require-runtime", "--require-advancing", "--sample-delay-ms", "100") `
        -RequiredProcess $RequiredProcess `
        -TimeoutSeconds $TimeoutSeconds `
        -LogPath $LogPath `
        -Description $Description `
        -Accept {
            param($snapshot)
            return [uint32]$snapshot.runtime.phase -eq 3 `
                -and [bool]$snapshot.runtime.cameraActive
        }
}

function Invoke-DesktopAssistRecoveryLoad {
    param(
        [Parameter(Mandatory = $true)][string]$CommandPath,
        [Parameter(Mandatory = $true)][string]$LogPath
    )

    # The plugin's desktop-assist consumer rejects every other mailbox action.
    # Keep this exact string in one place so the supervisor, source fuse, and
    # telemetry can all identify the same bounded recovery action.
    $output = & $CommandPath console "load FNVXR_HostExitRecovery" --wait-ms 15000 2>&1 | Out-String
    $exitCode = $LASTEXITCODE
    Add-Content -LiteralPath $LogPath -Value $output -Encoding UTF8
    $record = [pscustomobject][ordered]@{
        action = "load-fixed-recovery-save"
        command = "load FNVXR_HostExitRecovery"
        exitCode = $exitCode
        output = $output
        completed = $exitCode -eq 0
    }
    if (-not $record.completed) {
        throw "Desktop-assist fixed recovery load was rejected or timed out: $output"
    }
    return $record
}

function Initialize-DesktopAssistNativeEscapeInput {
    if ("Fnvxr.DesktopAssist.NativeInput" -as [type]) {
        return
    }

    Add-Type -Language CSharp -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Threading;

namespace Fnvxr.DesktopAssist
{
    public static class NativeInput
    {
        private const uint InputKeyboard = 1;
        private const uint KeyeventfKeyup = 0x0002;
        private const ushort VkEscape = 0x1B;
        private const int SwRestore = 9;

        private delegate bool EnumWindowsProc(IntPtr window, IntPtr parameter);

        [StructLayout(LayoutKind.Sequential)]
        private struct Input
        {
            public uint type;
            public InputUnion union;
        }

        [StructLayout(LayoutKind.Explicit)]
        private struct InputUnion
        {
            [FieldOffset(0)] public KeyboardInput keyboard;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct KeyboardInput
        {
            public ushort virtualKey;
            public ushort scanCode;
            public uint flags;
            public uint time;
            public IntPtr extraInfo;
        }

        [DllImport("user32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);

        [DllImport("user32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool IsWindowVisible(IntPtr window);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);

        [DllImport("user32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool ShowWindow(IntPtr window, int command);

        [DllImport("user32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool SetForegroundWindow(IntPtr window);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern IntPtr GetForegroundWindow();

        [DllImport("user32.dll", SetLastError = true)]
        private static extern uint SendInput(uint count, Input[] inputs, int inputSize);

        private static IntPtr FindVisibleWindow(uint processId)
        {
            IntPtr found = IntPtr.Zero;
            EnumWindows(delegate(IntPtr candidate, IntPtr ignored)
            {
                uint candidateProcessId;
                GetWindowThreadProcessId(candidate, out candidateProcessId);
                if (candidateProcessId == processId && IsWindowVisible(candidate))
                {
                    found = candidate;
                    return false;
                }
                return true;
            }, IntPtr.Zero);
            return found;
        }

        public static bool SendEscapeToProcess(
            uint processId,
            out IntPtr window,
            out uint foregroundProcessId,
            out uint inserted,
            out int lastError)
        {
            window = FindVisibleWindow(processId);
            foregroundProcessId = 0;
            inserted = 0;
            lastError = 0;
            if (window == IntPtr.Zero)
            {
                lastError = Marshal.GetLastWin32Error();
                return false;
            }

            ShowWindow(window, SwRestore);
            SetForegroundWindow(window);
            Thread.Sleep(50);
            IntPtr foreground = GetForegroundWindow();
            GetWindowThreadProcessId(foreground, out foregroundProcessId);
            // The process check prevents an Escape from leaking into another
            // app, and the HWND check closes the remaining same-process race:
            // a Fallout-owned modal/error window must not substitute for the
            // exact top-level window this supervisor selected and focused.
            if (foregroundProcessId != processId || foreground != window)
                return false;

            Input[] inputs = new Input[2];
            inputs[0].type = InputKeyboard;
            inputs[0].union.keyboard.virtualKey = VkEscape;
            inputs[1].type = InputKeyboard;
            inputs[1].union.keyboard.virtualKey = VkEscape;
            inputs[1].union.keyboard.flags = KeyeventfKeyup;
            inserted = SendInput(2, inputs, Marshal.SizeOf(typeof(Input)));
            lastError = Marshal.GetLastWin32Error();
            return inserted == 2;
        }
    }
}
'@
}

function Send-DesktopAssistEscape {
    param([Parameter(Mandatory = $true)][System.Diagnostics.Process]$Process)

    Initialize-DesktopAssistNativeEscapeInput
    $window = [IntPtr]::Zero
    [uint32]$foregroundProcessId = 0
    [uint32]$inserted = 0
    [int]$lastError = 0
    $sent = [Fnvxr.DesktopAssist.NativeInput]::SendEscapeToProcess(
        [uint32]$Process.Id,
        [ref]$window,
        [ref]$foregroundProcessId,
        [ref]$inserted,
        [ref]$lastError)
    $record = [pscustomobject][ordered]@{
        action = "tap-escape"
        processId = $Process.Id
        window = ("0x{0:x}" -f [uint64]$window.ToInt64())
        foregroundProcessId = $foregroundProcessId
        inserted = $inserted
        lastError = $lastError
        sent = [bool]$sent
    }
    if (-not $record.sent) {
        throw "Desktop-assist automated Escape was refused because the exact Fallout window was not foreground or SendInput failed."
    }
    return $record
}

function Invoke-DesktopAssistAutomatedMenuRoundTrip {
    param(
        [Parameter(Mandatory = $true)][string]$ProbePath,
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$RequiredProcess,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds,
        [Parameter(Mandatory = $true)][string]$LogPath
    )

    $result = [ordered]@{
        scope = "exact Fallout foreground check followed by two Escape taps; no mouse, text, controller, or game-world input"
        poseAppliedBeforeOpen = $null
        uiCaptureAfterOpen = $null
        gameplayAfterClose = $null
        actions = @()
    }
    $result.poseAppliedBeforeOpen = Wait-DesktopAssistProbeCondition `
        -ProbePath $ProbePath `
        -Arguments @("--require-desktop-assist", "--require-advancing", "--sample-delay-ms", "100") `
        -RequiredProcess $RequiredProcess `
        -TimeoutSeconds $TimeoutSeconds `
        -LogPath $LogPath `
        -Description "a current desktop-assist pose before opening the menu" `
        -Accept { param($snapshot) return $true }
    $result.actions += Send-DesktopAssistEscape -Process $RequiredProcess
    $result.uiCaptureAfterOpen = Wait-DesktopAssistProbeCondition `
        -ProbePath $ProbePath `
        -Arguments @("--require-desktop-assist-ui-quad", "--require-advancing", "--sample-delay-ms", "100") `
        -RequiredProcess $RequiredProcess `
        -TimeoutSeconds $TimeoutSeconds `
        -LogPath $LogPath `
        -Description "a real UI quad capture after the automated Escape" `
        -Accept { param($snapshot) return $true }
    $result.actions += Send-DesktopAssistEscape -Process $RequiredProcess
    $result.gameplayAfterClose = Wait-DesktopAssistGameplay `
        -ProbePath $ProbePath `
        -RequiredProcess $RequiredProcess `
        -TimeoutSeconds $TimeoutSeconds `
        -LogPath $LogPath `
        -Description "gameplay after the automated menu close"
    return [pscustomobject]$result
}

$root = Get-FnvxrProductRoot
$buildPlan = @(Get-DesktopAssistBuildPlan `
    -Root $root `
    -Configuration $Configuration)
$buildRecords = @()
if (-not $ValidateOnly) {
    # Build before preflight/staging so an approved run can never stage an
    # older DLL merely because a previous product build happened to exist.
    # A build failure occurs before any game-tree or process mutation.
    $buildRecords = @(Invoke-DesktopAssistBuildPlan `
        -Plan $buildPlan `
        -Configuration $Configuration)
}
$preflightScript = Join-Path $PSScriptRoot "validate-desktop-assist.ps1"
$preflightParameters = @{
    Configuration = $Configuration
    GameRoot = $GameRoot
    RequireNoRunningGame = $true
}
if ($AutomateAcceptance) {
    $preflightParameters.RequireAutomationRecoverySave = $true
}
$preflightText = & $preflightScript @preflightParameters 2>&1 | Out-String
$preflightExit = $LASTEXITCODE
if ($preflightExit -ne 0) {
    throw "Desktop-assist preflight failed with exit code $preflightExit`: $preflightText"
}
try {
    $preflight = $preflightText | ConvertFrom-Json -ErrorAction Stop
} catch {
    throw "Desktop-assist preflight did not produce valid JSON: $($_.Exception.Message)"
}
if (-not [bool]$preflight.requiredArtifactsPresent -or
    -not [bool]$preflight.noRunningGame -or
    [string]::IsNullOrWhiteSpace([string]$preflight.gameRootResolved)) {
    throw "Desktop-assist preflight did not establish a stageable, idle game root."
}
if ($AutomateAcceptance -and -not [bool]$preflight.automation.fixedRecoverySave.exists) {
    throw "Desktop-assist automation requires the fixed recovery save verified by preflight."
}

$game = Assert-FnvxrProductGameRoot -GameRoot $preflight.gameRootResolved
$stagePlan = @(Get-DesktopAssistStagePlan `
    -Root $root `
    -Configuration $Configuration `
    -GameRoot $game.root)
if ($stagePlan.Count -ne 2) {
    throw "Desktop-assist stage plan must contain exactly the D3D9 proxy and NVSE plugin."
}
foreach ($item in $stagePlan) {
    $identity = Get-FnvxrProductFileIdentity -Path $item.source -RequirePe
    if ($identity.peMachine -cne $item.machine) {
        throw "Desktop-assist source has the wrong architecture: $($item.source)"
    }
}

if ($ValidateOnly) {
    [pscustomobject][ordered]@{
        schema = "fnvxr-desktop-assist-launch-v1"
        liveActionsTaken = $false
        scope = "read-only desktop-assist stage/launch plan; no game-tree or process mutation"
        game = $game
        preflight = $preflight
        buildPlan = $buildPlan
        buildPerformed = $false
        stagePlan = $stagePlan
        requiredEnvironment = Get-DesktopAssistEnvironment `
            -RunId "validate-only" `
            -RunDirectory "validate-only" `
            -TrackedPropVisualTrial ([bool]$TrackedPropVisualTrial)
        automaticRestoration = "Not applicable in validate-only mode; live mode restores both staged files after its owned game process stops."
    } | ConvertTo-Json -Depth 10
    return
}

$existing = @(Get-Process FalloutNV,nvse_loader -ErrorAction SilentlyContinue)
if ($existing.Count -ne 0) {
    $summary = @($existing | ForEach-Object { "$($_.ProcessName):$($_.Id)" }) -join ", "
    throw "Desktop-assist launch refused because an existing game process is not owned by this supervisor: $summary"
}

$runId = "{0}-{1}" -f (Get-Date -Format "yyyyMMdd-HHmmss-fff"), [Guid]::NewGuid().ToString("N").Substring(0, 12)
$runDirectory = Join-Path $root "local\desktop-assist-runs\$runId"
$backupRoot = Join-Path $runDirectory "backups"
$manifestPath = Join-Path $runDirectory "manifest.json"
$probeLog = Join-Path $runDirectory "runtime-probe.log"
$supervisorLog = Join-Path $runDirectory "supervisor.log"
$acceptanceLog = Join-Path $runDirectory "assist-acceptance.log"
$acceptanceErrorLog = Join-Path $runDirectory "assist-acceptance.err.log"
$acceptanceReport = Join-Path $runDirectory "assist-head-body.json"
$engineEvidenceLog = Join-Path $runDirectory "retail-runtime-evidence.log"
$automationLog = Join-Path $runDirectory "desktop-automation.log"
$probePath = Join-Path $root "build-product-x64\$Configuration\fnvxr_shared_state_probe.exe"
$assistPath = Join-Path $root "build-product-x64\$Configuration\fnvxr_assist.exe"
$retailRuntimeProbePath = Join-Path $root "build-product-x64\$Configuration\fnvxr_retail_runtime_probe.exe"
$commandPath = Join-Path $root "build-product-x64\$Configuration\fnvxr_command.exe"
New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null

$manifest = [ordered]@{
    schema = "fnvxr-desktop-assist-launch-v1"
    scope = if ($TrackedPropVisualTrial) {
        "temporary two-file tracked-prop visual stage; controller-driven first-person rig and weapon transforms only; no OpenXR, stereo-world, input, projectile, hit, renderer, replay, UI capture, or world-transform path"
    } elseif ($AutomateAcceptance) {
        "temporary two-file desktop-assist stage; no OpenXR, stereo-world, rig, weapon, or world-transform path; explicit automation is limited to one fixed recovery load and two exact foreground-verified Escape taps"
    } else {
        "temporary two-file desktop-assist stage; no OpenXR, stereo-world, input, rig, weapon, or world-transform path"
    }
    runId = $runId
    startedAtUtc = [DateTime]::UtcNow.ToString("o")
    state = "initializing"
    error = $null
    game = $game
    preflight = $preflight
    build = [ordered]@{
        performed = $true
        plan = $buildPlan
        records = $buildRecords
    }
    stagePlan = $stagePlan
    staged = @()
    environment = $null
    processes = [ordered]@{}
    readiness = [ordered]@{
        exactModules = $false
        runtimeState = $false
        desktopAssistReady = $false
        readyForManualAssist = $false
        startMenuForAutomation = $false
    }
    acceptance = [ordered]@{
        requested = [bool]$RunAcceptanceTrial
        requestedCycles = $AcceptanceTrialCycles
        requestedStepMilliseconds = $AcceptanceStepMilliseconds
        expectedDurationSeconds = $acceptanceTrialDurationSeconds
        readinessObserved = $false
        startedAtUtc = $null
        completedAtUtc = $null
        exitCode = $null
        reportPath = $acceptanceReport
        report = $null
        evidence = $null
        passed = $false
    }
    automation = [ordered]@{
        requested = [bool]$AutomateAcceptance
        fixedRecoverySave = if ($AutomateAcceptance) { Get-DesktopAssistRecoverySavePath } else { $null }
        startMenu = $null
        recoveryLoad = $null
        menuRoundTrip = $null
        passed = $false
        scope = "default profile has no input; explicit automation is one fixed recovery load and exactly two Escape taps after foreground verification"
    }
    engineEvidence = [ordered]@{
        requested = [bool]$CollectEngineEvidence
        waitMilliseconds = $EngineEvidenceWaitMilliseconds
        probePath = $retailRuntimeProbePath
        readinessObserved = $false
        startedAtUtc = $null
        completedAtUtc = $null
        result = $null
    }
    cleanup = [ordered]@{
        falloutStopped = $false
        nvseLoaderStopped = $false
        assistStopped = $false
        stageRestorationRequired = $false
        stagedArtifactsRestored = $false
        failedStageRolledBack = $false
    }
    logs = [ordered]@{
        supervisor = $supervisorLog
        runtimeProbe = $probeLog
        acceptance = $acceptanceLog
        acceptanceError = $acceptanceErrorLog
        acceptanceReport = $acceptanceReport
        engineEvidence = $engineEvidenceLog
        automation = $automationLog
        menuSource = Join-Path $runDirectory "fnvxr_desktop_assist_ui.log"
    }
}
Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath

function Write-DesktopAssistSupervisorLog {
    param([Parameter(Mandatory = $true)][string]$Message)
    Add-Content -LiteralPath $supervisorLog -Value (
        "{0} {1}" -f [DateTime]::UtcNow.ToString("o"), $Message) -Encoding UTF8
}

$savedEnvironment = @{}
foreach ($entry in Get-ChildItem Env: | Where-Object { $_.Name -like "FNVXR_*" }) {
    $savedEnvironment[$entry.Name] = $entry.Value
}
$staged = @()
$nvse = $null
$fallout = $null
$assist = $null
$normalCompletion = $false

try {
    $environment = Get-DesktopAssistEnvironment `
        -RunId $runId `
        -RunDirectory $runDirectory `
        -AutomateAcceptance ([bool]$AutomateAcceptance) `
        -TrackedPropVisualTrial ([bool]$TrackedPropVisualTrial)
    Get-ChildItem Env: | Where-Object { $_.Name -like "FNVXR_*" } | ForEach-Object {
        Remove-Item -LiteralPath ("Env:{0}" -f $_.Name) -ErrorAction SilentlyContinue
    }
    foreach ($key in $environment.Keys) {
        Set-Item -LiteralPath ("Env:{0}" -f $key) -Value ([string]$environment[$key])
    }
    $manifest.environment = $environment
    $manifest.state = "staging-desktop-assist-artifacts"
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath

    $staged = @(Install-FnvxrProductArtifactSet `
        -Plan $stagePlan `
        -BackupRoot $backupRoot `
        -RunId $runId)
    $manifest.staged = $staged
    $manifest.state = "starting-desktop-fallout"
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath

    $nvse = Start-Process `
        -FilePath $game.nvseLoader.path `
        -WorkingDirectory $game.root `
        -PassThru
    $manifest.processes.nvseLoader = [ordered]@{
        processId = $nvse.Id
        path = $game.nvseLoader.path
        startedAtUtc = $nvse.StartTime.ToUniversalTime().ToString("o")
    }
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath

    $startupDeadline = [DateTime]::UtcNow.AddSeconds($StartupTimeoutSeconds)
    do {
        $fallout = Get-ExactDesktopAssistFalloutProcess -ExpectedPath $game.fallout.path
        if ($fallout) { break }
        Start-Sleep -Milliseconds 200
    } while ([DateTime]::UtcNow -lt $startupDeadline)
    if (-not $fallout) {
        throw "Timed out waiting for the exact desktop FalloutNV.exe process."
    }

    $manifest.processes.fallout = [ordered]@{
        processId = $fallout.Id
        path = $game.fallout.path
        startedAtUtc = $fallout.StartTime.ToUniversalTime().ToString("o")
        sha256 = $game.fallout.sha256
    }
    $d3d9Record = @($staged | Where-Object { $_.key -eq "x86/d3d9.dll" })[0]
    $pluginRecord = @($staged | Where-Object { $_.key -eq "x86/nvse_fnvxr.dll" })[0]
    $loadedD3d9 = Wait-DesktopAssistLoadedModule `
        -Process $fallout `
        -ExpectedPath $d3d9Record.destination.path `
        -ExpectedSha256 $d3d9Record.destination.sha256 `
        -TimeoutSeconds $StartupTimeoutSeconds
    $loadedPlugin = Wait-DesktopAssistLoadedModule `
        -Process $fallout `
        -ExpectedPath $pluginRecord.destination.path `
        -ExpectedSha256 $pluginRecord.destination.sha256 `
        -TimeoutSeconds $StartupTimeoutSeconds
    $manifest.processes.fallout.loadedDesktopAssistModules = @($loadedD3d9, $loadedPlugin)
    $manifest.readiness.exactModules = $true
    $manifest.state = if ($AutomateAcceptance) {
        "waiting-for-desktop-assist-automation-start-menu"
    } elseif ($TrackedPropVisualTrial) {
        "ready-for-manual-tracked-prop-visual-trial"
    } else {
        "ready-for-manual-desktop-assist"
    }
    $manifest.readiness.readyForManualAssist = $true
    if ($AutomateAcceptance) {
        Write-DesktopAssistSupervisorLog (
            "Exact two-file stage loaded; waiting for the real Start Menu before the fixed recovery load. " +
            "No OpenXR/world stereo/input bridge is enabled.")
    } elseif ($TrackedPropVisualTrial) {
        Write-DesktopAssistSupervisorLog "Exact two-file tracked-prop visual stage loaded; enter first-person gameplay, then run fnvxr_assist --tracked-prop manually. No input, projectile, hit, renderer, replay, or OpenXR path is enabled."
    } else {
        Write-DesktopAssistSupervisorLog "Exact two-file stage loaded; use normal desktop controls plus fnvxr_assist for the approved head/body and menu transition checks."
    }
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath

    if ($AutomateAcceptance) {
        $startMenu = Wait-DesktopAssistStartMenu `
            -ProbePath $probePath `
            -RequiredProcess $fallout `
            -TimeoutSeconds $StartupTimeoutSeconds `
            -LogPath $probeLog
        $manifest.readiness.startMenuForAutomation = $true
        $manifest.automation.startMenu = $startMenu.json.runtime
        $manifest.state = "loading-desktop-assist-fixed-recovery-save"
        Write-DesktopAssistSupervisorLog "Real Fallout Start Menu proven; submitting the exact fixed recovery load."
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        $manifest.automation.recoveryLoad = Invoke-DesktopAssistRecoveryLoad `
            -CommandPath $commandPath `
            -LogPath $automationLog
        Write-DesktopAssistSupervisorLog "Fixed recovery load completed; waiting for first-person body-root readiness."
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
    }

    $completion = $null
    $requiresDesktopAssistReadiness = $RunAcceptanceTrial -or $CollectEngineEvidence
    if ($requiresDesktopAssistReadiness) {
        $readinessPurpose = if ($RunAcceptanceTrial -and $CollectEngineEvidence) {
            "the read-only engine evidence capture and synthetic acceptance trace"
        } elseif ($RunAcceptanceTrial) {
            "the synthetic acceptance trace"
        } else {
            "the read-only engine evidence capture"
        }
        $manifest.state = "waiting-for-desktop-assist-body-root-readiness"
        Write-DesktopAssistSupervisorLog (
            "Waiting up to {0}s for first-person body-root readiness before {1}." -f
            $AcceptanceReadinessTimeoutSeconds,
            $readinessPurpose)
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        if ($AutomateAcceptance) {
            Write-Host (
                "Desktop assist loaded the fixed recovery save. The no-headset trace will begin automatically " +
                "once first-person body-root readiness is observed.")
        } else {
            Write-Host (
                "Desktop assist is staged. Load or resume first-person gameplay on the desktop; " +
                "the requested no-headset work will begin automatically once the explicit body-root observation is ready.")
        }
        Wait-FnvxrProductProbeReady `
            -ProbePath $probePath `
            -Arguments @("--require-desktop-assist-ready", "--require-advancing", "--sample-delay-ms", "100") `
            -RequiredProcess $fallout `
            -TimeoutSeconds $AcceptanceReadinessTimeoutSeconds `
            -LogPath $probeLog `
            -Description "desktop-assist first-person body-root readiness"
        $manifest.readiness.desktopAssistReady = $true
        if ($RunAcceptanceTrial) {
            $manifest.acceptance.readinessObserved = $true
        }
        if ($CollectEngineEvidence) {
            $manifest.engineEvidence.readinessObserved = $true
        }
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
    }

    if ($CollectEngineEvidence) {
        $manifest.state = "collecting-read-only-engine-evidence"
        $manifest.engineEvidence.startedAtUtc = [DateTime]::UtcNow.ToString("o")
        Write-DesktopAssistSupervisorLog (
            "Collecting read-only loaded-process engine evidence with no headset, OpenXR, or game-state mutation.")
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        $manifest.engineEvidence.result = Invoke-FnvxrProductReadOnlyRetailRuntimeProbe `
            -ProbePath $retailRuntimeProbePath `
            -ProcessId ([uint32]$fallout.Id) `
            -WaitMilliseconds $EngineEvidenceWaitMilliseconds `
            -LogPath $engineEvidenceLog
        $manifest.engineEvidence.completedAtUtc = [DateTime]::UtcNow.ToString("o")
        Write-DesktopAssistSupervisorLog (
            "Read-only engine evidence captured exit={0} probeReportedEngineCapability={1}; this is an observation, not a stereo authorization." -f
            $manifest.engineEvidence.result.exitCode,
            $manifest.engineEvidence.result.probeReportedEngineCapability)
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        if (-not $RunAcceptanceTrial) {
            $completion = "desktop-assist-engine-evidence-captured"
        }
    }

    if ($RunAcceptanceTrial) {
        $manifest.acceptance.startedAtUtc = [DateTime]::UtcNow.ToString("o")
        $manifest.state = "running-desktop-assist-acceptance"
        Write-DesktopAssistSupervisorLog (
            "Desktop-assist body-root readiness proved; starting {0}s synthetic head/body and menu-source trace." -f
            $acceptanceTrialDurationSeconds)
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        if ($AutomateAcceptance) {
            Write-Host (
                "Acceptance trace is running for about {0} seconds. The supervisor will make one verified desktop menu round-trip " +
                "with two Escape taps; it will reject any unpaired or stale capture." -f $acceptanceTrialDurationSeconds)
        } else {
            Write-Host (
                "Acceptance trace is running for about {0} seconds. On the desktop, open a normal menu and return to gameplay once; " +
                "the test will reject a stale or unpaired menu capture." -f $acceptanceTrialDurationSeconds)
        }

        $acceptanceArguments = @(
            "--scenario", "head-body",
            "--step-ms", [string]$AcceptanceStepMilliseconds,
            "--period-ms", "20",
            "--cycles", [string]$AcceptanceTrialCycles,
            "--require-desktop-assist",
            "--require-ui-quad-transition",
            "--report", $acceptanceReport)
        $acceptanceOutput = ""
        $acceptanceExitCode = $null
        if ($AutomateAcceptance) {
            $assist = Start-Process `
                -FilePath $assistPath `
                -ArgumentList $acceptanceArguments `
                -RedirectStandardOutput $acceptanceLog `
                -RedirectStandardError $acceptanceErrorLog `
                -WindowStyle Hidden `
                -PassThru
            $manifest.acceptance.processId = $assist.Id
            Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
            try {
                $manifest.automation.menuRoundTrip = Invoke-DesktopAssistAutomatedMenuRoundTrip `
                    -ProbePath $probePath `
                    -RequiredProcess $fallout `
                    -TimeoutSeconds $AcceptanceReadinessTimeoutSeconds `
                    -LogPath $automationLog
                $manifest.automation.passed = $manifest.automation.recoveryLoad.completed `
                    -and $manifest.automation.menuRoundTrip.actions.Count -eq 2
                if (-not $assist.WaitForExit((($acceptanceTrialDurationSeconds + 30) * 1000))) {
                    throw "Desktop-assist acceptance harness exceeded its bounded trace duration."
                }
                $acceptanceExitCode = $assist.ExitCode
                $acceptanceOutput = @(
                    if (Test-Path -LiteralPath $acceptanceLog) {
                        Get-Content -LiteralPath $acceptanceLog -Raw
                    }
                    if (Test-Path -LiteralPath $acceptanceErrorLog) {
                        Get-Content -LiteralPath $acceptanceErrorLog -Raw
                    }) -join "`n"
            } catch {
                Stop-FnvxrOwnedProcess -Process $assist
                throw
            }
        } else {
            $acceptanceOutput = & $assistPath @acceptanceArguments 2>&1 | Out-String
            $acceptanceExitCode = $LASTEXITCODE
            Add-Content -LiteralPath $acceptanceLog -Value $acceptanceOutput -Encoding UTF8
        }
        $manifest.acceptance.exitCode = $acceptanceExitCode
        $manifest.acceptance.completedAtUtc = [DateTime]::UtcNow.ToString("o")
        if (Test-Path -LiteralPath $acceptanceReport) {
            try {
                $manifest.acceptance.report = Get-Content -LiteralPath $acceptanceReport -Raw | ConvertFrom-Json -ErrorAction Stop
            } catch {
                $manifest.acceptance.reportParseError = $_.Exception.Message
            }
        }
        $manifest.acceptance.evidence = Get-DesktopAssistAcceptanceReportEvidence `
            $manifest.acceptance.report
        $manifest.acceptance.passed = $acceptanceExitCode -eq 0 `
            -and $manifest.acceptance.report `
            -and $manifest.acceptance.evidence.complete -eq $true
        if (-not $manifest.acceptance.passed) {
            Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
            throw "Desktop-assist acceptance trial failed; inspect $acceptanceReport and $acceptanceLog."
        }
        Write-DesktopAssistSupervisorLog "Desktop-assist acceptance trial passed with current-phase local-camera, body-root rotation/position, and UI-transition evidence."
        $manifest.state = "desktop-assist-acceptance-passed"
        Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
        $completion = "desktop-assist-acceptance-passed"
    }

    if (-not $completion) {
        $deadline = [DateTime]::UtcNow.AddSeconds($MaximumRunSeconds)
        do {
            $fallout.Refresh()
            if ($fallout.HasExited) {
                $completion = "retail-exited"
                break
            }
            if (Test-FnvxrProductProbeReady `
                    -ProbePath $probePath `
                    -Arguments @("--require-runtime", "--require-advancing", "--sample-delay-ms", "100") `
                    -LogPath $probeLog) {
                $manifest.readiness.runtimeState = $true
            }
            Start-Sleep -Milliseconds 500
        } while ([DateTime]::UtcNow -lt $deadline)
    }
    if (-not $completion) {
        $completion = "supervised-time-limit"
    }
    $normalCompletion = $true
    $manifest.completion = $completion
    $manifest.state = "cleaning-up"
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
} catch {
    $manifest.error = $_.Exception.Message
    $manifest.state = "failed"
    Write-DesktopAssistSupervisorLog ("ERROR " + $_.Exception.Message)
} finally {
    Stop-FnvxrOwnedProcess -Process $assist
    if ($assist) {
        try {
            $assist.Refresh()
            $manifest.cleanup.assistStopped = $assist.HasExited
        } catch {}
    } else {
        $manifest.cleanup.assistStopped = $true
    }
    Stop-FnvxrOwnedProcess -Process $fallout
    if ($fallout) {
        try {
            $fallout.Refresh()
            $manifest.cleanup.falloutStopped = $fallout.HasExited
        } catch {}
    } else {
        $manifest.cleanup.falloutStopped = $true
    }
    Stop-FnvxrOwnedProcess -Process $nvse
    if ($nvse) {
        try {
            $nvse.Refresh()
            $manifest.cleanup.nvseLoaderStopped = $nvse.HasExited
        } catch {}
    } else {
        $manifest.cleanup.nvseLoaderStopped = $true
    }

    if ($staged.Count -gt 0) {
        $manifest.cleanup.stageRestorationRequired = $true
        try {
            Restore-FnvxrProductArtifactSet -Records $staged
            $manifest.cleanup.stagedArtifactsRestored = $true
            if ($manifest.error) {
                $manifest.cleanup.failedStageRolledBack = $true
            }
        } catch {
            $manifest.cleanup.rollbackError = $_.Exception.Message
            if (-not $manifest.error) {
                $manifest.error = "Desktop-assist staged artifact restoration failed: $($_.Exception.Message)"
                $manifest.state = "failed"
                Write-DesktopAssistSupervisorLog ("ERROR " + $manifest.error)
            }
        }
    }

    Get-ChildItem Env: | Where-Object { $_.Name -like "FNVXR_*" } | ForEach-Object {
        Remove-Item -LiteralPath ("Env:{0}" -f $_.Name) -ErrorAction SilentlyContinue
    }
    foreach ($key in $savedEnvironment.Keys) {
        Set-Item -LiteralPath ("Env:{0}" -f $key) -Value ([string]$savedEnvironment[$key])
    }
    $manifest.completedAtUtc = [DateTime]::UtcNow.ToString("o")
    if (-not $manifest.error -and $normalCompletion) {
        $manifest.state = "complete"
    }
    Write-FnvxrProductJsonAtomic -Value $manifest -Path $manifestPath
}

if ($manifest.error) {
    throw $manifest.error
}
$manifest | ConvertTo-Json -Depth 12
