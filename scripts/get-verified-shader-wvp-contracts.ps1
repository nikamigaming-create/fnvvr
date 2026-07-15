[CmdletBinding()]
param(
    [string[]]$RunDir = @(),
    [string]$FxcPath = "",
    [switch]$ContractsOnly,
    [switch]$SelfTestOnly
)

$ErrorActionPreference = "Stop"

function Get-ShaderDestinationComponents {
    param([string]$Mask)
    if ([string]::IsNullOrWhiteSpace($Mask)) {
        return @('x', 'y', 'z', 'w')
    }
    return @($Mask.ToLowerInvariant().ToCharArray() | ForEach-Object { [string]$_ })
}

function Test-ShaderClipPositionDataFlow {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Dump,
        [Parameter(Mandatory = $true)]
        [int]$StartRegister
    )

    # Track only provenance relevant to clip position. Every unrecognized write
    # to a tracked temporary/oPos component clears that component, so dead
    # writes and later overwrites cannot accidentally certify a shader.
    $provenance = @{}
    foreach ($rawLine in ($Dump -split "`r?`n")) {
        $line = $rawLine.Trim()
        if ([string]::IsNullOrWhiteSpace($line) -or $line.StartsWith('//')) {
            continue
        }

        $instruction = [regex]::Match(
            $line,
            '^(?<opcode>[a-zA-Z][a-zA-Z0-9_]*)\s+(?<register>oPos|r\d+)(?:\.(?<mask>[xyzw]{1,4}))?(?:\s*,\s*(?<operands>.*))?$')
        if (-not $instruction.Success) {
            continue
        }

        $opcode = $instruction.Groups['opcode'].Value.ToLowerInvariant()
        $destination = $instruction.Groups['register'].Value.ToLowerInvariant()
        $mask = $instruction.Groups['mask'].Value
        $destinationComponents = @(Get-ShaderDestinationComponents -Mask $mask)
        foreach ($component in $destinationComponents) {
            [void]$provenance.Remove("$destination.$component")
        }

        $operands = @()
        if ($instruction.Groups['operands'].Success) {
            $operands = @($instruction.Groups['operands'].Value -split '\s*,\s*')
        }

        if ($opcode -eq 'dp4' -and $destinationComponents.Count -eq 1 -and $operands.Count -eq 2) {
            $constantIndex = -1
            $sourceIndex = -1
            for ($index = 0; $index -lt 2; $index++) {
                $constant = [regex]::Match(
                    $operands[$index],
                    '^c(?<register>\d+)(?:\.[xyzw]{1,4})?$')
                if ($constant.Success) {
                    if ($constantIndex -ge 0) {
                        $constantIndex = -2
                        break
                    }
                    $constantIndex = [int]$constant.Groups['register'].Value
                    $sourceIndex = 1 - $index
                }
            }
            $row = $constantIndex - $StartRegister
            if ($constantIndex -ge $StartRegister -and
                $constantIndex -lt ($StartRegister + 4) -and
                $sourceIndex -ge 0) {
                $source = $operands[$sourceIndex].ToLowerInvariant() -replace '\.xyzw$', ''
                if ($source -match '^(?:v\d+|r\d+)$') {
                    $component = $destinationComponents[0]
                    $provenance["$destination.$component"] = "row:$row|source:$source"
                }
            }
            continue
        }

        if ($opcode -eq 'mov' -and $operands.Count -eq 1) {
            $sourceMatch = [regex]::Match(
                $operands[0].ToLowerInvariant(),
                '^(?<register>oPos|r\d+)(?:\.(?<swizzle>[xyzw]{1,4}))?$')
            if ($sourceMatch.Success) {
                $sourceRegister = $sourceMatch.Groups['register'].Value.ToLowerInvariant()
                $sourceSwizzle = $sourceMatch.Groups['swizzle'].Value
                $sourceComponents = if ([string]::IsNullOrWhiteSpace($sourceSwizzle)) {
                    @($destinationComponents)
                }
                else {
                    @(Get-ShaderDestinationComponents -Mask $sourceSwizzle)
                }
                if ($sourceComponents.Count -eq 1 -and $destinationComponents.Count -gt 1) {
                    $sourceComponents = @($destinationComponents | ForEach-Object { $sourceComponents[0] })
                }
                if ($sourceComponents.Count -eq $destinationComponents.Count) {
                    for ($index = 0; $index -lt $destinationComponents.Count; $index++) {
                        $sourceKey = "$sourceRegister.$($sourceComponents[$index])"
                        $destinationKey = "$destination.$($destinationComponents[$index])"
                        if ($provenance.ContainsKey($sourceKey)) {
                            $provenance[$destinationKey] = $provenance[$sourceKey]
                        }
                    }
                }
            }
        }
    }

    if ($env:FNVXR_DEBUG_SHADER_DATAFLOW -eq "1") {
        Write-Host ($provenance | ConvertTo-Json -Compress)
    }
    $expectedSource = $null
    $components = @('x', 'y', 'z', 'w')
    for ($row = 0; $row -lt 4; $row++) {
        $key = "opos.$($components[$row])"
        if (-not $provenance.ContainsKey($key)) {
            return $false
        }
        $match = [regex]::Match($provenance[$key], '^row:(?<row>\d)\|source:(?<source>.+)$')
        if (-not $match.Success -or [int]$match.Groups['row'].Value -ne $row) {
            return $false
        }
        $source = $match.Groups['source'].Value
        if ($null -eq $expectedSource) {
            $expectedSource = $source
        }
        elseif ($source -ne $expectedSource) {
            return $false
        }
    }
    return $true
}

