param(
    [string]$BuildDir = '',
    [string]$Sdl3SourceDir = '',
    [ValidateRange(1, 10000)]
    [int]$Repeat = 100,
    [ValidateRange(1, 1000000)]
    [int]$Frames = 30,
    [ValidateRange(0, 100000)]
    [int]$EventBurst = 64,
    [switch]$NoFetch
)

$ErrorActionPreference = 'Stop'
$OutputEncoding = [Console]::OutputEncoding = [Text.UTF8Encoding]::new()

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '../..')
$sourceDir = Join-Path $repoRoot 'Backends/host/sdl3_smoke'
$pinnedRepository = 'https://github.com/libsdl-org/SDL.git'
$pinnedTag = 'release-3.2.8'
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $sourceDir 'cmake-build-backends-host-sdl3-stability'
}

$fetchEnabled = if ($NoFetch -or -not [string]::IsNullOrWhiteSpace($Sdl3SourceDir)) {
    'OFF'
} else {
    'ON'
}
$configureArgs = @(
    '-S', $sourceDir,
    '-B', $BuildDir,
    '-G', 'Ninja',
    '-DCHARM_USE_SYSTEM_SDL3=ON',
    "-DCHARM_SDL3_GIT_REPOSITORY=$pinnedRepository",
    "-DCHARM_SDL3_GIT_TAG=$pinnedTag",
    '-UFETCHCONTENT_SOURCE_DIR_SDL3',
    '-UFETCHCONTENT_FULLY_DISCONNECTED',
    '-UFETCHCONTENT_UPDATES_DISCONNECTED',
    '-UFETCHCONTENT_UPDATES_DISCONNECTED_SDL3',
    "-DCHARM_FETCHCONTENT_SDL3=$fetchEnabled"
)
if ([string]::IsNullOrWhiteSpace($Sdl3SourceDir)) {
    $configureArgs += '-DCHARM_SDL3_SOURCE_DIR='
} else {
    $resolvedSdl3 = (Resolve-Path -LiteralPath $Sdl3SourceDir).Path
    $configureArgs += "-DCHARM_SDL3_SOURCE_DIR=$resolvedSdl3"
}

Write-Host '[host-sdl3-stability-gate] configure'
& cmake @configureArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$cachePath = Join-Path $BuildDir 'CMakeCache.txt'
$originMatch = Select-String -Path $cachePath -Pattern '^CHARM_SDL3_RESOLVED_ORIGIN:INTERNAL=(.*)$'
$versionMatch = Select-String -Path $cachePath -Pattern '^CHARM_SDL3_RESOLVED_VERSION:INTERNAL=(.*)$'
if (-not $originMatch -or -not $versionMatch) {
    throw 'SDL3 dependency resolution was not recorded in CMakeCache.txt.'
}
$origin = $originMatch.Matches[0].Groups[1].Value
$version = $versionMatch.Matches[0].Groups[1].Value
if (-not [string]::IsNullOrWhiteSpace($Sdl3SourceDir)) {
    $expectedOrigin = 'source:' + ($resolvedSdl3 -replace '\\', '/')
    if ($origin -ne $expectedOrigin) {
        throw "Explicit SDL3 source was not selected: expected=$expectedOrigin actual=$origin"
    }
}
if ($NoFetch -and $origin.StartsWith('fetch:', [StringComparison]::Ordinal)) {
    throw "SDL3 was fetched while -NoFetch was active: $origin"
}
if ($origin.StartsWith('fetch:', [StringComparison]::Ordinal) -and
    -not $origin.StartsWith("fetch:$pinnedTag@", [StringComparison]::Ordinal)) {
    throw "SDL3 fetch did not prove the pinned revision: expected=fetch:$pinnedTag@<commit> actual=$origin"
}

Write-Host '[host-sdl3-stability-gate] build'
cmake --build $BuildDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host '[host-sdl3-stability-gate] functional smoke'
ctest --test-dir $BuildDir --output-on-failure -R '^backends_host_sdl3_smoke$'
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$executableName = if ($env:OS -eq 'Windows_NT') {
    'backends-host-sdl3-stability-smoke.exe'
} else {
    'backends-host-sdl3-stability-smoke'
}
$executable = Join-Path $BuildDir $executableName
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "Host SDL3 stability executable is missing: $executable"
}

Write-Host '[host-sdl3-stability-gate] stress'
$runArgs = @(
    "--repeat=$Repeat",
    "--frames=$Frames",
    "--event-burst=$EventBurst"
)
& $executable @runArgs 2>&1 |
    Tee-Object -Variable capturedOutput |
    ForEach-Object { Write-Host "$_" }
$runExit = $LASTEXITCODE
if ($runExit -ne 0) { exit $runExit }

$summary = $capturedOutput | Where-Object { "$_" -match '^\[host-sdl3-stability\]' }
if (-not $summary) {
    throw 'Host SDL3 stability summary is missing.'
}
Write-Host ("[host-sdl3-stability-gate] dependency_origin={0} dependency_version={1} fetch={2}" -f
    $origin, $version, $fetchEnabled)
Write-Host '[host-sdl3-stability-gate] ok'
