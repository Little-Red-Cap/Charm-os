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

function Replace-RootInValue {
    param(
        $Value,
        [string]$OldRoot,
        [string]$NewRoot
    )

    if ($Value -is [string]) {
        if ($Value.StartsWith($OldRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
            return ($NewRoot + $Value.Substring($OldRoot.Length))
        }

        return $Value
    }

    if ($Value -is [System.Collections.IDictionary]) {
        foreach ($key in @($Value.Keys)) {
            $Value[$key] = Replace-RootInValue -Value $Value[$key] -OldRoot $OldRoot -NewRoot $NewRoot
        }
        return $Value
    }

    if ($Value -is [System.Collections.IList]) {
        for ($index = 0; $index -lt $Value.Count; $index += 1) {
            $Value[$index] = Replace-RootInValue -Value $Value[$index] -OldRoot $OldRoot -NewRoot $NewRoot
        }
        return $Value
    }

    if ($null -ne $Value -and $Value.PSObject -and $Value.PSObject.Properties.Count -gt 0) {
        foreach ($property in @($Value.PSObject.Properties)) {
            $property.Value = Replace-RootInValue -Value $property.Value -OldRoot $OldRoot -NewRoot $NewRoot
        }
    }

    return $Value
}

function Require-Line {
    param(
        [string[]]$Lines,
        [string]$Needle
    )

    if (-not (@($Lines) | Where-Object { $_ -like "*$Needle*" })) {
        throw "expected compare output to contain: $Needle"
    }
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$resolvedOutputRoot = if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    Join-Path $repoRoot "cmake-build-minimal-kernel-runtime-session-witness-inspect-compare-smoke"
} else {
    Resolve-FullPath -Path $OutputRoot
}

$baselineRoot = Join-Path $resolvedOutputRoot "baseline"
$candidateRoot = Join-Path $resolvedOutputRoot "candidate"
$ciSmoke = Join-Path $PSScriptRoot "ci_minimal_kernel_runtime_session_witness_smoke.ps1"
$inspectScript = Join-Path $PSScriptRoot "inspect_minimal_kernel_runtime_session_witness_smoke.ps1"

foreach ($path in @($ciSmoke, $inspectScript)) {
    if (-not (Test-Path $path)) {
        throw "missing inspect compare dependency: $path"
    }
}

if (Test-Path $resolvedOutputRoot) {
    Remove-Item -Recurse -Force $resolvedOutputRoot
}
Ensure-Directory -Path $resolvedOutputRoot

Push-Location $repoRoot
try {
    & $ciSmoke -OutputRoot $baselineRoot -PythonExe $PythonExe -Clean
} finally {
    Pop-Location
}

Copy-Item -Recurse -Force $baselineRoot $candidateRoot

$baselineSummaryPath = Join-Path $baselineRoot "summary.json"
$candidateSummaryPath = Join-Path $candidateRoot "summary.json"

$candidate = Get-Content -LiteralPath $candidateSummaryPath -Raw -Encoding utf8 | ConvertFrom-Json
$candidate = Replace-RootInValue -Value $candidate -OldRoot $baselineRoot -NewRoot $candidateRoot
$candidate.generated_at = "2026-05-05T00:00:00Z"
$candidate.result = "fail"
$candidate.checks.session.session_status = "collapsed"
$candidate.checks.session.failure_domain = "runtime"
$candidate.checks.session.failure_count = 1
$candidate.checks.session.ledger_event_count = [int]$candidate.checks.session.ledger_event_count + 3
$candidate.checks.session.runtime.handoff_continuity = $false
$candidate.checks.session.runtime.task_syscall = $false

$candidate.checks.world_compare_session_drift.world_verdict = "drifted"
$candidate.checks.world_compare_session_drift.regression_count = [int]$candidate.checks.world_compare_session_drift.regression_count + 1
$candidate.checks.world_compare_session_drift.session_drift.failure_codes = @($candidate.checks.world_compare_session_drift.session_drift.failure_codes) + @("tick_not_observed")
$candidate.checks.world_compare_session_drift.session_drift.missing_runtime_facts = @($candidate.checks.world_compare_session_drift.session_drift.missing_runtime_facts) + @("task_syscall")
$candidate.checks.world_compare_session_drift.session_drift.affected_focus = @($candidate.checks.world_compare_session_drift.session_drift.affected_focus) + @("timer")

$candidate.checks.witness_session_failure_export.candidate_session_status = "collapsed"
$candidate.checks.witness_session_failure_export.session_drift.failure_codes = @($candidate.checks.witness_session_failure_export.session_drift.failure_codes) + @("thread_not_resumed")
$candidate.checks.witness_session_failure_export.session_drift.missing_runtime_facts = @($candidate.checks.witness_session_failure_export.session_drift.missing_runtime_facts) + @("task_syscall")
$candidate.checks.witness_session_failure_export.session_drift.affected_focus = @($candidate.checks.witness_session_failure_export.session_drift.affected_focus) + @("scheduler")

$candidate.violations = @("synthetic inspect compare drift", "session runtime regressed")
$candidate | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $candidateSummaryPath -Encoding utf8

$textOutput = @(
    & $inspectScript `
        -Summary $candidateSummaryPath `
        -BaselineSummary $baselineSummaryPath
)

Require-Line -Lines $textOutput -Needle "compare: changed=true result=ok->fail session=standing->collapsed"
Require-Line -Lines $textOutput -Needle "compare_runtime: regressed=task_syscall,handoff_continuity improved=-"
Require-Line -Lines $textOutput -Needle "compare_world_failure_codes: added=tick_not_observed removed=-"
Require-Line -Lines $textOutput -Needle "compare_world_missing_runtime_facts: added=task_syscall removed=-"
Require-Line -Lines $textOutput -Needle "compare_witness_failure_codes: added=thread_not_resumed removed=-"
Require-Line -Lines $textOutput -Needle "compare_witness_focus: added=scheduler removed=-"
Require-Line -Lines $textOutput -Needle "compare_violations: added=synthetic inspect compare drift,session runtime regressed removed=-"

$jsonText = & $inspectScript `
    -Summary $candidateSummaryPath `
    -BaselineSummary $baselineSummaryPath `
    -ShowNarratives `
    -AsJson
$json = $jsonText | ConvertFrom-Json

if (-not [bool]$json.comparison.changed) {
    throw "expected inspect comparison.changed=true"
}
if ([string]$json.comparison.result.current -ne "fail" -or [string]$json.comparison.result.baseline -ne "ok") {
    throw "unexpected inspect result comparison"
}
if ([string]$json.comparison.session.status.current -ne "collapsed" -or [string]$json.comparison.session.status.baseline -ne "standing") {
    throw "unexpected inspect session status comparison"
}
if (@($json.comparison.session.runtime.regressed) -notcontains "handoff_continuity") {
    throw "expected runtime regression handoff_continuity"
}
if (@($json.comparison.session.runtime.regressed) -notcontains "task_syscall") {
    throw "expected runtime regression task_syscall"
}
if (@($json.comparison.world_compare_session_drift.failure_codes.added) -notcontains "tick_not_observed") {
    throw "expected world compare failure code delta tick_not_observed"
}
if (@($json.comparison.witness_session_failure_export.failure_codes.added) -notcontains "thread_not_resumed") {
    throw "expected witness compare failure code delta thread_not_resumed"
}
if ([string]$json.comparison.witness_session_failure_export.candidate_session_status.current -ne "collapsed" -or
    [string]$json.comparison.witness_session_failure_export.candidate_session_status.baseline -ne "fail") {
    throw "unexpected witness candidate session status comparison"
}

Write-Host "==> inspect minimal kernel runtime session witness compare smoke"
Write-Host ("baseline={0}" -f $baselineSummaryPath)
Write-Host ("candidate={0}" -f $candidateSummaryPath)
Write-Host ("compare_changed={0}" -f [bool]$json.comparison.changed)
Write-Host ("runtime_regressed={0}" -f (@($json.comparison.session.runtime.regressed) -join ","))
Write-Host ("world_failure_codes_added={0}" -f (@($json.comparison.world_compare_session_drift.failure_codes.added) -join ","))
Write-Host ("witness_failure_codes_added={0}" -f (@($json.comparison.witness_session_failure_export.failure_codes.added) -join ","))
Write-Host "ok=1"