# Adversarial parser fixtures execute on every invocation. They prevent a
# future regex simplification from accepting unrelated temporaries, partial
# xy writes, or an oPos overwrite after a seemingly valid matrix multiply.
$positiveDataFlowFixture = @'
dp4 r2.x, c4, v0
dp4 r2.y, c5, v0
dp4 r2.z, c6, v0
dp4 r2.w, c7, v0
mov oPos, r2
'@
$unrelatedTemporaryFixture = @'
dp4 r0.x, c4, v0
dp4 r7.y, c5, v0
dp4 r0.z, c6, v0
dp4 r0.w, c7, v0
mov oPos, r0
'@
$partialFixture = @'
dp4 oPos.x, c4, v0
dp4 oPos.y, c5, v0
'@
$overwriteFixture = @'
dp4 oPos.x, c4, v0
dp4 oPos.y, c5, v0
dp4 oPos.z, c6, v0
dp4 oPos.w, c7, v0
mov oPos, r9
'@
if (-not (Test-ShaderClipPositionDataFlow -Dump $positiveDataFlowFixture -StartRegister 4) -or
    (Test-ShaderClipPositionDataFlow -Dump $unrelatedTemporaryFixture -StartRegister 4) -or
    (Test-ShaderClipPositionDataFlow -Dump $partialFixture -StartRegister 4) -or
    (Test-ShaderClipPositionDataFlow -Dump $overwriteFixture -StartRegister 4)) {
    throw "Internal shader oPos xyzw data-flow fixtures failed"
}
if ($SelfTestOnly) {
    [pscustomobject]@{
        Passed = $true
        PositiveCompleteChainAccepted = $true
        UnrelatedTemporariesRejected = $true
        PartialXyRejected = $true
        FinalOverwriteRejected = $true
    }
    return
}

