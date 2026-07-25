param(
    [string]$RunDir = "",
    [ValidateRange(1, 120)][int]$MinimumGpuSubmits = 2
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($RunDir)) {
    $runRoot = Join-Path $root "local\openxr-retail-sidecar-runs"
    $candidate = Get-ChildItem -LiteralPath $runRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object {
            (Test-Path -LiteralPath (Join-Path $_.FullName "fnvxr_retail_vr.log")) -or
            (Test-Path -LiteralPath (Join-Path $_.FullName "fnvxr_openxr_pose_host.out.log"))
        } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if (-not $candidate) {
        throw "No sidecar run with live telemetry exists under '$runRoot'."
    }
    $RunDir = $candidate.FullName
}
$RunDir = (Resolve-Path -LiteralPath $RunDir).Path

function Read-RunText([string]$Leaf) {
    $path = Join-Path $RunDir $Leaf
    if (Test-Path -LiteralPath $path -PathType Leaf) {
        return Get-Content -LiteralPath $path -Raw
    }
    return ""
}

function Read-JsonEvents([string]$Text, [string]$EventName) {
    $events = @()
    foreach ($line in ($Text -split "`r?`n")) {
        $start = $line.IndexOf('{"event":"' + $EventName + '"')
        if ($start -lt 0) { continue }
        try {
            $events += $line.Substring($start) | ConvertFrom-Json -ErrorAction Stop
        } catch {
        }
    }
    return @($events)
}

function Test-PositiveInteger($Value) {
    [uint64]$parsed = 0
    return [uint64]::TryParse([string]$Value, [ref]$parsed) -and $parsed -gt 0
}

function Test-NonzeroHash($Value) {
    $text = [string]$Value
    return $text -match '^0x[0-9a-fA-F]{1,8}$' -and
        $text -notmatch '^0x0+$'
}

function Get-EventValue($Event, [string]$Name, $DefaultValue) {
    if ($null -eq $Event) { return $DefaultValue }
    $property = $Event.PSObject.Properties[$Name]
    if ($null -eq $property) { return $DefaultValue }
    return $property.Value
}

$retailText = Read-RunText "fnvxr_retail_vr.log"
$d3d9Text = Read-RunText "fnvxr_d3d9_proxy.log"
$hostText = Read-RunText "fnvxr_openxr_pose_host.out.log"

$centerPattern = [regex]'retail center frame dispatch=(\d+) delivered=(\d+) controllerFailure=(\d+) disposition=(\d+) transaction=(\d+) centerFailure=(\d+) eyeCameraFailure=(\d+) rendererFailure=(\d+) visible=(\d+) stereoComplete=(\d+) drawPrimitive=(-?\d+) drawIndexed=(-?\d+) drawPrimitiveUP=(-?\d+) drawIndexedUP=(-?\d+)'
$centerSamples = @()
foreach ($match in $centerPattern.Matches($retailText)) {
    $draws = [int64]$match.Groups[11].Value +
        [int64]$match.Groups[12].Value +
        [int64]$match.Groups[13].Value +
        [int64]$match.Groups[14].Value
    $centerSamples += [pscustomobject]@{
        dispatch = [uint64]$match.Groups[1].Value
        delivered = [int]$match.Groups[2].Value
        controllerFailure = [int]$match.Groups[3].Value
        disposition = [int]$match.Groups[4].Value
        transaction = [uint64]$match.Groups[5].Value
        centerFailure = [int]$match.Groups[6].Value
        eyeCameraFailure = [int]$match.Groups[7].Value
        rendererFailure = [int]$match.Groups[8].Value
        visible = [uint32]$match.Groups[9].Value
        stereoComplete = [uint64]$match.Groups[10].Value
        drawCalls = $draws
    }
}
$goodCenterSamples = @($centerSamples | Where-Object {
    $_.delivered -eq 1 -and
    $_.controllerFailure -eq 0 -and
    $_.disposition -eq 2 -and
    $_.transaction -gt 0 -and
    $_.centerFailure -eq 0 -and
    $_.eyeCameraFailure -eq 0 -and
    $_.rendererFailure -eq 0 -and
    $_.visible -gt 0 -and
    $_.stereoComplete -gt 0 -and
    $_.drawCalls -gt 0
})

$eyeEvents = Read-JsonEvents $d3d9Text "fnvxrD3d9EyeTarget"
$sourcePixelProofs = @($eyeEvents | Where-Object {
    [string]$_.stage -eq "shared-stereo-publish" -and
    [int](Get-EventValue $_ "payloadHashSamples" 0) -ge 64 -and
    [int](Get-EventValue $_ "payloadLeftNonBlackSamples" 0) -ge 16 -and
    [int](Get-EventValue $_ "payloadRightNonBlackSamples" 0) -ge 16 -and
    [int](Get-EventValue $_ "payloadDifferentSamples" 0) -gt 0 -and
    (Test-NonzeroHash $_.payloadLeftHash) -and
    (Test-NonzeroHash $_.payloadRightHash) -and
    [string]$_.payloadLeftHash -ne [string]$_.payloadRightHash
})

