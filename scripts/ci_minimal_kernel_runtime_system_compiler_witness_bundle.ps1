param(
    [string]$CanonicalWorld = "",
    [string]$RuntimeEvidenceSummary = "",
    [string]$RuntimeEvidenceOutputRoot = "",
    [string]$RuntimeEvidenceValidationLogPath = "",
    [string]$BaselineRuntimeEvidenceSummary = "",
    [string]$BaselineRuntimeEvidenceOutputRoot = "",
    [string]$ArtifactRoot = "",
    [string[]]$ArtifactReport = @(),
    [string[]]$Case = @(),
    [string]$BaselineArtifactRoot = "",
    [string[]]$BaselineArtifactReport = @(),
    [string[]]$BaselineCase = @(),
    [string]$BaselineWitnessSummary = "",
    [string]$BaselineWitnessOutputRoot = "",
    [string]$OutputRoot = "",
    [string]$SummaryPath = "",
    [string]$ReportMarkdownPath = "",
    [string]$CheckTextPath = "",
    [string]$ValidationLogPath = "",
    [string]$BiographySummaryPath = "",
    [string]$BiographyReportMarkdownPath = "",
    [string]$BiographyCheckTextPath = "",
    [string]$BiographyValidationLogPath = "",
    [string]$WorldCompareOutputRoot = "",
    [string]$WorldCompareSummaryPath = "",
    [string]$WorldCompareReportMarkdownPath = "",
    [string]$WorldCompareCheckTextPath = "",
    [string]$WorldCompareValidationLogPath = "",
    [string]$CMakeExe = "cmake",
    [string]$Generator = "Ninja",
    [string]$QemuExe = "qemu-system-arm",
    [string]$PythonExe = "",
    [int]$HostJobs = 0,
    [int]$QemuBuildJobs = 1,
    [int]$QemuTimeoutSec = 30,
    [int]$QemuTailLines = 40,
    [switch]$Clean,
    [switch]$SkipWorldCompare,
    [string[]]$HostExamples
)

$ErrorActionPreference = "Stop"

$rootScript = Join-Path $PSScriptRoot "minimal_kernel_runtime_system_compiler_witness_bundle.ps1"
if (-not (Test-Path $rootScript)) {
    throw "missing system compiler witness bundle script: $rootScript"
}

$invokeArgs = @{
    CMakeExe       = $CMakeExe
    Generator      = $Generator
    QemuExe        = $QemuExe
    PythonExe      = $PythonExe
    HostJobs       = $HostJobs
    QemuBuildJobs  = $QemuBuildJobs
    QemuTimeoutSec = $QemuTimeoutSec
    QemuTailLines  = $QemuTailLines
}

if (-not [string]::IsNullOrWhiteSpace($CanonicalWorld)) {
    $invokeArgs.CanonicalWorld = $CanonicalWorld
}
if (-not [string]::IsNullOrWhiteSpace($RuntimeEvidenceSummary)) {
    $invokeArgs.RuntimeEvidenceSummary = $RuntimeEvidenceSummary
}
if (-not [string]::IsNullOrWhiteSpace($RuntimeEvidenceOutputRoot)) {
    $invokeArgs.RuntimeEvidenceOutputRoot = $RuntimeEvidenceOutputRoot
}
if (-not [string]::IsNullOrWhiteSpace($RuntimeEvidenceValidationLogPath)) {
    $invokeArgs.RuntimeEvidenceValidationLogPath = $RuntimeEvidenceValidationLogPath
}
if (-not [string]::IsNullOrWhiteSpace($BaselineRuntimeEvidenceSummary)) {
    $invokeArgs.BaselineRuntimeEvidenceSummary = $BaselineRuntimeEvidenceSummary
}
if (-not [string]::IsNullOrWhiteSpace($BaselineRuntimeEvidenceOutputRoot)) {
    $invokeArgs.BaselineRuntimeEvidenceOutputRoot = $BaselineRuntimeEvidenceOutputRoot
}
if (-not [string]::IsNullOrWhiteSpace($ArtifactRoot)) {
    $invokeArgs.ArtifactRoot = $ArtifactRoot
}
if ($PSBoundParameters.ContainsKey("ArtifactReport")) {
    $invokeArgs.ArtifactReport = $ArtifactReport
}
if ($PSBoundParameters.ContainsKey("Case")) {
    $invokeArgs.Case = $Case
}
if (-not [string]::IsNullOrWhiteSpace($BaselineArtifactRoot)) {
    $invokeArgs.BaselineArtifactRoot = $BaselineArtifactRoot
}
if ($PSBoundParameters.ContainsKey("BaselineArtifactReport")) {
    $invokeArgs.BaselineArtifactReport = $BaselineArtifactReport
}
if ($PSBoundParameters.ContainsKey("BaselineCase")) {
    $invokeArgs.BaselineCase = $BaselineCase
}
if (-not [string]::IsNullOrWhiteSpace($BaselineWitnessSummary)) {
    $invokeArgs.BaselineWitnessSummary = $BaselineWitnessSummary
}
if (-not [string]::IsNullOrWhiteSpace($BaselineWitnessOutputRoot)) {
    $invokeArgs.BaselineWitnessOutputRoot = $BaselineWitnessOutputRoot
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
if (-not [string]::IsNullOrWhiteSpace($ValidationLogPath)) {
    $invokeArgs.ValidationLogPath = $ValidationLogPath
}
if (-not [string]::IsNullOrWhiteSpace($BiographySummaryPath)) {
    $invokeArgs.BiographySummaryPath = $BiographySummaryPath
}
if (-not [string]::IsNullOrWhiteSpace($BiographyReportMarkdownPath)) {
    $invokeArgs.BiographyReportMarkdownPath = $BiographyReportMarkdownPath
}
if (-not [string]::IsNullOrWhiteSpace($BiographyCheckTextPath)) {
    $invokeArgs.BiographyCheckTextPath = $BiographyCheckTextPath
}
if (-not [string]::IsNullOrWhiteSpace($BiographyValidationLogPath)) {
    $invokeArgs.BiographyValidationLogPath = $BiographyValidationLogPath
}
if (-not [string]::IsNullOrWhiteSpace($WorldCompareOutputRoot)) {
    $invokeArgs.WorldCompareOutputRoot = $WorldCompareOutputRoot
}
if (-not [string]::IsNullOrWhiteSpace($WorldCompareSummaryPath)) {
    $invokeArgs.WorldCompareSummaryPath = $WorldCompareSummaryPath
}
if (-not [string]::IsNullOrWhiteSpace($WorldCompareReportMarkdownPath)) {
    $invokeArgs.WorldCompareReportMarkdownPath = $WorldCompareReportMarkdownPath
}
if (-not [string]::IsNullOrWhiteSpace($WorldCompareCheckTextPath)) {
    $invokeArgs.WorldCompareCheckTextPath = $WorldCompareCheckTextPath
}
if (-not [string]::IsNullOrWhiteSpace($WorldCompareValidationLogPath)) {
    $invokeArgs.WorldCompareValidationLogPath = $WorldCompareValidationLogPath
}
if ($Clean) {
    $invokeArgs.Clean = $true
}
if ($SkipWorldCompare) {
    $invokeArgs.SkipWorldCompare = $true
}
if ($PSBoundParameters.ContainsKey("HostExamples")) {
    $invokeArgs.HostExamples = $HostExamples
}

& $rootScript @invokeArgs
