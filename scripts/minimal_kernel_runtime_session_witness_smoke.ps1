param(
    [string]$OutputRoot = "",
    [string]$SummaryPath = "",
    [string]$ReportMarkdownPath = "",
    [string]$CheckTextPath = "",
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

function Ensure-ParentDirectory {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        Ensure-Directory -Path $parent
    }
}

function Get-OutputPath {
    param(
        [string]$ExplicitPath,
        [string]$OutputRootPath,
        [string]$DefaultFileName
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        return Resolve-FullPath -Path $ExplicitPath
    }

    return Join-Path $OutputRootPath $DefaultFileName
}

function Load-JsonFile {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path $Path)) {
        return $null
    }

    return (Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json)
}

function Get-SessionDriftView {
    param(
        $Summary
    )

    if ($null -eq $Summary -or
        $null -eq $Summary.collapse_surface -or
        $null -eq $Summary.collapse_surface.session_drift) {
        return $null
    }

    $drift = $Summary.collapse_surface.session_drift
    return [ordered]@{
        changed = [bool]$drift.changed
        regressed_sessions = @($drift.regressed_sessions)
        required_regressed_sessions = @($drift.required_regressed_sessions)
        affected_domains = @($drift.affected_domains)
        affected_focus = @($drift.affected_focus)
        missing_runtime_facts = @($drift.missing_runtime_facts)
        failure_codes = @($drift.failure_codes)
        narratives = @($drift.narratives)
    }
}

function Test-ArrayContains {
    param(
        [object[]]$Values,
        [string]$Expected
    )

    return (@($Values) | ForEach-Object { [string]$_ }) -contains $Expected
}

function Add-Violation {
    param(
        [System.Collections.Generic.List[string]]$Violations,
        [string]$Message
    )

    $Violations.Add($Message) | Out-Null
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$resolvedOutputRoot = if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    Join-Path $repoRoot "cmake-build-minimal-kernel-runtime-session-witness-smoke"
} else {
    Resolve-FullPath -Path $OutputRoot
}

Ensure-Directory -Path $resolvedOutputRoot

$summaryPathResolved = Get-OutputPath -ExplicitPath $SummaryPath -OutputRootPath $resolvedOutputRoot -DefaultFileName "summary.json"
$reportMarkdownPathResolved = Get-OutputPath -ExplicitPath $ReportMarkdownPath -OutputRootPath $resolvedOutputRoot -DefaultFileName "report.md"
$checkTextPathResolved = Get-OutputPath -ExplicitPath $CheckTextPath -OutputRootPath $resolvedOutputRoot -DefaultFileName "check.txt"

$sessionSmoke = Join-Path $PSScriptRoot "minimal_kernel_runtime_session_smoke.ps1"
$worldCompareSmoke = Join-Path $PSScriptRoot "system_compiler_world_compare_session_drift_smoke.ps1"
$witnessExportSmoke = Join-Path $PSScriptRoot "system_compiler_witness_session_failure_export_smoke.ps1"
$summaryValidator = Join-Path $PSScriptRoot "validate_minimal_kernel_runtime_session_witness_smoke.py"
$summaryGate = Join-Path $PSScriptRoot "check_minimal_kernel_runtime_session_witness_smoke_summary.ps1"

foreach ($script in @($sessionSmoke, $worldCompareSmoke, $witnessExportSmoke, $summaryValidator, $summaryGate)) {
    if (-not (Test-Path $script)) {
        throw "required session witness smoke not found: $script"
    }
}

$sessionOutputRoot = Join-Path $resolvedOutputRoot "session"
$worldCompareOutputRoot = Join-Path $resolvedOutputRoot "world_compare_session_drift"
$witnessExportOutputRoot = Join-Path $resolvedOutputRoot "witness_session_failure_export"

Push-Location $repoRoot
try {
    & $sessionSmoke `
        -OutputRoot $sessionOutputRoot `
        -PythonExe $PythonExe

    & $worldCompareSmoke `
        -OutputRoot $worldCompareOutputRoot `
        -PythonExe $PythonExe

    & $witnessExportSmoke `
        -OutputRoot $witnessExportOutputRoot `
        -PythonExe $PythonExe
} finally {
    Pop-Location
}

