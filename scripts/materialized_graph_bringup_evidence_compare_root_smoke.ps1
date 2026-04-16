param(
    [string]$BundleRoot = 'out/materialized-graph-ci/bundle-current',
    [string]$ChangedCase = 'bringup-minimal-observe-demo',
    [string]$ExpectedUnchangedCase = 'bringup-block-observe-demo',
    [string]$PublishedCapability = 'io.uart1',
    [string]$OutputRoot = 'out/materialized-graph-bringup-evidence-compare-root-smoke',
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
        generator = 'tests.synthetic.bringup_evidence_compare_root_smoke'
        published_capabilities = @($publishedCapabilities)
        observed_capabilities = @($observedCapabilities)
        publish_state_summary = [ordered]@{
            missing = 0
            published = @($publishedCapabilities).Count
        }
        export_state_summary = [ordered]@{
            missing = 0
            detached = 0
            attached = if ($Published) { 1 } else { 0 }
        }
        recent_transitions = @($transitions)
    }
}

$resolvedBundleRoot = Resolve-FullPath $BundleRoot
$resolvedOutputRoot = Resolve-FullPath $OutputRoot
$leftBundleRoot = Join-Path $resolvedOutputRoot 'left-bundle'
$rightBundleRoot = Join-Path $resolvedOutputRoot 'right-bundle'
$diffJsonPath = Join-Path $resolvedOutputRoot 'bringup_evidence_compare_root.diff.json'
$reportOutputRoot = Join-Path $resolvedOutputRoot 'report'
$artifactReportOutputRoot = Join-Path $resolvedOutputRoot 'artifact-report'
$inspectJsonPath = Join-Path $resolvedOutputRoot 'bringup_evidence_compare_root.inspect.json'
$capListInspectJsonPath = Join-Path $resolvedOutputRoot 'bringup_evidence_compare_root.cap_list.inspect.json'
$rootSummaryInspectJsonPath = Join-Path $resolvedOutputRoot 'bringup_evidence_compare_root.summary.inspect.json'
$summaryPath = Join-Path $resolvedOutputRoot 'bringup_evidence_compare_root_smoke.summary.json'

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

$runtimeObserveRelativePath = [System.IO.Path]::Combine($ChangedCase, 'synthetic.runtime_observe.snapshot.json')
$leftIndexPath = Join-Path $leftBundleRoot 'index.json'
$rightIndexPath = Join-Path $rightBundleRoot 'index.json'
$leftIndex = Get-Content -LiteralPath $leftIndexPath -Raw -Encoding utf8 | ConvertFrom-Json
$rightIndex = Get-Content -LiteralPath $rightIndexPath -Raw -Encoding utf8 | ConvertFrom-Json
$leftCase = Get-CaseEntry -Cases @($leftIndex.cases) -CaseName $ChangedCase
$rightCase = Get-CaseEntry -Cases @($rightIndex.cases) -CaseName $ChangedCase
$unchangedCase = Get-CaseEntry -Cases @($rightIndex.cases) -CaseName $ExpectedUnchangedCase
Assert-Condition ($null -ne $leftCase) "case not found in left synthetic bundle: $ChangedCase"
Assert-Condition ($null -ne $rightCase) "case not found in right synthetic bundle: $ChangedCase"
Assert-Condition ($null -ne $unchangedCase) "case not found in synthetic bundle: $ExpectedUnchangedCase"

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

Write-Host "[SYNTH] case=$ChangedCase runtime_observe sidecar path=$runtimeObserveRelativePath publish=$PublishedCapability"

$diffData = Invoke-CommandJson -OutputPath $diffJsonPath -Command {
    & $diffScript -LeftBundleRoot $leftBundleRoot -RightBundleRoot $rightBundleRoot -IncludeUnchanged -AsJson
}
Assert-Condition ([string]$diffData.schema -eq 'materialized_graph.bundle_diff/v1') 'unexpected diff schema'
Assert-Condition ([int]$diffData.case_count -ge 2) 'compare root smoke expects at least two diff cases'

