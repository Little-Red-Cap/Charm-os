param(
    [Parameter(Mandatory = $true)]
    [string]$FrontPageWorkspaceRoot,
    [string]$OutputRoot = "out/system-compiler-plan-ws",
    [string]$SelectorWorkspaceRoot = "",
    [string]$PlanRoot = "",
    [string]$PythonExe = "",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "front_page_entry_opening_flow_harness.ps1")

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$frontPageWorkspaceRootPath = Resolve-FullPath -Path $FrontPageWorkspaceRoot
$outputRootPath = Resolve-FullPath -Path $OutputRoot
$selectorWorkspaceRootPath = if ([string]::IsNullOrWhiteSpace($SelectorWorkspaceRoot)) {
    Join-Path $outputRootPath "sel"
} else {
    Resolve-FullPath -Path $SelectorWorkspaceRoot
}
$planRootPath = if ([string]::IsNullOrWhiteSpace($PlanRoot)) {
    Join-Path $outputRootPath "plan"
} else {
    Resolve-FullPath -Path $PlanRoot
}

if (-not (Test-Path -LiteralPath $frontPageWorkspaceRootPath)) {
    throw "front-page workspace root not found: $frontPageWorkspaceRootPath"
}

if ($Clean) {
    $cleanPaths = @($selectorWorkspaceRootPath, $planRootPath, $outputRootPath)
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
$selectorWorkspaceExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer_selector_workspace.ps1"
$planExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer_plan.py"
$planValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_consumer_plan.py"
Assert-RequiredPaths -Paths @($selectorWorkspaceExportScript, $planExportScript, $planValidateScript)

Push-Location $repoRoot
try {
    Invoke-PowerShellScript `
        -PowerShellExe $powerShellExe `
        -ScriptPath $selectorWorkspaceExportScript `
        -ArgumentList @(
            "-FrontPageWorkspaceRoot",
            $frontPageWorkspaceRootPath,
            "-OutputRoot",
            $selectorWorkspaceRootPath,
            "-PythonExe",
            $resolvedPythonExe,
            "-Clean"
        ) `
        -FailureMessage "front page entry opening-flow consumer selector workspace export failed"

    $selectorSummaryPath = Join-Path $selectorWorkspaceRootPath "selector\front-page.entry-opening-flow.consumer.selector.summary.json"
    if (-not (Test-Path -LiteralPath $selectorSummaryPath)) {
        throw "opening-flow consumer selector summary not found: $selectorSummaryPath"
    }

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $planExportScript,
            "--selector",
            $selectorSummaryPath,
            "--output-root",
            $planRootPath
        ) `
        -FailureMessage "front page entry opening-flow consumer plan export failed"

    $planSummaryPath = Join-Path $planRootPath "front-page.entry-opening-flow.consumer.plan.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($planValidateScript, "--summary", $planSummaryPath) `
        -FailureMessage "front page entry opening-flow consumer plan validation failed"
} finally {
    Pop-Location
}

$planSummaryPath = Join-Path $planRootPath "front-page.entry-opening-flow.consumer.plan.summary.json"
$planReportPath = Join-Path $planRootPath "front-page.entry-opening-flow.consumer.plan.report.md"
$planCheckPath = Join-Path $planRootPath "front-page.entry-opening-flow.consumer.plan.check.txt"

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-WORKSPACE] front_page_workspace_root={0}" -f $frontPageWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-WORKSPACE] selector_workspace_root={0}" -f $selectorWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-WORKSPACE] plan_root={0}" -f $planRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-WORKSPACE] summary={0}" -f $planSummaryPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-WORKSPACE] report={0}" -f $planReportPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-WORKSPACE] check={0}" -f $planCheckPath)