$sessionSummary = Join-Path $sessionOutputRoot "kernel_runtime_session.summary.json"
$sessionRuntimeLedger = Join-Path $sessionOutputRoot "runtime_ledger.json"
$sessionReport = Join-Path $sessionOutputRoot "report.md"
$sessionCheck = Join-Path $sessionOutputRoot "check.txt"
$worldCompareSummary = Join-Path $worldCompareOutputRoot "summary.json"
$worldCompareReport = Join-Path $worldCompareOutputRoot "report.md"
$worldCompareCheck = Join-Path $worldCompareOutputRoot "check.txt"
$witnessCompareSummary = Join-Path $witnessExportOutputRoot "world_compare\summary.json"
$witnessCompareReport = Join-Path $witnessExportOutputRoot "world_compare\report.md"
$witnessCompareCheck = Join-Path $witnessExportOutputRoot "world_compare\check.txt"
$witnessBaselineSummary = Join-Path $witnessExportOutputRoot "baseline\summary.json"
$witnessCandidateSummary = Join-Path $witnessExportOutputRoot "candidate\summary.json"

foreach ($path in @(
    $sessionSummary,
    $sessionRuntimeLedger,
    $sessionReport,
    $sessionCheck,
    $worldCompareSummary,
    $worldCompareReport,
    $worldCompareCheck,
    $witnessCompareSummary,
    $witnessCompareReport,
    $witnessCompareCheck,
    $witnessBaselineSummary,
    $witnessCandidateSummary
)) {
    if (-not (Test-Path $path)) {
        throw "missing session witness smoke artifact: $path"
    }
}

$sessionData = Load-JsonFile -Path $sessionSummary
$worldCompareData = Load-JsonFile -Path $worldCompareSummary
$witnessCompareData = Load-JsonFile -Path $witnessCompareSummary
$witnessCandidateData = Load-JsonFile -Path $witnessCandidateSummary

$worldCompareSessionDrift = Get-SessionDriftView -Summary $worldCompareData
$witnessCompareSessionDrift = Get-SessionDriftView -Summary $witnessCompareData

$violations = [System.Collections.Generic.List[string]]::new()

if ($null -eq $sessionData) {
    Add-Violation -Violations $violations -Message "missing or unreadable session summary"
} elseif ([string]$sessionData.verdict.session_status -ne "standing") {
    Add-Violation -Violations $violations -Message ("expected standing session, got {0}" -f [string]$sessionData.verdict.session_status)
} elseif (@($sessionData.failures).Count -ne 0) {
    Add-Violation -Violations $violations -Message ("standing session reported {0} failures" -f @($sessionData.failures).Count)
}

if ($null -eq $worldCompareData) {
    Add-Violation -Violations $violations -Message "missing or unreadable synthetic session drift world compare summary"
} elseif ([string]$worldCompareData.world_verdict -ne "collapsed") {
    Add-Violation -Violations $violations -Message ("expected synthetic session drift verdict collapsed, got {0}" -f [string]$worldCompareData.world_verdict)
}
if ($null -eq $worldCompareSessionDrift -or -not [bool]$worldCompareSessionDrift.changed) {
    Add-Violation -Violations $violations -Message "synthetic session drift was not projected"
} elseif (-not (Test-ArrayContains -Values $worldCompareSessionDrift.failure_codes -Expected "handoff_continuity_broken")) {
    Add-Violation -Violations $violations -Message "synthetic session drift did not include handoff_continuity_broken"
}

if ($null -eq $witnessCompareData) {
    Add-Violation -Violations $violations -Message "missing or unreadable witness-export session drift world compare summary"
} elseif ([string]$witnessCompareData.world_verdict -ne "collapsed") {
    Add-Violation -Violations $violations -Message ("expected witness-export session drift verdict collapsed, got {0}" -f [string]$witnessCompareData.world_verdict)
}
if ($null -eq $witnessCompareSessionDrift -or -not [bool]$witnessCompareSessionDrift.changed) {
    Add-Violation -Violations $violations -Message "witness-export session drift was not projected"
} elseif (-not (Test-ArrayContains -Values $witnessCompareSessionDrift.failure_codes -Expected "handoff_continuity_broken")) {
    Add-Violation -Violations $violations -Message "witness-export session drift did not include handoff_continuity_broken"
}

