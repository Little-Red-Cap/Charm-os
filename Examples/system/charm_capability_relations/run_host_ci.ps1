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
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $scriptRoot 'cmake-build-charm-capability-relations'
}

$coreHeader = Join-Path $scriptRoot '../../../Modules/core/capability/relations.hpp'
$coreText = Get-Content -LiteralPath $coreHeader -Raw -Encoding UTF8
$forbidden = [regex]::Match(
    $coreText,
    '(?i)(string(_view)?|provider|backend|adapter|contextview|registry|resolver|profile|resolvedbinding|resolutionfailure|void\s*\*|span\s*<|tuple\s*<)')
if ($forbidden.Success) {
    throw "core_relation_boundary_violation: token=$($forbidden.Value)"
}

$profiles = if ($Profile -eq 'all') { @('clang', 'gcc') } else { @($Profile) }
$negativeTargets = @(
    'charm-capability-relations-negative-same_requirement_key',
    'charm-capability-relations-negative-same_provision_key',
    'charm-capability-relations-negative-same_binding_key'
)

function Resolve-Compiler([string]$Name) {
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -eq $command) {
        throw "compiler_missing: $Name"
    }
    return $command.Source
}

foreach ($name in $profiles) {
    $compilerName = if ($name -eq 'clang') { 'clang++' } else { 'g++' }
    $compiler = Resolve-Compiler -Name $compilerName
    & cmake --fresh -S $scriptRoot -B $BuildDir -G Ninja "-DCMAKE_CXX_COMPILER=$compiler"
    if ($LASTEXITCODE -ne 0) { throw "configure_failed: profile=$name" }
    & cmake --build $BuildDir
    if ($LASTEXITCODE -ne 0) { throw "build_failed: profile=$name" }
    & ctest --test-dir $BuildDir --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "ctest_failed: profile=$name" }
    $targetHelp = (& cmake --build $BuildDir --target help) -join "`n"
    if ($LASTEXITCODE -ne 0) { throw "target_help_failed: profile=$name" }
    foreach ($target in $negativeTargets) {
        if (-not $targetHelp.Contains("${target}: phony")) {
            throw "compile_rejection_target_missing: profile=$name target=$target"
        }
        & cmake --build $BuildDir --target $target *> $null
        if ($LASTEXITCODE -eq 0) {
            throw "compile_rejection_failed: profile=$name target=$target"
        }
        Write-Output "[charm-capability-relations-ci] profile=$name target=$target status=rejected"
    }
    Write-Output "[charm-capability-relations-ci] profile=$name status=ok"
}

Write-Output '[charm-capability-relations-ci] ok'
