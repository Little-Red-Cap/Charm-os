param(
    [string]$Summary = "",
    [switch]$ShowArtifacts,
    [switch]$ShowNarratives,
    [switch]$AsJson
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

function Get-SummaryPath {
    param(
        [string]$Path
    )

    if (-not [string]::IsNullOrWhiteSpace($Path)) {
        return Resolve-FullPath -Path $Path
    }

    $repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
    return Join-Path $repoRoot "out\minimal-kernel-runtime-session-witness-smoke\summary.json"
}

function Load-SmokeSummary {
    param(
        [string]$Path
    )

    if (-not (Test-Path $Path)) {
        throw "summary not found: $Path"
    }

    $data = Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json
    if ([string]$data.schema -ne "minimal_kernel.runtime_session_witness_smoke/v0") {
        throw "unsupported runtime session witness smoke schema: $([string]$data.schema)"
    }

    return [pscustomobject]@{
        Path = $Path
        Data = $data
    }
}

function Get-StringArray {
    param(
        [AllowNull()]
        [object[]]$Values
    )

    return @(
        @($Values) |
            Where-Object { $null -ne $_ -and -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [string]$_ }
    )
}

function Format-StringArrayOrDash {
    param(
        [AllowNull()]
        [object[]]$Values
    )

    $strings = Get-StringArray -Values $Values
    if ($strings.Count -eq 0) {
        return "-"
    }

    return ($strings -join ",")
}

function Format-BoolToken {
    param(
        [bool]$Value
    )

    if ($Value) {
        return "true"
    }

    return "false"
}

function Format-NullableString {
    param(
        [AllowNull()]
        [string]$Value
    )

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return "-"
    }

    return $Value
}

function New-SessionView {
    param(
        $SessionCheck
    )

    if ($null -eq $SessionCheck) {
        return $null
    }

    return [pscustomobject][ordered]@{
        session_id = [string]$SessionCheck.session_id
        world = [string]$SessionCheck.world
        session_status = [string]$SessionCheck.session_status
        failure_domain = if ($null -eq $SessionCheck.failure_domain) { $null } else { [string]$SessionCheck.failure_domain }
        failure_count = [int]$SessionCheck.failure_count
        ledger_event_count = [int]$SessionCheck.ledger_event_count
        runtime = [pscustomobject][ordered]@{
            tick = [bool]$SessionCheck.runtime.tick
            trap = [bool]$SessionCheck.runtime.trap
            thread = [bool]$SessionCheck.runtime.thread
            task_syscall = [bool]$SessionCheck.runtime.task_syscall
            handoff_continuity = [bool]$SessionCheck.runtime.handoff_continuity
        }
    }
}

function New-SessionDriftView {
    param(
        $SessionDrift
    )

    if ($null -eq $SessionDrift) {
        return $null
    }

    return [pscustomobject][ordered]@{
        changed = [bool]$SessionDrift.changed
        regressed_sessions = @(Get-StringArray -Values $SessionDrift.regressed_sessions)
        required_regressed_sessions = @(Get-StringArray -Values $SessionDrift.required_regressed_sessions)
        affected_domains = @(Get-StringArray -Values $SessionDrift.affected_domains)
        affected_focus = @(Get-StringArray -Values $SessionDrift.affected_focus)
        missing_runtime_facts = @(Get-StringArray -Values $SessionDrift.missing_runtime_facts)
        failure_codes = @(Get-StringArray -Values $SessionDrift.failure_codes)
        narratives = @(Get-StringArray -Values $SessionDrift.narratives)
    }
}

function New-CompareView {
    param(
        $CompareCheck
    )

    if ($null -eq $CompareCheck) {
        return $null
    }

    $view = [ordered]@{
        result = if ($null -eq $CompareCheck.result) { $null } else { [string]$CompareCheck.result }
        world_verdict = if ($null -eq $CompareCheck.world_verdict) { $null } else { [string]$CompareCheck.world_verdict }
        regression_count = [int]$CompareCheck.regression_count
        required_regression_count = [int]$CompareCheck.required_regression_count
        session_drift = New-SessionDriftView -SessionDrift $CompareCheck.session_drift
    }

    if ($null -ne $CompareCheck.PSObject.Properties["candidate_session_status"]) {
        $view.candidate_session_status = if ($null -eq $CompareCheck.candidate_session_status) {
            $null
        } else {
            [string]$CompareCheck.candidate_session_status
        }
    }

    return [pscustomobject]$view
}