$candidateSessionEntry = $null
if ($null -ne $witnessCandidateData) {
    $candidateSessionEntry = $witnessCandidateData.witness_entries |
        Where-Object { [string]$_.kind -eq "kernel_runtime_session" } |
        Select-Object -First 1
}
if ($null -eq $candidateSessionEntry) {
    Add-Violation -Violations $violations -Message "candidate witness bundle did not include kernel_runtime_session"
} elseif ([string]$candidateSessionEntry.status -ne "fail") {
    Add-Violation -Violations $violations -Message ("expected candidate kernel_runtime_session status fail, got {0}" -f [string]$candidateSessionEntry.status)
}

$summaryObject = [ordered]@{
    schema = "minimal_kernel.runtime_session_witness_smoke/v0"
    kind = "minimal_kernel.runtime_session_witness_smoke"
    generated_at = (Get-Date).ToUniversalTime().ToString("o")
    result = if ($violations.Count -eq 0) { "ok" } else { "fail" }
    output_root = $resolvedOutputRoot
    artifacts = [ordered]@{
        summary = $summaryPathResolved
        report_markdown = $reportMarkdownPathResolved
        check_text = $checkTextPathResolved
        session = [ordered]@{
            output_root = $sessionOutputRoot
            summary = $sessionSummary
            runtime_ledger = $sessionRuntimeLedger
            report_markdown = $sessionReport
            check_text = $sessionCheck
        }
        world_compare_session_drift = [ordered]@{
            output_root = $worldCompareOutputRoot
            summary = $worldCompareSummary
            report_markdown = $worldCompareReport
            check_text = $worldCompareCheck
        }
        witness_session_failure_export = [ordered]@{
            output_root = $witnessExportOutputRoot
            baseline_summary = $witnessBaselineSummary
            candidate_summary = $witnessCandidateSummary
            world_compare_summary = $witnessCompareSummary
            world_compare_report_markdown = $witnessCompareReport
            world_compare_check_text = $witnessCompareCheck
        }
    }
    checks = [ordered]@{
        session = if ($null -eq $sessionData) {
            $null
        } else {
            [ordered]@{
                session_id = [string]$sessionData.session_id
                world = [string]$sessionData.world
                session_status = [string]$sessionData.verdict.session_status
                failure_domain = if ($null -eq $sessionData.verdict.failure_domain) { $null } else { [string]$sessionData.verdict.failure_domain }
                failure_count = @($sessionData.failures).Count
                runtime = [ordered]@{
                    tick = [bool]$sessionData.runtime.tick
                    trap = [bool]$sessionData.runtime.trap
                    thread = [bool]$sessionData.runtime.thread
                    task_syscall = [bool]$sessionData.runtime.task_syscall
                    handoff_continuity = [bool]$sessionData.runtime.handoff_continuity
                }
                ledger_event_count = if ($null -eq $sessionData.ledger) { 0 } else { [int]$sessionData.ledger.event_count }
            }
        }
        world_compare_session_drift = [ordered]@{
            result = if ($null -eq $worldCompareData) { $null } else { [string]$worldCompareData.result }
            world_verdict = if ($null -eq $worldCompareData) { $null } else { [string]$worldCompareData.world_verdict }
            regression_count = if ($null -eq $worldCompareData) { 0 } else { [int]$worldCompareData.witness_summary.regression_count }
            required_regression_count = if ($null -eq $worldCompareData) { 0 } else { [int]$worldCompareData.witness_summary.required_regression_count }
            session_drift = $worldCompareSessionDrift
        }
        witness_session_failure_export = [ordered]@{
            result = if ($null -eq $witnessCompareData) { $null } else { [string]$witnessCompareData.result }
            world_verdict = if ($null -eq $witnessCompareData) { $null } else { [string]$witnessCompareData.world_verdict }
            regression_count = if ($null -eq $witnessCompareData) { 0 } else { [int]$witnessCompareData.witness_summary.regression_count }
            required_regression_count = if ($null -eq $witnessCompareData) { 0 } else { [int]$witnessCompareData.witness_summary.required_regression_count }
            candidate_session_status = if ($null -eq $candidateSessionEntry) { $null } else { [string]$candidateSessionEntry.status }
            session_drift = $witnessCompareSessionDrift
        }
    }
    violations = @($violations)
}

Ensure-ParentDirectory -Path $summaryPathResolved
$summaryObject | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $summaryPathResolved -Encoding utf8

