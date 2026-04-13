param(
    [string]$Source = "Examples/init/materialize_observe_demo",
    [string]$BuildDir = "cmake-build-init-observe-demo-clang",
    [string]$CCompiler = "clang",
    [string]$CxxCompiler = "clang++",
    [string]$BuildTarget = "init-materialize-observe-demo",
    [string]$ExportTarget = "export_materialized_graph_demo",
    [string]$Dot = "",
    [string]$Json = "",
    [int]$Jobs = 8,
    [switch]$Clean,
    [switch]$ConfigureOnly
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$sourceDir = Join-Path $repoRoot $Source
$buildDir = Join-Path $repoRoot $BuildDir

if (-not (Test-Path $sourceDir)) {
    throw "source not found: $sourceDir"
}

if ($Clean -and (Test-Path $buildDir)) {
    Remove-Item -Recurse -Force $buildDir
}

Write-Host "[CFG] $sourceDir"
cmake -S $sourceDir -B $buildDir -G Ninja -D CMAKE_C_COMPILER=$CCompiler -D CMAKE_CXX_COMPILER=$CxxCompiler
if ($LASTEXITCODE -ne 0) {
    throw "configure failed"
}

if ($ConfigureOnly) {
    Write-Host "[OK] configure finished: $buildDir"
    exit 0
}

if ([string]::IsNullOrWhiteSpace($Dot) -and [string]::IsNullOrWhiteSpace($Json)) {
    Write-Host "[EXPORT] target=$ExportTarget"
    cmake --build $buildDir --target $ExportTarget -j $Jobs
    if ($LASTEXITCODE -ne 0) {
        throw "export target failed"
    }

    Write-Host "[OK] exported via target"
    Write-Host "[DOT]  $(Join-Path $buildDir 'materialized_graph.dot')"
    Write-Host "[JSON] $(Join-Path $buildDir 'materialized_graph.sample.json')"
    exit 0
}

Write-Host "[BUILD] target=$BuildTarget"
cmake --build $buildDir --target $BuildTarget -j $Jobs
if ($LASTEXITCODE -ne 0) {
    throw "build failed"
}

$exePath = Join-Path $buildDir "$BuildTarget.exe"
if (-not (Test-Path $exePath)) {
    throw "executable not found: $exePath"
}

$args = @()
if (-not [string]::IsNullOrWhiteSpace($Dot)) {
    $args += @("--dot", $Dot)
}
if (-not [string]::IsNullOrWhiteSpace($Json)) {
    $args += @("--json", $Json)
}

Write-Host "[RUN] $exePath $($args -join ' ')"
& $exePath @args
if ($LASTEXITCODE -ne 0) {
    throw "export run failed"
}

Write-Host "[OK] exported via direct run"
