param(
    [string]$ActionCompareRoot = "cmake-build-system-compiler-front-page-entry-opener-plan-action-compare-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opener-plan-action-compare-opener-smoke",
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

function New-MinimalActionSummary {
    param(
        [string]$WorkspaceRoot,
        [string]$EntryName,
        [string]$ActionKind,
        [string]$DisplayGroup,
        [string]$SelectedRole,
        [int]$Rank,
        [bool]$CompareContextAvailable,
        [string]$LandingVerdict,
        [string]$ProjectionKind,
        [string]$OpeningReasonKind,
        [string]$ExpectedConsumerOperation,
        [string]$ChosenBy
    )

    $workspaceRootPath = Resolve-FullPath -Path $WorkspaceRoot
    Ensure-Directory -Path $workspaceRootPath

    $actionRoot = Join-Path $workspaceRootPath "action"
    $planRoot = Join-Path $workspaceRootPath "plan"
    $targetRoot = Join-Path $workspaceRootPath "target"
    $openerRoot = Join-Path $workspaceRootPath ("opener\{0}" -f $ActionKind)
    foreach ($root in @($actionRoot, $planRoot, $targetRoot, $openerRoot)) {
        Ensure-Directory -Path $root
    }

    $actionSummaryPath = Resolve-FullPath -Path (Join-Path $actionRoot "front-page.entry-opening-flow.consumer.plan-action.summary.json")
    $actionReportPath = Resolve-FullPath -Path (Join-Path $actionRoot "front-page.entry-opening-flow.consumer.plan-action.report.md")
    $actionCheckPath = Resolve-FullPath -Path (Join-Path $actionRoot "front-page.entry-opening-flow.consumer.plan-action.check.txt")
    $planSummaryPath = Resolve-FullPath -Path (Join-Path $planRoot "front-page.entry-opening-flow.consumer.plan.summary.json")
    $targetSummaryPath = Resolve-FullPath -Path (Join-Path $targetRoot ("{0}.target.summary.json" -f $ActionKind))
    $openerSummaryPath = Resolve-FullPath -Path (Join-Path $openerRoot "front-page.entry-opener.summary.json")
    $openerReportPath = Resolve-FullPath -Path (Join-Path $openerRoot "front-page.entry-opener.report.md")
    $openerCheckPath = Resolve-FullPath -Path (Join-Path $openerRoot "front-page.entry-opener.check.txt")
    $actionId = "open-{0}-entry" -f $ActionKind
    $selectedTabId = "{0}_tab" -f ($ActionKind -replace "-", "_")
    $reasonSummary = "Select {0} action for a synthetic explain-open plan-action fixture." -f $ActionKind
    $projectionHeadline = "{0} projection for {1}" -f $ProjectionKind, $EntryName
    $projectionSummaryLines = @("synthetic projection summary for {0}" -f $EntryName)
    $projectionQuestionLines = @("should synthetic {0} action remain comparable?" -f $ActionKind)
    $inspectorBlocker = "synthetic {0} action target is not an artifact report" -f $ActionKind

    Write-JsonFile -Path $planSummaryPath -Value ([ordered]@{
        schema = "synthetic.front_page_entry_opening_flow_consumer_plan/v0"
        action_kind = $ActionKind
    })
    Write-JsonFile -Path $targetSummaryPath -Value ([ordered]@{
        schema = "synthetic.target/v0"
        action_kind = $ActionKind
    })
    Write-JsonFile -Path $openerSummaryPath -Value ([ordered]@{
        schema = "system_compiler.front_page_entry_opener/v0"
        synthetic = $true
        action_kind = $ActionKind
    })
    Write-TextFile -Path $openerReportPath -Content ("# Synthetic opener {0}`n" -f $ActionKind)
    Write-TextFile -Path $openerCheckPath -Content ("synthetic opener {0}`n" -f $ActionKind)
    Write-TextFile -Path $actionReportPath -Content ("# Synthetic plan action {0}`n" -f $ActionKind)
    Write-TextFile -Path $actionCheckPath -Content ("synthetic plan action {0}`n" -f $ActionKind)

    $openingReason = [ordered]@{
        kind = $OpeningReasonKind
        summary = $reasonSummary
        source_summary_path = $targetSummaryPath
        drift_changed = $CompareContextAvailable
        drift_verdict = $LandingVerdict
    }
    $selectedAction = [ordered]@{
        action_id = $actionId
        rank = $Rank
        source_rank = $Rank
        action_kind = $ActionKind
        entry_name = $EntryName
        display_group = $DisplayGroup
        selected_tab_id = $selectedTabId
        selected_role = $SelectedRole
        query_kind = "default_overview"
        query_scope = "report"
        target_summary_schema = "synthetic.target/v0"
        target_summary_kind = "synthetic.target"
        target_summary_path = $targetSummaryPath
        projection_kind = $ProjectionKind
        opening_reason = $openingReason
        projection_headline = $projectionHeadline
        projection_summary_lines = $projectionSummaryLines
        projection_question_lines = $projectionQuestionLines
        compare_context_available = $CompareContextAvailable
        landing_verdict = $LandingVerdict
        inspector_ready = $false
        inspector_mode = "unresolved"
        inspector_blockers = @($inspectorBlocker)
        opener_summary_path = $openerSummaryPath
        opener_report_markdown_path = $openerReportPath
        opener_check_text_path = $openerCheckPath
        expected_consumer_operation = $ExpectedConsumerOperation
        reason = ("Synthetic {0} action was selected by {1}." -f $ActionKind, $ChosenBy)
    }
    $openAction = [ordered]@{
        status = "ready"
        action_id = $actionId
        action_kind = $ActionKind
        entry_name = $EntryName
        display_group = $DisplayGroup
        selected_tab_id = $selectedTabId
        selected_role = $SelectedRole
        query_kind = "default_overview"
        query_scope = "report"
        target_summary_schema = "synthetic.target/v0"
        target_summary_kind = "synthetic.target"
        target_summary_path = $targetSummaryPath
        projection_kind = $ProjectionKind
        opening_reason = $openingReason
        projection_headline = $projectionHeadline
        projection_summary_lines = $projectionSummaryLines
        projection_question_lines = $projectionQuestionLines
        compare_context_available = $CompareContextAvailable
        landing_verdict = $LandingVerdict
        opener_summary_path = $openerSummaryPath
        opener_report_markdown_path = $openerReportPath
        opener_check_text_path = $openerCheckPath
        expected_consumer_operation = $ExpectedConsumerOperation
        reason = ("Synthetic {0} open action is ready." -f $ActionKind)
        blockers = @()
    }

    $summary = [ordered]@{
        schema = "system_compiler.front_page_entry_opening_flow_consumer_plan_action/v0"
        kind = "system_compiler.front_page_entry_opening_flow_consumer_plan_action"
        generated_at_utc = "2026-05-05T00:00:00Z"
        generator = "scripts/system_compiler_front_page_entry_opener_plan_action_compare_smoke.ps1"
        result = "ok"
        opening_flow_consumer_plan_action = [ordered]@{
            title = "Synthetic Consumer Plan Action"
            summary = "A minimal action summary used to prove opener projection for plan-action compare judgments."
        }
        front_page = [ordered]@{
            summary_path = $actionSummaryPath
            report_markdown_path = $actionReportPath
            check_text_path = $actionCheckPath
            supporting_surfaces = @()
        }
        artifact_context = [ordered]@{
            source_plan_summary_path = $planSummaryPath
            output_root = $workspaceRootPath
            action_summary_path = $actionSummaryPath
            report_markdown_path = $actionReportPath
            check_text_path = $actionCheckPath
        }
        selection_request = [ordered]@{
            requested_action_id = $actionId
            requested_action_kind = $ActionKind
            requested_entry_name = $EntryName
            effective_selector = $ActionKind
            matched_action_count = 1
        }
        source_plan = [ordered]@{
            result = "ok"
            execution_plan_status = "ready"
            planned_action_count = 2
            default_action_id = "open-default-entry"
            default_action_name = "default-runtime"
            compare_action_id = "open-compare-neighbor-entry"
            compare_action_name = "compare-runtime"
        }
        selected_action = $selectedAction
        open_action = $openAction
        opening_preview = [ordered]@{
            available = $true
            entry_name = $EntryName
            opening_reason = $openingReason
            projection_kind = $ProjectionKind
            headline = $projectionHeadline
            summary_lines = $projectionSummaryLines
            question_lines = $projectionQuestionLines
            opener_summary_path = $openerSummaryPath
            opener_report_markdown_path = $openerReportPath
            opener_check_text_path = $openerCheckPath
            blockers = @()
        }
        opener_surface = [ordered]@{
            available = $true
            summary_schema = "system_compiler.front_page_entry_opener/v0"
            summary_path = $openerSummaryPath
            report_markdown_path = $openerReportPath
            check_text_path = $openerCheckPath
        }
        execution_receipt = [ordered]@{
            consumer_operation = $ExpectedConsumerOperation
            selected_rank = $Rank
            source_rank = $Rank
            chosen_by = $ChosenBy
            planned_action_count = 2
            inspector_ready = $false
            inspector_mode = "unresolved"
            inspector_blockers = @($inspectorBlocker)
        }
        questions = [ordered]@{
            action_questions = @("Should this synthetic action remain a stable compare fixture?")
            next_questions = @("Should the action compare become the next opener target?")
        }
        violations = @()
    }

    Write-JsonFile -Path $actionSummaryPath -Value $summary
    return $actionSummaryPath
}

function New-EntryRef {
    param(
        [string]$SummaryPath,
        [string]$ReportPath,
        [string]$CheckPath
    )

    return [ordered]@{
        route_id = "plan-action-compare-route"
        depth = 0
        surface_id = "plan_action_compare"
        label = "Front page entry opening flow consumer plan action compare"
        role = "opening_action_compare"
        summary_schema = "system_compiler.front_page_entry_opening_flow_consumer_plan_action_compare/v0"
        summary_kind = "system_compiler.front_page_entry_opening_flow_consumer_plan_action_compare"
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
        tab_title = "Plan Action Compare"
        entry_role = "opening_action_compare"
        summary_schema = "system_compiler.front_page_entry_opening_flow_consumer_plan_action_compare/v0"
        summary_kind = "system_compiler.front_page_entry_opening_flow_consumer_plan_action_compare"
        scope = "report"
        selection_rule = "single_report"
        query_kind = "default_overview"
        compare_expected = $true
        followup_query_kinds = @("action_evidence_refs", "consumer_operation")
        rationale = "Open the plan-action compare as the first explainable action judgment preview."
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
    Write-TextFile -Path $landingReportPath -Content "# Synthetic Plan Action Compare Landing`n"
    Write-TextFile -Path $landingCheckPath -Content "synthetic plan action compare landing`n"

    $landingPath = Resolve-FullPath -Path $LandingPath
    $landingReportPath = Resolve-FullPath -Path $landingReportPath
    $landingCheckPath = Resolve-FullPath -Path $landingCheckPath
    $targetSummaryPath = Resolve-FullPath -Path $TargetSummaryPath
    $targetReportPath = Resolve-FullPath -Path $TargetReportPath
    $targetCheckPath = Resolve-FullPath -Path $TargetCheckPath
    $outputRoot = Resolve-FullPath -Path $caseRoot
    $tabId = "plan_action_compare"
    $entry = New-EntryRef -SummaryPath $targetSummaryPath -ReportPath $targetReportPath -CheckPath $targetCheckPath
    $query = New-QueryHint -TabId $tabId
    $landingTab = [ordered]@{
        tab_id = $tabId
        title = "Plan Action Compare"
        capability_ids = @("system_compiler.front_page_entry_opening_flow_consumer_plan_action_compare")
        entry = $entry
    }

    $summary = [ordered]@{
        schema = "system_compiler.front_page_entry_landing/v0"
        kind = "system_compiler.front_page_entry_landing"
        generated_at_utc = "2026-05-05T00:00:00Z"
        generator = "scripts/system_compiler_front_page_entry_opener_plan_action_compare_smoke.ps1"
        result = "ok"
        entry_landing = [ordered]@{
            title = "Synthetic Plan Action Compare Landing"
            summary = "A targeted landing that proves opener projection for plan-action compare judgments."
        }
        front_page = [ordered]@{
            summary_path = $landingPath
            report_markdown_path = $landingReportPath
            check_text_path = $landingCheckPath
            supporting_surfaces = @(
                [ordered]@{
                    id = "plan_action_compare"
                    label = "plan action compare"
                    role = "opening_action_compare"
                    summary_schema = "system_compiler.front_page_entry_opening_flow_consumer_plan_action_compare/v0"
                    summary_path = $targetSummaryPath
                    report_markdown_path = $targetReportPath
                    check_text_path = $targetCheckPath
                }
            )
        }
        route_provenance = @(
            [ordered]@{
                id = "plan-action-compare-route"
                route_kind = "synthetic_targeted_smoke"
                source_summary_schema = "system_compiler.front_page_entry_opening_flow_consumer_plan_action_compare/v0"
                source_summary_path = $targetSummaryPath
                source_input_summary_path = $targetSummaryPath
                source_root_summary_path = $targetSummaryPath
                source_report_markdown_path = $targetReportPath
                source_check_text_path = $targetCheckPath
                level1_surface_ids = @("plan_action_compare")
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
            surface_id = "plan_action_compare"
            label = "Front page entry opening flow consumer plan action compare"
            role = "opening_action_compare"
            summary_schema = "system_compiler.front_page_entry_opening_flow_consumer_plan_action_compare/v0"
            summary_kind = "system_compiler.front_page_entry_opening_flow_consumer_plan_action_compare"
            summary_path = $targetSummaryPath
        }
        landing_status = [ordered]@{
            landing_result = "ok"
            recommended_entry_mode = "compare"
            entry_tier = "compare_ready"
            opening_reason = [ordered]@{
                kind = "counterfactual_verdict"
                summary = "Open the plan-action compare because it judges whether the selected explain-open action drifted."
                source_summary_path = $targetSummaryPath
                drift_changed = $true
                drift_verdict = "drifted"
            }
            primary_tab_id = $tabId
            primary_summary_schema = "system_compiler.front_page_entry_opening_flow_consumer_plan_action_compare/v0"
            primary_summary_kind = "system_compiler.front_page_entry_opening_flow_consumer_plan_action_compare"
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
        fallback_mode_order = @("compare", "route")
        primary_landing = $landingTab
        secondary_landings = @()
        landing_tabs = @($landingTab)
        provenance_roots = @(
            [ordered]@{
                root_id = "plan_action_compare"
                root_kind = "plan_action_compare"
                source_summary_schema = "system_compiler.front_page_entry_opening_flow_consumer_plan_action_compare/v0"
                source_summary_path = $targetSummaryPath
                source_front_page_summary_path = $targetSummaryPath
                owner_route_ids = @("plan-action-compare-route")
                owner_surface_ids = @("plan_action_compare")
                available_supporting_surface_ids = @("plan_action_compare")
            }
        )
        query_hints = [ordered]@{
            primary_query = $query
            tab_queries = @($query)
        }
        questions = [ordered]@{
            compare_questions = @("Should this plan-action compare be shown before opening the full consumer plan?")
            next_questions = @("Should action-level drift become the next explain surface prompt?")
        }
        violations = @()
    }

    Write-JsonFile -Path $landingPath -Value $summary
    return $landingPath
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$actionCompareRootPath = Resolve-FullPath -Path $ActionCompareRoot
$outputRootPath = Resolve-FullPath -Path $OutputRoot

if ($Clean) {
    Remove-PathIfExists -Path $actionCompareRootPath
    Remove-PathIfExists -Path $outputRootPath
}
Ensure-Directory -Path $actionCompareRootPath
Ensure-Directory -Path $outputRootPath

$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}

$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_flow_consumer_plan_action.py"
$validateCompareScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_consumer_plan_action_compare.py"
$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opener.py"
$validateOpenerScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opener.py"
foreach ($requiredPath in @($compareScript, $validateCompareScript, $exportScript, $validateOpenerScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $fixtureRoot = Join-Path $actionCompareRootPath "_synthetic_actions"
    $baselineActionPath = New-MinimalActionSummary `
        -WorkspaceRoot (Join-Path $fixtureRoot "baseline") `
        -EntryName "default-runtime" `
        -ActionKind "default" `
        -DisplayGroup "primary" `
        -SelectedRole "supporting_evidence" `
        -Rank 0 `
        -CompareContextAvailable $false `
        -LandingVerdict "" `
        -ProjectionKind "runtime_evidence_bundle_overview" `
        -OpeningReasonKind "supporting_evidence" `
        -ExpectedConsumerOperation "open-default-action" `
        -ChosenBy "default_action"
    $candidateActionPath = New-MinimalActionSummary `
        -WorkspaceRoot (Join-Path $fixtureRoot "candidate") `
        -EntryName "compare-runtime" `
        -ActionKind "compare-neighbor" `
        -DisplayGroup "compare" `
        -SelectedRole "opening_chain_compare" `
        -Rank 1 `
        -CompareContextAvailable $true `
        -LandingVerdict "drifted" `
        -ProjectionKind "opener_compare_overview" `
        -OpeningReasonKind "counterfactual_verdict" `
        -ExpectedConsumerOperation "open-compare-neighbor-action" `
        -ChosenBy "compare_neighbor"

    $compareCaseRoot = Join-Path $actionCompareRootPath "default-to-compare-neighbor"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $compareScript,
            "--baseline",
            $baselineActionPath,
            "--candidate",
            $candidateActionPath,
            "--output-root",
            $compareCaseRoot
        ) `
        -FailureMessage "front page entry opening-flow consumer plan action compare export failed"

    $compareSummaryPath = Join-Path $compareCaseRoot "front-page.entry-opening-flow.consumer.plan-action.compare.summary.json"
    $compareReportPath = Join-Path $compareCaseRoot "front-page.entry-opening-flow.consumer.plan-action.compare.report.md"
    $compareCheckPath = Join-Path $compareCaseRoot "front-page.entry-opening-flow.consumer.plan-action.compare.check.txt"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateCompareScript, "--summary", $compareSummaryPath) `
        -FailureMessage "front page entry opening-flow consumer plan action compare validation failed"

    $compareSummary = Load-JsonObject -Path $compareSummaryPath
    Assert-Condition `
        -Condition ([string]$compareSummary.action_verdict -eq "drifted") `
        -Message ("expected drifted plan-action compare fixture but got '{0}'" -f $compareSummary.action_verdict)
    Assert-Condition `
        -Condition ([int]$compareSummary.change_summary.changed_field_count -eq 30) `
        -Message ("expected changed field count 30 but got '{0}'" -f $compareSummary.change_summary.changed_field_count)
    Assert-Condition `
        -Condition ([bool]$compareSummary.action_regression_surface.reason_changed -eq $true) `
        -Message "expected plan-action compare fixture to carry reason drift"

    $landingRoot = Join-Path $outputRootPath "_synthetic_landing"
    $landingPath = New-MinimalLandingSummary `
        -LandingPath (Join-Path $landingRoot "front-page.entry-landing.summary.json") `
        -TargetSummaryPath $compareSummaryPath `
        -TargetReportPath $compareReportPath `
        -TargetCheckPath $compareCheckPath

    $caseOutputRoot = Join-Path $outputRootPath "plan-action-compare"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $exportScript,
            "--landing",
            $landingPath,
            "--output-root",
            $caseOutputRoot
        ) `
        -FailureMessage "front page entry opener export failed for plan-action compare"

    $openerSummaryPath = Join-Path $caseOutputRoot "front-page.entry-opener.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateOpenerScript, "--summary", $openerSummaryPath) `
        -FailureMessage "front page entry opener validation failed for plan-action compare"

    $openerSummary = Load-JsonObject -Path $openerSummaryPath
    Assert-Condition `
        -Condition ([string]$openerSummary.open_action.status -eq "ready") `
        -Message ("expected open action ready but got '{0}'" -f $openerSummary.open_action.status)
    Assert-Condition `
        -Condition ([string]$openerSummary.open_action.selected_tab_id -eq "plan_action_compare") `
        -Message ("expected selected tab plan_action_compare but got '{0}'" -f $openerSummary.open_action.selected_tab_id)
    Assert-Condition `
        -Condition ([bool]$openerSummary.inspector_invocation.ready -eq $false) `
        -Message "expected inspector invocation to stay blocked for non-artifact-report target"
    Assert-Condition `
        -Condition ([string]$openerSummary.opened_projection.status -eq "available") `
        -Message ("expected opened projection available but got '{0}'" -f $openerSummary.opened_projection.status)
    Assert-Condition `
        -Condition ([string]$openerSummary.opened_projection.projection_kind -eq "plan_action_compare_overview") `
        -Message ("expected plan_action_compare_overview but got '{0}'" -f $openerSummary.opened_projection.projection_kind)
    Assert-Condition `
        -Condition (([string]$openerSummary.opened_projection.headline).Contains("verdict=drifted")) `
        -Message ("expected drifted verdict in projection headline but got '{0}'" -f $openerSummary.opened_projection.headline)
    Assert-Condition `
        -Condition (([string]$openerSummary.opened_projection.headline).Contains("changed=30")) `
        -Message ("expected changed=30 in projection headline but got '{0}'" -f $openerSummary.opened_projection.headline)

    $summaryLines = @($openerSummary.opened_projection.summary_lines)
    Assert-Condition `
        -Condition (($summaryLines | Where-Object { ([string]$_).StartsWith("change_counts selection=4 open_action=20 opener_surface=1 receipt=5", [System.StringComparison]::Ordinal) }).Count -gt 0) `
        -Message "expected opened projection to expose plan-action change counts"
    Assert-Condition `
        -Condition (($summaryLines | Where-Object { ([string]$_).StartsWith("regression_flags target=yes opener=yes reason=yes operation=yes compare_context_lost=no inspector_lost=no", [System.StringComparison]::Ordinal) }).Count -gt 0) `
        -Message "expected opened projection to expose regression flag digest"
    Assert-Condition `
        -Condition (($summaryLines | Where-Object { ([string]$_).StartsWith("action_regression ", [System.StringComparison]::Ordinal) }).Count -gt 0) `
        -Message "expected opened projection to expose action_regression narrative"
    Assert-Condition `
        -Condition (@($openerSummary.opened_projection.evidence_paths).Count -ge 2) `
        -Message "expected opened projection to reference baseline and candidate plan-action summaries"

    Write-Host (
        "[FRONT-PAGE-ENTRY-OPENER-PLAN-ACTION-COMPARE-SMOKE] projection={0}/{1} headline='{2}' evidence_paths={3} inspector_ready={4}" -f
        [string]$openerSummary.opened_projection.status,
        [string]$openerSummary.opened_projection.projection_kind,
        [string]$openerSummary.opened_projection.headline,
        [int]@($openerSummary.opened_projection.evidence_paths).Count,
        [bool]$openerSummary.inspector_invocation.ready
    )
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENER-PLAN-ACTION-COMPARE-SMOKE] output_root={0}" -f $outputRootPath)
