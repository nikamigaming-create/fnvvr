param(
    [ValidateSet("Release")][string]$Configuration = "Release",
    [string]$OpenXrLoaderPath = "",
    [switch]$ReuseAttestation
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

. (Join-Path $PSScriptRoot "fnvxr-product-common.ps1")

$root = Get-FnvxrProductRoot
$x64Build = Join-Path $root "build-product-x64"
$win32Build = Join-Path $root "build-product-win32"
$attestationPath = Join-Path $root "local\product-build\fnvxr-product-$Configuration.json"

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
# compiler exit. Four keeps both architectures deterministic on this machine.
& cmake --build $win32Build --config $Configuration --clean-first --parallel 4
if ($LASTEXITCODE -ne 0) { throw "Product Win32 clean build failed with exit code $LASTEXITCODE." }
& ctest --test-dir $win32Build -C $Configuration --no-tests=error --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Product Win32 CTest failed with exit code $LASTEXITCODE." }

& cmake -S $root -B $x64Build -A x64
if ($LASTEXITCODE -ne 0) { throw "Product x64 configure failed with exit code $LASTEXITCODE." }
& cmake --build $x64Build --config $Configuration --clean-first --parallel 4
if ($LASTEXITCODE -ne 0) { throw "Product x64 clean build failed with exit code $LASTEXITCODE." }
& ctest --test-dir $x64Build -C $Configuration --no-tests=error --output-on-failure
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
    -Tests $tests
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
} | ConvertTo-Json -Depth 5
