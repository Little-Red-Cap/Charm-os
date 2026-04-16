param(
    [string]$ArtifactRoot = 'out/materialized-graph-ci/artifact-report',
    [string]$Case = 'bringup-minimal-observe-demo',
    [string]$Report = '',
    [string]$ExpectedContract = 'needs_monotonic_clock',
    [string]$ExpectedFact = 'system.clock',
    [string]$ExpectedBoardFact = 'board.win_stub',
    [string]$ExpectedUnknownContract = '',
    [string]$OutputRoot = 'out/materialized-graph-resource-contract-smoke',
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

function Get-ObjectPropertyValue {
    param(
        $Object,
        [string]$Name
    )

    if ($null -eq $Object -or [string]::IsNullOrWhiteSpace($Name)) {
        return $null
    }

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        return $null
    }

    return $property.Value
}

function Get-ContractEntry {
    param(
        [object[]]$Contracts,
        [string]$ContractName
    )

    $matches = @(
        $Contracts |
            Where-Object {
                [string]$_.contract -eq $ContractName
            }
    )

    if ($matches.Count -eq 0) {
        return $null
    }

    return $matches[0]
}

function Invoke-ResourceSummaryJson {
    param(
        [string]$InspectScript,
        [string]$ArtifactRootPath,
        [string]$CaseName,
        [string]$ReportPath,
        [string]$OutputPath
    )

    $args = @{
        ResourceSummary = $true
        AsJson = $true
    }

    if (-not [string]::IsNullOrWhiteSpace($ReportPath)) {
        $args.Report = $ReportPath
    } else {
        $args.ArtifactRoot = $ArtifactRootPath
        $args.Case = $CaseName
    }

    $jsonText = (& $InspectScript @args | Out-String)
    $jsonText | Set-Content -LiteralPath $OutputPath -Encoding utf8
    return ($jsonText | ConvertFrom-Json)
}

$inspectScript = Join-Path $PSScriptRoot 'inspect_system_compiler_artifact_report.ps1'
$resolvedArtifactRoot = ''
$resolvedReport = ''
$resolvedOutputRoot = Resolve-FullPath $OutputRoot
$resourceSummaryJsonPath = Join-Path $resolvedOutputRoot 'resource_contract_summary.json'
$summaryPath = Join-Path $resolvedOutputRoot 'resource_contract_smoke.summary.json'

if (-not (Test-Path $inspectScript)) {
    throw "required script not found: $inspectScript"
}

if (-not [string]::IsNullOrWhiteSpace($Report)) {
    $resolvedReport = Resolve-FullPath $Report
    if (-not (Test-Path $resolvedReport)) {
        throw "report not found: $resolvedReport"
    }
} else {
    $resolvedArtifactRoot = Resolve-FullPath $ArtifactRoot
    if ([string]::IsNullOrWhiteSpace($Case)) {
        throw 'case is required when -Report is not provided'
    }
    if (-not (Test-Path $resolvedArtifactRoot)) {
        throw "artifact root not found: $resolvedArtifactRoot"
    }
}

if (-not $KeepOutput) {
    Remove-PathIfExists -Path $resolvedOutputRoot
}
Ensure-Directory -Path $resolvedOutputRoot

