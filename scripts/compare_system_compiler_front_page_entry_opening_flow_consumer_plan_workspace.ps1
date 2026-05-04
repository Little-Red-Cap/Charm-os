param(
    [Parameter(Mandatory = $true)]
    [string]$BaselineFrontPageWorkspaceRoot,
    [Parameter(Mandatory = $true)]
    [string]$CandidateFrontPageWorkspaceRoot,
    [string]$OutputRoot = "out/system-compiler-plan-ws-compare",
    [string]$BaselinePlanWorkspaceRoot = "",
    [string]$CandidatePlanWorkspaceRoot = "",
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

if (-not (Test-Path -LiteralPath $baselineFrontPageWorkspaceRootPath)) {
    throw "baseline front-page workspace root not found: $baselineFrontPageWorkspaceRootPath"
}
if (-not (Test-Path -LiteralPath $candidateFrontPageWorkspaceRootPath)) {
    throw "candidate front-page workspace root not found: $candidateFrontPageWorkspaceRootPath"
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

$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}
$powerShellExe = Resolve-ToolPath -Candidates @("powershell.exe", "pwsh.exe", "powershell", "pwsh")
$planWorkspaceExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer_plan_workspace.ps1"
$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_flow_consumer_plan.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_consumer_plan_compare.py"
foreach ($requiredPath in @($planWorkspaceExportScript, $compareScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $baselinePlanSummaryPath = Join-Path $baselinePlanWorkspaceRootPath "plan\front-page.entry-opening-flow.consumer.plan.summary.json"
    $candidatePlanSummaryPath = Join-Path $candidatePlanWorkspaceRootPath "plan\front-page.entry-opening-flow.consumer.plan.summary.json"

    if ((-not $Clean) -and (Test-Path -LiteralPath $baselinePlanSummaryPath)) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-WORKSPACE-COMPARE] baseline_plan_workspace=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $planWorkspaceExportScript,
                "-FrontPageWorkspaceRoot",
                $baselineFrontPageWorkspaceRootPath,
                "-OutputRoot",
                $baselinePlanWorkspaceRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "baseline opening-flow consumer plan workspace export failed"
    }

    if ((-not $Clean) -and (Test-Path -LiteralPath $candidatePlanSummaryPath)) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-WORKSPACE-COMPARE] candidate_plan_workspace=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $planWorkspaceExportScript,
                "-FrontPageWorkspaceRoot",
                $candidateFrontPageWorkspaceRootPath,
                "-OutputRoot",
                $candidatePlanWorkspaceRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "candidate opening-flow consumer plan workspace export failed"
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
