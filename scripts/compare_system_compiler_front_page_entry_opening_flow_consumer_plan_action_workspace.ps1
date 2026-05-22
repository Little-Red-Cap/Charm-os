param(
    [string]$BaselineFrontPageWorkspaceRoot = "",
    [string]$CandidateFrontPageWorkspaceRoot = "",
    [string]$OutputRoot = "out/system-compiler-plan-action-ws-compare",
    [string]$BaselinePlanWorkspaceRoot = "",
    [string]$CandidatePlanWorkspaceRoot = "",
    [string]$BaselineActionWorkspaceRoot = "",
    [string]$CandidateActionWorkspaceRoot = "",
    [string]$BaselineActionId = "",
    [string]$CandidateActionId = "",
    [string]$BaselineActionKind = "",
    [string]$CandidateActionKind = "",
    [string]$BaselineEntryName = "",
    [string]$CandidateEntryName = "",
    [string]$CompareRoot = "",
    [string]$PythonExe = "",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "front_page_entry_opening_flow_harness.ps1")

function Invoke-ActionWorkspaceExport {
    param(
        [string]$Role,
        [string]$ActionWorkspaceRoot,
        [string]$FrontPageWorkspaceRoot,
        [string]$PlanWorkspaceRoot,
        [string]$ActionId,
        [string]$ActionKind,
        [string]$EntryName
    )

    $arguments = @(
        "-OutputRoot",
        $ActionWorkspaceRoot,
        "-PythonExe",
        $resolvedPythonExe
    )

    if (-not [string]::IsNullOrWhiteSpace($FrontPageWorkspaceRoot)) {
        if (-not (Test-Path -LiteralPath $FrontPageWorkspaceRoot)) {
            throw "$Role front-page workspace root not found: $FrontPageWorkspaceRoot"
        }
        $arguments += @("-FrontPageWorkspaceRoot", $FrontPageWorkspaceRoot)
    }
    if (-not [string]::IsNullOrWhiteSpace($PlanWorkspaceRoot)) {
        $arguments += @("-PlanWorkspaceRoot", $PlanWorkspaceRoot)
    }
    if (-not [string]::IsNullOrWhiteSpace($ActionId)) {
        $arguments += @("-ActionId", $ActionId)
    }
    if (-not [string]::IsNullOrWhiteSpace($ActionKind)) {
        $arguments += @("-ActionKind", $ActionKind)
    }
    if (-not [string]::IsNullOrWhiteSpace($EntryName)) {
        $arguments += @("-EntryName", $EntryName)
    }

    Invoke-PowerShellScript `
        -PowerShellExe $powerShellExe `
        -ScriptPath $actionWorkspaceExportScript `
        -ArgumentList $arguments `
        -FailureMessage ("{0} opening-flow consumer plan action workspace export failed" -f $Role)
}

Assert-SingleSelector -Label "baseline" -ActionId $BaselineActionId -ActionKind $BaselineActionKind -EntryName $BaselineEntryName
Assert-SingleSelector -Label "candidate" -ActionId $CandidateActionId -ActionKind $CandidateActionKind -EntryName $CandidateEntryName

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$baselineFrontPageWorkspaceRootPath = Resolve-FullPath -Path $BaselineFrontPageWorkspaceRoot
$candidateFrontPageWorkspaceRootPath = Resolve-FullPath -Path $CandidateFrontPageWorkspaceRoot
$outputRootPath = Resolve-FullPath -Path $OutputRoot
$baselinePlanWorkspaceRootPath = Resolve-FullPath -Path $BaselinePlanWorkspaceRoot
$candidatePlanWorkspaceRootPath = Resolve-FullPath -Path $CandidatePlanWorkspaceRoot
$baselineActionWorkspaceRootPath = if ([string]::IsNullOrWhiteSpace($BaselineActionWorkspaceRoot)) {
    Join-Path $outputRootPath "ba"
} else {
    Resolve-FullPath -Path $BaselineActionWorkspaceRoot
}
$candidateActionWorkspaceRootPath = if ([string]::IsNullOrWhiteSpace($CandidateActionWorkspaceRoot)) {
    Join-Path $outputRootPath "ca"
} else {
    Resolve-FullPath -Path $CandidateActionWorkspaceRoot
}
$compareRootPath = if ([string]::IsNullOrWhiteSpace($CompareRoot)) {
    Join-Path $outputRootPath "compare"
} else {
    Resolve-FullPath -Path $CompareRoot
}

if ($Clean) {
    $cleanPaths = @($compareRootPath, $outputRootPath)
    foreach ($path in $cleanPaths) {
        Assert-CleanPath -Path $path -RootPath $repoRoot
    }
    foreach ($path in $cleanPaths) {
        Remove-PathIfExists -Path $path
    }
}
Ensure-Directory -Path $outputRootPath

