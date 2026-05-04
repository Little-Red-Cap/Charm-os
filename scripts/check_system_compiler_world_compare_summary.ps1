param(
    [string]$Summary = "",
    [string]$OutputPath = "",
    [string]$RequireVerdict = "",
    [int]$MaxRegressions = -1,
    [int]$MaxRequiredRegressions = -1,
    [int]$MaxAddedMissingContractRefs = -1,
    [string]$RequireSessionDrift = "",
    [string[]]$RequireSessionDomain = @(),
    [string[]]$RequireSessionFocus = @(),
    [string[]]$RequireSessionFailureCode = @(),
    [string[]]$RequireMissingRuntimeFact = @()
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
$sessionDrift = $collapseSurface.session_drift

$regressionCount = [int]$witnessSummary.regression_count
$requiredRegressionCount = [int]$witnessSummary.required_regression_count
$addedMissingContractRefs = @($collapseSurface.added_missing_contract_refs).Count
$sessionDriftChanged = if ($null -eq $sessionDrift) { $false } else { [bool]$sessionDrift.changed }

function Ensure-ContainsAll {
    param(
        [string]$Label,
        [object[]]$Actual,
        [string[]]$Expected,
        [System.Collections.Generic.List[string]]$Violations
    )

    if ($Expected.Count -eq 0) {
        return
    }

    $actualValues = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($value in @($Actual)) {
        if (-not [string]::IsNullOrWhiteSpace([string]$value)) {
            [void]$actualValues.Add([string]$value)
        }
    }

    foreach ($expectedValue in $Expected) {
        if ([string]::IsNullOrWhiteSpace($expectedValue)) {
            continue
        }
        if (-not $actualValues.Contains($expectedValue)) {
            $Violations.Add(("expected {0} to contain [{1}]" -f $Label, $expectedValue)) | Out-Null
        }
    }
}

function Convert-ToBoolStrict {
    param(
        [string]$Value,
        [string]$Label
    )

    switch ($Value.Trim().ToLowerInvariant()) {
        "true" { return $true }
        "1" { return $true }
        "yes" { return $true }
        "false" { return $false }
        "0" { return $false }
        "no" { return $false }
    }

    throw "invalid boolean for ${Label}: $Value"
}

$violations = [System.Collections.Generic.List[string]]::new()

if (-not [string]::IsNullOrWhiteSpace($RequireVerdict) -and $verdict -ne $RequireVerdict) {
    $violations.Add(("expected world verdict [{0}] but got [{1}]" -f $RequireVerdict, $verdict)) | Out-Null
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
if (-not [string]::IsNullOrWhiteSpace($RequireSessionDrift)) {
    $expectedSessionDrift = Convert-ToBoolStrict -Value $RequireSessionDrift -Label "RequireSessionDrift"
    if ($sessionDriftChanged -ne $expectedSessionDrift) {
        $violations.Add(("expected session_drift.changed [{0}] but got [{1}]" -f $expectedSessionDrift.ToString().ToLowerInvariant(), $sessionDriftChanged.ToString().ToLowerInvariant())) | Out-Null
    }
}
Ensure-ContainsAll -Label "session_drift.affected_domains" -Actual @($sessionDrift.affected_domains) -Expected $RequireSessionDomain -Violations $violations
Ensure-ContainsAll -Label "session_drift.affected_focus" -Actual @($sessionDrift.affected_focus) -Expected $RequireSessionFocus -Violations $violations
Ensure-ContainsAll -Label "session_drift.failure_codes" -Actual @($sessionDrift.failure_codes) -Expected $RequireSessionFailureCode -Violations $violations
Ensure-ContainsAll -Label "session_drift.missing_runtime_facts" -Actual @($sessionDrift.missing_runtime_facts) -Expected $RequireMissingRuntimeFact -Violations $violations

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add(("summary: {0}" -f $summaryPath)) | Out-Null
$lines.Add(("result: {0}" -f [string]$summaryData.result)) | Out-Null
$lines.Add(("world_verdict: {0}" -f $verdict)) | Out-Null
$lines.Add(("baseline_state: {0}" -f [string]$bundleStatus.baseline_state)) | Out-Null
$lines.Add(("candidate_state: {0}" -f [string]$bundleStatus.candidate_state)) | Out-Null
$lines.Add(("regression_count: {0}" -f $regressionCount)) | Out-Null
$lines.Add(("required_regression_count: {0}" -f $requiredRegressionCount)) | Out-Null
$lines.Add(("added_missing_contract_refs: {0}" -f $addedMissingContractRefs)) | Out-Null
$lines.Add(("session_drift_changed: {0}" -f $sessionDriftChanged.ToString().ToLowerInvariant())) | Out-Null
$lines.Add(("session_drift_domains: {0}" -f (@($sessionDrift.affected_domains) -join ","))) | Out-Null
$lines.Add(("session_drift_focus: {0}" -f (@($sessionDrift.affected_focus) -join ","))) | Out-Null
$lines.Add(("session_drift_failure_codes: {0}" -f (@($sessionDrift.failure_codes) -join ","))) | Out-Null
$lines.Add(("session_drift_missing_runtime_facts: {0}" -f (@($sessionDrift.missing_runtime_facts) -join ","))) | Out-Null

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
