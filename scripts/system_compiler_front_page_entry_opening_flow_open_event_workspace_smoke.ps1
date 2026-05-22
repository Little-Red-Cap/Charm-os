param(
    [string]$ActionWorkspaceRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-action-workspace-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-open-event-workspace-smoke",
    [string]$PythonExe = "",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "front_page_entry_opening_flow_harness.ps1")

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$actionWorkspaceRootPath = Resolve-FullPath -Path $ActionWorkspaceRoot
$outputRootPath = Resolve-FullPath -Path $OutputRoot

Initialize-SmokeOutputRoot -OutputRootPath $outputRootPath -Clean ([bool]$Clean)

$resolvedPythonExe = Resolve-PythonExe -PythonExe $PythonExe
$powerShellExe = Resolve-PowerShellExe

$workspaceScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_open_event_workspace.ps1"
Assert-RequiredPaths -Paths @($workspaceScript)

Push-Location $repoRoot
try {
    $defaultActionPath = Ensure-OpeningFlowConsumerPlanActionWorkspaceSmoke `
        -ScriptsRoot $PSScriptRoot `
        -ActionWorkspaceRootPath $actionWorkspaceRootPath `
        -PythonExe $resolvedPythonExe `
        -PowerShellExe $powerShellExe `
        -Clean ([bool]$Clean) `
        -LogPrefix "[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-SMOKE]"

    $cases = @(
        [ordered]@{
            Name = "from-action-summary"
            Arguments = @(
                "-ActionSummaryPath",
                $defaultActionPath
            )
            ExpectedStatus = "accepted"
            ExpectedActionId = "open-default"
        },
        [ordered]@{
            Name = "from-plan-workspace-compare-neighbor"
            Arguments = @(
                "-PlanWorkspaceRoot",
                (Join-Path $actionWorkspaceRootPath "cold-default\plan-ws"),
                "-ActionKind",
                "compare-neighbor"
            )
            ExpectedStatus = "accepted"
            ExpectedActionId = "open-compare-neighbor"
        }
    )

    foreach ($case in $cases) {
        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        Invoke-PowerShellScript `
            -PowerShellExe $powerShellExe `
            -ScriptPath $workspaceScript `
            -ArgumentList (@(
                "-OutputRoot",
                $caseOutputRoot,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) + @($case.Arguments)) `
            -FailureMessage ("front page entry opening-flow open-event workspace export failed for case '{0}'" -f $case.Name)

        $summaryPath = Join-Path $caseOutputRoot "open-event\front-page.entry-opening-flow.open-event.summary.json"
        $summary = Load-JsonObject -Path $summaryPath
        Assert-Condition `
            -Condition ([string]$summary.open_event.status -eq [string]$case.ExpectedStatus) `
            -Message ("case '{0}' expected status '{1}' but got '{2}'" -f $case.Name, $case.ExpectedStatus, $summary.open_event.status)
        Assert-Condition `
            -Condition ([string]$summary.consumer_decision.selected_consumer.selected_action_id -eq [string]$case.ExpectedActionId) `
            -Message ("case '{0}' expected selected action '{1}' but got '{2}'" -f $case.Name, $case.ExpectedActionId, $summary.consumer_decision.selected_consumer.selected_action_id)
        Assert-Condition `
            -Condition ([int]$summary.consumer_decision.candidate_consumer_count -ge 1) `
            -Message ("case '{0}' expected at least one candidate consumer" -f $case.Name)

        Write-Host (
            "[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-SMOKE] case={0} status={1} action={2} candidates={3} rejected={4}" -f
            $case.Name,
            [string]$summary.open_event.status,
            [string]$summary.consumer_decision.selected_consumer.selected_action_id,
            [int]$summary.consumer_decision.candidate_consumer_count,
            [int]$summary.consumer_decision.rejected_consumer_count
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-SMOKE] output_root={0}" -f $outputRootPath)
