param(
    [string]$BaselineWitnessSummary = "",
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

function Resolve-ToolPath {
    param(
        [string]$Tool
    )

    $command = Get-Command $Tool -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    if (Test-Path $Tool) {
        return (Resolve-Path $Tool).Path
    }

    throw "tool not found: $Tool"
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$resolvedBaseline = if ([string]::IsNullOrWhiteSpace($BaselineWitnessSummary)) {
    Join-Path $repoRoot "schemas\examples\system_compiler.witness_bundle.v0.sample.json"
} else {
    Resolve-FullPath -Path $BaselineWitnessSummary
}
$resolvedOutputRoot = if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    Join-Path $repoRoot "cmake-build-system-compiler-world-compare-session-drift-smoke"
} else {
    Resolve-FullPath -Path $OutputRoot
}

Ensure-Directory -Path $resolvedOutputRoot

$candidatePath = Join-Path $resolvedOutputRoot "candidate.session-drift.witness_bundle.json"
$summaryPath = Join-Path $resolvedOutputRoot "summary.json"
$reportPath = Join-Path $resolvedOutputRoot "report.md"
$checkPath = Join-Path $resolvedOutputRoot "check.txt"

$baseline = Get-Content -LiteralPath $resolvedBaseline -Raw -Encoding utf8 | ConvertFrom-Json
$candidate = $baseline | ConvertTo-Json -Depth 100 | ConvertFrom-Json

$candidate.generated_at_utc = "2026-05-04T00:00:00Z"
$candidate.result = "fail"
$candidate.front_page.summary_path = $candidatePath
$candidate.artifact_context.output_root = $resolvedOutputRoot
$candidate.artifact_context.report_markdown_path = $reportPath
$candidate.artifact_context.check_text_path = $checkPath
$candidate.witness_summary.ok_count = [Math]::Max(0, [int]$candidate.witness_summary.ok_count - 1)
$candidate.witness_summary.fail_count = [int]$candidate.witness_summary.fail_count + 1

$sessionEntry = $candidate.witness_entries | Where-Object { [string]$_.kind -eq "kernel_runtime_session" } | Select-Object -First 1
if ($null -eq $sessionEntry) {
    throw "baseline does not contain a kernel_runtime_session witness entry"
}

$sessionEntry.status = "fail"
$sessionEntry.observations = @(
    "session_status=collapsed",
    "semantic=standing",
    "machine=standing",
    "runtime=tick:True trap:True thread:True task_syscall:True handoff:False",
    "ledger_events=10",
    "failures=1",
    "failure=handoff_continuity_broken domain=runtime layer=lower_half phase=handoff.live focus=handoff,continuity,session"
)

$candidate.violations = @(
    "failed witness: $($sessionEntry.id)",
    "session failure: handoff_continuity_broken"
)

$candidate | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $candidatePath -Encoding utf8

$python = Resolve-ToolPath -Tool $PythonExe
$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_world.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_world_compare.py"

Push-Location $repoRoot
try {
    & $python $compareScript `
        --baseline $resolvedBaseline `
        --candidate $candidatePath `
        --output-root $resolvedOutputRoot `
        --summary $summaryPath `
        --report-markdown $reportPath `
        --check-text $checkPath
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    & $python $validateScript --summary $summaryPath
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
} finally {
    Pop-Location
}

$summary = Get-Content -LiteralPath $summaryPath -Raw -Encoding utf8 | ConvertFrom-Json
$sessionDrift = $summary.collapse_surface.session_drift

if ([string]$summary.world_verdict -ne "collapsed") {
    throw "expected collapsed world verdict, got $($summary.world_verdict)"
}
if (-not [bool]$sessionDrift.changed) {
    throw "expected session_drift.changed=true"
}
if (@($sessionDrift.failure_codes) -notcontains "handoff_continuity_broken") {
    throw "expected handoff_continuity_broken in session drift failure codes"
}
foreach ($requiredFocus in @("session", "runtime", "handoff", "continuity")) {
    if (@($sessionDrift.affected_focus) -notcontains $requiredFocus) {
        throw "expected session drift focus: $requiredFocus"
    }
}

Write-Host "==> system compiler world compare session drift smoke"
Write-Host ("baseline={0}" -f $resolvedBaseline)
Write-Host ("candidate={0}" -f $candidatePath)
Write-Host ("summary={0}" -f $summaryPath)
Write-Host ("world_verdict={0}" -f [string]$summary.world_verdict)
Write-Host ("session_drift_domains={0}" -f (@($sessionDrift.affected_domains) -join ","))
Write-Host ("session_drift_focus={0}" -f (@($sessionDrift.affected_focus) -join ","))
Write-Host ("session_drift_failure_codes={0}" -f (@($sessionDrift.failure_codes) -join ","))
Write-Host "ok=1"
