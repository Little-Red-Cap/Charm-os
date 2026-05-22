param(
    [Parameter(Mandatory = $true)]
    [string]$BaselineFrontPageWorkspaceRoot,
    [Parameter(Mandatory = $true)]
    [string]$CandidateFrontPageWorkspaceRoot,
    [string]$OutputRoot = "out/system-compiler-front-page-entry-opening-flow-consumer-selector-workspace-compare",
    [string]$BaselineSelectorWorkspaceRoot = "",
    [string]$CandidateSelectorWorkspaceRoot = "",
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
$baselineSelectorWorkspaceRootPath = if ([string]::IsNullOrWhiteSpace($BaselineSelectorWorkspaceRoot)) {
    Join-Path $outputRootPath "baseline_selector_workspace"
} else {
    Resolve-FullPath -Path $BaselineSelectorWorkspaceRoot
}
$candidateSelectorWorkspaceRootPath = if ([string]::IsNullOrWhiteSpace($CandidateSelectorWorkspaceRoot)) {
    Join-Path $outputRootPath "candidate_selector_workspace"
} else {
    Resolve-FullPath -Path $CandidateSelectorWorkspaceRoot
}
$compareRootPath = if ([string]::IsNullOrWhiteSpace($CompareRoot)) {
    Join-Path $outputRootPath "compare"
} else {
    Resolve-FullPath -Path $CompareRoot
}

if (-not (Test-Path -LiteralPath $baselineFrontPageWorkspaceRootPath)) {
    throw "baseline front-page workspace root not found: $baselineFrontPageWorkspaceRootPath"
}
if (-not (Test-Path -LiteralPath $candidateFrontPageWorkspaceRootPath)) {
    throw "candidate front-page workspace root not found: $candidateFrontPageWorkspaceRootPath"
}

if ($Clean) {
    $cleanPaths = @($baselineSelectorWorkspaceRootPath, $candidateSelectorWorkspaceRootPath, $compareRootPath, $outputRootPath)
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
$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_flow_consumer_selector.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_consumer_selector_compare.py"
Assert-RequiredPaths -Paths @($selectorWorkspaceExportScript, $compareScript, $validateScript)

Push-Location $repoRoot
try {
    Invoke-PowerShellScript `
        -PowerShellExe $powerShellExe `
        -ScriptPath $selectorWorkspaceExportScript `
        -ArgumentList @(
            "-FrontPageWorkspaceRoot",
            $baselineFrontPageWorkspaceRootPath,
            "-OutputRoot",
            $baselineSelectorWorkspaceRootPath,
            "-PythonExe",
            $resolvedPythonExe,
            "-Clean"
        ) `
        -FailureMessage "baseline opening-flow consumer selector workspace export failed"

    Invoke-PowerShellScript `
        -PowerShellExe $powerShellExe `
        -ScriptPath $selectorWorkspaceExportScript `
        -ArgumentList @(
            "-FrontPageWorkspaceRoot",
            $candidateFrontPageWorkspaceRootPath,
            "-OutputRoot",
            $candidateSelectorWorkspaceRootPath,
            "-PythonExe",
            $resolvedPythonExe,
            "-Clean"
        ) `
        -FailureMessage "candidate opening-flow consumer selector workspace export failed"

    $baselineSelectorSummaryPath = Resolve-OpeningFlowConsumerSelectorSummaryPath -SelectorWorkspaceRoot $baselineSelectorWorkspaceRootPath
    $candidateSelectorSummaryPath = Resolve-OpeningFlowConsumerSelectorSummaryPath -SelectorWorkspaceRoot $candidateSelectorWorkspaceRootPath
    foreach ($summaryPath in @($baselineSelectorSummaryPath, $candidateSelectorSummaryPath)) {
        if (-not (Test-Path -LiteralPath $summaryPath)) {
            throw "opening-flow consumer selector summary not found: $summaryPath"
        }
    }

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $compareScript,
            "--baseline",
            $baselineSelectorSummaryPath,
            "--candidate",
            $candidateSelectorSummaryPath,
            "--output-root",
            $compareRootPath
        ) `
        -FailureMessage "opening-flow consumer selector workspace compare export failed"

    $compareSummaryPath = Join-Path $compareRootPath "front-page.entry-opening-flow.consumer.selector.compare.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateScript, "--summary", $compareSummaryPath) `
        -FailureMessage "opening-flow consumer selector workspace compare validation failed"
} finally {
    Pop-Location
}

$compareSummaryPath = Join-Path $compareRootPath "front-page.entry-opening-flow.consumer.selector.compare.summary.json"
$compareReportPath = Join-Path $compareRootPath "front-page.entry-opening-flow.consumer.selector.compare.report.md"
$compareCheckPath = Join-Path $compareRootPath "front-page.entry-opening-flow.consumer.selector.compare.check.txt"

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-WORKSPACE-COMPARE] baseline_front_page_workspace_root={0}" -f $baselineFrontPageWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-WORKSPACE-COMPARE] candidate_front_page_workspace_root={0}" -f $candidateFrontPageWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-WORKSPACE-COMPARE] baseline_selector_workspace_root={0}" -f $baselineSelectorWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-WORKSPACE-COMPARE] candidate_selector_workspace_root={0}" -f $candidateSelectorWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-WORKSPACE-COMPARE] compare_root={0}" -f $compareRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-WORKSPACE-COMPARE] summary={0}" -f $compareSummaryPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-WORKSPACE-COMPARE] report={0}" -f $compareReportPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-WORKSPACE-COMPARE] check={0}" -f $compareCheckPath)
