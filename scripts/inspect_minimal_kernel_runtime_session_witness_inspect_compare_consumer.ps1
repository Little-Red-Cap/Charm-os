param(
    [string]$Summary = "",
    [string]$FocusId = "",
    [switch]$ShowArtifacts,
    [switch]$ShowFallbacks,
    [switch]$AsJson
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

function Get-SummaryPath {
    param(
        [string]$Path
    )

    if (-not [string]::IsNullOrWhiteSpace($Path)) {
        return Resolve-FullPath -Path $Path
    }

    $repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
    return Join-Path $repoRoot "out\minimal-kernel-runtime-session-witness-inspect-compare-consumer\session-witness.inspect.compare.consumer.summary.json"
}

function Get-StringArray {
    param(
        [AllowNull()]
        [object[]]$Values
    )

    return @(
        @($Values) |
            Where-Object { $null -ne $_ -and -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [string]$_ }
    )
}

function Format-StringArrayOrDash {
    param(
        [AllowNull()]
        [object[]]$Values
    )

    $items = Get-StringArray -Values $Values
    if ($items.Count -eq 0) {
        return "-"
    }

    return ($items -join ",")
}

function Format-BoolToken {
    param(
        [bool]$Value
    )

    if ($Value) {
        return "true"
    }

    return "false"
}

function Load-ConsumerSummary {
    param(
        [string]$Path
    )

    if (-not (Test-Path $Path)) {
        throw "consumer summary not found: $Path"
    }

    $data = Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json
    if ([string]$data.schema -ne "minimal_kernel.runtime_session_witness_inspect_compare_consumer/v0") {
        throw "unsupported inspect compare consumer schema: $([string]$data.schema)"
    }

    return $data
}

function Find-FocusEntry {
    param(
        $SummaryData,
        [string]$RequestedFocusId
    )

    $entries = @($SummaryData.focus_entries)
    if ([string]::IsNullOrWhiteSpace($RequestedFocusId)) {
        return $SummaryData.default_focus
    }

    foreach ($entry in $entries) {
        if ([string]$entry.focus_id -eq $RequestedFocusId) {
            return $entry
        }
    }

    throw "focus not found: $RequestedFocusId"
}

function Find-ExplainHop {
    param(
        $SummaryData,
        $FocusEntry
    )

    if ($null -ne $FocusEntry -and $null -ne $FocusEntry.PSObject.Properties["preferred_explain_hop"]) {
        return $FocusEntry.preferred_explain_hop
    }

    if ($null -ne $SummaryData.default_focus -and [string]$SummaryData.default_focus.focus_id -eq [string]$FocusEntry.focus_id) {
        return $SummaryData.default_explain_hop
    }

    foreach ($hop in @($SummaryData.fallback_explain_hops)) {
        if ([string]$hop.focus_id -eq [string]$FocusEntry.focus_id) {
            return $hop
        }
    }

    return $null
}

function New-InspectView {
    param(
        [string]$SummaryPath,
        $SummaryData,
        $SelectedFocus,
        $SelectedExplainHop
    )

    return [pscustomobject][ordered]@{
        summary_path = $SummaryPath
        result = [string]$SummaryData.result
        consumer_status = $SummaryData.consumer_status
        default_focus = $SummaryData.default_focus
        default_explain_hop = $SummaryData.default_explain_hop
        selected_focus = $SelectedFocus
        selected_explain_hop = $SelectedExplainHop
        fallback_explain_hops = @($SummaryData.fallback_explain_hops)
        supporting_artifacts = @($SummaryData.supporting_artifacts)
        questions = $SummaryData.questions
        violations = @(Get-StringArray -Values $SummaryData.violations)
    }
}

$summaryPath = Get-SummaryPath -Path $Summary
$summaryData = Load-ConsumerSummary -Path $summaryPath
$selectedFocus = Find-FocusEntry -SummaryData $summaryData -RequestedFocusId $FocusId
$selectedExplainHop = Find-ExplainHop -SummaryData $summaryData -FocusEntry $selectedFocus
$inspectView = New-InspectView `
    -SummaryPath $summaryPath `
    -SummaryData $summaryData `
    -SelectedFocus $selectedFocus `
    -SelectedExplainHop $selectedExplainHop

if ($AsJson) {
    $inspectView | ConvertTo-Json -Depth 16
    exit 0
}

Write-Output ("summary: {0}" -f $summaryPath)
Write-Output ("result: {0}" -f [string]$summaryData.result)
Write-Output ("focus_counts: total={0} changed={1} actionable={2}" -f `
    [int]$summaryData.consumer_status.total_focus_count,
    [int]$summaryData.consumer_status.changed_focus_count,
    [int]$summaryData.consumer_status.actionable_focus_count)
Write-Output ("default_focus: {0} kind={1} severity={2}" -f `
    [string]$summaryData.default_focus.focus_id,
    [string]$summaryData.default_focus.focus_kind,
    [string]$summaryData.default_focus.severity)
Write-Output ("default_explain_hop: artifact={0} reason={1}" -f `
    [string]$summaryData.default_explain_hop.artifact_ref.id,
    [string]$summaryData.default_explain_hop.reason_kind)
Write-Output ("selected_focus: {0} kind={1} severity={2} changed={3}" -f `
    [string]$selectedFocus.focus_id,
    [string]$selectedFocus.focus_kind,
    [string]$selectedFocus.severity,
    (Format-BoolToken -Value ([bool]$selectedFocus.changed)))
Write-Output ("selected_focus_runtime: regressed={0} improved={1}" -f `
    (Format-StringArrayOrDash -Values $selectedFocus.runtime_regressions),
    (Format-StringArrayOrDash -Values $selectedFocus.runtime_improvements))
Write-Output ("selected_focus_failures: added={0} removed={1}" -f `
    (Format-StringArrayOrDash -Values $selectedFocus.added_failure_codes),
    (Format-StringArrayOrDash -Values $selectedFocus.removed_failure_codes))
Write-Output ("selected_focus_missing_runtime_facts: added={0} removed={1}" -f `
    (Format-StringArrayOrDash -Values $selectedFocus.added_missing_runtime_facts),
    (Format-StringArrayOrDash -Values $selectedFocus.removed_missing_runtime_facts))
Write-Output ("selected_explain_hop: artifact={0} reason={1}" -f `
    [string]$selectedExplainHop.artifact_ref.id,
    [string]$selectedExplainHop.reason_kind)
Write-Output ("selected_explain_path: {0}" -f [string]$selectedExplainHop.artifact_ref.path)
Write-Output ("selected_headline: {0}" -f [string]$selectedFocus.headline)
foreach ($line in @(Get-StringArray -Values $selectedFocus.summary_lines)) {
    Write-Output ("selected_summary: {0}" -f $line)
}
foreach ($question in @(Get-StringArray -Values $selectedFocus.question_lines)) {
    Write-Output ("selected_question: {0}" -f $question)
}

if ($ShowFallbacks) {
    Write-Output ("fallback_explain_hops: {0}" -f [int]@($summaryData.fallback_explain_hops).Count)
    foreach ($hop in @($summaryData.fallback_explain_hops)) {
        Write-Output ("fallback_explain_hop: focus={0} artifact={1} reason={2}" -f `
            [string]$hop.focus_id,
            [string]$hop.artifact_ref.id,
            [string]$hop.reason_kind)
    }

    foreach ($fallbackRef in @($selectedExplainHop.fallback_artifact_refs)) {
        Write-Output ("selected_explain_fallback: artifact={0} path={1}" -f `
            [string]$fallbackRef.id,
            [string]$fallbackRef.path)
    }
}

if ($ShowArtifacts) {
    Write-Output "focus_artifacts:"
    foreach ($artifactRef in @($selectedFocus.artifact_refs)) {
        Write-Output ("  artifact={0} path={1}" -f [string]$artifactRef.id, [string]$artifactRef.path)
    }

    Write-Output "supporting_artifacts:"
    foreach ($artifactRef in @($summaryData.supporting_artifacts)) {
        Write-Output ("  artifact={0} path={1}" -f [string]$artifactRef.id, [string]$artifactRef.path)
    }
}
