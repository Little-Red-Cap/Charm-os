param(
    [string]$LeftBundleRoot = "",
    [string]$RightBundleRoot = "",
    [string]$LeftIndex = "",
    [string]$RightIndex = "",
    [string[]]$Case = @(),
    [switch]$ShowDetails,
    [switch]$IncludeUnchanged,
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

function Resolve-IndexPath {
    param(
        [string]$BundleRoot,
        [string]$IndexPath,
        [string]$SideName
    )

    if (-not [string]::IsNullOrWhiteSpace($IndexPath)) {
        return Resolve-FullPath $IndexPath
    }

    if ([string]::IsNullOrWhiteSpace($BundleRoot)) {
        throw "$SideName bundle root or index is required"
    }

    return Join-Path (Resolve-FullPath $BundleRoot) 'index.json'
}

function Load-Bundle {
    param(
        [string]$BundleRoot,
        [string]$IndexPath,
        [string]$SideName
    )

    $resolvedIndexPath = Resolve-IndexPath -BundleRoot $BundleRoot -IndexPath $IndexPath -SideName $SideName
    if (-not (Test-Path $resolvedIndexPath)) {
        throw "$SideName bundle index not found: $resolvedIndexPath"
    }

    $indexData = Get-Content -LiteralPath $resolvedIndexPath -Raw -Encoding utf8 | ConvertFrom-Json
    if ([string]$indexData.schema -ne 'materialized_graph.export_bundle/v1') {
        throw "$SideName bundle schema not supported: $($indexData.schema)"
    }

    $inputManifest = $null
    if ($null -ne $indexData.PSObject.Properties['input_manifest'] -and $null -ne $indexData.input_manifest) {
        $manifestPath = $null
        $manifestSchema = $null
        if ($null -ne $indexData.input_manifest.PSObject.Properties['path'] -and -not [string]::IsNullOrWhiteSpace([string]$indexData.input_manifest.path)) {
            $manifestPath = Resolve-FullPath ([string]$indexData.input_manifest.path)
        }
        if ($null -ne $indexData.input_manifest.PSObject.Properties['schema'] -and -not [string]::IsNullOrWhiteSpace([string]$indexData.input_manifest.schema)) {
            $manifestSchema = [string]$indexData.input_manifest.schema
        }
        if (-not [string]::IsNullOrWhiteSpace($manifestPath) -or -not [string]::IsNullOrWhiteSpace($manifestSchema)) {
            $inputManifest = [ordered]@{
                path = $manifestPath
                schema = $manifestSchema
            }
        }
    }

    return [pscustomobject]@{
        Side = $SideName
        IndexPath = $resolvedIndexPath
        BundleRoot = Split-Path -Parent $resolvedIndexPath
        InputManifest = $inputManifest
        Cases = @($indexData.cases)
    }
}

function Resolve-ArtifactPath {
    param(
        [string]$BundleRoot,
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ''
    }

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return Resolve-FullPath $Path
    }

    return Resolve-FullPath (Join-Path $BundleRoot $Path)
}

function Get-CaseStringProperty {
    param(
        $CaseEntry,
        [string]$PropertyName
    )

    if ($null -eq $CaseEntry -or [string]::IsNullOrWhiteSpace($PropertyName)) {
        return $null
    }

    $property = $CaseEntry.PSObject.Properties[$PropertyName]
    if ($null -eq $property) {
        return $null
    }

    $value = [string]$property.Value
    if ([string]::IsNullOrWhiteSpace($value)) {
        return $null
    }

    return $value
}

function Get-CaseKind {
    param(
        $CaseEntry
    )

    if ($null -eq $CaseEntry) {
        return 'absent'
    }

    $caseKind = Get-CaseStringProperty -CaseEntry $CaseEntry -PropertyName 'case_kind'
    if (-not [string]::IsNullOrWhiteSpace($caseKind)) {
        return $caseKind
    }

    if ($null -ne (Get-CaseGraphSummary -CaseEntry $CaseEntry)) {
        return 'materialized_graph'
    }

    if (-not [string]::IsNullOrWhiteSpace((Get-CaseStringProperty -CaseEntry $CaseEntry -PropertyName 'runtime_observe'))) {
        return 'runtime_only'
    }

    return 'materialized_graph'
}

function Get-CaseGraphSummary {
    param(
        $CaseEntry
    )

    if ($null -eq $CaseEntry -or $null -eq $CaseEntry.PSObject.Properties['graph']) {
        return $null
    }

    return $CaseEntry.graph
}

function Get-CaseNodeCount {
    param(
        $CaseEntry
    )

    $graphSummary = Get-CaseGraphSummary -CaseEntry $CaseEntry
    if ($null -eq $graphSummary) {
        return 0
    }

    return [int]$graphSummary.node_count
}

function Get-CaseEdgeCount {
    param(
        $CaseEntry
    )

    $graphSummary = Get-CaseGraphSummary -CaseEntry $CaseEntry
    if ($null -eq $graphSummary) {
        return 0
    }

    return [int]$graphSummary.edge_count
}

function Get-GraphAvailabilityLabel {
    param(
        $CaseEntry
    )

    if ($null -ne (Get-CaseGraphSummary -CaseEntry $CaseEntry)) {
        return 'available'
    }

    return 'unavailable'
}

function Resolve-CaseArtifactOrNull {
    param(
        $Bundle,
        $CaseEntry,
        [string]$FieldName
    )

    $pathValue = Get-CaseStringProperty -CaseEntry $CaseEntry -PropertyName $FieldName
    if ([string]::IsNullOrWhiteSpace($pathValue)) {
        return $null
    }

    return Resolve-ArtifactPath -BundleRoot $Bundle.BundleRoot -Path $pathValue
}

