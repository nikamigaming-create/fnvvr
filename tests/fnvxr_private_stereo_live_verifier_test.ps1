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
        # The exact hook publishes CPU pixels before its outer adapter emits
        # the successful engine-center completion record for that transaction.
        '2026-07-19 00:00:00.000 {"event":"fnvxrRetailEngineCenterCpuStereo","publicationCount":1,"transaction":300,"sourceFrame":20,"poseSequence":30,"runtimeStateSample":40,"renderedDisplayTime":1000,"width":1280,"height":720,"producerMode":4,"renderPairSequence":300,"publicationGeneration":"700","referenceSpaceGeneration":1,"producerEpoch":"2","rendererProducerEpoch":"3","producerProcessId":999,"separated":true,"worldCandidate":true,"uiActive":false,"pixelProof":true,"visualCoverage":true,"pixelHashSamples":1024,"leftNonBlackSamples":900,"rightNonBlackSamples":901,"differentSamples":700,"differentTiles":10,"leftHash":"0x12345678","rightHash":"0x87654321"}' + "`r`n" +
        '2026-07-19 00:00:00.010 {"event":"fnvxrRetailEngineCenterFrame","dispatch":300,"controllerFailure":0,"disposition":2,"transaction":300,"centerFailure":0,"rendererFailure":0,"rendererComplete":true,"visible":923,"visibleSetGeneration":300,"producerMode":4,"delivered":true,"stereoComplete":300}' + "`r`n" +
        '2026-07-19 00:00:01.000 {"event":"fnvxrRetailEngineCenterCpuStereo","publicationCount":2,"transaction":301,"sourceFrame":21,"poseSequence":31,"runtimeStateSample":41,"renderedDisplayTime":2000,"width":1280,"height":720,"producerMode":4,"renderPairSequence":301,"publicationGeneration":"701","referenceSpaceGeneration":1,"producerEpoch":"2","rendererProducerEpoch":"3","producerProcessId":999,"separated":true,"worldCandidate":true,"uiActive":false,"pixelProof":true,"visualCoverage":true,"pixelHashSamples":1024,"leftNonBlackSamples":900,"rightNonBlackSamples":901,"differentSamples":701,"differentTiles":10,"leftHash":"0x12345679","rightHash":"0x87654322"}' + "`r`n" +
        '2026-07-19 00:00:01.010 {"event":"fnvxrRetailEngineCenterFrame","dispatch":301,"controllerFailure":0,"disposition":2,"transaction":301,"centerFailure":0,"rendererFailure":0,"rendererComplete":true,"visible":924,"visibleSetGeneration":301,"producerMode":4,"delivered":true,"stereoComplete":301}' + "`r`n")
    [IO.File]::WriteAllText(
        (Join-Path $fixture "fnvxr_d3d9_proxy.log"),
        "")
    $retailLogPath = Join-Path $fixture "fnvxr_retail_vr.log"
    $hostLogPath = Join-Path $fixture "fnvxr_openxr_pose_host.out.log"
    $completeRetailLog = Get-Content -LiteralPath $retailLogPath -Raw

    $firstSubmit = '{"event":"fnvxrOpenXrSubmit","frame":100,"hostWallClockUnixMilliseconds":100000,"cpuEngineStereoActive":true,"cpuEngineProducerMode":4,"cpuEngineTransaction":300,"runtimeGameplay":true,"stereoFullscreen":true,"runtimeShouldRender":true,"projectionLayerSubmitted":true,"layerCount":1,"xrEndFrame":"XR_SUCCESS","sourcePoseAgeValid":true,"sourceStereoSequence":500,"sourceStereoPublicationGeneration":"700","sourceRenderPairSequence":300,"sourcePoseSequence":30,"sourceReferenceSpaceGeneration":1,"sourcePoseProducerEpoch":"2","sourceRendererProducerEpoch":"3","sourceProducerProcessId":999,"sourceRenderedDisplayTime":1000,"pixelSamples":1024,"nonBlackSamples":900,"meaningfulDifferentSamples":700,"leftActiveTiles":14,"rightActiveTiles":14,"differentTiles":10,"leftOutputProof":true,"leftOutputHash":"0x11111111","leftOutputNonBlackSamples":900,"leftOutputVariedSamples":800,"rightOutputProof":true,"rightOutputHash":"0x22222222","rightOutputNonBlackSamples":901,"rightOutputVariedSamples":801}'
    $secondSubmit = '{"event":"fnvxrOpenXrSubmit","frame":190,"hostWallClockUnixMilliseconds":101000,"cpuEngineStereoActive":true,"cpuEngineProducerMode":4,"cpuEngineTransaction":301,"runtimeGameplay":true,"stereoFullscreen":true,"runtimeShouldRender":true,"projectionLayerSubmitted":true,"layerCount":1,"xrEndFrame":"XR_SUCCESS","sourcePoseAgeValid":true,"sourceStereoSequence":501,"sourceStereoPublicationGeneration":"701","sourceRenderPairSequence":301,"sourcePoseSequence":31,"sourceReferenceSpaceGeneration":1,"sourcePoseProducerEpoch":"2","sourceRendererProducerEpoch":"3","sourceProducerProcessId":999,"sourceRenderedDisplayTime":2000,"pixelSamples":1024,"nonBlackSamples":900,"meaningfulDifferentSamples":701,"leftActiveTiles":14,"rightActiveTiles":14,"differentTiles":10,"leftOutputProof":true,"leftOutputHash":"0x11111112","leftOutputNonBlackSamples":900,"leftOutputVariedSamples":800,"rightOutputProof":true,"rightOutputHash":"0x22222223","rightOutputNonBlackSamples":901,"rightOutputVariedSamples":801}'
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
        [int]$verdict.counts.goodCenterSamples -ne 2 -or
        [int]$verdict.counts.sourcePixelTransactions -ne 2 -or
        [int]$verdict.counts.goodEngineCenterTransactions -ne 2 -or
        [int]$verdict.counts.outputPixelProofs -ne 2 -or
        [double]$verdict.hostSubmissionHz -ne 90.0) {
        throw "Synthetic verdict did not preserve the engine-center byte-chain/cadence proof."
    }

    # A plausible-looking second host submit must not be accepted when it
    # claims a transaction that the engine-center producer never published.
    $mismatchedSecond = $secondSubmit.Replace(
        '"cpuEngineTransaction":301',
        '"cpuEngineTransaction":999')
    [IO.File]::WriteAllText(
        (Join-Path $fixture "fnvxr_openxr_pose_host.out.log"),
        $firstSubmit + "`r`n" + $mismatchedSecond + "`r`n")
    $rejectedOutput = Join-Path $fixture "verifier.rejected.stdout.json"
    $rejectedProcess = Start-Process `
        -FilePath "powershell.exe" `
        -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-File", $verifier,
            "-RunDir", $fixture,
            "-MinimumGpuSubmits", "2") `
        -RedirectStandardOutput $rejectedOutput `
        -PassThru `
        -WindowStyle Hidden `
        -Wait
    if ($rejectedProcess.ExitCode -eq 0) {
        throw "Verifier accepted a host transaction not published by the engine-center renderer."
    }
    $rejectedVerdict = Get-Content -LiteralPath $verdictPath -Raw | ConvertFrom-Json
    if ($rejectedVerdict.passed -or
        [int]$rejectedVerdict.counts.goodEngineCenterTransactions -ne 1) {
        throw "Rejected transaction lineage did not produce the expected failed verdict."
    }

    # The shared 32-bit render-pair value can eventually wrap. The host must
    # additionally identify the exact 64-bit publication it copied, rather
    # than using a plausible old pair with the same low-word token.
    $wrongGenerationSecond = $secondSubmit.Replace(
        '"sourceStereoPublicationGeneration":"701"',
        '"sourceStereoPublicationGeneration":"700"')
    [IO.File]::WriteAllText(
        (Join-Path $fixture "fnvxr_openxr_pose_host.out.log"),
        $firstSubmit + "`r`n" + $wrongGenerationSecond + "`r`n")
    $wrongGenerationOutput = Join-Path $fixture "verifier.wrong-generation.stdout.json"
    $wrongGenerationProcess = Start-Process `
        -FilePath "powershell.exe" `
        -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-File", $verifier,
            "-RunDir", $fixture,
            "-MinimumGpuSubmits", "2") `
        -RedirectStandardOutput $wrongGenerationOutput `
        -PassThru `
        -WindowStyle Hidden `
        -Wait
    if ($wrongGenerationProcess.ExitCode -eq 0) {
        throw "Verifier accepted a host submit tied to the wrong 64-bit publication generation."
    }
    $wrongGenerationVerdict = Get-Content -LiteralPath $verdictPath -Raw | ConvertFrom-Json
    if ($wrongGenerationVerdict.passed -or
        [int]$wrongGenerationVerdict.counts.goodEngineCenterTransactions -ne 1) {
        throw "Wrong publication generation did not remove the host submission from proof."
    }

    function Invoke-FixtureVerifier([string]$OutputLeaf) {
        $runOutput = Join-Path $fixture $OutputLeaf
        return Start-Process `
            -FilePath "powershell.exe" `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy", "Bypass",
                "-File", $verifier,
                "-RunDir", $fixture,
                "-MinimumGpuSubmits", "2") `
            -RedirectStandardOutput $runOutput `
            -PassThru `
            -WindowStyle Hidden `
            -Wait
    }

    # A source-pixel line that appears after its claimed completion cannot be
    # the synchronous publication made inside that completed transaction.
    $retailLines = @($completeRetailLog -split "`r?`n" |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    $reversedRetailLines = @(
        $retailLines[1]
        $retailLines[0]
        $retailLines[3]
        $retailLines[2]
    )
    [IO.File]::WriteAllText(
        $retailLogPath,
        ($reversedRetailLines -join "`r`n") + "`r`n")
    [IO.File]::WriteAllText(
        $hostLogPath,
        $firstSubmit + "`r`n" + $secondSubmit + "`r`n")
    $reorderedProcess = Invoke-FixtureVerifier "verifier.reordered.stdout.json"
    if ($reorderedProcess.ExitCode -eq 0) {
        throw "Verifier accepted source pixels that appeared after their engine completion record."
    }
    $reorderedVerdict = Get-Content -LiteralPath $verdictPath -Raw | ConvertFrom-Json
    if ($reorderedVerdict.passed -or
        [int]$reorderedVerdict.counts.lineageRejectedEngineCenterPairs -ne 2) {
        throw "Reordered retail events did not fail the expected transaction-order guard."
    }

    # The full transaction and its 32-bit shared render-pair token must agree.
    # A plausible host log cannot repair a mismatched source-pixel token.
    $mismatchedPairRetailLog = $completeRetailLog.Replace(
        '"renderPairSequence":301',
        '"renderPairSequence":77')
    [IO.File]::WriteAllText($retailLogPath, $mismatchedPairRetailLog)
    [IO.File]::WriteAllText(
        $hostLogPath,
        $firstSubmit + "`r`n" + $secondSubmit + "`r`n")
    $mismatchedPairProcess = Invoke-FixtureVerifier "verifier.mismatched-pair.stdout.json"
    if ($mismatchedPairProcess.ExitCode -eq 0) {
        throw "Verifier accepted a source render-pair token unrelated to its engine transaction."
    }
    $mismatchedPairVerdict = Get-Content -LiteralPath $verdictPath -Raw | ConvertFrom-Json
    if ($mismatchedPairVerdict.passed -or
        [int]$mismatchedPairVerdict.counts.sourcePixelTransactions -ne 1) {
        throw "Mismatched render-pair token did not remove its source transaction from proof."
    }

    # Keep the 32-bit transaction token advancing while reversing only the
    # 64-bit publication order. The verifier must catch that independently;
    # an ABA replay can otherwise look fine after the low word wraps.
    $regressedGenerationRetailLog = $completeRetailLog.Replace(
        '"publicationGeneration":"700"',
        '"publicationGeneration":"__swap__"').Replace(
        '"publicationGeneration":"701"',
        '"publicationGeneration":"700"').Replace(
        '"publicationGeneration":"__swap__"',
        '"publicationGeneration":"701"')
    $regressedGenerationFirst = $firstSubmit.Replace(
        '"sourceStereoPublicationGeneration":"700"',
        '"sourceStereoPublicationGeneration":"701"')
    $regressedGenerationSecond = $secondSubmit.Replace(
        '"sourceStereoPublicationGeneration":"701"',
        '"sourceStereoPublicationGeneration":"700"')
    [IO.File]::WriteAllText($retailLogPath, $regressedGenerationRetailLog)
    [IO.File]::WriteAllText(
        $hostLogPath,
        $regressedGenerationFirst + "`r`n" + $regressedGenerationSecond + "`r`n")
    $regressedGenerationProcess = Invoke-FixtureVerifier "verifier.regressed-generation.stdout.json"
    if ($regressedGenerationProcess.ExitCode -eq 0) {
        throw "Verifier accepted a regressed 64-bit publication generation behind advancing 32-bit tokens."
    }
    $regressedGenerationVerdict = Get-Content -LiteralPath $verdictPath -Raw | ConvertFrom-Json
    if ($regressedGenerationVerdict.passed -or
        [int]$regressedGenerationVerdict.counts.goodEngineCenterTransactions -ne 2 -or
        [int]$regressedGenerationVerdict.counts.hostPairSequenceRegressions -ne 0 -or
        [int]$regressedGenerationVerdict.counts.hostPublicationGenerationRegressions -ne 1) {
        throw "Regressed 64-bit generation did not trip its independent host freshness guard."
    }

    # Repeating the latest source pair is normal when the headset runs faster
    # than the producer.  Returning to an older pair after a newer one is not.
    $replayedFirstSubmit = $firstSubmit.Replace(
        '"frame":100', '"frame":280').Replace(
        '"hostWallClockUnixMilliseconds":100000',
        '"hostWallClockUnixMilliseconds":102000')
    [IO.File]::WriteAllText($retailLogPath, $completeRetailLog)
    [IO.File]::WriteAllText(
        $hostLogPath,
        $firstSubmit + "`r`n" + $secondSubmit + "`r`n" +
        $replayedFirstSubmit + "`r`n")
    $replayedProcess = Invoke-FixtureVerifier "verifier.replayed-source.stdout.json"
    if ($replayedProcess.ExitCode -eq 0) {
        throw "Verifier accepted a host stream that returned to an older source pair."
    }
    $replayedVerdict = Get-Content -LiteralPath $verdictPath -Raw | ConvertFrom-Json
    if ($replayedVerdict.passed -or
        [int]$replayedVerdict.counts.hostPairSequenceRegressions -ne 1 -or
        [int]$replayedVerdict.counts.hostPublicationGenerationRegressions -ne 1) {
        throw "Replayed source pair did not trip the host-order guard."
    }

    # Signed LONG logging must preserve the actual 32-bit token across the
    # high bit and the producer's zero-to-one wrap normalization. The separate
    # nonzero 64-bit publication generation must also accept its own wrap.
    $wrappedRetailLog = $completeRetailLog
    $wrappedRetailLog = $wrappedRetailLog.Replace(
        '"transaction":300', '"transaction":4294967295')
    $wrappedRetailLog = $wrappedRetailLog.Replace(
        '"visibleSetGeneration":300', '"visibleSetGeneration":4294967295')
    $wrappedRetailLog = $wrappedRetailLog.Replace(
        '"stereoComplete":300', '"stereoComplete":4294967295')
    $wrappedRetailLog = $wrappedRetailLog.Replace(
        '"renderPairSequence":300', '"renderPairSequence":-1')
    $wrappedRetailLog = $wrappedRetailLog.Replace(
        '"poseSequence":30', '"poseSequence":-2147483648')
    $wrappedRetailLog = $wrappedRetailLog.Replace(
        '"transaction":301', '"transaction":4294967296')
    $wrappedRetailLog = $wrappedRetailLog.Replace(
        '"visibleSetGeneration":301', '"visibleSetGeneration":4294967296')
    $wrappedRetailLog = $wrappedRetailLog.Replace(
        '"stereoComplete":301', '"stereoComplete":4294967296')
    $wrappedRetailLog = $wrappedRetailLog.Replace(
        '"renderPairSequence":301', '"renderPairSequence":1')
    $wrappedRetailLog = $wrappedRetailLog.Replace(
        '"poseSequence":31', '"poseSequence":-2147483647')
    $wrappedRetailLog = $wrappedRetailLog.Replace(
        '"publicationGeneration":"700"',
        '"publicationGeneration":"18446744073709551615"')
    $wrappedRetailLog = $wrappedRetailLog.Replace(
        '"publicationGeneration":"701"',
        '"publicationGeneration":"1"')
    $wrappedFirstSubmit = $firstSubmit.Replace(
        '"cpuEngineTransaction":300', '"cpuEngineTransaction":4294967295').Replace(
        '"sourceStereoSequence":500', '"sourceStereoSequence":-1').Replace(
        '"sourceStereoPublicationGeneration":"700"',
        '"sourceStereoPublicationGeneration":"18446744073709551615"').Replace(
        '"sourceRenderPairSequence":300', '"sourceRenderPairSequence":-1').Replace(
        '"sourcePoseSequence":30', '"sourcePoseSequence":-2147483648')
    $wrappedSecondSubmit = $secondSubmit.Replace(
        '"cpuEngineTransaction":301', '"cpuEngineTransaction":1').Replace(
        '"sourceStereoSequence":501', '"sourceStereoSequence":1').Replace(
        '"sourceStereoPublicationGeneration":"701"',
        '"sourceStereoPublicationGeneration":"1"').Replace(
        '"sourceRenderPairSequence":301', '"sourceRenderPairSequence":1').Replace(
        '"sourcePoseSequence":31', '"sourcePoseSequence":-2147483647')
    [IO.File]::WriteAllText($retailLogPath, $wrappedRetailLog)
    [IO.File]::WriteAllText(
        $hostLogPath,
        $wrappedFirstSubmit + "`r`n" + $wrappedSecondSubmit + "`r`n")
    $wrappedProcess = Invoke-FixtureVerifier "verifier.wrapped-tokens.stdout.json"
    if ($wrappedProcess.ExitCode -ne 0) {
        throw "Verifier rejected a valid signed-LONG render-pair wrap: $(Get-Content -LiteralPath (Join-Path $fixture 'verifier.wrapped-tokens.stdout.json') -Raw)"
    }
    $wrappedVerdict = Get-Content -LiteralPath $verdictPath -Raw | ConvertFrom-Json
    if (-not $wrappedVerdict.passed -or
        [int]$wrappedVerdict.counts.sourcePixelTransactions -ne 2 -or
        [int]$wrappedVerdict.counts.goodEngineCenterTransactions -ne 2 -or
        [int]$wrappedVerdict.counts.hostPairSequenceRegressions -ne 0 -or
        [int]$wrappedVerdict.counts.hostPublicationGenerationRegressions -ne 0) {
        throw "Signed-LONG wrap did not retain the expected engine-center lineage."
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
