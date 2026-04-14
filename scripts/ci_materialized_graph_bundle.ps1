param(
    [string]$OutputRoot = 'out/materialized-graph-ci',
    [string]$CandidateBundleRoot = "",
    [string]$BaselineBundleRoot = "",
    [string]$BaselineIndex = "",
    [string[]]$Case = @(),
    [switch]$AllCases,
    [switch]$Clean,
    [int]$Jobs = 8,
    [ValidateSet('markdown', 'html', 'both')]
    [string]$ReportFormat = 'both',
    [switch]$IncludeUnchanged,
    [switch]$SkipExport,
    [switch]$FailOnDiff,
    [string]$SummaryPath = "",
    [string]$Title = 'Materialized Graph CI Report'
)

$ErrorActionPreference = 'Stop'

function Resolve-FullPath {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ''
    }

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

function Ensure-Directory {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    if (-not (Test-Path $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Remove-PathIfExists {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    if (Test-Path $Path) {
        Remove-Item -Recurse -Force $Path
    }
}

function Get-CaseSelection {
    if ($Case.Count -gt 0) {
        return [pscustomobject]@{
            AllCases = $false
            Cases = @($Case)
        }
    }

    if ($AllCases) {
        return [pscustomobject]@{
            AllCases = $true
            Cases = @()
        }
    }

    return [pscustomobject]@{
        AllCases = $true
        Cases = @()
    }
}

function Get-SummaryPath {
    param(
        [string]$OutputRootPath
    )

    if (-not [string]::IsNullOrWhiteSpace($SummaryPath)) {
        return Resolve-FullPath $SummaryPath
    }

    return Join-Path $OutputRootPath 'summary.json'
}

function Get-StatusCount {
    param(
        $Cases,
        [string]$Status
    )

    return @($Cases | Where-Object { $_.status -eq $Status }).Count
}

function Get-CaseNamesByStatus {
    param(
        $Cases,
        [string]$Status
    )

    return @($Cases | Where-Object { $_.status -eq $Status } | ForEach-Object { [string]$_.name })
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$exportScript = Join-Path $PSScriptRoot 'export_materialized_graph.ps1'
$diffScript = Join-Path $PSScriptRoot 'diff_materialized_graph_bundle.ps1'
$reportScript = Join-Path $PSScriptRoot 'report_materialized_graph_bundle.ps1'

foreach ($scriptPath in @($exportScript, $diffScript, $reportScript)) {
    if (-not (Test-Path $scriptPath)) {
        throw "required script not found: $scriptPath"
    }
}

$selection = Get-CaseSelection
$resolvedOutputRoot = Resolve-FullPath $OutputRoot
Ensure-Directory -Path $resolvedOutputRoot

$resolvedCandidateBundleRoot = if (-not [string]::IsNullOrWhiteSpace($CandidateBundleRoot)) {
    Resolve-FullPath $CandidateBundleRoot
} else {
    Join-Path $resolvedOutputRoot 'bundle-current'
}

$reportOutputRoot = Join-Path $resolvedOutputRoot 'report'
$resolvedSummaryPath = Get-SummaryPath -OutputRootPath $resolvedOutputRoot
$diffJsonPath = Join-Path $resolvedOutputRoot 'materialized_graph_bundle_diff.json'

if ($Clean) {
    if (-not $SkipExport) {
        Remove-PathIfExists -Path $resolvedCandidateBundleRoot
    }
    Remove-PathIfExists -Path $reportOutputRoot
    Remove-PathIfExists -Path $diffJsonPath
    Remove-PathIfExists -Path $resolvedSummaryPath
}

if (-not $SkipExport) {
    $exportArgs = @{
        OutputRoot = $resolvedCandidateBundleRoot
        Jobs = $Jobs
    }
    if ($selection.AllCases) {
        $exportArgs.AllCases = $true
    } else {
        $exportArgs.Case = $selection.Cases
    }

    Write-Host "[CI] export candidate bundle -> $resolvedCandidateBundleRoot"
    & $exportScript @exportArgs
}

$candidateIndexPath = Join-Path $resolvedCandidateBundleRoot 'index.json'
if (-not (Test-Path $candidateIndexPath)) {
    throw "candidate bundle index not found: $candidateIndexPath"
}

$hasBaseline = (-not [string]::IsNullOrWhiteSpace($BaselineBundleRoot)) -or (-not [string]::IsNullOrWhiteSpace($BaselineIndex))
$summary = [ordered]@{
    schema = 'materialized_graph.ci_summary/v1'
    generated_at_utc = (Get-Date).ToUniversalTime().ToString('o')
    output_root = $resolvedOutputRoot
    case_selection = [ordered]@{
        all_cases = $selection.AllCases
        cases = @($selection.Cases)
    }
    candidate = [ordered]@{
        bundle_root = $resolvedCandidateBundleRoot
        index = $candidateIndexPath
    }
    baseline = $null
    diff = $null
    report = $null
}

if (-not $hasBaseline) {
    $summary.mode = 'export_only'
    $summary.has_diff = $false
    $summary.status = 'exported'
    Ensure-Directory -Path (Split-Path -Parent $resolvedSummaryPath)
    $summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $resolvedSummaryPath -Encoding utf8
    Write-Host "[CI] export only summary -> $resolvedSummaryPath"
    exit 0
}

$resolvedBaselineBundleRoot = ''
if (-not [string]::IsNullOrWhiteSpace($BaselineBundleRoot)) {
    $resolvedBaselineBundleRoot = Resolve-FullPath $BaselineBundleRoot
}

$diffArgs = @{ AsJson = $true; RightBundleRoot = $resolvedCandidateBundleRoot }
if (-not [string]::IsNullOrWhiteSpace($resolvedBaselineBundleRoot)) {
    $diffArgs.LeftBundleRoot = $resolvedBaselineBundleRoot
}
if (-not [string]::IsNullOrWhiteSpace($BaselineIndex)) {
    $diffArgs.LeftIndex = (Resolve-FullPath $BaselineIndex)
}
if ($selection.Cases.Count -gt 0) {
    $diffArgs.Case = $selection.Cases
}
if ($IncludeUnchanged) {
    $diffArgs.IncludeUnchanged = $true
}

Write-Host '[CI] diff candidate bundle against baseline'
$diffJsonText = (& $diffScript @diffArgs | Out-String)
$diffData = $diffJsonText | ConvertFrom-Json
$diffJsonText | Set-Content -LiteralPath $diffJsonPath -Encoding utf8

$reportArgs = @{
    RightBundleRoot = $resolvedCandidateBundleRoot
    OutputDir = $reportOutputRoot
    Format = $ReportFormat
    Title = $Title
}
if (-not [string]::IsNullOrWhiteSpace($resolvedBaselineBundleRoot)) {
    $reportArgs.LeftBundleRoot = $resolvedBaselineBundleRoot
}
if (-not [string]::IsNullOrWhiteSpace($BaselineIndex)) {
    $reportArgs.LeftIndex = (Resolve-FullPath $BaselineIndex)
}
if ($selection.Cases.Count -gt 0) {
    $reportArgs.Case = $selection.Cases
}
if ($IncludeUnchanged) {
    $reportArgs.IncludeUnchanged = $true
}

Write-Host '[CI] generate human-readable report'
& $reportScript @reportArgs

$reportMarkdownPath = if ($ReportFormat -eq 'markdown' -or $ReportFormat -eq 'both') { Join-Path $reportOutputRoot 'materialized_graph_bundle_diff_report.md' } else { '' }
$reportHtmlPath = if ($ReportFormat -eq 'html' -or $ReportFormat -eq 'both') { Join-Path $reportOutputRoot 'materialized_graph_bundle_diff_report.html' } else { '' }
$reportManifestPath = Join-Path $reportOutputRoot 'materialized_graph_bundle_diff_report.manifest.json'
$changedCases = @(Get-CaseNamesByStatus -Cases $diffData.cases -Status 'changed')
$addedCases = @(Get-CaseNamesByStatus -Cases $diffData.cases -Status 'added')
$removedCases = @(Get-CaseNamesByStatus -Cases $diffData.cases -Status 'removed')
$unchangedCases = @(Get-CaseNamesByStatus -Cases $diffData.cases -Status 'unchanged')
$hasVisibleDiff = ($changedCases.Count + $addedCases.Count + $removedCases.Count) -gt 0

$summary.mode = 'compare'
$summary.has_diff = $hasVisibleDiff
$summary.status = if ($hasVisibleDiff) { 'different' } else { 'same' }
$summary.baseline = [ordered]@{
    bundle_root = if (-not [string]::IsNullOrWhiteSpace($resolvedBaselineBundleRoot)) { $resolvedBaselineBundleRoot } else { Split-Path -Parent ([string]$diffData.left.index) }
    index = [string]$diffData.left.index
}
$summary.diff = [ordered]@{
    json = $diffJsonPath
    case_count = [int]$diffData.case_count
    include_unchanged = $IncludeUnchanged.IsPresent
    status_counts = [ordered]@{
        changed = Get-StatusCount -Cases $diffData.cases -Status 'changed'
        added = Get-StatusCount -Cases $diffData.cases -Status 'added'
        removed = Get-StatusCount -Cases $diffData.cases -Status 'removed'
        unchanged = Get-StatusCount -Cases $diffData.cases -Status 'unchanged'
    }
    changed_cases = $changedCases
    added_cases = $addedCases
    removed_cases = $removedCases
    unchanged_cases = $unchangedCases
}
$summary.report = [ordered]@{
    output_root = $reportOutputRoot
    manifest = if (Test-Path $reportManifestPath) { $reportManifestPath } else { $null }
    markdown = if (-not [string]::IsNullOrWhiteSpace($reportMarkdownPath) -and (Test-Path $reportMarkdownPath)) { $reportMarkdownPath } else { $null }
    html = if (-not [string]::IsNullOrWhiteSpace($reportHtmlPath) -and (Test-Path $reportHtmlPath)) { $reportHtmlPath } else { $null }
}

Ensure-Directory -Path (Split-Path -Parent $resolvedSummaryPath)
$summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $resolvedSummaryPath -Encoding utf8

Write-Host "[CI] summary -> $resolvedSummaryPath"
Write-Host "[CI] diff    -> $diffJsonPath"
if ($summary.report.manifest) {
    Write-Host "[CI] report  -> $($summary.report.manifest)"
}
if ($summary.report.markdown) {
    Write-Host "[CI] report  -> $($summary.report.markdown)"
}
if ($summary.report.html) {
    Write-Host "[CI] report  -> $($summary.report.html)"
}

if ($FailOnDiff -and $hasVisibleDiff) {
    [Console]::Error.WriteLine('materialized graph differences detected')
    exit 2
}
