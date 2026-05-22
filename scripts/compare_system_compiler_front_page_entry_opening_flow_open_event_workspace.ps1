param(
    [string]$BaselineFrontPageWorkspaceRoot = "",
    [string]$CandidateFrontPageWorkspaceRoot = "",
    [string]$OutputRoot = "out/system-compiler-open-event-ws-compare",
    [string]$BaselinePlanWorkspaceRoot = "",
    [string]$CandidatePlanWorkspaceRoot = "",
    [string]$BaselineActionWorkspaceRoot = "",
    [string]$CandidateActionWorkspaceRoot = "",
    [string]$BaselineOpenEventWorkspaceRoot = "",
    [string]$CandidateOpenEventWorkspaceRoot = "",
    [string]$BaselineOpenEventSummaryPath = "",
    [string]$CandidateOpenEventSummaryPath = "",
    [string]$BaselineActionSummaryPath = "",
    [string]$CandidateActionSummaryPath = "",
    [string]$BaselineActionCompareSummaryPath = "",
    [string]$CandidateActionCompareSummaryPath = "",
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

function Assert-SingleSelector {
    param(
        [string]$Label,
        [string]$ActionId,
        [string]$ActionKind,
        [string]$EntryName
    )

    $selectorCount = 0
    foreach ($value in @($ActionId, $ActionKind, $EntryName)) {
        if (-not [string]::IsNullOrWhiteSpace($value)) {
            $selectorCount += 1
        }
    }

    if ($selectorCount -gt 1) {
        throw ("{0}: use only one of ActionId, ActionKind, or EntryName" -f $Label)
    }
}

function Invoke-OpenEventWorkspaceExport {
    param(
        [string]$Role,
        [string]$OpenEventWorkspaceRoot,
        [string]$FrontPageWorkspaceRoot,
        [string]$PlanWorkspaceRoot,
        [string]$ActionWorkspaceRoot,
        [string]$ActionSummaryPath,
        [string]$ActionCompareSummaryPath,
        [string]$ActionId,
        [string]$ActionKind,
        [string]$EntryName
    )

    $arguments = @(
        "-OutputRoot",
        $OpenEventWorkspaceRoot,
        "-PythonExe",
        $resolvedPythonExe
    )

    if (-not [string]::IsNullOrWhiteSpace($FrontPageWorkspaceRoot)) {
        $arguments += @("-FrontPageWorkspaceRoot", $FrontPageWorkspaceRoot)
    }
    if (-not [string]::IsNullOrWhiteSpace($PlanWorkspaceRoot)) {
        $arguments += @("-PlanWorkspaceRoot", $PlanWorkspaceRoot)
    }
    if (-not [string]::IsNullOrWhiteSpace($ActionWorkspaceRoot)) {
        $arguments += @("-ActionWorkspaceRoot", $ActionWorkspaceRoot)
    }
    if (-not [string]::IsNullOrWhiteSpace($ActionSummaryPath)) {
        $arguments += @("-ActionSummaryPath", $ActionSummaryPath)
    }
    if (-not [string]::IsNullOrWhiteSpace($ActionCompareSummaryPath)) {
        $arguments += @("-ActionCompareSummaryPath", $ActionCompareSummaryPath)
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
        -ScriptPath $openEventWorkspaceExportScript `
        -ArgumentList $arguments `
        -FailureMessage ("{0} opening-flow open-event workspace export failed" -f $Role)
}

Assert-SingleSelector -Label "baseline" -ActionId $BaselineActionId -ActionKind $BaselineActionKind -EntryName $BaselineEntryName
Assert-SingleSelector -Label "candidate" -ActionId $CandidateActionId -ActionKind $CandidateActionKind -EntryName $CandidateEntryName

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$baselineFrontPageWorkspaceRootPath = Resolve-FullPath -Path $BaselineFrontPageWorkspaceRoot
$candidateFrontPageWorkspaceRootPath = Resolve-FullPath -Path $CandidateFrontPageWorkspaceRoot
$outputRootPath = Resolve-FullPath -Path $OutputRoot
$baselinePlanWorkspaceRootPath = Resolve-FullPath -Path $BaselinePlanWorkspaceRoot
$candidatePlanWorkspaceRootPath = Resolve-FullPath -Path $CandidatePlanWorkspaceRoot
$baselineActionWorkspaceRootPath = Resolve-FullPath -Path $BaselineActionWorkspaceRoot
$candidateActionWorkspaceRootPath = Resolve-FullPath -Path $CandidateActionWorkspaceRoot
$baselineOpenEventWorkspaceRootPath = if ([string]::IsNullOrWhiteSpace($BaselineOpenEventWorkspaceRoot)) {
    Join-Path $outputRootPath "be"
} else {
    Resolve-FullPath -Path $BaselineOpenEventWorkspaceRoot
}
$candidateOpenEventWorkspaceRootPath = if ([string]::IsNullOrWhiteSpace($CandidateOpenEventWorkspaceRoot)) {
    Join-Path $outputRootPath "ce"
} else {
    Resolve-FullPath -Path $CandidateOpenEventWorkspaceRoot
}
$baselineOpenEventSummaryPath = Resolve-FullPath -Path $BaselineOpenEventSummaryPath
$candidateOpenEventSummaryPath = Resolve-FullPath -Path $CandidateOpenEventSummaryPath
$baselineActionSummaryPath = Resolve-FullPath -Path $BaselineActionSummaryPath
$candidateActionSummaryPath = Resolve-FullPath -Path $CandidateActionSummaryPath
$baselineActionCompareSummaryPath = Resolve-FullPath -Path $BaselineActionCompareSummaryPath
$candidateActionCompareSummaryPath = Resolve-FullPath -Path $CandidateActionCompareSummaryPath
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

$openEventWorkspaceExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_open_event_workspace.ps1"
$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_flow_open_event.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_open_event_compare.py"
Assert-RequiredPaths -Paths @($openEventWorkspaceExportScript, $compareScript, $validateScript)

Push-Location $repoRoot
try {
    if ([string]::IsNullOrWhiteSpace($baselineOpenEventSummaryPath)) {
        $baselineOpenEventSummaryPath = Resolve-OpeningFlowOpenEventSummaryPath -WorkspaceRoot $baselineOpenEventWorkspaceRootPath
        $baselineEventAvailable = (Test-Path -LiteralPath $baselineOpenEventSummaryPath) -and (
            (-not $Clean) -or (-not [string]::IsNullOrWhiteSpace($BaselineOpenEventWorkspaceRoot))
        )
        if ($baselineEventAvailable) {
            Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-COMPARE] baseline_open_event_workspace=reuse-existing"
        } else {
            if (
                [string]::IsNullOrWhiteSpace($baselineFrontPageWorkspaceRootPath) -and
                [string]::IsNullOrWhiteSpace($baselinePlanWorkspaceRootPath) -and
                [string]::IsNullOrWhiteSpace($baselineActionWorkspaceRootPath) -and
                [string]::IsNullOrWhiteSpace($baselineActionSummaryPath)
            ) {
                throw "baseline front-page, plan, action workspace, or action summary is required unless BaselineOpenEventSummaryPath or BaselineOpenEventWorkspaceRoot already contains an open-event summary"
            }
            Invoke-OpenEventWorkspaceExport `
                -Role "baseline" `
                -OpenEventWorkspaceRoot $baselineOpenEventWorkspaceRootPath `
                -FrontPageWorkspaceRoot $baselineFrontPageWorkspaceRootPath `
                -PlanWorkspaceRoot $baselinePlanWorkspaceRootPath `
                -ActionWorkspaceRoot $baselineActionWorkspaceRootPath `
                -ActionSummaryPath $baselineActionSummaryPath `
                -ActionCompareSummaryPath $baselineActionCompareSummaryPath `
                -ActionId $BaselineActionId `
                -ActionKind $BaselineActionKind `
                -EntryName $BaselineEntryName
            $baselineOpenEventSummaryPath = Resolve-OpeningFlowOpenEventSummaryPath -WorkspaceRoot $baselineOpenEventWorkspaceRootPath
        }
    }

    if ([string]::IsNullOrWhiteSpace($candidateOpenEventSummaryPath)) {
        $candidateOpenEventSummaryPath = Resolve-OpeningFlowOpenEventSummaryPath -WorkspaceRoot $candidateOpenEventWorkspaceRootPath
        $candidateEventAvailable = (Test-Path -LiteralPath $candidateOpenEventSummaryPath) -and (
            (-not $Clean) -or (-not [string]::IsNullOrWhiteSpace($CandidateOpenEventWorkspaceRoot))
        )
        if ($candidateEventAvailable) {
            Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-COMPARE] candidate_open_event_workspace=reuse-existing"
        } else {
            if (
                [string]::IsNullOrWhiteSpace($candidateFrontPageWorkspaceRootPath) -and
                [string]::IsNullOrWhiteSpace($candidatePlanWorkspaceRootPath) -and
                [string]::IsNullOrWhiteSpace($candidateActionWorkspaceRootPath) -and
                [string]::IsNullOrWhiteSpace($candidateActionSummaryPath)
            ) {
                throw "candidate front-page, plan, action workspace, or action summary is required unless CandidateOpenEventSummaryPath or CandidateOpenEventWorkspaceRoot already contains an open-event summary"
            }
            Invoke-OpenEventWorkspaceExport `
                -Role "candidate" `
                -OpenEventWorkspaceRoot $candidateOpenEventWorkspaceRootPath `
                -FrontPageWorkspaceRoot $candidateFrontPageWorkspaceRootPath `
                -PlanWorkspaceRoot $candidatePlanWorkspaceRootPath `
                -ActionWorkspaceRoot $candidateActionWorkspaceRootPath `
                -ActionSummaryPath $candidateActionSummaryPath `
                -ActionCompareSummaryPath $candidateActionCompareSummaryPath `
                -ActionId $CandidateActionId `
                -ActionKind $CandidateActionKind `
                -EntryName $CandidateEntryName
            $candidateOpenEventSummaryPath = Resolve-OpeningFlowOpenEventSummaryPath -WorkspaceRoot $candidateOpenEventWorkspaceRootPath
        }
    }

    foreach ($summaryPath in @($baselineOpenEventSummaryPath, $candidateOpenEventSummaryPath)) {
        if (-not (Test-Path -LiteralPath $summaryPath)) {
            throw "opening-flow open-event summary not found: $summaryPath"
        }
    }

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $compareScript,
            "--baseline",
            $baselineOpenEventSummaryPath,
            "--candidate",
            $candidateOpenEventSummaryPath,
            "--output-root",
            $compareRootPath
        ) `
        -FailureMessage "opening-flow open-event workspace compare export failed"

    $compareSummaryPath = Join-Path $compareRootPath "front-page.entry-opening-flow.open-event.compare.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateScript, "--summary", $compareSummaryPath) `
        -FailureMessage "opening-flow open-event workspace compare validation failed"
} finally {
    Pop-Location
}

