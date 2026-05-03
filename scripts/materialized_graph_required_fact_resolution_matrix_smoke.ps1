param(
    [string]$ArtifactRoot = 'out/materialized-graph-ci/artifact-report',
    [string]$ExpectedFact = 'pinmux:pb8/pb9.af4',
    [string]$ExpectedSatisfiedCase = 'board-i2c-fact-composition-smoke',
    [string]$ExpectedMissingCase = 'i2c-device-contract-facts-smoke',
    [string]$ExpectedProviderSource = 'platform.board.stm32_stub',
    [string]$OutputRoot = 'out/materialized-graph-required-fact-resolution-matrix-smoke',
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

function Get-RequiredFactResolutionEntry {
    param(
        [object[]]$Matrix,
        [string]$FactName
    )

    return @(
        $Matrix |
            Where-Object { [string]$_.fact -eq $FactName } |
            Select-Object -First 1
    ) | Select-Object -First 1
}

function Get-MatrixCaseEntry {
    param(
        [object[]]$Cases,
        [string]$CaseName
    )

    return @(
        $Cases |
            Where-Object { [string]$_.case -eq $CaseName } |
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
$matrixJsonPath = Join-Path $resolvedOutputRoot 'required_fact_resolution_matrix.json'
$summaryPath = Join-Path $resolvedOutputRoot 'required_fact_resolution_matrix_smoke.summary.json'

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
Assert-Condition ([string]$result.kind -eq 'fact_resolution_summary/v0') 'unexpected fact resolution summary kind'
Assert-Condition ([string]$result.mode -eq 'summary') 'unexpected fact resolution summary mode'

$factEntry = Get-RequiredFactResolutionEntry `
    -Matrix @($result.required_fact_resolution_matrix) `
    -FactName $ExpectedFact
Assert-Condition ($null -ne $factEntry) "expected fact not found in required fact resolution matrix: $ExpectedFact"
Assert-Condition ([int]$factEntry.case_count -ge 2) "expected fact should appear in at least two cases: $ExpectedFact"
Assert-Condition ([int]$factEntry.cases_satisfied -ge 1) "expected fact should have at least one satisfied case: $ExpectedFact"
Assert-Condition ([int]$factEntry.cases_missing -ge 1) "expected fact should have at least one missing case: $ExpectedFact"
Assert-Condition ((@($factEntry.provider_sources) -contains $ExpectedProviderSource)) "expected provider source missing: $ExpectedProviderSource"

$satisfiedCase = Get-MatrixCaseEntry -Cases @($factEntry.cases) -CaseName $ExpectedSatisfiedCase
Assert-Condition ($null -ne $satisfiedCase) "expected satisfied case missing: $ExpectedSatisfiedCase"
Assert-Condition ([string]$satisfiedCase.state -eq 'satisfied') "expected case should be satisfied: $ExpectedSatisfiedCase"
Assert-Condition ([int]$satisfiedCase.provider_count -gt 0) "expected satisfied case should have providers: $ExpectedSatisfiedCase"
Assert-Condition ((@($satisfiedCase.providers | ForEach-Object { [string]$_.source }) -contains $ExpectedProviderSource)) "expected provider missing from satisfied case: $ExpectedProviderSource"

$missingCase = Get-MatrixCaseEntry -Cases @($factEntry.cases) -CaseName $ExpectedMissingCase
Assert-Condition ($null -ne $missingCase) "expected missing case missing: $ExpectedMissingCase"
Assert-Condition ([string]$missingCase.state -eq 'missing') "expected case should be missing: $ExpectedMissingCase"
Assert-Condition ([int]$missingCase.provider_count -eq 0) "expected missing case should not have providers: $ExpectedMissingCase"

$summary = [ordered]@{
    artifact_root = $resolvedArtifactRoot
    matrix_json = $matrixJsonPath
    expected_fact = $ExpectedFact
    expected_satisfied_case = $ExpectedSatisfiedCase
    expected_missing_case = $ExpectedMissingCase
    expected_provider_source = $ExpectedProviderSource
    assertions = [ordered]@{
        artifact_root_scope = $true
        required_fact_resolution_matrix_present = $true
        expected_fact_has_satisfied_case = $true
        expected_fact_has_missing_case = $true
        expected_provider_source_present = $true
    }
}

$summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryPath -Encoding utf8

Write-Host "[OK] required fact resolution matrix smoke passed fact=$ExpectedFact"
Write-Host "[MATRIX]  $matrixJsonPath"
Write-Host "[SUMMARY] $summaryPath"
