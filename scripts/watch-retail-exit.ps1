param(
    [Parameter(Mandatory = $true)]
    [int]$FalloutPid,
    [Parameter(Mandatory = $true)]
    [int]$HostPid,
    [Parameter(Mandatory = $true)]
    [string]$RunDir,
    [string]$CommandExe = "",
    [string]$SaveName = "FNVXR_RecoverySave",
    [int]$CommandWaitMs = 10000,
    [int]$GameExitTimeoutSeconds = 20,
    [int]$HostExitGraceSeconds = 3,
    [switch]$SaveQuitOnHostExit
)

$ErrorActionPreference = "Stop"

$LogPath = Join-Path $RunDir "sidecar-exit-watcher.log"

function Write-WatcherLog {
    param([string]$Message)
    Add-Content -LiteralPath $LogPath -Value ("{0:o} {1}" -f (Get-Date), $Message) -Encoding UTF8
}

function Get-ExitCodeText {
    param([int]$ProcessId)
    try {
        $proc = Get-CimInstance Win32_Process -Filter ("ProcessId = {0}" -f $ProcessId) -ErrorAction SilentlyContinue
        if ($proc) {
            return "running"
        }
    } catch {
    }
    return "exited"
}

function Invoke-SaveQuit {
    param([string]$Reason)

    if (-not $SaveQuitOnHostExit) {
        Write-WatcherLog ("save-quit skipped reason={0} disabled=1" -f $Reason)
        return
    }
    if (-not $CommandExe -or -not (Test-Path -LiteralPath $CommandExe)) {
        Write-WatcherLog ("save-quit skipped reason={0} missingCommandExe='{1}'" -f $Reason, $CommandExe)
        return
    }

    Write-WatcherLog ("save-quit request reason={0} commandExe='{1}' saveName={2} waitMs={3}" -f $Reason, $CommandExe, $SaveName, $CommandWaitMs)
    try {
        & $CommandExe save-and-quit $SaveName --wait-ms $CommandWaitMs --delay-ms 1500 2>&1 |
            ForEach-Object { Write-WatcherLog ("save-quit output " + $_) }
        Write-WatcherLog ("save-quit exitCode={0}" -f $LASTEXITCODE)
    } catch {
        Write-WatcherLog ("save-quit exception reason={0} error={1}" -f $Reason, $_.Exception.Message)
    }
}

try {
    New-Item -ItemType Directory -Force -Path $RunDir | Out-Null
    Write-WatcherLog ("watch start falloutPid={0} hostPid={1} commandExe='{2}' saveQuitOnHostExit={3}" -f $FalloutPid, $HostPid, $CommandExe, [bool]$SaveQuitOnHostExit)

    $lastHeartbeat = Get-Date
    while ($true) {
        $fallout = Get-Process -Id $FalloutPid -ErrorAction SilentlyContinue
        $hostProcess = Get-Process -Id $HostPid -ErrorAction SilentlyContinue

        if (-not $hostProcess -and $fallout) {
            Write-WatcherLog ("host exited before fallout hostPid={0} falloutPid={1} hostStatus={2}" -f $HostPid, $FalloutPid, (Get-ExitCodeText -ProcessId $HostPid))
            Invoke-SaveQuit -Reason "host-exited-before-fallout"
            try {
                Write-WatcherLog ("waiting fallout after host-exit save-quit pid={0} timeoutSeconds={1}" -f $FalloutPid, $GameExitTimeoutSeconds)
                Wait-Process -Id $FalloutPid -Timeout $GameExitTimeoutSeconds -ErrorAction SilentlyContinue
            } catch {
                Write-WatcherLog ("wait fallout after host-exit exception={0}" -f $_.Exception.Message)
            }
            if (Get-Process -Id $FalloutPid -ErrorAction SilentlyContinue) {
                Write-WatcherLog ("fallout still alive after host-exit save-quit pid={0}" -f $FalloutPid)
            } else {
                Write-WatcherLog ("fallout exited after host-exit save-quit pid={0}" -f $FalloutPid)
            }
            break
        }

        if (-not $fallout) {
            Write-WatcherLog ("fallout exited pid={0}" -f $FalloutPid)
            break
        }

        if (((Get-Date) - $lastHeartbeat).TotalSeconds -ge 5) {
            $lastHeartbeat = Get-Date
            if ($env:FNVXR_TELEMETRY_HAMMER -ne "0") {
                Write-WatcherLog ("heartbeat falloutPid={0} hostPid={1} hostAlive={2}" -f $FalloutPid, $HostPid, [bool]$hostProcess)
            }
        }
        Start-Sleep -Milliseconds 250
    }

    $deadline = (Get-Date).AddSeconds($HostExitGraceSeconds)
    do {
        $hostProcess = Get-Process -Id $HostPid -ErrorAction SilentlyContinue
        if (-not $hostProcess) {
            Write-WatcherLog ("host already exited pid={0}" -f $HostPid)
            exit 0
        }
        Start-Sleep -Milliseconds 250
    } while ((Get-Date) -lt $deadline)

    $hostProcess = Get-Process -Id $HostPid -ErrorAction SilentlyContinue
    if ($hostProcess) {
        Write-WatcherLog ("stopping host after retail exit pid={0}" -f $HostPid)
        Stop-Process -Id $HostPid -Force -ErrorAction SilentlyContinue
    }
} catch {
    try {
        Write-WatcherLog ("ERROR " + $_.Exception.Message)
    } catch {
    }
    exit 1
}