$runPaths = [System.Collections.Generic.List[string]]::new()
foreach ($candidateRunDir in $RunDir) {
    if ([string]::IsNullOrWhiteSpace($candidateRunDir)) {
        continue
    }

    $resolvedRunPath = (Resolve-Path -LiteralPath $candidateRunDir).Path
    if (-not $runPaths.Contains($resolvedRunPath)) {
        $runPaths.Add($resolvedRunPath)
    }
}
if ($runPaths.Count -eq 0) {
    throw "At least one non-empty shader discovery run directory is required."
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

$discoveryRecords = [System.Collections.Generic.List[object]]::new()
foreach ($runPath in $runPaths) {
    $logPath = Join-Path $runPath "fnvxr_d3d9_proxy.log"
    if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
        throw "Shader discovery log not found: $logPath"
    }

    foreach ($line in Get-Content -LiteralPath $logPath) {
        $jsonStart = $line.IndexOf('{"event":"fnvxrShaderWvpDiscovery"')
        if ($jsonStart -lt 0) { continue }

        $shader = $line.Substring($jsonStart) | ConvertFrom-Json
        $sha = ([string]$shader.sha256).ToLowerInvariant()
        $fnv = ([string]$shader.fnv1a32).ToLowerInvariant()
        $expectedBytes = [int]$shader.bytes
        if ($sha -notmatch '^[0-9a-f]{64}$' -or $fnv -notmatch '^[0-9a-f]{8}$' -or $expectedBytes -le 0) {
            throw "Malformed shader discovery identity in $logPath`: $($shader | ConvertTo-Json -Compress)"
        }

        $discoveryRecords.Add([pscustomobject]@{
            RunPath = $runPath
            Shader = $shader
            Sha256 = $sha
            Fnv1a32 = $fnv
            Bytes = $expectedBytes
        })
    }
}
if ($discoveryRecords.Count -eq 0) {
    throw "No fnvxrShaderWvpDiscovery events were found in the requested run directories."
}

# A single Fallout session does not necessarily exercise every world/UI
# transition shader. Merge strong identities across independently captured
# runs so a later launch cannot forget an earlier, disassembly-verified shader.
$discoveries = [System.Collections.Generic.List[object]]::new()
$discoveryGroups = @($discoveryRecords | Group-Object Sha256 | Sort-Object Name)
foreach ($group in $discoveryGroups) {
    $identityVariants = @($group.Group |
        ForEach-Object { "{0}/{1}/{2}" -f $_.Fnv1a32, $_.Sha256, $_.Bytes } |
        Sort-Object -Unique)
    if ($identityVariants.Count -ne 1) {
        throw "Conflicting discovery identities share SHA-256 $($group.Name): $($identityVariants -join ', ')"
    }

    $sourceRecord = $group.Group |
        Where-Object {
            $candidateBytecode = Join-Path $_.RunPath ("fnvxr_shader_vs_{0}.bin" -f $_.Sha256)
            Test-Path -LiteralPath $candidateBytecode -PathType Leaf
        } |
        Select-Object -First 1
    if ($null -eq $sourceRecord) {
        $sourceRuns = @($group.Group | Select-Object -ExpandProperty RunPath -Unique)
        throw "Discovered vertex shader bytecode is missing for $($group.Name) in: $($sourceRuns -join ', ')"
    }
    $discoveries.Add($sourceRecord)
}

$fnvConflicts = @($discoveries |
    Group-Object Fnv1a32 |
    Where-Object { $_.Count -gt 1 })
if ($fnvConflicts.Count -gt 0) {
    $conflictDetails = @($fnvConflicts | ForEach-Object {
        "{0}=>{1}" -f $_.Name, (($_.Group | Select-Object -ExpandProperty Sha256) -join ',')
    })
    throw "Ambiguous FNV-1a shader identities cannot be loaded by the D3D9 proxy: $($conflictDetails -join '; ')"
}

$contracts = [System.Collections.Generic.List[string]]::new()
$screenShaders = [System.Collections.Generic.List[string]]::new()
foreach ($discovery in $discoveries) {
    $sha = $discovery.Sha256
    $fnv = $discovery.Fnv1a32
    $expectedBytes = $discovery.Bytes
    $runPath = $discovery.RunPath

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
        if (-not (Test-ShaderClipPositionDataFlow -Dump $dump -StartRegister $register)) {
            throw "Shader $sha declares $name at c$register but does not prove a complete, unoverwritten oPos.xyzw data-flow chain from c$register-c$($register + 3)"
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
    RunDir = @($runPaths)
    FxcPath = $FxcPath
    DiscoveredShaders = $discoveries.Count
    VerifiedWorldContracts = $contracts.Count
    VerifiedScreenShaders = $screenShaders.Count
    ScreenShaderSha256 = @($screenShaders)
    ContractStringLength = $contractString.Length
    ContractString = $contractString
}
