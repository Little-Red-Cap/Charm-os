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
    [switch]$RunWorldShelfFlow,
    [string]$WorldShelfOutputRoot = "",
    [string]$WorldShelfCompareOutputRoot = "",
    [string]$WorldShelfProfile = "minimal-kernel-runtime-system-compiler-world-compare-shelf",
    [string[]]$HostExamples
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

function Ensure-ParentDirectory {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    $parent = Split-Path -Parent $Path
    if ([string]::IsNullOrWhiteSpace($parent)) {
        return
    }

    if (-not (Test-Path $parent)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
}

function Get-OutputPath {
    param(
        [string]$ExplicitPath,
        [string]$OutputRootPath,
        [string]$DefaultFileName
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        return Resolve-FullPath -Path $ExplicitPath
    }

    return Resolve-FullPath -Path (Join-Path $OutputRootPath $DefaultFileName)
}

function Append-Utf8Text {
    param(
        [string]$Path,
        [string]$Text
    )

    Ensure-ParentDirectory -Path $Path
    $existing = if (Test-Path $Path) {
        Get-Content -LiteralPath $Path -Raw -Encoding utf8
    } else {
        ""
    }

    Set-Content -LiteralPath $Path -Encoding utf8 -Value ($existing + $Text)
}

function Load-JsonObject {
    param(
        [string]$Path
    )

    return (Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json)
}

function Append-WorldShelfReviewOverlay {
    param(
        [string]$ReportPath,
        [string]$CheckTextPath,
        [string]$ReviewSummaryPath,
        [string]$ReviewReportPath,
        [string]$ReviewCheckPath,
        [string]$CandidateShelfSummaryPath,
        [string]$CompareSummaryPath
    )

    foreach ($requiredPath in @($ReviewSummaryPath, $CandidateShelfSummaryPath, $ReviewReportPath, $ReviewCheckPath, $CompareSummaryPath)) {
        if (-not (Test-Path $requiredPath)) {
            throw "world shelf overlay input not found: $requiredPath"
        }
    }

    $reviewSummary = Load-JsonObject -Path $ReviewSummaryPath
    $candidateSummary = Load-JsonObject -Path $CandidateShelfSummaryPath
    $compareSummary = Load-JsonObject -Path $CompareSummaryPath

    $reportBuilder = [System.Text.StringBuilder]::new()
    [void]$reportBuilder.AppendLine("")
    [void]$reportBuilder.AppendLine("## World Shelf Review")
    [void]$reportBuilder.AppendLine(("- Review summary: {0}" -f $ReviewSummaryPath))
    [void]$reportBuilder.AppendLine(("- Review report: {0}" -f $ReviewReportPath))
    [void]$reportBuilder.AppendLine(("- Review check: {0}" -f $ReviewCheckPath))
    [void]$reportBuilder.AppendLine(("- Review mode: {0}" -f [string]$reviewSummary.review_status.compare_mode))
    [void]$reportBuilder.AppendLine(("- Review verdict: {0}" -f [string]$reviewSummary.review_verdict))
    [void]$reportBuilder.AppendLine(("- Candidate shelf summary: {0}" -f $CandidateShelfSummaryPath))
    [void]$reportBuilder.AppendLine(("- Candidate shelf result: {0}" -f [string]$candidateSummary.result))
    [void]$reportBuilder.AppendLine(("- Candidate shelf counts: biographies={0} worlds={1} compare_attached={2} not_attached={3}" -f [int]$candidateSummary.summary.biography_count, [int]$candidateSummary.summary.unique_world_count, [int]$candidateSummary.summary.compare_attached_count, [int]$candidateSummary.summary.not_attached_count))
    [void]$reportBuilder.AppendLine(("- Shelf compare summary: {0}" -f $CompareSummaryPath))
    [void]$reportBuilder.AppendLine(("- Shelf compare verdict: {0}" -f [string]$compareSummary.shelf_verdict))
    [void]$reportBuilder.AppendLine(("- Shelf compare changes: changed={0} added={1} removed={2} regressions={3} improvements={4}" -f [int]$compareSummary.entry_summary.changed_entry_count, [int]$compareSummary.entry_summary.added_entry_count, [int]$compareSummary.entry_summary.removed_entry_count, [int]$compareSummary.entry_summary.regression_count, [int]$compareSummary.entry_summary.improvement_count))
    Append-Utf8Text -Path $ReportPath -Text $reportBuilder.ToString()

    $checkBuilder = [System.Text.StringBuilder]::new()
    [void]$checkBuilder.AppendLine(("world_shelf_review_summary: {0}" -f $ReviewSummaryPath))
    [void]$checkBuilder.AppendLine(("world_shelf_review_report: {0}" -f $ReviewReportPath))
    [void]$checkBuilder.AppendLine(("world_shelf_review_check: {0}" -f $ReviewCheckPath))
    [void]$checkBuilder.AppendLine(("world_shelf_review_mode: {0}" -f [string]$reviewSummary.review_status.compare_mode))
    [void]$checkBuilder.AppendLine(("world_shelf_review_verdict: {0}" -f [string]$reviewSummary.review_verdict))
    [void]$checkBuilder.AppendLine(("world_shelf_candidate_summary: {0}" -f $CandidateShelfSummaryPath))
    [void]$checkBuilder.AppendLine(("world_shelf_candidate_result: {0}" -f [string]$candidateSummary.result))
    [void]$checkBuilder.AppendLine(("world_shelf_candidate_biography_count: {0}" -f [int]$candidateSummary.summary.biography_count))
    [void]$checkBuilder.AppendLine(("world_shelf_candidate_compare_attached_count: {0}" -f [int]$candidateSummary.summary.compare_attached_count))
    [void]$checkBuilder.AppendLine(("world_shelf_compare_summary: {0}" -f $CompareSummaryPath))
    [void]$checkBuilder.AppendLine(("world_shelf_compare_result: {0}" -f [string]$compareSummary.result))
    [void]$checkBuilder.AppendLine(("world_shelf_compare_verdict: {0}" -f [string]$compareSummary.shelf_verdict))
    [void]$checkBuilder.AppendLine(("world_shelf_compare_changed_entry_count: {0}" -f [int]$compareSummary.entry_summary.changed_entry_count))
    [void]$checkBuilder.AppendLine(("world_shelf_compare_regression_count: {0}" -f [int]$compareSummary.entry_summary.regression_count))
    Append-Utf8Text -Path $CheckTextPath -Text $checkBuilder.ToString()
}

$rootScript = Join-Path $PSScriptRoot "minimal_kernel_runtime_system_compiler_witness_bundle.ps1"
$reviewShelfScript = Join-Path $PSScriptRoot "review_system_compiler_world_shelf.ps1"

foreach ($requiredPath in @($rootScript, $reviewShelfScript)) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing script: $requiredPath"
    }
}

