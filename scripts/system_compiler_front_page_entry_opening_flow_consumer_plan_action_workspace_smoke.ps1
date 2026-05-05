param(
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-action-workspace-smoke",
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

function New-OpeningReason {
    param(
        [string]$Kind,
        [string]$Summary,
        [string]$SourceSummaryPath,
        [bool]$DriftChanged,
        [string]$DriftVerdict
    )

    return [ordered]@{
        kind = $Kind
        summary = $Summary
        source_summary_path = $SourceSummaryPath
        drift_changed = $DriftChanged
        drift_verdict = $DriftVerdict
    }
}

function New-FixtureQueryHint {
    param(
        [string]$TabId,
        [string]$TabTitle,
        [string]$Role,
        [string]$SummaryPath,
        [bool]$CompareExpected
    )

    return [ordered]@{
        tab_id = $TabId
        tab_title = $TabTitle
        entry_role = $Role
        summary_schema = "fixture.target/v0"
        summary_kind = "fixture.target"
        scope = "report"
        selection_rule = "single_report"
        query_kind = "default_overview"
        compare_expected = $CompareExpected
        followup_query_kinds = @()
        rationale = "fixture query hint for self-contained opening-flow smoke"
    }
}

function New-FixturePrimaryEntry {
    param(
        [string]$TabId,
        [string]$Title,
        [string]$Role,
        [string]$SummaryPath,
        [string]$ReportPath,
        [string]$CheckPath
    )

    return [ordered]@{
        tab_id = $TabId
        title = $Title
        route_id = ("fixture-route-{0}" -f $TabId)
        surface_id = ("fixture-surface-{0}" -f $TabId)
        role = $Role
        summary_schema = "fixture.target/v0"
        summary_kind = "fixture.target"
        summary_path = $SummaryPath
        report_markdown_path = $ReportPath
        check_text_path = $CheckPath
    }
}

function New-FixtureOpenerSummary {
    param(
        [string]$OutputRoot,
        [string]$ActionId,
        [string]$ActionKind,
        [string]$EntryName,
        [string]$TabId,
        [string]$TabTitle,
        [string]$SelectedRole,
        [string]$OpeningReasonKind,
        [bool]$CompareExpected,
        [bool]$CompareContextAvailable,
        [string]$LandingVerdict,
        [string]$TargetSummaryPath,
        [string]$TargetReportPath,
        [string]$TargetCheckPath
    )

    $openerRoot = Join-Path $OutputRoot ("opener\{0}" -f $ActionKind)
    Ensure-Directory -Path $openerRoot
    $openerSummaryPath = Resolve-FullPath -Path (Join-Path $openerRoot "front-page.entry-opener.summary.json")
    $openerReportPath = Resolve-FullPath -Path (Join-Path $openerRoot "front-page.entry-opener.report.md")
    $openerCheckPath = Resolve-FullPath -Path (Join-Path $openerRoot "front-page.entry-opener.check.txt")
    $landingSummaryPath = Resolve-FullPath -Path (Join-Path $openerRoot "front-page.entry-landing.summary.json")
    $landingReportPath = Resolve-FullPath -Path (Join-Path $openerRoot "front-page.entry-landing.report.md")
    $landingCheckPath = Resolve-FullPath -Path (Join-Path $openerRoot "front-page.entry-landing.check.txt")

    Write-JsonFile -Path $landingSummaryPath -Value ([ordered]@{
        schema = "fixture.front_page_entry_landing/v0"
        kind = "fixture.front_page_entry_landing"
        action_kind = $ActionKind
    })
    Write-TextFile -Path $landingReportPath -Content ("# Fixture landing {0}`n" -f $ActionKind)
    Write-TextFile -Path $landingCheckPath -Content ("fixture landing {0}`n" -f $ActionKind)
    Write-TextFile -Path $openerReportPath -Content ("# Fixture opener {0}`n" -f $ActionKind)
    Write-TextFile -Path $openerCheckPath -Content ("fixture opener {0}`n" -f $ActionKind)

    $compareNarratives = @()
    if ($CompareContextAvailable) {
        $compareNarratives = @("fixture compare context available")
    }
    $reason = New-OpeningReason `
        -Kind $OpeningReasonKind `
        -Summary ("Open fixture {0} entry for self-contained opening-flow smoke." -f $ActionKind) `
        -SourceSummaryPath $TargetSummaryPath `
        -DriftChanged $CompareContextAvailable `
        -DriftVerdict $LandingVerdict
    $primaryEntry = New-FixturePrimaryEntry `
        -TabId $TabId `
        -Title $TabTitle `
        -Role $SelectedRole `
        -SummaryPath $TargetSummaryPath `
        -ReportPath $TargetReportPath `
        -CheckPath $TargetCheckPath
    $query = New-FixtureQueryHint `
        -TabId $TabId `
        -TabTitle $TabTitle `
        -Role $SelectedRole `
        -SummaryPath $TargetSummaryPath `
        -CompareExpected $CompareExpected

    Write-JsonFile -Path $openerSummaryPath -Value ([ordered]@{
        schema = "system_compiler.front_page_entry_opener/v0"
        kind = "system_compiler.front_page_entry_opener"
        generated_at_utc = "2026-05-05T00:00:00Z"
        generator = "scripts/system_compiler_front_page_entry_opening_flow_consumer_plan_action_workspace_smoke.ps1"
        result = "ok"
        entry_opener = [ordered]@{
            title = "Fixture Front Page Entry Opener"
            summary = "A minimal opener surface for self-contained opening-flow smoke."
        }
        front_page = [ordered]@{
            summary_path = $openerSummaryPath
            report_markdown_path = $openerReportPath
            check_text_path = $openerCheckPath
            supporting_surfaces = @(
                (New-FrontPageSurface -Id "fixture_target" -Label "fixture target" -Role "target" -Schema "fixture.target/v0" -SummaryPath $TargetSummaryPath -ReportPath $TargetReportPath -CheckPath $TargetCheckPath)
            )
        }
        artifact_context = [ordered]@{
            source_landing_summary_path = $landingSummaryPath
            source_landing_compare_summary_path = ""
            output_root = (Resolve-FullPath -Path $openerRoot)
            opener_summary_path = $openerSummaryPath
            report_markdown_path = $openerReportPath
            check_text_path = $openerCheckPath
        }
        source_landing = [ordered]@{
            source_summary_path = $landingSummaryPath
            root_label = "fixture opening target"
            root_summary_schema = "fixture.target/v0"
            root_summary_kind = "fixture.target"
            root_summary_path = $TargetSummaryPath
            landing_result = "ok"
            recommended_entry_mode = "single_report"
            entry_tier = "primary"
            opening_reason = $reason
            primary_tab_id = $TabId
            available_tab_ids = @($TabId)
            primary_entry = $primaryEntry
            primary_query = $query
        }
        source_landing_compare = $null
        compare_context = [ordered]@{
            available = $CompareContextAvailable
            related_landing_role = ""
            landing_verdict = $LandingVerdict
            primary_query_changed = $CompareContextAvailable
            landing_regression_changed = $CompareContextAvailable
            query_regression_changed = $CompareContextAvailable
            narratives = [object[]]@($compareNarratives)
        }
        open_action = [ordered]@{
            action_id = $ActionId
            action_kind = "open_explain_entry"
            status = "ready"
            selected_tab_id = $TabId
            selected_tab_title = $TabTitle
            selected_route_id = ("fixture-route-{0}" -f $TabId)
            selected_surface_id = ("fixture-surface-{0}" -f $TabId)
            selected_role = $SelectedRole
            query_kind = "default_overview"
            query_scope = "report"
            selection_rule = "single_report"
            compare_expected = $CompareExpected
            followup_query_kinds = @()
            opening_reason = $reason
            target_summary_schema = "fixture.target/v0"
            target_summary_kind = "fixture.target"
            target_summary_path = $TargetSummaryPath
            target_report_markdown_path = $TargetReportPath
            target_check_text_path = $TargetCheckPath
            rationale = "fixture opener is ready for opening judgment carrier smoke"
            compare_context_available = $CompareContextAvailable
            blockers = @()
        }
        inspector_invocation = [ordered]@{
            script_path = "scripts/inspect_system_compiler_artifact_report.ps1"
            ready = $false
            mode = "unresolved"
            query_kind = "default_overview"
            output_format = "json"
            arguments = @()
            powershell_command = @()
            blockers = @("fixture opener does not invoke the real inspector")
        }
        opened_projection = [ordered]@{
            status = "available"
            projection_kind = "fixture_projection"
            source_summary_schema = "fixture.target/v0"
            source_summary_kind = "fixture.target"
            source_summary_path = $TargetSummaryPath
            headline = ("Fixture projection for {0}" -f $EntryName)
            summary_lines = @("fixture opened projection")
            question_lines = @("Should this fixture remain a deterministic smoke input?")
            supporting_summary_paths = @($TargetSummaryPath)
            evidence_paths = @($TargetSummaryPath)
            compare_paths = @()
            blockers = @()
        }
        questions = [ordered]@{
            compare_questions = @("Should fixture opener compare context be attached?")
            next_questions = @("Should this opener become an open-event carrier input?")
        }
        violations = @()
    })

    return [ordered]@{
        SummaryPath = $openerSummaryPath
        ReportPath = $openerReportPath
        CheckPath = $openerCheckPath
    }
}

function New-FixturePlanAction {
    param(
        [string]$OutputRoot,
        [string]$ActionId,
        [int]$Rank,
        [string]$ActionKind,
        [string]$EntryName,
        [string]$DisplayGroup,
        [string]$SelectedRole,
        [string]$OpeningReasonKind,
        [bool]$CompareContextAvailable,
        [string]$LandingVerdict
    )

    $targetRoot = Join-Path $OutputRoot ("target\{0}" -f $ActionKind)
    Ensure-Directory -Path $targetRoot
    $targetSummaryPath = Resolve-FullPath -Path (Join-Path $targetRoot "fixture.target.summary.json")
    $targetReportPath = Resolve-FullPath -Path (Join-Path $targetRoot "fixture.target.report.md")
    $targetCheckPath = Resolve-FullPath -Path (Join-Path $targetRoot "fixture.target.check.txt")
    Write-JsonFile -Path $targetSummaryPath -Value ([ordered]@{
        schema = "fixture.target/v0"
        kind = "fixture.target"
        action_kind = $ActionKind
        entry_name = $EntryName
    })
    Write-TextFile -Path $targetReportPath -Content ("# Fixture target {0}`n" -f $ActionKind)
    Write-TextFile -Path $targetCheckPath -Content ("fixture target {0}`n" -f $ActionKind)

    $tabId = "{0}_tab" -f ($ActionKind -replace "-", "_")
    $tabTitle = "Fixture {0}" -f $ActionKind
    $opener = New-FixtureOpenerSummary `
        -OutputRoot $OutputRoot `
        -ActionId $ActionId `
        -ActionKind $ActionKind `
        -EntryName $EntryName `
        -TabId $tabId `
        -TabTitle $tabTitle `
        -SelectedRole $SelectedRole `
        -OpeningReasonKind $OpeningReasonKind `
        -CompareExpected $CompareContextAvailable `
        -CompareContextAvailable $CompareContextAvailable `
        -LandingVerdict $LandingVerdict `
        -TargetSummaryPath $targetSummaryPath `
        -TargetReportPath $targetReportPath `
        -TargetCheckPath $targetCheckPath

    return [ordered]@{
        action_id = $ActionId
        rank = $Rank
        source_rank = $Rank
        action_kind = $ActionKind
        entry_name = $EntryName
        display_group = $DisplayGroup
        selected_tab_id = $tabId
        selected_role = $SelectedRole
        query_kind = "default_overview"
        query_scope = "report"
        target_summary_schema = "fixture.target/v0"
        target_summary_kind = "fixture.target"
        target_summary_path = $targetSummaryPath
        projection_kind = "fixture_projection"
        opening_reason = (New-OpeningReason -Kind $OpeningReasonKind -Summary ("Select {0} fixture action for opening-flow smoke." -f $ActionKind) -SourceSummaryPath $targetSummaryPath -DriftChanged $CompareContextAvailable -DriftVerdict $LandingVerdict)
        projection_headline = ("fixture projection for {0}" -f $EntryName)
        compare_context_available = $CompareContextAvailable
        landing_verdict = $LandingVerdict
        inspector_ready = $false
        inspector_mode = "unresolved"
        inspector_blockers = @("fixture action does not invoke the real inspector")
        opener_summary_path = [string]$opener.SummaryPath
        opener_report_markdown_path = [string]$opener.ReportPath
        opener_check_text_path = [string]$opener.CheckPath
        expected_consumer_operation = "open-opener-summary"
        reason = ("Fixture {0} action is available to the opening judgment carrier smoke." -f $ActionKind)
    }
}

function New-FixturePlanWorkspace {
    param(
        [string]$PlanWorkspaceRoot
    )

    $planWorkspaceRootPath = Resolve-FullPath -Path $PlanWorkspaceRoot
    $planRoot = Join-Path $planWorkspaceRootPath "plan"
    $selectorRoot = Join-Path $planWorkspaceRootPath "selector"
    Ensure-Directory -Path $planRoot
    Ensure-Directory -Path $selectorRoot

    $planSummaryPath = Resolve-FullPath -Path (Join-Path $planRoot "front-page.entry-opening-flow.consumer.plan.summary.json")
    $planReportPath = Resolve-FullPath -Path (Join-Path $planRoot "front-page.entry-opening-flow.consumer.plan.report.md")
    $planCheckPath = Resolve-FullPath -Path (Join-Path $planRoot "front-page.entry-opening-flow.consumer.plan.check.txt")
    $selectorSummaryPath = Resolve-FullPath -Path (Join-Path $selectorRoot "front-page.entry-opening-flow.consumer.selector.summary.json")
    $selectorReportPath = Resolve-FullPath -Path (Join-Path $selectorRoot "front-page.entry-opening-flow.consumer.selector.report.md")
    $selectorCheckPath = Resolve-FullPath -Path (Join-Path $selectorRoot "front-page.entry-opening-flow.consumer.selector.check.txt")

    Write-JsonFile -Path $selectorSummaryPath -Value ([ordered]@{
        schema = "fixture.consumer_selector/v0"
        kind = "fixture.consumer_selector"
    })
    Write-TextFile -Path $selectorReportPath -Content "# Fixture consumer selector`n"
    Write-TextFile -Path $selectorCheckPath -Content "fixture consumer selector`n"
    Write-TextFile -Path $planReportPath -Content "# Fixture consumer plan`n"
    Write-TextFile -Path $planCheckPath -Content "fixture consumer plan`n"

    $defaultAction = New-FixturePlanAction `
        -OutputRoot $planWorkspaceRootPath `
        -ActionId "open-default" `
        -Rank 0 `
        -ActionKind "default" `
        -EntryName "fixture-default" `
        -DisplayGroup "primary" `
        -SelectedRole "supporting_evidence" `
        -OpeningReasonKind "supporting_evidence" `
        -CompareContextAvailable $false `
        -LandingVerdict ""
    $compareAction = New-FixturePlanAction `
        -OutputRoot $planWorkspaceRootPath `
        -ActionId "open-compare-neighbor" `
        -Rank 1 `
        -ActionKind "compare-neighbor" `
        -EntryName "fixture-compare" `
        -DisplayGroup "compare" `
        -SelectedRole "opening_chain_compare" `
        -OpeningReasonKind "counterfactual_verdict" `
        -CompareContextAvailable $true `
        -LandingVerdict "drifted"
    $actionEntries = @($defaultAction, $compareAction)

    Write-JsonFile -Path $planSummaryPath -Value ([ordered]@{
        schema = "system_compiler.front_page_entry_opening_flow_consumer_plan/v0"
        kind = "system_compiler.front_page_entry_opening_flow_consumer_plan"
        generated_at_utc = "2026-05-05T00:00:00Z"
        generator = "scripts/system_compiler_front_page_entry_opening_flow_consumer_plan_action_workspace_smoke.ps1"
        result = "ok"
        opening_flow_consumer_plan = [ordered]@{
            title = "Fixture Opening Flow Consumer Plan"
            summary = "A minimal schema-valid consumer plan for self-contained plan-action workspace smoke."
        }
        front_page = [ordered]@{
            summary_path = $planSummaryPath
            report_markdown_path = $planReportPath
            check_text_path = $planCheckPath
            supporting_surfaces = @(
                (New-FrontPageSurface -Id "source_consumer_selector" -Label "fixture consumer selector" -Role "source_consumer_selector" -Schema "fixture.consumer_selector/v0" -SummaryPath $selectorSummaryPath -ReportPath $selectorReportPath -CheckPath $selectorCheckPath)
            )
        }
        artifact_context = [ordered]@{
            source_selector_summary_path = $selectorSummaryPath
            output_root = $planWorkspaceRootPath
            consumer_plan_summary_path = $planSummaryPath
            report_markdown_path = $planReportPath
            check_text_path = $planCheckPath
        }
        source_selector = [ordered]@{
            result = "ok"
            open_plan_status = "ready"
            selected_entry_count = 2
            default_entry_name = "fixture-default"
            compare_entry_name = "fixture-compare"
            fallback_entry_count = 0
        }
        planner_status = [ordered]@{
            result = "ok"
            execution_plan_status = "ready"
            planned_action_count = 2
            default_action_name = "fixture-default"
            compare_action_name = "fixture-compare"
            next_action_count = 0
            omitted_entry_count = 0
        }
        execution_plan = [ordered]@{
            status = "ready"
            default_action = $defaultAction
            compare_action = $compareAction
            next_actions = @()
            action_entries = $actionEntries
            planning_notes = @("fixture plan workspace generated because front-page workspace root was unavailable")
        }
        planning_surface = [ordered]@{
            planned_action_ids = @("open-default", "open-compare-neighbor")
            planned_entry_names = @("fixture-default", "fixture-compare")
            omitted_entry_names = @()
            projection_kinds = [ordered]@{
                fixture_projection = 2
            }
            target_summary_schemas = [ordered]@{
                "fixture.target/v0" = 2
            }
            compare_context_action_count = 1
            inspector_ready_action_count = 0
            default_action_id = "open-default"
            compare_action_id = "open-compare-neighbor"
            max_next_action_count = 3
        }
        questions = [ordered]@{
            planner_questions = @("Should this fixture plan remain a self-contained smoke fallback?")
            next_questions = @("Should open-event smoke consume this plan as a judgment carrier input?")
        }
        violations = @()
    })

    return $planSummaryPath
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

$workspaceScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer_plan_action_workspace.ps1"
foreach ($requiredPath in @($workspaceScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $coldRoot = Join-Path $outputRootPath "cold-default"
    $coldPlanWorkspaceRoot = Join-Path $coldRoot "plan-ws"
    $frontPageWorkspaceRootPath = Resolve-FullPath -Path "cmake-build-codex-system-compiler-front-page-smoke"
    if (Test-Path -LiteralPath $frontPageWorkspaceRootPath) {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $workspaceScript,
                "-FrontPageWorkspaceRoot",
                $frontPageWorkspaceRootPath,
                "-OutputRoot",
                $coldRoot,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "front page entry opening-flow consumer plan action cold workspace export failed"
    } else {
        New-FixturePlanWorkspace -PlanWorkspaceRoot $coldPlanWorkspaceRoot | Out-Null
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-SMOKE] plan_bootstrap=fixture"
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $workspaceScript,
                "-PlanWorkspaceRoot",
                $coldPlanWorkspaceRoot,
                "-OutputRoot",
                $coldRoot,
                "-PythonExe",
                $resolvedPythonExe
            ) `
            -FailureMessage "front page entry opening-flow consumer plan action cold workspace export failed"
    }

    $coldSummaryPath = Join-Path $coldRoot "action\front-page.entry-opening-flow.consumer.plan-action.summary.json"
    $coldSummary = Load-JsonObject -Path $coldSummaryPath
    Assert-Condition `
        -Condition ([string]$coldSummary.open_action.action_id -eq "open-default") `
        -Message ("cold default expected open-default but got '{0}'" -f $coldSummary.open_action.action_id)
    Assert-Condition `
        -Condition ([string]$coldSummary.selection_request.effective_selector -eq "default_action") `
        -Message ("cold default expected default selector but got '{0}'" -f $coldSummary.selection_request.effective_selector)

    $hotRoot = Join-Path $outputRootPath "hot-compare-neighbor"
    Invoke-ExternalTool `
        -Executable $powerShellExe `
        -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $workspaceScript,
            "-PlanWorkspaceRoot",
            $coldPlanWorkspaceRoot,
            "-OutputRoot",
            $hotRoot,
            "-ActionKind",
            "compare-neighbor",
            "-PythonExe",
            $resolvedPythonExe
        ) `
        -FailureMessage "front page entry opening-flow consumer plan action hot workspace export failed"

    $hotSummaryPath = Join-Path $hotRoot "action\front-page.entry-opening-flow.consumer.plan-action.summary.json"
    $hotSummary = Load-JsonObject -Path $hotSummaryPath
    Assert-Condition `
        -Condition ([string]$hotSummary.open_action.action_id -eq "open-compare-neighbor") `
        -Message ("hot compare expected open-compare-neighbor but got '{0}'" -f $hotSummary.open_action.action_id)
    Assert-Condition `
        -Condition ([string]$hotSummary.selection_request.effective_selector -eq "action_kind:compare-neighbor") `
        -Message ("hot compare expected action kind selector but got '{0}'" -f $hotSummary.selection_request.effective_selector)
    Assert-Condition `
        -Condition ([string]$hotSummary.open_action.display_group -eq "compare") `
        -Message ("hot compare expected compare display group but got '{0}'" -f $hotSummary.open_action.display_group)

    Write-Host (
        "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-SMOKE] case=cold-default selector={0} action={1} entry={2}" -f
        [string]$coldSummary.selection_request.effective_selector,
        [string]$coldSummary.open_action.action_id,
        [string]$coldSummary.open_action.entry_name
    )
    Write-Host (
        "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-SMOKE] case=hot-compare-neighbor selector={0} action={1} entry={2}" -f
        [string]$hotSummary.selection_request.effective_selector,
        [string]$hotSummary.open_action.action_id,
        [string]$hotSummary.open_action.entry_name
    )
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-SMOKE] output_root={0}" -f $outputRootPath)
