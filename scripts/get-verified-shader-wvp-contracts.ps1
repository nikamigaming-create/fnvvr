[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$RunDir,
    [string]$FxcPath = "",
    [switch]$ContractsOnly
)

$ErrorActionPreference = "Stop"
$runPath = (Resolve-Path -LiteralPath $RunDir).Path
$logPath = Join-Path $runPath "fnvxr_d3d9_proxy.log"
if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
    throw "Shader discovery log not found: $logPath"
}

if ([string]::IsNullOrWhiteSpace($FxcPath)) {
    $kitsBin = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin"
    $FxcPath = Get-ChildItem -LiteralPath $kitsBin -Filter fxc.exe -File -Recurse |
        Where-Object { $_.FullName -match '\\x86\\fxc\.exe$' } |
        Sort-Object FullName -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}
if ([string]::IsNullOrWhiteSpace($FxcPath) -or -not (Test-Path -LiteralPath $FxcPath -PathType Leaf)) {
    throw "Microsoft fxc.exe was not found. Install the Windows SDK or pass -FxcPath."
}

$discoveries = foreach ($line in Get-Content -LiteralPath $logPath) {
    $jsonStart = $line.IndexOf('{"event":"fnvxrShaderWvpDiscovery"')
    if ($jsonStart -lt 0) { continue }
    $line.Substring($jsonStart) | ConvertFrom-Json
}
$discoveries = @($discoveries | Group-Object sha256 | ForEach-Object { $_.Group[0] })
if ($discoveries.Count -eq 0) {
    throw "No fnvxrShaderWvpDiscovery events were found in $logPath"
}

$contracts = [System.Collections.Generic.List[string]]::new()
$screenShaders = [System.Collections.Generic.List[string]]::new()
foreach ($shader in $discoveries) {
    $sha = ([string]$shader.sha256).ToLowerInvariant()
    $fnv = ([string]$shader.fnv1a32).ToLowerInvariant()
    $expectedBytes = [int]$shader.bytes
    if ($sha -notmatch '^[0-9a-f]{64}$' -or $fnv -notmatch '^[0-9a-f]{8}$' -or $expectedBytes -le 0) {
        throw "Malformed shader discovery identity: $($shader | ConvertTo-Json -Compress)"
    }

    $bytecodePath = Join-Path $runPath ("fnvxr_shader_vs_{0}.bin" -f $sha)
    if (-not (Test-Path -LiteralPath $bytecodePath -PathType Leaf)) {
        throw "Discovered vertex shader bytecode is missing: $bytecodePath"
    }
    $file = Get-Item -LiteralPath $bytecodePath
    if ($file.Length -ne $expectedBytes) {
        throw "Shader byte count mismatch for $sha (log=$expectedBytes file=$($file.Length))"
    }
    $actualSha = (Get-FileHash -LiteralPath $bytecodePath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualSha -ne $sha) {
        throw "Shader SHA-256 mismatch for $bytecodePath"
    }

    $dump = (& $FxcPath /nologo /dumpbin $bytecodePath 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0) {
        throw "fxc /dumpbin failed for $bytecodePath`n$dump"
    }

    $registerMatch = [regex]::Match(
        $dump,
        '(?m)^\s*//\s+(?<name>(?:Skin)?ModelViewProj)\s+c(?<register>\d+)\s+4\s*$')
    if ($registerMatch.Success) {
        $name = $registerMatch.Groups['name'].Value
        $register = [int]$registerMatch.Groups['register'].Value
        if ($dump -notmatch ("(?m)^\s*//\s+row_major\s+float4x4\s+{0};\s*$" -f [regex]::Escape($name))) {
            throw "Shader $sha has a non-row-major or ambiguous $name declaration"
        }
        # Some foliage/skin shaders write the clip coordinates to a temporary
        # register and move that temporary to oPos after additional work.
        $xPattern = "(?m)^\s*dp4\s+(?:oPos|r\d+)\.x,\s*c$register(?:\[[^\]]+\])?,"
        $yPattern = "(?m)^\s*dp4\s+(?:oPos|r\d+)\.y,\s*c$($register + 1)(?:\[[^\]]+\])?,"
        if ($dump -notmatch $xPattern -or $dump -notmatch $yPattern) {
            throw "Shader $sha declares $name at c$register but does not use its first rows for oPos.xy"
        }
        $contracts.Add(("{0}/{1}/{2}@{3}@column" -f $fnv, $sha, $expectedBytes, $register))
        continue
    }

    $isScreenGeometry = $dump -match '(?m)^\s*//\s+float4\s+geometryOffset;\s*$' -and
        $dump -match '(?m)^\s*mad\s+oPos\.xy,' -and
        $dump -match '(?m)^\s*mov\s+oPos\.zw,\s*v0'
    if ($isScreenGeometry) {
        $screenShaders.Add($sha)
        continue
    }

    throw "Unclassified world-draw vertex shader $sha; refusing to generate an unsafe contract"
}

if ($contracts.Count -eq 0) {
    throw "No verified ModelViewProj contracts were generated"
}
$contractString = $contracts -join ';'
if ($contractString.Length -ge 4096) {
    throw "Verified contract string is $($contractString.Length) characters and exceeds the D3D9 proxy environment buffer"
}

if ($ContractsOnly) {
    $contractString
    return
}

[pscustomobject]@{
    RunDir = $runPath
    FxcPath = $FxcPath
    DiscoveredShaders = $discoveries.Count
    VerifiedWorldContracts = $contracts.Count
    VerifiedScreenShaders = $screenShaders.Count
    ScreenShaderSha256 = @($screenShaders)
    ContractStringLength = $contractString.Length
    ContractString = $contractString
}