$effectiveOutputRoot = if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    "out/minimal-kernel-runtime-system-compiler-world-compare"
} else {
    $OutputRoot
}

$resolvedOutputRoot = Resolve-FullPath -Path $effectiveOutputRoot
$resolvedReportMarkdownPath = Get-OutputPath `
    -ExplicitPath $ReportMarkdownPath `
    -OutputRootPath $resolvedOutputRoot `
    -DefaultFileName "report.md"
$resolvedCheckTextPath = Get-OutputPath `
    -ExplicitPath $CheckTextPath `
    -OutputRootPath $resolvedOutputRoot `
    -DefaultFileName "check.txt"

$effectiveRuntimeEvidenceOutputRoot = if ([string]::IsNullOrWhiteSpace($RuntimeEvidenceOutputRoot)) {
    Join-Path $effectiveOutputRoot "runtime_evidence"
} else {
    $RuntimeEvidenceOutputRoot
}

$selfCompareMode = [string]::IsNullOrWhiteSpace($BaselineWitnessSummary) `
    -and [string]::IsNullOrWhiteSpace($BaselineRuntimeEvidenceSummary) `
    -and [string]::IsNullOrWhiteSpace($BaselineRuntimeEvidenceOutputRoot)

$effectiveBaselineRuntimeEvidenceSummary = $BaselineRuntimeEvidenceSummary
$effectiveBaselineRuntimeEvidenceOutputRoot = $BaselineRuntimeEvidenceOutputRoot
if ($selfCompareMode) {
    if (-not [string]::IsNullOrWhiteSpace($RuntimeEvidenceSummary)) {
        $effectiveBaselineRuntimeEvidenceSummary = $RuntimeEvidenceSummary
    } else {
        $effectiveBaselineRuntimeEvidenceOutputRoot = $effectiveRuntimeEvidenceOutputRoot
    }
}

$invokeArgs = @{
    OutputRoot       = $effectiveOutputRoot
    CMakeExe         = $CMakeExe
    Generator        = $Generator
    QemuExe          = $QemuExe
    PythonExe        = $PythonExe
    HostJobs         = $HostJobs
    QemuBuildJobs    = $QemuBuildJobs
    QemuTimeoutSec   = $QemuTimeoutSec
    QemuTailLines    = $QemuTailLines
}