function Load-CaseGraph {
    param(
        $Bundle,
        $CaseEntry
    )

    $jsonReference = Get-CaseStringProperty -CaseEntry $CaseEntry -PropertyName 'json'
    if ([string]::IsNullOrWhiteSpace($jsonReference)) {
        if ((Get-CaseKind -CaseEntry $CaseEntry) -eq 'materialized_graph') {
            throw "$($Bundle.Side) case json missing for materialized_graph case: $([string]$CaseEntry.name)"
        }

        return $null
    }

    $jsonPath = Resolve-ArtifactPath -BundleRoot $Bundle.BundleRoot -Path $jsonReference
    if (-not (Test-Path $jsonPath)) {
        throw "$($Bundle.Side) case json not found: $jsonPath"
    }

    $graph = Get-Content -LiteralPath $jsonPath -Raw -Encoding utf8 | ConvertFrom-Json
    Assert-MaterializedGraphSampleShape -Graph $graph -Context $jsonPath

    return [pscustomobject]@{
        Path = $jsonPath
        Data = $graph
    }
}

function Get-CaseMap {
    param(
        $Bundle
    )

    $map = @{}
    foreach ($entry in $Bundle.Cases) {
        $map[[string]$entry.name] = $entry
    }

    return $map
}

function Get-SelectedCaseNames {
    param(
        $LeftBundle,
        $RightBundle
    )

    $names = @()
    $leftMap = Get-CaseMap -Bundle $LeftBundle
    $rightMap = Get-CaseMap -Bundle $RightBundle

    if ($Case.Count -gt 0) {
        foreach ($caseName in $Case) {
            if ((-not $leftMap.ContainsKey($caseName)) -and (-not $rightMap.ContainsKey($caseName))) {
                throw "unknown case: $caseName"
            }

            $names += $caseName
        }

        return $names
    }

    $allNames = @($leftMap.Keys + $rightMap.Keys | Sort-Object -Unique)
    return $allNames
}

function Get-CaseStatusCount {
    param(
        $CaseDiffs,
        [string]$Status
    )

    return @($CaseDiffs | Where-Object { $_.Status -eq $Status }).Count
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

function Join-Names {
    param(
        [string[]]$Names
    )

    return (@($Names) -join ', ')
}

function Format-NullableValue {
    param(
        [string]$Value
    )

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return '<null>'
    }

    return $Value
}

function Get-CountMapEntries {
    param(
        $Map
    )

    $items = @()
    if ($null -eq $Map) {
        return $items
    }

    foreach ($property in $Map.PSObject.Properties) {
        $items += "$($property.Name)=$($property.Value)"
    }

    return $items
}

function Format-NodeConnectionText {
    param(
        $NodeDescriptor
    )

    if ($null -eq $NodeDescriptor) {
        return ''
    }

    $source = [string]$NodeDescriptor.ConnectionSource
    $sink = [string]$NodeDescriptor.ConnectionSink
    $mode = [string]$NodeDescriptor.ConnectionMode
    if ([string]::IsNullOrWhiteSpace($source) -and [string]::IsNullOrWhiteSpace($sink) -and [string]::IsNullOrWhiteSpace($mode)) {
        return ''
    }

    $text = ''
    if (-not [string]::IsNullOrWhiteSpace($source) -or -not [string]::IsNullOrWhiteSpace($sink)) {
        $text = "$source -> $sink"
    }
    if (-not [string]::IsNullOrWhiteSpace($mode)) {
        if ([string]::IsNullOrWhiteSpace($text)) {
            $text = "[$mode]"
        } else {
            $text += " [$mode]"
        }
    }

    return $text
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
        Profile = $profile
        Board = $board
        ActiveFacets = @($activeFacets)
    }
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

function Get-CaseRequiredFacts {
    param(
        $CaseEntry
    )

    if ($null -eq $CaseEntry -or $null -eq $CaseEntry.PSObject.Properties['required_facts']) {
        return @()
    }

    return @(
        @($CaseEntry.required_facts) |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [string]$_ } |
            Select-Object -Unique
    )
}

