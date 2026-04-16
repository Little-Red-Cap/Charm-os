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

function New-ArtifactRootResourceCaseSummary {
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
        graph_provided_facts = @($resourceSummary.fact_inventory.graph_provided_facts)
        audit_provided_facts = @($resourceSummary.fact_inventory.audit_provided_facts)
        resource_hotspots = @($resourceSummary.resource_hotspots)
        contracts = @($resourceSummary.contracts)
    }
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

function New-ArtifactRootResourceProvidedFactEntry {
    param(
        [string]$FactName,
        [object[]]$CaseSummaries
    )

    if ([string]::IsNullOrWhiteSpace($FactName)) {
        return $null
    }

    $factCases = @()
    foreach ($caseSummary in @($CaseSummaries)) {
        if (-not (@($caseSummary.audit_provided_facts) -contains $FactName)) {
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

function New-ArtifactRootResourceSummaryResult {
    param(
        [object[]]$LoadedReports
    )

    $caseSummaries = @(
        @($LoadedReports) |
            ForEach-Object { New-ArtifactRootResourceCaseSummary -LoadedReport $_ } |
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

    $providedFacts = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.audit_provided_facts)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique

    $resourceHotspots = @(
        foreach ($caseSummary in @($caseSummaries)) {
            @($caseSummary.resource_hotspots)
        }
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Sort-Object -Unique

    $contractMatrix = @(
        foreach ($contractName in @($contractNames)) {
            New-ArtifactRootResourceContractMatrixEntry -ContractName $contractName -CaseSummaries $caseSummaries
        }
    )

    $providedFactMatrix = @(
        foreach ($factName in @($providedFacts)) {
            New-ArtifactRootResourceProvidedFactEntry -FactName $factName -CaseSummaries $caseSummaries
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
        contract_matrix = @($contractMatrix | Sort-Object contract)
        provided_fact_matrix = @($providedFactMatrix | Sort-Object fact)
        resource_hotspot_matrix = @($resourceHotspotMatrix | Sort-Object hotspot)
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

function New-RecentTransitionEntry {
    param(
        [int]$Index,
        $Transition
    )

    if ($null -eq $Transition) {
        return $null
    }

    return [pscustomobject][ordered]@{
        order = $Index
        capability = [string]$Transition.capability
        action = [string]$Transition.action
        before = [string]$Transition.before
        after = [string]$Transition.after
    }
}

function New-RecentTransitionsResult {
    param(
        $ReportData
    )

    $transitionEntries = @()
    $transitionIndex = 0
    foreach ($transition in @($ReportData.runtime_observe.recent_transitions)) {
        $entry = New-RecentTransitionEntry -Index $transitionIndex -Transition $transition
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
        transitions = @($transitionEntries)
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

    return [ordered]@{
        capability = $CapabilityName
        state = $state
        reasons = @($reasons)
        evidence = $evidence
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

function New-CapListComparisonSummaryResult {
    param(
        [object[]]$Capabilities
    )

    $comparedCapabilities = @(
        @($Capabilities) |
            Where-Object {
                if ($null -ne $_.PSObject.Properties['comparison'] -and $null -ne $_.comparison) {
                    return ([bool]$_.comparison.bringup_changed -or [bool]$_.comparison.resource_changed)
                }

                return ([bool]$_.bringup_compare -or [bool]$_.resource_compare)
            } |
            ForEach-Object { [string]$_.capability } |
            Sort-Object -Unique
    )
    $bringupComparedCapabilities = @(
        @($Capabilities) |
            Where-Object {
                if ($null -ne $_.PSObject.Properties['comparison'] -and $null -ne $_.comparison) {
                    return [bool]$_.comparison.bringup_changed
                }

                return [bool]$_.bringup_compare
            } |
            ForEach-Object { [string]$_.capability } |
            Sort-Object -Unique
    )
    $resourceComparedCapabilities = @(
        @($Capabilities) |
            Where-Object {
                if ($null -ne $_.PSObject.Properties['comparison'] -and $null -ne $_.comparison) {
                    return [bool]$_.comparison.resource_changed
                }

                return [bool]$_.resource_compare
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
    $bringupComparison = Get-BringupEvidenceComparisonFromReport -ReportData $report
    $resourceComparison = Get-ResourceContractComparisonFromReport -ReportData $report

    return [pscustomobject][ordered]@{
        case = [string]$report.subject.case
        profile = [string]$report.subject.profile
        board = [string]$report.subject.board
        compared = $hasComparison
        compare_status = Get-ComparisonStatus -ReportData $report
        metadata_change_count = [int](Get-MetadataChangeCount -ReportData $report)
        bringup_changed = ($null -ne $bringupComparison -and [bool]$bringupComparison.changed)
        bringup_change_count = if ($null -ne $bringupComparison) { [int]@($bringupComparison.capability_changes).Count } else { 0 }
        resource_changed = ($null -ne $resourceComparison -and [bool]$resourceComparison.changed)
        resource_change_count = if ($null -ne $resourceComparison) { [int]@($resourceComparison.contract_changes).Count } else { 0 }
    }
}

function New-ArtifactRootComparisonOverviewResult {
    param(
        [object[]]$LoadedReports,
        $CapabilityComparisonSummary
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
        bringup_changed_case_count = @($comparedCases | Where-Object { [bool]$_.bringup_changed }).Count
        resource_changed_case_count = @($comparedCases | Where-Object { [bool]$_.resource_changed }).Count
        compared_cases = @($comparedCases | ForEach-Object { [string]$_.case })
        metadata_changed_cases = @(
            @($comparedCases) |
                Where-Object { [int]$_.metadata_change_count -gt 0 } |
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
    }

    if ($null -ne $CapabilityComparisonSummary) {
        $result.capability_summary = $CapabilityComparisonSummary
    }

    return $result
}

function New-CaseSummaryRow {
    param(
        $LoadedReport
    )

    $report = $LoadedReport.Data
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
        Compare = Get-ComparisonStatus -ReportData $report
        Metadata = Get-MetadataChangeCount -ReportData $report
        BrCmp = if ($null -ne $bringupComparison -and [bool]$bringupComparison.changed) { [int]@($bringupComparison.capability_changes).Count } else { 0 }
        ResCmp = if ($null -ne $resourceComparison -and [bool]$resourceComparison.changed) { [int]@($resourceComparison.contract_changes).Count } else { 0 }
    }
}

function New-ArtifactJsonView {
    param(
        $LoadedReport
    )

    $report = $LoadedReport.Data
    return [ordered]@{
        report_path = $LoadedReport.Path
        summary = New-CaseSummaryRow -LoadedReport $LoadedReport
        subject = $report.subject
        structure = $report.structure
        bringup_evidence = $report.bringup_evidence
        resource_contract = $report.resource_contract
        runtime_observe = $report.runtime_observe
        comparison = if ($null -ne $report.PSObject.Properties['comparison']) { $report.comparison } else { $null }
        artifacts = $report.artifacts
    }
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

if (-not [string]::IsNullOrWhiteSpace($WhyCapability) -and $selectedReports.Count -ne 1) {
    throw "-WhyCapability requires exactly one selected artifact report"
}

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

    $comparisonOverview = New-ArtifactRootComparisonOverviewResult -LoadedReports $selectedReports -CapabilityComparisonSummary $comparisonCapabilitySummary
    if ($AsJson) {
        $payload = [ordered]@{
            artifact_root = $artifactRootPath
            case_count = $summaryRows.Count
            cases = $summaryRows
        }
        if ($null -ne $comparisonOverview) {
            $payload.comparison = $comparisonOverview
        }
        $payload | ConvertTo-Json -Depth 10
    } else {
        Write-Host "[ARTIFACT ROOT] $artifactRootPath"
        if ($null -ne $comparisonOverview) {
            Write-Host '[COMPARISON]'
            Write-Host "compared_case_count      = $([int]$comparisonOverview.compared_case_count)"
            Write-Host "metadata_changed_cases  = $([int]$comparisonOverview.metadata_changed_case_count)"
            Write-Host "bringup_changed_cases   = $([int]$comparisonOverview.bringup_changed_case_count)"
            Write-Host "resource_changed_cases  = $([int]$comparisonOverview.resource_changed_case_count)"
            if (@($comparisonOverview.compared_cases).Count -gt 0) {
                Write-Host "compared_cases          = $((@($comparisonOverview.compared_cases) -join ', '))"
            }
            if ($null -ne $comparisonOverview.capability_summary) {
                Write-Host "compare_capabilities    = $([int]$comparisonOverview.capability_summary.compared_capability_count)"
                Write-Host "bringup_compare_caps    = $([int]$comparisonOverview.capability_summary.bringup_compare_capability_count)"
                Write-Host "resource_compare_caps   = $([int]$comparisonOverview.capability_summary.resource_compare_capability_count)"
                if (@($comparisonOverview.capability_summary.compared_capabilities).Count -gt 0) {
                    Write-Host "compared_capabilities   = $((@($comparisonOverview.capability_summary.compared_capabilities) -join ', '))"
                }
            }
            Write-Host ''
        }
        $summaryRows | Sort-Object Case | Format-Table -AutoSize Case, Mode, Profile, Board, Facets, Nodes, Edges, Unresolved, Contracts, Satisfied, Violated, Unknown, Compare, Metadata, BrCmp, ResCmp | Out-Host
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
    if (@($recentTransitionsResult.transitions).Count -gt 0) {
        Write-Host ''
        Write-Host '[TRANSITIONS]'
        @($recentTransitionsResult.transitions) |
            Select-Object order, capability, action, before, after |
            Format-Table -AutoSize |
            Out-Host
    }
    exit 0
}

if ($ResourceSummary) {
    if ($selectedReports.Count -ne 1) {
        $artifactRootResourceSummary = New-ArtifactRootResourceSummaryResult -LoadedReports $selectedReports
        $artifactRootResourceComparison = New-ArtifactRootResourceContractComparisonResult -LoadedReports $selectedReports

        if ($AsJson) {
            $queryPayload = [ordered]@{
                kind = 'resource_summary'
                scope = 'artifact_root'
                result = $artifactRootResourceSummary
            }
            if ($null -ne $artifactRootResourceComparison) {
                $queryPayload.comparison = [ordered]@{
                    resource_contract = $artifactRootResourceComparison
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
                    @{ Name = 'provided_facts'; Expression = { @($_.audit_provided_facts).Count } },
                    @{ Name = 'hotspots'; Expression = { @($_.resource_hotspots).Count } } |
                Format-Table -Wrap -AutoSize |
                Out-Host
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
        exit 0
    }

    $resourceSummaryResult = New-ResourceSummaryResult -ReportData $reportData -GraphInfo $graphInfo
    $resourceContractComparison = Get-ResourceContractComparisonFromReport -ReportData $reportData

    if ($AsJson) {
        $queryPayload = [ordered]@{
            kind = 'resource_summary'
            scope = 'report'
            result = $resourceSummaryResult
        }
        if ($null -ne $resourceContractComparison) {
            $queryPayload.comparison = [ordered]@{
                resource_contract = $resourceContractComparison
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
    foreach ($factGroup in @('declared_facts', 'subject_facts', 'graph_provided_facts', 'audit_provided_facts', 'all_available_facts')) {
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

$summaryRows | Format-List Case, Mode, Profile, Board, Facets, Nodes, Edges, Unresolved, Contracts, Satisfied, Violated, Unknown, Compare, Metadata | Out-Host

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

if ($null -ne $reportData.PSObject.Properties['comparison'] -and $null -ne $reportData.comparison) {
    Write-Host '[COMPARISON]'
    Write-Host "status = $([string]($reportData.comparison.status))"
    if (@($reportData.comparison.summary_changes).Count -gt 0) {
        Write-Host "summary_changes  = $((@($reportData.comparison.summary_changes) -join '; '))"
    }
    if (@($reportData.comparison.metadata_changes).Count -gt 0) {
        Write-Host "metadata_changes = $((@($reportData.comparison.metadata_changes) -join '; '))"
    }
    if ($null -ne $reportData.comparison.PSObject.Properties['bringup_evidence'] -and $null -ne $reportData.comparison.bringup_evidence) {
        $bringupEvidenceComparison = $reportData.comparison.bringup_evidence
        Write-Host "bringup_evidence = changed:$([bool]$bringupEvidenceComparison.changed)"
        if (@($bringupEvidenceComparison.summary_changes).Count -gt 0) {
            Write-Host "bringup_evidence.summary_changes = $((@($bringupEvidenceComparison.summary_changes) -join '; '))"
        }
        if (@($bringupEvidenceComparison.capability_changes).Count -gt 0) {
            Write-Host "bringup_evidence.capability_changes = $([int]@($bringupEvidenceComparison.capability_changes).Count)"
        }
    }
    if ($null -ne $reportData.comparison.PSObject.Properties['resource_contract'] -and $null -ne $reportData.comparison.resource_contract) {
        $resourceContractComparison = $reportData.comparison.resource_contract
        Write-Host "resource_contract = changed:$([bool]$resourceContractComparison.changed)"
        if (@($resourceContractComparison.summary_changes).Count -gt 0) {
            Write-Host "resource_contract.summary_changes = $((@($resourceContractComparison.summary_changes) -join '; '))"
        }
        if (@($resourceContractComparison.contract_changes).Count -gt 0) {
            Write-Host "resource_contract.contract_changes = $([int]@($resourceContractComparison.contract_changes).Count)"
        }
    }
    Write-Host ''
}

if ($ShowTransitions -and @($reportData.runtime_observe.recent_transitions).Count -gt 0) {
    Write-Host '[TRANSITIONS]'
    @($reportData.runtime_observe.recent_transitions) |
        Select-Object capability, action, before, after |
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
