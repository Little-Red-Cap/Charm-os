param(
    [string]$BaselineFrontPageWorkspaceRoot = "",
    [string]$CandidateFrontPageWorkspaceRoot = "",
    [string]$OutputRoot = "out/system-compiler-plan-ws-compare",
    [string]$BaselinePlanWorkspaceRoot = "",
    [string]$CandidatePlanWorkspaceRoot = "",
    [string]$CompareRoot = "",
    [string]$PythonExe = "",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "front_page_entry_opening_flow_harness.ps1")

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$baselineFrontPageWorkspaceRootPath = Resolve-FullPath -Path $BaselineFrontPageWorkspaceRoot
$candidateFrontPageWorkspaceRootPath = Resolve-FullPath -Path $CandidateFrontPageWorkspaceRoot
$outputRootPath = Resolve-FullPath -Path $OutputRoot
$baselinePlanWorkspaceRootPath = if ([string]::IsNullOrWhiteSpace($BaselinePlanWorkspaceRoot)) {
    Join-Path $outputRootPath "bp"
} else {
    Resolve-FullPath -Path $BaselinePlanWorkspaceRoot
}
$candidatePlanWorkspaceRootPath = if ([string]::IsNullOrWhiteSpace($CandidatePlanWorkspaceRoot)) {
    Join-Path $outputRootPath "cp"
} else {
    Resolve-FullPath -Path $CandidatePlanWorkspaceRoot
}
$compareRootPath = if ([string]::IsNullOrWhiteSpace($CompareRoot)) {
    Join-Path $outputRootPath "compare"
} else {
    Resolve-FullPath -Path $CompareRoot
}

if ($Clean) {
    $cleanPaths = @($baselinePlanWorkspaceRootPath, $candidatePlanWorkspaceRootPath, $compareRootPath, $outputRootPath)
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
$planWorkspaceExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer_plan_workspace.ps1"
$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_flow_consumer_plan.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_consumer_plan_compare.py"
Assert-RequiredPaths -Paths @($planWorkspaceExportScript, $compareScript, $validateScript)

Push-Location $repoRoot
try {
    $baselinePlanSummaryPath = Resolve-OpeningFlowConsumerPlanSummaryPath -PlanWorkspaceRoot $baselinePlanWorkspaceRootPath
    $candidatePlanSummaryPath = Resolve-OpeningFlowConsumerPlanSummaryPath -PlanWorkspaceRoot $candidatePlanWorkspaceRootPath
    $baselinePlanAvailable = (-not $Clean) -and (Test-Path -LiteralPath $baselinePlanSummaryPath)
    $candidatePlanAvailable = (-not $Clean) -and (Test-Path -LiteralPath $candidatePlanSummaryPath)

    if ($baselinePlanAvailable) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-WORKSPACE-COMPARE] baseline_plan_workspace=reuse-existing"
    } else {
        if ([string]::IsNullOrWhiteSpace($baselineFrontPageWorkspaceRootPath)) {
            throw "baseline front-page workspace root is required unless BaselinePlanWorkspaceRoot already contains a plan summary"
        }
        if (-not (Test-Path -LiteralPath $baselineFrontPageWorkspaceRootPath)) {
            throw "baseline front-page workspace root not found: $baselineFrontPageWorkspaceRootPath"
        }
        Invoke-PowerShellScript `
            -PowerShellExe $powerShellExe `
            -ScriptPath $planWorkspaceExportScript `
            -ArgumentList @(
                "-FrontPageWorkspaceRoot",
                $baselineFrontPageWorkspaceRootPath,
                "-OutputRoot",
                $baselinePlanWorkspaceRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "baseline opening-flow consumer plan workspace export failed"
        $baselinePlanSummaryPath = Resolve-OpeningFlowConsumerPlanSummaryPath -PlanWorkspaceRoot $baselinePlanWorkspaceRootPath
    }

    if ($candidatePlanAvailable) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-WORKSPACE-COMPARE] candidate_plan_workspace=reuse-existing"
    } else {
        if ([string]::IsNullOrWhiteSpace($candidateFrontPageWorkspaceRootPath)) {
            throw "candidate front-page workspace root is required unless CandidatePlanWorkspaceRoot already contains a plan summary"
        }
        if (-not (Test-Path -LiteralPath $candidateFrontPageWorkspaceRootPath)) {
            throw "candidate front-page workspace root not found: $candidateFrontPageWorkspaceRootPath"
        }
        Invoke-PowerShellScript `
            -PowerShellExe $powerShellExe `
            -ScriptPath $planWorkspaceExportScript `
            -ArgumentList @(
                "-FrontPageWorkspaceRoot",
                $candidateFrontPageWorkspaceRootPath,
                "-OutputRoot",
                $candidatePlanWorkspaceRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "candidate opening-flow consumer plan workspace export failed"
        $candidatePlanSummaryPath = Resolve-OpeningFlowConsumerPlanSummaryPath -PlanWorkspaceRoot $candidatePlanWorkspaceRootPath
    }

    foreach ($summaryPath in @($baselinePlanSummaryPath, $candidatePlanSummaryPath)) {
        if (-not (Test-Path -LiteralPath $summaryPath)) {
            throw "opening-flow consumer plan summary not found: $summaryPath"
        }
    }

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $compareScript,
            "--baseline",
            $baselinePlanSummaryPath,
            "--candidate",
            $candidatePlanSummaryPath,
            "--output-root",
            $compareRootPath
        ) `
        -FailureMessage "opening-flow consumer plan workspace compare export failed"

    $compareSummaryPath = Join-Path $compareRootPath "front-page.entry-opening-flow.consumer.plan.compare.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateScript, "--summary", $compareSummaryPath) `
        -FailureMessage "opening-flow consumer plan workspace compare validation failed"
} finally {
    Pop-Location
}

$compareSummaryPath = Join-Path $compareRootPath "front-page.entry-opening-flow.consumer.plan.compare.summary.json"
$compareReportPath = Join-Path $compareRootPath "front-page.entry-opening-flow.consumer.plan.compare.report.md"
$compareCheckPath = Join-Path $compareRootPath "front-page.entry-opening-flow.consumer.plan.compare.check.txt"

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-WORKSPACE-COMPARE] baseline_front_page_workspace_root={0}" -f $baselineFrontPageWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-WORKSPACE-COMPARE] candidate_front_page_workspace_root={0}" -f $candidateFrontPageWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-WORKSPACE-COMPARE] baseline_plan_workspace_root={0}" -f $baselinePlanWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-WORKSPACE-COMPARE] candidate_plan_workspace_root={0}" -f $candidatePlanWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-WORKSPACE-COMPARE] compare_root={0}" -f $compareRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-WORKSPACE-COMPARE] summary={0}" -f $compareSummaryPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-WORKSPACE-COMPARE] report={0}" -f $compareReportPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-WORKSPACE-COMPARE] check={0}" -f $compareCheckPath)
