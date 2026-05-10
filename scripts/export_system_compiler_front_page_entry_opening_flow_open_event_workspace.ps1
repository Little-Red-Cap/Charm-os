param(
    [string]$FrontPageWorkspaceRoot = "",
    [string]$PlanWorkspaceRoot = "",
    [string]$ActionWorkspaceRoot = "",
    [string]$ActionSummaryPath = "",
    [string]$ActionCompareSummaryPath = "",
    [string]$OutputRoot = "out/system-compiler-open-event-ws",
    [string]$OpenEventRoot = "",
    [string]$ActionId = "",
    [string]$ActionKind = "",
    [string]$EntryName = "",
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

function Resolve-ActionSummaryPath {
    param(
        [string]$WorkspaceRoot
    )

    $workspaceSummaryPath = Join-Path $WorkspaceRoot "action\front-page.entry-opening-flow.consumer.plan-action.summary.json"
    if (Test-Path -LiteralPath $workspaceSummaryPath) {
        return $workspaceSummaryPath
    }

    return (Join-Path $WorkspaceRoot "front-page.entry-opening-flow.consumer.plan-action.summary.json")
}

$selectorCount = 0
foreach ($value in @($ActionId, $ActionKind, $EntryName)) {
    if (-not [string]::IsNullOrWhiteSpace($value)) {
        $selectorCount += 1
    }
}
if ($selectorCount -gt 1) {
    throw "use only one of ActionId, ActionKind, or EntryName"
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$frontPageWorkspaceRootPath = Resolve-FullPath -Path $FrontPageWorkspaceRoot
$planWorkspaceRootPath = Resolve-FullPath -Path $PlanWorkspaceRoot
$outputRootPath = Resolve-FullPath -Path $OutputRoot
$actionWorkspaceRootPath = if ([string]::IsNullOrWhiteSpace($ActionWorkspaceRoot)) {
    Join-Path $outputRootPath "action-ws"
} else {
    Resolve-FullPath -Path $ActionWorkspaceRoot
}
$openEventRootPath = if ([string]::IsNullOrWhiteSpace($OpenEventRoot)) {
    Join-Path $outputRootPath "open-event"
} else {
    Resolve-FullPath -Path $OpenEventRoot
}
$actionSummaryPath = Resolve-FullPath -Path $ActionSummaryPath
$actionCompareSummaryPath = Resolve-FullPath -Path $ActionCompareSummaryPath

if ($Clean) {
    $cleanPaths = @($openEventRootPath, $outputRootPath)
    if ([string]::IsNullOrWhiteSpace($ActionSummaryPath) -and [string]::IsNullOrWhiteSpace($ActionWorkspaceRoot)) {
        $cleanPaths += $actionWorkspaceRootPath
    }
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

$actionWorkspaceScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer_plan_action_workspace.ps1"
$openEventScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_open_event.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_open_event.py"
foreach ($requiredPath in @($actionWorkspaceScript, $openEventScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    if ([string]::IsNullOrWhiteSpace($actionSummaryPath)) {
        $actionSummaryPath = Resolve-ActionSummaryPath -WorkspaceRoot $actionWorkspaceRootPath
        $actionAvailable = (-not $Clean) -and (Test-Path -LiteralPath $actionSummaryPath)
        if ($actionAvailable) {
            Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE] action_workspace=reuse-existing"
        } else {
            if ([string]::IsNullOrWhiteSpace($frontPageWorkspaceRootPath) -and [string]::IsNullOrWhiteSpace($planWorkspaceRootPath)) {
                throw "front-page or plan workspace root is required unless ActionSummaryPath or ActionWorkspaceRoot already contains an action summary"
            }

            $arguments = @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $actionWorkspaceScript,
                "-OutputRoot",
                $actionWorkspaceRootPath,
                "-PythonExe",
                $resolvedPythonExe
            )
            if (-not [string]::IsNullOrWhiteSpace($frontPageWorkspaceRootPath)) {
                $arguments += @("-FrontPageWorkspaceRoot", $frontPageWorkspaceRootPath)
            }
            if (-not [string]::IsNullOrWhiteSpace($planWorkspaceRootPath)) {
                $arguments += @("-PlanWorkspaceRoot", $planWorkspaceRootPath)
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
                -FailureMessage "opening-flow consumer plan action workspace export failed"
            $actionSummaryPath = Resolve-ActionSummaryPath -WorkspaceRoot $actionWorkspaceRootPath
        }
    }

    if (-not (Test-Path -LiteralPath $actionSummaryPath)) {
        throw "opening-flow consumer plan action summary not found: $actionSummaryPath"
    }
    if (-not [string]::IsNullOrWhiteSpace($actionCompareSummaryPath) -and -not (Test-Path -LiteralPath $actionCompareSummaryPath)) {
        throw "opening-flow consumer plan action compare summary not found: $actionCompareSummaryPath"
    }

    $openEventArguments = @(
        $openEventScript,
        "--action",
        $actionSummaryPath,
        "--output-root",
        $openEventRootPath
    )
    if (-not [string]::IsNullOrWhiteSpace($actionCompareSummaryPath)) {
        $openEventArguments += @("--action-compare", $actionCompareSummaryPath)
    }

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList $openEventArguments `
        -FailureMessage "opening-flow open-event export failed"

    $openEventSummaryPath = Join-Path $openEventRootPath "front-page.entry-opening-flow.open-event.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateScript, "--summary", $openEventSummaryPath) `
        -FailureMessage "opening-flow open-event validation failed"
} finally {
    Pop-Location
}

$openEventSummaryPath = Join-Path $openEventRootPath "front-page.entry-opening-flow.open-event.summary.json"
$openEventReportPath = Join-Path $openEventRootPath "front-page.entry-opening-flow.open-event.report.md"
$openEventCheckPath = Join-Path $openEventRootPath "front-page.entry-opening-flow.open-event.check.txt"

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE] front_page_workspace_root={0}" -f $frontPageWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE] plan_workspace_root={0}" -f $planWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE] action_workspace_root={0}" -f $actionWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE] action_summary={0}" -f $actionSummaryPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE] action_compare_summary={0}" -f $actionCompareSummaryPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE] open_event_root={0}" -f $openEventRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE] summary={0}" -f $openEventSummaryPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE] report={0}" -f $openEventReportPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE] check={0}" -f $openEventCheckPath)
