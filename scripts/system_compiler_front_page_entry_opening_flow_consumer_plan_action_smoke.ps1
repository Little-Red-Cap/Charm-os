param(
    [string]$PlanWorkspaceRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-workspace-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-action-smoke",
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
$planWorkspaceRootPath = Resolve-FullPath -Path $PlanWorkspaceRoot
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

$planWorkspaceScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer_plan_workspace.ps1"
$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer_plan_action.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_consumer_plan_action.py"
foreach ($requiredPath in @($planWorkspaceScript, $exportScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $planSummaryPath = Join-Path $planWorkspaceRootPath "plan\front-page.entry-opening-flow.consumer.plan.summary.json"
    if (Test-Path -LiteralPath $planSummaryPath) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-SMOKE] bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $planWorkspaceScript,
                "-FrontPageWorkspaceRoot",
                "cmake-build-codex-system-compiler-front-page-smoke",
                "-OutputRoot",
                $planWorkspaceRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "front page entry opening-flow consumer plan workspace bootstrap failed"
    }

    $cases = @(
        [ordered]@{
            Name = "default"
            ExtraArguments = @()
            ExpectedActionId = "open-default"
            ExpectedActionKind = "default"
            ExpectedDisplayGroup = "primary"
        },
        [ordered]@{
            Name = "compare-neighbor"
            ExtraArguments = @("--action-kind", "compare-neighbor")
            ExpectedActionId = "open-compare-neighbor"
            ExpectedActionKind = "compare-neighbor"
            ExpectedDisplayGroup = "compare"
        },
        [ordered]@{
            Name = "next"
            ExtraArguments = @("--action-kind", "next")
            ExpectedActionId = "open-next-1"
            ExpectedActionKind = "next"
            ExpectedDisplayGroup = "fallback"
        },
        [ordered]@{
            Name = "explicit-action-id"
            ExtraArguments = @("--action-id", "open-next-2")
            ExpectedActionId = "open-next-2"
            ExpectedActionKind = "next"
            ExpectedDisplayGroup = "fallback"
        }
    )

    foreach ($case in $cases) {
        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        $arguments = @($exportScript, "--plan", $planSummaryPath, "--output-root", $caseOutputRoot) + @($case.ExtraArguments)
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList $arguments `
            -FailureMessage ("front page entry opening-flow consumer plan action export failed for case '{0}'" -f $case.Name)

        $actionSummaryPath = Join-Path $caseOutputRoot "front-page.entry-opening-flow.consumer.plan-action.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($validateScript, "--summary", $actionSummaryPath) `
            -FailureMessage ("front page entry opening-flow consumer plan action validation failed for case '{0}'" -f $case.Name)

        $actionSummary = Load-JsonObject -Path $actionSummaryPath
        Assert-Condition `
            -Condition ([string]$actionSummary.result -eq "ok") `
            -Message ("case '{0}' expected result ok but got '{1}'" -f $case.Name, $actionSummary.result)
        Assert-Condition `
            -Condition ([string]$actionSummary.open_action.status -eq "ready") `
            -Message ("case '{0}' expected open action ready but got '{1}'" -f $case.Name, $actionSummary.open_action.status)
        Assert-Condition `
            -Condition ([string]$actionSummary.open_action.action_id -eq [string]$case.ExpectedActionId) `
            -Message ("case '{0}' expected action id '{1}' but got '{2}'" -f $case.Name, $case.ExpectedActionId, $actionSummary.open_action.action_id)
        Assert-Condition `
            -Condition ([string]$actionSummary.open_action.action_kind -eq [string]$case.ExpectedActionKind) `
            -Message ("case '{0}' expected action kind '{1}' but got '{2}'" -f $case.Name, $case.ExpectedActionKind, $actionSummary.open_action.action_kind)
        Assert-Condition `
            -Condition ([string]$actionSummary.open_action.display_group -eq [string]$case.ExpectedDisplayGroup) `
            -Message ("case '{0}' expected display group '{1}' but got '{2}'" -f $case.Name, $case.ExpectedDisplayGroup, $actionSummary.open_action.display_group)
        Assert-Condition `
            -Condition ([string]$actionSummary.execution_receipt.consumer_operation -eq "open-opener-summary") `
            -Message ("case '{0}' must open opener summary" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$actionSummary.opener_surface.available) `
            -Message ("case '{0}' opener surface must be available" -f $case.Name)
        Assert-Condition `
            -Condition (-not [string]::IsNullOrWhiteSpace([string]$actionSummary.open_action.opening_reason.kind)) `
            -Message ("case '{0}' open action must expose opening_reason.kind" -f $case.Name)
        Assert-Condition `
            -Condition (-not [string]::IsNullOrWhiteSpace([string]$actionSummary.open_action.projection_headline)) `
            -Message ("case '{0}' open action must expose projection_headline" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$actionSummary.opening_preview.available) `
            -Message ("case '{0}' opening preview must be available" -f $case.Name)
        Assert-Condition `
            -Condition ([string]$actionSummary.opening_preview.opening_reason.kind -eq [string]$actionSummary.open_action.opening_reason.kind) `
            -Message ("case '{0}' opening preview reason must match open action reason" -f $case.Name)

        Write-Host (
            "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-SMOKE] case={0} selector={1} action={2} kind={3} entry={4} query={5}/{6} reason={7}" -f
            $case.Name,
            [string]$actionSummary.selection_request.effective_selector,
            [string]$actionSummary.open_action.action_id,
            [string]$actionSummary.open_action.action_kind,
            [string]$actionSummary.open_action.entry_name,
            [string]$actionSummary.open_action.query_kind,
            [string]$actionSummary.open_action.query_scope,
            [string]$actionSummary.open_action.opening_reason.kind
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-SMOKE] output_root={0}" -f $outputRootPath)
