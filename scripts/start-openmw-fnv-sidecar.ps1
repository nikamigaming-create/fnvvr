param(
    [string]$ModlistRoot = $env:FNVXR_MODLIST_ROOT,
    [int]$MonitorSeconds = 15,
    [switch]$StopExisting,
    [switch]$NoSave,
    [switch]$EnableRetailProjectionLayer,
    [switch]$DisableRetailProjectionLayer,
    [switch]$LaunchRetailOnLeftGrip,
    [switch]$DebugRun,
    [int]$AutoLaunchRetailAfterSeconds = -1,
    [int]$RetailLaunchWatchSeconds = 300
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$RunRoot = Join-Path $Root "local\openmw-sidecar-runs"
$Stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$RunDir = Join-Path $RunRoot $Stamp
New-Item -ItemType Directory -Force -Path $RunDir | Out-Null

$runBat = Join-Path $ModlistRoot "run_vr.bat"
$configDir = Join-Path $ModlistRoot "openmw-config"
$openmwLog = Join-Path $configDir "openmw.log"
$launchLog = Join-Path $configDir "run_vr_launch.log"
$outLog = Join-Path $RunDir "run_vr.out.log"
$errLog = Join-Path $RunDir "run_vr.err.log"
$manifestPath = Join-Path $RunDir "manifest.json"

if (-not (Test-Path -LiteralPath $runBat)) {
    throw "Missing OpenMW VR launcher: $runBat"
}

function ConvertTo-NativeArgument {
    param([AllowEmptyString()][string]$Argument)

    if ($null -eq $Argument) {
        return '""'
    }
    if ($Argument -notmatch '[\s"]') {
        return $Argument
    }

    $result = '"'
    $backslashCount = 0
    foreach ($character in $Argument.ToCharArray()) {
        if ($character -eq '\') {
            ++$backslashCount
            continue
        }
        if ($character -eq '"') {
            $result += ('\' * (($backslashCount * 2) + 1))
            $result += '"'
            $backslashCount = 0
            continue
        }
        if ($backslashCount -gt 0) {
            $result += ('\' * $backslashCount)
            $backslashCount = 0
        }
        $result += $character
    }
    if ($backslashCount -gt 0) {
        $result += ('\' * ($backslashCount * 2))
    }
    $result += '"'
    return $result
}

function Join-NativeArgumentList {
    param([string[]]$ArgumentList)

    return (($ArgumentList | ForEach-Object { ConvertTo-NativeArgument $_ }) -join " ")
}

if ($StopExisting) {
    Get-Process openmw,openmw_vr,FalloutNV,nvse_loader -ErrorAction SilentlyContinue |
        Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 1
}

$env:OPENMW_FNVXR_PROFILE = "retail-projection-bridge"
$env:OPENMW_FNVXR_RETAIL_SURFACE = "1"
$retailProjectionLayerEnabled = -not [bool]$DisableRetailProjectionLayer
$env:OPENMW_FNVXR_USE_RETAIL_PROJECTION_LAYER = if ($retailProjectionLayerEnabled) { "1" } else { "0" }
$env:OPENMW_FNVXR_KEEP_RETAIL_PANEL_IN_WORLD = "0"
$env:OPENMW_FNVXR_SHOW_RETAIL_WORLD_PANEL = "0"
$env:OPENMW_FNVXR_GAMEPLAY_FORCES_WORLD_VIEW = "1"
$env:OPENMW_FNVXR_ALLOW_RUNTIME_ONLY_WORLD_READY = "0"
$env:OPENMW_FNVXR_SYNC_RETAIL_PLAYER = "1"
$env:OPENMW_FNVXR_RETAIL_PANEL_REQUIRES_GRIP = "0"
$env:OPENMW_FNVXR_RETAIL_WORLD_READY_DEBOUNCE_FRAMES = "6"
$env:OPENMW_FNVXR_RETAIL_PROJECTION_STALE_MS = "500"
$env:OPENMW_FNVXR_ALLOW_RETAIL_WORLD_LATCH = "0"
$env:OPENMW_FNVXR_RETAIL_WORLD_LATCH_MS = "0"
$env:OPENMW_FNVXR_RETAIL_PROJECTION_FLIP_Y = "1"

$launcherArgs = @("/d", "/s", "/c", "call", $runBat, "nopause")
if ($DebugRun) {
    $launcherArgs += "debugrun"
}
if ($NoSave) {
    $launcherArgs += "nosave"
} else {
    $launcherArgs += "save"
}

$startedAt = Get-Date
$launcher = Start-Process `
    -FilePath "cmd.exe" `
    -ArgumentList (Join-NativeArgumentList $launcherArgs) `
    -WorkingDirectory $ModlistRoot `
    -RedirectStandardOutput $outLog `
    -RedirectStandardError $errLog `
    -PassThru `
    -WindowStyle Hidden

$launcher.WaitForExit()
$launcher.Refresh()
$launcherExitCode = $null
try {
    if ($launcher.HasExited) {
        $launcherExitCode = $launcher.ExitCode
    }
} catch {
    $launcherExitCode = $null
}

$deadline = (Get-Date).AddSeconds($MonitorSeconds)
$processSeen = $false
do {
    $processes = @(Get-Process openmw_vr -ErrorAction SilentlyContinue)
    if ($processes.Count -gt 0) {
        $processSeen = $true
    }
    if ((Get-Date) -ge $deadline) {
        break
    }
    Start-Sleep -Milliseconds 500
} while ($true)

$proofLines = @()
if (Test-Path -LiteralPath $openmwLog) {
    $proofLines = @(Select-String -LiteralPath $openmwLog -Pattern @(
        "FNVXR retail surface",
        "retail stereo projection layer",
        "stereo world ready",
        "projection layer is live",
        "retail projection upload failed",
        "VRHandsOnly attached surfaces count",
        "VR pointer source",
        "wrist HUD layer",
        "Quitting peacefully",
        "XR_ERROR",
        "Error caught while initializing VR device"
    ) -ErrorAction SilentlyContinue | Select-Object -Last 80 | ForEach-Object { $_.Line })
    Copy-Item -LiteralPath $openmwLog -Destination (Join-Path $RunDir "openmw.log") -Force
}
if (Test-Path -LiteralPath $launchLog) {
    Copy-Item -LiteralPath $launchLog -Destination (Join-Path $RunDir "run_vr_launch.log") -Force
}

$processSnapshot = @(Get-Process openmw_vr -ErrorAction SilentlyContinue | ForEach-Object {
    [ordered]@{
        id = $_.Id
        processName = $_.ProcessName
        mainWindowTitle = $_.MainWindowTitle
        started = try { $_.StartTime.ToString("o") } catch { $null }
    }
})

$retailLaunch = [ordered]@{
    enabled = ($AutoLaunchRetailAfterSeconds -ge 0) -or [bool]$LaunchRetailOnLeftGrip
    mode = if ($AutoLaunchRetailAfterSeconds -ge 0) { "auto-delay" } elseif ($LaunchRetailOnLeftGrip) { "left-grip" } else { "disabled" }
    autoLaunchAfterSeconds = $AutoLaunchRetailAfterSeconds
    triggered = $false
    alreadyRunning = $false
    exitCode = $null
    output = $null
    watcherPid = $null
    watcherLog = $null
    watcherErrorLog = $null
}

$fatalWindow = @($processSnapshot | Where-Object { $_.mainWindowTitle -like "*Fatal error*" }).Count -gt 0
$fatalLog = $false
if (Test-Path -LiteralPath $openmwLog) {
    $fatalLog = @(Select-String -LiteralPath $openmwLog -Pattern @(
        "Unexpected destruction of LuaWorker",
        "Fatal error",
        "XR_ERROR_SESSION_LOST"
    ) -ErrorAction SilentlyContinue | Select-Object -Last 1).Count -gt 0
}
$healthy = $processSeen -and -not $fatalWindow -and -not $fatalLog

if ($healthy -and $AutoLaunchRetailAfterSeconds -ge 0) {
    $retailLaunch.alreadyRunning = @(Get-Process FalloutNV,nvse_loader -ErrorAction SilentlyContinue).Count -gt 0
    if (-not $retailLaunch.alreadyRunning) {
        Start-Sleep -Seconds $AutoLaunchRetailAfterSeconds
        $retailScript = Join-Path $PSScriptRoot "start-retail-surface-producer.ps1"
        $retailOutputPath = Join-Path $RunDir "retail-launch-auto-delay.json"
        $retailArgs = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $retailScript)
        $retailOutput = & powershell.exe @retailArgs 2>&1
        $retailLaunch.triggered = $true
        $retailLaunch.exitCode = $LASTEXITCODE
        $retailOutput | Set-Content -LiteralPath $retailOutputPath -Encoding UTF8
        $retailLaunch.output = $retailOutputPath
    }
}
elseif ($LaunchRetailOnLeftGrip -and $healthy) {
    $retailLaunch.alreadyRunning = @(Get-Process FalloutNV,nvse_loader -ErrorAction SilentlyContinue).Count -gt 0
    if (-not $retailLaunch.alreadyRunning) {
        $watcherScript = Join-Path $PSScriptRoot "watch-openmw-left-grip-launch-retail.ps1"
        $watcherOutLog = Join-Path $RunDir "retail-left-grip-watch.process.out.log"
        $watcherErrLog = Join-Path $RunDir "retail-left-grip-watch.process.err.log"
        $watcherArgs = @(
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-File", $watcherScript,
            "-OpenMwLog", $openmwLog,
            "-RunDir", $RunDir,
            "-TimeoutSeconds", "$RetailLaunchWatchSeconds"
        )
        $watcher = Start-Process `
            -FilePath "powershell.exe" `
            -ArgumentList (Join-NativeArgumentList $watcherArgs) `
            -RedirectStandardOutput $watcherOutLog `
            -RedirectStandardError $watcherErrLog `
            -PassThru `
            -WindowStyle Hidden
        $retailLaunch.watcherPid = $watcher.Id
        $retailLaunch.watcherLog = $watcherOutLog
        $retailLaunch.watcherErrorLog = $watcherErrLog
    }
}

$manifest = [ordered]@{
    startedAt = $startedAt.ToString("o")
    finishedAt = (Get-Date).ToString("o")
    role = "openmw-fnv-vr-sidecar"
    modlistRoot = $ModlistRoot
    launcher = $runBat
    runDir = $RunDir
    profile = $env:OPENMW_FNVXR_PROFILE
    retailSurface = $env:OPENMW_FNVXR_RETAIL_SURFACE
    retailProjectionLayer = $env:OPENMW_FNVXR_USE_RETAIL_PROJECTION_LAYER
    keepRetailPanelInWorld = $env:OPENMW_FNVXR_KEEP_RETAIL_PANEL_IN_WORLD
    runtimeOnlyWorldReady = $env:OPENMW_FNVXR_ALLOW_RUNTIME_ONLY_WORLD_READY
    syncRetailPlayer = $env:OPENMW_FNVXR_SYNC_RETAIL_PLAYER
    retailPanelRequiresGrip = $env:OPENMW_FNVXR_RETAIL_PANEL_REQUIRES_GRIP
    retailWorldLatchMs = $env:OPENMW_FNVXR_RETAIL_WORLD_LATCH_MS
    retailProjectionFlipY = $env:OPENMW_FNVXR_RETAIL_PROJECTION_FLIP_Y
    stereoRuntime = "default"
    launchRetailOnLeftGrip = [bool]$LaunchRetailOnLeftGrip
    retailLaunch = $retailLaunch
    launcherExitCode = $launcherExitCode
    processSeen = $processSeen
    healthy = $healthy
    fatalWindow = $fatalWindow
    fatalLog = $fatalLog
    runningProcesses = $processSnapshot
    outLog = $outLog
    errLog = $errLog
    openmwLog = if (Test-Path -LiteralPath $openmwLog) { $openmwLog } else { $null }
    launchLog = if (Test-Path -LiteralPath $launchLog) { $launchLog } else { $null }
    retailProjectionLayerEnabled = $retailProjectionLayerEnabled
    proofLines = $proofLines
}

$manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
$manifest | ConvertTo-Json -Depth 6
