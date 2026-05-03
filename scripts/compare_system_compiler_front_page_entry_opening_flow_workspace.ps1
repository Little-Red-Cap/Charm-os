param(
    [Parameter(Mandatory = $true)]
    [string]$BaselineFrontPageWorkspaceRoot,
    [Parameter(Mandatory = $true)]
    [string]$CandidateFrontPageWorkspaceRoot,
    [string]$OutputRoot = "out/system-compiler-front-page-entry-opening-flow-workspace-compare",
    [string]$BaselineFlowRoot = "",
    [string]$CandidateFlowRoot = "",
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

    if (-not (Test-Path $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Remove-PathIfExists {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    if (Test-Path $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
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
$baselineWorkspaceRootPath = Resolve-FullPath -Path $BaselineFrontPageWorkspaceRoot
$candidateWorkspaceRootPath = Resolve-FullPath -Path $CandidateFrontPageWorkspaceRoot
$outputRootPath = Resolve-FullPath -Path $OutputRoot
$baselineFlowRootPath = if ([string]::IsNullOrWhiteSpace($BaselineFlowRoot)) {
    Join-Path $outputRootPath "baseline_opening_flow"
} else {
    Resolve-FullPath -Path $BaselineFlowRoot
}
$candidateFlowRootPath = if ([string]::IsNullOrWhiteSpace($CandidateFlowRoot)) {
    Join-Path $outputRootPath "candidate_opening_flow"
} else {
    Resolve-FullPath -Path $CandidateFlowRoot
}
$compareRootPath = if ([string]::IsNullOrWhiteSpace($CompareRoot)) {
    Join-Path $outputRootPath "compare"
} else {
    Resolve-FullPath -Path $CompareRoot
}

if (-not (Test-Path $baselineWorkspaceRootPath)) {
    throw "baseline front-page workspace root not found: $baselineWorkspaceRootPath"
}
if (-not (Test-Path $candidateWorkspaceRootPath)) {
    throw "candidate front-page workspace root not found: $candidateWorkspaceRootPath"
}

if ($Clean) {
    foreach ($path in @($baselineFlowRootPath, $candidateFlowRootPath, $compareRootPath, $outputRootPath)) {
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
$workspaceExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_workspace.ps1"
$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_flow.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_compare.py"
foreach ($requiredPath in @($workspaceExportScript, $compareScript, $validateScript)) {
    if (-not (Test-Path $requiredPath)) {
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
            $workspaceExportScript,
            "-FrontPageWorkspaceRoot",
            $baselineWorkspaceRootPath,
            "-OutputRoot",
            $baselineFlowRootPath,
            "-PythonExe",
            $resolvedPythonExe,
            "-Clean"
        ) `
        -FailureMessage "baseline opening-flow workspace export failed"

    Invoke-ExternalTool `
        -Executable $powerShellExe `
        -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $workspaceExportScript,
            "-FrontPageWorkspaceRoot",
            $candidateWorkspaceRootPath,
            "-OutputRoot",
            $candidateFlowRootPath,
            "-PythonExe",
            $resolvedPythonExe,
            "-Clean"
        ) `
        -FailureMessage "candidate opening-flow workspace export failed"

    $baselineSummaryPath = Join-Path $baselineFlowRootPath "front-page.entry-opening-flow.summary.json"
    $candidateSummaryPath = Join-Path $candidateFlowRootPath "front-page.entry-opening-flow.summary.json"
    foreach ($summaryPath in @($baselineSummaryPath, $candidateSummaryPath)) {
        if (-not (Test-Path $summaryPath)) {
            throw "opening-flow summary not found: $summaryPath"
        }
    }

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $compareScript,
            "--baseline",
            $baselineSummaryPath,
            "--candidate",
            $candidateSummaryPath,
            "--output-root",
            $compareRootPath
        ) `
        -FailureMessage "opening-flow workspace compare export failed"

    $compareSummaryPath = Join-Path $compareRootPath "front-page.entry-opening-flow.compare.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateScript, "--summary", $compareSummaryPath) `
        -FailureMessage "opening-flow workspace compare validation failed"
} finally {
    Pop-Location
}

$compareSummaryPath = Join-Path $compareRootPath "front-page.entry-opening-flow.compare.summary.json"
$compareReportPath = Join-Path $compareRootPath "front-page.entry-opening-flow.compare.report.md"
$compareCheckPath = Join-Path $compareRootPath "front-page.entry-opening-flow.compare.check.txt"

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-WORKSPACE-COMPARE] baseline_workspace_root={0}" -f $baselineWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-WORKSPACE-COMPARE] candidate_workspace_root={0}" -f $candidateWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-WORKSPACE-COMPARE] baseline_flow_root={0}" -f $baselineFlowRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-WORKSPACE-COMPARE] candidate_flow_root={0}" -f $candidateFlowRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-WORKSPACE-COMPARE] compare_root={0}" -f $compareRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-WORKSPACE-COMPARE] summary={0}" -f $compareSummaryPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-WORKSPACE-COMPARE] report={0}" -f $compareReportPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-WORKSPACE-COMPARE] check={0}" -f $compareCheckPath)
