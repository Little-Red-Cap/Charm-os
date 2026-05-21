param(
    [string]$Summary = "",
    [string]$OutputPath = "",
    [string]$RequireResult = "ok",
    [int]$RequireStateCount = 9,
    [int]$MaxSummaryViolations = 0,
    [string]$RequireFrozenStatus = "missing",
    [string]$RequireFrozenProjectionKind = "interpretive",
    [string]$RequireFrozenSidecarGap = "recommended",
    [string]$RequireLoweredProjectionKind = "",
    [string]$RequireArchivedCoverageStrength = "",
    [string]$RequireObservedStatus = ""
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

function Test-ObjectProperty {
    param(
        $Object,
        [string]$Name
    )

    if ($null -eq $Object) {
        return $false
    }

    return $null -ne $Object.PSObject.Properties[$Name]
}

function Get-ObjectPropertyValue {
    param(
        $Object,
        [string]$Name,
        $Default = $null
    )

    if (-not (Test-ObjectProperty -Object $Object -Name $Name)) {
        return $Default
    }

    $value = $Object.PSObject.Properties[$Name].Value
    if ($null -eq $value) {
        return $Default
    }

    return $value
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

function Add-IfMismatch {
    param(
        [System.Collections.Generic.List[string]]$Violations,
        [string]$Label,
        [string]$Actual,
        [string]$Expected
    )

    if ([string]::IsNullOrWhiteSpace($Expected)) {
        return
    }

    if ($Actual -ne $Expected) {
        $Violations.Add(("expected {0} [{1}] but got [{2}]" -f $Label, $Expected, $Actual)) | Out-Null
    }
}

$summaryPath = if ([string]::IsNullOrWhiteSpace($Summary)) {
    Resolve-FullPath -Path "out/compiler-lifecycle-summary/compiler_lifecycle.summary.json"
} else {
    Resolve-FullPath -Path $Summary
}

if (-not (Test-Path $summaryPath)) {
    throw "compiler lifecycle summary not found: $summaryPath"
}

$summaryData = Get-Content -LiteralPath $summaryPath -Raw -Encoding utf8 | ConvertFrom-Json
$states = Get-ObjectPropertyValue -Object $summaryData -Name "states" -Default $null
$summaryViolations = @(Get-ObjectPropertyValue -Object $summaryData -Name "violations" -Default @())

$expectedStates = @(
    "declared",
    "materialized",
    "proven",
    "frozen",
    "lowered",
    "witnessed",
    "observed",
    "archived",
    "compared"
)

$requiredStateFields = @(
    "status",
    "coverage_strength",
    "projection_kind",
    "sidecar_gap",
    "source_surfaces",
    "notes"
)

$violations = [System.Collections.Generic.List[string]]::new()

Add-IfMismatch -Violations $violations -Label "schema" -Actual (Get-ObjectStringValue -Object $summaryData -Name "schema") -Expected "charm.compiler_lifecycle.summary/v0"
Add-IfMismatch -Violations $violations -Label "kind" -Actual (Get-ObjectStringValue -Object $summaryData -Name "kind") -Expected "charm.compiler_lifecycle.summary"
Add-IfMismatch -Violations $violations -Label "result" -Actual (Get-ObjectStringValue -Object $summaryData -Name "result") -Expected $RequireResult

if ($RequireStateCount -ge 0) {
    $declaredStateCount = [int](Get-ObjectPropertyValue -Object $summaryData -Name "state_count" -Default -1)
    if ($declaredStateCount -ne $RequireStateCount) {
        $violations.Add(("expected state_count [{0}] but got [{1}]" -f $RequireStateCount, $declaredStateCount)) | Out-Null
    }
}

if ($MaxSummaryViolations -ge 0 -and $summaryViolations.Count -gt $MaxSummaryViolations) {
    $violations.Add(("summary violations {0} exceeds max {1}" -f $summaryViolations.Count, $MaxSummaryViolations)) | Out-Null
}

if ($null -eq $states) {
    $violations.Add("summary missing states object") | Out-Null
    $actualStateNames = @()
} else {
    $actualStateNames = @($states.PSObject.Properties.Name)
}

if ($RequireStateCount -ge 0 -and $actualStateNames.Count -ne $RequireStateCount) {
    $violations.Add(("expected actual state count [{0}] but got [{1}]" -f $RequireStateCount, $actualStateNames.Count)) | Out-Null
}

foreach ($expectedState in $expectedStates) {
    if ($actualStateNames -notcontains $expectedState) {
        $violations.Add(("missing lifecycle state [{0}]" -f $expectedState)) | Out-Null
    }
}

foreach ($actualState in $actualStateNames) {
    if ($expectedStates -notcontains $actualState) {
        $violations.Add(("unexpected lifecycle state [{0}]" -f $actualState)) | Out-Null
    }
}

foreach ($stateName in $expectedStates) {
    $state = Get-ObjectPropertyValue -Object $states -Name $stateName -Default $null
    if ($null -eq $state) {
        continue
    }

    foreach ($field in $requiredStateFields) {
        if (-not (Test-ObjectProperty -Object $state -Name $field)) {
            $violations.Add(("state [{0}] missing required field [{1}]" -f $stateName, $field)) | Out-Null
        }
    }
}

$frozen = Get-ObjectPropertyValue -Object $states -Name "frozen" -Default $null
$lowered = Get-ObjectPropertyValue -Object $states -Name "lowered" -Default $null
$archived = Get-ObjectPropertyValue -Object $states -Name "archived" -Default $null
$observed = Get-ObjectPropertyValue -Object $states -Name "observed" -Default $null

$frozenStatus = Get-ObjectStringValue -Object $frozen -Name "status"
$frozenProjectionKind = Get-ObjectStringValue -Object $frozen -Name "projection_kind"
$frozenSidecarGap = Get-ObjectStringValue -Object $frozen -Name "sidecar_gap"
$loweredStatus = Get-ObjectStringValue -Object $lowered -Name "status"
$loweredProjectionKind = Get-ObjectStringValue -Object $lowered -Name "projection_kind"
$archivedStatus = Get-ObjectStringValue -Object $archived -Name "status"
$archivedCoverageStrength = Get-ObjectStringValue -Object $archived -Name "coverage_strength"
$archivedProjectionKind = Get-ObjectStringValue -Object $archived -Name "projection_kind"
$observedStatus = Get-ObjectStringValue -Object $observed -Name "status"

Add-IfMismatch -Violations $violations -Label "frozen.status" -Actual $frozenStatus -Expected $RequireFrozenStatus
Add-IfMismatch -Violations $violations -Label "frozen.projection_kind" -Actual $frozenProjectionKind -Expected $RequireFrozenProjectionKind
Add-IfMismatch -Violations $violations -Label "frozen.sidecar_gap" -Actual $frozenSidecarGap -Expected $RequireFrozenSidecarGap
Add-IfMismatch -Violations $violations -Label "lowered.projection_kind" -Actual $loweredProjectionKind -Expected $RequireLoweredProjectionKind
Add-IfMismatch -Violations $violations -Label "archived.coverage_strength" -Actual $archivedCoverageStrength -Expected $RequireArchivedCoverageStrength
Add-IfMismatch -Violations $violations -Label "observed.status" -Actual $observedStatus -Expected $RequireObservedStatus

if ($loweredStatus -eq "present" -and $loweredProjectionKind -ne "interpretive") {
    $violations.Add(("present lowered state must remain interpretive, got [{0}]" -f $loweredProjectionKind)) | Out-Null
}

if ($archivedStatus -eq "present" -and $archivedProjectionKind -ne "interpretive") {
    $violations.Add(("present archived state must remain interpretive, got [{0}]" -f $archivedProjectionKind)) | Out-Null
}

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add(("summary: {0}" -f $summaryPath)) | Out-Null
$lines.Add(("schema: {0}" -f (Get-ObjectStringValue -Object $summaryData -Name "schema"))) | Out-Null
$lines.Add(("kind: {0}" -f (Get-ObjectStringValue -Object $summaryData -Name "kind"))) | Out-Null
$lines.Add(("result: {0}" -f (Get-ObjectStringValue -Object $summaryData -Name "result"))) | Out-Null
$lines.Add(("state_count: {0}" -f (Get-ObjectPropertyValue -Object $summaryData -Name "state_count" -Default ""))) | Out-Null
$lines.Add(("actual_state_count: {0}" -f $actualStateNames.Count)) | Out-Null
$lines.Add(("frozen_status: {0}" -f $frozenStatus)) | Out-Null
$lines.Add(("frozen_projection_kind: {0}" -f $frozenProjectionKind)) | Out-Null
$lines.Add(("frozen_sidecar_gap: {0}" -f $frozenSidecarGap)) | Out-Null
$lines.Add(("lowered_status: {0}" -f $loweredStatus)) | Out-Null
$lines.Add(("lowered_projection_kind: {0}" -f $loweredProjectionKind)) | Out-Null
$lines.Add(("archived_status: {0}" -f $archivedStatus)) | Out-Null
$lines.Add(("archived_coverage_strength: {0}" -f $archivedCoverageStrength)) | Out-Null
$lines.Add(("observed_status: {0}" -f $observedStatus)) | Out-Null
$lines.Add(("summary_violations: {0}" -f $summaryViolations.Count)) | Out-Null
$lines.Add(("check_violations: {0}" -f $violations.Count)) | Out-Null

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
    throw "compiler lifecycle summary gate failed"
}
