param(
    [string]$OpenEventRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-open-event-smoke",
    [string]$FrontPageWorkspaceRoot = "cmake-build-codex-system-compiler-front-page-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-open-event-witness-smoke",
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

function New-FixtureArtifact {
    param(
        [string]$Path,
        [string]$Label
    )

    Write-JsonFile `
        -Path $Path `
        -Value ([ordered]@{
            schema = "fixture.placeholder/v0"
            kind = "fixture.placeholder"
            label = $Label
        })
    return (Resolve-FullPath -Path $Path)
}

function New-FixtureText {
    param(
        [string]$Path,
        [string]$Content
    )

    Write-TextFile -Path $Path -Content $Content
    return (Resolve-FullPath -Path $Path)
}

function New-WitnessRef {
    param(
        [string]$Role,
        [string]$Schema,
        [string]$SummaryPath,
        [string]$ReportPath,
        [string]$CheckPath
    )

    return [ordered]@{
        role = $Role
        summary_schema = $Schema
        summary_path = $SummaryPath
        report_markdown_path = $ReportPath
        check_text_path = $CheckPath
    }
}

function New-FrontPageSurface {
    param(
        [string]$Id,
        [string]$Label,
        [string]$Role,
        [string]$Schema,
        [string]$SummaryPath,
        [string]$ReportPath,
        [string]$CheckPath
    )

    return [ordered]@{
        id = $Id
        label = $Label
        role = $Role
        summary_schema = $Schema
        summary_path = $SummaryPath
        report_markdown_path = $ReportPath
        check_text_path = $CheckPath
    }
}

function New-OpenEventFixture {
    param(
        [string]$Root,
        [string]$Name,
        [bool]$WithDriftCompare
    )

    $caseRoot = Join-Path $Root $Name
    Ensure-Directory -Path $caseRoot

    $summaryPath = Resolve-FullPath -Path (Join-Path $caseRoot "front-page.entry-opening-flow.open-event.summary.json")
    $reportPath = New-FixtureText -Path (Join-Path $caseRoot "front-page.entry-opening-flow.open-event.report.md") -Content "# Fixture Open Event`n"
    $checkPath = New-FixtureText -Path (Join-Path $caseRoot "front-page.entry-opening-flow.open-event.check.txt") -Content "fixture open event`n"
    $actionSummaryPath = New-FixtureArtifact -Path (Join-Path $caseRoot "source-plan-action.summary.json") -Label "source plan action"
    $actionReportPath = New-FixtureText -Path (Join-Path $caseRoot "source-plan-action.report.md") -Content "# Source Plan Action`n"
    $actionCheckPath = New-FixtureText -Path (Join-Path $caseRoot "source-plan-action.check.txt") -Content "source plan action`n"
    $openerSummaryPath = New-FixtureArtifact -Path (Join-Path $caseRoot "selected-opener.summary.json") -Label "selected opener"
    $openerReportPath = New-FixtureText -Path (Join-Path $caseRoot "selected-opener.report.md") -Content "# Selected Opener`n"
    $openerCheckPath = New-FixtureText -Path (Join-Path $caseRoot "selected-opener.check.txt") -Content "selected opener`n"
    $targetSummaryPath = New-FixtureArtifact -Path (Join-Path $caseRoot "target.summary.json") -Label "target summary"
    $compareSummaryPath = ""
    $compareReportPath = ""
    $compareCheckPath = ""
    if ($WithDriftCompare) {
        $compareSummaryPath = New-FixtureArtifact -Path (Join-Path $caseRoot "source-action-compare.summary.json") -Label "source action compare"
        $compareReportPath = New-FixtureText -Path (Join-Path $caseRoot "source-action-compare.report.md") -Content "# Source Action Compare`n"
        $compareCheckPath = New-FixtureText -Path (Join-Path $caseRoot "source-action-compare.check.txt") -Content "source action compare`n"
    }

    $compareVerdict = if ($WithDriftCompare) { "drifted" } else { "not_attached" }
    $compareChanged = if ($WithDriftCompare) { 28 } else { 0 }
    $eventStatus = if ($WithDriftCompare) { "accepted_with_drift" } else { "accepted" }
    $eventId = if ($WithDriftCompare) { "open-event-fixture-drift" } else { "open-event-fixture-clean" }
    $compareNarratives = [object[]]@()
    if ($WithDriftCompare) {
        $compareNarratives = [object[]]@("fixture compare drift attached")
    }
    $compareResultText = if ($WithDriftCompare) {
        "action compare verdict=drifted changed_fields=28"
    } else {
        "no action compare attached"
    }
    $compareTextLine = if ($WithDriftCompare) {
        "Compare: action compare verdict=drifted changed_fields=28"
    } else {
        "Compare: no action compare attached"
    }
    $diagnosticHeadline = "Fixture projection preview"
    $diagnosticSummaryLines = @("fixture projection summary: selected opener is ready")
    $diagnosticQuestionLines = @("fixture projection question: should this become witness input?")
    $judgmentGrade = if ($WithDriftCompare) { "compared" } else { "described" }
    $judgmentBasis = @("source_plan_action", "selected_opener", "open_event")
    if ($WithDriftCompare) {
        $judgmentBasis += "source_action_compare"
    }
    $judgmentSummary = if ($WithDriftCompare) {
        "This opening judgment stands with compare context because the selected consumer action produced a projected opener surface and the attached action compare preserves its decision context."
    } else {
        "This opening judgment stands as described because the selected consumer action produced a projected opener surface and the open event preserves its decision context."
    }
    $typedActionQuestion = if ($WithDriftCompare) {
        [ordered]@{
            kind = "inspect_action_compare"
            summary = "Inspect the attached action compare before rendering the selected opener as counterfactual context."
            target_ref = "compare_summary.summary_path"
        }
    } else {
        [ordered]@{
            kind = "attach_action_compare"
            summary = "Attach an action compare before publishing this open event as a compared opening judgment."
            target_ref = "artifact_context.source_action_compare_summary_path"
        }
    }
    $typedNextQuestions = @(
        $typedActionQuestion,
        [ordered]@{
            kind = "inspect_rejected_consumers"
            summary = "Inspect rejected consumer reasons as the next selector-facing explanation surface."
            target_ref = "consumer_decision.rejected_consumers"
        }
    )
    $witnessRefs = [System.Collections.Generic.List[object]]::new()
    $witnessRefs.Add((New-WitnessRef -Role "source_plan_action" -Schema "system_compiler.front_page_entry_opening_flow_consumer_plan_action/v0" -SummaryPath $actionSummaryPath -ReportPath $actionReportPath -CheckPath $actionCheckPath)) | Out-Null
    $witnessRefs.Add((New-WitnessRef -Role "selected_opener" -Schema "system_compiler.front_page_entry_opener/v0" -SummaryPath $openerSummaryPath -ReportPath $openerReportPath -CheckPath $openerCheckPath)) | Out-Null
    $witnessRefs.Add((New-WitnessRef -Role "open_event" -Schema "system_compiler.front_page_entry_opening_flow_open_event/v0" -SummaryPath $summaryPath -ReportPath $reportPath -CheckPath $checkPath)) | Out-Null
    if ($WithDriftCompare) {
        $witnessRefs.Add((New-WitnessRef -Role "source_action_compare" -Schema "system_compiler.front_page_entry_opening_flow_consumer_plan_action_compare/v0" -SummaryPath $compareSummaryPath -ReportPath $compareReportPath -CheckPath $compareCheckPath)) | Out-Null
    }

    $supportingSurfaces = [System.Collections.Generic.List[object]]::new()
    $supportingSurfaces.Add((New-FrontPageSurface -Id "source_plan_action" -Label "source opening-flow consumer plan action" -Role "source_plan_action" -Schema "system_compiler.front_page_entry_opening_flow_consumer_plan_action/v0" -SummaryPath $actionSummaryPath -ReportPath $actionReportPath -CheckPath $actionCheckPath)) | Out-Null
    $supportingSurfaces.Add((New-FrontPageSurface -Id "selected_opener" -Label "selected opener" -Role "selected_opener" -Schema "system_compiler.front_page_entry_opener/v0" -SummaryPath $openerSummaryPath -ReportPath $openerReportPath -CheckPath $openerCheckPath)) | Out-Null
    if ($WithDriftCompare) {
        $supportingSurfaces.Add((New-FrontPageSurface -Id "source_action_compare" -Label "source opening-flow consumer plan action compare" -Role "source_action_compare" -Schema "system_compiler.front_page_entry_opening_flow_consumer_plan_action_compare/v0" -SummaryPath $compareSummaryPath -ReportPath $compareReportPath -CheckPath $compareCheckPath)) | Out-Null
    }

    $summary = [ordered]@{
        schema = "system_compiler.front_page_entry_opening_flow_open_event/v0"
        kind = "system_compiler.front_page_entry_opening_flow_open_event"
        generated_at_utc = "2026-05-04T00:00:00Z"
        generator = "scripts/system_compiler_front_page_entry_opening_flow_open_event_witness_smoke.ps1"
        result = "ok"
        opening_flow_open_event = [ordered]@{
            title = "Fixture Opening Flow Open Event"
            summary = "A small fixture open-event used to prove OpenEventWitness projection."
        }
        front_page = [ordered]@{
            summary_path = $summaryPath
            report_markdown_path = $reportPath
            check_text_path = $checkPath
            supporting_surfaces = [object[]]@($supportingSurfaces)
        }
        artifact_context = [ordered]@{
            source_action_summary_path = $actionSummaryPath
            source_action_compare_summary_path = $compareSummaryPath
            output_root = (Resolve-FullPath -Path $caseRoot)
            open_event_summary_path = $summaryPath
            report_markdown_path = $reportPath
            check_text_path = $checkPath
        }
        open_event = [ordered]@{
            open_event_id = $eventId
            status = $eventStatus
            reason = [ordered]@{
                kind = "fixture.open_event_witness_smoke"
                summary = "Fixture opening reason for OpenEventWitness smoke."
                source_summary_path = $actionSummaryPath
                drift_changed = $WithDriftCompare
                drift_verdict = $compareVerdict
            }
            source_artifact = [ordered]@{
                summary_schema = "fixture.target/v0"
                summary_kind = "fixture.target"
                summary_path = $targetSummaryPath
                opener_summary_path = $openerSummaryPath
                opener_report_markdown_path = $openerReportPath
                opener_check_text_path = $openerCheckPath
            }
        }
        consumer_decision = [ordered]@{
            selected_consumer = [ordered]@{
                consumer_id = "fixture_consumer:default_overview:report"
                selected_action_id = "open-default"
                entry_name = "fixture-entry"
                selected_role = "fixture_consumer"
                query_kind = "default_overview"
                query_scope = "report"
                operation = "open-opener-summary"
                projection_kind = "fixture_projection"
                chosen_by = "default_action"
            }
            candidate_consumers = @(
                [ordered]@{
                    consumer_id = "fixture_consumer:default_overview:report"
                    action_id = "open-default"
                    entry_name = "fixture-entry"
                    rank = 0
                    action_kind = "default"
                    display_group = "default"
                    projection_kind = "fixture_projection"
                    target_summary_schema = "fixture.target/v0"
                    target_summary_kind = "fixture.target"
                    target_summary_path = $targetSummaryPath
                    selected = $true
                    selection_basis = "selected by default_action"
                },
                [ordered]@{
                    consumer_id = "fixture_compare:default_overview:artifact_root"
                    action_id = "open-compare-neighbor"
                    entry_name = "fixture-entry-compare"
                    rank = 1
                    action_kind = "compare-neighbor"
                    display_group = "compare"
                    projection_kind = "fixture_projection"
                    target_summary_schema = "fixture.target/v0"
                    target_summary_kind = "fixture.target"
                    target_summary_path = $targetSummaryPath
                    selected = $false
                    selection_basis = "available but not selected"
                }
            )
            rejected_consumers = @(
                [ordered]@{
                    consumer_id = "fixture_compare:default_overview:artifact_root"
                    action_id = "open-compare-neighbor"
                    entry_name = "fixture-entry-compare"
                    reason = "compare neighbor stayed available but selector default_action chose another action"
                }
            )
            candidate_consumer_count = 2
            rejected_consumer_count = 1
            decision_reason = "fixture opening action selected by default_action"
        }
        plan = [ordered]@{
            plan_id = "opening-flow-consumer-plan"
            result = "ok"
            execution_plan_status = "ready"
            planned_action_count = 2
            default_action_id = "open-default"
            compare_action_id = "open-compare-neighbor"
            selected_action_id = "open-default"
        }
        action_records = @(
            [ordered]@{
                action_id = "open-default"
                action_kind = "default"
                entry_name = "fixture-entry"
                expected = [ordered]@{
                    operation = "open-opener-summary"
                    projection_kind = "fixture_projection"
                    target_summary_schema = "fixture.target/v0"
                    target_summary_kind = "fixture.target"
                    target_summary_path = $targetSummaryPath
                }
                result = [ordered]@{
                    status = "ready"
                    opener_surface_available = $true
                    opener_summary_path = $openerSummaryPath
                    blockers = [object[]]@()
                }
                compare = [ordered]@{
                    available = $WithDriftCompare
                    summary_path = $compareSummaryPath
                    action_verdict = $compareVerdict
                    changed_field_count = $compareChanged
                    reason_changed = $WithDriftCompare
                    narratives = $compareNarratives
                }
            }
        )
        compare_summary = [ordered]@{
            available = $WithDriftCompare
            summary_path = $compareSummaryPath
            action_verdict = $compareVerdict
            changed_field_count = $compareChanged
            reason_changed = $WithDriftCompare
            narratives = $compareNarratives
        }
        workspace_facade = [ordered]@{
            status = "projected"
            facade_kind = "explain_open_event_view"
            primary_surface_role = "selected_opener"
            primary_summary_path = $openerSummaryPath
            primary_report_markdown_path = $openerReportPath
            primary_check_text_path = $openerCheckPath
        }
        diagnostic_preview = [ordered]@{
            available = $true
            entry_name = "fixture-entry"
            projection_kind = "fixture_projection"
            headline = $diagnosticHeadline
            summary_lines = $diagnosticSummaryLines
            question_lines = $diagnosticQuestionLines
            line_count = @($diagnosticSummaryLines).Count
            question_count = @($diagnosticQuestionLines).Count
            blockers = [object[]]@()
        }
        witness_refs = [object[]]@($witnessRefs)
        judgment = [ordered]@{
            semantic_role = "opening_judgment_carrier"
            status = $eventStatus
            grade = $judgmentGrade
            basis = [object[]]@($judgmentBasis)
            accepted = ($eventStatus -ne "blocked")
            summary = $judgmentSummary
        }
        explanation_view = [ordered]@{
            view_kind = "explain_open_event_view"
            status = $eventStatus
            why_opened = "Fixture opening reason for OpenEventWitness smoke."
            chosen_consumer = "fixture_consumer:default_overview:report selected by default_action"
            diagnostic_headline = $diagnosticHeadline
            diagnostic_summary_lines = $diagnosticSummaryLines
            diagnostic_question_lines = $diagnosticQuestionLines
            plan_actions = @("1. open-default -> ready")
            compare_result = $compareResultText
            witness_refs = @($witnessRefs | ForEach-Object { "{0}: {1}" -f [string]$_.role, [string]$_.summary_path })
            text_lines = @(
                "Why opened: Fixture opening reason for OpenEventWitness smoke.",
                "Chosen consumer: fixture_consumer:default_overview:report via default_action",
                "Diagnostic preview: 1 summary line(s), 1 question line(s)",
                "Plan: 2 action(s), selected open-default",
                "Rejected candidates: 1",
                $compareTextLine,
                ("Witness refs: {0}" -f @($witnessRefs).Count)
            )
        }
        questions = [ordered]@{
            open_event_questions = @("Should this fixture OpenEventRecord become a witness projection input?")
            next_questions = @("Should real opening flows attach OpenEventWitness after publication?")
            typed_next_questions = [object[]]@($typedNextQuestions)
        }
        violations = [object[]]@()
    }

    Write-JsonFile -Path $summaryPath -Value $summary
    return $summaryPath
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

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$openEventRootPath = Resolve-FullPath -Path $OpenEventRoot
$frontPageWorkspaceRootPath = Resolve-FullPath -Path $FrontPageWorkspaceRoot
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

$openEventSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_flow_open_event_smoke.ps1"
$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_open_event_witness.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_open_event_witness.py"
foreach ($requiredPath in @($openEventSmokeScript, $exportScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $defaultOpenEventPath = Join-Path $openEventRootPath "default-no-compare\front-page.entry-opening-flow.open-event.summary.json"
    $driftOpenEventPath = Join-Path $openEventRootPath "default-with-drift-compare\front-page.entry-opening-flow.open-event.summary.json"
    if ((-not $Clean) -and (Test-Path -LiteralPath $defaultOpenEventPath) -and (Test-Path -LiteralPath $driftOpenEventPath)) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-SMOKE] open_event_bootstrap=reuse-existing"
    } elseif (Test-Path -LiteralPath $frontPageWorkspaceRootPath) {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $openEventSmokeScript,
                "-OutputRoot",
                $openEventRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "front page entry opening-flow open-event smoke bootstrap failed"
    } else {
        $fixtureRoot = Join-Path $outputRootPath "_fixture-open-events"
        $defaultOpenEventPath = New-OpenEventFixture -Root $fixtureRoot -Name "default-no-compare" -WithDriftCompare $false
        $driftOpenEventPath = New-OpenEventFixture -Root $fixtureRoot -Name "default-with-drift-compare" -WithDriftCompare $true
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-SMOKE] open_event_bootstrap=fixture"
    }

    $cases = @(
        [ordered]@{
            Name = "default-no-compare-witness"
            OpenEvent = $defaultOpenEventPath
            ExpectedOpenEventStatus = "accepted"
            ExpectedWitnessStatus = "ok"
            ExpectedCompareAvailable = $false
            ExpectedCompareVerdict = "not_attached"
            ExpectedEvidenceRefs = 3
        },
        [ordered]@{
            Name = "default-with-drift-compare-witness"
            OpenEvent = $driftOpenEventPath
            ExpectedOpenEventStatus = "accepted_with_drift"
            ExpectedWitnessStatus = "ok"
            ExpectedCompareAvailable = $true
            ExpectedCompareVerdict = "drifted"
            ExpectedEvidenceRefs = 4
        }
    )

    foreach ($case in $cases) {
        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @(
                $exportScript,
                "--open-event",
                [string]$case.OpenEvent,
                "--output-root",
                $caseOutputRoot
            ) `
            -FailureMessage ("front page entry opening-flow open-event witness export failed for case '{0}'" -f $case.Name)

        $summaryPath = Join-Path $caseOutputRoot "front-page.entry-opening-flow.open-event.witness.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($validateScript, "--summary", $summaryPath) `
            -FailureMessage ("front page entry opening-flow open-event witness validation failed for case '{0}'" -f $case.Name)

        $summary = Load-JsonObject -Path $summaryPath
        Assert-Condition `
            -Condition ([string]$summary.result -eq "ok") `
            -Message ("case '{0}' expected result ok but got '{1}'" -f $case.Name, $summary.result)
        Assert-Condition `
            -Condition ([string]$summary.open_event_identity.open_event_status -eq [string]$case.ExpectedOpenEventStatus) `
            -Message ("case '{0}' expected open event status '{1}' but got '{2}'" -f $case.Name, $case.ExpectedOpenEventStatus, $summary.open_event_identity.open_event_status)
        Assert-Condition `
            -Condition ([string]$summary.judgment.witness_status -eq [string]$case.ExpectedWitnessStatus) `
            -Message ("case '{0}' expected witness status '{1}' but got '{2}'" -f $case.Name, $case.ExpectedWitnessStatus, $summary.judgment.witness_status)
        Assert-Condition `
            -Condition ([bool]$summary.judgment.compare_available -eq [bool]$case.ExpectedCompareAvailable) `
            -Message ("case '{0}' compare availability mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([string]$summary.judgment.compare_verdict -eq [string]$case.ExpectedCompareVerdict) `
            -Message ("case '{0}' expected compare verdict '{1}' but got '{2}'" -f $case.Name, $case.ExpectedCompareVerdict, $summary.judgment.compare_verdict)
        Assert-Condition `
            -Condition ([int]$summary.judgment.evidence_ref_count -eq [int]$case.ExpectedEvidenceRefs) `
            -Message ("case '{0}' expected evidence refs '{1}' but got '{2}'" -f $case.Name, $case.ExpectedEvidenceRefs, $summary.judgment.evidence_ref_count)
        Assert-Condition `
            -Condition ([int]$summary.judgment.artifact_ref_count -ge [int]$summary.judgment.evidence_ref_count) `
            -Message ("case '{0}' expected artifact refs to cover evidence refs" -f $case.Name)

        Write-Host (
            "[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-SMOKE] case={0} witness_status={1} event_status={2} compare={3}/{4} evidence_refs={5} artifact_refs={6}" -f
            $case.Name,
            [string]$summary.judgment.witness_status,
            [string]$summary.open_event_identity.open_event_status,
            [bool]$summary.judgment.compare_available,
            [string]$summary.judgment.compare_verdict,
            [int]$summary.judgment.evidence_ref_count,
            [int]$summary.judgment.artifact_ref_count
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-SMOKE] output_root={0}" -f $outputRootPath)
