param(
    [string]$BundleRoot = "out/materialized-graph-bundle",
    [string]$Index = "",
    [string[]]$Case = @(),
    [string]$OutputRoot = "out/system-compiler-artifact-report",
    [string]$OutputPath = "",
    [string]$Profile = "",
    [string]$Board = "",
    [string[]]$Facet = @(),
    [string]$CiSummary = "",
    [string]$ReportManifest = "",
    [string]$DiffJson = "",
    [ValidateSet('export_only', 'compare')]
    [string]$Mode = "export_only"
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

function Ensure-ParentDirectory {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent) -and -not (Test-Path $parent)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
}

function Resolve-IndexPath {
    if (-not [string]::IsNullOrWhiteSpace($Index)) {
        return Resolve-FullPath $Index
    }

    return Join-Path (Resolve-FullPath $BundleRoot) 'index.json'
}

function Load-BundleByIndexPath {
    param(
        [string]$IndexPath
    )

    if (-not (Test-Path $indexPath)) {
        throw "bundle index not found: $indexPath"
    }

    $indexData = Get-Content -LiteralPath $indexPath -Raw -Encoding utf8 | ConvertFrom-Json
    if ([string]$indexData.schema -ne 'materialized_graph.export_bundle/v1') {
        throw "unsupported bundle schema: $($indexData.schema)"
    }

    return [pscustomobject]@{
        IndexPath = $indexPath
        BundleRoot = Split-Path -Parent $indexPath
        InputManifestPath = if ($null -ne $indexData.PSObject.Properties['input_manifest'] -and $null -ne $indexData.input_manifest -and $null -ne $indexData.input_manifest.PSObject.Properties['path'] -and -not [string]::IsNullOrWhiteSpace([string]$indexData.input_manifest.path)) {
            Resolve-FullPath ([string]$indexData.input_manifest.path)
        } else {
            $null
        }
        Cases = @($indexData.cases)
    }
}

function Load-Bundle {
    $indexPath = Resolve-IndexPath
    return Load-BundleByIndexPath -IndexPath $indexPath
}

function Resolve-CaseArtifactPath {
    param(
        [string]$BundleRootPath,
        [string]$RelativeOrAbsolutePath
    )

    if ([string]::IsNullOrWhiteSpace($RelativeOrAbsolutePath)) {
        return ''
    }

    if ([System.IO.Path]::IsPathRooted($RelativeOrAbsolutePath)) {
        return Resolve-FullPath $RelativeOrAbsolutePath
    }

    return Resolve-FullPath (Join-Path $BundleRootPath $RelativeOrAbsolutePath)
}

function Get-CaseKind {
    param(
        $CaseEntry
    )

    if ($null -eq $CaseEntry -or
        $null -eq $CaseEntry.PSObject.Properties['case_kind'] -or
        [string]::IsNullOrWhiteSpace([string]$CaseEntry.case_kind)) {
        return 'materialized_graph'
    }

    $normalized = [string]$CaseEntry.case_kind
    if ($normalized -notin @('materialized_graph', 'runtime_only')) {
        throw "unsupported case kind '$normalized' in bundle case entry"
    }

    return $normalized
}

function New-EmptyRuntimeObserve {
    return [ordered]@{
        observed_capabilities = @()
        publish_state_summary = [ordered]@{
            missing = 0
            published = 0
        }
        export_state_summary = [ordered]@{
            missing = 0
            detached = 0
            attached = 0
        }
        recent_transitions = @()
    }
}

function Load-CaseGraph {
    param(
        $Bundle,
        $CaseEntry
    )

    $caseKind = Get-CaseKind -CaseEntry $CaseEntry
    $jsonValue = if ($null -ne $CaseEntry.PSObject.Properties['json']) {
        [string]$CaseEntry.json
    } else {
        ''
    }
    if ([string]::IsNullOrWhiteSpace($jsonValue)) {
        if ($caseKind -eq 'runtime_only') {
            return $null
        }
        throw "case json missing for materialized_graph case: $([string]$CaseEntry.name)"
    }

    $jsonPath = Resolve-CaseArtifactPath -BundleRootPath $Bundle.BundleRoot -RelativeOrAbsolutePath $jsonValue
    if (-not (Test-Path $jsonPath)) {
        throw "case json not found: $jsonPath"
    }

    $graph = Get-Content -LiteralPath $jsonPath -Raw -Encoding utf8 | ConvertFrom-Json
    Assert-MaterializedGraphSampleShape -Graph $graph -Context $jsonPath
    return [pscustomobject]@{
        Path = $jsonPath
        Data = $graph
    }
}

function Load-CaseRuntimeObserve {
    param(
        $Bundle,
        $CaseEntry
    )

    if ($null -eq $CaseEntry -or
        $null -eq $CaseEntry.PSObject.Properties['runtime_observe'] -or
        [string]::IsNullOrWhiteSpace([string]$CaseEntry.runtime_observe)) {
        return $null
    }

    $runtimeObservePath = Resolve-CaseArtifactPath -BundleRootPath $Bundle.BundleRoot -RelativeOrAbsolutePath ([string]$CaseEntry.runtime_observe)
    if (-not (Test-Path $runtimeObservePath)) {
        throw "case runtime observe artifact not found: $runtimeObservePath"
    }

    $runtimeObserve = Get-Content -LiteralPath $runtimeObservePath -Raw -Encoding utf8 | ConvertFrom-Json
    if ([string]$runtimeObserve.schema -ne 'system_compiler.runtime_observe_snapshot/v0') {
        throw "unsupported runtime observe schema: $([string]$runtimeObserve.schema)"
    }

    return [pscustomobject]@{
        Path = $runtimeObservePath
        Data = $runtimeObserve
    }
}

function Get-SelectedCases {
    param(
        $Bundle
    )

    if ($Case.Count -eq 0) {
        return @($Bundle.Cases)
    }

    $selected = @()
    foreach ($caseName in $Case) {
        $match = @($Bundle.Cases | Where-Object { $_.name -eq $caseName })
        if ($match.Count -eq 0) {
            throw "unknown case: $caseName"
        }

        $selected += $match[0]
    }

    return $selected
}

function Get-CaseEntryByName {
    param(
        $Bundle,
        [string]$CaseName
    )

    if ($null -eq $Bundle -or [string]::IsNullOrWhiteSpace($CaseName)) {
        return $null
    }

    return @(
        @($Bundle.Cases) |
            Where-Object { [string]$_.name -eq $CaseName } |
            Select-Object -First 1
    ) | Select-Object -First 1
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

function Get-UniqueNodeNames {
    param(
        $Graph
    )

    $names = @()
    foreach ($node in @($Graph.nodes | Sort-Object index)) {
        $name = [string]$node.name
        if (-not [string]::IsNullOrWhiteSpace($name)) {
            $names += $name
        }
    }

    return @($names | Sort-Object -Unique)
}

function Get-MaterializedOrder {
    param(
        $Graph
    )

    $names = @()
    foreach ($node in @($Graph.nodes | Sort-Object index)) {
        $name = [string]$node.name
        if (-not [string]::IsNullOrWhiteSpace($name)) {
            $names += $name
        }
    }

    return $names
}

function Get-RequiredFacts {
    param(
        $Graph
    )

    $required = @()
    foreach ($node in @($Graph.nodes)) {
        $required += Get-CapabilityNames -Capabilities $node.requires
    }

    return @($required | Sort-Object -Unique)
}

function Get-ProvidedFacts {
    param(
        $Graph
    )

    $provided = @()
    foreach ($node in @($Graph.nodes)) {
        $provided += Get-CapabilityNames -Capabilities $node.provides
    }

    return @($provided | Sort-Object -Unique)
}

function Get-UnresolvedBindings {
    param(
        [string[]]$RequiredFacts,
        [string[]]$ProvidedFacts
    )

    $providedSet = @{}
    foreach ($fact in @($ProvidedFacts)) {
        $providedSet[$fact] = $true
    }

    $unresolved = @()
    foreach ($fact in @($RequiredFacts)) {
        if (-not $providedSet.ContainsKey($fact)) {
            $unresolved += $fact
        }
    }

    return @($unresolved | Sort-Object -Unique)
}

function Get-BindingEntries {
    param(
        $Graph
    )

    if ($null -eq $Graph) {
        return @()
    }

    $providerMap = @{}
    $consumerMap = @{}
    foreach ($node in @($Graph.nodes)) {
        $nodeName = [string]$node.name
        if ([string]::IsNullOrWhiteSpace($nodeName)) {
            continue
        }

        foreach ($capabilityName in @(Get-CapabilityNames -Capabilities $node.provides)) {
            if (-not $providerMap.ContainsKey($capabilityName)) {
                $providerMap[$capabilityName] = @()
            }
            $providerMap[$capabilityName] = @($providerMap[$capabilityName] + $nodeName | Sort-Object -Unique)
        }

        foreach ($capabilityName in @(Get-CapabilityNames -Capabilities $node.requires)) {
            if (-not $consumerMap.ContainsKey($capabilityName)) {
                $consumerMap[$capabilityName] = @()
            }
            $consumerMap[$capabilityName] = @($consumerMap[$capabilityName] + $nodeName | Sort-Object -Unique)
        }
    }

    $entries = @()
    foreach ($capabilityName in @($consumerMap.Keys | Sort-Object)) {
        $providerNodes = if ($providerMap.ContainsKey($capabilityName)) {
            @($providerMap[$capabilityName] | Sort-Object -Unique)
        } else {
            @()
        }
        $consumerNodes = @($consumerMap[$capabilityName] | Sort-Object -Unique)
        $state = if (@($providerNodes).Count -gt 0) { 'resolved' } else { 'unresolved' }
        $reason = if ($state -eq 'resolved') {
            'required capability is provided by at least one materialized node'
        } else {
            'required capability was not provided by any materialized node'
        }

        $entries += [ordered]@{
            capability = $capabilityName
            state = $state
            provider_nodes = @($providerNodes)
            consumer_nodes = @($consumerNodes)
            reason = $reason
        }
    }

    return @($entries)
}

function Get-BindingResultSummary {
    param(
        $Graph
    )

    $bindingEntries = @(Get-BindingEntries -Graph $Graph)
    $resolvedCapabilities = @(
        @($bindingEntries) |
            Where-Object { [string]$_.state -eq 'resolved' } |
            ForEach-Object { [string]$_.capability } |
            Sort-Object -Unique
    )
    $unresolvedCapabilities = @(
        @($bindingEntries) |
            Where-Object { [string]$_.state -eq 'unresolved' } |
            ForEach-Object { [string]$_.capability } |
            Sort-Object -Unique
    )

    return [ordered]@{
        required_binding_count = @($bindingEntries).Count
        resolved_binding_count = @($resolvedCapabilities).Count
        unresolved_binding_count = @($unresolvedCapabilities).Count
        resolved_capabilities = @($resolvedCapabilities)
        unresolved_capabilities = @($unresolvedCapabilities)
        binding_entries = @($bindingEntries)
    }
}

function Add-CountMapEntry {
    param(
        [hashtable]$Counts,
        [string]$Name
    )

    if ([string]::IsNullOrWhiteSpace($Name)) {
        return
    }

    if ($Counts.ContainsKey($Name)) {
        $Counts[$Name] = [int]$Counts[$Name] + 1
    } else {
        $Counts[$Name] = 1
    }
}

function ConvertTo-OrderedCountMap {
    param(
        [hashtable]$Counts
    )

    $result = [ordered]@{}
    foreach ($entry in @($Counts.GetEnumerator() | Sort-Object Key)) {
        $result[[string]$entry.Key] = [int]$entry.Value
    }

    return $result
}

function Get-GraphConnectionSummary {
    param(
        $Graph
    )

    $connections = @()
    $connectionModes = @{}

    if ($null -ne $Graph -and $null -ne $Graph.PSObject.Properties['nodes']) {
        foreach ($node in @($Graph.nodes | Sort-Object index)) {
            if ($null -eq $node) {
                continue
            }

            $hasConnectionPayload = ($null -ne $node.PSObject.Properties['connection']) -and ($null -ne $node.connection)
            if (([string]$node.kind -ne 'connection') -and -not $hasConnectionPayload) {
                continue
            }

            $connectionInfo = if ($hasConnectionPayload) { $node.connection } else { $null }
            $source = $null
            $sink = $null
            $mode = $null
            if ($null -ne $connectionInfo) {
                if ($null -ne $connectionInfo.PSObject.Properties['source'] -and -not [string]::IsNullOrWhiteSpace([string]$connectionInfo.source)) {
                    $source = [string]$connectionInfo.source
                }
                if ($null -ne $connectionInfo.PSObject.Properties['sink'] -and -not [string]::IsNullOrWhiteSpace([string]$connectionInfo.sink)) {
                    $sink = [string]$connectionInfo.sink
                }
                if ($null -ne $connectionInfo.PSObject.Properties['mode'] -and -not [string]::IsNullOrWhiteSpace([string]$connectionInfo.mode)) {
                    $mode = [string]$connectionInfo.mode
                }
            }

            Add-CountMapEntry -Counts $connectionModes -Name $mode

            $phase = $null
            if ($null -ne $node.PSObject.Properties['phase'] -and -not [string]::IsNullOrWhiteSpace([string]$node.phase)) {
                $phase = [string]$node.phase
            }

            $runlevelText = $null
            if ($null -ne $node.PSObject.Properties['runlevel_text'] -and -not [string]::IsNullOrWhiteSpace([string]$node.runlevel_text)) {
                $runlevelText = [string]$node.runlevel_text
            }

            $connections += [ordered]@{
                name = [string]$node.name
                phase = $phase
                runlevel_text = $runlevelText
                source = $source
                sink = $sink
                mode = $mode
                requires = @(Get-CapabilityNames -Capabilities $node.requires)
            }
        }
    }

    return [ordered]@{
        connection_node_count = @($connections).Count
        connection_modes = ConvertTo-OrderedCountMap -Counts $connectionModes
        connections = @($connections)
    }
}

function Get-BringupOrderSummary {
    param(
        $Graph
    )

    if ($null -eq $Graph) {
        return [ordered]@{
            ordered_node_count = 0
            blocked_node_count = 0
            phase_counts = [ordered]@{}
            entries = @()
        }
    }

    $nodeByIndex = @{}
    foreach ($node in @($Graph.nodes)) {
        $nodeByIndex[[int]$node.index] = $node
    }

    $dependencyMap = @{}
    foreach ($edge in @($Graph.edges)) {
        if ($null -eq $edge) {
            continue
        }

        $consumerIndex = [int]$edge.consumer_index
        $providerIndex = [int]$edge.provider_index
        if (-not $dependencyMap.ContainsKey($consumerIndex)) {
            $dependencyMap[$consumerIndex] = @()
        }

        $providerNode = if ($nodeByIndex.ContainsKey($providerIndex)) { $nodeByIndex[$providerIndex] } else { $null }
        $providerName = if ($null -ne $providerNode) { [string]$providerNode.name } else { '' }
        $capabilityName = $null
        if ($null -ne $edge.PSObject.Properties['capability'] -and $null -ne $edge.capability) {
            $capabilityName = [string]$edge.capability.name
            if ([string]::IsNullOrWhiteSpace($capabilityName)) {
                $capabilityName = [string]$edge.capability.id
            }
        }

        $dependencyMap[$consumerIndex] += [ordered]@{
            provider_node = $providerName
            capability = $capabilityName
        }
    }

    $phaseCounts = @{}
    $entries = @()
    foreach ($node in @($Graph.nodes | Sort-Object index)) {
        if ($null -eq $node) {
            continue
        }

        $phaseName = [string]$node.phase
        Add-CountMapEntry -Counts $phaseCounts -Name $phaseName

        $requires = @(Get-CapabilityNames -Capabilities $node.requires)
        $provides = @(Get-CapabilityNames -Capabilities $node.provides)
        $dependencyEntries = if ($dependencyMap.ContainsKey([int]$node.index)) {
            @(
                @($dependencyMap[[int]$node.index]) |
                    Sort-Object capability, provider_node
            )
        } else {
            @()
        }
        $resolvedRequires = @(
            @($dependencyEntries) |
                ForEach-Object { [string]$_.capability } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        $dependencyNodes = @(
            @($dependencyEntries) |
                ForEach-Object { [string]$_.provider_node } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                Sort-Object -Unique
        )
        $missingRequires = @(
            @($requires) |
                Where-Object { @($resolvedRequires) -notcontains [string]$_ } |
                Sort-Object -Unique
        )
        $state = if (@($missingRequires).Count -gt 0) { 'blocked' } else { 'ready' }

        $entries += [ordered]@{
            order = [int]$node.index
            node = [string]$node.name
            kind = [string]$node.kind
            phase = $phaseName
            runlevel_text = [string]$node.runlevel_text
            provides = @($provides)
            requires = @($requires)
            dependency_nodes = @($dependencyNodes)
            resolved_requires = @($resolvedRequires)
            missing_requires = @($missingRequires)
            state = $state
        }
    }

    return [ordered]@{
        ordered_node_count = @($entries).Count
        blocked_node_count = @(
            @($entries) |
                Where-Object { [string]$_.state -eq 'blocked' }
        ).Count
        phase_counts = ConvertTo-OrderedCountMap -Counts $phaseCounts
        entries = @($entries)
    }
}

function Get-RuntimeObserveSummary {
    param(
        $RuntimeObserveInfo
    )

    if ($null -eq $RuntimeObserveInfo) {
        return New-EmptyRuntimeObserve
    }

    return [ordered]@{
        observed_capabilities = @(Get-RuntimeCapabilityNames -RuntimeObserveInfo $RuntimeObserveInfo)
        publish_state_summary = [ordered]@{
            missing = [int]$RuntimeObserveInfo.Data.publish_state_summary.missing
            published = [int]$RuntimeObserveInfo.Data.publish_state_summary.published
        }
        export_state_summary = [ordered]@{
            missing = [int]$RuntimeObserveInfo.Data.export_state_summary.missing
            detached = [int]$RuntimeObserveInfo.Data.export_state_summary.detached
            attached = [int]$RuntimeObserveInfo.Data.export_state_summary.attached
        }
        recent_transitions = @($RuntimeObserveInfo.Data.recent_transitions)
    }
}

function Get-RuntimePublishedCapabilities {
    param(
        $RuntimeObserveInfo
    )

    if ($null -eq $RuntimeObserveInfo -or
        $null -eq $RuntimeObserveInfo.Data.PSObject.Properties['published_capabilities']) {
        return @()
    }

    return @(
        @($RuntimeObserveInfo.Data.published_capabilities) |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [string]$_ } |
            Sort-Object -Unique
    )
}

function Get-RuntimeCapabilityNames {
    param(
        $RuntimeObserveInfo
    )

    if ($null -eq $RuntimeObserveInfo) {
        return @()
    }

    $names = @()
    if ($null -ne $RuntimeObserveInfo.Data.PSObject.Properties['observed_capabilities']) {
        $names += @(
            @($RuntimeObserveInfo.Data.observed_capabilities) |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                ForEach-Object { [string]$_ }
        )
    }
    if ($null -ne $RuntimeObserveInfo.Data.PSObject.Properties['published_capabilities']) {
        $names += @(
            @($RuntimeObserveInfo.Data.published_capabilities) |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                ForEach-Object { [string]$_ }
        )
    }
    if ($null -ne $RuntimeObserveInfo.Data.PSObject.Properties['recent_transitions']) {
        $names += @(
            @($RuntimeObserveInfo.Data.recent_transitions) |
                ForEach-Object { [string]$_.capability } |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) }
        )
    }

    return @($names | Sort-Object -Unique)
}

