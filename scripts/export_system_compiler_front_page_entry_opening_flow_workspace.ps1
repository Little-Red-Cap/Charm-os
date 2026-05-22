param(
    [Parameter(Mandatory = $true)]
    [string]$FrontPageWorkspaceRoot,
    [string]$RouteRoot = "",
    [string]$OutputRoot = "out/system-compiler-front-page-entry-opening-flow-workspace",
    [string]$PythonExe = "",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "front_page_entry_opening_flow_harness.ps1")

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$frontPageWorkspaceRootPath = Resolve-FullPath -Path $FrontPageWorkspaceRoot
$outputRootPath = Resolve-FullPath -Path $OutputRoot
$routeRootPath = if ([string]::IsNullOrWhiteSpace($RouteRoot)) {
    Join-Path $outputRootPath "front_page_route"
} else {
    Resolve-FullPath -Path $RouteRoot
}
$capabilityRootPath = Join-Path $outputRootPath "entry_capability"
$landingRootPath = Join-Path $outputRootPath "entry_landing"
$landingCompareRootPath = Join-Path $outputRootPath "entry_landing_compare"
$openerRootPath = Join-Path $outputRootPath "entry_opener"
$summaryPath = Join-Path $outputRootPath "front-page.entry-opening-flow.summary.json"
$reportMarkdownPath = Join-Path $outputRootPath "front-page.entry-opening-flow.report.md"
$checkTextPath = Join-Path $outputRootPath "front-page.entry-opening-flow.check.txt"

Ensure-Directory -Path $outputRootPath

$resolvedPythonExe = Resolve-PythonExe -PythonExe $PythonExe
$powerShellExe = Resolve-PowerShellExe
Ensure-FrontPageSmokeFixtureWorkspace `
    -ScriptsRoot $PSScriptRoot `
    -FrontPageWorkspaceRootPath $frontPageWorkspaceRootPath `
    -PowerShellExe $powerShellExe

$routeSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_route_smoke.ps1"
$smokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_flow_smoke.ps1"
Assert-RequiredPaths -Paths @($routeSmokeScript, $smokeScript)

Push-Location $repoRoot
try {
    $routeArgumentList = [System.Collections.Generic.List[string]]::new()
    $routeArgumentList.Add("-InputRoot") | Out-Null
    $routeArgumentList.Add($frontPageWorkspaceRootPath) | Out-Null
    $routeArgumentList.Add("-OutputRoot") | Out-Null
    $routeArgumentList.Add($routeRootPath) | Out-Null
    $routeArgumentList.Add("-PythonExe") | Out-Null
    $routeArgumentList.Add($resolvedPythonExe) | Out-Null
    if ($Clean) {
        $routeArgumentList.Add("-Clean") | Out-Null
    }

    Invoke-PowerShellScript `
        -PowerShellExe $powerShellExe `
        -ScriptPath $routeSmokeScript `
        -ArgumentList $routeArgumentList.ToArray() `
        -FailureMessage "front page route workspace export failed"

    $argumentList = [System.Collections.Generic.List[string]]::new()
    $argumentList.Add("-RouteRoot") | Out-Null
    $argumentList.Add($routeRootPath) | Out-Null
    $argumentList.Add("-CapabilityRoot") | Out-Null
    $argumentList.Add($capabilityRootPath) | Out-Null
    $argumentList.Add("-LandingRoot") | Out-Null
    $argumentList.Add($landingRootPath) | Out-Null
    $argumentList.Add("-LandingCompareRoot") | Out-Null
    $argumentList.Add($landingCompareRootPath) | Out-Null
    $argumentList.Add("-OpenerRoot") | Out-Null
    $argumentList.Add($openerRootPath) | Out-Null
    $argumentList.Add("-OutputRoot") | Out-Null
    $argumentList.Add($outputRootPath) | Out-Null
    $argumentList.Add("-PythonExe") | Out-Null
    $argumentList.Add($resolvedPythonExe) | Out-Null
    if ($Clean) {
        $argumentList.Add("-Clean") | Out-Null
    }

    Invoke-PowerShellScript `
        -PowerShellExe $powerShellExe `
        -ScriptPath $smokeScript `
        -ArgumentList $argumentList.ToArray() `
        -FailureMessage "front page entry opening flow workspace export failed"
} finally {
    Pop-Location
}

if (-not (Test-Path $summaryPath)) {
    throw "front page entry opening flow summary not found: $summaryPath"
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-WORKSPACE] front_page_workspace_root={0}" -f $frontPageWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-WORKSPACE] route_root={0}" -f $routeRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-WORKSPACE] output_root={0}" -f $outputRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-WORKSPACE] summary={0}" -f $summaryPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-WORKSPACE] report={0}" -f $reportMarkdownPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-WORKSPACE] check={0}" -f $checkTextPath)
