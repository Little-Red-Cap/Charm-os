param(
    [string]$Summary = "",
    [string]$OutputPath = "",
    [string]$RequireResult = "ok",
    [int]$RequireBiographyCount = -1,
    [int]$RequireUniqueWorldCount = -1,
    [int]$RequireOkCount = -1,
    [int]$MaxFailCount = -1,
    [int]$RequireCompareAttachedCount = -1,
    [int]$RequireNotAttachedCount = -1,
    [int]$RequireStandingCount = -1,
    [int]$RequireImprovedCount = -1,
    [int]$RequireDriftedCount = -1,
    [int]$RequireCollapsedCount = -1
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
    Resolve-FullPath -Path "out/system-compiler-biography-index/biography.index.summary.json"
} else {
    Resolve-FullPath -Path $Summary
}

if (-not (Test-Path $summaryPath)) {
    throw "system compiler biography index summary not found: $summaryPath"
}

$summaryData = Get-Content -LiteralPath $summaryPath -Raw -Encoding utf8 | ConvertFrom-Json
$summaryBlock = $summaryData.summary

$result = [string]$summaryData.result
$biographyCount = [int]$summaryBlock.biography_count
$uniqueWorldCount = [int]$summaryBlock.unique_world_count
$okCount = [int]$summaryBlock.ok_count
$failCount = [int]$summaryBlock.fail_count
$compareAttachedCount = [int]$summaryBlock.compare_attached_count
$notAttachedCount = [int]$summaryBlock.not_attached_count
$standingCount = [int]$summaryBlock.standing_count
$improvedCount = [int]$summaryBlock.improved_count
$driftedCount = [int]$summaryBlock.drifted_count
$collapsedCount = [int]$summaryBlock.collapsed_count
$nextQuestionCount = @($summaryData.questions.next_questions).Count

$violations = [System.Collections.Generic.List[string]]::new()

if (-not [string]::IsNullOrWhiteSpace($RequireResult) -and $result -ne $RequireResult) {
    $violations.Add(('expected result `{0}` but got `{1}`' -f $RequireResult, $result)) | Out-Null
}
if ($RequireBiographyCount -ge 0 -and $biographyCount -ne $RequireBiographyCount) {
    $violations.Add(("biography_count expected {0} but got {1}" -f $RequireBiographyCount, $biographyCount)) | Out-Null
}
if ($RequireUniqueWorldCount -ge 0 -and $uniqueWorldCount -ne $RequireUniqueWorldCount) {
    $violations.Add(("unique_world_count expected {0} but got {1}" -f $RequireUniqueWorldCount, $uniqueWorldCount)) | Out-Null
}
if ($RequireOkCount -ge 0 -and $okCount -ne $RequireOkCount) {
    $violations.Add(("ok_count expected {0} but got {1}" -f $RequireOkCount, $okCount)) | Out-Null
}
if ($MaxFailCount -ge 0 -and $failCount -gt $MaxFailCount) {
    $violations.Add(("fail_count {0} exceeds max {1}" -f $failCount, $MaxFailCount)) | Out-Null
}
if ($RequireCompareAttachedCount -ge 0 -and $compareAttachedCount -ne $RequireCompareAttachedCount) {
    $violations.Add(("compare_attached_count expected {0} but got {1}" -f $RequireCompareAttachedCount, $compareAttachedCount)) | Out-Null
}
if ($RequireNotAttachedCount -ge 0 -and $notAttachedCount -ne $RequireNotAttachedCount) {
    $violations.Add(("not_attached_count expected {0} but got {1}" -f $RequireNotAttachedCount, $notAttachedCount)) | Out-Null
}
if ($RequireStandingCount -ge 0 -and $standingCount -ne $RequireStandingCount) {
    $violations.Add(("standing_count expected {0} but got {1}" -f $RequireStandingCount, $standingCount)) | Out-Null
}
if ($RequireImprovedCount -ge 0 -and $improvedCount -ne $RequireImprovedCount) {
    $violations.Add(("improved_count expected {0} but got {1}" -f $RequireImprovedCount, $improvedCount)) | Out-Null
}
if ($RequireDriftedCount -ge 0 -and $driftedCount -ne $RequireDriftedCount) {
    $violations.Add(("drifted_count expected {0} but got {1}" -f $RequireDriftedCount, $driftedCount)) | Out-Null
}
if ($RequireCollapsedCount -ge 0 -and $collapsedCount -ne $RequireCollapsedCount) {
    $violations.Add(("collapsed_count expected {0} but got {1}" -f $RequireCollapsedCount, $collapsedCount)) | Out-Null
}

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add(("summary: {0}" -f $summaryPath)) | Out-Null
$lines.Add(("result: {0}" -f $result)) | Out-Null
$lines.Add(("biography_count: {0}" -f $biographyCount)) | Out-Null
$lines.Add(("unique_world_count: {0}" -f $uniqueWorldCount)) | Out-Null
$lines.Add(("ok_count: {0}" -f $okCount)) | Out-Null
$lines.Add(("fail_count: {0}" -f $failCount)) | Out-Null
$lines.Add(("compare_attached_count: {0}" -f $compareAttachedCount)) | Out-Null
$lines.Add(("not_attached_count: {0}" -f $notAttachedCount)) | Out-Null
$lines.Add(("standing_count: {0}" -f $standingCount)) | Out-Null
$lines.Add(("improved_count: {0}" -f $improvedCount)) | Out-Null
$lines.Add(("drifted_count: {0}" -f $driftedCount)) | Out-Null
$lines.Add(("collapsed_count: {0}" -f $collapsedCount)) | Out-Null
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
    throw "system compiler biography index summary gate failed"
}
