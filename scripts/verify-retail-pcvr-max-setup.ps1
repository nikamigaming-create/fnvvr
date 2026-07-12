param(
    [string]$Configuration = "Release",
    [string]$GameRoot = $(if ($env:FNVXR_GAME_ROOT) { $env:FNVXR_GAME_ROOT } else { "C:\Program Files (x86)\Steam\steamapps\common\Fallout New Vegas" }),
    [int]$SharedStateSampleDelayMs = 50,
    [switch]$Stage
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $Root "build"
$BuildWin32Dir = Join-Path $Root "build-win32"
$ReportRoot = Join-Path $Root "local\pcvr-setup-checks"
$Stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$ReportDir = Join-Path $ReportRoot $Stamp
$ReportPath = Join-Path $ReportDir "manifest.json"

New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null

$script:Steps = @()

function Add-StepResult {
    param(
        [string]$Name,
        [bool]$Passed,
        [double]$ElapsedSeconds,
        [string]$Detail = ""
    )

    $script:Steps += [ordered]@{
        name = $Name
        passed = $Passed
        elapsedSeconds = [Math]::Round($ElapsedSeconds, 3)
        detail = $Detail
    }
}

function Write-SetupManifest {
    param([string]$Status)

    $manifest = [ordered]@{
        generatedAt = (Get-Date).ToString("o")
        status = $Status
        configuration = $Configuration
        gameRoot = $GameRoot
        stage = [bool]$Stage
        sharedStateSampleDelayMs = $SharedStateSampleDelayMs
        reportDir = $ReportDir
        steps = $script:Steps
    }

    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ReportPath -Encoding UTF8
}

function Invoke-SetupStep {
    param(
        [string]$Name,
        [scriptblock]$ScriptBlock
    )

    Write-Output "[SETUP] $Name"
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    try {
        & $ScriptBlock
        $timer.Stop()
        Add-StepResult -Name $Name -Passed $true -ElapsedSeconds $timer.Elapsed.TotalSeconds
    } catch {
        $timer.Stop()
        Add-StepResult -Name $Name -Passed $false -ElapsedSeconds $timer.Elapsed.TotalSeconds -Detail $_.Exception.Message
        Write-SetupManifest -Status "failed"
        throw
    }
}

function Invoke-NativeChecked {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath failed with exit code $LASTEXITCODE"
    }
}

function Get-LaunchSensitiveProcess {
    $names = @(
        "FalloutNV",
        "nvse_loader",
        "fnvxr_openxr_pose_host",
        "openmw",
        "openmw_vr",
        "vrserver",
        "vrcompositor",
        "SteamVR"
    )

    return @(Get-Process -Name $names -ErrorAction SilentlyContinue | Sort-Object ProcessName, Id)
}

Invoke-SetupStep "launch-sensitive processes are stopped" {
    $active = @(Get-LaunchSensitiveProcess)
    if ($active.Count -gt 0) {
        $summary = ($active | ForEach-Object { "$($_.ProcessName):$($_.Id)" }) -join ", "
        throw "Refusing no-runtime PCVR setup verification while these processes are active: $summary"
    }
}

Invoke-SetupStep "launch scripts parse and safety guards are present" {
    & (Join-Path $PSScriptRoot "audit-launch-safety.ps1") | Out-Host
}

Invoke-SetupStep "configure x64 proof build" {
    Invoke-NativeChecked -FilePath "cmake" -Arguments @("-S", $Root, "-B", $BuildDir)
}

Invoke-SetupStep "build x64 host and proof targets" {
    Invoke-NativeChecked -FilePath "cmake" -Arguments @(
        "--build", $BuildDir,
        "--config", $Configuration,
        "--target",
        "fnvxr_openxr_pose_host",
        "fnvxr_shared_state_probe",
        "fnvxr_protocol_layout_test",
        "fnvxr_stereo_math_test",
        "fnvxr_player_transform_test",
        "fnvxr_plugin_exports_test",
        "fnvxr_nvse_bridge_test"
    )
}

Invoke-SetupStep "test x64 proof targets" {
    Invoke-NativeChecked -FilePath "ctest" -Arguments @(
        "--test-dir", $BuildDir,
        "-C", $Configuration,
        "--output-on-failure"
    )
}

Invoke-SetupStep "build and test Win32 retail hooks" {
    & (Join-Path $PSScriptRoot "build-win32.ps1") -Configuration $Configuration
}

Invoke-SetupStep "validate or stage retail PCVR artifacts" {
    $args = @{
        Configuration = $Configuration
        GameRoot = $GameRoot
    }

    if ($Stage) {
        $args.StageOnly = $true
    } else {
        $args.ValidateOnly = $true
    }

    & (Join-Path $PSScriptRoot "start-retail-pcvr-max.ps1") @args | Out-Host
}

Invoke-SetupStep "shared-state proof fails closed without live producers" {
    $probeExe = Join-Path $BuildDir "$Configuration\fnvxr_shared_state_probe.exe"
    if (-not (Test-Path -LiteralPath $probeExe)) {
        throw "Missing shared-state probe: $probeExe"
    }

    $probeArgs = @(
        "--require-player",
        "--require-runtime",
        "--require-camera",
        "--require-video",
        "--require-stereo",
        "--require-world-stereo",
        "--require-pose",
        "--require-advancing",
        "--sample-delay-ms",
        ([string]$SharedStateSampleDelayMs)
    )
    & $probeExe @probeArgs | Set-Content -LiteralPath (Join-Path $ReportDir "shared-state-no-runtime.json") -Encoding UTF8
    if ($LASTEXITCODE -ne 2) {
        throw "Shared-state probe should fail closed with exit code 2 when no live producers are running; got $LASTEXITCODE"
    }
}

Write-SetupManifest -Status "passed"
Write-Output "Retail PCVR max setup verification passed."
Write-Output "Manifest: $ReportPath"