function Get-RuntimeObservedCount {
    param(
        $RuntimeObserveInfo
    )

    if ($null -eq $RuntimeObserveInfo) {
        return $null
    }

    if ($null -ne $RuntimeObserveInfo.Data.PSObject.Properties['observed_capabilities']) {
        return @(
            @($RuntimeObserveInfo.Data.observed_capabilities) |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                ForEach-Object { [string]$_ } |
                Sort-Object -Unique
        ).Count
    }

    return ([int]$RuntimeObserveInfo.Data.export_state_summary.missing) +
           ([int]$RuntimeObserveInfo.Data.export_state_summary.detached) +
           ([int]$RuntimeObserveInfo.Data.export_state_summary.attached)
}

function Get-GraphCapabilityProjection {
    param(
        $Graph,
        [string]$CapabilityName
    )

    $providers = @()
    $consumers = @()
    if ($null -ne $Graph) {
        foreach ($node in @($Graph.nodes)) {
            $provided = @(Get-CapabilityNames -Capabilities $node.provides)
            $required = @(Get-CapabilityNames -Capabilities $node.requires)
            if ($provided -contains $CapabilityName) {
                $providers += [string]$node.name
            }
            if ($required -contains $CapabilityName) {
                $consumers += [string]$node.name
            }
        }
    }

    $providerNodes = @($providers | Sort-Object -Unique)
    $consumerNodes = @($consumers | Sort-Object -Unique)

    return [ordered]@{
        provider_nodes = $providerNodes
        consumer_nodes = $consumerNodes
        materialized = ($providerNodes.Count -gt 0) -or ($consumerNodes.Count -gt 0)
    }
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

function Get-OptionalStringArrayValue {
    param(
        $Object,
        [string]$PropertyName
    )

    if ($null -eq $Object -or $null -eq $Object.PSObject.Properties[$PropertyName]) {
        return @()
    }

    return @(
        @($Object.$PropertyName) |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [string]$_ }
    )
}

function Get-RuntimeExportStateMap {
    param(
        $RuntimeObserveInfo
    )

    $states = @{}
    if ($null -eq $RuntimeObserveInfo -or
        $null -eq $RuntimeObserveInfo.Data.PSObject.Properties['recent_transitions']) {
        return $states
    }

    foreach ($transition in @($RuntimeObserveInfo.Data.recent_transitions)) {
        if ($null -eq $transition) {
            continue
        }

        $capabilityName = [string]$transition.capability
        if ([string]::IsNullOrWhiteSpace($capabilityName)) {
            continue
        }

        $afterState = [string]$transition.after
        if ([string]::IsNullOrWhiteSpace($afterState)) {
            continue
        }

        $states[$capabilityName] = $afterState
    }

    return $states
}

function Get-UnifiedObservedCapabilities {
    param(
        $Graph,
        [string[]]$RuntimeCapabilities
    )

    $names = @()
    if ($null -ne $Graph) {
        $names += @(Get-ProvidedFacts -Graph $Graph)
        $names += @(Get-RequiredFacts -Graph $Graph)
    }
    $names += @($RuntimeCapabilities)

    return @(
        @($names) |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            Sort-Object -Unique
    )
}

function New-BringupEvidenceEntries {
    param(
        $Graph,
        [string[]]$CapabilityNames,
        [string[]]$ObservedCapabilities,
        [string[]]$PublishedCapabilities,
        [string[]]$BlockedReasons,
        [string[]]$FailedReasons,
        $RuntimeExportStates
    )

    $entries = @()
    $hasGraph = $null -ne $Graph
    foreach ($capabilityName in @($CapabilityNames)) {
        if ([string]::IsNullOrWhiteSpace([string]$capabilityName)) {
            continue
        }

        $graphProjection = Get-GraphCapabilityProjection -Graph $Graph -CapabilityName $capabilityName
        $published = @($PublishedCapabilities) -contains $capabilityName
        $observed = @($ObservedCapabilities) -contains $capabilityName
        $blockedReasonsForCapability = @(Get-CapabilityScopedReasons -Reasons $BlockedReasons -CapabilityName $capabilityName)
        $failedReasonsForCapability = @(Get-CapabilityScopedReasons -Reasons $FailedReasons -CapabilityName $capabilityName)
        $blocked = @($blockedReasonsForCapability).Count -gt 0
        $failed = @($failedReasonsForCapability).Count -gt 0
        $materialized = if ($hasGraph) {
            [bool]$graphProjection.materialized
        } else {
            $observed -or $published -or $RuntimeExportStates.ContainsKey($capabilityName)
        }
        $declared = $materialized -or $observed -or $published -or $blocked -or $failed
        $publishState = if ($published) {
            'published'
        } elseif ($declared) {
            'missing'
        } else {
            $null
        }
        $exportState = if ($RuntimeExportStates.ContainsKey($capabilityName)) {
            [string]$RuntimeExportStates[$capabilityName]
        } else {
            $null
        }

        $entries += [ordered]@{
            capability = [string]$capabilityName
            declared = [bool]$declared
            materialized = [bool]$materialized
            published = [bool]$published
            observed = [bool]$observed
            blocked = [bool]$blocked
            failed = [bool]$failed
            publish_state = $publishState
            export_state = $exportState
            provider_nodes = @($graphProjection.provider_nodes)
            consumer_nodes = @($graphProjection.consumer_nodes)
            blocked_reasons = @($blockedReasonsForCapability)
            failed_reasons = @($failedReasonsForCapability)
        }
    }

    return @($entries | Sort-Object capability)
}

function Get-SubjectInfo {
    param(
        $SubjectLike
    )

    $profile = $null
    $board = $null
    $activeFacets = @()

    if ($null -ne $SubjectLike) {
        if ($null -ne $SubjectLike.PSObject.Properties['profile'] -and -not [string]::IsNullOrWhiteSpace([string]$SubjectLike.profile)) {
            $profile = [string]$SubjectLike.profile
        }
        if ($null -ne $SubjectLike.PSObject.Properties['board'] -and -not [string]::IsNullOrWhiteSpace([string]$SubjectLike.board)) {
            $board = [string]$SubjectLike.board
        }
        if ($null -ne $SubjectLike.PSObject.Properties['active_facets']) {
            $activeFacets = @(
                @($SubjectLike.active_facets) |
                    Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                    ForEach-Object { [string]$_ }
            )
        }
    }

    return [pscustomobject]@{
        Profile = $profile
        Board = $board
        ActiveFacets = @($activeFacets)
    }
}

function Get-OptionalCaseEntryString {
    param(
        $CaseEntry,
        [string]$PropertyName
    )

    if ($null -eq $CaseEntry -or [string]::IsNullOrWhiteSpace($PropertyName)) {
        return $null
    }

    if ($null -eq $CaseEntry.PSObject.Properties[$PropertyName]) {
        return $null
    }

    $value = [string]$CaseEntry.$PropertyName
    if ([string]::IsNullOrWhiteSpace($value)) {
        return $null
    }

    return $value
}

function Get-CaseDeclaredFacts {
    param(
        $CaseEntry
    )

    if ($null -eq $CaseEntry -or $null -eq $CaseEntry.PSObject.Properties['declared_facts']) {
        return @()
    }

    return @(
        @($CaseEntry.declared_facts) |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [string]$_ } |
            Select-Object -Unique
    )
}

function Get-CaseDeclaredContracts {
    param(
        $CaseEntry
    )

    if ($null -eq $CaseEntry -or $null -eq $CaseEntry.PSObject.Properties['declared_contracts']) {
        return @()
    }

    $contracts = @()
    foreach ($entry in @($CaseEntry.declared_contracts)) {
        if ($null -eq $entry) {
            continue
        }

        $contractName = [string]$entry.contract
        if ([string]::IsNullOrWhiteSpace($contractName)) {
            continue
        }

        $requires = @()
        if ($null -ne $entry.PSObject.Properties['requires']) {
            $requires = @(
                @($entry.requires) |
                    Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                    ForEach-Object { [string]$_ } |
                    Sort-Object -Unique
            )
        }

        $contracts += [ordered]@{
            contract = $contractName
            requires = @($requires)
        }
    }

    return @($contracts)
}

function Get-SubjectFacts {
    param(
        [string]$ProfileValue,
        [string]$BoardValue,
        [string[]]$ActiveFacets
    )

    $facts = @()
    if (-not [string]::IsNullOrWhiteSpace($ProfileValue)) {
        $facts += "profile.$ProfileValue"
    }
    if (-not [string]::IsNullOrWhiteSpace($BoardValue)) {
        $facts += "board.$BoardValue"
    }
    foreach ($facetName in @($ActiveFacets)) {
        if (-not [string]::IsNullOrWhiteSpace([string]$facetName)) {
            $facts += "facet.$([string]$facetName)"
        }
    }

    return @($facts | Sort-Object -Unique)
}

function Format-DeclaredContractEntry {
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

    $requiresSource = if ($ContractEntry -is [System.Collections.IDictionary]) {
        $ContractEntry['requires']
    } else {
        $ContractEntry.requires
    }
    $requires = @($requiresSource)

    return "$contractName requires [$((@($requires) -join ', '))]"
}

function Get-ResourceContractSummary {
    param(
        $DeclaredContracts,
        [string[]]$AvailableFacts
    )

    $availableSet = @{}
    foreach ($fact in @($AvailableFacts | Sort-Object -Unique)) {
        if (-not [string]::IsNullOrWhiteSpace([string]$fact)) {
            $availableSet[[string]$fact] = $true
        }
    }

    $declaredEntries = @()
    $satisfiedContracts = @()
    $violations = @()
    $unknownContracts = @()
    $resourceHotspots = @()
    $providedFacts = @()

    foreach ($contractEntry in @($DeclaredContracts)) {
        if ($null -eq $contractEntry) {
            continue
        }

        $declaredEntries += [ordered]@{
            contract = [string]$contractEntry.contract
            requires = @($contractEntry.requires)
        }

        $contractName = [string]$contractEntry.contract
        $requires = @(
            @($contractEntry.requires) |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                ForEach-Object { [string]$_ } |
                Sort-Object -Unique
        )

        if ($requires.Count -eq 0) {
            $unknownText = Format-DeclaredContractEntry -ContractEntry $contractEntry
            if (-not [string]::IsNullOrWhiteSpace($unknownText)) {
                $unknownContracts += $unknownText
                $resourceHotspots += $unknownText
            }
            continue
        }

        $missingFacts = @()
        foreach ($requiredFact in @($requires)) {
            if ($availableSet.ContainsKey($requiredFact)) {
                $providedFacts += $requiredFact
            } else {
                $missingFacts += $requiredFact
            }
        }

        if ($missingFacts.Count -eq 0) {
            $satisfiedText = Format-DeclaredContractEntry -ContractEntry $contractEntry
            if (-not [string]::IsNullOrWhiteSpace($satisfiedText)) {
                $satisfiedContracts += $satisfiedText
            }
            continue
        }

        $violationText = "$contractName missing [$((@($missingFacts) -join ', '))] requires [$((@($requires) -join ', '))]"
        $violations += $violationText
        $resourceHotspots += $violationText
    }

    return [ordered]@{
        declared_contracts = @($declaredEntries).Count
        declared_contract_entries = @($declaredEntries)
        provided_facts = @($providedFacts | Sort-Object -Unique)
        audited_count = @($declaredEntries).Count
        satisfied_count = @($satisfiedContracts).Count
        violated_count = @($violations).Count
        unknown_count = @($unknownContracts).Count
        satisfied_contracts = @($satisfiedContracts)
        violations = @($violations)
        unknown_contracts = @($unknownContracts)
        resource_hotspots = @($resourceHotspots | Sort-Object -Unique)
    }
}

function New-FactResolutionFactInventory {
    param(
        $SystemInputSummary,
        [string[]]$RequiredFacts,
        [string[]]$GraphProvidedFacts,
        $ResourceContractSummary
    )

    $declaredFacts = if ($null -ne $SystemInputSummary) {
        @(
            @($SystemInputSummary.declared_input.declared_facts) |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                ForEach-Object { [string]$_ } |
                Sort-Object -Unique
        )
    } else {
        @()
    }
    $subjectFacts = if ($null -ne $SystemInputSummary) {
        @(
            @($SystemInputSummary.resolved_input.subject_facts) |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                ForEach-Object { [string]$_ } |
                Sort-Object -Unique
        )
    } else {
        @()
    }
    $requiredFactsValue = @(
        @($RequiredFacts) |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [string]$_ } |
            Sort-Object -Unique
    )
    $graphProvidedFactsValue = @(
        @($GraphProvidedFacts) |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [string]$_ } |
            Sort-Object -Unique
    )
    $auditProvidedFacts = if ($null -ne $ResourceContractSummary) {
        @(
            @($ResourceContractSummary.provided_facts) |
                Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                ForEach-Object { [string]$_ } |
                Sort-Object -Unique
        )
    } else {
        @()
    }
    $allAvailableFacts = @(
        @($declaredFacts) +
        @($subjectFacts) +
        @($graphProvidedFactsValue) +
        @($auditProvidedFacts) |
            Sort-Object -Unique
    )

    return [ordered]@{
        declared_facts = @($declaredFacts)
        subject_facts = @($subjectFacts)
        required_facts = @($requiredFactsValue)
        graph_provided_facts = @($graphProvidedFactsValue)
        audit_provided_facts = @($auditProvidedFacts)
        all_available_facts = @($allAvailableFacts)
    }
}

