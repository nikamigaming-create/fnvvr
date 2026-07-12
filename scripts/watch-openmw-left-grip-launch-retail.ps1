param(
    [Parameter(Mandatory = $true)]
    [string]$OpenMwLog,
    [Parameter(Mandatory = $true)]
    [string]$RunDir,
    [int]$TimeoutSeconds = 300
)

$ErrorActionPreference = "Stop"

$marker = "FNVXR retail surface: left grip requested retail sidecar"
$readyMarkers = @(
    "FNV/ESM4 diag: VRHandsOnly attached surfaces count=",
    "FNV/ESM4 diag: VR static hand finger deform attached"
)
$root = Split-Path -Parent $PSScriptRoot
$retailScript = Join-Path $PSScriptRoot "start-retail-surface-producer.ps1"
$watchLog = Join-Path $RunDir "retail-left-grip-watch.log"
$manifestPath = Join-Path $RunDir "retail-left-grip-watch.json"

function Write-WatchLog {
    param([string]$Message)
    Add-Content -LiteralPath $watchLog -Value ("{0:o} {1}" -f (Get-Date), $Message) -Encoding UTF8
}

$manifest = [ordered]@{
    startedAt = (Get-Date).ToString("o")
    openMwLog = $OpenMwLog
    marker = $marker
    readyMarkers = $readyMarkers
    timeoutSeconds = $TimeoutSeconds
    ready = $false
    stereoRuntime = "default"
    triggered = $false
    ignoredBeforeReady = 0
    alreadyRunning = $false
    retailExitCode = $null
    retailOutput = $null
}

try {
    Write-WatchLog "watcher started"
    $offset = 0L
    if (Test-Path -LiteralPath $OpenMwLog) {
        $offset = (Get-Item -LiteralPath $OpenMwLog).Length
    }

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        if (Test-Path -LiteralPath $OpenMwLog) {
            $item = Get-Item -LiteralPath $OpenMwLog
            $newText = ""
            if ($item.Length -gt $offset) {
                $stream = [System.IO.File]::Open(
                    $OpenMwLog,
                    [System.IO.FileMode]::Open,
                    [System.IO.FileAccess]::Read,
                    [System.IO.FileShare]::ReadWrite)
                try {
                    $stream.Seek($offset, [System.IO.SeekOrigin]::Begin) | Out-Null
                    $reader = New-Object System.IO.StreamReader($stream)
                    $newText = $reader.ReadToEnd()
                } finally {
                    $stream.Dispose()
                }
                $offset = $item.Length
            }

            if (-not $manifest.ready) {
                foreach ($readyMarker in $readyMarkers) {
                    if ($newText.Contains($readyMarker)) {
                        $manifest.ready = $true
                        Write-WatchLog ("ready marker observed: {0}" -f $readyMarker)
                        break
                    }
                }
            }

            if ($newText.Contains($marker)) {
                if (-not $manifest.ready) {
                    ++$manifest.ignoredBeforeReady
                    Write-WatchLog "left grip marker ignored before world/hands ready"
                    Start-Sleep -Milliseconds 200
                    continue
                }

                $manifest.triggered = $true
                Write-WatchLog "left grip marker observed"
                $manifest.alreadyRunning = @(Get-Process FalloutNV,nvse_loader -ErrorAction SilentlyContinue).Count -gt 0
                if (-not $manifest.alreadyRunning) {
                    $retailOutputPath = Join-Path $RunDir "retail-launch-on-left-grip.json"
                    $retailArgs = @("-ExecutionPolicy", "Bypass", "-File", $retailScript)
                    $retailOutput = & powershell @retailArgs 2>&1
                    $manifest.retailExitCode = $LASTEXITCODE
                    $retailOutput | Set-Content -LiteralPath $retailOutputPath -Encoding UTF8
                    $manifest.retailOutput = $retailOutputPath
                    Write-WatchLog ("retail launch complete exit={0}" -f $LASTEXITCODE)
                } else {
                    Write-WatchLog "retail already running; not launching another process"
                }
                break
            }
        }
        Start-Sleep -Milliseconds 200
    }
} catch {
    $manifest.error = $_.Exception.Message
    Write-WatchLog ("ERROR " + $_.Exception.Message)
} finally {
    $manifest.finishedAt = (Get-Date).ToString("o")
    $manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
}
