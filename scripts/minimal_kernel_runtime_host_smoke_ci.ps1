param(
    [string]$CMakeExe = "cmake",
    [string]$Generator = "Ninja",
    [string]$SummaryPath = "",
    [int]$Jobs = 0,
    [switch]$KeepBuildDirs,
    [switch]$StopOnFailure,
    [string[]]$Examples
)

$ErrorActionPreference = "Stop"

$rootScript = Join-Path $PSScriptRoot "minimal_kernel_runtime_host_smoke.ps1"
if (-not (Test-Path $rootScript)) {
    throw "missing host smoke script: $rootScript"
}

$invokeArgs = @{
    CMakeExe  = $CMakeExe
    Generator = $Generator
    Fresh     = $true
    Jobs      = $Jobs
}

if (-not [string]::IsNullOrWhiteSpace($SummaryPath)) {
    $invokeArgs.SummaryPath = $SummaryPath
}

if ($KeepBuildDirs) {
    $invokeArgs.KeepBuildDirs = $true
}

if ($StopOnFailure) {
    $invokeArgs.StopOnFailure = $true
}

if ($PSBoundParameters.ContainsKey("Examples")) {
    $invokeArgs.Examples = $Examples
}

& $rootScript @invokeArgs
exit $LASTEXITCODE