function Get-FactResolutionFactSourceMap {
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

function New-FactResolutionContractEntrySummary {
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
    $statusText = Format-DeclaredContractEntry -ContractEntry ([ordered]@{
            contract = $contractName
            requires = @($requires)
        })
    if ($requires.Count -eq 0) {
        $state = 'unknown'
    } elseif ($missingFacts.Count -eq 0) {
        $state = 'satisfied'
    } else {
        $state = 'violated'
        $statusText = "$contractName missing [$((@($missingFacts) -join ', '))] requires [$((@($requires) -join ', '))]"
    }

    return [ordered]@{
        contract = $contractName
        state = $state
        requires = @($requires)
        present_facts = @($presentFacts)
        missing_facts = @($missingFacts)
        fact_sources = $factSources
        status_text = $statusText
    }
}

function New-FactResolutionSummary {
    param(
        $SystemInputSummary,
        [string[]]$RequiredFacts,
        [string[]]$GraphProvidedFacts,
        $ResourceContractSummary
    )

    $resourceSummaryValue = if ($null -eq $ResourceContractSummary) {
        New-EmptyResourceContractSummary
    } else {
        $ResourceContractSummary
    }
    $factInventory = New-FactResolutionFactInventory `
        -SystemInputSummary $SystemInputSummary `
        -RequiredFacts $RequiredFacts `
        -GraphProvidedFacts $GraphProvidedFacts `
        -ResourceContractSummary $resourceSummaryValue
    $factSourceMap = Get-FactResolutionFactSourceMap -FactInventory $factInventory
    $contracts = @()
    foreach ($contractEntry in @($resourceSummaryValue.declared_contract_entries)) {
        $entrySummary = New-FactResolutionContractEntrySummary -ContractEntry $contractEntry -FactSourceMap $factSourceMap
        if ($null -ne $entrySummary) {
            $contracts += $entrySummary
        }
    }

    return [ordered]@{
        declared_contracts = [int]$resourceSummaryValue.declared_contracts
        audited_count = [int]$resourceSummaryValue.audited_count
        satisfied_count = [int]$resourceSummaryValue.satisfied_count
        violated_count = [int]$resourceSummaryValue.violated_count
        unknown_count = [int]$resourceSummaryValue.unknown_count
        fact_inventory = $factInventory
        contracts = @($contracts | Sort-Object contract)
        satisfied_contracts = @($resourceSummaryValue.satisfied_contracts)
        violations = @($resourceSummaryValue.violations)
        unknown_contracts = @($resourceSummaryValue.unknown_contracts)
        resource_hotspots = @($resourceSummaryValue.resource_hotspots)
    }
}

function New-EmptyResourceContractSummary {
    return [ordered]@{
        declared_contracts = 0
        declared_contract_entries = @()
        provided_facts = @()
        audited_count = 0
        satisfied_count = 0
        violated_count = 0
        unknown_count = 0
        satisfied_contracts = @()
        violations = @()
        unknown_contracts = @()
        resource_hotspots = @()
    }
}

function New-EmptyFactResolutionSummary {
    return [ordered]@{
        declared_contracts = 0
        audited_count = 0
        satisfied_count = 0
        violated_count = 0
        unknown_count = 0
        fact_inventory = [ordered]@{
            declared_facts = @()
            subject_facts = @()
            required_facts = @()
            graph_provided_facts = @()
            audit_provided_facts = @()
            all_available_facts = @()
        }
        contracts = @()
        satisfied_contracts = @()
        violations = @()
        unknown_contracts = @()
        resource_hotspots = @()
    }
}

function New-EmptyBringupEvidenceSummary {
    return [ordered]@{
        declared_count = 0
        materialized_count = 0
        published_count = 0
        observed_count = 0
        blocked_count = 0
        failed_count = 0
        published_capabilities = @()
        blocked_reasons = @()
        failed_reasons = @()
        evidence_entries = @()
    }
}

function New-EmptyBindingResultSummary {
    return [ordered]@{
        required_binding_count = 0
        resolved_binding_count = 0
        unresolved_binding_count = 0
        resolved_capabilities = @()
        unresolved_capabilities = @()
        binding_entries = @()
    }
}

function New-EmptyBringupOrderSummary {
    return [ordered]@{
        ordered_node_count = 0
        blocked_node_count = 0
        phase_counts = [ordered]@{}
        entries = @()
    }
}

function Join-Names {
    param(
        [string[]]$Names
    )

    return (@($Names) -join ', ')
}

function Compare-StringArrays {
    param(
        [string[]]$Left,
        [string[]]$Right
    )

    $leftText = Join-Names (@($Left | Sort-Object -Unique))
    $rightText = Join-Names (@($Right | Sort-Object -Unique))
    return $leftText -eq $rightText
}

function Copy-OrderedCountMap {
    param(
        $CountMapLike
    )

    $result = [ordered]@{}
    if ($null -eq $CountMapLike) {
        return $result
    }

    if ($CountMapLike -is [System.Collections.IDictionary]) {
        foreach ($key in @($CountMapLike.Keys | Sort-Object)) {
            $result[[string]$key] = [int]$CountMapLike[$key]
        }

        return $result
    }

    foreach ($property in @($CountMapLike.PSObject.Properties | Sort-Object Name)) {
        $result[[string]$property.Name] = [int]$property.Value
    }

    return $result
}

function Format-CountMapText {
    param(
        $CountMapLike
    )

    $orderedMap = Copy-OrderedCountMap -CountMapLike $CountMapLike
    $parts = @()
    foreach ($entry in @($orderedMap.GetEnumerator())) {
        $parts += "$([string]$entry.Key):$([int]$entry.Value)"
    }

    return (@($parts) -join ', ')
}

function Format-ComparisonScalarText {
    param(
        $Value
    )

    if ($null -eq $Value -or [string]::IsNullOrWhiteSpace([string]$Value)) {
        return '<null>'
    }

    return [string]$Value
}

function New-ResolvedCaseSubject {
    param(
        $CaseEntry,
        $ArtifactContext
    )

    $caseSubject = if ($null -ne $CaseEntry -and $null -ne $CaseEntry.PSObject.Properties['subject']) {
        Get-SubjectInfo -SubjectLike $CaseEntry.subject
    } else {
        Get-SubjectInfo -SubjectLike $null
    }
    $defaultSubject = $ArtifactContext.SubjectDefaults
    $profileResolution = Resolve-SubjectScalarInfo -ExplicitValue $Profile -DefaultValue $defaultSubject.Profile -CaseValue $caseSubject.Profile
    $boardResolution = Resolve-SubjectScalarInfo -ExplicitValue $Board -DefaultValue $defaultSubject.Board -CaseValue $caseSubject.Board
    $facetResolution = Resolve-SubjectFacetsInfo -ExplicitFacets $Facet -DefaultFacets $defaultSubject.ActiveFacets -CaseFacets $caseSubject.ActiveFacets

    return [pscustomobject]@{
        Profile = $profileResolution.value
        Board = $boardResolution.value
        ActiveFacets = @($facetResolution.values)
        ProfileResolution = $profileResolution
        BoardResolution = $boardResolution
        ActiveFacetResolution = $facetResolution
    }
}

function New-SystemInputSummary {
    param(
        $CaseEntry,
        $ResolvedCaseSubject
    )

    $declaredSubject = if ($null -ne $CaseEntry -and $null -ne $CaseEntry.PSObject.Properties['subject']) {
        Get-SubjectInfo -SubjectLike $CaseEntry.subject
    } else {
        Get-SubjectInfo -SubjectLike $null
    }
    $declaredFacts = @(Get-CaseDeclaredFacts -CaseEntry $CaseEntry)
    $declaredContracts = @(Get-CaseDeclaredContracts -CaseEntry $CaseEntry)
    $subjectFacts = @(Get-SubjectFacts -ProfileValue $ResolvedCaseSubject.Profile -BoardValue $ResolvedCaseSubject.Board -ActiveFacets $ResolvedCaseSubject.ActiveFacets)

    return [ordered]@{
        system_spec = [ordered]@{
            case_name = [string]$CaseEntry.name
            case_kind = Get-CaseKind -CaseEntry $CaseEntry
            source = Get-OptionalCaseEntryString -CaseEntry $CaseEntry -PropertyName 'source'
            build_dir = Get-OptionalCaseEntryString -CaseEntry $CaseEntry -PropertyName 'build_dir'
            build_target = Get-OptionalCaseEntryString -CaseEntry $CaseEntry -PropertyName 'build_target'
            export_target = Get-OptionalCaseEntryString -CaseEntry $CaseEntry -PropertyName 'export_target'
        }
        declared_input = [ordered]@{
            subject = [ordered]@{
                profile = $declaredSubject.Profile
                board = $declaredSubject.Board
                active_facets = @($declaredSubject.ActiveFacets)
            }
            declared_facts = @($declaredFacts)
            declared_contract_entries = @($declaredContracts)
        }
        resolved_input = [ordered]@{
            profile = [ordered]@{
                value = $ResolvedCaseSubject.Profile
                source = [string]$ResolvedCaseSubject.ProfileResolution.source
            }
            board = [ordered]@{
                value = $ResolvedCaseSubject.Board
                source = [string]$ResolvedCaseSubject.BoardResolution.source
            }
            active_facets = [ordered]@{
                values = @($ResolvedCaseSubject.ActiveFacets)
                source = [string]$ResolvedCaseSubject.ActiveFacetResolution.source
            }
            subject_facts = @($subjectFacts)
        }
    }
}

function New-EmptySystemInputComparisonSide {
    return [ordered]@{
        system_spec = [ordered]@{
            case_name = $null
            case_kind = $null
            source = $null
            build_dir = $null
            build_target = $null
            export_target = $null
        }
        declared_input = [ordered]@{
            subject = [ordered]@{
                profile = $null
                board = $null
                active_facets = @()
            }
            declared_facts = @()
            declared_contract_entries = @()
        }
        resolved_input = [ordered]@{
            profile = [ordered]@{
                value = $null
                source = 'missing'
            }
            board = [ordered]@{
                value = $null
                source = 'missing'
            }
            active_facets = [ordered]@{
                values = @()
                source = 'missing'
            }
            subject_facts = @()
        }
    }
}

function New-SystemInputComparisonSide {
    param(
        $SystemInputSummary
    )

    if ($null -eq $SystemInputSummary) {
        return New-EmptySystemInputComparisonSide
    }

    return [ordered]@{
        system_spec = [ordered]@{
            case_name = $SystemInputSummary.system_spec.case_name
            case_kind = $SystemInputSummary.system_spec.case_kind
            source = $SystemInputSummary.system_spec.source
            build_dir = $SystemInputSummary.system_spec.build_dir
            build_target = $SystemInputSummary.system_spec.build_target
            export_target = $SystemInputSummary.system_spec.export_target
        }
        declared_input = [ordered]@{
            subject = [ordered]@{
                profile = $SystemInputSummary.declared_input.subject.profile
                board = $SystemInputSummary.declared_input.subject.board
                active_facets = @($SystemInputSummary.declared_input.subject.active_facets)
            }
            declared_facts = @($SystemInputSummary.declared_input.declared_facts)
            declared_contract_entries = @($SystemInputSummary.declared_input.declared_contract_entries)
        }
        resolved_input = [ordered]@{
            profile = [ordered]@{
                value = $SystemInputSummary.resolved_input.profile.value
                source = [string]$SystemInputSummary.resolved_input.profile.source
            }
            board = [ordered]@{
                value = $SystemInputSummary.resolved_input.board.value
                source = [string]$SystemInputSummary.resolved_input.board.source
            }
            active_facets = [ordered]@{
                values = @($SystemInputSummary.resolved_input.active_facets.values)
                source = [string]$SystemInputSummary.resolved_input.active_facets.source
            }
            subject_facts = @($SystemInputSummary.resolved_input.subject_facts)
        }
    }
}

function New-SystemInputContractStateMap {
    param(
        [object[]]$DeclaredContractEntries
    )

    $stateMap = @{}
    foreach ($entry in @($DeclaredContractEntries)) {
        if ($null -eq $entry) {
            continue
        }

        $contractName = [string]$entry.contract
        if ([string]::IsNullOrWhiteSpace($contractName)) {
            continue
        }

        $requires = @()
        if ($null -ne $entry.PSObject.Properties['requires']) {
            $requires = @(
                @($entry.requires) |
                    Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
                    ForEach-Object { [string]$_ } |
                    Sort-Object -Unique
            )
        }

        $stateMap[$contractName] = [ordered]@{
            contract = $contractName
            requires = @($requires)
        }
    }

    return $stateMap
}

function New-SystemInputComparison {
    param(
        $LeftSummary,
        $RightSummary
    )

    $leftSide = New-SystemInputComparisonSide -SystemInputSummary $LeftSummary
    $rightSide = New-SystemInputComparisonSide -SystemInputSummary $RightSummary
    $leftContractStateMap = New-SystemInputContractStateMap -DeclaredContractEntries @($leftSide.declared_input.declared_contract_entries)
    $rightContractStateMap = New-SystemInputContractStateMap -DeclaredContractEntries @($rightSide.declared_input.declared_contract_entries)

    $systemSpecChanges = @()
    foreach ($fieldName in @('case_name', 'case_kind', 'source', 'build_dir', 'build_target', 'export_target')) {
        $leftValue = $leftSide.system_spec.$fieldName
        $rightValue = $rightSide.system_spec.$fieldName
        if ([string]$leftValue -ne [string]$rightValue) {
            $systemSpecChanges += "${fieldName}:$(Format-ComparisonScalarText $leftValue)->$(Format-ComparisonScalarText $rightValue)"
        }
    }

    $declaredSubjectChanges = @()
    foreach ($fieldName in @('profile', 'board')) {
        $leftValue = $leftSide.declared_input.subject.$fieldName
        $rightValue = $rightSide.declared_input.subject.$fieldName
        if ([string]$leftValue -ne [string]$rightValue) {
            $declaredSubjectChanges += "${fieldName}:$(Format-ComparisonScalarText $leftValue)->$(Format-ComparisonScalarText $rightValue)"
        }
    }
    if (-not (Compare-StringArrays -Left @($leftSide.declared_input.subject.active_facets) -Right @($rightSide.declared_input.subject.active_facets))) {
        $declaredSubjectChanges += "active_facets:[$(Join-Names @($leftSide.declared_input.subject.active_facets))]->[$(Join-Names @($rightSide.declared_input.subject.active_facets))]"
    }

    $resolvedInputChanges = @()
    foreach ($resolvedFieldName in @('profile', 'board')) {
        $leftValue = $leftSide.resolved_input.$resolvedFieldName.value
        $rightValue = $rightSide.resolved_input.$resolvedFieldName.value
        if ([string]$leftValue -ne [string]$rightValue) {
            $resolvedInputChanges += "${resolvedFieldName}.value:$(Format-ComparisonScalarText $leftValue)->$(Format-ComparisonScalarText $rightValue)"
        }

        $leftSource = [string]$leftSide.resolved_input.$resolvedFieldName.source
        $rightSource = [string]$rightSide.resolved_input.$resolvedFieldName.source
        if ($leftSource -ne $rightSource) {
            $resolvedInputChanges += "${resolvedFieldName}.source:$(Format-ComparisonScalarText $leftSource)->$(Format-ComparisonScalarText $rightSource)"
        }
    }
    if (-not (Compare-StringArrays -Left @($leftSide.resolved_input.active_facets.values) -Right @($rightSide.resolved_input.active_facets.values))) {
        $resolvedInputChanges += "active_facets.values:[$(Join-Names @($leftSide.resolved_input.active_facets.values))]->[$(Join-Names @($rightSide.resolved_input.active_facets.values))]"
    }
    $leftFacetSource = [string]$leftSide.resolved_input.active_facets.source
    $rightFacetSource = [string]$rightSide.resolved_input.active_facets.source
    if ($leftFacetSource -ne $rightFacetSource) {
        $resolvedInputChanges += "active_facets.source:$(Format-ComparisonScalarText $leftFacetSource)->$(Format-ComparisonScalarText $rightFacetSource)"
    }

    $declaredFactsAdded = @(
        @($rightSide.declared_input.declared_facts) |
            Where-Object { @($leftSide.declared_input.declared_facts) -notcontains [string]$_ } |
            Sort-Object -Unique
    )
    $declaredFactsRemoved = @(
        @($leftSide.declared_input.declared_facts) |
            Where-Object { @($rightSide.declared_input.declared_facts) -notcontains [string]$_ } |
            Sort-Object -Unique
    )

    $subjectFactsAdded = @(
        @($rightSide.resolved_input.subject_facts) |
            Where-Object { @($leftSide.resolved_input.subject_facts) -notcontains [string]$_ } |
            Sort-Object -Unique
    )
    $subjectFactsRemoved = @(
        @($leftSide.resolved_input.subject_facts) |
            Where-Object { @($rightSide.resolved_input.subject_facts) -notcontains [string]$_ } |
            Sort-Object -Unique
    )

    $contractNames = @(
        @($leftContractStateMap.Keys) +
        @($rightContractStateMap.Keys) |
            Sort-Object -Unique
    )
    $declaredContractChanges = @()
    foreach ($contractName in @($contractNames)) {
        $leftEntry = if ($leftContractStateMap.ContainsKey($contractName)) { $leftContractStateMap[$contractName] } else { $null }
        $rightEntry = if ($rightContractStateMap.ContainsKey($contractName)) { $rightContractStateMap[$contractName] } else { $null }
        $leftRequires = if ($null -eq $leftEntry) { @() } else { @($leftEntry.requires) }
        $rightRequires = if ($null -eq $rightEntry) { @() } else { @($rightEntry.requires) }

        $changeKind = 'unchanged'
        if ($null -eq $leftEntry -and $null -ne $rightEntry) {
            $changeKind = 'added'
        } elseif ($null -ne $leftEntry -and $null -eq $rightEntry) {
            $changeKind = 'removed'
        } elseif (-not (Compare-StringArrays -Left $leftRequires -Right $rightRequires)) {
            $changeKind = 'changed'
        }

        if ($changeKind -eq 'unchanged') {
            continue
        }

        $declaredContractChanges += [ordered]@{
            contract = $contractName
            change_kind = $changeKind
            left_requires = @($leftRequires)
            right_requires = @($rightRequires)
        }
    }

    $summaryChanges = @()
    $summaryChanges += @($systemSpecChanges | ForEach-Object { "system_spec.$_" })
    $summaryChanges += @($declaredSubjectChanges | ForEach-Object { "declared_subject.$_" })
    $summaryChanges += @($resolvedInputChanges | ForEach-Object { "resolved_input.$_" })
    if (@($declaredFactsAdded).Count -gt 0 -or @($declaredFactsRemoved).Count -gt 0) {
        $summaryChanges += "declared_facts:[$(Join-Names @($leftSide.declared_input.declared_facts))]->[$(Join-Names @($rightSide.declared_input.declared_facts))]"
    }
    foreach ($contractChange in @($declaredContractChanges | Sort-Object contract)) {
        if ([string]$contractChange.change_kind -eq 'changed') {
            $summaryChanges += "declared_contract_entries.$([string]$contractChange.contract):[$(Join-Names @($contractChange.left_requires))]->[$(Join-Names @($contractChange.right_requires))]"
        } else {
            $summaryChanges += "declared_contract_entries.$([string]$contractChange.contract):$([string]$contractChange.change_kind)"
        }
    }
    if (@($subjectFactsAdded).Count -gt 0 -or @($subjectFactsRemoved).Count -gt 0) {
        $summaryChanges += "subject_facts:[$(Join-Names @($leftSide.resolved_input.subject_facts))]->[$(Join-Names @($rightSide.resolved_input.subject_facts))]"
    }

    return [ordered]@{
        changed = (
            @($summaryChanges).Count -gt 0 -or
            @($declaredFactsAdded).Count -gt 0 -or
            @($declaredFactsRemoved).Count -gt 0 -or
            @($declaredContractChanges).Count -gt 0 -or
            @($subjectFactsAdded).Count -gt 0 -or
            @($subjectFactsRemoved).Count -gt 0
        )
        left = $leftSide
        right = $rightSide
        summary_changes = @($summaryChanges)
        system_spec_changes = @($systemSpecChanges)
        declared_subject_changes = @($declaredSubjectChanges)
        declared_fact_changes = [ordered]@{
            added = @($declaredFactsAdded)
            removed = @($declaredFactsRemoved)
        }
        declared_contract_changes = @($declaredContractChanges | Sort-Object contract)
        resolved_input_changes = @($resolvedInputChanges)
        subject_fact_changes = [ordered]@{
            added = @($subjectFactsAdded)
            removed = @($subjectFactsRemoved)
        }
    }
}

function Get-SystemFormationBlockers {
    param(
        $BindingResultSummary,
        $BringupOrderSummary
    )

    $bindingResultValue = if ($null -eq $BindingResultSummary) { New-EmptyBindingResultSummary } else { $BindingResultSummary }
    $bringupOrderValue = if ($null -eq $BringupOrderSummary) { New-EmptyBringupOrderSummary } else { $BringupOrderSummary }

    $blockers = @()
    foreach ($bindingEntry in @($bindingResultValue.binding_entries)) {
        if ($null -eq $bindingEntry -or [string]$bindingEntry.state -ne 'unresolved') {
            continue
        }

        $capabilityName = [string]$bindingEntry.capability
        if ([string]::IsNullOrWhiteSpace($capabilityName)) {
            continue
        }

        $reasonText = if ([string]::IsNullOrWhiteSpace([string]$bindingEntry.reason)) {
            "unresolved binding: $capabilityName"
        } else {
            [string]$bindingEntry.reason
        }

        $blockers += [ordered]@{
            kind = 'binding'
            name = $capabilityName
            state = 'unresolved'
            reason = $reasonText
            missing_requires = @()
            dependency_nodes = @($bindingEntry.consumer_nodes)
        }
    }

    foreach ($bringupEntry in @($bringupOrderValue.entries)) {
        if ($null -eq $bringupEntry -or [string]$bringupEntry.state -ne 'blocked') {
            continue
        }

        $nodeName = [string]$bringupEntry.node
        if ([string]::IsNullOrWhiteSpace($nodeName)) {
            continue
        }

        $missingRequires = @($bringupEntry.missing_requires)
        $reasonText = if (@($missingRequires).Count -gt 0) {
            "missing requires [$((@($missingRequires) -join ', '))]"
        } else {
            "blocked node: $nodeName"
        }

        $blockers += [ordered]@{
            kind = 'node'
            name = $nodeName
            state = 'blocked'
            reason = $reasonText
            missing_requires = @($missingRequires)
            dependency_nodes = @($bringupEntry.dependency_nodes)
        }
    }

    return @(
        @($blockers) |
            Sort-Object kind, name
    )
}

function New-SystemFormationSummary {
    param(
        $SystemInputSummary,
        $BindingResultSummary,
        $BringupOrderSummary
    )

    $bindingResultValue = if ($null -eq $BindingResultSummary) { New-EmptyBindingResultSummary } else { $BindingResultSummary }
    $bringupOrderValue = if ($null -eq $BringupOrderSummary) { New-EmptyBringupOrderSummary } else { $BringupOrderSummary }
    $systemInputValue = if ($null -eq $SystemInputSummary) { New-EmptySystemInputComparisonSide } else { $SystemInputSummary }

    $blockers = @(Get-SystemFormationBlockers -BindingResultSummary $bindingResultValue -BringupOrderSummary $bringupOrderValue)
    $status = if (@($blockers).Count -gt 0) { 'blocked' } else { 'formed' }

    return [ordered]@{
        status = $status
        formation_basis = [ordered]@{
            case_kind = [string]$systemInputValue.system_spec.case_kind
            declared_fact_count = [int]@($systemInputValue.declared_input.declared_facts).Count
            declared_contract_count = [int]@($systemInputValue.declared_input.declared_contract_entries).Count
            subject_fact_count = [int]@($systemInputValue.resolved_input.subject_facts).Count
        }
        binding_summary = [ordered]@{
            required_binding_count = [int]$bindingResultValue.required_binding_count
            resolved_binding_count = [int]$bindingResultValue.resolved_binding_count
            unresolved_binding_count = [int]$bindingResultValue.unresolved_binding_count
            unresolved_capabilities = @($bindingResultValue.unresolved_capabilities)
        }
        bringup_summary = [ordered]@{
            ordered_node_count = [int]$bringupOrderValue.ordered_node_count
            blocked_node_count = [int]$bringupOrderValue.blocked_node_count
            blocked_nodes = @(Get-BlockedBringupNodes -BringupOrderSummary $bringupOrderValue)
        }
        blocker_count = [int]@($blockers).Count
        blockers = @($blockers)
    }
}

function New-EmptySystemFormationComparisonSide {
    return [ordered]@{
        status = 'missing'
        formation_basis = [ordered]@{
            case_kind = $null
            declared_fact_count = 0
            declared_contract_count = 0
            subject_fact_count = 0
        }
        binding_summary = [ordered]@{
            required_binding_count = 0
            resolved_binding_count = 0
            unresolved_binding_count = 0
            unresolved_capabilities = @()
        }
        bringup_summary = [ordered]@{
            ordered_node_count = 0
            blocked_node_count = 0
            blocked_nodes = @()
        }
        blocker_count = 0
        blockers = @()
    }
}

function New-SystemFormationComparisonSide {
    param(
        $SystemFormationSummary
    )

    if ($null -eq $SystemFormationSummary) {
        return New-EmptySystemFormationComparisonSide
    }

    return [ordered]@{
        status = [string]$SystemFormationSummary.status
        formation_basis = [ordered]@{
            case_kind = $SystemFormationSummary.formation_basis.case_kind
            declared_fact_count = [int]$SystemFormationSummary.formation_basis.declared_fact_count
            declared_contract_count = [int]$SystemFormationSummary.formation_basis.declared_contract_count
            subject_fact_count = [int]$SystemFormationSummary.formation_basis.subject_fact_count
        }
        binding_summary = [ordered]@{
            required_binding_count = [int]$SystemFormationSummary.binding_summary.required_binding_count
            resolved_binding_count = [int]$SystemFormationSummary.binding_summary.resolved_binding_count
            unresolved_binding_count = [int]$SystemFormationSummary.binding_summary.unresolved_binding_count
            unresolved_capabilities = @($SystemFormationSummary.binding_summary.unresolved_capabilities)
        }
        bringup_summary = [ordered]@{
            ordered_node_count = [int]$SystemFormationSummary.bringup_summary.ordered_node_count
            blocked_node_count = [int]$SystemFormationSummary.bringup_summary.blocked_node_count
            blocked_nodes = @($SystemFormationSummary.bringup_summary.blocked_nodes)
        }
        blocker_count = [int]$SystemFormationSummary.blocker_count
        blockers = @($SystemFormationSummary.blockers)
    }
}

function New-SystemFormationBlockerStateMap {
    param(
        [object[]]$Blockers
    )

    $stateMap = @{}
    foreach ($blocker in @($Blockers)) {
        if ($null -eq $blocker) {
            continue
        }

        $kind = [string]$blocker.kind
        $name = [string]$blocker.name
        if ([string]::IsNullOrWhiteSpace($kind) -or [string]::IsNullOrWhiteSpace($name)) {
            continue
        }

        $stateMap["${kind}:$name"] = [ordered]@{
            kind = $kind
            name = $name
            state = [string]$blocker.state
            reason = if ([string]::IsNullOrWhiteSpace([string]$blocker.reason)) { $null } else { [string]$blocker.reason }
            missing_requires = @($blocker.missing_requires)
            dependency_nodes = @($blocker.dependency_nodes)
        }
    }

    return $stateMap
}

function New-SystemFormationComparison {
    param(
        $LeftSummary,
        $RightSummary
    )

    $leftSide = New-SystemFormationComparisonSide -SystemFormationSummary $LeftSummary
    $rightSide = New-SystemFormationComparisonSide -SystemFormationSummary $RightSummary
    $leftBlockerStateMap = New-SystemFormationBlockerStateMap -Blockers @($leftSide.blockers)
    $rightBlockerStateMap = New-SystemFormationBlockerStateMap -Blockers @($rightSide.blockers)

    $blockerKeys = @(
        @($leftBlockerStateMap.Keys) +
        @($rightBlockerStateMap.Keys) |
            Sort-Object -Unique
    )
    $blockerChanges = @()
    foreach ($blockerKey in @($blockerKeys)) {
        $leftEntry = if ($leftBlockerStateMap.ContainsKey($blockerKey)) { $leftBlockerStateMap[$blockerKey] } else { $null }
        $rightEntry = if ($rightBlockerStateMap.ContainsKey($blockerKey)) { $rightBlockerStateMap[$blockerKey] } else { $null }

        $kind = if ($null -ne $rightEntry) { [string]$rightEntry.kind } elseif ($null -ne $leftEntry) { [string]$leftEntry.kind } else { 'binding' }
        $name = if ($null -ne $rightEntry) { [string]$rightEntry.name } elseif ($null -ne $leftEntry) { [string]$leftEntry.name } else { '' }
        $leftState = if ($null -eq $leftEntry) { 'absent' } else { [string]$leftEntry.state }
        $rightState = if ($null -eq $rightEntry) { 'absent' } else { [string]$rightEntry.state }
        $leftReason = if ($null -eq $leftEntry) { $null } else { $leftEntry.reason }
        $rightReason = if ($null -eq $rightEntry) { $null } else { $rightEntry.reason }
        $leftMissingRequires = if ($null -eq $leftEntry) { @() } else { @($leftEntry.missing_requires) }
        $rightMissingRequires = if ($null -eq $rightEntry) { @() } else { @($rightEntry.missing_requires) }
        $leftDependencyNodes = if ($null -eq $leftEntry) { @() } else { @($leftEntry.dependency_nodes) }
        $rightDependencyNodes = if ($null -eq $rightEntry) { @() } else { @($rightEntry.dependency_nodes) }

        $changeKind = 'unchanged'
        if ($leftState -eq 'absent' -and $rightState -ne 'absent') {
            $changeKind = 'added'
        } elseif ($leftState -ne 'absent' -and $rightState -eq 'absent') {
            $changeKind = 'removed'
        } elseif ($leftState -ne $rightState -or
            [string]$leftReason -ne [string]$rightReason -or
            -not (Compare-StringArrays -Left $leftMissingRequires -Right $rightMissingRequires) -or
            -not (Compare-StringArrays -Left $leftDependencyNodes -Right $rightDependencyNodes)) {
            $changeKind = 'changed'
        }

        if ($changeKind -eq 'unchanged') {
            continue
        }

        $blockerChanges += [ordered]@{
            kind = $kind
            name = $name
            change_kind = $changeKind
            left_state = $leftState
            right_state = $rightState
            left_reason = $leftReason
            right_reason = $rightReason
            left_missing_requires = @($leftMissingRequires)
            right_missing_requires = @($rightMissingRequires)
            left_dependency_nodes = @($leftDependencyNodes)
            right_dependency_nodes = @($rightDependencyNodes)
        }
    }

    $unresolvedCapabilitiesAdded = @(
        @($rightSide.binding_summary.unresolved_capabilities) |
            Where-Object { @($leftSide.binding_summary.unresolved_capabilities) -notcontains [string]$_ } |
            Sort-Object -Unique
    )
    $unresolvedCapabilitiesRemoved = @(
        @($leftSide.binding_summary.unresolved_capabilities) |
            Where-Object { @($rightSide.binding_summary.unresolved_capabilities) -notcontains [string]$_ } |
            Sort-Object -Unique
    )
    $blockedNodesAdded = @(
        @($rightSide.bringup_summary.blocked_nodes) |
            Where-Object { @($leftSide.bringup_summary.blocked_nodes) -notcontains [string]$_ } |
            Sort-Object -Unique
    )
    $blockedNodesRemoved = @(
        @($leftSide.bringup_summary.blocked_nodes) |
            Where-Object { @($rightSide.bringup_summary.blocked_nodes) -notcontains [string]$_ } |
            Sort-Object -Unique
    )

    $summaryChanges = @()
    if ([string]$leftSide.status -ne [string]$rightSide.status) {
        $summaryChanges += "status:$([string]$leftSide.status)->$([string]$rightSide.status)"
    }

    foreach ($fieldName in @('case_kind', 'declared_fact_count', 'declared_contract_count', 'subject_fact_count')) {
        $leftValue = $leftSide.formation_basis.$fieldName
        $rightValue = $rightSide.formation_basis.$fieldName
        if ([string]$leftValue -ne [string]$rightValue) {
            $summaryChanges += "formation_basis.${fieldName}:$(Format-ComparisonScalarText $leftValue)->$(Format-ComparisonScalarText $rightValue)"
        }
    }

    foreach ($fieldName in @('required_binding_count', 'resolved_binding_count', 'unresolved_binding_count')) {
        $leftValue = [int]$leftSide.binding_summary.$fieldName
        $rightValue = [int]$rightSide.binding_summary.$fieldName
        if ($leftValue -ne $rightValue) {
            $summaryChanges += "binding_summary.${fieldName}:$leftValue->$rightValue"
        }
    }
    if (-not (Compare-StringArrays -Left @($leftSide.binding_summary.unresolved_capabilities) -Right @($rightSide.binding_summary.unresolved_capabilities))) {
        $summaryChanges += "binding_summary.unresolved_capabilities:[$(Join-Names @($leftSide.binding_summary.unresolved_capabilities))]->[$(Join-Names @($rightSide.binding_summary.unresolved_capabilities))]"
    }

    foreach ($fieldName in @('ordered_node_count', 'blocked_node_count')) {
        $leftValue = [int]$leftSide.bringup_summary.$fieldName
        $rightValue = [int]$rightSide.bringup_summary.$fieldName
        if ($leftValue -ne $rightValue) {
            $summaryChanges += "bringup_summary.${fieldName}:$leftValue->$rightValue"
        }
    }
    if (-not (Compare-StringArrays -Left @($leftSide.bringup_summary.blocked_nodes) -Right @($rightSide.bringup_summary.blocked_nodes))) {
        $summaryChanges += "bringup_summary.blocked_nodes:[$(Join-Names @($leftSide.bringup_summary.blocked_nodes))]->[$(Join-Names @($rightSide.bringup_summary.blocked_nodes))]"
    }

    if ([int]$leftSide.blocker_count -ne [int]$rightSide.blocker_count) {
        $summaryChanges += "blocker_count:$([int]$leftSide.blocker_count)->$([int]$rightSide.blocker_count)"
    }

    return [ordered]@{
        changed = (
            @($summaryChanges).Count -gt 0 -or
            @($blockerChanges).Count -gt 0 -or
            @($unresolvedCapabilitiesAdded).Count -gt 0 -or
            @($unresolvedCapabilitiesRemoved).Count -gt 0 -or
            @($blockedNodesAdded).Count -gt 0 -or
            @($blockedNodesRemoved).Count -gt 0
        )
        left = $leftSide
        right = $rightSide
        summary_changes = @($summaryChanges)
        blocker_changes = @($blockerChanges | Sort-Object kind, name)
        unresolved_capability_changes = [ordered]@{
            added = @($unresolvedCapabilitiesAdded)
            removed = @($unresolvedCapabilitiesRemoved)
        }
        blocked_node_changes = [ordered]@{
            added = @($blockedNodesAdded)
            removed = @($blockedNodesRemoved)
        }
    }
}

function Get-BringupEvidenceSummary {
    param(
        $Graph,
        $RuntimeObserveInfo
    )

    $runtimeCapabilities = @(Get-RuntimeCapabilityNames -RuntimeObserveInfo $RuntimeObserveInfo)
    $publishedCapabilities = @(Get-RuntimePublishedCapabilities -RuntimeObserveInfo $RuntimeObserveInfo)
    $graphProvidedFacts = if ($null -ne $Graph) {
        @(Get-ProvidedFacts -Graph $Graph)
    } else {
        @()
    }
    $requiredFacts = if ($null -ne $Graph) {
        @(Get-RequiredFacts -Graph $Graph)
    } else {
        @()
    }
    $allCapabilities = @(
        @($graphProvidedFacts) +
        @($requiredFacts) +
        @($runtimeCapabilities) |
            Sort-Object -Unique
    )
    $unresolvedBindings = if ($null -ne $Graph) {
        @(Get-UnresolvedBindings -RequiredFacts $requiredFacts -ProvidedFacts $graphProvidedFacts)
    } else {
        @()
    }
    $blockedReasons = @($unresolvedBindings | ForEach-Object { "unresolved binding: $_" })
    $failedReasons = @()
    $runtimeExportStates = Get-RuntimeExportStateMap -RuntimeObserveInfo $RuntimeObserveInfo
    $observedCapabilities = @(Get-UnifiedObservedCapabilities -Graph $Graph -RuntimeCapabilities $runtimeCapabilities)
    $bringupEvidenceEntries = @(New-BringupEvidenceEntries `
        -Graph $Graph `
        -CapabilityNames $allCapabilities `
        -ObservedCapabilities $observedCapabilities `
        -PublishedCapabilities $publishedCapabilities `
        -BlockedReasons $blockedReasons `
        -FailedReasons $failedReasons `
        -RuntimeExportStates $runtimeExportStates)

    return [ordered]@{
        declared_count = @($bringupEvidenceEntries | Where-Object { [bool]$_.declared }).Count
        materialized_count = @($bringupEvidenceEntries | Where-Object { [bool]$_.materialized }).Count
        published_count = @($bringupEvidenceEntries | Where-Object { [bool]$_.published }).Count
        observed_count = @($bringupEvidenceEntries | Where-Object { [bool]$_.observed }).Count
        blocked_count = @($bringupEvidenceEntries | Where-Object { [bool]$_.blocked }).Count
        failed_count = @($bringupEvidenceEntries | Where-Object { [bool]$_.failed }).Count
        published_capabilities = @($publishedCapabilities)
        blocked_reasons = @($blockedReasons)
        failed_reasons = @($failedReasons)
        evidence_entries = @($bringupEvidenceEntries)
    }
}

function New-CaseBringupEvidenceSummary {
    param(
        $Bundle,
        $CaseEntry
    )

    if ($null -eq $CaseEntry) {
        return New-EmptyBringupEvidenceSummary
    }

    $caseGraph = Load-CaseGraph -Bundle $Bundle -CaseEntry $CaseEntry
    $runtimeObserveInfo = Load-CaseRuntimeObserve -Bundle $Bundle -CaseEntry $CaseEntry
    $graph = if ($null -ne $caseGraph) { $caseGraph.Data } else { $null }

    return Get-BringupEvidenceSummary -Graph $graph -RuntimeObserveInfo $runtimeObserveInfo
}

function New-CaseBindingResultSummary {
    param(
        $Bundle,
        $CaseEntry
    )

    if ($null -eq $CaseEntry) {
        return New-EmptyBindingResultSummary
    }

    $caseGraph = Load-CaseGraph -Bundle $Bundle -CaseEntry $CaseEntry
    $graph = if ($null -ne $caseGraph) { $caseGraph.Data } else { $null }

    return Get-BindingResultSummary -Graph $graph
}

function New-CaseBringupOrderSummary {
    param(
        $Bundle,
        $CaseEntry
    )

    if ($null -eq $CaseEntry) {
        return New-EmptyBringupOrderSummary
    }

    $caseGraph = Load-CaseGraph -Bundle $Bundle -CaseEntry $CaseEntry
    $graph = if ($null -ne $caseGraph) { $caseGraph.Data } else { $null }

    return Get-BringupOrderSummary -Graph $graph
}

function New-CaseResourceContractSummary {
    param(
        $Bundle,
        $CaseEntry,
        $ArtifactContext
    )

    if ($null -eq $CaseEntry) {
        return New-EmptyResourceContractSummary
    }

    $resolvedSubject = New-ResolvedCaseSubject -CaseEntry $CaseEntry -ArtifactContext $ArtifactContext
    $caseGraph = Load-CaseGraph -Bundle $Bundle -CaseEntry $CaseEntry
    $runtimeObserveInfo = Load-CaseRuntimeObserve -Bundle $Bundle -CaseEntry $CaseEntry
    $graph = if ($null -ne $caseGraph) { $caseGraph.Data } else { $null }
    $graphProvidedFacts = if ($null -ne $graph) {
        @(Get-ProvidedFacts -Graph $graph)
    } else {
        @()
    }
    $runtimeCapabilities = @(Get-RuntimeCapabilityNames -RuntimeObserveInfo $runtimeObserveInfo)
    $availableFacts = @(
        @($graphProvidedFacts) +
        @($runtimeCapabilities) +
        @(Get-CaseDeclaredFacts -CaseEntry $CaseEntry) +
        @(Get-SubjectFacts -ProfileValue $resolvedSubject.Profile -BoardValue $resolvedSubject.Board -ActiveFacets $resolvedSubject.ActiveFacets) |
            Sort-Object -Unique
    )

    return Get-ResourceContractSummary -DeclaredContracts @(Get-CaseDeclaredContracts -CaseEntry $CaseEntry) -AvailableFacts $availableFacts
}

function New-CaseFactResolutionSummary {
    param(
        $Bundle,
        $CaseEntry,
        $ArtifactContext
    )

    if ($null -eq $CaseEntry) {
        return New-EmptyFactResolutionSummary
    }

    $resolvedSubject = New-ResolvedCaseSubject -CaseEntry $CaseEntry -ArtifactContext $ArtifactContext
    $systemInputSummary = New-SystemInputSummary -CaseEntry $CaseEntry -ResolvedCaseSubject $resolvedSubject
    $caseGraph = Load-CaseGraph -Bundle $Bundle -CaseEntry $CaseEntry
    $runtimeObserveInfo = Load-CaseRuntimeObserve -Bundle $Bundle -CaseEntry $CaseEntry
    $graph = if ($null -ne $caseGraph) { $caseGraph.Data } else { $null }
    $graphProvidedFacts = if ($null -ne $graph) {
        @(Get-ProvidedFacts -Graph $graph)
    } else {
        @()
    }
    $requiredFacts = if ($null -ne $graph) {
        @(Get-RequiredFacts -Graph $graph)
    } else {
        @()
    }
    $runtimeCapabilities = @(Get-RuntimeCapabilityNames -RuntimeObserveInfo $runtimeObserveInfo)
    $availableFacts = @(
        @($graphProvidedFacts) +
        @($runtimeCapabilities) +
        @(Get-CaseDeclaredFacts -CaseEntry $CaseEntry) +
        @(Get-SubjectFacts -ProfileValue $resolvedSubject.Profile -BoardValue $resolvedSubject.Board -ActiveFacets $resolvedSubject.ActiveFacets) |
            Sort-Object -Unique
    )
    $resourceContractSummary = Get-ResourceContractSummary -DeclaredContracts @(Get-CaseDeclaredContracts -CaseEntry $CaseEntry) -AvailableFacts $availableFacts

    return New-FactResolutionSummary `
        -SystemInputSummary $systemInputSummary `
        -RequiredFacts $requiredFacts `
        -GraphProvidedFacts $graphProvidedFacts `
        -ResourceContractSummary $resourceContractSummary
}

function New-BringupEvidenceStateMap {
    param(
        $BringupEvidenceSummary
    )

    $stateMap = @{}
    if ($null -eq $BringupEvidenceSummary) {
        return $stateMap
    }

    foreach ($entry in @($BringupEvidenceSummary.evidence_entries)) {
        if ($null -eq $entry) {
            continue
        }

        $capabilityName = [string]$entry.capability
        if ([string]::IsNullOrWhiteSpace($capabilityName)) {
            continue
        }

        $stateMap[$capabilityName] = [ordered]@{
            capability = $capabilityName
            declared = [bool]$entry.declared
            materialized = [bool]$entry.materialized
            published = [bool]$entry.published
            observed = [bool]$entry.observed
            blocked = [bool]$entry.blocked
            failed = [bool]$entry.failed
            publish_state = if ([string]::IsNullOrWhiteSpace([string]$entry.publish_state)) { $null } else { [string]$entry.publish_state }
            export_state = if ([string]::IsNullOrWhiteSpace([string]$entry.export_state)) { $null } else { [string]$entry.export_state }
            provider_nodes = @($entry.provider_nodes)
            consumer_nodes = @($entry.consumer_nodes)
            blocked_reasons = @($entry.blocked_reasons)
            failed_reasons = @($entry.failed_reasons)
        }
    }

    return $stateMap
}

function New-BindingResultStateMap {
    param(
        $BindingResultSummary
    )

    $stateMap = @{}
    if ($null -eq $BindingResultSummary) {
        return $stateMap
    }

    foreach ($entry in @($BindingResultSummary.binding_entries)) {
        if ($null -eq $entry) {
            continue
        }

        $capabilityName = [string]$entry.capability
        if ([string]::IsNullOrWhiteSpace($capabilityName)) {
            continue
        }

        $reasonText = if ([string]::IsNullOrWhiteSpace([string]$entry.reason)) { $null } else { [string]$entry.reason }
        $stateMap[$capabilityName] = [ordered]@{
            capability = $capabilityName
            state = [string]$entry.state
            provider_nodes = @($entry.provider_nodes)
            consumer_nodes = @($entry.consumer_nodes)
            reason = $reasonText
        }
    }

    return $stateMap
}

function New-BindingResultComparisonSide {
    param(
        $BindingResultSummary
    )

    if ($null -eq $BindingResultSummary) {
        $BindingResultSummary = New-EmptyBindingResultSummary
    }

    return [ordered]@{
        required_binding_count = [int]$BindingResultSummary.required_binding_count
        resolved_binding_count = [int]$BindingResultSummary.resolved_binding_count
        unresolved_binding_count = [int]$BindingResultSummary.unresolved_binding_count
        resolved_capabilities = @($BindingResultSummary.resolved_capabilities)
        unresolved_capabilities = @($BindingResultSummary.unresolved_capabilities)
    }
}

function New-BindingResultComparison {
    param(
        $LeftSummary,
        $RightSummary
    )

    $leftSummaryValue = if ($null -eq $LeftSummary) { New-EmptyBindingResultSummary } else { $LeftSummary }
    $rightSummaryValue = if ($null -eq $RightSummary) { New-EmptyBindingResultSummary } else { $RightSummary }
    $leftSide = New-BindingResultComparisonSide -BindingResultSummary $leftSummaryValue
    $rightSide = New-BindingResultComparisonSide -BindingResultSummary $rightSummaryValue
    $leftStateMap = New-BindingResultStateMap -BindingResultSummary $leftSummaryValue
    $rightStateMap = New-BindingResultStateMap -BindingResultSummary $rightSummaryValue

    $capabilityNames = @(
        @($leftStateMap.Keys) +
        @($rightStateMap.Keys) |
            Sort-Object -Unique
    )

    $bindingChanges = @()
    foreach ($capabilityName in @($capabilityNames)) {
        $leftEntry = if ($leftStateMap.ContainsKey($capabilityName)) { $leftStateMap[$capabilityName] } else { $null }
        $rightEntry = if ($rightStateMap.ContainsKey($capabilityName)) { $rightStateMap[$capabilityName] } else { $null }

        $leftState = if ($null -eq $leftEntry) { 'absent' } else { [string]$leftEntry.state }
        $rightState = if ($null -eq $rightEntry) { 'absent' } else { [string]$rightEntry.state }
        $leftProviderNodes = if ($null -eq $leftEntry) { @() } else { @($leftEntry.provider_nodes) }
        $rightProviderNodes = if ($null -eq $rightEntry) { @() } else { @($rightEntry.provider_nodes) }
        $leftConsumerNodes = if ($null -eq $leftEntry) { @() } else { @($leftEntry.consumer_nodes) }
        $rightConsumerNodes = if ($null -eq $rightEntry) { @() } else { @($rightEntry.consumer_nodes) }
        $leftReason = if ($null -eq $leftEntry) { $null } else { $leftEntry.reason }
        $rightReason = if ($null -eq $rightEntry) { $null } else { $rightEntry.reason }

        $changeKind = 'unchanged'
        if ($leftState -eq 'absent' -and $rightState -ne 'absent') {
            $changeKind = 'added'
        } elseif ($leftState -ne 'absent' -and $rightState -eq 'absent') {
            $changeKind = 'removed'
        } elseif ($leftState -ne $rightState -or
            -not (Compare-StringArrays -Left $leftProviderNodes -Right $rightProviderNodes) -or
            -not (Compare-StringArrays -Left $leftConsumerNodes -Right $rightConsumerNodes) -or
            [string]$leftReason -ne [string]$rightReason) {
            $changeKind = 'changed'
        }

        if ($changeKind -eq 'unchanged') {
            continue
        }

        $bindingChanges += [ordered]@{
            capability = $capabilityName
            change_kind = $changeKind
            left_state = $leftState
            right_state = $rightState
            left_provider_nodes = @($leftProviderNodes)
            right_provider_nodes = @($rightProviderNodes)
            left_consumer_nodes = @($leftConsumerNodes)
            right_consumer_nodes = @($rightConsumerNodes)
            left_reason = $leftReason
            right_reason = $rightReason
        }
    }

    $resolvedCapabilitiesAdded = @(
        @($rightSide.resolved_capabilities) |
            Where-Object { @($leftSide.resolved_capabilities) -notcontains [string]$_ } |
            Sort-Object -Unique
    )
    $resolvedCapabilitiesRemoved = @(
        @($leftSide.resolved_capabilities) |
            Where-Object { @($rightSide.resolved_capabilities) -notcontains [string]$_ } |
            Sort-Object -Unique
    )
    $unresolvedCapabilitiesAdded = @(
        @($rightSide.unresolved_capabilities) |
            Where-Object { @($leftSide.unresolved_capabilities) -notcontains [string]$_ } |
            Sort-Object -Unique
    )
    $unresolvedCapabilitiesRemoved = @(
        @($leftSide.unresolved_capabilities) |
            Where-Object { @($rightSide.unresolved_capabilities) -notcontains [string]$_ } |
            Sort-Object -Unique
    )

    $summaryChanges = @()
    foreach ($countField in @('required_binding_count', 'resolved_binding_count', 'unresolved_binding_count')) {
        if ([int]$leftSide.$countField -ne [int]$rightSide.$countField) {
            $summaryChanges += "${countField}:$([int]$leftSide.$countField)->$([int]$rightSide.$countField)"
        }
    }
    if (-not (Compare-StringArrays -Left @($leftSide.resolved_capabilities) -Right @($rightSide.resolved_capabilities))) {
        $summaryChanges += "resolved_capabilities:[$(Join-Names @($leftSide.resolved_capabilities))]->[$(Join-Names @($rightSide.resolved_capabilities))]"
    }
    if (-not (Compare-StringArrays -Left @($leftSide.unresolved_capabilities) -Right @($rightSide.unresolved_capabilities))) {
        $summaryChanges += "unresolved_capabilities:[$(Join-Names @($leftSide.unresolved_capabilities))]->[$(Join-Names @($rightSide.unresolved_capabilities))]"
    }

    return [ordered]@{
        changed = (
            @($summaryChanges).Count -gt 0 -or
            @($bindingChanges).Count -gt 0 -or
            @($resolvedCapabilitiesAdded).Count -gt 0 -or
            @($resolvedCapabilitiesRemoved).Count -gt 0 -or
            @($unresolvedCapabilitiesAdded).Count -gt 0 -or
            @($unresolvedCapabilitiesRemoved).Count -gt 0
        )
        left = $leftSide
        right = $rightSide
        summary_changes = @($summaryChanges)
        binding_changes = @($bindingChanges | Sort-Object capability)
        resolved_capability_changes = [ordered]@{
            added = @($resolvedCapabilitiesAdded)
            removed = @($resolvedCapabilitiesRemoved)
        }
        unresolved_capability_changes = [ordered]@{
            added = @($unresolvedCapabilitiesAdded)
            removed = @($unresolvedCapabilitiesRemoved)
        }
    }
}

function Get-BlockedBringupNodes {
    param(
        $BringupOrderSummary
    )

    if ($null -eq $BringupOrderSummary) {
        return @()
    }

    return @(
        @($BringupOrderSummary.entries) |
            Where-Object { [string]$_.state -eq 'blocked' } |
            ForEach-Object { [string]$_.node } |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            Sort-Object -Unique
    )
}

function New-BringupOrderStateMap {
    param(
        $BringupOrderSummary
    )

    $stateMap = @{}
    if ($null -eq $BringupOrderSummary) {
        return $stateMap
    }

    foreach ($entry in @($BringupOrderSummary.entries)) {
        if ($null -eq $entry) {
            continue
        }

        $nodeName = [string]$entry.node
        if ([string]::IsNullOrWhiteSpace($nodeName)) {
            continue
        }

        $stateMap[$nodeName] = [ordered]@{
            node = $nodeName
            order = [int]$entry.order
            kind = [string]$entry.kind
            phase = if ([string]::IsNullOrWhiteSpace([string]$entry.phase)) { $null } else { [string]$entry.phase }
            runlevel_text = if ([string]::IsNullOrWhiteSpace([string]$entry.runlevel_text)) { $null } else { [string]$entry.runlevel_text }
            provides = @($entry.provides)
            requires = @($entry.requires)
            dependency_nodes = @($entry.dependency_nodes)
            resolved_requires = @($entry.resolved_requires)
            missing_requires = @($entry.missing_requires)
            state = [string]$entry.state
        }
    }

    return $stateMap
}

function New-BringupOrderComparisonSide {
    param(
        $BringupOrderSummary
    )

    if ($null -eq $BringupOrderSummary) {
        $BringupOrderSummary = New-EmptyBringupOrderSummary
    }

    return [ordered]@{
        ordered_node_count = [int]$BringupOrderSummary.ordered_node_count
        blocked_node_count = [int]$BringupOrderSummary.blocked_node_count
        phase_counts = Copy-OrderedCountMap -CountMapLike $BringupOrderSummary.phase_counts
        blocked_nodes = @(Get-BlockedBringupNodes -BringupOrderSummary $BringupOrderSummary)
    }
}

function New-BringupOrderComparison {
    param(
        $LeftSummary,
        $RightSummary
    )

    $leftSummaryValue = if ($null -eq $LeftSummary) { New-EmptyBringupOrderSummary } else { $LeftSummary }
    $rightSummaryValue = if ($null -eq $RightSummary) { New-EmptyBringupOrderSummary } else { $RightSummary }
    $leftSide = New-BringupOrderComparisonSide -BringupOrderSummary $leftSummaryValue
    $rightSide = New-BringupOrderComparisonSide -BringupOrderSummary $rightSummaryValue
    $leftStateMap = New-BringupOrderStateMap -BringupOrderSummary $leftSummaryValue
    $rightStateMap = New-BringupOrderStateMap -BringupOrderSummary $rightSummaryValue

    $nodeNames = @(
        @($leftStateMap.Keys) +
        @($rightStateMap.Keys) |
            Sort-Object -Unique
    )

    $entryChanges = @()
    foreach ($nodeName in @($nodeNames)) {
        $leftEntry = if ($leftStateMap.ContainsKey($nodeName)) { $leftStateMap[$nodeName] } else { $null }
        $rightEntry = if ($rightStateMap.ContainsKey($nodeName)) { $rightStateMap[$nodeName] } else { $null }

        $leftOrder = if ($null -eq $leftEntry) { $null } else { [int]$leftEntry.order }
        $rightOrder = if ($null -eq $rightEntry) { $null } else { [int]$rightEntry.order }
        $leftKind = if ($null -eq $leftEntry) { $null } else { [string]$leftEntry.kind }
        $rightKind = if ($null -eq $rightEntry) { $null } else { [string]$rightEntry.kind }
        $leftPhase = if ($null -eq $leftEntry) { $null } else { $leftEntry.phase }
        $rightPhase = if ($null -eq $rightEntry) { $null } else { $rightEntry.phase }
        $leftRunlevelText = if ($null -eq $leftEntry) { $null } else { $leftEntry.runlevel_text }
        $rightRunlevelText = if ($null -eq $rightEntry) { $null } else { $rightEntry.runlevel_text }
        $leftState = if ($null -eq $leftEntry) { 'absent' } else { [string]$leftEntry.state }
        $rightState = if ($null -eq $rightEntry) { 'absent' } else { [string]$rightEntry.state }
        $leftProvides = if ($null -eq $leftEntry) { @() } else { @($leftEntry.provides) }
        $rightProvides = if ($null -eq $rightEntry) { @() } else { @($rightEntry.provides) }
        $leftRequires = if ($null -eq $leftEntry) { @() } else { @($leftEntry.requires) }
        $rightRequires = if ($null -eq $rightEntry) { @() } else { @($rightEntry.requires) }
        $leftDependencyNodes = if ($null -eq $leftEntry) { @() } else { @($leftEntry.dependency_nodes) }
        $rightDependencyNodes = if ($null -eq $rightEntry) { @() } else { @($rightEntry.dependency_nodes) }
        $leftMissingRequires = if ($null -eq $leftEntry) { @() } else { @($leftEntry.missing_requires) }
        $rightMissingRequires = if ($null -eq $rightEntry) { @() } else { @($rightEntry.missing_requires) }

        $changeKind = 'unchanged'
        if ($leftState -eq 'absent' -and $rightState -ne 'absent') {
            $changeKind = 'added'
        } elseif ($leftState -ne 'absent' -and $rightState -eq 'absent') {
            $changeKind = 'removed'
        } elseif (
            $leftOrder -ne $rightOrder -or
            [string]$leftKind -ne [string]$rightKind -or
            [string]$leftPhase -ne [string]$rightPhase -or
            [string]$leftRunlevelText -ne [string]$rightRunlevelText -or
            $leftState -ne $rightState -or
            -not (Compare-StringArrays -Left $leftProvides -Right $rightProvides) -or
            -not (Compare-StringArrays -Left $leftRequires -Right $rightRequires) -or
            -not (Compare-StringArrays -Left $leftDependencyNodes -Right $rightDependencyNodes) -or
            -not (Compare-StringArrays -Left $leftMissingRequires -Right $rightMissingRequires)
        ) {
            $changeKind = 'changed'
        }

        if ($changeKind -eq 'unchanged') {
            continue
        }

        $entryChanges += [ordered]@{
            node = $nodeName
            change_kind = $changeKind
            left_order = $leftOrder
            right_order = $rightOrder
            left_kind = $leftKind
            right_kind = $rightKind
            left_phase = $leftPhase
            right_phase = $rightPhase
            left_runlevel_text = $leftRunlevelText
            right_runlevel_text = $rightRunlevelText
            left_state = $leftState
            right_state = $rightState
            left_provides = @($leftProvides)
            right_provides = @($rightProvides)
            left_requires = @($leftRequires)
            right_requires = @($rightRequires)
            left_dependency_nodes = @($leftDependencyNodes)
            right_dependency_nodes = @($rightDependencyNodes)
            left_missing_requires = @($leftMissingRequires)
            right_missing_requires = @($rightMissingRequires)
        }
    }

    $blockedNodesAdded = @(
        @($rightSide.blocked_nodes) |
            Where-Object { @($leftSide.blocked_nodes) -notcontains [string]$_ } |
            Sort-Object -Unique
    )
    $blockedNodesRemoved = @(
        @($leftSide.blocked_nodes) |
            Where-Object { @($rightSide.blocked_nodes) -notcontains [string]$_ } |
            Sort-Object -Unique
    )

    $summaryChanges = @()
    foreach ($countField in @('ordered_node_count', 'blocked_node_count')) {
        if ([int]$leftSide.$countField -ne [int]$rightSide.$countField) {
            $summaryChanges += "${countField}:$([int]$leftSide.$countField)->$([int]$rightSide.$countField)"
        }
    }
    if ((Format-CountMapText -CountMapLike $leftSide.phase_counts) -ne (Format-CountMapText -CountMapLike $rightSide.phase_counts)) {
        $summaryChanges += "phase_counts:[$(Format-CountMapText -CountMapLike $leftSide.phase_counts)]->[$(Format-CountMapText -CountMapLike $rightSide.phase_counts)]"
    }
    if (-not (Compare-StringArrays -Left @($leftSide.blocked_nodes) -Right @($rightSide.blocked_nodes))) {
        $summaryChanges += "blocked_nodes:[$(Join-Names @($leftSide.blocked_nodes))]->[$(Join-Names @($rightSide.blocked_nodes))]"
    }

    return [ordered]@{
        changed = (
            @($summaryChanges).Count -gt 0 -or
            @($entryChanges).Count -gt 0 -or
            @($blockedNodesAdded).Count -gt 0 -or
            @($blockedNodesRemoved).Count -gt 0
        )
        left = $leftSide
        right = $rightSide
        summary_changes = @($summaryChanges)
        entry_changes = @($entryChanges | Sort-Object node)
        blocked_node_changes = [ordered]@{
            added = @($blockedNodesAdded)
            removed = @($blockedNodesRemoved)
        }
    }
}

function New-BringupEvidenceComparisonSide {
    param(
        $BringupEvidenceSummary
    )

    if ($null -eq $BringupEvidenceSummary) {
        $BringupEvidenceSummary = New-EmptyBringupEvidenceSummary
    }

    return [ordered]@{
        declared_count = [int]$BringupEvidenceSummary.declared_count
        materialized_count = [int]$BringupEvidenceSummary.materialized_count
        published_count = [int]$BringupEvidenceSummary.published_count
        observed_count = [int]$BringupEvidenceSummary.observed_count
        blocked_count = [int]$BringupEvidenceSummary.blocked_count
        failed_count = [int]$BringupEvidenceSummary.failed_count
        published_capabilities = @($BringupEvidenceSummary.published_capabilities)
        blocked_reasons = @($BringupEvidenceSummary.blocked_reasons)
        failed_reasons = @($BringupEvidenceSummary.failed_reasons)
    }
}

function New-BringupEvidenceComparison {
    param(
        $LeftSummary,
        $RightSummary
    )

    $leftSummaryValue = if ($null -eq $LeftSummary) { New-EmptyBringupEvidenceSummary } else { $LeftSummary }
    $rightSummaryValue = if ($null -eq $RightSummary) { New-EmptyBringupEvidenceSummary } else { $RightSummary }
    $leftSide = New-BringupEvidenceComparisonSide -BringupEvidenceSummary $leftSummaryValue
    $rightSide = New-BringupEvidenceComparisonSide -BringupEvidenceSummary $rightSummaryValue
    $leftStateMap = New-BringupEvidenceStateMap -BringupEvidenceSummary $leftSummaryValue
    $rightStateMap = New-BringupEvidenceStateMap -BringupEvidenceSummary $rightSummaryValue

    $capabilityNames = @(
        @($leftStateMap.Keys) +
        @($rightStateMap.Keys) |
            Sort-Object -Unique
    )

    $capabilityChanges = @()
    foreach ($capabilityName in @($capabilityNames)) {
        $leftEntry = if ($leftStateMap.ContainsKey($capabilityName)) { $leftStateMap[$capabilityName] } else { $null }
        $rightEntry = if ($rightStateMap.ContainsKey($capabilityName)) { $rightStateMap[$capabilityName] } else { $null }

        $leftDeclared = if ($null -eq $leftEntry) { $false } else { [bool]$leftEntry.declared }
        $rightDeclared = if ($null -eq $rightEntry) { $false } else { [bool]$rightEntry.declared }
        $leftMaterialized = if ($null -eq $leftEntry) { $false } else { [bool]$leftEntry.materialized }
        $rightMaterialized = if ($null -eq $rightEntry) { $false } else { [bool]$rightEntry.materialized }
        $leftPublished = if ($null -eq $leftEntry) { $false } else { [bool]$leftEntry.published }
        $rightPublished = if ($null -eq $rightEntry) { $false } else { [bool]$rightEntry.published }
        $leftObserved = if ($null -eq $leftEntry) { $false } else { [bool]$leftEntry.observed }
        $rightObserved = if ($null -eq $rightEntry) { $false } else { [bool]$rightEntry.observed }
        $leftBlocked = if ($null -eq $leftEntry) { $false } else { [bool]$leftEntry.blocked }
        $rightBlocked = if ($null -eq $rightEntry) { $false } else { [bool]$rightEntry.blocked }
        $leftFailed = if ($null -eq $leftEntry) { $false } else { [bool]$leftEntry.failed }
        $rightFailed = if ($null -eq $rightEntry) { $false } else { [bool]$rightEntry.failed }
        $leftPublishState = if ($null -eq $leftEntry) { $null } else { $leftEntry.publish_state }
        $rightPublishState = if ($null -eq $rightEntry) { $null } else { $rightEntry.publish_state }
        $leftExportState = if ($null -eq $leftEntry) { $null } else { $leftEntry.export_state }
        $rightExportState = if ($null -eq $rightEntry) { $null } else { $rightEntry.export_state }
        $leftProviderNodes = if ($null -eq $leftEntry) { @() } else { @($leftEntry.provider_nodes) }
        $rightProviderNodes = if ($null -eq $rightEntry) { @() } else { @($rightEntry.provider_nodes) }
        $leftConsumerNodes = if ($null -eq $leftEntry) { @() } else { @($leftEntry.consumer_nodes) }
        $rightConsumerNodes = if ($null -eq $rightEntry) { @() } else { @($rightEntry.consumer_nodes) }
        $leftBlockedReasons = if ($null -eq $leftEntry) { @() } else { @($leftEntry.blocked_reasons) }
        $rightBlockedReasons = if ($null -eq $rightEntry) { @() } else { @($rightEntry.blocked_reasons) }
        $leftFailedReasons = if ($null -eq $leftEntry) { @() } else { @($leftEntry.failed_reasons) }
        $rightFailedReasons = if ($null -eq $rightEntry) { @() } else { @($rightEntry.failed_reasons) }

        $changeKind = 'unchanged'
        if ($null -eq $leftEntry -and $null -ne $rightEntry) {
            $changeKind = 'added'
        } elseif ($null -ne $leftEntry -and $null -eq $rightEntry) {
            $changeKind = 'removed'
        } elseif (
            $leftDeclared -ne $rightDeclared -or
            $leftMaterialized -ne $rightMaterialized -or
            $leftPublished -ne $rightPublished -or
            $leftObserved -ne $rightObserved -or
            $leftBlocked -ne $rightBlocked -or
            $leftFailed -ne $rightFailed -or
            [string]$leftPublishState -ne [string]$rightPublishState -or
            [string]$leftExportState -ne [string]$rightExportState -or
            -not (Compare-StringArrays -Left $leftProviderNodes -Right $rightProviderNodes) -or
            -not (Compare-StringArrays -Left $leftConsumerNodes -Right $rightConsumerNodes) -or
            -not (Compare-StringArrays -Left $leftBlockedReasons -Right $rightBlockedReasons) -or
            -not (Compare-StringArrays -Left $leftFailedReasons -Right $rightFailedReasons)
        ) {
            $changeKind = 'changed'
        }

        if ($changeKind -eq 'unchanged') {
            continue
        }

        $capabilityChanges += [ordered]@{
            capability = $capabilityName
            change_kind = $changeKind
            left_declared = $leftDeclared
            right_declared = $rightDeclared
            left_materialized = $leftMaterialized
            right_materialized = $rightMaterialized
            left_published = $leftPublished
            right_published = $rightPublished
            left_observed = $leftObserved
            right_observed = $rightObserved
            left_blocked = $leftBlocked
            right_blocked = $rightBlocked
            left_failed = $leftFailed
            right_failed = $rightFailed
            left_publish_state = $leftPublishState
            right_publish_state = $rightPublishState
            left_export_state = $leftExportState
            right_export_state = $rightExportState
            left_provider_nodes = @($leftProviderNodes)
            right_provider_nodes = @($rightProviderNodes)
            left_consumer_nodes = @($leftConsumerNodes)
            right_consumer_nodes = @($rightConsumerNodes)
            left_blocked_reasons = @($leftBlockedReasons)
            right_blocked_reasons = @($rightBlockedReasons)
            left_failed_reasons = @($leftFailedReasons)
            right_failed_reasons = @($rightFailedReasons)
        }
    }

    $publishedCapabilitiesAdded = @(
        @($rightSide.published_capabilities) |
            Where-Object { @($leftSide.published_capabilities) -notcontains [string]$_ } |
            Sort-Object -Unique
    )
    $publishedCapabilitiesRemoved = @(
        @($leftSide.published_capabilities) |
            Where-Object { @($rightSide.published_capabilities) -notcontains [string]$_ } |
            Sort-Object -Unique
    )
    $blockedReasonsAdded = @(
        @($rightSide.blocked_reasons) |
            Where-Object { @($leftSide.blocked_reasons) -notcontains [string]$_ } |
            Sort-Object -Unique
    )
    $blockedReasonsRemoved = @(
        @($leftSide.blocked_reasons) |
            Where-Object { @($rightSide.blocked_reasons) -notcontains [string]$_ } |
            Sort-Object -Unique
    )
    $failedReasonsAdded = @(
        @($rightSide.failed_reasons) |
            Where-Object { @($leftSide.failed_reasons) -notcontains [string]$_ } |
            Sort-Object -Unique
    )
    $failedReasonsRemoved = @(
        @($leftSide.failed_reasons) |
            Where-Object { @($rightSide.failed_reasons) -notcontains [string]$_ } |
            Sort-Object -Unique
    )

    $summaryChanges = @()
    foreach ($countField in @('declared_count', 'materialized_count', 'published_count', 'observed_count', 'blocked_count', 'failed_count')) {
        if ([int]$leftSide.$countField -ne [int]$rightSide.$countField) {
            $summaryChanges += "${countField}:$([int]$leftSide.$countField)->$([int]$rightSide.$countField)"
        }
    }
    if (-not (Compare-StringArrays -Left @($leftSide.published_capabilities) -Right @($rightSide.published_capabilities))) {
        $summaryChanges += "published_capabilities:[$(Join-Names @($leftSide.published_capabilities))]->[$(Join-Names @($rightSide.published_capabilities))]"
    }
    if (-not (Compare-StringArrays -Left @($leftSide.blocked_reasons) -Right @($rightSide.blocked_reasons))) {
        $summaryChanges += "blocked_reasons:[$(Join-Names @($leftSide.blocked_reasons))]->[$(Join-Names @($rightSide.blocked_reasons))]"
    }
    if (-not (Compare-StringArrays -Left @($leftSide.failed_reasons) -Right @($rightSide.failed_reasons))) {
        $summaryChanges += "failed_reasons:[$(Join-Names @($leftSide.failed_reasons))]->[$(Join-Names @($rightSide.failed_reasons))]"
    }

    return [ordered]@{
        changed = (
            @($summaryChanges).Count -gt 0 -or
            @($capabilityChanges).Count -gt 0 -or
            @($publishedCapabilitiesAdded).Count -gt 0 -or
            @($publishedCapabilitiesRemoved).Count -gt 0 -or
            @($blockedReasonsAdded).Count -gt 0 -or
            @($blockedReasonsRemoved).Count -gt 0 -or
            @($failedReasonsAdded).Count -gt 0 -or
            @($failedReasonsRemoved).Count -gt 0
        )
        left = $leftSide
        right = $rightSide
        summary_changes = @($summaryChanges)
        capability_changes = @($capabilityChanges | Sort-Object capability)
        published_capability_changes = [ordered]@{
            added = @($publishedCapabilitiesAdded)
            removed = @($publishedCapabilitiesRemoved)
        }
        blocked_reason_changes = [ordered]@{
            added = @($blockedReasonsAdded)
            removed = @($blockedReasonsRemoved)
        }
        failed_reason_changes = [ordered]@{
            added = @($failedReasonsAdded)
            removed = @($failedReasonsRemoved)
        }
    }
}

function New-ResourceContractStateMap {
    param(
        $ResourceContractSummary
    )

    $stateMap = @{}
    if ($null -eq $ResourceContractSummary) {
        return $stateMap
    }

    foreach ($entry in @($ResourceContractSummary.declared_contract_entries)) {
        if ($null -eq $entry) {
            continue
        }

        $contractName = [string]$entry.contract
        if ([string]::IsNullOrWhiteSpace($contractName)) {
            continue
        }

        $requires = @($entry.requires)
        $defaultText = Format-DeclaredContractEntry -ContractEntry $entry
        $state = 'declared'
        $statusText = $defaultText

        if (@($ResourceContractSummary.satisfied_contracts) -contains $defaultText) {
            $state = 'satisfied'
        } elseif (@($ResourceContractSummary.unknown_contracts) -contains $defaultText) {
            $state = 'unknown'
        } else {
            $violationText = @(
                @($ResourceContractSummary.violations) |
                    Where-Object { [string]$_ -like "$contractName*" } |
                    Select-Object -First 1
            ) | Select-Object -First 1
            if (-not [string]::IsNullOrWhiteSpace([string]$violationText)) {
                $state = 'violated'
                $statusText = [string]$violationText
            }
        }

        $stateMap[$contractName] = [ordered]@{
            contract = $contractName
            requires = @($requires)
            state = $state
            status_text = $statusText
        }
    }

    return $stateMap
}

function New-ResourceContractComparisonSide {
    param(
        $ResourceContractSummary
    )

    if ($null -eq $ResourceContractSummary) {
        $ResourceContractSummary = New-EmptyResourceContractSummary
    }

    return [ordered]@{
        declared_contracts = [int]$ResourceContractSummary.declared_contracts
        audited_count = [int]$ResourceContractSummary.audited_count
        satisfied_count = [int]$ResourceContractSummary.satisfied_count
        violated_count = [int]$ResourceContractSummary.violated_count
        unknown_count = [int]$ResourceContractSummary.unknown_count
        provided_facts = @($ResourceContractSummary.provided_facts)
        resource_hotspots = @($ResourceContractSummary.resource_hotspots)
    }
}

function New-ResourceContractComparison {
    param(
        $LeftSummary,
        $RightSummary
    )

    $leftSummaryValue = if ($null -eq $LeftSummary) { New-EmptyResourceContractSummary } else { $LeftSummary }
    $rightSummaryValue = if ($null -eq $RightSummary) { New-EmptyResourceContractSummary } else { $RightSummary }
    $leftSide = New-ResourceContractComparisonSide -ResourceContractSummary $leftSummaryValue
    $rightSide = New-ResourceContractComparisonSide -ResourceContractSummary $rightSummaryValue
    $leftStateMap = New-ResourceContractStateMap -ResourceContractSummary $leftSummaryValue
    $rightStateMap = New-ResourceContractStateMap -ResourceContractSummary $rightSummaryValue

    $contractNames = @(
        @($leftStateMap.Keys) +
        @($rightStateMap.Keys) |
            Sort-Object -Unique
    )

    $contractChanges = @()
    foreach ($contractName in @($contractNames)) {
        $leftEntry = if ($leftStateMap.ContainsKey($contractName)) { $leftStateMap[$contractName] } else { $null }
        $rightEntry = if ($rightStateMap.ContainsKey($contractName)) { $rightStateMap[$contractName] } else { $null }

        $leftState = if ($null -eq $leftEntry) { 'absent' } else { [string]$leftEntry.state }
        $rightState = if ($null -eq $rightEntry) { 'absent' } else { [string]$rightEntry.state }
        $leftRequires = if ($null -eq $leftEntry) { @() } else { @($leftEntry.requires) }
        $rightRequires = if ($null -eq $rightEntry) { @() } else { @($rightEntry.requires) }
        $leftStatusText = if ($null -eq $leftEntry) { $null } else { [string]$leftEntry.status_text }
        $rightStatusText = if ($null -eq $rightEntry) { $null } else { [string]$rightEntry.status_text }

        $changeKind = 'unchanged'
        if ($leftState -eq 'absent' -and $rightState -ne 'absent') {
            $changeKind = 'added'
        } elseif ($leftState -ne 'absent' -and $rightState -eq 'absent') {
            $changeKind = 'removed'
        } elseif ($leftState -ne $rightState -or
            -not (Compare-StringArrays -Left $leftRequires -Right $rightRequires) -or
            [string]$leftStatusText -ne [string]$rightStatusText) {
            $changeKind = 'changed'
        }

        if ($changeKind -eq 'unchanged') {
            continue
        }

        $contractChanges += [ordered]@{
            contract = $contractName
            change_kind = $changeKind
            left_state = $leftState
            right_state = $rightState
            left_requires = @($leftRequires)
            right_requires = @($rightRequires)
            left_status_text = $leftStatusText
            right_status_text = $rightStatusText
        }
    }

    $providedFactsAdded = @(
        @($rightSide.provided_facts) |
            Where-Object { @($leftSide.provided_facts) -notcontains [string]$_ } |
            Sort-Object -Unique
    )
    $providedFactsRemoved = @(
        @($leftSide.provided_facts) |
            Where-Object { @($rightSide.provided_facts) -notcontains [string]$_ } |
            Sort-Object -Unique
    )
    $hotspotsAdded = @(
        @($rightSide.resource_hotspots) |
            Where-Object { @($leftSide.resource_hotspots) -notcontains [string]$_ } |
            Sort-Object -Unique
    )
    $hotspotsRemoved = @(
        @($leftSide.resource_hotspots) |
            Where-Object { @($rightSide.resource_hotspots) -notcontains [string]$_ } |
            Sort-Object -Unique
    )

    $summaryChanges = @()
    foreach ($countField in @('declared_contracts', 'audited_count', 'satisfied_count', 'violated_count', 'unknown_count')) {
        if ([int]$leftSide.$countField -ne [int]$rightSide.$countField) {
            $summaryChanges += "${countField}:$([int]$leftSide.$countField)->$([int]$rightSide.$countField)"
        }
    }
    if (-not (Compare-StringArrays -Left @($leftSide.provided_facts) -Right @($rightSide.provided_facts))) {
        $summaryChanges += "provided_facts:[$(Join-Names @($leftSide.provided_facts))]->[$(Join-Names @($rightSide.provided_facts))]"
    }
    if (-not (Compare-StringArrays -Left @($leftSide.resource_hotspots) -Right @($rightSide.resource_hotspots))) {
        $summaryChanges += "resource_hotspots:[$(Join-Names @($leftSide.resource_hotspots))]->[$(Join-Names @($rightSide.resource_hotspots))]"
    }

    return [ordered]@{
        changed = (
            @($summaryChanges).Count -gt 0 -or
            @($contractChanges).Count -gt 0 -or
            @($providedFactsAdded).Count -gt 0 -or
            @($providedFactsRemoved).Count -gt 0 -or
            @($hotspotsAdded).Count -gt 0 -or
            @($hotspotsRemoved).Count -gt 0
        )
        left = $leftSide
        right = $rightSide
        summary_changes = @($summaryChanges)
        contract_changes = @($contractChanges | Sort-Object contract)
        provided_fact_changes = [ordered]@{
            added = @($providedFactsAdded)
            removed = @($providedFactsRemoved)
        }
        hotspot_changes = [ordered]@{
            added = @($hotspotsAdded)
            removed = @($hotspotsRemoved)
        }
    }
}

function New-FactResolutionComparisonSide {
    param(
        $FactResolutionSummary
    )

    $summaryValue = if ($null -eq $FactResolutionSummary) {
        New-EmptyFactResolutionSummary
    } else {
        $FactResolutionSummary
    }

    return [ordered]@{
        declared_contracts = [int]$summaryValue.declared_contracts
        audited_count = [int]$summaryValue.audited_count
        satisfied_count = [int]$summaryValue.satisfied_count
        violated_count = [int]$summaryValue.violated_count
        unknown_count = [int]$summaryValue.unknown_count
        fact_inventory = [ordered]@{
            declared_facts = @($summaryValue.fact_inventory.declared_facts)
            subject_facts = @($summaryValue.fact_inventory.subject_facts)
            required_facts = @($summaryValue.fact_inventory.required_facts)
            graph_provided_facts = @($summaryValue.fact_inventory.graph_provided_facts)
            audit_provided_facts = @($summaryValue.fact_inventory.audit_provided_facts)
            all_available_facts = @($summaryValue.fact_inventory.all_available_facts)
        }
        contracts = @($summaryValue.contracts)
        satisfied_contracts = @($summaryValue.satisfied_contracts)
        violations = @($summaryValue.violations)
        unknown_contracts = @($summaryValue.unknown_contracts)
        resource_hotspots = @($summaryValue.resource_hotspots)
    }
}

function New-FactResolutionContractStateMap {
    param(
        $FactResolutionSummary
    )

    $stateMap = @{}
    if ($null -eq $FactResolutionSummary) {
        return $stateMap
    }

    foreach ($contractEntry in @($FactResolutionSummary.contracts)) {
        if ($null -eq $contractEntry) {
            continue
        }

        $contractName = [string]$contractEntry.contract
        if ([string]::IsNullOrWhiteSpace($contractName)) {
            continue
        }

        $stateMap[$contractName] = [ordered]@{
            contract = $contractName
            requires = @($contractEntry.requires)
            state = [string]$contractEntry.state
            status_text = [string]$contractEntry.status_text
        }
    }

    return $stateMap
}

function New-FactResolutionComparison {
    param(
        $LeftSummary,
        $RightSummary
    )

    $leftSide = New-FactResolutionComparisonSide -FactResolutionSummary $LeftSummary
    $rightSide = New-FactResolutionComparisonSide -FactResolutionSummary $RightSummary
    $leftStateMap = New-FactResolutionContractStateMap -FactResolutionSummary $leftSide
    $rightStateMap = New-FactResolutionContractStateMap -FactResolutionSummary $rightSide

    $summaryChanges = @()
    foreach ($countField in @('declared_contracts', 'audited_count', 'satisfied_count', 'violated_count', 'unknown_count')) {
        if ([int]$leftSide.$countField -ne [int]$rightSide.$countField) {
            $summaryChanges += "${countField}:$([int]$leftSide.$countField)->$([int]$rightSide.$countField)"
        }
    }

    $factInventoryChanges = [ordered]@{}
    foreach ($factGroup in @('declared_facts', 'subject_facts', 'required_facts', 'graph_provided_facts', 'audit_provided_facts', 'all_available_facts')) {
        $addedFacts = @(
            @($rightSide.fact_inventory.$factGroup) |
                Where-Object { @($leftSide.fact_inventory.$factGroup) -notcontains [string]$_ } |
                Sort-Object -Unique
        )
        $removedFacts = @(
            @($leftSide.fact_inventory.$factGroup) |
                Where-Object { @($rightSide.fact_inventory.$factGroup) -notcontains [string]$_ } |
                Sort-Object -Unique
        )

        $factInventoryChanges[$factGroup] = [ordered]@{
            added = @($addedFacts)
            removed = @($removedFacts)
        }

        if (-not (Compare-StringArrays -Left @($leftSide.fact_inventory.$factGroup) -Right @($rightSide.fact_inventory.$factGroup))) {
            $summaryChanges += "fact_inventory.${factGroup}:[$(Join-Names @($leftSide.fact_inventory.$factGroup))]->[$(Join-Names @($rightSide.fact_inventory.$factGroup))]"
        }
    }

    $contractNames = @(
        @($leftStateMap.Keys) +
        @($rightStateMap.Keys) |
            Sort-Object -Unique
    )

    $contractChanges = @()
    foreach ($contractName in @($contractNames)) {
        $leftEntry = if ($leftStateMap.ContainsKey($contractName)) { $leftStateMap[$contractName] } else { $null }
        $rightEntry = if ($rightStateMap.ContainsKey($contractName)) { $rightStateMap[$contractName] } else { $null }

        $leftState = if ($null -eq $leftEntry) { 'absent' } else { [string]$leftEntry.state }
        $rightState = if ($null -eq $rightEntry) { 'absent' } else { [string]$rightEntry.state }
        $leftRequires = if ($null -eq $leftEntry) { @() } else { @($leftEntry.requires) }
        $rightRequires = if ($null -eq $rightEntry) { @() } else { @($rightEntry.requires) }
        $leftStatusText = if ($null -eq $leftEntry) { $null } else { [string]$leftEntry.status_text }
        $rightStatusText = if ($null -eq $rightEntry) { $null } else { [string]$rightEntry.status_text }

        $changeKind = 'unchanged'
        if ($leftState -eq 'absent' -and $rightState -ne 'absent') {
            $changeKind = 'added'
        } elseif ($leftState -ne 'absent' -and $rightState -eq 'absent') {
            $changeKind = 'removed'
        } elseif ($leftState -ne $rightState -or
            -not (Compare-StringArrays -Left $leftRequires -Right $rightRequires) -or
            [string]$leftStatusText -ne [string]$rightStatusText) {
            $changeKind = 'changed'
        }

        if ($changeKind -eq 'unchanged') {
            continue
        }

        $contractChanges += [ordered]@{
            contract = $contractName
            change_kind = $changeKind
            left_state = $leftState
            right_state = $rightState
            left_requires = @($leftRequires)
            right_requires = @($rightRequires)
            left_status_text = $leftStatusText
            right_status_text = $rightStatusText
        }
    }

    $hotspotsAdded = @(
        @($rightSide.resource_hotspots) |
            Where-Object { @($leftSide.resource_hotspots) -notcontains [string]$_ } |
            Sort-Object -Unique
    )
    $hotspotsRemoved = @(
        @($leftSide.resource_hotspots) |
            Where-Object { @($rightSide.resource_hotspots) -notcontains [string]$_ } |
            Sort-Object -Unique
    )
    if (-not (Compare-StringArrays -Left @($leftSide.resource_hotspots) -Right @($rightSide.resource_hotspots))) {
        $summaryChanges += "resource_hotspots:[$(Join-Names @($leftSide.resource_hotspots))]->[$(Join-Names @($rightSide.resource_hotspots))]"
    }

    return [ordered]@{
        changed = (
            @($summaryChanges).Count -gt 0 -or
            @($contractChanges).Count -gt 0 -or
            @($hotspotsAdded).Count -gt 0 -or
            @($hotspotsRemoved).Count -gt 0
        )
        left = $leftSide
        right = $rightSide
        summary_changes = @($summaryChanges)
        fact_inventory_changes = $factInventoryChanges
        contract_changes = @($contractChanges | Sort-Object contract)
        hotspot_changes = [ordered]@{
            added = @($hotspotsAdded)
            removed = @($hotspotsRemoved)
        }
    }
}

function Resolve-SubjectScalar {
    param(
        [string]$ExplicitValue,
        [string]$DefaultValue,
        [string]$CaseValue
    )

    return (Resolve-SubjectScalarInfo -ExplicitValue $ExplicitValue -DefaultValue $DefaultValue -CaseValue $CaseValue).value
}

function Resolve-SubjectScalarInfo {
    param(
        [string]$ExplicitValue,
        [string]$DefaultValue,
        [string]$CaseValue
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitValue)) {
        return [ordered]@{
            value = [string]$ExplicitValue
            source = 'explicit_argument'
        }
    }
    if (-not [string]::IsNullOrWhiteSpace($DefaultValue)) {
        return [ordered]@{
            value = [string]$DefaultValue
            source = 'subject_default'
        }
    }
    if (-not [string]::IsNullOrWhiteSpace($CaseValue)) {
        return [ordered]@{
            value = [string]$CaseValue
            source = 'case_subject'
        }
    }

    return [ordered]@{
        value = $null
        source = 'missing'
    }
}

function Resolve-SubjectFacets {
    param(
        [string[]]$ExplicitFacets,
        [string[]]$DefaultFacets,
        [string[]]$CaseFacets
    )

    return @((Resolve-SubjectFacetsInfo -ExplicitFacets $ExplicitFacets -DefaultFacets $DefaultFacets -CaseFacets $CaseFacets).values)
}

function Resolve-SubjectFacetsInfo {
    param(
        [string[]]$ExplicitFacets,
        [string[]]$DefaultFacets,
        [string[]]$CaseFacets
    )

    $normalizedExplicit = @(
        @($ExplicitFacets) |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [string]$_ } |
            Select-Object -Unique
    )
    if (@($normalizedExplicit).Count -gt 0) {
        return [ordered]@{
            values = @($normalizedExplicit)
            source = 'explicit_argument'
        }
    }

    $normalizedDefault = @(
        @($DefaultFacets) |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [string]$_ } |
            Select-Object -Unique
    )
    if (@($normalizedDefault).Count -gt 0) {
        return [ordered]@{
            values = @($normalizedDefault)
            source = 'subject_default'
        }
    }

    $normalizedCase = @(
        @($CaseFacets) |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [string]$_ } |
            Select-Object -Unique
    )
    if (@($normalizedCase).Count -gt 0) {
        return [ordered]@{
            values = @($normalizedCase)
            source = 'case_subject'
        }
    }

    return [ordered]@{
        values = @()
        source = 'missing'
    }
}

function Load-CiSummaryData {
    if ([string]::IsNullOrWhiteSpace($CiSummary)) {
        return $null
    }

    $ciSummaryPath = Resolve-FullPath $CiSummary
    if (-not (Test-Path $ciSummaryPath)) {
        throw "ci summary not found: $ciSummaryPath"
    }

    $data = Get-Content -LiteralPath $ciSummaryPath -Raw -Encoding utf8 | ConvertFrom-Json
    return [pscustomobject]@{
        Path = $ciSummaryPath
        Data = $data
    }
}

function Resolve-OptionalArtifactPath {
    param(
        [string]$PathValue
    )

    if ([string]::IsNullOrWhiteSpace($PathValue)) {
        return $null
    }

    return Resolve-FullPath $PathValue
}

function Get-ArtifactContext {
    $ciSummaryInfo = Load-CiSummaryData
    $resolvedMode = $Mode
    $resolvedCiSummary = $null
    $resolvedDiff = Resolve-OptionalArtifactPath -PathValue $DiffJson
    $resolvedReportManifest = Resolve-OptionalArtifactPath -PathValue $ReportManifest
    $diffData = $null
    $reportManifestData = $null
    $leftBundle = $null
    $subjectDefaults = Get-SubjectInfo -SubjectLike $null

    if ($null -ne $ciSummaryInfo) {
        $resolvedCiSummary = $ciSummaryInfo.Path
        if ($null -ne $ciSummaryInfo.Data.PSObject.Properties['mode']) {
            $resolvedMode = [string]$ciSummaryInfo.Data.mode
        }
        if ($null -eq $resolvedDiff -and $null -ne $ciSummaryInfo.Data.diff -and $null -ne $ciSummaryInfo.Data.diff.json) {
            $resolvedDiff = Resolve-OptionalArtifactPath -PathValue ([string]$ciSummaryInfo.Data.diff.json)
        }
        if ($null -eq $resolvedReportManifest -and $null -ne $ciSummaryInfo.Data.report -and $null -ne $ciSummaryInfo.Data.report.manifest) {
            $resolvedReportManifest = Resolve-OptionalArtifactPath -PathValue ([string]$ciSummaryInfo.Data.report.manifest)
        }
        if ($null -ne $ciSummaryInfo.Data.PSObject.Properties['subject_defaults']) {
            $subjectDefaults = Get-SubjectInfo -SubjectLike $ciSummaryInfo.Data.subject_defaults
        }
    }

    if ($null -ne $resolvedDiff -and (Test-Path $resolvedDiff)) {
        $diffData = Get-Content -LiteralPath $resolvedDiff -Raw -Encoding utf8 | ConvertFrom-Json
        if ($null -ne $diffData.PSObject.Properties['left'] -and
            $null -ne $diffData.left -and
            $null -ne $diffData.left.PSObject.Properties['index'] -and
            -not [string]::IsNullOrWhiteSpace([string]$diffData.left.index) -and
            (Test-Path ([string]$diffData.left.index))) {
            $leftBundle = Load-BundleByIndexPath -IndexPath (Resolve-FullPath ([string]$diffData.left.index))
        }
    }
    if ($null -ne $resolvedReportManifest -and (Test-Path $resolvedReportManifest)) {
        $reportManifestData = Get-Content -LiteralPath $resolvedReportManifest -Raw -Encoding utf8 | ConvertFrom-Json
    }

    return [pscustomobject]@{
        Mode = $resolvedMode
        CiSummary = $resolvedCiSummary
        Diff = $resolvedDiff
        ReportManifest = $resolvedReportManifest
        DiffData = $diffData
        LeftBundle = $leftBundle
        ReportManifestData = $reportManifestData
        SubjectDefaults = $subjectDefaults
    }
}

function Get-OutputPathForCase {
    param(
        [string]$CaseName,
        [int]$SelectedCount
    )

    if ($SelectedCount -eq 1 -and -not [string]::IsNullOrWhiteSpace($OutputPath)) {
        return Resolve-FullPath $OutputPath
    }

    if ($SelectedCount -gt 1 -and -not [string]::IsNullOrWhiteSpace($OutputPath)) {
        throw "-OutputPath can only be used with a single selected case"
    }

    $outputRootPath = Resolve-FullPath $OutputRoot
    if (-not (Test-Path $outputRootPath)) {
        New-Item -ItemType Directory -Path $outputRootPath -Force | Out-Null
    }

    return Join-Path $outputRootPath ($CaseName + '.artifact_report.json')
}

function New-ArtifactReport {
    param(
        $Bundle,
        $CaseEntry,
        $CaseGraph,
        $ArtifactContext
    )

    $runtimeObserveInfo = Load-CaseRuntimeObserve -Bundle $Bundle -CaseEntry $CaseEntry
    $runtimeObserveSummary = Get-RuntimeObserveSummary -RuntimeObserveInfo $runtimeObserveInfo
    $runtimeCapabilities = @(Get-RuntimeCapabilityNames -RuntimeObserveInfo $runtimeObserveInfo)

    $graph = if ($null -ne $CaseGraph) { $CaseGraph.Data } else { $null }
    $graphProvidedFacts = if ($null -ne $graph) {
        @(Get-ProvidedFacts -Graph $graph)
    } else {
        @()
    }
    $requiredFacts = if ($null -ne $graph) {
        @(Get-RequiredFacts -Graph $graph)
    } else {
        @()
    }
    $providedFacts = if ($null -ne $graph) {
        @($graphProvidedFacts)
    } else {
        @($runtimeCapabilities)
    }
    $unresolvedBindings = if ($null -ne $graph) {
        @(Get-UnresolvedBindings -RequiredFacts $requiredFacts -ProvidedFacts $graphProvidedFacts)
    } else {
        @()
    }
    $bringupEvidenceSummary = Get-BringupEvidenceSummary -Graph $graph -RuntimeObserveInfo $runtimeObserveInfo
    $allCapabilities = @(
        @($bringupEvidenceSummary.evidence_entries) |
            ForEach-Object { [string]$_.capability } |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            Sort-Object -Unique
    )
    $comparison = $null
    if ($ArtifactContext.Mode -eq 'compare' -and $null -ne $ArtifactContext.DiffData) {
        $caseName = [string]$CaseEntry.name
        $caseDiff = @($ArtifactContext.DiffData.cases | Where-Object { [string]$_.name -eq $caseName } | Select-Object -First 1)
        if ($caseDiff.Count -gt 0) {
            $summaryChanges = @(Get-OptionalStringArrayValue -Object $caseDiff[0] -PropertyName 'summary_changes')
            $metadataChanges = @(Get-OptionalStringArrayValue -Object $caseDiff[0] -PropertyName 'metadata_changes')
            $comparison = [ordered]@{
                status = [string]$caseDiff[0].status
                summary_changes = $summaryChanges
                metadata_changes = $metadataChanges
                node_changes = [ordered]@{
                    added = @($caseDiff[0].node_changes.added).Count
                    removed = @($caseDiff[0].node_changes.removed).Count
                    changed = @($caseDiff[0].node_changes.changed).Count
                }
                edge_changes = [ordered]@{
                    added = @($caseDiff[0].edge_changes.added).Count
                    removed = @($caseDiff[0].edge_changes.removed).Count
                }
            }
        } else {
            $comparison = [ordered]@{
                status = 'unchanged'
                summary_changes = @()
                metadata_changes = @()
                node_changes = [ordered]@{
                    added = 0
                    removed = 0
                    changed = 0
                }
                edge_changes = [ordered]@{
                    added = 0
                    removed = 0
                }
            }
        }
    }

    $reportMarkdown = $null
    $reportHtml = $null
    if ($null -ne $ArtifactContext.ReportManifestData -and $null -ne $ArtifactContext.ReportManifestData.reports) {
        if ($null -ne $ArtifactContext.ReportManifestData.reports.markdown) {
            $reportMarkdown = Resolve-OptionalArtifactPath -PathValue ([string]$ArtifactContext.ReportManifestData.reports.markdown)
        }
        if ($null -ne $ArtifactContext.ReportManifestData.reports.html) {
            $reportHtml = Resolve-OptionalArtifactPath -PathValue ([string]$ArtifactContext.ReportManifestData.reports.html)
        }
    }

    $resolvedSubject = New-ResolvedCaseSubject -CaseEntry $CaseEntry -ArtifactContext $ArtifactContext
    $resolvedProfile = $resolvedSubject.Profile
    $resolvedBoard = $resolvedSubject.Board
    $resolvedFacets = @($resolvedSubject.ActiveFacets)
    $systemInputSummary = New-SystemInputSummary -CaseEntry $CaseEntry -ResolvedCaseSubject $resolvedSubject
    $declaredContracts = @(Get-CaseDeclaredContracts -CaseEntry $CaseEntry)
    $resourceAvailableFacts = @(
        @($providedFacts) +
        @($runtimeCapabilities) +
        @(Get-CaseDeclaredFacts -CaseEntry $CaseEntry) +
        @(Get-SubjectFacts -ProfileValue $resolvedProfile -BoardValue $resolvedBoard -ActiveFacets $resolvedFacets)
    ) | Sort-Object -Unique
    $resourceContractSummary = Get-ResourceContractSummary -DeclaredContracts $declaredContracts -AvailableFacts $resourceAvailableFacts
    $factResolutionSummary = New-FactResolutionSummary `
        -SystemInputSummary $systemInputSummary `
        -RequiredFacts $requiredFacts `
        -GraphProvidedFacts $graphProvidedFacts `
        -ResourceContractSummary $resourceContractSummary
    $materializedOrder = if ($null -ne $graph) { @(Get-MaterializedOrder -Graph $graph) } else { @() }
    $bindingResultSummary = Get-BindingResultSummary -Graph $graph
    $bringupOrderSummary = Get-BringupOrderSummary -Graph $graph
    $systemFormationSummary = New-SystemFormationSummary -SystemInputSummary $systemInputSummary -BindingResultSummary $bindingResultSummary -BringupOrderSummary $bringupOrderSummary
    $connectionSummary = Get-GraphConnectionSummary -Graph $graph
    if ($null -ne $comparison) {
        $baselineCaseEntry = Get-CaseEntryByName -Bundle $ArtifactContext.LeftBundle -CaseName ([string]$CaseEntry.name)
        $baselineResolvedSubject = New-ResolvedCaseSubject -CaseEntry $baselineCaseEntry -ArtifactContext $ArtifactContext
        $baselineSystemInputSummary = New-SystemInputSummary -CaseEntry $baselineCaseEntry -ResolvedCaseSubject $baselineResolvedSubject
        $baselineBindingResultSummary = New-CaseBindingResultSummary -Bundle $ArtifactContext.LeftBundle -CaseEntry $baselineCaseEntry
        $baselineBringupOrderSummary = New-CaseBringupOrderSummary -Bundle $ArtifactContext.LeftBundle -CaseEntry $baselineCaseEntry
        $baselineBringupEvidenceSummary = New-CaseBringupEvidenceSummary -Bundle $ArtifactContext.LeftBundle -CaseEntry $baselineCaseEntry
        $baselineResourceContractSummary = New-CaseResourceContractSummary -Bundle $ArtifactContext.LeftBundle -CaseEntry $baselineCaseEntry -ArtifactContext $ArtifactContext
        $baselineFactResolutionSummary = New-CaseFactResolutionSummary -Bundle $ArtifactContext.LeftBundle -CaseEntry $baselineCaseEntry -ArtifactContext $ArtifactContext
        $baselineSystemFormationSummary = New-SystemFormationSummary -SystemInputSummary $baselineSystemInputSummary -BindingResultSummary $baselineBindingResultSummary -BringupOrderSummary $baselineBringupOrderSummary
        $comparison.system_input = New-SystemInputComparison -LeftSummary $baselineSystemInputSummary -RightSummary $systemInputSummary
        $comparison.system_formation = New-SystemFormationComparison -LeftSummary $baselineSystemFormationSummary -RightSummary $systemFormationSummary
        $comparison.binding_result = New-BindingResultComparison -LeftSummary $baselineBindingResultSummary -RightSummary $bindingResultSummary
        $comparison.bringup_order = New-BringupOrderComparison -LeftSummary $baselineBringupOrderSummary -RightSummary $bringupOrderSummary
        $comparison.bringup_evidence = New-BringupEvidenceComparison -LeftSummary $baselineBringupEvidenceSummary -RightSummary $bringupEvidenceSummary
        $comparison.resource_contract = New-ResourceContractComparison -LeftSummary $baselineResourceContractSummary -RightSummary $resourceContractSummary
        $comparison.fact_resolution = New-FactResolutionComparison -LeftSummary $baselineFactResolutionSummary -RightSummary $factResolutionSummary
    }
    $dotArtifactPath = if ($null -ne $CaseEntry.PSObject.Properties['dot'] -and
        -not [string]::IsNullOrWhiteSpace([string]$CaseEntry.dot)) {
        Resolve-CaseArtifactPath -BundleRootPath $Bundle.BundleRoot -RelativeOrAbsolutePath ([string]$CaseEntry.dot)
    } else {
        $null
    }
    $sampleJsonPath = if ($null -ne $CaseGraph) { $CaseGraph.Path } else { $null }

    $report = [ordered]@{
        schema = 'system_compiler.artifact_report/v0'
        generated_at_utc = (Get-Date).ToUniversalTime().ToString('o')
        generator = 'scripts/export_system_compiler_artifact_report.ps1'
        report_kind = 'system_compiler.artifact_report'
        mode = $ArtifactContext.Mode
        subject = [ordered]@{
            case = [string]$CaseEntry.name
            profile = $resolvedProfile
            board = $resolvedBoard
            active_facets = @($resolvedFacets)
        }
        system_input = $systemInputSummary
        structure = [ordered]@{
            capability_count = $allCapabilities.Count
            node_count = if ($null -ne $graph) { [int]$graph.node_count } else { 0 }
            edge_count = if ($null -ne $graph) { [int]$graph.edge_count } else { 0 }
            materialized_order = @($materializedOrder)
            declared_facts = @(Get-CaseDeclaredFacts -CaseEntry $CaseEntry)
            required_facts = @($requiredFacts)
            unresolved_bindings = @($unresolvedBindings)
        }
        binding_result = $bindingResultSummary
        bringup_order = $bringupOrderSummary
        system_formation = $systemFormationSummary
        connection_summary = $connectionSummary
        bringup_evidence = [ordered]@{
            declared_count = [int]$bringupEvidenceSummary.declared_count
            materialized_count = [int]$bringupEvidenceSummary.materialized_count
            published_count = [int]$bringupEvidenceSummary.published_count
            observed_count = [int]$bringupEvidenceSummary.observed_count
            blocked_count = [int]$bringupEvidenceSummary.blocked_count
            failed_count = [int]$bringupEvidenceSummary.failed_count
            published_capabilities = @($bringupEvidenceSummary.published_capabilities)
            blocked_reasons = @($bringupEvidenceSummary.blocked_reasons)
            failed_reasons = @($bringupEvidenceSummary.failed_reasons)
            evidence_entries = @($bringupEvidenceSummary.evidence_entries)
        }
        resource_contract = [ordered]@{
            declared_contracts = [int]$resourceContractSummary.declared_contracts
            declared_contract_entries = @($resourceContractSummary.declared_contract_entries)
            provided_facts = @($resourceContractSummary.provided_facts)
            audited_count = [int]$resourceContractSummary.audited_count
            satisfied_count = [int]$resourceContractSummary.satisfied_count
            violated_count = [int]$resourceContractSummary.violated_count
            unknown_count = [int]$resourceContractSummary.unknown_count
            satisfied_contracts = @($resourceContractSummary.satisfied_contracts)
            violations = @($resourceContractSummary.violations)
            unknown_contracts = @($resourceContractSummary.unknown_contracts)
            resource_hotspots = @($resourceContractSummary.resource_hotspots)
        }
        fact_resolution = $factResolutionSummary
        runtime_observe = $runtimeObserveSummary
        artifacts = [ordered]@{
            bundle = $Bundle.IndexPath
            input_manifest = $Bundle.InputManifestPath
            dot = $dotArtifactPath
            sample_json = $sampleJsonPath
            runtime_observe = if ($null -ne $runtimeObserveInfo) { $runtimeObserveInfo.Path } else { $null }
            diff = $ArtifactContext.Diff
            ci_summary = $ArtifactContext.CiSummary
            report_manifest = $ArtifactContext.ReportManifest
            report_markdown = $reportMarkdown
            report_html = $reportHtml
        }
    }

    if ($null -ne $comparison) {
        $report.comparison = $comparison
    }

    return $report
}

$bundle = Load-Bundle
$selectedCases = @(Get-SelectedCases -Bundle $bundle)
$artifactContext = Get-ArtifactContext

$written = @()
foreach ($caseEntry in $selectedCases) {
    $caseGraph = Load-CaseGraph -Bundle $bundle -CaseEntry $caseEntry
    $report = New-ArtifactReport -Bundle $bundle -CaseEntry $caseEntry -CaseGraph $caseGraph -ArtifactContext $artifactContext
    $reportPath = Get-OutputPathForCase -CaseName ([string]$caseEntry.name) -SelectedCount $selectedCases.Count
    Ensure-ParentDirectory -Path $reportPath
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $reportPath -Encoding utf8
    $written += $reportPath
    Write-Host "[ARTIFACT][$($caseEntry.name)] $reportPath"
}

Write-Host "[OK] generated $($written.Count) artifact report(s)"
