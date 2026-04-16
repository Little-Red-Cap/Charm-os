param(
    [string]$ArtifactRoot = 'out/materialized-graph-ci/artifact-report',
    [string[]]$ExpectedCase = @('bringup-block-observe-demo', 'bringup-minimal-observe-demo'),
    [string]$ExpectedContract = 'needs_monotonic_clock',
    [string]$ExpectedFact = 'system.clock',
    [string]$OutputRoot = 'out/materialized-graph-resource-contract-matrix-smoke',
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

function Get-MatrixContractEntry {
    param(
        [object[]]$ContractMatrix,
        [string]$ContractName
    )

    return @(
        $ContractMatrix |
            Where-Object { [string]$_.contract -eq $ContractName } |
            Select-Object -First 1
    ) | Select-Object -First 1
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

function Get-ProvidedFactEntry {
    param(
        [object[]]$ProvidedFactMatrix,
        [string]$FactName
    )

    return @(
        $ProvidedFactMatrix |
            Where-Object { [string]$_.fact -eq $FactName } |
            Select-Object -First 1
    ) | Select-Object -First 1
}

function Invoke-ArtifactRootResourceSummaryJson {
    param(
        [string]$InspectScript,
        [string]$ArtifactRootPath,
        [string]$OutputPath
    )

    $jsonText = (& $InspectScript -ArtifactRoot $ArtifactRootPath -ResourceSummary -AsJson | Out-String)
    $jsonText | Set-Content -LiteralPath $OutputPath -Encoding utf8
    return ($jsonText | ConvertFrom-Json)
}

$inspectScript = Join-Path $PSScriptRoot 'inspect_system_compiler_artifact_report.ps1'
$resolvedArtifactRoot = Resolve-FullPath $ArtifactRoot
$resolvedOutputRoot = Resolve-FullPath $OutputRoot
$matrixJsonPath = Join-Path $resolvedOutputRoot 'resource_contract_matrix.json'
$summaryPath = Join-Path $resolvedOutputRoot 'resource_contract_matrix_smoke.summary.json'

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

$resourceMatrix = Invoke-ArtifactRootResourceSummaryJson `
    -InspectScript $inspectScript `
    -ArtifactRootPath $resolvedArtifactRoot `
    -OutputPath $matrixJsonPath

Assert-Condition ([string]$resourceMatrix.query.kind -eq 'resource_summary') 'unexpected query kind'
Assert-Condition ([string]$resourceMatrix.query.scope -eq 'artifact_root') 'unexpected query scope'

$result = $resourceMatrix.query.result
Assert-Condition ($null -ne $result) 'resource summary result is missing'
Assert-Condition ([int]$result.case_count -gt 0) 'artifact_root resource summary must include at least one case'

$expectedCaseCount = @($ExpectedCase).Count
Assert-Condition ([int]$result.case_count -ge $expectedCaseCount) 'artifact_root resource summary case_count is smaller than expected'

$contractEntry = Get-MatrixContractEntry -ContractMatrix @($result.contract_matrix) -ContractName $ExpectedContract
Assert-Condition ($null -ne $contractEntry) "expected contract not found in matrix: $ExpectedContract"
Assert-Condition ((@($contractEntry.requires) -contains $ExpectedFact)) "expected contract matrix requires missing fact: $ExpectedFact"

foreach ($caseName in @($ExpectedCase)) {
    $caseSummary = Get-CaseSummaryEntry -CaseSummaries @($result.cases) -CaseName $caseName
    Assert-Condition ($null -ne $caseSummary) "expected case not found in resource matrix: $caseName"
    Assert-Condition ([int]$caseSummary.declared_contracts -gt 0) "expected case must declare at least one contract: $caseName"
    Assert-Condition ((@($caseSummary.audit_provided_facts) -contains $ExpectedFact)) "expected case missing audit_provided_fact '$ExpectedFact': $caseName"

    $caseContract = @(
        @($contractEntry.cases) |
            Where-Object { [string]$_.case -eq $caseName } |
            Select-Object -First 1
    ) | Select-Object -First 1
    Assert-Condition ($null -ne $caseContract) "expected case missing from contract matrix entry: $caseName"
    Assert-Condition ([string]$caseContract.state -eq 'satisfied') "expected case contract state must be satisfied: $caseName"
    Assert-Condition ((@($caseContract.present_facts) -contains $ExpectedFact)) "expected case contract missing present fact '$ExpectedFact': $caseName"
}

Assert-Condition ([int]$contractEntry.cases_declared -ge $expectedCaseCount) "contract matrix declared count too small for $ExpectedContract"
Assert-Condition ([int]$contractEntry.cases_satisfied -ge $expectedCaseCount) "contract matrix satisfied count too small for $ExpectedContract"

$providedFactEntry = Get-ProvidedFactEntry -ProvidedFactMatrix @($result.provided_fact_matrix) -FactName $ExpectedFact
Assert-Condition ($null -ne $providedFactEntry) "expected provided fact not found in matrix: $ExpectedFact"
Assert-Condition ([int]$providedFactEntry.case_count -ge $expectedCaseCount) "provided fact matrix case_count too small for $ExpectedFact"

foreach ($caseName in @($ExpectedCase)) {
    $factCase = @(
        @($providedFactEntry.cases) |
            Where-Object { [string]$_.case -eq $caseName } |
            Select-Object -First 1
    ) | Select-Object -First 1
    Assert-Condition ($null -ne $factCase) "expected provided fact matrix missing case '$caseName' for $ExpectedFact"
}

$summary = [ordered]@{
    artifact_root = $resolvedArtifactRoot
    matrix_json = $matrixJsonPath
    case_count = [int]$result.case_count
    expected_cases = @($ExpectedCase)
    expected_contract = $ExpectedContract
    expected_fact = $ExpectedFact
    assertions = [ordered]@{
        artifact_root_scope = $true
        expected_cases_present = $true
        expected_contract_in_matrix = $true
        expected_contract_satisfied_across_cases = $true
        expected_fact_in_provided_fact_matrix = $true
    }
}

$summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryPath -Encoding utf8

Write-Host "[OK] resource contract matrix smoke passed contract=$ExpectedContract fact=$ExpectedFact"
Write-Host "[MATRIX]  $matrixJsonPath"
Write-Host "[SUMMARY] $summaryPath"