$changedCaseDiff = @(
    @($diffData.cases) |
        Where-Object { [string]$_.name -eq $ChangedCase } |
        Select-Object -First 1
) | Select-Object -First 1
$unchangedCaseDiff = @(
    @($diffData.cases) |
        Where-Object { [string]$_.name -eq $ExpectedUnchangedCase } |
        Select-Object -First 1
) | Select-Object -First 1
Assert-Condition ($null -ne $changedCaseDiff) "diff case missing: $ChangedCase"
Assert-Condition ($null -ne $unchangedCaseDiff) "diff case missing: $ExpectedUnchangedCase"
Assert-Condition ([string]$changedCaseDiff.status -eq 'unchanged') 'changed case must stay unchanged for same-path sidecar compare'
Assert-Condition ([string]$unchangedCaseDiff.status -eq 'unchanged') 'unchanged case must stay unchanged'

& $reportScript -LeftBundleRoot $leftBundleRoot -RightBundleRoot $rightBundleRoot -IncludeUnchanged -Format markdown -OutputDir $reportOutputRoot
if ($LASTEXITCODE -ne 0) {
    throw 'report generation failed'
}

$reportManifestPath = Join-Path $reportOutputRoot 'materialized_graph_bundle_diff_report.manifest.json'
Assert-Condition (Test-Path $reportManifestPath) "report manifest not found: $reportManifestPath"