$submitEvents = Read-JsonEvents $hostText "fnvxrOpenXrSubmit"
$gameplaySubmits = @($submitEvents | Where-Object { [bool]$_.runtimeGameplay })
$goodGpuSubmits = @($gameplaySubmits | Where-Object {
    (Test-PositiveInteger $_.gpuV5Transaction) -and
    (Test-PositiveInteger $_.gpuV5SourceFrame) -and
    (Test-PositiveInteger $_.gpuV5PoseSequence) -and
    [bool]$_.gpuV5RuntimeLineage -and
    [bool]$_.gpuV5ExactSourceView -and
    [bool]$_.stereoFullscreen -and
    [bool]$_.runtimeShouldRender -and
    [bool]$_.projectionLayerSubmitted -and
    [int]$_.layerCount -eq 1 -and
    [string]$_.xrEndFrame -eq "XR_SUCCESS"
} | Sort-Object { [uint64]$_.frame })

$outputPixelProofs = @($goodGpuSubmits | Where-Object {
    [bool](Get-EventValue $_ "leftOutputProofSampled" $false) -and
    [bool](Get-EventValue $_ "rightOutputProofSampled" $false) -and
    [bool]$_.leftOutputProof -and
    [bool]$_.rightOutputProof -and
    [int]$_.leftOutputNonBlackSamples -ge 16 -and
    [int]$_.rightOutputNonBlackSamples -ge 16 -and
    [int]$_.leftOutputVariedSamples -ge 16 -and
    [int]$_.rightOutputVariedSamples -ge 16 -and
    (Test-NonzeroHash $_.leftOutputHash) -and
    (Test-NonzeroHash $_.rightOutputHash) -and
    [string]$_.leftOutputHash -ne [string]$_.rightOutputHash
})

$firstGoodFrame = if ($goodGpuSubmits.Count -gt 0) {
    [uint64]$goodGpuSubmits[0].frame
} else {
    [uint64]0
}
$badGameplayAfterHandoff = @($gameplaySubmits | Where-Object {
    $firstGoodFrame -gt 0 -and
    [uint64]$_.frame -ge $firstGoodFrame -and
    -not ((Test-PositiveInteger $_.gpuV5Transaction) -and
        [bool]$_.stereoFullscreen -and
        [bool]$_.projectionLayerSubmitted -and
        [int]$_.layerCount -eq 1 -and
        [string]$_.xrEndFrame -eq "XR_SUCCESS")
})

$hostSubmissionHz = 0.0
if ($goodGpuSubmits.Count -ge 2) {
    $first = $goodGpuSubmits[0]
    $last = $goodGpuSubmits[-1]
    $milliseconds = [int64]$last.hostWallClockUnixMilliseconds -
        [int64]$first.hostWallClockUnixMilliseconds
    $frames = [uint64]$last.frame - [uint64]$first.frame
    if ($milliseconds -gt 0 -and $frames -gt 0) {
        $hostSubmissionHz = [double]$frames * 1000.0 / [double]$milliseconds
    }
}

$checks = @(
    [ordered]@{
        name = "retail_private_eye_draws"
        pass = $goodCenterSamples.Count -gt 0
        detail = "good=$($goodCenterSamples.Count) total=$($centerSamples.Count)"
    },
    [ordered]@{
        name = "retail_source_pixels"
        pass = $sourcePixelProofs.Count -gt 0
        detail = "nonblackSeparatedPayloads=$($sourcePixelProofs.Count)"
    },
    [ordered]@{
        name = "gpu_v5_projection_submit"
        pass = $goodGpuSubmits.Count -ge $MinimumGpuSubmits
        detail = "good=$($goodGpuSubmits.Count) required=$MinimumGpuSubmits"
    },
    [ordered]@{
        name = "post_composition_eye_pixels"
        pass = $outputPixelProofs.Count -gt 0
        detail = "sampledNonblackSeparatedOutputs=$($outputPixelProofs.Count)"
    },
    [ordered]@{
        name = "stable_after_handoff"
        pass = $goodGpuSubmits.Count -gt 0 -and $badGameplayAfterHandoff.Count -eq 0
        detail = "badGameplaySubmitsAfterFirstGood=$($badGameplayAfterHandoff.Count)"
    },
    [ordered]@{
        name = "openxr_90hz_class_cadence"
        pass = $hostSubmissionHz -ge 80.0
        detail = ("measuredHz={0:N2}" -f $hostSubmissionHz)
    }
)

$failed = @($checks | Where-Object { -not $_.pass })
$verdict = [ordered]@{
    generatedAt = [DateTime]::UtcNow.ToString("o")
    runDir = $RunDir
    passed = $failed.Count -eq 0
    failedCount = $failed.Count
    hostSubmissionHz = [Math]::Round($hostSubmissionHz, 3)
    counts = [ordered]@{
        centerSamples = $centerSamples.Count
        goodCenterSamples = $goodCenterSamples.Count
        sourcePixelProofs = $sourcePixelProofs.Count
        gameplaySubmits = $gameplaySubmits.Count
        goodGpuSubmits = $goodGpuSubmits.Count
        outputPixelProofs = $outputPixelProofs.Count
        badGameplayAfterHandoff = $badGameplayAfterHandoff.Count
    }
    checks = $checks
}
$verdictPath = Join-Path $RunDir "private-stereo-verdict.json"
$verdict | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $verdictPath -Encoding UTF8
$verdict | ConvertTo-Json -Depth 6
if ($failed.Count -gt 0) { exit 2 }
exit 0
