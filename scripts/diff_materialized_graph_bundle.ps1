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

    return [pscustomobject]@{
        Side = $SideName
        IndexPath = $resolvedIndexPath
        BundleRoot = Split-Path -Parent $resolvedIndexPath
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

function Load-CaseGraph {
    param(
        $Bundle,
        $CaseEntry
    )

    $jsonPath = Resolve-ArtifactPath -BundleRoot $Bundle.BundleRoot -Path ([string]$CaseEntry.json)
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

        $descriptors[$key] = [pscustomobject]@{
            Key = $key
            Index = [int]$node.index
            Name = [string]$node.name
            Kind = [string]$node.kind
            Phase = [string]$node.phase
            Runlevel = [string]$node.runlevel_text
            Provides = @(Get-CapabilityNames $node.provides)
            Requires = @(Get-CapabilityNames $node.requires)
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
    if ([string]$LeftCase.graph.schema -ne [string]$RightCase.graph.schema) {
        $changes += "schema:$([string]$LeftCase.graph.schema)->$([string]$RightCase.graph.schema)"
    }
    if ([int]$LeftCase.graph.node_count -ne [int]$RightCase.graph.node_count) {
        $changes += "node_count:$([int]$LeftCase.graph.node_count)->$([int]$RightCase.graph.node_count)"
    }
    if ([int]$LeftCase.graph.edge_count -ne [int]$RightCase.graph.edge_count) {
        $changes += "edge_count:$([int]$LeftCase.graph.edge_count)->$([int]$RightCase.graph.edge_count)"
    }
    if ([string]$LeftCase.graph.effective_max_phase -ne [string]$RightCase.graph.effective_max_phase) {
        $changes += "phase:$([string]$LeftCase.graph.effective_max_phase)->$([string]$RightCase.graph.effective_max_phase)"
    }
    if ([string]$LeftCase.graph.effective_runlevel_text -ne [string]$RightCase.graph.effective_runlevel_text) {
        $changes += "runlevel:$([string]$LeftCase.graph.effective_runlevel_text)->$([string]$RightCase.graph.effective_runlevel_text)"
    }

    $leftKinds = @()
    $rightKinds = @()
    if ($null -ne $LeftCase.graph.node_kinds) {
        foreach ($property in $LeftCase.graph.node_kinds.PSObject.Properties) {
            $leftKinds += "$($property.Name)=$($property.Value)"
        }
    }
    if ($null -ne $RightCase.graph.node_kinds) {
        foreach ($property in $RightCase.graph.node_kinds.PSObject.Properties) {
            $rightKinds += "$($property.Name)=$($property.Value)"
        }
    }
    if (-not (Compare-StringArrays -Left $leftKinds -Right $rightKinds)) {
        $changes += "node_kinds:[$(Join-Names $leftKinds)]->[$(Join-Names $rightKinds)]"
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
        $addedNodes = @((Get-NodeDescriptors -Graph $rightGraph.Data).Values)
        $addedEdges = @((Get-EdgeDescriptors -Graph $rightGraph.Data).Values)
        return [pscustomobject]@{
            Case = $CaseName
            Status = 'added'
            SummaryChanges = @('case added')
            NodeChanges = [pscustomobject]@{ Added = $addedNodes; Removed = @(); Changed = @() }
            EdgeChanges = [pscustomobject]@{ Added = $addedEdges; Removed = @() }
            Left = $null
            Right = $RightCase
        }
    }

    if ($null -eq $RightCase) {
        $leftGraph = Load-CaseGraph -Bundle $LeftBundle -CaseEntry $LeftCase
        $removedNodes = @((Get-NodeDescriptors -Graph $leftGraph.Data).Values)
        $removedEdges = @((Get-EdgeDescriptors -Graph $leftGraph.Data).Values)
        return [pscustomobject]@{
            Case = $CaseName
            Status = 'removed'
            SummaryChanges = @('case removed')
            NodeChanges = [pscustomobject]@{ Added = @(); Removed = $removedNodes; Changed = @() }
            EdgeChanges = [pscustomobject]@{ Added = @(); Removed = $removedEdges }
            Left = $LeftCase
            Right = $null
        }
    }

    $summaryChanges = @(Compare-CaseSummary -LeftCase $LeftCase -RightCase $RightCase)
    $leftGraph = Load-CaseGraph -Bundle $LeftBundle -CaseEntry $LeftCase
    $rightGraph = Load-CaseGraph -Bundle $RightBundle -CaseEntry $RightCase
    $nodeChanges = Compare-Nodes -LeftGraph $leftGraph.Data -RightGraph $rightGraph.Data
    $edgeChanges = Compare-Edges -LeftGraph $leftGraph.Data -RightGraph $rightGraph.Data

    $status = 'unchanged'
    if ($summaryChanges.Count -gt 0 -or $nodeChanges.Added.Count -gt 0 -or $nodeChanges.Removed.Count -gt 0 -or $nodeChanges.Changed.Count -gt 0 -or $edgeChanges.Added.Count -gt 0 -or $edgeChanges.Removed.Count -gt 0) {
        $status = 'changed'
    }

    return [pscustomobject]@{
        Case = $CaseName
        Status = $status
        SummaryChanges = $summaryChanges
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

    $leftNodes = if ($null -ne $CaseDiff.Left) { [int]$CaseDiff.Left.graph.node_count } else { 0 }
    $rightNodes = if ($null -ne $CaseDiff.Right) { [int]$CaseDiff.Right.graph.node_count } else { 0 }
    $leftEdges = if ($null -ne $CaseDiff.Left) { [int]$CaseDiff.Left.graph.edge_count } else { 0 }
    $rightEdges = if ($null -ne $CaseDiff.Right) { [int]$CaseDiff.Right.graph.edge_count } else { 0 }

    return [pscustomobject]@{
        Case = $CaseDiff.Case
        Status = $CaseDiff.Status
        Nodes = "$leftNodes->$rightNodes"
        Edges = "$leftEdges->$rightEdges"
        NodeDelta = "+$($CaseDiff.NodeChanges.Added.Count) -$($CaseDiff.NodeChanges.Removed.Count) ~$($CaseDiff.NodeChanges.Changed.Count)"
        EdgeDelta = "+$($CaseDiff.EdgeChanges.Added.Count) -$($CaseDiff.EdgeChanges.Removed.Count)"
        Summary = ($CaseDiff.SummaryChanges -join '; ')
    }
}

function New-NodeJsonView {
    param(
        $NodeDescriptor
    )

    return [ordered]@{
        name = $NodeDescriptor.Name
        key = $NodeDescriptor.Key
        kind = $NodeDescriptor.Kind
        phase = $NodeDescriptor.Phase
        runlevel = $NodeDescriptor.Runlevel
        provides = @($NodeDescriptor.Provides)
        requires = @($NodeDescriptor.Requires)
    }
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
        export_target = [string]$CaseEntry.export_target
        dot = Resolve-ArtifactPath -BundleRoot $Bundle.BundleRoot -Path ([string]$CaseEntry.dot)
        json = Resolve-ArtifactPath -BundleRoot $Bundle.BundleRoot -Path ([string]$CaseEntry.json)
        graph = $CaseEntry.graph
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
    if ($CaseDiff.SummaryChanges.Count -gt 0) {
        Write-Host "[SUMMARY] $($CaseDiff.SummaryChanges -join '; ')"
    }

    if ($CaseDiff.NodeChanges.Added.Count -gt 0) {
        Write-Host '[NODES ADDED]'
        @($CaseDiff.NodeChanges.Added | Sort-Object Name) | Select-Object Name, Kind, Phase, Runlevel, @{Name='Provides';Expression={ Join-Names $_.Provides }}, @{Name='Requires';Expression={ Join-Names $_.Requires }} | Format-Table -AutoSize | Out-Host
    }
    if ($CaseDiff.NodeChanges.Removed.Count -gt 0) {
        Write-Host '[NODES REMOVED]'
        @($CaseDiff.NodeChanges.Removed | Sort-Object Name) | Select-Object Name, Kind, Phase, Runlevel, @{Name='Provides';Expression={ Join-Names $_.Provides }}, @{Name='Requires';Expression={ Join-Names $_.Requires }} | Format-Table -AutoSize | Out-Host
    }
    if ($CaseDiff.NodeChanges.Changed.Count -gt 0) {
        Write-Host '[NODES CHANGED]'
        @($CaseDiff.NodeChanges.Changed | Sort-Object Name) | Select-Object Name, @{Name='Fields';Expression={ $_.Fields -join '; ' }} | Format-Table -AutoSize | Out-Host
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
    $caseDiffs = @($caseDiffs | Where-Object { $_.Status -ne 'unchanged' })
}

if ($AsJson) {
    $payload = [ordered]@{
        schema = 'materialized_graph.bundle_diff/v1'
        generated_at_utc = (Get-Date).ToUniversalTime().ToString('o')
        include_unchanged = $IncludeUnchanged.IsPresent
        left = [ordered]@{
            index = $leftBundle.IndexPath
            bundle_root = $leftBundle.BundleRoot
        }
        right = [ordered]@{
            index = $rightBundle.IndexPath
            bundle_root = $rightBundle.BundleRoot
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
Write-Host "[RIGHT] $($rightBundle.IndexPath)"
Write-Host ''

if ($caseDiffs.Count -eq 0) {
    Write-Host '[OK] no visible differences'
    exit 0
}

$summaryRows = @($caseDiffs | ForEach-Object { New-CaseSummaryRow -CaseDiff $_ })
$summaryRows | Sort-Object Case | Format-Table -AutoSize Case, Status, Nodes, Edges, NodeDelta, EdgeDelta, Summary | Out-Host

if ($ShowDetails -or $caseDiffs.Count -eq 1) {
    foreach ($caseDiff in @($caseDiffs | Sort-Object Case)) {
        Write-Host ''
        Write-CaseDetails -CaseDiff $caseDiff
    }
}
