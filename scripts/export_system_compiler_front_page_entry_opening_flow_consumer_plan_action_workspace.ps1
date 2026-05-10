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

$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}
$powerShellExe = Resolve-ToolPath -Candidates @("powershell.exe", "pwsh.exe", "powershell", "pwsh")

$planWorkspaceScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer_plan_workspace.ps1"
$actionExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer_plan_action.py"
$actionValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_consumer_plan_action.py"
foreach ($requiredPath in @($planWorkspaceScript, $actionExportScript, $actionValidateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

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
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $planWorkspaceScript,
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
