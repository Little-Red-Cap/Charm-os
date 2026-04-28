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
    [string]$FrontPageRouteOutputRoot = "",
    [string]$FrontPageRouteSummaryPath = "",
    [string]$FrontPageRouteReportMarkdownPath = "",
    [string]$FrontPageRouteCheckTextPath = "",
    [string]$FrontPageRouteValidationLogPath = "",
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

function Resolve-ToolPath {
    param(
        [string[]]$Candidates
    )

    foreach ($candidate in $Candidates) {
        $command = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($null -ne $command) {
            return $command.Source
        }
    }

    throw "tool not found: $($Candidates -join ', ')"
}

function Invoke-ExternalTool {
    param(
        [string]$Executable,
        [string[]]$ArgumentList,
        [string]$LogPath,
        [string]$FailureMessage
    )

    Ensure-ParentDirectory -Path $LogPath
    Write-Host ("==> {0}" -f [System.IO.Path]::GetFileName($Executable))

    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & $Executable @ArgumentList 2>&1 | Tee-Object -FilePath $LogPath | Out-Host
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    if ($exitCode -ne 0) {
        throw ("{0} (exit code {1})" -f $FailureMessage, $exitCode)
    }
}

function Export-FrontPageRouteArtifacts {
    param(
        [string]$PythonExe,
        [string]$ExportScript,
        [string]$ValidateScript,
        [string]$InputSummaryPath,
        [string]$OutputRootPath,
        [string]$SummaryPath,
        [string]$ReportMarkdownPath,
        [string]$CheckTextPath,
        [string]$ValidationLogPath,
        [string]$FailurePrefix
    )

    $exportLogPath = Join-Path $OutputRootPath "front-page.route.export.log"
    Invoke-ExternalTool `
        -Executable $PythonExe `
        -ArgumentList @(
            $ExportScript,
            "--summary",
            $InputSummaryPath,
            "--output-root",
            $OutputRootPath,
            "--route-summary",
            $SummaryPath,
            "--report-markdown",
            $ReportMarkdownPath,
            "--check-text",
            $CheckTextPath
        ) `
        -LogPath $exportLogPath `
        -FailureMessage ("{0} front page route export failed" -f $FailurePrefix)

    Invoke-ExternalTool `
        -Executable $PythonExe `
        -ArgumentList @(
            $ValidateScript,
            "--summary",
            $SummaryPath
        ) `
        -LogPath $ValidationLogPath `
        -FailureMessage ("{0} front page route validation failed" -f $FailurePrefix)
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
$updateWitnessFrontPageScript = Join-Path $PSScriptRoot "update_system_compiler_witness_bundle_front_page.ps1"
$validateWitnessScript = Join-Path $PSScriptRoot "validate_system_compiler_witness_bundle.py"
$exportFrontPageRouteScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_route.py"
$validateFrontPageRouteScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_route.py"

foreach ($requiredPath in @($rootScript, $reviewShelfScript, $updateWitnessFrontPageScript, $validateWitnessScript, $exportFrontPageRouteScript, $validateFrontPageRouteScript)) {
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
$resolvedSummaryPath = Get-OutputPath `
    -ExplicitPath $SummaryPath `
    -OutputRootPath $resolvedOutputRoot `
    -DefaultFileName "summary.json"
$resolvedReportMarkdownPath = Get-OutputPath `
    -ExplicitPath $ReportMarkdownPath `
    -OutputRootPath $resolvedOutputRoot `
    -DefaultFileName "report.md"
$resolvedCheckTextPath = Get-OutputPath `
    -ExplicitPath $CheckTextPath `
    -OutputRootPath $resolvedOutputRoot `
    -DefaultFileName "check.txt"
$resolvedValidationLogPath = Get-OutputPath `
    -ExplicitPath $ValidationLogPath `
    -OutputRootPath $resolvedOutputRoot `
    -DefaultFileName "validate.log"
$resolvedRuntimeEvidenceOutputRoot = if ([string]::IsNullOrWhiteSpace($RuntimeEvidenceOutputRoot)) {
    Resolve-FullPath -Path (Join-Path $resolvedOutputRoot "runtime_evidence")
} else {
    Resolve-FullPath -Path $RuntimeEvidenceOutputRoot
}
$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
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
$resolvedFrontPageRouteOutputRoot = if ([string]::IsNullOrWhiteSpace($FrontPageRouteOutputRoot)) {
    Resolve-FullPath -Path (Join-Path $resolvedOutputRoot "front_page_route")
} else {
    Resolve-FullPath -Path $FrontPageRouteOutputRoot
}
$resolvedFrontPageRouteSummaryPath = Get-OutputPath `
    -ExplicitPath $FrontPageRouteSummaryPath `
    -OutputRootPath $resolvedFrontPageRouteOutputRoot `
    -DefaultFileName "front-page.route.summary.json"
$resolvedFrontPageRouteReportMarkdownPath = Get-OutputPath `
    -ExplicitPath $FrontPageRouteReportMarkdownPath `
    -OutputRootPath $resolvedFrontPageRouteOutputRoot `
    -DefaultFileName "front-page.route.report.md"
$resolvedFrontPageRouteCheckTextPath = Get-OutputPath `
    -ExplicitPath $FrontPageRouteCheckTextPath `
    -OutputRootPath $resolvedFrontPageRouteOutputRoot `
    -DefaultFileName "front-page.route.check.txt"
$resolvedFrontPageRouteValidationLogPath = Get-OutputPath `
    -ExplicitPath $FrontPageRouteValidationLogPath `
    -OutputRootPath $resolvedFrontPageRouteOutputRoot `
    -DefaultFileName "front-page.route.validate.log"

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
if (-not [string]::IsNullOrWhiteSpace($FrontPageRouteOutputRoot)) {
    $primaryArgs.FrontPageRouteOutputRoot = $FrontPageRouteOutputRoot
}
if (-not [string]::IsNullOrWhiteSpace($FrontPageRouteSummaryPath)) {
    $primaryArgs.FrontPageRouteSummaryPath = $FrontPageRouteSummaryPath
}
if (-not [string]::IsNullOrWhiteSpace($FrontPageRouteReportMarkdownPath)) {
    $primaryArgs.FrontPageRouteReportMarkdownPath = $FrontPageRouteReportMarkdownPath
}
if (-not [string]::IsNullOrWhiteSpace($FrontPageRouteCheckTextPath)) {
    $primaryArgs.FrontPageRouteCheckTextPath = $FrontPageRouteCheckTextPath
}
if (-not [string]::IsNullOrWhiteSpace($FrontPageRouteValidationLogPath)) {
    $primaryArgs.FrontPageRouteValidationLogPath = $FrontPageRouteValidationLogPath
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

& $updateWitnessFrontPageScript `
    -SummaryPath $resolvedSummaryPath `
    -RuntimeEvidenceSummary $resolvedPrimaryRuntimeEvidenceSummary `
    -BiographySummary $resolvedPrimaryBiographySummary `
    -WorldShelfReviewSummary $resolvedWorldShelfReviewSummaryPath

Ensure-ParentDirectory -Path $resolvedValidationLogPath
$previousErrorActionPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    & $resolvedPythonExe $validateWitnessScript --summary $resolvedSummaryPath 2>&1 | Tee-Object -FilePath $resolvedValidationLogPath | Out-Host
    $exitCode = $LASTEXITCODE
} finally {
    $ErrorActionPreference = $previousErrorActionPreference
}

if ($exitCode -ne 0) {
    throw ("system compiler witness validation failed after world shelf front_page update (exit code {0})" -f $exitCode)
}

Export-FrontPageRouteArtifacts `
    -PythonExe $resolvedPythonExe `
    -ExportScript $exportFrontPageRouteScript `
    -ValidateScript $validateFrontPageRouteScript `
    -InputSummaryPath $resolvedSummaryPath `
    -OutputRootPath $resolvedFrontPageRouteOutputRoot `
    -SummaryPath $resolvedFrontPageRouteSummaryPath `
    -ReportMarkdownPath $resolvedFrontPageRouteReportMarkdownPath `
    -CheckTextPath $resolvedFrontPageRouteCheckTextPath `
    -ValidationLogPath $resolvedFrontPageRouteValidationLogPath `
    -FailurePrefix "system compiler witness"

Write-Host "==> minimal kernel runtime system compiler witness shelf flow"
Write-Host ("primary_biography={0}" -f $resolvedPrimaryBiographySummary)
Write-Host ("self_compare_biography={0}" -f $resolvedSelfCompareBiographySummary)
Write-Host ("world_shelf_output_root={0}" -f $resolvedWorldShelfOutputRoot)
Write-Host ("world_shelf_baseline_output_root={0}" -f $resolvedWorldShelfBaselineOutputRoot)
Write-Host ("world_shelf_compare_output_root={0}" -f $resolvedWorldShelfCompareOutputRoot)
Write-Host ("world_shelf_review_summary={0}" -f $resolvedWorldShelfReviewSummaryPath)
Write-Host ("world_shelf_review_report={0}" -f $resolvedWorldShelfReviewReportPath)
Write-Host ("world_shelf_review_check={0}" -f $resolvedWorldShelfReviewCheckPath)
Write-Host ("front_page_route_output_root={0}" -f $resolvedFrontPageRouteOutputRoot)
Write-Host ("front_page_route_summary={0}" -f $resolvedFrontPageRouteSummaryPath)
Write-Host ("front_page_route_report={0}" -f $resolvedFrontPageRouteReportMarkdownPath)
Write-Host ("front_page_route_check={0}" -f $resolvedFrontPageRouteCheckTextPath)
Write-Host ("front_page_route_validation_log={0}" -f $resolvedFrontPageRouteValidationLogPath)
