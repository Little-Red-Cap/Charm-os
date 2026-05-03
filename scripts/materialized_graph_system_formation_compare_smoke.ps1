param(
    [string]$BundleRoot = 'out/materialized-graph-ci/bundle-current',
    [string]$ChangedCase = 'bringup-minimal-observe-demo',
    [string]$ExpectedUnchangedCase = 'bringup-block-observe-demo',
    [string]$RemovedCapability = 'platform.irq',
    [string]$BlockedNode = 'hal.uart1',
    [string]$OutputRoot = 'out/materialized-graph-system-formation-compare-smoke',
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

. (Join-Path $PSScriptRoot 'system_compiler_result_map_contract.ps1')

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

function Get-CapabilityName {
    param(
        $Capability
    )

    if ($null -eq $Capability) {
        return ''
    }

    $name = [string]$Capability.name
    if ([string]::IsNullOrWhiteSpace($name)) {
        $name = [string]$Capability.id
    }

    return $name
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
$diffJsonPath = Join-Path $resolvedOutputRoot 'system_formation_compare.diff.json'
$reportOutputRoot = Join-Path $resolvedOutputRoot 'report'
$artifactReportOutputRoot = Join-Path $resolvedOutputRoot 'artifact-report'
$reportInspectJsonPath = Join-Path $resolvedOutputRoot 'system_formation_compare.report.inspect.json'
$rootSummaryInspectJsonPath = Join-Path $resolvedOutputRoot 'system_formation_compare.summary.inspect.json'
$summaryPath = Join-Path $resolvedOutputRoot 'system_formation_compare_smoke.summary.json'

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
Assert-Condition (-not [string]::IsNullOrWhiteSpace([string]$rightCase.json)) "case json missing in synthetic bundle: $ChangedCase"

$graphPath = Join-Path $rightBundleRoot ([string]$rightCase.json)
Assert-Condition (Test-Path $graphPath) "case graph not found: $graphPath"
$graph = Get-Content -LiteralPath $graphPath -Raw -Encoding utf8 | ConvertFrom-Json

$removedProviderCount = 0
foreach ($node in @($graph.nodes)) {
    if ($null -eq $node) {
        continue
    }

    $existingProvides = @($node.provides)
    $filteredProvides = @(
        @($existingProvides) |
            Where-Object { (Get-CapabilityName -Capability $_) -ne $RemovedCapability }
    )
    if (@($filteredProvides).Count -ne @($existingProvides).Count) {
        $removedProviderCount += 1
    }
    Set-ObjectProperty -Object $node -Name 'provides' -Value @($filteredProvides)
}
Assert-Condition ($removedProviderCount -gt 0) "target capability provider not found in graph: $RemovedCapability"

$originalEdgeCount = [int]@($graph.edges).Count
$filteredEdges = @(
    @($graph.edges) |
        Where-Object { (Get-CapabilityName -Capability $_.capability) -ne $RemovedCapability }
)
Assert-Condition (@($filteredEdges).Count -lt $originalEdgeCount) "target capability edge not found in graph: $RemovedCapability"
Set-ObjectProperty -Object $graph -Name 'edges' -Value @($filteredEdges)
Set-ObjectProperty -Object $graph -Name 'edge_count' -Value ([int]@($filteredEdges).Count)

if ($null -ne $rightCase.PSObject.Properties['graph'] -and $null -ne $rightCase.graph) {
    Set-ObjectProperty -Object $rightCase.graph -Name 'edge_count' -Value ([int]@($filteredEdges).Count)
}
$graph | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $graphPath -Encoding utf8
$rightIndex | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $rightIndexPath -Encoding utf8

Write-Host "[SYNTH] case=$ChangedCase remove capability=$RemovedCapability edge_count=$originalEdgeCount->$([int]@($filteredEdges).Count)"

$diffData = Invoke-CommandJson -OutputPath $diffJsonPath -Command {
    & $diffScript -LeftBundleRoot $leftBundleRoot -RightBundleRoot $rightBundleRoot -IncludeUnchanged -AsJson
}
Assert-Condition ([string]$diffData.schema -eq 'materialized_graph.bundle_diff/v1') 'unexpected diff schema'
Assert-Condition ([int]$diffData.case_count -ge 2) 'system formation compare smoke expects at least two diff cases'

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
Assert-Condition ([string]$changedCaseDiff.status -eq 'changed') 'system formation compare changed case must be marked changed'
Assert-Condition ([string]$unchangedCaseDiff.status -eq 'unchanged') 'system formation compare unchanged case must stay unchanged'
Assert-Condition ((@($changedCaseDiff.summary_changes) | Where-Object { [string]$_ -like 'edge_count:*' }).Count -gt 0) 'system formation compare changed case must report edge_count drift'

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
Assert-Condition ([string]$artifactReport.comparison.status -eq 'changed') 'artifact report comparison.status must be changed for system formation drift'
Assert-Condition ($null -ne $artifactReport.system_formation) 'artifact report system_formation is missing'
Assert-Condition ([string]$artifactReport.system_formation.status -eq 'blocked') 'artifact report system_formation.status must become blocked after capability removal'
Assert-Condition ((@($artifactReport.system_formation.binding_summary.unresolved_capabilities) -contains $RemovedCapability)) 'artifact report system_formation.binding_summary must expose removed capability as unresolved'
Assert-Condition ((@($artifactReport.system_formation.bringup_summary.blocked_nodes) -contains $BlockedNode)) 'artifact report system_formation.bringup_summary must expose blocked node'
$bindingBlocker = @(
    @($artifactReport.system_formation.blockers) |
        Where-Object { [string]$_.kind -eq 'binding' -and [string]$_.name -eq $RemovedCapability } |
        Select-Object -First 1
) | Select-Object -First 1
$nodeBlocker = @(
    @($artifactReport.system_formation.blockers) |
        Where-Object { [string]$_.kind -eq 'node' -and [string]$_.name -eq $BlockedNode } |
        Select-Object -First 1
) | Select-Object -First 1
Assert-Condition ($null -ne $bindingBlocker) "artifact report system_formation missing binding blocker: $RemovedCapability"
Assert-Condition ($null -ne $nodeBlocker) "artifact report system_formation missing node blocker: $BlockedNode"
Assert-Condition ($null -ne $artifactReport.comparison.system_formation) 'artifact report comparison.system_formation is missing'
Assert-Condition ([bool]$artifactReport.comparison.system_formation.changed) 'artifact report comparison.system_formation must be marked changed'
Assert-Condition ((@($artifactReport.comparison.system_formation.unresolved_capability_changes.added) -contains $RemovedCapability)) 'comparison.system_formation unresolved capability changes must include removed capability'
Assert-Condition ((@($artifactReport.comparison.system_formation.blocked_node_changes.added) -contains $BlockedNode)) 'comparison.system_formation blocked node changes must include blocked node'
Assert-Condition ($null -ne $artifactReport.comparison.binding_result) 'artifact report comparison.binding_result is missing'
Assert-Condition ($null -ne $artifactReport.comparison.bringup_order) 'artifact report comparison.bringup_order is missing'

$bindingResultComparison = $artifactReport.comparison.binding_result
Assert-Condition ([bool]$bindingResultComparison.changed) 'binding_result comparison must be marked changed'
Assert-Condition ((@($bindingResultComparison.unresolved_capability_changes.added) -contains $RemovedCapability)) 'binding_result unresolved_capability_changes.added missing removed capability'
Assert-Condition ((@($bindingResultComparison.resolved_capability_changes.removed) -contains $RemovedCapability)) 'binding_result resolved_capability_changes.removed missing removed capability'
$bindingChange = @(
    @($bindingResultComparison.binding_changes) |
        Where-Object { [string]$_.capability -eq $RemovedCapability } |
        Select-Object -First 1
) | Select-Object -First 1
Assert-Condition ($null -ne $bindingChange) "binding_result comparison missing capability change: $RemovedCapability"
Assert-Condition ([string]$bindingChange.change_kind -eq 'changed') 'binding_result compare must mark target capability as changed'
Assert-Condition ([string]$bindingChange.left_state -eq 'resolved') 'binding_result left_state must stay resolved for baseline capability'
Assert-Condition ([string]$bindingChange.right_state -eq 'unresolved') 'binding_result right_state must become unresolved after provider removal'

$bringupOrderComparison = $artifactReport.comparison.bringup_order
Assert-Condition ([bool]$bringupOrderComparison.changed) 'bringup_order comparison must be marked changed'
Assert-Condition ((@($bringupOrderComparison.blocked_node_changes.added) -contains $BlockedNode)) 'bringup_order blocked_node_changes.added missing blocked node'
$blockedNodeChange = @(
    @($bringupOrderComparison.entry_changes) |
        Where-Object { [string]$_.node -eq $BlockedNode } |
        Select-Object -First 1
) | Select-Object -First 1
Assert-Condition ($null -ne $blockedNodeChange) "bringup_order comparison missing node change: $BlockedNode"
Assert-Condition ([string]$blockedNodeChange.left_state -eq 'ready') 'bringup_order left_state must stay ready for baseline blocked node'
Assert-Condition ([string]$blockedNodeChange.right_state -eq 'blocked') 'bringup_order right_state must become blocked after capability removal'
Assert-Condition ((@($blockedNodeChange.right_missing_requires) -contains $RemovedCapability)) 'bringup_order right_missing_requires missing removed capability'

$reportInspectResult = Invoke-CommandJson -OutputPath $reportInspectJsonPath -Command {
    & $inspectScript -ArtifactRoot $artifactReportOutputRoot -Case $ChangedCase -AsJson
}
Assert-Condition ([string]$reportInspectResult.summary.Case -eq $ChangedCase) 'report inspect summary case mismatch'
Assert-Condition ($null -ne $reportInspectResult.compiler_headline) 'default report summary must expose compiler_headline'
Assert-Condition ([string]$reportInspectResult.compiler_headline.status -eq 'blocked') 'default report compiler_headline status must be blocked'
Assert-Condition ([bool]$reportInspectResult.compiler_headline.has_comparison) 'default report compiler_headline must mark comparison as present'
Assert-Condition ([bool]$reportInspectResult.compiler_headline.has_drift) 'default report compiler_headline must mark drift as present'
Assert-Condition ([int]$reportInspectResult.compiler_headline.case_count -eq 1) 'default report compiler_headline case_count must be 1'
Assert-Condition ([int]$reportInspectResult.compiler_headline.blocked_case_count -eq 1) 'default report compiler_headline blocked_case_count must be 1'
Assert-Condition ((@($reportInspectResult.compiler_headline.drift_dimensions) -contains 'formation')) 'default report compiler_headline missing formation drift dimension'
Assert-Condition ((@($reportInspectResult.compiler_headline.drift_dimensions) -contains 'binding')) 'default report compiler_headline missing binding drift dimension'
Assert-Condition ((@($reportInspectResult.compiler_headline.drift_dimensions) -contains 'bringup_order')) 'default report compiler_headline missing bringup_order drift dimension'
Assert-Condition ((@($reportInspectResult.compiler_headline.blocked_cases) -contains $ChangedCase)) 'default report compiler_headline missing blocked case'
Assert-Condition ((@($reportInspectResult.compiler_headline.unresolved_capabilities) -contains $RemovedCapability)) 'default report compiler_headline missing removed capability'
Assert-Condition ((@($reportInspectResult.compiler_headline.blocked_nodes) -contains $BlockedNode)) 'default report compiler_headline missing blocked node'
Assert-Condition ([string]$reportInspectResult.compiler_headline.text -like '*status:blocked*') 'default report compiler_headline text missing blocked status'
Assert-Condition ([string]$reportInspectResult.compiler_headline.text -like '*drift:*') 'default report compiler_headline text missing drift token'
Assert-Condition ($null -ne $reportInspectResult.formation_headline) 'default report summary must expose formation_headline'
Assert-Condition ([string]$reportInspectResult.formation_headline.status -eq 'blocked') 'default report formation_headline status must be blocked'
Assert-Condition ([int]$reportInspectResult.formation_headline.case_count -eq 1) 'default report formation_headline case_count must be 1'
Assert-Condition ([int]$reportInspectResult.formation_headline.status_counts.blocked -eq 1) 'default report formation_headline blocked status count must be 1'
Assert-Condition ((@($reportInspectResult.formation_headline.blocked_cases) -contains $ChangedCase)) 'default report formation_headline missing blocked case'
Assert-Condition ((@($reportInspectResult.formation_headline.unresolved_capabilities) -contains $RemovedCapability)) 'default report formation_headline missing removed capability'
Assert-Condition ((@($reportInspectResult.formation_headline.blocked_nodes) -contains $BlockedNode)) 'default report formation_headline missing blocked node'
Assert-Condition ([string]$reportInspectResult.formation_headline.text -like '*status:blocked*') 'default report formation_headline text missing blocked status'
Assert-Condition ($null -ne $reportInspectResult.comparison) 'default report summary must expose comparison payload'
Assert-Condition ($null -ne $reportInspectResult.system_formation) 'default report summary must expose system_formation payload'
Assert-Condition ([string]$reportInspectResult.summary.Formation -eq 'blocked') 'default report summary Formation must become blocked'
Assert-Condition ([bool]$reportInspectResult.comparison.system_formation.changed) 'default report summary comparison.system_formation must be changed'
Assert-Condition ([int]$reportInspectResult.summary.FormCmp -gt 0) 'default report summary FormCmp must be nonzero for system formation drift'
Assert-Condition ([bool]$reportInspectResult.comparison.binding_result.changed) 'default report summary comparison.binding_result must be changed'
Assert-Condition ([bool]$reportInspectResult.comparison.bringup_order.changed) 'default report summary comparison.bringup_order must be changed'
Assert-Condition ([int]$reportInspectResult.summary.BindCmp -gt 0) 'default report summary BindCmp must be nonzero for system formation drift'
Assert-Condition ([int]$reportInspectResult.summary.OrdCmp -gt 0) 'default report summary OrdCmp must be nonzero for system formation drift'

$rootSummaryInspectResult = Invoke-CommandJson -OutputPath $rootSummaryInspectJsonPath -Command {
    & $inspectScript -ArtifactRoot $artifactReportOutputRoot -AsJson
}
$artifactReportIndexPath = Join-Path $artifactReportOutputRoot 'index.json'
Assert-Condition (Test-Path $artifactReportIndexPath) 'artifact report root must expose index.json'
$artifactReportIndex = Get-Content -LiteralPath $artifactReportIndexPath -Raw -Encoding utf8 | ConvertFrom-Json
Assert-Condition ([string]$artifactReportIndex.schema -eq 'system_compiler.artifact_report_index/v0') 'artifact report index schema mismatch'
Assert-Condition ($null -ne $artifactReportIndex.compiler_headline) 'artifact report index must expose compiler_headline'
Assert-Condition ([string]$artifactReportIndex.compiler_headline.status -eq 'blocked') 'artifact report index compiler_headline status must be blocked'
Assert-Condition ([bool]$artifactReportIndex.compiler_headline.has_comparison) 'artifact report index compiler_headline must mark comparison as present'
Assert-Condition ([bool]$artifactReportIndex.compiler_headline.has_drift) 'artifact report index compiler_headline must mark drift as present'
Assert-Condition ([int]$artifactReportIndex.compiler_headline.case_count -ge 2) 'artifact report index compiler_headline case_count must be at least 2'
Assert-Condition ([int]$artifactReportIndex.compiler_headline.blocked_case_count -eq 1) 'artifact report index compiler_headline blocked_case_count must be 1'
Assert-Condition ((@($artifactReportIndex.compiler_headline.drift_dimensions) -contains 'formation')) 'artifact report index compiler_headline missing formation drift dimension'
Assert-Condition ((@($artifactReportIndex.compiler_headline.drift_dimensions) -contains 'binding')) 'artifact report index compiler_headline missing binding drift dimension'
Assert-Condition ((@($artifactReportIndex.compiler_headline.drift_dimensions) -contains 'bringup_order')) 'artifact report index compiler_headline missing bringup_order drift dimension'
Assert-Condition ((@($artifactReportIndex.compiler_headline.blocked_cases) -contains $ChangedCase)) 'artifact report index compiler_headline missing blocked case'
Assert-Condition ((@($artifactReportIndex.compiler_headline.unresolved_capabilities) -contains $RemovedCapability)) 'artifact report index compiler_headline missing removed capability'
Assert-Condition ((@($artifactReportIndex.compiler_headline.blocked_nodes) -contains $BlockedNode)) 'artifact report index compiler_headline missing blocked node'
Assert-Condition ([string]$artifactReportIndex.compiler_headline.text -like '*status:blocked*') 'artifact report index compiler_headline text missing blocked status'
Assert-Condition ([string]$artifactReportIndex.compiler_headline.text -like '*drift:*') 'artifact report index compiler_headline text missing drift token'
Assert-Condition ([string]$artifactReportIndex.compiler_headline.text -eq [string]$rootSummaryInspectResult.compiler_headline.text) 'artifact report index compiler_headline text must match root summary compiler_headline text'
Assert-Condition (@($artifactReportIndex.cases).Count -ge 2) 'artifact report index must expose case entries'
Assert-Condition ((@($artifactReportIndex.cases | ForEach-Object { [string]$_.name }) -contains $ChangedCase)) 'artifact report index missing changed case entry'
Assert-Condition ($null -ne $rootSummaryInspectResult.system_compiler_summary) 'artifact_root summary must expose system_compiler_summary'
Assert-Condition ($null -ne $rootSummaryInspectResult.compiler_headline) 'artifact_root summary must expose compiler_headline'
Assert-Condition ([string]$rootSummaryInspectResult.compiler_headline.status -eq 'blocked') 'artifact_root compiler_headline status must be blocked'
Assert-Condition ([bool]$rootSummaryInspectResult.compiler_headline.has_comparison) 'artifact_root compiler_headline must mark comparison as present'
Assert-Condition ([bool]$rootSummaryInspectResult.compiler_headline.has_drift) 'artifact_root compiler_headline must mark drift as present'
Assert-Condition ([int]$rootSummaryInspectResult.compiler_headline.case_count -ge 2) 'artifact_root compiler_headline case_count must be at least 2'
Assert-Condition ([int]$rootSummaryInspectResult.compiler_headline.formed_case_count -ge 1) 'artifact_root compiler_headline must retain formed status count'
Assert-Condition ([int]$rootSummaryInspectResult.compiler_headline.blocked_case_count -eq 1) 'artifact_root compiler_headline blocked_case_count must be 1'
Assert-Condition ((@($rootSummaryInspectResult.compiler_headline.drift_dimensions) -contains 'formation')) 'artifact_root compiler_headline missing formation drift dimension'
Assert-Condition ((@($rootSummaryInspectResult.compiler_headline.drift_dimensions) -contains 'binding')) 'artifact_root compiler_headline missing binding drift dimension'
Assert-Condition ((@($rootSummaryInspectResult.compiler_headline.drift_dimensions) -contains 'bringup_order')) 'artifact_root compiler_headline missing bringup_order drift dimension'
Assert-Condition ((@($rootSummaryInspectResult.compiler_headline.blocked_cases) -contains $ChangedCase)) 'artifact_root compiler_headline missing blocked case'
Assert-Condition ((@($rootSummaryInspectResult.compiler_headline.unresolved_capabilities) -contains $RemovedCapability)) 'artifact_root compiler_headline missing removed capability'
Assert-Condition ((@($rootSummaryInspectResult.compiler_headline.blocked_nodes) -contains $BlockedNode)) 'artifact_root compiler_headline missing blocked node'
Assert-Condition ([string]$rootSummaryInspectResult.compiler_headline.text -like '*status:blocked*') 'artifact_root compiler_headline text missing blocked status'
Assert-Condition ([string]$rootSummaryInspectResult.compiler_headline.text -like '*drift:*') 'artifact_root compiler_headline text missing drift token'
Assert-Condition ($null -ne $rootSummaryInspectResult.formation_headline) 'artifact_root summary must expose formation_headline'
Assert-Condition ([string]$rootSummaryInspectResult.formation_headline.status -eq 'blocked') 'artifact_root formation_headline status must be blocked'
Assert-Condition ([int]$rootSummaryInspectResult.formation_headline.case_count -ge 2) 'artifact_root formation_headline case_count must be at least 2'
Assert-Condition ([int]$rootSummaryInspectResult.formation_headline.status_counts.formed -ge 1) 'artifact_root formation_headline must retain formed status count'
Assert-Condition ([int]$rootSummaryInspectResult.formation_headline.status_counts.blocked -eq 1) 'artifact_root formation_headline blocked status count must be 1'
Assert-Condition ((@($rootSummaryInspectResult.formation_headline.blocked_cases) -contains $ChangedCase)) 'artifact_root formation_headline missing blocked case'
Assert-Condition ((@($rootSummaryInspectResult.formation_headline.formed_cases) -contains $ExpectedUnchangedCase)) 'artifact_root formation_headline missing formed case'
Assert-Condition ((@($rootSummaryInspectResult.formation_headline.unresolved_capabilities) -contains $RemovedCapability)) 'artifact_root formation_headline missing removed capability'
Assert-Condition ((@($rootSummaryInspectResult.formation_headline.blocked_nodes) -contains $BlockedNode)) 'artifact_root formation_headline missing blocked node'
Assert-Condition ([string]$rootSummaryInspectResult.formation_headline.text -like '*status:blocked*') 'artifact_root formation_headline text missing blocked status'
Assert-Condition ($null -ne $rootSummaryInspectResult.binding_result_summary) 'artifact_root summary must expose binding_result_summary'
Assert-Condition ($null -ne $rootSummaryInspectResult.bringup_order_summary) 'artifact_root summary must expose bringup_order_summary'
Assert-Condition ($null -ne $rootSummaryInspectResult.system_formation_summary) 'artifact_root summary must expose system_formation_summary'
Assert-Condition ([int]$rootSummaryInspectResult.system_compiler_summary.case_count -ge 2) 'artifact_root system_compiler_summary.case_count must be at least 2'
Assert-Condition ($null -ne $rootSummaryInspectResult.system_compiler_summary.formation_basis) 'artifact_root system_compiler_summary must expose formation_basis'
Assert-Condition ($null -ne $rootSummaryInspectResult.system_compiler_summary.binding_basis) 'artifact_root system_compiler_summary must expose binding_basis'
Assert-Condition ($null -ne $rootSummaryInspectResult.system_compiler_summary.bringup_basis) 'artifact_root system_compiler_summary must expose bringup_basis'
Assert-Condition ($null -ne $rootSummaryInspectResult.system_compiler_summary.result_map) 'artifact_root system_compiler_summary must expose result_map'
Assert-Condition ([string]$rootSummaryInspectResult.system_compiler_summary.kind -eq 'system_compiler_summary/v0') 'artifact_root system_compiler_summary kind mismatch'
Assert-Condition ([string]$rootSummaryInspectResult.system_compiler_summary.mode -eq 'summary') 'artifact_root system_compiler_summary mode mismatch'
Assert-SystemCompilerResultMapContract -RootSummary $rootSummaryInspectResult -Context 'artifact_root system_compiler_summary'
Assert-Condition ([int]$rootSummaryInspectResult.binding_result_summary.case_count -ge 2) 'artifact_root binding_result_summary.case_count must be at least 2'
Assert-Condition ([int]$rootSummaryInspectResult.bringup_order_summary.case_count -ge 2) 'artifact_root bringup_order_summary.case_count must be at least 2'
Assert-Condition ([string]$rootSummaryInspectResult.bringup_order_summary.kind -eq 'bringup_order_summary/v0') 'artifact_root summary bringup_order_summary kind mismatch'
Assert-Condition ([string]$rootSummaryInspectResult.bringup_order_summary.mode -eq 'summary') 'artifact_root summary bringup_order_summary mode mismatch'
Assert-Condition ([int]$rootSummaryInspectResult.system_compiler_summary.formed_case_count -ge 1) 'artifact_root system_compiler_summary must retain at least one formed case'
Assert-Condition ([int]$rootSummaryInspectResult.system_compiler_summary.blocked_case_count -eq 1) 'artifact_root system_compiler_summary blocked_case_count must be 1'
Assert-Condition ((@($rootSummaryInspectResult.system_compiler_summary.blocked_cases) -contains $ChangedCase)) 'artifact_root system_compiler_summary missing blocked case'
Assert-Condition ((@($rootSummaryInspectResult.system_compiler_summary.unresolved_capability_matrix | ForEach-Object { [string]$_.capability }) -contains $RemovedCapability)) 'artifact_root system_compiler_summary unresolved_capability_matrix missing removed capability'
Assert-Condition ((@($rootSummaryInspectResult.system_compiler_summary.blocked_node_matrix | ForEach-Object { [string]$_.node }) -contains $BlockedNode)) 'artifact_root system_compiler_summary blocked_node_matrix missing blocked node'
Assert-Condition ((@($rootSummaryInspectResult.binding_result_summary.unresolved_capability_matrix | ForEach-Object { [string]$_.capability }) -contains $RemovedCapability)) 'artifact_root binding_result_summary unresolved_capability_matrix missing removed capability'
Assert-Condition ((@($rootSummaryInspectResult.bringup_order_summary.blocked_node_matrix | ForEach-Object { [string]$_.node }) -contains $BlockedNode)) 'artifact_root bringup_order_summary blocked_node_matrix missing blocked node'
Assert-Condition ([int]$rootSummaryInspectResult.system_formation_summary.case_count -ge 2) 'artifact_root system_formation_summary.case_count must be at least 2'
Assert-Condition ([string]$rootSummaryInspectResult.system_formation_summary.kind -eq 'system_formation_summary/v0') 'artifact_root summary system_formation_summary kind mismatch'
Assert-Condition ([string]$rootSummaryInspectResult.system_formation_summary.mode -eq 'summary') 'artifact_root summary system_formation_summary mode mismatch'
Assert-Condition ([int]$rootSummaryInspectResult.system_formation_summary.formed_case_count -ge 1) 'artifact_root system_formation_summary must retain at least one formed case'
Assert-Condition ([int]$rootSummaryInspectResult.system_formation_summary.blocked_case_count -eq 1) 'artifact_root system_formation_summary blocked_case_count must be 1'
Assert-Condition ((@($rootSummaryInspectResult.system_formation_summary.blocked_cases) -contains $ChangedCase)) 'artifact_root system_formation_summary missing blocked case'
Assert-Condition ((@($rootSummaryInspectResult.system_formation_summary.formed_cases) -contains $ExpectedUnchangedCase)) 'artifact_root system_formation_summary missing formed case'
Assert-Condition ((@($rootSummaryInspectResult.system_formation_summary.unresolved_capability_matrix | ForEach-Object { [string]$_.capability }) -contains $RemovedCapability)) 'artifact_root system_formation_summary unresolved_capability_matrix missing removed capability'
Assert-Condition ((@($rootSummaryInspectResult.system_formation_summary.blocked_node_matrix | ForEach-Object { [string]$_.node }) -contains $BlockedNode)) 'artifact_root system_formation_summary blocked_node_matrix missing blocked node'
Assert-Condition ([int]$rootSummaryInspectResult.comparison.compared_case_count -ge 2) 'artifact_root summary compared_case_count must be at least 2'
Assert-Condition ([int]$rootSummaryInspectResult.comparison.system_formation_changed_case_count -eq 1) 'artifact_root summary system_formation_changed_case_count must be 1'
Assert-Condition ([int]$rootSummaryInspectResult.comparison.binding_result_changed_case_count -eq 1) 'artifact_root summary binding_result_changed_case_count must be 1'
Assert-Condition ([int]$rootSummaryInspectResult.comparison.bringup_order_changed_case_count -eq 1) 'artifact_root summary bringup_order_changed_case_count must be 1'
Assert-Condition ((@($rootSummaryInspectResult.comparison.system_formation_changed_cases) -contains $ChangedCase)) 'artifact_root summary missing system_formation changed case'
Assert-Condition ((@($rootSummaryInspectResult.comparison.binding_result_changed_cases) -contains $ChangedCase)) 'artifact_root summary missing binding_result changed case'
Assert-Condition ((@($rootSummaryInspectResult.comparison.bringup_order_changed_cases) -contains $ChangedCase)) 'artifact_root summary missing bringup_order changed case'
Assert-Condition ((@($rootSummaryInspectResult.comparison.system_formation_changed_cases) -notcontains $ExpectedUnchangedCase)) 'artifact_root summary incorrectly marks unchanged case as system_formation changed'
Assert-Condition ((@($rootSummaryInspectResult.comparison.binding_result_changed_cases) -notcontains $ExpectedUnchangedCase)) 'artifact_root summary incorrectly marks unchanged case as binding_result changed'
Assert-Condition ((@($rootSummaryInspectResult.comparison.bringup_order_changed_cases) -notcontains $ExpectedUnchangedCase)) 'artifact_root summary incorrectly marks unchanged case as bringup_order changed'
Assert-Condition ($null -ne $rootSummaryInspectResult.comparison.system_compiler_summary) 'artifact_root summary comparison must expose system_compiler_summary'
Assert-Condition ($null -ne $rootSummaryInspectResult.comparison.binding_result_summary) 'artifact_root summary comparison must expose binding_result_summary'
Assert-Condition ($null -ne $rootSummaryInspectResult.comparison.bringup_order_summary) 'artifact_root summary comparison must expose bringup_order_summary'
Assert-Condition ($null -ne $rootSummaryInspectResult.comparison.system_formation_summary) 'artifact_root summary comparison must expose system_formation_summary'
Assert-Condition ([int]$rootSummaryInspectResult.comparison.system_compiler_summary.changed_case_count -eq 1) 'artifact_root comparison.system_compiler_summary changed_case_count must be 1'
Assert-Condition ([string]$rootSummaryInspectResult.binding_result_summary.kind -eq 'binding_result_summary/v0') 'artifact_root summary binding_result_summary kind mismatch'
Assert-Condition ([string]$rootSummaryInspectResult.binding_result_summary.mode -eq 'summary') 'artifact_root summary binding_result_summary mode mismatch'
Assert-Condition ($null -ne $rootSummaryInspectResult.comparison.system_compiler_summary.formation_drift) 'artifact_root comparison.system_compiler_summary must expose formation_drift'
Assert-Condition ($null -ne $rootSummaryInspectResult.comparison.system_compiler_summary.binding_drift) 'artifact_root comparison.system_compiler_summary must expose binding_drift'
Assert-Condition ($null -ne $rootSummaryInspectResult.comparison.system_compiler_summary.bringup_drift) 'artifact_root comparison.system_compiler_summary must expose bringup_drift'
Assert-Condition ($null -ne $rootSummaryInspectResult.comparison.system_compiler_summary.result_map) 'artifact_root comparison.system_compiler_summary must expose result_map'
Assert-Condition ([string]$rootSummaryInspectResult.comparison.system_compiler_summary.kind -eq 'system_compiler_summary/v0') 'artifact_root comparison.system_compiler_summary kind mismatch'
Assert-Condition ([string]$rootSummaryInspectResult.comparison.system_compiler_summary.mode -eq 'comparison') 'artifact_root comparison.system_compiler_summary mode mismatch'
Assert-SystemCompilerResultMapContract -RootSummary $rootSummaryInspectResult -Comparison -Context 'artifact_root comparison.system_compiler_summary'
Assert-Condition ((@($rootSummaryInspectResult.comparison.system_compiler_summary.changed_cases) -contains $ChangedCase)) 'artifact_root comparison.system_compiler_summary missing changed case'
Assert-Condition ([int]$rootSummaryInspectResult.comparison.system_compiler_summary.stage_changed_case_counts.system_input -eq 0) 'artifact_root comparison.system_compiler_summary system_input stage count must stay 0'
Assert-Condition ([int]$rootSummaryInspectResult.comparison.system_compiler_summary.stage_changed_case_counts.binding_result -eq 1) 'artifact_root comparison.system_compiler_summary binding_result stage count must be 1'
Assert-Condition ([int]$rootSummaryInspectResult.comparison.system_compiler_summary.stage_changed_case_counts.bringup_order -eq 1) 'artifact_root comparison.system_compiler_summary bringup_order stage count must be 1'
Assert-Condition ([int]$rootSummaryInspectResult.comparison.system_compiler_summary.stage_changed_case_counts.system_formation -eq 1) 'artifact_root comparison.system_compiler_summary system_formation stage count must be 1'
Assert-Condition ([int]$rootSummaryInspectResult.comparison.system_compiler_summary.formation_drift.changed_case_count -eq 1) 'artifact_root comparison.system_compiler_summary formation_drift.changed_case_count must be 1'
Assert-Condition ((@($rootSummaryInspectResult.comparison.system_compiler_summary.formation_drift.changed_cases) -contains $ChangedCase)) 'artifact_root comparison.system_compiler_summary formation_drift missing changed case'
Assert-Condition (@($rootSummaryInspectResult.comparison.system_compiler_summary.formation_drift.status_change_matrix).Count -gt 0) 'artifact_root comparison.system_compiler_summary formation_drift.status_change_matrix must expose formation drift transitions'
$formationDriftStatusTransition = @(
    @($rootSummaryInspectResult.comparison.system_compiler_summary.formation_drift.status_change_matrix) |
        Where-Object { [string]$_.transition -eq 'formed->blocked' } |
        Select-Object -First 1
) | Select-Object -First 1
Assert-Condition ($null -ne $formationDriftStatusTransition) 'artifact_root comparison.system_compiler_summary formation_drift missing formed->blocked status transition'
Assert-Condition ([int]$formationDriftStatusTransition.case_count -eq 1) 'artifact_root comparison.system_compiler_summary formation_drift formed->blocked status transition must only cover changed case'
Assert-Condition ((@($rootSummaryInspectResult.comparison.system_compiler_summary.formation_drift.unresolved_capability_change_matrix | ForEach-Object { [string]$_.capability }) -contains $RemovedCapability)) 'artifact_root comparison.system_compiler_summary formation_drift unresolved_capability_change_matrix missing removed capability'
Assert-Condition ((@($rootSummaryInspectResult.comparison.system_compiler_summary.formation_drift.blocked_node_change_matrix | ForEach-Object { [string]$_.node }) -contains $BlockedNode)) 'artifact_root comparison.system_compiler_summary formation_drift blocked_node_change_matrix missing blocked node'
Assert-Condition (@($rootSummaryInspectResult.comparison.system_compiler_summary.formation_drift.blocker_change_matrix).Count -gt 0) 'artifact_root comparison.system_compiler_summary formation_drift.blocker_change_matrix must expose blocker drift hotspots'
Assert-Condition (@($rootSummaryInspectResult.comparison.system_compiler_summary.formation_drift.blocker_reason_change_matrix).Count -gt 0) 'artifact_root comparison.system_compiler_summary formation_drift.blocker_reason_change_matrix must expose blocker reason drift hotspots'
Assert-Condition (@($rootSummaryInspectResult.comparison.system_compiler_summary.formation_drift.blocker_depends_on_change_matrix).Count -gt 0) 'artifact_root comparison.system_compiler_summary formation_drift.blocker_depends_on_change_matrix must expose dependency drift hotspots'
Assert-Condition ([int]$rootSummaryInspectResult.comparison.system_compiler_summary.binding_drift.changed_case_count -eq 1) 'artifact_root comparison.system_compiler_summary binding_drift.changed_case_count must be 1'
Assert-Condition ([int]$rootSummaryInspectResult.comparison.system_compiler_summary.binding_drift.binding_change_count -gt 0) 'artifact_root comparison.system_compiler_summary binding_drift.binding_change_count must expose binding drift'
Assert-Condition ((@($rootSummaryInspectResult.comparison.system_compiler_summary.binding_drift.changed_cases) -contains $ChangedCase)) 'artifact_root comparison.system_compiler_summary binding_drift missing changed case'
Assert-Condition (@($rootSummaryInspectResult.comparison.system_compiler_summary.binding_drift.reason_change_matrix).Count -gt 0) 'artifact_root comparison.system_compiler_summary binding_drift.reason_change_matrix must expose binding drift hotspots'
Assert-Condition ((@($rootSummaryInspectResult.comparison.system_compiler_summary.binding_drift.unresolved_capability_change_matrix | ForEach-Object { [string]$_.capability }) -contains $RemovedCapability)) 'artifact_root comparison.system_compiler_summary binding_drift unresolved_capability_change_matrix missing removed capability'
Assert-Condition ([int]$rootSummaryInspectResult.comparison.system_compiler_summary.bringup_drift.changed_case_count -eq 1) 'artifact_root comparison.system_compiler_summary bringup_drift.changed_case_count must be 1'
Assert-Condition ([int]$rootSummaryInspectResult.comparison.system_compiler_summary.bringup_drift.entry_change_count -gt 0) 'artifact_root comparison.system_compiler_summary bringup_drift.entry_change_count must expose bringup drift'
Assert-Condition ((@($rootSummaryInspectResult.comparison.system_compiler_summary.bringup_drift.changed_cases) -contains $ChangedCase)) 'artifact_root comparison.system_compiler_summary bringup_drift missing changed case'
Assert-Condition (@($rootSummaryInspectResult.comparison.system_compiler_summary.bringup_drift.phase_change_matrix).Count -gt 0) 'artifact_root comparison.system_compiler_summary bringup_drift.phase_change_matrix must expose bringup phase drift hotspots'
Assert-Condition (@($rootSummaryInspectResult.comparison.system_compiler_summary.bringup_drift.dependency_change_matrix).Count -gt 0) 'artifact_root comparison.system_compiler_summary bringup_drift.dependency_change_matrix must expose bringup dependency drift hotspots'
Assert-Condition ((@($rootSummaryInspectResult.comparison.system_compiler_summary.bringup_drift.blocked_node_change_matrix | ForEach-Object { [string]$_.node }) -contains $BlockedNode)) 'artifact_root comparison.system_compiler_summary bringup_drift blocked_node_change_matrix missing blocked node'
Assert-Condition ([int]$rootSummaryInspectResult.comparison.binding_result_summary.changed_case_count -eq 1) 'artifact_root comparison.binding_result_summary changed_case_count must be 1'
Assert-Condition ([string]$rootSummaryInspectResult.comparison.binding_result_summary.kind -eq 'binding_result_summary/v0') 'artifact_root comparison.binding_result_summary kind mismatch'
Assert-Condition ([string]$rootSummaryInspectResult.comparison.binding_result_summary.mode -eq 'comparison') 'artifact_root comparison.binding_result_summary mode mismatch'
Assert-Condition ([int]$rootSummaryInspectResult.comparison.bringup_order_summary.changed_case_count -eq 1) 'artifact_root comparison.bringup_order_summary changed_case_count must be 1'
Assert-Condition ([string]$rootSummaryInspectResult.comparison.bringup_order_summary.kind -eq 'bringup_order_summary/v0') 'artifact_root comparison.bringup_order_summary kind mismatch'
Assert-Condition ([string]$rootSummaryInspectResult.comparison.bringup_order_summary.mode -eq 'comparison') 'artifact_root comparison.bringup_order_summary mode mismatch'
Assert-Condition ([string]$rootSummaryInspectResult.comparison.system_formation_summary.kind -eq 'system_formation_summary/v0') 'artifact_root comparison.system_formation_summary kind mismatch'
Assert-Condition ([string]$rootSummaryInspectResult.comparison.system_formation_summary.mode -eq 'comparison') 'artifact_root comparison.system_formation_summary mode mismatch'
Assert-Condition ((@($rootSummaryInspectResult.comparison.binding_result_summary.changed_cases) -contains $ChangedCase)) 'artifact_root comparison.binding_result_summary missing changed case'
Assert-Condition ((@($rootSummaryInspectResult.comparison.bringup_order_summary.changed_cases) -contains $ChangedCase)) 'artifact_root comparison.bringup_order_summary missing changed case'
Assert-Condition ((@($rootSummaryInspectResult.comparison.binding_result_summary.unresolved_capability_change_matrix | ForEach-Object { [string]$_.capability }) -contains $RemovedCapability)) 'artifact_root comparison.binding_result_summary unresolved_capability_change_matrix missing removed capability'
Assert-Condition ((@($rootSummaryInspectResult.comparison.bringup_order_summary.blocked_node_change_matrix | ForEach-Object { [string]$_.node }) -contains $BlockedNode)) 'artifact_root comparison.bringup_order_summary blocked_node_change_matrix missing blocked node'
Assert-Condition ([int]$rootSummaryInspectResult.comparison.system_formation_summary.changed_case_count -eq 1) 'artifact_root comparison.system_formation_summary changed_case_count must be 1'
Assert-Condition ((@($rootSummaryInspectResult.comparison.system_formation_summary.changed_cases) -contains $ChangedCase)) 'artifact_root comparison.system_formation_summary missing changed case'
$formationStatusTransition = @(
    @($rootSummaryInspectResult.comparison.system_formation_summary.status_change_matrix) |
        Where-Object { [string]$_.transition -eq 'formed->blocked' } |
        Select-Object -First 1
) | Select-Object -First 1
Assert-Condition ($null -ne $formationStatusTransition) 'artifact_root comparison.system_formation_summary missing formed->blocked status transition'
Assert-Condition ((@($rootSummaryInspectResult.comparison.system_formation_summary.unresolved_capability_change_matrix | ForEach-Object { [string]$_.capability }) -contains $RemovedCapability)) 'artifact_root comparison.system_formation_summary unresolved_capability_change_matrix missing removed capability'
Assert-Condition ((@($rootSummaryInspectResult.comparison.system_formation_summary.blocked_node_change_matrix | ForEach-Object { [string]$_.node }) -contains $BlockedNode)) 'artifact_root comparison.system_formation_summary blocked_node_change_matrix missing blocked node'
$compilerStatusTransition = @(
    @($rootSummaryInspectResult.comparison.system_compiler_summary.status_change_matrix) |
        Where-Object { [string]$_.transition -eq 'formed->blocked' } |
        Select-Object -First 1
) | Select-Object -First 1
Assert-Condition ($null -ne $compilerStatusTransition) 'artifact_root comparison.system_compiler_summary missing formed->blocked status transition'
Assert-Condition ((@($rootSummaryInspectResult.comparison.system_compiler_summary.unresolved_capability_change_matrix | ForEach-Object { [string]$_.capability }) -contains $RemovedCapability)) 'artifact_root comparison.system_compiler_summary unresolved_capability_change_matrix missing removed capability'
Assert-Condition ((@($rootSummaryInspectResult.comparison.system_compiler_summary.blocked_node_change_matrix | ForEach-Object { [string]$_.node }) -contains $BlockedNode)) 'artifact_root comparison.system_compiler_summary blocked_node_change_matrix missing blocked node'

$changedCompilerSummary = @(
    @($rootSummaryInspectResult.system_compiler_summary.cases) |
        Where-Object { [string]$_.case -eq $ChangedCase } |
        Select-Object -First 1
) | Select-Object -First 1
$changedCompilerComparisonSummary = @(
    @($rootSummaryInspectResult.comparison.system_compiler_summary.cases) |
        Where-Object { [string]$_.case -eq $ChangedCase } |
        Select-Object -First 1
) | Select-Object -First 1
Assert-Condition ($null -ne $changedCompilerSummary) "artifact_root system_compiler_summary missing case row: $ChangedCase"
Assert-Condition ($null -ne $changedCompilerComparisonSummary) "artifact_root comparison.system_compiler_summary missing case row: $ChangedCase"
Assert-Condition ($null -ne $changedCompilerSummary.formation_basis) 'artifact_root system_compiler_summary changed case must expose formation_basis'
Assert-Condition ($null -ne $changedCompilerSummary.binding_summary) 'artifact_root system_compiler_summary changed case must expose binding_summary'
Assert-Condition ($null -ne $changedCompilerSummary.bringup_summary) 'artifact_root system_compiler_summary changed case must expose bringup_summary'
Assert-Condition ((@($changedCompilerSummary.binding_summary.unresolved_capabilities) -contains $RemovedCapability)) 'artifact_root system_compiler_summary changed case binding_summary must expose removed capability as unresolved'
Assert-Condition ((@($changedCompilerSummary.bringup_summary.blocked_nodes) -contains $BlockedNode)) 'artifact_root system_compiler_summary changed case bringup_summary must expose blocked node'
Assert-Condition ($null -ne $rootSummaryInspectResult.system_compiler_summary.blocker_reason_matrix) 'artifact_root system_compiler_summary must expose blocker_reason_matrix'
Assert-Condition ($null -ne $rootSummaryInspectResult.system_compiler_summary.blocker_missing_requires_matrix) 'artifact_root system_compiler_summary must expose blocker_missing_requires_matrix'
Assert-Condition ($null -ne $rootSummaryInspectResult.system_compiler_summary.blocker_depends_on_matrix) 'artifact_root system_compiler_summary must expose blocker_depends_on_matrix'
Assert-Condition ($null -ne $rootSummaryInspectResult.system_compiler_summary.binding_reason_matrix) 'artifact_root system_compiler_summary must expose binding_reason_matrix'
Assert-Condition ($null -ne $rootSummaryInspectResult.system_compiler_summary.bringup_phase_matrix) 'artifact_root system_compiler_summary must expose bringup_phase_matrix'
Assert-Condition ($null -ne $rootSummaryInspectResult.system_compiler_summary.bringup_dependency_matrix) 'artifact_root system_compiler_summary must expose bringup_dependency_matrix'
Assert-Condition (@($rootSummaryInspectResult.system_compiler_summary.formation_basis.blocker_reason_matrix).Count -gt 0) 'artifact_root system_compiler_summary formation_basis.blocker_reason_matrix must expose blocker hotspots'
Assert-Condition (@($rootSummaryInspectResult.system_compiler_summary.blocker_reason_matrix).Count -gt 0) 'artifact_root system_compiler_summary blocker_reason_matrix must expose blocker hotspots'
$missingRequireEntry = @(
    @($rootSummaryInspectResult.system_compiler_summary.blocker_missing_requires_matrix) |
        Where-Object { [string]$_.require -eq $RemovedCapability } |
        Select-Object -First 1
) | Select-Object -First 1
Assert-Condition ($null -ne $missingRequireEntry) 'artifact_root system_compiler_summary blocker_missing_requires_matrix missing removed capability'
Assert-Condition ((@($rootSummaryInspectResult.system_compiler_summary.formation_basis.blocked_node_matrix | ForEach-Object { [string]$_.node }) -contains $BlockedNode)) 'artifact_root system_compiler_summary formation_basis blocked_node_matrix missing blocked node'
Assert-Condition (@($rootSummaryInspectResult.system_compiler_summary.blocker_depends_on_matrix).Count -gt 0) 'artifact_root system_compiler_summary blocker_depends_on_matrix must expose dependency hotspots'
Assert-Condition (@($rootSummaryInspectResult.system_compiler_summary.binding_reason_matrix).Count -gt 0) 'artifact_root system_compiler_summary binding_reason_matrix must expose binding hotspots'
Assert-Condition (@($rootSummaryInspectResult.system_compiler_summary.bringup_phase_matrix).Count -gt 0) 'artifact_root system_compiler_summary bringup_phase_matrix must expose bringup hotspots'
Assert-Condition (@($rootSummaryInspectResult.system_compiler_summary.bringup_dependency_matrix).Count -gt 0) 'artifact_root system_compiler_summary bringup_dependency_matrix must expose bringup dependencies'
Assert-Condition ($null -ne $changedCompilerComparisonSummary.formation_basis_changes) 'artifact_root comparison.system_compiler_summary changed case must expose formation_basis_changes'
Assert-Condition ($null -ne $changedCompilerComparisonSummary.binding_summary_changes) 'artifact_root comparison.system_compiler_summary changed case must expose binding_summary_changes'
Assert-Condition ($null -ne $changedCompilerComparisonSummary.bringup_summary_changes) 'artifact_root comparison.system_compiler_summary changed case must expose bringup_summary_changes'
Assert-Condition ((@($changedCompilerComparisonSummary.binding_summary_changes.unresolved_capability_changes.added) -contains $RemovedCapability)) 'artifact_root comparison.system_compiler_summary binding_summary_changes must include removed capability'
Assert-Condition ((@($changedCompilerComparisonSummary.bringup_summary_changes.blocked_node_changes.added) -contains $BlockedNode)) 'artifact_root comparison.system_compiler_summary bringup_summary_changes must include blocked node'
Assert-Condition ([int]$changedCompilerComparisonSummary.binding_summary_changes.binding_change_count -gt 0) 'artifact_root comparison.system_compiler_summary binding_summary_changes must expose changed bindings'
Assert-Condition ([int]$changedCompilerComparisonSummary.bringup_summary_changes.entry_change_count -gt 0) 'artifact_root comparison.system_compiler_summary bringup_summary_changes must expose changed bringup entries'
Assert-Condition ($null -ne $rootSummaryInspectResult.comparison.system_compiler_summary.blocker_reason_change_matrix) 'artifact_root comparison.system_compiler_summary must expose blocker_reason_change_matrix'
Assert-Condition ($null -ne $rootSummaryInspectResult.comparison.system_compiler_summary.blocker_missing_requires_change_matrix) 'artifact_root comparison.system_compiler_summary must expose blocker_missing_requires_change_matrix'
Assert-Condition ($null -ne $rootSummaryInspectResult.comparison.system_compiler_summary.blocker_depends_on_change_matrix) 'artifact_root comparison.system_compiler_summary must expose blocker_depends_on_change_matrix'
Assert-Condition ($null -ne $rootSummaryInspectResult.comparison.system_compiler_summary.binding_reason_change_matrix) 'artifact_root comparison.system_compiler_summary must expose binding_reason_change_matrix'
Assert-Condition ($null -ne $rootSummaryInspectResult.comparison.system_compiler_summary.bringup_phase_change_matrix) 'artifact_root comparison.system_compiler_summary must expose bringup_phase_change_matrix'
Assert-Condition ($null -ne $rootSummaryInspectResult.comparison.system_compiler_summary.bringup_dependency_change_matrix) 'artifact_root comparison.system_compiler_summary must expose bringup_dependency_change_matrix'
Assert-Condition (@($rootSummaryInspectResult.comparison.system_compiler_summary.blocker_reason_change_matrix).Count -gt 0) 'artifact_root comparison.system_compiler_summary blocker_reason_change_matrix must expose blocker drift hotspots'
$missingRequireChangeEntry = @(
    @($rootSummaryInspectResult.comparison.system_compiler_summary.blocker_missing_requires_change_matrix) |
        Where-Object { [string]$_.require -eq $RemovedCapability } |
        Select-Object -First 1
) | Select-Object -First 1
Assert-Condition ($null -ne $missingRequireChangeEntry) 'artifact_root comparison.system_compiler_summary blocker_missing_requires_change_matrix missing removed capability'
Assert-Condition (@($rootSummaryInspectResult.comparison.system_compiler_summary.blocker_depends_on_change_matrix).Count -gt 0) 'artifact_root comparison.system_compiler_summary blocker_depends_on_change_matrix must expose dependency drift hotspots'
Assert-Condition (@($rootSummaryInspectResult.comparison.system_compiler_summary.binding_reason_change_matrix).Count -gt 0) 'artifact_root comparison.system_compiler_summary binding_reason_change_matrix must expose binding drift hotspots'
Assert-Condition (@($rootSummaryInspectResult.comparison.system_compiler_summary.bringup_phase_change_matrix).Count -gt 0) 'artifact_root comparison.system_compiler_summary bringup_phase_change_matrix must expose bringup phase drift hotspots'
Assert-Condition (@($rootSummaryInspectResult.comparison.system_compiler_summary.bringup_dependency_change_matrix).Count -gt 0) 'artifact_root comparison.system_compiler_summary bringup_dependency_change_matrix must expose bringup dependency drift hotspots'

$changedCaseSummary = Get-CaseSummaryRow -Rows @($rootSummaryInspectResult.cases) -CaseName $ChangedCase
$unchangedCaseSummary = Get-CaseSummaryRow -Rows @($rootSummaryInspectResult.cases) -CaseName $ExpectedUnchangedCase
Assert-Condition ($null -ne $changedCaseSummary) "artifact_root summary missing case row: $ChangedCase"
Assert-Condition ($null -ne $unchangedCaseSummary) "artifact_root summary missing case row: $ExpectedUnchangedCase"
Assert-Condition ([string]$changedCaseSummary.Formation -eq 'blocked') 'artifact_root summary changed case Formation must become blocked'
Assert-Condition ([int]$changedCaseSummary.FormCmp -gt 0) 'artifact_root summary changed case FormCmp must be nonzero'
Assert-Condition ([int]$changedCaseSummary.BindCmp -gt 0) 'artifact_root summary changed case BindCmp must be nonzero'
Assert-Condition ([int]$changedCaseSummary.OrdCmp -gt 0) 'artifact_root summary changed case OrdCmp must be nonzero'
Assert-Condition ([string]$unchangedCaseSummary.Formation -eq 'formed') 'artifact_root summary unchanged case Formation must stay formed'
Assert-Condition ([int]$unchangedCaseSummary.FormCmp -eq 0) 'artifact_root summary unchanged case FormCmp must stay zero'
Assert-Condition ([int]$unchangedCaseSummary.BindCmp -eq 0) 'artifact_root summary unchanged case BindCmp must stay zero'
Assert-Condition ([int]$unchangedCaseSummary.OrdCmp -eq 0) 'artifact_root summary unchanged case OrdCmp must stay zero'

$summary = [ordered]@{
    left_bundle_root = $leftBundleRoot
    right_bundle_root = $rightBundleRoot
    changed_case = $ChangedCase
    unchanged_case = $ExpectedUnchangedCase
    removed_capability = $RemovedCapability
    blocked_node = $BlockedNode
    diff_json = $diffJsonPath
    artifact_report_root = $artifactReportOutputRoot
    artifact_report_index = $artifactReportIndexPath
    captures = [ordered]@{
        report_summary = $reportInspectJsonPath
        root_summary = $rootSummaryInspectJsonPath
    }
    assertions = [ordered]@{
        diff_marks_system_formation_as_changed = $true
        artifact_report_exposes_system_formation = $true
        system_compiler_summary_supported = $true
        compiler_headline_supported = $true
        artifact_report_index_compiler_headline_supported = $true
        formation_headline_supported = $true
        binding_result_compare_supported = $true
        bringup_order_compare_supported = $true
        default_report_summary_exposes_system_formation_counts = $true
        default_root_summary_exposes_system_formation_counts = $true
    }
}
$summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $summaryPath -Encoding utf8

Write-Host "[OK] system formation compare smoke passed"
Write-Host "[ARTIFACT] $artifactReportOutputRoot"
Write-Host "[SUMMARY]  $summaryPath"
