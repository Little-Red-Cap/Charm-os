param(
    [string]$BundleRoot = 'out/materialized-graph-ci/bundle-current',
    [string]$ChangedCase = 'bringup-minimal-observe-demo',
    [string]$ExpectedUnchangedCase = 'bringup-block-observe-demo',
    [string]$AddedContract = 'needs_heap',
    [string]$AddedRequiredFact = 'system.heap',
    [string]$OutputRoot = 'out/materialized-graph-resource-contract-compare-root-smoke',
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
$diffJsonPath = Join-Path $resolvedOutputRoot 'resource_contract_compare_root.diff.json'
$reportOutputRoot = Join-Path $resolvedOutputRoot 'report'
$artifactReportOutputRoot = Join-Path $resolvedOutputRoot 'artifact-report'
$inspectJsonPath = Join-Path $resolvedOutputRoot 'resource_contract_compare_root.inspect.json'
$rootSummaryInspectJsonPath = Join-Path $resolvedOutputRoot 'resource_contract_compare_root.summary.inspect.json'
$whyInspectJsonPath = Join-Path $resolvedOutputRoot 'resource_contract_compare_root.why.inspect.json'
$summaryPath = Join-Path $resolvedOutputRoot 'resource_contract_compare_root_smoke.summary.json'

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

$existingContracts = @(
    @($rightCase.declared_contracts) |
        Where-Object { $null -ne $_ -and [string]$_.contract -ne $AddedContract }
)
$existingContracts += [pscustomobject][ordered]@{
    contract = $AddedContract
    requires = @($AddedRequiredFact)
}
Set-ObjectProperty -Object $rightCase -Name 'declared_contracts' -Value @($existingContracts)
$rightIndex | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $rightIndexPath -Encoding utf8

Write-Host "[SYNTH] case=$ChangedCase add contract=$AddedContract requires=$AddedRequiredFact"

$diffData = Invoke-CommandJson -OutputPath $diffJsonPath -Command {
    & $diffScript -LeftBundleRoot $leftBundleRoot -RightBundleRoot $rightBundleRoot -IncludeUnchanged -AsJson
}
Assert-Condition ([string]$diffData.schema -eq 'materialized_graph.bundle_diff/v1') 'unexpected diff schema'
Assert-Condition ([int]$diffData.case_count -ge 2) 'compare root smoke expects at least two diff cases'

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
Assert-Condition ([string]$changedCaseDiff.status -eq 'unchanged') 'resource compare root changed case must stay metadata-only unchanged'
Assert-Condition ((@($changedCaseDiff.metadata_changes) | Where-Object { [string]$_ -like 'declared_contracts:*' }).Count -gt 0) 'resource compare root changed case must preserve declared_contracts metadata change'
Assert-Condition ([string]$unchangedCaseDiff.status -eq 'unchanged') 'resource compare root unchanged case must stay unchanged'

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

$artifactReports = @(Get-ChildItem -LiteralPath $artifactReportOutputRoot -Filter '*.artifact_report.json' -File)
Assert-Condition (@($artifactReports).Count -ge 2) 'resource compare root smoke expects at least two artifact reports'

$inspectResult = Invoke-CommandJson -OutputPath $inspectJsonPath -Command {
    & $inspectScript -ArtifactRoot $artifactReportOutputRoot -ResourceSummary -AsJson
}
Assert-Condition ([string]$inspectResult.query.kind -eq 'resource_summary') 'inspect resource summary query kind mismatch'
Assert-Condition ([string]$inspectResult.query.scope -eq 'artifact_root') 'inspect resource summary scope mismatch'
Assert-Condition ($null -ne $inspectResult.query.comparison) 'artifact_root resource summary must expose comparison payload'
Assert-Condition ($null -ne $inspectResult.query.comparison.resource_contract) 'artifact_root resource summary missing comparison.resource_contract'

$resourceComparison = $inspectResult.query.comparison.resource_contract
Assert-Condition ([int]$resourceComparison.changed_case_count -eq 1) 'resource compare root changed_case_count must be 1'
Assert-Condition ((@($resourceComparison.changed_cases) -contains $ChangedCase)) 'resource compare root changed_cases missing target case'
Assert-Condition ((@($resourceComparison.unchanged_cases) -contains $ExpectedUnchangedCase)) 'resource compare root unchanged_cases missing baseline peer case'
Assert-Condition ((@($resourceComparison.summary_change_matrix | Where-Object { [string]$_.change -eq 'violated_count:0->1' }).Count -eq 1)) 'resource compare root summary_change_matrix missing violated_count change'

$contractChangeEntry = @(
    @($resourceComparison.contract_change_matrix) |
        Where-Object { [string]$_.contract -eq $AddedContract } |
        Select-Object -First 1
) | Select-Object -First 1
Assert-Condition ($null -ne $contractChangeEntry) "resource compare root missing contract change entry: $AddedContract"
Assert-Condition ((@($contractChangeEntry.change_kinds) -contains 'added')) 'resource compare root contract entry must include added kind'
$contractCase = @(
    @($contractChangeEntry.cases) |
        Where-Object { [string]$_.case -eq $ChangedCase } |
        Select-Object -First 1
) | Select-Object -First 1
Assert-Condition ($null -ne $contractCase) "resource compare root contract entry missing target case: $ChangedCase"
Assert-Condition ([string]$contractCase.right_state -eq 'violated') 'resource compare root contract right_state must become violated'

$rootSummaryInspectResult = Invoke-CommandJson -OutputPath $rootSummaryInspectJsonPath -Command {
    & $inspectScript -ArtifactRoot $artifactReportOutputRoot -AsJson
}
Assert-Condition ([int]$rootSummaryInspectResult.comparison.compared_case_count -eq 2) 'artifact_root summary compared_case_count must be 2'
Assert-Condition ([int]$rootSummaryInspectResult.comparison.resource_changed_case_count -eq 1) 'artifact_root summary resource_changed_case_count must be 1'
Assert-Condition ((@($rootSummaryInspectResult.comparison.resource_changed_cases) -contains $ChangedCase)) 'artifact_root summary missing resource changed case'
Assert-Condition ($null -ne $rootSummaryInspectResult.comparison.capability_summary) 'artifact_root summary must expose capability summary'
Assert-Condition ([int]$rootSummaryInspectResult.comparison.capability_summary.resource_compare_capability_count -eq 1) 'artifact_root summary resource compare capability count must be 1'
Assert-Condition ((@($rootSummaryInspectResult.comparison.capability_summary.resource_compare_capabilities) -contains $AddedRequiredFact)) 'artifact_root summary capability summary missing required fact'

$whyInspectResult = Invoke-CommandJson -OutputPath $whyInspectJsonPath -Command {
    & $inspectScript -ArtifactRoot $artifactReportOutputRoot -WhyCapability $AddedRequiredFact -AsJson
}
Assert-Condition ([string]$whyInspectResult.query.kind -eq 'why_capability') 'artifact_root why query kind mismatch'
Assert-Condition ([string]$whyInspectResult.query.scope -eq 'artifact_root') 'artifact_root why query scope mismatch'
Assert-Condition ([string]$whyInspectResult.query.result.capability -eq $AddedRequiredFact) 'artifact_root why result capability mismatch'
Assert-Condition ([int]$whyInspectResult.query.result.resource_compare_case_count -eq 1) 'artifact_root why resource compare case count must be 1'
Assert-Condition ((@($whyInspectResult.query.result.resource_compare_cases) -contains $ChangedCase)) 'artifact_root why missing changed case'
Assert-Condition ((@($whyInspectResult.query.result.resource_contracts) -contains $AddedContract)) 'artifact_root why missing changed contract'

$summary = [ordered]@{
    left_bundle_root = $leftBundleRoot
    right_bundle_root = $rightBundleRoot
    changed_case = $ChangedCase
    unchanged_case = $ExpectedUnchangedCase
    added_contract = $AddedContract
    added_required_fact = $AddedRequiredFact
    diff_json = $diffJsonPath
    artifact_report_root = $artifactReportOutputRoot
    inspect_json = $inspectJsonPath
    root_summary_inspect_json = $rootSummaryInspectJsonPath
    why_inspect_json = $whyInspectJsonPath
    assertions = [ordered]@{
        artifact_root_compare_present = $true
        changed_case_detected = $true
        unchanged_case_detected = $true
        contract_change_matrix_detected = $true
        root_summary_compare_detected = $true
        root_summary_capability_compare_detected = $true
        root_why_compare_detected = $true
    }
}
$summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryPath -Encoding utf8

Write-Host "[OK] resource contract compare root smoke passed changed_case=$ChangedCase contract=$AddedContract"
Write-Host "[DIFF]    $diffJsonPath"
Write-Host "[REPORT]  $artifactReportOutputRoot"
Write-Host "[INSPECT] $inspectJsonPath"
Write-Host "[SUMMARY] $summaryPath"
