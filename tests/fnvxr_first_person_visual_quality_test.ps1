param(
    [Parameter(Mandatory = $true)][string]$SourceRoot
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

$python = (Get-Command python -ErrorAction Stop).Source
$analyzer = Join-Path $SourceRoot "tools\analyze_first_person_visual.py"
$goodImage = Join-Path $SourceRoot "output\fnvxr-hands-pipboy-gate-preview.png"
if (-not (Test-Path -LiteralPath $analyzer -PathType Leaf) -or
    -not (Test-Path -LiteralPath $goodImage -PathType Leaf)) {
    throw "First-person visual-quality test inputs are missing."
}

$testRoot = Join-Path `
    ([System.IO.Path]::GetTempPath()) `
    ("fnvxr-first-person-visual-" + [Guid]::NewGuid().ToString("N"))
$resolvedTemp = [System.IO.Path]::GetFullPath(
    [System.IO.Path]::GetTempPath()).TrimEnd('\', '/') + '\'
$resolvedTestRoot = [System.IO.Path]::GetFullPath($testRoot)
if (-not $resolvedTestRoot.StartsWith(
        $resolvedTemp,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing unsafe visual-quality test root: $resolvedTestRoot"
}

try {
    New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
    $goodReportPath = Join-Path $testRoot "good.json"
    & $python $analyzer `
        --image $goodImage `
        --content-top 180 `
        --content-height 720 `
        --report $goodReportPath | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "The retained authentic hands/Pip-Boy reference failed the visual gate."
    }
    $good = Get-Content -LiteralPath $goodReportPath -Raw | ConvertFrom-Json
    if (-not [bool]$good.accepted -or @($good.failures).Count -ne 0) {
        throw "The authentic reference report did not record a clean pass."
    }

    # Two large brown lower-eye components reproduce the known stretched-arm
    # morphology without depending on a retained failing product run.
    $width = 400
    $height = 120
    $pixels = New-Object byte[] ($width * $height * 3)
    for ($y = 0; $y -lt $height; ++$y) {
        for ($x = 0; $x -lt $width; ++$x) {
            $insideArm = $y -ge 68 -and
                (($x -ge 20 -and $x -lt 180) -or
                 ($x -ge 220 -and $x -lt 380))
            $red = if ($insideArm) { 120 } else { 20 }
            $green = if ($insideArm) { 70 } else { 20 }
            $blue = if ($insideArm) { 30 } else { 20 }
            $offset = ($y * $width + $x) * 3
            $pixels[$offset] = [byte]$red
            $pixels[$offset + 1] = [byte]$green
            $pixels[$offset + 2] = [byte]$blue
        }
    }
    $badImage = Join-Path $testRoot "stretched.ppm"
    $header = [System.Text.Encoding]::ASCII.GetBytes(
        "P6`n$width $height`n255`n")
    $payload = New-Object byte[] ($header.Length + $pixels.Length)
    [Array]::Copy($header, 0, $payload, 0, $header.Length)
    [Array]::Copy($pixels, 0, $payload, $header.Length, $pixels.Length)
    [System.IO.File]::WriteAllBytes($badImage, $payload)

    $badReportPath = Join-Path $testRoot "bad.json"
    & $python $analyzer --image $badImage --report $badReportPath | Out-Null
    if ($LASTEXITCODE -ne 2) {
        throw "The synthetic stretched-arm fixture was not rejected."
    }
    $bad = Get-Content -LiteralPath $badReportPath -Raw | ConvertFrom-Json
    if ([bool]$bad.accepted -or @($bad.failures).Count -eq 0 -or
        -not (@($bad.failures.metric) -contains "brown_component_ratio")) {
        throw "The rejected visual report lacks its stretched-arm evidence."
    }
} finally {
    $checked = [System.IO.Path]::GetFullPath($testRoot)
    if ($checked.StartsWith(
            $resolvedTemp,
            [System.StringComparison]::OrdinalIgnoreCase) -and
        (Test-Path -LiteralPath $checked)) {
        Remove-Item -LiteralPath $checked -Recurse -Force
    }
}

Write-Host "First-person final-pixel visual-quality gate passed."
