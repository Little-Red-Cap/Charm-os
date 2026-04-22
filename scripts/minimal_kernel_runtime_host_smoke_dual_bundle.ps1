param(
    [string]$OutputRoot = "",
    [string]$CiOutputRoot = "",
    [string]$DailyOutputRoot = "",
    [string]$CiReportTitle = "Minimal Kernel Host Smoke Cold Start Report",
    [string]$DailyReportTitle = "Minimal Kernel Host Smoke Warm Reuse Report",
    [string]$CMakeExe = "cmake",
    [string]$Generator = "Ninja",
    [int]$Jobs = 0,
    [int]$Top = 10,
    [switch]$Clean,
    [switch]$StopOnFailure,
    [string[]]$Examples,
    [int]$MaxFailures = 0,
    [int]$MaxOtherResults = 0,
    [int64]$MaxTotalElapsedMs = -1,
    [int64]$MaxAverageElapsedMs = -1,
    [int64]$MaxMaxElapsedMs = -1,
    [int]$MaxRegressionCount = -1,
    [int64]$MaxRegressionMs = -1,
    [double]$MaxRegressionPct = -1,
    [switch]$AllowAddedExamples,
    [switch]$AllowRemovedExamples
)

$ErrorActionPreference = "Stop"

function Resolve-FullPath {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ""
    }

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

$ciScript = Join-Path $PSScriptRoot "ci_minimal_kernel_runtime_host_smoke_bundle.ps1"
$dailyScript = Join-Path $PSScriptRoot "daily_minimal_kernel_runtime_host_smoke_bundle.ps1"
foreach ($scriptPath in @($ciScript, $dailyScript)) {
    if (-not (Test-Path $scriptPath)) {
        throw "missing host smoke dual bundle dependency: $scriptPath"
    }
}

$resolvedBaseOutputRoot = if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    Resolve-FullPath -Path "out/minimal-kernel-runtime-host-smoke"
} else {
    Resolve-FullPath -Path $OutputRoot
}

$resolvedCiOutputRoot = if ([string]::IsNullOrWhiteSpace($CiOutputRoot)) {
    Join-Path $resolvedBaseOutputRoot "ci"
} else {
    Resolve-FullPath -Path $CiOutputRoot
}

$resolvedDailyOutputRoot = if ([string]::IsNullOrWhiteSpace($DailyOutputRoot)) {
    Join-Path $resolvedBaseOutputRoot "daily"
} else {
    Resolve-FullPath -Path $DailyOutputRoot
}

$ciSummaryPath = Join-Path $resolvedCiOutputRoot "summary.json"

$commonArgs = @{
    CMakeExe           = $CMakeExe
    Generator          = $Generator
    Jobs               = $Jobs
    Top                = $Top
    MaxFailures        = $MaxFailures
    MaxOtherResults    = $MaxOtherResults
    MaxTotalElapsedMs  = $MaxTotalElapsedMs
    MaxAverageElapsedMs = $MaxAverageElapsedMs
    MaxMaxElapsedMs    = $MaxMaxElapsedMs
}

if ($StopOnFailure) {
    $commonArgs.StopOnFailure = $true
}
if ($PSBoundParameters.ContainsKey("Examples")) {
    $commonArgs.Examples = $Examples
}
$ciArgs = @{}
foreach ($entry in $commonArgs.GetEnumerator()) {
    $ciArgs[$entry.Key] = $entry.Value
}
$ciArgs.OutputRoot = $resolvedCiOutputRoot
$ciArgs.ReportTitle = $CiReportTitle
if ($Clean) {
    $ciArgs.Clean = $true
}

$dailyArgs = @{}
foreach ($entry in $commonArgs.GetEnumerator()) {
    $dailyArgs[$entry.Key] = $entry.Value
}
$dailyArgs.OutputRoot = $resolvedDailyOutputRoot
$dailyArgs.BaselineSummary = $ciSummaryPath
$dailyArgs.ReportTitle = $DailyReportTitle
$dailyArgs.MaxRegressionCount = $MaxRegressionCount
$dailyArgs.MaxRegressionMs = $MaxRegressionMs
$dailyArgs.MaxRegressionPct = $MaxRegressionPct
if ($AllowAddedExamples) {
    $dailyArgs.AllowAddedExamples = $true
}
if ($AllowRemovedExamples) {
    $dailyArgs.AllowRemovedExamples = $true
}
if ($Clean) {
    $dailyArgs.Clean = $true
}

Write-Host "==> minimal kernel host smoke dual bundle"
Write-Host ("output_root={0}" -f $resolvedBaseOutputRoot)
Write-Host ("ci_output_root={0}" -f $resolvedCiOutputRoot)
Write-Host ("daily_output_root={0}" -f $resolvedDailyOutputRoot)

& $ciScript @ciArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $dailyScript @dailyArgs
exit $LASTEXITCODE
