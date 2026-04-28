param(
    [string]$OutputRoot = "",
    [string]$SummaryPath = "",
    [string]$ReportMarkdownPath = "",
    [string]$CheckTextPath = "",
    [string]$CanonicalWorld = "",
    [string]$WitnessBundleOutputRoot = "",
    [string]$WitnessBundleSummaryPath = "",
    [string]$WitnessBundleReportMarkdownPath = "",
    [string]$WitnessBundleCheckTextPath = "",
    [string]$HostOutputRoot = "",
    [string]$QemuOutputRoot = "",
    [string]$CMakeExe = "cmake",
    [string]$Generator = "Ninja",
    [string]$QemuExe = "qemu-system-arm",
    [int]$HostJobs = 0,
    [int]$QemuBuildJobs = 1,
    [int]$QemuTimeoutSec = 30,
    [int]$QemuTailLines = 40,
    [switch]$Clean,
    [switch]$SkipWitnessBundle,
    [string[]]$HostExamples
)

$ErrorActionPreference = "Stop"

$rootScript = Join-Path $PSScriptRoot "minimal_kernel_runtime_evidence_bundle.ps1"
if (-not (Test-Path $rootScript)) {
    throw "missing runtime evidence bundle script: $rootScript"
}

$invokeArgs = @{
    CMakeExe       = $CMakeExe
    Generator      = $Generator
    QemuExe        = $QemuExe
    HostJobs       = $HostJobs
    QemuBuildJobs  = $QemuBuildJobs
    QemuTimeoutSec = $QemuTimeoutSec
    QemuTailLines  = $QemuTailLines
}

if (-not [string]::IsNullOrWhiteSpace($OutputRoot)) {
    $invokeArgs.OutputRoot = $OutputRoot
}
if (-not [string]::IsNullOrWhiteSpace($SummaryPath)) {
    $invokeArgs.SummaryPath = $SummaryPath
}
if (-not [string]::IsNullOrWhiteSpace($ReportMarkdownPath)) {
    $invokeArgs.ReportMarkdownPath = $ReportMarkdownPath
}
if (-not [string]::IsNullOrWhiteSpace($CheckTextPath)) {
    $invokeArgs.CheckTextPath = $CheckTextPath
}
if (-not [string]::IsNullOrWhiteSpace($CanonicalWorld)) {
    $invokeArgs.CanonicalWorld = $CanonicalWorld
}
if (-not [string]::IsNullOrWhiteSpace($WitnessBundleOutputRoot)) {
    $invokeArgs.WitnessBundleOutputRoot = $WitnessBundleOutputRoot
}
if (-not [string]::IsNullOrWhiteSpace($WitnessBundleSummaryPath)) {
    $invokeArgs.WitnessBundleSummaryPath = $WitnessBundleSummaryPath
}
if (-not [string]::IsNullOrWhiteSpace($WitnessBundleReportMarkdownPath)) {
    $invokeArgs.WitnessBundleReportMarkdownPath = $WitnessBundleReportMarkdownPath
}
if (-not [string]::IsNullOrWhiteSpace($WitnessBundleCheckTextPath)) {
    $invokeArgs.WitnessBundleCheckTextPath = $WitnessBundleCheckTextPath
}
if (-not [string]::IsNullOrWhiteSpace($HostOutputRoot)) {
    $invokeArgs.HostOutputRoot = $HostOutputRoot
}
if (-not [string]::IsNullOrWhiteSpace($QemuOutputRoot)) {
    $invokeArgs.QemuOutputRoot = $QemuOutputRoot
}
if ($Clean) {
    $invokeArgs.Clean = $true
}
if ($SkipWitnessBundle) {
    $invokeArgs.SkipWitnessBundle = $true
}
if ($PSBoundParameters.ContainsKey("HostExamples")) {
    $invokeArgs.HostExamples = $HostExamples
}

& $rootScript @invokeArgs
