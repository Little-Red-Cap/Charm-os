param(
    [string]$ArtifactRoot = 'out/materialized-graph-ci/artifact-report',
    [string[]]$ExpectedCase = @('bringup-block-observe-demo', 'bringup-minimal-observe-demo'),
    [string]$ExpectedBoardCapability = 'board.win_stub',
    [string]$ExpectedCommonCapability = 'system.clock',
    [string]$ExpectedBlockCapability = 'block.sd0',
    [string]$OutputRoot = 'out/materialized-graph-bringup-evidence-matrix-smoke',
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

function Get-CaseSummaryEntry {
    param(
        [object[]]$CaseSummaries,
        [string]$CaseName
    )

    return @(
        $CaseSummaries |
            Where-Object { [string]$_.case -eq $CaseName } |
            Select-Object -First 1
    ) | Select-Object -First 1
}

function Get-CapabilityMatrixEntry {
    param(
        [object[]]$CapabilityMatrix,
        [string]$CapabilityName
    )

    return @(
        $CapabilityMatrix |
            Where-Object { [string]$_.capability -eq $CapabilityName } |
            Select-Object -First 1
    ) | Select-Object -First 1
}

function Invoke-ArtifactRootBringupEvidenceJson {
    param(
        [string]$InspectScript,
        [string]$ArtifactRootPath,
        [string]$OutputPath
    )

    $jsonText = (& $InspectScript -ArtifactRoot $ArtifactRootPath -BringupEvidence -AsJson | Out-String)
    $jsonText | Set-Content -LiteralPath $OutputPath -Encoding utf8
    return ($jsonText | ConvertFrom-Json)
}

$inspectScript = Join-Path $PSScriptRoot 'inspect_system_compiler_artifact_report.ps1'
$resolvedArtifactRoot = Resolve-FullPath $ArtifactRoot
$resolvedOutputRoot = Resolve-FullPath $OutputRoot
$matrixJsonPath = Join-Path $resolvedOutputRoot 'bringup_evidence_matrix.json'
$summaryPath = Join-Path $resolvedOutputRoot 'bringup_evidence_matrix_smoke.summary.json'

if (-not (Test-Path $inspectScript)) {
    throw "required script not found: $inspectScript"
}

if (-not (Test-Path $resolvedArtifactRoot)) {
    throw "artifact root not found: $resolvedArtifactRoot"
}

if (-not $KeepOutput) {
    Remove-PathIfExists -Path $resolvedOutputRoot
}
Ensure-Directory -Path $resolvedOutputRoot

$bringupMatrix = Invoke-ArtifactRootBringupEvidenceJson `
    -InspectScript $inspectScript `
    -ArtifactRootPath $resolvedArtifactRoot `
    -OutputPath $matrixJsonPath

Assert-Condition ([string]$bringupMatrix.query.kind -eq 'bringup_evidence') 'unexpected query kind'
Assert-Condition ([string]$bringupMatrix.query.scope -eq 'artifact_root') 'unexpected query scope'

$result = $bringupMatrix.query.result
Assert-Condition ($null -ne $result) 'bringup evidence result is missing'

$expectedCaseCount = @($ExpectedCase).Count
Assert-Condition ([int]$result.case_count -ge $expectedCaseCount) 'artifact_root bringup evidence case_count is smaller than expected'

foreach ($caseName in @($ExpectedCase)) {
    $caseSummary = Get-CaseSummaryEntry -CaseSummaries @($result.cases) -CaseName $caseName
    Assert-Condition ($null -ne $caseSummary) "expected case not found in bringup matrix: $caseName"
    Assert-Condition ([int]$caseSummary.declared_count -gt 0) "expected case must declare capabilities: $caseName"
    Assert-Condition ([int]$caseSummary.materialized_count -gt 0) "expected case must materialize capabilities: $caseName"
    Assert-Condition ([int]$caseSummary.observed_count -gt 0) "expected case must observe capabilities: $caseName"
    Assert-Condition ([int]$caseSummary.published_count -eq 0) "published_count must stay zero in synthetic bringup cases: $caseName"
    Assert-Condition ([int]$caseSummary.blocked_count -eq 0) "blocked_count must stay zero in synthetic bringup cases: $caseName"
    Assert-Condition ([int]$caseSummary.failed_count -eq 0) "failed_count must stay zero in synthetic bringup cases: $caseName"
    Assert-Condition (@($caseSummary.published_capabilities).Count -eq 0) "published_capabilities must stay empty in synthetic bringup cases: $caseName"
}

$boardCapability = Get-CapabilityMatrixEntry -CapabilityMatrix @($result.capability_matrix) -CapabilityName $ExpectedBoardCapability
Assert-Condition ($null -ne $boardCapability) "expected board capability not found in matrix: $ExpectedBoardCapability"
foreach ($caseName in @($ExpectedCase)) {
    Assert-Condition ((@($boardCapability.declared_cases) -contains $caseName)) "board capability must stay declared in case: $caseName"
}
Assert-Condition (@($boardCapability.materialized_cases).Count -eq 0) 'board capability must stay non-materialized'
Assert-Condition (@($boardCapability.observed_cases).Count -eq 0) 'board capability must stay non-observed'
Assert-Condition (@($boardCapability.provider_nodes).Count -eq 0) 'board capability must not expose provider nodes'

$commonCapability = Get-CapabilityMatrixEntry -CapabilityMatrix @($result.capability_matrix) -CapabilityName $ExpectedCommonCapability
Assert-Condition ($null -ne $commonCapability) "expected common capability not found in matrix: $ExpectedCommonCapability"
foreach ($caseName in @($ExpectedCase)) {
    Assert-Condition ((@($commonCapability.materialized_cases) -contains $caseName)) "common capability must materialize in case: $caseName"
    Assert-Condition ((@($commonCapability.observed_cases) -contains $caseName)) "common capability must be observed in case: $caseName"
    Assert-Condition ((@($commonCapability.provider_nodes) -contains ($caseName + ':' + $ExpectedCommonCapability))) "common capability missing qualified provider node for case: $caseName"
}

$blockCapability = Get-CapabilityMatrixEntry -CapabilityMatrix @($result.capability_matrix) -CapabilityName $ExpectedBlockCapability
Assert-Condition ($null -ne $blockCapability) "expected block capability not found in matrix: $ExpectedBlockCapability"
Assert-Condition ([int]$blockCapability.case_count -eq 1) "block capability must only appear in one case: $ExpectedBlockCapability"
Assert-Condition ((@($blockCapability.materialized_cases) -contains 'bringup-block-observe-demo')) "block capability must materialize in bringup-block-observe-demo"
Assert-Condition ((@($blockCapability.observed_cases) -contains 'bringup-block-observe-demo')) "block capability must be observed in bringup-block-observe-demo"
Assert-Condition (-not (@($blockCapability.declared_cases) -contains 'bringup-minimal-observe-demo')) "block capability must not appear in bringup-minimal-observe-demo"

Assert-Condition (@($result.blocked_reason_matrix).Count -eq 0) 'blocked_reason_matrix must stay empty'
Assert-Condition (@($result.failed_reason_matrix).Count -eq 0) 'failed_reason_matrix must stay empty'

$summary = [ordered]@{
    artifact_root = $resolvedArtifactRoot
    matrix_json = $matrixJsonPath
    case_count = [int]$result.case_count
    expected_cases = @($ExpectedCase)
    expected_board_capability = $ExpectedBoardCapability
    expected_common_capability = $ExpectedCommonCapability
    expected_block_capability = $ExpectedBlockCapability
    assertions = [ordered]@{
        artifact_root_scope = $true
        expected_cases_present = $true
        board_capability_stays_declared_only = $true
        common_capability_materialized_across_cases = $true
        block_capability_stays_case_specific = $true
        blocked_and_failed_reason_matrices_empty = $true
    }
}

$summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryPath -Encoding utf8

Write-Host "[OK] bringup evidence matrix smoke passed common=$ExpectedCommonCapability block=$ExpectedBlockCapability"
Write-Host "[MATRIX]  $matrixJsonPath"
Write-Host "[SUMMARY] $summaryPath"