$compareSummaryPath = Join-Path $compareRootPath "front-page.entry-opening-flow.open-event.compare.summary.json"
$compareReportPath = Join-Path $compareRootPath "front-page.entry-opening-flow.open-event.compare.report.md"
$compareCheckPath = Join-Path $compareRootPath "front-page.entry-opening-flow.open-event.compare.check.txt"

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-COMPARE] baseline_front_page_workspace_root={0}" -f $baselineFrontPageWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-COMPARE] candidate_front_page_workspace_root={0}" -f $candidateFrontPageWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-COMPARE] baseline_plan_workspace_root={0}" -f $baselinePlanWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-COMPARE] candidate_plan_workspace_root={0}" -f $candidatePlanWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-COMPARE] baseline_action_workspace_root={0}" -f $baselineActionWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-COMPARE] candidate_action_workspace_root={0}" -f $candidateActionWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-COMPARE] baseline_open_event_workspace_root={0}" -f $baselineOpenEventWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-COMPARE] candidate_open_event_workspace_root={0}" -f $candidateOpenEventWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-COMPARE] baseline_open_event_summary={0}" -f $baselineOpenEventSummaryPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-COMPARE] candidate_open_event_summary={0}" -f $candidateOpenEventSummaryPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-COMPARE] compare_root={0}" -f $compareRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-COMPARE] summary={0}" -f $compareSummaryPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-COMPARE] report={0}" -f $compareReportPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-COMPARE] check={0}" -f $compareCheckPath)
