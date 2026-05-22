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

$actionWorkspaceSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_flow_consumer_plan_action_workspace_smoke.ps1"
$workspaceScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_open_event_workspace.ps1"
foreach ($requiredPath in @($actionWorkspaceSmokeScript, $workspaceScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $defaultActionPath = Join-Path $actionWorkspaceRootPath "cold-default\action\front-page.entry-opening-flow.consumer.plan-action.summary.json"
    if ((-not $Clean) -and (Test-Path -LiteralPath $defaultActionPath)) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-SMOKE] action_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $actionWorkspaceSmokeScript,
                "-OutputRoot",
                $actionWorkspaceRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "front page entry opening-flow consumer plan action workspace smoke bootstrap failed"
    }

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
        $arguments = @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $workspaceScript,
            "-OutputRoot",
            $caseOutputRoot,
            "-PythonExe",
            $resolvedPythonExe,
            "-Clean"
        ) + @($case.Arguments)

        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList $arguments `
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
