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
    [switch]$RunWorldShelfFlow,
    [string]$SelfCompareOutputRoot = "",
    [string]$WorldShelfOutputRoot = "",
    [string]$WorldShelfBaselineOutputRoot = "",
    [string]$WorldShelfCompareOutputRoot = "",
    [string]$WorldShelfProfile = "minimal-kernel-runtime-system-compiler-witness-shelf",
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

function Copy-Hashtable {
    param(
        [hashtable]$Source
    )

    $copy = @{}
    foreach ($key in $Source.Keys) {
        $copy[$key] = $Source[$key]
    }
    return $copy
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
        [string]$BaselineShelfSummaryPath,
        [string]$CompareSummaryPath
    )

    foreach ($requiredPath in @($ReviewSummaryPath, $CandidateShelfSummaryPath, $ReviewReportPath, $ReviewCheckPath, $CompareSummaryPath)) {
        if (-not (Test-Path $requiredPath)) {
            throw "world shelf overlay input not found: $requiredPath"
        }
    }

    $reviewSummary = Load-JsonObject -Path $ReviewSummaryPath
    $candidateSummary = Load-JsonObject -Path $CandidateShelfSummaryPath
    $baselineSummary = if ([string]::IsNullOrWhiteSpace($BaselineShelfSummaryPath) -or -not (Test-Path $BaselineShelfSummaryPath)) {
        $null
    } else {
        Load-JsonObject -Path $BaselineShelfSummaryPath
    }
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
    if ($null -ne $baselineSummary) {
        [void]$reportBuilder.AppendLine(("- Baseline shelf summary: {0}" -f $BaselineShelfSummaryPath))
        [void]$reportBuilder.AppendLine(("- Baseline shelf result: {0}" -f [string]$baselineSummary.result))
        [void]$reportBuilder.AppendLine(("- Baseline shelf counts: biographies={0} worlds={1} compare_attached={2} not_attached={3}" -f [int]$baselineSummary.summary.biography_count, [int]$baselineSummary.summary.unique_world_count, [int]$baselineSummary.summary.compare_attached_count, [int]$baselineSummary.summary.not_attached_count))
    }
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
    if ($null -ne $baselineSummary) {
        [void]$checkBuilder.AppendLine(("world_shelf_baseline_summary: {0}" -f $BaselineShelfSummaryPath))
        [void]$checkBuilder.AppendLine(("world_shelf_baseline_result: {0}" -f [string]$baselineSummary.result))
    }
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
    "out/minimal-kernel-runtime-system-compiler-witness"
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
$resolvedRuntimeEvidenceOutputRoot = if ([string]::IsNullOrWhiteSpace($RuntimeEvidenceOutputRoot)) {
    Resolve-FullPath -Path (Join-Path $resolvedOutputRoot "runtime_evidence")
} else {
    Resolve-FullPath -Path $RuntimeEvidenceOutputRoot
}
$resolvedPrimaryRuntimeEvidenceSummary = if ([string]::IsNullOrWhiteSpace($RuntimeEvidenceSummary)) {
    Resolve-FullPath -Path (Join-Path $resolvedRuntimeEvidenceOutputRoot "summary.json")
} else {
    Resolve-FullPath -Path $RuntimeEvidenceSummary
}
$resolvedPrimaryBiographySummary = Get-OutputPath `
    -ExplicitPath $BiographySummaryPath `
    -OutputRootPath $resolvedOutputRoot `
    -DefaultFileName "biography.summary.json"

$commonArgs = @{
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
    $commonArgs.CanonicalWorld = $CanonicalWorld
}
if (-not [string]::IsNullOrWhiteSpace($RuntimeEvidenceSummary)) {
    $commonArgs.RuntimeEvidenceSummary = $RuntimeEvidenceSummary
}
if (-not [string]::IsNullOrWhiteSpace($RuntimeEvidenceOutputRoot)) {
    $commonArgs.RuntimeEvidenceOutputRoot = $RuntimeEvidenceOutputRoot
}
if (-not [string]::IsNullOrWhiteSpace($RuntimeEvidenceValidationLogPath)) {
    $commonArgs.RuntimeEvidenceValidationLogPath = $RuntimeEvidenceValidationLogPath
}
if (-not [string]::IsNullOrWhiteSpace($ArtifactRoot)) {
    $commonArgs.ArtifactRoot = $ArtifactRoot
}
if ($PSBoundParameters.ContainsKey("ArtifactReport")) {
    $commonArgs.ArtifactReport = $ArtifactReport
}
if ($PSBoundParameters.ContainsKey("Case")) {
    $commonArgs.Case = $Case
}
if (-not [string]::IsNullOrWhiteSpace($OutputRoot)) {
    $commonArgs.OutputRoot = $OutputRoot
}
if (-not [string]::IsNullOrWhiteSpace($SummaryPath)) {
    $commonArgs.SummaryPath = $SummaryPath
}
if (-not [string]::IsNullOrWhiteSpace($ReportMarkdownPath)) {
    $commonArgs.ReportMarkdownPath = $ReportMarkdownPath
}
if (-not [string]::IsNullOrWhiteSpace($CheckTextPath)) {
    $commonArgs.CheckTextPath = $CheckTextPath
}
if (-not [string]::IsNullOrWhiteSpace($ValidationLogPath)) {
    $commonArgs.ValidationLogPath = $ValidationLogPath
}
if (-not [string]::IsNullOrWhiteSpace($BiographySummaryPath)) {
    $commonArgs.BiographySummaryPath = $BiographySummaryPath
}
if (-not [string]::IsNullOrWhiteSpace($BiographyReportMarkdownPath)) {
    $commonArgs.BiographyReportMarkdownPath = $BiographyReportMarkdownPath
}
if (-not [string]::IsNullOrWhiteSpace($BiographyCheckTextPath)) {
    $commonArgs.BiographyCheckTextPath = $BiographyCheckTextPath
}
if (-not [string]::IsNullOrWhiteSpace($BiographyValidationLogPath)) {
    $commonArgs.BiographyValidationLogPath = $BiographyValidationLogPath
}
if ($PSBoundParameters.ContainsKey("HostExamples")) {
    $commonArgs.HostExamples = $HostExamples
}

$primaryArgs = Copy-Hashtable -Source $commonArgs
if ($Clean) {
    $primaryArgs.Clean = $true
}

if ($RunWorldShelfFlow) {
    $primaryArgs.SkipWorldCompare = $true
} else {
    if (-not [string]::IsNullOrWhiteSpace($BaselineRuntimeEvidenceSummary)) {
        $primaryArgs.BaselineRuntimeEvidenceSummary = $BaselineRuntimeEvidenceSummary
    }
    if (-not [string]::IsNullOrWhiteSpace($BaselineRuntimeEvidenceOutputRoot)) {
        $primaryArgs.BaselineRuntimeEvidenceOutputRoot = $BaselineRuntimeEvidenceOutputRoot
    }
    if (-not [string]::IsNullOrWhiteSpace($BaselineArtifactRoot)) {
        $primaryArgs.BaselineArtifactRoot = $BaselineArtifactRoot
    }
    if ($PSBoundParameters.ContainsKey("BaselineArtifactReport")) {
        $primaryArgs.BaselineArtifactReport = $BaselineArtifactReport
    }
    if ($PSBoundParameters.ContainsKey("BaselineCase")) {
        $primaryArgs.BaselineCase = $BaselineCase
    }
    if (-not [string]::IsNullOrWhiteSpace($BaselineWitnessSummary)) {
        $primaryArgs.BaselineWitnessSummary = $BaselineWitnessSummary
    }
    if (-not [string]::IsNullOrWhiteSpace($BaselineWitnessOutputRoot)) {
        $primaryArgs.BaselineWitnessOutputRoot = $BaselineWitnessOutputRoot
    }
    if (-not [string]::IsNullOrWhiteSpace($WorldCompareOutputRoot)) {
        $primaryArgs.WorldCompareOutputRoot = $WorldCompareOutputRoot
    }
    if (-not [string]::IsNullOrWhiteSpace($WorldCompareSummaryPath)) {
        $primaryArgs.WorldCompareSummaryPath = $WorldCompareSummaryPath
    }
    if (-not [string]::IsNullOrWhiteSpace($WorldCompareReportMarkdownPath)) {
        $primaryArgs.WorldCompareReportMarkdownPath = $WorldCompareReportMarkdownPath
    }
    if (-not [string]::IsNullOrWhiteSpace($WorldCompareCheckTextPath)) {
        $primaryArgs.WorldCompareCheckTextPath = $WorldCompareCheckTextPath
    }
    if (-not [string]::IsNullOrWhiteSpace($WorldCompareValidationLogPath)) {
        $primaryArgs.WorldCompareValidationLogPath = $WorldCompareValidationLogPath
    }
    if ($SkipWorldCompare) {
        $primaryArgs.SkipWorldCompare = $true
    }
}

& $rootScript @primaryArgs

if (-not $RunWorldShelfFlow) {
    return
}

$resolvedSelfCompareOutputRoot = if ([string]::IsNullOrWhiteSpace($SelfCompareOutputRoot)) {
    Resolve-FullPath -Path (Join-Path $resolvedOutputRoot "self-compare")
} else {
    Resolve-FullPath -Path $SelfCompareOutputRoot
}
$resolvedSelfCompareBiographySummary = Resolve-FullPath -Path (Join-Path $resolvedSelfCompareOutputRoot "biography.summary.json")
$resolvedWorldShelfOutputRoot = if ([string]::IsNullOrWhiteSpace($WorldShelfOutputRoot)) {
    Resolve-FullPath -Path (Join-Path $resolvedOutputRoot "world-shelf")
} else {
    Resolve-FullPath -Path $WorldShelfOutputRoot
}
$resolvedWorldShelfBaselineOutputRoot = if ([string]::IsNullOrWhiteSpace($WorldShelfBaselineOutputRoot)) {
    Resolve-FullPath -Path (Join-Path $resolvedOutputRoot "world-shelf-baseline")
} else {
    Resolve-FullPath -Path $WorldShelfBaselineOutputRoot
}
$resolvedWorldShelfCompareOutputRoot = if ([string]::IsNullOrWhiteSpace($WorldShelfCompareOutputRoot)) {
    Resolve-FullPath -Path (Join-Path $resolvedOutputRoot "world-shelf-compare")
} else {
    Resolve-FullPath -Path $WorldShelfCompareOutputRoot
}
$resolvedWorldShelfReviewSummaryPath = Resolve-FullPath -Path (Join-Path $resolvedOutputRoot "world-shelf.review.summary.json")
$resolvedWorldShelfReviewReportPath = Resolve-FullPath -Path (Join-Path $resolvedOutputRoot "world-shelf.review.md")
$resolvedWorldShelfReviewCheckPath = Resolve-FullPath -Path (Join-Path $resolvedOutputRoot "world-shelf.check.txt")

$resolvedSelfCompareBaselineRuntimeEvidenceSummary = if ([string]::IsNullOrWhiteSpace($BaselineRuntimeEvidenceSummary)) {
    $resolvedPrimaryRuntimeEvidenceSummary
} else {
    Resolve-FullPath -Path $BaselineRuntimeEvidenceSummary
}

$selfCompareArgs = Copy-Hashtable -Source $commonArgs
$selfCompareArgs.RuntimeEvidenceSummary = $resolvedPrimaryRuntimeEvidenceSummary
$selfCompareArgs.BaselineRuntimeEvidenceSummary = $resolvedSelfCompareBaselineRuntimeEvidenceSummary
$selfCompareArgs.OutputRoot = $resolvedSelfCompareOutputRoot
$selfCompareArgs.Clean = $true

& $rootScript @selfCompareArgs

$reviewArgs = @{
    BiographySummary                    = @($resolvedPrimaryBiographySummary, $resolvedSelfCompareBiographySummary)
    BaselineBiographySummary            = @($resolvedPrimaryBiographySummary)
    OutputRoot                          = $resolvedOutputRoot
    CandidateShelfOutputRoot            = $resolvedWorldShelfOutputRoot
    BaselineShelfOutputRoot             = $resolvedWorldShelfBaselineOutputRoot
    CompareOutputRoot                   = $resolvedWorldShelfCompareOutputRoot
    ReviewSummaryPath                   = $resolvedWorldShelfReviewSummaryPath
    ReviewReportMarkdownPath            = $resolvedWorldShelfReviewReportPath
    ReviewCheckTextPath                 = $resolvedWorldShelfReviewCheckPath
    PythonExe                           = $PythonExe
    CandidateProfile                    = $WorldShelfProfile
    BaselineProfile                     = $WorldShelfProfile
    CandidateRequireBiographyCount      = 2
    CandidateRequireUniqueWorldCount    = 1
    CandidateRequireOkCount             = 2
    CandidateMaxFailCount               = 0
    CandidateRequireCompareAttachedCount = 1
    CandidateRequireNotAttachedCount    = 1
    CandidateRequireStandingCount       = 1
    CandidateRequireImprovedCount       = 0
    CandidateRequireDriftedCount        = 0
    CandidateRequireCollapsedCount      = 0
    BaselineRequireBiographyCount       = 1
    BaselineRequireUniqueWorldCount     = 1
    BaselineRequireOkCount              = 1
    BaselineMaxFailCount                = 0
    BaselineRequireCompareAttachedCount = 0
    BaselineRequireNotAttachedCount     = 1
    BaselineRequireStandingCount        = 0
    BaselineRequireImprovedCount        = 0
    BaselineRequireDriftedCount         = 0
    BaselineRequireCollapsedCount       = 0
    CompareRequireVerdict               = "improved"
    CompareMaxRegressions               = 0
    CompareRequireAddedEntries          = 1
    CompareRequireRemovedEntries        = 0
    CompareRequireChangedEntries        = 1
    CompareRequireImprovementCount      = 1
    CompareRequireAddedWorlds           = 0
    CompareRequireRemovedWorlds         = 0
    CompareMaxAddedFailedEntries        = 0
}

& $reviewShelfScript @reviewArgs

Append-WorldShelfReviewOverlay `
    -ReportPath $resolvedReportMarkdownPath `
    -CheckTextPath $resolvedCheckTextPath `
    -ReviewSummaryPath $resolvedWorldShelfReviewSummaryPath `
    -ReviewReportPath $resolvedWorldShelfReviewReportPath `
    -ReviewCheckPath $resolvedWorldShelfReviewCheckPath `
    -CandidateShelfSummaryPath (Join-Path $resolvedWorldShelfOutputRoot "biography.index.summary.json") `
    -BaselineShelfSummaryPath (Join-Path $resolvedWorldShelfBaselineOutputRoot "biography.index.summary.json") `
    -CompareSummaryPath (Join-Path $resolvedWorldShelfCompareOutputRoot "summary.json")

Write-Host "==> minimal kernel runtime system compiler witness shelf flow"
Write-Host ("primary_biography={0}" -f $resolvedPrimaryBiographySummary)
Write-Host ("self_compare_biography={0}" -f $resolvedSelfCompareBiographySummary)
Write-Host ("world_shelf_output_root={0}" -f $resolvedWorldShelfOutputRoot)
Write-Host ("world_shelf_baseline_output_root={0}" -f $resolvedWorldShelfBaselineOutputRoot)
Write-Host ("world_shelf_compare_output_root={0}" -f $resolvedWorldShelfCompareOutputRoot)
Write-Host ("world_shelf_review_summary={0}" -f $resolvedWorldShelfReviewSummaryPath)
Write-Host ("world_shelf_review_report={0}" -f $resolvedWorldShelfReviewReportPath)
Write-Host ("world_shelf_review_check={0}" -f $resolvedWorldShelfReviewCheckPath)
