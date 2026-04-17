param(
    [string[]]$ExportCases = @(
        'bringup-block-observe-demo',
        'bringup-minimal-observe-demo',
        'usb-host-runtime-multi-smoke'
    ),
    [string[]]$SubsetCases = @(
        'bringup-block-observe-demo',
        'bringup-minimal-observe-demo'
    ),
    [string]$ReportCase = 'bringup-minimal-observe-demo',
    [string]$RuntimeCase = 'usb-host-runtime-multi-smoke',
    [string]$WhyCapability = 'io.uart1',
    [string]$GraphPathCapability = 'io.uart1',
    [string]$AggregatedCapability = 'block.usb0',
    [string]$OutputRoot = 'out/system-compiler-explain-surface-contract-smoke',
    [switch]$KeepOutput,
    [switch]$SkipCompareSmokes
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

function Get-ObjectPropertyValue {
    param(
        $Object,
        [string]$PropertyName
    )

    if ($null -eq $Object) {
        return $null
    }

    $property = $Object.PSObject.Properties[$PropertyName]
    if ($null -eq $property) {
        return $null
    }

    return $property.Value
}

function Find-CapabilityEntry {
    param(
        [object[]]$Entries,
        [string]$CapabilityName
    )

    return @(
        @($Entries) |
            Where-Object { [string]$_.capability -eq $CapabilityName } |
            Select-Object -First 1
    ) | Select-Object -First 1
}

function Find-CaseEntry {
    param(
        [object[]]$Entries,
        [string]$CaseName
    )

    return @(
        @($Entries) |
            Where-Object { [string](Get-ObjectPropertyValue -Object $_ -PropertyName 'case') -eq $CaseName } |
            Select-Object -First 1
    ) | Select-Object -First 1
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

function Invoke-ExpectedFailure {
    param(
        [scriptblock]$Command,
        [string]$ExpectedMessage,
        [string]$OutputPath
    )

    $message = $null
    try {
        $null = (& $Command | Out-String)
        throw 'command unexpectedly succeeded'
    } catch {
        $message = $_.Exception.Message
    }

    $message | Set-Content -LiteralPath $OutputPath -Encoding utf8
    Assert-Condition ([string]$message -eq $ExpectedMessage) "unexpected error message. expected '$ExpectedMessage', actual '$message'"
    return $message
}

function Invoke-SmokeSubtest {
    param(
        [string]$Name,
        [string]$ScriptPath,
        [string]$BundleRootPath,
        [string]$SubOutputRoot
    )

    & $ScriptPath -BundleRoot $BundleRootPath -OutputRoot $SubOutputRoot
    if ($LASTEXITCODE -ne 0) {
        throw "$Name smoke failed"
    }

    $summaryFile = @(
        Get-ChildItem -LiteralPath $SubOutputRoot -Filter '*.summary.json' -File |
            Select-Object -First 1
    ) | Select-Object -First 1
    Assert-Condition ($null -ne $summaryFile) "$Name smoke summary not found under $SubOutputRoot"
    return $summaryFile.FullName
}

function New-BundleSubsetCopy {
    param(
        [string]$SourceBundleRoot,
        [string]$DestinationRoot,
        [string[]]$CaseNames
    )

    Remove-PathIfExists -Path $DestinationRoot
    Copy-Item -LiteralPath $SourceBundleRoot -Destination $DestinationRoot -Recurse -Force

    $indexPath = Join-Path $DestinationRoot 'index.json'
    Assert-Condition (Test-Path $indexPath) "subset bundle index not found: $indexPath"

    $indexData = Get-Content -LiteralPath $indexPath -Raw -Encoding utf8 | ConvertFrom-Json
    $filteredCases = @(
        @($indexData.cases) |
            Where-Object { @($CaseNames) -contains [string]$_.name }
    )
    Assert-Condition (@($filteredCases).Count -eq @($CaseNames).Count) 'subset bundle copy could not preserve requested cases'

    $indexData.cases = @($filteredCases)
    $indexData | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $indexPath -Encoding utf8

    return $DestinationRoot
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$resolvedOutputRoot = Resolve-FullPath $OutputRoot
$bundleRoot = Join-Path $resolvedOutputRoot 'bundle'
$artifactReportRoot = Join-Path $resolvedOutputRoot 'artifact-report'
$capturesRoot = Join-Path $resolvedOutputRoot 'captures'
$compareRoot = Join-Path $resolvedOutputRoot 'compare-smokes'
$summaryPath = Join-Path $resolvedOutputRoot 'system_compiler_explain_surface_contract_smoke.summary.json'

$exportScript = Join-Path $PSScriptRoot 'export_materialized_graph.ps1'
$artifactReportScript = Join-Path $PSScriptRoot 'export_system_compiler_artifact_report.ps1'
$inspectScript = Join-Path $PSScriptRoot 'inspect_system_compiler_artifact_report.ps1'
$bringupCompareSmokeScript = Join-Path $PSScriptRoot 'materialized_graph_bringup_evidence_compare_smoke.ps1'
$bringupCompareRootSmokeScript = Join-Path $PSScriptRoot 'materialized_graph_bringup_evidence_compare_root_smoke.ps1'
$resourceCompareSmokeScript = Join-Path $PSScriptRoot 'materialized_graph_resource_contract_compare_smoke.ps1'
$resourceCompareRootSmokeScript = Join-Path $PSScriptRoot 'materialized_graph_resource_contract_compare_root_smoke.ps1'
$systemInputCompareSmokeScript = Join-Path $PSScriptRoot 'materialized_graph_system_input_compare_smoke.ps1'
$systemFormationCompareSmokeScript = Join-Path $PSScriptRoot 'materialized_graph_system_formation_compare_smoke.ps1'

foreach ($requiredPath in @(
    $exportScript,
    $artifactReportScript,
    $inspectScript,
    $bringupCompareSmokeScript,
    $bringupCompareRootSmokeScript,
    $resourceCompareSmokeScript,
    $resourceCompareRootSmokeScript,
    $systemInputCompareSmokeScript,
    $systemFormationCompareSmokeScript
)) {
    if (-not (Test-Path $requiredPath)) {
        throw "required path not found: $requiredPath"
    }
}

Assert-Condition ((@($ExportCases) -contains $ReportCase)) "export cases must include report case: $ReportCase"
Assert-Condition ((@($ExportCases) -contains $RuntimeCase)) "export cases must include runtime case: $RuntimeCase"
foreach ($subsetCase in @($SubsetCases)) {
    Assert-Condition ((@($ExportCases) -contains $subsetCase)) "subset case missing from export cases: $subsetCase"
}

if (-not $KeepOutput) {
    Remove-PathIfExists -Path $resolvedOutputRoot
}
Ensure-Directory -Path $resolvedOutputRoot
Ensure-Directory -Path $capturesRoot

& $exportScript -Case $ExportCases -OutputRoot $bundleRoot
if ($LASTEXITCODE -ne 0) {
    throw 'export bundle generation failed'
}

$bundleIndexPath = Join-Path $bundleRoot 'index.json'
Assert-Condition (Test-Path $bundleIndexPath) "bundle index not found: $bundleIndexPath"

& $artifactReportScript -BundleRoot $bundleRoot -OutputRoot $artifactReportRoot
if ($LASTEXITCODE -ne 0) {
    throw 'artifact report export failed'
}

$artifactReports = @(
    Get-ChildItem -LiteralPath $artifactReportRoot -Filter '*.artifact_report.json' -File |
        Sort-Object Name
)
Assert-Condition ($artifactReports.Count -eq @($ExportCases).Count) 'artifact report count mismatch for explain surface contract smoke'

$reportSummary = Invoke-CommandJson -OutputPath (Join-Path $capturesRoot 'summary.report.json') -Command {
    & $inspectScript -ArtifactRoot $artifactReportRoot -Case $ReportCase -AsJson
}
Assert-Condition ([string]$reportSummary.summary.Case -eq $ReportCase) 'default report summary case mismatch'
Assert-Condition ([string]$reportSummary.summary.Mode -eq 'export_only') 'default report summary mode must stay export_only'
Assert-Condition ($null -eq $reportSummary.comparison) 'default report summary comparison must stay null in export_only mode'
Assert-Condition ($null -ne $reportSummary.system_input) 'default report summary must expose system_input'
Assert-Condition ([string]$reportSummary.system_input.system_spec.case_name -eq $ReportCase) 'default report summary system_input.system_spec.case_name mismatch'
Assert-Condition ([string]$reportSummary.system_input.system_spec.case_kind -eq 'materialized_graph') 'default report summary system_input.system_spec.case_kind mismatch'
Assert-Condition ([string]$reportSummary.system_input.resolved_input.board.source -eq 'case_subject') 'default report summary resolved board source must stay case_subject'
Assert-Condition ($null -ne $reportSummary.binding_result) 'default report summary must expose binding_result'
Assert-Condition ($null -ne $reportSummary.bringup_order) 'default report summary must expose bringup_order'
Assert-Condition ($null -ne $reportSummary.system_formation) 'default report summary must expose system_formation'
Assert-Condition ($null -ne $reportSummary.fact_resolution) 'default report summary must expose fact_resolution'
Assert-Condition ([int]$reportSummary.binding_result.required_binding_count -gt 0) 'materialized report binding_result must expose required bindings'
Assert-Condition ([int]$reportSummary.binding_result.unresolved_binding_count -eq @($reportSummary.structure.unresolved_bindings).Count) 'binding_result unresolved count must match structure.unresolved_bindings'
Assert-Condition ([int]$reportSummary.bringup_order.ordered_node_count -eq [int]$reportSummary.structure.node_count) 'bringup_order ordered_node_count must match structure.node_count'
Assert-Condition (@($reportSummary.bringup_order.entries).Count -eq [int]$reportSummary.bringup_order.ordered_node_count) 'bringup_order entries length mismatch'
Assert-Condition ([string]$reportSummary.system_formation.status -eq 'formed') 'materialized report system_formation must stay formed'
Assert-Condition ([int]$reportSummary.system_formation.blocker_count -eq 0) 'materialized report system_formation must not expose blockers'
Assert-Condition (@($reportSummary.fact_resolution.fact_inventory.required_facts).Count -gt 0) 'fact_resolution must expose required_facts inventory'
Assert-Condition ((@($reportSummary.fact_resolution.fact_inventory.required_facts) -contains 'system.clock')) 'fact_resolution required_facts must include system.clock'
Assert-Condition ([string]$reportSummary.summary.Formation -eq 'formed') 'default report summary Formation must stay formed'
Assert-Condition ([int]$reportSummary.summary.FormCmp -eq 0) 'default report summary FormCmp must stay zero in export_only mode'

$runtimeSummary = Invoke-CommandJson -OutputPath (Join-Path $capturesRoot 'summary.runtime_report.json') -Command {
    & $inspectScript -ArtifactRoot $artifactReportRoot -Case $RuntimeCase -AsJson
}
Assert-Condition ([string]$runtimeSummary.summary.Case -eq $RuntimeCase) 'runtime report summary case mismatch'
Assert-Condition ($null -ne $runtimeSummary.system_input) 'runtime report summary must expose system_input'
Assert-Condition ($null -ne $runtimeSummary.binding_result) 'runtime report summary must expose binding_result'
Assert-Condition ($null -ne $runtimeSummary.bringup_order) 'runtime report summary must expose bringup_order'
Assert-Condition ($null -ne $runtimeSummary.system_formation) 'runtime report summary must expose system_formation'
Assert-Condition ([string]$runtimeSummary.system_input.system_spec.case_kind -eq 'runtime_only') 'runtime report summary case_kind must stay runtime_only'
Assert-Condition ([int]$runtimeSummary.binding_result.required_binding_count -eq 0) 'runtime_only report binding_result must stay empty'
Assert-Condition ([int]$runtimeSummary.bringup_order.ordered_node_count -eq 0) 'runtime_only report bringup_order must stay empty'
Assert-Condition ([string]$runtimeSummary.system_formation.status -eq 'formed') 'runtime_only report system_formation must stay formed'
Assert-Condition ([int]$runtimeSummary.system_formation.binding_summary.required_binding_count -eq 0) 'runtime_only report system_formation must expose empty binding summary'
Assert-Condition ([int]$runtimeSummary.system_formation.bringup_summary.ordered_node_count -eq 0) 'runtime_only report system_formation must expose empty bringup summary'
Assert-Condition ([int]$runtimeSummary.system_formation.blocker_count -eq 0) 'runtime_only report system_formation must stay blocker-free'
Assert-Condition ([string]$runtimeSummary.summary.Formation -eq 'formed') 'runtime report summary Formation must stay formed'
Assert-Condition ([int]$runtimeSummary.summary.FormCmp -eq 0) 'runtime report summary FormCmp must stay zero in export_only mode'

$rootSummary = Invoke-CommandJson -OutputPath (Join-Path $capturesRoot 'summary.root.json') -Command {
    & $inspectScript -ArtifactRoot $artifactReportRoot -AsJson
}
Assert-Condition ([int]$rootSummary.case_count -eq @($ExportCases).Count) 'default artifact_root summary case_count mismatch'
Assert-Condition (@($rootSummary.cases).Count -eq @($ExportCases).Count) 'default artifact_root summary cases length mismatch'
Assert-Condition ($null -ne $rootSummary.binding_result_summary) 'default artifact_root summary must expose binding_result_summary'
Assert-Condition ($null -ne $rootSummary.bringup_order_summary) 'default artifact_root summary must expose bringup_order_summary'
Assert-Condition ($null -ne $rootSummary.system_formation_summary) 'default artifact_root summary must expose system_formation_summary'
Assert-Condition ($null -ne $rootSummary.fact_resolution_summary) 'default artifact_root summary must expose fact_resolution_summary'
Assert-Condition ([int]$rootSummary.binding_result_summary.case_count -eq @($ExportCases).Count) 'default artifact_root summary binding_result_summary.case_count mismatch'
Assert-Condition ([int]$rootSummary.bringup_order_summary.case_count -eq @($ExportCases).Count) 'default artifact_root summary bringup_order_summary.case_count mismatch'
Assert-Condition (@($rootSummary.binding_result_summary.capability_matrix).Count -gt 0) 'default artifact_root summary must expose binding_result capability_matrix'
Assert-Condition (@($rootSummary.bringup_order_summary.node_matrix).Count -gt 0) 'default artifact_root summary must expose bringup_order node_matrix'
Assert-Condition ([int]$rootSummary.system_formation_summary.case_count -eq @($ExportCases).Count) 'default artifact_root summary system_formation_summary.case_count mismatch'
Assert-Condition ([int]$rootSummary.system_formation_summary.formed_case_count -eq @($ExportCases).Count) 'default artifact_root summary formed_case_count mismatch'
Assert-Condition ([int]$rootSummary.system_formation_summary.blocked_case_count -eq 0) 'default artifact_root summary blocked_case_count must stay zero in export_only mode'
Assert-Condition (@($rootSummary.system_formation_summary.cases).Count -eq @($ExportCases).Count) 'default artifact_root summary system_formation_summary.cases length mismatch'
Assert-Condition ([int]$rootSummary.fact_resolution_summary.case_count -eq @($ExportCases).Count) 'default artifact_root summary fact_resolution_summary.case_count mismatch'
Assert-Condition (@($rootSummary.fact_resolution_summary.required_fact_matrix).Count -gt 0) 'default artifact_root summary must expose required_fact_matrix'
$rootReportSummary = @(
    @($rootSummary.cases) |
        Where-Object { [string]$_.Case -eq $ReportCase } |
        Select-Object -First 1
) | Select-Object -First 1
$rootRuntimeSummary = @(
    @($rootSummary.cases) |
        Where-Object { [string]$_.Case -eq $RuntimeCase } |
        Select-Object -First 1
) | Select-Object -First 1
Assert-Condition ($null -ne $rootReportSummary) "default artifact_root summary missing case row: $ReportCase"
Assert-Condition ($null -ne $rootRuntimeSummary) "default artifact_root summary missing case row: $RuntimeCase"
Assert-Condition ([string]$rootReportSummary.Formation -eq 'formed') 'default artifact_root summary report case Formation must stay formed'
Assert-Condition ([int]$rootReportSummary.FormCmp -eq 0) 'default artifact_root summary report case FormCmp must stay zero'
Assert-Condition ([string]$rootRuntimeSummary.Formation -eq 'formed') 'default artifact_root summary runtime case Formation must stay formed'
Assert-Condition ([int]$rootRuntimeSummary.FormCmp -eq 0) 'default artifact_root summary runtime case FormCmp must stay zero'

$subsetSummary = Invoke-CommandJson -OutputPath (Join-Path $capturesRoot 'summary.root_subset.json') -Command {
    & $inspectScript -ArtifactRoot $artifactReportRoot -Case $SubsetCases -AsJson
}
Assert-Condition ([int]$subsetSummary.case_count -eq @($SubsetCases).Count) 'subset artifact_root summary case_count mismatch'
Assert-Condition ($null -ne $subsetSummary.binding_result_summary) 'subset artifact_root summary must expose binding_result_summary'
Assert-Condition ($null -ne $subsetSummary.bringup_order_summary) 'subset artifact_root summary must expose bringup_order_summary'
Assert-Condition ($null -ne $subsetSummary.system_formation_summary) 'subset artifact_root summary must expose system_formation_summary'
Assert-Condition ($null -ne $subsetSummary.fact_resolution_summary) 'subset artifact_root summary must expose fact_resolution_summary'
Assert-Condition ([int]$subsetSummary.binding_result_summary.case_count -eq @($SubsetCases).Count) 'subset artifact_root binding_result_summary.case_count mismatch'
Assert-Condition ([int]$subsetSummary.bringup_order_summary.case_count -eq @($SubsetCases).Count) 'subset artifact_root bringup_order_summary.case_count mismatch'
Assert-Condition ([int]$subsetSummary.system_formation_summary.case_count -eq @($SubsetCases).Count) 'subset artifact_root system_formation_summary.case_count mismatch'
Assert-Condition ([int]$subsetSummary.fact_resolution_summary.case_count -eq @($SubsetCases).Count) 'subset artifact_root fact_resolution_summary.case_count mismatch'

$capListReport = Invoke-CommandJson -OutputPath (Join-Path $capturesRoot 'cap_list.report.json') -Command {
    & $inspectScript -ArtifactRoot $artifactReportRoot -Case $ReportCase -CapList -AsJson
}
Assert-Condition ([string]$capListReport.query.kind -eq 'cap_list') 'cap_list report query kind mismatch'
Assert-Condition ([string]$capListReport.query.scope -eq 'report') 'cap_list report scope mismatch'
Assert-Condition ($null -eq $capListReport.query.comparison) 'cap_list report comparison must stay null in export_only mode'
Assert-Condition (@($capListReport.query.capabilities).Count -gt 0) 'cap_list report must expose at least one capability'
Assert-Condition ($null -ne (Find-CapabilityEntry -Entries @($capListReport.query.capabilities) -CapabilityName $WhyCapability)) 'cap_list report missing target capability'

$capListRoot = Invoke-CommandJson -OutputPath (Join-Path $capturesRoot 'cap_list.root.json') -Command {
    & $inspectScript -ArtifactRoot $artifactReportRoot -CapList -AsJson
}
Assert-Condition ([string]$capListRoot.query.kind -eq 'cap_list') 'cap_list artifact_root query kind mismatch'
Assert-Condition ([string]$capListRoot.query.scope -eq 'artifact_root') 'cap_list artifact_root scope mismatch'
Assert-Condition ([int]$capListRoot.query.case_count -eq @($ExportCases).Count) 'cap_list artifact_root case_count mismatch'
Assert-Condition ($null -eq $capListRoot.query.comparison) 'cap_list artifact_root comparison must stay null in export_only mode'
$aggregatedCapabilityEntry = Find-CapabilityEntry -Entries @($capListRoot.query.capabilities) -CapabilityName $AggregatedCapability
Assert-Condition ($null -ne $aggregatedCapabilityEntry) "cap_list artifact_root missing aggregated capability: $AggregatedCapability"
Assert-Condition ((@($aggregatedCapabilityEntry.cases) -contains $RuntimeCase)) 'cap_list artifact_root aggregated capability must include runtime case'

Invoke-ExpectedFailure -OutputPath (Join-Path $capturesRoot 'cap_list.partial_root.error.txt') -ExpectedMessage '-CapList only supports a single selected report or full artifact root aggregation' -Command {
    & $inspectScript -ArtifactRoot $artifactReportRoot -Case $SubsetCases -CapList -AsJson
} | Out-Null

$whyReport = Invoke-CommandJson -OutputPath (Join-Path $capturesRoot 'why_capability.report.json') -Command {
    & $inspectScript -ArtifactRoot $artifactReportRoot -Case $ReportCase -WhyCapability $WhyCapability -AsJson
}
Assert-Condition ([string]$whyReport.query.kind -eq 'why_capability') 'why_capability report query kind mismatch'
Assert-Condition ([string]$whyReport.query.scope -eq 'report') 'why_capability report scope mismatch'
Assert-Condition ([string]$whyReport.query.capability -eq $WhyCapability) 'why_capability report capability mismatch'
Assert-Condition (-not [string]::IsNullOrWhiteSpace([string]$whyReport.query.state)) 'why_capability report state must not be empty'
Assert-Condition ($null -eq $whyReport.query.comparison) 'why_capability report comparison must stay null in export_only mode'

$whyRoot = Invoke-CommandJson -OutputPath (Join-Path $capturesRoot 'why_capability.root.json') -Command {
    & $inspectScript -ArtifactRoot $artifactReportRoot -WhyCapability $WhyCapability -AsJson
}
Assert-Condition ([string]$whyRoot.query.kind -eq 'why_capability') 'why_capability artifact_root query kind mismatch'
Assert-Condition ([string]$whyRoot.query.scope -eq 'artifact_root') 'why_capability artifact_root scope mismatch'
Assert-Condition ([string]$whyRoot.query.result.capability -eq $WhyCapability) 'why_capability artifact_root capability mismatch'
Assert-Condition ([int]$whyRoot.query.result.case_count -eq @($ExportCases).Count) 'why_capability artifact_root case_count mismatch'
Assert-Condition ([int]$whyRoot.query.result.compared_case_count -eq 0) 'why_capability artifact_root compared_case_count must stay 0 in export_only mode'
$whyRootCase = Find-CaseEntry -Entries @($whyRoot.query.result.cases) -CaseName $ReportCase
Assert-Condition ($null -ne $whyRootCase) "why_capability artifact_root missing report case: $ReportCase"

Invoke-ExpectedFailure -OutputPath (Join-Path $capturesRoot 'why_capability.partial_root.error.txt') -ExpectedMessage '-WhyCapability only supports a single selected report or full artifact root aggregation' -Command {
    & $inspectScript -ArtifactRoot $artifactReportRoot -Case $SubsetCases -WhyCapability $WhyCapability -AsJson
} | Out-Null

$graphPathReport = Invoke-CommandJson -OutputPath (Join-Path $capturesRoot 'graph_path.report.json') -Command {
    & $inspectScript -ArtifactRoot $artifactReportRoot -Case $ReportCase -GraphPath $GraphPathCapability -AsJson
}
Assert-Condition ([string]$graphPathReport.query.kind -eq 'graph_path') 'graph_path report query kind mismatch'
Assert-Condition ([string]$graphPathReport.query.scope -eq 'report') 'graph_path report scope mismatch'
Assert-Condition ([string]$graphPathReport.query.result.capability -eq $GraphPathCapability) 'graph_path report capability mismatch'
Assert-Condition ($null -eq $graphPathReport.query.result.comparison) 'graph_path report comparison must stay null in export_only mode'
Assert-Condition (@($graphPathReport.query.result.direct_edges).Count -gt 0) 'graph_path report must expose direct edges for target capability'

Invoke-ExpectedFailure -OutputPath (Join-Path $capturesRoot 'graph_path.root.error.txt') -ExpectedMessage '-GraphPath requires exactly one selected artifact report' -Command {
    & $inspectScript -ArtifactRoot $artifactReportRoot -GraphPath $GraphPathCapability -AsJson
} | Out-Null

$recentTransitionsReport = Invoke-CommandJson -OutputPath (Join-Path $capturesRoot 'recent_transitions.report.json') -Command {
    & $inspectScript -ArtifactRoot $artifactReportRoot -Case $RuntimeCase -RecentTransitions -AsJson
}
Assert-Condition ([string]$recentTransitionsReport.query.kind -eq 'recent_transitions') 'recent_transitions report query kind mismatch'
Assert-Condition ([string]$recentTransitionsReport.query.scope -eq 'report') 'recent_transitions report scope mismatch'
Assert-Condition ([int]$recentTransitionsReport.query.result.transition_count -gt 0) 'recent_transitions report must expose runtime transitions'
Assert-Condition ($null -eq $recentTransitionsReport.query.result.comparison) 'recent_transitions report comparison must stay null in export_only mode'
Assert-Condition (@($recentTransitionsReport.query.result.transitions).Count -eq [int]$recentTransitionsReport.query.result.transition_count) 'recent_transitions report transition list length mismatch'
Assert-Condition ((@($recentTransitionsReport.query.result.transition_capabilities) -contains $AggregatedCapability)) 'recent_transitions report must include runtime capability'

Invoke-ExpectedFailure -OutputPath (Join-Path $capturesRoot 'recent_transitions.root.error.txt') -ExpectedMessage '-RecentTransitions requires exactly one selected artifact report' -Command {
    & $inspectScript -ArtifactRoot $artifactReportRoot -RecentTransitions -AsJson
} | Out-Null

$bringupEvidenceReport = Invoke-CommandJson -OutputPath (Join-Path $capturesRoot 'bringup_evidence.report.json') -Command {
    & $inspectScript -ArtifactRoot $artifactReportRoot -Case $ReportCase -BringupEvidence -AsJson
}
Assert-Condition ([string]$bringupEvidenceReport.query.kind -eq 'bringup_evidence') 'bringup_evidence report query kind mismatch'
Assert-Condition ([string]$bringupEvidenceReport.query.scope -eq 'report') 'bringup_evidence report scope mismatch'
Assert-Condition (@($bringupEvidenceReport.query.result.evidence_entries).Count -gt 0) 'bringup_evidence report must expose evidence entries'
Assert-Condition ($null -eq (Get-ObjectPropertyValue -Object $bringupEvidenceReport.query -PropertyName 'comparison')) 'bringup_evidence report comparison must stay null in export_only mode'

$bringupEvidenceRoot = Invoke-CommandJson -OutputPath (Join-Path $capturesRoot 'bringup_evidence.root.json') -Command {
    & $inspectScript -ArtifactRoot $artifactReportRoot -BringupEvidence -AsJson
}
Assert-Condition ([string]$bringupEvidenceRoot.query.kind -eq 'bringup_evidence') 'bringup_evidence artifact_root query kind mismatch'
Assert-Condition ([string]$bringupEvidenceRoot.query.scope -eq 'artifact_root') 'bringup_evidence artifact_root scope mismatch'
Assert-Condition ([int]$bringupEvidenceRoot.query.result.case_count -eq @($ExportCases).Count) 'bringup_evidence artifact_root case_count mismatch'

$bringupEvidenceSubset = Invoke-CommandJson -OutputPath (Join-Path $capturesRoot 'bringup_evidence.root_subset.json') -Command {
    & $inspectScript -ArtifactRoot $artifactReportRoot -Case $SubsetCases -BringupEvidence -AsJson
}
Assert-Condition ([int]$bringupEvidenceSubset.query.result.case_count -eq @($SubsetCases).Count) 'bringup_evidence subset artifact_root case_count mismatch'

$resourceSummaryReport = Invoke-CommandJson -OutputPath (Join-Path $capturesRoot 'resource_summary.report.json') -Command {
    & $inspectScript -ArtifactRoot $artifactReportRoot -Case $ReportCase -ResourceSummary -AsJson
}
Assert-Condition ([string]$resourceSummaryReport.query.kind -eq 'resource_summary') 'resource_summary report query kind mismatch'
Assert-Condition ([string]$resourceSummaryReport.query.scope -eq 'report') 'resource_summary report scope mismatch'
Assert-Condition ([int]$resourceSummaryReport.query.result.declared_contracts -ge 1) 'resource_summary report must expose declared contracts'
Assert-Condition (@($resourceSummaryReport.query.result.fact_inventory.required_facts).Count -gt 0) 'resource_summary report must expose required_facts inventory'
Assert-Condition ($null -eq (Get-ObjectPropertyValue -Object $resourceSummaryReport.query -PropertyName 'comparison')) 'resource_summary report comparison must stay null in export_only mode'

$resourceSummaryRoot = Invoke-CommandJson -OutputPath (Join-Path $capturesRoot 'resource_summary.root.json') -Command {
    & $inspectScript -ArtifactRoot $artifactReportRoot -ResourceSummary -AsJson
}
Assert-Condition ([string]$resourceSummaryRoot.query.kind -eq 'resource_summary') 'resource_summary artifact_root query kind mismatch'
Assert-Condition ([string]$resourceSummaryRoot.query.scope -eq 'artifact_root') 'resource_summary artifact_root scope mismatch'
Assert-Condition ([int]$resourceSummaryRoot.query.result.case_count -eq @($ExportCases).Count) 'resource_summary artifact_root case_count mismatch'
Assert-Condition (@($resourceSummaryRoot.query.result.required_fact_matrix).Count -gt 0) 'resource_summary artifact_root must expose required_fact_matrix'

$resourceSummarySubset = Invoke-CommandJson -OutputPath (Join-Path $capturesRoot 'resource_summary.root_subset.json') -Command {
    & $inspectScript -ArtifactRoot $artifactReportRoot -Case $SubsetCases -ResourceSummary -AsJson
}
Assert-Condition ([int]$resourceSummarySubset.query.result.case_count -eq @($SubsetCases).Count) 'resource_summary subset artifact_root case_count mismatch'

$showTransitionsTextPath = Join-Path $capturesRoot 'show_transitions.report.txt'
$showTransitionsText = (& $inspectScript -ArtifactRoot $artifactReportRoot -Case $RuntimeCase -ShowTransitions 6>&1 | Out-String)
$showTransitionsText | Set-Content -LiteralPath $showTransitionsTextPath -Encoding utf8
Assert-Condition ($showTransitionsText.Contains('[TRANSITIONS]')) 'show transitions output must contain transitions section'
Assert-Condition (-not $showTransitionsText.Contains('[TRANSITION COMPARE]')) 'export_only show transitions must not print compare summary'

$compareSummaries = [ordered]@{}
if (-not $SkipCompareSmokes) {
    Ensure-Directory -Path $compareRoot
    $rootCompareBundleRoot = New-BundleSubsetCopy -SourceBundleRoot $bundleRoot -DestinationRoot (Join-Path $compareRoot 'root-compare-bundle') -CaseNames $SubsetCases

    $compareSummaries['bringup_report'] = Invoke-SmokeSubtest -Name 'bringup_report' -ScriptPath $bringupCompareSmokeScript -BundleRootPath $bundleRoot -SubOutputRoot (Join-Path $compareRoot 'bringup-report')
    $compareSummaries['bringup_root'] = Invoke-SmokeSubtest -Name 'bringup_root' -ScriptPath $bringupCompareRootSmokeScript -BundleRootPath $rootCompareBundleRoot -SubOutputRoot (Join-Path $compareRoot 'bringup-root')
    $compareSummaries['resource_report'] = Invoke-SmokeSubtest -Name 'resource_report' -ScriptPath $resourceCompareSmokeScript -BundleRootPath $bundleRoot -SubOutputRoot (Join-Path $compareRoot 'resource-report')
    $compareSummaries['resource_root'] = Invoke-SmokeSubtest -Name 'resource_root' -ScriptPath $resourceCompareRootSmokeScript -BundleRootPath $rootCompareBundleRoot -SubOutputRoot (Join-Path $compareRoot 'resource-root')
    $compareSummaries['system_input'] = Invoke-SmokeSubtest -Name 'system_input' -ScriptPath $systemInputCompareSmokeScript -BundleRootPath $rootCompareBundleRoot -SubOutputRoot (Join-Path $compareRoot 'system-input')
    $compareSummaries['system_formation'] = Invoke-SmokeSubtest -Name 'system_formation' -ScriptPath $systemFormationCompareSmokeScript -BundleRootPath $rootCompareBundleRoot -SubOutputRoot (Join-Path $compareRoot 'system-formation')
}

$summary = [ordered]@{
    bundle_root = $bundleRoot
    artifact_report_root = $artifactReportRoot
    export_cases = @($ExportCases)
    subset_cases = @($SubsetCases)
    report_case = $ReportCase
    runtime_case = $RuntimeCase
    why_capability = $WhyCapability
    graph_path_capability = $GraphPathCapability
    aggregated_capability = $AggregatedCapability
    captures = [ordered]@{
        summary_report = Join-Path $capturesRoot 'summary.report.json'
        summary_runtime_report = Join-Path $capturesRoot 'summary.runtime_report.json'
        summary_root = Join-Path $capturesRoot 'summary.root.json'
        summary_root_subset = Join-Path $capturesRoot 'summary.root_subset.json'
        cap_list_report = Join-Path $capturesRoot 'cap_list.report.json'
        cap_list_root = Join-Path $capturesRoot 'cap_list.root.json'
        why_report = Join-Path $capturesRoot 'why_capability.report.json'
        why_root = Join-Path $capturesRoot 'why_capability.root.json'
        graph_path_report = Join-Path $capturesRoot 'graph_path.report.json'
        recent_transitions_report = Join-Path $capturesRoot 'recent_transitions.report.json'
        bringup_evidence_report = Join-Path $capturesRoot 'bringup_evidence.report.json'
        bringup_evidence_root = Join-Path $capturesRoot 'bringup_evidence.root.json'
        bringup_evidence_root_subset = Join-Path $capturesRoot 'bringup_evidence.root_subset.json'
        resource_summary_report = Join-Path $capturesRoot 'resource_summary.report.json'
        resource_summary_root = Join-Path $capturesRoot 'resource_summary.root.json'
        resource_summary_root_subset = Join-Path $capturesRoot 'resource_summary.root_subset.json'
        show_transitions_text = $showTransitionsTextPath
        cap_list_partial_root_error = Join-Path $capturesRoot 'cap_list.partial_root.error.txt'
        why_partial_root_error = Join-Path $capturesRoot 'why_capability.partial_root.error.txt'
        graph_path_root_error = Join-Path $capturesRoot 'graph_path.root.error.txt'
        recent_transitions_root_error = Join-Path $capturesRoot 'recent_transitions.root.error.txt'
    }
    compare_smokes = $compareSummaries
    assertions = [ordered]@{
        default_summary_report_supported = $true
        default_summary_report_exposes_system_input = $true
        default_summary_report_exposes_binding_result = $true
        default_summary_report_exposes_bringup_order = $true
        default_summary_report_exposes_system_formation = $true
        default_summary_report_exposes_fact_resolution = $true
        runtime_only_summary_exposes_empty_binding_result = $true
        runtime_only_summary_exposes_empty_bringup_order = $true
        runtime_only_summary_exposes_formed_system_formation = $true
        default_summary_artifact_root_supported = $true
        default_summary_subset_supported = $true
        default_summary_artifact_root_exposes_fact_resolution = $true
        cap_list_report_supported = $true
        cap_list_artifact_root_supported = $true
        cap_list_partial_root_rejected = $true
        why_capability_report_supported = $true
        why_capability_artifact_root_supported = $true
        why_capability_partial_root_rejected = $true
        graph_path_report_supported = $true
        graph_path_artifact_root_rejected = $true
        recent_transitions_report_supported = $true
        recent_transitions_artifact_root_rejected = $true
        bringup_evidence_report_supported = $true
        bringup_evidence_artifact_root_supported = $true
        bringup_evidence_subset_supported = $true
        resource_summary_report_supported = $true
        resource_summary_report_exposes_required_facts = $true
        resource_summary_artifact_root_supported = $true
        resource_summary_artifact_root_exposes_required_fact_matrix = $true
        resource_summary_subset_supported = $true
        show_transitions_appendix_reuses_report_language = $true
        compare_smokes_reused = (-not $SkipCompareSmokes)
    }
}
$summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $summaryPath -Encoding utf8

Write-Host "[OK] explain surface contract smoke passed"
Write-Host "[BUNDLE]   $bundleRoot"
Write-Host "[ARTIFACT] $artifactReportRoot"
Write-Host "[SUMMARY]  $summaryPath"
