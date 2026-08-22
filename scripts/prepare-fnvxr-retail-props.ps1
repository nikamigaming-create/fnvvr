param(
    [Parameter(Mandatory = $true)][string]$GameRoot,
    [switch]$Force,
    [switch]$ValidateOnly
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

$sourceRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$assetRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $sourceRoot "local\retail-assets"))
$allowedPrefix = [System.IO.Path]::GetFullPath(
    (Join-Path $sourceRoot "local")).TrimEnd('\', '/') + '\'
if (-not $assetRoot.StartsWith(
        $allowedPrefix,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing retail-prop output outside the workspace local directory: $assetRoot"
}

$resolvedGameRoot = [System.IO.Path]::GetFullPath($GameRoot)
$meshArchive = Join-Path $resolvedGameRoot "Data\Fallout - Meshes.bsa"
$textureArchive = Join-Path $resolvedGameRoot "Data\Fallout - Textures.bsa"
$textureArchive2 = Join-Path $resolvedGameRoot "Data\Fallout - Textures2.bsa"
foreach ($archive in @($meshArchive, $textureArchive, $textureArchive2)) {
    if (-not (Test-Path -LiteralPath $archive -PathType Leaf)) {
        throw "Installed retail archive is missing: $archive"
    }
}

$python = (Get-Command python -ErrorAction Stop).Source
$extractor = Join-Path $sourceRoot "tools\extract_bsa_file.py"
$handConverter = Join-Path $sourceRoot "tools\convert_nif_hand_mesh.py"
$pipBoyConverter = Join-Path $sourceRoot "tools\convert_nif_pipboy_mesh.py"
$forearmConverter = Join-Path $sourceRoot "tools\convert_nif_forearm_mesh.py"
foreach ($tool in @(
        $extractor,
        $handConverter,
        $pipBoyConverter,
        $forearmConverter)) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) {
        throw "Retail-prop preparation tool is missing: $tool"
    }
}

