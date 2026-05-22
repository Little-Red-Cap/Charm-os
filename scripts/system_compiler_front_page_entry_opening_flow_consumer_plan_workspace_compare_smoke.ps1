param(
    [string]$PlanWorkspaceRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-workspace-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-workspace-compare-smoke",
    [string]$PythonExe = "",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "front_page_entry_opening_flow_harness.ps1")

function Assert-PathExists {
    param(
        [string]$Path,
        [string]$Message
    )

    Assert-Condition `
        -Condition (-not [string]::IsNullOrWhiteSpace($Path)) `
        -Message ("{0}: missing path" -f $Message)
    Assert-Condition `
        -Condition (Test-Path -LiteralPath $Path) `
        -Message ("{0}: path not found: {1}" -f $Message, $Path)
}

function Assert-SurfacePaths {
    param(
        $Surface,
        [string]$Message
    )

    Assert-PathExists -Path ([string]$Surface.summary_path) -Message ("{0}.summary_path" -f $Message)
    Assert-PathExists -Path ([string]$Surface.report_markdown_path) -Message ("{0}.report_markdown_path" -f $Message)
    Assert-PathExists -Path ([string]$Surface.check_text_path) -Message ("{0}.check_text_path" -f $Message)
}

function Assert-CompareWitnessShape {
    param(
        $CompareSummary,
        [string]$CompareSummaryPath,
        [string]$CaseName
    )

    Assert-SurfacePaths -Surface $CompareSummary.front_page -Message ("case '{0}' front_page" -f $CaseName)
    Assert-Condition `
        -Condition ([string]$CompareSummary.front_page.summary_path -eq [string]$CompareSummary.artifact_context.compare_summary_path) `
        -Message ("case '{0}' front_page summary path does not match artifact context" -f $CaseName)
    Assert-Condition `
        -Condition ([string]$CompareSummary.front_page.summary_path -eq [string]$CompareSummaryPath) `
        -Message ("case '{0}' front_page summary path does not match smoke output" -f $CaseName)

    $surfaces = @($CompareSummary.front_page.supporting_surfaces)
    Assert-Condition `
        -Condition ($surfaces.Count -eq 2) `
        -Message ("case '{0}' expected 2 supporting surfaces but got {1}" -f $CaseName, $surfaces.Count)

    $surfaceById = @{}
    foreach ($surface in $surfaces) {
        $surfaceById[[string]$surface.id] = $surface
        Assert-Condition `
            -Condition ([string]$surface.summary_schema -eq "system_compiler.front_page_entry_opening_flow_consumer_plan/v0") `
            -Message ("case '{0}' supporting surface '{1}' has unexpected schema" -f $CaseName, [string]$surface.id)
        Assert-SurfacePaths -Surface $surface -Message ("case '{0}' supporting surface '{1}'" -f $CaseName, [string]$surface.id)
    }

    foreach ($expectedId in @("baseline_consumer_plan", "candidate_consumer_plan")) {
        Assert-Condition `
            -Condition ($surfaceById.ContainsKey($expectedId)) `
            -Message ("case '{0}' missing supporting surface '{1}'" -f $CaseName, $expectedId)
    }

    $provenance = @($CompareSummary.plan_provenance)
    Assert-Condition `
        -Condition ($provenance.Count -eq 2) `
        -Message ("case '{0}' expected 2 plan provenance entries but got {1}" -f $CaseName, $provenance.Count)

    $provenanceById = @{}
    foreach ($entry in $provenance) {
        $provenanceById[[string]$entry.id] = $entry
        Assert-PathExists -Path ([string]$entry.source_summary_path) -Message ("case '{0}' provenance '{1}'.source_summary_path" -f $CaseName, [string]$entry.id)
        Assert-PathExists -Path ([string]$entry.source_report_markdown_path) -Message ("case '{0}' provenance '{1}'.source_report_markdown_path" -f $CaseName, [string]$entry.id)
        Assert-PathExists -Path ([string]$entry.source_check_text_path) -Message ("case '{0}' provenance '{1}'.source_check_text_path" -f $CaseName, [string]$entry.id)
    }

    foreach ($expectedId in @("baseline_consumer_plan", "candidate_consumer_plan")) {
        Assert-Condition `
            -Condition ($provenanceById.ContainsKey($expectedId)) `
            -Message ("case '{0}' missing provenance '{1}'" -f $CaseName, $expectedId)
        Assert-Condition `
            -Condition ([string]$surfaceById[$expectedId].summary_path -eq [string]$provenanceById[$expectedId].source_summary_path) `
            -Message ("case '{0}' supporting surface and provenance summary path diverged for '{1}'" -f $CaseName, $expectedId)
    }
}

