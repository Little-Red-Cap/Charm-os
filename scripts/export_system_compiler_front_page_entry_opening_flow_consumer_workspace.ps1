param(
    [Parameter(Mandatory = $true)]
    [string]$FrontPageWorkspaceRoot,
    [string]$OutputRoot = "out/system-compiler-front-page-entry-opening-flow-consumer-workspace",
    [string]$FlowRoot = "",
    [string]$ConsumerRoot = "",
    [string]$PythonExe = "",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "front_page_entry_opening_flow_harness.ps1")

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$frontPageWorkspaceRootPath = Resolve-FullPath -Path $FrontPageWorkspaceRoot
$outputRootPath = Resolve-FullPath -Path $OutputRoot
$flowRootPath = if ([string]::IsNullOrWhiteSpace($FlowRoot)) {
    Join-Path $outputRootPath "opening_flow"
} else {
    Resolve-FullPath -Path $FlowRoot
}
$consumerRootPath = if ([string]::IsNullOrWhiteSpace($ConsumerRoot)) {
    Join-Path $outputRootPath "consumer"
} else {
    Resolve-FullPath -Path $ConsumerRoot
}

if ($Clean) {
    $cleanPaths = @($flowRootPath, $consumerRootPath, $outputRootPath)
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
Ensure-FrontPageSmokeFixtureWorkspace `
    -ScriptsRoot $PSScriptRoot `
    -FrontPageWorkspaceRootPath $frontPageWorkspaceRootPath `
    -PowerShellExe $powerShellExe

$workspaceExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_workspace.ps1"
$consumerExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer.py"
$consumerValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_consumer.py"
Assert-RequiredPaths -Paths @($workspaceExportScript, $consumerExportScript, $consumerValidateScript)

Push-Location $repoRoot
try {
    Invoke-PowerShellScript `
        -PowerShellExe $powerShellExe `
        -ScriptPath $workspaceExportScript `
        -ArgumentList @(
            "-FrontPageWorkspaceRoot",
            $frontPageWorkspaceRootPath,
            "-OutputRoot",
            $flowRootPath,
            "-PythonExe",
            $resolvedPythonExe,
            "-Clean"
        ) `
        -FailureMessage "front page entry opening-flow workspace export failed"

    $flowSummaryPath = Resolve-OpeningFlowSummaryPath -WorkspaceRoot $flowRootPath
    if (-not (Test-Path $flowSummaryPath)) {
        throw "opening-flow summary not found: $flowSummaryPath"
    }

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $consumerExportScript,
            "--flow",
            $flowSummaryPath,
            "--output-root",
            $consumerRootPath
        ) `
        -FailureMessage "front page entry opening-flow consumer export failed"

    $consumerSummaryPath = Join-Path $consumerRootPath "front-page.entry-opening-flow.consumer.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($consumerValidateScript, "--summary", $consumerSummaryPath) `
        -FailureMessage "front page entry opening-flow consumer validation failed"
} finally {
    Pop-Location
}

$consumerSummaryPath = Join-Path $consumerRootPath "front-page.entry-opening-flow.consumer.summary.json"
$consumerReportPath = Join-Path $consumerRootPath "front-page.entry-opening-flow.consumer.report.md"
$consumerCheckPath = Join-Path $consumerRootPath "front-page.entry-opening-flow.consumer.check.txt"

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-WORKSPACE] front_page_workspace_root={0}" -f $frontPageWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-WORKSPACE] flow_root={0}" -f $flowRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-WORKSPACE] consumer_root={0}" -f $consumerRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-WORKSPACE] summary={0}" -f $consumerSummaryPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-WORKSPACE] report={0}" -f $consumerReportPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-WORKSPACE] check={0}" -f $consumerCheckPath)
