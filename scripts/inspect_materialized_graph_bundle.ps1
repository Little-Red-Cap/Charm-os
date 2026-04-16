param(
    [string]$BundleRoot = "out/materialized-graph-bundle",
    [string]$Index = "",
    [string[]]$Case = @(),
    [switch]$ListCases,
    [switch]$ShowEdges,
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
    if (-not [string]::IsNullOrWhiteSpace($Index)) {
        return Resolve-FullPath $Index
    }

    return Join-Path (Resolve-FullPath $BundleRoot) 'index.json'
}

function Load-BundleIndex {
    $indexPath = Resolve-IndexPath
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
        Data = $indexData
    }
}

function Get-BundleInputManifestInfo {
    param(
        $Bundle
    )

    if ($null -eq $Bundle -or $null -eq $Bundle.Data -or $null -eq $Bundle.Data.PSObject.Properties['input_manifest']) {
        return $null
    }

    $manifest = $Bundle.Data.input_manifest
    if ($null -eq $manifest) {
        return $null
    }

    $path = $null
    $schema = $null
    if ($null -ne $manifest.PSObject.Properties['path'] -and -not [string]::IsNullOrWhiteSpace([string]$manifest.path)) {
        $path = [string]$manifest.path
    }
    if ($null -ne $manifest.PSObject.Properties['schema'] -and -not [string]::IsNullOrWhiteSpace([string]$manifest.schema)) {
        $schema = [string]$manifest.schema
    }
    if ([string]::IsNullOrWhiteSpace($path) -and [string]::IsNullOrWhiteSpace($schema)) {
        return $null
    }

    return [ordered]@{
        path = $path
        schema = $schema
    }
}

function Format-NodeKinds {
    param(
        $NodeKinds
    )

    if ($null -eq $NodeKinds) {
        return ''
    }

    $parts = @()
    foreach ($property in $NodeKinds.PSObject.Properties) {
        $parts += "$($property.Name)=$($property.Value)"
    }

    return ($parts -join ', ')
}

function Format-CapabilityList {
    param(
        $Capabilities
    )

    if ($null -eq $Capabilities) {
        return ''
    }

    $names = @()
    foreach ($capability in @($Capabilities)) {
        if ($null -eq $capability) {
            continue
        }

        $name = [string]$capability.name
        if ([string]::IsNullOrWhiteSpace($name)) {
            $name = [string]$capability.id
        }
        $names += $name
    }

    return ($names -join ', ')
}