function New-SyntheticPlanWorkspaceDrift {
    param(
        [string]$SourcePlanWorkspaceRoot,
        [string]$OutputPlanWorkspaceRoot
    )

    $sourcePlanRoot = Join-Path $SourcePlanWorkspaceRoot "plan"
    $outputPlanRoot = Join-Path $OutputPlanWorkspaceRoot "plan"
    Remove-PathIfExists -Path $OutputPlanWorkspaceRoot
    Copy-Item -LiteralPath $sourcePlanRoot -Destination $outputPlanRoot -Recurse -Force

    $planSummaryPath = Join-Path $outputPlanRoot "front-page.entry-opening-flow.consumer.plan.summary.json"
    $summary = Load-JsonObject -Path $planSummaryPath
    $removedActionId = [string]$summary.execution_plan.compare_action.action_id
    $removedEntryName = [string]$summary.execution_plan.compare_action.entry_name

    $keptActions = @()
    foreach ($action in @($summary.execution_plan.action_entries)) {
        if ([string]$action.action_id -ne $removedActionId) {
            $keptActions += $action
        }
    }

    $keptNextActions = @()
    foreach ($action in @($summary.execution_plan.next_actions)) {
        if ([string]$action.action_id -ne $removedActionId) {
            $keptNextActions += $action
        }
    }

    $rank = 0
    foreach ($action in @($keptActions)) {
        $action.rank = $rank
        $rank += 1
    }

    $summary.execution_plan.compare_action = [ordered]@{}
    $summary.execution_plan.action_entries = @($keptActions)
    $summary.execution_plan.next_actions = @($keptNextActions)
    $summary.planning_surface.planned_action_ids = @($keptActions | ForEach-Object { [string]$_.action_id })
    $summary.planning_surface.planned_entry_names = @($keptActions | ForEach-Object { [string]$_.entry_name })
    $summary.planning_surface.compare_action_id = ""
    $summary.planner_status.planned_action_count = @($keptActions).Count
    $summary.planner_status.compare_action_name = ""
    $summary.planner_status.next_action_count = @($keptNextActions).Count
    $summary.planner_status.omitted_entry_count = [int]$summary.planner_status.omitted_entry_count + 1
    $summary.planning_surface.omitted_entry_names += $removedEntryName
    $summary.artifact_context.output_root = $outputPlanRoot
    $summary.artifact_context.consumer_plan_summary_path = $planSummaryPath
    $summary.front_page.summary_path = $planSummaryPath

    Write-JsonFile -Path $planSummaryPath -Value $summary
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$planWorkspaceRootPath = Resolve-FullPath -Path $PlanWorkspaceRoot
$outputRootPath = Resolve-FullPath -Path $OutputRoot

Initialize-SmokeOutputRoot -OutputRootPath $outputRootPath -Clean ([bool]$Clean)

$resolvedPythonExe = Resolve-PythonExe -PythonExe $PythonExe
$powerShellExe = Resolve-PowerShellExe

$planWorkspaceScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer_plan_workspace.ps1"
$compareWorkspaceScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_flow_consumer_plan_workspace.ps1"
Assert-RequiredPaths -Paths @($planWorkspaceScript, $compareWorkspaceScript)

Push-Location $repoRoot
try {
    $planSummaryPath = Join-Path $planWorkspaceRootPath "plan\front-page.entry-opening-flow.consumer.plan.summary.json"
    if (Test-Path -LiteralPath $planSummaryPath) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-WORKSPACE-COMPARE-SMOKE] bootstrap=reuse-existing"
    } else {
        Invoke-PowerShellScript `
            -PowerShellExe $powerShellExe `
            -ScriptPath $planWorkspaceScript `
            -ArgumentList @(
                "-FrontPageWorkspaceRoot",
                "cmake-build-codex-system-compiler-front-page-smoke",
                "-OutputRoot",
                $planWorkspaceRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "front page entry opening-flow consumer plan workspace bootstrap failed"
    }

    $syntheticRoot = Join-Path $outputRootPath "_synthetic"
    $driftedPlanWorkspacePath = Join-Path $syntheticRoot "removed-compare-action-plan-workspace"
    New-SyntheticPlanWorkspaceDrift -SourcePlanWorkspaceRoot $planWorkspaceRootPath -OutputPlanWorkspaceRoot $driftedPlanWorkspacePath

    $cases = @(
        [ordered]@{
            Name = "plan-workspace-self-standing"
            BaselinePlanWorkspaceRoot = $planWorkspaceRootPath
            CandidatePlanWorkspaceRoot = $planWorkspaceRootPath
            ExpectedVerdict = "standing"
            ExpectedChangedActions = 0
            ExpectedRemoved = @()
            ExpectedDefaultChanged = $false
            ExpectedCompareChanged = $false
        },
        [ordered]@{
            Name = "plan-workspace-removed-compare-action"
            BaselinePlanWorkspaceRoot = $planWorkspaceRootPath
            CandidatePlanWorkspaceRoot = $driftedPlanWorkspacePath
            ExpectedVerdict = "drifted"
            ExpectedChangedActions = 3
            ExpectedRemoved = @("open-compare-neighbor")
            ExpectedDefaultChanged = $false
            ExpectedCompareChanged = $true
        }
    )

    foreach ($case in $cases) {
        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        Invoke-PowerShellScript `
            -PowerShellExe $powerShellExe `
            -ScriptPath $compareWorkspaceScript `
            -ArgumentList @(
                "-BaselinePlanWorkspaceRoot",
                $case.BaselinePlanWorkspaceRoot,
                "-CandidatePlanWorkspaceRoot",
                $case.CandidatePlanWorkspaceRoot,
                "-OutputRoot",
                $caseOutputRoot,
                "-PythonExe",
                $resolvedPythonExe
            ) `
            -FailureMessage ("front page entry opening-flow consumer plan workspace compare failed for case '{0}'" -f $case.Name)

        $compareSummaryPath = Join-Path $caseOutputRoot "compare\front-page.entry-opening-flow.consumer.plan.compare.summary.json"
        $compareSummary = Load-JsonObject -Path $compareSummaryPath
        Assert-CompareWitnessShape -CompareSummary $compareSummary -CompareSummaryPath $compareSummaryPath -CaseName $case.Name
        Assert-Condition `
            -Condition ([string]$compareSummary.plan_verdict -eq [string]$case.ExpectedVerdict) `
            -Message ("case '{0}' expected verdict '{1}' but got '{2}'" -f $case.Name, $case.ExpectedVerdict, $compareSummary.plan_verdict)
        Assert-Condition `
            -Condition ([int]$compareSummary.plan_action_summary.changed_action_count -eq [int]$case.ExpectedChangedActions) `
            -Message ("case '{0}' expected changed actions '{1}' but got '{2}'" -f $case.Name, $case.ExpectedChangedActions, $compareSummary.plan_action_summary.changed_action_count)
        Assert-Condition `
            -Condition ([bool]$compareSummary.plan_regression_surface.default_action_changed -eq [bool]$case.ExpectedDefaultChanged) `
            -Message ("case '{0}' default changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$compareSummary.plan_regression_surface.compare_action_changed -eq [bool]$case.ExpectedCompareChanged) `
            -Message ("case '{0}' compare changed expectation mismatch" -f $case.Name)

        $removedActions = @([string[]]$compareSummary.plan_changes.ordered_action_id_changes.removed)
        foreach ($actionId in @($case.ExpectedRemoved)) {
            Assert-Condition `
                -Condition ($removedActions -contains [string]$actionId) `
                -Message ("case '{0}' expected removed plan action '{1}'" -f $case.Name, $actionId)
        }

        Write-Host (
            "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-WORKSPACE-COMPARE-SMOKE] case={0} verdict={1} changed={2} added={3} removed={4} default_changed={5} compare_changed={6}" -f
            $case.Name,
            [string]$compareSummary.plan_verdict,
            [int]$compareSummary.plan_action_summary.changed_action_count,
            [int]$compareSummary.plan_action_summary.added_action_count,
            [int]$compareSummary.plan_action_summary.removed_action_count,
            [bool]$compareSummary.plan_regression_surface.default_action_changed,
            [bool]$compareSummary.plan_regression_surface.compare_action_changed
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-WORKSPACE-COMPARE-SMOKE] output_root={0}" -f $outputRootPath)
