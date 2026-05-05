param(
    [string]$Summary = "",
    [string]$OutputPath = "",
    [string]$RequireResult = "",
    [string]$RequireSessionStatus = "",
    [string]$RequireWorldCompareVerdict = "",
    [string]$RequireWitnessCompareVerdict = "",
    [string]$RequireSessionDrift = "",
    [string[]]$RequireSessionFailureCode = @(),
    [string[]]$RequireMissingRuntimeFact = @(),
    [string[]]$RequireSessionFocus = @(),
    [int]$MaxViolations = 0
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

function Get-ObjectPropertyValue {
    param(
        $Object,
        [string]$Name,
        $Default = $null
    )

    if ($null -eq $Object) {
        return $Default
    }

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        return $Default
    }

    if ($null -eq $property.Value) {
        return $Default
    }

    return $property.Value
}

function Get-ObjectStringValue {
    param(
        $Object,
        [string]$Name,
        [string]$Default = ""
    )

    $value = Get-ObjectPropertyValue -Object $Object -Name $Name -Default $null
    if ($null -eq $value) {
        return $Default
    }

    return [string]$value
}

function Get-ArrayValues {
    param(
        $Value
    )

    if ($null -eq $Value) {
        return @()
    }

    return @($Value | ForEach-Object { [string]$_ })
}

function Expand-ExpectedValues {
    param(
        [string[]]$Values
    )

    $expanded = [System.Collections.Generic.List[string]]::new()
    foreach ($value in @($Values)) {
        foreach ($entry in ([string]$value -split ",")) {
            $trimmed = $entry.Trim()
            if (-not [string]::IsNullOrWhiteSpace($trimmed)) {
                $expanded.Add($trimmed) | Out-Null
            }
        }
    }

    return $expanded.ToArray()
}

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

function Get-SessionDriftChanged {
    param(
        $Drift
    )

    if ($null -eq $Drift) {
        return $false
    }

    return [bool]$Drift.changed
}

$summaryPath = if ([string]::IsNullOrWhiteSpace($Summary)) {
    Resolve-FullPath -Path "cmake-build-minimal-kernel-runtime-session-witness-smoke\summary.json"
} else {
    Resolve-FullPath -Path $Summary
}

if (-not (Test-Path $summaryPath)) {
    throw "runtime session witness smoke summary not found: $summaryPath"
}

$summaryData = Get-Content -LiteralPath $summaryPath -Raw -Encoding utf8 | ConvertFrom-Json
$RequireSessionFailureCode = Expand-ExpectedValues -Values $RequireSessionFailureCode
$RequireMissingRuntimeFact = Expand-ExpectedValues -Values $RequireMissingRuntimeFact
$RequireSessionFocus = Expand-ExpectedValues -Values $RequireSessionFocus

$checks = $summaryData.checks
$session = $checks.session
$worldCompare = $checks.world_compare_session_drift
$witnessExport = $checks.witness_session_failure_export
$worldSessionDrift = $worldCompare.session_drift
$witnessSessionDrift = $witnessExport.session_drift

$result = Get-ObjectStringValue -Object $summaryData -Name "result"
$sessionStatus = Get-ObjectStringValue -Object $session -Name "session_status"
$worldCompareVerdict = Get-ObjectStringValue -Object $worldCompare -Name "world_verdict"
$witnessCompareVerdict = Get-ObjectStringValue -Object $witnessExport -Name "world_verdict"
$worldSessionDriftChanged = Get-SessionDriftChanged -Drift $worldSessionDrift
$witnessSessionDriftChanged = Get-SessionDriftChanged -Drift $witnessSessionDrift
$violationCount = @($summaryData.violations).Count

$violations = [System.Collections.Generic.List[string]]::new()

