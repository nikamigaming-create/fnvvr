$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$OutDir = Join-Path $Root "local"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$candidates = New-Object System.Collections.Generic.List[string]

$configPath = if ($env:FNVXR_OPENMW_CONFIG) {
    $env:FNVXR_OPENMW_CONFIG
} elseif ($env:FNVXR_MODLIST_ROOT) {
    Join-Path $env:FNVXR_MODLIST_ROOT "openmw-config\openmw.cfg"
} else {
    ""
}
if ($configPath -and (Test-Path -LiteralPath $configPath)) {
    Get-Content -LiteralPath $configPath | ForEach-Object {
        if ($_ -match '^data=(.+)$') {
            $dataPath = $Matches[1] -replace '/', '\'
            if ($dataPath.EndsWith('\Data', [System.StringComparison]::OrdinalIgnoreCase)) {
                $candidates.Add((Split-Path -Parent $dataPath))
            }
        }
    }
}

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
