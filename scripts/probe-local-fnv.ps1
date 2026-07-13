$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$OutDir = Join-Path $Root "local"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$candidates = New-Object System.Collections.Generic.List[string]

$commonRoots = @(
    $env:FNVXR_GAME_ROOT,
    "C:\Program Files (x86)\Steam\steamapps\common\Fallout New Vegas",
    "C:\GOG Games\Fallout New Vegas"
)

foreach ($root in $commonRoots) {
    if ($root) {
        $candidates.Add($root)
    }
}

$uniqueCandidates = $candidates | Select-Object -Unique
$found = @()

foreach ($root in $uniqueCandidates) {
    $exe = Join-Path $root "FalloutNV.exe"
    $data = Join-Path $root "Data"
    $nvseLoader = Join-Path $root "nvse_loader.exe"
    $found += [ordered]@{
        root = $root
        exists = Test-Path -LiteralPath $root
        falloutNVExe = $exe
        falloutNVExeExists = Test-Path -LiteralPath $exe
        dataDir = $data
        dataDirExists = Test-Path -LiteralPath $data
        falloutNVESMExists = Test-Path -LiteralPath (Join-Path $data "FalloutNV.esm")
        nvseLoaderExists = Test-Path -LiteralPath $nvseLoader
    }
}

$probe = [ordered]@{
    probedAt = (Get-Date).ToString("o")
    candidates = $found
}

$outPath = Join-Path $OutDir "fnv-probe.json"
$probe | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $outPath -Encoding UTF8
$probe | ConvertTo-Json -Depth 6
