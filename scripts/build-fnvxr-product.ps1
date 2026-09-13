param(
    [ValidateSet("Release")][string]$Configuration = "Release",
    [string]$OpenXrLoaderPath = "",
    # The product build compiles both architectures and numerous standalone
    # verifier targets. Two workers avoid intermittent compiler/linker
    # corruption on this machine while retaining bounded parallelism.
    [ValidateRange(1, 4)][int]$Parallelism = 2,
    # CMake still rebuilds changed sources/dependencies and every test runs.
    # Useful for a local edit/test loop; default builds remain clean builds.
    [switch]$Incremental,
    # The interactive render/input loop can validate its affected components
    # without repeating unrelated fixture and release checks.
    [switch]$Focused,
    [switch]$ReuseAttestation
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

. (Join-Path $PSScriptRoot "fnvxr-product-common.ps1")

$root = Get-FnvxrProductRoot
$x64Build = Join-Path $root "build-product-x64"
$win32Build = Join-Path $root "build-product-win32"
$attestationPath = Join-Path $root "local\product-build\fnvxr-product-$Configuration.json"
$testArguments = @()
$testFilter = ""
if ($Focused) {
    if (-not $Incremental) { throw "Focused validation requires -Incremental." }
    $testFilter = '^fnvxr_(mirror_writer|rig_c_api|runtime_config(_loader)?|presentation_coordinator_kernel|product_kernel_adapter|wrist_ui_kernel|wrist_content_readiness|pipboy_panel_contract|native_control_pulses|native_menu_geometry|world_continuity|haptics|input_proxy_(safety|inert_fuse)|physical_input_authority|product_launcher|source_pose_history|gpu_color_route|retail_(rig_lifetime|tracked_frame|world_accumulation_controller|center_runtime|center_renderer_operations|eye_camera_transaction|ui_quad_capture_source_fuse)|headset_mirror_capture)(_test)?$'
    $testArguments = @('-R', $testFilter)
}

if (@(Get-Process FalloutNV,nvse_loader,fnvxr_openxr_pose_host -ErrorAction SilentlyContinue).Count -ne 0) {
    throw "Product build refused while Fallout/NVSE/FNVXR host processes are running."
}

if ($ReuseAttestation) {
    $attestation = Assert-FnvxrProductBuildAttestation `
        -Path $attestationPath `
        -Root $root `
        -Configuration $Configuration
    [pscustomobject][ordered]@{
        reused = $true
        attestationPath = $attestationPath
        nonce = $attestation.nonce
        sourceSha256 = $attestation.source.sha256
        artifactSha256 = $attestation.artifacts.sha256
        testCatalogSha256 = $attestation.tests.sha256
    } | ConvertTo-Json -Depth 5
    return
}

if (Test-Path -LiteralPath $attestationPath -PathType Leaf) {
    Remove-Item -LiteralPath $attestationPath -Force
}
$sourceBefore = Get-FnvxrProductSourceSnapshot -Root $root
$nonce = [Guid]::NewGuid().ToString("N")

& cmake -S $root -B $win32Build -A Win32
if ($LASTEXITCODE -ne 0) { throw "Product Win32 configure failed with exit code $LASTEXITCODE." }
# Bound compiler fan-out: unbounded MSBuild parallelism can start dozens of
# multi-gigabyte CL processes and turn a clean build into an out-of-memory
# compiler exit. The default is deliberately conservative and can be raised
# only through the explicit bounded parameter.
if ($Incremental) {
    & cmake --build $win32Build --config $Configuration --parallel $Parallelism
} else {
    & cmake --build $win32Build --config $Configuration --clean-first --parallel $Parallelism
}
if ($LASTEXITCODE -ne 0) { throw "Product Win32 clean build failed with exit code $LASTEXITCODE." }
& ctest --test-dir $win32Build -C $Configuration --no-tests=error --output-on-failure @testArguments
if ($LASTEXITCODE -ne 0) { throw "Product Win32 CTest failed with exit code $LASTEXITCODE." }

& cmake -S $root -B $x64Build -A x64
if ($LASTEXITCODE -ne 0) { throw "Product x64 configure failed with exit code $LASTEXITCODE." }
if ($Incremental) {
    & cmake --build $x64Build --config $Configuration --parallel $Parallelism
} else {
    & cmake --build $x64Build --config $Configuration --clean-first --parallel $Parallelism
}
if ($LASTEXITCODE -ne 0) { throw "Product x64 clean build failed with exit code $LASTEXITCODE." }
& ctest --test-dir $x64Build -C $Configuration --no-tests=error --output-on-failure @testArguments
if ($LASTEXITCODE -ne 0) { throw "Product x64 CTest failed with exit code $LASTEXITCODE." }

$loaderSource = Resolve-FnvxrProductOpenXrLoader -ExplicitPath $OpenXrLoaderPath
$loaderDestination = Join-Path $x64Build "$Configuration\openxr_loader.dll"
$loaderSourceIdentity = Get-FnvxrProductFileIdentity -Path $loaderSource -RequirePe
Copy-Item -LiteralPath $loaderSourceIdentity.path -Destination $loaderDestination -Force
$loaderDestinationIdentity = Get-FnvxrProductFileIdentity -Path $loaderDestination -RequirePe
if ($loaderDestinationIdentity.peMachine -cne "0x8664" -or
    $loaderDestinationIdentity.sha256 -cne $loaderSourceIdentity.sha256) {
    throw "Staged OpenXR loader failed its x64/hash identity check."
}

$sourceAfter = Get-FnvxrProductSourceSnapshot -Root $root
if ($sourceBefore.count -ne $sourceAfter.count -or $sourceBefore.sha256 -cne $sourceAfter.sha256) {
    throw "Product build refused because source inputs changed during build/test."
}
$tests = Get-FnvxrProductCtestSnapshot `
    -X64BuildDirectory $x64Build `
    -Win32BuildDirectory $win32Build `
    -Configuration $Configuration
$artifacts = Get-FnvxrProductArtifactSnapshot -Descriptors (
    Get-FnvxrProductArtifactDescriptors -Root $root -Configuration $Configuration)
Write-FnvxrProductBuildAttestation `
    -Path $attestationPath `
    -Root $root `
    -Configuration $Configuration `
    -Nonce $nonce `
    -Source $sourceAfter `
    -Artifacts $artifacts `
    -Tests $tests `
    -ValidationFilter $testFilter
$attestation = Assert-FnvxrProductBuildAttestation `
    -Path $attestationPath `
    -Root $root `
    -Configuration $Configuration

[pscustomobject][ordered]@{
    reused = $false
    attestationPath = $attestationPath
    nonce = $attestation.nonce
    sourceSha256 = $attestation.source.sha256
    artifactSha256 = $attestation.artifacts.sha256
    testCatalogSha256 = $attestation.tests.sha256
    testCount = $attestation.tests.count
    validationScope = $attestation.tests.validationScope
    executedTestCount = $attestation.tests.executedCount
} | ConvertTo-Json -Depth 5
