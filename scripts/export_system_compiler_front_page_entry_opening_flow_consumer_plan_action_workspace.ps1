param(
    [string]$FrontPageWorkspaceRoot = "",
    [string]$OutputRoot = "out/system-compiler-plan-action-ws",
    [string]$PlanWorkspaceRoot = "",
    [string]$ActionRoot = "",
    [string]$ActionId = "",
    [string]$ActionKind = "",
    [string]$EntryName = "",
    [string]$PythonExe = "",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "front_page_entry_opening_flow_harness.ps1")

Assert-SingleSelector -Label "action workspace" -ActionId $ActionId -ActionKind $ActionKind -EntryName $EntryName

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$frontPageWorkspaceRootPath = Resolve-FullPath -Path $FrontPageWorkspaceRoot
$outputRootPath = Resolve-FullPath -Path $OutputRoot
$planWorkspaceRootPath = if ([string]::IsNullOrWhiteSpace($PlanWorkspaceRoot)) {
    Join-Path $outputRootPath "plan-ws"
} else {
    Resolve-FullPath -Path $PlanWorkspaceRoot
}
$actionRootPath = if ([string]::IsNullOrWhiteSpace($ActionRoot)) {
    Join-Path $outputRootPath "action"
} else {
    Resolve-FullPath -Path $ActionRoot
}

if ($Clean) {
    $cleanPaths = @($actionRootPath, $planWorkspaceRootPath, $outputRootPath)
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

$planWorkspaceScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer_plan_workspace.ps1"
$actionExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer_plan_action.py"
$actionValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_consumer_plan_action.py"
Assert-RequiredPaths -Paths @($planWorkspaceScript, $actionExportScript, $actionValidateScript)

Push-Location $repoRoot
try {
    $planSummaryPath = Join-Path $planWorkspaceRootPath "plan\front-page.entry-opening-flow.consumer.plan.summary.json"
    $planAvailable = (-not $Clean) -and (Test-Path -LiteralPath $planSummaryPath)
    if ($planAvailable) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE] plan_workspace=reuse-existing"
    } else {
        if ([string]::IsNullOrWhiteSpace($frontPageWorkspaceRootPath)) {
            throw "front-page workspace root is required unless PlanWorkspaceRoot already contains a plan summary"
        }
        if (-not (Test-Path -LiteralPath $frontPageWorkspaceRootPath)) {
            throw "front-page workspace root not found: $frontPageWorkspaceRootPath"
        }
        Invoke-PowerShellScript `
            -PowerShellExe $powerShellExe `
            -ScriptPath $planWorkspaceScript `
            -ArgumentList @(
                "-FrontPageWorkspaceRoot",
                $frontPageWorkspaceRootPath,
                "-OutputRoot",
                $planWorkspaceRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "front page entry opening-flow consumer plan workspace export failed"
    }

    if (-not (Test-Path -LiteralPath $planSummaryPath)) {
        throw "opening-flow consumer plan summary not found: $planSummaryPath"
    }

    $actionArguments = @(
        $actionExportScript,
        "--plan",
        $planSummaryPath,
        "--output-root",
        $actionRootPath
    )
    if (-not [string]::IsNullOrWhiteSpace($ActionId)) {
        $actionArguments += @("--action-id", $ActionId)
    }
    if (-not [string]::IsNullOrWhiteSpace($ActionKind)) {
        $actionArguments += @("--action-kind", $ActionKind)
    }
    if (-not [string]::IsNullOrWhiteSpace($EntryName)) {
        $actionArguments += @("--entry-name", $EntryName)
    }

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList $actionArguments `
        -FailureMessage "front page entry opening-flow consumer plan action export failed"

    $actionSummaryPath = Join-Path $actionRootPath "front-page.entry-opening-flow.consumer.plan-action.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($actionValidateScript, "--summary", $actionSummaryPath) `
        -FailureMessage "front page entry opening-flow consumer plan action validation failed"
} finally {
    Pop-Location
}

$actionSummaryPath = Join-Path $actionRootPath "front-page.entry-opening-flow.consumer.plan-action.summary.json"
$actionReportPath = Join-Path $actionRootPath "front-page.entry-opening-flow.consumer.plan-action.report.md"
$actionCheckPath = Join-Path $actionRootPath "front-page.entry-opening-flow.consumer.plan-action.check.txt"

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE] front_page_workspace_root={0}" -f $frontPageWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE] plan_workspace_root={0}" -f $planWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE] action_root={0}" -f $actionRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE] summary={0}" -f $actionSummaryPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE] report={0}" -f $actionReportPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE] check={0}" -f $actionCheckPath)
