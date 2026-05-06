param(
    [string]$Case = 'i2c-whoami-probe-evidence-smoke',
    [string]$ExpectedFact = 'i2c.probe.board_real',
    [string]$ExpectedProviderSource = 'board.bringup',
    [string]$OutputRoot = 'out/i2c-whoami-probe-evidence-compare-smoke',
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

function Add-UniqueString {
    param(
        [object[]]$Values,
        [string]$Value
    )

    return @(
        @($Values) +
        @($Value) |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [string]$_ } |
            Sort-Object -Unique
    )
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

function Set-WhoAmIProbeBoardEvidenceProvided {
    param(
        [string]$FactEvidencePath,
        [string]$FactName,
        [string]$ProviderSource
    )

    $evidence = Get-Content -LiteralPath $FactEvidencePath -Raw -Encoding utf8 | ConvertFrom-Json
    Assert-Condition ([string]$evidence.schema -eq 'system_compiler.fact_evidence/v0') 'unexpected fact evidence schema'

    $requiredFacts = @($evidence.facts.required_facts)
    Assert-Condition ((@($requiredFacts) -contains $FactName)) "expected required fact missing from sidecar: $FactName"

    $evidence.facts.audit_provided_facts = Add-UniqueString -Values @($evidence.facts.audit_provided_facts) -Value $FactName

    $targetRawFact = @(
        @($evidence.raw_facts) |
            Where-Object { [string]$_.name -eq $FactName } |
            Select-Object -First 1
    ) | Select-Object -First 1
    Assert-Condition ($null -ne $targetRawFact) "expected raw fact missing from sidecar: $FactName"

    $targetRawFact.state = 'provided'
    $targetRawFact.source = $ProviderSource
    $targetRawFact.role = 'real_board_probe_evidence'

    if ($null -ne $evidence.PSObject.Properties['summary'] -and $null -ne $evidence.summary) {
        if ($null -ne $evidence.summary.PSObject.Properties['provided_count']) {
            $evidence.summary.provided_count = [int]$evidence.summary.provided_count + 1
        }
        if ($null -ne $evidence.summary.PSObject.Properties['missing_count'] -and
            [int]$evidence.summary.missing_count -gt 0) {
            $evidence.summary.missing_count = [int]$evidence.summary.missing_count - 1
        }
    }

    $evidence |
        ConvertTo-Json -Depth 12 |
        Set-Content -LiteralPath $FactEvidencePath -Encoding utf8
}

$resolvedOutputRoot = Resolve-FullPath $OutputRoot
$leftBundleRoot = Join-Path $resolvedOutputRoot 'left-bundle'
$rightBundleRoot = Join-Path $resolvedOutputRoot 'right-bundle'
$diffJsonPath = Join-Path $resolvedOutputRoot 'i2c_whoami_probe_evidence_compare.diff.json'
$reportOutputRoot = Join-Path $resolvedOutputRoot 'report'
$artifactReportOutputRoot = Join-Path $resolvedOutputRoot 'artifact-report'
$inspectJsonPath = Join-Path $resolvedOutputRoot 'i2c_whoami_probe_evidence_compare.inspect.json'
$whyInspectJsonPath = Join-Path $resolvedOutputRoot 'i2c_whoami_probe_evidence_compare.why.inspect.json'
$capListInspectJsonPath = Join-Path $resolvedOutputRoot 'i2c_whoami_probe_evidence_compare.cap_list.inspect.json'
$rootInspectJsonPath = Join-Path $resolvedOutputRoot 'i2c_whoami_probe_evidence_compare.root.inspect.json'
$summaryPath = Join-Path $resolvedOutputRoot 'i2c_whoami_probe_evidence_compare_smoke.summary.json'

$exportScript = Join-Path $PSScriptRoot 'export_materialized_graph.ps1'
$diffScript = Join-Path $PSScriptRoot 'diff_materialized_graph_bundle.ps1'
$reportScript = Join-Path $PSScriptRoot 'report_materialized_graph_bundle.ps1'
$artifactReportScript = Join-Path $PSScriptRoot 'export_system_compiler_artifact_report.ps1'
$inspectScript = Join-Path $PSScriptRoot 'inspect_system_compiler_artifact_report.ps1'

foreach ($requiredPath in @($exportScript, $diffScript, $reportScript, $artifactReportScript, $inspectScript)) {
    if (-not (Test-Path $requiredPath)) {
        throw "required path not found: $requiredPath"
    }
}

if (-not $KeepOutput) {
    Remove-PathIfExists -Path $resolvedOutputRoot
}
Ensure-Directory -Path $resolvedOutputRoot
Ensure-Directory -Path $reportOutputRoot

Remove-PathIfExists -Path $leftBundleRoot
Remove-PathIfExists -Path $rightBundleRoot

& $exportScript -Case $Case -OutputRoot $leftBundleRoot
if ($LASTEXITCODE -ne 0) {
    throw 'baseline fact bundle export failed'
}

Copy-Item -LiteralPath $leftBundleRoot -Destination $rightBundleRoot -Recurse -Force

$rightIndexPath = Join-Path $rightBundleRoot 'index.json'
$rightIndex = Get-Content -LiteralPath $rightIndexPath -Raw -Encoding utf8 | ConvertFrom-Json
$rightCase = Get-CaseEntry -Cases @($rightIndex.cases) -CaseName $Case
Assert-Condition ($null -ne $rightCase) "case not found in synthetic candidate bundle: $Case"
Assert-Condition (-not [string]::IsNullOrWhiteSpace([string]$rightCase.fact_evidence)) "case has no fact_evidence: $Case"

$rightFactEvidencePath = Resolve-BundleArtifactPath -Root $rightBundleRoot -Path ([string]$rightCase.fact_evidence)
Assert-Condition (Test-Path $rightFactEvidencePath) "candidate fact evidence sidecar not found: $rightFactEvidencePath"
Set-WhoAmIProbeBoardEvidenceProvided `
    -FactEvidencePath $rightFactEvidencePath `
    -FactName $ExpectedFact `
    -ProviderSource $ExpectedProviderSource
$rightCase.audit_provided_facts = Add-UniqueString -Values @($rightCase.audit_provided_facts) -Value $ExpectedFact
$rightIndex | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $rightIndexPath -Encoding utf8

Write-Host "[SYNTH] case=$Case fact=$ExpectedFact provider=$ExpectedProviderSource"

$diffData = Invoke-CommandJson -OutputPath $diffJsonPath -Command {
    & $diffScript -LeftBundleRoot $leftBundleRoot -RightBundleRoot $rightBundleRoot -Case $Case -IncludeUnchanged -AsJson
}
Assert-Condition ([string]$diffData.schema -eq 'materialized_graph.bundle_diff/v1') 'unexpected diff schema'
Assert-Condition ([int]$diffData.case_count -eq 1) 'compare smoke expects exactly one diff case'

$caseDiff = @($diffData.cases)[0]
Assert-Condition ([string]$caseDiff.name -eq $Case) 'unexpected diff case name'
Assert-Condition ([string]$caseDiff.status -eq 'unchanged') 'fact evidence content-only compare should keep graph status unchanged'
Assert-Condition (@($caseDiff.summary_changes).Count -eq 0) 'summary_changes must stay empty for same-path sidecar content compare'
$metadataChanges = @($caseDiff.metadata_changes | ForEach-Object { [string]$_ })
Assert-Condition ($metadataChanges.Count -gt 0) 'metadata_changes must mention audit_provided_facts for fact evidence compare'
Assert-Condition (($metadataChanges | Where-Object { [string]$_ -notlike 'audit_provided_facts:*' }).Count -eq 0) 'metadata_changes must only mention audit_provided_facts for fact evidence compare'
Assert-Condition (($metadataChanges | Where-Object { [string]$_ -like "*$ExpectedFact*" }).Count -gt 0) "metadata_changes must mention expected fact: $ExpectedFact"

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
Assert-Condition ([string]$artifactReport.comparison.status -eq 'unchanged') 'artifact report comparison.status must preserve content-only unchanged graph diff'
Assert-Condition ((@($artifactReport.comparison.metadata_changes | ForEach-Object { [string]$_ }) | Where-Object { [string]$_ -like "*$ExpectedFact*" }).Count -gt 0) "artifact report comparison.metadata_changes must mention expected fact: $ExpectedFact"
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
Assert-Condition ([bool]$whyInspectResult.query.comparison.fact_resolution_changed) 'inspect why comparison must mark fact resolution changed'
Assert-Condition ((@($whyInspectResult.query.comparison.required_fact_resolution_change_kinds) -contains 'state_changed')) 'inspect why comparison missing state_changed kind'
Assert-Condition ((@($whyInspectResult.query.comparison.required_facts_changed) -contains $ExpectedFact)) "inspect why missing changed required fact: $ExpectedFact"
Assert-Condition (@(
    @($whyInspectResult.query.reasons) |
        Where-Object { [string]$_ -like "*compare required_fact $ExpectedFact changed: state_changed*" }
).Count -eq 1) 'inspect why reasons missing required fact resolution explanation'

$capListInspectResult = Invoke-CommandJson -OutputPath $capListInspectJsonPath -Command {
    & $inspectScript -ArtifactRoot $artifactReportOutputRoot -Case $Case -CapList -AsJson
}
$capListEntry = @(
    @($capListInspectResult.query.capabilities) |
        Where-Object { [string]$_.capability -eq $ExpectedFact } |
        Select-Object -First 1
) | Select-Object -First 1
Assert-Condition ($null -ne $capListEntry) "inspect cap list entry missing: $ExpectedFact"
Assert-Condition ([bool]$capListEntry.comparison.fact_resolution_changed) 'inspect cap list entry must mark fact resolution changed'
Assert-Condition ((@($capListEntry.comparison.required_fact_resolution_change_kinds) -contains 'state_changed')) 'inspect cap list entry missing state_changed kind'

$rootInspectResult = Invoke-CommandJson -OutputPath $rootInspectJsonPath -Command {
    & $inspectScript -ArtifactRoot $artifactReportOutputRoot -ResourceSummary -AsJson
}
Assert-Condition ($null -ne $rootInspectResult.query.comparison.fact_resolution) 'artifact_root resource summary missing comparison.fact_resolution'
$matrixEntry = Get-RequiredFactResolutionChange `
    -Changes @($rootInspectResult.query.comparison.fact_resolution.required_fact_resolution_change_matrix) `
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

$summary = [ordered]@{
    left_bundle_root = $leftBundleRoot
    right_bundle_root = $rightBundleRoot
    artifact_report = $artifactReportPath
    inspect_json = $inspectJsonPath
    why_inspect_json = $whyInspectJsonPath
    cap_list_inspect_json = $capListInspectJsonPath
    root_inspect_json = $rootInspectJsonPath
    case = $Case
    expected_fact = $ExpectedFact
    expected_provider_source = $ExpectedProviderSource
    assertions = [ordered]@{
        graph_diff_preserved = $true
        fact_resolution_changed = $true
        required_fact_missing_to_satisfied = $true
        expected_provider_source_present = $true
        resource_summary_exposes_compare = $true
        why_explains_required_fact_change = $true
        cap_list_exposes_required_fact_change = $true
        artifact_root_matrix_exposes_required_fact_change = $true
    }
}

$summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $summaryPath -Encoding utf8

Write-Host "[OK] i2c whoami probe evidence compare smoke passed fact=$ExpectedFact"
Write-Host "[REPORT]  $artifactReportPath"
Write-Host "[INSPECT] $inspectJsonPath"
Write-Host "[SUMMARY] $summaryPath"
