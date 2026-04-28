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
    "out/minimal-kernel-runtime-system-compiler-world-compare"
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
$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}

$effectiveRuntimeEvidenceOutputRoot = if ([string]::IsNullOrWhiteSpace($RuntimeEvidenceOutputRoot)) {
    Join-Path $effectiveOutputRoot "runtime_evidence"
} else {
    $RuntimeEvidenceOutputRoot
}
$resolvedRuntimeEvidenceSummary = if ([string]::IsNullOrWhiteSpace($RuntimeEvidenceSummary)) {
    Resolve-FullPath -Path (Join-Path (Resolve-FullPath -Path $effectiveRuntimeEvidenceOutputRoot) "summary.json")
} else {
    Resolve-FullPath -Path $RuntimeEvidenceSummary
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
if (-not [string]::IsNullOrWhiteSpace($FrontPageRouteOutputRoot)) {
    $invokeArgs.FrontPageRouteOutputRoot = $FrontPageRouteOutputRoot
}
if (-not [string]::IsNullOrWhiteSpace($FrontPageRouteSummaryPath)) {
    $invokeArgs.FrontPageRouteSummaryPath = $FrontPageRouteSummaryPath
}
if (-not [string]::IsNullOrWhiteSpace($FrontPageRouteReportMarkdownPath)) {
    $invokeArgs.FrontPageRouteReportMarkdownPath = $FrontPageRouteReportMarkdownPath
}
if (-not [string]::IsNullOrWhiteSpace($FrontPageRouteCheckTextPath)) {
    $invokeArgs.FrontPageRouteCheckTextPath = $FrontPageRouteCheckTextPath
}
if (-not [string]::IsNullOrWhiteSpace($FrontPageRouteValidationLogPath)) {
    $invokeArgs.FrontPageRouteValidationLogPath = $FrontPageRouteValidationLogPath
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
$resolvedWorldCompareOutputRoot = if ([string]::IsNullOrWhiteSpace($WorldCompareOutputRoot)) {
    Resolve-FullPath -Path (Join-Path $resolvedOutputRoot "world_compare")
} else {
    Resolve-FullPath -Path $WorldCompareOutputRoot
}
$resolvedWorldCompareSummaryPath = Get-OutputPath `
    -ExplicitPath $WorldCompareSummaryPath `
    -OutputRootPath $resolvedWorldCompareOutputRoot `
    -DefaultFileName "summary.json"
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

& $updateWitnessFrontPageScript `
    -SummaryPath $resolvedSummaryPath `
    -RuntimeEvidenceSummary $resolvedRuntimeEvidenceSummary `
    -BiographySummary $resolvedBiographySummary `
    -WorldCompareSummary $resolvedWorldCompareSummaryPath `
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
    -FailurePrefix "system compiler world-compare witness"

Write-Host "==> minimal kernel runtime system compiler world-compare shelf flow"
Write-Host ("biography={0}" -f $resolvedBiographySummary)
Write-Host ("world_shelf_output_root={0}" -f $resolvedWorldShelfOutputRoot)
Write-Host ("world_shelf_compare_output_root={0}" -f $resolvedWorldShelfCompareOutputRoot)
Write-Host ("world_shelf_review_summary={0}" -f $resolvedWorldShelfReviewSummaryPath)
Write-Host ("world_shelf_review_report={0}" -f $resolvedWorldShelfReviewReportPath)
Write-Host ("world_shelf_review_check={0}" -f $resolvedWorldShelfReviewCheckPath)
Write-Host ("front_page_route_output_root={0}" -f $resolvedFrontPageRouteOutputRoot)
Write-Host ("front_page_route_summary={0}" -f $resolvedFrontPageRouteSummaryPath)
Write-Host ("front_page_route_report={0}" -f $resolvedFrontPageRouteReportMarkdownPath)
Write-Host ("front_page_route_check={0}" -f $resolvedFrontPageRouteCheckTextPath)
Write-Host ("front_page_route_validation_log={0}" -f $resolvedFrontPageRouteValidationLogPath)
