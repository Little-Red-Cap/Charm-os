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

function Load-Bundle {
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
        Cases = @($indexData.cases)
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
        $Bundle,
        $CaseEntry
    )

    $jsonPath = Resolve-CaseArtifactPath -BundleRootPath $Bundle.BundleRoot -RelativeOrAbsolutePath ([string]$CaseEntry.json)
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
    }

    return [pscustomobject]@{
        Mode = $resolvedMode
        CiSummary = $resolvedCiSummary
        Diff = $resolvedDiff
        ReportManifest = $resolvedReportManifest
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

    $graph = $CaseGraph.Data
    $providedFacts = @(Get-ProvidedFacts -Graph $graph)
    $requiredFacts = @(Get-RequiredFacts -Graph $graph)
    $allCapabilities = @($providedFacts + $requiredFacts | Sort-Object -Unique)
    $unresolvedBindings = @(Get-UnresolvedBindings -RequiredFacts $requiredFacts -ProvidedFacts $providedFacts)
    $blockedReasons = @($unresolvedBindings | ForEach-Object { "unresolved binding: $_" })

    return [ordered]@{
        schema = 'system_compiler.artifact_report/v0'
        generated_at_utc = (Get-Date).ToUniversalTime().ToString('o')
        generator = 'scripts/export_system_compiler_artifact_report.ps1'
        report_kind = 'system_compiler.artifact_report'
        mode = $ArtifactContext.Mode
        subject = [ordered]@{
            case = [string]$CaseEntry.name
            profile = if ([string]::IsNullOrWhiteSpace($Profile)) { $null } else { $Profile }
            board = if ([string]::IsNullOrWhiteSpace($Board)) { $null } else { $Board }
            active_facets = @($Facet)
        }
        structure = [ordered]@{
            capability_count = $allCapabilities.Count
            node_count = [int]$graph.node_count
            edge_count = [int]$graph.edge_count
            materialized_order = @(Get-MaterializedOrder -Graph $graph)
            required_facts = $requiredFacts
            unresolved_bindings = $unresolvedBindings
        }
        bringup_evidence = [ordered]@{
            declared_count = $allCapabilities.Count
            materialized_count = $allCapabilities.Count
            published_count = 0
            observed_count = $allCapabilities.Count
            blocked_count = $unresolvedBindings.Count
            failed_count = 0
            published_capabilities = @()
            blocked_reasons = $blockedReasons
            failed_reasons = @()
        }
        resource_contract = [ordered]@{
            declared_contracts = 0
            provided_facts = @()
            audited_count = 0
            satisfied_count = 0
            violated_count = 0
            unknown_count = 0
            violations = @()
            unknown_contracts = @()
            resource_hotspots = @()
        }
        runtime_observe = [ordered]@{
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
        artifacts = [ordered]@{
            bundle = $Bundle.IndexPath
            dot = Resolve-CaseArtifactPath -BundleRootPath $Bundle.BundleRoot -RelativeOrAbsolutePath ([string]$CaseEntry.dot)
            sample_json = $CaseGraph.Path
            diff = $ArtifactContext.Diff
            ci_summary = $ArtifactContext.CiSummary
            report_manifest = $ArtifactContext.ReportManifest
        }
    }
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
