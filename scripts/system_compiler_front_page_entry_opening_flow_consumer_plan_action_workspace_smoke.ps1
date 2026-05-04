param(
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-action-workspace-smoke",
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

function Remove-PathIfExists {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    if (Test-Path -LiteralPath $Path) {
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

function Load-JsonObject {
    param(
        [string]$Path
    )

    return (Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json)
}

function Assert-Condition {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$outputRootPath = Resolve-FullPath -Path $OutputRoot

if ($Clean) {
    Remove-PathIfExists -Path $outputRootPath
}
Ensure-Directory -Path $outputRootPath

$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}
$powerShellExe = Resolve-ToolPath -Candidates @("powershell.exe", "pwsh.exe", "powershell", "pwsh")

$workspaceScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer_plan_action_workspace.ps1"
foreach ($requiredPath in @($workspaceScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $coldRoot = Join-Path $outputRootPath "cold-default"
    Invoke-ExternalTool `
        -Executable $powerShellExe `
        -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $workspaceScript,
            "-FrontPageWorkspaceRoot",
            "cmake-build-codex-system-compiler-front-page-smoke",
            "-OutputRoot",
            $coldRoot,
            "-PythonExe",
            $resolvedPythonExe,
            "-Clean"
        ) `
        -FailureMessage "front page entry opening-flow consumer plan action cold workspace export failed"

    $coldSummaryPath = Join-Path $coldRoot "action\front-page.entry-opening-flow.consumer.plan-action.summary.json"
    $coldSummary = Load-JsonObject -Path $coldSummaryPath
    Assert-Condition `
        -Condition ([string]$coldSummary.open_action.action_id -eq "open-default") `
        -Message ("cold default expected open-default but got '{0}'" -f $coldSummary.open_action.action_id)
    Assert-Condition `
        -Condition ([string]$coldSummary.selection_request.effective_selector -eq "default_action") `
        -Message ("cold default expected default selector but got '{0}'" -f $coldSummary.selection_request.effective_selector)

    $hotRoot = Join-Path $outputRootPath "hot-compare-neighbor"
    $coldPlanWorkspaceRoot = Join-Path $coldRoot "plan-ws"
    Invoke-ExternalTool `
        -Executable $powerShellExe `
        -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $workspaceScript,
            "-PlanWorkspaceRoot",
            $coldPlanWorkspaceRoot,
            "-OutputRoot",
            $hotRoot,
            "-ActionKind",
            "compare-neighbor",
            "-PythonExe",
            $resolvedPythonExe
        ) `
        -FailureMessage "front page entry opening-flow consumer plan action hot workspace export failed"

    $hotSummaryPath = Join-Path $hotRoot "action\front-page.entry-opening-flow.consumer.plan-action.summary.json"
    $hotSummary = Load-JsonObject -Path $hotSummaryPath
    Assert-Condition `
        -Condition ([string]$hotSummary.open_action.action_id -eq "open-compare-neighbor") `
        -Message ("hot compare expected open-compare-neighbor but got '{0}'" -f $hotSummary.open_action.action_id)
    Assert-Condition `
        -Condition ([string]$hotSummary.selection_request.effective_selector -eq "action_kind:compare-neighbor") `
        -Message ("hot compare expected action kind selector but got '{0}'" -f $hotSummary.selection_request.effective_selector)
    Assert-Condition `
        -Condition ([string]$hotSummary.open_action.display_group -eq "compare") `
        -Message ("hot compare expected compare display group but got '{0}'" -f $hotSummary.open_action.display_group)

    Write-Host (
        "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-SMOKE] case=cold-default selector={0} action={1} entry={2}" -f
        [string]$coldSummary.selection_request.effective_selector,
        [string]$coldSummary.open_action.action_id,
        [string]$coldSummary.open_action.entry_name
    )
    Write-Host (
        "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-SMOKE] case=hot-compare-neighbor selector={0} action={1} entry={2}" -f
        [string]$hotSummary.selection_request.effective_selector,
        [string]$hotSummary.open_action.action_id,
        [string]$hotSummary.open_action.entry_name
    )
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-SMOKE] output_root={0}" -f $outputRootPath)
