param(
    [string]$OutputRoot = "",
    [string]$PythonExe = "python"
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

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    if (-not (Test-Path $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$resolvedOutputRoot = if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    Join-Path $repoRoot "cmake-build-minimal-kernel-runtime-session-witness-compare-summary-smoke"
} else {
    Resolve-FullPath -Path $OutputRoot
}

if (Test-Path $resolvedOutputRoot) {
    Remove-Item -Recurse -Force $resolvedOutputRoot
}
Ensure-Directory -Path $resolvedOutputRoot

$compareSmoke = Join-Path $PSScriptRoot "inspect_minimal_kernel_runtime_session_witness_smoke_compare_smoke.ps1"
$inspectScript = Join-Path $PSScriptRoot "inspect_minimal_kernel_runtime_session_witness_smoke.ps1"
$validator = Join-Path $PSScriptRoot "validate_minimal_kernel_runtime_session_witness_inspect_compare.py"

foreach ($path in @($compareSmoke, $inspectScript, $validator)) {
    if (-not (Test-Path $path)) {
        throw "missing compare summary smoke dependency: $path"
    }
}

$compareSmokeRoot = Join-Path $resolvedOutputRoot "compare-smoke"
Push-Location $repoRoot
try {
    & $compareSmoke -OutputRoot $compareSmokeRoot -PythonExe $PythonExe
} finally {
    Pop-Location
}

$baselineSummaryPath = Join-Path $compareSmokeRoot "baseline\summary.json"
$candidateSummaryPath = Join-Path $compareSmokeRoot "candidate\summary.json"
$compareSummaryPath = Join-Path $resolvedOutputRoot "session-witness.inspect.compare.summary.json"

$jsonText = & $inspectScript `
    -Summary $candidateSummaryPath `
    -BaselineSummary $baselineSummaryPath `
    -CompareSummaryPath $compareSummaryPath `
    -ShowNarratives `
    -AsJson

$summary = $jsonText | ConvertFrom-Json
if ([string]$summary.schema -ne "minimal_kernel.runtime_session_witness_inspect_compare/v0") {
    throw "unexpected compare summary schema: $($summary.schema)"
}
if ([string]$summary.kind -ne "minimal_kernel.runtime_session_witness_inspect_compare") {
    throw "unexpected compare summary kind: $($summary.kind)"
}
if (-not (Test-Path $compareSummaryPath)) {
    throw "expected compare summary file to be written: $compareSummaryPath"
}

Push-Location $repoRoot
try {
    & $PythonExe $validator --summary $compareSummaryPath
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
} finally {
    Pop-Location
}

if (-not [bool]$summary.comparison.changed) {
    throw "expected compare summary changed=true"
}
if (@($summary.comparison.session.runtime.regressed) -notcontains "handoff_continuity") {
    throw "expected compare summary runtime regression handoff_continuity"
}
if (@($summary.comparison.world_compare_session_drift.failure_codes.added) -notcontains "tick_not_observed") {
    throw "expected compare summary world failure code delta tick_not_observed"
}
if (@($summary.comparison.witness_session_failure_export.failure_codes.added) -notcontains "thread_not_resumed") {
    throw "expected compare summary witness failure code delta thread_not_resumed"
}

Write-Host "==> inspect minimal kernel runtime session witness compare summary smoke"
Write-Host ("baseline={0}" -f $baselineSummaryPath)
Write-Host ("candidate={0}" -f $candidateSummaryPath)
Write-Host ("summary={0}" -f $compareSummaryPath)
Write-Host ("changed={0}" -f [bool]$summary.comparison.changed)
Write-Host ("runtime_regressed={0}" -f (@($summary.comparison.session.runtime.regressed) -join ","))
Write-Host ("world_failure_codes_added={0}" -f (@($summary.comparison.world_compare_session_drift.failure_codes.added) -join ","))
Write-Host ("witness_failure_codes_added={0}" -f (@($summary.comparison.witness_session_failure_export.failure_codes.added) -join ","))
Write-Host "ok=1"
