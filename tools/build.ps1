param([switch]$SkipDependencies)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    if (-not $SkipDependencies) {
        git submodule update --init --recursive --depth 1
        if ($LASTEXITCODE) { throw 'Dependency checkout failed.' }
    }
    cmake -S . -B build -A x64 -DOPENJKDF2_DROIDWORKS=OFF
    if ($LASTEXITCODE) { throw 'CMake configuration failed.' }
    cmake --build build --config Release --parallel 8
    if ($LASTEXITCODE) { throw 'Engine build failed.' }
    cmake -S ./tools/light-baker -B build-light-baker -A Win32
    if ($LASTEXITCODE) { throw 'Light Baker host configuration failed.' }
    cmake --build build-light-baker --config Release --parallel 8
    if ($LASTEXITCODE) { throw 'Light Baker host build failed.' }
    Copy-Item build-light-baker/Release/GeoSithLightBaker.exe build/Release/GeoSithLightBaker.exe -Force
    cmake -S tests -B build-tests -A x64
    if ($LASTEXITCODE) { throw 'Test configuration failed.' }
    cmake --build build-tests --config Release
    if ($LASTEXITCODE) { throw 'Test build failed.' }
    & ./build-tests/Release/editor_tests.exe
    if ($LASTEXITCODE) { throw 'Geometry tests failed.' }
    Write-Host 'Build complete. Run Launch GeoSith.cmd.'
} finally { Pop-Location }