$reportBuilder = [System.Text.StringBuilder]::new()
[void]$reportBuilder.AppendLine("# Minimal Kernel Runtime Session Witness Smoke")
[void]$reportBuilder.AppendLine("")
[void]$reportBuilder.AppendLine(('- Generated at: `{0}`' -f [string]$summaryObject.generated_at))
[void]$reportBuilder.AppendLine(('- Result: `{0}`' -f [string]$summaryObject.result))
[void]$reportBuilder.AppendLine(('- Output root: `{0}`' -f $resolvedOutputRoot))
[void]$reportBuilder.AppendLine(('- Summary JSON: `{0}`' -f $summaryPathResolved))
[void]$reportBuilder.AppendLine("")
[void]$reportBuilder.AppendLine("## Session Export")
if ($null -ne $summaryObject.checks.session) {
    [void]$reportBuilder.AppendLine(('- Session: `{0}` in world `{1}`' -f [string]$summaryObject.checks.session.session_id, [string]$summaryObject.checks.session.world))
    [void]$reportBuilder.AppendLine(('- Status: `{0}` failures=`{1}` ledger_events=`{2}`' -f [string]$summaryObject.checks.session.session_status, [int]$summaryObject.checks.session.failure_count, [int]$summaryObject.checks.session.ledger_event_count))
    [void]$reportBuilder.AppendLine(('- Runtime facts: `tick={0} trap={1} thread={2} task_syscall={3} handoff_continuity={4}`' -f [bool]$summaryObject.checks.session.runtime.tick, [bool]$summaryObject.checks.session.runtime.trap, [bool]$summaryObject.checks.session.runtime.thread, [bool]$summaryObject.checks.session.runtime.task_syscall, [bool]$summaryObject.checks.session.runtime.handoff_continuity))
}
[void]$reportBuilder.AppendLine(('- Session summary: `{0}`' -f $sessionSummary))
[void]$reportBuilder.AppendLine(('- Runtime ledger: `{0}`' -f $sessionRuntimeLedger))
[void]$reportBuilder.AppendLine("")
[void]$reportBuilder.AppendLine("## Session Drift Projection")
if ($null -ne $worldCompareSessionDrift) {
    [void]$reportBuilder.AppendLine(('- Verdict: `{0}` regressions=`{1}` required=`{2}`' -f [string]$summaryObject.checks.world_compare_session_drift.world_verdict, [int]$summaryObject.checks.world_compare_session_drift.regression_count, [int]$summaryObject.checks.world_compare_session_drift.required_regression_count))
    [void]$reportBuilder.AppendLine(('- Domains: `{0}`' -f (@($worldCompareSessionDrift.affected_domains) -join ",")))
    [void]$reportBuilder.AppendLine(('- Focus: `{0}`' -f (@($worldCompareSessionDrift.affected_focus) -join ",")))
    [void]$reportBuilder.AppendLine(('- Failure codes: `{0}`' -f (@($worldCompareSessionDrift.failure_codes) -join ",")))
}
[void]$reportBuilder.AppendLine(('- World compare summary: `{0}`' -f $worldCompareSummary))
[void]$reportBuilder.AppendLine("")
[void]$reportBuilder.AppendLine("## Witness Export Failure Projection")
if ($null -ne $witnessCompareSessionDrift) {
    [void]$reportBuilder.AppendLine(('- Verdict: `{0}` regressions=`{1}` required=`{2}` candidate_session=`{3}`' -f [string]$summaryObject.checks.witness_session_failure_export.world_verdict, [int]$summaryObject.checks.witness_session_failure_export.regression_count, [int]$summaryObject.checks.witness_session_failure_export.required_regression_count, [string]$summaryObject.checks.witness_session_failure_export.candidate_session_status))
    [void]$reportBuilder.AppendLine(('- Domains: `{0}`' -f (@($witnessCompareSessionDrift.affected_domains) -join ",")))
    [void]$reportBuilder.AppendLine(('- Focus: `{0}`' -f (@($witnessCompareSessionDrift.affected_focus) -join ",")))
    [void]$reportBuilder.AppendLine(('- Missing runtime facts: `{0}`' -f (@($witnessCompareSessionDrift.missing_runtime_facts) -join ",")))
    [void]$reportBuilder.AppendLine(('- Failure codes: `{0}`' -f (@($witnessCompareSessionDrift.failure_codes) -join ",")))
}
[void]$reportBuilder.AppendLine(('- Candidate witness summary: `{0}`' -f $witnessCandidateSummary))
[void]$reportBuilder.AppendLine(('- Witness world compare summary: `{0}`' -f $witnessCompareSummary))

