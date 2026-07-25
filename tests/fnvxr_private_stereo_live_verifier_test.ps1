param(
    [Parameter(Mandatory = $true)][string]$SourceRoot
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

$verifier = Join-Path $SourceRoot "scripts\verify-private-stereo-live.ps1"
if (-not (Test-Path -LiteralPath $verifier -PathType Leaf)) {
    throw "Missing private-stereo verifier: $verifier"
}

$fixture = Join-Path ([IO.Path]::GetTempPath()) (
    "fnvxr-private-stereo-verifier-" + [Guid]::NewGuid().ToString("N"))
$output = Join-Path $fixture "verifier.stdout.json"
try {
    New-Item -ItemType Directory -Path $fixture -Force | Out-Null
    [IO.File]::WriteAllText(
        (Join-Path $fixture "fnvxr_retail_vr.log"),
        "2026-07-19 00:00:00.000 retail center frame dispatch=300 delivered=1 controllerFailure=0 disposition=2 transaction=300 centerFailure=0 eyeCameraFailure=0 rendererFailure=0 visible=923 stereoComplete=300 drawPrimitive=5 drawIndexed=1800 drawPrimitiveUP=0 drawIndexedUP=0`r`n")
    [IO.File]::WriteAllText(
        (Join-Path $fixture "fnvxr_d3d9_proxy.log"),
        '2026-07-19 00:00:00.000 {"event":"fnvxrD3d9EyeTarget","stage":"shared-stereo-publish","payloadHashSamples":1024,"payloadLeftHash":"0x12345678","payloadRightHash":"0x87654321","payloadLeftNonBlackSamples":900,"payloadRightNonBlackSamples":901,"payloadDifferentSamples":700}' + "`r`n")

    $firstSubmit = '{"event":"fnvxrOpenXrSubmit","frame":100,"hostWallClockUnixMilliseconds":100000,"gpuV5Transaction":10,"gpuV5SourceFrame":20,"gpuV5PoseSequence":30,"gpuV5RuntimeLineage":true,"gpuV5ExactSourceView":true,"runtimeGameplay":true,"stereoFullscreen":true,"runtimeShouldRender":true,"projectionLayerSubmitted":true,"layerCount":1,"xrEndFrame":"XR_SUCCESS","leftOutputProofSampled":true,"leftOutputProof":true,"leftOutputHash":"0x11111111","leftOutputNonBlackSamples":900,"leftOutputVariedSamples":800,"rightOutputProofSampled":true,"rightOutputProof":true,"rightOutputHash":"0x22222222","rightOutputNonBlackSamples":901,"rightOutputVariedSamples":801}'
    $secondSubmit = '{"event":"fnvxrOpenXrSubmit","frame":190,"hostWallClockUnixMilliseconds":101000,"gpuV5Transaction":11,"gpuV5SourceFrame":21,"gpuV5PoseSequence":31,"gpuV5RuntimeLineage":true,"gpuV5ExactSourceView":true,"runtimeGameplay":true,"stereoFullscreen":true,"runtimeShouldRender":true,"projectionLayerSubmitted":true,"layerCount":1,"xrEndFrame":"XR_SUCCESS","leftOutputProofSampled":false,"leftOutputProof":false,"leftOutputHash":"0x0","leftOutputNonBlackSamples":0,"leftOutputVariedSamples":0,"rightOutputProofSampled":false,"rightOutputProof":false,"rightOutputHash":"0x0","rightOutputNonBlackSamples":0,"rightOutputVariedSamples":0}'
    [IO.File]::WriteAllText(
        (Join-Path $fixture "fnvxr_openxr_pose_host.out.log"),
        $firstSubmit + "`r`n" + $secondSubmit + "`r`n")

    $process = Start-Process `
        -FilePath "powershell.exe" `
        -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-File", $verifier,
            "-RunDir", $fixture,
            "-MinimumGpuSubmits", "2") `
        -RedirectStandardOutput $output `
        -PassThru `
        -WindowStyle Hidden `
        -Wait
    if ($process.ExitCode -ne 0) {
        throw "Synthetic complete pixel chain was rejected with exit $($process.ExitCode): $(Get-Content -LiteralPath $output -Raw)"
    }
    $verdictPath = Join-Path $fixture "private-stereo-verdict.json"
    $verdict = Get-Content -LiteralPath $verdictPath -Raw | ConvertFrom-Json
    if (-not $verdict.passed -or
        [int]$verdict.counts.goodCenterSamples -ne 1 -or
        [int]$verdict.counts.sourcePixelProofs -ne 1 -or
        [int]$verdict.counts.goodGpuSubmits -ne 2 -or
        [int]$verdict.counts.outputPixelProofs -ne 1 -or
        [double]$verdict.hostSubmissionHz -ne 90.0) {
        throw "Synthetic verdict did not preserve the complete byte-chain/cadence proof."
    }
} finally {
    $resolved = [IO.Path]::GetFullPath($fixture)
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if ((Test-Path -LiteralPath $resolved -PathType Container) -and
        $resolved.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}

Write-Output "Private stereo live verifier fixture PASS"
