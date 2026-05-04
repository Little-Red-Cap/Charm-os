param(
    [string]$BaselineFrontPageWorkspaceRoot = "",
    [string]$CandidateFrontPageWorkspaceRoot = "",
    [string]$OutputRoot = "out/system-compiler-plan-action-ws-compare",
    [string]$BaselinePlanWorkspaceRoot = "",
    [string]$CandidatePlanWorkspaceRoot = "",
    [string]$BaselineActionWorkspaceRoot = "",
    [string]$CandidateActionWorkspaceRoot = "",
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

function Resolve-ActionSummaryPath {
    param(
        [string]$ActionWorkspaceRoot
    )

    $workspaceSummaryPath = Join-Path $ActionWorkspaceRoot "action\front-page.entry-opening-flow.consumer.plan-action.summary.json"
    if (Test-Path -LiteralPath $workspaceSummaryPath) {
        return $workspaceSummaryPath
    }

    return (Join-Path $ActionWorkspaceRoot "front-page.entry-opening-flow.consumer.plan-action.summary.json")
}

function Invoke-ActionWorkspaceExport {
    param(
        [string]$Role,
        [string]$ActionWorkspaceRoot,
        [string]$FrontPageWorkspaceRoot,
        [string]$PlanWorkspaceRoot,
        [string]$ActionId,
        [string]$ActionKind,
        [string]$EntryName
    )

    $arguments = @(
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        $actionWorkspaceExportScript,
        "-OutputRoot",
        $ActionWorkspaceRoot,
        "-PythonExe",
        $resolvedPythonExe
    )

    if (-not [string]::IsNullOrWhiteSpace($FrontPageWorkspaceRoot)) {
        if (-not (Test-Path -LiteralPath $FrontPageWorkspaceRoot)) {
            throw "$Role front-page workspace root not found: $FrontPageWorkspaceRoot"
        }
        $arguments += @("-FrontPageWorkspaceRoot", $FrontPageWorkspaceRoot)
    }
    if (-not [string]::IsNullOrWhiteSpace($PlanWorkspaceRoot)) {
        $arguments += @("-PlanWorkspaceRoot", $PlanWorkspaceRoot)
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

    Invoke-ExternalTool `
        -Executable $powerShellExe `
        -ArgumentList $arguments `
        -FailureMessage ("{0} opening-flow consumer plan action workspace export failed" -f $Role)
}

Assert-SingleSelector -Label "baseline" -ActionId $BaselineActionId -ActionKind $BaselineActionKind -EntryName $BaselineEntryName
Assert-SingleSelector -Label "candidate" -ActionId $CandidateActionId -ActionKind $CandidateActionKind -EntryName $CandidateEntryName

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$baselineFrontPageWorkspaceRootPath = Resolve-FullPath -Path $BaselineFrontPageWorkspaceRoot
$candidateFrontPageWorkspaceRootPath = Resolve-FullPath -Path $CandidateFrontPageWorkspaceRoot
$outputRootPath = Resolve-FullPath -Path $OutputRoot
$baselinePlanWorkspaceRootPath = Resolve-FullPath -Path $BaselinePlanWorkspaceRoot
$candidatePlanWorkspaceRootPath = Resolve-FullPath -Path $CandidatePlanWorkspaceRoot
$baselineActionWorkspaceRootPath = if ([string]::IsNullOrWhiteSpace($BaselineActionWorkspaceRoot)) {
    Join-Path $outputRootPath "ba"
} else {
    Resolve-FullPath -Path $BaselineActionWorkspaceRoot
}
$candidateActionWorkspaceRootPath = if ([string]::IsNullOrWhiteSpace($CandidateActionWorkspaceRoot)) {
    Join-Path $outputRootPath "ca"
} else {
    Resolve-FullPath -Path $CandidateActionWorkspaceRoot
}
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

$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}
$powerShellExe = Resolve-ToolPath -Candidates @("powershell.exe", "pwsh.exe", "powershell", "pwsh")
$actionWorkspaceExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer_plan_action_workspace.ps1"
$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_flow_consumer_plan_action.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_consumer_plan_action_compare.py"
foreach ($requiredPath in @($actionWorkspaceExportScript, $compareScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $baselineActionSummaryPath = Resolve-ActionSummaryPath -ActionWorkspaceRoot $baselineActionWorkspaceRootPath
    $candidateActionSummaryPath = Resolve-ActionSummaryPath -ActionWorkspaceRoot $candidateActionWorkspaceRootPath
    $baselineActionAvailable = (-not $Clean) -and (Test-Path -LiteralPath $baselineActionSummaryPath)
    $candidateActionAvailable = (-not $Clean) -and (Test-Path -LiteralPath $candidateActionSummaryPath)

    if ($baselineActionAvailable) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE] baseline_action_workspace=reuse-existing"
    } else {
        if ([string]::IsNullOrWhiteSpace($baselineFrontPageWorkspaceRootPath) -and [string]::IsNullOrWhiteSpace($baselinePlanWorkspaceRootPath)) {
            throw "baseline front-page or plan workspace root is required unless BaselineActionWorkspaceRoot already contains an action summary"
        }
        Invoke-ActionWorkspaceExport `
            -Role "baseline" `
            -ActionWorkspaceRoot $baselineActionWorkspaceRootPath `
            -FrontPageWorkspaceRoot $baselineFrontPageWorkspaceRootPath `
            -PlanWorkspaceRoot $baselinePlanWorkspaceRootPath `
            -ActionId $BaselineActionId `
            -ActionKind $BaselineActionKind `
            -EntryName $BaselineEntryName
        $baselineActionSummaryPath = Resolve-ActionSummaryPath -ActionWorkspaceRoot $baselineActionWorkspaceRootPath
    }

    if ($candidateActionAvailable) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE] candidate_action_workspace=reuse-existing"
    } else {
        if ([string]::IsNullOrWhiteSpace($candidateFrontPageWorkspaceRootPath) -and [string]::IsNullOrWhiteSpace($candidatePlanWorkspaceRootPath)) {
            throw "candidate front-page or plan workspace root is required unless CandidateActionWorkspaceRoot already contains an action summary"
        }
        Invoke-ActionWorkspaceExport `
            -Role "candidate" `
            -ActionWorkspaceRoot $candidateActionWorkspaceRootPath `
            -FrontPageWorkspaceRoot $candidateFrontPageWorkspaceRootPath `
            -PlanWorkspaceRoot $candidatePlanWorkspaceRootPath `
            -ActionId $CandidateActionId `
            -ActionKind $CandidateActionKind `
            -EntryName $CandidateEntryName
        $candidateActionSummaryPath = Resolve-ActionSummaryPath -ActionWorkspaceRoot $candidateActionWorkspaceRootPath
    }

    foreach ($summaryPath in @($baselineActionSummaryPath, $candidateActionSummaryPath)) {
        if (-not (Test-Path -LiteralPath $summaryPath)) {
            throw "opening-flow consumer plan action summary not found: $summaryPath"
        }
    }

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $compareScript,
            "--baseline",
            $baselineActionSummaryPath,
            "--candidate",
            $candidateActionSummaryPath,
            "--output-root",
            $compareRootPath
        ) `
        -FailureMessage "opening-flow consumer plan action workspace compare export failed"

    $compareSummaryPath = Join-Path $compareRootPath "front-page.entry-opening-flow.consumer.plan-action.compare.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateScript, "--summary", $compareSummaryPath) `
        -FailureMessage "opening-flow consumer plan action workspace compare validation failed"
} finally {
    Pop-Location
}

$compareSummaryPath = Join-Path $compareRootPath "front-page.entry-opening-flow.consumer.plan-action.compare.summary.json"
$compareReportPath = Join-Path $compareRootPath "front-page.entry-opening-flow.consumer.plan-action.compare.report.md"
$compareCheckPath = Join-Path $compareRootPath "front-page.entry-opening-flow.consumer.plan-action.compare.check.txt"

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE] baseline_front_page_workspace_root={0}" -f $baselineFrontPageWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE] candidate_front_page_workspace_root={0}" -f $candidateFrontPageWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE] baseline_plan_workspace_root={0}" -f $baselinePlanWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE] candidate_plan_workspace_root={0}" -f $candidatePlanWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE] baseline_action_workspace_root={0}" -f $baselineActionWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE] candidate_action_workspace_root={0}" -f $candidateActionWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE] compare_root={0}" -f $compareRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE] summary={0}" -f $compareSummaryPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE] report={0}" -f $compareReportPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE] check={0}" -f $compareCheckPath)