& $artifactReportScript `
    -BundleRoot $rightBundleRoot `
    -OutputRoot $artifactReportOutputRoot `
    -Mode compare `
    -DiffJson $diffJsonPath `
    -ReportManifest $reportManifestPath
if ($LASTEXITCODE -ne 0) {
    throw 'artifact report compare export failed'
}

$artifactReports = @(Get-ChildItem -LiteralPath $artifactReportOutputRoot -Filter '*.artifact_report.json' -File)
Assert-Condition (@($artifactReports).Count -ge 2) 'compare root smoke expects at least two artifact reports'

$inspectResult = Invoke-CommandJson -OutputPath $inspectJsonPath -Command {
    & $inspectScript -ArtifactRoot $artifactReportOutputRoot -BringupEvidence -AsJson
}
Assert-Condition ([string]$inspectResult.query.kind -eq 'bringup_evidence') 'inspect bringup evidence query kind mismatch'
Assert-Condition ([string]$inspectResult.query.scope -eq 'artifact_root') 'inspect bringup evidence scope mismatch'
Assert-Condition ($null -ne $inspectResult.query.comparison) 'artifact_root bringup evidence must expose comparison payload'
Assert-Condition ($null -ne $inspectResult.query.comparison.bringup_evidence) 'artifact_root bringup evidence missing comparison.bringup_evidence'

$bringupComparison = $inspectResult.query.comparison.bringup_evidence
Assert-Condition ([int]$bringupComparison.changed_case_count -eq 1) 'bringup compare root changed_case_count must be 1'
Assert-Condition ((@($bringupComparison.changed_cases) -contains $ChangedCase)) 'bringup compare root changed_cases missing target case'
Assert-Condition ((@($bringupComparison.unchanged_cases) -contains $ExpectedUnchangedCase)) 'bringup compare root unchanged_cases missing baseline peer case'
Assert-Condition ((@($bringupComparison.summary_change_matrix | Where-Object { [string]$_.change -eq 'published_count:0->1' }).Count -eq 1)) 'bringup compare root summary_change_matrix missing published_count change'

$capabilityChangeEntry = @(
    @($bringupComparison.capability_change_matrix) |
        Where-Object { [string]$_.capability -eq $PublishedCapability } |
        Select-Object -First 1
) | Select-Object -First 1
Assert-Condition ($null -ne $capabilityChangeEntry) "bringup compare root missing capability change entry: $PublishedCapability"
Assert-Condition ((@($capabilityChangeEntry.change_kinds) -contains 'changed')) 'bringup compare root capability entry must include changed kind'
$capabilityCase = @(
    @($capabilityChangeEntry.cases) |
        Where-Object { [string]$_.case -eq $ChangedCase } |
        Select-Object -First 1
) | Select-Object -First 1
Assert-Condition ($null -ne $capabilityCase) "bringup compare root capability entry missing target case: $ChangedCase"
Assert-Condition ([string]$capabilityCase.right_export_state -eq 'attached') 'bringup compare root capability right_export_state must become attached'

$capListInspectResult = Invoke-CommandJson -OutputPath $capListInspectJsonPath -Command {
    & $inspectScript -ArtifactRoot $artifactReportOutputRoot -CapList -AsJson
}
Assert-Condition ([string]$capListInspectResult.query.kind -eq 'cap_list') 'inspect cap list query kind mismatch'
Assert-Condition ([string]$capListInspectResult.query.scope -eq 'artifact_root') 'inspect cap list scope mismatch'
Assert-Condition ($null -ne $capListInspectResult.query.comparison) 'artifact_root cap list must expose comparison payload'
Assert-Condition ([int]$capListInspectResult.query.comparison.bringup_compare_capability_count -eq 1) 'artifact_root cap list bringup compare capability count must be 1'
Assert-Condition ((@($capListInspectResult.query.comparison.bringup_compare_capabilities) -contains $PublishedCapability)) 'artifact_root cap list comparison missing published capability'
$capListCapabilityEntry = @(
    @($capListInspectResult.query.capabilities) |
        Where-Object { [string]$_.capability -eq $PublishedCapability } |
        Select-Object -First 1
) | Select-Object -First 1
Assert-Condition ($null -ne $capListCapabilityEntry) "artifact_root cap list missing capability entry: $PublishedCapability"
Assert-Condition ((@($capListCapabilityEntry.bringup_compare_cases) -contains $ChangedCase)) 'artifact_root cap list capability entry missing changed case'

$rootSummaryInspectResult = Invoke-CommandJson -OutputPath $rootSummaryInspectJsonPath -Command {
    & $inspectScript -ArtifactRoot $artifactReportOutputRoot -AsJson
}
Assert-Condition ([int]$rootSummaryInspectResult.comparison.compared_case_count -eq 2) 'artifact_root summary compared_case_count must be 2'
Assert-Condition ([int]$rootSummaryInspectResult.comparison.bringup_changed_case_count -eq 1) 'artifact_root summary bringup_changed_case_count must be 1'
Assert-Condition ((@($rootSummaryInspectResult.comparison.bringup_changed_cases) -contains $ChangedCase)) 'artifact_root summary missing bringup changed case'
Assert-Condition ($null -ne $rootSummaryInspectResult.comparison.capability_summary) 'artifact_root summary must expose capability summary'
Assert-Condition ([int]$rootSummaryInspectResult.comparison.capability_summary.bringup_compare_capability_count -eq 1) 'artifact_root summary bringup compare capability count must be 1'
Assert-Condition ((@($rootSummaryInspectResult.comparison.capability_summary.bringup_compare_capabilities) -contains $PublishedCapability)) 'artifact_root summary capability summary missing published capability'

$summary = [ordered]@{
    left_bundle_root = $leftBundleRoot
    right_bundle_root = $rightBundleRoot
    changed_case = $ChangedCase
    unchanged_case = $ExpectedUnchangedCase
    published_capability = $PublishedCapability
    diff_json = $diffJsonPath
    artifact_report_root = $artifactReportOutputRoot
    inspect_json = $inspectJsonPath
    cap_list_inspect_json = $capListInspectJsonPath
    root_summary_inspect_json = $rootSummaryInspectJsonPath
    assertions = [ordered]@{
        artifact_root_compare_present = $true
        changed_case_detected = $true
        unchanged_case_detected = $true
        capability_change_matrix_detected = $true
        cap_list_compare_detected = $true
        root_summary_compare_detected = $true
        root_summary_capability_compare_detected = $true
    }
}
$summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryPath -Encoding utf8

Write-Host "[OK] bringup evidence compare root smoke passed changed_case=$ChangedCase capability=$PublishedCapability"
Write-Host "[DIFF]    $diffJsonPath"
Write-Host "[REPORT]  $artifactReportOutputRoot"
Write-Host "[INSPECT] $inspectJsonPath"
Write-Host "[SUMMARY] $summaryPath"
