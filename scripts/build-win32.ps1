param(
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $Root "build-win32"

cmake -S $Root -B $BuildDir -A Win32
if ($LASTEXITCODE -ne 0) {
    throw "CMake Win32 configure failed with exit code $LASTEXITCODE"
}

cmake --build $BuildDir --config $Configuration --clean-first --parallel
if ($LASTEXITCODE -ne 0) {
    throw "CMake Win32 build failed with exit code $LASTEXITCODE"
}

ctest --test-dir $BuildDir -C $Configuration --no-tests=error --output-on-failure
if ($LASTEXITCODE -ne 0) {
    throw "CTest Win32 failed with exit code $LASTEXITCODE"
}

Write-Host "Win32 build complete: $(Join-Path $BuildDir "$Configuration\nvse_fnvxr.dll")"
