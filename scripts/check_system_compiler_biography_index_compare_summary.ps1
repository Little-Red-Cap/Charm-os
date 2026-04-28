param(
    [string]$Summary = "",
    [string]$OutputPath = "",
    [string]$RequireResult = "ok",
    [string]$RequireVerdict = "",
    [int]$MaxRegressions = -1,
    [int]$RequireAddedEntries = -1,
    [int]$RequireRemovedEntries = -1,
    [int]$RequireChangedEntries = -1,
    [int]$RequireImprovementCount = -1,
    [int]$RequireAddedWorlds = -1,
    [int]$RequireRemovedWorlds = -1,
    [int]$MaxAddedFailedEntries = -1
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
    Resolve-FullPath -Path "out/system-compiler-biography-index-compare/summary.json"
} else {
    Resolve-FullPath -Path $Summary
}

if (-not (Test-Path $summaryPath)) {
    throw "system compiler biography index compare summary not found: $summaryPath"
}

$summaryData = Get-Content -LiteralPath $summaryPath -Raw -Encoding utf8 | ConvertFrom-Json
$entrySummary = $summaryData.entry_summary
$shelfChanges = $summaryData.shelf_changes
$collapseSurface = $summaryData.collapse_surface

$result = [string]$summaryData.result
$verdict = [string]$summaryData.shelf_verdict
$changedEntries = [int]$entrySummary.changed_entry_count
$addedEntries = [int]$entrySummary.added_entry_count
$removedEntries = [int]$entrySummary.removed_entry_count
$regressionCount = [int]$entrySummary.regression_count
$improvementCount = [int]$entrySummary.improvement_count
$addedWorlds = @($shelfChanges.world_name_changes.added).Count
$removedWorlds = @($shelfChanges.world_name_changes.removed).Count
$addedFailedEntries = @($collapseSurface.added_failed_entries).Count
$nextQuestionCount = @($summaryData.questions.next_questions).Count

$violations = [System.Collections.Generic.List[string]]::new()

if (-not [string]::IsNullOrWhiteSpace($RequireResult) -and $result -ne $RequireResult) {
    $violations.Add(('expected result `{0}` but got `{1}`' -f $RequireResult, $result)) | Out-Null
}
if (-not [string]::IsNullOrWhiteSpace($RequireVerdict) -and $verdict -ne $RequireVerdict) {
    $violations.Add(('expected shelf verdict `{0}` but got `{1}`' -f $RequireVerdict, $verdict)) | Out-Null
}
if ($MaxRegressions -ge 0 -and $regressionCount -gt $MaxRegressions) {
    $violations.Add(("regression_count {0} exceeds max {1}" -f $regressionCount, $MaxRegressions)) | Out-Null
}
if ($RequireAddedEntries -ge 0 -and $addedEntries -ne $RequireAddedEntries) {
    $violations.Add(("added_entry_count expected {0} but got {1}" -f $RequireAddedEntries, $addedEntries)) | Out-Null
}
if ($RequireRemovedEntries -ge 0 -and $removedEntries -ne $RequireRemovedEntries) {
    $violations.Add(("removed_entry_count expected {0} but got {1}" -f $RequireRemovedEntries, $removedEntries)) | Out-Null
}
if ($RequireChangedEntries -ge 0 -and $changedEntries -ne $RequireChangedEntries) {
    $violations.Add(("changed_entry_count expected {0} but got {1}" -f $RequireChangedEntries, $changedEntries)) | Out-Null
}
if ($RequireImprovementCount -ge 0 -and $improvementCount -ne $RequireImprovementCount) {
    $violations.Add(("improvement_count expected {0} but got {1}" -f $RequireImprovementCount, $improvementCount)) | Out-Null
}
if ($RequireAddedWorlds -ge 0 -and $addedWorlds -ne $RequireAddedWorlds) {
    $violations.Add(("added_world_count expected {0} but got {1}" -f $RequireAddedWorlds, $addedWorlds)) | Out-Null
}
if ($RequireRemovedWorlds -ge 0 -and $removedWorlds -ne $RequireRemovedWorlds) {
    $violations.Add(("removed_world_count expected {0} but got {1}" -f $RequireRemovedWorlds, $removedWorlds)) | Out-Null
}
if ($MaxAddedFailedEntries -ge 0 -and $addedFailedEntries -gt $MaxAddedFailedEntries) {
    $violations.Add(("added_failed_entries {0} exceeds max {1}" -f $addedFailedEntries, $MaxAddedFailedEntries)) | Out-Null
}

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add(("summary: {0}" -f $summaryPath)) | Out-Null
$lines.Add(("result: {0}" -f $result)) | Out-Null
$lines.Add(("shelf_verdict: {0}" -f $verdict)) | Out-Null
$lines.Add(("changed_entry_count: {0}" -f $changedEntries)) | Out-Null
$lines.Add(("added_entry_count: {0}" -f $addedEntries)) | Out-Null
$lines.Add(("removed_entry_count: {0}" -f $removedEntries)) | Out-Null
$lines.Add(("regression_count: {0}" -f $regressionCount)) | Out-Null
$lines.Add(("improvement_count: {0}" -f $improvementCount)) | Out-Null
$lines.Add(("added_world_count: {0}" -f $addedWorlds)) | Out-Null
$lines.Add(("removed_world_count: {0}" -f $removedWorlds)) | Out-Null
$lines.Add(("added_failed_entries: {0}" -f $addedFailedEntries)) | Out-Null
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
    throw "system compiler biography index compare summary gate failed"
}