if ($violations.Count -gt 0) {
    [void]$reportBuilder.AppendLine("")
    [void]$reportBuilder.AppendLine("## Violations")
    foreach ($message in @($violations)) {
        [void]$reportBuilder.AppendLine(("- {0}" -f [string]$message))
    }
}

Ensure-ParentDirectory -Path $reportMarkdownPathResolved
Set-Content -LiteralPath $reportMarkdownPathResolved -Encoding utf8 ($reportBuilder.ToString())

$checkBuilder = [System.Text.StringBuilder]::new()
[void]$checkBuilder.AppendLine(("summary: {0}" -f $summaryPathResolved))
[void]$checkBuilder.AppendLine(("result: {0}" -f [string]$summaryObject.result))
if ($null -ne $summaryObject.checks.session) {
    [void]$checkBuilder.AppendLine(("session_status: {0}" -f [string]$summaryObject.checks.session.session_status))
    [void]$checkBuilder.AppendLine(("session_failures: {0}" -f [int]$summaryObject.checks.session.failure_count))
    [void]$checkBuilder.AppendLine(("session_runtime: tick={0} trap={1} thread={2} task_syscall={3} handoff_continuity={4}" -f [bool]$summaryObject.checks.session.runtime.tick, [bool]$summaryObject.checks.session.runtime.trap, [bool]$summaryObject.checks.session.runtime.thread, [bool]$summaryObject.checks.session.runtime.task_syscall, [bool]$summaryObject.checks.session.runtime.handoff_continuity))
}
[void]$checkBuilder.AppendLine(("world_compare_session_drift: verdict={0} changed={1} failures={2}" -f [string]$summaryObject.checks.world_compare_session_drift.world_verdict, [bool]$worldCompareSessionDrift.changed, (@($worldCompareSessionDrift.failure_codes) -join ",")))
[void]$checkBuilder.AppendLine(("witness_session_failure_export: verdict={0} changed={1} candidate_session={2} failures={3}" -f [string]$summaryObject.checks.witness_session_failure_export.world_verdict, [bool]$witnessCompareSessionDrift.changed, [string]$summaryObject.checks.witness_session_failure_export.candidate_session_status, (@($witnessCompareSessionDrift.failure_codes) -join ",")))
if ($violations.Count -gt 0) {
    [void]$checkBuilder.AppendLine("violations:")
    foreach ($message in @($violations)) {
        [void]$checkBuilder.AppendLine(("- {0}" -f [string]$message))
    }
}

Ensure-ParentDirectory -Path $checkTextPathResolved
Set-Content -LiteralPath $checkTextPathResolved -Encoding utf8 ($checkBuilder.ToString())

Push-Location $repoRoot
try {
    & $PythonExe $summaryValidator --summary $summaryPathResolved
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    & $summaryGate `
        -Summary $summaryPathResolved `
        -RequireResult "ok" `
        -RequireSessionStatus "standing" `
        -RequireWorldCompareVerdict "collapsed" `
        -RequireWitnessCompareVerdict "collapsed" `
        -RequireSessionDrift "true" `
        -RequireSessionFailureCode @("handoff_continuity_broken") `
        -RequireMissingRuntimeFact @("handoff") `
        -RequireSessionFocus @("session", "runtime", "handoff", "continuity") `
        -MaxViolations 0
} finally {
    Pop-Location
}

Write-Host "==> minimal kernel runtime session witness smoke"
Write-Host ("output_root={0}" -f $resolvedOutputRoot)
Write-Host ("summary={0}" -f $summaryPathResolved)
Write-Host ("report={0}" -f $reportMarkdownPathResolved)
Write-Host ("check={0}" -f $checkTextPathResolved)
Write-Host ("session_summary={0}" -f $sessionSummary)
Write-Host ("world_compare_session_drift={0}" -f $worldCompareSummary)
Write-Host ("witness_failure_export_compare={0}" -f $witnessCompareSummary)
if ($violations.Count -gt 0) {
    throw "minimal kernel runtime session witness smoke failed"
}
Write-Host "ok=1"
