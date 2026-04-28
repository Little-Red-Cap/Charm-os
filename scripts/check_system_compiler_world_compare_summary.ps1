param(
    [string]$Summary = "",
    [string]$OutputPath = "",
    [string]$RequireVerdict = "",
    [int]$MaxRegressions = -1,
    [int]$MaxRequiredRegressions = -1,
    [int]$MaxAddedMissingContractRefs = -1
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
    Resolve-FullPath -Path "out/system-compiler-world-compare/summary.json"
} else {
    Resolve-FullPath -Path $Summary
}

if (-not (Test-Path $summaryPath)) {
    throw "world compare summary not found: $summaryPath"
}

$summaryData = Get-Content -LiteralPath $summaryPath -Raw -Encoding utf8 | ConvertFrom-Json

$verdict = [string]$summaryData.world_verdict
$bundleStatus = $summaryData.bundle_status
$witnessSummary = $summaryData.witness_summary
$collapseSurface = $summaryData.collapse_surface

$regressionCount = [int]$witnessSummary.regression_count
$requiredRegressionCount = [int]$witnessSummary.required_regression_count
$addedMissingContractRefs = @($collapseSurface.added_missing_contract_refs).Count

$violations = [System.Collections.Generic.List[string]]::new()

if (-not [string]::IsNullOrWhiteSpace($RequireVerdict) -and $verdict -ne $RequireVerdict) {
    $violations.Add(('expected world verdict `{0}` but got `{1}`' -f $RequireVerdict, $verdict)) | Out-Null
}
if ($MaxRegressions -ge 0 -and $regressionCount -gt $MaxRegressions) {
    $violations.Add(("regression_count {0} exceeds max {1}" -f $regressionCount, $MaxRegressions)) | Out-Null
}
if ($MaxRequiredRegressions -ge 0 -and $requiredRegressionCount -gt $MaxRequiredRegressions) {
    $violations.Add(("required_regression_count {0} exceeds max {1}" -f $requiredRegressionCount, $MaxRequiredRegressions)) | Out-Null
}
if ($MaxAddedMissingContractRefs -ge 0 -and $addedMissingContractRefs -gt $MaxAddedMissingContractRefs) {
    $violations.Add(("added_missing_contract_refs {0} exceeds max {1}" -f $addedMissingContractRefs, $MaxAddedMissingContractRefs)) | Out-Null
}

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add(("summary: {0}" -f $summaryPath)) | Out-Null
$lines.Add(("result: {0}" -f [string]$summaryData.result)) | Out-Null
$lines.Add(("world_verdict: {0}" -f $verdict)) | Out-Null
$lines.Add(("baseline_state: {0}" -f [string]$bundleStatus.baseline_state)) | Out-Null
$lines.Add(("candidate_state: {0}" -f [string]$bundleStatus.candidate_state)) | Out-Null
$lines.Add(("regression_count: {0}" -f $regressionCount)) | Out-Null
$lines.Add(("required_regression_count: {0}" -f $requiredRegressionCount)) | Out-Null
$lines.Add(("added_missing_contract_refs: {0}" -f $addedMissingContractRefs)) | Out-Null

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
    throw "system compiler world compare summary gate failed"
}
