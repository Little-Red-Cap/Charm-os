param(
    [string]$OutputRoot = "out/compiler-lifecycle-summary-sidecar-smoke",
    [string]$PythonExe = "python",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

function Resolve-FullPath {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ""
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

    if (-not (Test-Path $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Assert-Equal {
    param(
        $Actual,
        $Expected,
        [string]$Label
    )

    if ($Actual -ne $Expected) {
        throw ("{0}: expected [{1}] but got [{2}]" -f $Label, $Expected, $Actual)
    }
}

function Read-Json {
    param(
        [string]$Path
    )

    return Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json
}

function Write-Json {
    param(
        $Data,
        [string]$Path
    )

    $Data | ConvertTo-Json -Depth 64 | Set-Content -LiteralPath $Path -Encoding utf8
}

function Assert-CommandFails {
    param(
        [scriptblock]$Command,
        [string]$Label
    )

    $failed = $false
    try {
        & $Command
    } catch {
        $failed = $true
        Write-Host ("[EXPECTED-FAIL] {0}: {1}" -f $Label, $_.Exception.Message)
    }

    if (-not $failed) {
        throw ("{0}: expected command to fail" -f $Label)
    }
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$outputRootPath = Resolve-FullPath -Path $OutputRoot

if ($Clean -and (Test-Path $outputRootPath)) {
    Remove-Item -LiteralPath $outputRootPath -Recurse -Force
}
Ensure-Directory -Path $outputRootPath

$exportScript = Join-Path $PSScriptRoot "export_compiler_lifecycle_summary.py"
$checkScript = Join-Path $PSScriptRoot "check_compiler_lifecycle_summary.ps1"
$artifactReportIndex = Join-Path $repoRoot "schemas/examples/system_compiler.artifact_report_index.v0.sample.json"
$kernelRuntimeSession = Join-Path $repoRoot "schemas/examples/minimal_kernel.kernel_runtime_session.v0.sample.json"
$witnessBundle = Join-Path $repoRoot "schemas/examples/system_compiler.witness_bundle.v0.sample.json"
$worldCompare = Join-Path $repoRoot "schemas/examples/system_compiler.world_compare.v0.sample.json"

foreach ($requiredPath in @($exportScript, $checkScript, $artifactReportIndex, $kernelRuntimeSession, $witnessBundle, $worldCompare)) {
    if (-not (Test-Path $requiredPath)) {
        throw "required input not found: $requiredPath"
    }
}

$summaryPath = Join-Path $outputRootPath "compiler_lifecycle.summary.json"
$reportPath = Join-Path $outputRootPath "compiler_lifecycle.report.md"
$checkPath = Join-Path $outputRootPath "compiler_lifecycle.check.txt"
$gatePath = Join-Path $outputRootPath "compiler_lifecycle.gate.txt"
$jsonToolPath = Join-Path $outputRootPath "compiler_lifecycle.summary.normalized.json"
$forgedFrozenPath = Join-Path $outputRootPath "compiler_lifecycle.forged_frozen.summary.json"
$forgedFrozenGatePath = Join-Path $outputRootPath "compiler_lifecycle.forged_frozen.gate.txt"
$loweredDirectPath = Join-Path $outputRootPath "compiler_lifecycle.lowered_direct.summary.json"
$loweredDirectGatePath = Join-Path $outputRootPath "compiler_lifecycle.lowered_direct.gate.txt"

& $PythonExe $exportScript `
    --artifact-report-index $artifactReportIndex `
    --kernel-runtime-session $kernelRuntimeSession `
    --witness-bundle $witnessBundle `
    --world-compare $worldCompare `
    --output $summaryPath `
    --report-markdown $reportPath `
    --check-text $checkPath

if ($LASTEXITCODE -ne 0) {
    throw ("compiler lifecycle summary exporter failed with exit code {0}" -f $LASTEXITCODE)
}

& $PythonExe -m json.tool $summaryPath | Set-Content -LiteralPath $jsonToolPath -Encoding utf8
if ($LASTEXITCODE -ne 0) {
    throw ("python -m json.tool failed with exit code {0}" -f $LASTEXITCODE)
}

& $checkScript `
    -Summary $summaryPath `
    -OutputPath $gatePath `
    -RequireLoweredProjectionKind "interpretive" `
    -RequireArchivedCoverageStrength "weak_to_medium" `
    -RequireObservedStatus "present"

$summary = Get-Content -LiteralPath $summaryPath -Raw -Encoding utf8 | ConvertFrom-Json
$session = Get-Content -LiteralPath $kernelRuntimeSession -Raw -Encoding utf8 | ConvertFrom-Json

Assert-Equal -Actual ([string]$summary.result) -Expected "ok" -Label "result"
Assert-Equal -Actual ([int]$summary.state_count) -Expected 9 -Label "state_count"

$stateNames = @($summary.states.PSObject.Properties.Name)
$expectedStates = @(
    "declared",
    "materialized",
    "proven",
    "frozen",
    "lowered",
    "witnessed",
    "observed",
    "archived",
    "compared"
)
Assert-Equal -Actual $stateNames.Count -Expected $expectedStates.Count -Label "states.count"
foreach ($stateName in $expectedStates) {
    if ($stateNames -notcontains $stateName) {
        throw "missing lifecycle state: $stateName"
    }
}

Assert-Equal -Actual ([string]$summary.states.frozen.status) -Expected "missing" -Label "frozen.status"
Assert-Equal -Actual ([string]$summary.states.lowered.projection_kind) -Expected "interpretive" -Label "lowered.projection_kind"
Assert-Equal -Actual ([string]$summary.states.archived.coverage_strength) -Expected "weak_to_medium" -Label "archived.coverage_strength"
Assert-Equal -Actual ([string]$summary.states.observed.status) -Expected "present" -Label "observed.status"
Assert-Equal -Actual ([string]$session.verdict.session_status) -Expected "standing" -Label "source session verdict"

$forgedFrozenSummary = Read-Json -Path $summaryPath
$forgedFrozenSummary.states.frozen.status = "present"
$forgedFrozenSummary.states.frozen.coverage_strength = "strong"
$forgedFrozenSummary.states.frozen.projection_kind = "direct"
$forgedFrozenSummary.states.frozen.sidecar_gap = "not_required"
Write-Json -Data $forgedFrozenSummary -Path $forgedFrozenPath
Assert-CommandFails -Label "forged frozen coverage" -Command {
    & $checkScript -Summary $forgedFrozenPath -OutputPath $forgedFrozenGatePath
}

$loweredDirectSummary = Read-Json -Path $summaryPath
$loweredDirectSummary.states.lowered.projection_kind = "direct"
Write-Json -Data $loweredDirectSummary -Path $loweredDirectPath
Assert-CommandFails -Label "lowered direct truth" -Command {
    & $checkScript -Summary $loweredDirectPath -OutputPath $loweredDirectGatePath
}

Write-Host "[COMPILER-LIFECYCLE-SUMMARY-SIDECAR-SMOKE] result=ok"
Write-Host ("summary={0}" -f $summaryPath)
Write-Host ("report={0}" -f $reportPath)
Write-Host ("check={0}" -f $checkPath)
Write-Host ("gate={0}" -f $gatePath)
Write-Host ("forged_frozen_gate={0}" -f $forgedFrozenGatePath)
Write-Host ("lowered_direct_gate={0}" -f $loweredDirectGatePath)