function New-ArtifactsView {
    param(
        $Artifacts
    )

    if ($null -eq $Artifacts) {
        return $null
    }

    return [pscustomobject][ordered]@{
        summary = [string]$Artifacts.summary
        report_markdown = [string]$Artifacts.report_markdown
        check_text = [string]$Artifacts.check_text
        session = [pscustomobject][ordered]@{
            output_root = [string]$Artifacts.session.output_root
            summary = [string]$Artifacts.session.summary
            runtime_ledger = [string]$Artifacts.session.runtime_ledger
            report_markdown = [string]$Artifacts.session.report_markdown
            check_text = [string]$Artifacts.session.check_text
        }
        world_compare_session_drift = [pscustomobject][ordered]@{
            output_root = [string]$Artifacts.world_compare_session_drift.output_root
            summary = [string]$Artifacts.world_compare_session_drift.summary
            report_markdown = [string]$Artifacts.world_compare_session_drift.report_markdown
            check_text = [string]$Artifacts.world_compare_session_drift.check_text
        }
        witness_session_failure_export = [pscustomobject][ordered]@{
            output_root = [string]$Artifacts.witness_session_failure_export.output_root
            baseline_summary = [string]$Artifacts.witness_session_failure_export.baseline_summary
            candidate_summary = [string]$Artifacts.witness_session_failure_export.candidate_summary
            world_compare_summary = [string]$Artifacts.witness_session_failure_export.world_compare_summary
            world_compare_report_markdown = [string]$Artifacts.witness_session_failure_export.world_compare_report_markdown
            world_compare_check_text = [string]$Artifacts.witness_session_failure_export.world_compare_check_text
        }
    }
}

function New-JsonView {
    param(
        [string]$SummaryPath,
        $SummaryData
    )

    return [pscustomobject][ordered]@{
        summary_path = $SummaryPath
        result = [string]$SummaryData.result
        output_root = [string]$SummaryData.output_root
        session = New-SessionView -SessionCheck $SummaryData.checks.session
        world_compare_session_drift = New-CompareView -CompareCheck $SummaryData.checks.world_compare_session_drift
        witness_session_failure_export = New-CompareView -CompareCheck $SummaryData.checks.witness_session_failure_export
        violations = @(Get-StringArray -Values $SummaryData.violations)
        artifacts = New-ArtifactsView -Artifacts $SummaryData.artifacts
    }
}