$resolvedPythonExe = Resolve-PythonExe -PythonExe $PythonExe
$powerShellExe = Resolve-PowerShellExe
$actionWorkspaceExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer_plan_action_workspace.ps1"
$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_flow_consumer_plan_action.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_consumer_plan_action_compare.py"
Assert-RequiredPaths -Paths @($actionWorkspaceExportScript, $compareScript, $validateScript)

Push-Location $repoRoot
try {
    $baselineActionSummaryPath = Resolve-OpeningFlowConsumerPlanActionSummaryPath -ActionWorkspaceRoot $baselineActionWorkspaceRootPath
    $candidateActionSummaryPath = Resolve-OpeningFlowConsumerPlanActionSummaryPath -ActionWorkspaceRoot $candidateActionWorkspaceRootPath
    $baselineActionAvailable = (-not $Clean) -and (Test-Path -LiteralPath $baselineActionSummaryPath)
    $candidateActionAvailable = (-not $Clean) -and (Test-Path -LiteralPath $candidateActionSummaryPath)

    if ($baselineActionAvailable) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE] baseline_action_workspace=reuse-existing"
    } else {
        if ([string]::IsNullOrWhiteSpace($baselineFrontPageWorkspaceRootPath) -and [string]::IsNullOrWhiteSpace($baselinePlanWorkspaceRootPath)) {
            throw "baseline front-page or plan workspace root is required unless BaselineActionWorkspaceRoot already contains an action summary"
        }
        Invoke-ActionWorkspaceExport `
            -Role "baseline" `
            -ActionWorkspaceRoot $baselineActionWorkspaceRootPath `
            -FrontPageWorkspaceRoot $baselineFrontPageWorkspaceRootPath `
            -PlanWorkspaceRoot $baselinePlanWorkspaceRootPath `
            -ActionId $BaselineActionId `
            -ActionKind $BaselineActionKind `
            -EntryName $BaselineEntryName
        $baselineActionSummaryPath = Resolve-OpeningFlowConsumerPlanActionSummaryPath -ActionWorkspaceRoot $baselineActionWorkspaceRootPath
    }

    if ($candidateActionAvailable) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE] candidate_action_workspace=reuse-existing"
    } else {
        if ([string]::IsNullOrWhiteSpace($candidateFrontPageWorkspaceRootPath) -and [string]::IsNullOrWhiteSpace($candidatePlanWorkspaceRootPath)) {
            throw "candidate front-page or plan workspace root is required unless CandidateActionWorkspaceRoot already contains an action summary"
        }
        Invoke-ActionWorkspaceExport `
            -Role "candidate" `
            -ActionWorkspaceRoot $candidateActionWorkspaceRootPath `
            -FrontPageWorkspaceRoot $candidateFrontPageWorkspaceRootPath `
            -PlanWorkspaceRoot $candidatePlanWorkspaceRootPath `
            -ActionId $CandidateActionId `
            -ActionKind $CandidateActionKind `
            -EntryName $CandidateEntryName
        $candidateActionSummaryPath = Resolve-OpeningFlowConsumerPlanActionSummaryPath -ActionWorkspaceRoot $candidateActionWorkspaceRootPath
    }

    foreach ($summaryPath in @($baselineActionSummaryPath, $candidateActionSummaryPath)) {
        if (-not (Test-Path -LiteralPath $summaryPath)) {
            throw "opening-flow consumer plan action summary not found: $summaryPath"
        }
    }

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $compareScript,
            "--baseline",
            $baselineActionSummaryPath,
            "--candidate",
            $candidateActionSummaryPath,
            "--output-root",
            $compareRootPath
        ) `
        -FailureMessage "opening-flow consumer plan action workspace compare export failed"

    $compareSummaryPath = Join-Path $compareRootPath "front-page.entry-opening-flow.consumer.plan-action.compare.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateScript, "--summary", $compareSummaryPath) `
        -FailureMessage "opening-flow consumer plan action workspace compare validation failed"
} finally {
    Pop-Location
}

$compareSummaryPath = Join-Path $compareRootPath "front-page.entry-opening-flow.consumer.plan-action.compare.summary.json"
$compareReportPath = Join-Path $compareRootPath "front-page.entry-opening-flow.consumer.plan-action.compare.report.md"
$compareCheckPath = Join-Path $compareRootPath "front-page.entry-opening-flow.consumer.plan-action.compare.check.txt"

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE] baseline_front_page_workspace_root={0}" -f $baselineFrontPageWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE] candidate_front_page_workspace_root={0}" -f $candidateFrontPageWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE] baseline_plan_workspace_root={0}" -f $baselinePlanWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE] candidate_plan_workspace_root={0}" -f $candidatePlanWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE] baseline_action_workspace_root={0}" -f $baselineActionWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE] candidate_action_workspace_root={0}" -f $candidateActionWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE] compare_root={0}" -f $compareRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE] summary={0}" -f $compareSummaryPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE] report={0}" -f $compareReportPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE] check={0}" -f $compareCheckPath)
