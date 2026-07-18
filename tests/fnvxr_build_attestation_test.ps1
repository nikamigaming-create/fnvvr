param(
    [Parameter(Mandatory = $true)][string]$SourceRoot
)

$ErrorActionPreference = "Stop"

. (Join-Path $SourceRoot "scripts\fnvxr-sidecar-common.ps1")

function Require-Throws {
    param(
        [Parameter(Mandatory = $true)][scriptblock]$Action,
        [Parameter(Mandatory = $true)][string]$MessageFragment
    )

    try {
        & $Action | Out-Null
    } catch {
        if (-not $_.Exception.Message.Contains($MessageFragment)) {
            throw "Unexpected failure '$($_.Exception.Message)'; expected '$MessageFragment'"
        }
        return
    }
    throw "Expected failure containing '$MessageFragment'"
}

$testRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("fnvxr-build-attestation-" + [Guid]::NewGuid().ToString("N"))
$resolvedTemp = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
$resolvedTestRoot = [System.IO.Path]::GetFullPath($testRoot)
if (-not $resolvedTestRoot.StartsWith(
        $resolvedTemp,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing unsafe attestation test root: $resolvedTestRoot"
}

try {
    New-Item -ItemType Directory -Path (Join-Path $testRoot "src") -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $testRoot "artifacts") -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $testRoot "CMakeLists.txt") -Value "cmake_minimum_required(VERSION 3.24)" -Encoding ASCII
    $sourceFile = Join-Path $testRoot "src\input.cpp"
    Set-Content -LiteralPath $sourceFile -Value "int value = 1;" -Encoding ASCII
    $artifactA = Join-Path $testRoot "artifacts\a.bin"
    $artifactB = Join-Path $testRoot "artifacts\b.bin"
    Set-Content -LiteralPath $artifactA -Value "artifact-a" -Encoding ASCII
    Set-Content -LiteralPath $artifactB -Value "artifact-b" -Encoding ASCII

    $descriptors = @(
        [ordered]@{ key = "a"; path = $artifactA; machine = "" },
        [ordered]@{ key = "b"; path = $artifactB; machine = "" }
    )
    $sourceDirectories = @("src")
    $testRecords = @(
        [ordered]@{
            key = "fixture/attestation-contract"
            length = 0
            sha256 = Get-FnvxrStringSha256 -Value "fixture/attestation-contract"
        })
    $testCatalogSnapshot = [ordered]@{
        count = $testRecords.Count
        sha256 = Get-FnvxrRecordDigest -Records $testRecords
        records = $testRecords
    }
    $sourceSnapshot = Get-FnvxrBuildSourceSnapshot `
        -Root $testRoot `
        -SourceDirectories $sourceDirectories
    $artifactSnapshot = Get-FnvxrArtifactContentSnapshot -Descriptors $descriptors
    $attestationPath = Join-Path $testRoot "attestation.json"
    $nonce = [Guid]::NewGuid().ToString("N")
    Write-FnvxrBuildAttestation `
        -Path $attestationPath `
        -Root $testRoot `
        -Configuration "Release" `
        -Nonce $nonce `
        -SourceSnapshot $sourceSnapshot `
        -ArtifactSnapshot $artifactSnapshot `
        -TestCatalogSnapshot $testCatalogSnapshot

    Assert-FnvxrBuildAttestation `
        -Path $attestationPath `
        -Root $testRoot `
        -Configuration "Release" `
        -ArtifactDescriptors $descriptors `
        -TestCatalogSnapshot $testCatalogSnapshot `
        -ExpectedNonce $nonce `
        -SourceDirectories $sourceDirectories | Out-Null

    Require-Throws -MessageFragment "nonce mismatch" -Action {
        Assert-FnvxrBuildAttestation `
            -Path $attestationPath `
            -Root $testRoot `
            -Configuration "Release" `
            -ArtifactDescriptors $descriptors `
            -TestCatalogSnapshot $testCatalogSnapshot `
            -ExpectedNonce "not-the-build-nonce" `
            -SourceDirectories $sourceDirectories
    }

    $differentTestRecords = @(
        [ordered]@{
            key = "fixture/different-test"
            length = 0
            sha256 = Get-FnvxrStringSha256 -Value "fixture/different-test"
        })
    $differentTestCatalog = [ordered]@{
        count = $differentTestRecords.Count
        sha256 = Get-FnvxrRecordDigest -Records $differentTestRecords
        records = $differentTestRecords
    }
    Require-Throws -MessageFragment "test catalog mismatch" -Action {
        Assert-FnvxrBuildAttestation `
            -Path $attestationPath `
            -Root $testRoot `
            -Configuration "Release" `
            -ArtifactDescriptors $descriptors `
            -TestCatalogSnapshot $differentTestCatalog `
            -SourceDirectories $sourceDirectories
    }

    $sourceTimestamp = (Get-Item -LiteralPath $sourceFile).LastWriteTimeUtc
    Set-Content -LiteralPath $sourceFile -Value "int value = 2;" -Encoding ASCII
    (Get-Item -LiteralPath $sourceFile).LastWriteTimeUtc = $sourceTimestamp
    Require-Throws -MessageFragment "source digest mismatch" -Action {
        Assert-FnvxrBuildAttestation `
            -Path $attestationPath `
            -Root $testRoot `
            -Configuration "Release" `
            -ArtifactDescriptors $descriptors `
            -TestCatalogSnapshot $testCatalogSnapshot `
            -SourceDirectories $sourceDirectories
    }

    Set-Content -LiteralPath $sourceFile -Value "int value = 1;" -Encoding ASCII
    $addedSource = Join-Path $testRoot "src\untracked-new-input.h"
    Set-Content -LiteralPath $addedSource -Value "#pragma once" -Encoding ASCII
    Require-Throws -MessageFragment "source digest mismatch" -Action {
        Assert-FnvxrBuildAttestation `
            -Path $attestationPath `
            -Root $testRoot `
            -Configuration "Release" `
            -ArtifactDescriptors $descriptors `
            -TestCatalogSnapshot $testCatalogSnapshot `
            -SourceDirectories $sourceDirectories
    }
    Remove-Item -LiteralPath $addedSource -Force

    $artifactTimestamp = (Get-Item -LiteralPath $artifactB).LastWriteTimeUtc
    Set-Content -LiteralPath $artifactB -Value "artifact-b-mutated" -Encoding ASCII
    (Get-Item -LiteralPath $artifactB).LastWriteTimeUtc = $artifactTimestamp
    Require-Throws -MessageFragment "artifact digest mismatch" -Action {
        Assert-FnvxrBuildAttestation `
            -Path $attestationPath `
            -Root $testRoot `
            -Configuration "Release" `
            -ArtifactDescriptors $descriptors `
            -TestCatalogSnapshot $testCatalogSnapshot `
            -SourceDirectories $sourceDirectories
    }

    Set-Content -LiteralPath $artifactB -Value "artifact-b" -Encoding ASCII
    Remove-Item -LiteralPath $artifactB -Force
    Require-Throws -MessageFragment "artifact is missing" -Action {
        Assert-FnvxrBuildAttestation `
            -Path $attestationPath `
            -Root $testRoot `
            -Configuration "Release" `
            -ArtifactDescriptors $descriptors `
            -TestCatalogSnapshot $testCatalogSnapshot `
            -SourceDirectories $sourceDirectories
    }
    Set-Content -LiteralPath $artifactB -Value "artifact-b" -Encoding ASCII

    Require-Throws -MessageFragment "context mismatch" -Action {
        Assert-FnvxrBuildAttestation `
            -Path $attestationPath `
            -Root $testRoot `
            -Configuration "Debug" `
            -ArtifactDescriptors $descriptors `
            -TestCatalogSnapshot $testCatalogSnapshot `
            -SourceDirectories $sourceDirectories
    }

    $otherRoot = Join-Path $testRoot "other-root"
    New-Item -ItemType Directory -Path (Join-Path $otherRoot "src") -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $otherRoot "CMakeLists.txt") -Value "cmake_minimum_required(VERSION 3.24)" -Encoding ASCII
    Set-Content -LiteralPath (Join-Path $otherRoot "src\input.cpp") -Value "int value = 1;" -Encoding ASCII
    Require-Throws -MessageFragment "context mismatch" -Action {
        Assert-FnvxrBuildAttestation `
            -Path $attestationPath `
            -Root $otherRoot `
            -Configuration "Release" `
            -ArtifactDescriptors $descriptors `
            -TestCatalogSnapshot $testCatalogSnapshot `
            -SourceDirectories $sourceDirectories
    }

    $validAttestationText = Get-Content -LiteralPath $attestationPath -Raw
    Set-Content -LiteralPath $attestationPath -Value "{" -Encoding ASCII
    Require-Throws -MessageFragment "is unreadable" -Action {
        Assert-FnvxrBuildAttestation `
            -Path $attestationPath `
            -Root $testRoot `
            -Configuration "Release" `
            -ArtifactDescriptors $descriptors `
            -TestCatalogSnapshot $testCatalogSnapshot `
            -SourceDirectories $sourceDirectories
    }
    Set-Content -LiteralPath $attestationPath -Value $validAttestationText -Encoding UTF8

    Remove-Item -LiteralPath $attestationPath -Force
    Require-Throws -MessageFragment "is missing" -Action {
        Assert-FnvxrBuildAttestation `
            -Path $attestationPath `
            -Root $testRoot `
            -Configuration "Release" `
            -ArtifactDescriptors $descriptors `
            -TestCatalogSnapshot $testCatalogSnapshot `
            -SourceDirectories $sourceDirectories
    }
} finally {
    $checkedTestRoot = [System.IO.Path]::GetFullPath($testRoot)
    if ($checkedTestRoot.StartsWith(
            $resolvedTemp,
            [System.StringComparison]::OrdinalIgnoreCase) -and
        (Test-Path -LiteralPath $checkedTestRoot)) {
        Remove-Item -LiteralPath $checkedTestRoot -Recurse -Force
    }
}
