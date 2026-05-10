param(
    [string]$OpenEventWitnessRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-open-event-witness-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opener-open-event-witness-smoke",
    [string]$PythonExe = "",
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

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    if (-not (Test-Path -LiteralPath $Path)) {
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

    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
}

function Resolve-ToolPath {
    param(
        [string[]]$Candidates
    )

    foreach ($candidate in $Candidates) {
        $command = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($null -ne $command) {
            return $command.Source
        }
    }

    throw "tool not found: $($Candidates -join ', ')"
}

function Invoke-ExternalTool {
    param(
        [string]$Executable,
        [string[]]$ArgumentList,
        [string]$FailureMessage
    )

    Write-Host ("==> {0}" -f [System.IO.Path]::GetFileName($Executable))

    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & $Executable @ArgumentList
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    if ($exitCode -ne 0) {
        throw ("{0} (exit code {1})" -f $FailureMessage, $exitCode)
    }
}

function Load-JsonObject {
    param(
        [string]$Path
    )

    return (Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json)
}

function Write-TextFile {
    param(
        [string]$Path,
        [string]$Content
    )

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        Ensure-Directory -Path $parent
    }

    Set-Content -LiteralPath $Path -Encoding utf8 -Value $Content
}

function Write-JsonFile {
    param(
        [string]$Path,
        $Value
    )

    $json = $Value | ConvertTo-Json -Depth 100
    Write-TextFile -Path $Path -Content ($json + [Environment]::NewLine)
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

function New-EntryRef {
    param(
        [string]$SummaryPath,
        [string]$ReportPath,
        [string]$CheckPath
    )

    return [ordered]@{
        route_id = "open-event-witness-route"
        depth = 0
        surface_id = "open_event_witness"
        label = "Open event witness"
        role = "supporting_testimony"
        summary_schema = "system_compiler.front_page_entry_opening_flow_open_event_witness/v0"
        summary_kind = "system_compiler.front_page_entry_opening_flow_open_event_witness"
        summary_path = $SummaryPath
        report_markdown_path = $ReportPath
        check_text_path = $CheckPath
        revisit = $false
        cycle = $false
        expanded = $true
        route_provenance_count = 1
        supporting_surface_count = 0
    }
}

function New-QueryHint {
    param(
        [string]$TabId
    )

    return [ordered]@{
        tab_id = $TabId
        tab_title = "Open Event Witness"
        entry_role = "supporting_testimony"
        summary_schema = "system_compiler.front_page_entry_opening_flow_open_event_witness/v0"
        summary_kind = "system_compiler.front_page_entry_opening_flow_open_event_witness"
        scope = "report"
        selection_rule = "single_report"
        query_kind = "default_overview"
        compare_expected = $false
        followup_query_kinds = @("evidence_refs", "explanation")
        rationale = "Open the open-event witness as the nearest explainable testimony for the runtime-session opening judgment."
    }
}

function New-MinimalLandingSummary {
    param(
        [string]$LandingPath,
        [string]$TargetSummaryPath,
        [string]$TargetReportPath,
        [string]$TargetCheckPath
    )

    $caseRoot = Split-Path -Parent $LandingPath
    Ensure-Directory -Path $caseRoot
    $landingReportPath = Join-Path $caseRoot "front-page.entry-landing.report.md"
    $landingCheckPath = Join-Path $caseRoot "front-page.entry-landing.check.txt"
    Write-TextFile -Path $landingReportPath -Content "# Synthetic Open Event Witness Landing`n"
    Write-TextFile -Path $landingCheckPath -Content "synthetic open event witness landing`n"

    $landingPath = Resolve-FullPath -Path $LandingPath
    $landingReportPath = Resolve-FullPath -Path $landingReportPath
    $landingCheckPath = Resolve-FullPath -Path $landingCheckPath
    $targetSummaryPath = Resolve-FullPath -Path $TargetSummaryPath
    $targetReportPath = Resolve-FullPath -Path $TargetReportPath
    $targetCheckPath = Resolve-FullPath -Path $TargetCheckPath
    $outputRoot = Resolve-FullPath -Path $caseRoot
    $tabId = "open_event_witness"
    $entry = New-EntryRef -SummaryPath $targetSummaryPath -ReportPath $targetReportPath -CheckPath $targetCheckPath
    $query = New-QueryHint -TabId $tabId
    $landingTab = [ordered]@{
        tab_id = $tabId
        title = "Open Event Witness"
        capability_ids = @("system_compiler.open_event_witness")
        entry = $entry
    }

    $summary = [ordered]@{
        schema = "system_compiler.front_page_entry_landing/v0"
        kind = "system_compiler.front_page_entry_landing"
        generated_at_utc = "2026-05-05T00:00:00Z"
        generator = "scripts/system_compiler_front_page_entry_opener_open_event_witness_smoke.ps1"
        result = "ok"
        entry_landing = [ordered]@{
            title = "Synthetic Open Event Witness Landing"
            summary = "A targeted landing that proves opener projection for OpenEventWitness."
        }
        front_page = [ordered]@{
            summary_path = $landingPath
            report_markdown_path = $landingReportPath
            check_text_path = $landingCheckPath
            supporting_surfaces = @(
                [ordered]@{
                    id = "open_event_witness"
                    label = "open event witness"
                    role = "supporting_testimony"
                    summary_schema = "system_compiler.front_page_entry_opening_flow_open_event_witness/v0"
                    summary_path = $targetSummaryPath
                    report_markdown_path = $targetReportPath
                    check_text_path = $targetCheckPath
                }
            )
        }
        route_provenance = @(
            [ordered]@{
                id = "open-event-witness-route"
                route_kind = "synthetic_targeted_smoke"
                source_summary_schema = "system_compiler.front_page_entry_opening_flow_open_event_witness/v0"
                source_summary_path = $targetSummaryPath
                source_input_summary_path = $targetSummaryPath
                source_root_summary_path = $targetSummaryPath
                source_report_markdown_path = $targetReportPath
                source_check_text_path = $targetCheckPath
                level1_surface_ids = @("open_event_witness")
            }
        )
        artifact_context = [ordered]@{
            input_capability_summary_path = $targetSummaryPath
            output_root = $outputRoot
            landing_summary_path = $landingPath
            report_markdown_path = $landingReportPath
            check_text_path = $landingCheckPath
        }
        root_surface = [ordered]@{
            surface_id = "open_event_witness"
            label = "open event witness"
            role = "supporting_testimony"
            summary_schema = "system_compiler.front_page_entry_opening_flow_open_event_witness/v0"
            summary_kind = "system_compiler.front_page_entry_opening_flow_open_event_witness"
            summary_path = $targetSummaryPath
        }
        landing_status = [ordered]@{
            landing_result = "ok"
            recommended_entry_mode = "evidence"
            entry_tier = "evidence_only"
            opening_reason = [ordered]@{
                kind = "supporting_evidence"
                summary = "OpenEventWitness is available as the nearest testimony target for the runtime-session opening judgment."
                source_summary_path = $targetSummaryPath
                drift_changed = $true
                drift_verdict = "drifted"
            }
            primary_tab_id = $tabId
            primary_summary_schema = "system_compiler.front_page_entry_opening_flow_open_event_witness/v0"
            primary_summary_kind = "system_compiler.front_page_entry_opening_flow_open_event_witness"
            available_tab_ids = @($tabId)
            fallback_tab_ids = @()
            tab_count = 1
            fallback_tab_count = 0
            provenance_root_count = 1
            route_provenance_entry_count = 1
            direct_review_available = $false
            direct_compare_available = $false
            direct_biography_available = $false
            direct_evidence_available = $true
            direct_runtime_session_available = $false
        }
        fallback_mode_order = @("evidence", "route")
        primary_landing = $landingTab
        secondary_landings = @()
        landing_tabs = @($landingTab)
        provenance_roots = @(
            [ordered]@{
                root_id = "open_event_witness"
                root_kind = "open_event_witness"
                source_summary_schema = "system_compiler.front_page_entry_opening_flow_open_event_witness/v0"
                source_summary_path = $targetSummaryPath
                source_front_page_summary_path = $targetSummaryPath
                owner_route_ids = @("open-event-witness-route")
                owner_surface_ids = @("open_event_witness")
                available_supporting_surface_ids = @("open_event_witness")
            }
        )
        query_hints = [ordered]@{
            primary_query = $query
            tab_queries = @($query)
        }
        questions = [ordered]@{
            compare_questions = @("Should this OpenEventWitness projection be shown before opening the selected workspace facade?")
            next_questions = @("Should evidence refs and opening input refs become the next opener follow-up?")
        }
        violations = @()
    }

    Write-JsonFile -Path $landingPath -Value $summary
    return $landingPath
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$openEventWitnessRootPath = Resolve-FullPath -Path $OpenEventWitnessRoot
$outputRootPath = Resolve-FullPath -Path $OutputRoot

if ($Clean) {
    Remove-PathIfExists -Path $outputRootPath
}
Ensure-Directory -Path $outputRootPath

$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}
$powerShellExe = Resolve-ToolPath -Candidates @("powershell.exe", "pwsh.exe", "powershell", "pwsh")

$openEventWitnessSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_open_event_witness_smoke.ps1"
$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opener.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opener.py"
foreach ($requiredPath in @($openEventWitnessSmokeScript, $exportScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $witnessRoot = Join-Path $openEventWitnessRootPath "witness"
    $witnessSummaryPath = Join-Path $witnessRoot "front-page.entry-opening-flow.open-event.witness.summary.json"
    $witnessReportPath = Join-Path $witnessRoot "front-page.entry-opening-flow.open-event.witness.report.md"
    $witnessCheckPath = Join-Path $witnessRoot "front-page.entry-opening-flow.open-event.witness.check.txt"

    if ($Clean -or -not (Test-Path -LiteralPath $witnessSummaryPath)) {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $openEventWitnessSmokeScript,
                "-OutputRoot",
                $openEventWitnessRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "runtime-session open-event witness smoke bootstrap failed"
    } else {
        Write-Host "[FRONT-PAGE-ENTRY-OPENER-OPEN-EVENT-WITNESS-SMOKE] open_event_witness_bootstrap=reuse-existing"
    }

    foreach ($requiredArtifact in @($witnessSummaryPath, $witnessReportPath, $witnessCheckPath)) {
        if (-not (Test-Path -LiteralPath $requiredArtifact)) {
            throw "missing open-event witness artifact: $requiredArtifact"
        }
    }

    $landingRoot = Join-Path $outputRootPath "_synthetic_landing"
    $landingPath = New-MinimalLandingSummary `
        -LandingPath (Join-Path $landingRoot "front-page.entry-landing.summary.json") `
        -TargetSummaryPath $witnessSummaryPath `
        -TargetReportPath $witnessReportPath `
        -TargetCheckPath $witnessCheckPath

    $caseOutputRoot = Join-Path $outputRootPath "open-event-witness"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $exportScript,
            "--landing",
            $landingPath,
            "--output-root",
            $caseOutputRoot
        ) `
        -FailureMessage "front page entry opener export failed for open-event witness"

    $openerSummaryPath = Join-Path $caseOutputRoot "front-page.entry-opener.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateScript, "--summary", $openerSummaryPath) `
        -FailureMessage "front page entry opener validation failed for open-event witness"

    $openerSummary = Load-JsonObject -Path $openerSummaryPath
    Assert-Condition `
        -Condition ([string]$openerSummary.open_action.status -eq "ready") `
        -Message ("expected open action ready but got '{0}'" -f $openerSummary.open_action.status)
    Assert-Condition `
        -Condition ([string]$openerSummary.open_action.selected_tab_id -eq "open_event_witness") `
        -Message ("expected selected tab open_event_witness but got '{0}'" -f $openerSummary.open_action.selected_tab_id)
    Assert-Condition `
        -Condition ([bool]$openerSummary.inspector_invocation.ready -eq $false) `
        -Message "expected inspector invocation to stay blocked for non-artifact-report target"
    Assert-Condition `
        -Condition ([string]$openerSummary.opened_projection.status -eq "available") `
        -Message ("expected opened projection available but got '{0}'" -f $openerSummary.opened_projection.status)
    Assert-Condition `
        -Condition ([string]$openerSummary.opened_projection.projection_kind -eq "open_event_witness_overview") `
        -Message ("expected open_event_witness_overview but got '{0}'" -f $openerSummary.opened_projection.projection_kind)
    Assert-Condition `
        -Condition (([string]$openerSummary.opened_projection.headline).Contains("status=ok")) `
        -Message ("expected witness status in projection headline but got '{0}'" -f $openerSummary.opened_projection.headline)
    Assert-Condition `
        -Condition (([string]$openerSummary.opened_projection.headline).Contains("event=accepted_with_drift")) `
        -Message ("expected event status in projection headline but got '{0}'" -f $openerSummary.opened_projection.headline)

    $summaryLines = @($openerSummary.opened_projection.summary_lines)
    $observationLines = @(
        $summaryLines | Where-Object { ([string]$_).StartsWith("observation ", [System.StringComparison]::Ordinal) }
    )
    Assert-Condition `
        -Condition ($observationLines.Count -gt 0) `
        -Message "expected opened projection to expose observation summary lines"
    Assert-Condition `
        -Condition (@($summaryLines | Where-Object { ([string]$_).StartsWith("opening_input_refs=", [System.StringComparison]::Ordinal) }).Count -eq 1) `
        -Message "expected opened projection to expose opening_input_refs summary line"
    Assert-Condition `
        -Condition (@($openerSummary.opened_projection.evidence_paths).Count -ge 2) `
        -Message "expected opened projection to reference witness evidence paths"
    Assert-Condition `
        -Condition (@($openerSummary.opened_projection.supporting_summary_paths).Count -ge 1) `
        -Message "expected opened projection to reference supporting summary paths"

    Write-Host (
        "[FRONT-PAGE-ENTRY-OPENER-OPEN-EVENT-WITNESS-SMOKE] projection={0}/{1} headline='{2}' observations={3} inspector_ready={4}" -f
        [string]$openerSummary.opened_projection.status,
        [string]$openerSummary.opened_projection.projection_kind,
        [string]$openerSummary.opened_projection.headline,
        [int]$observationLines.Count,
        [bool]$openerSummary.inspector_invocation.ready
    )
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENER-OPEN-EVENT-WITNESS-SMOKE] output_root={0}" -f $outputRootPath)
