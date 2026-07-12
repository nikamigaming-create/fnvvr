param(
    [string]$Configuration = "Debug",
    [string]$SaveName = "FNVXR_QuickSave",
    [int]$PluginAckTimeoutMs = 10000,
    [int]$SaveFileTimeoutSeconds = 20,
    [int]$GameExitTimeoutSeconds = 20,
    [switch]$NoBuild,
    [switch]$NoQuit,
    [switch]$SkipSaveFileProof
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $Root "build"
$CommandExe = Join-Path $BuildDir "$Configuration\fnvxr_command.exe"
$RunDir = if ($env:FNVXR_RUN_LOG_DIR) { $env:FNVXR_RUN_LOG_DIR } else { Join-Path $Root "local\command-runs" }
$TraceLog = Join-Path $RunDir "fnvxr_command_trace.log"

function Write-CommandTrace {
    param([string]$Message)
    if ($env:FNVXR_TELEMETRY_HAMMER -eq "0") {
        return
    }
    New-Item -ItemType Directory -Force -Path $RunDir | Out-Null
    Add-Content -LiteralPath $TraceLog -Value ("{0:o} {1}" -f (Get-Date), $Message) -Encoding UTF8
}

function Get-FnvxrSaveRoots {
    @(
        (Join-Path $env:USERPROFILE "Documents\My Games\FalloutNV\Saves"),
        (Join-Path $env:USERPROFILE "OneDrive\Documents\My Games\FalloutNV\Saves")
    ) | Where-Object { Test-Path -LiteralPath $_ }
}

function Get-SanitizedSaveName {
    param([string]$Name)
    $chars = New-Object System.Text.StringBuilder
    foreach ($ch in $Name.ToCharArray()) {
        if ([char]::IsLetterOrDigit($ch) -or $ch -eq '_' -or $ch -eq '-') {
            [void]$chars.Append($ch)
        } elseif ($ch -eq ' ' -or $ch -eq '.') {
            [void]$chars.Append('_')
        }
        if ($chars.Length -ge 63) {
            break
        }
    }
    if ($chars.Length -eq 0) {
        return "FNVXR_QuickSave"
    }
    return $chars.ToString()
}

function Wait-FnvxrSaveProof {
    param(
        [datetime]$Start,
        [string]$ExpectedName,
        [int]$TimeoutSeconds
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $expectedFile = "$ExpectedName.fos"
    do {
        foreach ($root in Get-FnvxrSaveRoots) {
            $exact = Join-Path $root $expectedFile
            if (Test-Path -LiteralPath $exact) {
                $item = Get-Item -LiteralPath $exact
                if ($item.LastWriteTime -ge $Start.AddSeconds(-1)) {
                    return $item
                }
            }

            $latest = Get-ChildItem -LiteralPath $root -Filter "*.fos" -File -ErrorAction SilentlyContinue |
                Sort-Object LastWriteTime -Descending |
                Select-Object -First 1
            if ($latest -and $latest.LastWriteTime -ge $Start.AddSeconds(-1)) {
                return $latest
            }
        }
        Start-Sleep -Milliseconds 250
    } while ((Get-Date) -lt $deadline)

    return $null
}

$fallout = Get-Process FalloutNV -ErrorAction SilentlyContinue | Sort-Object StartTime -Descending | Select-Object -First 1
if (-not $fallout) {
    Write-CommandTrace "abort reason=fallout-not-running"
    throw "FalloutNV is not running; cannot ask the in-game NVSE plugin to save."
}
Write-CommandTrace ("start falloutPid={0} saveName={1} noQuit={2} skipSaveProof={3} commandExe={4}" -f $fallout.Id, $SaveName, [bool]$NoQuit, [bool]$SkipSaveFileProof, $CommandExe)

if (-not $NoBuild) {
    cmake -S $Root -B $BuildDir
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed with exit code $LASTEXITCODE"
    }
    cmake --build $BuildDir --config $Configuration --target fnvxr_command
    if ($LASTEXITCODE -ne 0) {
        throw "fnvxr_command build failed with exit code $LASTEXITCODE"
    }
}

if (-not (Test-Path -LiteralPath $CommandExe)) {
    throw "Missing command helper: $CommandExe"
}

$sanitizedSaveName = Get-SanitizedSaveName -Name $SaveName
$saveStarted = Get-Date
Write-CommandTrace ("request save saveName={0} timeoutMs={1}" -f $sanitizedSaveName, $PluginAckTimeoutMs)
& $CommandExe save $sanitizedSaveName --wait-ms $PluginAckTimeoutMs
if ($LASTEXITCODE -ne 0) {
    Write-CommandTrace ("save failed exitCode={0}" -f $LASTEXITCODE)
    throw "Plugin save request failed with exit code $LASTEXITCODE"
}
Write-CommandTrace "save acknowledged"

$saveProof = $null
if (-not $SkipSaveFileProof) {
    Write-CommandTrace ("wait save proof timeoutSeconds={0}" -f $SaveFileTimeoutSeconds)
    $saveProof = Wait-FnvxrSaveProof -Start $saveStarted -ExpectedName $sanitizedSaveName -TimeoutSeconds $SaveFileTimeoutSeconds
    if (-not $saveProof) {
        Write-CommandTrace "save proof missing"
        throw "Plugin acknowledged save, but no fresh .fos save file appeared within $SaveFileTimeoutSeconds seconds."
    }
    Write-CommandTrace ("save proof path={0} time={1:o}" -f $saveProof.FullName, $saveProof.LastWriteTime)
}

if (-not $NoQuit) {
    Write-CommandTrace ("request quit timeoutMs={0}" -f $PluginAckTimeoutMs)
    & $CommandExe quit --wait-ms $PluginAckTimeoutMs
    if ($LASTEXITCODE -ne 0) {
        Write-CommandTrace ("quit failed exitCode={0}" -f $LASTEXITCODE)
        throw "Plugin quit request failed with exit code $LASTEXITCODE"
    }
    Write-CommandTrace "quit acknowledged"

    try {
        Write-CommandTrace ("wait fallout exit pid={0} timeoutSeconds={1}" -f $fallout.Id, $GameExitTimeoutSeconds)
        Wait-Process -Id $fallout.Id -Timeout $GameExitTimeoutSeconds -ErrorAction SilentlyContinue
    } catch {
        Write-CommandTrace ("wait fallout exit exception={0}" -f $_.Exception.Message)
    }
    if (Get-Process -Id $fallout.Id -ErrorAction SilentlyContinue) {
        Write-CommandTrace "fallout still running after quit ack"
        throw "Plugin quit request was acknowledged, but FalloutNV did not exit within $GameExitTimeoutSeconds seconds."
    }
    Write-CommandTrace "fallout exited after quit ack"
}

$hostProcesses = @(Get-Process fnvxr_openxr_pose_host -ErrorAction SilentlyContinue)
if (-not $NoQuit) {
    foreach ($hostProcess in $hostProcesses) {
        try {
            Write-CommandTrace ("stop host pid={0}" -f $hostProcess.Id)
            Stop-Process -Id $hostProcess.Id -Force -ErrorAction SilentlyContinue
        } catch {
            Write-CommandTrace ("stop host exception pid={0} error={1}" -f $hostProcess.Id, $_.Exception.Message)
        }
    }
}

[ordered]@{
    saveRequested = $true
    saveName = $sanitizedSaveName
    saveProofPath = if ($saveProof) { $saveProof.FullName } else { $null }
    quitRequested = -not [bool]$NoQuit
    falloutPid = $fallout.Id
    hostProcessesStopped = if ($NoQuit) { 0 } else { $hostProcesses.Count }
} | ConvertTo-Json -Depth 4
Write-CommandTrace "done"