function Get-Sha256Lower {
    param([Parameter(Mandatory = $true)][string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

$raw = [ordered]@{
    leftHand = Join-Path $assetRoot "lefthand1st.nif"
    leftPipBoyGlove = Join-Path $assetRoot "lefthandpipboyglove1st.nif"
    rightHand = Join-Path $assetRoot "righthand1st.nif"
    skeleton = Join-Path $assetRoot "skeleton1st.nif"
    pistolGrip = Join-Path $assetRoot "1hphandgrip1.kf"
    pipBoy = Join-Path $assetRoot "pipboyarm.nif"
    upperBody = Join-Path $assetRoot "upperbody.nif"
    handTexture = Join-Path $assetRoot "HandMale.dds"
    upperBodyTexture = Join-Path $assetRoot "UpperBodyMale.dds"
    pipBoyTexture = Join-Path $assetRoot "PipBoyArm01.dds"
    pipBoyGloveTexture = Join-Path $assetRoot "PipBoyGlove01.dds"
}
$outputs = [ordered]@{
    leftHand = Join-Path $assetRoot "lefthandpipboy-hand.fhm"
    leftCuff = Join-Path $assetRoot "lefthandpipboy-cuff.fhm"
    rightHand = Join-Path $assetRoot "righthand1st-grip.fhm"
    pipBoy = Join-Path $assetRoot "pipboyarm.fpm"
    pipBoyScreen = Join-Path $assetRoot "pipboyscreen.fps"
    leftForearm = Join-Path $assetRoot "left-forearm.fhm"
}
$manifestPath = Join-Path $assetRoot "retail-props-v3.json"
$toolHashes = [ordered]@{
    extractor = Get-Sha256Lower -Path $extractor
    handConverter = Get-Sha256Lower -Path $handConverter
    pipBoyConverter = Get-Sha256Lower -Path $pipBoyConverter
    forearmConverter = Get-Sha256Lower -Path $forearmConverter
}

function Test-PreparedManifest {
    if ($Force -or -not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        return $false
    }
    try {
        $cached = Get-Content -LiteralPath $manifestPath -Raw |
            ConvertFrom-Json -ErrorAction Stop
        if ([string]$cached.schema -cne "fnvxr-retail-props/v3" -or
            [string]$cached.coordinateBasis -cne "openxr-grip-minus-z" -or
            [string]$cached.rightHandPose -cne "1hphandgrip1@end") {
            return $false
        }
        foreach ($name in $toolHashes.Keys) {
            if ([string]$cached.tools.$name -cne [string]$toolHashes[$name]) {
                return $false
            }
        }
        foreach ($name in $raw.Keys) {
            $path = $raw[$name]
            if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or
                [string]$cached.inputs.$name.sha256 -cne
                    (Get-Sha256Lower -Path $path)) {
                return $false
            }
        }
        foreach ($name in $outputs.Keys) {
            $path = $outputs[$name]
            if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or
                [string]$cached.outputs.$name.sha256 -cne
                    (Get-Sha256Lower -Path $path)) {
                return $false
            }
        }
        return $true
    } catch {
        return $false
    }
}

if (Test-PreparedManifest) {
    Get-Content -LiteralPath $manifestPath -Raw
    return
}
if ($ValidateOnly) {
    throw "Retail-derived spatial props are missing or stale; run prepare-fnvxr-retail-props.ps1 without -ValidateOnly."
}

& $python -c "import pyffi" 2>$null
if ($LASTEXITCODE -ne 0) {
    throw "Python package 'pyffi' is required to prepare installed retail NIF meshes."
}
New-Item -ItemType Directory -Path $assetRoot -Force | Out-Null

function Invoke-ExactExtraction {
    param(
        [Parameter(Mandatory = $true)][string]$Archive,
        [Parameter(Mandatory = $true)][string]$Entry,
        [Parameter(Mandatory = $true)][string]$Output
    )
    & $python $extractor `
        --archive $Archive `
        --extract $Entry `
        --output $Output | Out-Null
    if ($LASTEXITCODE -ne 0 -or
        -not (Test-Path -LiteralPath $Output -PathType Leaf)) {
        throw "Could not extract installed retail asset '$Entry' from '$Archive'."
    }
}

# Refresh every raw source together. Mixing files from different archive
# revisions would make the skeleton/KF skin bake non-reproducible.
Invoke-ExactExtraction -Archive $meshArchive `
    -Entry "meshes\characters\_male\lefthand1st.nif" `
    -Output $raw.leftHand
Invoke-ExactExtraction -Archive $meshArchive `
    -Entry "meshes\characters\_male\lefthandpipboyglove1st.nif" `
    -Output $raw.leftPipBoyGlove
Invoke-ExactExtraction -Archive $meshArchive `
    -Entry "meshes\characters\_male\righthand1st.nif" `
    -Output $raw.rightHand
Invoke-ExactExtraction -Archive $meshArchive `
    -Entry "meshes\characters\_1stperson\skeleton.nif" `
    -Output $raw.skeleton
Invoke-ExactExtraction -Archive $meshArchive `
    -Entry "meshes\characters\_male\1hphandgrip1.kf" `
    -Output $raw.pistolGrip
Invoke-ExactExtraction -Archive $meshArchive `
    -Entry "meshes\pipboy3000\pipboyarm.nif" `
    -Output $raw.pipBoy
Invoke-ExactExtraction -Archive $meshArchive `
    -Entry "meshes\characters\_male\upperbody.nif" `
    -Output $raw.upperBody
Invoke-ExactExtraction -Archive $textureArchive `
    -Entry "textures\characters\male\handmale.dds" `
    -Output $raw.handTexture
Invoke-ExactExtraction -Archive $textureArchive `
    -Entry "textures\characters\male\upperbodymale.dds" `
    -Output $raw.upperBodyTexture
Invoke-ExactExtraction -Archive $textureArchive2 `
    -Entry "textures\pipboy3000\pipboyarm01.dds" `
    -Output $raw.pipBoyTexture
Invoke-ExactExtraction -Archive $textureArchive2 `
    -Entry "textures\pipboy3000\pipboyglove01.dds" `
    -Output $raw.pipBoyGloveTexture

& $python $handConverter `
    --input $raw.leftPipBoyGlove `
    --output $outputs.leftHand `
    --side left `
    --shape-name "LeftHandPipBoyGlove:0" | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Left retail hand conversion failed." }
& $python $handConverter `
    --input $raw.leftPipBoyGlove `
    --output $outputs.leftCuff `
    --side left `
    --shape-name "LeftHandPipBoyGlove:1" | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Left Pip-Boy cuff conversion failed." }
& $python $handConverter `
    --input $raw.rightHand `
    --output $outputs.rightHand `
    --side right `
    --skeleton $raw.skeleton `
    --animation $raw.pistolGrip `
    --animation-time 2.4666667 | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Authored pistol-grip hand conversion failed." }
& $python $pipBoyConverter `
    --input $raw.pipBoy `
    --output $outputs.pipBoy `
    --screen-output $outputs.pipBoyScreen | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Retail Pip-Boy housing conversion failed." }
& $python $forearmConverter `
    --input $raw.upperBody `
    --output $outputs.leftForearm | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Retail left-forearm conversion failed." }

$inputRecords = [ordered]@{}
foreach ($name in $raw.Keys) {
    $item = Get-Item -LiteralPath $raw[$name] -Force
    $inputRecords[$name] = [ordered]@{
        path = $item.FullName
        length = $item.Length
        sha256 = Get-Sha256Lower -Path $item.FullName
    }
}
$outputRecords = [ordered]@{}
foreach ($name in $outputs.Keys) {
    $item = Get-Item -LiteralPath $outputs[$name] -Force
    $outputRecords[$name] = [ordered]@{
        path = $item.FullName
        length = $item.Length
        sha256 = Get-Sha256Lower -Path $item.FullName
    }
}
$manifest = [ordered]@{
    schema = "fnvxr-retail-props/v3"
    generatedAtUtc = [DateTime]::UtcNow.ToString("o")
    provenance = "locally derived from the user's installed Fallout BSAs; never staged into the game or distributed"
    coordinateBasis = "openxr-grip-minus-z"
    rightHandPose = "1hphandgrip1@end"
    tools = $toolHashes
    inputs = $inputRecords
    outputs = $outputRecords
}
$json = $manifest | ConvertTo-Json -Depth 8
[System.IO.File]::WriteAllText(
    $manifestPath,
    $json + [Environment]::NewLine,
    [System.Text.UTF8Encoding]::new($false))
$json
