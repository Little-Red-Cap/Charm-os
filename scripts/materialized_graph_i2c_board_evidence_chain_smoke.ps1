param(
    [string]$OutputRoot = 'out/i2c-board-evidence-chain-smoke',
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

function Invoke-Step {
    param(
        [string]$Name,
        [scriptblock]$Command
    )

    Write-Host "[CHAIN][$Name] begin"
    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "chain step failed: $Name"
    }
    Write-Host "[CHAIN][$Name] ok"
}

$resolvedOutputRoot = Resolve-FullPath $OutputRoot
$baselineBundleRoot = Join-Path $resolvedOutputRoot 'whoami-probe-bundle'
$boardEvidenceBundleRoot = Join-Path $resolvedOutputRoot 'board-bringup-bundle'
$producerCompareRoot = Join-Path $resolvedOutputRoot 'producer-compare'
$sampleValidationRoot = Join-Path $resolvedOutputRoot 'sample-validation'
$sampleValidationLogPath = Join-Path $sampleValidationRoot 'board_i2c_whoami_bringup_sample_validation.log'
$summaryPath = Join-Path $resolvedOutputRoot 'i2c_board_evidence_chain_smoke.summary.json'

$exportScript = Join-Path $PSScriptRoot 'export_materialized_graph.ps1'
$producerCompareScript = Join-Path $PSScriptRoot 'materialized_graph_i2c_whoami_board_bringup_evidence_compare_smoke.ps1'
$validateScript = Join-Path $PSScriptRoot 'validate_materialized_graph_artifacts.py'
$samplePath = Join-Path (Split-Path -Parent $PSScriptRoot) 'schemas/examples/system_compiler.fact_evidence.v0.board_i2c_whoami_bringup.sample.json'

foreach ($requiredPath in @($exportScript, $producerCompareScript, $validateScript, $samplePath)) {
    if (-not (Test-Path $requiredPath)) {
        throw "required path not found: $requiredPath"
    }
}

if (-not $KeepOutput) {
    Remove-PathIfExists -Path $resolvedOutputRoot
}
Ensure-Directory -Path $resolvedOutputRoot
Ensure-Directory -Path $sampleValidationRoot

Invoke-Step -Name 'baseline-whoami-probe-evidence' -Command {
    & $exportScript -Case i2c-whoami-probe-evidence-smoke -OutputRoot $baselineBundleRoot
}

Invoke-Step -Name 'board-bringup-host-fixture-evidence' -Command {
    & $exportScript -Case board-i2c-whoami-bringup-evidence-smoke -OutputRoot $boardEvidenceBundleRoot
}

Invoke-Step -Name 'producer-side-compare' -Command {
    & $producerCompareScript -OutputRoot $producerCompareRoot
}

Invoke-Step -Name 'board-bringup-sample-validation' -Command {
    $validationText = (& python $validateScript $samplePath | Out-String)
    $validationText | Set-Content -LiteralPath $sampleValidationLogPath -Encoding utf8
    Write-Host $validationText
}

$producerSummaryPath = Join-Path $producerCompareRoot 'i2c_whoami_board_bringup_evidence_compare_smoke.summary.json'
Assert-Condition (Test-Path $producerSummaryPath) "producer compare summary not found: $producerSummaryPath"
$producerSummary = Get-Content -LiteralPath $producerSummaryPath -Raw -Encoding utf8 | ConvertFrom-Json
Assert-Condition ([string]$producerSummary.expected_fact -eq 'i2c.probe.board_real') 'producer compare expected fact mismatch'
Assert-Condition ([string]$producerSummary.expected_provider_source -eq 'board.bringup') 'producer compare expected provider source mismatch'
Assert-Condition ([bool]$producerSummary.assertions.required_fact_missing_to_satisfied) 'producer compare did not assert missing_to_satisfied'
Assert-Condition (Test-Path $sampleValidationLogPath) "sample validation log not found: $sampleValidationLogPath"

$summary = [ordered]@{
    baseline_bundle_root = $baselineBundleRoot
    board_evidence_bundle_root = $boardEvidenceBundleRoot
    producer_compare_root = $producerCompareRoot
    producer_compare_summary = $producerSummaryPath
    sample_validation_log = $sampleValidationLogPath
    sample = $samplePath
    assertions = [ordered]@{
        baseline_whoami_probe_exported = $true
        board_bringup_host_fixture_exported = $true
        producer_compare_missing_to_satisfied = $true
        sample_validation_passed = $true
    }
}

$summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $summaryPath -Encoding utf8

Write-Host '[OK] i2c board evidence chain smoke passed'
Write-Host "[SUMMARY] $summaryPath"
