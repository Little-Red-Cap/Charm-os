[CmdletBinding()]
param(
    [string]$BuildDir = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$OutputEncoding = [Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $scriptRoot 'cmake-build-charm-core-arm-build-only'
}
$buildDirFull = [IO.Path]::GetFullPath($BuildDir)
$compilerCommand = Get-Command arm-none-eabi-g++ -ErrorAction SilentlyContinue
if ($null -eq $compilerCommand) {
    throw 'compiler_missing: arm-none-eabi-g++'
}
$compiler = $compilerCommand.Source

& $compiler --version | Select-Object -First 1
& cmake --fresh -S $scriptRoot -B $buildDirFull -G Ninja `
    "-DCMAKE_SYSTEM_NAME=Generic" `
    "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY" `
    "-DCMAKE_CXX_COMPILER=$compiler"
if ($LASTEXITCODE -ne 0) {
    throw "configure_failed: exit=$LASTEXITCODE"
}
& cmake --build $buildDirFull --target charm-core-arm-build-only
if ($LASTEXITCODE -ne 0) {
    throw "build_failed: exit=$LASTEXITCODE"
}

$objectRoot = Join-Path $buildDirFull 'CMakeFiles/charm-core-arm-build-only.dir'
$objects = @(Get-ChildItem -LiteralPath $objectRoot -Recurse -File |
    Where-Object { $_.Extension -in @('.o', '.obj') })
if ($objects.Count -eq 0) {
    throw 'build_evidence_missing: object file'
}
Write-Output "[charm-core-arm-build-only] ok objects=$($objects.Count)"
