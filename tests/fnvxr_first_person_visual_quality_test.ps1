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

    # Final-eye alpha is presentation-inert under the product's required
    # OPAQUE OpenXR blend mode. The host writes exact per-category tags there
    # so this gate proves visible pixel contribution instead of trusting node
    # discovery or transform residuals.
    Add-Type -AssemblyName System.Drawing
    function New-SpatialPairDirectory {
        param(
            [Parameter(Mandatory = $true)][string]$Directory,
            [int]$MissingRightHandFrame = 0,
            [switch]$VoidBackground,
            [switch]$DetachedPipBoy
        )
        New-Item -ItemType Directory -Path $Directory -Force | Out-Null
        for ($frame = 1; $frame -le 6; ++$frame) {
            foreach ($eye in @("left", "right")) {
                $bitmap = [System.Drawing.Bitmap]::new(
                    240,
                    140,
                    [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
                $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
                try {
                    $graphics.CompositingMode =
                        [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
                    $graphics.Clear([System.Drawing.Color]::FromArgb(
                        255, 0, 0, 0))
                    if (-not $VoidBackground) {
                        for ($tile = 0; $tile -lt 12; ++$tile) {
                            $shade = if (($tile % 2) -eq 0) { 24 } else { 104 }
                            $worldBrush = [System.Drawing.SolidBrush]::new(
                                [System.Drawing.Color]::FromArgb(
                                    255, $shade, ($shade + 8), ($shade + 16)))
                            try {
                                $graphics.FillRectangle(
                                    $worldBrush, $tile * 20, 0, 20, 140)
                            } finally {
                                $worldBrush.Dispose()
                            }
                        }
                    }
                    $leftBrush = [System.Drawing.SolidBrush]::new(
                        [System.Drawing.Color]::FromArgb(51, 150, 105, 82))
                    $rightBrush = [System.Drawing.SolidBrush]::new(
                        [System.Drawing.Color]::FromArgb(102, 150, 105, 82))
                    $housingBrush = [System.Drawing.SolidBrush]::new(
                        [System.Drawing.Color]::FromArgb(153, 72, 82, 58))
                    $screenBrush = [System.Drawing.SolidBrush]::new(
                        [System.Drawing.Color]::FromArgb(204, 25, 55, 30))
                    try {
                        $graphics.FillRectangle($leftBrush, 42, 86, 28, 44)
                        if ($frame -ne $MissingRightHandFrame) {
                            $graphics.FillRectangle(
                                $rightBrush, 174, 84, 30, 46)
                        }
                        $housingX = if ($DetachedPipBoy) { 112 } else { 26 }
                        $graphics.FillRectangle(
                            $housingBrush, $housingX, 76, 74, 25)
                        if ($frame -ge 2 -and $frame -le 5) {
                            $graphics.FillRectangle(
                                $screenBrush, ($housingX + 12), 80, 48, 15)
                        }
                    } finally {
                        $leftBrush.Dispose()
                        $rightBrush.Dispose()
                        $housingBrush.Dispose()
                        $screenBrush.Dispose()
                    }
                    $path = Join-Path $Directory (
                        "pair_{0:D6}_{1}.png" -f $frame, $eye)
                    $bitmap.Save(
                        $path,
                        [System.Drawing.Imaging.ImageFormat]::Png)
                } finally {
                    $graphics.Dispose()
                    $bitmap.Dispose()
                }
            }
        }
    }

    $spatialGoodDirectory = Join-Path $testRoot "spatial-good"
    New-SpatialPairDirectory -Directory $spatialGoodDirectory
    $spatialGoodReportPath = Join-Path $testRoot "spatial-good.json"
    & $python $analyzer `
        --pair-directory $spatialGoodDirectory `
        --spatial-overlay `
        --require-pipboy-screen `
        --require-world-background `
        --max-red-ratio 0.0005 `
        --max-red-jump 0.0001 `
        --report $spatialGoodReportPath | Out-Null
    if ($LASTEXITCODE -ne 0) {
        $failedSpatialGood = Get-Content `
            -LiteralPath $spatialGoodReportPath `
            -Raw | ConvertFrom-Json
        throw (
            "The stable tagged hands/Pip-Boy sequence failed its pixel gate: " +
            (@($failedSpatialGood.failures.metric) -join ", "))
    }
    $spatialGood = Get-Content `
        -LiteralPath $spatialGoodReportPath `
        -Raw | ConvertFrom-Json
    if (-not [bool]$spatialGood.accepted -or
        @($spatialGood.failures).Count -ne 0 -or
        $null -eq $spatialGood.spatial_overlay) {
        throw "The stable tagged sequence lacks a clean spatial-overlay proof."
    }

    $spatialVoidDirectory = Join-Path $testRoot "spatial-void"
    New-SpatialPairDirectory `
        -Directory $spatialVoidDirectory `
        -VoidBackground
    $spatialVoidReportPath = Join-Path $testRoot "spatial-void.json"
    & $python $analyzer `
        --pair-directory $spatialVoidDirectory `
        --spatial-overlay `
        --require-pipboy-screen `
        --require-world-background `
        --max-red-ratio 0.0005 `
        --max-red-jump 0.0001 `
        --report $spatialVoidReportPath | Out-Null
    if ($LASTEXITCODE -ne 2) {
        throw "A black final-eye background was not rejected."
    }
    $spatialVoid = Get-Content `
        -LiteralPath $spatialVoidReportPath `
        -Raw | ConvertFrom-Json
    if ([bool]$spatialVoid.accepted -or
        -not (@($spatialVoid.failures.metric) -contains
            "world_background_nonblack_fraction")) {
        throw "The rejected void sequence lacks final-eye background evidence."
    }

    $spatialDetachedDirectory = Join-Path $testRoot "spatial-detached"
    New-SpatialPairDirectory `
        -Directory $spatialDetachedDirectory `
        -DetachedPipBoy
    $spatialDetachedReportPath = Join-Path $testRoot "spatial-detached.json"
    & $python $analyzer `
        --pair-directory $spatialDetachedDirectory `
        --spatial-overlay `
        --max-red-ratio 0.0005 `
        --max-red-jump 0.0001 `
        --report $spatialDetachedReportPath | Out-Null
    if ($LASTEXITCODE -ne 2) {
        throw "A visibly detached Pip-Boy housing was not rejected."
    }
    $spatialDetached = Get-Content `
        -LiteralPath $spatialDetachedReportPath `
        -Raw | ConvertFrom-Json
    if ([bool]$spatialDetached.accepted -or
        -not (@($spatialDetached.failures.metric) -contains
            "pipboy_wrist_socket_bbox_gap_pixels")) {
        throw "The rejected detached Pip-Boy lacks wrist-socket evidence."
    }

    $spatialMissingDirectory = Join-Path $testRoot "spatial-missing"
    New-SpatialPairDirectory `
        -Directory $spatialMissingDirectory `
        -MissingRightHandFrame 4
    $spatialMissingReportPath = Join-Path $testRoot "spatial-missing.json"
    & $python $analyzer `
        --pair-directory $spatialMissingDirectory `
        --spatial-overlay `
        --max-red-ratio 0.0005 `
        --max-red-jump 0.0001 `
        --report $spatialMissingReportPath | Out-Null
    if ($LASTEXITCODE -ne 2) {
        throw "A one-frame missing right hand was not rejected."
    }
    $spatialMissing = Get-Content `
        -LiteralPath $spatialMissingReportPath `
        -Raw | ConvertFrom-Json
    if ([bool]$spatialMissing.accepted -or
        -not (@($spatialMissing.failures.metric) -contains
            "right_hand_minimum_ratio")) {
        throw "The missing-hand report lacks final-pixel disappearance evidence."
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