function Write-CompareSection {
    param(
        [string]$Name,
        $CompareView,
        [switch]$IncludeNarratives
    )

    if ($null -eq $CompareView) {
        Write-Output ("{0}: -" -f $Name)
        return
    }

    $drift = $CompareView.session_drift
    $changed = $false
    if ($null -ne $drift) {
        $changed = [bool]$drift.changed
    }

    $header = "{0}: verdict={1} regressions={2} required={3} changed={4}" -f `
        $Name, `
        (Format-NullableString -Value $CompareView.world_verdict), `
        [int]$CompareView.regression_count, `
        [int]$CompareView.required_regression_count, `
        (Format-BoolToken -Value $changed)

    if ($null -ne $CompareView.PSObject.Properties["candidate_session_status"]) {
        $header += " candidate_session={0}" -f (Format-NullableString -Value $CompareView.candidate_session_status)
    }

    Write-Output $header

    if ($null -eq $drift) {
        return
    }

    Write-Output ("{0}_domains: {1}" -f $Name, (Format-StringArrayOrDash -Values $drift.affected_domains))
    Write-Output ("{0}_focus: {1}" -f $Name, (Format-StringArrayOrDash -Values $drift.affected_focus))
    Write-Output ("{0}_failure_codes: {1}" -f $Name, (Format-StringArrayOrDash -Values $drift.failure_codes))
    Write-Output ("{0}_missing_runtime_facts: {1}" -f $Name, (Format-StringArrayOrDash -Values $drift.missing_runtime_facts))
    Write-Output ("{0}_regressed_sessions: {1}" -f $Name, (Format-StringArrayOrDash -Values $drift.regressed_sessions))

    if ($IncludeNarratives) {
        Write-Output ("{0}_narratives: {1}" -f $Name, (Format-StringArrayOrDash -Values $drift.narratives))
    }
}

$summaryPath = Get-SummaryPath -Path $Summary
$loaded = Load-SmokeSummary -Path $summaryPath
$view = New-JsonView -SummaryPath $summaryPath -SummaryData $loaded.Data

if ($AsJson) {
    $view | ConvertTo-Json -Depth 10
    exit 0
}

Write-Output ("summary: {0}" -f $summaryPath)
Write-Output ("result: {0}" -f [string]$view.result)
Write-Output ("output_root: {0}" -f [string]$view.output_root)

if ($null -ne $view.session) {
    Write-Output ("session: {0}|world={1}|status={2}|failures={3}|ledger_events={4}" -f `
        [string]$view.session.session_id,
        [string]$view.session.world,
        [string]$view.session.session_status,
        [int]$view.session.failure_count,
        [int]$view.session.ledger_event_count)
    Write-Output ("session_failure_domain: {0}" -f (Format-NullableString -Value $view.session.failure_domain))
    Write-Output ("runtime: tick={0} trap={1} thread={2} task_syscall={3} handoff_continuity={4}" -f `
        (Format-BoolToken -Value ([bool]$view.session.runtime.tick)),
        (Format-BoolToken -Value ([bool]$view.session.runtime.trap)),
        (Format-BoolToken -Value ([bool]$view.session.runtime.thread)),
        (Format-BoolToken -Value ([bool]$view.session.runtime.task_syscall)),
        (Format-BoolToken -Value ([bool]$view.session.runtime.handoff_continuity)))
} else {
    Write-Output "session: -"
}

Write-Output ""
Write-CompareSection -Name "world_compare_session_drift" -CompareView $view.world_compare_session_drift -IncludeNarratives:$ShowNarratives
Write-Output ""
Write-CompareSection -Name "witness_session_failure_export" -CompareView $view.witness_session_failure_export -IncludeNarratives:$ShowNarratives
Write-Output ""
Write-Output ("violations: {0}" -f @($view.violations).Count)

if (@($view.violations).Count -gt 0) {
    foreach ($violation in @($view.violations)) {
        Write-Output ("violation: {0}" -f [string]$violation)
    }
}

if ($ShowArtifacts -and $null -ne $view.artifacts) {
    Write-Output ""
    Write-Output "artifacts:"
    Write-Output ("  summary={0}" -f [string]$view.artifacts.summary)
    Write-Output ("  report_markdown={0}" -f [string]$view.artifacts.report_markdown)
    Write-Output ("  check_text={0}" -f [string]$view.artifacts.check_text)
    Write-Output ("  session.summary={0}" -f [string]$view.artifacts.session.summary)
    Write-Output ("  session.runtime_ledger={0}" -f [string]$view.artifacts.session.runtime_ledger)
    Write-Output ("  world_compare_session_drift.summary={0}" -f [string]$view.artifacts.world_compare_session_drift.summary)
    Write-Output ("  witness_session_failure_export.baseline_summary={0}" -f [string]$view.artifacts.witness_session_failure_export.baseline_summary)
    Write-Output ("  witness_session_failure_export.candidate_summary={0}" -f [string]$view.artifacts.witness_session_failure_export.candidate_summary)
    Write-Output ("  witness_session_failure_export.world_compare_summary={0}" -f [string]$view.artifacts.witness_session_failure_export.world_compare_summary)
}
