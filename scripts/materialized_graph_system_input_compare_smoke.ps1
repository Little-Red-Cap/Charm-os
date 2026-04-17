param(
    [string]$BundleRoot = 'out/materialized-graph-ci/bundle-current',
    [string]$ChangedCase = 'bringup-minimal-observe-demo',
    [string]$ExpectedUnchangedCase = 'bringup-block-observe-demo',
    [string]$AddedDeclaredFact = 'synthetic.system_input_compare',
    [string]$OutputRoot = 'out/materialized-graph-system-input-compare-smoke',
    [switch]$KeepOutput
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
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
}

function Assert-Condition {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Get-CaseEntry {
    param(
        [object[]]$Cases,
        [string]$CaseName
    )

    return @(
        $Cases |
            Where-Object { [string]$_.name -eq $CaseName } |
            Select-Object -First 1
    ) | Select-Object -First 1
}

function Get-CaseSummaryRow {
    param(
        [object[]]$Rows,
        [string]$CaseName
    )

    return @(
        @($Rows) |
            Where-Object { [string]$_.Case -eq $CaseName } |
            Select-Object -First 1
    ) | Select-Object -First 1
}

function Set-ObjectProperty {
    param(
        $Object,
        [string]$Name,
        $Value
    )

    $property = $Object.PSObject.Properties[$Name]
    if ($null -ne $property) {
        $property.Value = $Value
        return
    }

    $Object | Add-Member -NotePropertyName $Name -NotePropertyValue $Value
}

function Invoke-CommandJson {
    param(
        [scriptblock]$Command,
        [string]$OutputPath
    )

    $jsonText = (& $Command | Out-String)
    $jsonText | Set-Content -LiteralPath $OutputPath -Encoding utf8
    return ($jsonText | ConvertFrom-Json)
}

$resolvedBundleRoot = Resolve-FullPath $BundleRoot
$resolvedOutputRoot = Resolve-FullPath $OutputRoot
$leftBundleRoot = Join-Path $resolvedOutputRoot 'left-bundle'
$rightBundleRoot = Join-Path $resolvedOutputRoot 'right-bundle'
$diffJsonPath = Join-Path $resolvedOutputRoot 'system_input_compare.diff.json'
$reportOutputRoot = Join-Path $resolvedOutputRoot 'report'
$artifactReportOutputRoot = Join-Path $resolvedOutputRoot 'artifact-report'
$reportInspectJsonPath = Join-Path $resolvedOutputRoot 'system_input_compare.report.inspect.json'
$rootSummaryInspectJsonPath = Join-Path $resolvedOutputRoot 'system_input_compare.summary.inspect.json'
$summaryPath = Join-Path $resolvedOutputRoot 'system_input_compare_smoke.summary.json'

$diffScript = Join-Path $PSScriptRoot 'diff_materialized_graph_bundle.ps1'
$reportScript = Join-Path $PSScriptRoot 'report_materialized_graph_bundle.ps1'
$artifactReportScript = Join-Path $PSScriptRoot 'export_system_compiler_artifact_report.ps1'
$inspectScript = Join-Path $PSScriptRoot 'inspect_system_compiler_artifact_report.ps1'

foreach ($requiredPath in @($resolvedBundleRoot, $diffScript, $reportScript, $artifactReportScript, $inspectScript)) {
    if (-not (Test-Path $requiredPath)) {
        throw "required path not found: $requiredPath"
    }
}

if (-not $KeepOutput) {
    Remove-PathIfExists -Path $resolvedOutputRoot
}
Ensure-Directory -Path $resolvedOutputRoot
Ensure-Directory -Path $reportOutputRoot

Copy-Item -LiteralPath $resolvedBundleRoot -Destination $leftBundleRoot -Recurse -Force
Copy-Item -LiteralPath $resolvedBundleRoot -Destination $rightBundleRoot -Recurse -Force

$rightIndexPath = Join-Path $rightBundleRoot 'index.json'
$rightIndex = Get-Content -LiteralPath $rightIndexPath -Raw -Encoding utf8 | ConvertFrom-Json
$rightCase = Get-CaseEntry -Cases @($rightIndex.cases) -CaseName $ChangedCase
$unchangedCase = Get-CaseEntry -Cases @($rightIndex.cases) -CaseName $ExpectedUnchangedCase
Assert-Condition ($null -ne $rightCase) "case not found in synthetic bundle: $ChangedCase"
Assert-Condition ($null -ne $unchangedCase) "case not found in synthetic bundle: $ExpectedUnchangedCase"

$declaredFacts = @(
    @($rightCase.declared_facts) |
        Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
        ForEach-Object { [string]$_ }
)
if (@($declaredFacts) -notcontains $AddedDeclaredFact) {
    $declaredFacts += $AddedDeclaredFact
}
Set-ObjectProperty -Object $rightCase -Name 'declared_facts' -Value @($declaredFacts | Sort-Object -Unique)
$rightIndex | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $rightIndexPath -Encoding utf8

Write-Host "[SYNTH] case=$ChangedCase add declared_fact=$AddedDeclaredFact"

$diffData = Invoke-CommandJson -OutputPath $diffJsonPath -Command {
    & $diffScript -LeftBundleRoot $leftBundleRoot -RightBundleRoot $rightBundleRoot -IncludeUnchanged -AsJson
}
Assert-Condition ([string]$diffData.schema -eq 'materialized_graph.bundle_diff/v1') 'unexpected diff schema'
Assert-Condition ([int]$diffData.case_count -ge 2) 'system input compare smoke expects at least two diff cases'

$changedCaseDiff = @(
    @($diffData.cases) |
        Where-Object { [string]$_.name -eq $ChangedCase } |
        Select-Object -First 1
) | Select-Object -First 1
$unchangedCaseDiff = @(
    @($diffData.cases) |
        Where-Object { [string]$_.name -eq $ExpectedUnchangedCase } |
        Select-Object -First 1
) | Select-Object -First 1
Assert-Condition ($null -ne $changedCaseDiff) "diff case missing: $ChangedCase"
Assert-Condition ($null -ne $unchangedCaseDiff) "diff case missing: $ExpectedUnchangedCase"
Assert-Condition ([string]$changedCaseDiff.status -eq 'unchanged') 'system input compare changed case must preserve unchanged diff status for metadata-only drift'
Assert-Condition ([string]$unchangedCaseDiff.status -eq 'unchanged') 'system input compare unchanged case must stay unchanged'
Assert-Condition ((@($changedCaseDiff.metadata_changes) | Where-Object { [string]$_ -like 'declared_facts:*' }).Count -gt 0) 'metadata_changes must mention declared_facts for synthetic input drift'

& $reportScript -LeftBundleRoot $leftBundleRoot -RightBundleRoot $rightBundleRoot -IncludeUnchanged -Format markdown -OutputDir $reportOutputRoot
if ($LASTEXITCODE -ne 0) {
    throw 'report generation failed'
}

$reportManifestPath = Join-Path $reportOutputRoot 'materialized_graph_bundle_diff_report.manifest.json'
Assert-Condition (Test-Path $reportManifestPath) "report manifest not found: $reportManifestPath"

& $artifactReportScript `
    -BundleRoot $rightBundleRoot `
    -OutputRoot $artifactReportOutputRoot `
    -Mode compare `
    -DiffJson $diffJsonPath `
    -ReportManifest $reportManifestPath
if ($LASTEXITCODE -ne 0) {
    throw 'artifact report compare export failed'
}

$artifactReportPath = Join-Path $artifactReportOutputRoot ($ChangedCase + '.artifact_report.json')
Assert-Condition (Test-Path $artifactReportPath) "artifact report not found: $artifactReportPath"
$artifactReport = Get-Content -LiteralPath $artifactReportPath -Raw -Encoding utf8 | ConvertFrom-Json

Assert-Condition ([string]$artifactReport.mode -eq 'compare') 'artifact report mode must be compare'
Assert-Condition ($null -ne $artifactReport.comparison) 'artifact report comparison is missing'
Assert-Condition ([string]$artifactReport.comparison.status -eq 'unchanged') 'artifact report comparison.status must preserve metadata-only unchanged'
Assert-Condition ((@($artifactReport.comparison.metadata_changes) | Where-Object { [string]$_ -like 'declared_facts:*' }).Count -gt 0) 'artifact report comparison must preserve declared_facts metadata change'
Assert-Condition ($null -ne $artifactReport.comparison.system_input) 'artifact report comparison.system_input is missing'
Assert-Condition ([bool]$artifactReport.comparison.system_input.changed) 'system_input comparison must be marked changed'

$systemInputComparison = $artifactReport.comparison.system_input
Assert-Condition ((@($systemInputComparison.declared_fact_changes.added) -contains $AddedDeclaredFact)) 'system_input declared_fact_changes.added missing synthetic fact'
Assert-Condition ((@($systemInputComparison.left.declared_input.declared_facts) -notcontains $AddedDeclaredFact)) 'system_input left declared_input must not contain synthetic fact'
Assert-Condition ((@($systemInputComparison.right.declared_input.declared_facts) -contains $AddedDeclaredFact)) 'system_input right declared_input must contain synthetic fact'
Assert-Condition ((@($systemInputComparison.summary_changes) | Where-Object { [string]$_ -like 'declared_facts:*' }).Count -gt 0) 'system_input summary_changes must mention declared_facts drift'

$reportInspectResult = Invoke-CommandJson -OutputPath $reportInspectJsonPath -Command {
    & $inspectScript -ArtifactRoot $artifactReportOutputRoot -Case $ChangedCase -AsJson
}
Assert-Condition ([string]$reportInspectResult.summary.Case -eq $ChangedCase) 'report inspect summary case mismatch'
Assert-Condition ([string]$reportInspectResult.summary.Compare -eq 'unchanged') 'report inspect Compare must stay unchanged for metadata-only input drift'
Assert-Condition ([int]$reportInspectResult.summary.InpCmp -gt 0) 'report inspect InpCmp must be nonzero for input drift'
Assert-Condition ($null -ne $reportInspectResult.comparison) 'report inspect comparison payload is missing'
Assert-Condition ([bool]$reportInspectResult.comparison.system_input.changed) 'report inspect comparison.system_input must be changed'

$rootSummaryInspectResult = Invoke-CommandJson -OutputPath $rootSummaryInspectJsonPath -Command {
    & $inspectScript -ArtifactRoot $artifactReportOutputRoot -AsJson
}
Assert-Condition ([int]$rootSummaryInspectResult.comparison.compared_case_count -ge 2) 'artifact_root summary compared_case_count must be at least 2'
Assert-Condition ([int]$rootSummaryInspectResult.comparison.input_changed_case_count -eq 1) 'artifact_root summary input_changed_case_count must be 1'
Assert-Condition ((@($rootSummaryInspectResult.comparison.input_changed_cases) -contains $ChangedCase)) 'artifact_root summary missing input changed case'
Assert-Condition ((@($rootSummaryInspectResult.comparison.input_changed_cases) -notcontains $ExpectedUnchangedCase)) 'artifact_root summary incorrectly marks unchanged case as input changed'

$changedCaseSummary = Get-CaseSummaryRow -Rows @($rootSummaryInspectResult.cases) -CaseName $ChangedCase
$unchangedCaseSummary = Get-CaseSummaryRow -Rows @($rootSummaryInspectResult.cases) -CaseName $ExpectedUnchangedCase
Assert-Condition ($null -ne $changedCaseSummary) "artifact_root summary missing case row: $ChangedCase"
Assert-Condition ($null -ne $unchangedCaseSummary) "artifact_root summary missing case row: $ExpectedUnchangedCase"
Assert-Condition ([int]$changedCaseSummary.InpCmp -gt 0) 'artifact_root summary changed case InpCmp must be nonzero'
Assert-Condition ([int]$unchangedCaseSummary.InpCmp -eq 0) 'artifact_root summary unchanged case InpCmp must stay zero'

$summary = [ordered]@{
    left_bundle_root = $leftBundleRoot
    right_bundle_root = $rightBundleRoot
    changed_case = $ChangedCase
    unchanged_case = $ExpectedUnchangedCase
    added_declared_fact = $AddedDeclaredFact
    diff_json = $diffJsonPath
    artifact_report_root = $artifactReportOutputRoot
    captures = [ordered]@{
        report_summary = $reportInspectJsonPath
        root_summary = $rootSummaryInspectJsonPath
    }
    assertions = [ordered]@{
        metadata_only_diff_preserved = $true
        comparison_system_input_present = $true
        declared_fact_drift_detected = $true
        default_report_summary_exposes_input_compare = $true
        default_root_summary_exposes_input_changed_counts = $true
    }
}
$summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $summaryPath -Encoding utf8

Write-Host "[OK] system input compare smoke passed"
Write-Host "[ARTIFACT] $artifactReportOutputRoot"
Write-Host "[SUMMARY]  $summaryPath"
