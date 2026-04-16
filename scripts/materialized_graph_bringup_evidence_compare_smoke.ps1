param(
    [string]$BundleRoot = 'out/materialized-graph-ci/bundle-current',
    [string]$Case = 'bringup-minimal-observe-demo',
    [string]$PublishedCapability = 'io.uart1',
    [string]$OutputRoot = 'out/materialized-graph-bringup-evidence-compare-smoke',
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

function Get-CaseEntry {
    param(
        [object[]]$Cases,
        [string]$CaseName
    )

    return @(
        $Cases |
            Where-Object { [string]$_.name -eq $CaseName } |
            Select-Object -First 1
    ) | Select-Object -First 1
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

function Invoke-CommandJson {
    param(
        [scriptblock]$Command,
        [string]$OutputPath
    )

    $jsonText = (& $Command | Out-String)
    $jsonText | Set-Content -LiteralPath $OutputPath -Encoding utf8
    return ($jsonText | ConvertFrom-Json)
}

function New-RuntimeObserveSnapshot {
    param(
        [string]$CapabilityName,
        [bool]$Published
    )

    $publishedCapabilities = if ($Published) { @($CapabilityName) } else { @() }
    $observedCapabilities = if ($Published) { @($CapabilityName) } else { @() }
    $transitions = if ($Published) {
        @(
            [ordered]@{
                capability = $CapabilityName
                action = 'ensure_exported'
                before = 'missing'
                after = 'detached'
            },
            [ordered]@{
                capability = $CapabilityName
                action = 'attach'
                before = 'detached'
                after = 'attached'
            }
        )
    } else {
        @()
    }

    return [ordered]@{
        schema = 'system_compiler.runtime_observe_snapshot/v0'
        generated_at_utc = (Get-Date).ToUniversalTime().ToString('o')
        generator = 'tests.synthetic.bringup_evidence_compare_smoke'
        published_capabilities = @($publishedCapabilities)
        observed_capabilities = @($observedCapabilities)
        publish_state_summary = [ordered]@{
            missing = 0
            published = @($publishedCapabilities).Count
        }
        export_state_summary = [ordered]@{
            missing = 0
            detached = if ($Published) { 0 } else { 0 }
            attached = if ($Published) { 1 } else { 0 }
        }
        recent_transitions = @($transitions)
    }
}

$resolvedBundleRoot = Resolve-FullPath $BundleRoot
$resolvedOutputRoot = Resolve-FullPath $OutputRoot
$leftBundleRoot = Join-Path $resolvedOutputRoot 'left-bundle'
$rightBundleRoot = Join-Path $resolvedOutputRoot 'right-bundle'
$diffJsonPath = Join-Path $resolvedOutputRoot 'bringup_evidence_compare.diff.json'
$reportOutputRoot = Join-Path $resolvedOutputRoot 'report'
$artifactReportOutputRoot = Join-Path $resolvedOutputRoot 'artifact-report'
$inspectJsonPath = Join-Path $resolvedOutputRoot 'bringup_evidence_compare.inspect.json'
$summaryInspectJsonPath = Join-Path $resolvedOutputRoot 'bringup_evidence_compare.summary.inspect.json'
$summaryPath = Join-Path $resolvedOutputRoot 'bringup_evidence_compare_smoke.summary.json'

$diffScript = Join-Path $PSScriptRoot 'diff_materialized_graph_bundle.ps1'
$reportScript = Join-Path $PSScriptRoot 'report_materialized_graph_bundle.ps1'
$artifactReportScript = Join-Path $PSScriptRoot 'export_system_compiler_artifact_report.ps1'
$inspectScript = Join-Path $PSScriptRoot 'inspect_system_compiler_artifact_report.ps1'

foreach ($requiredPath in @($resolvedBundleRoot, $diffScript, $reportScript, $artifactReportScript, $inspectScript)) {
    if (-not (Test-Path $requiredPath)) {
        throw "required path not found: $requiredPath"
    }
}

if (-not $KeepOutput) {
    Remove-PathIfExists -Path $resolvedOutputRoot
}
Ensure-Directory -Path $resolvedOutputRoot
Ensure-Directory -Path $reportOutputRoot

Copy-Item -LiteralPath $resolvedBundleRoot -Destination $leftBundleRoot -Recurse -Force
Copy-Item -LiteralPath $resolvedBundleRoot -Destination $rightBundleRoot -Recurse -Force

$runtimeObserveRelativePath = [System.IO.Path]::Combine($Case, 'synthetic.runtime_observe.snapshot.json')
$leftIndexPath = Join-Path $leftBundleRoot 'index.json'
$rightIndexPath = Join-Path $rightBundleRoot 'index.json'
$leftIndex = Get-Content -LiteralPath $leftIndexPath -Raw -Encoding utf8 | ConvertFrom-Json
$rightIndex = Get-Content -LiteralPath $rightIndexPath -Raw -Encoding utf8 | ConvertFrom-Json
$leftCase = Get-CaseEntry -Cases @($leftIndex.cases) -CaseName $Case
$rightCase = Get-CaseEntry -Cases @($rightIndex.cases) -CaseName $Case
Assert-Condition ($null -ne $leftCase) "case not found in left synthetic bundle: $Case"
Assert-Condition ($null -ne $rightCase) "case not found in right synthetic bundle: $Case"

Set-ObjectProperty -Object $leftCase -Name 'runtime_observe' -Value $runtimeObserveRelativePath
Set-ObjectProperty -Object $rightCase -Name 'runtime_observe' -Value $runtimeObserveRelativePath
$leftIndex | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $leftIndexPath -Encoding utf8
$rightIndex | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $rightIndexPath -Encoding utf8

$leftRuntimeObservePath = Join-Path $leftBundleRoot $runtimeObserveRelativePath
$rightRuntimeObservePath = Join-Path $rightBundleRoot $runtimeObserveRelativePath
Ensure-Directory -Path (Split-Path -Parent $leftRuntimeObservePath)
Ensure-Directory -Path (Split-Path -Parent $rightRuntimeObservePath)

(New-RuntimeObserveSnapshot -CapabilityName $PublishedCapability -Published $false) |
    ConvertTo-Json -Depth 8 |
    Set-Content -LiteralPath $leftRuntimeObservePath -Encoding utf8
(New-RuntimeObserveSnapshot -CapabilityName $PublishedCapability -Published $true) |
    ConvertTo-Json -Depth 8 |
    Set-Content -LiteralPath $rightRuntimeObservePath -Encoding utf8

Write-Host "[SYNTH] case=$Case runtime_observe sidecar path=$runtimeObserveRelativePath publish=$PublishedCapability"

$diffData = Invoke-CommandJson -OutputPath $diffJsonPath -Command {
    & $diffScript -LeftBundleRoot $leftBundleRoot -RightBundleRoot $rightBundleRoot -Case $Case -IncludeUnchanged -AsJson
}
Assert-Condition ([string]$diffData.schema -eq 'materialized_graph.bundle_diff/v1') 'unexpected diff schema'
Assert-Condition ([int]$diffData.case_count -eq 1) 'compare smoke expects exactly one diff case'

$caseDiff = @($diffData.cases)[0]
Assert-Condition ([string]$caseDiff.name -eq $Case) 'unexpected diff case name'
Assert-Condition ([string]$caseDiff.status -eq 'unchanged') 'bringup evidence compare smoke expects sidecar-only diff to stay unchanged'
Assert-Condition (@($caseDiff.summary_changes).Count -eq 0) 'summary_changes must stay empty for same-path sidecar compare'
Assert-Condition (@($caseDiff.metadata_changes).Count -eq 0) 'metadata_changes must stay empty for same-path sidecar compare'

& $reportScript -LeftBundleRoot $leftBundleRoot -RightBundleRoot $rightBundleRoot -Case $Case -IncludeUnchanged -Format markdown -OutputDir $reportOutputRoot
if ($LASTEXITCODE -ne 0) {
    throw 'report generation failed'
}

$reportManifestPath = Join-Path $reportOutputRoot 'materialized_graph_bundle_diff_report.manifest.json'
Assert-Condition (Test-Path $reportManifestPath) "report manifest not found: $reportManifestPath"

& $artifactReportScript `
    -BundleRoot $rightBundleRoot `
    -Case $Case `
    -OutputRoot $artifactReportOutputRoot `
    -Mode compare `
    -DiffJson $diffJsonPath `
    -ReportManifest $reportManifestPath
if ($LASTEXITCODE -ne 0) {
    throw 'artifact report compare export failed'
}

$artifactReportPath = Join-Path $artifactReportOutputRoot ($Case + '.artifact_report.json')
Assert-Condition (Test-Path $artifactReportPath) "artifact report not found: $artifactReportPath"
$artifactReport = Get-Content -LiteralPath $artifactReportPath -Raw -Encoding utf8 | ConvertFrom-Json

Assert-Condition ([string]$artifactReport.mode -eq 'compare') 'artifact report mode must be compare'
Assert-Condition ($null -ne $artifactReport.comparison) 'artifact report comparison is missing'
Assert-Condition ([string]$artifactReport.comparison.status -eq 'unchanged') 'artifact report comparison.status must preserve unchanged sidecar-only compare'
Assert-Condition (@($artifactReport.comparison.summary_changes).Count -eq 0) 'artifact report comparison.summary_changes must stay empty'
Assert-Condition (@($artifactReport.comparison.metadata_changes).Count -eq 0) 'artifact report comparison.metadata_changes must stay empty'
Assert-Condition ($null -ne $artifactReport.comparison.bringup_evidence) 'artifact report comparison.bringup_evidence is missing'
Assert-Condition ([bool]$artifactReport.comparison.bringup_evidence.changed) 'bringup evidence comparison must be marked changed'

$bringupEvidenceComparison = $artifactReport.comparison.bringup_evidence
Assert-Condition ([int]$bringupEvidenceComparison.left.published_count -eq 0) 'baseline published_count must stay 0'
Assert-Condition ([int]$bringupEvidenceComparison.right.published_count -eq 1) 'candidate published_count must become 1'
Assert-Condition ((@($bringupEvidenceComparison.summary_changes) -contains 'published_count:0->1')) 'bringup evidence summary_changes missing published_count change'
Assert-Condition ((@($bringupEvidenceComparison.published_capability_changes.added) -contains $PublishedCapability)) 'published_capability_changes.added missing published capability'

$capabilityChange = @(
    @($bringupEvidenceComparison.capability_changes) |
        Where-Object { [string]$_.capability -eq $PublishedCapability } |
        Select-Object -First 1
) | Select-Object -First 1
Assert-Condition ($null -ne $capabilityChange) "bringup evidence comparison missing capability change: $PublishedCapability"
Assert-Condition ([string]$capabilityChange.change_kind -eq 'changed') 'existing graph capability must be marked changed'
Assert-Condition (-not [bool]$capabilityChange.left_published) 'left_published must stay false'
Assert-Condition ([bool]$capabilityChange.right_published) 'right_published must become true'
Assert-Condition ([string]$capabilityChange.left_publish_state -eq 'missing') 'left_publish_state must stay missing'
Assert-Condition ([string]$capabilityChange.right_publish_state -eq 'published') 'right_publish_state must become published'
Assert-Condition ($null -eq $capabilityChange.left_export_state) 'left_export_state must stay null'
Assert-Condition ([string]$capabilityChange.right_export_state -eq 'attached') 'right_export_state must become attached'

$inspectResult = Invoke-CommandJson -OutputPath $inspectJsonPath -Command {
    & $inspectScript -ArtifactRoot $artifactReportOutputRoot -Case $Case -BringupEvidence -AsJson
}
Assert-Condition ([string]$inspectResult.query.kind -eq 'bringup_evidence') 'inspect bringup evidence query kind mismatch'
Assert-Condition ([string]$inspectResult.query.scope -eq 'report') 'inspect bringup evidence scope mismatch'
Assert-Condition ($null -ne $inspectResult.query.comparison) 'inspect bringup evidence must expose comparison payload'
Assert-Condition ($null -ne $inspectResult.query.comparison.bringup_evidence) 'inspect bringup evidence missing comparison.bringup_evidence'
Assert-Condition ([bool]$inspectResult.query.comparison.bringup_evidence.changed) 'inspect bringup evidence comparison must be changed'

$summaryInspectResult = Invoke-CommandJson -OutputPath $summaryInspectJsonPath -Command {
    & $inspectScript -ArtifactRoot $artifactReportOutputRoot -Case $Case -AsJson
}
Assert-Condition ($null -ne $summaryInspectResult.comparison) 'inspect default summary must expose comparison payload'
Assert-Condition ($null -ne $summaryInspectResult.comparison.capability_summary) 'inspect default summary must expose capability summary'
Assert-Condition ([int]$summaryInspectResult.comparison.capability_summary.bringup_compare_capability_count -eq 1) 'inspect default summary bringup compare capability count must be 1'
Assert-Condition ((@($summaryInspectResult.comparison.capability_summary.bringup_compare_capabilities) -contains $PublishedCapability)) 'inspect default summary capability summary missing published capability'

$summary = [ordered]@{
    left_bundle_root = $leftBundleRoot
    right_bundle_root = $rightBundleRoot
    case = $Case
    published_capability = $PublishedCapability
    diff_json = $diffJsonPath
    artifact_report = $artifactReportPath
    inspect_json = $inspectJsonPath
    summary_inspect_json = $summaryInspectJsonPath
    assertions = [ordered]@{
        sidecar_only_diff_preserved = $true
        comparison_bringup_evidence_present = $true
        published_capability_change_detected = $true
        inspect_bringup_evidence_exposes_compare = $true
        inspect_default_summary_exposes_capability_compare = $true
    }
}
$summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryPath -Encoding utf8

Write-Host "[OK] bringup evidence compare smoke passed case=$Case capability=$PublishedCapability"
Write-Host "[DIFF]    $diffJsonPath"
Write-Host "[REPORT]  $artifactReportPath"
Write-Host "[INSPECT] $inspectJsonPath"
Write-Host "[SUMMARY] $summaryPath"
