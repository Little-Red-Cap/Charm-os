param(
    [string]$ArtifactRoot = "out/system-compiler-artifact-report",
    [string]$Report = "",
    [string[]]$Case = @(),
    [string]$WhyCapability = "",
    [string]$GraphPath = "",
    [switch]$ResourceSummary,
    [switch]$RecentTransitions,
    [switch]$BringupEvidence,
    [switch]$CapList,
    [switch]$ListCases,
    [switch]$ShowArtifacts,
    [switch]$ShowTransitions,
    [switch]$AsJson
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'materialized_graph_schema.ps1')

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

function Get-ReportFiles {
    param(
        [string]$ArtifactRootPath
    )

    $resolvedRoot = Resolve-FullPath $ArtifactRootPath
    if (-not (Test-Path $resolvedRoot)) {
        throw "artifact root not found: $resolvedRoot"
    }

    return @(
        Get-ChildItem -LiteralPath $resolvedRoot -Filter '*.artifact_report.json' -File |
            Sort-Object Name
    )
}

function Load-ArtifactReport {
    param(
        [string]$Path
    )

    $resolvedPath = Resolve-FullPath $Path
    if (-not (Test-Path $resolvedPath)) {
        throw "artifact report not found: $resolvedPath"
    }

    $report = Get-Content -LiteralPath $resolvedPath -Raw -Encoding utf8 | ConvertFrom-Json
    if ([string]$report.schema -ne 'system_compiler.artifact_report/v0') {
        throw "unsupported artifact report schema: $([string]$report.schema)"
    }

    return [pscustomobject]@{
        Path = $resolvedPath
        Data = $report
    }
}

function Load-GraphFromArtifactReport {
    param(
        $ReportData
    )

    if ($null -eq $ReportData -or $null -eq $ReportData.PSObject.Properties['artifacts'] -or $null -eq $ReportData.artifacts) {
        return $null
    }

    if ($null -eq $ReportData.artifacts.PSObject.Properties['sample_json'] -or [string]::IsNullOrWhiteSpace([string]$ReportData.artifacts.sample_json)) {
        return $null
    }

    $sampleJsonPath = Resolve-FullPath ([string]$ReportData.artifacts.sample_json)
    if (-not (Test-Path $sampleJsonPath)) {
        return $null
    }

    $graph = Get-Content -LiteralPath $sampleJsonPath -Raw -Encoding utf8 | ConvertFrom-Json
    Assert-MaterializedGraphSampleShape -Graph $graph -Context $sampleJsonPath
    return [pscustomobject]@{
        Path = $sampleJsonPath
        Data = $graph
    }
}

function Get-SelectedReports {
    param(
        [string]$ArtifactRootPath
    )

    if (-not [string]::IsNullOrWhiteSpace($Report)) {
        return @(
            Load-ArtifactReport -Path $Report
        )
    }

    $reportFiles = @(Get-ReportFiles -ArtifactRootPath $ArtifactRootPath)
    if ($Case.Count -eq 0) {
        return @(
            $reportFiles |
                ForEach-Object { Load-ArtifactReport -Path $_.FullName }
        )
    }

    $reportMap = @{}
    foreach ($file in @($reportFiles)) {
        $caseName = [System.IO.Path]::GetFileNameWithoutExtension([System.IO.Path]::GetFileNameWithoutExtension($file.Name))
        $reportMap[$caseName] = $file.FullName
    }

    $selected = @()
    foreach ($caseName in @($Case)) {
        if (-not $reportMap.ContainsKey($caseName)) {
            throw "unknown artifact report case: $caseName"
        }
        $selected += Load-ArtifactReport -Path $reportMap[$caseName]
    }

    return @($selected)
}

function Format-StringArray {
    param(
        [string[]]$Values
    )

    return (@($Values) -join ', ')
}

function Format-BoolFlag {
    param(
        [bool]$Value
    )

    if ($Value) {
        return 'yes'
    }

    return 'no'
}

function Format-OptionalState {
    param(
        [AllowNull()]
        [string]$Value
    )

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return '-'
    }

    return $Value
}

function Format-StringArrayOrDash {
    param(
        [string[]]$Values
    )

    if (@($Values).Count -eq 0) {
        return '-'
    }

    return Format-StringArray -Values $Values
}

function Format-ResolvedScalarInputText {
    param(
        $ResolvedInput
    )

    if ($null -eq $ResolvedInput) {
        return '- [missing]'
    }

    $value = if ($null -ne $ResolvedInput.PSObject.Properties['value'] -and -not [string]::IsNullOrWhiteSpace([string]$ResolvedInput.value)) {
        [string]$ResolvedInput.value
    } else {
        '-'
    }
    $source = if ($null -ne $ResolvedInput.PSObject.Properties['source'] -and -not [string]::IsNullOrWhiteSpace([string]$ResolvedInput.source)) {
        [string]$ResolvedInput.source
    } else {
        'missing'
    }

    return "$value [$source]"
}

function Format-ResolvedFacetInputText {
    param(
        $ResolvedInput
    )

    if ($null -eq $ResolvedInput) {
        return '- [missing]'
    }

    $values = if ($null -ne $ResolvedInput.PSObject.Properties['values']) {
        @($ResolvedInput.values)
    } else {
        @()
    }
    $source = if ($null -ne $ResolvedInput.PSObject.Properties['source'] -and -not [string]::IsNullOrWhiteSpace([string]$ResolvedInput.source)) {
        [string]$ResolvedInput.source
    } else {
        'missing'
    }

    return "$(Format-StringArrayOrDash -Values $values) [$source]"
}

function Format-DeclaredContractText {
    param(
        $ContractEntry
    )

    if ($null -eq $ContractEntry) {
        return ''
    }

    $contractName = [string]$ContractEntry.contract
    if ([string]::IsNullOrWhiteSpace($contractName)) {
        return ''
    }

    $requires = if ($null -ne $ContractEntry.PSObject.Properties['requires']) {
        @($ContractEntry.requires)
    } else {
        @()
    }

    return "$contractName requires [$((@($requires) -join ', '))]"
}

function Format-BindingResultDisplayRow {
    param(
        $Entry
    )

    return [pscustomobject]@{
        Capability = [string]$Entry.capability
        State = [string]$Entry.state
        Providers = Format-StringArrayOrDash -Values @($Entry.provider_nodes)
        Consumers = Format-StringArrayOrDash -Values @($Entry.consumer_nodes)
        Reason = [string]$Entry.reason
    }
}

function Format-BringupOrderDisplayRow {
    param(
        $Entry
    )

    return [pscustomobject]@{
        Order = [int]$Entry.order
        Node = [string]$Entry.node
        Kind = [string]$Entry.kind
        Phase = [string]$Entry.phase
        State = [string]$Entry.state
        Needs = Format-StringArrayOrDash -Values @($Entry.requires)
        Missing = Format-StringArrayOrDash -Values @($Entry.missing_requires)
        DependsOn = Format-StringArrayOrDash -Values @($Entry.dependency_nodes)
        Provides = Format-StringArrayOrDash -Values @($Entry.provides)
    }
}

function Format-SystemFormationBlockerDisplayRow {
    param(
        $Entry
    )

    return [pscustomobject]@{
        Kind = [string]$Entry.kind
        Name = [string]$Entry.name
        State = [string]$Entry.state
        Missing = Format-StringArrayOrDash -Values @($Entry.missing_requires)
        DependsOn = Format-StringArrayOrDash -Values @($Entry.dependency_nodes)
        Reason = [string]$Entry.reason
    }
}

function Get-CapabilityNames {
    param(
        $Capabilities
    )

    $names = @()
    foreach ($capability in @($Capabilities)) {
        if ($null -eq $capability) {
            continue
        }

        $name = [string]$capability.name
        if ([string]::IsNullOrWhiteSpace($name)) {
            $name = [string]$capability.id
        }

        if (-not [string]::IsNullOrWhiteSpace($name)) {
            $names += $name
        }
    }

    return @($names | Sort-Object -Unique)
}

function Get-CapabilityNamesFromGraph {
    param(
        $GraphInfo
    )

    $names = @()
    if ($null -eq $GraphInfo) {
        return @()
    }

    foreach ($node in @($GraphInfo.Data.nodes)) {
        $names += @(Get-CapabilityNames -Capabilities $node.provides)
        $names += @(Get-CapabilityNames -Capabilities $node.requires)
    }

    foreach ($edge in @($GraphInfo.Data.edges)) {
        if ($null -eq $edge -or $null -eq $edge.capability) {
            continue
        }

        $edgeCapabilityName = [string]$edge.capability.name
        if ([string]::IsNullOrWhiteSpace($edgeCapabilityName)) {
            $edgeCapabilityName = [string]$edge.capability.id
        }

        if (-not [string]::IsNullOrWhiteSpace($edgeCapabilityName)) {
            $names += $edgeCapabilityName
        }
    }

    return @($names | Sort-Object -Unique)
}

function Get-BringupEvidenceEntriesFromReport {
    param(
        $ReportData
    )

    if ($null -eq $ReportData -or
        $null -eq $ReportData.PSObject.Properties['bringup_evidence'] -or
        $null -eq $ReportData.bringup_evidence -or
        $null -eq $ReportData.bringup_evidence.PSObject.Properties['evidence_entries']) {
        return @()
    }

    return @(
        @($ReportData.bringup_evidence.evidence_entries) |
            Where-Object {
                $null -ne $_ -and
                -not [string]::IsNullOrWhiteSpace([string]$_.capability)
            } |
            Sort-Object capability
    )
}

function Find-BringupEvidenceEntry {
    param(
        $ReportData,
        [string]$CapabilityName
    )

    if ([string]::IsNullOrWhiteSpace($CapabilityName)) {
        return $null
    }

    return @(
        Get-BringupEvidenceEntriesFromReport -ReportData $ReportData |
            Where-Object { [string]$_.capability -eq $CapabilityName } |
            Select-Object -First 1
    ) | Select-Object -First 1
}

function Get-CapabilityScopedReasons {
    param(
        [string[]]$Reasons,
        [string]$CapabilityName
    )

    if ([string]::IsNullOrWhiteSpace($CapabilityName)) {
        return @()
    }

    $exactUnresolvedText = "unresolved binding: $CapabilityName"
    return @(
        @($Reasons) |
            Where-Object {
                $reasonText = [string]$_
                (-not [string]::IsNullOrWhiteSpace($reasonText)) -and (
                    ($reasonText -eq $exactUnresolvedText) -or
                    ($reasonText -like "*$CapabilityName*")
                )
            } |
            Sort-Object -Unique
    )
}

function Get-ReportCapabilityNames {
    param(
        $ReportData,
        $GraphInfo
    )

    $names = @()
    $names += @(Get-CapabilityNamesFromGraph -GraphInfo $GraphInfo)

    if ($null -ne $ReportData) {
        $names += @($ReportData.structure.declared_facts)
        $names += @($ReportData.structure.required_facts)
        $names += @($ReportData.structure.unresolved_bindings)
        $names += @($ReportData.bringup_evidence.published_capabilities)
        $names += @(Get-BringupEvidenceEntriesFromReport -ReportData $ReportData | ForEach-Object { [string]$_.capability })
        $names += @($ReportData.resource_contract.provided_facts)
        $names += @(Get-RuntimeObservedCapabilitiesFromReport -ReportData $ReportData)

        foreach ($entry in @($ReportData.resource_contract.declared_contract_entries)) {
            $names += @($entry.requires)
        }

        foreach ($transition in @($ReportData.runtime_observe.recent_transitions)) {
            $capabilityName = [string]$transition.capability
            if (-not [string]::IsNullOrWhiteSpace($capabilityName)) {
                $names += $capabilityName
            }
        }
    }

    return @(
        @($names) |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            Sort-Object -Unique
    )
}

function Get-RuntimeObservedCapabilitiesFromReport {
    param(
        $ReportData
    )

    if ($null -eq $ReportData -or
        $null -eq $ReportData.PSObject.Properties['runtime_observe'] -or
        $null -eq $ReportData.runtime_observe) {
        return @()
    }

    if ($null -ne $ReportData.runtime_observe.PSObject.Properties['observed_capabilities']) {
        return @(
            @($ReportData.runtime_observe.observed_capabilities) |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                ForEach-Object { [string]$_ } |
                Sort-Object -Unique
        )
    }

    return @(
        @($ReportData.runtime_observe.recent_transitions) |
            ForEach-Object { [string]$_.capability } |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            Sort-Object -Unique
    )
}

function Get-ObservedCapabilitiesForEvidence {
    param(
        $ReportData,
        $GraphInfo
    )

    $names = @()
    $observedEntries = @(
        Get-BringupEvidenceEntriesFromReport -ReportData $ReportData |
            Where-Object { [bool]$_.observed } |
            ForEach-Object { [string]$_.capability }
    )
    if (@($observedEntries).Count -gt 0) {
        $names += @($observedEntries)
    }

    $names += @(Get-CapabilityNamesFromGraph -GraphInfo $GraphInfo)
    $names += @(Get-RuntimeObservedCapabilitiesFromReport -ReportData $ReportData)

    if ($null -ne $ReportData -and
        $null -ne $ReportData.PSObject.Properties['bringup_evidence'] -and
        $null -ne $ReportData.bringup_evidence) {
        $names += @($ReportData.bringup_evidence.published_capabilities)
    }

    return @(
        @($names) |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            Sort-Object -Unique
    )
}

function Get-CapabilityExportStateFromReport {
    param(
        $ReportData,
        [string]$CapabilityName
    )

    if ($null -eq $ReportData -or
        [string]::IsNullOrWhiteSpace($CapabilityName) -or
        $null -eq $ReportData.PSObject.Properties['runtime_observe'] -or
        $null -eq $ReportData.runtime_observe) {
        return $null
    }

    $state = $null
    foreach ($transition in @($ReportData.runtime_observe.recent_transitions)) {
        if ($null -eq $transition) {
            continue
        }

        if ([string]$transition.capability -ne $CapabilityName) {
            continue
        }

        $afterState = [string]$transition.after
        if ([string]::IsNullOrWhiteSpace($afterState)) {
            continue
        }

        $state = $afterState
    }

    return $state
}

function Get-ComparisonStatus {
    param(
        $ReportData
    )

    if ($null -eq $ReportData -or $null -eq $ReportData.PSObject.Properties['comparison'] -or $null -eq $ReportData.comparison) {
        return $null
    }

    return [string]$ReportData.comparison.status
}

function Get-MetadataChangeCount {
    param(
        $ReportData
    )

    if ($null -eq $ReportData -or $null -eq $ReportData.PSObject.Properties['comparison'] -or $null -eq $ReportData.comparison) {
        return 0
    }

    if ($null -eq $ReportData.comparison.PSObject.Properties['metadata_changes']) {
        return 0
    }

    return @($ReportData.comparison.metadata_changes).Count
}

function Get-SystemInputComparisonFromReport {
    param(
        $ReportData
    )

    if ($null -eq $ReportData -or
        $null -eq $ReportData.PSObject.Properties['comparison'] -or
        $null -eq $ReportData.comparison -or
        $null -eq $ReportData.comparison.PSObject.Properties['system_input']) {
        return $null
    }

    return $ReportData.comparison.system_input
}

function Get-SystemInputComparisonChangeCount {
    param(
        $SystemInputComparison
    )

    if ($null -eq $SystemInputComparison) {
        return 0
    }

    return [int]@($SystemInputComparison.summary_changes).Count
}

function Get-SystemFormationComparisonFromReport {
    param(
        $ReportData
    )

    if ($null -eq $ReportData -or
        $null -eq $ReportData.PSObject.Properties['comparison'] -or
        $null -eq $ReportData.comparison -or
        $null -eq $ReportData.comparison.PSObject.Properties['system_formation']) {
        return $null
    }

    return $ReportData.comparison.system_formation
}

function Get-SystemFormationComparisonChangeCount {
    param(
        $SystemFormationComparison
    )

    if ($null -eq $SystemFormationComparison) {
        return 0
    }

    return (
        [int]@($SystemFormationComparison.summary_changes).Count +
        [int]@($SystemFormationComparison.blocker_changes).Count +
        [int]@($SystemFormationComparison.unresolved_capability_changes.added).Count +
        [int]@($SystemFormationComparison.unresolved_capability_changes.removed).Count +
        [int]@($SystemFormationComparison.blocked_node_changes.added).Count +
        [int]@($SystemFormationComparison.blocked_node_changes.removed).Count
    )
}

function Get-BringupEvidenceComparisonFromReport {
    param(
        $ReportData
    )

    if ($null -eq $ReportData -or
        $null -eq $ReportData.PSObject.Properties['comparison'] -or
        $null -eq $ReportData.comparison -or
        $null -eq $ReportData.comparison.PSObject.Properties['bringup_evidence']) {
        return $null
    }

    return $ReportData.comparison.bringup_evidence
}

function Get-BindingResultComparisonFromReport {
    param(
        $ReportData
    )

    if ($null -eq $ReportData -or
        $null -eq $ReportData.PSObject.Properties['comparison'] -or
        $null -eq $ReportData.comparison -or
        $null -eq $ReportData.comparison.PSObject.Properties['binding_result']) {
        return $null
    }

    return $ReportData.comparison.binding_result
}

function Get-BringupOrderComparisonFromReport {
    param(
        $ReportData
    )

    if ($null -eq $ReportData -or
        $null -eq $ReportData.PSObject.Properties['comparison'] -or
        $null -eq $ReportData.comparison -or
        $null -eq $ReportData.comparison.PSObject.Properties['bringup_order']) {
        return $null
    }

    return $ReportData.comparison.bringup_order
}

function Get-ResourceContractComparisonFromReport {
    param(
        $ReportData
    )

    if ($null -eq $ReportData -or
        $null -eq $ReportData.PSObject.Properties['comparison'] -or
        $null -eq $ReportData.comparison -or
        $null -eq $ReportData.comparison.PSObject.Properties['resource_contract']) {
        return $null
    }

    return $ReportData.comparison.resource_contract
}

function Get-FactResolutionComparisonFromReport {
    param(
        $ReportData
    )

    if ($null -eq $ReportData -or
        $null -eq $ReportData.PSObject.Properties['comparison'] -or
        $null -eq $ReportData.comparison -or
        $null -eq $ReportData.comparison.PSObject.Properties['fact_resolution']) {
        return $null
    }

    return $ReportData.comparison.fact_resolution
}

function Get-BringupComparisonCapabilityChange {
    param(
        $ReportData,
        [string]$CapabilityName
    )

    if ([string]::IsNullOrWhiteSpace($CapabilityName)) {
        return $null
    }

    $comparison = Get-BringupEvidenceComparisonFromReport -ReportData $ReportData
    if ($null -eq $comparison) {
        return $null
    }

    return @(
        @($comparison.capability_changes) |
            Where-Object { [string]$_.capability -eq $CapabilityName } |
            Select-Object -First 1
    ) | Select-Object -First 1
}

function Get-ResourceComparisonContractChangesForCapability {
    param(
        $ReportData,
        [string]$CapabilityName
    )

    if ([string]::IsNullOrWhiteSpace($CapabilityName)) {
        return @()
    }

    $comparison = Get-ResourceContractComparisonFromReport -ReportData $ReportData
    if ($null -eq $comparison) {
        return @()
    }

    return @(
        @($comparison.contract_changes) |
            Where-Object {
                @($_.left_requires) -contains $CapabilityName -or
                @($_.right_requires) -contains $CapabilityName
            } |
            Sort-Object contract
    )
}

function New-CapabilityComparisonResult {
    param(
        $ReportData,
        [string]$CapabilityName
    )

    $bringupChange = Get-BringupComparisonCapabilityChange -ReportData $ReportData -CapabilityName $CapabilityName
    $resourceComparison = Get-ResourceContractComparisonFromReport -ReportData $ReportData
    $resourceContractChanges = @(Get-ResourceComparisonContractChangesForCapability -ReportData $ReportData -CapabilityName $CapabilityName)

    $resourceChangeKinds = @()
    $resourceContracts = @()
    $resourceFactAdded = $false
    $resourceFactRemoved = $false

    if ($null -ne $resourceComparison) {
        $resourceFactAdded = @($resourceComparison.provided_fact_changes.added) -contains $CapabilityName
        $resourceFactRemoved = @($resourceComparison.provided_fact_changes.removed) -contains $CapabilityName
        if ($resourceFactAdded) {
            $resourceChangeKinds += 'fact_added'
        }
        if ($resourceFactRemoved) {
            $resourceChangeKinds += 'fact_removed'
        }
    }

    foreach ($contractChange in @($resourceContractChanges)) {
        $changeKind = [string]$contractChange.change_kind
        if (-not [string]::IsNullOrWhiteSpace($changeKind)) {
            $resourceChangeKinds += "contract_$changeKind"
        }

        $contractName = [string]$contractChange.contract
        if (-not [string]::IsNullOrWhiteSpace($contractName)) {
            $resourceContracts += $contractName
        }
    }

    $bringupChangeKinds = @()
    if ($null -ne $bringupChange -and -not [string]::IsNullOrWhiteSpace([string]$bringupChange.change_kind)) {
        $bringupChangeKinds += [string]$bringupChange.change_kind
    }

    return [ordered]@{
        bringup_changed = ($null -ne $bringupChange)
        bringup_change_kinds = @($bringupChangeKinds | Sort-Object -Unique)
        resource_changed = ($resourceFactAdded -or $resourceFactRemoved -or @($resourceContractChanges).Count -gt 0)
        resource_change_kinds = @($resourceChangeKinds | Sort-Object -Unique)
        resource_contracts = @($resourceContracts | Sort-Object -Unique)
    }
}

function New-WhyCapabilityComparisonResult {
    param(
        $ReportData,
        [string]$CapabilityName
    )

    $summary = New-CapabilityComparisonResult -ReportData $ReportData -CapabilityName $CapabilityName
    $bringupChange = Get-BringupComparisonCapabilityChange -ReportData $ReportData -CapabilityName $CapabilityName
    $resourceComparison = Get-ResourceContractComparisonFromReport -ReportData $ReportData
    $resourceContractChanges = @(Get-ResourceComparisonContractChangesForCapability -ReportData $ReportData -CapabilityName $CapabilityName)

    $resourceFactAdded = $false
    $resourceFactRemoved = $false
    if ($null -ne $resourceComparison) {
        $resourceFactAdded = @($resourceComparison.provided_fact_changes.added) -contains $CapabilityName
        $resourceFactRemoved = @($resourceComparison.provided_fact_changes.removed) -contains $CapabilityName
    }

    $bringupEvidence = $null
    if ($null -ne $bringupChange -and @($bringupChange.PSObject.Properties).Count -gt 0) {
        $bringupEvidence = $bringupChange
    }

    $changed = [bool]$summary.bringup_changed -or [bool]$summary.resource_changed
    if (-not $changed) {
        return $null
    }

    return [ordered]@{
        changed = $changed
        bringup_changed = [bool]$summary.bringup_changed
        bringup_change_kinds = @($summary.bringup_change_kinds)
        resource_changed = [bool]$summary.resource_changed
        resource_change_kinds = @($summary.resource_change_kinds)
        resource_contracts = @($summary.resource_contracts)
        bringup_evidence = $bringupEvidence
        resource_contract = [ordered]@{
            provided_fact_added = $resourceFactAdded
            provided_fact_removed = $resourceFactRemoved
            contract_changes = @($resourceContractChanges)
        }
    }
}

function Get-ResourceContractMentions {
    param(
        $ReportData,
        [string]$CapabilityName
    )

    $mentions = [ordered]@{
        provided_fact = $false
        satisfied = @()
        violations = @()
        unknown = @()
        hotspots = @()
    }

    if ($null -eq $ReportData -or [string]::IsNullOrWhiteSpace($CapabilityName)) {
        return $mentions
    }

    if ($null -ne $ReportData.resource_contract) {
        $mentions.provided_fact = @($ReportData.resource_contract.provided_facts) -contains $CapabilityName
        $mentions.satisfied = @(
            @($ReportData.resource_contract.satisfied_contracts) |
                Where-Object { [string]$_ -like "*$CapabilityName*" }
        )
        $mentions.violations = @(
            @($ReportData.resource_contract.violations) |
                Where-Object { [string]$_ -like "*$CapabilityName*" }
        )
        $mentions.unknown = @(
            @($ReportData.resource_contract.unknown_contracts) |
                Where-Object { [string]$_ -like "*$CapabilityName*" }
        )
        $mentions.hotspots = @(
            @($ReportData.resource_contract.resource_hotspots) |
                Where-Object { [string]$_ -like "*$CapabilityName*" }
        )
    }

    return $mentions
}

function Get-ReportSubjectFacts {
    param(
        $ReportData
    )

    if ($null -eq $ReportData -or $null -eq $ReportData.subject) {
        return @()
    }

    $facts = @()
    if (-not [string]::IsNullOrWhiteSpace([string]$ReportData.subject.profile)) {
        $facts += "profile.$([string]$ReportData.subject.profile)"
    }
    if (-not [string]::IsNullOrWhiteSpace([string]$ReportData.subject.board)) {
        $facts += "board.$([string]$ReportData.subject.board)"
    }
    foreach ($facetName in @($ReportData.subject.active_facets)) {
        if (-not [string]::IsNullOrWhiteSpace([string]$facetName)) {
            $facts += "facet.$([string]$facetName)"
        }
    }

    return @($facts | Sort-Object -Unique)
}

function Get-GraphProvidedFacts {
    param(
        $GraphInfo
    )

    if ($null -eq $GraphInfo) {
        return @()
    }

    $facts = @()
    foreach ($node in @($GraphInfo.Data.nodes)) {
        $facts += @(Get-CapabilityNames -Capabilities $node.provides)
    }

    return @($facts | Sort-Object -Unique)
}

function New-ResourceFactInventory {
    param(
        $ReportData,
        $GraphInfo
    )

    $declaredFacts = @(
        @($ReportData.structure.declared_facts) |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [string]$_ } |
            Sort-Object -Unique
    )
    $subjectFacts = @(Get-ReportSubjectFacts -ReportData $ReportData)
    $requiredFacts = @(
        @($ReportData.structure.required_facts) |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [string]$_ } |
            Sort-Object -Unique
    )
    $graphProvidedFacts = @(Get-GraphProvidedFacts -GraphInfo $GraphInfo)
    $auditProvidedFacts = @(
        @($ReportData.resource_contract.provided_facts) |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [string]$_ } |
            Sort-Object -Unique
    )
    $allAvailableFacts = @(
        @($declaredFacts) +
        @($subjectFacts) +
        @($graphProvidedFacts) +
        @($auditProvidedFacts) |
            Sort-Object -Unique
    )

    return [ordered]@{
        declared_facts = @($declaredFacts)
        subject_facts = @($subjectFacts)
        required_facts = @($requiredFacts)
        graph_provided_facts = @($graphProvidedFacts)
        audit_provided_facts = @($auditProvidedFacts)
        all_available_facts = @($allAvailableFacts)
    }
}

function Get-ResourceFactSourceMap {
    param(
        $FactInventory
    )

    $factSourceMap = @{}
    foreach ($sourceName in @('declared_facts', 'subject_facts', 'graph_provided_facts', 'audit_provided_facts')) {
        foreach ($factName in @($FactInventory.$sourceName)) {
            $factKey = [string]$factName
            if ([string]::IsNullOrWhiteSpace($factKey)) {
                continue
            }

            if (-not $factSourceMap.ContainsKey($factKey)) {
                $factSourceMap[$factKey] = @()
            }

            $factSourceMap[$factKey] = @($factSourceMap[$factKey] + $sourceName)
        }
    }

    foreach ($factKey in @($factSourceMap.Keys)) {
        $factSourceMap[$factKey] = @($factSourceMap[$factKey] | Sort-Object -Unique)
    }

    return $factSourceMap
}

function Format-DeclaredContractText {
    param(
        [string]$ContractName,
        [string[]]$Requires
    )

    if ([string]::IsNullOrWhiteSpace($ContractName)) {
        return ''
    }

    return "$ContractName requires [$((@($Requires) -join ', '))]"
}

function New-ResourceContractEntrySummary {
    param(
        $ContractEntry,
        $FactSourceMap
    )

    if ($null -eq $ContractEntry) {
        return $null
    }

    $contractName = [string]$ContractEntry.contract
    if ([string]::IsNullOrWhiteSpace($contractName)) {
        return $null
    }

    $requires = @(
        @($ContractEntry.requires) |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [string]$_ } |
            Sort-Object -Unique
    )

    $presentFacts = @()
    $missingFacts = @()
    $factSources = [ordered]@{}
    foreach ($requiredFact in @($requires)) {
        if ($FactSourceMap.ContainsKey($requiredFact)) {
            $presentFacts += $requiredFact
            $factSources[$requiredFact] = @($FactSourceMap[$requiredFact])
        } else {
            $missingFacts += $requiredFact
        }
    }

    $state = 'unknown'
    $statusText = Format-DeclaredContractText -ContractName $contractName -Requires $requires
    if ($requires.Count -eq 0) {
        $state = 'unknown'
    } elseif ($missingFacts.Count -eq 0) {
        $state = 'satisfied'
    } else {
        $state = 'violated'
        $statusText = "$contractName missing [$((@($missingFacts) -join ', '))] requires [$((@($requires) -join ', '))]"
    }

    return [pscustomobject][ordered]@{
        contract = $contractName
        requires = @($requires)
        state = $state
        present_facts = @($presentFacts)
        missing_facts = @($missingFacts)
        fact_sources = $factSources
        status_text = $statusText
    }
}

function New-ResourceSummaryResult {
    param(
        $ReportData,
        $GraphInfo
    )

    if ($null -ne $ReportData -and
        $null -ne $ReportData.PSObject.Properties['fact_resolution'] -and
        $null -ne $ReportData.fact_resolution) {
        return $ReportData.fact_resolution
    }

    $factInventory = New-ResourceFactInventory -ReportData $ReportData -GraphInfo $GraphInfo
    $factSourceMap = Get-ResourceFactSourceMap -FactInventory $factInventory
    $contracts = @()
    foreach ($contractEntry in @($ReportData.resource_contract.declared_contract_entries)) {
        $entrySummary = New-ResourceContractEntrySummary -ContractEntry $contractEntry -FactSourceMap $factSourceMap
        if ($null -ne $entrySummary) {
            $contracts += $entrySummary
        }
    }

    return [ordered]@{
        declared_contracts = [int]$ReportData.resource_contract.declared_contracts
        audited_count = [int]$ReportData.resource_contract.audited_count
        satisfied_count = [int]$ReportData.resource_contract.satisfied_count
        violated_count = [int]$ReportData.resource_contract.violated_count
        unknown_count = [int]$ReportData.resource_contract.unknown_count
        fact_inventory = $factInventory
        contracts = @($contracts | Sort-Object contract)
        satisfied_contracts = @($ReportData.resource_contract.satisfied_contracts)
        violations = @($ReportData.resource_contract.violations)
        unknown_contracts = @($ReportData.resource_contract.unknown_contracts)
        resource_hotspots = @($ReportData.resource_contract.resource_hotspots)
    }
}

function New-ArtifactRootFactResolutionCaseSummary {
    param(
        $LoadedReport
    )

    $report = $LoadedReport.Data
    $graphInfo = Load-GraphFromArtifactReport -ReportData $report
    $resourceSummary = New-ResourceSummaryResult -ReportData $report -GraphInfo $graphInfo

    return [pscustomobject][ordered]@{
        report_path = $LoadedReport.Path
        case = [string]$report.subject.case
        profile = [string]$report.subject.profile
        board = [string]$report.subject.board
        active_facets = @($report.subject.active_facets)
        declared_contracts = [int]$resourceSummary.declared_contracts
        audited_count = [int]$resourceSummary.audited_count
        satisfied_count = [int]$resourceSummary.satisfied_count
        violated_count = [int]$resourceSummary.violated_count
        unknown_count = [int]$resourceSummary.unknown_count
        declared_facts = @($resourceSummary.fact_inventory.declared_facts)
        subject_facts = @($resourceSummary.fact_inventory.subject_facts)
        required_facts = @($resourceSummary.fact_inventory.required_facts)
        graph_provided_facts = @($resourceSummary.fact_inventory.graph_provided_facts)
        audit_provided_facts = @($resourceSummary.fact_inventory.audit_provided_facts)
        all_available_facts = @($resourceSummary.fact_inventory.all_available_facts)
        resource_hotspots = @($resourceSummary.resource_hotspots)
        contracts = @($resourceSummary.contracts)
    }
}

function New-ArtifactRootResourceCaseSummary {
    param(
        $LoadedReport
    )

    return New-ArtifactRootFactResolutionCaseSummary -LoadedReport $LoadedReport
}

function New-ArtifactRootResourceContractMatrixEntry {
    param(
        [string]$ContractName,
        [object[]]$CaseSummaries
    )

    if ([string]::IsNullOrWhiteSpace($ContractName)) {
        return $null
    }

    $contractCases = @()
    $requires = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        $matchingEntries = @(
            @($caseSummary.contracts) |
                Where-Object { [string]$_.contract -eq $ContractName }
        )
        if ($matchingEntries.Count -eq 0) {
            continue
        }

        $entry = $matchingEntries[0]
        $requires += @($entry.requires)
        $contractCases += [pscustomobject][ordered]@{
            case = [string]$caseSummary.case
            profile = [string]$caseSummary.profile
            board = [string]$caseSummary.board
            state = [string]$entry.state
            present_facts = @($entry.present_facts)
            missing_facts = @($entry.missing_facts)
            status_text = [string]$entry.status_text
        }
    }

    return [pscustomobject][ordered]@{
        contract = $ContractName
        requires = @($requires | Sort-Object -Unique)
        cases_declared = @($contractCases).Count
        cases_satisfied = @($contractCases | Where-Object { [string]$_.state -eq 'satisfied' }).Count
        cases_violated = @($contractCases | Where-Object { [string]$_.state -eq 'violated' }).Count
        cases_unknown = @($contractCases | Where-Object { [string]$_.state -eq 'unknown' }).Count
        cases = @($contractCases | Sort-Object case)
    }
}

function New-ArtifactRootFactInventoryMatrixEntry {
    param(
        [string]$FactName,
        [string]$FactGroup,
        [object[]]$CaseSummaries
    )

    if ([string]::IsNullOrWhiteSpace($FactName) -or [string]::IsNullOrWhiteSpace($FactGroup)) {
        return $null
    }

    $factCases = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        if (-not (@($caseSummary.$FactGroup) -contains $FactName)) {
            continue
        }

        $factCases += [pscustomobject][ordered]@{
            case = [string]$caseSummary.case
            profile = [string]$caseSummary.profile
            board = [string]$caseSummary.board
        }
    }

    return [pscustomobject][ordered]@{
        fact = $FactName
        case_count = @($factCases).Count
        cases = @($factCases | Sort-Object case)
    }
}

function New-ArtifactRootFactInventoryMatrix {
    param(
        [object[]]$CaseSummaries
    )

    $matrix = [ordered]@{}
    foreach ($factGroup in @('declared_facts', 'subject_facts', 'required_facts', 'graph_provided_facts', 'audit_provided_facts', 'all_available_facts')) {
        $factNames = @(
            foreach ($caseSummary in @($CaseSummaries)) {
                @($caseSummary.$factGroup)
            }
        ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique

        $matrix[$factGroup] = @(
            foreach ($factName in @($factNames)) {
                New-ArtifactRootFactInventoryMatrixEntry -FactName $factName -FactGroup $factGroup -CaseSummaries $CaseSummaries
            }
        ) | Where-Object { $null -ne $_ } | Sort-Object fact
    }

    return $matrix
}

function New-ArtifactRootResourceProvidedFactEntry {
    param(
        [string]$FactName,
        [object[]]$CaseSummaries
    )

    return New-ArtifactRootFactInventoryMatrixEntry -FactName $FactName -FactGroup 'audit_provided_facts' -CaseSummaries $CaseSummaries
}

function New-ArtifactRootResourceHotspotEntry {
    param(
        [string]$HotspotText,
        [object[]]$CaseSummaries
    )

    if ([string]::IsNullOrWhiteSpace($HotspotText)) {
        return $null
    }

    $hotspotCases = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        if (-not (@($caseSummary.resource_hotspots) -contains $HotspotText)) {
            continue
        }

        $hotspotCases += [pscustomobject][ordered]@{
            case = [string]$caseSummary.case
            profile = [string]$caseSummary.profile
            board = [string]$caseSummary.board
        }
    }

    return [pscustomobject][ordered]@{
        hotspot = $HotspotText
        case_count = @($hotspotCases).Count
        cases = @($hotspotCases | Sort-Object case)
    }
}

function New-ArtifactRootFactResolutionSummaryResult {
    param(
        [object[]]$LoadedReports
    )

    $caseSummaries = @(
        @($LoadedReports) |
            ForEach-Object { New-ArtifactRootFactResolutionCaseSummary -LoadedReport $_ } |
            Sort-Object case
    )

    $contractNames = @(
        foreach ($caseSummary in @($caseSummaries)) {
            foreach ($contractSummary in @($caseSummary.contracts)) {
                $contractName = [string]$contractSummary.contract
                if (-not [string]::IsNullOrWhiteSpace($contractName)) {
                    $contractName
                }
            }
        }
    ) | Sort-Object -Unique

    $resourceHotspots = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.resource_hotspots)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique

    $factInventoryMatrix = New-ArtifactRootFactInventoryMatrix -CaseSummaries $caseSummaries

    $contractMatrix = @(
        foreach ($contractName in @($contractNames)) {
            New-ArtifactRootResourceContractMatrixEntry -ContractName $contractName -CaseSummaries $caseSummaries
        }
    )

    $resourceHotspotMatrix = @(
        foreach ($hotspotText in @($resourceHotspots)) {
            New-ArtifactRootResourceHotspotEntry -HotspotText $hotspotText -CaseSummaries $caseSummaries
        }
    )

    return [ordered]@{
        case_count = @($caseSummaries).Count
        totals = [ordered]@{
            declared_contracts = (@($caseSummaries | Measure-Object -Property declared_contracts -Sum).Sum)
            audited_count = (@($caseSummaries | Measure-Object -Property audited_count -Sum).Sum)
            satisfied_count = (@($caseSummaries | Measure-Object -Property satisfied_count -Sum).Sum)
            violated_count = (@($caseSummaries | Measure-Object -Property violated_count -Sum).Sum)
            unknown_count = (@($caseSummaries | Measure-Object -Property unknown_count -Sum).Sum)
        }
        cases = @($caseSummaries)
        fact_inventory_matrix = $factInventoryMatrix
        contract_matrix = @($contractMatrix | Sort-Object contract)
        required_fact_matrix = @($factInventoryMatrix.required_facts)
        provided_fact_matrix = @($factInventoryMatrix.audit_provided_facts)
        resource_hotspot_matrix = @($resourceHotspotMatrix | Sort-Object hotspot)
    }
}

function New-ArtifactRootResourceSummaryResult {
    param(
        [object[]]$LoadedReports
    )

    return New-ArtifactRootFactResolutionSummaryResult -LoadedReports $LoadedReports
}

function Get-BindingResultFromReport {
    param(
        $ReportData
    )

    if ($null -eq $ReportData -or
        $null -eq $ReportData.PSObject.Properties['binding_result'] -or
        $null -eq $ReportData.binding_result) {
        return [ordered]@{
            required_binding_count = 0
            resolved_binding_count = 0
            unresolved_binding_count = 0
            resolved_capabilities = @()
            unresolved_capabilities = @()
            binding_entries = @()
        }
    }

    return $ReportData.binding_result
}

function Get-BringupOrderFromReport {
    param(
        $ReportData
    )

    if ($null -eq $ReportData -or
        $null -eq $ReportData.PSObject.Properties['bringup_order'] -or
        $null -eq $ReportData.bringup_order) {
        return [ordered]@{
            ordered_node_count = 0
            blocked_node_count = 0
            phase_counts = [ordered]@{}
            entries = @()
        }
    }

    return $ReportData.bringup_order
}

function Add-AggregatedCountMapEntry {
    param(
        [hashtable]$Counts,
        [string]$Name,
        [int]$Amount = 1
    )

    if ([string]::IsNullOrWhiteSpace($Name)) {
        return
    }

    if ($Counts.ContainsKey($Name)) {
        $Counts[$Name] = [int]$Counts[$Name] + [int]$Amount
    } else {
        $Counts[$Name] = [int]$Amount
    }
}

function ConvertTo-AggregatedOrderedCountMap {
    param(
        [hashtable]$Counts
    )

    $result = [ordered]@{}
    if ($null -eq $Counts) {
        return $result
    }

    foreach ($entry in @($Counts.GetEnumerator() | Sort-Object Key)) {
        $result[[string]$entry.Key] = [int]$entry.Value
    }

    return $result
}

function Get-ArtifactRootMatrixName {
    param(
        [AllowNull()]
        [string]$Value,
        [string]$Fallback = '[unspecified]'
    )

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return $Fallback
    }

    return [string]$Value
}

function Get-ArtifactRootMatrixNames {
    param(
        [object[]]$Values,
        [string]$Fallback = '[none]'
    )

    $names = @(
        @($Values) |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [string]$_ } |
            Sort-Object -Unique
    )

    if (@($names).Count -eq 0) {
        return @($Fallback)
    }

    return @($names)
}

function New-ArtifactRootSystemInputMatrixCaseEntry {
    param(
        $CaseSummary
    )

    return [pscustomobject][ordered]@{
        case = [string]$CaseSummary.case
        profile = [string]$CaseSummary.profile
        board = [string]$CaseSummary.board
        active_facets = @($CaseSummary.active_facets)
    }
}

function New-ArtifactRootSystemInputValueMatrixEntry {
    param(
        [string]$ValueName,
        [object[]]$CaseSummaries,
        [string]$PropertyName,
        [string]$FieldName,
        [string]$Fallback = '[unspecified]'
    )

    if ([string]::IsNullOrWhiteSpace($ValueName)) {
        return $null
    }

    $cases = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        $currentValue = Get-ArtifactRootMatrixName -Value ([string]$caseSummary.$PropertyName) -Fallback $Fallback
        if ($currentValue -ne $ValueName) {
            continue
        }

        $cases += New-ArtifactRootSystemInputMatrixCaseEntry -CaseSummary $caseSummary
    }

    return [ordered]@{
        $FieldName = $ValueName
        case_count = @($cases).Count
        cases = @($cases | Sort-Object case)
    }
}

function New-ArtifactRootSystemInputArrayMatrixEntry {
    param(
        [string]$Name,
        [object[]]$CaseSummaries,
        [string]$PropertyName,
        [string]$FieldName,
        [string]$Fallback = '[none]'
    )

    if ([string]::IsNullOrWhiteSpace($Name)) {
        return $null
    }

    $cases = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        $currentNames = @(Get-ArtifactRootMatrixNames -Values @($caseSummary.$PropertyName) -Fallback $Fallback)
        if (@($currentNames) -notcontains $Name) {
            continue
        }

        $cases += New-ArtifactRootSystemInputMatrixCaseEntry -CaseSummary $caseSummary
    }

    return [ordered]@{
        $FieldName = $Name
        case_count = @($cases).Count
        cases = @($cases | Sort-Object case)
    }
}

function New-ArtifactRootSystemInputContractMatrixEntry {
    param(
        [string]$ContractName,
        [object[]]$CaseSummaries
    )

    if ([string]::IsNullOrWhiteSpace($ContractName)) {
        return $null
    }

    $contractCases = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        $contractEntry = @(
            @($caseSummary.declared_contract_entries) |
                Where-Object { [string]$_.contract -eq $ContractName } |
                Select-Object -First 1
        ) | Select-Object -First 1

        if ($null -eq $contractEntry) {
            continue
        }

        $contractCases += [pscustomobject][ordered]@{
            case = [string]$caseSummary.case
            profile = [string]$caseSummary.profile
            board = [string]$caseSummary.board
            active_facets = @($caseSummary.active_facets)
            requires = @(
                @($contractEntry.requires) |
                    Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                    ForEach-Object { [string]$_ } |
                    Sort-Object -Unique
            )
        }
    }

    return [ordered]@{
        contract = $ContractName
        case_count = @($contractCases).Count
        requires = @(
            @($contractCases) |
                ForEach-Object { @($_.requires) } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        cases = @($contractCases | Sort-Object case)
    }
}

function New-ArtifactRootSystemInputCaseSummary {
    param(
        $LoadedReport
    )

    if ($null -eq $LoadedReport -or $null -eq $LoadedReport.Data) {
        return $null
    }

    $report = $LoadedReport.Data
    if ($null -eq $report.PSObject.Properties['system_input'] -or $null -eq $report.system_input) {
        return $null
    }

    $systemInput = $report.system_input
    $systemSpec = if ($null -ne $systemInput.PSObject.Properties['system_spec']) { $systemInput.system_spec } else { $null }
    $declaredInput = if ($null -ne $systemInput.PSObject.Properties['declared_input']) { $systemInput.declared_input } else { $null }
    $declaredSubject = if ($null -ne $declaredInput -and $null -ne $declaredInput.PSObject.Properties['subject']) { $declaredInput.subject } else { $null }
    $resolvedInput = if ($null -ne $systemInput.PSObject.Properties['resolved_input']) { $systemInput.resolved_input } else { $null }
    $resolvedProfile = if ($null -ne $resolvedInput -and $null -ne $resolvedInput.PSObject.Properties['profile']) { $resolvedInput.profile } else { $null }
    $resolvedBoard = if ($null -ne $resolvedInput -and $null -ne $resolvedInput.PSObject.Properties['board']) { $resolvedInput.board } else { $null }
    $resolvedFacets = if ($null -ne $resolvedInput -and $null -ne $resolvedInput.PSObject.Properties['active_facets']) { $resolvedInput.active_facets } else { $null }

    return [pscustomobject][ordered]@{
        case = [string]$report.subject.case
        profile = [string]$report.subject.profile
        board = [string]$report.subject.board
        active_facets = @($report.subject.active_facets)
        case_kind = if ($null -eq $systemSpec -or [string]::IsNullOrWhiteSpace([string]$systemSpec.case_kind)) { $null } else { [string]$systemSpec.case_kind }
        source = if ($null -eq $systemSpec -or [string]::IsNullOrWhiteSpace([string]$systemSpec.source)) { $null } else { [string]$systemSpec.source }
        build_target = if ($null -eq $systemSpec -or [string]::IsNullOrWhiteSpace([string]$systemSpec.build_target)) { $null } else { [string]$systemSpec.build_target }
        export_target = if ($null -eq $systemSpec -or [string]::IsNullOrWhiteSpace([string]$systemSpec.export_target)) { $null } else { [string]$systemSpec.export_target }
        declared_profile = if ($null -eq $declaredSubject -or [string]::IsNullOrWhiteSpace([string]$declaredSubject.profile)) { $null } else { [string]$declaredSubject.profile }
        declared_board = if ($null -eq $declaredSubject -or [string]::IsNullOrWhiteSpace([string]$declaredSubject.board)) { $null } else { [string]$declaredSubject.board }
        declared_active_facets = if ($null -eq $declaredSubject) { @() } else { @($declaredSubject.active_facets) }
        declared_facts = if ($null -eq $declaredInput) { @() } else { @($declaredInput.declared_facts) }
        declared_contract_entries = if ($null -eq $declaredInput) { @() } else { @($declaredInput.declared_contract_entries) }
        resolved_profile = if ($null -eq $resolvedProfile -or [string]::IsNullOrWhiteSpace([string]$resolvedProfile.value)) { $null } else { [string]$resolvedProfile.value }
        resolved_profile_source = if ($null -eq $resolvedProfile -or [string]::IsNullOrWhiteSpace([string]$resolvedProfile.source)) { $null } else { [string]$resolvedProfile.source }
        resolved_board = if ($null -eq $resolvedBoard -or [string]::IsNullOrWhiteSpace([string]$resolvedBoard.value)) { $null } else { [string]$resolvedBoard.value }
        resolved_board_source = if ($null -eq $resolvedBoard -or [string]::IsNullOrWhiteSpace([string]$resolvedBoard.source)) { $null } else { [string]$resolvedBoard.source }
        resolved_active_facets = if ($null -eq $resolvedFacets) { @() } else { @($resolvedFacets.values) }
        resolved_active_facets_source = if ($null -eq $resolvedFacets -or [string]::IsNullOrWhiteSpace([string]$resolvedFacets.source)) { $null } else { [string]$resolvedFacets.source }
        subject_facts = if ($null -eq $resolvedInput) { @() } else { @($resolvedInput.subject_facts) }
    }
}

function New-ArtifactRootSystemInputSummaryResult {
    param(
        [object[]]$LoadedReports
    )

    $caseSummaries = @(
        @($LoadedReports) |
            ForEach-Object { New-ArtifactRootSystemInputCaseSummary -LoadedReport $_ } |
            Where-Object { $null -ne $_ } |
            Sort-Object case
    )

    if (@($caseSummaries).Count -eq 0) {
        return $null
    }

    $caseKinds = @(
        foreach ($caseSummary in @($caseSummaries)) {
            Get-ArtifactRootMatrixName -Value ([string]$caseSummary.case_kind)
        }
    ) | Sort-Object -Unique
    $sources = @(
        foreach ($caseSummary in @($caseSummaries)) {
            Get-ArtifactRootMatrixName -Value ([string]$caseSummary.source)
        }
    ) | Sort-Object -Unique
    $buildTargets = @(
        foreach ($caseSummary in @($caseSummaries)) {
            Get-ArtifactRootMatrixName -Value ([string]$caseSummary.build_target)
        }
    ) | Sort-Object -Unique
    $exportTargets = @(
        foreach ($caseSummary in @($caseSummaries)) {
            Get-ArtifactRootMatrixName -Value ([string]$caseSummary.export_target)
        }
    ) | Sort-Object -Unique
    $declaredProfiles = @(
        foreach ($caseSummary in @($caseSummaries)) {
            Get-ArtifactRootMatrixName -Value ([string]$caseSummary.declared_profile)
        }
    ) | Sort-Object -Unique
    $declaredBoards = @(
        foreach ($caseSummary in @($caseSummaries)) {
            Get-ArtifactRootMatrixName -Value ([string]$caseSummary.declared_board)
        }
    ) | Sort-Object -Unique
    $resolvedProfiles = @(
        foreach ($caseSummary in @($caseSummaries)) {
            Get-ArtifactRootMatrixName -Value ([string]$caseSummary.resolved_profile)
        }
    ) | Sort-Object -Unique
    $resolvedProfileSources = @(
        foreach ($caseSummary in @($caseSummaries)) {
            Get-ArtifactRootMatrixName -Value ([string]$caseSummary.resolved_profile_source) -Fallback 'missing'
        }
    ) | Sort-Object -Unique
    $resolvedBoards = @(
        foreach ($caseSummary in @($caseSummaries)) {
            Get-ArtifactRootMatrixName -Value ([string]$caseSummary.resolved_board)
        }
    ) | Sort-Object -Unique
    $resolvedBoardSources = @(
        foreach ($caseSummary in @($caseSummaries)) {
            Get-ArtifactRootMatrixName -Value ([string]$caseSummary.resolved_board_source) -Fallback 'missing'
        }
    ) | Sort-Object -Unique
    $resolvedFacetSources = @(
        foreach ($caseSummary in @($caseSummaries)) {
            Get-ArtifactRootMatrixName -Value ([string]$caseSummary.resolved_active_facets_source) -Fallback 'missing'
        }
    ) | Sort-Object -Unique
    $declaredActiveFacets = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @(Get-ArtifactRootMatrixNames -Values @($caseSummary.declared_active_facets))
        }
    ) | Sort-Object -Unique
    $resolvedActiveFacets = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @(Get-ArtifactRootMatrixNames -Values @($caseSummary.resolved_active_facets))
        }
    ) | Sort-Object -Unique
    $declaredFacts = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @(Get-ArtifactRootMatrixNames -Values @($caseSummary.declared_facts))
        }
    ) | Sort-Object -Unique
    $subjectFacts = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @(Get-ArtifactRootMatrixNames -Values @($caseSummary.subject_facts))
        }
    ) | Sort-Object -Unique
    $declaredContracts = @(
        foreach ($caseSummary in @($caseSummaries)) {
            foreach ($contractEntry in @($caseSummary.declared_contract_entries)) {
                $contractName = [string]$contractEntry.contract
                if (-not [string]::IsNullOrWhiteSpace($contractName)) {
                    $contractName
                }
            }
        }
    ) | Sort-Object -Unique

    $caseKindMatrix = @(
        foreach ($caseKind in @($caseKinds)) {
            New-ArtifactRootSystemInputValueMatrixEntry -ValueName $caseKind -CaseSummaries $caseSummaries -PropertyName 'case_kind' -FieldName 'case_kind'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object case_kind
    $sourceMatrix = @(
        foreach ($sourceName in @($sources)) {
            New-ArtifactRootSystemInputValueMatrixEntry -ValueName $sourceName -CaseSummaries $caseSummaries -PropertyName 'source' -FieldName 'source'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object source
    $buildTargetMatrix = @(
        foreach ($buildTarget in @($buildTargets)) {
            New-ArtifactRootSystemInputValueMatrixEntry -ValueName $buildTarget -CaseSummaries $caseSummaries -PropertyName 'build_target' -FieldName 'build_target'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object build_target
    $exportTargetMatrix = @(
        foreach ($exportTarget in @($exportTargets)) {
            New-ArtifactRootSystemInputValueMatrixEntry -ValueName $exportTarget -CaseSummaries $caseSummaries -PropertyName 'export_target' -FieldName 'export_target'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object export_target
    $declaredProfileMatrix = @(
        foreach ($declaredProfile in @($declaredProfiles)) {
            New-ArtifactRootSystemInputValueMatrixEntry -ValueName $declaredProfile -CaseSummaries $caseSummaries -PropertyName 'declared_profile' -FieldName 'profile'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object profile
    $declaredBoardMatrix = @(
        foreach ($declaredBoard in @($declaredBoards)) {
            New-ArtifactRootSystemInputValueMatrixEntry -ValueName $declaredBoard -CaseSummaries $caseSummaries -PropertyName 'declared_board' -FieldName 'board'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object board
    $declaredActiveFacetMatrix = @(
        foreach ($facetName in @($declaredActiveFacets)) {
            New-ArtifactRootSystemInputArrayMatrixEntry -Name $facetName -CaseSummaries $caseSummaries -PropertyName 'declared_active_facets' -FieldName 'facet'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object facet
    $resolvedProfileMatrix = @(
        foreach ($resolvedProfile in @($resolvedProfiles)) {
            New-ArtifactRootSystemInputValueMatrixEntry -ValueName $resolvedProfile -CaseSummaries $caseSummaries -PropertyName 'resolved_profile' -FieldName 'profile'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object profile
    $resolvedProfileSourceMatrix = @(
        foreach ($sourceName in @($resolvedProfileSources)) {
            New-ArtifactRootSystemInputValueMatrixEntry -ValueName $sourceName -CaseSummaries $caseSummaries -PropertyName 'resolved_profile_source' -FieldName 'source' -Fallback 'missing'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object source
    $resolvedBoardMatrix = @(
        foreach ($resolvedBoard in @($resolvedBoards)) {
            New-ArtifactRootSystemInputValueMatrixEntry -ValueName $resolvedBoard -CaseSummaries $caseSummaries -PropertyName 'resolved_board' -FieldName 'board'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object board
    $resolvedBoardSourceMatrix = @(
        foreach ($sourceName in @($resolvedBoardSources)) {
            New-ArtifactRootSystemInputValueMatrixEntry -ValueName $sourceName -CaseSummaries $caseSummaries -PropertyName 'resolved_board_source' -FieldName 'source' -Fallback 'missing'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object source
    $resolvedActiveFacetMatrix = @(
        foreach ($facetName in @($resolvedActiveFacets)) {
            New-ArtifactRootSystemInputArrayMatrixEntry -Name $facetName -CaseSummaries $caseSummaries -PropertyName 'resolved_active_facets' -FieldName 'facet'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object facet
    $resolvedActiveFacetSourceMatrix = @(
        foreach ($sourceName in @($resolvedFacetSources)) {
            New-ArtifactRootSystemInputValueMatrixEntry -ValueName $sourceName -CaseSummaries $caseSummaries -PropertyName 'resolved_active_facets_source' -FieldName 'source' -Fallback 'missing'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object source
    $declaredFactMatrix = @(
        foreach ($factName in @($declaredFacts)) {
            New-ArtifactRootSystemInputArrayMatrixEntry -Name $factName -CaseSummaries $caseSummaries -PropertyName 'declared_facts' -FieldName 'fact'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object fact
    $subjectFactMatrix = @(
        foreach ($factName in @($subjectFacts)) {
            New-ArtifactRootSystemInputArrayMatrixEntry -Name $factName -CaseSummaries $caseSummaries -PropertyName 'subject_facts' -FieldName 'fact'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object fact
    $declaredContractMatrix = @(
        foreach ($contractName in @($declaredContracts)) {
            New-ArtifactRootSystemInputContractMatrixEntry -ContractName $contractName -CaseSummaries $caseSummaries
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object contract

    return [ordered]@{
        kind = 'system_input_summary/v0'
        mode = 'summary'
        case_count = @($caseSummaries).Count
        totals = [ordered]@{
            declared_fact_count = [int](@($caseSummaries | ForEach-Object { @($_.declared_facts).Count } | Measure-Object -Sum).Sum)
            declared_contract_count = [int](@($caseSummaries | ForEach-Object { @($_.declared_contract_entries).Count } | Measure-Object -Sum).Sum)
            subject_fact_count = [int](@($caseSummaries | ForEach-Object { @($_.subject_facts).Count } | Measure-Object -Sum).Sum)
        }
        cases = @(
            @($caseSummaries) |
                Select-Object `
                    case,
                    board,
                    profile,
                    active_facets,
                    case_kind,
                    source,
                    build_target,
                    export_target,
                    declared_profile,
                    declared_board,
                    declared_active_facets,
                    @{ Name = 'declared_fact_count'; Expression = { @($_.declared_facts).Count } },
                    @{ Name = 'declared_contract_count'; Expression = { @($_.declared_contract_entries).Count } },
                    resolved_profile,
                    resolved_profile_source,
                    resolved_board,
                    resolved_board_source,
                    resolved_active_facets,
                    resolved_active_facets_source,
                    @{ Name = 'subject_fact_count'; Expression = { @($_.subject_facts).Count } } |
                Sort-Object case
        )
        case_kind_matrix = @($caseKindMatrix)
        source_matrix = @($sourceMatrix)
        build_target_matrix = @($buildTargetMatrix)
        export_target_matrix = @($exportTargetMatrix)
        declared_profile_matrix = @($declaredProfileMatrix)
        declared_board_matrix = @($declaredBoardMatrix)
        declared_active_facet_matrix = @($declaredActiveFacetMatrix)
        resolved_profile_matrix = @($resolvedProfileMatrix)
        resolved_profile_source_matrix = @($resolvedProfileSourceMatrix)
        resolved_board_matrix = @($resolvedBoardMatrix)
        resolved_board_source_matrix = @($resolvedBoardSourceMatrix)
        resolved_active_facet_matrix = @($resolvedActiveFacetMatrix)
        resolved_active_facet_source_matrix = @($resolvedActiveFacetSourceMatrix)
        declared_fact_matrix = @($declaredFactMatrix)
        declared_contract_matrix = @($declaredContractMatrix)
        subject_fact_matrix = @($subjectFactMatrix)
    }
}

function New-ArtifactRootSystemInputCompareCaseSummary {
    param(
        $LoadedReport
    )

    if ($null -eq $LoadedReport -or $null -eq $LoadedReport.Data) {
        return $null
    }

    $report = $LoadedReport.Data
    $comparison = Get-SystemInputComparisonFromReport -ReportData $report
    if ($null -eq $comparison) {
        return $null
    }

    return [pscustomobject][ordered]@{
        case = [string]$report.subject.case
        profile = [string]$report.subject.profile
        board = [string]$report.subject.board
        active_facets = @($report.subject.active_facets)
        changed = [bool]$comparison.changed
        summary_changes = @($comparison.summary_changes)
        system_spec_changes = @($comparison.system_spec_changes)
        declared_subject_changes = @($comparison.declared_subject_changes)
        declared_fact_changes = [ordered]@{
            added = @($comparison.declared_fact_changes.added)
            removed = @($comparison.declared_fact_changes.removed)
        }
        declared_contract_changes = @($comparison.declared_contract_changes)
        resolved_input_changes = @($comparison.resolved_input_changes)
        subject_fact_changes = [ordered]@{
            added = @($comparison.subject_fact_changes.added)
            removed = @($comparison.subject_fact_changes.removed)
        }
    }
}

function New-ArtifactRootSystemInputCompareChangeEntry {
    param(
        [string]$ChangeText,
        [object[]]$CaseSummaries,
        [string]$CollectionName
    )

    if ([string]::IsNullOrWhiteSpace($ChangeText)) {
        return $null
    }

    $cases = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        if (-not (@($caseSummary.$CollectionName) -contains $ChangeText)) {
            continue
        }

        $cases += New-ArtifactRootSystemInputMatrixCaseEntry -CaseSummary $caseSummary
    }

    return [ordered]@{
        change = $ChangeText
        case_count = @($cases).Count
        cases = @($cases | Sort-Object case)
    }
}

function New-ArtifactRootSystemInputCompareContractEntry {
    param(
        [string]$ContractName,
        [object[]]$CaseSummaries
    )

    if ([string]::IsNullOrWhiteSpace($ContractName)) {
        return $null
    }

    $contractCases = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        $contractChange = @(
            @($caseSummary.declared_contract_changes) |
                Where-Object { [string]$_.contract -eq $ContractName } |
                Select-Object -First 1
        ) | Select-Object -First 1

        if ($null -eq $contractChange) {
            continue
        }

        $contractCases += [pscustomobject][ordered]@{
            case = [string]$caseSummary.case
            profile = [string]$caseSummary.profile
            board = [string]$caseSummary.board
            active_facets = @($caseSummary.active_facets)
            change_kind = [string]$contractChange.change_kind
            left_requires = @($contractChange.left_requires)
            right_requires = @($contractChange.right_requires)
        }
    }

    return [ordered]@{
        contract = $ContractName
        case_count = @($contractCases).Count
        change_kinds = @(
            @($contractCases) |
                ForEach-Object { [string]$_.change_kind } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        cases = @($contractCases | Sort-Object case)
    }
}

function New-ArtifactRootSystemInputComparisonResult {
    param(
        [object[]]$LoadedReports
    )

    $caseSummaries = @(
        @($LoadedReports) |
            ForEach-Object { New-ArtifactRootSystemInputCompareCaseSummary -LoadedReport $_ } |
            Where-Object { $null -ne $_ } |
            Sort-Object case
    )

    if (@($caseSummaries).Count -eq 0) {
        return $null
    }

    $changedCases = @(
        @($caseSummaries) |
            Where-Object { [bool]$_.changed } |
            ForEach-Object { [string]$_.case } |
            Sort-Object
    )
    $unchangedCases = @(
        @($caseSummaries) |
            Where-Object { -not [bool]$_.changed } |
            ForEach-Object { [string]$_.case } |
            Sort-Object
    )
    $summaryChanges = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.summary_changes)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $systemSpecChanges = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.system_spec_changes)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $declaredSubjectChanges = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.declared_subject_changes)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $resolvedInputChanges = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.resolved_input_changes)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $declaredFactNames = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.declared_fact_changes.added)
            @($caseSummary.declared_fact_changes.removed)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $subjectFactNames = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.subject_fact_changes.added)
            @($caseSummary.subject_fact_changes.removed)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $declaredContractNames = @(
        foreach ($caseSummary in @($caseSummaries)) {
            foreach ($contractChange in @($caseSummary.declared_contract_changes)) {
                $contractName = [string]$contractChange.contract
                if (-not [string]::IsNullOrWhiteSpace($contractName)) {
                    $contractName
                }
            }
        }
    ) | Sort-Object -Unique

    $summaryChangeMatrix = @(
        foreach ($changeText in @($summaryChanges)) {
            New-ArtifactRootSystemInputCompareChangeEntry -ChangeText $changeText -CaseSummaries $caseSummaries -CollectionName 'summary_changes'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object change
    $systemSpecChangeMatrix = @(
        foreach ($changeText in @($systemSpecChanges)) {
            New-ArtifactRootSystemInputCompareChangeEntry -ChangeText $changeText -CaseSummaries $caseSummaries -CollectionName 'system_spec_changes'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object change
    $declaredSubjectChangeMatrix = @(
        foreach ($changeText in @($declaredSubjectChanges)) {
            New-ArtifactRootSystemInputCompareChangeEntry -ChangeText $changeText -CaseSummaries $caseSummaries -CollectionName 'declared_subject_changes'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object change
    $resolvedInputChangeMatrix = @(
        foreach ($changeText in @($resolvedInputChanges)) {
            New-ArtifactRootSystemInputCompareChangeEntry -ChangeText $changeText -CaseSummaries $caseSummaries -CollectionName 'resolved_input_changes'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object change
    $declaredFactChangeMatrix = @(
        foreach ($factName in @($declaredFactNames)) {
            New-ArtifactRootSystemFormationCompareNamedChangeEntry -Name $factName -CaseSummaries $caseSummaries -FieldName 'fact' -CollectionName 'declared_fact_changes'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object fact
    $subjectFactChangeMatrix = @(
        foreach ($factName in @($subjectFactNames)) {
            New-ArtifactRootSystemFormationCompareNamedChangeEntry -Name $factName -CaseSummaries $caseSummaries -FieldName 'fact' -CollectionName 'subject_fact_changes'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object fact
    $declaredContractChangeMatrix = @(
        foreach ($contractName in @($declaredContractNames)) {
            New-ArtifactRootSystemInputCompareContractEntry -ContractName $contractName -CaseSummaries $caseSummaries
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object contract

    return [ordered]@{
        kind = 'system_input_summary/v0'
        mode = 'comparison'
        compared_case_count = @($caseSummaries).Count
        changed_case_count = @($changedCases).Count
        unchanged_case_count = @($unchangedCases).Count
        changed_cases = @($changedCases)
        unchanged_cases = @($unchangedCases)
        cases = @(
            @($caseSummaries) |
                Select-Object `
                    case,
                    board,
                    profile,
                    active_facets,
                    changed,
                    summary_changes,
                    system_spec_changes,
                    declared_subject_changes,
                    declared_fact_changes,
                    declared_contract_changes,
                    resolved_input_changes,
                    subject_fact_changes |
                Sort-Object case
        )
        summary_change_matrix = @($summaryChangeMatrix)
        system_spec_change_matrix = @($systemSpecChangeMatrix)
        declared_subject_change_matrix = @($declaredSubjectChangeMatrix)
        resolved_input_change_matrix = @($resolvedInputChangeMatrix)
        declared_fact_change_matrix = @($declaredFactChangeMatrix)
        declared_contract_change_matrix = @($declaredContractChangeMatrix)
        subject_fact_change_matrix = @($subjectFactChangeMatrix)
    }
}

function New-ArtifactRootBindingResultCaseSummary {
    param(
        $LoadedReport
    )

    if ($null -eq $LoadedReport -or $null -eq $LoadedReport.Data) {
        return $null
    }

    $report = $LoadedReport.Data
    $bindingResult = Get-BindingResultFromReport -ReportData $report

    return [pscustomobject][ordered]@{
        case = [string]$report.subject.case
        profile = [string]$report.subject.profile
        board = [string]$report.subject.board
        active_facets = @($report.subject.active_facets)
        required_binding_count = [int]$bindingResult.required_binding_count
        resolved_binding_count = [int]$bindingResult.resolved_binding_count
        unresolved_binding_count = [int]$bindingResult.unresolved_binding_count
        resolved_capabilities = @($bindingResult.resolved_capabilities)
        unresolved_capabilities = @($bindingResult.unresolved_capabilities)
        binding_entries = @($bindingResult.binding_entries)
    }
}

function New-ArtifactRootBindingCapabilityMatrixEntry {
    param(
        [string]$CapabilityName,
        [object[]]$CaseSummaries
    )

    if ([string]::IsNullOrWhiteSpace($CapabilityName)) {
        return $null
    }

    $capabilityCases = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        $bindingEntry = @(
            @($caseSummary.binding_entries) |
                Where-Object { [string]$_.capability -eq $CapabilityName } |
                Select-Object -First 1
        ) | Select-Object -First 1

        if ($null -eq $bindingEntry) {
            continue
        }

        $capabilityCases += [pscustomobject][ordered]@{
            case = [string]$caseSummary.case
            profile = [string]$caseSummary.profile
            board = [string]$caseSummary.board
            state = [string]$bindingEntry.state
            provider_nodes = @($bindingEntry.provider_nodes)
            consumer_nodes = @($bindingEntry.consumer_nodes)
            reason = if ([string]::IsNullOrWhiteSpace([string]$bindingEntry.reason)) { $null } else { [string]$bindingEntry.reason }
        }
    }

    if (@($capabilityCases).Count -eq 0) {
        return $null
    }

    return [ordered]@{
        capability = $CapabilityName
        case_count = @($capabilityCases).Count
        resolved_case_count = @($capabilityCases | Where-Object { [string]$_.state -eq 'resolved' }).Count
        unresolved_case_count = @($capabilityCases | Where-Object { [string]$_.state -eq 'unresolved' }).Count
        states = @(
            @($capabilityCases) |
                ForEach-Object { [string]$_.state } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        reasons = @(
            @($capabilityCases) |
                ForEach-Object { [string]$_.reason } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        provider_nodes = @(
            foreach ($capabilityCase in @($capabilityCases)) {
                @(Get-CaseQualifiedNodeNames -CaseName ([string]$capabilityCase.case) -NodeNames @($capabilityCase.provider_nodes))
            }
        ) | Sort-Object -Unique
        consumer_nodes = @(
            foreach ($capabilityCase in @($capabilityCases)) {
                @(Get-CaseQualifiedNodeNames -CaseName ([string]$capabilityCase.case) -NodeNames @($capabilityCase.consumer_nodes))
            }
        ) | Sort-Object -Unique
        cases = @($capabilityCases | Sort-Object case)
    }
}

function New-ArtifactRootBindingResultSummaryResult {
    param(
        [object[]]$LoadedReports
    )

    $caseSummaries = @(
        @($LoadedReports) |
            ForEach-Object { New-ArtifactRootBindingResultCaseSummary -LoadedReport $_ } |
            Where-Object { $null -ne $_ } |
            Sort-Object case
    )

    if (@($caseSummaries).Count -eq 0) {
        return $null
    }

    $capabilityNames = @(
        foreach ($caseSummary in @($caseSummaries)) {
            foreach ($bindingEntry in @($caseSummary.binding_entries)) {
                $capabilityName = [string]$bindingEntry.capability
                if (-not [string]::IsNullOrWhiteSpace($capabilityName)) {
                    $capabilityName
                }
            }
        }
    ) | Sort-Object -Unique

    $capabilityMatrix = @(
        foreach ($capabilityName in @($capabilityNames)) {
            New-ArtifactRootBindingCapabilityMatrixEntry -CapabilityName $capabilityName -CaseSummaries $caseSummaries
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object capability

    return [ordered]@{
        kind = 'binding_result_summary/v0'
        mode = 'summary'
        case_count = @($caseSummaries).Count
        totals = [ordered]@{
            required_binding_count = [int](@($caseSummaries | Measure-Object -Property required_binding_count -Sum).Sum)
            resolved_binding_count = [int](@($caseSummaries | Measure-Object -Property resolved_binding_count -Sum).Sum)
            unresolved_binding_count = [int](@($caseSummaries | Measure-Object -Property unresolved_binding_count -Sum).Sum)
        }
        cases = @(
            @($caseSummaries) |
                Select-Object `
                    case,
                    board,
                    profile,
                    active_facets,
                    required_binding_count,
                    resolved_binding_count,
                    unresolved_binding_count,
                    resolved_capabilities,
                    unresolved_capabilities |
                Sort-Object case
        )
        capability_matrix = @($capabilityMatrix)
        resolved_capability_matrix = @(
            @($capabilityMatrix) |
                Where-Object { [int]$_.resolved_case_count -gt 0 } |
                Sort-Object capability
        )
        unresolved_capability_matrix = @(
            @($capabilityMatrix) |
                Where-Object { [int]$_.unresolved_case_count -gt 0 } |
                Sort-Object capability
        )
    }
}

function New-ArtifactRootBringupOrderCaseSummary {
    param(
        $LoadedReport
    )

    if ($null -eq $LoadedReport -or $null -eq $LoadedReport.Data) {
        return $null
    }

    $report = $LoadedReport.Data
    $bringupOrder = Get-BringupOrderFromReport -ReportData $report

    return [pscustomobject][ordered]@{
        case = [string]$report.subject.case
        profile = [string]$report.subject.profile
        board = [string]$report.subject.board
        active_facets = @($report.subject.active_facets)
        ordered_node_count = [int]$bringupOrder.ordered_node_count
        blocked_node_count = [int]$bringupOrder.blocked_node_count
        phase_counts = $bringupOrder.phase_counts
        blocked_nodes = @(
            @($bringupOrder.entries) |
                Where-Object { [string]$_.state -eq 'blocked' } |
                ForEach-Object { [string]$_.node } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        entries = @($bringupOrder.entries)
    }
}

function New-ArtifactRootBringupOrderNodeMatrixEntry {
    param(
        [string]$NodeName,
        [object[]]$CaseSummaries
    )

    if ([string]::IsNullOrWhiteSpace($NodeName)) {
        return $null
    }

    $nodeCases = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        $bringupEntry = @(
            @($caseSummary.entries) |
                Where-Object { [string]$_.node -eq $NodeName } |
                Select-Object -First 1
        ) | Select-Object -First 1

        if ($null -eq $bringupEntry) {
            continue
        }

        $nodeCases += [pscustomobject][ordered]@{
            case = [string]$caseSummary.case
            profile = [string]$caseSummary.profile
            board = [string]$caseSummary.board
            order = [int]$bringupEntry.order
            kind = if ([string]::IsNullOrWhiteSpace([string]$bringupEntry.kind)) { $null } else { [string]$bringupEntry.kind }
            phase = if ([string]::IsNullOrWhiteSpace([string]$bringupEntry.phase)) { $null } else { [string]$bringupEntry.phase }
            runlevel_text = if ([string]::IsNullOrWhiteSpace([string]$bringupEntry.runlevel_text)) { $null } else { [string]$bringupEntry.runlevel_text }
            state = [string]$bringupEntry.state
            provides = @($bringupEntry.provides)
            requires = @($bringupEntry.requires)
            dependency_nodes = @($bringupEntry.dependency_nodes)
            missing_requires = @($bringupEntry.missing_requires)
        }
    }

    if (@($nodeCases).Count -eq 0) {
        return $null
    }

    return [ordered]@{
        node = $NodeName
        case_count = @($nodeCases).Count
        ready_case_count = @($nodeCases | Where-Object { [string]$_.state -eq 'ready' }).Count
        blocked_case_count = @($nodeCases | Where-Object { [string]$_.state -eq 'blocked' }).Count
        orders = @(
            @($nodeCases) |
                ForEach-Object { [int]$_.order } |
                Sort-Object -Unique
        )
        kinds = @(
            @($nodeCases) |
                ForEach-Object { [string]$_.kind } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        phases = @(
            @($nodeCases) |
                ForEach-Object { [string]$_.phase } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        runlevels = @(
            @($nodeCases) |
                ForEach-Object { [string]$_.runlevel_text } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        provides = @(
            foreach ($nodeCase in @($nodeCases)) {
                @($nodeCase.provides)
            }
        ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
        requires = @(
            foreach ($nodeCase in @($nodeCases)) {
                @($nodeCase.requires)
            }
        ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
        dependency_nodes = @(
            foreach ($nodeCase in @($nodeCases)) {
                @(Get-CaseQualifiedNodeNames -CaseName ([string]$nodeCase.case) -NodeNames @($nodeCase.dependency_nodes))
            }
        ) | Sort-Object -Unique
        missing_requires = @(
            foreach ($nodeCase in @($nodeCases)) {
                @($nodeCase.missing_requires)
            }
        ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
        cases = @($nodeCases | Sort-Object case)
    }
}

function New-ArtifactRootBringupOrderSummaryResult {
    param(
        [object[]]$LoadedReports
    )

    $caseSummaries = @(
        @($LoadedReports) |
            ForEach-Object { New-ArtifactRootBringupOrderCaseSummary -LoadedReport $_ } |
            Where-Object { $null -ne $_ } |
            Sort-Object case
    )

    if (@($caseSummaries).Count -eq 0) {
        return $null
    }

    $phaseCounts = @{}
    foreach ($caseSummary in @($caseSummaries)) {
        if ($null -eq $caseSummary.phase_counts) {
            continue
        }

        foreach ($phaseEntry in @($caseSummary.phase_counts.PSObject.Properties)) {
            Add-AggregatedCountMapEntry -Counts $phaseCounts -Name ([string]$phaseEntry.Name) -Amount ([int]$phaseEntry.Value)
        }
    }

    $nodeNames = @(
        foreach ($caseSummary in @($caseSummaries)) {
            foreach ($bringupEntry in @($caseSummary.entries)) {
                $nodeName = [string]$bringupEntry.node
                if (-not [string]::IsNullOrWhiteSpace($nodeName)) {
                    $nodeName
                }
            }
        }
    ) | Sort-Object -Unique

    $nodeMatrix = @(
        foreach ($nodeName in @($nodeNames)) {
            New-ArtifactRootBringupOrderNodeMatrixEntry -NodeName $nodeName -CaseSummaries $caseSummaries
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object node

    return [ordered]@{
        kind = 'bringup_order_summary/v0'
        mode = 'summary'
        case_count = @($caseSummaries).Count
        totals = [ordered]@{
            ordered_node_count = [int](@($caseSummaries | Measure-Object -Property ordered_node_count -Sum).Sum)
            blocked_node_count = [int](@($caseSummaries | Measure-Object -Property blocked_node_count -Sum).Sum)
        }
        phase_counts = ConvertTo-AggregatedOrderedCountMap -Counts $phaseCounts
        cases = @(
            @($caseSummaries) |
                Select-Object `
                    case,
                    board,
                    profile,
                    active_facets,
                    ordered_node_count,
                    blocked_node_count,
                    blocked_nodes,
                    phase_counts |
                Sort-Object case
        )
        node_matrix = @($nodeMatrix)
        blocked_node_matrix = @(
            @($nodeMatrix) |
                Where-Object { [int]$_.blocked_case_count -gt 0 } |
                Sort-Object node
        )
    }
}

function New-ArtifactRootResourceCompareCaseSummary {
    param(
        $LoadedReport
    )

    $report = $LoadedReport.Data
    $comparison = Get-ResourceContractComparisonFromReport -ReportData $report
    if ($null -eq $comparison) {
        return $null
    }

    return [pscustomobject][ordered]@{
        case = [string]$report.subject.case
        profile = [string]$report.subject.profile
        board = [string]$report.subject.board
        active_facets = @($report.subject.active_facets)
        changed = [bool]$comparison.changed
        summary_changes = @($comparison.summary_changes)
        contract_change_count = @($comparison.contract_changes).Count
        contracts_changed = @(
            @($comparison.contract_changes) |
                ForEach-Object { [string]$_.contract } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        provided_facts_added = @($comparison.provided_fact_changes.added)
        provided_facts_removed = @($comparison.provided_fact_changes.removed)
        hotspots_added = @($comparison.hotspot_changes.added)
        hotspots_removed = @($comparison.hotspot_changes.removed)
        contract_changes = @($comparison.contract_changes)
    }
}

function New-ArtifactRootResourceCompareSummaryChangeEntry {
    param(
        [string]$ChangeText,
        [object[]]$CaseSummaries
    )

    if ([string]::IsNullOrWhiteSpace($ChangeText)) {
        return $null
    }

    $changeCases = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        if (-not (@($caseSummary.summary_changes) -contains $ChangeText)) {
            continue
        }

        $changeCases += [pscustomobject][ordered]@{
            case = [string]$caseSummary.case
            profile = [string]$caseSummary.profile
            board = [string]$caseSummary.board
        }
    }

    return [ordered]@{
        change = $ChangeText
        case_count = @($changeCases).Count
        cases = @($changeCases | Sort-Object case)
    }
}

function New-ArtifactRootResourceCompareContractEntry {
    param(
        [string]$ContractName,
        [object[]]$CaseSummaries
    )

    if ([string]::IsNullOrWhiteSpace($ContractName)) {
        return $null
    }

    $contractCases = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        $contractChange = @(
            @($caseSummary.contract_changes) |
                Where-Object { [string]$_.contract -eq $ContractName } |
                Select-Object -First 1
        ) | Select-Object -First 1

        if ($null -eq $contractChange) {
            continue
        }

        $contractCases += [pscustomobject][ordered]@{
            case = [string]$caseSummary.case
            profile = [string]$caseSummary.profile
            board = [string]$caseSummary.board
            change_kind = [string]$contractChange.change_kind
            left_state = [string]$contractChange.left_state
            right_state = [string]$contractChange.right_state
            left_requires = @($contractChange.left_requires)
            right_requires = @($contractChange.right_requires)
            left_status_text = if ([string]::IsNullOrWhiteSpace([string]$contractChange.left_status_text)) { $null } else { [string]$contractChange.left_status_text }
            right_status_text = if ([string]::IsNullOrWhiteSpace([string]$contractChange.right_status_text)) { $null } else { [string]$contractChange.right_status_text }
        }
    }

    return [ordered]@{
        contract = $ContractName
        case_count = @($contractCases).Count
        change_kinds = @(
            @($contractCases) |
                ForEach-Object { [string]$_.change_kind } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        cases = @($contractCases | Sort-Object case)
    }
}

function New-ArtifactRootResourceContractComparisonResult {
    param(
        [object[]]$LoadedReports
    )

    $caseSummaries = @(
        @($LoadedReports) |
            ForEach-Object { New-ArtifactRootResourceCompareCaseSummary -LoadedReport $_ } |
            Where-Object { $null -ne $_ } |
            Sort-Object case
    )

    if (@($caseSummaries).Count -eq 0) {
        return $null
    }

    $summaryChanges = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.summary_changes)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique

    $contractNames = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.contracts_changed)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique

    $summaryChangeMatrix = @(
        foreach ($changeText in @($summaryChanges)) {
            New-ArtifactRootResourceCompareSummaryChangeEntry -ChangeText $changeText -CaseSummaries $caseSummaries
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object change

    $contractChangeMatrix = @(
        foreach ($contractName in @($contractNames)) {
            New-ArtifactRootResourceCompareContractEntry -ContractName $contractName -CaseSummaries $caseSummaries
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object contract

    return [ordered]@{
        compared_case_count = @($caseSummaries).Count
        changed_case_count = @($caseSummaries | Where-Object { [bool]$_.changed }).Count
        unchanged_case_count = @($caseSummaries | Where-Object { -not [bool]$_.changed }).Count
        changed_cases = @(
            @($caseSummaries) |
                Where-Object { [bool]$_.changed } |
                ForEach-Object { [string]$_.case } |
                Sort-Object
        )
        unchanged_cases = @(
            @($caseSummaries) |
                Where-Object { -not [bool]$_.changed } |
                ForEach-Object { [string]$_.case } |
                Sort-Object
        )
        contract_change_count = [int](@($caseSummaries | Measure-Object -Property contract_change_count -Sum).Sum)
        cases = @($caseSummaries)
        summary_change_matrix = @($summaryChangeMatrix)
        contract_change_matrix = @($contractChangeMatrix)
    }
}

function New-ArtifactRootFactResolutionCompareCaseSummary {
    param(
        $LoadedReport
    )

    $report = $LoadedReport.Data
    $comparison = Get-FactResolutionComparisonFromReport -ReportData $report
    if ($null -eq $comparison) {
        return $null
    }

    return [pscustomobject][ordered]@{
        case = [string]$report.subject.case
        profile = [string]$report.subject.profile
        board = [string]$report.subject.board
        active_facets = @($report.subject.active_facets)
        changed = [bool]$comparison.changed
        summary_changes = @($comparison.summary_changes)
        contract_change_count = @($comparison.contract_changes).Count
        contracts_changed = @(
            @($comparison.contract_changes) |
                ForEach-Object { [string]$_.contract } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        fact_inventory_changes = $comparison.fact_inventory_changes
        hotspots_added = @($comparison.hotspot_changes.added)
        hotspots_removed = @($comparison.hotspot_changes.removed)
        contract_changes = @($comparison.contract_changes)
    }
}

function New-ArtifactRootFactResolutionCompareFactEntry {
    param(
        [string]$FactName,
        [string]$FactGroup,
        [object[]]$CaseSummaries
    )

    if ([string]::IsNullOrWhiteSpace($FactName) -or [string]::IsNullOrWhiteSpace($FactGroup)) {
        return $null
    }

    $addedCases = @()
    $removedCases = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        if (@($caseSummary.fact_inventory_changes.$FactGroup.added) -contains $FactName) {
            $addedCases += [pscustomobject][ordered]@{
                case = [string]$caseSummary.case
                profile = [string]$caseSummary.profile
                board = [string]$caseSummary.board
            }
        }

        if (@($caseSummary.fact_inventory_changes.$FactGroup.removed) -contains $FactName) {
            $removedCases += [pscustomobject][ordered]@{
                case = [string]$caseSummary.case
                profile = [string]$caseSummary.profile
                board = [string]$caseSummary.board
            }
        }
    }

    return [ordered]@{
        fact = $FactName
        added_case_count = @($addedCases).Count
        removed_case_count = @($removedCases).Count
        added_cases = @($addedCases | Sort-Object case)
        removed_cases = @($removedCases | Sort-Object case)
    }
}

function New-ArtifactRootFactResolutionCompareFactInventoryMatrix {
    param(
        [object[]]$CaseSummaries
    )

    $matrix = [ordered]@{}
    foreach ($factGroup in @('declared_facts', 'subject_facts', 'required_facts', 'graph_provided_facts', 'audit_provided_facts', 'all_available_facts')) {
        $factNames = @(
            foreach ($caseSummary in @($CaseSummaries)) {
                @($caseSummary.fact_inventory_changes.$factGroup.added)
                @($caseSummary.fact_inventory_changes.$factGroup.removed)
            }
        ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique

        $matrix[$factGroup] = @(
            foreach ($factName in @($factNames)) {
                New-ArtifactRootFactResolutionCompareFactEntry -FactName $factName -FactGroup $factGroup -CaseSummaries $CaseSummaries
            }
        ) | Where-Object { $null -ne $_ } | Sort-Object fact
    }

    return $matrix
}

function New-ArtifactRootFactResolutionComparisonResult {
    param(
        [object[]]$LoadedReports
    )

    $caseSummaries = @(
        @($LoadedReports) |
            ForEach-Object { New-ArtifactRootFactResolutionCompareCaseSummary -LoadedReport $_ } |
            Where-Object { $null -ne $_ } |
            Sort-Object case
    )

    if (@($caseSummaries).Count -eq 0) {
        return $null
    }

    $summaryChanges = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.summary_changes)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique

    $contractNames = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.contracts_changed)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique

    $summaryChangeMatrix = @(
        foreach ($changeText in @($summaryChanges)) {
            New-ArtifactRootResourceCompareSummaryChangeEntry -ChangeText $changeText -CaseSummaries $caseSummaries
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object change

    $contractChangeMatrix = @(
        foreach ($contractName in @($contractNames)) {
            New-ArtifactRootResourceCompareContractEntry -ContractName $contractName -CaseSummaries $caseSummaries
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object contract

    return [ordered]@{
        compared_case_count = @($caseSummaries).Count
        changed_case_count = @($caseSummaries | Where-Object { [bool]$_.changed }).Count
        unchanged_case_count = @($caseSummaries | Where-Object { -not [bool]$_.changed }).Count
        changed_cases = @(
            @($caseSummaries) |
                Where-Object { [bool]$_.changed } |
                ForEach-Object { [string]$_.case } |
                Sort-Object
        )
        unchanged_cases = @(
            @($caseSummaries) |
                Where-Object { -not [bool]$_.changed } |
                ForEach-Object { [string]$_.case } |
                Sort-Object
        )
        contract_change_count = [int](@($caseSummaries | Measure-Object -Property contract_change_count -Sum).Sum)
        cases = @($caseSummaries)
        summary_change_matrix = @($summaryChangeMatrix)
        contract_change_matrix = @($contractChangeMatrix)
        fact_inventory_change_matrix = New-ArtifactRootFactResolutionCompareFactInventoryMatrix -CaseSummaries $caseSummaries
    }
}

function New-RecentTransitionEntry {
    param(
        [int]$Index,
        $ReportData,
        $Transition
    )

    if ($null -eq $Transition) {
        return $null
    }

    $capabilityName = [string]$Transition.capability
    $comparison = New-CapabilityComparisonResult -ReportData $ReportData -CapabilityName $capabilityName

    return [pscustomobject][ordered]@{
        order = $Index
        capability = $capabilityName
        action = [string]$Transition.action
        before = [string]$Transition.before
        after = [string]$Transition.after
        comparison = $comparison
    }
}

function New-RecentTransitionsComparisonResult {
    param(
        [object[]]$TransitionEntries
    )

    $comparedTransitions = @(
        @($TransitionEntries) |
            Where-Object {
                $comparison = $_.comparison
                ($null -ne $comparison) -and (
                    [bool]$comparison.bringup_changed -or
                    [bool]$comparison.resource_changed
                )
            }
    )
    $bringupComparedTransitions = @(
        @($TransitionEntries) |
            Where-Object {
                $comparison = $_.comparison
                ($null -ne $comparison) -and [bool]$comparison.bringup_changed
            }
    )
    $resourceComparedTransitions = @(
        @($TransitionEntries) |
            Where-Object {
                $comparison = $_.comparison
                ($null -ne $comparison) -and [bool]$comparison.resource_changed
            }
    )

    if (@($comparedTransitions).Count -eq 0) {
        return $null
    }

    $comparedCapabilities = @(
        @($comparedTransitions) |
            ForEach-Object { [string]$_.capability } |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            Sort-Object -Unique
    )
    $bringupComparedCapabilities = @(
        @($bringupComparedTransitions) |
            ForEach-Object { [string]$_.capability } |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            Sort-Object -Unique
    )
    $resourceComparedCapabilities = @(
        @($resourceComparedTransitions) |
            ForEach-Object { [string]$_.capability } |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            Sort-Object -Unique
    )

    $bringupChangeKinds = @(
        foreach ($entry in @($comparedTransitions)) {
            @($entry.comparison.bringup_change_kinds)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $resourceChangeKinds = @(
        foreach ($entry in @($comparedTransitions)) {
            @($entry.comparison.resource_change_kinds)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $resourceContracts = @(
        foreach ($entry in @($comparedTransitions)) {
            @($entry.comparison.resource_contracts)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique

    return [ordered]@{
        compared_transition_count = @($comparedTransitions).Count
        bringup_compare_transition_count = @($bringupComparedTransitions).Count
        resource_compare_transition_count = @($resourceComparedTransitions).Count
        compared_capability_count = @($comparedCapabilities).Count
        bringup_compare_capability_count = @($bringupComparedCapabilities).Count
        resource_compare_capability_count = @($resourceComparedCapabilities).Count
        compared_capabilities = @($comparedCapabilities)
        bringup_compare_capabilities = @($bringupComparedCapabilities)
        resource_compare_capabilities = @($resourceComparedCapabilities)
        bringup_change_kinds = @($bringupChangeKinds)
        resource_change_kinds = @($resourceChangeKinds)
        resource_contracts = @($resourceContracts)
    }
}

function New-RecentTransitionsResult {
    param(
        $ReportData
    )

    $transitionEntries = @()
    $transitionIndex = 0
    foreach ($transition in @($ReportData.runtime_observe.recent_transitions)) {
        $entry = New-RecentTransitionEntry -Index $transitionIndex -ReportData $ReportData -Transition $transition
        if ($null -ne $entry) {
            $transitionEntries += $entry
            $transitionIndex += 1
        }
    }

    $actionCounts = [ordered]@{}
    foreach ($entry in @($transitionEntries)) {
        $actionName = [string]$entry.action
        if ([string]::IsNullOrWhiteSpace($actionName)) {
            $actionName = 'unknown'
        }

        if ($actionCounts.Contains($actionName)) {
            $actionCounts[$actionName] += 1
        } else {
            $actionCounts[$actionName] = 1
        }
    }

    return [ordered]@{
        observed_capabilities = @(Get-RuntimeObservedCapabilitiesFromReport -ReportData $ReportData)
        publish_state_summary = $ReportData.runtime_observe.publish_state_summary
        export_state_summary = $ReportData.runtime_observe.export_state_summary
        transition_count = @($transitionEntries).Count
        transition_capabilities = @(
            @($transitionEntries) |
                ForEach-Object { [string]$_.capability } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        action_counts = $actionCounts
        comparison = New-RecentTransitionsComparisonResult -TransitionEntries $transitionEntries
        transitions = @($transitionEntries)
    }
}

function Format-RecentTransitionDisplayRow {
    param(
        $Entry
    )

    return [pscustomobject]@{
        order = [int]$Entry.order
        capability = [string]$Entry.capability
        action = [string]$Entry.action
        before = [string]$Entry.before
        after = [string]$Entry.after
        BrCmp = Format-StringArrayOrDash -Values @($Entry.comparison.bringup_change_kinds)
        ResCmp = Format-StringArrayOrDash -Values @($Entry.comparison.resource_change_kinds)
    }
}

function Get-CapabilityEvidence {
    param(
        $ReportData,
        $GraphInfo,
        [string]$CapabilityName
    )

    $entry = Find-BringupEvidenceEntry -ReportData $ReportData -CapabilityName $CapabilityName
    $providers = @()
    $consumers = @()
    $edges = @()
    if ($null -ne $GraphInfo) {
        foreach ($node in @($GraphInfo.Data.nodes)) {
            $provided = @(Get-CapabilityNames -Capabilities $node.provides)
            $required = @(Get-CapabilityNames -Capabilities $node.requires)
            if ($provided -contains $CapabilityName) {
                $providers += [string]$node.name
            }
            if ($required -contains $CapabilityName) {
                $consumers += [string]$node.name
            }
        }

        foreach ($edge in @($GraphInfo.Data.edges)) {
            $edgeCapabilityName = [string]$edge.capability.name
            if ([string]::IsNullOrWhiteSpace($edgeCapabilityName)) {
                $edgeCapabilityName = [string]$edge.capability.id
            }

            if ($edgeCapabilityName -ne $CapabilityName) {
                continue
            }

            $providerName = [string]$GraphInfo.Data.nodes[[int]$edge.provider_index].name
            $consumerName = [string]$GraphInfo.Data.nodes[[int]$edge.consumer_index].name
            $edges += "${providerName}->${consumerName}"
        }
    }

    $providerNodes = if ($null -ne $entry -and $null -ne $entry.PSObject.Properties['provider_nodes']) {
        @($entry.provider_nodes)
    } else {
        @($providers | Sort-Object -Unique)
    }
    $consumerNodes = if ($null -ne $entry -and $null -ne $entry.PSObject.Properties['consumer_nodes']) {
        @($entry.consumer_nodes)
    } else {
        @($consumers | Sort-Object -Unique)
    }

    $published = if ($null -ne $entry -and $null -ne $entry.PSObject.Properties['published']) {
        [bool]$entry.published
    } else {
        @($ReportData.bringup_evidence.published_capabilities) -contains $CapabilityName
    }
    $observed = if ($null -ne $entry -and $null -ne $entry.PSObject.Properties['observed']) {
        [bool]$entry.observed
    } else {
        @(Get-ObservedCapabilitiesForEvidence -ReportData $ReportData -GraphInfo $GraphInfo) -contains $CapabilityName
    }
    $unresolved = @($ReportData.structure.unresolved_bindings) -contains $CapabilityName
    $declaredFact = @($ReportData.structure.declared_facts) -contains $CapabilityName
    $requiredFact = @($ReportData.structure.required_facts) -contains $CapabilityName
    $resourceMentions = Get-ResourceContractMentions -ReportData $ReportData -CapabilityName $CapabilityName
    $materialized = if ($null -ne $entry -and $null -ne $entry.PSObject.Properties['materialized']) {
        [bool]$entry.materialized
    } else {
        ((@($providerNodes).Count -gt 0) -or (@($consumerNodes).Count -gt 0) -or (($null -eq $GraphInfo) -and ($observed -or $published)))
    }
    $required = (@($consumerNodes).Count -gt 0) -or $requiredFact
    $resourceFact = [bool]$resourceMentions.provided_fact
    $blockedReasons = if ($null -ne $entry -and $null -ne $entry.PSObject.Properties['blocked_reasons']) {
        @($entry.blocked_reasons)
    } else {
        @(Get-CapabilityScopedReasons -Reasons @($ReportData.bringup_evidence.blocked_reasons) -CapabilityName $CapabilityName)
    }
    $failedReasons = if ($null -ne $entry -and $null -ne $entry.PSObject.Properties['failed_reasons']) {
        @($entry.failed_reasons)
    } else {
        @(Get-CapabilityScopedReasons -Reasons @($ReportData.bringup_evidence.failed_reasons) -CapabilityName $CapabilityName)
    }
    $blocked = if ($null -ne $entry -and $null -ne $entry.PSObject.Properties['blocked']) {
        [bool]$entry.blocked
    } else {
        (@($blockedReasons).Count -gt 0) -or $unresolved
    }
    $failed = if ($null -ne $entry -and $null -ne $entry.PSObject.Properties['failed']) {
        [bool]$entry.failed
    } else {
        @($failedReasons).Count -gt 0
    }
    $declared = if ($null -ne $entry -and $null -ne $entry.PSObject.Properties['declared']) {
        [bool]$entry.declared
    } else {
        $materialized -or $observed -or $required -or $declaredFact -or $resourceFact -or $published -or $unresolved -or $blocked -or $failed
    }
    $publishState = if ($null -ne $entry -and $null -ne $entry.PSObject.Properties['publish_state']) {
        if ($null -eq $entry.publish_state -or [string]::IsNullOrWhiteSpace([string]$entry.publish_state)) { $null } else { [string]$entry.publish_state }
    } else {
        if ($published) {
            'published'
        } elseif ($declared) {
            'missing'
        } else {
            $null
        }
    }
    $exportState = if ($null -ne $entry -and $null -ne $entry.PSObject.Properties['export_state']) {
        if ($null -eq $entry.export_state -or [string]::IsNullOrWhiteSpace([string]$entry.export_state)) { $null } else { [string]$entry.export_state }
    } else {
        Get-CapabilityExportStateFromReport -ReportData $ReportData -CapabilityName $CapabilityName
    }

    return [ordered]@{
        capability = $CapabilityName
        provider_nodes = @($providerNodes | Sort-Object -Unique)
        consumer_nodes = @($consumerNodes | Sort-Object -Unique)
        edges = @($edges | Sort-Object -Unique)
        materialized = $materialized
        observed = $observed
        published = $published
        required = $required
        declared_fact = $declaredFact
        required_fact = $requiredFact
        resource_fact = $resourceFact
        unresolved_binding = $unresolved
        declared = $declared
        blocked = $blocked
        failed = $failed
        publish_state = $publishState
        export_state = $exportState
        blocked_reasons = @($blockedReasons)
        failed_reasons = @($failedReasons)
        resource_contract = $resourceMentions
    }
}

function Get-GraphEdgeRecords {
    param(
        $GraphInfo
    )

    if ($null -eq $GraphInfo) {
        return @()
    }

    $edges = @()
    foreach ($edge in @($GraphInfo.Data.edges)) {
        if ($null -eq $edge) {
            continue
        }

        $capabilityName = ''
        if ($null -ne $edge.capability) {
            $capabilityName = [string]$edge.capability.name
            if ([string]::IsNullOrWhiteSpace($capabilityName)) {
                $capabilityName = [string]$edge.capability.id
            }
        }

        $providerName = [string]$GraphInfo.Data.nodes[[int]$edge.provider_index].name
        $consumerName = [string]$GraphInfo.Data.nodes[[int]$edge.consumer_index].name
        $edges += [pscustomobject]@{
            provider = $providerName
            capability = $capabilityName
            consumer = $consumerName
        }
    }

    return @($edges)
}

function Format-GraphEdgeText {
    param(
        $Edge
    )

    if ($null -eq $Edge) {
        return ''
    }

    return "$([string]$Edge.provider) -[$([string]$Edge.capability)]-> $([string]$Edge.consumer)"
}

function Format-GraphPathText {
    param(
        $PathRecord
    )

    if ($null -eq $PathRecord) {
        return ''
    }

    if (@($PathRecord.edges).Count -eq 0) {
        return [string]$PathRecord.target_node
    }

    $parts = @([string]$PathRecord.root_node)
    foreach ($edge in @($PathRecord.edges)) {
        $parts += "-[$([string]$edge.capability)]->"
        $parts += [string]$edge.consumer
    }

    return ($parts -join ' ')
}

function Get-NodeDependencyPaths {
    param(
        $GraphInfo,
        [string]$TargetNode,
        [int]$MaxPathCount = 32
    )

    if ($null -eq $GraphInfo -or [string]::IsNullOrWhiteSpace($TargetNode)) {
        return @()
    }

    $nodeNames = @(
        @($GraphInfo.Data.nodes) |
            ForEach-Object { [string]$_.name } |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) }
    )
    if ($nodeNames -notcontains $TargetNode) {
        return @()
    }

    $incomingByConsumer = @{}
    foreach ($edge in @(Get-GraphEdgeRecords -GraphInfo $GraphInfo)) {
        $consumerName = [string]$edge.consumer
        if (-not $incomingByConsumer.ContainsKey($consumerName)) {
            $incomingByConsumer[$consumerName] = @()
        }

        $incomingByConsumer[$consumerName] = @($incomingByConsumer[$consumerName] + $edge)
    }

    $pathMap = @{}

    function Add-NodeDependencyPaths {
        param(
            [string]$NodeName,
            [object[]]$EdgeSuffix,
            [string[]]$VisitedNodes
        )

        if ($pathMap.Count -ge $MaxPathCount) {
            return
        }

        if (@($VisitedNodes) -contains $NodeName) {
            return
        }

        $incomingEdges = @()
        if ($incomingByConsumer.ContainsKey($NodeName)) {
            $incomingEdges = @($incomingByConsumer[$NodeName])
        }

        if ($incomingEdges.Count -eq 0) {
            $orderedEdges = @($EdgeSuffix)
            $pathNodes = @($NodeName)
            foreach ($edgeRecord in @($orderedEdges)) {
                $pathNodes += [string]$edgeRecord.consumer
            }

            $pathKey = [string]::Join(' -> ', @($pathNodes))
            if (-not $pathMap.ContainsKey($pathKey)) {
                $pathRecord = [ordered]@{
                    root_node = $NodeName
                    target_node = $TargetNode
                    nodes = @($pathNodes)
                    capabilities = @(
                        @($orderedEdges) |
                            ForEach-Object { [string]$_.capability }
                    )
                    edges = @($orderedEdges)
                }
                $pathRecord.text = Format-GraphPathText -PathRecord $pathRecord
                $pathMap[$pathKey] = [pscustomobject]$pathRecord
            }

            return
        }

        foreach ($incomingEdge in @($incomingEdges | Sort-Object provider, capability, consumer)) {
            Add-NodeDependencyPaths -NodeName ([string]$incomingEdge.provider) -EdgeSuffix (@($incomingEdge) + @($EdgeSuffix)) -VisitedNodes @($VisitedNodes + $NodeName)
        }
    }

    Add-NodeDependencyPaths -NodeName $TargetNode -EdgeSuffix @() -VisitedNodes @()

    return @(
        $pathMap.Values |
            Sort-Object root_node, target_node, text
    )
}

function New-GraphPathRecord {
    param(
        [string]$Role,
        $PathRecord
    )

    return [pscustomobject][ordered]@{
        role = $Role
        root_node = [string]$PathRecord.root_node
        target_node = [string]$PathRecord.target_node
        nodes = @($PathRecord.nodes)
        capabilities = @($PathRecord.capabilities)
        edges = @($PathRecord.edges)
        text = [string]$PathRecord.text
    }
}

function New-GraphPathResult {
    param(
        $ReportData,
        $GraphInfo,
        [string]$CapabilityName
    )

    $whyResult = New-WhyCapabilityResult -ReportData $ReportData -GraphInfo $GraphInfo -CapabilityName $CapabilityName
    $directEdges = @(
        @(Get-GraphEdgeRecords -GraphInfo $GraphInfo) |
            Where-Object { [string]$_.capability -eq $CapabilityName } |
            Sort-Object provider, consumer
    )

    $providerPaths = @()
    foreach ($providerNode in @($whyResult.evidence.provider_nodes)) {
        $providerPaths += @(
            @(Get-NodeDependencyPaths -GraphInfo $GraphInfo -TargetNode ([string]$providerNode)) |
                ForEach-Object { New-GraphPathRecord -Role 'provider' -PathRecord $_ }
        )
    }

    $consumerPaths = @()
    foreach ($consumerNode in @($whyResult.evidence.consumer_nodes)) {
        $consumerPaths += @(
            @(Get-NodeDependencyPaths -GraphInfo $GraphInfo -TargetNode ([string]$consumerNode)) |
                ForEach-Object { New-GraphPathRecord -Role 'consumer' -PathRecord $_ }
        )
    }

    if ($directEdges.Count -gt 0) {
        $consumerPaths = @(
            @($consumerPaths) |
                Where-Object {
                    @(
                        @($_.edges) |
                            Where-Object { [string]$_.capability -eq $CapabilityName }
                    ).Count -gt 0
                }
        )
    }

    $graphState = 'undeclared'
    if ($null -eq $GraphInfo) {
        $graphState = 'graph_unavailable'
    } elseif ($directEdges.Count -gt 0) {
        $graphState = 'edge_paths'
    } elseif (@($whyResult.evidence.provider_nodes).Count -gt 0) {
        $graphState = 'provider_terminal'
    } elseif (@($whyResult.evidence.consumer_nodes).Count -gt 0) {
        $graphState = 'required_without_provider'
    }

    return [ordered]@{
        capability = $CapabilityName
        state = $graphState
        availability_state = [string]$whyResult.state
        reasons = @($whyResult.reasons)
        comparison = $whyResult.comparison
        direct_edges = @(
            @($directEdges) |
                ForEach-Object {
                    [ordered]@{
                        provider = [string]$_.provider
                        capability = [string]$_.capability
                        consumer = [string]$_.consumer
                        text = Format-GraphEdgeText -Edge $_
                    }
                }
        )
        provider_nodes = @($whyResult.evidence.provider_nodes)
        consumer_nodes = @($whyResult.evidence.consumer_nodes)
        provider_paths = @($providerPaths | Sort-Object root_node, target_node, text)
        consumer_paths = @($consumerPaths | Sort-Object root_node, target_node, text)
    }
}

function New-WhyCapabilityResult {
    param(
        $ReportData,
        $GraphInfo,
        [string]$CapabilityName
    )

    $evidence = Get-CapabilityEvidence -ReportData $ReportData -GraphInfo $GraphInfo -CapabilityName $CapabilityName
    $comparison = New-WhyCapabilityComparisonResult -ReportData $ReportData -CapabilityName $CapabilityName
    $state = 'unknown'
    $reasons = @()

    if ($evidence.published) {
        $state = 'available'
        $reasons += 'capability is present in published_capabilities'
    } elseif ($evidence.failed) {
        $state = 'failed'
        $reasons += 'capability reached an explicit failed state in bringup evidence'
    } elseif ($evidence.unresolved_binding) {
        $state = 'unresolved_binding'
        $requiredBy = if (@($evidence.consumer_nodes).Count -gt 0) {
            "required by [$((@($evidence.consumer_nodes) -join ', '))]"
        } else {
            'marked unresolved in artifact report structure'
        }
        $reasons += "no materialized provider satisfied this binding; $requiredBy"
    } elseif ($evidence.blocked) {
        $state = 'blocked'
        $reasons += 'capability is currently blocked by unmet bringup preconditions'
    } elseif ($evidence.observed -and $null -eq $GraphInfo) {
        $state = 'runtime_observed_not_published'
        $reasons += 'capability appears in runtime observe evidence but is not present in published_capabilities'
        $reasons += 'this report has no materialized graph; capability is currently known only from runtime-side evidence'
    } elseif ($evidence.materialized) {
        $state = 'materialized_not_published'
        if (@($evidence.provider_nodes).Count -gt 0) {
            $reasons += "provided by [$((@($evidence.provider_nodes) -join ', '))] but not present in published_capabilities"
        } else {
            $reasons += 'capability is materialized in current bringup evidence but not present in published_capabilities'
        }
    } elseif ($evidence.observed) {
        $state = 'runtime_observed_not_materialized'
        $reasons += 'capability appears in observed evidence but no provider or consumer node was found in the materialized graph'
        $reasons += 'runtime observation and static graph have not yet converged on the same capability path'
    } elseif (@($evidence.consumer_nodes).Count -gt 0) {
        $state = 'required_without_provider'
        $reasons += "required by [$((@($evidence.consumer_nodes) -join ', '))] but no provider node was found in the materialized graph"
    } else {
        $state = 'undeclared'
        $reasons += 'capability was not found in materialized graph providers, consumers, unresolved bindings, or runtime observe evidence'
    }

    if (-not [string]::IsNullOrWhiteSpace([string]$evidence.publish_state)) {
        $reasons += "publish_state = $([string]$evidence.publish_state)"
    }
    if (-not [string]::IsNullOrWhiteSpace([string]$evidence.export_state)) {
        $reasons += "export_state = $([string]$evidence.export_state)"
    }
    if (@($evidence.blocked_reasons).Count -gt 0) {
        $reasons += @(
            @($evidence.blocked_reasons) |
                ForEach-Object { "blocked: $([string]$_)" }
        )
    }
    if (@($evidence.failed_reasons).Count -gt 0) {
        $reasons += @(
            @($evidence.failed_reasons) |
                ForEach-Object { "failed: $([string]$_)" }
        )
    }

    if (@($evidence.resource_contract.violations).Count -gt 0) {
        $reasons += @(
            @($evidence.resource_contract.violations) |
                ForEach-Object { "resource contract violated: $([string]$_)" }
        )
    }
    if (@($evidence.resource_contract.unknown).Count -gt 0) {
        $reasons += @(
            @($evidence.resource_contract.unknown) |
                ForEach-Object { "resource contract unknown: $([string]$_)" }
        )
    }

    if ($null -ne $comparison) {
        if ([bool]$comparison.bringup_changed) {
            $reasons += "compare bringup changed: $((@($comparison.bringup_change_kinds) -join ', '))"
            if ($null -ne $comparison.bringup_evidence) {
                $leftPublishState = Format-OptionalState -Value ([string]$comparison.bringup_evidence.left_publish_state)
                $rightPublishState = Format-OptionalState -Value ([string]$comparison.bringup_evidence.right_publish_state)
                $reasons += "compare publish_state = $leftPublishState -> $rightPublishState"

                $leftExportState = Format-OptionalState -Value ([string]$comparison.bringup_evidence.left_export_state)
                $rightExportState = Format-OptionalState -Value ([string]$comparison.bringup_evidence.right_export_state)
                if ($leftExportState -ne '-' -or $rightExportState -ne '-') {
                    $reasons += "compare export_state = $leftExportState -> $rightExportState"
                }
            }
        }

        if ([bool]$comparison.resource_changed) {
            $reasons += "compare resource changed: $((@($comparison.resource_change_kinds) -join ', '))"
            if ([bool]$comparison.resource_contract.provided_fact_added) {
                $reasons += 'compare provided_fact added in candidate'
            }
            if ([bool]$comparison.resource_contract.provided_fact_removed) {
                $reasons += 'compare provided_fact removed from candidate'
            }
            foreach ($contractChange in @($comparison.resource_contract.contract_changes)) {
                $contractName = [string]$contractChange.contract
                $changeKind = [string]$contractChange.change_kind
                $leftState = Format-OptionalState -Value ([string]$contractChange.left_state)
                $rightState = Format-OptionalState -Value ([string]$contractChange.right_state)
                $reasons += "compare contract $contractName changed: $changeKind ($leftState -> $rightState)"
            }
        }
    }

    return [ordered]@{
        kind = 'why_capability'
        scope = 'report'
        capability = $CapabilityName
        state = $state
        reasons = @($reasons)
        evidence = $evidence
        comparison = $comparison
    }
}

function New-ArtifactRootWhyCapabilityCaseResult {
    param(
        $LoadedReport,
        [string]$CapabilityName
    )

    $report = $LoadedReport.Data
    $graphInfo = Load-GraphFromArtifactReport -ReportData $report
    $whyResult = New-WhyCapabilityResult -ReportData $report -GraphInfo $graphInfo -CapabilityName $CapabilityName
    $bringupChangeKinds = @()
    $resourceChangeKinds = @()
    $resourceContracts = @()
    if ($null -ne $whyResult.comparison) {
        $bringupChangeKinds = @($whyResult.comparison.bringup_change_kinds)
        $resourceChangeKinds = @($whyResult.comparison.resource_change_kinds)
        $resourceContracts = @($whyResult.comparison.resource_contracts)
    }

    return [pscustomobject][ordered]@{
        case = [string]$report.subject.case
        profile = [string]$report.subject.profile
        board = [string]$report.subject.board
        state = [string]$whyResult.state
        reasons = @($whyResult.reasons)
        provider_nodes = @($whyResult.evidence.provider_nodes)
        consumer_nodes = @($whyResult.evidence.consumer_nodes)
        publish_state = if ([string]::IsNullOrWhiteSpace([string]$whyResult.evidence.publish_state)) { $null } else { [string]$whyResult.evidence.publish_state }
        export_state = if ([string]::IsNullOrWhiteSpace([string]$whyResult.evidence.export_state)) { $null } else { [string]$whyResult.evidence.export_state }
        compare_changed = ($null -ne $whyResult.comparison -and [bool]$whyResult.comparison.changed)
        bringup_changed = ($null -ne $whyResult.comparison -and [bool]$whyResult.comparison.bringup_changed)
        resource_changed = ($null -ne $whyResult.comparison -and [bool]$whyResult.comparison.resource_changed)
        bringup_change_kinds = @($bringupChangeKinds)
        resource_change_kinds = @($resourceChangeKinds)
        resource_contracts = @($resourceContracts)
    }
}

function New-ArtifactRootWhyCapabilityResult {
    param(
        [object[]]$LoadedReports,
        [string]$CapabilityName
    )

    $caseResults = @(
        @($LoadedReports) |
            ForEach-Object { New-ArtifactRootWhyCapabilityCaseResult -LoadedReport $_ -CapabilityName $CapabilityName } |
            Sort-Object case
    )

    $stateCounts = [ordered]@{}
    foreach ($caseResult in @($caseResults)) {
        $stateName = [string]$caseResult.state
        if ([string]::IsNullOrWhiteSpace($stateName)) {
            $stateName = 'unknown'
        }

        if ($stateCounts.Contains($stateName)) {
            $stateCounts[$stateName] += 1
        } else {
            $stateCounts[$stateName] = 1
        }
    }

    $comparedCases = @(
        @($caseResults) |
            Where-Object { [bool]$_.compare_changed } |
            ForEach-Object { [string]$_.case }
    )
    $bringupCompareCases = @(
        @($caseResults) |
            Where-Object { [bool]$_.bringup_changed } |
            ForEach-Object { [string]$_.case }
    )
    $resourceCompareCases = @(
        @($caseResults) |
            Where-Object { [bool]$_.resource_changed } |
            ForEach-Object { [string]$_.case }
    )
    $resourceContracts = @(
        foreach ($caseResult in @($caseResults)) {
            @($caseResult.resource_contracts)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique

    return [ordered]@{
        capability = $CapabilityName
        case_count = @($caseResults).Count
        state_counts = $stateCounts
        compared_case_count = @($comparedCases).Count
        bringup_compare_case_count = @($bringupCompareCases).Count
        resource_compare_case_count = @($resourceCompareCases).Count
        compared_cases = @($comparedCases | Sort-Object -Unique)
        bringup_compare_cases = @($bringupCompareCases | Sort-Object -Unique)
        resource_compare_cases = @($resourceCompareCases | Sort-Object -Unique)
        resource_contracts = @($resourceContracts)
        cases = @($caseResults)
    }
}

function New-CapListEntry {
    param(
        $ReportData,
        $GraphInfo,
        [string]$CapabilityName
    )

    $evidence = Get-CapabilityEvidence -ReportData $ReportData -GraphInfo $GraphInfo -CapabilityName $CapabilityName
    $comparison = New-CapabilityComparisonResult -ReportData $ReportData -CapabilityName $CapabilityName
    return [ordered]@{
        capability = $CapabilityName
        materialized = [bool]$evidence.materialized
        observed = [bool]$evidence.observed
        published = [bool]$evidence.published
        required = [bool]$evidence.required
        declared_fact = [bool]$evidence.declared_fact
        resource_fact = [bool]$evidence.resource_fact
        unresolved_binding = [bool]$evidence.unresolved_binding
        provider_nodes = @($evidence.provider_nodes)
        consumer_nodes = @($evidence.consumer_nodes)
        comparison = $comparison
    }
}

function Get-CapListEntries {
    param(
        $ReportData,
        $GraphInfo
    )

    $entries = @()
    foreach ($capabilityName in @(Get-ReportCapabilityNames -ReportData $ReportData -GraphInfo $GraphInfo)) {
        $entries += New-CapListEntry -ReportData $ReportData -GraphInfo $GraphInfo -CapabilityName $capabilityName
    }

    return @($entries | Sort-Object capability)
}

function Format-CapListDisplayRow {
    param(
        $Entry
    )

    return [pscustomobject]@{
        Capability = [string]$Entry.capability
        Mat = Format-BoolFlag -Value ([bool]$Entry.materialized)
        Obs = Format-BoolFlag -Value ([bool]$Entry.observed)
        Pub = Format-BoolFlag -Value ([bool]$Entry.published)
        Req = Format-BoolFlag -Value ([bool]$Entry.required)
        DecFact = Format-BoolFlag -Value ([bool]$Entry.declared_fact)
        ResFact = Format-BoolFlag -Value ([bool]$Entry.resource_fact)
        Unres = Format-BoolFlag -Value ([bool]$Entry.unresolved_binding)
        BrCmp = Format-StringArrayOrDash -Values @($Entry.comparison.bringup_change_kinds)
        ResCmp = Format-StringArrayOrDash -Values @($Entry.comparison.resource_change_kinds)
        Providers = Format-StringArray @($Entry.provider_nodes)
        Consumers = Format-StringArray @($Entry.consumer_nodes)
    }
}

function Get-CaseQualifiedNodeNames {
    param(
        [string]$CaseName,
        [string[]]$NodeNames
    )

    $qualified = @()
    foreach ($nodeName in @($NodeNames)) {
        if ([string]::IsNullOrWhiteSpace([string]$nodeName)) {
            continue
        }

        $qualified += "${CaseName}:$([string]$nodeName)"
    }

    return @($qualified)
}

function Add-CaseIf {
    param(
        $Entry,
        [bool]$Condition,
        [string]$CaseName,
        [string]$PropertyName
    )

    if (-not $Condition) {
        return
    }

    $existing = @($Entry.$PropertyName)
    $Entry.$PropertyName = @($existing + $CaseName)
}

function New-AggregatedCapListEntry {
    param(
        [string]$CapabilityName
    )

    return [pscustomobject]@{
        capability = $CapabilityName
        cases = @()
        materialized_cases = @()
        observed_cases = @()
        published_cases = @()
        required_cases = @()
        declared_fact_cases = @()
        resource_fact_cases = @()
        unresolved_binding_cases = @()
        compare_cases = @()
        bringup_compare_cases = @()
        resource_compare_cases = @()
        bringup_change_kinds = @()
        resource_change_kinds = @()
        resource_contracts = @()
        provider_nodes = @()
        consumer_nodes = @()
    }
}

function Normalize-AggregatedCapListEntry {
    param(
        $Entry
    )

    $cases = @($Entry.cases | Sort-Object -Unique)
    $materializedCases = @($Entry.materialized_cases | Sort-Object -Unique)
    $observedCases = @($Entry.observed_cases | Sort-Object -Unique)
    $publishedCases = @($Entry.published_cases | Sort-Object -Unique)
    $requiredCases = @($Entry.required_cases | Sort-Object -Unique)
    $declaredFactCases = @($Entry.declared_fact_cases | Sort-Object -Unique)
    $resourceFactCases = @($Entry.resource_fact_cases | Sort-Object -Unique)
    $unresolvedCases = @($Entry.unresolved_binding_cases | Sort-Object -Unique)
    $compareCases = @($Entry.compare_cases | Sort-Object -Unique)
    $bringupCompareCases = @($Entry.bringup_compare_cases | Sort-Object -Unique)
    $resourceCompareCases = @($Entry.resource_compare_cases | Sort-Object -Unique)

    return [ordered]@{
        capability = [string]$Entry.capability
        cases = $cases
        materialized = ($materializedCases.Count -gt 0)
        observed = ($observedCases.Count -gt 0)
        published = ($publishedCases.Count -gt 0)
        required = ($requiredCases.Count -gt 0)
        declared_fact = ($declaredFactCases.Count -gt 0)
        resource_fact = ($resourceFactCases.Count -gt 0)
        unresolved_binding = ($unresolvedCases.Count -gt 0)
        compare = ($compareCases.Count -gt 0)
        bringup_compare = ($bringupCompareCases.Count -gt 0)
        resource_compare = ($resourceCompareCases.Count -gt 0)
        materialized_cases = $materializedCases
        observed_cases = $observedCases
        published_cases = $publishedCases
        required_cases = $requiredCases
        declared_fact_cases = $declaredFactCases
        resource_fact_cases = $resourceFactCases
        unresolved_binding_cases = $unresolvedCases
        compare_cases = $compareCases
        bringup_compare_cases = $bringupCompareCases
        resource_compare_cases = $resourceCompareCases
        bringup_change_kinds = @($Entry.bringup_change_kinds | Sort-Object -Unique)
        resource_change_kinds = @($Entry.resource_change_kinds | Sort-Object -Unique)
        resource_contracts = @($Entry.resource_contracts | Sort-Object -Unique)
        provider_nodes = @($Entry.provider_nodes | Sort-Object -Unique)
        consumer_nodes = @($Entry.consumer_nodes | Sort-Object -Unique)
    }
}

function Format-AggregatedCapListDisplayRow {
    param(
        $Entry
    )

    return [pscustomobject]@{
        Capability = [string]$Entry.capability
        Cases = Format-StringArray @($Entry.cases)
        Mat = Format-StringArray @($Entry.materialized_cases)
        Obs = Format-StringArray @($Entry.observed_cases)
        Pub = Format-StringArray @($Entry.published_cases)
        Req = Format-StringArray @($Entry.required_cases)
        DecFact = Format-StringArray @($Entry.declared_fact_cases)
        ResFact = Format-StringArray @($Entry.resource_fact_cases)
        Unres = Format-StringArray @($Entry.unresolved_binding_cases)
        BrCmp = Format-StringArray @($Entry.bringup_compare_cases)
        ResCmp = Format-StringArray @($Entry.resource_compare_cases)
        ResCtr = Format-StringArray @($Entry.resource_contracts)
        Providers = Format-StringArray @($Entry.provider_nodes)
        Consumers = Format-StringArray @($Entry.consumer_nodes)
    }
}

function Format-ArtifactRootWhyCapabilityDisplayRow {
    param(
        $Entry
    )

    return [pscustomobject]@{
        Case = [string]$Entry.case
        State = [string]$Entry.state
        Compare = Format-BoolFlag -Value ([bool]$Entry.compare_changed)
        BrCmp = Format-StringArrayOrDash -Values @($Entry.bringup_change_kinds)
        ResCmp = Format-StringArrayOrDash -Values @($Entry.resource_change_kinds)
        Contracts = Format-StringArrayOrDash -Values @($Entry.resource_contracts)
        PubState = Format-OptionalState -Value ([string]$Entry.publish_state)
        ExpState = Format-OptionalState -Value ([string]$Entry.export_state)
        Providers = Format-StringArray @($Entry.provider_nodes)
        Consumers = Format-StringArray @($Entry.consumer_nodes)
        Reasons = (@($Entry.reasons) -join '; ')
    }
}

function Get-OptionalMemberValue {
    param(
        $Object,
        [string]$Name
    )

    if ($null -eq $Object -or [string]::IsNullOrWhiteSpace($Name)) {
        return $null
    }

    if ($Object -is [System.Collections.IDictionary]) {
        if ($Object.Contains($Name)) {
            return $Object[$Name]
        }

        return $null
    }

    if ($null -ne $Object.PSObject.Properties[$Name]) {
        return $Object.$Name
    }

    return $null
}

function New-CapListComparisonSummaryResult {
    param(
        [object[]]$Capabilities
    )

    $comparedCapabilities = @(
        @($Capabilities) |
            Where-Object {
                $comparison = Get-OptionalMemberValue -Object $_ -Name 'comparison'
                if ($null -ne $comparison) {
                    return (
                        [bool](Get-OptionalMemberValue -Object $comparison -Name 'bringup_changed') -or
                        [bool](Get-OptionalMemberValue -Object $comparison -Name 'resource_changed')
                    )
                }

                return (
                    [bool](Get-OptionalMemberValue -Object $_ -Name 'bringup_compare') -or
                    [bool](Get-OptionalMemberValue -Object $_ -Name 'resource_compare')
                )
            } |
            ForEach-Object { [string]$_.capability } |
            Sort-Object -Unique
    )
    $bringupComparedCapabilities = @(
        @($Capabilities) |
            Where-Object {
                $comparison = Get-OptionalMemberValue -Object $_ -Name 'comparison'
                if ($null -ne $comparison) {
                    return [bool](Get-OptionalMemberValue -Object $comparison -Name 'bringup_changed')
                }

                return [bool](Get-OptionalMemberValue -Object $_ -Name 'bringup_compare')
            } |
            ForEach-Object { [string]$_.capability } |
            Sort-Object -Unique
    )
    $resourceComparedCapabilities = @(
        @($Capabilities) |
            Where-Object {
                $comparison = Get-OptionalMemberValue -Object $_ -Name 'comparison'
                if ($null -ne $comparison) {
                    return [bool](Get-OptionalMemberValue -Object $comparison -Name 'resource_changed')
                }

                return [bool](Get-OptionalMemberValue -Object $_ -Name 'resource_compare')
            } |
            ForEach-Object { [string]$_.capability } |
            Sort-Object -Unique
    )

    if (@($comparedCapabilities).Count -eq 0) {
        return $null
    }

    return [ordered]@{
        compared_capability_count = @($comparedCapabilities).Count
        bringup_compare_capability_count = @($bringupComparedCapabilities).Count
        resource_compare_capability_count = @($resourceComparedCapabilities).Count
        compared_capabilities = @($comparedCapabilities)
        bringup_compare_capabilities = @($bringupComparedCapabilities)
        resource_compare_capabilities = @($resourceComparedCapabilities)
    }
}

function New-CapListReportView {
    param(
        $LoadedReport
    )

    $graphInfo = Load-GraphFromArtifactReport -ReportData $LoadedReport.Data

    $capabilities = @(Get-CapListEntries -ReportData $LoadedReport.Data -GraphInfo $graphInfo)
    $comparison = New-CapListComparisonSummaryResult -Capabilities $capabilities

    return [ordered]@{
        report_path = $LoadedReport.Path
        subject = $LoadedReport.Data.subject
        query = [ordered]@{
            kind = 'cap_list'
            scope = 'report'
            capabilities = $capabilities
            comparison = $comparison
        }
    }
}

function New-CapListArtifactRootAggregationResult {
    param(
        [object[]]$LoadedReports
    )

    $capabilityMap = @{}
    $caseNames = @()
    foreach ($loadedReport in @($LoadedReports)) {
        $caseName = [string]$loadedReport.Data.subject.case
        if (-not [string]::IsNullOrWhiteSpace($caseName)) {
            $caseNames += $caseName
        }

        $graphInfo = Load-GraphFromArtifactReport -ReportData $loadedReport.Data
        $entries = @(Get-CapListEntries -ReportData $loadedReport.Data -GraphInfo $graphInfo)
        foreach ($entry in @($entries)) {
            $capabilityName = [string]$entry.capability
            if (-not $capabilityMap.ContainsKey($capabilityName)) {
                $capabilityMap[$capabilityName] = New-AggregatedCapListEntry -CapabilityName $capabilityName
            }

            $aggregate = $capabilityMap[$capabilityName]
            $aggregate.cases = @($aggregate.cases + $caseName)
            Add-CaseIf -Entry $aggregate -Condition ([bool]$entry.materialized) -CaseName $caseName -PropertyName 'materialized_cases'
            Add-CaseIf -Entry $aggregate -Condition ([bool]$entry.observed) -CaseName $caseName -PropertyName 'observed_cases'
            Add-CaseIf -Entry $aggregate -Condition ([bool]$entry.published) -CaseName $caseName -PropertyName 'published_cases'
            Add-CaseIf -Entry $aggregate -Condition ([bool]$entry.required) -CaseName $caseName -PropertyName 'required_cases'
            Add-CaseIf -Entry $aggregate -Condition ([bool]$entry.declared_fact) -CaseName $caseName -PropertyName 'declared_fact_cases'
            Add-CaseIf -Entry $aggregate -Condition ([bool]$entry.resource_fact) -CaseName $caseName -PropertyName 'resource_fact_cases'
            Add-CaseIf -Entry $aggregate -Condition ([bool]$entry.unresolved_binding) -CaseName $caseName -PropertyName 'unresolved_binding_cases'
            Add-CaseIf -Entry $aggregate -Condition ([bool]$entry.comparison.bringup_changed -or [bool]$entry.comparison.resource_changed) -CaseName $caseName -PropertyName 'compare_cases'
            Add-CaseIf -Entry $aggregate -Condition ([bool]$entry.comparison.bringup_changed) -CaseName $caseName -PropertyName 'bringup_compare_cases'
            Add-CaseIf -Entry $aggregate -Condition ([bool]$entry.comparison.resource_changed) -CaseName $caseName -PropertyName 'resource_compare_cases'
            $aggregate.bringup_change_kinds = @($aggregate.bringup_change_kinds + @($entry.comparison.bringup_change_kinds))
            $aggregate.resource_change_kinds = @($aggregate.resource_change_kinds + @($entry.comparison.resource_change_kinds))
            $aggregate.resource_contracts = @($aggregate.resource_contracts + @($entry.comparison.resource_contracts))
            $aggregate.provider_nodes = @($aggregate.provider_nodes + @(Get-CaseQualifiedNodeNames -CaseName $caseName -NodeNames @($entry.provider_nodes)))
            $aggregate.consumer_nodes = @($aggregate.consumer_nodes + @(Get-CaseQualifiedNodeNames -CaseName $caseName -NodeNames @($entry.consumer_nodes)))
        }
    }

    $capabilities = @(
        $capabilityMap.Values |
            ForEach-Object { Normalize-AggregatedCapListEntry -Entry $_ } |
            Sort-Object capability
    )

    return [ordered]@{
        case_count = @($caseNames | Sort-Object -Unique).Count
        cases = @($caseNames | Sort-Object -Unique)
        capabilities = $capabilities
    }
}

function New-CapListArtifactRootView {
    param(
        [object[]]$LoadedReports,
        [string]$ArtifactRootPath
    )

    $aggregation = New-CapListArtifactRootAggregationResult -LoadedReports $LoadedReports
    $comparison = New-CapListComparisonSummaryResult -Capabilities @($aggregation.capabilities)

    return [ordered]@{
        artifact_root = $ArtifactRootPath
        query = [ordered]@{
            kind = 'cap_list'
            scope = 'artifact_root'
            case_count = [int]$aggregation.case_count
            cases = @($aggregation.cases)
            capabilities = @($aggregation.capabilities)
            comparison = $comparison
        }
    }
}

function New-ArtifactRootSystemFormationCaseSummary {
    param(
        $LoadedReport
    )

    if ($null -eq $LoadedReport -or $null -eq $LoadedReport.Data) {
        return $null
    }

    $report = $LoadedReport.Data
    if ($null -eq $report.PSObject.Properties['system_formation'] -or $null -eq $report.system_formation) {
        return $null
    }

    $formation = $report.system_formation
    return [pscustomobject][ordered]@{
        case = [string]$report.subject.case
        profile = [string]$report.subject.profile
        board = [string]$report.subject.board
        active_facets = @($report.subject.active_facets)
        status = [string]$formation.status
        case_kind = if ($null -ne $formation.PSObject.Properties['formation_basis'] -and $null -ne $formation.formation_basis) { [string]$formation.formation_basis.case_kind } else { $null }
        declared_fact_count = if ($null -ne $formation.PSObject.Properties['formation_basis'] -and $null -ne $formation.formation_basis) { [int]$formation.formation_basis.declared_fact_count } else { 0 }
        declared_contract_count = if ($null -ne $formation.PSObject.Properties['formation_basis'] -and $null -ne $formation.formation_basis) { [int]$formation.formation_basis.declared_contract_count } else { 0 }
        subject_fact_count = if ($null -ne $formation.PSObject.Properties['formation_basis'] -and $null -ne $formation.formation_basis) { [int]$formation.formation_basis.subject_fact_count } else { 0 }
        formation_basis = [ordered]@{
            case_kind = if ($null -ne $formation.PSObject.Properties['formation_basis'] -and $null -ne $formation.formation_basis) { [string]$formation.formation_basis.case_kind } else { $null }
            declared_fact_count = if ($null -ne $formation.PSObject.Properties['formation_basis'] -and $null -ne $formation.formation_basis) { [int]$formation.formation_basis.declared_fact_count } else { 0 }
            declared_contract_count = if ($null -ne $formation.PSObject.Properties['formation_basis'] -and $null -ne $formation.formation_basis) { [int]$formation.formation_basis.declared_contract_count } else { 0 }
            subject_fact_count = if ($null -ne $formation.PSObject.Properties['formation_basis'] -and $null -ne $formation.formation_basis) { [int]$formation.formation_basis.subject_fact_count } else { 0 }
        }
        required_binding_count = if ($null -ne $formation.PSObject.Properties['binding_summary'] -and $null -ne $formation.binding_summary) { [int]$formation.binding_summary.required_binding_count } else { 0 }
        resolved_binding_count = if ($null -ne $formation.PSObject.Properties['binding_summary'] -and $null -ne $formation.binding_summary) { [int]$formation.binding_summary.resolved_binding_count } else { 0 }
        unresolved_binding_count = if ($null -ne $formation.PSObject.Properties['binding_summary'] -and $null -ne $formation.binding_summary) { [int]$formation.binding_summary.unresolved_binding_count } else { 0 }
        unresolved_capabilities = if ($null -ne $formation.PSObject.Properties['binding_summary'] -and $null -ne $formation.binding_summary) { @($formation.binding_summary.unresolved_capabilities) } else { @() }
        binding_summary = [ordered]@{
            required_binding_count = if ($null -ne $formation.PSObject.Properties['binding_summary'] -and $null -ne $formation.binding_summary) { [int]$formation.binding_summary.required_binding_count } else { 0 }
            resolved_binding_count = if ($null -ne $formation.PSObject.Properties['binding_summary'] -and $null -ne $formation.binding_summary) { [int]$formation.binding_summary.resolved_binding_count } else { 0 }
            unresolved_binding_count = if ($null -ne $formation.PSObject.Properties['binding_summary'] -and $null -ne $formation.binding_summary) { [int]$formation.binding_summary.unresolved_binding_count } else { 0 }
            unresolved_capabilities = if ($null -ne $formation.PSObject.Properties['binding_summary'] -and $null -ne $formation.binding_summary) { @($formation.binding_summary.unresolved_capabilities) } else { @() }
        }
        ordered_node_count = if ($null -ne $formation.PSObject.Properties['bringup_summary'] -and $null -ne $formation.bringup_summary) { [int]$formation.bringup_summary.ordered_node_count } else { 0 }
        blocked_node_count = if ($null -ne $formation.PSObject.Properties['bringup_summary'] -and $null -ne $formation.bringup_summary) { [int]$formation.bringup_summary.blocked_node_count } else { 0 }
        blocked_nodes = if ($null -ne $formation.PSObject.Properties['bringup_summary'] -and $null -ne $formation.bringup_summary) { @($formation.bringup_summary.blocked_nodes) } else { @() }
        bringup_summary = [ordered]@{
            ordered_node_count = if ($null -ne $formation.PSObject.Properties['bringup_summary'] -and $null -ne $formation.bringup_summary) { [int]$formation.bringup_summary.ordered_node_count } else { 0 }
            blocked_node_count = if ($null -ne $formation.PSObject.Properties['bringup_summary'] -and $null -ne $formation.bringup_summary) { [int]$formation.bringup_summary.blocked_node_count } else { 0 }
            blocked_nodes = if ($null -ne $formation.PSObject.Properties['bringup_summary'] -and $null -ne $formation.bringup_summary) { @($formation.bringup_summary.blocked_nodes) } else { @() }
        }
        blocker_count = [int]$formation.blocker_count
        blockers = @($formation.blockers)
    }
}

function New-ArtifactRootSystemFormationMatrixCaseEntry {
    param(
        $CaseSummary
    )

    return [pscustomobject][ordered]@{
        case = [string]$CaseSummary.case
        profile = [string]$CaseSummary.profile
        board = [string]$CaseSummary.board
        status = [string]$CaseSummary.status
    }
}

function New-ArtifactRootSystemFormationNamedMatrixEntry {
    param(
        [string]$Name,
        [object[]]$CaseSummaries,
        [string]$PropertyName,
        [string]$FieldName
    )

    if ([string]::IsNullOrWhiteSpace($Name)) {
        return $null
    }

    $cases = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        if (-not (@($caseSummary.$PropertyName) -contains $Name)) {
            continue
        }

        $cases += New-ArtifactRootSystemFormationMatrixCaseEntry -CaseSummary $caseSummary
    }

    return [ordered]@{
        $FieldName = $Name
        case_count = @($cases).Count
        cases = @($cases | Sort-Object case)
    }
}

function New-ArtifactRootSystemFormationBlockerMatrixEntry {
    param(
        [string]$Kind,
        [string]$Name,
        [object[]]$CaseSummaries
    )

    if ([string]::IsNullOrWhiteSpace($Kind) -or [string]::IsNullOrWhiteSpace($Name)) {
        return $null
    }

    $cases = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        $matchedBlockers = @(
            @($caseSummary.blockers) |
                Where-Object { [string]$_.kind -eq $Kind -and [string]$_.name -eq $Name }
        )

        foreach ($blocker in @($matchedBlockers)) {
            $cases += [pscustomobject][ordered]@{
                case = [string]$caseSummary.case
                profile = [string]$caseSummary.profile
                board = [string]$caseSummary.board
                status = [string]$caseSummary.status
                state = [string]$blocker.state
                missing_requires = @($blocker.missing_requires)
                depends_on = @($blocker.dependency_nodes)
                reason = if ([string]::IsNullOrWhiteSpace([string]$blocker.reason)) { $null } else { [string]$blocker.reason }
            }
        }
    }

    return [ordered]@{
        kind = $Kind
        name = $Name
        case_count = @($cases).Count
        states = @(
            @($cases) |
                ForEach-Object { [string]$_.state } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        missing_requires = @(
            @($cases) |
                ForEach-Object { @($_.missing_requires) } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        depends_on = @(
            @($cases) |
                ForEach-Object { @($_.depends_on) } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        reasons = @(
            @($cases) |
                ForEach-Object { [string]$_.reason } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        cases = @($cases | Sort-Object case)
    }
}

function Convert-ArtifactRootBlockerKeysToEntries {
    param(
        [string[]]$BlockerKeys
    )

    $entries = @()
    foreach ($blockerKey in @($BlockerKeys | Sort-Object -Unique)) {
        $parts = [string]$blockerKey -split '\|', 2
        if (@($parts).Count -ne 2) {
            continue
        }

        $entries += [ordered]@{
            kind = [string]$parts[0]
            name = [string]$parts[1]
        }
    }

    return @($entries)
}

function New-ArtifactRootSystemFormationBlockerReasonMatrixEntry {
    param(
        [string]$ReasonText,
        [object[]]$CaseSummaries
    )

    if ([string]::IsNullOrWhiteSpace($ReasonText)) {
        return $null
    }

    $cases = @()
    $blockerKeys = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        $matchedBlockers = @(
            @($caseSummary.blockers) |
                Where-Object { [string]$_.reason -eq $ReasonText }
        )
        if (@($matchedBlockers).Count -eq 0) {
            continue
        }

        $caseBlockerKeys = @(
            foreach ($blocker in @($matchedBlockers)) {
                $kind = [string]$blocker.kind
                $name = [string]$blocker.name
                if ([string]::IsNullOrWhiteSpace($kind) -or [string]::IsNullOrWhiteSpace($name)) {
                    continue
                }

                "${kind}|${name}"
            }
        ) | Sort-Object -Unique
        $blockerKeys += @($caseBlockerKeys)

        $cases += [ordered]@{
            case = [string]$caseSummary.case
            profile = [string]$caseSummary.profile
            board = [string]$caseSummary.board
            status = [string]$caseSummary.status
            blockers = @(Convert-ArtifactRootBlockerKeysToEntries -BlockerKeys @($caseBlockerKeys))
        }
    }

    return [ordered]@{
        reason = $ReasonText
        case_count = @($cases).Count
        blocker_count = @($blockerKeys | Sort-Object -Unique).Count
        blockers = @(Convert-ArtifactRootBlockerKeysToEntries -BlockerKeys @($blockerKeys))
        cases = @($cases | Sort-Object case)
    }
}

function New-ArtifactRootSystemFormationBlockerDetailMatrixEntry {
    param(
        [string]$DetailName,
        [object[]]$CaseSummaries,
        [string]$CollectionName,
        [string]$FieldName
    )

    if ([string]::IsNullOrWhiteSpace($DetailName) -or
        [string]::IsNullOrWhiteSpace($CollectionName) -or
        [string]::IsNullOrWhiteSpace($FieldName)) {
        return $null
    }

    $cases = @()
    $blockerKeys = @()
    $reasons = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        $matchedBlockers = @(
            @($caseSummary.blockers) |
                Where-Object { @($_.$CollectionName) -contains $DetailName }
        )
        if (@($matchedBlockers).Count -eq 0) {
            continue
        }

        $caseBlockerKeys = @(
            foreach ($blocker in @($matchedBlockers)) {
                $kind = [string]$blocker.kind
                $name = [string]$blocker.name
                if ([string]::IsNullOrWhiteSpace($kind) -or [string]::IsNullOrWhiteSpace($name)) {
                    continue
                }

                "${kind}|${name}"
            }
        ) | Sort-Object -Unique
        $blockerKeys += @($caseBlockerKeys)
        $caseReasons = @(
            @($matchedBlockers) |
                ForEach-Object { [string]$_.reason } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        $reasons += @($caseReasons)

        $cases += [ordered]@{
            case = [string]$caseSummary.case
            profile = [string]$caseSummary.profile
            board = [string]$caseSummary.board
            status = [string]$caseSummary.status
            reasons = @($caseReasons)
            blockers = @(Convert-ArtifactRootBlockerKeysToEntries -BlockerKeys @($caseBlockerKeys))
        }
    }

    $result = [ordered]@{}
    $result[$FieldName] = $DetailName
    $result.case_count = @($cases).Count
    $result.blocker_count = @($blockerKeys | Sort-Object -Unique).Count
    $result.blockers = @(Convert-ArtifactRootBlockerKeysToEntries -BlockerKeys @($blockerKeys))
    $result.reasons = @(
        @($reasons) |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            Sort-Object -Unique
    )
    $result.cases = @($cases | Sort-Object case)
    return $result
}

function New-ArtifactRootSystemFormationSummaryResult {
    param(
        [object[]]$LoadedReports
    )

    $caseSummaries = @(
        @($LoadedReports) |
            ForEach-Object { New-ArtifactRootSystemFormationCaseSummary -LoadedReport $_ } |
            Where-Object { $null -ne $_ } |
            Sort-Object case
    )

    if (@($caseSummaries).Count -eq 0) {
        return $null
    }

    $formedCases = @(
        @($caseSummaries) |
            Where-Object { [string]$_.status -eq 'formed' } |
            ForEach-Object { [string]$_.case } |
            Sort-Object
    )
    $blockedCases = @(
        @($caseSummaries) |
            Where-Object { [string]$_.status -eq 'blocked' } |
            ForEach-Object { [string]$_.case } |
            Sort-Object
    )
    $unresolvedCapabilities = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.unresolved_capabilities)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $blockedNodes = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.blocked_nodes)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $blockerKeys = @(
        foreach ($caseSummary in @($caseSummaries)) {
            foreach ($blocker in @($caseSummary.blockers)) {
                $kind = [string]$blocker.kind
                $name = [string]$blocker.name
                if ([string]::IsNullOrWhiteSpace($kind) -or [string]::IsNullOrWhiteSpace($name)) {
                    continue
                }

                "${kind}|${name}"
            }
        }
    ) | Sort-Object -Unique

    $unresolvedCapabilityMatrix = @(
        foreach ($capabilityName in @($unresolvedCapabilities)) {
            New-ArtifactRootSystemFormationNamedMatrixEntry -Name $capabilityName -CaseSummaries $caseSummaries -PropertyName 'unresolved_capabilities' -FieldName 'capability'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object capability
    $blockedNodeMatrix = @(
        foreach ($nodeName in @($blockedNodes)) {
            New-ArtifactRootSystemFormationNamedMatrixEntry -Name $nodeName -CaseSummaries $caseSummaries -PropertyName 'blocked_nodes' -FieldName 'node'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object node
    $blockerMatrix = @(
        foreach ($blockerKey in @($blockerKeys)) {
            $parts = [string]$blockerKey -split '\|', 2
            if (@($parts).Count -ne 2) {
                continue
            }

            New-ArtifactRootSystemFormationBlockerMatrixEntry -Kind ([string]$parts[0]) -Name ([string]$parts[1]) -CaseSummaries $caseSummaries
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object kind, name

    return [ordered]@{
        case_count = @($caseSummaries).Count
        status_counts = [ordered]@{
            formed = @($formedCases).Count
            blocked = @($blockedCases).Count
        }
        formed_case_count = @($formedCases).Count
        blocked_case_count = @($blockedCases).Count
        formed_cases = @($formedCases)
        blocked_cases = @($blockedCases)
        totals = [ordered]@{
            required_binding_count = [int](@($caseSummaries | Measure-Object -Property required_binding_count -Sum).Sum)
            resolved_binding_count = [int](@($caseSummaries | Measure-Object -Property resolved_binding_count -Sum).Sum)
            unresolved_binding_count = [int](@($caseSummaries | Measure-Object -Property unresolved_binding_count -Sum).Sum)
            ordered_node_count = [int](@($caseSummaries | Measure-Object -Property ordered_node_count -Sum).Sum)
            blocked_node_count = [int](@($caseSummaries | Measure-Object -Property blocked_node_count -Sum).Sum)
            blocker_count = [int](@($caseSummaries | Measure-Object -Property blocker_count -Sum).Sum)
        }
        cases = @(
            @($caseSummaries) |
                Select-Object `
                    case,
                    board,
                    profile,
                    active_facets,
                    status,
                    case_kind,
                    declared_fact_count,
                    declared_contract_count,
                    subject_fact_count,
                    required_binding_count,
                    resolved_binding_count,
                    unresolved_binding_count,
                    unresolved_capabilities,
                    ordered_node_count,
                    blocked_node_count,
                    blocked_nodes,
                    blocker_count,
                    blockers |
                Sort-Object case
        )
        unresolved_capability_matrix = @($unresolvedCapabilityMatrix)
        blocked_node_matrix = @($blockedNodeMatrix)
        blocker_matrix = @($blockerMatrix)
    }
}

function New-ArtifactRootSystemFormationCompareCaseSummary {
    param(
        $LoadedReport
    )

    if ($null -eq $LoadedReport -or $null -eq $LoadedReport.Data) {
        return $null
    }

    $report = $LoadedReport.Data
    $comparison = Get-SystemFormationComparisonFromReport -ReportData $report
    if ($null -eq $comparison) {
        return $null
    }

    return [pscustomobject][ordered]@{
        case = [string]$report.subject.case
        profile = [string]$report.subject.profile
        board = [string]$report.subject.board
        active_facets = @($report.subject.active_facets)
        changed = [bool]$comparison.changed
        left_status = [string]$comparison.left.status
        right_status = [string]$comparison.right.status
        summary_changes = @($comparison.summary_changes)
        blocker_changes = @($comparison.blocker_changes)
        unresolved_capability_changes = [ordered]@{
            added = @($comparison.unresolved_capability_changes.added)
            removed = @($comparison.unresolved_capability_changes.removed)
        }
        blocked_node_changes = [ordered]@{
            added = @($comparison.blocked_node_changes.added)
            removed = @($comparison.blocked_node_changes.removed)
        }
    }
}

function New-ArtifactRootSystemFormationCompareStatusEntry {
    param(
        [string]$Transition,
        [object[]]$CaseSummaries
    )

    if ([string]::IsNullOrWhiteSpace($Transition)) {
        return $null
    }

    $cases = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        $currentTransition = "$([string]$caseSummary.left_status)->$([string]$caseSummary.right_status)"
        if ([string]$currentTransition -ne $Transition) {
            continue
        }

        $cases += [pscustomobject][ordered]@{
            case = [string]$caseSummary.case
            profile = [string]$caseSummary.profile
            board = [string]$caseSummary.board
        }
    }

    return [ordered]@{
        transition = $Transition
        case_count = @($cases).Count
        cases = @($cases | Sort-Object case)
    }
}

function New-ArtifactRootSystemFormationCompareSummaryChangeEntry {
    param(
        [string]$ChangeText,
        [object[]]$CaseSummaries
    )

    if ([string]::IsNullOrWhiteSpace($ChangeText)) {
        return $null
    }

    $cases = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        if (-not (@($caseSummary.summary_changes) -contains $ChangeText)) {
            continue
        }

        $cases += [pscustomobject][ordered]@{
            case = [string]$caseSummary.case
            profile = [string]$caseSummary.profile
            board = [string]$caseSummary.board
        }
    }

    return [ordered]@{
        change = $ChangeText
        case_count = @($cases).Count
        cases = @($cases | Sort-Object case)
    }
}

function New-ArtifactRootSystemFormationCompareBlockerEntry {
    param(
        [string]$Kind,
        [string]$Name,
        [object[]]$CaseSummaries
    )

    if ([string]::IsNullOrWhiteSpace($Kind) -or [string]::IsNullOrWhiteSpace($Name)) {
        return $null
    }

    $cases = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        $matchedChanges = @(
            @($caseSummary.blocker_changes) |
                Where-Object { [string]$_.kind -eq $Kind -and [string]$_.name -eq $Name }
        )

        foreach ($change in @($matchedChanges)) {
            $cases += [pscustomobject][ordered]@{
                case = [string]$caseSummary.case
                profile = [string]$caseSummary.profile
                board = [string]$caseSummary.board
                change_kind = [string]$change.change_kind
                left_state = if ([string]::IsNullOrWhiteSpace([string]$change.left_state)) { $null } else { [string]$change.left_state }
                right_state = if ([string]::IsNullOrWhiteSpace([string]$change.right_state)) { $null } else { [string]$change.right_state }
                left_reason = if ([string]::IsNullOrWhiteSpace([string]$change.left_reason)) { $null } else { [string]$change.left_reason }
                right_reason = if ([string]::IsNullOrWhiteSpace([string]$change.right_reason)) { $null } else { [string]$change.right_reason }
                left_missing_requires = @($change.left_missing_requires)
                right_missing_requires = @($change.right_missing_requires)
                left_depends_on = @($change.left_dependency_nodes)
                right_depends_on = @($change.right_dependency_nodes)
            }
        }
    }

    return [ordered]@{
        kind = $Kind
        name = $Name
        case_count = @($cases).Count
        change_kinds = @(
            @($cases) |
                ForEach-Object { [string]$_.change_kind } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        states = @(
            @($cases) |
                ForEach-Object { @([string]$_.left_state, [string]$_.right_state) } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        reasons = @(
            @($cases) |
                ForEach-Object { @([string]$_.left_reason, [string]$_.right_reason) } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        missing_requires = @(
            @($cases) |
                ForEach-Object { @($_.left_missing_requires) + @($_.right_missing_requires) } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        depends_on = @(
            @($cases) |
                ForEach-Object { @($_.left_depends_on) + @($_.right_depends_on) } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        cases = @($cases | Sort-Object case)
    }
}

function New-ArtifactRootSystemFormationCompareBlockerReasonEntry {
    param(
        [string]$ReasonText,
        [object[]]$CaseSummaries
    )

    if ([string]::IsNullOrWhiteSpace($ReasonText)) {
        return $null
    }

    $cases = @()
    $blockerKeys = @()
    $changeKinds = @()
    $states = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        $matchedChanges = @(
            @($caseSummary.blocker_changes) |
                Where-Object {
                    ([string]$_.left_reason -eq $ReasonText) -or
                    ([string]$_.right_reason -eq $ReasonText)
                }
        )
        if (@($matchedChanges).Count -eq 0) {
            continue
        }

        $caseBlockerKeys = @(
            foreach ($change in @($matchedChanges)) {
                $kind = [string]$change.kind
                $name = [string]$change.name
                if ([string]::IsNullOrWhiteSpace($kind) -or [string]::IsNullOrWhiteSpace($name)) {
                    continue
                }

                "${kind}|${name}"
            }
        ) | Sort-Object -Unique
        $blockerKeys += @($caseBlockerKeys)
        $caseChangeKinds = @(
            @($matchedChanges) |
                ForEach-Object { [string]$_.change_kind } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        $changeKinds += @($caseChangeKinds)
        $caseStates = @(
            @($matchedChanges) |
                ForEach-Object { @([string]$_.left_state, [string]$_.right_state) } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        $states += @($caseStates)

        $cases += [ordered]@{
            case = [string]$caseSummary.case
            profile = [string]$caseSummary.profile
            board = [string]$caseSummary.board
            change_kinds = @($caseChangeKinds)
            states = @($caseStates)
            blockers = @(Convert-ArtifactRootBlockerKeysToEntries -BlockerKeys @($caseBlockerKeys))
        }
    }

    return [ordered]@{
        reason = $ReasonText
        case_count = @($cases).Count
        blocker_count = @($blockerKeys | Sort-Object -Unique).Count
        blockers = @(Convert-ArtifactRootBlockerKeysToEntries -BlockerKeys @($blockerKeys))
        change_kinds = @(
            @($changeKinds) |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        states = @(
            @($states) |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        cases = @($cases | Sort-Object case)
    }
}

function New-ArtifactRootSystemFormationCompareBlockerDetailEntry {
    param(
        [string]$DetailName,
        [object[]]$CaseSummaries,
        [string]$LeftCollectionName,
        [string]$RightCollectionName,
        [string]$FieldName
    )

    if ([string]::IsNullOrWhiteSpace($DetailName) -or
        [string]::IsNullOrWhiteSpace($LeftCollectionName) -or
        [string]::IsNullOrWhiteSpace($RightCollectionName) -or
        [string]::IsNullOrWhiteSpace($FieldName)) {
        return $null
    }

    $cases = @()
    $blockerKeys = @()
    $changeKinds = @()
    $reasons = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        $matchedChanges = @(
            @($caseSummary.blocker_changes) |
                Where-Object {
                    (@($_.$LeftCollectionName) -contains $DetailName) -or
                    (@($_.$RightCollectionName) -contains $DetailName)
                }
        )
        if (@($matchedChanges).Count -eq 0) {
            continue
        }

        $caseBlockerKeys = @(
            foreach ($change in @($matchedChanges)) {
                $kind = [string]$change.kind
                $name = [string]$change.name
                if ([string]::IsNullOrWhiteSpace($kind) -or [string]::IsNullOrWhiteSpace($name)) {
                    continue
                }

                "${kind}|${name}"
            }
        ) | Sort-Object -Unique
        $blockerKeys += @($caseBlockerKeys)
        $caseChangeKinds = @(
            @($matchedChanges) |
                ForEach-Object { [string]$_.change_kind } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        $changeKinds += @($caseChangeKinds)
        $caseReasons = @(
            @($matchedChanges) |
                ForEach-Object { @([string]$_.left_reason, [string]$_.right_reason) } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        $reasons += @($caseReasons)

        $cases += [ordered]@{
            case = [string]$caseSummary.case
            profile = [string]$caseSummary.profile
            board = [string]$caseSummary.board
            change_kinds = @($caseChangeKinds)
            reasons = @($caseReasons)
            blockers = @(Convert-ArtifactRootBlockerKeysToEntries -BlockerKeys @($caseBlockerKeys))
        }
    }

    $result = [ordered]@{}
    $result[$FieldName] = $DetailName
    $result.case_count = @($cases).Count
    $result.blocker_count = @($blockerKeys | Sort-Object -Unique).Count
    $result.blockers = @(Convert-ArtifactRootBlockerKeysToEntries -BlockerKeys @($blockerKeys))
    $result.change_kinds = @(
        @($changeKinds) |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            Sort-Object -Unique
    )
    $result.reasons = @(
        @($reasons) |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            Sort-Object -Unique
    )
    $result.cases = @($cases | Sort-Object case)
    return $result
}

function New-ArtifactRootSystemFormationCompareNamedChangeEntry {
    param(
        [string]$Name,
        [object[]]$CaseSummaries,
        [string]$FieldName,
        [string]$CollectionName
    )

    if ([string]::IsNullOrWhiteSpace($Name)) {
        return $null
    }

    $cases = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        $changeKinds = @()
        if (@($caseSummary.$CollectionName.added) -contains $Name) {
            $changeKinds += 'added'
        }
        if (@($caseSummary.$CollectionName.removed) -contains $Name) {
            $changeKinds += 'removed'
        }
        if (@($changeKinds).Count -eq 0) {
            continue
        }

        $cases += [pscustomobject][ordered]@{
            case = [string]$caseSummary.case
            profile = [string]$caseSummary.profile
            board = [string]$caseSummary.board
            change_kinds = @($changeKinds | Sort-Object -Unique)
        }
    }

    return [ordered]@{
        $FieldName = $Name
        case_count = @($cases).Count
        change_kinds = @(
            @($cases) |
                ForEach-Object { @($_.change_kinds) } |
                Sort-Object -Unique
        )
        cases = @($cases | Sort-Object case)
    }
}

function New-ArtifactRootSystemFormationComparisonResult {
    param(
        [object[]]$LoadedReports
    )

    $caseSummaries = @(
        @($LoadedReports) |
            ForEach-Object { New-ArtifactRootSystemFormationCompareCaseSummary -LoadedReport $_ } |
            Where-Object { $null -ne $_ } |
            Sort-Object case
    )

    if (@($caseSummaries).Count -eq 0) {
        return $null
    }

    $changedCases = @(
        @($caseSummaries) |
            Where-Object { [bool]$_.changed } |
            ForEach-Object { [string]$_.case } |
            Sort-Object
    )
    $unchangedCases = @(
        @($caseSummaries) |
            Where-Object { -not [bool]$_.changed } |
            ForEach-Object { [string]$_.case } |
            Sort-Object
    )
    $statusTransitions = @(
        foreach ($caseSummary in @($caseSummaries)) {
            "$([string]$caseSummary.left_status)->$([string]$caseSummary.right_status)"
        }
    ) | Where-Object { $_ -ne '->' } | Sort-Object -Unique
    $summaryChanges = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.summary_changes)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $blockerKeys = @(
        foreach ($caseSummary in @($caseSummaries)) {
            foreach ($change in @($caseSummary.blocker_changes)) {
                $kind = [string]$change.kind
                $name = [string]$change.name
                if ([string]::IsNullOrWhiteSpace($kind) -or [string]::IsNullOrWhiteSpace($name)) {
                    continue
                }

                "${kind}|${name}"
            }
        }
    ) | Sort-Object -Unique
    $unresolvedCapabilityNames = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.unresolved_capability_changes.added)
            @($caseSummary.unresolved_capability_changes.removed)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $blockedNodeNames = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.blocked_node_changes.added)
            @($caseSummary.blocked_node_changes.removed)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique

    $statusChangeMatrix = @(
        foreach ($transition in @($statusTransitions)) {
            New-ArtifactRootSystemFormationCompareStatusEntry -Transition $transition -CaseSummaries $caseSummaries
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object transition
    $summaryChangeMatrix = @(
        foreach ($changeText in @($summaryChanges)) {
            New-ArtifactRootSystemFormationCompareSummaryChangeEntry -ChangeText $changeText -CaseSummaries $caseSummaries
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object change
    $blockerChangeMatrix = @(
        foreach ($blockerKey in @($blockerKeys)) {
            $parts = [string]$blockerKey -split '\|', 2
            if (@($parts).Count -ne 2) {
                continue
            }

            New-ArtifactRootSystemFormationCompareBlockerEntry -Kind ([string]$parts[0]) -Name ([string]$parts[1]) -CaseSummaries $caseSummaries
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object kind, name
    $unresolvedCapabilityChangeMatrix = @(
        foreach ($capabilityName in @($unresolvedCapabilityNames)) {
            New-ArtifactRootSystemFormationCompareNamedChangeEntry -Name $capabilityName -CaseSummaries $caseSummaries -FieldName 'capability' -CollectionName 'unresolved_capability_changes'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object capability
    $blockedNodeChangeMatrix = @(
        foreach ($nodeName in @($blockedNodeNames)) {
            New-ArtifactRootSystemFormationCompareNamedChangeEntry -Name $nodeName -CaseSummaries $caseSummaries -FieldName 'node' -CollectionName 'blocked_node_changes'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object node

    return [ordered]@{
        compared_case_count = @($caseSummaries).Count
        changed_case_count = @($changedCases).Count
        unchanged_case_count = @($unchangedCases).Count
        changed_cases = @($changedCases)
        unchanged_cases = @($unchangedCases)
        cases = @(
            @($caseSummaries) |
                Select-Object `
                    case,
                    board,
                    profile,
                    active_facets,
                    changed,
                    left_status,
                    right_status,
                    summary_changes,
                    blocker_changes,
                    unresolved_capability_changes,
                    blocked_node_changes |
                Sort-Object case
        )
        status_change_matrix = @($statusChangeMatrix)
        summary_change_matrix = @($summaryChangeMatrix)
        blocker_change_matrix = @($blockerChangeMatrix)
        unresolved_capability_change_matrix = @($unresolvedCapabilityChangeMatrix)
        blocked_node_change_matrix = @($blockedNodeChangeMatrix)
    }
}

function New-ArtifactRootSystemCompilerSubjectProjection {
    param(
        $Report
    )

    if ($null -eq $Report) {
        return $null
    }

    return [ordered]@{
        case = [string]$Report.subject.case
        profile = [string]$Report.subject.profile
        board = [string]$Report.subject.board
        active_facets = @($Report.subject.active_facets)
    }
}

function New-ArtifactRootSystemCompilerCaseProjection {
    param(
        $LoadedReport
    )

    if ($null -eq $LoadedReport -or $null -eq $LoadedReport.Data) {
        return $null
    }

    $report = $LoadedReport.Data
    $systemInputSummary = New-ArtifactRootSystemInputCaseSummary -LoadedReport $LoadedReport
    $bindingResultSummary = New-ArtifactRootBindingResultCaseSummary -LoadedReport $LoadedReport
    $bringupOrderSummary = New-ArtifactRootBringupOrderCaseSummary -LoadedReport $LoadedReport
    $systemFormationSummary = New-ArtifactRootSystemFormationCaseSummary -LoadedReport $LoadedReport

    if ($null -eq $systemInputSummary -and
        $null -eq $bindingResultSummary -and
        $null -eq $bringupOrderSummary -and
        $null -eq $systemFormationSummary) {
        return $null
    }

    $formationBasis = if ($null -ne $systemFormationSummary -and $null -ne $systemFormationSummary.formation_basis) {
        [ordered]@{
            case_kind = [string]$systemFormationSummary.formation_basis.case_kind
            declared_fact_count = [int]$systemFormationSummary.formation_basis.declared_fact_count
            declared_contract_count = [int]$systemFormationSummary.formation_basis.declared_contract_count
            subject_fact_count = [int]$systemFormationSummary.formation_basis.subject_fact_count
        }
    } elseif ($null -ne $systemInputSummary) {
        [ordered]@{
            case_kind = if ($null -eq $systemInputSummary.case_kind) { $null } else { [string]$systemInputSummary.case_kind }
            declared_fact_count = [int]@($systemInputSummary.declared_facts).Count
            declared_contract_count = [int]@($systemInputSummary.declared_contract_entries).Count
            subject_fact_count = [int]@($systemInputSummary.subject_facts).Count
        }
    } else {
        $null
    }
    $bindingSummary = if ($null -ne $systemFormationSummary -and $null -ne $systemFormationSummary.binding_summary) {
        [ordered]@{
            required_binding_count = [int]$systemFormationSummary.binding_summary.required_binding_count
            resolved_binding_count = [int]$systemFormationSummary.binding_summary.resolved_binding_count
            unresolved_binding_count = [int]$systemFormationSummary.binding_summary.unresolved_binding_count
            resolved_capabilities = if ($null -eq $bindingResultSummary) { @() } else { @($bindingResultSummary.resolved_capabilities) }
            unresolved_capabilities = @($systemFormationSummary.binding_summary.unresolved_capabilities)
        }
    } elseif ($null -ne $bindingResultSummary) {
        [ordered]@{
            required_binding_count = [int]$bindingResultSummary.required_binding_count
            resolved_binding_count = [int]$bindingResultSummary.resolved_binding_count
            unresolved_binding_count = [int]$bindingResultSummary.unresolved_binding_count
            resolved_capabilities = @($bindingResultSummary.resolved_capabilities)
            unresolved_capabilities = @($bindingResultSummary.unresolved_capabilities)
        }
    } else {
        $null
    }
    $bringupSummary = if ($null -ne $systemFormationSummary -and $null -ne $systemFormationSummary.bringup_summary) {
        [ordered]@{
            ordered_node_count = [int]$systemFormationSummary.bringup_summary.ordered_node_count
            blocked_node_count = [int]$systemFormationSummary.bringup_summary.blocked_node_count
            blocked_nodes = @($systemFormationSummary.bringup_summary.blocked_nodes)
            phase_counts = if ($null -eq $bringupOrderSummary) { @{} } else { $bringupOrderSummary.phase_counts }
        }
    } elseif ($null -ne $bringupOrderSummary) {
        [ordered]@{
            ordered_node_count = [int]$bringupOrderSummary.ordered_node_count
            blocked_node_count = [int]$bringupOrderSummary.blocked_node_count
            blocked_nodes = @($bringupOrderSummary.blocked_nodes)
            phase_counts = $bringupOrderSummary.phase_counts
        }
    } else {
        $null
    }

    return [pscustomobject][ordered]@{
        subject = New-ArtifactRootSystemCompilerSubjectProjection -Report $report
        stages = [ordered]@{
            system_input = $systemInputSummary
            binding_result = $bindingResultSummary
            bringup_order = $bringupOrderSummary
            system_formation = $systemFormationSummary
        }
        projections = [ordered]@{
            formation_basis = $formationBasis
            binding_summary = $bindingSummary
            bringup_summary = $bringupSummary
        }
        totals = [ordered]@{
            declared_fact_count = if ($null -eq $systemInputSummary) { 0 } else { [int]@($systemInputSummary.declared_facts).Count }
            declared_contract_count = if ($null -eq $systemInputSummary) { 0 } else { [int]@($systemInputSummary.declared_contract_entries).Count }
            subject_fact_count = if ($null -eq $systemInputSummary) { 0 } else { [int]@($systemInputSummary.subject_facts).Count }
            required_binding_count = if ($null -eq $bindingResultSummary) { 0 } else { [int]$bindingResultSummary.required_binding_count }
            resolved_binding_count = if ($null -eq $bindingResultSummary) { 0 } else { [int]$bindingResultSummary.resolved_binding_count }
            unresolved_binding_count = if ($null -eq $bindingResultSummary) { 0 } else { [int]$bindingResultSummary.unresolved_binding_count }
            ordered_node_count = if ($null -eq $bringupOrderSummary) { 0 } else { [int]$bringupOrderSummary.ordered_node_count }
            blocked_node_count = if ($null -eq $bringupOrderSummary) { 0 } else { [int]$bringupOrderSummary.blocked_node_count }
            blocker_count = if ($null -eq $systemFormationSummary) { 0 } else { [int]$systemFormationSummary.blocker_count }
        }
        status = if ($null -eq $systemFormationSummary) { $null } else { [string]$systemFormationSummary.status }
        blockers = if ($null -eq $systemFormationSummary) { @() } else { @($systemFormationSummary.blockers) }
    }
}

function Convert-ArtifactRootSystemCompilerCaseProjectionToSummary {
    param(
        $CaseProjection
    )

    if ($null -eq $CaseProjection) {
        return $null
    }

    $subject = $CaseProjection.subject
    $systemInputSummary = $CaseProjection.stages.system_input
    $bindingResultSummary = $CaseProjection.stages.binding_result
    $bringupOrderSummary = $CaseProjection.stages.bringup_order
    $totals = $CaseProjection.totals

    return [pscustomobject][ordered]@{
        case = [string]$subject.case
        profile = [string]$subject.profile
        board = [string]$subject.board
        active_facets = @($subject.active_facets)
        case_kind = if ($null -eq $systemInputSummary) { $null } else { [string]$systemInputSummary.case_kind }
        source = if ($null -eq $systemInputSummary) { $null } else { [string]$systemInputSummary.source }
        build_target = if ($null -eq $systemInputSummary) { $null } else { [string]$systemInputSummary.build_target }
        export_target = if ($null -eq $systemInputSummary) { $null } else { [string]$systemInputSummary.export_target }
        resolved_profile = if ($null -eq $systemInputSummary) { $null } else { [string]$systemInputSummary.resolved_profile }
        resolved_profile_source = if ($null -eq $systemInputSummary) { $null } else { [string]$systemInputSummary.resolved_profile_source }
        resolved_board = if ($null -eq $systemInputSummary) { $null } else { [string]$systemInputSummary.resolved_board }
        resolved_board_source = if ($null -eq $systemInputSummary) { $null } else { [string]$systemInputSummary.resolved_board_source }
        resolved_active_facets = if ($null -eq $systemInputSummary) { @() } else { @($systemInputSummary.resolved_active_facets) }
        resolved_active_facets_source = if ($null -eq $systemInputSummary) { $null } else { [string]$systemInputSummary.resolved_active_facets_source }
        formation_basis = $CaseProjection.projections.formation_basis
        binding_summary = $CaseProjection.projections.binding_summary
        bringup_summary = $CaseProjection.projections.bringup_summary
        declared_fact_count = [int]$totals.declared_fact_count
        declared_contract_count = [int]$totals.declared_contract_count
        subject_fact_count = [int]$totals.subject_fact_count
        required_binding_count = [int]$totals.required_binding_count
        resolved_binding_count = [int]$totals.resolved_binding_count
        unresolved_binding_count = [int]$totals.unresolved_binding_count
        unresolved_capabilities = if ($null -eq $bindingResultSummary) { @() } else { @($bindingResultSummary.unresolved_capabilities) }
        ordered_node_count = [int]$totals.ordered_node_count
        blocked_node_count = [int]$totals.blocked_node_count
        blocked_nodes = if ($null -eq $bringupOrderSummary) { @() } else { @($bringupOrderSummary.blocked_nodes) }
        status = [string]$CaseProjection.status
        blocker_count = [int]$totals.blocker_count
        blockers = @($CaseProjection.blockers)
    }
}

function New-ArtifactRootSystemCompilerCaseSummary {
    param(
        $LoadedReport
    )

    return Convert-ArtifactRootSystemCompilerCaseProjectionToSummary -CaseProjection (
        New-ArtifactRootSystemCompilerCaseProjection -LoadedReport $LoadedReport
    )
}

function New-ArtifactRootSystemCompilerBindingReasonMatrixEntry {
    param(
        [string]$ReasonText,
        [object[]]$CaseProjections
    )

    if ([string]::IsNullOrWhiteSpace($ReasonText)) {
        return $null
    }

    $cases = @()
    $capabilities = @()
    $states = @()
    $providerNodes = @()
    $consumerNodes = @()
    foreach ($caseProjection in @($CaseProjections)) {
        $bindingStage = $caseProjection.stages.binding_result
        if ($null -eq $bindingStage) {
            continue
        }

        $matchedEntries = @(
            @($bindingStage.binding_entries) |
                Where-Object { [string]$_.reason -eq $ReasonText }
        )
        if (@($matchedEntries).Count -eq 0) {
            continue
        }

        $caseCapabilities = @(
            @($matchedEntries) |
                ForEach-Object { [string]$_.capability } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        $caseStates = @(
            @($matchedEntries) |
                ForEach-Object { [string]$_.state } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        $caseProviderNodes = @(
            foreach ($entry in @($matchedEntries)) {
                @($entry.provider_nodes)
            }
        ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
        $caseConsumerNodes = @(
            foreach ($entry in @($matchedEntries)) {
                @($entry.consumer_nodes)
            }
        ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique

        $capabilities += @($caseCapabilities)
        $states += @($caseStates)
        $providerNodes += @($caseProviderNodes)
        $consumerNodes += @($caseConsumerNodes)

        $cases += [ordered]@{
            case = [string]$caseProjection.subject.case
            profile = [string]$caseProjection.subject.profile
            board = [string]$caseProjection.subject.board
            capabilities = @($caseCapabilities)
            states = @($caseStates)
            provider_nodes = @($caseProviderNodes)
            consumer_nodes = @($caseConsumerNodes)
        }
    }

    return [ordered]@{
        reason = $ReasonText
        case_count = @($cases).Count
        capability_count = @($capabilities | Sort-Object -Unique).Count
        capabilities = @($capabilities | Sort-Object -Unique)
        states = @($states | Sort-Object -Unique)
        provider_nodes = @($providerNodes | Sort-Object -Unique)
        consumer_nodes = @($consumerNodes | Sort-Object -Unique)
        cases = @($cases | Sort-Object case)
    }
}

function New-ArtifactRootSystemCompilerBringupPhaseMatrixEntry {
    param(
        [string]$PhaseName,
        [object[]]$CaseProjections
    )

    if ([string]::IsNullOrWhiteSpace($PhaseName)) {
        return $null
    }

    $cases = @()
    $nodes = @()
    $states = @()
    $kinds = @()
    foreach ($caseProjection in @($CaseProjections)) {
        $bringupStage = $caseProjection.stages.bringup_order
        if ($null -eq $bringupStage) {
            continue
        }

        $matchedEntries = @(
            @($bringupStage.entries) |
                Where-Object { [string]$_.phase -eq $PhaseName }
        )
        if (@($matchedEntries).Count -eq 0) {
            continue
        }

        $caseNodes = @(
            @($matchedEntries) |
                ForEach-Object { [string]$_.node } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        $caseStates = @(
            @($matchedEntries) |
                ForEach-Object { [string]$_.state } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        $caseKinds = @(
            @($matchedEntries) |
                ForEach-Object { [string]$_.kind } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )

        $nodes += @($caseNodes)
        $states += @($caseStates)
        $kinds += @($caseKinds)

        $cases += [ordered]@{
            case = [string]$caseProjection.subject.case
            profile = [string]$caseProjection.subject.profile
            board = [string]$caseProjection.subject.board
            nodes = @($caseNodes)
            states = @($caseStates)
            kinds = @($caseKinds)
        }
    }

    return [ordered]@{
        phase = $PhaseName
        case_count = @($cases).Count
        node_count = @($nodes | Sort-Object -Unique).Count
        nodes = @($nodes | Sort-Object -Unique)
        states = @($states | Sort-Object -Unique)
        kinds = @($kinds | Sort-Object -Unique)
        cases = @($cases | Sort-Object case)
    }
}

function New-ArtifactRootSystemCompilerBringupDependencyMatrixEntry {
    param(
        [string]$NodeName,
        [object[]]$CaseProjections
    )

    if ([string]::IsNullOrWhiteSpace($NodeName)) {
        return $null
    }

    $cases = @()
    $dependentNodes = @()
    $states = @()
    $phases = @()
    foreach ($caseProjection in @($CaseProjections)) {
        $bringupStage = $caseProjection.stages.bringup_order
        if ($null -eq $bringupStage) {
            continue
        }

        $matchedEntries = @(
            @($bringupStage.entries) |
                Where-Object { @($_.dependency_nodes) -contains $NodeName }
        )
        if (@($matchedEntries).Count -eq 0) {
            continue
        }

        $caseDependentNodes = @(
            @($matchedEntries) |
                ForEach-Object { [string]$_.node } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        $caseStates = @(
            @($matchedEntries) |
                ForEach-Object { [string]$_.state } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        $casePhases = @(
            @($matchedEntries) |
                ForEach-Object { [string]$_.phase } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )

        $dependentNodes += @($caseDependentNodes)
        $states += @($caseStates)
        $phases += @($casePhases)

        $cases += [ordered]@{
            case = [string]$caseProjection.subject.case
            profile = [string]$caseProjection.subject.profile
            board = [string]$caseProjection.subject.board
            dependent_nodes = @($caseDependentNodes)
            states = @($caseStates)
            phases = @($casePhases)
        }
    }

    return [ordered]@{
        node = $NodeName
        case_count = @($cases).Count
        dependent_node_count = @($dependentNodes | Sort-Object -Unique).Count
        dependent_nodes = @($dependentNodes | Sort-Object -Unique)
        states = @($states | Sort-Object -Unique)
        phases = @($phases | Sort-Object -Unique)
        cases = @($cases | Sort-Object case)
    }
}

function New-ArtifactRootSystemCompilerAggregateProjection {
    param(
        [object[]]$LoadedReports
    )

    $caseProjections = @(
        @($LoadedReports) |
            ForEach-Object { New-ArtifactRootSystemCompilerCaseProjection -LoadedReport $_ } |
            Where-Object { $null -ne $_ } |
            Sort-Object { [string]$_.subject.case }
    )

    if (@($caseProjections).Count -eq 0) {
        return $null
    }

    $caseSummaries = @(
        @($caseProjections) |
            ForEach-Object { Convert-ArtifactRootSystemCompilerCaseProjectionToSummary -CaseProjection $_ } |
            Where-Object { $null -ne $_ } |
            Sort-Object case
    )

    $statusCounts = @{}
    foreach ($caseSummary in @($caseSummaries)) {
        Add-AggregatedCountMapEntry -Counts $statusCounts -Name (Get-ArtifactRootMatrixName -Value ([string]$caseSummary.status))
    }

    $formedCases = @(
        @($caseSummaries) |
            Where-Object { [string]$_.status -eq 'formed' } |
            ForEach-Object { [string]$_.case } |
            Sort-Object
    )
    $blockedCases = @(
        @($caseSummaries) |
            Where-Object { [string]$_.status -eq 'blocked' } |
            ForEach-Object { [string]$_.case } |
            Sort-Object
    )
    $caseKinds = @(
        foreach ($caseSummary in @($caseSummaries)) {
            Get-ArtifactRootMatrixName -Value ([string]$caseSummary.case_kind)
        }
    ) | Sort-Object -Unique
    $resolvedProfiles = @(
        foreach ($caseSummary in @($caseSummaries)) {
            Get-ArtifactRootMatrixName -Value ([string]$caseSummary.resolved_profile)
        }
    ) | Sort-Object -Unique
    $resolvedBoards = @(
        foreach ($caseSummary in @($caseSummaries)) {
            Get-ArtifactRootMatrixName -Value ([string]$caseSummary.resolved_board)
        }
    ) | Sort-Object -Unique
    $resolvedActiveFacets = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @(Get-ArtifactRootMatrixNames -Values @($caseSummary.resolved_active_facets))
        }
    ) | Sort-Object -Unique
    $unresolvedCapabilities = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.unresolved_capabilities)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $blockedNodes = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.blocked_nodes)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $blockerKeys = @(
        foreach ($caseSummary in @($caseSummaries)) {
            foreach ($blocker in @($caseSummary.blockers)) {
                $kind = [string]$blocker.kind
                $name = [string]$blocker.name
                if ([string]::IsNullOrWhiteSpace($kind) -or [string]::IsNullOrWhiteSpace($name)) {
                    continue
                }

                "${kind}|${name}"
            }
        }
    ) | Sort-Object -Unique
    $blockerReasons = @(
        foreach ($caseSummary in @($caseSummaries)) {
            foreach ($blocker in @($caseSummary.blockers)) {
                $reasonText = [string]$blocker.reason
                if (-not [string]::IsNullOrWhiteSpace($reasonText)) {
                    $reasonText
                }
            }
        }
    ) | Sort-Object -Unique
    $blockerMissingRequires = @(
        foreach ($caseSummary in @($caseSummaries)) {
            foreach ($blocker in @($caseSummary.blockers)) {
                @($blocker.missing_requires)
            }
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $blockerDependsOn = @(
        foreach ($caseSummary in @($caseSummaries)) {
            foreach ($blocker in @($caseSummary.blockers)) {
                @($blocker.dependency_nodes)
            }
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $bindingReasons = @(
        foreach ($caseProjection in @($caseProjections)) {
            foreach ($bindingEntry in @($caseProjection.stages.binding_result.binding_entries)) {
                $reasonText = [string]$bindingEntry.reason
                if (-not [string]::IsNullOrWhiteSpace($reasonText)) {
                    $reasonText
                }
            }
        }
    ) | Sort-Object -Unique
    $bringupPhases = @(
        foreach ($caseProjection in @($caseProjections)) {
            foreach ($bringupEntry in @($caseProjection.stages.bringup_order.entries)) {
                $phaseName = [string]$bringupEntry.phase
                if (-not [string]::IsNullOrWhiteSpace($phaseName)) {
                    $phaseName
                }
            }
        }
    ) | Sort-Object -Unique
    $bringupDependencyNodes = @(
        foreach ($caseProjection in @($caseProjections)) {
            foreach ($bringupEntry in @($caseProjection.stages.bringup_order.entries)) {
                @($bringupEntry.dependency_nodes)
            }
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique

    $bindingReasonMatrix = @(
        foreach ($reasonText in @($bindingReasons)) {
            New-ArtifactRootSystemCompilerBindingReasonMatrixEntry -ReasonText $reasonText -CaseProjections $caseProjections
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object reason
    $bringupPhaseMatrix = @(
        foreach ($phaseName in @($bringupPhases)) {
            New-ArtifactRootSystemCompilerBringupPhaseMatrixEntry -PhaseName $phaseName -CaseProjections $caseProjections
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object phase
    $bringupDependencyMatrix = @(
        foreach ($nodeName in @($bringupDependencyNodes)) {
            New-ArtifactRootSystemCompilerBringupDependencyMatrixEntry -NodeName $nodeName -CaseProjections $caseProjections
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object node

    return [pscustomobject][ordered]@{
        case_projections = @($caseProjections)
        case_summaries = @($caseSummaries)
        status_counts = ConvertTo-AggregatedOrderedCountMap -Counts $statusCounts
        formed_cases = @($formedCases)
        blocked_cases = @($blockedCases)
        totals = [ordered]@{
            declared_fact_count = [int](@($caseSummaries | Measure-Object -Property declared_fact_count -Sum).Sum)
            declared_contract_count = [int](@($caseSummaries | Measure-Object -Property declared_contract_count -Sum).Sum)
            subject_fact_count = [int](@($caseSummaries | Measure-Object -Property subject_fact_count -Sum).Sum)
            required_binding_count = [int](@($caseSummaries | Measure-Object -Property required_binding_count -Sum).Sum)
            resolved_binding_count = [int](@($caseSummaries | Measure-Object -Property resolved_binding_count -Sum).Sum)
            unresolved_binding_count = [int](@($caseSummaries | Measure-Object -Property unresolved_binding_count -Sum).Sum)
            ordered_node_count = [int](@($caseSummaries | Measure-Object -Property ordered_node_count -Sum).Sum)
            blocked_node_count = [int](@($caseSummaries | Measure-Object -Property blocked_node_count -Sum).Sum)
            blocker_count = [int](@($caseSummaries | Measure-Object -Property blocker_count -Sum).Sum)
        }
        universes = [ordered]@{
            case_kinds = @($caseKinds)
            resolved_profiles = @($resolvedProfiles)
            resolved_boards = @($resolvedBoards)
            resolved_active_facets = @($resolvedActiveFacets)
            unresolved_capabilities = @($unresolvedCapabilities)
            blocked_nodes = @($blockedNodes)
            blocker_keys = @($blockerKeys)
            blocker_reasons = @($blockerReasons)
            blocker_missing_requires = @($blockerMissingRequires)
            blocker_depends_on = @($blockerDependsOn)
            binding_reasons = @($bindingReasons)
            bringup_phases = @($bringupPhases)
            bringup_dependency_nodes = @($bringupDependencyNodes)
        }
        matrices = [ordered]@{
            binding_reason_matrix = @($bindingReasonMatrix)
            bringup_phase_matrix = @($bringupPhaseMatrix)
            bringup_dependency_matrix = @($bringupDependencyMatrix)
        }
    }
}

function New-ArtifactRootSystemCompilerResultMapFieldRelation {
    param(
        [string]$RootField,
        [string]$BlockFieldPath,
        [string]$BlockRelation = 'none',
        [string]$SummaryFieldPath,
        [string]$SummaryRelation = 'none'
    )

    return [ordered]@{
        root_field = [string]$RootField
        block_field_path = if ([string]::IsNullOrWhiteSpace($BlockFieldPath)) { $null } else { [string]$BlockFieldPath }
        block_relation = [string]$BlockRelation
        summary_field_path = if ([string]::IsNullOrWhiteSpace($SummaryFieldPath)) { $null } else { [string]$SummaryFieldPath }
        summary_relation = [string]$SummaryRelation
    }
}

function New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate {
    param(
        [string]$Stage,
        [string]$FieldPath,
        [string]$Relation = 'same_field'
    )

    return [ordered]@{
        stage = [string]$Stage
        field_path = [string]$FieldPath
        relation = [string]$Relation
    }
}

function New-ArtifactRootSystemCompilerCaseProjectionFieldRelation {
    param(
        [string]$ProjectionField,
        [object[]]$SourceCandidates
    )

    return [ordered]@{
        projection_field = [string]$ProjectionField
        source_candidates = @($SourceCandidates)
    }
}

function New-ArtifactRootSystemCompilerResultMap {
    param(
        [switch]$Comparison
    )

    if ($Comparison) {
        return [ordered]@{
            kind = 'system_compiler_result_map/v0'
            mode = 'comparison'
            input_bridge = [ordered]@{
                summary_field = 'comparison.system_input_summary'
                root_fields = @(
                    'system_spec_change_matrix'
                    'resolved_input_change_matrix'
                    'declared_fact_change_matrix'
                    'declared_contract_change_matrix'
                    'subject_fact_change_matrix'
                )
                field_relations = @(
                    New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'system_spec_change_matrix' -SummaryFieldPath 'system_spec_change_matrix' -SummaryRelation 'same_field'
                    New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'resolved_input_change_matrix' -SummaryFieldPath 'resolved_input_change_matrix' -SummaryRelation 'same_field'
                    New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'declared_fact_change_matrix' -SummaryFieldPath 'declared_fact_change_matrix' -SummaryRelation 'same_field'
                    New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'declared_contract_change_matrix' -SummaryFieldPath 'declared_contract_change_matrix' -SummaryRelation 'same_field'
                    New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'subject_fact_change_matrix' -SummaryFieldPath 'subject_fact_change_matrix' -SummaryRelation 'same_field'
                )
            }
            case_projection_fields = [ordered]@{
                formation = 'cases[*].formation_basis_changes'
                binding = 'cases[*].binding_summary_changes'
                bringup = 'cases[*].bringup_summary_changes'
            }
            case_projection_field_relations = [ordered]@{
                formation = @(
                    New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'system_spec_changes' -SourceCandidates @(
                        New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'system_input' -FieldPath 'system_spec_changes'
                    )
                    New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'resolved_input_changes' -SourceCandidates @(
                        New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'system_input' -FieldPath 'resolved_input_changes'
                    )
                    New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'declared_fact_changes' -SourceCandidates @(
                        New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'system_input' -FieldPath 'declared_fact_changes'
                    )
                    New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'declared_contract_changes' -SourceCandidates @(
                        New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'system_input' -FieldPath 'declared_contract_changes'
                    )
                    New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'subject_fact_changes' -SourceCandidates @(
                        New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'system_input' -FieldPath 'subject_fact_changes'
                    )
                )
                binding = @(
                    New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'summary_changes' -SourceCandidates @(
                        New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'binding_result' -FieldPath 'summary_changes'
                    )
                    New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'binding_change_count' -SourceCandidates @(
                        New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'binding_result' -FieldPath 'binding_change_count'
                    )
                    New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'capabilities_changed' -SourceCandidates @(
                        New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'binding_result' -FieldPath 'capabilities_changed'
                    )
                    New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'resolved_capability_changes' -SourceCandidates @(
                        New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'binding_result' -FieldPath 'resolved_capability_changes'
                    )
                    New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'unresolved_capability_changes' -SourceCandidates @(
                        New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'binding_result' -FieldPath 'unresolved_capability_changes'
                    )
                )
                bringup = @(
                    New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'summary_changes' -SourceCandidates @(
                        New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'bringup_order' -FieldPath 'summary_changes'
                    )
                    New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'entry_change_count' -SourceCandidates @(
                        New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'bringup_order' -FieldPath 'entry_change_count'
                    )
                    New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'nodes_changed' -SourceCandidates @(
                        New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'bringup_order' -FieldPath 'nodes_changed'
                    )
                    New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'blocked_node_changes' -SourceCandidates @(
                        New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'bringup_order' -FieldPath 'blocked_node_changes'
                    )
                )
            }
            stage_blocks = @(
                [ordered]@{
                    stage = 'formation'
                    block_field = 'formation_drift'
                    summary_field = 'comparison.system_formation_summary'
                    root_fields = @(
                        'status_change_matrix'
                        'declared_fact_change_matrix'
                        'declared_contract_change_matrix'
                        'subject_fact_change_matrix'
                        'unresolved_capability_change_matrix'
                        'blocked_node_change_matrix'
                        'blocker_change_matrix'
                        'blocker_reason_change_matrix'
                        'blocker_missing_requires_change_matrix'
                        'blocker_depends_on_change_matrix'
                    )
                    field_relations = @(
                        New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'status_change_matrix' -BlockFieldPath 'status_change_matrix' -BlockRelation 'same_field' -SummaryFieldPath 'status_change_matrix' -SummaryRelation 'same_field'
                        New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'declared_fact_change_matrix' -BlockFieldPath 'declared_fact_change_matrix' -BlockRelation 'same_field'
                        New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'declared_contract_change_matrix' -BlockFieldPath 'declared_contract_change_matrix' -BlockRelation 'same_field'
                        New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'subject_fact_change_matrix' -BlockFieldPath 'subject_fact_change_matrix' -BlockRelation 'same_field'
                        New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'unresolved_capability_change_matrix' -BlockFieldPath 'unresolved_capability_change_matrix' -BlockRelation 'same_field' -SummaryFieldPath 'unresolved_capability_change_matrix' -SummaryRelation 'same_field'
                        New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'blocked_node_change_matrix' -BlockFieldPath 'blocked_node_change_matrix' -BlockRelation 'same_field' -SummaryFieldPath 'blocked_node_change_matrix' -SummaryRelation 'same_field'
                        New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'blocker_change_matrix' -BlockFieldPath 'blocker_change_matrix' -BlockRelation 'same_field' -SummaryFieldPath 'blocker_change_matrix' -SummaryRelation 'same_field'
                        New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'blocker_reason_change_matrix' -BlockFieldPath 'blocker_reason_change_matrix' -BlockRelation 'same_field'
                        New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'blocker_missing_requires_change_matrix' -BlockFieldPath 'blocker_missing_requires_change_matrix' -BlockRelation 'same_field'
                        New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'blocker_depends_on_change_matrix' -BlockFieldPath 'blocker_depends_on_change_matrix' -BlockRelation 'same_field'
                    )
                }
                [ordered]@{
                    stage = 'binding'
                    block_field = 'binding_drift'
                    summary_field = 'comparison.binding_result_summary'
                    root_fields = @(
                        'binding_reason_change_matrix'
                        'resolved_capability_change_matrix'
                        'unresolved_capability_change_matrix'
                    )
                    field_relations = @(
                        New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'binding_reason_change_matrix' -BlockFieldPath 'reason_change_matrix' -BlockRelation 'field_alias'
                        New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'resolved_capability_change_matrix' -BlockFieldPath 'resolved_capability_change_matrix' -BlockRelation 'same_field' -SummaryFieldPath 'resolved_capability_change_matrix' -SummaryRelation 'same_field'
                        New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'unresolved_capability_change_matrix' -BlockFieldPath 'unresolved_capability_change_matrix' -BlockRelation 'same_field' -SummaryFieldPath 'unresolved_capability_change_matrix' -SummaryRelation 'same_field'
                    )
                }
                [ordered]@{
                    stage = 'bringup'
                    block_field = 'bringup_drift'
                    summary_field = 'comparison.bringup_order_summary'
                    root_fields = @(
                        'bringup_phase_change_matrix'
                        'bringup_dependency_change_matrix'
                        'blocked_node_change_matrix'
                    )
                    field_relations = @(
                        New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'bringup_phase_change_matrix' -BlockFieldPath 'phase_change_matrix' -BlockRelation 'field_alias'
                        New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'bringup_dependency_change_matrix' -BlockFieldPath 'dependency_change_matrix' -BlockRelation 'field_alias'
                        New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'blocked_node_change_matrix' -BlockFieldPath 'blocked_node_change_matrix' -BlockRelation 'same_field' -SummaryFieldPath 'blocked_node_change_matrix' -SummaryRelation 'same_field'
                    )
                }
            )
        }
    }

    return [ordered]@{
        kind = 'system_compiler_result_map/v0'
        mode = 'summary'
        input_bridge = [ordered]@{
            summary_field = 'system_input_summary'
            root_fields = @(
                'case_kind_matrix'
                'resolved_profile_matrix'
                'resolved_board_matrix'
                'resolved_active_facet_matrix'
            )
            field_relations = @(
                New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'case_kind_matrix' -SummaryFieldPath 'case_kind_matrix' -SummaryRelation 'same_field'
                New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'resolved_profile_matrix' -SummaryFieldPath 'resolved_profile_matrix' -SummaryRelation 'same_field'
                New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'resolved_board_matrix' -SummaryFieldPath 'resolved_board_matrix' -SummaryRelation 'same_field'
                New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'resolved_active_facet_matrix' -SummaryFieldPath 'resolved_active_facet_matrix' -SummaryRelation 'same_field'
            )
        }
        case_projection_fields = [ordered]@{
            formation = 'cases[*].formation_basis'
            binding = 'cases[*].binding_summary'
            bringup = 'cases[*].bringup_summary'
        }
        case_projection_field_relations = [ordered]@{
            formation = @(
                New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'case_kind' -SourceCandidates @(
                    New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'system_formation' -FieldPath 'case_kind'
                    New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'system_input' -FieldPath 'case_kind'
                )
                New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'declared_fact_count' -SourceCandidates @(
                    New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'system_formation' -FieldPath 'declared_fact_count'
                    New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'system_input' -FieldPath 'declared_fact_count'
                )
                New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'declared_contract_count' -SourceCandidates @(
                    New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'system_formation' -FieldPath 'declared_contract_count'
                    New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'system_input' -FieldPath 'declared_contract_count'
                )
                New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'subject_fact_count' -SourceCandidates @(
                    New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'system_formation' -FieldPath 'subject_fact_count'
                    New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'system_input' -FieldPath 'subject_fact_count'
                )
            )
            binding = @(
                New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'required_binding_count' -SourceCandidates @(
                    New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'system_formation' -FieldPath 'required_binding_count'
                    New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'binding_result' -FieldPath 'required_binding_count'
                )
                New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'resolved_binding_count' -SourceCandidates @(
                    New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'system_formation' -FieldPath 'resolved_binding_count'
                    New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'binding_result' -FieldPath 'resolved_binding_count'
                )
                New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'unresolved_binding_count' -SourceCandidates @(
                    New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'system_formation' -FieldPath 'unresolved_binding_count'
                    New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'binding_result' -FieldPath 'unresolved_binding_count'
                )
                New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'resolved_capabilities' -SourceCandidates @(
                    New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'binding_result' -FieldPath 'resolved_capabilities'
                )
                New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'unresolved_capabilities' -SourceCandidates @(
                    New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'system_formation' -FieldPath 'unresolved_capabilities'
                    New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'binding_result' -FieldPath 'unresolved_capabilities'
                )
            )
            bringup = @(
                New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'ordered_node_count' -SourceCandidates @(
                    New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'system_formation' -FieldPath 'ordered_node_count'
                    New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'bringup_order' -FieldPath 'ordered_node_count'
                )
                New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'blocked_node_count' -SourceCandidates @(
                    New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'system_formation' -FieldPath 'blocked_node_count'
                    New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'bringup_order' -FieldPath 'blocked_node_count'
                )
                New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'blocked_nodes' -SourceCandidates @(
                    New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'system_formation' -FieldPath 'blocked_nodes'
                    New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'bringup_order' -FieldPath 'blocked_nodes'
                )
                New-ArtifactRootSystemCompilerCaseProjectionFieldRelation -ProjectionField 'phase_counts' -SourceCandidates @(
                    New-ArtifactRootSystemCompilerCaseProjectionSourceCandidate -Stage 'bringup_order' -FieldPath 'phase_counts'
                )
            )
        }
        stage_blocks = @(
            [ordered]@{
                stage = 'formation'
                block_field = 'formation_basis'
                summary_field = 'system_formation_summary'
                root_fields = @(
                    'status_counts'
                    'formed_case_count'
                    'blocked_case_count'
                    'unresolved_capability_matrix'
                    'blocked_node_matrix'
                    'blocker_matrix'
                    'blocker_reason_matrix'
                    'blocker_missing_requires_matrix'
                    'blocker_depends_on_matrix'
                )
                field_relations = @(
                    New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'status_counts' -BlockFieldPath 'status_counts' -BlockRelation 'same_field' -SummaryFieldPath 'status_counts' -SummaryRelation 'same_field'
                    New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'formed_case_count' -BlockFieldPath 'formed_case_count' -BlockRelation 'same_field' -SummaryFieldPath 'formed_case_count' -SummaryRelation 'same_field'
                    New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'blocked_case_count' -BlockFieldPath 'blocked_case_count' -BlockRelation 'same_field' -SummaryFieldPath 'blocked_case_count' -SummaryRelation 'same_field'
                    New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'unresolved_capability_matrix' -BlockFieldPath 'unresolved_capability_matrix' -BlockRelation 'same_field' -SummaryFieldPath 'unresolved_capability_matrix' -SummaryRelation 'same_field'
                    New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'blocked_node_matrix' -BlockFieldPath 'blocked_node_matrix' -BlockRelation 'same_field' -SummaryFieldPath 'blocked_node_matrix' -SummaryRelation 'same_field'
                    New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'blocker_matrix' -BlockFieldPath 'blocker_matrix' -BlockRelation 'same_field' -SummaryFieldPath 'blocker_matrix' -SummaryRelation 'same_field'
                    New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'blocker_reason_matrix' -BlockFieldPath 'blocker_reason_matrix' -BlockRelation 'same_field'
                    New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'blocker_missing_requires_matrix' -BlockFieldPath 'blocker_missing_requires_matrix' -BlockRelation 'same_field'
                    New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'blocker_depends_on_matrix' -BlockFieldPath 'blocker_depends_on_matrix' -BlockRelation 'same_field'
                )
            }
            [ordered]@{
                stage = 'binding'
                block_field = 'binding_basis'
                summary_field = 'binding_result_summary'
                root_fields = @(
                    'binding_reason_matrix'
                    'unresolved_capability_matrix'
                )
                field_relations = @(
                    New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'binding_reason_matrix' -BlockFieldPath 'reason_matrix' -BlockRelation 'field_alias'
                    New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'unresolved_capability_matrix' -BlockFieldPath 'unresolved_capability_matrix' -BlockRelation 'same_field' -SummaryFieldPath 'unresolved_capability_matrix' -SummaryRelation 'same_field'
                )
            }
            [ordered]@{
                stage = 'bringup'
                block_field = 'bringup_basis'
                summary_field = 'bringup_order_summary'
                root_fields = @(
                    'bringup_phase_matrix'
                    'bringup_dependency_matrix'
                    'blocked_node_matrix'
                )
                field_relations = @(
                    New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'bringup_phase_matrix' -BlockFieldPath 'phase_matrix' -BlockRelation 'field_alias'
                    New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'bringup_dependency_matrix' -BlockFieldPath 'dependency_matrix' -BlockRelation 'field_alias'
                    New-ArtifactRootSystemCompilerResultMapFieldRelation -RootField 'blocked_node_matrix' -BlockFieldPath 'blocked_node_matrix' -BlockRelation 'same_field' -SummaryFieldPath 'blocked_node_matrix' -SummaryRelation 'same_field'
                )
            }
        )
    }
}

function New-ArtifactRootSystemCompilerSummaryResult {
    param(
        [object[]]$LoadedReports
    )

    $aggregateProjection = New-ArtifactRootSystemCompilerAggregateProjection -LoadedReports $LoadedReports
    if ($null -eq $aggregateProjection) {
        return $null
    }

    $caseSummaries = @($aggregateProjection.case_summaries)
    $caseKinds = @($aggregateProjection.universes.case_kinds)
    $resolvedProfiles = @($aggregateProjection.universes.resolved_profiles)
    $resolvedBoards = @($aggregateProjection.universes.resolved_boards)
    $resolvedActiveFacets = @($aggregateProjection.universes.resolved_active_facets)
    $unresolvedCapabilities = @($aggregateProjection.universes.unresolved_capabilities)
    $blockedNodes = @($aggregateProjection.universes.blocked_nodes)
    $blockerKeys = @($aggregateProjection.universes.blocker_keys)
    $blockerReasons = @($aggregateProjection.universes.blocker_reasons)
    $blockerMissingRequires = @($aggregateProjection.universes.blocker_missing_requires)
    $blockerDependsOn = @($aggregateProjection.universes.blocker_depends_on)

    $caseKindMatrix = @(
        foreach ($caseKind in @($caseKinds)) {
            New-ArtifactRootSystemInputValueMatrixEntry -ValueName $caseKind -CaseSummaries $caseSummaries -PropertyName 'case_kind' -FieldName 'case_kind'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object case_kind
    $resolvedProfileMatrix = @(
        foreach ($profileName in @($resolvedProfiles)) {
            New-ArtifactRootSystemInputValueMatrixEntry -ValueName $profileName -CaseSummaries $caseSummaries -PropertyName 'resolved_profile' -FieldName 'profile'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object profile
    $resolvedBoardMatrix = @(
        foreach ($boardName in @($resolvedBoards)) {
            New-ArtifactRootSystemInputValueMatrixEntry -ValueName $boardName -CaseSummaries $caseSummaries -PropertyName 'resolved_board' -FieldName 'board'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object board
    $resolvedActiveFacetMatrix = @(
        foreach ($facetName in @($resolvedActiveFacets)) {
            New-ArtifactRootSystemInputArrayMatrixEntry -Name $facetName -CaseSummaries $caseSummaries -PropertyName 'resolved_active_facets' -FieldName 'facet'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object facet
    $unresolvedCapabilityMatrix = @(
        foreach ($capabilityName in @($unresolvedCapabilities)) {
            New-ArtifactRootSystemFormationNamedMatrixEntry -Name $capabilityName -CaseSummaries $caseSummaries -PropertyName 'unresolved_capabilities' -FieldName 'capability'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object capability
    $blockedNodeMatrix = @(
        foreach ($nodeName in @($blockedNodes)) {
            New-ArtifactRootSystemFormationNamedMatrixEntry -Name $nodeName -CaseSummaries $caseSummaries -PropertyName 'blocked_nodes' -FieldName 'node'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object node
    $blockerMatrix = @(
        foreach ($blockerKey in @($blockerKeys)) {
            $parts = [string]$blockerKey -split '\|', 2
            if (@($parts).Count -ne 2) {
                continue
            }

            New-ArtifactRootSystemFormationBlockerMatrixEntry -Kind ([string]$parts[0]) -Name ([string]$parts[1]) -CaseSummaries $caseSummaries
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object kind, name
    $blockerReasonMatrix = @(
        foreach ($reasonText in @($blockerReasons)) {
            New-ArtifactRootSystemFormationBlockerReasonMatrixEntry -ReasonText $reasonText -CaseSummaries $caseSummaries
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object reason
    $blockerMissingRequiresMatrix = @(
        foreach ($requireName in @($blockerMissingRequires)) {
            New-ArtifactRootSystemFormationBlockerDetailMatrixEntry -DetailName $requireName -CaseSummaries $caseSummaries -CollectionName 'missing_requires' -FieldName 'require'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object require
    $blockerDependsOnMatrix = @(
        foreach ($nodeName in @($blockerDependsOn)) {
            New-ArtifactRootSystemFormationBlockerDetailMatrixEntry -DetailName $nodeName -CaseSummaries $caseSummaries -CollectionName 'depends_on' -FieldName 'node'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object node
    $formationBasis = [ordered]@{
        case_count = @($caseSummaries).Count
        status_counts = $aggregateProjection.status_counts
        formed_case_count = @($aggregateProjection.formed_cases).Count
        blocked_case_count = @($aggregateProjection.blocked_cases).Count
        formed_cases = @($aggregateProjection.formed_cases)
        blocked_cases = @($aggregateProjection.blocked_cases)
        totals = [ordered]@{
            declared_fact_count = [int]$aggregateProjection.totals.declared_fact_count
            declared_contract_count = [int]$aggregateProjection.totals.declared_contract_count
            subject_fact_count = [int]$aggregateProjection.totals.subject_fact_count
            blocker_count = [int]$aggregateProjection.totals.blocker_count
        }
        unresolved_capability_matrix = @($unresolvedCapabilityMatrix)
        blocked_node_matrix = @($blockedNodeMatrix)
        blocker_matrix = @($blockerMatrix)
        blocker_reason_matrix = @($blockerReasonMatrix)
        blocker_missing_requires_matrix = @($blockerMissingRequiresMatrix)
        blocker_depends_on_matrix = @($blockerDependsOnMatrix)
    }
    $bindingBasis = [ordered]@{
        case_count = @($caseSummaries).Count
        totals = [ordered]@{
            required_binding_count = [int]$aggregateProjection.totals.required_binding_count
            resolved_binding_count = [int]$aggregateProjection.totals.resolved_binding_count
            unresolved_binding_count = [int]$aggregateProjection.totals.unresolved_binding_count
        }
        reason_matrix = @($aggregateProjection.matrices.binding_reason_matrix)
        unresolved_capability_matrix = @($unresolvedCapabilityMatrix)
    }
    $bringupBasis = [ordered]@{
        case_count = @($caseSummaries).Count
        totals = [ordered]@{
            ordered_node_count = [int]$aggregateProjection.totals.ordered_node_count
            blocked_node_count = [int]$aggregateProjection.totals.blocked_node_count
        }
        phase_matrix = @($aggregateProjection.matrices.bringup_phase_matrix)
        dependency_matrix = @($aggregateProjection.matrices.bringup_dependency_matrix)
        blocked_node_matrix = @($blockedNodeMatrix)
    }
    $resultMap = New-ArtifactRootSystemCompilerResultMap

    return [ordered]@{
        kind = 'system_compiler_summary/v0'
        mode = 'summary'
        case_count = @($caseSummaries).Count
        status_counts = $aggregateProjection.status_counts
        formed_case_count = @($aggregateProjection.formed_cases).Count
        blocked_case_count = @($aggregateProjection.blocked_cases).Count
        formed_cases = @($aggregateProjection.formed_cases)
        blocked_cases = @($aggregateProjection.blocked_cases)
        totals = $aggregateProjection.totals
        cases = @(
            @($caseSummaries) |
                Select-Object `
                    case,
                    board,
                    profile,
                    active_facets,
                    case_kind,
                    source,
                    build_target,
                    export_target,
                    resolved_profile,
                    resolved_profile_source,
                    resolved_board,
                    resolved_board_source,
                    resolved_active_facets,
                    resolved_active_facets_source,
                    formation_basis,
                    binding_summary,
                    bringup_summary,
                    declared_fact_count,
                    declared_contract_count,
                    subject_fact_count,
                    required_binding_count,
                    resolved_binding_count,
                    unresolved_binding_count,
                    unresolved_capabilities,
                    ordered_node_count,
                    blocked_node_count,
                    blocked_nodes,
                    status,
                    blocker_count,
                    blockers |
                Sort-Object case
        )
        case_kind_matrix = @($caseKindMatrix)
        resolved_profile_matrix = @($resolvedProfileMatrix)
        resolved_board_matrix = @($resolvedBoardMatrix)
        resolved_active_facet_matrix = @($resolvedActiveFacetMatrix)
        unresolved_capability_matrix = @($unresolvedCapabilityMatrix)
        blocked_node_matrix = @($blockedNodeMatrix)
        blocker_matrix = @($blockerMatrix)
        blocker_reason_matrix = @($blockerReasonMatrix)
        blocker_missing_requires_matrix = @($blockerMissingRequiresMatrix)
        blocker_depends_on_matrix = @($blockerDependsOnMatrix)
        binding_reason_matrix = @($aggregateProjection.matrices.binding_reason_matrix)
        bringup_phase_matrix = @($aggregateProjection.matrices.bringup_phase_matrix)
        bringup_dependency_matrix = @($aggregateProjection.matrices.bringup_dependency_matrix)
        formation_basis = $formationBasis
        binding_basis = $bindingBasis
        bringup_basis = $bringupBasis
        result_map = $resultMap
    }
}

function New-ArtifactRootSystemCompilerCompareCaseProjection {
    param(
        $LoadedReport
    )

    if ($null -eq $LoadedReport -or $null -eq $LoadedReport.Data) {
        return $null
    }

    $report = $LoadedReport.Data
    $systemInputComparison = New-ArtifactRootSystemInputCompareCaseSummary -LoadedReport $LoadedReport
    $bindingResultComparison = New-ArtifactRootBindingResultCompareCaseSummary -LoadedReport $LoadedReport
    $bringupOrderComparison = New-ArtifactRootBringupOrderCompareCaseSummary -LoadedReport $LoadedReport
    $systemFormationComparison = New-ArtifactRootSystemFormationCompareCaseSummary -LoadedReport $LoadedReport

    if ($null -eq $systemInputComparison -and
        $null -eq $bindingResultComparison -and
        $null -eq $bringupOrderComparison -and
        $null -eq $systemFormationComparison) {
        return $null
    }

    $changedStages = @()
    $inputChanged = ($null -ne $systemInputComparison -and [bool]$systemInputComparison.changed)
    if ($inputChanged) {
        $changedStages += 'system_input'
    }
    $bindingResultChanged = ($null -ne $bindingResultComparison -and [bool]$bindingResultComparison.changed)
    if ($bindingResultChanged) {
        $changedStages += 'binding_result'
    }
    $bringupOrderChanged = ($null -ne $bringupOrderComparison -and [bool]$bringupOrderComparison.changed)
    if ($bringupOrderChanged) {
        $changedStages += 'bringup_order'
    }
    $systemFormationChanged = ($null -ne $systemFormationComparison -and [bool]$systemFormationComparison.changed)
    if ($systemFormationChanged) {
        $changedStages += 'system_formation'
    }

    $formationBasisChanges = if ($null -eq $systemInputComparison) {
        $null
    } else {
        [ordered]@{
            system_spec_changes = @($systemInputComparison.system_spec_changes)
            resolved_input_changes = @($systemInputComparison.resolved_input_changes)
            declared_fact_changes = [ordered]@{
                added = @($systemInputComparison.declared_fact_changes.added)
                removed = @($systemInputComparison.declared_fact_changes.removed)
            }
            declared_contract_changes = @($systemInputComparison.declared_contract_changes)
            subject_fact_changes = [ordered]@{
                added = @($systemInputComparison.subject_fact_changes.added)
                removed = @($systemInputComparison.subject_fact_changes.removed)
            }
        }
    }
    $bindingSummaryChanges = if ($null -eq $bindingResultComparison) {
        $null
    } else {
        [ordered]@{
            summary_changes = @($bindingResultComparison.summary_changes)
            binding_change_count = [int]$bindingResultComparison.binding_change_count
            capabilities_changed = @($bindingResultComparison.capabilities_changed)
            resolved_capability_changes = [ordered]@{
                added = @($bindingResultComparison.resolved_capability_changes.added)
                removed = @($bindingResultComparison.resolved_capability_changes.removed)
            }
            unresolved_capability_changes = [ordered]@{
                added = @($bindingResultComparison.unresolved_capability_changes.added)
                removed = @($bindingResultComparison.unresolved_capability_changes.removed)
            }
        }
    }
    $bringupSummaryChanges = if ($null -eq $bringupOrderComparison) {
        $null
    } else {
        [ordered]@{
            summary_changes = @($bringupOrderComparison.summary_changes)
            entry_change_count = [int]$bringupOrderComparison.entry_change_count
            nodes_changed = @($bringupOrderComparison.nodes_changed)
            blocked_node_changes = [ordered]@{
                added = @($bringupOrderComparison.blocked_node_changes.added)
                removed = @($bringupOrderComparison.blocked_node_changes.removed)
            }
        }
    }

    return [pscustomobject][ordered]@{
        subject = New-ArtifactRootSystemCompilerSubjectProjection -Report $report
        stages = [ordered]@{
            system_input = $systemInputComparison
            binding_result = $bindingResultComparison
            bringup_order = $bringupOrderComparison
            system_formation = $systemFormationComparison
        }
        change_state = [ordered]@{
            changed = (@($changedStages).Count -gt 0)
            changed_stages = @($changedStages)
            system_input_changed = $inputChanged
            binding_result_changed = $bindingResultChanged
            bringup_order_changed = $bringupOrderChanged
            system_formation_changed = $systemFormationChanged
        }
        status = [ordered]@{
            left = if ($null -eq $systemFormationComparison) { $null } else { [string]$systemFormationComparison.left_status }
            right = if ($null -eq $systemFormationComparison) { $null } else { [string]$systemFormationComparison.right_status }
        }
        projections = [ordered]@{
            formation_basis_changes = $formationBasisChanges
            binding_summary_changes = $bindingSummaryChanges
            bringup_summary_changes = $bringupSummaryChanges
        }
        deltas = [ordered]@{
            system_spec_changes = if ($null -eq $systemInputComparison) { @() } else { @($systemInputComparison.system_spec_changes) }
            resolved_input_changes = if ($null -eq $systemInputComparison) { @() } else { @($systemInputComparison.resolved_input_changes) }
            declared_fact_changes = if ($null -eq $systemInputComparison) {
                [ordered]@{ added = @(); removed = @() }
            } else {
                [ordered]@{
                    added = @($systemInputComparison.declared_fact_changes.added)
                    removed = @($systemInputComparison.declared_fact_changes.removed)
                }
            }
            declared_contract_changes = if ($null -eq $systemInputComparison) { @() } else { @($systemInputComparison.declared_contract_changes) }
            subject_fact_changes = if ($null -eq $systemInputComparison) {
                [ordered]@{ added = @(); removed = @() }
            } else {
                [ordered]@{
                    added = @($systemInputComparison.subject_fact_changes.added)
                    removed = @($systemInputComparison.subject_fact_changes.removed)
                }
            }
            unresolved_capability_changes = if ($null -eq $systemFormationComparison) {
                [ordered]@{ added = @(); removed = @() }
            } else {
                [ordered]@{
                    added = @($systemFormationComparison.unresolved_capability_changes.added)
                    removed = @($systemFormationComparison.unresolved_capability_changes.removed)
                }
            }
            blocked_node_changes = if ($null -eq $systemFormationComparison) {
                [ordered]@{ added = @(); removed = @() }
            } else {
                [ordered]@{
                    added = @($systemFormationComparison.blocked_node_changes.added)
                    removed = @($systemFormationComparison.blocked_node_changes.removed)
                }
            }
            blocker_changes = if ($null -eq $systemFormationComparison) { @() } else { @($systemFormationComparison.blocker_changes) }
        }
    }
}

function Convert-ArtifactRootSystemCompilerCompareCaseProjectionToSummary {
    param(
        $CaseProjection
    )

    if ($null -eq $CaseProjection) {
        return $null
    }

    $subject = $CaseProjection.subject
    $changeState = $CaseProjection.change_state
    $deltas = $CaseProjection.deltas

    return [pscustomobject][ordered]@{
        case = [string]$subject.case
        profile = [string]$subject.profile
        board = [string]$subject.board
        active_facets = @($subject.active_facets)
        changed = [bool]$changeState.changed
        changed_stages = @($changeState.changed_stages)
        system_input_changed = [bool]$changeState.system_input_changed
        binding_result_changed = [bool]$changeState.binding_result_changed
        bringup_order_changed = [bool]$changeState.bringup_order_changed
        system_formation_changed = [bool]$changeState.system_formation_changed
        left_status = if ([string]::IsNullOrWhiteSpace([string]$CaseProjection.status.left)) { $null } else { [string]$CaseProjection.status.left }
        right_status = if ([string]::IsNullOrWhiteSpace([string]$CaseProjection.status.right)) { $null } else { [string]$CaseProjection.status.right }
        formation_basis_changes = $CaseProjection.projections.formation_basis_changes
        binding_summary_changes = $CaseProjection.projections.binding_summary_changes
        bringup_summary_changes = $CaseProjection.projections.bringup_summary_changes
        system_spec_changes = @($deltas.system_spec_changes)
        resolved_input_changes = @($deltas.resolved_input_changes)
        declared_fact_changes = [ordered]@{
            added = @($deltas.declared_fact_changes.added)
            removed = @($deltas.declared_fact_changes.removed)
        }
        declared_contract_changes = @($deltas.declared_contract_changes)
        subject_fact_changes = [ordered]@{
            added = @($deltas.subject_fact_changes.added)
            removed = @($deltas.subject_fact_changes.removed)
        }
        unresolved_capability_changes = [ordered]@{
            added = @($deltas.unresolved_capability_changes.added)
            removed = @($deltas.unresolved_capability_changes.removed)
        }
        blocked_node_changes = [ordered]@{
            added = @($deltas.blocked_node_changes.added)
            removed = @($deltas.blocked_node_changes.removed)
        }
        blocker_changes = @($deltas.blocker_changes)
    }
}

function New-ArtifactRootSystemCompilerCompareCaseSummary {
    param(
        $LoadedReport
    )

    return Convert-ArtifactRootSystemCompilerCompareCaseProjectionToSummary -CaseProjection (
        New-ArtifactRootSystemCompilerCompareCaseProjection -LoadedReport $LoadedReport
    )
}

function New-ArtifactRootSystemCompilerBindingReasonChangeMatrixEntry {
    param(
        [string]$ReasonText,
        [object[]]$CaseProjections
    )

    if ([string]::IsNullOrWhiteSpace($ReasonText)) {
        return $null
    }

    $cases = @()
    $capabilities = @()
    $changeKinds = @()
    $states = @()
    foreach ($caseProjection in @($CaseProjections)) {
        $bindingStage = $caseProjection.stages.binding_result
        if ($null -eq $bindingStage) {
            continue
        }

        $matchedChanges = @(
            @($bindingStage.binding_changes) |
                Where-Object {
                    ([string]$_.left_reason -eq $ReasonText) -or
                    ([string]$_.right_reason -eq $ReasonText)
                }
        )
        if (@($matchedChanges).Count -eq 0) {
            continue
        }

        $caseCapabilities = @(
            @($matchedChanges) |
                ForEach-Object { [string]$_.capability } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        $caseChangeKinds = @(
            @($matchedChanges) |
                ForEach-Object { [string]$_.change_kind } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        $caseStates = @(
            @($matchedChanges) |
                ForEach-Object { @([string]$_.left_state, [string]$_.right_state) } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )

        $capabilities += @($caseCapabilities)
        $changeKinds += @($caseChangeKinds)
        $states += @($caseStates)

        $cases += [ordered]@{
            case = [string]$caseProjection.subject.case
            profile = [string]$caseProjection.subject.profile
            board = [string]$caseProjection.subject.board
            capabilities = @($caseCapabilities)
            change_kinds = @($caseChangeKinds)
            states = @($caseStates)
        }
    }

    return [ordered]@{
        reason = $ReasonText
        case_count = @($cases).Count
        capability_count = @($capabilities | Sort-Object -Unique).Count
        capabilities = @($capabilities | Sort-Object -Unique)
        change_kinds = @($changeKinds | Sort-Object -Unique)
        states = @($states | Sort-Object -Unique)
        cases = @($cases | Sort-Object case)
    }
}

function New-ArtifactRootSystemCompilerBringupPhaseChangeMatrixEntry {
    param(
        [string]$PhaseName,
        [object[]]$CaseProjections
    )

    if ([string]::IsNullOrWhiteSpace($PhaseName)) {
        return $null
    }

    $cases = @()
    $nodes = @()
    $changeKinds = @()
    $states = @()
    foreach ($caseProjection in @($CaseProjections)) {
        $bringupStage = $caseProjection.stages.bringup_order
        if ($null -eq $bringupStage) {
            continue
        }

        $matchedChanges = @(
            @($bringupStage.entry_changes) |
                Where-Object {
                    ([string]$_.left_phase -eq $PhaseName) -or
                    ([string]$_.right_phase -eq $PhaseName)
                }
        )
        if (@($matchedChanges).Count -eq 0) {
            continue
        }

        $caseNodes = @(
            @($matchedChanges) |
                ForEach-Object { [string]$_.node } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        $caseChangeKinds = @(
            @($matchedChanges) |
                ForEach-Object { [string]$_.change_kind } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        $caseStates = @(
            @($matchedChanges) |
                ForEach-Object { @([string]$_.left_state, [string]$_.right_state) } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )

        $nodes += @($caseNodes)
        $changeKinds += @($caseChangeKinds)
        $states += @($caseStates)

        $cases += [ordered]@{
            case = [string]$caseProjection.subject.case
            profile = [string]$caseProjection.subject.profile
            board = [string]$caseProjection.subject.board
            nodes = @($caseNodes)
            change_kinds = @($caseChangeKinds)
            states = @($caseStates)
        }
    }

    return [ordered]@{
        phase = $PhaseName
        case_count = @($cases).Count
        node_count = @($nodes | Sort-Object -Unique).Count
        nodes = @($nodes | Sort-Object -Unique)
        change_kinds = @($changeKinds | Sort-Object -Unique)
        states = @($states | Sort-Object -Unique)
        cases = @($cases | Sort-Object case)
    }
}

function New-ArtifactRootSystemCompilerBringupDependencyChangeMatrixEntry {
    param(
        [string]$NodeName,
        [object[]]$CaseProjections
    )

    if ([string]::IsNullOrWhiteSpace($NodeName)) {
        return $null
    }

    $cases = @()
    $dependentNodes = @()
    $changeKinds = @()
    $states = @()
    $phases = @()
    foreach ($caseProjection in @($CaseProjections)) {
        $bringupStage = $caseProjection.stages.bringup_order
        if ($null -eq $bringupStage) {
            continue
        }

        $matchedChanges = @(
            @($bringupStage.entry_changes) |
                Where-Object {
                    (@($_.left_dependency_nodes) -contains $NodeName) -or
                    (@($_.right_dependency_nodes) -contains $NodeName)
                }
        )
        if (@($matchedChanges).Count -eq 0) {
            continue
        }

        $caseDependentNodes = @(
            @($matchedChanges) |
                ForEach-Object { [string]$_.node } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        $caseChangeKinds = @(
            @($matchedChanges) |
                ForEach-Object { [string]$_.change_kind } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        $caseStates = @(
            @($matchedChanges) |
                ForEach-Object { @([string]$_.left_state, [string]$_.right_state) } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        $casePhases = @(
            @($matchedChanges) |
                ForEach-Object { @([string]$_.left_phase, [string]$_.right_phase) } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )

        $dependentNodes += @($caseDependentNodes)
        $changeKinds += @($caseChangeKinds)
        $states += @($caseStates)
        $phases += @($casePhases)

        $cases += [ordered]@{
            case = [string]$caseProjection.subject.case
            profile = [string]$caseProjection.subject.profile
            board = [string]$caseProjection.subject.board
            dependent_nodes = @($caseDependentNodes)
            change_kinds = @($caseChangeKinds)
            states = @($caseStates)
            phases = @($casePhases)
        }
    }

    return [ordered]@{
        node = $NodeName
        case_count = @($cases).Count
        dependent_node_count = @($dependentNodes | Sort-Object -Unique).Count
        dependent_nodes = @($dependentNodes | Sort-Object -Unique)
        change_kinds = @($changeKinds | Sort-Object -Unique)
        states = @($states | Sort-Object -Unique)
        phases = @($phases | Sort-Object -Unique)
        cases = @($cases | Sort-Object case)
    }
}

function New-ArtifactRootSystemCompilerCompareAggregateProjection {
    param(
        [object[]]$LoadedReports
    )

    $caseProjections = @(
        @($LoadedReports) |
            ForEach-Object { New-ArtifactRootSystemCompilerCompareCaseProjection -LoadedReport $_ } |
            Where-Object { $null -ne $_ } |
            Sort-Object { [string]$_.subject.case }
    )

    if (@($caseProjections).Count -eq 0) {
        return $null
    }

    $caseSummaries = @(
        @($caseProjections) |
            ForEach-Object { Convert-ArtifactRootSystemCompilerCompareCaseProjectionToSummary -CaseProjection $_ } |
            Where-Object { $null -ne $_ } |
            Sort-Object case
    )

    $changedCases = @(
        @($caseSummaries) |
            Where-Object { [bool]$_.changed } |
            ForEach-Object { [string]$_.case } |
            Sort-Object
    )
    $unchangedCases = @(
        @($caseSummaries) |
            Where-Object { -not [bool]$_.changed } |
            ForEach-Object { [string]$_.case } |
            Sort-Object
    )
    $systemSpecChanges = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.system_spec_changes)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $resolvedInputChanges = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.resolved_input_changes)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $statusTransitions = @(
        foreach ($caseSummary in @($caseSummaries)) {
            "$([string]$caseSummary.left_status)->$([string]$caseSummary.right_status)"
        }
    ) | Where-Object { $_ -ne '->' } | Sort-Object -Unique
    $declaredFactNames = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.declared_fact_changes.added)
            @($caseSummary.declared_fact_changes.removed)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $subjectFactNames = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.subject_fact_changes.added)
            @($caseSummary.subject_fact_changes.removed)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $declaredContractNames = @(
        foreach ($caseSummary in @($caseSummaries)) {
            foreach ($contractChange in @($caseSummary.declared_contract_changes)) {
                $contractName = [string]$contractChange.contract
                if (-not [string]::IsNullOrWhiteSpace($contractName)) {
                    $contractName
                }
            }
        }
    ) | Sort-Object -Unique
    $resolvedCapabilityNames = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.resolved_capability_changes.added)
            @($caseSummary.resolved_capability_changes.removed)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $unresolvedCapabilityNames = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.unresolved_capability_changes.added)
            @($caseSummary.unresolved_capability_changes.removed)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $blockedNodeNames = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.blocked_node_changes.added)
            @($caseSummary.blocked_node_changes.removed)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $blockerKeys = @(
        foreach ($caseSummary in @($caseSummaries)) {
            foreach ($change in @($caseSummary.blocker_changes)) {
                $kind = [string]$change.kind
                $name = [string]$change.name
                if ([string]::IsNullOrWhiteSpace($kind) -or [string]::IsNullOrWhiteSpace($name)) {
                    continue
                }

                "${kind}|${name}"
            }
        }
    ) | Sort-Object -Unique
    $blockerReasons = @(
        foreach ($caseSummary in @($caseSummaries)) {
            foreach ($change in @($caseSummary.blocker_changes)) {
                @([string]$change.left_reason, [string]$change.right_reason)
            }
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $blockerMissingRequires = @(
        foreach ($caseSummary in @($caseSummaries)) {
            foreach ($change in @($caseSummary.blocker_changes)) {
                @($change.left_missing_requires)
                @($change.right_missing_requires)
            }
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $blockerDependsOn = @(
        foreach ($caseSummary in @($caseSummaries)) {
            foreach ($change in @($caseSummary.blocker_changes)) {
                @($change.left_dependency_nodes)
                @($change.right_dependency_nodes)
            }
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $bindingReasonChanges = @(
        foreach ($caseProjection in @($caseProjections)) {
            foreach ($bindingChange in @($caseProjection.stages.binding_result.binding_changes)) {
                @([string]$bindingChange.left_reason, [string]$bindingChange.right_reason)
            }
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $bringupPhaseChanges = @(
        foreach ($caseProjection in @($caseProjections)) {
            foreach ($entryChange in @($caseProjection.stages.bringup_order.entry_changes)) {
                @([string]$entryChange.left_phase, [string]$entryChange.right_phase)
            }
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique
    $bringupDependencyChanges = @(
        foreach ($caseProjection in @($caseProjections)) {
            foreach ($entryChange in @($caseProjection.stages.bringup_order.entry_changes)) {
                @($entryChange.left_dependency_nodes)
                @($entryChange.right_dependency_nodes)
            }
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique

    $bindingReasonChangeMatrix = @(
        foreach ($reasonText in @($bindingReasonChanges)) {
            New-ArtifactRootSystemCompilerBindingReasonChangeMatrixEntry -ReasonText $reasonText -CaseProjections $caseProjections
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object reason
    $bringupPhaseChangeMatrix = @(
        foreach ($phaseName in @($bringupPhaseChanges)) {
            New-ArtifactRootSystemCompilerBringupPhaseChangeMatrixEntry -PhaseName $phaseName -CaseProjections $caseProjections
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object phase
    $bringupDependencyChangeMatrix = @(
        foreach ($nodeName in @($bringupDependencyChanges)) {
            New-ArtifactRootSystemCompilerBringupDependencyChangeMatrixEntry -NodeName $nodeName -CaseProjections $caseProjections
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object node

    return [pscustomobject][ordered]@{
        case_projections = @($caseProjections)
        case_summaries = @($caseSummaries)
        changed_cases = @($changedCases)
        unchanged_cases = @($unchangedCases)
        stage_changed_case_counts = [ordered]@{
            system_input = @($caseSummaries | Where-Object { [bool]$_.system_input_changed }).Count
            binding_result = @($caseSummaries | Where-Object { [bool]$_.binding_result_changed }).Count
            bringup_order = @($caseSummaries | Where-Object { [bool]$_.bringup_order_changed }).Count
            system_formation = @($caseSummaries | Where-Object { [bool]$_.system_formation_changed }).Count
        }
        universes = [ordered]@{
            system_spec_changes = @($systemSpecChanges)
            resolved_input_changes = @($resolvedInputChanges)
            status_transitions = @($statusTransitions)
            declared_fact_names = @($declaredFactNames)
            declared_contract_names = @($declaredContractNames)
            subject_fact_names = @($subjectFactNames)
            resolved_capability_names = @($resolvedCapabilityNames)
            unresolved_capability_names = @($unresolvedCapabilityNames)
            blocked_node_names = @($blockedNodeNames)
            blocker_keys = @($blockerKeys)
            blocker_reasons = @($blockerReasons)
            blocker_missing_requires = @($blockerMissingRequires)
            blocker_depends_on = @($blockerDependsOn)
            binding_reason_changes = @($bindingReasonChanges)
            bringup_phase_changes = @($bringupPhaseChanges)
            bringup_dependency_changes = @($bringupDependencyChanges)
        }
        matrices = [ordered]@{
            binding_reason_change_matrix = @($bindingReasonChangeMatrix)
            bringup_phase_change_matrix = @($bringupPhaseChangeMatrix)
            bringup_dependency_change_matrix = @($bringupDependencyChangeMatrix)
        }
    }
}

function New-ArtifactRootSystemCompilerStageChangeEntry {
    param(
        [string]$StageName,
        [object[]]$CaseSummaries,
        [string]$PropertyName
    )

    if ([string]::IsNullOrWhiteSpace($StageName)) {
        return $null
    }

    $cases = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        if (-not [bool]$caseSummary.$PropertyName) {
            continue
        }

        $cases += New-ArtifactRootSystemInputMatrixCaseEntry -CaseSummary $caseSummary
    }

    return [ordered]@{
        stage = $StageName
        case_count = @($cases).Count
        cases = @($cases | Sort-Object case)
    }
}

function New-ArtifactRootSystemCompilerComparisonResult {
    param(
        [object[]]$LoadedReports
    )

    $aggregateProjection = New-ArtifactRootSystemCompilerCompareAggregateProjection -LoadedReports $LoadedReports
    if ($null -eq $aggregateProjection) {
        return $null
    }

    $caseSummaries = @($aggregateProjection.case_summaries)
    $changedCases = @($aggregateProjection.changed_cases)
    $unchangedCases = @($aggregateProjection.unchanged_cases)
    $systemSpecChanges = @($aggregateProjection.universes.system_spec_changes)
    $resolvedInputChanges = @($aggregateProjection.universes.resolved_input_changes)
    $statusTransitions = @($aggregateProjection.universes.status_transitions)
    $declaredFactNames = @($aggregateProjection.universes.declared_fact_names)
    $subjectFactNames = @($aggregateProjection.universes.subject_fact_names)
    $declaredContractNames = @($aggregateProjection.universes.declared_contract_names)
    $resolvedCapabilityNames = @($aggregateProjection.universes.resolved_capability_names)
    $unresolvedCapabilityNames = @($aggregateProjection.universes.unresolved_capability_names)
    $blockedNodeNames = @($aggregateProjection.universes.blocked_node_names)
    $blockerKeys = @($aggregateProjection.universes.blocker_keys)
    $blockerReasons = @($aggregateProjection.universes.blocker_reasons)
    $blockerMissingRequires = @($aggregateProjection.universes.blocker_missing_requires)
    $blockerDependsOn = @($aggregateProjection.universes.blocker_depends_on)

    $stageChangeMatrix = @(
        New-ArtifactRootSystemCompilerStageChangeEntry -StageName 'system_input' -CaseSummaries $caseSummaries -PropertyName 'system_input_changed'
        New-ArtifactRootSystemCompilerStageChangeEntry -StageName 'binding_result' -CaseSummaries $caseSummaries -PropertyName 'binding_result_changed'
        New-ArtifactRootSystemCompilerStageChangeEntry -StageName 'bringup_order' -CaseSummaries $caseSummaries -PropertyName 'bringup_order_changed'
        New-ArtifactRootSystemCompilerStageChangeEntry -StageName 'system_formation' -CaseSummaries $caseSummaries -PropertyName 'system_formation_changed'
    ) | Where-Object { $null -ne $_ } | Sort-Object stage
    $systemSpecChangeMatrix = @(
        foreach ($changeText in @($systemSpecChanges)) {
            New-ArtifactRootSystemInputCompareChangeEntry -ChangeText $changeText -CaseSummaries $caseSummaries -CollectionName 'system_spec_changes'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object change
    $resolvedInputChangeMatrix = @(
        foreach ($changeText in @($resolvedInputChanges)) {
            New-ArtifactRootSystemInputCompareChangeEntry -ChangeText $changeText -CaseSummaries $caseSummaries -CollectionName 'resolved_input_changes'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object change
    $statusChangeMatrix = @(
        foreach ($transition in @($statusTransitions)) {
            New-ArtifactRootSystemFormationCompareStatusEntry -Transition $transition -CaseSummaries $caseSummaries
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object transition
    $declaredFactChangeMatrix = @(
        foreach ($factName in @($declaredFactNames)) {
            New-ArtifactRootSystemFormationCompareNamedChangeEntry -Name $factName -CaseSummaries $caseSummaries -FieldName 'fact' -CollectionName 'declared_fact_changes'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object fact
    $declaredContractChangeMatrix = @(
        foreach ($contractName in @($declaredContractNames)) {
            New-ArtifactRootSystemInputCompareContractEntry -ContractName $contractName -CaseSummaries $caseSummaries
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object contract
    $subjectFactChangeMatrix = @(
        foreach ($factName in @($subjectFactNames)) {
            New-ArtifactRootSystemFormationCompareNamedChangeEntry -Name $factName -CaseSummaries $caseSummaries -FieldName 'fact' -CollectionName 'subject_fact_changes'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object fact
    $resolvedCapabilityChangeMatrix = @(
        foreach ($capabilityName in @($resolvedCapabilityNames)) {
            New-ArtifactRootSystemFormationCompareNamedChangeEntry -Name $capabilityName -CaseSummaries $caseSummaries -FieldName 'capability' -CollectionName 'resolved_capability_changes'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object capability
    $unresolvedCapabilityChangeMatrix = @(
        foreach ($capabilityName in @($unresolvedCapabilityNames)) {
            New-ArtifactRootSystemFormationCompareNamedChangeEntry -Name $capabilityName -CaseSummaries $caseSummaries -FieldName 'capability' -CollectionName 'unresolved_capability_changes'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object capability
    $blockedNodeChangeMatrix = @(
        foreach ($nodeName in @($blockedNodeNames)) {
            New-ArtifactRootSystemFormationCompareNamedChangeEntry -Name $nodeName -CaseSummaries $caseSummaries -FieldName 'node' -CollectionName 'blocked_node_changes'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object node
    $blockerChangeMatrix = @(
        foreach ($blockerKey in @($blockerKeys)) {
            $parts = [string]$blockerKey -split '\|', 2
            if (@($parts).Count -ne 2) {
                continue
            }

            New-ArtifactRootSystemFormationCompareBlockerEntry -Kind ([string]$parts[0]) -Name ([string]$parts[1]) -CaseSummaries $caseSummaries
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object kind, name
    $blockerReasonChangeMatrix = @(
        foreach ($reasonText in @($blockerReasons)) {
            New-ArtifactRootSystemFormationCompareBlockerReasonEntry -ReasonText $reasonText -CaseSummaries $caseSummaries
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object reason
    $blockerMissingRequiresChangeMatrix = @(
        foreach ($requireName in @($blockerMissingRequires)) {
            New-ArtifactRootSystemFormationCompareBlockerDetailEntry -DetailName $requireName -CaseSummaries $caseSummaries -LeftCollectionName 'left_missing_requires' -RightCollectionName 'right_missing_requires' -FieldName 'require'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object require
    $blockerDependsOnChangeMatrix = @(
        foreach ($nodeName in @($blockerDependsOn)) {
            New-ArtifactRootSystemFormationCompareBlockerDetailEntry -DetailName $nodeName -CaseSummaries $caseSummaries -LeftCollectionName 'left_dependency_nodes' -RightCollectionName 'right_dependency_nodes' -FieldName 'node'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object node
    $formationChangedCases = @(
        @($caseSummaries) |
            Where-Object { [bool]$_.system_formation_changed } |
            ForEach-Object { [string]$_.case } |
            Sort-Object
    )
    $formationChangedCaseSummaries = @(
        @($caseSummaries) |
            Where-Object { [bool]$_.system_formation_changed } |
            Sort-Object case
    )
    $formationDriftTransitions = @(
        foreach ($caseSummary in @($formationChangedCaseSummaries)) {
            "$([string]$caseSummary.left_status)->$([string]$caseSummary.right_status)"
        }
    ) | Where-Object { $_ -ne '->' } | Sort-Object -Unique
    $formationDriftStatusChangeMatrix = @(
        foreach ($transition in @($formationDriftTransitions)) {
            New-ArtifactRootSystemFormationCompareStatusEntry -Transition $transition -CaseSummaries $formationChangedCaseSummaries
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object transition
    $bindingChangedCases = @(
        @($caseSummaries) |
            Where-Object { [bool]$_.binding_result_changed } |
            ForEach-Object { [string]$_.case } |
            Sort-Object
    )
    $bringupChangedCases = @(
        @($caseSummaries) |
            Where-Object { [bool]$_.bringup_order_changed } |
            ForEach-Object { [string]$_.case } |
            Sort-Object
    )
    $bindingChangeCount = 0
    $bringupEntryChangeCount = 0
    foreach ($caseProjection in @($aggregateProjection.case_projections)) {
        if ($null -ne $caseProjection.stages.binding_result) {
            $bindingChangeCount += [int]$caseProjection.stages.binding_result.binding_change_count
        }

        if ($null -ne $caseProjection.stages.bringup_order) {
            $bringupEntryChangeCount += [int]$caseProjection.stages.bringup_order.entry_change_count
        }
    }
    $formationDrift = [ordered]@{
        compared_case_count = @($caseSummaries).Count
        changed_case_count = @($formationChangedCases).Count
        changed_cases = @($formationChangedCases)
        status_change_matrix = @($formationDriftStatusChangeMatrix)
        declared_fact_change_matrix = @($declaredFactChangeMatrix)
        declared_contract_change_matrix = @($declaredContractChangeMatrix)
        subject_fact_change_matrix = @($subjectFactChangeMatrix)
        unresolved_capability_change_matrix = @($unresolvedCapabilityChangeMatrix)
        blocked_node_change_matrix = @($blockedNodeChangeMatrix)
        blocker_change_matrix = @($blockerChangeMatrix)
        blocker_reason_change_matrix = @($blockerReasonChangeMatrix)
        blocker_missing_requires_change_matrix = @($blockerMissingRequiresChangeMatrix)
        blocker_depends_on_change_matrix = @($blockerDependsOnChangeMatrix)
    }
    $bindingDrift = [ordered]@{
        compared_case_count = @($caseSummaries).Count
        changed_case_count = @($bindingChangedCases).Count
        changed_cases = @($bindingChangedCases)
        binding_change_count = $bindingChangeCount
        reason_change_matrix = @($aggregateProjection.matrices.binding_reason_change_matrix)
        resolved_capability_change_matrix = @($resolvedCapabilityChangeMatrix)
        unresolved_capability_change_matrix = @($unresolvedCapabilityChangeMatrix)
    }
    $bringupDrift = [ordered]@{
        compared_case_count = @($caseSummaries).Count
        changed_case_count = @($bringupChangedCases).Count
        changed_cases = @($bringupChangedCases)
        entry_change_count = $bringupEntryChangeCount
        phase_change_matrix = @($aggregateProjection.matrices.bringup_phase_change_matrix)
        dependency_change_matrix = @($aggregateProjection.matrices.bringup_dependency_change_matrix)
        blocked_node_change_matrix = @($blockedNodeChangeMatrix)
    }
    $resultMap = New-ArtifactRootSystemCompilerResultMap -Comparison

    return [ordered]@{
        kind = 'system_compiler_summary/v0'
        mode = 'comparison'
        compared_case_count = @($caseSummaries).Count
        changed_case_count = @($changedCases).Count
        unchanged_case_count = @($unchangedCases).Count
        changed_cases = @($changedCases)
        unchanged_cases = @($unchangedCases)
        stage_changed_case_counts = $aggregateProjection.stage_changed_case_counts
        cases = @(
            @($caseSummaries) |
                Select-Object `
                    case,
                    board,
                    profile,
                    active_facets,
                    changed,
                    changed_stages,
                    system_input_changed,
                    binding_result_changed,
                    bringup_order_changed,
                    system_formation_changed,
                    left_status,
                    right_status,
                    formation_basis_changes,
                    binding_summary_changes,
                    bringup_summary_changes,
                    system_spec_changes,
                    resolved_input_changes,
                    declared_fact_changes,
                    declared_contract_changes,
                    subject_fact_changes,
                    unresolved_capability_changes,
                    blocked_node_changes,
                    blocker_changes |
                Sort-Object case
        )
        stage_change_matrix = @($stageChangeMatrix)
        status_change_matrix = @($statusChangeMatrix)
        system_spec_change_matrix = @($systemSpecChangeMatrix)
        resolved_input_change_matrix = @($resolvedInputChangeMatrix)
        declared_fact_change_matrix = @($declaredFactChangeMatrix)
        declared_contract_change_matrix = @($declaredContractChangeMatrix)
        subject_fact_change_matrix = @($subjectFactChangeMatrix)
        resolved_capability_change_matrix = @($resolvedCapabilityChangeMatrix)
        unresolved_capability_change_matrix = @($unresolvedCapabilityChangeMatrix)
        blocked_node_change_matrix = @($blockedNodeChangeMatrix)
        blocker_change_matrix = @($blockerChangeMatrix)
        blocker_reason_change_matrix = @($blockerReasonChangeMatrix)
        blocker_missing_requires_change_matrix = @($blockerMissingRequiresChangeMatrix)
        blocker_depends_on_change_matrix = @($blockerDependsOnChangeMatrix)
        binding_reason_change_matrix = @($aggregateProjection.matrices.binding_reason_change_matrix)
        bringup_phase_change_matrix = @($aggregateProjection.matrices.bringup_phase_change_matrix)
        bringup_dependency_change_matrix = @($aggregateProjection.matrices.bringup_dependency_change_matrix)
        formation_drift = $formationDrift
        binding_drift = $bindingDrift
        bringup_drift = $bringupDrift
        result_map = $resultMap
    }
}

function New-BringupEvidenceEntryResult {
    param(
        $ReportData,
        $GraphInfo,
        [string]$CapabilityName
    )

    $evidence = Get-CapabilityEvidence -ReportData $ReportData -GraphInfo $GraphInfo -CapabilityName $CapabilityName
    return [ordered]@{
        capability = [string]$CapabilityName
        declared = [bool]$evidence.declared
        materialized = [bool]$evidence.materialized
        published = [bool]$evidence.published
        observed = [bool]$evidence.observed
        blocked = [bool]$evidence.blocked
        failed = [bool]$evidence.failed
        publish_state = $evidence.publish_state
        export_state = $evidence.export_state
        provider_nodes = @($evidence.provider_nodes)
        consumer_nodes = @($evidence.consumer_nodes)
        blocked_reasons = @($evidence.blocked_reasons)
        failed_reasons = @($evidence.failed_reasons)
    }
}

function New-BringupEvidenceResult {
    param(
        $ReportData,
        $GraphInfo
    )

    $entries = @()
    foreach ($capabilityName in @(Get-ReportCapabilityNames -ReportData $ReportData -GraphInfo $GraphInfo)) {
        $entries += New-BringupEvidenceEntryResult -ReportData $ReportData -GraphInfo $GraphInfo -CapabilityName $capabilityName
    }

    $entries = @($entries | Sort-Object capability)
    return [ordered]@{
        declared_count = @($entries | Where-Object { [bool]$_.declared }).Count
        materialized_count = @($entries | Where-Object { [bool]$_.materialized }).Count
        published_count = @($entries | Where-Object { [bool]$_.published }).Count
        observed_count = @($entries | Where-Object { [bool]$_.observed }).Count
        blocked_count = @($entries | Where-Object { [bool]$_.blocked }).Count
        failed_count = @($entries | Where-Object { [bool]$_.failed }).Count
        published_capabilities = @(
            @($entries) |
                Where-Object { [bool]$_.published } |
                ForEach-Object { [string]$_.capability } |
                Sort-Object -Unique
        )
        blocked_reasons = @(
            @($entries) |
                ForEach-Object { @($_.blocked_reasons) } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        failed_reasons = @(
            @($entries) |
                ForEach-Object { @($_.failed_reasons) } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        evidence_entries = @($entries)
    }
}

function New-ArtifactRootBringupCaseSummary {
    param(
        $LoadedReport
    )

    $graphInfo = Load-GraphFromArtifactReport -ReportData $LoadedReport.Data
    $bringupEvidence = New-BringupEvidenceResult -ReportData $LoadedReport.Data -GraphInfo $graphInfo

    return [pscustomobject][ordered]@{
        case = [string]$LoadedReport.Data.subject.case
        profile = [string]$LoadedReport.Data.subject.profile
        board = [string]$LoadedReport.Data.subject.board
        active_facets = @($LoadedReport.Data.subject.active_facets)
        declared_count = [int]$bringupEvidence.declared_count
        materialized_count = [int]$bringupEvidence.materialized_count
        published_count = [int]$bringupEvidence.published_count
        observed_count = [int]$bringupEvidence.observed_count
        blocked_count = [int]$bringupEvidence.blocked_count
        failed_count = [int]$bringupEvidence.failed_count
        published_capabilities = @($bringupEvidence.published_capabilities)
        blocked_reasons = @($bringupEvidence.blocked_reasons)
        failed_reasons = @($bringupEvidence.failed_reasons)
        evidence_entries = @($bringupEvidence.evidence_entries)
    }
}

function New-AggregatedBringupCapabilityEntry {
    param(
        [string]$CapabilityName
    )

    return [pscustomobject]@{
        capability = $CapabilityName
        cases = @()
        declared_cases = @()
        materialized_cases = @()
        published_cases = @()
        observed_cases = @()
        blocked_cases = @()
        failed_cases = @()
        publish_states = @()
        export_states = @()
        provider_nodes = @()
        consumer_nodes = @()
        blocked_reasons = @()
        failed_reasons = @()
    }
}

function Normalize-AggregatedBringupCapabilityEntry {
    param(
        $Entry
    )

    $cases = @($Entry.cases | Sort-Object case)
    $declaredCases = @($Entry.declared_cases | Sort-Object -Unique)
    $materializedCases = @($Entry.materialized_cases | Sort-Object -Unique)
    $publishedCases = @($Entry.published_cases | Sort-Object -Unique)
    $observedCases = @($Entry.observed_cases | Sort-Object -Unique)
    $blockedCases = @($Entry.blocked_cases | Sort-Object -Unique)
    $failedCases = @($Entry.failed_cases | Sort-Object -Unique)

    return [ordered]@{
        capability = [string]$Entry.capability
        case_count = @($cases).Count
        cases = $cases
        declared = ($declaredCases.Count -gt 0)
        materialized = ($materializedCases.Count -gt 0)
        published = ($publishedCases.Count -gt 0)
        observed = ($observedCases.Count -gt 0)
        blocked = ($blockedCases.Count -gt 0)
        failed = ($failedCases.Count -gt 0)
        declared_cases = $declaredCases
        materialized_cases = $materializedCases
        published_cases = $publishedCases
        observed_cases = $observedCases
        blocked_cases = $blockedCases
        failed_cases = $failedCases
        publish_states = @(
            @($Entry.publish_states) |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        export_states = @(
            @($Entry.export_states) |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        provider_nodes = @($Entry.provider_nodes | Sort-Object -Unique)
        consumer_nodes = @($Entry.consumer_nodes | Sort-Object -Unique)
        blocked_reasons = @(
            @($Entry.blocked_reasons) |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        failed_reasons = @(
            @($Entry.failed_reasons) |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
    }
}

function New-ArtifactRootBringupReasonEntry {
    param(
        [string]$ReasonText,
        [object[]]$CaseSummaries,
        [string]$PropertyName
    )

    if ([string]::IsNullOrWhiteSpace($ReasonText)) {
        return $null
    }

    $reasonCases = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        if (-not (@($caseSummary.$PropertyName) -contains $ReasonText)) {
            continue
        }

        $reasonCases += [pscustomobject][ordered]@{
            case = [string]$caseSummary.case
            profile = [string]$caseSummary.profile
            board = [string]$caseSummary.board
        }
    }

    return [ordered]@{
        reason = $ReasonText
        case_count = @($reasonCases).Count
        cases = @($reasonCases | Sort-Object case)
    }
}

function New-ArtifactRootBringupEvidenceResult {
    param(
        [object[]]$LoadedReports
    )

    $caseSummaries = @(
        @($LoadedReports) |
            ForEach-Object { New-ArtifactRootBringupCaseSummary -LoadedReport $_ } |
            Sort-Object case
    )

    $capabilityMap = @{}
    foreach ($caseSummary in @($caseSummaries)) {
        $caseName = [string]$caseSummary.case
        foreach ($entry in @($caseSummary.evidence_entries)) {
            $capabilityName = [string]$entry.capability
            if ([string]::IsNullOrWhiteSpace($capabilityName)) {
                continue
            }

            if (-not $capabilityMap.ContainsKey($capabilityName)) {
                $capabilityMap[$capabilityName] = New-AggregatedBringupCapabilityEntry -CapabilityName $capabilityName
            }

            $aggregate = $capabilityMap[$capabilityName]
            $aggregate.cases = @(
                @($aggregate.cases) + [pscustomobject][ordered]@{
                    case = $caseName
                    profile = [string]$caseSummary.profile
                    board = [string]$caseSummary.board
                    declared = [bool]$entry.declared
                    materialized = [bool]$entry.materialized
                    published = [bool]$entry.published
                    observed = [bool]$entry.observed
                    blocked = [bool]$entry.blocked
                    failed = [bool]$entry.failed
                    publish_state = if ([string]::IsNullOrWhiteSpace([string]$entry.publish_state)) { $null } else { [string]$entry.publish_state }
                    export_state = if ([string]::IsNullOrWhiteSpace([string]$entry.export_state)) { $null } else { [string]$entry.export_state }
                }
            )
            Add-CaseIf -Entry $aggregate -Condition ([bool]$entry.declared) -CaseName $caseName -PropertyName 'declared_cases'
            Add-CaseIf -Entry $aggregate -Condition ([bool]$entry.materialized) -CaseName $caseName -PropertyName 'materialized_cases'
            Add-CaseIf -Entry $aggregate -Condition ([bool]$entry.published) -CaseName $caseName -PropertyName 'published_cases'
            Add-CaseIf -Entry $aggregate -Condition ([bool]$entry.observed) -CaseName $caseName -PropertyName 'observed_cases'
            Add-CaseIf -Entry $aggregate -Condition ([bool]$entry.blocked) -CaseName $caseName -PropertyName 'blocked_cases'
            Add-CaseIf -Entry $aggregate -Condition ([bool]$entry.failed) -CaseName $caseName -PropertyName 'failed_cases'

            if (-not [string]::IsNullOrWhiteSpace([string]$entry.publish_state)) {
                $aggregate.publish_states = @($aggregate.publish_states + [string]$entry.publish_state)
            }
            if (-not [string]::IsNullOrWhiteSpace([string]$entry.export_state)) {
                $aggregate.export_states = @($aggregate.export_states + [string]$entry.export_state)
            }

            $aggregate.provider_nodes = @(
                @($aggregate.provider_nodes) +
                @(Get-CaseQualifiedNodeNames -CaseName $caseName -NodeNames @($entry.provider_nodes))
            )
            $aggregate.consumer_nodes = @(
                @($aggregate.consumer_nodes) +
                @(Get-CaseQualifiedNodeNames -CaseName $caseName -NodeNames @($entry.consumer_nodes))
            )
            $aggregate.blocked_reasons = @($aggregate.blocked_reasons + @($entry.blocked_reasons))
            $aggregate.failed_reasons = @($aggregate.failed_reasons + @($entry.failed_reasons))
        }
    }

    $blockedReasons = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.blocked_reasons)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique

    $failedReasons = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.failed_reasons)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique

    $blockedReasonMatrix = @(
        foreach ($reasonText in @($blockedReasons)) {
            New-ArtifactRootBringupReasonEntry -ReasonText $reasonText -CaseSummaries $caseSummaries -PropertyName 'blocked_reasons'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object reason

    $failedReasonMatrix = @(
        foreach ($reasonText in @($failedReasons)) {
            New-ArtifactRootBringupReasonEntry -ReasonText $reasonText -CaseSummaries $caseSummaries -PropertyName 'failed_reasons'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object reason

    return [ordered]@{
        case_count = @($caseSummaries).Count
        totals = [ordered]@{
            declared_count = [int](@($caseSummaries | Measure-Object -Property declared_count -Sum).Sum)
            materialized_count = [int](@($caseSummaries | Measure-Object -Property materialized_count -Sum).Sum)
            published_count = [int](@($caseSummaries | Measure-Object -Property published_count -Sum).Sum)
            observed_count = [int](@($caseSummaries | Measure-Object -Property observed_count -Sum).Sum)
            blocked_count = [int](@($caseSummaries | Measure-Object -Property blocked_count -Sum).Sum)
            failed_count = [int](@($caseSummaries | Measure-Object -Property failed_count -Sum).Sum)
        }
        cases = @(
            @($caseSummaries) |
                Select-Object `
                    case,
                    board,
                    profile,
                    active_facets,
                    declared_count,
                    materialized_count,
                    published_count,
                    observed_count,
                    blocked_count,
                    failed_count,
                    published_capabilities,
                    blocked_reasons,
                    failed_reasons |
                Sort-Object case
        )
        capability_matrix = @(
            $capabilityMap.Values |
                ForEach-Object { Normalize-AggregatedBringupCapabilityEntry -Entry $_ } |
                Sort-Object capability
        )
        blocked_reason_matrix = @($blockedReasonMatrix)
        failed_reason_matrix = @($failedReasonMatrix)
    }
}

function New-ArtifactRootBringupCompareCaseSummary {
    param(
        $LoadedReport
    )

    $report = $LoadedReport.Data
    $comparison = Get-BringupEvidenceComparisonFromReport -ReportData $report
    if ($null -eq $comparison) {
        return $null
    }

    return [pscustomobject][ordered]@{
        case = [string]$report.subject.case
        profile = [string]$report.subject.profile
        board = [string]$report.subject.board
        active_facets = @($report.subject.active_facets)
        changed = [bool]$comparison.changed
        summary_changes = @($comparison.summary_changes)
        capability_change_count = @($comparison.capability_changes).Count
        capabilities_changed = @(
            @($comparison.capability_changes) |
                ForEach-Object { [string]$_.capability } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        published_capabilities_added = @($comparison.published_capability_changes.added)
        published_capabilities_removed = @($comparison.published_capability_changes.removed)
        blocked_reasons_added = @($comparison.blocked_reason_changes.added)
        blocked_reasons_removed = @($comparison.blocked_reason_changes.removed)
        failed_reasons_added = @($comparison.failed_reason_changes.added)
        failed_reasons_removed = @($comparison.failed_reason_changes.removed)
        capability_changes = @($comparison.capability_changes)
    }
}

function New-ArtifactRootBringupCompareSummaryChangeEntry {
    param(
        [string]$ChangeText,
        [object[]]$CaseSummaries
    )

    if ([string]::IsNullOrWhiteSpace($ChangeText)) {
        return $null
    }

    $changeCases = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        if (-not (@($caseSummary.summary_changes) -contains $ChangeText)) {
            continue
        }

        $changeCases += [pscustomobject][ordered]@{
            case = [string]$caseSummary.case
            profile = [string]$caseSummary.profile
            board = [string]$caseSummary.board
        }
    }

    return [ordered]@{
        change = $ChangeText
        case_count = @($changeCases).Count
        cases = @($changeCases | Sort-Object case)
    }
}

function New-ArtifactRootBringupCompareCapabilityEntry {
    param(
        [string]$CapabilityName,
        [object[]]$CaseSummaries
    )

    if ([string]::IsNullOrWhiteSpace($CapabilityName)) {
        return $null
    }

    $capabilityCases = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        $capabilityChange = @(
            @($caseSummary.capability_changes) |
                Where-Object { [string]$_.capability -eq $CapabilityName } |
                Select-Object -First 1
        ) | Select-Object -First 1

        if ($null -eq $capabilityChange) {
            continue
        }

        $capabilityCases += [pscustomobject][ordered]@{
            case = [string]$caseSummary.case
            profile = [string]$caseSummary.profile
            board = [string]$caseSummary.board
            change_kind = [string]$capabilityChange.change_kind
            left_published = [bool]$capabilityChange.left_published
            right_published = [bool]$capabilityChange.right_published
            left_observed = [bool]$capabilityChange.left_observed
            right_observed = [bool]$capabilityChange.right_observed
            left_publish_state = if ([string]::IsNullOrWhiteSpace([string]$capabilityChange.left_publish_state)) { $null } else { [string]$capabilityChange.left_publish_state }
            right_publish_state = if ([string]::IsNullOrWhiteSpace([string]$capabilityChange.right_publish_state)) { $null } else { [string]$capabilityChange.right_publish_state }
            left_export_state = if ([string]::IsNullOrWhiteSpace([string]$capabilityChange.left_export_state)) { $null } else { [string]$capabilityChange.left_export_state }
            right_export_state = if ([string]::IsNullOrWhiteSpace([string]$capabilityChange.right_export_state)) { $null } else { [string]$capabilityChange.right_export_state }
        }
    }

    return [ordered]@{
        capability = $CapabilityName
        case_count = @($capabilityCases).Count
        change_kinds = @(
            @($capabilityCases) |
                ForEach-Object { [string]$_.change_kind } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        cases = @($capabilityCases | Sort-Object case)
    }
}

function New-ArtifactRootBringupEvidenceComparisonResult {
    param(
        [object[]]$LoadedReports
    )

    $caseSummaries = @(
        @($LoadedReports) |
            ForEach-Object { New-ArtifactRootBringupCompareCaseSummary -LoadedReport $_ } |
            Where-Object { $null -ne $_ } |
            Sort-Object case
    )

    if (@($caseSummaries).Count -eq 0) {
        return $null
    }

    $summaryChanges = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.summary_changes)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique

    $capabilityNames = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.capabilities_changed)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique

    $summaryChangeMatrix = @(
        foreach ($changeText in @($summaryChanges)) {
            New-ArtifactRootBringupCompareSummaryChangeEntry -ChangeText $changeText -CaseSummaries $caseSummaries
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object change

    $capabilityChangeMatrix = @(
        foreach ($capabilityName in @($capabilityNames)) {
            New-ArtifactRootBringupCompareCapabilityEntry -CapabilityName $capabilityName -CaseSummaries $caseSummaries
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object capability

    return [ordered]@{
        compared_case_count = @($caseSummaries).Count
        changed_case_count = @($caseSummaries | Where-Object { [bool]$_.changed }).Count
        unchanged_case_count = @($caseSummaries | Where-Object { -not [bool]$_.changed }).Count
        changed_cases = @(
            @($caseSummaries) |
                Where-Object { [bool]$_.changed } |
                ForEach-Object { [string]$_.case } |
                Sort-Object
        )
        unchanged_cases = @(
            @($caseSummaries) |
                Where-Object { -not [bool]$_.changed } |
                ForEach-Object { [string]$_.case } |
                Sort-Object
        )
        capability_change_count = [int](@($caseSummaries | Measure-Object -Property capability_change_count -Sum).Sum)
        cases = @($caseSummaries)
        summary_change_matrix = @($summaryChangeMatrix)
        capability_change_matrix = @($capabilityChangeMatrix)
    }
}

function New-ArtifactRootBindingResultCompareCaseSummary {
    param(
        $LoadedReport
    )

    if ($null -eq $LoadedReport -or $null -eq $LoadedReport.Data) {
        return $null
    }

    $report = $LoadedReport.Data
    $comparison = Get-BindingResultComparisonFromReport -ReportData $report
    if ($null -eq $comparison) {
        return $null
    }

    return [pscustomobject][ordered]@{
        case = [string]$report.subject.case
        profile = [string]$report.subject.profile
        board = [string]$report.subject.board
        active_facets = @($report.subject.active_facets)
        changed = [bool]$comparison.changed
        summary_changes = @($comparison.summary_changes)
        binding_change_count = @($comparison.binding_changes).Count
        capabilities_changed = @(
            @($comparison.binding_changes) |
                ForEach-Object { [string]$_.capability } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        resolved_capability_changes = [ordered]@{
            added = @($comparison.resolved_capability_changes.added)
            removed = @($comparison.resolved_capability_changes.removed)
        }
        unresolved_capability_changes = [ordered]@{
            added = @($comparison.unresolved_capability_changes.added)
            removed = @($comparison.unresolved_capability_changes.removed)
        }
        binding_changes = @($comparison.binding_changes)
    }
}

function New-ArtifactRootBindingResultCompareCapabilityEntry {
    param(
        [string]$CapabilityName,
        [object[]]$CaseSummaries
    )

    if ([string]::IsNullOrWhiteSpace($CapabilityName)) {
        return $null
    }

    $capabilityCases = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        $bindingChange = @(
            @($caseSummary.binding_changes) |
                Where-Object { [string]$_.capability -eq $CapabilityName } |
                Select-Object -First 1
        ) | Select-Object -First 1

        if ($null -eq $bindingChange) {
            continue
        }

        $capabilityCases += [pscustomobject][ordered]@{
            case = [string]$caseSummary.case
            profile = [string]$caseSummary.profile
            board = [string]$caseSummary.board
            change_kind = [string]$bindingChange.change_kind
            left_state = if ([string]::IsNullOrWhiteSpace([string]$bindingChange.left_state)) { $null } else { [string]$bindingChange.left_state }
            right_state = if ([string]::IsNullOrWhiteSpace([string]$bindingChange.right_state)) { $null } else { [string]$bindingChange.right_state }
            left_provider_nodes = @($bindingChange.left_provider_nodes)
            right_provider_nodes = @($bindingChange.right_provider_nodes)
            left_consumer_nodes = @($bindingChange.left_consumer_nodes)
            right_consumer_nodes = @($bindingChange.right_consumer_nodes)
            left_reason = if ([string]::IsNullOrWhiteSpace([string]$bindingChange.left_reason)) { $null } else { [string]$bindingChange.left_reason }
            right_reason = if ([string]::IsNullOrWhiteSpace([string]$bindingChange.right_reason)) { $null } else { [string]$bindingChange.right_reason }
        }
    }

    if (@($capabilityCases).Count -eq 0) {
        return $null
    }

    return [ordered]@{
        capability = $CapabilityName
        case_count = @($capabilityCases).Count
        change_kinds = @(
            @($capabilityCases) |
                ForEach-Object { [string]$_.change_kind } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        states = @(
            @($capabilityCases) |
                ForEach-Object { @([string]$_.left_state, [string]$_.right_state) } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        reasons = @(
            @($capabilityCases) |
                ForEach-Object { @([string]$_.left_reason, [string]$_.right_reason) } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        cases = @($capabilityCases | Sort-Object case)
    }
}

function New-ArtifactRootBindingResultComparisonResult {
    param(
        [object[]]$LoadedReports
    )

    $caseSummaries = @(
        @($LoadedReports) |
            ForEach-Object { New-ArtifactRootBindingResultCompareCaseSummary -LoadedReport $_ } |
            Where-Object { $null -ne $_ } |
            Sort-Object case
    )

    if (@($caseSummaries).Count -eq 0) {
        return $null
    }

    $summaryChanges = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.summary_changes)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique

    $capabilityNames = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.capabilities_changed)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique

    $resolvedCapabilityNames = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.resolved_capability_changes.added)
            @($caseSummary.resolved_capability_changes.removed)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique

    $unresolvedCapabilityNames = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.unresolved_capability_changes.added)
            @($caseSummary.unresolved_capability_changes.removed)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique

    $summaryChangeMatrix = @(
        foreach ($changeText in @($summaryChanges)) {
            New-ArtifactRootResourceCompareSummaryChangeEntry -ChangeText $changeText -CaseSummaries $caseSummaries
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object change

    $capabilityChangeMatrix = @(
        foreach ($capabilityName in @($capabilityNames)) {
            New-ArtifactRootBindingResultCompareCapabilityEntry -CapabilityName $capabilityName -CaseSummaries $caseSummaries
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object capability

    $resolvedCapabilityChangeMatrix = @(
        foreach ($capabilityName in @($resolvedCapabilityNames)) {
            New-ArtifactRootSystemFormationCompareNamedChangeEntry -Name $capabilityName -CaseSummaries $caseSummaries -FieldName 'capability' -CollectionName 'resolved_capability_changes'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object capability

    $unresolvedCapabilityChangeMatrix = @(
        foreach ($capabilityName in @($unresolvedCapabilityNames)) {
            New-ArtifactRootSystemFormationCompareNamedChangeEntry -Name $capabilityName -CaseSummaries $caseSummaries -FieldName 'capability' -CollectionName 'unresolved_capability_changes'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object capability

    return [ordered]@{
        kind = 'binding_result_summary/v0'
        mode = 'comparison'
        compared_case_count = @($caseSummaries).Count
        changed_case_count = @($caseSummaries | Where-Object { [bool]$_.changed }).Count
        unchanged_case_count = @($caseSummaries | Where-Object { -not [bool]$_.changed }).Count
        changed_cases = @(
            @($caseSummaries) |
                Where-Object { [bool]$_.changed } |
                ForEach-Object { [string]$_.case } |
                Sort-Object
        )
        unchanged_cases = @(
            @($caseSummaries) |
                Where-Object { -not [bool]$_.changed } |
                ForEach-Object { [string]$_.case } |
                Sort-Object
        )
        binding_change_count = [int](@($caseSummaries | Measure-Object -Property binding_change_count -Sum).Sum)
        cases = @($caseSummaries)
        summary_change_matrix = @($summaryChangeMatrix)
        capability_change_matrix = @($capabilityChangeMatrix)
        resolved_capability_change_matrix = @($resolvedCapabilityChangeMatrix)
        unresolved_capability_change_matrix = @($unresolvedCapabilityChangeMatrix)
    }
}

function New-ArtifactRootBringupOrderCompareCaseSummary {
    param(
        $LoadedReport
    )

    if ($null -eq $LoadedReport -or $null -eq $LoadedReport.Data) {
        return $null
    }

    $report = $LoadedReport.Data
    $comparison = Get-BringupOrderComparisonFromReport -ReportData $report
    if ($null -eq $comparison) {
        return $null
    }

    return [pscustomobject][ordered]@{
        case = [string]$report.subject.case
        profile = [string]$report.subject.profile
        board = [string]$report.subject.board
        active_facets = @($report.subject.active_facets)
        changed = [bool]$comparison.changed
        summary_changes = @($comparison.summary_changes)
        entry_change_count = @($comparison.entry_changes).Count
        nodes_changed = @(
            @($comparison.entry_changes) |
                ForEach-Object { [string]$_.node } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        blocked_node_changes = [ordered]@{
            added = @($comparison.blocked_node_changes.added)
            removed = @($comparison.blocked_node_changes.removed)
        }
        entry_changes = @($comparison.entry_changes)
    }
}

function New-ArtifactRootBringupOrderCompareEntryChangeEntry {
    param(
        [string]$NodeName,
        [object[]]$CaseSummaries
    )

    if ([string]::IsNullOrWhiteSpace($NodeName)) {
        return $null
    }

    $nodeCases = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        $entryChange = @(
            @($caseSummary.entry_changes) |
                Where-Object { [string]$_.node -eq $NodeName } |
                Select-Object -First 1
        ) | Select-Object -First 1

        if ($null -eq $entryChange) {
            continue
        }

        $nodeCases += [pscustomobject][ordered]@{
            case = [string]$caseSummary.case
            profile = [string]$caseSummary.profile
            board = [string]$caseSummary.board
            change_kind = [string]$entryChange.change_kind
            left_order = $entryChange.left_order
            right_order = $entryChange.right_order
            left_kind = if ([string]::IsNullOrWhiteSpace([string]$entryChange.left_kind)) { $null } else { [string]$entryChange.left_kind }
            right_kind = if ([string]::IsNullOrWhiteSpace([string]$entryChange.right_kind)) { $null } else { [string]$entryChange.right_kind }
            left_phase = if ([string]::IsNullOrWhiteSpace([string]$entryChange.left_phase)) { $null } else { [string]$entryChange.left_phase }
            right_phase = if ([string]::IsNullOrWhiteSpace([string]$entryChange.right_phase)) { $null } else { [string]$entryChange.right_phase }
            left_runlevel_text = if ([string]::IsNullOrWhiteSpace([string]$entryChange.left_runlevel_text)) { $null } else { [string]$entryChange.left_runlevel_text }
            right_runlevel_text = if ([string]::IsNullOrWhiteSpace([string]$entryChange.right_runlevel_text)) { $null } else { [string]$entryChange.right_runlevel_text }
            left_state = if ([string]::IsNullOrWhiteSpace([string]$entryChange.left_state)) { $null } else { [string]$entryChange.left_state }
            right_state = if ([string]::IsNullOrWhiteSpace([string]$entryChange.right_state)) { $null } else { [string]$entryChange.right_state }
            left_provides = @($entryChange.left_provides)
            right_provides = @($entryChange.right_provides)
            left_requires = @($entryChange.left_requires)
            right_requires = @($entryChange.right_requires)
            left_dependency_nodes = @($entryChange.left_dependency_nodes)
            right_dependency_nodes = @($entryChange.right_dependency_nodes)
            left_missing_requires = @($entryChange.left_missing_requires)
            right_missing_requires = @($entryChange.right_missing_requires)
        }
    }

    if (@($nodeCases).Count -eq 0) {
        return $null
    }

    return [ordered]@{
        node = $NodeName
        case_count = @($nodeCases).Count
        change_kinds = @(
            @($nodeCases) |
                ForEach-Object { [string]$_.change_kind } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        states = @(
            @($nodeCases) |
                ForEach-Object { @([string]$_.left_state, [string]$_.right_state) } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        phases = @(
            @($nodeCases) |
                ForEach-Object { @([string]$_.left_phase, [string]$_.right_phase) } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        missing_requires = @(
            @($nodeCases) |
                ForEach-Object { @($_.left_missing_requires) + @($_.right_missing_requires) } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        cases = @($nodeCases | Sort-Object case)
    }
}

function New-ArtifactRootBringupOrderComparisonResult {
    param(
        [object[]]$LoadedReports
    )

    $caseSummaries = @(
        @($LoadedReports) |
            ForEach-Object { New-ArtifactRootBringupOrderCompareCaseSummary -LoadedReport $_ } |
            Where-Object { $null -ne $_ } |
            Sort-Object case
    )

    if (@($caseSummaries).Count -eq 0) {
        return $null
    }

    $summaryChanges = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.summary_changes)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique

    $nodeNames = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.nodes_changed)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique

    $blockedNodeNames = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.blocked_node_changes.added)
            @($caseSummary.blocked_node_changes.removed)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique

    $summaryChangeMatrix = @(
        foreach ($changeText in @($summaryChanges)) {
            New-ArtifactRootResourceCompareSummaryChangeEntry -ChangeText $changeText -CaseSummaries $caseSummaries
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object change

    $entryChangeMatrix = @(
        foreach ($nodeName in @($nodeNames)) {
            New-ArtifactRootBringupOrderCompareEntryChangeEntry -NodeName $nodeName -CaseSummaries $caseSummaries
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object node

    $blockedNodeChangeMatrix = @(
        foreach ($nodeName in @($blockedNodeNames)) {
            New-ArtifactRootSystemFormationCompareNamedChangeEntry -Name $nodeName -CaseSummaries $caseSummaries -FieldName 'node' -CollectionName 'blocked_node_changes'
        }
    ) | Where-Object { $null -ne $_ } | Sort-Object node

    return [ordered]@{
        kind = 'bringup_order_summary/v0'
        mode = 'comparison'
        compared_case_count = @($caseSummaries).Count
        changed_case_count = @($caseSummaries | Where-Object { [bool]$_.changed }).Count
        unchanged_case_count = @($caseSummaries | Where-Object { -not [bool]$_.changed }).Count
        changed_cases = @(
            @($caseSummaries) |
                Where-Object { [bool]$_.changed } |
                ForEach-Object { [string]$_.case } |
                Sort-Object
        )
        unchanged_cases = @(
            @($caseSummaries) |
                Where-Object { -not [bool]$_.changed } |
                ForEach-Object { [string]$_.case } |
                Sort-Object
        )
        entry_change_count = [int](@($caseSummaries | Measure-Object -Property entry_change_count -Sum).Sum)
        cases = @($caseSummaries)
        summary_change_matrix = @($summaryChangeMatrix)
        entry_change_matrix = @($entryChangeMatrix)
        blocked_node_change_matrix = @($blockedNodeChangeMatrix)
    }
}

function Format-BringupEvidenceDisplayRow {
    param(
        $Entry
    )

    return [pscustomobject]@{
        Capability = [string]$Entry.capability
        Dec = Format-BoolFlag -Value ([bool]$Entry.declared)
        Mat = Format-BoolFlag -Value ([bool]$Entry.materialized)
        Obs = Format-BoolFlag -Value ([bool]$Entry.observed)
        Pub = Format-BoolFlag -Value ([bool]$Entry.published)
        Blk = Format-BoolFlag -Value ([bool]$Entry.blocked)
        Fail = Format-BoolFlag -Value ([bool]$Entry.failed)
        PubState = Format-OptionalState -Value ([string]$Entry.publish_state)
        ExpState = Format-OptionalState -Value ([string]$Entry.export_state)
        Providers = Format-StringArray @($Entry.provider_nodes)
        Consumers = Format-StringArray @($Entry.consumer_nodes)
    }
}

function New-ComparisonOverviewCaseSummary {
    param(
        $LoadedReport
    )

    $report = $LoadedReport.Data
    $hasComparison = ($null -ne $report.PSObject.Properties['comparison'] -and $null -ne $report.comparison)
    $systemInputComparison = Get-SystemInputComparisonFromReport -ReportData $report
    $systemFormationComparison = Get-SystemFormationComparisonFromReport -ReportData $report
    $bindingResultComparison = Get-BindingResultComparisonFromReport -ReportData $report
    $bringupOrderComparison = Get-BringupOrderComparisonFromReport -ReportData $report
    $bringupComparison = Get-BringupEvidenceComparisonFromReport -ReportData $report
    $resourceComparison = Get-ResourceContractComparisonFromReport -ReportData $report
    $factResolutionComparison = Get-FactResolutionComparisonFromReport -ReportData $report

    return [pscustomobject][ordered]@{
        case = [string]$report.subject.case
        profile = [string]$report.subject.profile
        board = [string]$report.subject.board
        compared = $hasComparison
        compare_status = Get-ComparisonStatus -ReportData $report
        metadata_change_count = [int](Get-MetadataChangeCount -ReportData $report)
        input_changed = ($null -ne $systemInputComparison -and [bool]$systemInputComparison.changed)
        input_change_count = Get-SystemInputComparisonChangeCount -SystemInputComparison $systemInputComparison
        formation_changed = ($null -ne $systemFormationComparison -and [bool]$systemFormationComparison.changed)
        formation_change_count = Get-SystemFormationComparisonChangeCount -SystemFormationComparison $systemFormationComparison
        binding_result_changed = ($null -ne $bindingResultComparison -and [bool]$bindingResultComparison.changed)
        binding_result_change_count = if ($null -ne $bindingResultComparison) { [int]@($bindingResultComparison.binding_changes).Count } else { 0 }
        bringup_order_changed = ($null -ne $bringupOrderComparison -and [bool]$bringupOrderComparison.changed)
        bringup_order_change_count = if ($null -ne $bringupOrderComparison) { [int]@($bringupOrderComparison.entry_changes).Count } else { 0 }
        bringup_changed = ($null -ne $bringupComparison -and [bool]$bringupComparison.changed)
        bringup_change_count = if ($null -ne $bringupComparison) { [int]@($bringupComparison.capability_changes).Count } else { 0 }
        resource_changed = ($null -ne $resourceComparison -and [bool]$resourceComparison.changed)
        resource_change_count = if ($null -ne $resourceComparison) { [int]@($resourceComparison.contract_changes).Count } else { 0 }
        fact_resolution_changed = ($null -ne $factResolutionComparison -and [bool]$factResolutionComparison.changed)
        fact_resolution_change_count = if ($null -ne $factResolutionComparison) { [int]@($factResolutionComparison.contract_changes).Count } else { 0 }
    }
}

function New-ArtifactRootComparisonOverviewResult {
    param(
        [object[]]$LoadedReports,
        $CapabilityComparisonSummary,
        $SystemCompilerComparisonSummary,
        $SystemInputComparisonSummary,
        $BindingResultComparisonSummary,
        $BringupOrderComparisonSummary,
        $SystemFormationComparisonSummary,
        $FactResolutionComparisonSummary
    )

    $caseSummaries = @(
        @($LoadedReports) |
            ForEach-Object { New-ComparisonOverviewCaseSummary -LoadedReport $_ } |
            Sort-Object case
    )

    $comparedCases = @(
        @($caseSummaries) |
            Where-Object { [bool]$_.compared } |
            Sort-Object case
    )

    if (@($comparedCases).Count -eq 0) {
        return $null
    }

    $statusCounts = [ordered]@{}
    foreach ($caseSummary in @($comparedCases)) {
        $statusName = [string]$caseSummary.compare_status
        if ([string]::IsNullOrWhiteSpace($statusName)) {
            $statusName = 'unknown'
        }

        if ($statusCounts.Contains($statusName)) {
            $statusCounts[$statusName] += 1
        } else {
            $statusCounts[$statusName] = 1
        }
    }

    $result = [ordered]@{
        compared_case_count = @($comparedCases).Count
        status_counts = $statusCounts
        metadata_changed_case_count = @($comparedCases | Where-Object { [int]$_.metadata_change_count -gt 0 }).Count
        input_changed_case_count = @($comparedCases | Where-Object { [bool]$_.input_changed }).Count
        system_formation_changed_case_count = @($comparedCases | Where-Object { [bool]$_.formation_changed }).Count
        binding_result_changed_case_count = @($comparedCases | Where-Object { [bool]$_.binding_result_changed }).Count
        bringup_order_changed_case_count = @($comparedCases | Where-Object { [bool]$_.bringup_order_changed }).Count
        bringup_changed_case_count = @($comparedCases | Where-Object { [bool]$_.bringup_changed }).Count
        resource_changed_case_count = @($comparedCases | Where-Object { [bool]$_.resource_changed }).Count
        fact_resolution_changed_case_count = @($comparedCases | Where-Object { [bool]$_.fact_resolution_changed }).Count
        compared_cases = @($comparedCases | ForEach-Object { [string]$_.case })
        metadata_changed_cases = @(
            @($comparedCases) |
                Where-Object { [int]$_.metadata_change_count -gt 0 } |
                ForEach-Object { [string]$_.case }
        )
        input_changed_cases = @(
            @($comparedCases) |
                Where-Object { [bool]$_.input_changed } |
                ForEach-Object { [string]$_.case }
        )
        system_formation_changed_cases = @(
            @($comparedCases) |
                Where-Object { [bool]$_.formation_changed } |
                ForEach-Object { [string]$_.case }
        )
        binding_result_changed_cases = @(
            @($comparedCases) |
                Where-Object { [bool]$_.binding_result_changed } |
                ForEach-Object { [string]$_.case }
        )
        bringup_order_changed_cases = @(
            @($comparedCases) |
                Where-Object { [bool]$_.bringup_order_changed } |
                ForEach-Object { [string]$_.case }
        )
        bringup_changed_cases = @(
            @($comparedCases) |
                Where-Object { [bool]$_.bringup_changed } |
                ForEach-Object { [string]$_.case }
        )
        resource_changed_cases = @(
            @($comparedCases) |
                Where-Object { [bool]$_.resource_changed } |
                ForEach-Object { [string]$_.case }
        )
        fact_resolution_changed_cases = @(
            @($comparedCases) |
                Where-Object { [bool]$_.fact_resolution_changed } |
                ForEach-Object { [string]$_.case }
        )
    }

    if ($null -ne $CapabilityComparisonSummary) {
        $result.capability_summary = $CapabilityComparisonSummary
    }

    if ($null -ne $SystemCompilerComparisonSummary) {
        $result.system_compiler_summary = $SystemCompilerComparisonSummary
    }

    if ($null -ne $SystemInputComparisonSummary) {
        $result.system_input_summary = $SystemInputComparisonSummary
    }

    if ($null -ne $SystemFormationComparisonSummary) {
        $result.system_formation_summary = $SystemFormationComparisonSummary
    }

    if ($null -ne $BindingResultComparisonSummary) {
        $result.binding_result_summary = $BindingResultComparisonSummary
    }

    if ($null -ne $BringupOrderComparisonSummary) {
        $result.bringup_order_summary = $BringupOrderComparisonSummary
    }

    if ($null -ne $FactResolutionComparisonSummary) {
        $result.fact_resolution_summary = $FactResolutionComparisonSummary
    }

    return $result
}

function New-CaseSummaryRow {
    param(
        $LoadedReport
    )

    $report = $LoadedReport.Data
    $systemInputComparison = Get-SystemInputComparisonFromReport -ReportData $report
    $systemFormationComparison = Get-SystemFormationComparisonFromReport -ReportData $report
    $bindingResultComparison = Get-BindingResultComparisonFromReport -ReportData $report
    $bringupOrderComparison = Get-BringupOrderComparisonFromReport -ReportData $report
    $bringupComparison = Get-BringupEvidenceComparisonFromReport -ReportData $report
    $resourceComparison = Get-ResourceContractComparisonFromReport -ReportData $report
    return [pscustomobject]@{
        Case = [string]$report.subject.case
        Mode = [string]$report.mode
        Profile = [string]$report.subject.profile
        Board = [string]$report.subject.board
        Facets = Format-StringArray @($report.subject.active_facets)
        Nodes = [int]$report.structure.node_count
        Edges = [int]$report.structure.edge_count
        Unresolved = @($report.structure.unresolved_bindings).Count
        Contracts = [int]$report.resource_contract.declared_contracts
        Satisfied = [int]$report.resource_contract.satisfied_count
        Violated = [int]$report.resource_contract.violated_count
        Unknown = [int]$report.resource_contract.unknown_count
        Formation = if ($null -ne $report.PSObject.Properties['system_formation'] -and $null -ne $report.system_formation) { [string]$report.system_formation.status } else { $null }
        Compare = Get-ComparisonStatus -ReportData $report
        Metadata = Get-MetadataChangeCount -ReportData $report
        InpCmp = Get-SystemInputComparisonChangeCount -SystemInputComparison $systemInputComparison
        FormCmp = Get-SystemFormationComparisonChangeCount -SystemFormationComparison $systemFormationComparison
        BindCmp = if ($null -ne $bindingResultComparison -and [bool]$bindingResultComparison.changed) { [int]@($bindingResultComparison.binding_changes).Count } else { 0 }
        OrdCmp = if ($null -ne $bringupOrderComparison -and [bool]$bringupOrderComparison.changed) { [int]@($bringupOrderComparison.entry_changes).Count } else { 0 }
        BrCmp = if ($null -ne $bringupComparison -and [bool]$bringupComparison.changed) { [int]@($bringupComparison.capability_changes).Count } else { 0 }
        ResCmp = if ($null -ne $resourceComparison -and [bool]$resourceComparison.changed) { [int]@($resourceComparison.contract_changes).Count } else { 0 }
    }
}

function New-ArtifactJsonView {
    param(
        $LoadedReport
    )

    $report = $LoadedReport.Data
    $comparison = New-ReportComparisonOverviewResult -LoadedReport $LoadedReport
    return [ordered]@{
        report_path = $LoadedReport.Path
        summary = New-CaseSummaryRow -LoadedReport $LoadedReport
        subject = $report.subject
        system_input = $report.system_input
        structure = $report.structure
        binding_result = $report.binding_result
        bringup_order = $report.bringup_order
        system_formation = $report.system_formation
        bringup_evidence = $report.bringup_evidence
        resource_contract = $report.resource_contract
        fact_resolution = New-ResourceSummaryResult -ReportData $report -GraphInfo $graphInfo
        runtime_observe = $report.runtime_observe
        comparison = $comparison
        artifacts = $report.artifacts
    }
}

function New-ReportComparisonOverviewResult {
    param(
        $LoadedReport
    )

    if ($null -eq $LoadedReport -or $null -eq $LoadedReport.Data) {
        return $null
    }

    $report = $LoadedReport.Data
    if ($null -eq $report.PSObject.Properties['comparison'] -or $null -eq $report.comparison) {
        return $null
    }

    $comparisonOverview = [ordered]@{}
    foreach ($property in @($report.comparison.PSObject.Properties)) {
        $comparisonOverview[$property.Name] = $property.Value
    }

    $graphInfo = Load-GraphFromArtifactReport -ReportData $report
    $capabilities = @(Get-CapListEntries -ReportData $report -GraphInfo $graphInfo)
    $capabilitySummary = New-CapListComparisonSummaryResult -Capabilities $capabilities
    if ($null -ne $capabilitySummary) {
        $comparisonOverview.capability_summary = $capabilitySummary
    }

    return $comparisonOverview
}

$selectedReports = @(Get-SelectedReports -ArtifactRootPath $ArtifactRoot)
$artifactRootPath = if (-not [string]::IsNullOrWhiteSpace($Report)) {
    Split-Path -Parent (Resolve-FullPath $Report)
} else {
    Resolve-FullPath $ArtifactRoot
}

if ($ResourceSummary -and -not [string]::IsNullOrWhiteSpace($WhyCapability)) {
    throw "-ResourceSummary cannot be combined with -WhyCapability"
}

if ($ResourceSummary -and -not [string]::IsNullOrWhiteSpace($GraphPath)) {
    throw "-ResourceSummary cannot be combined with -GraphPath"
}

if ($ResourceSummary -and $CapList) {
    throw "-ResourceSummary cannot be combined with -CapList"
}

if ($RecentTransitions -and -not [string]::IsNullOrWhiteSpace($WhyCapability)) {
    throw "-RecentTransitions cannot be combined with -WhyCapability"
}

if ($RecentTransitions -and -not [string]::IsNullOrWhiteSpace($GraphPath)) {
    throw "-RecentTransitions cannot be combined with -GraphPath"
}

if ($RecentTransitions -and $CapList) {
    throw "-RecentTransitions cannot be combined with -CapList"
}

if ($RecentTransitions -and $ResourceSummary) {
    throw "-RecentTransitions cannot be combined with -ResourceSummary"
}

if ($BringupEvidence -and -not [string]::IsNullOrWhiteSpace($WhyCapability)) {
    throw "-BringupEvidence cannot be combined with -WhyCapability"
}

if ($BringupEvidence -and -not [string]::IsNullOrWhiteSpace($GraphPath)) {
    throw "-BringupEvidence cannot be combined with -GraphPath"
}

if ($BringupEvidence -and $CapList) {
    throw "-BringupEvidence cannot be combined with -CapList"
}

if ($BringupEvidence -and $ResourceSummary) {
    throw "-BringupEvidence cannot be combined with -ResourceSummary"
}

if ($BringupEvidence -and $RecentTransitions) {
    throw "-BringupEvidence cannot be combined with -RecentTransitions"
}

if ($CapList -and -not [string]::IsNullOrWhiteSpace($WhyCapability)) {
    throw "-CapList cannot be combined with -WhyCapability"
}

if ($CapList -and -not [string]::IsNullOrWhiteSpace($GraphPath)) {
    throw "-CapList cannot be combined with -GraphPath"
}

if (-not [string]::IsNullOrWhiteSpace($WhyCapability) -and -not [string]::IsNullOrWhiteSpace($GraphPath)) {
    throw "-WhyCapability cannot be combined with -GraphPath"
}

if ($CapList -and $selectedReports.Count -gt 1 -and $Case.Count -gt 0) {
    throw "-CapList only supports a single selected report or full artifact root aggregation"
}

if (-not [string]::IsNullOrWhiteSpace($WhyCapability) -and $selectedReports.Count -gt 1 -and $Case.Count -gt 0) {
    throw "-WhyCapability only supports a single selected report or full artifact root aggregation"
}

if ($ListCases) {
    if ($AsJson) {
        @($selectedReports | ForEach-Object { [string]$_.Data.subject.case }) | ConvertTo-Json -Depth 2
    } else {
        Write-Host "[ARTIFACT ROOT] $artifactRootPath"
        $selectedReports | ForEach-Object { [string]$_.Data.subject.case }
    }
    exit 0
}

$summaryRows = @($selectedReports | ForEach-Object { New-CaseSummaryRow -LoadedReport $_ })

if (-not [string]::IsNullOrWhiteSpace($GraphPath) -and $selectedReports.Count -ne 1) {
    throw "-GraphPath requires exactly one selected artifact report"
}

if ($RecentTransitions -and $selectedReports.Count -ne 1) {
    throw "-RecentTransitions requires exactly one selected artifact report"
}

if ($CapList) {
    if ($selectedReports.Count -eq 1) {
        $capListView = New-CapListReportView -LoadedReport $selectedReports[0]
        if ($AsJson) {
            $capListView | ConvertTo-Json -Depth 8
        } else {
            Write-Host "[ARTIFACT ROOT] $artifactRootPath"
            Write-Host "[REPORT] $($capListView.report_path)"
            Write-Host "[CASE] $([string]($capListView.subject.case))"
            Write-Host "[CAP LIST]"
            if ($null -ne $capListView.query.comparison) {
                Write-Host "compare_capabilities = $([int]$capListView.query.comparison.compared_capability_count) bringup_compare = $([int]$capListView.query.comparison.bringup_compare_capability_count) resource_compare = $([int]$capListView.query.comparison.resource_compare_capability_count)"
            }
            @($capListView.query.capabilities) |
                ForEach-Object { Format-CapListDisplayRow -Entry $_ } |
                Format-Table -Wrap -AutoSize Capability, Mat, Obs, Pub, Req, DecFact, ResFact, Unres, BrCmp, ResCmp, Providers, Consumers |
                Out-Host
        }
        exit 0
    }

    $capListView = New-CapListArtifactRootView -LoadedReports $selectedReports -ArtifactRootPath $artifactRootPath
    if ($AsJson) {
        $capListView | ConvertTo-Json -Depth 8
    } else {
        Write-Host "[ARTIFACT ROOT] $artifactRootPath"
        Write-Host "[CAP LIST] scope=artifact_root cases=$([int]$capListView.query.case_count)"
        if ($null -ne $capListView.query.comparison) {
            Write-Host "compare_capabilities = $([int]$capListView.query.comparison.compared_capability_count) bringup_compare = $([int]$capListView.query.comparison.bringup_compare_capability_count) resource_compare = $([int]$capListView.query.comparison.resource_compare_capability_count)"
        }
        @($capListView.query.capabilities) |
            ForEach-Object { Format-AggregatedCapListDisplayRow -Entry $_ } |
            Format-List Capability, Cases, Mat, Obs, Pub, Req, DecFact, ResFact, Unres, BrCmp, ResCmp, ResCtr, Providers, Consumers |
            Out-Host
    }
    exit 0
}

if ($selectedReports.Count -ne 1 -and -not $ResourceSummary -and -not $BringupEvidence) {
    if (-not [string]::IsNullOrWhiteSpace($WhyCapability)) {
        $artifactRootWhyResult = New-ArtifactRootWhyCapabilityResult -LoadedReports $selectedReports -CapabilityName $WhyCapability
        if ($AsJson) {
            [ordered]@{
                artifact_root = $artifactRootPath
                query = [ordered]@{
                    kind = 'why_capability'
                    scope = 'artifact_root'
                    result = $artifactRootWhyResult
                }
            } | ConvertTo-Json -Depth 10
        } else {
            Write-Host "[ARTIFACT ROOT] $artifactRootPath"
            Write-Host "[WHY CAPABILITY] $WhyCapability scope=artifact_root cases=$([int]$artifactRootWhyResult.case_count)"
            if ($null -ne $artifactRootWhyResult.state_counts -and $artifactRootWhyResult.state_counts.Count -gt 0) {
                $stateSummary = @(
                    foreach ($stateName in @($artifactRootWhyResult.state_counts.Keys)) {
                        ('{0}:{1}' -f $stateName, [int]$artifactRootWhyResult.state_counts[$stateName])
                    }
                ) -join ', '
                Write-Host "state_counts = $stateSummary"
            }
            Write-Host "compare_cases = $([int]$artifactRootWhyResult.compared_case_count) bringup_compare = $([int]$artifactRootWhyResult.bringup_compare_case_count) resource_compare = $([int]$artifactRootWhyResult.resource_compare_case_count)"
            if (@($artifactRootWhyResult.resource_contracts).Count -gt 0) {
                Write-Host "resource_contracts = $((@($artifactRootWhyResult.resource_contracts) -join ', '))"
            }
            @($artifactRootWhyResult.cases) |
                ForEach-Object { Format-ArtifactRootWhyCapabilityDisplayRow -Entry $_ } |
                Format-List Case, State, Compare, BrCmp, ResCmp, Contracts, PubState, ExpState, Providers, Consumers, Reasons |
                Out-Host
        }
        exit 0
    }

    $comparisonCapabilitySummary = $null
    $hasComparisonReports = @(
        @($selectedReports) |
            Where-Object { $null -ne $_.Data.PSObject.Properties['comparison'] -and $null -ne $_.Data.comparison }
    ).Count -gt 0
    if ($hasComparisonReports) {
        $comparisonCapabilitySummary = New-CapListComparisonSummaryResult -Capabilities @(
            (New-CapListArtifactRootAggregationResult -LoadedReports $selectedReports).capabilities
        )
    }

    $systemCompilerSummary = New-ArtifactRootSystemCompilerSummaryResult -LoadedReports $selectedReports
    $systemInputSummary = New-ArtifactRootSystemInputSummaryResult -LoadedReports $selectedReports
    $bindingResultSummary = New-ArtifactRootBindingResultSummaryResult -LoadedReports $selectedReports
    $bringupOrderSummary = New-ArtifactRootBringupOrderSummaryResult -LoadedReports $selectedReports
    $systemFormationSummary = New-ArtifactRootSystemFormationSummaryResult -LoadedReports $selectedReports
    $factResolutionSummary = New-ArtifactRootFactResolutionSummaryResult -LoadedReports $selectedReports
    $systemCompilerComparisonSummary = New-ArtifactRootSystemCompilerComparisonResult -LoadedReports $selectedReports
    $systemInputComparisonSummary = New-ArtifactRootSystemInputComparisonResult -LoadedReports $selectedReports
    $bindingResultComparisonSummary = New-ArtifactRootBindingResultComparisonResult -LoadedReports $selectedReports
    $bringupOrderComparisonSummary = New-ArtifactRootBringupOrderComparisonResult -LoadedReports $selectedReports
    $systemFormationComparisonSummary = New-ArtifactRootSystemFormationComparisonResult -LoadedReports $selectedReports
    $factResolutionComparisonSummary = New-ArtifactRootFactResolutionComparisonResult -LoadedReports $selectedReports
    $comparisonOverview = New-ArtifactRootComparisonOverviewResult `
        -LoadedReports $selectedReports `
        -CapabilityComparisonSummary $comparisonCapabilitySummary `
        -SystemCompilerComparisonSummary $systemCompilerComparisonSummary `
        -SystemInputComparisonSummary $systemInputComparisonSummary `
        -BindingResultComparisonSummary $bindingResultComparisonSummary `
        -BringupOrderComparisonSummary $bringupOrderComparisonSummary `
        -SystemFormationComparisonSummary $systemFormationComparisonSummary `
        -FactResolutionComparisonSummary $factResolutionComparisonSummary
    if ($AsJson) {
        $payload = [ordered]@{
            artifact_root = $artifactRootPath
            case_count = $summaryRows.Count
            cases = $summaryRows
        }
        if ($null -ne $systemCompilerSummary) {
            $payload.system_compiler_summary = $systemCompilerSummary
        }
        if ($null -ne $systemInputSummary) {
            $payload.system_input_summary = $systemInputSummary
        }
        if ($null -ne $bindingResultSummary) {
            $payload.binding_result_summary = $bindingResultSummary
        }
        if ($null -ne $bringupOrderSummary) {
            $payload.bringup_order_summary = $bringupOrderSummary
        }
        if ($null -ne $systemFormationSummary) {
            $payload.system_formation_summary = $systemFormationSummary
        }
        if ($null -ne $factResolutionSummary) {
            $payload.fact_resolution_summary = $factResolutionSummary
        }
        if ($null -ne $comparisonOverview) {
            $payload.comparison = $comparisonOverview
        }
        $payload | ConvertTo-Json -Depth 14
    } else {
        Write-Host "[ARTIFACT ROOT] $artifactRootPath"
        if ($null -ne $systemCompilerSummary) {
            Write-Host '[SYSTEM COMPILER SUMMARY]'
            Write-Host "case_count              = $([int]$systemCompilerSummary.case_count)"
            Write-Host "formed_case_count       = $([int]$systemCompilerSummary.formed_case_count)"
            Write-Host "blocked_case_count      = $([int]$systemCompilerSummary.blocked_case_count)"
            Write-Host "declared_fact_count     = $([int]$systemCompilerSummary.totals.declared_fact_count)"
            Write-Host "declared_contract_count = $([int]$systemCompilerSummary.totals.declared_contract_count)"
            Write-Host "subject_fact_count      = $([int]$systemCompilerSummary.totals.subject_fact_count)"
            Write-Host "required_binding_count  = $([int]$systemCompilerSummary.totals.required_binding_count)"
            Write-Host "unresolved_binding_count = $([int]$systemCompilerSummary.totals.unresolved_binding_count)"
            Write-Host "ordered_node_count      = $([int]$systemCompilerSummary.totals.ordered_node_count)"
            Write-Host "blocked_node_count      = $([int]$systemCompilerSummary.totals.blocked_node_count)"
            Write-Host "blocker_count           = $([int]$systemCompilerSummary.totals.blocker_count)"
            if (@($systemCompilerSummary.case_kind_matrix).Count -gt 0) {
                Write-Host "case_kinds              = $((@($systemCompilerSummary.case_kind_matrix | ForEach-Object { [string]$_.case_kind }) -join ', '))"
            }
            if (@($systemCompilerSummary.resolved_profile_matrix).Count -gt 0) {
                Write-Host "resolved_profiles       = $((@($systemCompilerSummary.resolved_profile_matrix | ForEach-Object { [string]$_.profile }) -join ', '))"
            }
            if (@($systemCompilerSummary.resolved_board_matrix).Count -gt 0) {
                Write-Host "resolved_boards         = $((@($systemCompilerSummary.resolved_board_matrix | ForEach-Object { [string]$_.board }) -join ', '))"
            }
            if (@($systemCompilerSummary.resolved_active_facet_matrix).Count -gt 0) {
                Write-Host "resolved_active_facets  = $((@($systemCompilerSummary.resolved_active_facet_matrix | ForEach-Object { [string]$_.facet }) -join ', '))"
            }
            if (@($systemCompilerSummary.unresolved_capability_matrix).Count -gt 0) {
                Write-Host "unresolved_capabilities = $((@($systemCompilerSummary.unresolved_capability_matrix | ForEach-Object { [string]$_.capability }) -join ', '))"
            }
            if (@($systemCompilerSummary.blocked_node_matrix).Count -gt 0) {
                Write-Host "blocked_nodes           = $((@($systemCompilerSummary.blocked_node_matrix | ForEach-Object { [string]$_.node }) -join ', '))"
            }
            if (@($systemCompilerSummary.blocker_matrix).Count -gt 0) {
                Write-Host "blockers                = $((@($systemCompilerSummary.blocker_matrix | ForEach-Object { ('{0}:{1}' -f [string]$_.kind, [string]$_.name) }) -join ', '))"
            }
            if (@($systemCompilerSummary.blocker_reason_matrix).Count -gt 0) {
                Write-Host "blocker_reasons         = $((@($systemCompilerSummary.blocker_reason_matrix | ForEach-Object { [string]$_.reason }) -join ', '))"
            }
            if (@($systemCompilerSummary.blocker_missing_requires_matrix).Count -gt 0) {
                Write-Host "blocker_missing_requires = $((@($systemCompilerSummary.blocker_missing_requires_matrix | ForEach-Object { [string]$_.require }) -join ', '))"
            }
            if (@($systemCompilerSummary.blocker_depends_on_matrix).Count -gt 0) {
                Write-Host "blocker_depends_on      = $((@($systemCompilerSummary.blocker_depends_on_matrix | ForEach-Object { [string]$_.node }) -join ', '))"
            }
            if (@($systemCompilerSummary.binding_reason_matrix).Count -gt 0) {
                Write-Host "binding_reasons         = $((@($systemCompilerSummary.binding_reason_matrix | ForEach-Object { [string]$_.reason }) -join ', '))"
            }
            if (@($systemCompilerSummary.bringup_phase_matrix).Count -gt 0) {
                Write-Host "bringup_phases          = $((@($systemCompilerSummary.bringup_phase_matrix | ForEach-Object { [string]$_.phase }) -join ', '))"
            }
            if (@($systemCompilerSummary.bringup_dependency_matrix).Count -gt 0) {
                Write-Host "bringup_dependencies    = $((@($systemCompilerSummary.bringup_dependency_matrix | ForEach-Object { [string]$_.node }) -join ', '))"
            }
            Write-Host ''
        }
        if ($null -ne $systemInputSummary) {
            Write-Host '[SYSTEM INPUT SUMMARY]'
            Write-Host "case_count              = $([int]$systemInputSummary.case_count)"
            Write-Host "declared_fact_count     = $([int]$systemInputSummary.totals.declared_fact_count)"
            Write-Host "declared_contract_count = $([int]$systemInputSummary.totals.declared_contract_count)"
            Write-Host "subject_fact_count      = $([int]$systemInputSummary.totals.subject_fact_count)"
            if (@($systemInputSummary.case_kind_matrix).Count -gt 0) {
                Write-Host "case_kinds              = $((@($systemInputSummary.case_kind_matrix | ForEach-Object { [string]$_.case_kind }) -join ', '))"
            }
            if (@($systemInputSummary.declared_profile_matrix).Count -gt 0) {
                Write-Host "declared_profiles       = $((@($systemInputSummary.declared_profile_matrix | ForEach-Object { [string]$_.profile }) -join ', '))"
            }
            if (@($systemInputSummary.declared_board_matrix).Count -gt 0) {
                Write-Host "declared_boards         = $((@($systemInputSummary.declared_board_matrix | ForEach-Object { [string]$_.board }) -join ', '))"
            }
            if (@($systemInputSummary.resolved_profile_matrix).Count -gt 0) {
                Write-Host "resolved_profiles       = $((@($systemInputSummary.resolved_profile_matrix | ForEach-Object { [string]$_.profile }) -join ', '))"
            }
            if (@($systemInputSummary.resolved_board_matrix).Count -gt 0) {
                Write-Host "resolved_boards         = $((@($systemInputSummary.resolved_board_matrix | ForEach-Object { [string]$_.board }) -join ', '))"
            }
            if (@($systemInputSummary.resolved_active_facet_matrix).Count -gt 0) {
                Write-Host "resolved_active_facets  = $((@($systemInputSummary.resolved_active_facet_matrix | ForEach-Object { [string]$_.facet }) -join ', '))"
            }
            Write-Host ''
        }
        if ($null -ne $bindingResultSummary) {
            Write-Host '[BINDING RESULT SUMMARY]'
            Write-Host "case_count              = $([int]$bindingResultSummary.case_count)"
            Write-Host "required_binding_count  = $([int]$bindingResultSummary.totals.required_binding_count)"
            Write-Host "resolved_binding_count  = $([int]$bindingResultSummary.totals.resolved_binding_count)"
            Write-Host "unresolved_binding_count = $([int]$bindingResultSummary.totals.unresolved_binding_count)"
            if (@($bindingResultSummary.resolved_capability_matrix).Count -gt 0) {
                Write-Host "resolved_capabilities   = $((@($bindingResultSummary.resolved_capability_matrix | ForEach-Object { [string]$_.capability }) -join ', '))"
            }
            if (@($bindingResultSummary.unresolved_capability_matrix).Count -gt 0) {
                Write-Host "unresolved_capabilities = $((@($bindingResultSummary.unresolved_capability_matrix | ForEach-Object { [string]$_.capability }) -join ', '))"
            }
            Write-Host ''
        }
        if ($null -ne $bringupOrderSummary) {
            Write-Host '[BRINGUP ORDER SUMMARY]'
            Write-Host "case_count              = $([int]$bringupOrderSummary.case_count)"
            Write-Host "ordered_node_count      = $([int]$bringupOrderSummary.totals.ordered_node_count)"
            Write-Host "blocked_node_count      = $([int]$bringupOrderSummary.totals.blocked_node_count)"
            if ($null -ne $bringupOrderSummary.phase_counts -and @($bringupOrderSummary.phase_counts.PSObject.Properties).Count -gt 0) {
                $phaseParts = @()
                foreach ($phaseEntry in @($bringupOrderSummary.phase_counts.PSObject.Properties)) {
                    $phaseParts += "$([string]$phaseEntry.Name):$([int]$phaseEntry.Value)"
                }
                Write-Host "phase_counts            = $((@($phaseParts) -join ', '))"
            }
            if (@($bringupOrderSummary.blocked_node_matrix).Count -gt 0) {
                Write-Host "blocked_nodes           = $((@($bringupOrderSummary.blocked_node_matrix | ForEach-Object { [string]$_.node }) -join ', '))"
            }
            Write-Host ''
        }
        if ($null -ne $systemFormationSummary) {
            Write-Host '[SYSTEM FORMATION SUMMARY]'
            Write-Host "case_count              = $([int]$systemFormationSummary.case_count)"
            Write-Host "formed_case_count       = $([int]$systemFormationSummary.formed_case_count)"
            Write-Host "blocked_case_count      = $([int]$systemFormationSummary.blocked_case_count)"
            Write-Host "required_binding_count  = $([int]$systemFormationSummary.totals.required_binding_count)"
            Write-Host "resolved_binding_count  = $([int]$systemFormationSummary.totals.resolved_binding_count)"
            Write-Host "unresolved_binding_count = $([int]$systemFormationSummary.totals.unresolved_binding_count)"
            Write-Host "ordered_node_count      = $([int]$systemFormationSummary.totals.ordered_node_count)"
            Write-Host "blocked_node_count      = $([int]$systemFormationSummary.totals.blocked_node_count)"
            Write-Host "blocker_count           = $([int]$systemFormationSummary.totals.blocker_count)"
            if (@($systemFormationSummary.formed_cases).Count -gt 0) {
                Write-Host "formed_cases            = $((@($systemFormationSummary.formed_cases) -join ', '))"
            }
            if (@($systemFormationSummary.blocked_cases).Count -gt 0) {
                Write-Host "blocked_cases           = $((@($systemFormationSummary.blocked_cases) -join ', '))"
            }
            if (@($systemFormationSummary.unresolved_capability_matrix).Count -gt 0) {
                Write-Host "unresolved_capabilities = $((@($systemFormationSummary.unresolved_capability_matrix | ForEach-Object { [string]$_.capability }) -join ', '))"
            }
            if (@($systemFormationSummary.blocked_node_matrix).Count -gt 0) {
                Write-Host "blocked_nodes           = $((@($systemFormationSummary.blocked_node_matrix | ForEach-Object { [string]$_.node }) -join ', '))"
            }
            if (@($systemFormationSummary.blocker_matrix).Count -gt 0) {
                Write-Host "blockers                = $((@($systemFormationSummary.blocker_matrix | ForEach-Object { ('{0}:{1}' -f [string]$_.kind, [string]$_.name) }) -join ', '))"
            }
            Write-Host ''
        }
        if ($null -ne $factResolutionSummary) {
            Write-Host '[FACT RESOLUTION SUMMARY]'
            Write-Host "case_count              = $([int]$factResolutionSummary.case_count)"
            Write-Host "declared_contracts      = $([int]$factResolutionSummary.totals.declared_contracts)"
            Write-Host "audited_count           = $([int]$factResolutionSummary.totals.audited_count)"
            Write-Host "satisfied_count         = $([int]$factResolutionSummary.totals.satisfied_count)"
            Write-Host "violated_count          = $([int]$factResolutionSummary.totals.violated_count)"
            Write-Host "unknown_count           = $([int]$factResolutionSummary.totals.unknown_count)"
            if (@($factResolutionSummary.required_fact_matrix).Count -gt 0) {
                Write-Host "required_facts          = $((@($factResolutionSummary.required_fact_matrix | ForEach-Object { [string]$_.fact }) -join ', '))"
            }
            if (@($factResolutionSummary.provided_fact_matrix).Count -gt 0) {
                Write-Host "audit_provided_facts    = $((@($factResolutionSummary.provided_fact_matrix | ForEach-Object { [string]$_.fact }) -join ', '))"
            }
            Write-Host ''
        }
        if ($null -ne $comparisonOverview) {
            Write-Host '[COMPARISON]'
            Write-Host "compared_case_count      = $([int]$comparisonOverview.compared_case_count)"
            Write-Host "metadata_changed_cases   = $([int]$comparisonOverview.metadata_changed_case_count)"
            Write-Host "input_changed_cases      = $([int]$comparisonOverview.input_changed_case_count)"
            Write-Host "system_formation_changed = $([int]$comparisonOverview.system_formation_changed_case_count)"
            Write-Host "binding_result_changed   = $([int]$comparisonOverview.binding_result_changed_case_count)"
            Write-Host "bringup_order_changed    = $([int]$comparisonOverview.bringup_order_changed_case_count)"
            Write-Host "bringup_changed_cases    = $([int]$comparisonOverview.bringup_changed_case_count)"
            Write-Host "resource_changed_cases   = $([int]$comparisonOverview.resource_changed_case_count)"
            Write-Host "fact_resolution_changed  = $([int]$comparisonOverview.fact_resolution_changed_case_count)"
            if (@($comparisonOverview.compared_cases).Count -gt 0) {
                Write-Host "compared_cases           = $((@($comparisonOverview.compared_cases) -join ', '))"
            }
            if (@($comparisonOverview.system_formation_changed_cases).Count -gt 0) {
                Write-Host "system_formation_cases   = $((@($comparisonOverview.system_formation_changed_cases) -join ', '))"
            }
            if (@($comparisonOverview.fact_resolution_changed_cases).Count -gt 0) {
                Write-Host "fact_resolution_cases    = $((@($comparisonOverview.fact_resolution_changed_cases) -join ', '))"
            }
            if ($null -ne $comparisonOverview.capability_summary) {
                Write-Host "compare_capabilities     = $([int]$comparisonOverview.capability_summary.compared_capability_count)"
                Write-Host "bringup_compare_caps     = $([int]$comparisonOverview.capability_summary.bringup_compare_capability_count)"
                Write-Host "resource_compare_caps    = $([int]$comparisonOverview.capability_summary.resource_compare_capability_count)"
                if (@($comparisonOverview.capability_summary.compared_capabilities).Count -gt 0) {
                    Write-Host "compared_capabilities    = $((@($comparisonOverview.capability_summary.compared_capabilities) -join ', '))"
                }
            }
            if ($null -ne $comparisonOverview.system_compiler_summary) {
                Write-Host "system_compiler_cmp     = $([int]$comparisonOverview.system_compiler_summary.changed_case_count)"
                if (@($comparisonOverview.system_compiler_summary.changed_cases).Count -gt 0) {
                    Write-Host "system_compiler_list    = $((@($comparisonOverview.system_compiler_summary.changed_cases) -join ', '))"
                }
                if (@($comparisonOverview.system_compiler_summary.blocker_reason_change_matrix).Count -gt 0) {
                    Write-Host "system_compiler_blocker_reasons = $((@($comparisonOverview.system_compiler_summary.blocker_reason_change_matrix | ForEach-Object { [string]$_.reason }) -join ', '))"
                }
                if (@($comparisonOverview.system_compiler_summary.binding_reason_change_matrix).Count -gt 0) {
                    Write-Host "system_compiler_binding_reasons = $((@($comparisonOverview.system_compiler_summary.binding_reason_change_matrix | ForEach-Object { [string]$_.reason }) -join ', '))"
                }
                if (@($comparisonOverview.system_compiler_summary.bringup_phase_change_matrix).Count -gt 0) {
                    Write-Host "system_compiler_bringup_phases = $((@($comparisonOverview.system_compiler_summary.bringup_phase_change_matrix | ForEach-Object { [string]$_.phase }) -join ', '))"
                }
                if (@($comparisonOverview.system_compiler_summary.bringup_dependency_change_matrix).Count -gt 0) {
                    Write-Host "system_compiler_bringup_dependencies = $((@($comparisonOverview.system_compiler_summary.bringup_dependency_change_matrix | ForEach-Object { [string]$_.node }) -join ', '))"
                }
            }
            if ($null -ne $comparisonOverview.system_input_summary) {
                Write-Host "system_input_cmp        = $([int]$comparisonOverview.system_input_summary.changed_case_count)"
                if (@($comparisonOverview.system_input_summary.changed_cases).Count -gt 0) {
                    Write-Host "system_input_list       = $((@($comparisonOverview.system_input_summary.changed_cases) -join ', '))"
                }
            }
            if ($null -ne $comparisonOverview.system_formation_summary) {
                Write-Host "formation_changed_cases  = $([int]$comparisonOverview.system_formation_summary.changed_case_count)"
                if (@($comparisonOverview.system_formation_summary.changed_cases).Count -gt 0) {
                    Write-Host "formation_changed_list   = $((@($comparisonOverview.system_formation_summary.changed_cases) -join ', '))"
                }
            }
            if ($null -ne $comparisonOverview.binding_result_summary) {
                Write-Host "binding_result_cmp      = $([int]$comparisonOverview.binding_result_summary.changed_case_count)"
                if (@($comparisonOverview.binding_result_summary.changed_cases).Count -gt 0) {
                    Write-Host "binding_result_list     = $((@($comparisonOverview.binding_result_summary.changed_cases) -join ', '))"
                }
            }
            if ($null -ne $comparisonOverview.bringup_order_summary) {
                Write-Host "bringup_order_cmp       = $([int]$comparisonOverview.bringup_order_summary.changed_case_count)"
                if (@($comparisonOverview.bringup_order_summary.changed_cases).Count -gt 0) {
                    Write-Host "bringup_order_list      = $((@($comparisonOverview.bringup_order_summary.changed_cases) -join ', '))"
                }
            }
            if ($null -ne $comparisonOverview.fact_resolution_summary) {
                Write-Host "fact_resolution_cmp      = $([int]$comparisonOverview.fact_resolution_summary.changed_case_count)"
                if (@($comparisonOverview.fact_resolution_summary.changed_cases).Count -gt 0) {
                    Write-Host "fact_resolution_list     = $((@($comparisonOverview.fact_resolution_summary.changed_cases) -join ', '))"
                }
            }
            Write-Host ''
        }
        $summaryRows | Sort-Object Case | Format-Table -AutoSize Case, Mode, Profile, Board, Facets, Nodes, Edges, Unresolved, Contracts, Satisfied, Violated, Unknown, Formation, Compare, Metadata, InpCmp, FormCmp, BindCmp, OrdCmp, BrCmp, ResCmp | Out-Host
    }
    exit 0
}

$loadedReport = $selectedReports[0]
$reportData = $loadedReport.Data
$graphInfo = Load-GraphFromArtifactReport -ReportData $reportData

if ($BringupEvidence) {
    if ($selectedReports.Count -ne 1) {
        $artifactRootBringupEvidence = New-ArtifactRootBringupEvidenceResult -LoadedReports $selectedReports
        $artifactRootBringupComparison = New-ArtifactRootBringupEvidenceComparisonResult -LoadedReports $selectedReports

        if ($AsJson) {
            $queryPayload = [ordered]@{
                kind = 'bringup_evidence'
                scope = 'artifact_root'
                result = $artifactRootBringupEvidence
            }
            if ($null -ne $artifactRootBringupComparison) {
                $queryPayload.comparison = [ordered]@{
                    bringup_evidence = $artifactRootBringupComparison
                }
            }

            [ordered]@{
                artifact_root = $artifactRootPath
                query = $queryPayload
            } | ConvertTo-Json -Depth 14
            exit 0
        }

        Write-Host "[ARTIFACT ROOT] $artifactRootPath"
        Write-Host "[BRINGUP EVIDENCE] scope=artifact_root cases=$([int]$artifactRootBringupEvidence.case_count)"
        Write-Host "declared_count     = $([int]$artifactRootBringupEvidence.totals.declared_count)"
        Write-Host "materialized_count = $([int]$artifactRootBringupEvidence.totals.materialized_count)"
        Write-Host "published_count    = $([int]$artifactRootBringupEvidence.totals.published_count)"
        Write-Host "observed_count     = $([int]$artifactRootBringupEvidence.totals.observed_count)"
        Write-Host "blocked_count      = $([int]$artifactRootBringupEvidence.totals.blocked_count)"
        Write-Host "failed_count       = $([int]$artifactRootBringupEvidence.totals.failed_count)"
        Write-Host ''

        if (@($artifactRootBringupEvidence.cases).Count -gt 0) {
            Write-Host '[CASES]'
            @($artifactRootBringupEvidence.cases) |
                Select-Object `
                    case,
                    board,
                    profile,
                    @{ Name = 'facets'; Expression = { Format-StringArray @($_.active_facets) } },
                    declared_count,
                    materialized_count,
                    published_count,
                    observed_count,
                    blocked_count,
                    failed_count,
                    @{ Name = 'published_capabilities'; Expression = { @($_.published_capabilities).Count } } |
                Format-Table -Wrap -AutoSize |
                Out-Host
            Write-Host ''
        }

        if (@($artifactRootBringupEvidence.capability_matrix).Count -gt 0) {
            Write-Host '[CAPABILITY MATRIX]'
            foreach ($capabilityEntry in @($artifactRootBringupEvidence.capability_matrix)) {
                Write-Host "capability = $([string]$capabilityEntry.capability) declared=[$((@($capabilityEntry.declared_cases) -join ', '))] materialized=[$((@($capabilityEntry.materialized_cases) -join ', '))] observed=[$((@($capabilityEntry.observed_cases) -join ', '))] published=[$((@($capabilityEntry.published_cases) -join ', '))] blocked=[$((@($capabilityEntry.blocked_cases) -join ', '))] failed=[$((@($capabilityEntry.failed_cases) -join ', '))]"
                if (@($capabilityEntry.publish_states).Count -gt 0) {
                    Write-Host "publish_states = $((@($capabilityEntry.publish_states) -join ', '))"
                }
                if (@($capabilityEntry.export_states).Count -gt 0) {
                    Write-Host "export_states  = $((@($capabilityEntry.export_states) -join ', '))"
                }
                if (@($capabilityEntry.provider_nodes).Count -gt 0) {
                    Write-Host "provider_nodes = $((@($capabilityEntry.provider_nodes) -join ', '))"
                }
                if (@($capabilityEntry.consumer_nodes).Count -gt 0) {
                    Write-Host "consumer_nodes = $((@($capabilityEntry.consumer_nodes) -join ', '))"
                }
                if (@($capabilityEntry.blocked_reasons).Count -gt 0) {
                    Write-Host "blocked_reasons = $((@($capabilityEntry.blocked_reasons) -join '; '))"
                }
                if (@($capabilityEntry.failed_reasons).Count -gt 0) {
                    Write-Host "failed_reasons = $((@($capabilityEntry.failed_reasons) -join '; '))"
                }
            }
            Write-Host ''
        }

        if (@($artifactRootBringupEvidence.blocked_reason_matrix).Count -gt 0) {
            Write-Host '[BLOCKED REASONS]'
            foreach ($reasonEntry in @($artifactRootBringupEvidence.blocked_reason_matrix)) {
                $caseNames = @(
                    @($reasonEntry.cases) |
                        ForEach-Object { [string]$_.case }
                )
                Write-Host "reason = $([string]$reasonEntry.reason) case_count=$([int]$reasonEntry.case_count) cases=[$((@($caseNames) -join ', '))]"
            }
            Write-Host ''
        }

        if (@($artifactRootBringupEvidence.failed_reason_matrix).Count -gt 0) {
            Write-Host '[FAILED REASONS]'
            foreach ($reasonEntry in @($artifactRootBringupEvidence.failed_reason_matrix)) {
                $caseNames = @(
                    @($reasonEntry.cases) |
                        ForEach-Object { [string]$_.case }
                )
                Write-Host "reason = $([string]$reasonEntry.reason) case_count=$([int]$reasonEntry.case_count) cases=[$((@($caseNames) -join ', '))]"
            }
        }

        if ($null -ne $artifactRootBringupComparison) {
            Write-Host ''
            Write-Host "[BRINGUP EVIDENCE COMPARE] scope=artifact_root compared=$([int]$artifactRootBringupComparison.compared_case_count) changed=$([int]$artifactRootBringupComparison.changed_case_count) unchanged=$([int]$artifactRootBringupComparison.unchanged_case_count)"
            if (@($artifactRootBringupComparison.changed_cases).Count -gt 0) {
                Write-Host "changed_cases = $((@($artifactRootBringupComparison.changed_cases) -join ', '))"
            }
            if (@($artifactRootBringupComparison.unchanged_cases).Count -gt 0) {
                Write-Host "unchanged_cases = $((@($artifactRootBringupComparison.unchanged_cases) -join ', '))"
            }

            if (@($artifactRootBringupComparison.cases).Count -gt 0) {
                Write-Host ''
                Write-Host '[COMPARE CASES]'
                @($artifactRootBringupComparison.cases) |
                    Select-Object `
                        case,
                        board,
                        profile,
                        @{ Name = 'changed'; Expression = { [bool]$_.changed } },
                        @{ Name = 'capability_changes'; Expression = { [int]$_.capability_change_count } },
                        @{ Name = 'summary_changes'; Expression = { @($_.summary_changes).Count } } |
                    Format-Table -Wrap -AutoSize |
                    Out-Host
            }

            if (@($artifactRootBringupComparison.summary_change_matrix).Count -gt 0) {
                Write-Host ''
                Write-Host '[COMPARE SUMMARY CHANGES]'
                foreach ($changeEntry in @($artifactRootBringupComparison.summary_change_matrix)) {
                    $caseNames = @(
                        @($changeEntry.cases) |
                            ForEach-Object { [string]$_.case }
                    )
                    Write-Host "change = $([string]$changeEntry.change) case_count=$([int]$changeEntry.case_count) cases=[$((@($caseNames) -join ', '))]"
                }
            }

            if (@($artifactRootBringupComparison.capability_change_matrix).Count -gt 0) {
                Write-Host ''
                Write-Host '[COMPARE CAPABILITY MATRIX]'
                foreach ($capabilityEntry in @($artifactRootBringupComparison.capability_change_matrix)) {
                    $caseStates = @(
                        @($capabilityEntry.cases) |
                            ForEach-Object { "$([string]$_.case):$([string]$_.change_kind)" }
                    )
                    Write-Host "capability = $([string]$capabilityEntry.capability) case_count=$([int]$capabilityEntry.case_count) change_kinds=[$((@($capabilityEntry.change_kinds) -join ', '))]"
                    if (@($caseStates).Count -gt 0) {
                        Write-Host "cases = $((@($caseStates) -join ', '))"
                    }
                }
            }
        }
        exit 0
    }

    $bringupEvidenceResult = New-BringupEvidenceResult -ReportData $reportData -GraphInfo $graphInfo
    $bringupEvidenceComparison = Get-BringupEvidenceComparisonFromReport -ReportData $reportData

    if ($AsJson) {
        $queryPayload = [ordered]@{
            kind = 'bringup_evidence'
            scope = 'report'
            result = $bringupEvidenceResult
        }
        if ($null -ne $bringupEvidenceComparison) {
            $queryPayload.comparison = [ordered]@{
                bringup_evidence = $bringupEvidenceComparison
            }
        }

        [ordered]@{
            report_path = $loadedReport.Path
            subject = $reportData.subject
            query = $queryPayload
        } | ConvertTo-Json -Depth 10
        exit 0
    }

    Write-Host "[ARTIFACT ROOT] $artifactRootPath"
    Write-Host "[REPORT] $($loadedReport.Path)"
    Write-Host "[CASE] $([string]($reportData.subject.case))"
    Write-Host '[BRINGUP EVIDENCE]'
    Write-Host "declared_count    = $([int]$bringupEvidenceResult.declared_count)"
    Write-Host "materialized_count = $([int]$bringupEvidenceResult.materialized_count)"
    Write-Host "published_count   = $([int]$bringupEvidenceResult.published_count)"
    Write-Host "observed_count    = $([int]$bringupEvidenceResult.observed_count)"
    Write-Host "blocked_count     = $([int]$bringupEvidenceResult.blocked_count)"
    Write-Host "failed_count      = $([int]$bringupEvidenceResult.failed_count)"
    if (@($bringupEvidenceResult.published_capabilities).Count -gt 0) {
        Write-Host "published_capabilities = $((@($bringupEvidenceResult.published_capabilities) -join ', '))"
    }
    @($bringupEvidenceResult.evidence_entries) |
        ForEach-Object { Format-BringupEvidenceDisplayRow -Entry $_ } |
        Format-Table -Wrap -AutoSize Capability, Dec, Mat, Obs, Pub, Blk, Fail, PubState, ExpState, Providers, Consumers |
        Out-Host

    foreach ($entry in @($bringupEvidenceResult.evidence_entries | Where-Object { @($_.blocked_reasons).Count -gt 0 -or @($_.failed_reasons).Count -gt 0 })) {
        Write-Host "evidence[$([string]$entry.capability)]"
        if (@($entry.blocked_reasons).Count -gt 0) {
            Write-Host "  blocked = $((@($entry.blocked_reasons) -join '; '))"
        }
        if (@($entry.failed_reasons).Count -gt 0) {
            Write-Host "  failed  = $((@($entry.failed_reasons) -join '; '))"
        }
    }

    if ($null -ne $bringupEvidenceComparison) {
        Write-Host ''
        Write-Host '[BRINGUP EVIDENCE COMPARE]'
        Write-Host "left  = declared:$([int]$bringupEvidenceComparison.left.declared_count), materialized:$([int]$bringupEvidenceComparison.left.materialized_count), published:$([int]$bringupEvidenceComparison.left.published_count), observed:$([int]$bringupEvidenceComparison.left.observed_count), blocked:$([int]$bringupEvidenceComparison.left.blocked_count), failed:$([int]$bringupEvidenceComparison.left.failed_count)"
        Write-Host "right = declared:$([int]$bringupEvidenceComparison.right.declared_count), materialized:$([int]$bringupEvidenceComparison.right.materialized_count), published:$([int]$bringupEvidenceComparison.right.published_count), observed:$([int]$bringupEvidenceComparison.right.observed_count), blocked:$([int]$bringupEvidenceComparison.right.blocked_count), failed:$([int]$bringupEvidenceComparison.right.failed_count)"
        if (@($bringupEvidenceComparison.summary_changes).Count -gt 0) {
            Write-Host "summary_changes = $((@($bringupEvidenceComparison.summary_changes) -join '; '))"
        }
        if (@($bringupEvidenceComparison.published_capability_changes.added).Count -gt 0 -or @($bringupEvidenceComparison.published_capability_changes.removed).Count -gt 0) {
            Write-Host "published_capability_changes = +[$((@($bringupEvidenceComparison.published_capability_changes.added) -join ', '))] -[$((@($bringupEvidenceComparison.published_capability_changes.removed) -join ', '))]"
        }
        if (@($bringupEvidenceComparison.blocked_reason_changes.added).Count -gt 0 -or @($bringupEvidenceComparison.blocked_reason_changes.removed).Count -gt 0) {
            Write-Host "blocked_reason_changes = +[$((@($bringupEvidenceComparison.blocked_reason_changes.added) -join '; '))] -[$((@($bringupEvidenceComparison.blocked_reason_changes.removed) -join '; '))]"
        }
        if (@($bringupEvidenceComparison.failed_reason_changes.added).Count -gt 0 -or @($bringupEvidenceComparison.failed_reason_changes.removed).Count -gt 0) {
            Write-Host "failed_reason_changes = +[$((@($bringupEvidenceComparison.failed_reason_changes.added) -join '; '))] -[$((@($bringupEvidenceComparison.failed_reason_changes.removed) -join '; '))]"
        }
        foreach ($capabilityChange in @($bringupEvidenceComparison.capability_changes)) {
            Write-Host "capability[$([string]$capabilityChange.capability)] kind=$([string]$capabilityChange.change_kind) published:$([bool]$capabilityChange.left_published)->$([bool]$capabilityChange.right_published) observed:$([bool]$capabilityChange.left_observed)->$([bool]$capabilityChange.right_observed) export:$([string]$capabilityChange.left_export_state)->$([string]$capabilityChange.right_export_state)"
        }
    }
    exit 0
}

if ($RecentTransitions) {
    $recentTransitionsResult = New-RecentTransitionsResult -ReportData $reportData

    if ($AsJson) {
        [ordered]@{
            report_path = $loadedReport.Path
            subject = $reportData.subject
            query = [ordered]@{
                kind = 'recent_transitions'
                scope = 'report'
                result = $recentTransitionsResult
            }
        } | ConvertTo-Json -Depth 10
        exit 0
    }

    Write-Host "[ARTIFACT ROOT] $artifactRootPath"
    Write-Host "[REPORT] $($loadedReport.Path)"
    Write-Host "[CASE] $([string]($reportData.subject.case))"
    Write-Host '[RECENT TRANSITIONS]'
    Write-Host "transition_count = $([int]$recentTransitionsResult.transition_count)"
    if (@($recentTransitionsResult.observed_capabilities).Count -gt 0) {
        Write-Host "observed_capabilities = $((@($recentTransitionsResult.observed_capabilities) -join ', '))"
    }
    if ($null -ne $recentTransitionsResult.publish_state_summary) {
        Write-Host "publish_state_summary = missing:$([int]$recentTransitionsResult.publish_state_summary.missing), published:$([int]$recentTransitionsResult.publish_state_summary.published)"
    }
    if ($null -ne $recentTransitionsResult.export_state_summary) {
        Write-Host "export_state_summary  = missing:$([int]$recentTransitionsResult.export_state_summary.missing), detached:$([int]$recentTransitionsResult.export_state_summary.detached), attached:$([int]$recentTransitionsResult.export_state_summary.attached)"
    }
    if (@($recentTransitionsResult.transition_capabilities).Count -gt 0) {
        Write-Host "transition_capabilities = $((@($recentTransitionsResult.transition_capabilities) -join ', '))"
    }
    if (@($recentTransitionsResult.action_counts.Keys).Count -gt 0) {
        $actionParts = @()
        foreach ($actionName in @($recentTransitionsResult.action_counts.Keys)) {
            $actionParts += "${actionName}:$([int]$recentTransitionsResult.action_counts[$actionName])"
        }
        Write-Host "action_counts = $((@($actionParts) -join ', '))"
    }
    if ($null -ne $recentTransitionsResult.comparison) {
        Write-Host ''
        Write-Host '[COMPARE]'
        Write-Host "compared_transitions = $([int]$recentTransitionsResult.comparison.compared_transition_count) bringup_compare = $([int]$recentTransitionsResult.comparison.bringup_compare_transition_count) resource_compare = $([int]$recentTransitionsResult.comparison.resource_compare_transition_count)"
        Write-Host "compare_capabilities = $([int]$recentTransitionsResult.comparison.compared_capability_count) bringup_capabilities = $([int]$recentTransitionsResult.comparison.bringup_compare_capability_count) resource_capabilities = $([int]$recentTransitionsResult.comparison.resource_compare_capability_count)"
        if (@($recentTransitionsResult.comparison.compared_capabilities).Count -gt 0) {
            Write-Host "compared_capabilities = $((@($recentTransitionsResult.comparison.compared_capabilities) -join ', '))"
        }
        if (@($recentTransitionsResult.comparison.bringup_change_kinds).Count -gt 0) {
            Write-Host "bringup_change_kinds = $((@($recentTransitionsResult.comparison.bringup_change_kinds) -join ', '))"
        }
        if (@($recentTransitionsResult.comparison.resource_change_kinds).Count -gt 0) {
            Write-Host "resource_change_kinds = $((@($recentTransitionsResult.comparison.resource_change_kinds) -join ', '))"
        }
        if (@($recentTransitionsResult.comparison.resource_contracts).Count -gt 0) {
            Write-Host "resource_contracts = $((@($recentTransitionsResult.comparison.resource_contracts) -join ', '))"
        }
    }
    if (@($recentTransitionsResult.transitions).Count -gt 0) {
        Write-Host ''
        Write-Host '[TRANSITIONS]'
        @($recentTransitionsResult.transitions) |
            ForEach-Object { Format-RecentTransitionDisplayRow -Entry $_ } |
            Format-Table -AutoSize |
            Out-Host
    }
    exit 0
}

if ($ResourceSummary) {
    if ($selectedReports.Count -ne 1) {
        $artifactRootResourceSummary = New-ArtifactRootFactResolutionSummaryResult -LoadedReports $selectedReports
        $artifactRootResourceComparison = New-ArtifactRootResourceContractComparisonResult -LoadedReports $selectedReports
        $artifactRootFactResolutionComparison = New-ArtifactRootFactResolutionComparisonResult -LoadedReports $selectedReports

        if ($AsJson) {
            $queryPayload = [ordered]@{
                kind = 'resource_summary'
                scope = 'artifact_root'
                result = $artifactRootResourceSummary
            }
            if ($null -ne $artifactRootResourceComparison -or $null -ne $artifactRootFactResolutionComparison) {
                $queryPayload.comparison = [ordered]@{}
                if ($null -ne $artifactRootResourceComparison) {
                    $queryPayload.comparison.resource_contract = $artifactRootResourceComparison
                }
                if ($null -ne $artifactRootFactResolutionComparison) {
                    $queryPayload.comparison.fact_resolution = $artifactRootFactResolutionComparison
                }
            }

            [ordered]@{
                artifact_root = $artifactRootPath
                query = $queryPayload
            } | ConvertTo-Json -Depth 14
            exit 0
        }

        Write-Host "[ARTIFACT ROOT] $artifactRootPath"
        Write-Host "[RESOURCE SUMMARY] scope=artifact_root cases=$([int]$artifactRootResourceSummary.case_count)"
        Write-Host "declared_contracts = $([int]$artifactRootResourceSummary.totals.declared_contracts)"
        Write-Host "audited_count      = $([int]$artifactRootResourceSummary.totals.audited_count)"
        Write-Host "satisfied_count    = $([int]$artifactRootResourceSummary.totals.satisfied_count)"
        Write-Host "violated_count     = $([int]$artifactRootResourceSummary.totals.violated_count)"
        Write-Host "unknown_count      = $([int]$artifactRootResourceSummary.totals.unknown_count)"
        Write-Host ''

        if (@($artifactRootResourceSummary.cases).Count -gt 0) {
            Write-Host '[CASES]'
            @($artifactRootResourceSummary.cases) |
                Select-Object `
                    case,
                    board,
                    profile,
                    @{ Name = 'facets'; Expression = { Format-StringArray @($_.active_facets) } },
                    declared_contracts,
                    satisfied_count,
                    violated_count,
                    unknown_count,
                    @{ Name = 'required_facts'; Expression = { @($_.required_facts).Count } },
                    @{ Name = 'provided_facts'; Expression = { @($_.audit_provided_facts).Count } },
                    @{ Name = 'hotspots'; Expression = { @($_.resource_hotspots).Count } } |
                Format-Table -Wrap -AutoSize |
                Out-Host
            Write-Host ''
        }

        if (@($artifactRootResourceSummary.required_fact_matrix).Count -gt 0) {
            Write-Host '[REQUIRED FACT MATRIX]'
            foreach ($factEntry in @($artifactRootResourceSummary.required_fact_matrix)) {
                $caseNames = @(
                    @($factEntry.cases) |
                        ForEach-Object { [string]$_.case }
                )
                Write-Host "fact = $([string]$factEntry.fact) case_count=$([int]$factEntry.case_count) cases=[$((@($caseNames) -join ', '))]"
            }
            Write-Host ''
        }

        if (@($artifactRootResourceSummary.contract_matrix).Count -gt 0) {
            Write-Host '[CONTRACT MATRIX]'
            foreach ($contractEntry in @($artifactRootResourceSummary.contract_matrix)) {
                $caseStates = @(
                    @($contractEntry.cases) |
                        ForEach-Object { "$([string]$_.case):$([string]$_.state)" }
                )
                Write-Host "contract = $([string]$contractEntry.contract) requires=[$((@($contractEntry.requires) -join ', '))] declared=$([int]$contractEntry.cases_declared) satisfied=$([int]$contractEntry.cases_satisfied) violated=$([int]$contractEntry.cases_violated) unknown=$([int]$contractEntry.cases_unknown)"
                if (@($caseStates).Count -gt 0) {
                    Write-Host "cases = $((@($caseStates) -join ', '))"
                }
            }
            Write-Host ''
        }

        if (@($artifactRootResourceSummary.provided_fact_matrix).Count -gt 0) {
            Write-Host '[PROVIDED FACT MATRIX]'
            foreach ($factEntry in @($artifactRootResourceSummary.provided_fact_matrix)) {
                $caseNames = @(
                    @($factEntry.cases) |
                        ForEach-Object { [string]$_.case }
                )
                Write-Host "fact = $([string]$factEntry.fact) case_count=$([int]$factEntry.case_count) cases=[$((@($caseNames) -join ', '))]"
            }
            Write-Host ''
        }

        if (@($artifactRootResourceSummary.resource_hotspot_matrix).Count -gt 0) {
            Write-Host '[RESOURCE HOTSPOTS]'
            foreach ($hotspotEntry in @($artifactRootResourceSummary.resource_hotspot_matrix)) {
                $caseNames = @(
                    @($hotspotEntry.cases) |
                    ForEach-Object { [string]$_.case }
                )
                Write-Host "hotspot = $([string]$hotspotEntry.hotspot) case_count=$([int]$hotspotEntry.case_count) cases=[$((@($caseNames) -join ', '))]"
            }
        }

        if ($null -ne $artifactRootResourceComparison) {
            Write-Host ''
            Write-Host "[RESOURCE CONTRACT COMPARE] scope=artifact_root compared=$([int]$artifactRootResourceComparison.compared_case_count) changed=$([int]$artifactRootResourceComparison.changed_case_count) unchanged=$([int]$artifactRootResourceComparison.unchanged_case_count)"
            if (@($artifactRootResourceComparison.changed_cases).Count -gt 0) {
                Write-Host "changed_cases = $((@($artifactRootResourceComparison.changed_cases) -join ', '))"
            }
            if (@($artifactRootResourceComparison.unchanged_cases).Count -gt 0) {
                Write-Host "unchanged_cases = $((@($artifactRootResourceComparison.unchanged_cases) -join ', '))"
            }

            if (@($artifactRootResourceComparison.cases).Count -gt 0) {
                Write-Host ''
                Write-Host '[COMPARE CASES]'
                @($artifactRootResourceComparison.cases) |
                    Select-Object `
                        case,
                        board,
                        profile,
                        @{ Name = 'changed'; Expression = { [bool]$_.changed } },
                        @{ Name = 'contract_changes'; Expression = { [int]$_.contract_change_count } },
                        @{ Name = 'summary_changes'; Expression = { @($_.summary_changes).Count } } |
                    Format-Table -Wrap -AutoSize |
                    Out-Host
            }

            if (@($artifactRootResourceComparison.summary_change_matrix).Count -gt 0) {
                Write-Host ''
                Write-Host '[COMPARE SUMMARY CHANGES]'
                foreach ($changeEntry in @($artifactRootResourceComparison.summary_change_matrix)) {
                    $caseNames = @(
                        @($changeEntry.cases) |
                            ForEach-Object { [string]$_.case }
                    )
                    Write-Host "change = $([string]$changeEntry.change) case_count=$([int]$changeEntry.case_count) cases=[$((@($caseNames) -join ', '))]"
                }
            }

            if (@($artifactRootResourceComparison.contract_change_matrix).Count -gt 0) {
                Write-Host ''
                Write-Host '[COMPARE CONTRACT MATRIX]'
                foreach ($contractEntry in @($artifactRootResourceComparison.contract_change_matrix)) {
                    $caseStates = @(
                        @($contractEntry.cases) |
                            ForEach-Object { "$([string]$_.case):$([string]$_.change_kind)" }
                    )
                    Write-Host "contract = $([string]$contractEntry.contract) case_count=$([int]$contractEntry.case_count) change_kinds=[$((@($contractEntry.change_kinds) -join ', '))]"
                    if (@($caseStates).Count -gt 0) {
                        Write-Host "cases = $((@($caseStates) -join ', '))"
                    }
                }
            }
        }
        if ($null -ne $artifactRootFactResolutionComparison) {
            Write-Host ''
            Write-Host "[FACT RESOLUTION COMPARE] scope=artifact_root compared=$([int]$artifactRootFactResolutionComparison.compared_case_count) changed=$([int]$artifactRootFactResolutionComparison.changed_case_count) unchanged=$([int]$artifactRootFactResolutionComparison.unchanged_case_count)"
            if (@($artifactRootFactResolutionComparison.changed_cases).Count -gt 0) {
                Write-Host "changed_cases = $((@($artifactRootFactResolutionComparison.changed_cases) -join ', '))"
            }
            if (@($artifactRootFactResolutionComparison.fact_inventory_change_matrix.required_facts).Count -gt 0) {
                foreach ($factEntry in @($artifactRootFactResolutionComparison.fact_inventory_change_matrix.required_facts)) {
                    $addedCaseNames = @($factEntry.added_cases | ForEach-Object { [string]$_.case })
                    $removedCaseNames = @($factEntry.removed_cases | ForEach-Object { [string]$_.case })
                    Write-Host "required_fact = $([string]$factEntry.fact) added=[$((@($addedCaseNames) -join ', '))] removed=[$((@($removedCaseNames) -join ', '))]"
                }
            }
        }
        exit 0
    }

    $resourceSummaryResult = New-ResourceSummaryResult -ReportData $reportData -GraphInfo $graphInfo
    $resourceContractComparison = Get-ResourceContractComparisonFromReport -ReportData $reportData
    $factResolutionComparison = Get-FactResolutionComparisonFromReport -ReportData $reportData

    if ($AsJson) {
        $queryPayload = [ordered]@{
            kind = 'resource_summary'
            scope = 'report'
            result = $resourceSummaryResult
        }
        if ($null -ne $resourceContractComparison -or $null -ne $factResolutionComparison) {
            $queryPayload.comparison = [ordered]@{}
            if ($null -ne $resourceContractComparison) {
                $queryPayload.comparison.resource_contract = $resourceContractComparison
            }
            if ($null -ne $factResolutionComparison) {
                $queryPayload.comparison.fact_resolution = $factResolutionComparison
            }
        }

        [ordered]@{
            report_path = $loadedReport.Path
            subject = $reportData.subject
            query = $queryPayload
        } | ConvertTo-Json -Depth 10
        exit 0
    }

    Write-Host "[ARTIFACT ROOT] $artifactRootPath"
    Write-Host "[REPORT] $($loadedReport.Path)"
    Write-Host "[CASE] $([string]($reportData.subject.case))"
    Write-Host '[RESOURCE SUMMARY]'
    Write-Host "declared_contracts = $([int]$resourceSummaryResult.declared_contracts)"
    Write-Host "audited_count      = $([int]$resourceSummaryResult.audited_count)"
    Write-Host "satisfied_count    = $([int]$resourceSummaryResult.satisfied_count)"
    Write-Host "violated_count     = $([int]$resourceSummaryResult.violated_count)"
    Write-Host "unknown_count      = $([int]$resourceSummaryResult.unknown_count)"
    Write-Host ''

    Write-Host '[FACT INVENTORY]'
    foreach ($factGroup in @('declared_facts', 'subject_facts', 'required_facts', 'graph_provided_facts', 'audit_provided_facts', 'all_available_facts')) {
        $factValues = @($resourceSummaryResult.fact_inventory.$factGroup)
        if ($factValues.Count -gt 0) {
            Write-Host "$factGroup = $((@($factValues) -join ', '))"
        }
    }
    Write-Host ''

    Write-Host '[CONTRACTS]'
    foreach ($contractSummary in @($resourceSummaryResult.contracts)) {
        Write-Host "contract = $([string]$contractSummary.contract) state=$([string]$contractSummary.state) requires=[$((@($contractSummary.requires) -join ', '))]"
        if (@($contractSummary.present_facts).Count -gt 0) {
            Write-Host "present_facts = $((@($contractSummary.present_facts) -join ', '))"
        }
        if (@($contractSummary.missing_facts).Count -gt 0) {
            Write-Host "missing_facts = $((@($contractSummary.missing_facts) -join ', '))"
        }
        foreach ($factName in @($contractSummary.fact_sources.Keys)) {
            Write-Host "fact_sources[$factName] = $((@($contractSummary.fact_sources[$factName]) -join ', '))"
        }
        if (-not [string]::IsNullOrWhiteSpace([string]$contractSummary.status_text)) {
            Write-Host "status_text = $([string]$contractSummary.status_text)"
        }
    }
    Write-Host ''

    if (@($resourceSummaryResult.resource_hotspots).Count -gt 0) {
        Write-Host '[RESOURCE HOTSPOTS]'
        foreach ($hotspot in @($resourceSummaryResult.resource_hotspots)) {
            Write-Host "hotspot = $([string]$hotspot)"
        }
    }

    if ($null -ne $resourceContractComparison) {
        Write-Host ''
        Write-Host '[RESOURCE CONTRACT COMPARE]'
        Write-Host "left  = declared:$([int]$resourceContractComparison.left.declared_contracts), satisfied:$([int]$resourceContractComparison.left.satisfied_count), violated:$([int]$resourceContractComparison.left.violated_count), unknown:$([int]$resourceContractComparison.left.unknown_count)"
        Write-Host "right = declared:$([int]$resourceContractComparison.right.declared_contracts), satisfied:$([int]$resourceContractComparison.right.satisfied_count), violated:$([int]$resourceContractComparison.right.violated_count), unknown:$([int]$resourceContractComparison.right.unknown_count)"
        if (@($resourceContractComparison.summary_changes).Count -gt 0) {
            Write-Host "summary_changes = $((@($resourceContractComparison.summary_changes) -join '; '))"
        }
        if (@($resourceContractComparison.provided_fact_changes.added).Count -gt 0 -or @($resourceContractComparison.provided_fact_changes.removed).Count -gt 0) {
            Write-Host "provided_fact_changes = +[$((@($resourceContractComparison.provided_fact_changes.added) -join ', '))] -[$((@($resourceContractComparison.provided_fact_changes.removed) -join ', '))]"
        }
        if (@($resourceContractComparison.hotspot_changes.added).Count -gt 0 -or @($resourceContractComparison.hotspot_changes.removed).Count -gt 0) {
            Write-Host "hotspot_changes = +[$((@($resourceContractComparison.hotspot_changes.added) -join '; '))] -[$((@($resourceContractComparison.hotspot_changes.removed) -join '; '))]"
        }
        foreach ($contractChange in @($resourceContractComparison.contract_changes)) {
            Write-Host "contract = $([string]$contractChange.contract) change=$([string]$contractChange.change_kind) $([string]$contractChange.left_state)->$([string]$contractChange.right_state)"
            if (@($contractChange.left_requires).Count -gt 0 -or @($contractChange.right_requires).Count -gt 0) {
                Write-Host "requires = left[$((@($contractChange.left_requires) -join ', '))] right[$((@($contractChange.right_requires) -join ', '))]"
            }
            if (-not [string]::IsNullOrWhiteSpace([string]$contractChange.left_status_text) -or -not [string]::IsNullOrWhiteSpace([string]$contractChange.right_status_text)) {
                Write-Host "status_text = left{$([string]$contractChange.left_status_text)} right{$([string]$contractChange.right_status_text)}"
            }
        }
    }
    exit 0
}

if (-not [string]::IsNullOrWhiteSpace($GraphPath)) {
    $graphPathResult = New-GraphPathResult -ReportData $reportData -GraphInfo $graphInfo -CapabilityName $GraphPath

    if ($AsJson) {
        [ordered]@{
            report_path = $loadedReport.Path
            subject = $reportData.subject
            query = [ordered]@{
                kind = 'graph_path'
                scope = 'report'
                result = $graphPathResult
            }
        } | ConvertTo-Json -Depth 10
        exit 0
    }

    Write-Host "[ARTIFACT ROOT] $artifactRootPath"
    Write-Host "[REPORT] $($loadedReport.Path)"
    Write-Host "[CASE] $([string]($reportData.subject.case))"
    Write-Host "[GRAPH PATH] $GraphPath"
    Write-Host "state = $([string]$graphPathResult.state)"
    Write-Host "availability_state = $([string]$graphPathResult.availability_state)"
    if (@($graphPathResult.reasons).Count -gt 0) {
        Write-Host "reasons = $((@($graphPathResult.reasons) -join '; '))"
    }
    if ($null -ne $graphPathResult.comparison) {
        Write-Host '[COMPARE]'
        Write-Host "changed = $([bool]$graphPathResult.comparison.changed)"
        Write-Host "bringup_changed = $([bool]$graphPathResult.comparison.bringup_changed)"
        Write-Host "resource_changed = $([bool]$graphPathResult.comparison.resource_changed)"
        if (@($graphPathResult.comparison.bringup_change_kinds).Count -gt 0) {
            Write-Host "bringup_change_kinds = $((@($graphPathResult.comparison.bringup_change_kinds) -join ', '))"
        }
        if (@($graphPathResult.comparison.resource_change_kinds).Count -gt 0) {
            Write-Host "resource_change_kinds = $((@($graphPathResult.comparison.resource_change_kinds) -join ', '))"
        }
        if (@($graphPathResult.comparison.resource_contracts).Count -gt 0) {
            Write-Host "resource_contracts = $((@($graphPathResult.comparison.resource_contracts) -join ', '))"
        }
    }
    if (@($graphPathResult.direct_edges).Count -gt 0) {
        Write-Host '[DIRECT EDGES]'
        foreach ($edgeRecord in @($graphPathResult.direct_edges)) {
            Write-Host "edge = $([string]$edgeRecord.text)"
        }
    }
    if (@($graphPathResult.provider_paths).Count -gt 0) {
        Write-Host '[PROVIDER PATHS]'
        foreach ($pathRecord in @($graphPathResult.provider_paths)) {
            Write-Host "path = $([string]$pathRecord.text)"
        }
    }
    if (@($graphPathResult.consumer_paths).Count -gt 0) {
        Write-Host '[CONSUMER PATHS]'
        foreach ($pathRecord in @($graphPathResult.consumer_paths)) {
            Write-Host "path = $([string]$pathRecord.text)"
        }
    }
    exit 0
}

if (-not [string]::IsNullOrWhiteSpace($WhyCapability)) {
    $whyResult = New-WhyCapabilityResult -ReportData $reportData -GraphInfo $graphInfo -CapabilityName $WhyCapability

    if ($AsJson) {
        [ordered]@{
            report_path = $loadedReport.Path
            subject = $reportData.subject
            query = $whyResult
        } | ConvertTo-Json -Depth 8
        exit 0
    }

    Write-Host "[ARTIFACT ROOT] $artifactRootPath"
    Write-Host "[REPORT] $($loadedReport.Path)"
    Write-Host "[CASE] $([string]($reportData.subject.case))"
    Write-Host "[WHY UNAVAILABLE] $WhyCapability"
    Write-Host "state = $([string]$whyResult.state)"
    if (@($whyResult.reasons).Count -gt 0) {
        Write-Host "reasons = $((@($whyResult.reasons) -join '; '))"
    }
    if (@($whyResult.evidence.provider_nodes).Count -gt 0) {
        Write-Host "provider_nodes = $((@($whyResult.evidence.provider_nodes) -join ', '))"
    }
    if (@($whyResult.evidence.consumer_nodes).Count -gt 0) {
        Write-Host "consumer_nodes = $((@($whyResult.evidence.consumer_nodes) -join ', '))"
    }
    if (@($whyResult.evidence.edges).Count -gt 0) {
        Write-Host "edges = $((@($whyResult.evidence.edges) -join ', '))"
    }
    if (@($whyResult.evidence.blocked_reasons).Count -gt 0) {
        Write-Host "blocked_reasons = $((@($whyResult.evidence.blocked_reasons) -join '; '))"
    }
    if (@($whyResult.evidence.failed_reasons).Count -gt 0) {
        Write-Host "failed_reasons = $((@($whyResult.evidence.failed_reasons) -join '; '))"
    }
    if ($whyResult.evidence.resource_contract.provided_fact) {
        Write-Host "resource_contract = capability also appears in resource_contract.provided_facts"
    }
    if (@($whyResult.evidence.resource_contract.hotspots).Count -gt 0) {
        Write-Host "resource_hotspots = $((@($whyResult.evidence.resource_contract.hotspots) -join '; '))"
    }
    if ($null -ne $whyResult.comparison) {
        Write-Host ''
        Write-Host '[COMPARE]'
        Write-Host "changed = $([bool]$whyResult.comparison.changed)"
        Write-Host "bringup_changed = $([bool]$whyResult.comparison.bringup_changed)"
        Write-Host "resource_changed = $([bool]$whyResult.comparison.resource_changed)"
        if (@($whyResult.comparison.bringup_change_kinds).Count -gt 0) {
            Write-Host "bringup_change_kinds = $((@($whyResult.comparison.bringup_change_kinds) -join ', '))"
        }
        if (@($whyResult.comparison.resource_change_kinds).Count -gt 0) {
            Write-Host "resource_change_kinds = $((@($whyResult.comparison.resource_change_kinds) -join ', '))"
        }
        if (@($whyResult.comparison.resource_contracts).Count -gt 0) {
            Write-Host "resource_contracts = $((@($whyResult.comparison.resource_contracts) -join ', '))"
        }
    }
    exit 0
}

if ($AsJson) {
    New-ArtifactJsonView -LoadedReport $loadedReport | ConvertTo-Json -Depth 8
    exit 0
}

Write-Host "[ARTIFACT ROOT] $artifactRootPath"
Write-Host "[REPORT] $($loadedReport.Path)"
Write-Host "[CASE] $([string]($reportData.subject.case))"
Write-Host "[MODE] $([string]($reportData.mode))"
Write-Host ''

$summaryRows | Format-List Case, Mode, Profile, Board, Facets, Nodes, Edges, Unresolved, Contracts, Satisfied, Violated, Unknown, Formation, Compare, Metadata, InpCmp, FormCmp | Out-Host

if ($null -ne $reportData.PSObject.Properties['system_input'] -and $null -ne $reportData.system_input) {
    $systemInput = $reportData.system_input
    Write-Host '[INPUT]'
    if ($null -ne $systemInput.PSObject.Properties['system_spec'] -and $null -ne $systemInput.system_spec) {
        Write-Host "case_kind       = $([string]$systemInput.system_spec.case_kind)"
        if (-not [string]::IsNullOrWhiteSpace([string]$systemInput.system_spec.source)) {
            Write-Host "source          = $([string]$systemInput.system_spec.source)"
        }
        if (-not [string]::IsNullOrWhiteSpace([string]$systemInput.system_spec.build_dir)) {
            Write-Host "build_dir       = $([string]$systemInput.system_spec.build_dir)"
        }
        if (-not [string]::IsNullOrWhiteSpace([string]$systemInput.system_spec.build_target)) {
            Write-Host "build_target    = $([string]$systemInput.system_spec.build_target)"
        }
        if (-not [string]::IsNullOrWhiteSpace([string]$systemInput.system_spec.export_target)) {
            Write-Host "export_target   = $([string]$systemInput.system_spec.export_target)"
        }
    }
    if ($null -ne $systemInput.PSObject.Properties['declared_input'] -and $null -ne $systemInput.declared_input) {
        $declaredInput = $systemInput.declared_input
        if ($null -ne $declaredInput.PSObject.Properties['subject'] -and $null -ne $declaredInput.subject) {
            $declaredSubjectParts = @()
            if (-not [string]::IsNullOrWhiteSpace([string]$declaredInput.subject.profile)) {
                $declaredSubjectParts += "profile=$([string]$declaredInput.subject.profile)"
            }
            if (-not [string]::IsNullOrWhiteSpace([string]$declaredInput.subject.board)) {
                $declaredSubjectParts += "board=$([string]$declaredInput.subject.board)"
            }
            if (@($declaredInput.subject.active_facets).Count -gt 0) {
                $declaredSubjectParts += "facets=$((@($declaredInput.subject.active_facets) -join ', '))"
            }
            if (@($declaredSubjectParts).Count -gt 0) {
                Write-Host "declared_subject = $((@($declaredSubjectParts) -join '; '))"
            }
        }
        if (@($declaredInput.declared_facts).Count -gt 0) {
            Write-Host "declared_facts  = $((@($declaredInput.declared_facts) -join ', '))"
        }
        if (@($declaredInput.declared_contract_entries).Count -gt 0) {
            $contractTexts = @(
                @($declaredInput.declared_contract_entries) |
                    ForEach-Object { Format-DeclaredContractText -ContractEntry $_ } |
                    Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) }
            )
            if (@($contractTexts).Count -gt 0) {
                Write-Host "declared_contracts = $((@($contractTexts) -join '; '))"
            }
        }
    }
    if ($null -ne $systemInput.PSObject.Properties['resolved_input'] -and $null -ne $systemInput.resolved_input) {
        $resolvedInput = $systemInput.resolved_input
        Write-Host "resolved_profile = $(Format-ResolvedScalarInputText -ResolvedInput $resolvedInput.profile)"
        Write-Host "resolved_board   = $(Format-ResolvedScalarInputText -ResolvedInput $resolvedInput.board)"
        Write-Host "resolved_facets  = $(Format-ResolvedFacetInputText -ResolvedInput $resolvedInput.active_facets)"
        if (@($resolvedInput.subject_facts).Count -gt 0) {
            Write-Host "subject_facts    = $((@($resolvedInput.subject_facts) -join ', '))"
        }
    }
    Write-Host ''
}

Write-Host '[STRUCTURE]'
Write-Host "materialized_order = $((@($reportData.structure.materialized_order) -join ', '))"
if (@($reportData.structure.declared_facts).Count -gt 0) {
    Write-Host "declared_facts    = $((@($reportData.structure.declared_facts) -join ', '))"
}
if (@($reportData.structure.required_facts).Count -gt 0) {
    Write-Host "required_facts    = $((@($reportData.structure.required_facts) -join ', '))"
}
if (@($reportData.structure.unresolved_bindings).Count -gt 0) {
    Write-Host "unresolved        = $((@($reportData.structure.unresolved_bindings) -join ', '))"
}
Write-Host ''

Write-Host '[BINDING RESULT]'
Write-Host "required_bindings   = $([int]$reportData.binding_result.required_binding_count)"
Write-Host "resolved_bindings   = $([int]$reportData.binding_result.resolved_binding_count)"
Write-Host "unresolved_bindings = $([int]$reportData.binding_result.unresolved_binding_count)"
if (@($reportData.binding_result.unresolved_capabilities).Count -gt 0) {
    Write-Host "unresolved_caps     = $((@($reportData.binding_result.unresolved_capabilities) -join ', '))"
}
if (@($reportData.binding_result.binding_entries).Count -gt 0) {
    @($reportData.binding_result.binding_entries) |
        ForEach-Object { Format-BindingResultDisplayRow -Entry $_ } |
        Format-Table -Wrap -AutoSize Capability, State, Providers, Consumers, Reason |
        Out-Host
}
Write-Host ''

Write-Host '[BRINGUP ORDER]'
Write-Host "ordered_nodes = $([int]$reportData.bringup_order.ordered_node_count)"
Write-Host "blocked_nodes = $([int]$reportData.bringup_order.blocked_node_count)"
if ($null -ne $reportData.bringup_order.PSObject.Properties['phase_counts'] -and
    $null -ne $reportData.bringup_order.phase_counts -and
    @($reportData.bringup_order.phase_counts.PSObject.Properties).Count -gt 0) {
    $phaseParts = @()
    foreach ($phaseEntry in @($reportData.bringup_order.phase_counts.PSObject.Properties)) {
        $phaseParts += "$([string]$phaseEntry.Name):$([int]$phaseEntry.Value)"
    }
    Write-Host "phase_counts  = $((@($phaseParts) -join ', '))"
}
if (@($reportData.bringup_order.entries).Count -gt 0) {
    @($reportData.bringup_order.entries) |
        ForEach-Object { Format-BringupOrderDisplayRow -Entry $_ } |
        Format-Table -Wrap -AutoSize Order, Node, Kind, Phase, State, Needs, Missing, DependsOn, Provides |
        Out-Host
}
Write-Host ''

if ($null -ne $reportData.PSObject.Properties['system_formation'] -and $null -ne $reportData.system_formation) {
    $systemFormation = $reportData.system_formation
    Write-Host '[SYSTEM FORMATION]'
    Write-Host "status                  = $([string]$systemFormation.status)"
    if ($null -ne $systemFormation.PSObject.Properties['formation_basis'] -and $null -ne $systemFormation.formation_basis) {
        Write-Host "case_kind               = $([string]$systemFormation.formation_basis.case_kind)"
        Write-Host "declared_fact_count     = $([int]$systemFormation.formation_basis.declared_fact_count)"
        Write-Host "declared_contract_count = $([int]$systemFormation.formation_basis.declared_contract_count)"
        Write-Host "subject_fact_count      = $([int]$systemFormation.formation_basis.subject_fact_count)"
    }
    if ($null -ne $systemFormation.PSObject.Properties['binding_summary'] -and $null -ne $systemFormation.binding_summary) {
        Write-Host "required_bindings       = $([int]$systemFormation.binding_summary.required_binding_count)"
        Write-Host "resolved_bindings       = $([int]$systemFormation.binding_summary.resolved_binding_count)"
        Write-Host "unresolved_bindings     = $([int]$systemFormation.binding_summary.unresolved_binding_count)"
        if (@($systemFormation.binding_summary.unresolved_capabilities).Count -gt 0) {
            Write-Host "unresolved_caps         = $((@($systemFormation.binding_summary.unresolved_capabilities) -join ', '))"
        }
    }
    if ($null -ne $systemFormation.PSObject.Properties['bringup_summary'] -and $null -ne $systemFormation.bringup_summary) {
        Write-Host "ordered_nodes           = $([int]$systemFormation.bringup_summary.ordered_node_count)"
        Write-Host "blocked_nodes           = $([int]$systemFormation.bringup_summary.blocked_node_count)"
        if (@($systemFormation.bringup_summary.blocked_nodes).Count -gt 0) {
            Write-Host "blocked_node_names      = $((@($systemFormation.bringup_summary.blocked_nodes) -join ', '))"
        }
    }
    Write-Host "blocker_count           = $([int]$systemFormation.blocker_count)"
    if (@($systemFormation.blockers).Count -gt 0) {
        @($systemFormation.blockers) |
            ForEach-Object { Format-SystemFormationBlockerDisplayRow -Entry $_ } |
            Format-Table -Wrap -AutoSize Kind, Name, State, Missing, DependsOn, Reason |
            Out-Host
    }
    Write-Host ''
}

Write-Host '[RESOURCE CONTRACT]'
if (@($reportData.resource_contract.declared_contract_entries).Count -gt 0) {
    foreach ($entry in @($reportData.resource_contract.declared_contract_entries)) {
        $contractName = [string]$entry.contract
        $requiredFacts = @($entry.requires)
        Write-Host "declared = $contractName requires [$((@($requiredFacts) -join ', '))]"
    }
}
if (@($reportData.resource_contract.provided_facts).Count -gt 0) {
    Write-Host "provided_facts = $((@($reportData.resource_contract.provided_facts) -join ', '))"
}
if (@($reportData.resource_contract.satisfied_contracts).Count -gt 0) {
    Write-Host "satisfied      = $((@($reportData.resource_contract.satisfied_contracts) -join '; '))"
}
if (@($reportData.resource_contract.violations).Count -gt 0) {
    Write-Host "violations     = $((@($reportData.resource_contract.violations) -join '; '))"
}
if (@($reportData.resource_contract.unknown_contracts).Count -gt 0) {
    Write-Host "unknown        = $((@($reportData.resource_contract.unknown_contracts) -join '; '))"
}
if (@($reportData.resource_contract.resource_hotspots).Count -gt 0) {
    Write-Host "hotspots       = $((@($reportData.resource_contract.resource_hotspots) -join '; '))"
}
Write-Host ''

if ($null -ne $reportData.PSObject.Properties['fact_resolution'] -and $null -ne $reportData.fact_resolution) {
    $factResolution = $reportData.fact_resolution
    Write-Host '[FACT RESOLUTION]'
    Write-Host "declared_contracts = $([int]$factResolution.declared_contracts)"
    Write-Host "audited_count      = $([int]$factResolution.audited_count)"
    Write-Host "satisfied_count    = $([int]$factResolution.satisfied_count)"
    Write-Host "violated_count     = $([int]$factResolution.violated_count)"
    Write-Host "unknown_count      = $([int]$factResolution.unknown_count)"
    foreach ($factGroup in @('declared_facts', 'subject_facts', 'required_facts', 'graph_provided_facts', 'audit_provided_facts')) {
        $factValues = @($factResolution.fact_inventory.$factGroup)
        if (@($factValues).Count -gt 0) {
            Write-Host "$factGroup = $((@($factValues) -join ', '))"
        }
    }
    Write-Host ''
}

if ($null -ne $reportData.PSObject.Properties['comparison'] -and $null -ne $reportData.comparison) {
    $comparisonOverview = New-ReportComparisonOverviewResult -LoadedReport $loadedReport
    Write-Host '[COMPARISON]'
    Write-Host "status = $([string]($comparisonOverview.status))"
    if (@($comparisonOverview.summary_changes).Count -gt 0) {
        Write-Host "summary_changes  = $((@($comparisonOverview.summary_changes) -join '; '))"
    }
    if (@($comparisonOverview.metadata_changes).Count -gt 0) {
        Write-Host "metadata_changes = $((@($comparisonOverview.metadata_changes) -join '; '))"
    }
    if ($null -ne $comparisonOverview.capability_summary) {
        Write-Host "compare_capabilities = $([int]$comparisonOverview.capability_summary.compared_capability_count)"
        Write-Host "bringup_compare_caps = $([int]$comparisonOverview.capability_summary.bringup_compare_capability_count)"
        Write-Host "resource_compare_caps = $([int]$comparisonOverview.capability_summary.resource_compare_capability_count)"
        if (@($comparisonOverview.capability_summary.compared_capabilities).Count -gt 0) {
            Write-Host "compared_capabilities = $((@($comparisonOverview.capability_summary.compared_capabilities) -join ', '))"
        }
    }
    if ($null -ne $comparisonOverview.PSObject.Properties['system_input'] -and $null -ne $comparisonOverview.system_input) {
        $systemInputComparison = $comparisonOverview.system_input
        Write-Host "system_input = changed:$([bool]$systemInputComparison.changed)"
        if (@($systemInputComparison.summary_changes).Count -gt 0) {
            Write-Host "system_input.summary_changes = $((@($systemInputComparison.summary_changes) -join '; '))"
        }
        if (@($systemInputComparison.system_spec_changes).Count -gt 0) {
            Write-Host "system_input.system_spec_changes = $([int]@($systemInputComparison.system_spec_changes).Count)"
        }
        if (@($systemInputComparison.declared_subject_changes).Count -gt 0) {
            Write-Host "system_input.declared_subject_changes = $([int]@($systemInputComparison.declared_subject_changes).Count)"
        }
        if (@($systemInputComparison.declared_fact_changes.added).Count -gt 0 -or @($systemInputComparison.declared_fact_changes.removed).Count -gt 0) {
            Write-Host "system_input.declared_fact_changes = +$([int]@($systemInputComparison.declared_fact_changes.added).Count) -$([int]@($systemInputComparison.declared_fact_changes.removed).Count)"
        }
        if (@($systemInputComparison.declared_contract_changes).Count -gt 0) {
            Write-Host "system_input.declared_contract_changes = $([int]@($systemInputComparison.declared_contract_changes).Count)"
        }
        if (@($systemInputComparison.resolved_input_changes).Count -gt 0) {
            Write-Host "system_input.resolved_input_changes = $([int]@($systemInputComparison.resolved_input_changes).Count)"
        }
        if (@($systemInputComparison.subject_fact_changes.added).Count -gt 0 -or @($systemInputComparison.subject_fact_changes.removed).Count -gt 0) {
            Write-Host "system_input.subject_fact_changes = +$([int]@($systemInputComparison.subject_fact_changes.added).Count) -$([int]@($systemInputComparison.subject_fact_changes.removed).Count)"
        }
    }
    if ($null -ne $comparisonOverview.PSObject.Properties['system_formation'] -and $null -ne $comparisonOverview.system_formation) {
        $systemFormationComparison = $comparisonOverview.system_formation
        Write-Host "system_formation = changed:$([bool]$systemFormationComparison.changed)"
        if (@($systemFormationComparison.summary_changes).Count -gt 0) {
            Write-Host "system_formation.summary_changes = $((@($systemFormationComparison.summary_changes) -join '; '))"
        }
        if (@($systemFormationComparison.blocker_changes).Count -gt 0) {
            Write-Host "system_formation.blocker_changes = $([int]@($systemFormationComparison.blocker_changes).Count)"
        }
        if (@($systemFormationComparison.unresolved_capability_changes.added).Count -gt 0 -or @($systemFormationComparison.unresolved_capability_changes.removed).Count -gt 0) {
            Write-Host "system_formation.unresolved_capability_changes = +$([int]@($systemFormationComparison.unresolved_capability_changes.added).Count) -$([int]@($systemFormationComparison.unresolved_capability_changes.removed).Count)"
        }
        if (@($systemFormationComparison.blocked_node_changes.added).Count -gt 0 -or @($systemFormationComparison.blocked_node_changes.removed).Count -gt 0) {
            Write-Host "system_formation.blocked_node_changes = +$([int]@($systemFormationComparison.blocked_node_changes.added).Count) -$([int]@($systemFormationComparison.blocked_node_changes.removed).Count)"
        }
    }
    if ($null -ne $comparisonOverview.PSObject.Properties['binding_result'] -and $null -ne $comparisonOverview.binding_result) {
        $bindingResultComparison = $comparisonOverview.binding_result
        Write-Host "binding_result = changed:$([bool]$bindingResultComparison.changed)"
        if (@($bindingResultComparison.summary_changes).Count -gt 0) {
            Write-Host "binding_result.summary_changes = $((@($bindingResultComparison.summary_changes) -join '; '))"
        }
        if (@($bindingResultComparison.binding_changes).Count -gt 0) {
            Write-Host "binding_result.binding_changes = $([int]@($bindingResultComparison.binding_changes).Count)"
        }
    }
    if ($null -ne $comparisonOverview.PSObject.Properties['bringup_order'] -and $null -ne $comparisonOverview.bringup_order) {
        $bringupOrderComparison = $comparisonOverview.bringup_order
        Write-Host "bringup_order = changed:$([bool]$bringupOrderComparison.changed)"
        if (@($bringupOrderComparison.summary_changes).Count -gt 0) {
            Write-Host "bringup_order.summary_changes = $((@($bringupOrderComparison.summary_changes) -join '; '))"
        }
        if (@($bringupOrderComparison.entry_changes).Count -gt 0) {
            Write-Host "bringup_order.entry_changes = $([int]@($bringupOrderComparison.entry_changes).Count)"
        }
    }
    if ($null -ne $comparisonOverview.PSObject.Properties['bringup_evidence'] -and $null -ne $comparisonOverview.bringup_evidence) {
        $bringupEvidenceComparison = $comparisonOverview.bringup_evidence
        Write-Host "bringup_evidence = changed:$([bool]$bringupEvidenceComparison.changed)"
        if (@($bringupEvidenceComparison.summary_changes).Count -gt 0) {
            Write-Host "bringup_evidence.summary_changes = $((@($bringupEvidenceComparison.summary_changes) -join '; '))"
        }
        if (@($bringupEvidenceComparison.capability_changes).Count -gt 0) {
            Write-Host "bringup_evidence.capability_changes = $([int]@($bringupEvidenceComparison.capability_changes).Count)"
        }
    }
    if ($null -ne $comparisonOverview.PSObject.Properties['resource_contract'] -and $null -ne $comparisonOverview.resource_contract) {
        $resourceContractComparison = $comparisonOverview.resource_contract
        Write-Host "resource_contract = changed:$([bool]$resourceContractComparison.changed)"
        if (@($resourceContractComparison.summary_changes).Count -gt 0) {
            Write-Host "resource_contract.summary_changes = $((@($resourceContractComparison.summary_changes) -join '; '))"
        }
        if (@($resourceContractComparison.contract_changes).Count -gt 0) {
            Write-Host "resource_contract.contract_changes = $([int]@($resourceContractComparison.contract_changes).Count)"
        }
    }
    if ($null -ne $comparisonOverview.PSObject.Properties['fact_resolution'] -and $null -ne $comparisonOverview.fact_resolution) {
        $factResolutionComparison = $comparisonOverview.fact_resolution
        Write-Host "fact_resolution = changed:$([bool]$factResolutionComparison.changed)"
        if (@($factResolutionComparison.summary_changes).Count -gt 0) {
            Write-Host "fact_resolution.summary_changes = $((@($factResolutionComparison.summary_changes) -join '; '))"
        }
        if (@($factResolutionComparison.contract_changes).Count -gt 0) {
            Write-Host "fact_resolution.contract_changes = $([int]@($factResolutionComparison.contract_changes).Count)"
        }
    }
    Write-Host ''
}

if ($ShowTransitions -and @($reportData.runtime_observe.recent_transitions).Count -gt 0) {
    $recentTransitionsResult = New-RecentTransitionsResult -ReportData $reportData
    if ($null -ne $recentTransitionsResult.comparison) {
        Write-Host "[TRANSITION COMPARE] compared=$([int]$recentTransitionsResult.comparison.compared_transition_count) bringup=$([int]$recentTransitionsResult.comparison.bringup_compare_transition_count) resource=$([int]$recentTransitionsResult.comparison.resource_compare_transition_count)"
        if (@($recentTransitionsResult.comparison.compared_capabilities).Count -gt 0) {
            Write-Host "compared_capabilities = $((@($recentTransitionsResult.comparison.compared_capabilities) -join ', '))"
        }
    }
    Write-Host '[TRANSITIONS]'
    @($recentTransitionsResult.transitions) |
        ForEach-Object { Format-RecentTransitionDisplayRow -Entry $_ } |
        Format-Table -AutoSize |
        Out-Host
    Write-Host ''
}

if ($ShowArtifacts) {
    Write-Host '[ARTIFACTS]'
    foreach ($property in @($reportData.artifacts.PSObject.Properties)) {
        if (-not [string]::IsNullOrWhiteSpace([string]$property.Value)) {
            Write-Host "$($property.Name) = $([string]$property.Value)"
        }
    }
}
