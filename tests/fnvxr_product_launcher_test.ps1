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
$launcher = Get-Content -LiteralPath $launcherPath -Raw
$builder = Get-Content -LiteralPath $buildPath -Raw

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
    'FNVXR_ALLOW_STEREO_WORLD_2D_FALLBACK = "0"',
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
foreach ($trialBoundary in @(
    'acceptanceScope = "stereo-visual-trial-only"',
    'fullProductAccepted = $false',
    'controllerMutationAuthorized = $false',
    'trackedWeaponAuthorized = $false',
    '$manifest.trialReady = $true')) {
    if (-not $launcher.Contains($trialBoundary)) {
        throw "Product launcher lost its honest visual-trial boundary: $trialBoundary"
    }
}
if ($launcher.Contains('$manifest.accepted = $true')) {
    throw "Visual trial must not represent itself as full product acceptance."
}

$hostStart = $launcher.IndexOf('$hostProcess = Start-Process')
$hostReady = $launcher.IndexOf('-Description "advancing OpenXR v8 pose publication"')
$stage = $launcher.IndexOf('$staged = @(Install-FnvxrProductArtifactSet')
$retailStart = $launcher.IndexOf('$nvse = Start-Process')
$runtimeReady = $launcher.IndexOf('-Description "advancing retail runtime plus OpenXR pose publication"')
if ($hostStart -lt 0 -or $hostReady -le $hostStart -or $stage -le $hostReady -or
    $retailStart -le $stage -or $runtimeReady -le $retailStart) {
    throw "Product launcher ordering is not host -> advancing pose -> stage -> NVSE -> advancing runtime."
}

$root = (Resolve-Path -LiteralPath $SourceRoot).Path
$fixtureRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("fnvxr-product-launcher-" + [Guid]::NewGuid().ToString("N"))
$tempPrefix = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\') + '\'
$resolvedFixture = [System.IO.Path]::GetFullPath($fixtureRoot)
if (-not $resolvedFixture.StartsWith($tempPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Unsafe product launcher fixture path: $resolvedFixture"
}

try {
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