$resourceSummary = Invoke-ResourceSummaryJson `
    -InspectScript $inspectScript `
    -ArtifactRootPath $resolvedArtifactRoot `
    -CaseName $Case `
    -ReportPath $resolvedReport `
    -OutputPath $resourceSummaryJsonPath

Assert-Condition ([string]$resourceSummary.query.kind -eq 'resource_summary') 'unexpected query kind'
Assert-Condition ([string]$resourceSummary.query.scope -eq 'report') 'unexpected query scope'

$subjectCase = [string]$resourceSummary.subject.case
if ([string]::IsNullOrWhiteSpace($resolvedReport)) {
    Assert-Condition ($subjectCase -eq $Case) "resource summary returned unexpected case: $subjectCase"
}

$result = $resourceSummary.query.result
Assert-Condition ($null -ne $result) 'resource summary result is missing'
Assert-Condition ([int]$result.declared_contracts -gt 0) 'declared_contracts must be greater than zero'
Assert-Condition ([int]$result.audited_count -eq [int]$result.declared_contracts) 'audited_count must match declared_contracts'

$factInventory = $result.fact_inventory
Assert-Condition ($null -ne $factInventory) 'fact inventory is missing'
Assert-Condition ((@($factInventory.audit_provided_facts) -contains $ExpectedFact)) "audit_provided_facts missing expected fact: $ExpectedFact"
Assert-Condition ((@($factInventory.all_available_facts) -contains $ExpectedFact)) "all_available_facts missing expected fact: $ExpectedFact"

if (-not [string]::IsNullOrWhiteSpace($ExpectedBoardFact)) {
    Assert-Condition ((@($factInventory.declared_facts) -contains $ExpectedBoardFact)) "declared_facts missing expected board fact: $ExpectedBoardFact"
    Assert-Condition ((@($factInventory.subject_facts) -contains $ExpectedBoardFact)) "subject_facts missing expected board fact: $ExpectedBoardFact"
}

$contractEntry = Get-ContractEntry -Contracts @($result.contracts) -ContractName $ExpectedContract
Assert-Condition ($null -ne $contractEntry) "expected contract not found: $ExpectedContract"
Assert-Condition ([string]$contractEntry.state -eq 'satisfied') "expected contract must be satisfied: $ExpectedContract"
Assert-Condition ((@($contractEntry.present_facts) -contains $ExpectedFact)) "expected contract missing present fact: $ExpectedFact"
Assert-Condition ((@($contractEntry.missing_facts)).Count -eq 0) "expected contract must not report missing facts: $ExpectedContract"

$expectedFactSources = @(
    @(Get-ObjectPropertyValue -Object $contractEntry.fact_sources -Name $ExpectedFact) |
        Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
        ForEach-Object { [string]$_ }
)
Assert-Condition (($expectedFactSources -contains 'audit_provided_facts')) "fact_sources[$ExpectedFact] must include audit_provided_facts"

if (-not [string]::IsNullOrWhiteSpace($ExpectedUnknownContract)) {
    $unknownContractEntry = Get-ContractEntry -Contracts @($result.contracts) -ContractName $ExpectedUnknownContract
    Assert-Condition ($null -ne $unknownContractEntry) "expected unknown contract not found: $ExpectedUnknownContract"
    Assert-Condition ([string]$unknownContractEntry.state -eq 'unknown') "expected unknown contract must stay unknown: $ExpectedUnknownContract"
}

$summary = [ordered]@{
    artifact_root = if ([string]::IsNullOrWhiteSpace($resolvedArtifactRoot)) { $null } else { $resolvedArtifactRoot }
    report_path = [string]$resourceSummary.report_path
    case = $subjectCase
    subject_board = [string]$resourceSummary.subject.board
    resource_summary_json = $resourceSummaryJsonPath
    expected_contract = $ExpectedContract
    expected_fact = $ExpectedFact
    expected_board_fact = if ([string]::IsNullOrWhiteSpace($ExpectedBoardFact)) { $null } else { $ExpectedBoardFact }
    expected_unknown_contract = if ([string]::IsNullOrWhiteSpace($ExpectedUnknownContract)) { $null } else { $ExpectedUnknownContract }
    assertions = [ordered]@{
        declared_contracts_present = $true
        audited_count_matches_declared_contracts = $true
        expected_fact_available = $true
        expected_contract_satisfied = $true
        expected_fact_has_audit_source = $true
        expected_board_fact_declared = if ([string]::IsNullOrWhiteSpace($ExpectedBoardFact)) { $null } else { $true }
        expected_board_fact_in_subject = if ([string]::IsNullOrWhiteSpace($ExpectedBoardFact)) { $null } else { $true }
        expected_unknown_contract_preserved = if ([string]::IsNullOrWhiteSpace($ExpectedUnknownContract)) { $null } else { $true }
    }
}

$summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryPath -Encoding utf8

Write-Host "[OK] resource contract smoke passed case=$subjectCase contract=$ExpectedContract"
Write-Host "[RESOURCE] $resourceSummaryJsonPath"
Write-Host "[SUMMARY]  $summaryPath"
