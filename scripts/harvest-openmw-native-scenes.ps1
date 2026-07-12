param(
    [string]$ModlistRoot = $env:FNVXR_MODLIST_ROOT,
    [string]$SceneName = "goodsprings",
    [string]$StartCell = "Goodsprings",
    [int]$RunSeconds = 70,
    [string]$ScreenshotFrames = "900,1200,1500,1800,2100",
    [int]$InventoryFrame = 9999,
    [string]$PaneFrames = "9999",
    [string]$PaneIndices = "0"
)

$ErrorActionPreference = "Stop"

if (-not $ModlistRoot) {
    throw "Set -ModlistRoot or FNVXR_MODLIST_ROOT to the local FNV OpenMW modlist root."
}

$Root = Split-Path -Parent $PSScriptRoot
$HarvestRoot = Join-Path $Root "local\openmw-harvests"
$Stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$HarvestDir = Join-Path $HarvestRoot "$SceneName-$Stamp"
$LatestPath = Join-Path $HarvestRoot "latest.json"
New-Item -ItemType Directory -Force -Path $HarvestDir | Out-Null

$tool = Join-Path $ModlistRoot "tools\run_fnv_native_screenshots.ps1"
$proofRoot = Join-Path $ModlistRoot "openmw-config\native-screenshot-proof"
if (-not (Test-Path -LiteralPath $tool)) {
    throw "Missing OpenMW native screenshot tool: $tool"
}

$startedAt = Get-Date
& powershell -NoProfile -ExecutionPolicy Bypass -File $tool `
    -RunSeconds $RunSeconds `
    -StartCell $StartCell `
    -ScreenshotFrames $ScreenshotFrames `
    -InventoryFrame $InventoryFrame `
    -PaneFrames $PaneFrames `
    -PaneIndices $PaneIndices
if ($LASTEXITCODE -ne 0) {
    throw "OpenMW native screenshot harvest failed with exit code $LASTEXITCODE"
}

$proofDir = Get-ChildItem -LiteralPath $proofRoot -Directory -ErrorAction Stop |
    Where-Object { $_.LastWriteTime -ge $startedAt.AddSeconds(-5) } |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if (-not $proofDir) {
    throw "No fresh OpenMW proof directory appeared under $proofRoot"
}

$screenshots = @(Get-ChildItem -LiteralPath $proofDir.FullName -File -Include *.png,*.jpg,*.jpeg -ErrorAction SilentlyContinue)
foreach ($shot in $screenshots) {
    Copy-Item -LiteralPath $shot.FullName -Destination (Join-Path $HarvestDir $shot.Name) -Force
}
foreach ($name in @("openmw.log", "MyGUI.log", "summary.txt", "console.txt")) {
    $path = Join-Path $proofDir.FullName $name
    if (Test-Path -LiteralPath $path) {
        Copy-Item -LiteralPath $path -Destination (Join-Path $HarvestDir $name) -Force
    }
}

$manifest = [ordered]@{
    harvestedAt = (Get-Date).ToString("o")
    sceneName = $SceneName
    startCell = $StartCell
    role = "openmw-native-scene-harvest"
    harvestType = "flat-screenshot-proof"
    isSixDofSceneCache = $false
    note = "This proves OpenMW can load and render real FNV scene content. It is not a reusable 6DOF scene cache yet."
    modlistRoot = $ModlistRoot
    sourceProofDir = $proofDir.FullName
    harvestDir = $HarvestDir
    screenshotCount = $screenshots.Count
    screenshots = @($screenshots | ForEach-Object { Join-Path $HarvestDir $_.Name })
}

$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $HarvestDir "manifest.json") -Encoding UTF8
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $LatestPath -Encoding UTF8
$manifest | ConvertTo-Json -Depth 5
