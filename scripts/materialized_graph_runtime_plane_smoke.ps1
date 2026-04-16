param(
    [string]$BundleRoot = 'out/materialized-graph-bundle',
    [string]$RuntimeOnlyCase = 'usb-host-runtime-multi-smoke',
    [string]$GraphDonorCase = 'usb-msc-block-demo',
    [string]$OutputRoot = 'out/materialized-graph-runtime-plane-smoke',
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

function Get-CaseEntry {
    param(
        [object[]]$Cases,
        [string]$CaseName
    )

    $matches = @($Cases | Where-Object { [string]$_.name -eq $CaseName })
    if ($matches.Count -eq 0) {
        throw "case not found: $CaseName"
    }

    return $matches[0]
}

function Invoke-DiffJson {
    param(
        [string]$DiffScript,
        [string]$LeftBundleRoot,
        [string]$RightBundleRoot,
        [string]$CaseName,
        [string]$OutputPath
    )

    $jsonText = (& $DiffScript -LeftBundleRoot $LeftBundleRoot -RightBundleRoot $RightBundleRoot -Case $CaseName -IncludeUnchanged -AsJson | Out-String)
    $jsonText | Set-Content -LiteralPath $OutputPath -Encoding utf8
    return ($jsonText | ConvertFrom-Json)
}

function Invoke-InspectJson {
    param(
        [string]$InspectScript,
        [string]$BundleRootPath,
        [string]$CaseName,
        [string]$OutputPath
    )

    $jsonText = (& $InspectScript -BundleRoot $BundleRootPath -Case $CaseName -AsJson | Out-String)
    $jsonText | Set-Content -LiteralPath $OutputPath -Encoding utf8
    return ($jsonText | ConvertFrom-Json)
}

function Assert-ShowEdgesFailsClearly {
    param(
        [string]$InspectScript,
        [string]$BundleRootPath,
        [string]$CaseName
    )

    try {
        & $InspectScript -BundleRoot $BundleRootPath -Case $CaseName -ShowEdges | Out-Null
    } catch {
        $message = $_.Exception.Message
        Assert-Condition ($message -like '*no static graph*') "unexpected -ShowEdges failure: $message"
        return
    }

    throw '-ShowEdges unexpectedly succeeded for runtime_only case'
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$resolvedBundleRoot = Resolve-FullPath $BundleRoot
$resolvedOutputRoot = Resolve-FullPath $OutputRoot
$rightBundleRoot = Join-Path $resolvedOutputRoot 'right-bundle'
$diffJsonPath = Join-Path $resolvedOutputRoot 'runtime_plane_diff.json'
$inspectJsonPath = Join-Path $resolvedOutputRoot 'runtime_plane_inspect.json'
$reportOutputRoot = Join-Path $resolvedOutputRoot 'report'
$reportMarkdownPath = Join-Path $reportOutputRoot 'materialized_graph_bundle_diff_report.md'
$summaryPath = Join-Path $resolvedOutputRoot 'runtime_plane_smoke.summary.json'

$diffScript = Join-Path $PSScriptRoot 'diff_materialized_graph_bundle.ps1'
$inspectScript = Join-Path $PSScriptRoot 'inspect_materialized_graph_bundle.ps1'
$reportScript = Join-Path $PSScriptRoot 'report_materialized_graph_bundle.ps1'

foreach ($requiredPath in @($resolvedBundleRoot, $diffScript, $inspectScript, $reportScript)) {
    if (-not (Test-Path $requiredPath)) {
        throw "required path not found: $requiredPath"
    }
}

if (-not $KeepOutput) {
    Remove-PathIfExists -Path $resolvedOutputRoot
}
Ensure-Directory -Path $resolvedOutputRoot
Ensure-Directory -Path $reportOutputRoot

$sourceIndexPath = Join-Path $resolvedBundleRoot 'index.json'
if (-not (Test-Path $sourceIndexPath)) {
    throw "bundle index not found: $sourceIndexPath"
}

Copy-Item -LiteralPath $resolvedBundleRoot -Destination $rightBundleRoot -Recurse -Force

$rightIndexPath = Join-Path $rightBundleRoot 'index.json'
$rightIndex = Get-Content -LiteralPath $rightIndexPath -Raw -Encoding utf8 | ConvertFrom-Json
$runtimeCase = Get-CaseEntry -Cases @($rightIndex.cases) -CaseName $RuntimeOnlyCase
$graphDonor = Get-CaseEntry -Cases @($rightIndex.cases) -CaseName $GraphDonorCase

Assert-Condition ([string]$runtimeCase.case_kind -eq 'runtime_only') "runtime case must be runtime_only: $RuntimeOnlyCase"
Assert-Condition ($null -eq $runtimeCase.graph) "runtime case must not expose graph before mutation: $RuntimeOnlyCase"
Assert-Condition ([string]$graphDonor.case_kind -eq 'materialized_graph') "graph donor must be materialized_graph: $GraphDonorCase"
Assert-Condition ($null -ne $graphDonor.graph) "graph donor must expose graph summary: $GraphDonorCase"
Assert-Condition (-not [string]::IsNullOrWhiteSpace([string]$graphDonor.dot)) "graph donor must expose dot path: $GraphDonorCase"
Assert-Condition (-not [string]::IsNullOrWhiteSpace([string]$graphDonor.json)) "graph donor must expose json path: $GraphDonorCase"

Set-ObjectProperty -Object $runtimeCase -Name 'case_kind' -Value 'materialized_graph'
Set-ObjectProperty -Object $runtimeCase -Name 'export_target' -Value 'synthetic.runtime_plane.graph'
Set-ObjectProperty -Object $runtimeCase -Name 'dot' -Value ([string]$graphDonor.dot)
Set-ObjectProperty -Object $runtimeCase -Name 'json' -Value ([string]$graphDonor.json)
Set-ObjectProperty -Object $runtimeCase -Name 'graph' -Value $graphDonor.graph

$rightIndex | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $rightIndexPath -Encoding utf8

Write-Host "[SYNTH] runtime_only -> materialized_graph case=$RuntimeOnlyCase donor=$GraphDonorCase"

$diffData = Invoke-DiffJson -DiffScript $diffScript -LeftBundleRoot $resolvedBundleRoot -RightBundleRoot $rightBundleRoot -CaseName $RuntimeOnlyCase -OutputPath $diffJsonPath
Assert-Condition ([string]$diffData.schema -eq 'materialized_graph.bundle_diff/v1') 'unexpected diff schema'
Assert-Condition ([int]$diffData.case_count -eq 1) 'runtime plane smoke expects exactly one diff case'

$caseDiff = @($diffData.cases)[0]
Assert-Condition ([string]$caseDiff.name -eq $RuntimeOnlyCase) 'unexpected diff case name'
Assert-Condition ([string]$caseDiff.status -eq 'changed') 'runtime plane transition must be reported as changed'
Assert-Condition ((@($caseDiff.summary_changes) -contains 'case_kind:runtime_only->materialized_graph')) 'summary_changes missing case_kind transition'
Assert-Condition ((@($caseDiff.summary_changes) -contains 'graph:unavailable->available')) 'summary_changes missing graph availability transition'
Assert-Condition ((@($caseDiff.node_changes.added)).Count -eq 0) 'node_changes.added must stay empty for cross-plane transition'
Assert-Condition ((@($caseDiff.node_changes.removed)).Count -eq 0) 'node_changes.removed must stay empty for cross-plane transition'
Assert-Condition ((@($caseDiff.node_changes.changed)).Count -eq 0) 'node_changes.changed must stay empty for cross-plane transition'
Assert-Condition ((@($caseDiff.edge_changes.added)).Count -eq 0) 'edge_changes.added must stay empty for cross-plane transition'
Assert-Condition ((@($caseDiff.edge_changes.removed)).Count -eq 0) 'edge_changes.removed must stay empty for cross-plane transition'
Assert-Condition ([string]$caseDiff.left_case.case_kind -eq 'runtime_only') 'left case_kind must remain runtime_only'
Assert-Condition ([string]$caseDiff.right_case.case_kind -eq 'materialized_graph') 'right case_kind must become materialized_graph'
Assert-Condition ($null -eq $caseDiff.left_case.graph) 'left graph must stay null'
Assert-Condition ($null -ne $caseDiff.right_case.graph) 'right graph must be populated'
Assert-Condition ([int]$caseDiff.right_case.graph.node_count -gt 0) 'right graph summary must carry node_count'

$inspectData = Invoke-InspectJson -InspectScript $inspectScript -BundleRootPath $resolvedBundleRoot -CaseName $RuntimeOnlyCase -OutputPath $inspectJsonPath
Assert-Condition ([string]$inspectData.case.case_kind -eq 'runtime_only') 'inspect must preserve runtime_only case_kind'
Assert-Condition ($null -eq $inspectData.case.graph) 'inspect must expose null graph for runtime_only case'
Assert-Condition ((@($inspectData.nodes)).Count -eq 0) 'inspect nodes must stay empty for runtime_only case'
Assert-Condition (-not [string]::IsNullOrWhiteSpace([string]$inspectData.case.runtime_observe)) 'inspect must preserve runtime_observe path'

Assert-ShowEdgesFailsClearly -InspectScript $inspectScript -BundleRootPath $resolvedBundleRoot -CaseName $RuntimeOnlyCase

& $reportScript -LeftBundleRoot $resolvedBundleRoot -RightBundleRoot $rightBundleRoot -Case $RuntimeOnlyCase -IncludeUnchanged -Format markdown -OutputDir $reportOutputRoot
if ($LASTEXITCODE -ne 0) {
    throw 'report generation failed'
}

if (-not (Test-Path $reportMarkdownPath)) {
    throw "report markdown not found: $reportMarkdownPath"
}

$reportText = Get-Content -LiteralPath $reportMarkdownPath -Raw -Encoding utf8
Assert-Condition ($reportText.Contains('Kind: `runtime_only -> materialized_graph`')) 'report must mention runtime plane transition kind'
Assert-Condition ($reportText.Contains('runtime_observe')) 'report must include runtime_observe artifact link'

$summary = [ordered]@{
    bundle_root = $resolvedBundleRoot
    runtime_case = $RuntimeOnlyCase
    graph_donor_case = $GraphDonorCase
    synthetic_bundle_root = $rightBundleRoot
    diff_json = $diffJsonPath
    inspect_json = $inspectJsonPath
    report_markdown = $reportMarkdownPath
    assertions = [ordered]@{
        runtime_only_to_graph_summary = $true
        no_structural_node_or_edge_diff = $true
        inspect_runtime_only_graph_null = $true
        inspect_show_edges_fails_clearly = $true
        report_mentions_runtime_observe = $true
    }
}
$summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryPath -Encoding utf8

Write-Host "[OK] runtime plane smoke passed"
Write-Host "[DIFF] $diffJsonPath"
Write-Host "[INSPECT] $inspectJsonPath"
Write-Host "[REPORT] $reportMarkdownPath"
Write-Host "[SUMMARY] $summaryPath"