if (-not [string]::IsNullOrWhiteSpace($CanonicalWorld)) {
    $invokeArgs.CanonicalWorld = $CanonicalWorld
}
if (-not [string]::IsNullOrWhiteSpace($RuntimeEvidenceSummary)) {
    $invokeArgs.RuntimeEvidenceSummary = $RuntimeEvidenceSummary
}
if (-not [string]::IsNullOrWhiteSpace($effectiveRuntimeEvidenceOutputRoot)) {
    $invokeArgs.RuntimeEvidenceOutputRoot = $effectiveRuntimeEvidenceOutputRoot
}
if (-not [string]::IsNullOrWhiteSpace($RuntimeEvidenceValidationLogPath)) {
    $invokeArgs.RuntimeEvidenceValidationLogPath = $RuntimeEvidenceValidationLogPath
}
if (-not [string]::IsNullOrWhiteSpace($effectiveBaselineRuntimeEvidenceSummary)) {
    $invokeArgs.BaselineRuntimeEvidenceSummary = $effectiveBaselineRuntimeEvidenceSummary
}
if (-not [string]::IsNullOrWhiteSpace($effectiveBaselineRuntimeEvidenceOutputRoot)) {
    $invokeArgs.BaselineRuntimeEvidenceOutputRoot = $effectiveBaselineRuntimeEvidenceOutputRoot
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
if ($PSBoundParameters.ContainsKey("HostExamples")) {
    $invokeArgs.HostExamples = $HostExamples
}

& $rootScript @invokeArgs

if (-not $RunWorldShelfFlow) {
    return
}

$resolvedBiographySummary = if ([string]::IsNullOrWhiteSpace($BiographySummaryPath)) {
    Resolve-FullPath -Path (Join-Path $resolvedOutputRoot "biography.summary.json")
} else {
    Resolve-FullPath -Path $BiographySummaryPath
}
$resolvedWorldShelfOutputRoot = if ([string]::IsNullOrWhiteSpace($WorldShelfOutputRoot)) {
    Resolve-FullPath -Path (Join-Path $resolvedOutputRoot "world-shelf")
} else {
    Resolve-FullPath -Path $WorldShelfOutputRoot
}
$resolvedWorldShelfCompareOutputRoot = if ([string]::IsNullOrWhiteSpace($WorldShelfCompareOutputRoot)) {
    Resolve-FullPath -Path (Join-Path $resolvedOutputRoot "world-shelf-compare")
} else {
    Resolve-FullPath -Path $WorldShelfCompareOutputRoot
}
$resolvedWorldShelfReviewSummaryPath = Resolve-FullPath -Path (Join-Path $resolvedOutputRoot "world-shelf.review.summary.json")
$resolvedWorldShelfReviewReportPath = Resolve-FullPath -Path (Join-Path $resolvedOutputRoot "world-shelf.review.md")
$resolvedWorldShelfReviewCheckPath = Resolve-FullPath -Path (Join-Path $resolvedOutputRoot "world-shelf.check.txt")

& $reviewShelfScript `
    -BiographySummary @($resolvedBiographySummary) `
    -OutputRoot $resolvedOutputRoot `
    -CandidateShelfOutputRoot $resolvedWorldShelfOutputRoot `
    -CompareOutputRoot $resolvedWorldShelfCompareOutputRoot `
    -ReviewSummaryPath $resolvedWorldShelfReviewSummaryPath `
    -ReviewReportMarkdownPath $resolvedWorldShelfReviewReportPath `
    -ReviewCheckTextPath $resolvedWorldShelfReviewCheckPath `
    -PythonExe $PythonExe `
    -CompareAgainstSelf `
    -CandidateProfile $WorldShelfProfile `
    -CandidateRequireBiographyCount 1 `
    -CandidateRequireUniqueWorldCount 1 `
    -CandidateRequireOkCount 1 `
    -CandidateMaxFailCount 0 `
    -CandidateRequireCompareAttachedCount 1 `
    -CandidateRequireNotAttachedCount 0 `
    -CandidateRequireStandingCount 1 `
    -CandidateRequireImprovedCount 0 `
    -CandidateRequireDriftedCount 0 `
    -CandidateRequireCollapsedCount 0 `
    -CompareRequireVerdict standing `
    -CompareMaxRegressions 0 `
    -CompareRequireAddedEntries 0 `
    -CompareRequireRemovedEntries 0 `
    -CompareRequireChangedEntries 0 `
    -CompareRequireImprovementCount 0 `
    -CompareRequireAddedWorlds 0 `
    -CompareRequireRemovedWorlds 0 `
    -CompareMaxAddedFailedEntries 0

Append-WorldShelfReviewOverlay `
    -ReportPath $resolvedReportMarkdownPath `
    -CheckTextPath $resolvedCheckTextPath `
    -ReviewSummaryPath $resolvedWorldShelfReviewSummaryPath `
    -ReviewReportPath $resolvedWorldShelfReviewReportPath `
    -ReviewCheckPath $resolvedWorldShelfReviewCheckPath `
    -CandidateShelfSummaryPath (Join-Path $resolvedWorldShelfOutputRoot "biography.index.summary.json") `
    -CompareSummaryPath (Join-Path $resolvedWorldShelfCompareOutputRoot "summary.json")

Write-Host "==> minimal kernel runtime system compiler world-compare shelf flow"
Write-Host ("biography={0}" -f $resolvedBiographySummary)
Write-Host ("world_shelf_output_root={0}" -f $resolvedWorldShelfOutputRoot)
Write-Host ("world_shelf_compare_output_root={0}" -f $resolvedWorldShelfCompareOutputRoot)
Write-Host ("world_shelf_review_summary={0}" -f $resolvedWorldShelfReviewSummaryPath)
Write-Host ("world_shelf_review_report={0}" -f $resolvedWorldShelfReviewReportPath)
Write-Host ("world_shelf_review_check={0}" -f $resolvedWorldShelfReviewCheckPath)
