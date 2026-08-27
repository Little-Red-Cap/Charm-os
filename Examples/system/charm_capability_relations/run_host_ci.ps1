[CmdletBinding()]
param(
    [ValidateSet('clang', 'gcc', 'all')]
    [string]$Profile = 'all',
    [string]$BuildDir = (Join-Path $PSScriptRoot 'cmake-build-charm-capability-relations')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$OutputEncoding = [Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)

$coreHeader = Join-Path $PSScriptRoot '../../../Modules/core/capability/relations.hpp'
$coreText = Get-Content -LiteralPath $coreHeader -Raw -Encoding UTF8
$forbidden = [regex]::Match(
    $coreText,
    '(?i)(string(_view)?|provider|backend|adapter|contextview|registry|resolver|profile|void\s*\*|span\s*<|tuple\s*<)')
if ($forbidden.Success) {
    throw "core_relation_boundary_violation: token=$($forbidden.Value)"
}

$profiles = if ($Profile -eq 'all') { @('clang', 'gcc') } else { @($Profile) }
foreach ($name in $profiles) {
    $compiler = if ($name -eq 'clang') { 'D:/Toolchains/LLVM/bin/clang++.exe' } else { 'D:/Toolchains/w64devkit/bin/g++.exe' }
    & cmake --fresh -S $PSScriptRoot -B $BuildDir -G Ninja "-DCMAKE_CXX_COMPILER=$compiler"
    if ($LASTEXITCODE -ne 0) { throw "configure_failed: profile=$name" }
    & cmake --build $BuildDir
    if ($LASTEXITCODE -ne 0) { throw "build_failed: profile=$name" }
    & ctest --test-dir $BuildDir --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "ctest_failed: profile=$name" }
    Write-Output "[charm-capability-relations-ci] profile=$name status=ok"
}

Write-Output '[charm-capability-relations-ci] ok'
