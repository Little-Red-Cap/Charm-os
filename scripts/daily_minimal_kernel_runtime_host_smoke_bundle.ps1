param(
    [string]$OutputRoot = "",
    [string]$SummaryPath = "",
    [string]$SmokeLogPath = "",
    [string]$InspectTextPath = "",
    [string]$InspectJsonPath = "",
    [string]$ReportMarkdownPath = "",
    [string]$ReportTitle = "",
    [string]$CheckTextPath = "",
    [string]$BaselineSummary = "",
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

$rootScript = Join-Path $PSScriptRoot "minimal_kernel_runtime_host_smoke_bundle.ps1"
if (-not (Test-Path $rootScript)) {
    throw "missing host smoke bundle script: $rootScript"
}

$invokeArgs = @{
    Profile             = "daily"
    CMakeExe            = $CMakeExe
    Generator           = $Generator
    Jobs                = $Jobs
    Top                 = $Top
    MaxFailures         = $MaxFailures
    MaxOtherResults     = $MaxOtherResults
    MaxTotalElapsedMs   = $MaxTotalElapsedMs
    MaxAverageElapsedMs = $MaxAverageElapsedMs
    MaxMaxElapsedMs     = $MaxMaxElapsedMs
    MaxRegressionCount  = $MaxRegressionCount
    MaxRegressionMs     = $MaxRegressionMs
    MaxRegressionPct    = $MaxRegressionPct
}

if (-not [string]::IsNullOrWhiteSpace($OutputRoot)) {
    $invokeArgs.OutputRoot = $OutputRoot
}
if (-not [string]::IsNullOrWhiteSpace($SummaryPath)) {
    $invokeArgs.SummaryPath = $SummaryPath
}
if (-not [string]::IsNullOrWhiteSpace($SmokeLogPath)) {
    $invokeArgs.SmokeLogPath = $SmokeLogPath
}
if (-not [string]::IsNullOrWhiteSpace($InspectTextPath)) {
    $invokeArgs.InspectTextPath = $InspectTextPath
}
if (-not [string]::IsNullOrWhiteSpace($InspectJsonPath)) {
    $invokeArgs.InspectJsonPath = $InspectJsonPath
}
if (-not [string]::IsNullOrWhiteSpace($ReportMarkdownPath)) {
    $invokeArgs.ReportMarkdownPath = $ReportMarkdownPath
}
if (-not [string]::IsNullOrWhiteSpace($ReportTitle)) {
    $invokeArgs.ReportTitle = $ReportTitle
}
if (-not [string]::IsNullOrWhiteSpace($CheckTextPath)) {
    $invokeArgs.CheckTextPath = $CheckTextPath
}
if (-not [string]::IsNullOrWhiteSpace($BaselineSummary)) {
    $invokeArgs.BaselineSummary = $BaselineSummary
}
if ($Clean) {
    $invokeArgs.Clean = $true
}
if ($StopOnFailure) {
    $invokeArgs.StopOnFailure = $true
}
if ($PSBoundParameters.ContainsKey("Examples")) {
    $invokeArgs.Examples = $Examples
}
if ($AllowAddedExamples) {
    $invokeArgs.AllowAddedExamples = $true
}
if ($AllowRemovedExamples) {
    $invokeArgs.AllowRemovedExamples = $true
}

& $rootScript @invokeArgs
