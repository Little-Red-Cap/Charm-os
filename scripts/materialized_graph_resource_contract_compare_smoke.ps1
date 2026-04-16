param(
    [string]$BundleRoot = 'out/materialized-graph-ci/bundle-current',
    [string]$Case = 'bringup-minimal-observe-demo',
    [string]$AddedContract = 'needs_heap',
    [string]$AddedRequiredFact = 'system.heap',
    [string]$OutputRoot = 'out/materialized-graph-resource-contract-compare-smoke',
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

$repoRoot = Split-Path -Parent $PSScriptRoot
$resolvedBundleRoot = Resolve-FullPath $BundleRoot
$resolvedOutputRoot = Resolve-FullPath $OutputRoot
$rightBundleRoot = Join-Path $resolvedOutputRoot 'right-bundle'
$diffJsonPath = Join-Path $resolvedOutputRoot 'resource_contract_compare.diff.json'
$reportOutputRoot = Join-Path $resolvedOutputRoot 'report'
$artifactReportOutputRoot = Join-Path $resolvedOutputRoot 'artifact-report'
$inspectJsonPath = Join-Path $resolvedOutputRoot 'resource_contract_compare.inspect.json'
$summaryInspectJsonPath = Join-Path $resolvedOutputRoot 'resource_contract_compare.summary.inspect.json'
$summaryPath = Join-Path $resolvedOutputRoot 'resource_contract_compare_smoke.summary.json'

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

Copy-Item -LiteralPath $resolvedBundleRoot -Destination $rightBundleRoot -Recurse -Force

$rightIndexPath = Join-Path $rightBundleRoot 'index.json'
$rightIndex = Get-Content -LiteralPath $rightIndexPath -Raw -Encoding utf8 | ConvertFrom-Json
$rightCase = Get-CaseEntry -Cases @($rightIndex.cases) -CaseName $Case
Assert-Condition ($null -ne $rightCase) "case not found in synthetic bundle: $Case"

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

Write-Host "[SYNTH] case=$Case add contract=$AddedContract requires=$AddedRequiredFact"

$diffData = Invoke-CommandJson -OutputPath $diffJsonPath -Command {
    & $diffScript -LeftBundleRoot $resolvedBundleRoot -RightBundleRoot $rightBundleRoot -Case $Case -IncludeUnchanged -AsJson
}
Assert-Condition ([string]$diffData.schema -eq 'materialized_graph.bundle_diff/v1') 'unexpected diff schema'
Assert-Condition ([int]$diffData.case_count -eq 1) 'compare smoke expects exactly one diff case'

$caseDiff = @($diffData.cases)[0]
Assert-Condition ([string]$caseDiff.name -eq $Case) 'unexpected diff case name'
Assert-Condition ([string]$caseDiff.status -eq 'unchanged') 'resource contract compare smoke expects metadata-only diff to stay unchanged'
Assert-Condition ((@($caseDiff.metadata_changes) | Where-Object { [string]$_ -like 'declared_contracts:*' }).Count -gt 0) 'metadata_changes must mention declared_contracts'

& $reportScript -LeftBundleRoot $resolvedBundleRoot -RightBundleRoot $rightBundleRoot -Case $Case -IncludeUnchanged -Format markdown -OutputDir $reportOutputRoot
if ($LASTEXITCODE -ne 0) {
    throw 'report generation failed'
}

$reportManifestPath = Join-Path $reportOutputRoot 'materialized_graph_bundle_diff_report.manifest.json'
Assert-Condition (Test-Path $reportManifestPath) "report manifest not found: $reportManifestPath"

& $artifactReportScript `
    -BundleRoot $rightBundleRoot `
    -Case $Case `
    -OutputRoot $artifactReportOutputRoot `
    -Mode compare `
    -DiffJson $diffJsonPath `
    -ReportManifest $reportManifestPath
if ($LASTEXITCODE -ne 0) {
    throw 'artifact report compare export failed'
}

$artifactReportPath = Join-Path $artifactReportOutputRoot ($Case + '.artifact_report.json')
Assert-Condition (Test-Path $artifactReportPath) "artifact report not found: $artifactReportPath"
$artifactReport = Get-Content -LiteralPath $artifactReportPath -Raw -Encoding utf8 | ConvertFrom-Json

Assert-Condition ([string]$artifactReport.mode -eq 'compare') 'artifact report mode must be compare'
Assert-Condition ($null -ne $artifactReport.comparison) 'artifact report comparison is missing'
Assert-Condition ([string]$artifactReport.comparison.status -eq 'unchanged') 'artifact report comparison.status must preserve metadata-only unchanged'
Assert-Condition ((@($artifactReport.comparison.metadata_changes) | Where-Object { [string]$_ -like 'declared_contracts:*' }).Count -gt 0) 'artifact report comparison must preserve declared_contracts metadata change'
Assert-Condition ($null -ne $artifactReport.comparison.resource_contract) 'artifact report comparison.resource_contract is missing'
Assert-Condition ([bool]$artifactReport.comparison.resource_contract.changed) 'resource contract comparison must be marked changed'

$resourceContractComparison = $artifactReport.comparison.resource_contract
$expectedViolationText = "$AddedContract missing [$AddedRequiredFact] requires [$AddedRequiredFact]"

Assert-Condition ([int]$resourceContractComparison.left.declared_contracts -eq 1) 'left declared_contracts must stay at 1 for baseline fixture'
Assert-Condition ([int]$resourceContractComparison.right.declared_contracts -eq 2) 'right declared_contracts must become 2 after synthetic mutation'
Assert-Condition ([int]$resourceContractComparison.left.violated_count -eq 0) 'baseline violated_count must stay 0'
Assert-Condition ([int]$resourceContractComparison.right.violated_count -eq 1) 'candidate violated_count must become 1'
Assert-Condition ((@($resourceContractComparison.summary_changes) -contains 'declared_contracts:1->2')) 'resource contract summary_changes missing declared_contracts change'
Assert-Condition ((@($resourceContractComparison.summary_changes) -contains 'violated_count:0->1')) 'resource contract summary_changes missing violated_count change'
Assert-Condition ((@($resourceContractComparison.hotspot_changes.added) -contains $expectedViolationText)) 'resource contract hotspot_changes.added missing expected violation text'

$contractChange = @(
    @($resourceContractComparison.contract_changes) |
        Where-Object { [string]$_.contract -eq $AddedContract } |
        Select-Object -First 1
) | Select-Object -First 1
Assert-Condition ($null -ne $contractChange) "resource contract comparison missing contract change: $AddedContract"
Assert-Condition ([string]$contractChange.change_kind -eq 'added') 'resource contract compare must mark synthetic contract as added'
Assert-Condition ([string]$contractChange.left_state -eq 'absent') 'left_state must be absent for synthetic added contract'
Assert-Condition ([string]$contractChange.right_state -eq 'violated') 'right_state must be violated for synthetic added contract'
Assert-Condition ((@($contractChange.right_requires) -contains $AddedRequiredFact)) 'right_requires missing synthetic required fact'
Assert-Condition ([string]$contractChange.right_status_text -eq $expectedViolationText) 'right_status_text mismatch for synthetic violated contract'

$inspectResult = Invoke-CommandJson -OutputPath $inspectJsonPath -Command {
    & $inspectScript -ArtifactRoot $artifactReportOutputRoot -Case $Case -ResourceSummary -AsJson
}
Assert-Condition ([string]$inspectResult.query.kind -eq 'resource_summary') 'inspect resource summary query kind mismatch'
Assert-Condition ([string]$inspectResult.query.scope -eq 'report') 'inspect resource summary scope mismatch'
Assert-Condition ($null -ne $inspectResult.query.comparison) 'inspect resource summary must expose comparison payload'
Assert-Condition ($null -ne $inspectResult.query.comparison.resource_contract) 'inspect resource summary missing comparison.resource_contract'
Assert-Condition ([bool]$inspectResult.query.comparison.resource_contract.changed) 'inspect resource summary comparison.resource_contract must be changed'

$summaryInspectResult = Invoke-CommandJson -OutputPath $summaryInspectJsonPath -Command {
    & $inspectScript -ArtifactRoot $artifactReportOutputRoot -Case $Case -AsJson
}
Assert-Condition ($null -ne $summaryInspectResult.comparison) 'inspect default summary must expose comparison payload'
Assert-Condition ($null -ne $summaryInspectResult.comparison.capability_summary) 'inspect default summary must expose capability summary'
Assert-Condition ([int]$summaryInspectResult.comparison.capability_summary.resource_compare_capability_count -eq 1) 'inspect default summary resource compare capability count must be 1'
Assert-Condition ((@($summaryInspectResult.comparison.capability_summary.resource_compare_capabilities) -contains $AddedRequiredFact)) 'inspect default summary capability summary missing required fact'

$summary = [ordered]@{
    left_bundle_root = $resolvedBundleRoot
    right_bundle_root = $rightBundleRoot
    case = $Case
    added_contract = $AddedContract
    added_required_fact = $AddedRequiredFact
    diff_json = $diffJsonPath
    artifact_report = $artifactReportPath
    inspect_json = $inspectJsonPath
    summary_inspect_json = $summaryInspectJsonPath
    assertions = [ordered]@{
        metadata_only_diff_preserved = $true
        comparison_resource_contract_present = $true
        violated_contract_change_detected = $true
        inspect_resource_summary_exposes_compare = $true
        inspect_default_summary_exposes_capability_compare = $true
    }
}
$summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryPath -Encoding utf8

Write-Host "[OK] resource contract compare smoke passed case=$Case contract=$AddedContract"
Write-Host "[DIFF]    $diffJsonPath"
Write-Host "[REPORT]  $artifactReportPath"
Write-Host "[INSPECT] $inspectJsonPath"
Write-Host "[SUMMARY] $summaryPath"
