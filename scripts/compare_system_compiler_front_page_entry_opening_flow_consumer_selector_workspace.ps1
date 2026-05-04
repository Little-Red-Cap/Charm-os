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

function Resolve-FullPath {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ""
    }

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

function Ensure-Directory {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Assert-CleanPath {
    param(
        [string]$Path,
        [string]$RootPath
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    $resolvedPath = Resolve-FullPath -Path $Path
    $resolvedRoot = Resolve-FullPath -Path $RootPath
    $comparison = [System.StringComparison]::OrdinalIgnoreCase
    $rootPrefix = $resolvedRoot.TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar
    if ($resolvedPath.Equals($resolvedRoot, $comparison)) {
        throw "refusing to clean repo root: $resolvedPath"
    }
    if (-not $resolvedPath.StartsWith($rootPrefix, $comparison)) {
        throw "refusing to clean outside repo root: $resolvedPath"
    }
}

function Remove-PathIfExists {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    $resolvedPath = Resolve-FullPath -Path $Path
    if (Test-Path -LiteralPath $resolvedPath) {
        Remove-Item -LiteralPath $resolvedPath -Recurse -Force
    }
}

function Resolve-ToolPath {
    param(
        [string[]]$Candidates
    )

    foreach ($candidate in $Candidates) {
        $command = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($null -ne $command) {
            return $command.Source
        }
    }

    throw "tool not found: $($Candidates -join ', ')"
}

function Invoke-ExternalTool {
    param(
        [string]$Executable,
        [string[]]$ArgumentList,
        [string]$FailureMessage
    )

    Write-Host ("==> {0}" -f [System.IO.Path]::GetFileName($Executable))

    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & $Executable @ArgumentList
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    if ($exitCode -ne 0) {
        throw ("{0} (exit code {1})" -f $FailureMessage, $exitCode)
    }
}

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

$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}
$powerShellExe = Resolve-ToolPath -Candidates @("powershell.exe", "pwsh.exe", "powershell", "pwsh")
$selectorWorkspaceExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer_selector_workspace.ps1"
$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_flow_consumer_selector.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_consumer_selector_compare.py"
foreach ($requiredPath in @($selectorWorkspaceExportScript, $compareScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    Invoke-ExternalTool `
        -Executable $powerShellExe `
        -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $selectorWorkspaceExportScript,
            "-FrontPageWorkspaceRoot",
            $baselineFrontPageWorkspaceRootPath,
            "-OutputRoot",
            $baselineSelectorWorkspaceRootPath,
            "-PythonExe",
            $resolvedPythonExe,
            "-Clean"
        ) `
        -FailureMessage "baseline opening-flow consumer selector workspace export failed"

    Invoke-ExternalTool `
        -Executable $powerShellExe `
        -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $selectorWorkspaceExportScript,
            "-FrontPageWorkspaceRoot",
            $candidateFrontPageWorkspaceRootPath,
            "-OutputRoot",
            $candidateSelectorWorkspaceRootPath,
            "-PythonExe",
            $resolvedPythonExe,
            "-Clean"
        ) `
        -FailureMessage "candidate opening-flow consumer selector workspace export failed"

    $baselineSelectorSummaryPath = Join-Path $baselineSelectorWorkspaceRootPath "selector\front-page.entry-opening-flow.consumer.selector.summary.json"
    $candidateSelectorSummaryPath = Join-Path $candidateSelectorWorkspaceRootPath "selector\front-page.entry-opening-flow.consumer.selector.summary.json"
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