if (-not [string]::IsNullOrWhiteSpace($RequireResult) -and $result -ne $RequireResult) {
    $violations.Add(("expected result [{0}] but got [{1}]" -f $RequireResult, $result)) | Out-Null
}
if (-not [string]::IsNullOrWhiteSpace($RequireSessionStatus) -and $sessionStatus -ne $RequireSessionStatus) {
    $violations.Add(("expected session status [{0}] but got [{1}]" -f $RequireSessionStatus, $sessionStatus)) | Out-Null
}
if (-not [string]::IsNullOrWhiteSpace($RequireWorldCompareVerdict) -and $worldCompareVerdict -ne $RequireWorldCompareVerdict) {
    $violations.Add(("expected world compare verdict [{0}] but got [{1}]" -f $RequireWorldCompareVerdict, $worldCompareVerdict)) | Out-Null
}
if (-not [string]::IsNullOrWhiteSpace($RequireWitnessCompareVerdict) -and $witnessCompareVerdict -ne $RequireWitnessCompareVerdict) {
    $violations.Add(("expected witness compare verdict [{0}] but got [{1}]" -f $RequireWitnessCompareVerdict, $witnessCompareVerdict)) | Out-Null
}
if (-not [string]::IsNullOrWhiteSpace($RequireSessionDrift)) {
    $expectedSessionDrift = Convert-ToBoolStrict -Value $RequireSessionDrift -Label "RequireSessionDrift"
    if ($worldSessionDriftChanged -ne $expectedSessionDrift) {
        $violations.Add(("expected world_compare_session_drift.changed [{0}] but got [{1}]" -f $expectedSessionDrift.ToString().ToLowerInvariant(), $worldSessionDriftChanged.ToString().ToLowerInvariant())) | Out-Null
    }
    if ($witnessSessionDriftChanged -ne $expectedSessionDrift) {
        $violations.Add(("expected witness_session_failure_export.session_drift.changed [{0}] but got [{1}]" -f $expectedSessionDrift.ToString().ToLowerInvariant(), $witnessSessionDriftChanged.ToString().ToLowerInvariant())) | Out-Null
    }
}
if ($MaxViolations -ge 0 -and $violationCount -gt $MaxViolations) {
    $violations.Add(("summary violations {0} exceeds max {1}" -f $violationCount, $MaxViolations)) | Out-Null
}

Ensure-ContainsAll -Label "world_compare_session_drift.failure_codes" -Actual (Get-ArrayValues -Value $worldSessionDrift.failure_codes) -Expected $RequireSessionFailureCode -Violations $violations
Ensure-ContainsAll -Label "witness_session_failure_export.session_drift.failure_codes" -Actual (Get-ArrayValues -Value $witnessSessionDrift.failure_codes) -Expected $RequireSessionFailureCode -Violations $violations
Ensure-ContainsAll -Label "world_compare_session_drift.missing_runtime_facts" -Actual (Get-ArrayValues -Value $worldSessionDrift.missing_runtime_facts) -Expected $RequireMissingRuntimeFact -Violations $violations
Ensure-ContainsAll -Label "witness_session_failure_export.session_drift.missing_runtime_facts" -Actual (Get-ArrayValues -Value $witnessSessionDrift.missing_runtime_facts) -Expected $RequireMissingRuntimeFact -Violations $violations
Ensure-ContainsAll -Label "world_compare_session_drift.affected_focus" -Actual (Get-ArrayValues -Value $worldSessionDrift.affected_focus) -Expected $RequireSessionFocus -Violations $violations
Ensure-ContainsAll -Label "witness_session_failure_export.session_drift.affected_focus" -Actual (Get-ArrayValues -Value $witnessSessionDrift.affected_focus) -Expected $RequireSessionFocus -Violations $violations

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add(("summary: {0}" -f $summaryPath)) | Out-Null
$lines.Add(("result: {0}" -f $result)) | Out-Null
$lines.Add(("session_status: {0}" -f $sessionStatus)) | Out-Null
$lines.Add(("world_compare_verdict: {0}" -f $worldCompareVerdict)) | Out-Null
$lines.Add(("witness_compare_verdict: {0}" -f $witnessCompareVerdict)) | Out-Null
$lines.Add(("world_compare_session_drift_changed: {0}" -f $worldSessionDriftChanged.ToString().ToLowerInvariant())) | Out-Null
$lines.Add(("witness_session_drift_changed: {0}" -f $witnessSessionDriftChanged.ToString().ToLowerInvariant())) | Out-Null
$lines.Add(("world_compare_failure_codes: {0}" -f ((Get-ArrayValues -Value $worldSessionDrift.failure_codes) -join ","))) | Out-Null
$lines.Add(("witness_failure_codes: {0}" -f ((Get-ArrayValues -Value $witnessSessionDrift.failure_codes) -join ","))) | Out-Null
$lines.Add(("world_compare_missing_runtime_facts: {0}" -f ((Get-ArrayValues -Value $worldSessionDrift.missing_runtime_facts) -join ","))) | Out-Null
$lines.Add(("witness_missing_runtime_facts: {0}" -f ((Get-ArrayValues -Value $witnessSessionDrift.missing_runtime_facts) -join ","))) | Out-Null
$lines.Add(("summary_violations: {0}" -f $violationCount)) | Out-Null

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
    throw "minimal kernel runtime session witness smoke summary gate failed"
}
