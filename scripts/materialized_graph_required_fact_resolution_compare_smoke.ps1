param(
    [string]$BundleRoot = 'out/materialized-graph-ci/bundle-current',
    [string]$Case = 'i2c-device-contract-facts-smoke',
    [string]$DonorCase = 'board-i2c-fact-composition-smoke',
    [string]$ExpectedFact = 'pinmux:pb8/pb9.af4',
    [string]$ExpectedProviderSource = 'platform.board.stm32_stub',
    [string]$OutputRoot = 'out/materialized-graph-required-fact-resolution-compare-smoke',
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

function Resolve-BundleArtifactPath {
    param(
        [string]$Root,
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ''
    }

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $Root $Path))
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

function Get-RequiredFactResolutionChange {
    param(
        [object[]]$Changes,
        [string]$FactName
    )

    return @(
        $Changes |
            Where-Object { [string]$_.fact -eq $FactName } |
            Select-Object -First 1
    ) | Select-Object -First 1
}

$resolvedBundleRoot = Resolve-FullPath $BundleRoot
$resolvedOutputRoot = Resolve-FullPath $OutputRoot
$rightBundleRoot = Join-Path $resolvedOutputRoot 'right-bundle'
$diffJsonPath = Join-Path $resolvedOutputRoot 'required_fact_resolution_compare.diff.json'
$reportOutputRoot = Join-Path $resolvedOutputRoot 'report'
$artifactReportOutputRoot = Join-Path $resolvedOutputRoot 'artifact-report'
$inspectJsonPath = Join-Path $resolvedOutputRoot 'required_fact_resolution_compare.inspect.json'
$rootInspectJsonPath = Join-Path $resolvedOutputRoot 'required_fact_resolution_compare.root.inspect.json'
$whyInspectJsonPath = Join-Path $resolvedOutputRoot 'required_fact_resolution_compare.why.inspect.json'
$graphPathInspectJsonPath = Join-Path $resolvedOutputRoot 'required_fact_resolution_compare.graph_path.inspect.json'
$capListInspectJsonPath = Join-Path $resolvedOutputRoot 'required_fact_resolution_compare.cap_list.inspect.json'
$rootCapListInspectJsonPath = Join-Path $resolvedOutputRoot 'required_fact_resolution_compare.root.cap_list.inspect.json'
$defaultOverviewInspectJsonPath = Join-Path $resolvedOutputRoot 'required_fact_resolution_compare.default_overview.inspect.json'
$summaryPath = Join-Path $resolvedOutputRoot 'required_fact_resolution_compare_smoke.summary.json'

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

Copy-Item -LiteralPath $resolvedBundleRoot -Destination $rightBundleRoot -Recurse -Force

$leftIndexPath = Join-Path $resolvedBundleRoot 'index.json'
$rightIndexPath = Join-Path $rightBundleRoot 'index.json'
$rightIndex = Get-Content -LiteralPath $rightIndexPath -Raw -Encoding utf8 | ConvertFrom-Json
$rightCase = Get-CaseEntry -Cases @($rightIndex.cases) -CaseName $Case
$donorCaseEntry = Get-CaseEntry -Cases @($rightIndex.cases) -CaseName $DonorCase
Assert-Condition ($null -ne $rightCase) "case not found in synthetic bundle: $Case"
Assert-Condition ($null -ne $donorCaseEntry) "donor case not found in synthetic bundle: $DonorCase"
Assert-Condition (-not [string]::IsNullOrWhiteSpace([string]($donorCaseEntry.fact_evidence))) "donor case has no fact_evidence: $DonorCase"

$rightCase.fact_evidence = [string]($donorCaseEntry.fact_evidence)
if ($null -ne $rightCase.PSObject.Properties['declared_facts']) {
    $rightCase.declared_facts = @($donorCaseEntry.declared_facts)
}
if ($null -ne $rightCase.PSObject.Properties['required_facts']) {
    $rightCase.required_facts = @($donorCaseEntry.required_facts)
}
if ($null -ne $rightCase.PSObject.Properties['audit_provided_facts']) {
    $rightCase.audit_provided_facts = @($donorCaseEntry.audit_provided_facts)
}
$rightIndex | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $rightIndexPath -Encoding utf8

