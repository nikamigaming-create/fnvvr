[CmdletBinding()]
param(
    [string]$SourceRoot = "",
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$BuildDirectory = "",
    [switch]$ConfigureOnly
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$productRoot = Split-Path -Parent $PSScriptRoot
$patchPath = Join-Path $productRoot "patches\openxr-simulator-fnvxr-headless.patch"
$controllerPosePatchPath = Join-Path $productRoot "patches\openxr-simulator-controller-local-6dof.patch"
$expectedUpstreamCommit = "48a70f440ac7d9bda385994937e3da8e15a4d9bb"
if ([string]::IsNullOrWhiteSpace($SourceRoot)) {
    $SourceRoot = Join-Path $productRoot "local\OpenXR-Simulator"
}
$SourceRoot = (Resolve-Path -LiteralPath $SourceRoot).Path
if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $SourceRoot "build-headless-fnvxr"
}

$requiredSource = Join-Path $SourceRoot "src\runtime.cpp"
$requiredMcpHeader = Join-Path $SourceRoot "src\mcp_integration.h"
foreach ($requiredPath in @($requiredSource, $requiredMcpHeader)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "OpenXR-Simulator source is incomplete: $requiredPath"
    }
}
if (-not (Test-Path -LiteralPath $patchPath -PathType Leaf)) {
    throw "The reproducible FNVXR runtime patch is missing: $patchPath"
}
if (-not (Test-Path -LiteralPath $controllerPosePatchPath -PathType Leaf)) {
    throw "The reproducible LOCAL controller-pose patch is missing: $controllerPosePatchPath"
}

$runtimeSource = Get-Content -LiteralPath $requiredSource -Raw
$mcpHeader = Get-Content -LiteralPath $requiredMcpHeader -Raw
$requiredRuntimeContracts = @(
    'OPENXR_SIMULATOR_HEADLESS',
    'OPENXR_SIMULATOR_LOG_PATH',
    'XR_SPACE_LOCATION_POSITION_TRACKED_BIT',
    'XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT',
    'button_a',
    'button_x')
$runtimePatchPresent = $true
foreach ($requiredContract in $requiredRuntimeContracts) {
    if (-not $runtimeSource.Contains($requiredContract)) {
        $runtimePatchPresent = $false
    }
}
$runtimePatchPresent = $runtimePatchPresent -and
    $mcpHeader.Contains('OPENXR_SIMULATOR_DATA_DIR')

$gitPath = (Get-Command git.exe -ErrorAction Stop).Source
$safeDirectory = $SourceRoot.Replace("\", "/")
$sourceCommit = (& $gitPath `
    -c "safe.directory=$safeDirectory" `
    -C $SourceRoot `
    rev-parse HEAD 2>&1 | Out-String).Trim()
if ($LASTEXITCODE -ne 0) {
    throw "Cannot identify the OpenXR-Simulator source commit: $sourceCommit"
}
if ($sourceCommit -cne $expectedUpstreamCommit) {
    throw (
        "OpenXR-Simulator source commit mismatch. Expected {0}; found {1}." -f
        $expectedUpstreamCommit,
        $sourceCommit)
}

if (-not $runtimePatchPresent) {
    $sourceStatus = (& $gitPath `
        -c "safe.directory=$safeDirectory" `
        -C $SourceRoot `
        status --porcelain 2>&1 | Out-String).Trim()
    if ($LASTEXITCODE -ne 0) {
        throw "Cannot inspect the OpenXR-Simulator worktree: $sourceStatus"
    }
    if (-not [string]::IsNullOrWhiteSpace($sourceStatus)) {
        throw (
            "OpenXR-Simulator has local changes but is missing the complete FNVXR patch; refusing to overwrite them: {0}" -f
            $sourceStatus)
    }
    & $gitPath `
        -c "safe.directory=$safeDirectory" `
        -C $SourceRoot `
        apply --check $patchPath
    if ($LASTEXITCODE -ne 0) {
        throw "The FNVXR headless patch does not apply cleanly to $sourceCommit."
    }
    & $gitPath `
        -c "safe.directory=$safeDirectory" `
        -C $SourceRoot `
        apply $patchPath
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to apply the FNVXR headless runtime patch."
    }
    $runtimeSource = Get-Content -LiteralPath $requiredSource -Raw
    $mcpHeader = Get-Content -LiteralPath $requiredMcpHeader -Raw
}

