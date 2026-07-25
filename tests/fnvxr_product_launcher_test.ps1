param(
    [Parameter(Mandatory = $true)][string]$SourceRoot
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

. (Join-Path $SourceRoot "scripts\fnvxr-product-common.ps1")

function Require-Throws {
    param(
        [Parameter(Mandatory = $true)][scriptblock]$Action,
        [Parameter(Mandatory = $true)][string]$Fragment
    )
    try { & $Action | Out-Null }
    catch {
        if (-not $_.Exception.Message.Contains($Fragment)) {
            throw "Expected failure containing '$Fragment', got '$($_.Exception.Message)'."
        }
        return
    }
    throw "Expected failure containing '$Fragment'."
}

$launcherPath = Join-Path $SourceRoot "scripts\start-fnvxr-product.ps1"
$buildPath = Join-Path $SourceRoot "scripts\build-fnvxr-product.ps1"
$commonPath = Join-Path $SourceRoot "scripts\fnvxr-product-common.ps1"
$hostSourcePath = Join-Path $SourceRoot "host\fnvxr_openxr_pose_host.cpp"
$launcher = Get-Content -LiteralPath $launcherPath -Raw
$builder = Get-Content -LiteralPath $buildPath -Raw
$common = Get-Content -LiteralPath $commonPath -Raw
$hostSource = Get-Content -LiteralPath $hostSourcePath -Raw

if ($launcher.Contains('$Process.Modules')) {
    throw "Product launcher must not use the architecture-dependent Process.Modules census for Win32 Fallout."
}
foreach ($moduleCensusContract in @(
    'Get-FnvxrProductLoadedModuleCensus',
    'TH32CS_SNAPMODULE32',
    'CreateToolhelp32Snapshot',
    'Module32FirstW',
    'Module32NextW')) {
    if (-not ($launcher.Contains($moduleCensusContract) -or $common.Contains($moduleCensusContract))) {
        throw "Product launcher lost the native Win32 module census contract: $moduleCensusContract"
    }
}

if (-not $launcher.Contains('[ValidateRange(5, 900)][int]$MaximumRunSeconds = 40')) {
    throw "Product launcher must allow a bounded lunch/headset wait of up to 900 seconds."
}
if (-not $launcher.Contains('[ValidateRange(5, 900)][int]$RetailReadyTimeoutSeconds = 60')) {
    throw "Product launcher must allow the combined pose/runtime readiness wait to remain bounded at up to 900 seconds."
}
if (-not $hostSource.Contains('envInt("FNVXR_SESSION_READY_TIMEOUT_SECONDS", 45), 1, 900)')) {
    throw "OpenXR host must support the launcher's bounded 900-second off-face session-ready wait."
}

foreach ($forbidden in @(
    '$host =',
    "fnvxr-sidecar-common.ps1",
    "start-openxr-retail-sidecar.ps1",
    "FNVXR_D3D9_STEREO_REPLAY",
    "FNVXR_D3D9_STEREO_READBACK",
    'FNVXR_ENABLE_LEGACY_IMAGE_DIAGNOSTICS = "1"',
    'FNVXR_ALLOW_STEREO_WORLD_2D_FALLBACK = "1"')) {
    if ($launcher.Contains($forbidden) -or $builder.Contains($forbidden)) {
        throw "Product scripts contain retired harness text: $forbidden"
    }
}
foreach ($required in @(
    'FNVXR_RUN_PROFILE = "stereo-visual-trial-v5"',
    'FNVXR_ENABLE_LEGACY_IMAGE_DIAGNOSTICS = "0"',
    'FNVXR_ENABLE_ENGINE_CENTER_STEREO = "1"',
    'FNVXR_ALLOW_STEREO_WORLD_2D_FALLBACK = "0"',
    'FNVXR_SESSION_READY_TIMEOUT_SECONDS',
    'Get-ChildItem Env:',
    'Get-FnvxrProductSourceSnapshot',
    'ctest --test-dir $win32Build',
    'ctest --test-dir $x64Build',
    '--clean-first',
    '--parallel 4')) {
    if (-not ($launcher.Contains($required) -or $builder.Contains($required) -or
        (Get-Content -LiteralPath (Join-Path $SourceRoot "scripts\fnvxr-product-common.ps1") -Raw).Contains($required))) {
        throw "Product scripts are missing required contract text: $required"
    }
}
$sessionTimeoutBinding = '-SessionReadyTimeoutSeconds $RetailReadyTimeoutSeconds'
if ([regex]::Matches($launcher, [regex]::Escape($sessionTimeoutBinding)).Count -ne 2) {
    throw "Product launcher must bind the validated readiness timeout into both validation metadata and the live host environment."
}
foreach ($trialBoundary in @(
    'acceptanceScope = "stereo-visual-trial-only"',
    'fullProductAccepted = $false',
    'controllerMutationAuthorized = $false',
    'trackedWeaponAuthorized = $false',
    '$manifest.trialReady = $true',
    '$manifest.readiness.retailVrBridge = $true',
    '$manifest.readiness.stereoOutput = $true',
    'Get-FnvxrProductStereoOutputProof')) {
    if (-not $launcher.Contains($trialBoundary)) {
        throw "Product launcher lost its honest visual-trial boundary: $trialBoundary"
    }
}
if ($launcher.Contains('$manifest.accepted = $true')) {
    throw "Visual trial must not represent itself as full product acceptance."
}
if (-not $launcher.Contains(
    'retail VR bridge initialized: exact world hook, ordinary-D3D9 CPU-v7 stereo transport, and deferred Present bootstrap ready')) {
    throw "Visual trial must prove bridge initialization instead of accepting a merely loaded proxy."
}
if (-not $launcher.Contains(
    'No proven binocular engine-stereo frame reached OpenXR')) {
    throw "Visual trial must fail when no binocular engine-stereo output was observed."
}

$hostStart = $launcher.IndexOf('$hostProcess = Start-Process')
$hostReady = $launcher.IndexOf('$hostBridgeReady = Wait-FnvxrProductHostBridgeReady')
$stage = $launcher.IndexOf('$staged = @(Install-FnvxrProductArtifactSet')
$retailStart = $launcher.IndexOf('$nvse = Start-Process')
$runtimeReady = $launcher.IndexOf('-Description "advancing retail runtime plus OpenXR pose publication"')
if ($hostStart -lt 0 -or $hostReady -le $hostStart -or $stage -le $hostReady -or
    $retailStart -le $stage -or $runtimeReady -le $retailStart) {
    throw "Product launcher ordering is not host -> authoritative bridge -> stage -> NVSE -> advancing pose/runtime."
}
$stagedToRetail = $launcher.Substring($stage, $retailStart - $stage)
if (-not $stagedToRetail.Contains('OpenXR host exited after bridge readiness but before retail launch.')) {
    throw "Product launcher must recheck host liveness after staging and before launching retail."
}
foreach ($bridgeContract in @(
    'function Wait-FnvxrProductHostBridgeReady',
    'fnvxrHostBridgeReady xrSessionCreated=1 sharedMappingsReady=1',
    '$manifest.readiness.hostBridge = $true')) {
    if (-not $launcher.Contains($bridgeContract)) {
        throw "Product launcher is missing the pre-retail host bridge contract: $bridgeContract"
    }
}
$bridgeWaitStart = $launcher.IndexOf('function Wait-FnvxrProductHostBridgeReady')
$bridgeWaitEnd = $launcher.IndexOf("`ntry {", $bridgeWaitStart)
if ($bridgeWaitEnd -le $bridgeWaitStart) {
    throw "Product launcher bridge wait function has no bounded implementation body."
}
$bridgeWaitBody = $launcher.Substring($bridgeWaitStart, $bridgeWaitEnd - $bridgeWaitStart)
foreach ($livenessContract in @(
    '$Process.Refresh()',
    'if ($Process.HasExited) { return $false }',
    'return -not $Process.HasExited',
    '[DateTime]::UtcNow.AddSeconds($TimeoutSeconds)')) {
    if (-not $bridgeWaitBody.Contains($livenessContract)) {
        throw "Product launcher bridge wait does not fail closed on host lifetime/timeout: $livenessContract"
    }
}
if ($launcher.Contains('-Description "advancing OpenXR v8 pose publication"')) {
    throw "Product launcher must not require an advancing pose before Fallout is staged and launched."
}
$hostPoseAccepted = $launcher.IndexOf('$manifest.readiness.hostPose = $true')
if ($hostPoseAccepted -le $runtimeReady) {
    throw "Product launcher must record pose readiness only after the combined post-retail pose/runtime gate."
}

$finallyStart = $launcher.IndexOf('} finally {')
$finallyEnd = $launcher.IndexOf('if ($manifest.error) { throw $manifest.error }', $finallyStart)
if ($finallyStart -lt 0 -or $finallyEnd -le $finallyStart) {
    throw "Product launcher has no inspectable finally cleanup body."
}
$finallyBody = $launcher.Substring($finallyStart, $finallyEnd - $finallyStart)
$restoreCall = $finallyBody.IndexOf('Restore-FnvxrProductArtifactSet -Records $staged')
$lastOwnedStop = $finallyBody.LastIndexOf('Stop-FnvxrOwnedProcess')
if ($restoreCall -le $lastOwnedStop) {
    throw "Product launcher must restore the temporary stage only after every owned process is stopped."
}
foreach ($restorationContract in @(
    'if ($staged.Count -gt 0)',
    '$manifest.cleanup.stageRestorationRequired = $true',
    '$manifest.cleanup.stagedArtifactsRestored = $true')) {
    if (-not $finallyBody.Contains($restorationContract)) {
        throw "Product launcher lost unconditional temporary-stage restoration: $restorationContract"
    }
}
foreach ($forbiddenCleanupGate in @(
    '-not $runtimeReady',
    'if ($manifest.error -and $staged.Count')) {
    if ($finallyBody.Contains($forbiddenCleanupGate)) {
        throw "Product launcher may not retain temporary game files after readiness or successful supervision: $forbiddenCleanupGate"
    }
}

$root = (Resolve-Path -LiteralPath $SourceRoot).Path
$fixtureRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("fnvxr-product-launcher-" + [Guid]::NewGuid().ToString("N"))
$tempPrefix = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\') + '\'
$resolvedFixture = [System.IO.Path]::GetFullPath($fixtureRoot)
if (-not $resolvedFixture.StartsWith($tempPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Unsafe product launcher fixture path: $resolvedFixture"
}

try {
    $selfCensus = @(Get-FnvxrProductLoadedModuleCensus -ProcessId ([uint32]$PID))
    $selfPath = (Get-Process -Id $PID).Path
    if (-not @($selfCensus | Where-Object {
        [string]::Equals($_.path, $selfPath, [System.StringComparison]::OrdinalIgnoreCase)
    })) {
        throw "Native module census did not report its own exact executable path."
    }

    if ([IntPtr]::Size -eq 8) {
        $win32PowerShell = Join-Path $env:SystemRoot "SysWOW64\WindowsPowerShell\v1.0\powershell.exe"
        if (-not (Test-Path -LiteralPath $win32PowerShell -PathType Leaf)) {
            throw "Cross-bitness module-census regression requires Win32 PowerShell: $win32PowerShell"
        }
        $win32Fixture = $null
        try {
            $win32Fixture = Start-Process `
                -FilePath $win32PowerShell `
                -ArgumentList @("-NoLogo", "-NoProfile", "-NonInteractive", "-Command", "Start-Sleep -Seconds 30") `
                -WindowStyle Hidden `
                -PassThru
            $fixtureDeadline = [DateTime]::UtcNow.AddSeconds(5)
            do {
                $win32Fixture.Refresh()
                if ($win32Fixture.HasExited) { throw "Win32 module-census fixture exited before inspection." }
                $win32Census = @(Get-FnvxrProductLoadedModuleCensus -ProcessId ([uint32]$win32Fixture.Id))
                $win32Kernel = @($win32Census | Where-Object {
                    [System.IO.Path]::GetFileName([string]$_.path) -ieq "kernel32.dll" -and
                    [uint64]$_.baseAddress -lt 0x100000000L
                })
                if ($win32Kernel.Count -gt 0) { break }
                Start-Sleep -Milliseconds 50
            } while ([DateTime]::UtcNow -lt $fixtureDeadline)
            if ($win32Kernel.Count -eq 0) {
                throw "Native module census did not expose the Win32 loader list from an x64 supervisor."
            }
        } finally {
            if ($win32Fixture) {
                Stop-Process -Id $win32Fixture.Id -Force -ErrorAction SilentlyContinue
                $win32Fixture.WaitForExit(5000) | Out-Null
            }
        }
    }

    New-Item -ItemType Directory -Path $fixtureRoot -Force | Out-Null
    $peCandidates = @(
        (Join-Path $env:SystemRoot "SysWOW64\notepad.exe"),
        (Join-Path $env:SystemRoot "SysWOW64\WindowsPowerShell\v1.0\powershell.exe"),
        (Join-Path $root "build-win32\Release\fnvxr_shared_state_probe.exe"))
    $win32Pe = $null
    foreach ($candidate in $peCandidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            $candidateIdentity = Get-FnvxrProductFileIdentity -Path $candidate -RequirePe
            if ($candidateIdentity.peMachine -eq "0x014c") {
                $win32Pe = $candidateIdentity
                break
            }
        }
    }
    if (-not $win32Pe) { throw "Test fixture could not locate a Win32 PE image." }

    $destinationRoot = Join-Path $fixtureRoot "game"
    $pluginRoot = Join-Path $destinationRoot "Data\NVSE\Plugins"
    New-Item -ItemType Directory -Path $pluginRoot -Force | Out-Null
    $existingDestination = Join-Path $destinationRoot "d3d9.dll"
    [System.IO.File]::WriteAllText($existingDestination, "previous-user-file")
    $previousHash = (Get-FnvxrProductFileIdentity -Path $existingDestination).sha256
    $plan = @(
        [pscustomobject]@{ key = "x86/d3d9.dll"; source = $win32Pe.path; destination = $existingDestination; machine = "0x014c" },
        [pscustomobject]@{ key = "x86/nvse_fnvxr.dll"; source = $win32Pe.path; destination = Join-Path $pluginRoot "nvse_fnvxr.dll"; machine = "0x014c" },
        [pscustomobject]@{ key = "x86/dinput8.dll"; source = $win32Pe.path; destination = Join-Path $destinationRoot "dinput8.dll"; machine = "0x014c" },
        [pscustomobject]@{ key = "x86/xinput1_3.dll"; source = $win32Pe.path; destination = Join-Path $destinationRoot "xinput1_3.dll"; machine = "0x014c" })
    $records = @(Install-FnvxrProductArtifactSet `
        -Plan $plan `
        -BackupRoot (Join-Path $fixtureRoot "backup") `
        -RunId "contract-test")
    if ($records.Count -ne 4) { throw "Stage transaction did not install exactly four artifacts." }
    foreach ($record in $records) {
        if ($record.destination.sha256 -cne $win32Pe.sha256) {
            throw "Stage transaction did not preserve source bytes."
        }
    }
    foreach ($forbiddenAlias in @("xinput9_1_0.dll", "xinput1_4.dll", "xinput1_2.dll", "xinput1_1.dll")) {
        if (Test-Path -LiteralPath (Join-Path $destinationRoot $forbiddenAlias)) {
            throw "Narrow product staging created an unrequested XInput alias: $forbiddenAlias"
        }
    }
    Restore-FnvxrProductArtifactSet -Records $records
    if ((Get-FnvxrProductFileIdentity -Path $existingDestination).sha256 -cne $previousHash) {
        throw "Stage rollback did not restore the pre-existing destination."
    }
    foreach ($newPath in @(
        (Join-Path $pluginRoot "nvse_fnvxr.dll"),
        (Join-Path $destinationRoot "dinput8.dll"),
        (Join-Path $destinationRoot "xinput1_3.dll"))) {
        if (Test-Path -LiteralPath $newPath) { throw "Stage rollback left a newly-created artifact: $newPath" }
    }

    $badPlan = @([pscustomobject]@{
        key = "bad"
        source = $win32Pe.path
        destination = Join-Path $destinationRoot "bad.dll"
        machine = "0x8664"
    })
    Require-Throws -Fragment "wrong architecture" -Action {
        Install-FnvxrProductArtifactSet -Plan $badPlan -BackupRoot (Join-Path $fixtureRoot "bad-backup") -RunId "bad"
    }

    $stagePlan = Get-FnvxrProductStagePlan -Root $root -Configuration Release -GameRoot $destinationRoot
    $expectedKeys = @("x86/d3d9.dll", "x86/nvse_fnvxr.dll", "x86/dinput8.dll", "x86/xinput1_3.dll")
    if (@($stagePlan.key).Count -ne $expectedKeys.Count -or
        (Compare-Object -ReferenceObject $expectedKeys -DifferenceObject @($stagePlan.key))) {
        throw "Product stage plan is not the exact four-file Win32 set."
    }
} finally {
    if ((Test-Path -LiteralPath $resolvedFixture -PathType Container) -and
        $resolvedFixture.StartsWith($tempPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        Remove-Item -LiteralPath $resolvedFixture -Recurse -Force
    }
}
