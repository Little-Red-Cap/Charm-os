param(
    [string]$OutputRoot = 'out/materialized-graph-ci',
    [string]$CandidateBundleRoot = "",
    [string]$BaselineBundleRoot = "",
    [string]$BaselineIndex = "",
    [string]$CaseManifest = "",
    [string]$Profile = "",
    [string]$Board = "",
    [string[]]$Facet = @(),
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

function Compare-StringArrays {
    param(
        [string[]]$Left,
        [string[]]$Right
    )

    $leftValues = @($Left | ForEach-Object { [string]$_ } | Sort-Object)
    $rightValues = @($Right | ForEach-Object { [string]$_ } | Sort-Object)
    if ($leftValues.Count -ne $rightValues.Count) {
        return $false
    }

    for ($i = 0; $i -lt $leftValues.Count; ++$i) {
        if ($leftValues[$i] -ne $rightValues[$i]) {
            return $false
        }
    }

    return $true
}

function Get-CaseSubjectInfo {
    param(
        $CaseEntry
    )

    $profile = $null
    $board = $null
    $activeFacets = @()

    if ($null -ne $CaseEntry -and $null -ne $CaseEntry.PSObject.Properties['subject']) {
        $subject = $CaseEntry.subject
        if ($null -ne $subject.PSObject.Properties['profile'] -and -not [string]::IsNullOrWhiteSpace([string]$subject.profile)) {
            $profile = [string]$subject.profile
        }
        if ($null -ne $subject.PSObject.Properties['board'] -and -not [string]::IsNullOrWhiteSpace([string]$subject.board)) {
            $board = [string]$subject.board
        }
        if ($null -ne $subject.PSObject.Properties['active_facets']) {
            $activeFacets = @(
                @($subject.active_facets) |
                    Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                    ForEach-Object { [string]$_ }
            )
        }
    }

    return [pscustomobject]@{
        profile = $profile
        board = $board
        active_facets = @($activeFacets)
    }
}

function Get-DerivedSubjectDefaults {
    param(
        [object[]]$CaseEntries
    )

    if (@($CaseEntries).Count -eq 0) {
        return [ordered]@{
            profile = $null
            board = $null
            active_facets = @()
        }
    }

    $subjects = @($CaseEntries | ForEach-Object { Get-CaseSubjectInfo -CaseEntry $_ })
    $first = $subjects[0]
    foreach ($subject in @($subjects | Select-Object -Skip 1)) {
        if ($first.profile -ne $subject.profile) {
            return [ordered]@{
                profile = $null
                board = $null
                active_facets = @()
            }
        }
        if ($first.board -ne $subject.board) {
            return [ordered]@{
                profile = $null
                board = $null
                active_facets = @()
            }
        }
        if (-not (Compare-StringArrays -Left $first.active_facets -Right $subject.active_facets)) {
            return [ordered]@{
                profile = $null
                board = $null
                active_facets = @()
            }
        }
    }

    return [ordered]@{
        profile = $first.profile
        board = $first.board
        active_facets = @($first.active_facets)
    }
}

function Get-BundleInputManifestInfo {
    param(
        $IndexData
    )

    if ($null -eq $IndexData -or $null -eq $IndexData.PSObject.Properties['input_manifest']) {
        return $null
    }

    $manifest = $IndexData.input_manifest
    if ($null -eq $manifest) {
        return $null
    }

    $path = $null
    $schema = $null
    if ($null -ne $manifest.PSObject.Properties['path'] -and -not [string]::IsNullOrWhiteSpace([string]$manifest.path)) {
        $path = [string]$manifest.path
    }
    if ($null -ne $manifest.PSObject.Properties['schema'] -and -not [string]::IsNullOrWhiteSpace([string]$manifest.schema)) {
        $schema = [string]$manifest.schema
    }
    if ([string]::IsNullOrWhiteSpace($path) -and [string]::IsNullOrWhiteSpace($schema)) {
        return $null
    }

    return [ordered]@{
        path = $path
        schema = $schema
    }
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$exportScript = Join-Path $PSScriptRoot 'export_materialized_graph.ps1'
$diffScript = Join-Path $PSScriptRoot 'diff_materialized_graph_bundle.ps1'
$reportScript = Join-Path $PSScriptRoot 'report_materialized_graph_bundle.ps1'
$artifactReportScript = Join-Path $PSScriptRoot 'export_system_compiler_artifact_report.ps1'

foreach ($scriptPath in @($exportScript, $diffScript, $reportScript, $artifactReportScript)) {
    if (-not (Test-Path $scriptPath)) {
        throw "required script not found: $scriptPath"
    }
}

$selection = Get-CaseSelection
$resolvedOutputRoot = Resolve-FullPath $OutputRoot
Ensure-Directory -Path $resolvedOutputRoot

if ($SkipExport -and -not [string]::IsNullOrWhiteSpace($CaseManifest)) {
    throw "-CaseManifest cannot be combined with -SkipExport"
}

$resolvedCandidateBundleRoot = if (-not [string]::IsNullOrWhiteSpace($CandidateBundleRoot)) {
    Resolve-FullPath $CandidateBundleRoot
} else {
    Join-Path $resolvedOutputRoot 'bundle-current'
}

$reportOutputRoot = Join-Path $resolvedOutputRoot 'report'
$artifactReportOutputRoot = Join-Path $resolvedOutputRoot 'artifact-report'
$resolvedSummaryPath = Get-SummaryPath -OutputRootPath $resolvedOutputRoot
$diffJsonPath = Join-Path $resolvedOutputRoot 'materialized_graph_bundle_diff.json'

if ($Clean) {
    if (-not $SkipExport) {
        Remove-PathIfExists -Path $resolvedCandidateBundleRoot
    }
    Remove-PathIfExists -Path $reportOutputRoot
    Remove-PathIfExists -Path $artifactReportOutputRoot
    Remove-PathIfExists -Path $diffJsonPath
    Remove-PathIfExists -Path $resolvedSummaryPath
}

if (-not $SkipExport) {
    $exportArgs = @{
        OutputRoot = $resolvedCandidateBundleRoot
        Jobs = $Jobs
    }
    if (-not [string]::IsNullOrWhiteSpace($CaseManifest)) {
        $exportArgs.CaseManifest = $CaseManifest
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
    subject_defaults = [ordered]@{
        profile = if ([string]::IsNullOrWhiteSpace($Profile)) { $null } else { $Profile }
        board = if ([string]::IsNullOrWhiteSpace($Board)) { $null } else { $Board }
        active_facets = @($Facet)
    }
    case_selection = [ordered]@{
        all_cases = $selection.AllCases
        cases = @($selection.Cases)
    }
    candidate = [ordered]@{
        bundle_root = $resolvedCandidateBundleRoot
        index = $candidateIndexPath
        input_manifest = $null
    }
    baseline = $null
    diff = $null
    report = $null
    artifact_report = $null
}

$candidateIndex = Get-Content -LiteralPath $candidateIndexPath -Raw -Encoding utf8 | ConvertFrom-Json
$selectedCaseNames = @($candidateIndex.cases | ForEach-Object { [string]$_.name })
$summary.candidate.input_manifest = Get-BundleInputManifestInfo -IndexData $candidateIndex
$derivedSubjectDefaults = Get-DerivedSubjectDefaults -CaseEntries @($candidateIndex.cases)
if ([string]::IsNullOrWhiteSpace($Profile)) {
    $summary.subject_defaults.profile = $derivedSubjectDefaults.profile
}
if ([string]::IsNullOrWhiteSpace($Board)) {
    $summary.subject_defaults.board = $derivedSubjectDefaults.board
}
if ($Facet.Count -eq 0) {
    $summary.subject_defaults.active_facets = @($derivedSubjectDefaults.active_facets)
}

function New-ArtifactReportSummary {
    param(
        [string]$OutputRootPath,
        [string[]]$CaseNames
    )

    $entries = @()
    foreach ($caseName in @($CaseNames)) {
        $path = Join-Path $OutputRootPath ($caseName + '.artifact_report.json')
        if (-not (Test-Path $path)) {
            throw "artifact report not found: $path"
        }

        $entries += [ordered]@{
            name = $caseName
            path = $path
        }
    }

    return [ordered]@{
        output_root = $OutputRootPath
        count = $entries.Count
        cases = $entries
    }
}

function Invoke-ArtifactReportExport {
    param(
        [string]$ModeValue,
        [string]$DiffPath = '',
        [string]$ReportManifestPath = ''
    )

    $artifactArgs = @{
        BundleRoot = $resolvedCandidateBundleRoot
        OutputRoot = $artifactReportOutputRoot
        Mode = $ModeValue
    }
    if (-not $selection.AllCases) {
        $artifactArgs.Case = $selection.Cases
    }
    if (-not [string]::IsNullOrWhiteSpace($Profile)) {
        $artifactArgs.Profile = $Profile
    }
    if (-not [string]::IsNullOrWhiteSpace($Board)) {
        $artifactArgs.Board = $Board
    }
    if ($Facet.Count -gt 0) {
        $artifactArgs.Facet = @($Facet)
    }
    if (-not [string]::IsNullOrWhiteSpace($DiffPath)) {
        $artifactArgs.DiffJson = $DiffPath
    }
    if (-not [string]::IsNullOrWhiteSpace($ReportManifestPath)) {
        $artifactArgs.ReportManifest = $ReportManifestPath
    }

    Write-Host "[CI] generate artifact reports -> $artifactReportOutputRoot"
    & $artifactReportScript @artifactArgs
}

if (-not $hasBaseline) {
    Invoke-ArtifactReportExport -ModeValue 'export_only'
    $summary.mode = 'export_only'
    $summary.has_diff = $false
    $summary.status = 'exported'
    $summary.artifact_report = New-ArtifactReportSummary -OutputRootPath $artifactReportOutputRoot -CaseNames $selectedCaseNames
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
    input_manifest = $null
}
$baselineIndexData = Get-Content -LiteralPath ([string]$diffData.left.index) -Raw -Encoding utf8 | ConvertFrom-Json
$summary.baseline.input_manifest = Get-BundleInputManifestInfo -IndexData $baselineIndexData
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

Invoke-ArtifactReportExport -ModeValue 'compare' -DiffPath $diffJsonPath -ReportManifestPath $summary.report.manifest
$summary.artifact_report = New-ArtifactReportSummary -OutputRootPath $artifactReportOutputRoot -CaseNames $selectedCaseNames

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
if ($summary.artifact_report -and $summary.artifact_report.output_root) {
    Write-Host "[CI] artifact reports -> $($summary.artifact_report.output_root)"
}

if ($FailOnDiff -and $hasVisibleDiff) {
    [Console]::Error.WriteLine('materialized graph differences detected')
    exit 2
}
