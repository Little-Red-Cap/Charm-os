param(
    [string]$WitnessCompareRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-open-event-witness-compare-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opener-open-event-witness-compare-smoke",
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
        route_id = "open-event-witness-compare-route"
        depth = 0
        surface_id = "open_event_witness_compare"
        label = "Open event witness compare"
        role = "supporting_testimony"
        summary_schema = "system_compiler.front_page_entry_opening_flow_open_event_witness_compare/v0"
        summary_kind = "system_compiler.front_page_entry_opening_flow_open_event_witness_compare"
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
        tab_title = "Open Event Witness Compare"
        entry_role = "supporting_testimony"
        summary_schema = "system_compiler.front_page_entry_opening_flow_open_event_witness_compare/v0"
        summary_kind = "system_compiler.front_page_entry_opening_flow_open_event_witness_compare"
        scope = "report"
        selection_rule = "single_report"
        query_kind = "default_overview"
        compare_expected = $false
        followup_query_kinds = @("evidence_refs", "explanation")
        rationale = "Open the open-event witness compare as the first explainable opening judgment preview."
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
    Write-TextFile -Path $landingReportPath -Content "# Synthetic Open Event Witness Compare Landing`n"
    Write-TextFile -Path $landingCheckPath -Content "synthetic open event witness compare landing`n"

    $landingPath = Resolve-FullPath -Path $LandingPath
    $landingReportPath = Resolve-FullPath -Path $landingReportPath
    $landingCheckPath = Resolve-FullPath -Path $landingCheckPath
    $targetSummaryPath = Resolve-FullPath -Path $TargetSummaryPath
    $targetReportPath = Resolve-FullPath -Path $TargetReportPath
    $targetCheckPath = Resolve-FullPath -Path $TargetCheckPath
    $outputRoot = Resolve-FullPath -Path $caseRoot
    $tabId = "open_event_witness_compare"
    $entry = New-EntryRef -SummaryPath $targetSummaryPath -ReportPath $targetReportPath -CheckPath $targetCheckPath
    $query = New-QueryHint -TabId $tabId
    $landingTab = [ordered]@{
        tab_id = $tabId
        title = "Open Event Witness Compare"
        capability_ids = @("system_compiler.open_event_witness_compare")
        entry = $entry
    }

    $summary = [ordered]@{
        schema = "system_compiler.front_page_entry_landing/v0"
        kind = "system_compiler.front_page_entry_landing"
        generated_at_utc = "2026-05-05T00:00:00Z"
        generator = "scripts/system_compiler_front_page_entry_opener_open_event_witness_compare_smoke.ps1"
        result = "ok"
        entry_landing = [ordered]@{
            title = "Synthetic Open Event Witness Compare Landing"
            summary = "A targeted landing that proves opener projection for OpenEventWitnessCompare."
        }
        front_page = [ordered]@{
            summary_path = $landingPath
            report_markdown_path = $landingReportPath
            check_text_path = $landingCheckPath
            supporting_surfaces = @(
                [ordered]@{
                    id = "open_event_witness_compare"
                    label = "open event witness compare"
                    role = "supporting_testimony"
                    summary_schema = "system_compiler.front_page_entry_opening_flow_open_event_witness_compare/v0"
                    summary_path = $targetSummaryPath
                    report_markdown_path = $targetReportPath
                    check_text_path = $targetCheckPath
                }
            )
        }
        route_provenance = @(
            [ordered]@{
                id = "open-event-witness-compare-route"
                route_kind = "synthetic_targeted_smoke"
                source_summary_schema = "system_compiler.front_page_entry_opening_flow_open_event_witness_compare/v0"
                source_summary_path = $targetSummaryPath
                source_input_summary_path = $targetSummaryPath
                source_root_summary_path = $targetSummaryPath
                source_report_markdown_path = $targetReportPath
                source_check_text_path = $targetCheckPath
                level1_surface_ids = @("open_event_witness_compare")
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
            surface_id = "open_event_witness_compare"
            label = "Open event witness compare"
            role = "supporting_testimony"
            summary_schema = "system_compiler.front_page_entry_opening_flow_open_event_witness_compare/v0"
            summary_kind = "system_compiler.front_page_entry_opening_flow_open_event_witness_compare"
            summary_path = $targetSummaryPath
        }
        landing_status = [ordered]@{
            landing_result = "ok"
            recommended_entry_mode = "evidence"
            entry_tier = "evidence_only"
            opening_reason = [ordered]@{
                kind = "supporting_evidence"
                summary = "OpenEventWitnessCompare is available as the nearest testimony compare target."
                source_summary_path = $targetSummaryPath
                drift_changed = $true
                drift_verdict = "drifted"
            }
            primary_tab_id = $tabId
            primary_summary_schema = "system_compiler.front_page_entry_opening_flow_open_event_witness_compare/v0"
            primary_summary_kind = "system_compiler.front_page_entry_opening_flow_open_event_witness_compare"
            available_tab_ids = @($tabId)
            fallback_tab_ids = @()
            tab_count = 1
            fallback_tab_count = 0
            provenance_root_count = 1
            route_provenance_entry_count = 1
            direct_review_available = $false
            direct_compare_available = $true
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
                root_id = "open_event_witness_compare"
                root_kind = "open_event_witness_compare"
                source_summary_schema = "system_compiler.front_page_entry_opening_flow_open_event_witness_compare/v0"
                source_summary_path = $targetSummaryPath
                source_front_page_summary_path = $targetSummaryPath
                owner_route_ids = @("open-event-witness-compare-route")
                owner_surface_ids = @("open_event_witness_compare")
                available_supporting_surface_ids = @("open_event_witness_compare")
            }
        )
        query_hints = [ordered]@{
            primary_query = $query
            tab_queries = @($query)
        }
        questions = [ordered]@{
            compare_questions = @("Should this OpenEventWitnessCompare projection be shown before opening witness details?")
            next_questions = @("Should evidence refs and explanation lines become the next opener follow-up?")
        }
        violations = @()
    }

    Write-JsonFile -Path $landingPath -Value $summary
    return $landingPath
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$witnessCompareRootPath = Resolve-FullPath -Path $WitnessCompareRoot
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

$witnessCompareSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_flow_open_event_witness_compare_smoke.ps1"
$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opener.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opener.py"
foreach ($requiredPath in @($witnessCompareSmokeScript, $exportScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $witnessCompareCaseRoot = Join-Path $witnessCompareRootPath "open-event-witness-default-to-drift-context"
    $witnessCompareSummaryPath = Join-Path $witnessCompareCaseRoot "front-page.entry-opening-flow.open-event.witness.compare.summary.json"
    $witnessCompareReportPath = Join-Path $witnessCompareCaseRoot "front-page.entry-opening-flow.open-event.witness.compare.report.md"
    $witnessCompareCheckPath = Join-Path $witnessCompareCaseRoot "front-page.entry-opening-flow.open-event.witness.compare.check.txt"

    if ($Clean -or -not (Test-Path -LiteralPath $witnessCompareSummaryPath)) {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $witnessCompareSmokeScript,
                "-OutputRoot",
                $witnessCompareRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "front page entry opening-flow open-event witness compare smoke bootstrap failed"
    } else {
        Write-Host "[FRONT-PAGE-ENTRY-OPENER-OPEN-EVENT-WITNESS-COMPARE-SMOKE] witness_compare_bootstrap=reuse-existing"
    }

    foreach ($requiredArtifact in @($witnessCompareSummaryPath, $witnessCompareReportPath, $witnessCompareCheckPath)) {
        if (-not (Test-Path -LiteralPath $requiredArtifact)) {
            throw "missing open-event witness compare artifact: $requiredArtifact"
        }
    }

    $landingRoot = Join-Path $outputRootPath "_synthetic_landing"
    $landingPath = New-MinimalLandingSummary `
        -LandingPath (Join-Path $landingRoot "front-page.entry-landing.summary.json") `
        -TargetSummaryPath $witnessCompareSummaryPath `
        -TargetReportPath $witnessCompareReportPath `
        -TargetCheckPath $witnessCompareCheckPath

    $caseOutputRoot = Join-Path $outputRootPath "open-event-witness-compare"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $exportScript,
            "--landing",
            $landingPath,
            "--output-root",
            $caseOutputRoot
        ) `
        -FailureMessage "front page entry opener export failed for open-event witness compare"

    $openerSummaryPath = Join-Path $caseOutputRoot "front-page.entry-opener.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateScript, "--summary", $openerSummaryPath) `
        -FailureMessage "front page entry opener validation failed for open-event witness compare"

    $openerSummary = Load-JsonObject -Path $openerSummaryPath
    Assert-Condition `
        -Condition ([string]$openerSummary.open_action.status -eq "ready") `
        -Message ("expected open action ready but got '{0}'" -f $openerSummary.open_action.status)
    Assert-Condition `
        -Condition ([string]$openerSummary.open_action.selected_tab_id -eq "open_event_witness_compare") `
        -Message ("expected selected tab open_event_witness_compare but got '{0}'" -f $openerSummary.open_action.selected_tab_id)
    Assert-Condition `
        -Condition ([bool]$openerSummary.inspector_invocation.ready -eq $false) `
        -Message "expected inspector invocation to stay blocked for non-artifact-report target"
    Assert-Condition `
        -Condition ([string]$openerSummary.opened_projection.status -eq "available") `
        -Message ("expected opened projection available but got '{0}'" -f $openerSummary.opened_projection.status)
    Assert-Condition `
        -Condition ([string]$openerSummary.opened_projection.projection_kind -eq "open_event_witness_compare_overview") `
        -Message ("expected open_event_witness_compare_overview but got '{0}'" -f $openerSummary.opened_projection.projection_kind)
    Assert-Condition `
        -Condition (([string]$openerSummary.opened_projection.headline).Contains("verdict=drifted")) `
        -Message ("expected drifted verdict in projection headline but got '{0}'" -f $openerSummary.opened_projection.headline)
    Assert-Condition `
        -Condition (([string]$openerSummary.opened_projection.headline).Contains("changed=")) `
        -Message ("expected changed count in projection headline but got '{0}'" -f $openerSummary.opened_projection.headline)

    $driftLines = @(
        @($openerSummary.opened_projection.summary_lines) |
            Where-Object { ([string]$_).StartsWith("witness_drift ", [System.StringComparison]::Ordinal) }
    )
    Assert-Condition `
        -Condition ($driftLines.Count -gt 0) `
        -Message "expected opened projection to expose witness_drift summary line"
    Assert-Condition `
        -Condition (@($openerSummary.opened_projection.evidence_paths).Count -ge 2) `
        -Message "expected opened projection to reference baseline and candidate witness evidence paths"

    Write-Host (
        "[FRONT-PAGE-ENTRY-OPENER-OPEN-EVENT-WITNESS-COMPARE-SMOKE] projection={0}/{1} headline='{2}' drift_lines={3} inspector_ready={4}" -f
        [string]$openerSummary.opened_projection.status,
        [string]$openerSummary.opened_projection.projection_kind,
        [string]$openerSummary.opened_projection.headline,
        [int]$driftLines.Count,
        [bool]$openerSummary.inspector_invocation.ready
    )
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENER-OPEN-EVENT-WITNESS-COMPARE-SMOKE] output_root={0}" -f $outputRootPath)