function Format-ActiveFacets {
    param(
        [string[]]$ActiveFacets
    )

    return (@($ActiveFacets) -join ', ')
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

function Format-DeclaredContracts {
    param(
        $Contracts
    )

    $parts = @()
    foreach ($contract in @($Contracts)) {
        if ($null -eq $contract) {
            continue
        }

        $contractName = [string]$contract.contract
        if ([string]::IsNullOrWhiteSpace($contractName)) {
            continue
        }

        $requires = @()
        if ($null -ne $contract.PSObject.Properties['requires']) {
            $requires = @($contract.requires)
        }
        $parts += "$contractName requires [$((@($requires) -join ', '))]"
    }

    return ($parts -join '; ')
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

function Get-CaseEffectivePhase {
    param(
        $CaseEntry
    )

    $graphSummary = Get-CaseGraphSummary -CaseEntry $CaseEntry
    if ($null -eq $graphSummary) {
        return $null
    }

    return [string]$graphSummary.effective_max_phase
}

function Get-CaseEffectiveRunlevel {
    param(
        $CaseEntry
    )

    $graphSummary = Get-CaseGraphSummary -CaseEntry $CaseEntry
    if ($null -eq $graphSummary) {
        return $null
    }

    return [string]$graphSummary.effective_runlevel_text
}

function Get-CaseNodeKindsText {
    param(
        $CaseEntry
    )

    $graphSummary = Get-CaseGraphSummary -CaseEntry $CaseEntry
    if ($null -eq $graphSummary) {
        return ''
    }

    return Format-NodeKinds $graphSummary.node_kinds
}

function Load-CaseGraph {
    param(
        [string]$BundleRootPath,
        $CaseEntry
    )

    $jsonReference = Get-CaseStringProperty -CaseEntry $CaseEntry -PropertyName 'json'
    if ([string]::IsNullOrWhiteSpace($jsonReference)) {
        if ((Get-CaseKind -CaseEntry $CaseEntry) -eq 'materialized_graph') {
            throw "case json missing for materialized_graph case: $([string]$CaseEntry.name)"
        }

        return $null
    }

    $jsonPath = Resolve-CaseArtifactPath -BundleRootPath $BundleRootPath -RelativeOrAbsolutePath $jsonReference
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

function Get-SelectedCases {
    param(
        $Bundle
    )

    $allCases = @($Bundle.Data.cases)
    if ($Case.Count -eq 0) {
        return $allCases
    }

    $selected = @()
    foreach ($caseName in $Case) {
        $match = @($allCases | Where-Object { $_.name -eq $caseName })
        if ($match.Count -eq 0) {
            throw "unknown case: $caseName"
        }

        $selected += $match[0]
    }

    return $selected
}

function New-CaseSummaryRow {
    param(
        $CaseEntry
    )

    $subject = Get-CaseSubjectInfo -CaseEntry $CaseEntry
    return [pscustomobject]@{
        Case = [string]$CaseEntry.name
        CaseKind = Get-CaseKind -CaseEntry $CaseEntry
        Profile = $subject.Profile
        Board = $subject.Board
        Facets = Format-ActiveFacets $subject.ActiveFacets
        DeclaredFacts = @(Get-CaseDeclaredFacts -CaseEntry $CaseEntry)
        DeclaredContracts = @(Get-CaseDeclaredContracts -CaseEntry $CaseEntry)
        Nodes = Get-CaseNodeCount -CaseEntry $CaseEntry
        Edges = Get-CaseEdgeCount -CaseEntry $CaseEntry
        Phase = Get-CaseEffectivePhase -CaseEntry $CaseEntry
        Runlevel = Get-CaseEffectiveRunlevel -CaseEntry $CaseEntry
        Kinds = Get-CaseNodeKindsText -CaseEntry $CaseEntry
    }
}

function New-NodeRows {
    param(
        $Graph
    )

    $rows = @()
    foreach ($node in @($Graph.nodes)) {
        $rows += [pscustomobject]@{
            Index = [int]$node.index
            Kind = [string]$node.kind
            Phase = [string]$node.phase
            Name = [string]$node.name
            Provides = Format-CapabilityList $node.provides
            Requires = Format-CapabilityList $node.requires
        }
    }

    return $rows
}

function New-EdgeRows {
    param(
        $Graph
    )

    $rows = @()
    foreach ($edge in @($Graph.edges)) {
        $providerName = [string]$Graph.nodes[[int]$edge.provider_index].name
        $consumerName = [string]$Graph.nodes[[int]$edge.consumer_index].name
        $capabilityName = [string]$edge.capability.name
        if ([string]::IsNullOrWhiteSpace($capabilityName)) {
            $capabilityName = [string]$edge.capability.id
        }

        $rows += [pscustomobject]@{
            Provider = $providerName
            Consumer = $consumerName
            Capability = $capabilityName
        }
    }

    return $rows
}

$bundle = Load-BundleIndex
$inputManifest = Get-BundleInputManifestInfo -Bundle $bundle
$selectedCases = @(Get-SelectedCases -Bundle $bundle)

if ($ListCases) {
    if ($AsJson) {
        @($selectedCases | ForEach-Object { [string]$_.name }) | ConvertTo-Json -Depth 2
    } else {
        if ($null -ne $inputManifest) {
            Write-Host "[MANIFEST] $($inputManifest.path) ($($inputManifest.schema))"
        }
        $selectedCases | ForEach-Object { [string]$_.name }
    }
    exit 0
}

if ($ShowEdges -and $selectedCases.Count -ne 1) {
    throw "-ShowEdges requires exactly one selected case"
}

$summaryRows = @($selectedCases | ForEach-Object { New-CaseSummaryRow -CaseEntry $_ })

if ($selectedCases.Count -ne 1) {
    if ($AsJson) {
        $payload = [ordered]@{
            index = $bundle.IndexPath
            bundle_root = $bundle.BundleRoot
            case_count = $summaryRows.Count
            cases = $summaryRows
        }
        if ($null -ne $inputManifest) {
            $payload.input_manifest = $inputManifest
        }
        $payload | ConvertTo-Json -Depth 6
    } else {
        Write-Host "[BUNDLE] $($bundle.IndexPath)"
        if ($null -ne $inputManifest) {
            Write-Host "[MANIFEST] $($inputManifest.path) ($($inputManifest.schema))"
        }
        $summaryRows | Sort-Object Case | Format-Table -AutoSize Case, CaseKind, Profile, Board, Facets, Nodes, Edges, Phase, Runlevel, Kinds | Out-Host
    }
    exit 0
}

$selectedCase = $selectedCases[0]
$caseGraph = Load-CaseGraph -BundleRootPath $bundle.BundleRoot -CaseEntry $selectedCase
if ($ShowEdges -and $null -eq $caseGraph) {
    throw "selected case has no static graph; -ShowEdges is unavailable for runtime_only cases"
}

$nodeRows = @()
$edgeRows = @()
if ($null -ne $caseGraph) {
    $nodeRows = @(New-NodeRows -Graph $caseGraph.Data)
    $edgeRows = @(New-EdgeRows -Graph $caseGraph.Data)
}

if ($AsJson) {
    $payload = [ordered]@{
        index = $bundle.IndexPath
        bundle_root = $bundle.BundleRoot
        case = [ordered]@{
            name = [string]$selectedCase.name
            source = [string]$selectedCase.source
            build_dir = [string]$selectedCase.build_dir
            build_target = [string]$selectedCase.build_target
            case_kind = Get-CaseKind -CaseEntry $selectedCase
            export_target = Get-CaseStringProperty -CaseEntry $selectedCase -PropertyName 'export_target'
            dot = if ([string]::IsNullOrWhiteSpace((Get-CaseStringProperty -CaseEntry $selectedCase -PropertyName 'dot'))) { $null } else { Resolve-CaseArtifactPath -BundleRootPath $bundle.BundleRoot -RelativeOrAbsolutePath ([string]$selectedCase.dot) }
            json = if ($null -ne $caseGraph) { $caseGraph.Path } else { $null }
            runtime_observe = if ([string]::IsNullOrWhiteSpace((Get-CaseStringProperty -CaseEntry $selectedCase -PropertyName 'runtime_observe'))) { $null } else { Resolve-CaseArtifactPath -BundleRootPath $bundle.BundleRoot -RelativeOrAbsolutePath ([string]$selectedCase.runtime_observe) }
            declared_facts = @(Get-CaseDeclaredFacts -CaseEntry $selectedCase)
            declared_contracts = @(Get-CaseDeclaredContracts -CaseEntry $selectedCase)
            graph = Get-CaseGraphSummary -CaseEntry $selectedCase
        }
        nodes = $nodeRows
    }
    if ($null -ne $inputManifest) {
        $payload.input_manifest = $inputManifest
    }
    $subjectJson = New-CaseSubjectJsonView -CaseEntry $selectedCase
    if ($null -ne $subjectJson) {
        $payload.case.subject = $subjectJson
    }
    if ($ShowEdges) {
        $payload.edges = $edgeRows
    }

    $payload | ConvertTo-Json -Depth 8
    exit 0
}

Write-Host "[BUNDLE] $($bundle.IndexPath)"
if ($null -ne $inputManifest) {
    Write-Host "[MANIFEST] $($inputManifest.path) ($($inputManifest.schema))"
}
Write-Host "[CASE] $($selectedCase.name)"
Write-Host "[CASE KIND] $(Get-CaseKind -CaseEntry $selectedCase)"
$dotPath = if ([string]::IsNullOrWhiteSpace((Get-CaseStringProperty -CaseEntry $selectedCase -PropertyName 'dot'))) { $null } else { Resolve-CaseArtifactPath -BundleRootPath $bundle.BundleRoot -RelativeOrAbsolutePath ([string]$selectedCase.dot) }
$runtimeObservePath = if ([string]::IsNullOrWhiteSpace((Get-CaseStringProperty -CaseEntry $selectedCase -PropertyName 'runtime_observe'))) { $null } else { Resolve-CaseArtifactPath -BundleRootPath $bundle.BundleRoot -RelativeOrAbsolutePath ([string]$selectedCase.runtime_observe) }
if ($null -ne $dotPath) {
    Write-Host "[DOT]  $dotPath"
}
if ($null -ne $caseGraph) {
    Write-Host "[JSON] $($caseGraph.Path)"
} else {
    Write-Host "[GRAPH] no static graph"
}
if ($null -ne $runtimeObservePath) {
    Write-Host "[RUNTIME OBSERVE] $runtimeObservePath"
}
$declaredFacts = @(Get-CaseDeclaredFacts -CaseEntry $selectedCase)
if ($declaredFacts.Count -gt 0) {
    Write-Host "[DECLARED FACTS] $($declaredFacts -join ', ')"
}
$declaredContracts = @(Get-CaseDeclaredContracts -CaseEntry $selectedCase)
if ($declaredContracts.Count -gt 0) {
    Write-Host "[DECLARED CONTRACTS] $(Format-DeclaredContracts -Contracts $declaredContracts)"
}
Write-Host ''

$summaryRows | Format-List Case, CaseKind, Profile, Board, Facets, Nodes, Edges, Phase, Runlevel, Kinds | Out-Host
if ($null -ne $caseGraph) {
    Write-Host '[NODES]'
    $nodeRows | Format-Table -AutoSize Index, Kind, Phase, Name, Provides, Requires | Out-Host
} else {
    Write-Host '[NODES] no static graph'
}

if ($ShowEdges) {
    Write-Host ''
    Write-Host '[EDGES]'
    $edgeRows | Format-Table -AutoSize Provider, Consumer, Capability | Out-Host
}