Write-Host "[SYNTH] case=$Case donor_fact_evidence=$DonorCase expected_fact=$ExpectedFact"

$diffData = Invoke-CommandJson -OutputPath $diffJsonPath -Command {
    & $diffScript -LeftIndex $leftIndexPath -RightIndex $rightIndexPath -Case $Case -IncludeUnchanged -AsJson
}
Assert-Condition ([string]$diffData.schema -eq 'materialized_graph.bundle_diff/v1') 'unexpected diff schema'
Assert-Condition ([int]$diffData.case_count -eq 1) 'compare smoke expects exactly one diff case'

& $reportScript -LeftBundleRoot $resolvedBundleRoot -RightBundleRoot $rightBundleRoot -Case $Case -IncludeUnchanged -Format markdown -OutputDir $reportOutputRoot
if ($LASTEXITCODE -ne 0) {
    throw 'report generation failed'
}

$reportManifestPath = Join-Path $reportOutputRoot 'materialized_graph_bundle_diff_report.manifest.json'
Assert-Condition (Test-Path $reportManifestPath) "report manifest not found: $reportManifestPath"

& $artifactReportScript `
    -BundleRoot $rightBundleRoot `
    -Case @($Case, $DonorCase) `
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
Assert-Condition ($null -ne $artifactReport.comparison.fact_resolution) 'artifact report comparison.fact_resolution is missing'
Assert-Condition ([bool]$artifactReport.comparison.fact_resolution.changed) 'fact resolution comparison must be changed'

$factChange = Get-RequiredFactResolutionChange `
    -Changes @($artifactReport.comparison.fact_resolution.required_fact_resolution_changes) `
    -FactName $ExpectedFact
Assert-Condition ($null -ne $factChange) "required fact resolution change missing: $ExpectedFact"
Assert-Condition ([string]$factChange.change_kind -eq 'state_changed') "required fact change kind mismatch: $([string]$factChange.change_kind)"
Assert-Condition ([string]$factChange.left_state -eq 'missing') 'left required fact state must be missing'
Assert-Condition ([string]$factChange.right_state -eq 'satisfied') 'right required fact state must be satisfied'
Assert-Condition ([int]$factChange.left_provider_count -eq 0) 'left provider_count must be 0'
Assert-Condition ([int]$factChange.right_provider_count -gt 0) 'right provider_count must be greater than 0'
Assert-Condition ((@($factChange.right_providers | ForEach-Object { [string]$_.source }) -contains $ExpectedProviderSource)) "expected provider source missing from right providers: $ExpectedProviderSource"

$capListInspectResult = Invoke-CommandJson -OutputPath $capListInspectJsonPath -Command {
    & $inspectScript -ArtifactRoot $artifactReportOutputRoot -Case $Case -CapList -AsJson
}
Assert-Condition ([string]$capListInspectResult.query.kind -eq 'cap_list') 'inspect cap list query kind mismatch'
Assert-Condition ([string]$capListInspectResult.query.scope -eq 'report') 'inspect cap list query scope mismatch'
Assert-Condition ($null -ne $capListInspectResult.query.comparison) 'inspect cap list missing comparison payload'
Assert-Condition ([int]$capListInspectResult.query.comparison.fact_resolution_compare_capability_count -ge 1) 'inspect cap list fact resolution compare count must be positive'
Assert-Condition ((@($capListInspectResult.query.comparison.fact_resolution_compare_capabilities) -contains $ExpectedFact)) "inspect cap list missing fact resolution capability: $ExpectedFact"
Assert-Condition ((@($capListInspectResult.query.comparison.required_facts_changed) -contains $ExpectedFact)) "inspect cap list summary missing changed required fact: $ExpectedFact"
$capListEntry = @(
    @($capListInspectResult.query.capabilities) |
        Where-Object { [string]$_.capability -eq $ExpectedFact } |
        Select-Object -First 1
) | Select-Object -First 1
Assert-Condition ($null -ne $capListEntry) "inspect cap list entry missing: $ExpectedFact"
Assert-Condition ([bool]$capListEntry.comparison.fact_resolution_changed) 'inspect cap list entry must mark fact resolution changed'
Assert-Condition ((@($capListEntry.comparison.required_fact_resolution_change_kinds) -contains 'state_changed')) 'inspect cap list entry missing state_changed required fact kind'
Assert-Condition ((@($capListEntry.comparison.required_facts_changed) -contains $ExpectedFact)) "inspect cap list entry missing changed required fact: $ExpectedFact"

$inspectResult = Invoke-CommandJson -OutputPath $inspectJsonPath -Command {
    & $inspectScript -ArtifactRoot $artifactReportOutputRoot -Case $Case -ResourceSummary -AsJson
}
Assert-Condition ($null -ne $inspectResult.query.comparison.fact_resolution) 'inspect resource summary missing comparison.fact_resolution'
$inspectFactChange = Get-RequiredFactResolutionChange `
    -Changes @($inspectResult.query.comparison.fact_resolution.required_fact_resolution_changes) `
    -FactName $ExpectedFact
Assert-Condition ($null -ne $inspectFactChange) "inspect required fact resolution change missing: $ExpectedFact"
Assert-Condition ([string]$inspectFactChange.left_state -eq 'missing') 'inspect left required fact state must be missing'
Assert-Condition ([string]$inspectFactChange.right_state -eq 'satisfied') 'inspect right required fact state must be satisfied'

$whyInspectResult = Invoke-CommandJson -OutputPath $whyInspectJsonPath -Command {
    & $inspectScript -ArtifactRoot $artifactReportOutputRoot -Case $Case -WhyCapability $ExpectedFact -AsJson
}
Assert-Condition ([string]$whyInspectResult.query.kind -eq 'why_capability') 'inspect why query kind mismatch'
Assert-Condition ([string]$whyInspectResult.query.scope -eq 'report') 'inspect why query scope mismatch'
Assert-Condition ($null -ne $whyInspectResult.query.comparison) 'inspect why missing comparison payload'
Assert-Condition ([bool]$whyInspectResult.query.comparison.resource_changed) 'inspect why comparison must mark resource changed'
Assert-Condition ([bool]$whyInspectResult.query.comparison.fact_resolution_changed) 'inspect why comparison must mark fact resolution changed'
Assert-Condition ((@($whyInspectResult.query.comparison.resource_change_kinds) -contains 'required_fact_state_changed')) 'inspect why comparison missing required_fact_state_changed kind'
Assert-Condition ((@($whyInspectResult.query.comparison.required_fact_resolution_change_kinds) -contains 'state_changed')) 'inspect why comparison missing state_changed required fact kind'
Assert-Condition ((@($whyInspectResult.query.comparison.required_facts_changed) -contains $ExpectedFact)) "inspect why comparison missing changed required fact: $ExpectedFact"
$whyFactChange = Get-RequiredFactResolutionChange `
    -Changes @($whyInspectResult.query.comparison.fact_resolution.required_fact_resolution_changes) `
    -FactName $ExpectedFact
Assert-Condition ($null -ne $whyFactChange) "inspect why required fact resolution change missing: $ExpectedFact"
Assert-Condition ([string]$whyFactChange.left_state -eq 'missing') 'inspect why left required fact state must be missing'
Assert-Condition ([string]$whyFactChange.right_state -eq 'satisfied') 'inspect why right required fact state must be satisfied'
Assert-Condition (@(
    @($whyInspectResult.query.reasons) |
        Where-Object { [string]$_ -like "*compare required_fact $ExpectedFact changed: state_changed*" }
).Count -eq 1) 'inspect why reasons missing required fact resolution explanation'

$graphPathInspectResult = Invoke-CommandJson -OutputPath $graphPathInspectJsonPath -Command {
    & $inspectScript -ArtifactRoot $artifactReportOutputRoot -Case $Case -GraphPath $ExpectedFact -AsJson
}
Assert-Condition ([string]$graphPathInspectResult.query.kind -eq 'graph_path') 'inspect graph path query kind mismatch'
Assert-Condition ([string]$graphPathInspectResult.query.scope -eq 'report') 'inspect graph path scope mismatch'
Assert-Condition ([string]$graphPathInspectResult.query.result.capability -eq $ExpectedFact) 'inspect graph path capability mismatch'
Assert-Condition ($null -ne $graphPathInspectResult.query.result.comparison) 'inspect graph path missing comparison payload'
Assert-Condition ([bool]$graphPathInspectResult.query.result.comparison.fact_resolution_changed) 'inspect graph path comparison must mark fact resolution changed'
Assert-Condition ((@($graphPathInspectResult.query.result.comparison.required_facts_changed) -contains $ExpectedFact)) "inspect graph path comparison missing changed required fact: $ExpectedFact"

$rootInspectResult = Invoke-CommandJson -OutputPath $rootInspectJsonPath -Command {
    & $inspectScript -ArtifactRoot $artifactReportOutputRoot -ResourceSummary -AsJson
}
Assert-Condition ($null -ne $rootInspectResult.query.comparison.fact_resolution) 'artifact_root resource summary missing comparison.fact_resolution'
$rootComparison = $rootInspectResult.query.comparison.fact_resolution
Assert-Condition ([string]$rootComparison.kind -eq 'fact_resolution_summary/v0') 'artifact_root fact resolution comparison kind mismatch'
Assert-Condition ([string]$rootComparison.mode -eq 'comparison') 'artifact_root fact resolution comparison mode mismatch'
Assert-Condition ([int]$rootComparison.required_fact_resolution_change_count -ge 1) 'artifact_root required fact resolution change count must be positive'

$matrixEntry = Get-RequiredFactResolutionChange `
    -Changes @($rootComparison.required_fact_resolution_change_matrix) `
    -FactName $ExpectedFact
Assert-Condition ($null -ne $matrixEntry) "artifact_root required fact resolution change matrix missing: $ExpectedFact"
Assert-Condition ((@($matrixEntry.change_kinds) -contains 'state_changed')) 'artifact_root matrix missing state_changed kind'
$matrixCase = @(
    @($matrixEntry.cases) |
        Where-Object { [string]$_.case -eq $Case } |
        Select-Object -First 1
) | Select-Object -First 1
Assert-Condition ($null -ne $matrixCase) "artifact_root matrix missing case: $Case"
Assert-Condition ([string]$matrixCase.left_state -eq 'missing') 'artifact_root matrix left state must be missing'
Assert-Condition ([string]$matrixCase.right_state -eq 'satisfied') 'artifact_root matrix right state must be satisfied'

$rootCapListInspectResult = Invoke-CommandJson -OutputPath $rootCapListInspectJsonPath -Command {
    & $inspectScript -ArtifactRoot $artifactReportOutputRoot -CapList -AsJson
}
Assert-Condition ([string]$rootCapListInspectResult.query.kind -eq 'cap_list') 'artifact_root cap list query kind mismatch'
Assert-Condition ([string]$rootCapListInspectResult.query.scope -eq 'artifact_root') 'artifact_root cap list query scope mismatch'
Assert-Condition ($null -ne $rootCapListInspectResult.query.comparison) 'artifact_root cap list missing comparison payload'
Assert-Condition ([int]$rootCapListInspectResult.query.comparison.fact_resolution_compare_capability_count -ge 1) 'artifact_root cap list fact resolution compare count must be positive'
Assert-Condition ((@($rootCapListInspectResult.query.comparison.fact_resolution_compare_capabilities) -contains $ExpectedFact)) "artifact_root cap list missing fact resolution capability: $ExpectedFact"
Assert-Condition ((@($rootCapListInspectResult.query.comparison.required_facts_changed) -contains $ExpectedFact)) "artifact_root cap list summary missing changed required fact: $ExpectedFact"
$rootCapListEntry = @(
    @($rootCapListInspectResult.query.capabilities) |
        Where-Object { [string]$_.capability -eq $ExpectedFact } |
        Select-Object -First 1
) | Select-Object -First 1
Assert-Condition ($null -ne $rootCapListEntry) "artifact_root cap list entry missing: $ExpectedFact"
Assert-Condition ([bool]$rootCapListEntry.fact_resolution_compare) 'artifact_root cap list entry must mark fact resolution compare'
Assert-Condition ((@($rootCapListEntry.fact_resolution_compare_cases) -contains $Case)) "artifact_root cap list entry missing fact resolution compare case: $Case"
Assert-Condition ((@($rootCapListEntry.required_fact_resolution_change_kinds) -contains 'state_changed')) 'artifact_root cap list entry missing state_changed required fact kind'
Assert-Condition ((@($rootCapListEntry.required_facts_changed) -contains $ExpectedFact)) "artifact_root cap list entry missing changed required fact: $ExpectedFact"

$defaultOverviewInspectResult = Invoke-CommandJson -OutputPath $defaultOverviewInspectJsonPath -Command {
    & $inspectScript -ArtifactRoot $artifactReportOutputRoot -AsJson
}
$defaultOverviewCase = @(
    @($defaultOverviewInspectResult.cases) |
        Where-Object { [string]$_.Case -eq $Case } |
        Select-Object -First 1
) | Select-Object -First 1
Assert-Condition ($null -ne $defaultOverviewCase) "default overview case summary missing: $Case"
Assert-Condition ([int]$defaultOverviewCase.FactCmp -ge 1) 'default overview case summary FactCmp must be positive'
Assert-Condition ($null -ne $defaultOverviewInspectResult.comparison) 'default overview missing comparison payload'
Assert-Condition ([int]$defaultOverviewInspectResult.comparison.fact_resolution_changed_case_count -ge 1) 'default overview fact resolution changed case count must be positive'
Assert-Condition ([int]$defaultOverviewInspectResult.comparison.capability_summary.fact_resolution_compare_capability_count -ge 1) 'default overview capability summary fact resolution count must be positive'
Assert-Condition ($null -ne $defaultOverviewInspectResult.comparison.drift_headline) 'default overview missing drift headline'
Assert-Condition ((@($defaultOverviewInspectResult.comparison.drift_headline.changed_dimensions) -contains 'fact_resolution')) 'default overview drift headline missing fact_resolution dimension'
Assert-Condition ([int]$defaultOverviewInspectResult.comparison.drift_headline.dimension_counts.fact_resolution -ge 1) 'default overview drift headline fact_resolution count must be positive'
Assert-Condition ([string]$defaultOverviewInspectResult.comparison.drift_headline.text -like '*fact_resolution:*') 'default overview drift headline text missing fact_resolution segment'

$summary = [ordered]@{
    bundle_root = $resolvedBundleRoot
    artifact_report = $artifactReportPath
    inspect_json = $inspectJsonPath
    root_inspect_json = $rootInspectJsonPath
    why_inspect_json = $whyInspectJsonPath
    graph_path_inspect_json = $graphPathInspectJsonPath
    cap_list_inspect_json = $capListInspectJsonPath
    root_cap_list_inspect_json = $rootCapListInspectJsonPath
    default_overview_inspect_json = $defaultOverviewInspectJsonPath
    case = $Case
    donor_case = $DonorCase
    expected_fact = $ExpectedFact
    expected_provider_source = $ExpectedProviderSource
    assertions = [ordered]@{
        comparison_fact_resolution_present = $true
        required_fact_changed_missing_to_satisfied = $true
        expected_provider_source_present = $true
        why_explains_required_fact_resolution_change = $true
        graph_path_exposes_required_fact_resolution_change = $true
        artifact_root_change_matrix_present = $true
        cap_list_exposes_required_fact_resolution_change = $true
        artifact_root_cap_list_exposes_required_fact_resolution_change = $true
        default_overview_exposes_factcmp = $true
        default_overview_exposes_drift_headline = $true
    }
}

$summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $summaryPath -Encoding utf8

Write-Host "[OK] required fact resolution compare smoke passed fact=$ExpectedFact"
Write-Host "[REPORT]  $artifactReportPath"
Write-Host "[INSPECT] $inspectJsonPath"
Write-Host "[SUMMARY] $summaryPath"
