[CmdletBinding()]
param(
    [ValidateSet('clang', 'gcc', 'all')]
    [string]$Profile = 'clang',
    [string]$BuildDir = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$OutputEncoding = [Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = [IO.Path]::GetFullPath((Join-Path $scriptRoot '../../..'))
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $scriptRoot 'cmake-build-charm-core-external-consumer'
}
$buildRoot = [IO.Path]::GetFullPath($BuildDir)
$producerBuild = Join-Path $buildRoot 'producer'
$consumerBuild = Join-Path $buildRoot 'consumer'
$installDir = Join-Path $buildRoot 'install'

function Resolve-Compiler([string]$Name) {
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -eq $command) {
        throw "compiler_missing: $Name"
    }
    return $command.Source
}

$profiles = if ($Profile -eq 'all') { @('clang', 'gcc') } else { @($Profile) }
foreach ($name in $profiles) {
    $compilerName = if ($name -eq 'clang') { 'clang++' } else { 'g++' }
    $compiler = Resolve-Compiler -Name $compilerName

    & cmake --fresh -S $repoRoot -B $producerBuild -G Ninja `
        "-DCMAKE_BUILD_TYPE=Release" `
        "-DCMAKE_CXX_COMPILER=$compiler" `
        "-DCMAKE_INSTALL_PREFIX=$installDir"
    if ($LASTEXITCODE -ne 0) { throw "producer_configure_failed: profile=$name" }
    & cmake --build $producerBuild
    if ($LASTEXITCODE -ne 0) { throw "producer_build_failed: profile=$name" }
    & cmake --install $producerBuild
    if ($LASTEXITCODE -ne 0) { throw "producer_install_failed: profile=$name" }

    $installedHeader = Join-Path $installDir 'include/core/capability/relations.hpp'
    if (-not (Test-Path -LiteralPath $installedHeader -PathType Leaf)) {
        throw "installed_header_missing: $installedHeader"
    }
    $config = Get-ChildItem -LiteralPath $installDir -Filter CharmCoreConfig.cmake -File -Recurse |
        Select-Object -First 1
    if ($null -eq $config) {
        throw 'installed_config_missing: CharmCoreConfig.cmake'
    }
    $configText = Get-Content -LiteralPath $config.FullName -Raw -Encoding UTF8
    $repoRootUnix = $repoRoot.Replace('\', '/')
    if ($configText.Contains($repoRoot) -or $configText.Contains($repoRootUnix)) {
        throw 'installed_config_leaks_source_root'
    }

    & cmake --fresh -S $scriptRoot -B $consumerBuild -G Ninja `
        "-DCMAKE_BUILD_TYPE=Release" `
        "-DCMAKE_CXX_COMPILER=$compiler" `
        "-DCMAKE_PREFIX_PATH=$installDir"
    if ($LASTEXITCODE -ne 0) { throw "consumer_configure_failed: profile=$name" }
    & cmake --build $consumerBuild
    if ($LASTEXITCODE -ne 0) { throw "consumer_build_failed: profile=$name" }
    & ctest --test-dir $consumerBuild --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "consumer_ctest_failed: profile=$name" }
    Write-Output "[charm-core-external-consumer-ci] profile=$name status=ok"
}

Write-Output '[charm-core-external-consumer-ci] ok'
