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
    [int]$HostProgressTimeoutSeconds = 15,
    [string]$LiveAnalyzer = "",
    [ValidateSet('full-vr','quad-2d-transport','diagnostic-producer')]
    [string]$ExpectedProfile = 'full-vr',
    [string]$StereoCaptureScript = "",
    [int]$StereoCaptureIntervalSeconds = 15,
    [switch]$SaveQuitOnHostExit
)

$ErrorActionPreference = "Stop"

if ($HostProgressTimeoutSeconds -lt 5 -or $HostProgressTimeoutSeconds -gt 120) {
    throw "-HostProgressTimeoutSeconds must be between 5 and 120."
}

$LogPath = Join-Path $RunDir "sidecar-exit-watcher.log"

function Write-WatcherLog {
    param([string]$Message)
    Add-Content -LiteralPath $LogPath -Value ("{0:o} {1}" -f (Get-Date), $Message) -Encoding UTF8
}

function Set-ManifestAcceptance {
    param(
        [string]$State,
        [bool]$Accepted,
        [string]$Reason
    )
    $manifestPath = Join-Path $RunDir "manifest.json"
    if (-not (Test-Path -LiteralPath $manifestPath)) { return }
    try {
        $source = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json -ErrorAction Stop
        $updated = [ordered]@{}
        foreach ($property in $source.PSObject.Properties) {
            $updated[$property.Name] = $property.Value
        }
        $updated["acceptanceState"] = $State
        $updated["accepted"] = $Accepted
        $updated["acceptanceUpdatedAt"] = (Get-Date).ToString("o")
        $updated["acceptanceReason"] = $Reason
        $temporaryPath = "$manifestPath.$PID.tmp"
        $updated | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $temporaryPath -Encoding UTF8
        Move-Item -LiteralPath $temporaryPath -Destination $manifestPath -Force
    } catch {
        Write-WatcherLog ("manifest acceptance update failed state={0} error={1}" -f $State,$_.Exception.Message)
    }
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

function Invoke-LiveAcceptanceAnalysis {
    param([switch]$Final)
    if (-not $LiveAnalyzer -or -not (Test-Path -LiteralPath $LiveAnalyzer)) {
        return
    }
    $analysisLog = Join-Path $RunDir "live-acceptance.log"
    & powershell.exe `
        -NoProfile `
        -ExecutionPolicy Bypass `
        -File $LiveAnalyzer `
        -RunDir $RunDir `
        -ExpectedProfile $ExpectedProfile 2>&1 |
        Set-Content -LiteralPath $analysisLog -Encoding UTF8
    $analysisExitCode = $LASTEXITCODE
    Write-WatcherLog ("live acceptance analysis exitCode={0} final={1}" -f $analysisExitCode,[bool]$Final)
    $verdictPath = Join-Path $RunDir "live-acceptance.json"
    if ($analysisExitCode -eq 0 -and (Test-Path -LiteralPath $verdictPath)) {
        try {
            $verdict = Get-Content -LiteralPath $verdictPath -Raw | ConvertFrom-Json -ErrorAction Stop
            if ([bool]$verdict.passed) {
                Set-ManifestAcceptance -State "accepted" -Accepted $true -Reason "live analyzer and independent capture passed"
                return
            }
        } catch {
            Write-WatcherLog ("live verdict read failed error={0}" -f $_.Exception.Message)
        }
    }
    if ($Final) {
        Set-ManifestAcceptance -State "rejected" -Accepted $false -Reason ("final live analyzer exitCode={0}" -f $analysisExitCode)
    } else {
        Set-ManifestAcceptance -State "observing" -Accepted $false -Reason ("current live analyzer exitCode={0}; acceptance is never sticky across a later failure" -f $analysisExitCode)
    }
}

function Invoke-IndependentStereoCapture {
    if (-not $StereoCaptureScript -or -not (Test-Path -LiteralPath $StereoCaptureScript)) {
        return
    }
    $captureRoot = Join-Path $RunDir "live-stereo-captures"
    $sceneName = "eye-" + (Get-Date -Format "yyyyMMdd-HHmmss-fff")
    & powershell.exe `
        -NoProfile `
        -ExecutionPolicy Bypass `
        -File $StereoCaptureScript `
        -OutputRoot $captureRoot `
        -SceneName $sceneName `
        -ExpectedProducerProcessId $FalloutPid `
        -TimeoutSeconds 2 2>&1 |
        ForEach-Object { Write-WatcherLog ("stereo capture " + $_) }
    Write-WatcherLog ("independent stereo capture exitCode={0} scene={1}" -f $LASTEXITCODE,$sceneName)
}

try {
    New-Item -ItemType Directory -Force -Path $RunDir | Out-Null
    Write-WatcherLog ("watch start falloutPid={0} hostPid={1} commandExe='{2}' saveQuitOnHostExit={3}" -f $FalloutPid, $HostPid, $CommandExe, [bool]$SaveQuitOnHostExit)

    $lastHeartbeat = Get-Date
    $lastStereoCapture = [DateTime]::MinValue
    $hostProgressPath = Join-Path $RunDir "fnvxr_openxr_pose_host.out.log"
    $lastHostProgress = Get-Date
    $lastHostProgressSignature = ""
    while ($true) {
        $fallout = Get-Process -Id $FalloutPid -ErrorAction SilentlyContinue
        $hostProcess = Get-Process -Id $HostPid -ErrorAction SilentlyContinue

        if ($hostProcess) {
            $progressItem = Get-Item -LiteralPath $hostProgressPath -ErrorAction SilentlyContinue
            if ($progressItem) {
                $progressSignature = "{0}:{1}" -f $progressItem.Length,$progressItem.LastWriteTimeUtc.Ticks
                if ($progressSignature -ne $lastHostProgressSignature) {
                    $lastHostProgressSignature = $progressSignature
                    $lastHostProgress = Get-Date
                }
            }
            if (((Get-Date) - $lastHostProgress).TotalSeconds -ge $HostProgressTimeoutSeconds) {
                Write-WatcherLog ("host progress watchdog expired hostPid={0} timeoutSeconds={1}; terminating hung host" -f $HostPid,$HostProgressTimeoutSeconds)
                Set-ManifestAcceptance -State "rejected" -Accepted $false -Reason "OpenXR host progress heartbeat timed out"
                Stop-Process -Id $HostPid -Force -ErrorAction SilentlyContinue
                $hostProcess = $null
                continue
            }
        }

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
            if (((Get-Date) - $lastStereoCapture).TotalSeconds -ge $StereoCaptureIntervalSeconds) {
                $lastStereoCapture = Get-Date
                Invoke-IndependentStereoCapture
            }
            Invoke-LiveAcceptanceAnalysis
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
            Invoke-LiveAcceptanceAnalysis -Final
            exit 0
        }
        Start-Sleep -Milliseconds 250
    } while ((Get-Date) -lt $deadline)

    $hostProcess = Get-Process -Id $HostPid -ErrorAction SilentlyContinue
    if ($hostProcess) {
        Write-WatcherLog ("stopping host after retail exit pid={0}" -f $HostPid)
        Stop-Process -Id $HostPid -Force -ErrorAction SilentlyContinue
    }
    Invoke-LiveAcceptanceAnalysis -Final
} catch {
    try {
        Write-WatcherLog ("ERROR " + $_.Exception.Message)
    } catch {
    }
    exit 1
}
