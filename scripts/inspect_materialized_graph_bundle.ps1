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

function Load-CaseGraph {
    param(
        [string]$BundleRootPath,
        $CaseEntry
    )

    $jsonPath = Resolve-CaseArtifactPath -BundleRootPath $BundleRootPath -RelativeOrAbsolutePath ([string]$CaseEntry.json)
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
        Profile = $subject.Profile
        Board = $subject.Board
        Facets = Format-ActiveFacets $subject.ActiveFacets
        Nodes = [int]$CaseEntry.graph.node_count
        Edges = [int]$CaseEntry.graph.edge_count
        Phase = [string]$CaseEntry.graph.effective_max_phase
        Runlevel = [string]$CaseEntry.graph.effective_runlevel_text
        Kinds = Format-NodeKinds $CaseEntry.graph.node_kinds
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
        $summaryRows | Sort-Object Case | Format-Table -AutoSize Case, Profile, Board, Facets, Nodes, Edges, Phase, Runlevel, Kinds | Out-Host
    }
    exit 0
}

$selectedCase = $selectedCases[0]
$caseGraph = Load-CaseGraph -BundleRootPath $bundle.BundleRoot -CaseEntry $selectedCase
$nodeRows = @(New-NodeRows -Graph $caseGraph.Data)
$edgeRows = @(New-EdgeRows -Graph $caseGraph.Data)

if ($AsJson) {
    $payload = [ordered]@{
        index = $bundle.IndexPath
        bundle_root = $bundle.BundleRoot
        case = [ordered]@{
            name = [string]$selectedCase.name
            source = [string]$selectedCase.source
            build_dir = [string]$selectedCase.build_dir
            build_target = [string]$selectedCase.build_target
            export_target = [string]$selectedCase.export_target
            dot = Resolve-CaseArtifactPath -BundleRootPath $bundle.BundleRoot -RelativeOrAbsolutePath ([string]$selectedCase.dot)
            json = $caseGraph.Path
            graph = $selectedCase.graph
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
Write-Host "[DOT]  $(Resolve-CaseArtifactPath -BundleRootPath $bundle.BundleRoot -RelativeOrAbsolutePath ([string]$selectedCase.dot))"
Write-Host "[JSON] $($caseGraph.Path)"
Write-Host ''

$summaryRows | Format-List Case, Profile, Board, Facets, Nodes, Edges, Phase, Runlevel, Kinds | Out-Host
Write-Host '[NODES]'
$nodeRows | Format-Table -AutoSize Index, Kind, Phase, Name, Provides, Requires | Out-Host

if ($ShowEdges) {
    Write-Host ''
    Write-Host '[EDGES]'
    $edgeRows | Format-Table -AutoSize Provider, Consumer, Capability | Out-Host
}
