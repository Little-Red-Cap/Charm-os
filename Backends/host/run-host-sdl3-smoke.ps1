param(
    [string]$BuildDir = '',
    [string]$Sdl3SourceDir = ''
)

$ErrorActionPreference = 'Stop'
$OutputEncoding = [Console]::OutputEncoding = [Text.UTF8Encoding]::new()

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '../..')
$sourceDir = Join-Path $repoRoot 'Backends/host/sdl3_smoke'
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $sourceDir 'cmake-build-backends-host-sdl3-smoke'
}

$configureArgs = @('-S', $sourceDir, '-B', $BuildDir, '-G', 'Ninja')
if (-not [string]::IsNullOrWhiteSpace($Sdl3SourceDir)) {
    $resolvedSdl3 = (Resolve-Path -LiteralPath $Sdl3SourceDir).Path
    $configureArgs += "-DCHARM_SDL3_SOURCE_DIR=$resolvedSdl3"
    $configureArgs += '-DCHARM_FETCHCONTENT_SDL3=OFF'
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
