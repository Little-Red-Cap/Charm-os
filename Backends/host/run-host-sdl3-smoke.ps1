param(
    [string]$BuildDir = '',
    [string]$Sdl3SourceDir = '',
    [switch]$NoFetch
)

$ErrorActionPreference = 'Stop'
$OutputEncoding = [Console]::OutputEncoding = [Text.UTF8Encoding]::new()

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '../..')
$sourceDir = Join-Path $repoRoot 'Backends/host/sdl3_smoke'
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $sourceDir 'cmake-build-backends-host-sdl3-smoke'
}

$configureArgs = @(
    '-S', $sourceDir,
    '-B', $BuildDir,
    '-G', 'Ninja',
    '-DCHARM_USE_SYSTEM_SDL3=ON',
    '-DCHARM_SDL3_GIT_REPOSITORY=https://github.com/libsdl-org/SDL.git',
    '-DCHARM_SDL3_GIT_TAG=release-3.2.8',
    '-UFETCHCONTENT_SOURCE_DIR_SDL3',
    '-UFETCHCONTENT_FULLY_DISCONNECTED',
    '-UFETCHCONTENT_UPDATES_DISCONNECTED',
    '-UFETCHCONTENT_UPDATES_DISCONNECTED_SDL3'
)
if (-not [string]::IsNullOrWhiteSpace($Sdl3SourceDir)) {
    $resolvedSdl3 = (Resolve-Path -LiteralPath $Sdl3SourceDir).Path
    $configureArgs += "-DCHARM_SDL3_SOURCE_DIR=$resolvedSdl3"
    $configureArgs += '-DCHARM_FETCHCONTENT_SDL3=OFF'
} else {
    $configureArgs += '-DCHARM_SDL3_SOURCE_DIR='
    if ($NoFetch) {
        $configureArgs += '-DCHARM_FETCHCONTENT_SDL3=OFF'
    } else {
        $configureArgs += '-DCHARM_FETCHCONTENT_SDL3=ON'
    }
}

Write-Host '[host-sdl3] configure'
& cmake @configureArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host '[host-sdl3] build'
cmake --build $BuildDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host '[host-sdl3] test'
ctest --test-dir $BuildDir --output-on-failure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host '[host-sdl3] ok'
