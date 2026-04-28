param(
    [string]$Summary = "",
    [string]$OutputPath = "",
    [string]$RequireResult = "ok",
    [string]$RequireRuntimeEvidenceResult = "ok",
    [string]$RequireWitnessBundleResult = "ok",
    [string]$RequireWorldCompareResult = "",
    [string]$RequireVerdict = "",
    [int]$MaxRequiredMissing = -1,
    [int]$MaxRegressions = -1,
    [int]$MaxRequiredRegressions = -1,
    [switch]$RequireCollapseSurfaceUnchanged
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

$summaryPath = if ([string]::IsNullOrWhiteSpace($Summary)) {
    Resolve-FullPath -Path "out/system-compiler-biography/biography.summary.json"
} else {
    Resolve-FullPath -Path $Summary
}

if (-not (Test-Path $summaryPath)) {
    throw "system compiler biography summary not found: $summaryPath"
}

$summaryData = Get-Content -LiteralPath $summaryPath -Raw -Encoding utf8 | ConvertFrom-Json

$topLevelResult = [string]$summaryData.result
$runtimeEvidenceResult = [string]$summaryData.runtime_evidence.result
$witnessBundleResult = [string]$summaryData.witness_bundle.result
$requiredMissingCount = [int]$summaryData.witness_bundle.required_missing_count
$nextQuestionCount = @($summaryData.biography.next_questions).Count

$compareAttached = $null -ne $summaryData.world_compare
$worldCompareResult = if ($compareAttached) {
    [string]$summaryData.world_compare.result
} else {
    "not-attached"
}
$effectiveVerdict = if ($compareAttached -and -not [string]::IsNullOrWhiteSpace([string]$summaryData.world_verdict)) {
    [string]$summaryData.world_verdict
} else {
    "not-attached"
}
$regressionCount = if ($compareAttached) {
    [int]$summaryData.world_compare.regression_count
} else {
    0
}
$requiredRegressionCount = if ($compareAttached) {
    [int]$summaryData.world_compare.required_regression_count
} else {
    0
}
$collapseSurfaceChanged = if ($compareAttached) {
    [bool]$summaryData.world_compare.collapse_surface_changed
} else {
    $false
}

$violations = [System.Collections.Generic.List[string]]::new()

if (-not [string]::IsNullOrWhiteSpace($RequireResult) -and $topLevelResult -ne $RequireResult) {
    $violations.Add(('expected result `{0}` but got `{1}`' -f $RequireResult, $topLevelResult)) | Out-Null
}
if (-not [string]::IsNullOrWhiteSpace($RequireRuntimeEvidenceResult) -and $runtimeEvidenceResult -ne $RequireRuntimeEvidenceResult) {
    $violations.Add(('expected runtime_evidence.result `{0}` but got `{1}`' -f $RequireRuntimeEvidenceResult, $runtimeEvidenceResult)) | Out-Null
}
if (-not [string]::IsNullOrWhiteSpace($RequireWitnessBundleResult) -and $witnessBundleResult -ne $RequireWitnessBundleResult) {
    $violations.Add(('expected witness_bundle.result `{0}` but got `{1}`' -f $RequireWitnessBundleResult, $witnessBundleResult)) | Out-Null
}
if (-not [string]::IsNullOrWhiteSpace($RequireWorldCompareResult) -and $worldCompareResult -ne $RequireWorldCompareResult) {
    $violations.Add(('expected world_compare.result `{0}` but got `{1}`' -f $RequireWorldCompareResult, $worldCompareResult)) | Out-Null
}
if (-not [string]::IsNullOrWhiteSpace($RequireVerdict) -and $effectiveVerdict -ne $RequireVerdict) {
    $violations.Add(('expected world verdict `{0}` but got `{1}`' -f $RequireVerdict, $effectiveVerdict)) | Out-Null
}
if ($MaxRequiredMissing -ge 0 -and $requiredMissingCount -gt $MaxRequiredMissing) {
    $violations.Add(("required_missing_count {0} exceeds max {1}" -f $requiredMissingCount, $MaxRequiredMissing)) | Out-Null
}
if ($MaxRegressions -ge 0 -and $regressionCount -gt $MaxRegressions) {
    $violations.Add(("regression_count {0} exceeds max {1}" -f $regressionCount, $MaxRegressions)) | Out-Null
}
if ($MaxRequiredRegressions -ge 0 -and $requiredRegressionCount -gt $MaxRequiredRegressions) {
    $violations.Add(("required_regression_count {0} exceeds max {1}" -f $requiredRegressionCount, $MaxRequiredRegressions)) | Out-Null
}
if ($RequireCollapseSurfaceUnchanged -and $collapseSurfaceChanged) {
    $violations.Add("collapse_surface_changed is true") | Out-Null
}

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add(("summary: {0}" -f $summaryPath)) | Out-Null
$lines.Add(("result: {0}" -f $topLevelResult)) | Out-Null
$lines.Add(("runtime_evidence_result: {0}" -f $runtimeEvidenceResult)) | Out-Null
$lines.Add(("witness_bundle_result: {0}" -f $witnessBundleResult)) | Out-Null
$lines.Add(("world_compare_attached: {0}" -f $compareAttached)) | Out-Null
$lines.Add(("world_compare_result: {0}" -f $worldCompareResult)) | Out-Null
$lines.Add(("world_verdict: {0}" -f $effectiveVerdict)) | Out-Null
$lines.Add(("required_missing_count: {0}" -f $requiredMissingCount)) | Out-Null
$lines.Add(("regression_count: {0}" -f $regressionCount)) | Out-Null
$lines.Add(("required_regression_count: {0}" -f $requiredRegressionCount)) | Out-Null
$lines.Add(("collapse_surface_changed: {0}" -f $collapseSurfaceChanged)) | Out-Null
$lines.Add(("next_question_count: {0}" -f $nextQuestionCount)) | Out-Null

if ($violations.Count -gt 0) {
    $lines.Add("violations:") | Out-Null
    foreach ($message in $violations) {
        $lines.Add(("- {0}" -f $message)) | Out-Null
    }
}

$output = ($lines -join [Environment]::NewLine)
Write-Output $output

if (-not [string]::IsNullOrWhiteSpace($OutputPath)) {
    $resolvedOutputPath = Resolve-FullPath -Path $OutputPath
    Ensure-ParentDirectory -Path $resolvedOutputPath
    Set-Content -LiteralPath $resolvedOutputPath -Encoding utf8 $output
}

if ($violations.Count -gt 0) {
    throw "system compiler biography summary gate failed"
}
