param(
    [Parameter(Mandatory = $true)][int]$FalloutPid,
    [Parameter(Mandatory = $true)][string]$HostExe,
    [Parameter(Mandatory = $true)][string]$WorkingDir,
    [int]$Frames = 0,
    [Parameter(Mandatory = $true)][string]$HostOut,
    [Parameter(Mandatory = $true)][string]$HostErr,
    [Parameter(Mandatory = $true)][string]$DebugLog,
    [int]$PollMs = 2000,
    [int]$MaxRestarts = 0
)

$ErrorActionPreference = "SilentlyContinue"

throw "Legacy host restart loops are intentionally blocked: process-existence polling cannot prove OpenXR progress and may relaunch into a live retail process."

function Write-WatchdogLog {
    param([string]$Message)

    $line = "{0:o} watchdog {1}" -f (Get-Date), $Message
    Add-Content -LiteralPath $DebugLog -Value $line -Encoding UTF8
}

Write-WatchdogLog ("start falloutPid={0} frames={1}" -f $FalloutPid, $Frames)

if ($Frames -gt 0 -and $MaxRestarts -gt 0) {
    Write-WatchdogLog ("unsafe finite-frame restart refused frames={0} maxRestarts={1}" -f $Frames, $MaxRestarts)
    exit 2
}

$lastHostPid = $null
$restartCount = 0

function Resolve-GameProcess {
    param([int]$PreferredPid)

    $byId = Get-Process -Id $PreferredPid -ErrorAction SilentlyContinue
    if ($null -ne $byId) {
        return $byId
    }

    $fallout = @(Get-Process FalloutNV -ErrorAction SilentlyContinue | Sort-Object StartTime -Descending | Select-Object -First 1)
    if ($fallout.Count -gt 0) {
        return $fallout[0]
    }

    $loader = @(Get-Process nvse_loader -ErrorAction SilentlyContinue | Sort-Object StartTime -Descending | Select-Object -First 1)
    if ($loader.Count -gt 0) {
        return $loader[0]
    }

    return $null
}

while ($true) {
    $falloutProcess = Resolve-GameProcess -PreferredPid $FalloutPid
    if ($null -eq $falloutProcess) {
        Write-WatchdogLog "stop fallout exited"
        break
    }

    if ($falloutProcess.Id -ne $FalloutPid) {
        $FalloutPid = $falloutProcess.Id
        Write-WatchdogLog ("tracking game pid={0} name={1}" -f $FalloutPid, $falloutProcess.ProcessName)
    }

    $hostProcess = @(Get-Process fnvxr_openxr_pose_host -ErrorAction SilentlyContinue | Sort-Object StartTime -Descending | Select-Object -First 1)
    if ($hostProcess.Count -eq 0) {
        if ($null -ne $lastHostPid) {
            $previous = Get-Process -Id $lastHostPid -ErrorAction SilentlyContinue
            if ($null -eq $previous) {
                Write-WatchdogLog ("host pid={0} exited" -f $lastHostPid)
            }
        }

        if ($restartCount -ge $MaxRestarts) {
            Write-WatchdogLog ("host missing; restart suppressed restartCount={0} maxRestarts={1}" -f $restartCount, $MaxRestarts)
            break
        }

        $restartCount += 1
        $hostOutPath = $HostOut
        $hostErrPath = $HostErr
        if ($restartCount -gt 1) {
            $outDirectory = Split-Path -Parent $HostOut
            $outBase = [System.IO.Path]::GetFileNameWithoutExtension($HostOut)
            $outExtension = [System.IO.Path]::GetExtension($HostOut)
            $errDirectory = Split-Path -Parent $HostErr
            $errBase = [System.IO.Path]::GetFileNameWithoutExtension($HostErr)
            $errExtension = [System.IO.Path]::GetExtension($HostErr)
            $hostOutPath = Join-Path $outDirectory ("{0}.restart{1:000}{2}" -f $outBase, $restartCount, $outExtension)
            $hostErrPath = Join-Path $errDirectory ("{0}.restart{1:000}{2}" -f $errBase, $restartCount, $errExtension)
        }

        Write-WatchdogLog "host missing; restarting"
        $newHostProcess = Start-Process `
            -FilePath $HostExe `
            -ArgumentList $Frames `
            -WorkingDirectory $WorkingDir `
            -RedirectStandardOutput $hostOutPath `
            -RedirectStandardError $hostErrPath `
            -PassThru `
            -WindowStyle Hidden
        if ($null -ne $newHostProcess) {
            $lastHostPid = $newHostProcess.Id
            Write-WatchdogLog ("host restarted pid={0} out={1} err={2}" -f $newHostProcess.Id, $hostOutPath, $hostErrPath)
        }
    } else {
        $lastHostPid = $hostProcess[0].Id
    }

    Start-Sleep -Milliseconds $PollMs
}