function Get-CaseAuditProvidedFacts {
    param(
        $CaseEntry
    )

    if ($null -eq $CaseEntry -or $null -eq $CaseEntry.PSObject.Properties['audit_provided_facts']) {
        return @()
    }

    return @(
        @($CaseEntry.audit_provided_facts) |
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

function Format-DeclaredContract {
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

function Get-DeclaredContractTexts {
    param(
        $CaseEntry
    )

    return @(
        @(Get-CaseDeclaredContracts -CaseEntry $CaseEntry) |
            ForEach-Object { Format-DeclaredContract -ContractEntry $_ } |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            Sort-Object -Unique
    )
}

function New-CaseSubjectJsonView {
    param(
        $CaseEntry
    )

    if ($null -eq $CaseEntry -or $null -eq $CaseEntry.PSObject.Properties['subject']) {
        return $null
    }

    $subject = Get-CaseSubjectInfo -CaseEntry $CaseEntry
    return [ordered]@{
        profile = $subject.Profile
        board = $subject.Board
        active_facets = @($subject.ActiveFacets)
    }
}

function Get-DerivedDeclaredFactsDefaults {
    param(
        [object[]]$CaseEntries
    )

    if (@($CaseEntries).Count -eq 0) {
        return $null
    }

    $firstFacts = @(Get-CaseDeclaredFacts -CaseEntry $CaseEntries[0])
    foreach ($caseEntry in @($CaseEntries | Select-Object -Skip 1)) {
        $nextFacts = @(Get-CaseDeclaredFacts -CaseEntry $caseEntry)
        if (-not (Compare-StringArrays -Left $firstFacts -Right $nextFacts)) {
            return $null
        }
    }

    return ,@($firstFacts)
}

function Get-DerivedDeclaredContractsDefaults {
    param(
        [object[]]$CaseEntries
    )

    if (@($CaseEntries).Count -eq 0) {
        return $null
    }

    $firstContracts = @(Get-CaseDeclaredContracts -CaseEntry $CaseEntries[0])
    $firstTexts = @(Get-DeclaredContractTexts -CaseEntry $CaseEntries[0])
    foreach ($caseEntry in @($CaseEntries | Select-Object -Skip 1)) {
        $nextTexts = @(Get-DeclaredContractTexts -CaseEntry $caseEntry)
        if (-not (Compare-StringArrays -Left $firstTexts -Right $nextTexts)) {
            return $null
        }
    }

    return ,@($firstContracts)
}

function Get-NodeKey {
    param(
        $Node,
        [int]$Occurrence
    )

    $name = [string]$Node.name
    if ([string]::IsNullOrWhiteSpace($name)) {
        return "#anon:$Occurrence"
    }

    return $name
}

function Get-NodeDescriptors {
    param(
        $Graph
    )

    $descriptors = @{}
    $seenNames = @{}
    foreach ($node in @($Graph.nodes)) {
        $name = [string]$node.name
        if ($seenNames.ContainsKey($name)) {
            $seenNames[$name] += 1
        } else {
            $seenNames[$name] = 1
        }

        $key = Get-NodeKey -Node $node -Occurrence $seenNames[$name]
        if ($descriptors.ContainsKey($key)) {
            $key = "$key#$($seenNames[$name])"
        }

        $connectionSource = $null
        $connectionSink = $null
        $connectionMode = $null
        if ($null -ne $node.PSObject.Properties['connection'] -and $null -ne $node.connection) {
            if ($null -ne $node.connection.PSObject.Properties['source'] -and -not [string]::IsNullOrWhiteSpace([string]$node.connection.source)) {
                $connectionSource = [string]$node.connection.source
            }
            if ($null -ne $node.connection.PSObject.Properties['sink'] -and -not [string]::IsNullOrWhiteSpace([string]$node.connection.sink)) {
                $connectionSink = [string]$node.connection.sink
            }
            if ($null -ne $node.connection.PSObject.Properties['mode'] -and -not [string]::IsNullOrWhiteSpace([string]$node.connection.mode)) {
                $connectionMode = [string]$node.connection.mode
            }
        }

        $descriptors[$key] = [pscustomobject]@{
            Key = $key
            Index = [int]$node.index
            Name = [string]$node.name
            Kind = [string]$node.kind
            Phase = [string]$node.phase
            Runlevel = [string]$node.runlevel_text
            Provides = @(Get-CapabilityNames $node.provides)
            Requires = @(Get-CapabilityNames $node.requires)
            ConnectionSource = $connectionSource
            ConnectionSink = $connectionSink
            ConnectionMode = $connectionMode
        }
    }

    return $descriptors
}

function Get-EdgeDescriptors {
    param(
        $Graph
    )

    $descriptors = @{}
    foreach ($edge in @($Graph.edges)) {
        $providerName = [string]$Graph.nodes[[int]$edge.provider_index].name
        $consumerName = [string]$Graph.nodes[[int]$edge.consumer_index].name
        $capabilityName = [string]$edge.capability.name
        if ([string]::IsNullOrWhiteSpace($capabilityName)) {
            $capabilityName = [string]$edge.capability.id
        }

        $key = "${providerName}->${consumerName}:$capabilityName"
        $descriptors[$key] = [pscustomobject]@{
            Key = $key
            Provider = $providerName
            Consumer = $consumerName
            Capability = $capabilityName
        }
    }

    return $descriptors
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

function Get-ChangedFields {
    param(
        $LeftNode,
        $RightNode
    )

    $fields = @()
    if ([string]$LeftNode.Kind -ne [string]$RightNode.Kind) {
        $fields += "kind:$($LeftNode.Kind)->$($RightNode.Kind)"
    }
    if ([string]$LeftNode.Phase -ne [string]$RightNode.Phase) {
        $fields += "phase:$($LeftNode.Phase)->$($RightNode.Phase)"
    }
    if ([string]$LeftNode.Runlevel -ne [string]$RightNode.Runlevel) {
        $fields += "runlevel:$($LeftNode.Runlevel)->$($RightNode.Runlevel)"
    }
    if (-not (Compare-StringArrays -Left $LeftNode.Provides -Right $RightNode.Provides)) {
        $fields += "provides:[$(Join-Names $LeftNode.Provides)]->[$(Join-Names $RightNode.Provides)]"
    }
    if (-not (Compare-StringArrays -Left $LeftNode.Requires -Right $RightNode.Requires)) {
        $fields += "requires:[$(Join-Names $LeftNode.Requires)]->[$(Join-Names $RightNode.Requires)]"
    }
    if ([string]$LeftNode.ConnectionSource -ne [string]$RightNode.ConnectionSource) {
        $fields += "connection.source:$(Format-NullableValue ([string]$LeftNode.ConnectionSource))->$(Format-NullableValue ([string]$RightNode.ConnectionSource))"
    }
    if ([string]$LeftNode.ConnectionSink -ne [string]$RightNode.ConnectionSink) {
        $fields += "connection.sink:$(Format-NullableValue ([string]$LeftNode.ConnectionSink))->$(Format-NullableValue ([string]$RightNode.ConnectionSink))"
    }
    if ([string]$LeftNode.ConnectionMode -ne [string]$RightNode.ConnectionMode) {
        $fields += "connection.mode:$(Format-NullableValue ([string]$LeftNode.ConnectionMode))->$(Format-NullableValue ([string]$RightNode.ConnectionMode))"
    }

    return $fields
}

function Compare-Nodes {
    param(
        $LeftGraph,
        $RightGraph
    )

    $leftNodes = Get-NodeDescriptors -Graph $LeftGraph
    $rightNodes = Get-NodeDescriptors -Graph $RightGraph
    $allKeys = @($leftNodes.Keys + $rightNodes.Keys | Sort-Object -Unique)

    $added = @()
    $removed = @()
    $changed = @()
    foreach ($key in $allKeys) {
        $hasLeft = $leftNodes.ContainsKey($key)
        $hasRight = $rightNodes.ContainsKey($key)
        if ($hasLeft -and (-not $hasRight)) {
            $removed += $leftNodes[$key]
            continue
        }
        if ((-not $hasLeft) -and $hasRight) {
            $added += $rightNodes[$key]
            continue
        }

        $diffFields = @(Get-ChangedFields -LeftNode $leftNodes[$key] -RightNode $rightNodes[$key])
        if ($diffFields.Count -gt 0) {
            $changed += [pscustomobject]@{
                Key = $key
                Name = $rightNodes[$key].Name
                Fields = $diffFields
                Left = $leftNodes[$key]
                Right = $rightNodes[$key]
            }
        }
    }

    return [pscustomobject]@{
        Added = @($added)
        Removed = @($removed)
        Changed = @($changed)
    }
}

function Compare-Edges {
    param(
        $LeftGraph,
        $RightGraph
    )

    $leftEdges = Get-EdgeDescriptors -Graph $LeftGraph
    $rightEdges = Get-EdgeDescriptors -Graph $RightGraph
    $allKeys = @($leftEdges.Keys + $rightEdges.Keys | Sort-Object -Unique)

    $added = @()
    $removed = @()
    foreach ($key in $allKeys) {
        $hasLeft = $leftEdges.ContainsKey($key)
        $hasRight = $rightEdges.ContainsKey($key)
        if ($hasLeft -and (-not $hasRight)) {
            $removed += $leftEdges[$key]
        } elseif ((-not $hasLeft) -and $hasRight) {
            $added += $rightEdges[$key]
        }
    }

    return [pscustomobject]@{
        Added = @($added)
        Removed = @($removed)
    }
}

function Compare-CaseSummary {
    param(
        $LeftCase,
        $RightCase
    )

    $changes = @()
    $leftCaseKind = Get-CaseKind -CaseEntry $LeftCase
    $rightCaseKind = Get-CaseKind -CaseEntry $RightCase
    if ($leftCaseKind -ne $rightCaseKind) {
        $changes += "case_kind:$leftCaseKind->$rightCaseKind"
    }

    $leftGraph = Get-CaseGraphSummary -CaseEntry $LeftCase
    $rightGraph = Get-CaseGraphSummary -CaseEntry $RightCase
    $leftGraphAvailability = Get-GraphAvailabilityLabel -CaseEntry $LeftCase
    $rightGraphAvailability = Get-GraphAvailabilityLabel -CaseEntry $RightCase
    if ($leftGraphAvailability -ne $rightGraphAvailability) {
        $changes += "graph:$leftGraphAvailability->$rightGraphAvailability"
    }

    if ($null -ne $leftGraph -and $null -ne $rightGraph) {
        if ([string]$leftGraph.schema -ne [string]$rightGraph.schema) {
            $changes += "schema:$([string]$leftGraph.schema)->$([string]$rightGraph.schema)"
        }
        if ([int]$leftGraph.node_count -ne [int]$rightGraph.node_count) {
            $changes += "node_count:$([int]$leftGraph.node_count)->$([int]$rightGraph.node_count)"
        }
        if ([int]$leftGraph.edge_count -ne [int]$rightGraph.edge_count) {
            $changes += "edge_count:$([int]$leftGraph.edge_count)->$([int]$rightGraph.edge_count)"
        }
        if ([string]$leftGraph.effective_max_phase -ne [string]$rightGraph.effective_max_phase) {
            $changes += "phase:$([string]$leftGraph.effective_max_phase)->$([string]$rightGraph.effective_max_phase)"
        }
        if ([string]$leftGraph.effective_runlevel_text -ne [string]$rightGraph.effective_runlevel_text) {
            $changes += "runlevel:$([string]$leftGraph.effective_runlevel_text)->$([string]$rightGraph.effective_runlevel_text)"
        }

        $leftKinds = @(Get-CountMapEntries -Map $leftGraph.node_kinds)
        $rightKinds = @(Get-CountMapEntries -Map $rightGraph.node_kinds)
        if (-not (Compare-StringArrays -Left $leftKinds -Right $rightKinds)) {
            $changes += "node_kinds:[$(Join-Names $leftKinds)]->[$(Join-Names $rightKinds)]"
        }

        $leftConnectionModes = @(Get-CountMapEntries -Map $leftGraph.connection_modes)
        $rightConnectionModes = @(Get-CountMapEntries -Map $rightGraph.connection_modes)
        if (-not (Compare-StringArrays -Left $leftConnectionModes -Right $rightConnectionModes)) {
            $changes += "connection_modes:[$(Join-Names $leftConnectionModes)]->[$(Join-Names $rightConnectionModes)]"
        }
    }

    $leftRuntimeObserve = Get-CaseStringProperty -CaseEntry $LeftCase -PropertyName 'runtime_observe'
    $rightRuntimeObserve = Get-CaseStringProperty -CaseEntry $RightCase -PropertyName 'runtime_observe'
    if ($leftRuntimeObserve -ne $rightRuntimeObserve) {
        $changes += "runtime_observe:$(Format-NullableValue $leftRuntimeObserve)->$(Format-NullableValue $rightRuntimeObserve)"
    }
    $leftFactEvidence = Get-CaseStringProperty -CaseEntry $LeftCase -PropertyName 'fact_evidence'
    $rightFactEvidence = Get-CaseStringProperty -CaseEntry $RightCase -PropertyName 'fact_evidence'
    if ($leftFactEvidence -ne $rightFactEvidence) {
        $changes += "fact_evidence:$(Format-NullableValue $leftFactEvidence)->$(Format-NullableValue $rightFactEvidence)"
    }

    $leftSubject = Get-CaseSubjectInfo -CaseEntry $LeftCase
    $rightSubject = Get-CaseSubjectInfo -CaseEntry $RightCase
    if ($leftSubject.Profile -ne $rightSubject.Profile) {
        $changes += "subject.profile:$(Format-NullableValue $leftSubject.Profile)->$(Format-NullableValue $rightSubject.Profile)"
    }
    if ($leftSubject.Board -ne $rightSubject.Board) {
        $changes += "subject.board:$(Format-NullableValue $leftSubject.Board)->$(Format-NullableValue $rightSubject.Board)"
    }
    if (-not (Compare-StringArrays -Left $leftSubject.ActiveFacets -Right $rightSubject.ActiveFacets)) {
        $changes += "subject.active_facets:[$(Join-Names $leftSubject.ActiveFacets)]->[$(Join-Names $rightSubject.ActiveFacets)]"
    }

    return $changes
}

function Compare-CaseMetadata {
    param(
        $LeftCase,
        $RightCase
    )

    $changes = @()
    $leftDeclaredFacts = @(Get-CaseDeclaredFacts -CaseEntry $LeftCase)
    $rightDeclaredFacts = @(Get-CaseDeclaredFacts -CaseEntry $RightCase)
    if (-not (Compare-StringArrays -Left $leftDeclaredFacts -Right $rightDeclaredFacts)) {
        $changes += "declared_facts:[$(Join-Names $leftDeclaredFacts)]->[$(Join-Names $rightDeclaredFacts)]"
    }
    $leftRequiredFacts = @(Get-CaseRequiredFacts -CaseEntry $LeftCase)
    $rightRequiredFacts = @(Get-CaseRequiredFacts -CaseEntry $RightCase)
    if (-not (Compare-StringArrays -Left $leftRequiredFacts -Right $rightRequiredFacts)) {
        $changes += "required_facts:[$(Join-Names $leftRequiredFacts)]->[$(Join-Names $rightRequiredFacts)]"
    }
    $leftAuditProvidedFacts = @(Get-CaseAuditProvidedFacts -CaseEntry $LeftCase)
    $rightAuditProvidedFacts = @(Get-CaseAuditProvidedFacts -CaseEntry $RightCase)
    if (-not (Compare-StringArrays -Left $leftAuditProvidedFacts -Right $rightAuditProvidedFacts)) {
        $changes += "audit_provided_facts:[$(Join-Names $leftAuditProvidedFacts)]->[$(Join-Names $rightAuditProvidedFacts)]"
    }
    $leftDeclaredContracts = @(Get-DeclaredContractTexts -CaseEntry $LeftCase)
    $rightDeclaredContracts = @(Get-DeclaredContractTexts -CaseEntry $RightCase)
    if (-not (Compare-StringArrays -Left $leftDeclaredContracts -Right $rightDeclaredContracts)) {
        $changes += "declared_contracts:[$(Join-Names $leftDeclaredContracts)]->[$(Join-Names $rightDeclaredContracts)]"
    }

    return @($changes)
}

function New-EmptyNodeChanges {
    return [pscustomobject]@{
        Added = @()
        Removed = @()
        Changed = @()
    }
}

function New-EmptyEdgeChanges {
    return [pscustomobject]@{
        Added = @()
        Removed = @()
    }
}

function Compare-Case {
    param(
        [string]$CaseName,
        $LeftBundle,
        $RightBundle,
        $LeftCase,
        $RightCase
    )

    if ($null -eq $LeftCase -and $null -eq $RightCase) {
        throw "internal error: compare case called without entries"
    }

    if ($null -eq $LeftCase) {
        $rightGraph = Load-CaseGraph -Bundle $RightBundle -CaseEntry $RightCase
        $addedNodes = @()
        $addedEdges = @()
        if ($null -ne $rightGraph) {
            $addedNodes = @((Get-NodeDescriptors -Graph $rightGraph.Data).Values)
            $addedEdges = @((Get-EdgeDescriptors -Graph $rightGraph.Data).Values)
        }
        return [pscustomobject]@{
            Case = $CaseName
            Status = 'added'
            SummaryChanges = @('case added')
            MetadataChanges = @()
            NodeChanges = [pscustomobject]@{ Added = $addedNodes; Removed = @(); Changed = @() }
            EdgeChanges = [pscustomobject]@{ Added = $addedEdges; Removed = @() }
            Left = $null
            Right = $RightCase
        }
    }

    if ($null -eq $RightCase) {
        $leftGraph = Load-CaseGraph -Bundle $LeftBundle -CaseEntry $LeftCase
        $removedNodes = @()
        $removedEdges = @()
        if ($null -ne $leftGraph) {
            $removedNodes = @((Get-NodeDescriptors -Graph $leftGraph.Data).Values)
            $removedEdges = @((Get-EdgeDescriptors -Graph $leftGraph.Data).Values)
        }
        return [pscustomobject]@{
            Case = $CaseName
            Status = 'removed'
            SummaryChanges = @('case removed')
            MetadataChanges = @()
            NodeChanges = [pscustomobject]@{ Added = @(); Removed = $removedNodes; Changed = @() }
            EdgeChanges = [pscustomobject]@{ Added = @(); Removed = $removedEdges }
            Left = $LeftCase
            Right = $null
        }
    }

    $summaryChanges = @(Compare-CaseSummary -LeftCase $LeftCase -RightCase $RightCase)
    $metadataChanges = @(Compare-CaseMetadata -LeftCase $LeftCase -RightCase $RightCase)
    $leftGraph = Load-CaseGraph -Bundle $LeftBundle -CaseEntry $LeftCase
    $rightGraph = Load-CaseGraph -Bundle $RightBundle -CaseEntry $RightCase
    $nodeChanges = New-EmptyNodeChanges
    $edgeChanges = New-EmptyEdgeChanges
    if ($null -ne $leftGraph -and $null -ne $rightGraph) {
        $nodeChanges = Compare-Nodes -LeftGraph $leftGraph.Data -RightGraph $rightGraph.Data
        $edgeChanges = Compare-Edges -LeftGraph $leftGraph.Data -RightGraph $rightGraph.Data
    }

    $status = 'unchanged'
    if ($summaryChanges.Count -gt 0 -or $nodeChanges.Added.Count -gt 0 -or $nodeChanges.Removed.Count -gt 0 -or $nodeChanges.Changed.Count -gt 0 -or $edgeChanges.Added.Count -gt 0 -or $edgeChanges.Removed.Count -gt 0) {
        $status = 'changed'
    }

    return [pscustomobject]@{
        Case = $CaseName
        Status = $status
        SummaryChanges = $summaryChanges
        MetadataChanges = $metadataChanges
        NodeChanges = $nodeChanges
        EdgeChanges = $edgeChanges
        Left = $LeftCase
        Right = $RightCase
    }
}

function New-CaseSummaryRow {
    param(
        $CaseDiff
    )

    $leftNodes = Get-CaseNodeCount -CaseEntry $CaseDiff.Left
    $rightNodes = Get-CaseNodeCount -CaseEntry $CaseDiff.Right
    $leftEdges = Get-CaseEdgeCount -CaseEntry $CaseDiff.Left
    $rightEdges = Get-CaseEdgeCount -CaseEntry $CaseDiff.Right

    return [pscustomobject]@{
        Case = $CaseDiff.Case
        Status = $CaseDiff.Status
        Kind = "$(Get-CaseKind -CaseEntry $CaseDiff.Left)->$(Get-CaseKind -CaseEntry $CaseDiff.Right)"
        Nodes = "$leftNodes->$rightNodes"
        Edges = "$leftEdges->$rightEdges"
        NodeDelta = "+$($CaseDiff.NodeChanges.Added.Count) -$($CaseDiff.NodeChanges.Removed.Count) ~$($CaseDiff.NodeChanges.Changed.Count)"
        EdgeDelta = "+$($CaseDiff.EdgeChanges.Added.Count) -$($CaseDiff.EdgeChanges.Removed.Count)"
        Summary = ($CaseDiff.SummaryChanges -join '; ')
        Metadata = ($CaseDiff.MetadataChanges -join '; ')
    }
}

function New-NodeJsonView {
    param(
        $NodeDescriptor
    )

    $view = [ordered]@{
        name = $NodeDescriptor.Name
        key = $NodeDescriptor.Key
        kind = $NodeDescriptor.Kind
        phase = $NodeDescriptor.Phase
        runlevel = $NodeDescriptor.Runlevel
        provides = @($NodeDescriptor.Provides)
        requires = @($NodeDescriptor.Requires)
    }

    if (-not [string]::IsNullOrWhiteSpace([string]$NodeDescriptor.ConnectionSource) -or -not [string]::IsNullOrWhiteSpace([string]$NodeDescriptor.ConnectionSink) -or -not [string]::IsNullOrWhiteSpace([string]$NodeDescriptor.ConnectionMode)) {
        $view.connection = [ordered]@{
            source = [string]$NodeDescriptor.ConnectionSource
            sink = [string]$NodeDescriptor.ConnectionSink
            mode = [string]$NodeDescriptor.ConnectionMode
        }
    }

    return $view
}

function New-EdgeJsonView {
    param(
        $EdgeDescriptor
    )

    return [ordered]@{
        key = $EdgeDescriptor.Key
        provider = $EdgeDescriptor.Provider
        consumer = $EdgeDescriptor.Consumer
        capability = $EdgeDescriptor.Capability
    }
}

function New-CaseJsonView {
    param(
        $Bundle,
        $CaseEntry
    )

    if ($null -eq $CaseEntry) {
        return $null
    }

    $caseJson = [ordered]@{
        name = [string]$CaseEntry.name
        source = [string]$CaseEntry.source
        build_dir = [string]$CaseEntry.build_dir
        build_target = [string]$CaseEntry.build_target
        case_kind = Get-CaseKind -CaseEntry $CaseEntry
        export_target = Get-CaseStringProperty -CaseEntry $CaseEntry -PropertyName 'export_target'
        dot = Resolve-CaseArtifactOrNull -Bundle $Bundle -CaseEntry $CaseEntry -FieldName 'dot'
        json = Resolve-CaseArtifactOrNull -Bundle $Bundle -CaseEntry $CaseEntry -FieldName 'json'
        runtime_observe = Resolve-CaseArtifactOrNull -Bundle $Bundle -CaseEntry $CaseEntry -FieldName 'runtime_observe'
        fact_evidence = Resolve-CaseArtifactOrNull -Bundle $Bundle -CaseEntry $CaseEntry -FieldName 'fact_evidence'
        declared_facts = @(Get-CaseDeclaredFacts -CaseEntry $CaseEntry)
        required_facts = @(Get-CaseRequiredFacts -CaseEntry $CaseEntry)
        audit_provided_facts = @(Get-CaseAuditProvidedFacts -CaseEntry $CaseEntry)
        declared_contracts = @(Get-CaseDeclaredContracts -CaseEntry $CaseEntry)
        graph = Get-CaseGraphSummary -CaseEntry $CaseEntry
    }

    $subjectJson = New-CaseSubjectJsonView -CaseEntry $CaseEntry
    if ($null -ne $subjectJson) {
        $caseJson.subject = $subjectJson
    }

    return $caseJson
}

function Write-CaseDetails {
    param(
        $CaseDiff
    )

    Write-Host "[CASE] $($CaseDiff.Case) status=$($CaseDiff.Status)"
    Write-Host "[KIND] $(Get-CaseKind -CaseEntry $CaseDiff.Left) -> $(Get-CaseKind -CaseEntry $CaseDiff.Right)"
    if ($CaseDiff.SummaryChanges.Count -gt 0) {
        Write-Host "[SUMMARY] $($CaseDiff.SummaryChanges -join '; ')"
    }
    if ($CaseDiff.MetadataChanges.Count -gt 0) {
        Write-Host "[METADATA] $($CaseDiff.MetadataChanges -join '; ')"
    }
    $leftContracts = @()
    if ($null -ne $CaseDiff.Left) {
        $leftContracts = @(Get-DeclaredContractTexts -CaseEntry $CaseDiff.Left)
    }
    if ($leftContracts.Count -gt 0) {
        Write-Host "[LEFT DECLARED CONTRACTS] $($leftContracts -join '; ')"
    }
    $rightContracts = @()
    if ($null -ne $CaseDiff.Right) {
        $rightContracts = @(Get-DeclaredContractTexts -CaseEntry $CaseDiff.Right)
    }
    if ($rightContracts.Count -gt 0) {
        Write-Host "[RIGHT DECLARED CONTRACTS] $($rightContracts -join '; ')"
    }

    if ($CaseDiff.NodeChanges.Added.Count -gt 0) {
        Write-Host '[NODES ADDED]'
        @($CaseDiff.NodeChanges.Added | Sort-Object Name) | Select-Object Name, Kind, Phase, Runlevel, @{Name='Connection';Expression={ Format-NodeConnectionText $_ }}, @{Name='Provides';Expression={ Join-Names $_.Provides }}, @{Name='Requires';Expression={ Join-Names $_.Requires }} | Format-Table -Wrap -AutoSize | Out-Host
    }
    if ($CaseDiff.NodeChanges.Removed.Count -gt 0) {
        Write-Host '[NODES REMOVED]'
        @($CaseDiff.NodeChanges.Removed | Sort-Object Name) | Select-Object Name, Kind, Phase, Runlevel, @{Name='Connection';Expression={ Format-NodeConnectionText $_ }}, @{Name='Provides';Expression={ Join-Names $_.Provides }}, @{Name='Requires';Expression={ Join-Names $_.Requires }} | Format-Table -Wrap -AutoSize | Out-Host
    }
    if ($CaseDiff.NodeChanges.Changed.Count -gt 0) {
        Write-Host '[NODES CHANGED]'
        @($CaseDiff.NodeChanges.Changed | Sort-Object Name) | Select-Object Name, @{Name='Fields';Expression={ $_.Fields -join '; ' }}, @{Name='LeftConnection';Expression={ Format-NodeConnectionText $_.Left }}, @{Name='RightConnection';Expression={ Format-NodeConnectionText $_.Right }} | Format-Table -Wrap -AutoSize | Out-Host
    }
    if ($CaseDiff.EdgeChanges.Added.Count -gt 0) {
        Write-Host '[EDGES ADDED]'
        @($CaseDiff.EdgeChanges.Added | Sort-Object Provider, Consumer, Capability) | Format-Table -AutoSize Provider, Consumer, Capability | Out-Host
    }
    if ($CaseDiff.EdgeChanges.Removed.Count -gt 0) {
        Write-Host '[EDGES REMOVED]'
        @($CaseDiff.EdgeChanges.Removed | Sort-Object Provider, Consumer, Capability) | Format-Table -AutoSize Provider, Consumer, Capability | Out-Host
    }
}

$leftBundle = Load-Bundle -BundleRoot $LeftBundleRoot -IndexPath $LeftIndex -SideName 'left'
$rightBundle = Load-Bundle -BundleRoot $RightBundleRoot -IndexPath $RightIndex -SideName 'right'

$leftMap = Get-CaseMap -Bundle $leftBundle
$rightMap = Get-CaseMap -Bundle $rightBundle
$selectedCaseNames = @(Get-SelectedCaseNames -LeftBundle $leftBundle -RightBundle $rightBundle)

$caseDiffs = @()
foreach ($caseName in $selectedCaseNames) {
    $leftCase = if ($leftMap.ContainsKey($caseName)) { $leftMap[$caseName] } else { $null }
    $rightCase = if ($rightMap.ContainsKey($caseName)) { $rightMap[$caseName] } else { $null }
    $caseDiffs += Compare-Case -CaseName $caseName -LeftBundle $leftBundle -RightBundle $rightBundle -LeftCase $leftCase -RightCase $rightCase
}

if (-not $IncludeUnchanged) {
    $caseDiffs = @(
        $caseDiffs |
            Where-Object {
                $_.Status -ne 'unchanged' -or @($_.MetadataChanges).Count -gt 0
            }
    )
}

$selectedLeftCases = @($selectedCaseNames | Where-Object { $leftMap.ContainsKey($_) } | ForEach-Object { $leftMap[$_] })
$selectedRightCases = @($selectedCaseNames | Where-Object { $rightMap.ContainsKey($_) } | ForEach-Object { $rightMap[$_] })
$leftDeclaredFactsDefaults = Get-DerivedDeclaredFactsDefaults -CaseEntries $selectedLeftCases
$rightDeclaredFactsDefaults = Get-DerivedDeclaredFactsDefaults -CaseEntries $selectedRightCases
$leftDeclaredContractsDefaults = Get-DerivedDeclaredContractsDefaults -CaseEntries $selectedLeftCases
$rightDeclaredContractsDefaults = Get-DerivedDeclaredContractsDefaults -CaseEntries $selectedRightCases

if ($AsJson) {
    $payload = [ordered]@{
        schema = 'materialized_graph.bundle_diff/v1'
        generated_at_utc = (Get-Date).ToUniversalTime().ToString('o')
        include_unchanged = $IncludeUnchanged.IsPresent
        left = [ordered]@{
            index = $leftBundle.IndexPath
            bundle_root = $leftBundle.BundleRoot
            input_manifest = $leftBundle.InputManifest
            declared_facts_defaults = if ($null -eq $leftDeclaredFactsDefaults) { $null } else { ,@($leftDeclaredFactsDefaults) }
            declared_contracts_defaults = if ($null -eq $leftDeclaredContractsDefaults) { $null } else { ,@($leftDeclaredContractsDefaults) }
        }
        right = [ordered]@{
            index = $rightBundle.IndexPath
            bundle_root = $rightBundle.BundleRoot
            input_manifest = $rightBundle.InputManifest
            declared_facts_defaults = if ($null -eq $rightDeclaredFactsDefaults) { $null } else { ,@($rightDeclaredFactsDefaults) }
            declared_contracts_defaults = if ($null -eq $rightDeclaredContractsDefaults) { $null } else { ,@($rightDeclaredContractsDefaults) }
        }
        status_counts = [ordered]@{
            changed = Get-CaseStatusCount -CaseDiffs $caseDiffs -Status 'changed'
            added = Get-CaseStatusCount -CaseDiffs $caseDiffs -Status 'added'
            removed = Get-CaseStatusCount -CaseDiffs $caseDiffs -Status 'removed'
            unchanged = Get-CaseStatusCount -CaseDiffs $caseDiffs -Status 'unchanged'
        }
        case_count = $caseDiffs.Count
        cases = @($caseDiffs | ForEach-Object {
            [ordered]@{
                name = $_.Case
                status = $_.Status
                left_case = New-CaseJsonView -Bundle $leftBundle -CaseEntry $_.Left
                right_case = New-CaseJsonView -Bundle $rightBundle -CaseEntry $_.Right
                summary_changes = $_.SummaryChanges
                metadata_changes = $_.MetadataChanges
                node_changes = [ordered]@{
                    added = @($_.NodeChanges.Added | ForEach-Object { New-NodeJsonView -NodeDescriptor $_ })
                    removed = @($_.NodeChanges.Removed | ForEach-Object { New-NodeJsonView -NodeDescriptor $_ })
                    changed = @($_.NodeChanges.Changed | ForEach-Object {
                        [ordered]@{
                            name = $_.Name
                            key = $_.Key
                            fields = $_.Fields
                            left = New-NodeJsonView -NodeDescriptor $_.Left
                            right = New-NodeJsonView -NodeDescriptor $_.Right
                        }
                    })
                }
                edge_changes = [ordered]@{
                    added = @($_.EdgeChanges.Added | ForEach-Object { New-EdgeJsonView -EdgeDescriptor $_ })
                    removed = @($_.EdgeChanges.Removed | ForEach-Object { New-EdgeJsonView -EdgeDescriptor $_ })
                }
            }
        })
    }
    $payload | ConvertTo-Json -Depth 8
    exit 0
}

Write-Host "[LEFT]  $($leftBundle.IndexPath)"
if ($null -ne $leftBundle.InputManifest) {
    Write-Host "[LEFT MANIFEST]  $($leftBundle.InputManifest.path) ($($leftBundle.InputManifest.schema))"
}
if ($null -ne $leftDeclaredFactsDefaults) {
    Write-Host "[LEFT DECLARED FACTS DEFAULTS]  $($leftDeclaredFactsDefaults -join ', ')"
}
if ($null -ne $leftDeclaredContractsDefaults) {
    Write-Host "[LEFT DECLARED CONTRACTS DEFAULTS]  $((@($leftDeclaredContractsDefaults | ForEach-Object { Format-DeclaredContract -ContractEntry $_ }) -join '; '))"
}
Write-Host "[RIGHT] $($rightBundle.IndexPath)"
if ($null -ne $rightBundle.InputManifest) {
    Write-Host "[RIGHT MANIFEST] $($rightBundle.InputManifest.path) ($($rightBundle.InputManifest.schema))"
}
if ($null -ne $rightDeclaredFactsDefaults) {
    Write-Host "[RIGHT DECLARED FACTS DEFAULTS] $($rightDeclaredFactsDefaults -join ', ')"
}
if ($null -ne $rightDeclaredContractsDefaults) {
    Write-Host "[RIGHT DECLARED CONTRACTS DEFAULTS] $((@($rightDeclaredContractsDefaults | ForEach-Object { Format-DeclaredContract -ContractEntry $_ }) -join '; '))"
}
Write-Host ''

if ($caseDiffs.Count -eq 0) {
    Write-Host '[OK] no visible differences'
    exit 0
}

$summaryRows = @($caseDiffs | ForEach-Object { New-CaseSummaryRow -CaseDiff $_ })
$summaryRows | Sort-Object Case | Format-Table -AutoSize Case, Status, Kind, Nodes, Edges, NodeDelta, EdgeDelta, Summary, Metadata | Out-Host

if ($ShowDetails -or $caseDiffs.Count -eq 1) {
    foreach ($caseDiff in @($caseDiffs | Sort-Object Case)) {
        Write-Host ''
        Write-CaseDetails -CaseDiff $caseDiff
    }
}
