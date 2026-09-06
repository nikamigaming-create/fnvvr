param(
    [string]$ProducerPath = "$PSScriptRoot\..\build-audit-win32\Release\fnvxr_gpu_channel_probe.exe",
    [string]$ConsumerPath = "$PSScriptRoot\..\build-audit-x64\Release\fnvxr_gpu_channel_probe.exe",
    [string]$OutputDirectory = "$PSScriptRoot\..\local\gpu-channel-probes"
)
$ErrorActionPreference = 'Stop'
$producerExe = (Resolve-Path -LiteralPath $ProducerPath).Path
$consumerExe = (Resolve-Path -LiteralPath $ConsumerPath).Path
$probeId = [Guid]::NewGuid().ToString('N')
$runDirectory = Join-Path $OutputDirectory $probeId
New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
$runDirectory = (Resolve-Path -LiteralPath $runDirectory).Path
$producer = $null
$consumer = $null
try {
    $consumer = Start-Process -FilePath $consumerExe -ArgumentList @('--consume', $probeId) `
        -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $runDirectory 'consumer.jsonl') `
        -RedirectStandardError (Join-Path $runDirectory 'consumer.stderr.log')
    $producer = Start-Process -FilePath $producerExe -ArgumentList @('--produce', $probeId) `
        -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $runDirectory 'producer.jsonl') `
        -RedirectStandardError (Join-Path $runDirectory 'producer.stderr.log')
    if (-not $producer.WaitForExit(30000)) { throw 'GPU producer timed out.' }
    if (-not $consumer.WaitForExit(15000)) { throw 'GPU consumer timed out.' }
    $producer.WaitForExit()
    $consumer.WaitForExit()
    Get-Content -LiteralPath (Join-Path $runDirectory 'producer.jsonl')
    Get-Content -LiteralPath (Join-Path $runDirectory 'consumer.jsonl')
    if ($producer.ExitCode -ne 0 -or $consumer.ExitCode -ne 0) {
        Get-Content -LiteralPath (Join-Path $runDirectory 'producer.stderr.log')
        Get-Content -LiteralPath (Join-Path $runDirectory 'consumer.stderr.log')
        throw "GPU channel probe failed. Evidence: $runDirectory"
    }
    Write-Output "GPU channel evidence: $runDirectory"
} finally {
    foreach ($process in @($producer, $consumer)) {
        if ($null -ne $process -and -not $process.HasExited) { $process.Kill() }
    }
}