foreach ($requiredContract in $requiredRuntimeContracts) {
    if (-not $runtimeSource.Contains($requiredContract)) {
        throw "The FNVXR headless runtime contract is missing: $requiredContract"
    }
}
if (-not $mcpHeader.Contains('OPENXR_SIMULATOR_DATA_DIR')) {
    throw "The FNVXR per-run simulator data-directory contract is missing."
}

$controllerPosePatchPresent =
    $runtimeSource.Contains('poseInLocalSpace') -and
    $runtimeSource.Contains('QuatFromYawPitchRoll(float yaw, float pitch, float roll)') -and
    $mcpHeader.Contains('localSpaceSet') -and
    $mcpHeader.Contains('cmd.rollSet')

# The controller patch is intentionally layered after the reviewed headless
# patch. Its nearby runtime edits make the base patch's reverse check
# inapplicable after this second layer has been installed.
if (-not $controllerPosePatchPresent) {
    & $gitPath `
        -c "safe.directory=$safeDirectory" `
        -C $SourceRoot `
        apply --check --reverse $patchPath
    if ($LASTEXITCODE -ne 0) {
        throw "The OpenXR-Simulator source does not exactly contain the reviewed FNVXR headless patch."
    }

    & $gitPath -c "safe.directory=$safeDirectory" -C $SourceRoot apply --check $controllerPosePatchPath
    if ($LASTEXITCODE -ne 0) {
        throw "The LOCAL controller-pose patch does not apply cleanly to $sourceCommit."
    }
    & $gitPath -c "safe.directory=$safeDirectory" -C $SourceRoot apply $controllerPosePatchPath
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to apply the LOCAL controller-pose patch."
    }
    $runtimeSource = Get-Content -LiteralPath $requiredSource -Raw
    $mcpHeader = Get-Content -LiteralPath $requiredMcpHeader -Raw
}

foreach ($requiredControllerContract in @(
        'poseInLocalSpace',
        'QuatFromYawPitchRoll(float yaw, float pitch, float roll)',
        'localSpaceSet',
        'cmd.rollSet')) {
    if (-not $runtimeSource.Contains($requiredControllerContract) -and
        -not $mcpHeader.Contains($requiredControllerContract)) {
        throw "The LOCAL controller-pose contract is missing: $requiredControllerContract"
    }
}

& $gitPath -c "safe.directory=$safeDirectory" -C $SourceRoot apply --check --reverse $controllerPosePatchPath
if ($LASTEXITCODE -ne 0) {
    throw "The OpenXR-Simulator source does not exactly contain the reviewed LOCAL controller-pose patch."
}

function ConvertTo-NativeArgument {
    param([Parameter(Mandatory = $true)][string]$Value)

    if ($Value.Length -gt 0 -and $Value -notmatch '[\s"]') {
        return $Value
    }
    # These build arguments never intentionally contain a quote. Reject one
    # instead of relying on ambiguous native command-line escaping.
    if ($Value.Contains('"')) {
        throw "Unsupported quote in native process argument: $Value"
    }
    return '"' + $Value + '"'
}

function Invoke-NormalizedBuildProcess {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory
    )

    # ProcessStartInfo constructs its environment dictionary lazily. If the
    # parent's native block contains both Path and PATH, that getter can itself
    # fail before we have a chance to normalize the child. Canonicalize the
    # current PowerShell process first; the value is preserved byte-for-byte.
    $processPath = [Environment]::GetEnvironmentVariable(
        "Path",
        [EnvironmentVariableTarget]::Process)
    [Environment]::SetEnvironmentVariable(
        "PATH",
        $null,
        [EnvironmentVariableTarget]::Process)
    [Environment]::SetEnvironmentVariable(
        "Path",
        $processPath,
        [EnvironmentVariableTarget]::Process)

    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $FilePath
    $startInfo.Arguments = (($Arguments | ForEach-Object {
        ConvertTo-NativeArgument -Value ([string]$_)
    }) -join " ")
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    # Inherit the caller's redirected console handles. This keeps output
    # stream-safe without asynchronous reader races or pipe-buffer deadlocks.
    $startInfo.RedirectStandardOutput = $false
    $startInfo.RedirectStandardError = $false
    $childEnvironment = $startInfo.EnvironmentVariables
    if ($null -eq $childEnvironment) {
        throw "The command-line process environment block is unavailable."
    }

    # Some launch environments contain both Path and PATH. MSBuild treats
    # those as duplicate keys and aborts before compiling. Rebuild the child
    # block through a case-insensitive dictionary and emit one canonical Path.
    $normalized = New-Object 'System.Collections.Generic.Dictionary[string,string]' (
        [System.StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in [Environment]::GetEnvironmentVariables().GetEnumerator()) {
        $name = [string]$entry.Key
        if ($name -ieq "Path") {
            continue
        }
        $normalized[$name] = [string]$entry.Value
    }
    $normalized["Path"] = [Environment]::GetEnvironmentVariable(
        "Path",
        [EnvironmentVariableTarget]::Process)
    $normalized["MSBUILDDISABLENODEREUSE"] = "1"
    $normalized["UseSharedCompilation"] = "false"

    $childEnvironment.Clear()
    foreach ($entry in $normalized.GetEnumerator()) {
        $childEnvironment[$entry.Key] = $entry.Value
    }

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    if (-not $process.Start()) {
        throw "Failed to start command-line build process: $FilePath"
    }
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) {
        throw (
            "Command-line build failed with exit code {0}: {1} {2}" -f
            $process.ExitCode,
            $FilePath,
            $startInfo.Arguments)
    }
}

$cmakePath = (Get-Command cmake.exe -ErrorAction Stop).Source
New-Item -ItemType Directory -Path $BuildDirectory -Force | Out-Null
Invoke-NormalizedBuildProcess `
    -FilePath $cmakePath `
    -Arguments @(
        "-S", $SourceRoot,
        "-B", $BuildDirectory,
        "-G", "Visual Studio 17 2022",
        "-A", "x64") `
    -WorkingDirectory $SourceRoot

if (-not $ConfigureOnly) {
    Invoke-NormalizedBuildProcess `
        -FilePath $cmakePath `
        -Arguments @(
            "--build", $BuildDirectory,
            "--config", $Configuration,
            "--target", "openxr_simulator",
            "--parallel") `
        -WorkingDirectory $SourceRoot
}

$manifestPath = Join-Path $SourceRoot "bin\openxr_simulator.json"
$runtimePath = Join-Path $SourceRoot "bin\openxr_simulator.dll"
if (-not $ConfigureOnly) {
    foreach ($outputPath in @($manifestPath, $runtimePath)) {
        if (-not (Test-Path -LiteralPath $outputPath -PathType Leaf)) {
            throw "The headless runtime build did not produce: $outputPath"
        }
    }
}

[pscustomobject][ordered]@{
    schema = "fnvxr-headless-openxr-simulator-build-v1"
    sourceRoot = $SourceRoot
    buildDirectory = $BuildDirectory
    configuration = $Configuration
    configureOnly = [bool]$ConfigureOnly
    upstreamCommit = $sourceCommit
    patch = [ordered]@{
        path = (Resolve-Path -LiteralPath $patchPath).Path
        sha256 = (Get-FileHash -LiteralPath $patchPath -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    controllerPosePatch = [ordered]@{
        path = (Resolve-Path -LiteralPath $controllerPosePatchPath).Path
        sha256 = (Get-FileHash -LiteralPath $controllerPosePatchPath -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    runtimeManifest = if (Test-Path -LiteralPath $manifestPath -PathType Leaf) {
        (Resolve-Path -LiteralPath $manifestPath).Path
    } else {
        $null
    }
    runtimeDll = if (Test-Path -LiteralPath $runtimePath -PathType Leaf) {
        [ordered]@{
            path = (Resolve-Path -LiteralPath $runtimePath).Path
            sha256 = (Get-FileHash -LiteralPath $runtimePath -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    } else {
        $null
    }
} | ConvertTo-Json -Depth 5
