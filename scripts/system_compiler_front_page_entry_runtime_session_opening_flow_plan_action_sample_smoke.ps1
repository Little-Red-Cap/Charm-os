param(
    [string]$SampleSummary = "schemas/examples/system_compiler.witness_bundle.v0.sample.json",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-flow-plan-action-sample-smoke",
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

    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path $Path)) {
        return
    }

    Remove-Item -LiteralPath $Path -Recurse -Force
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
$sampleSummaryPath = Resolve-FullPath -Path $SampleSummary
$outputRootPath = Resolve-FullPath -Path $OutputRoot

if (-not (Test-Path $sampleSummaryPath)) {
    throw "sample summary not found: $sampleSummaryPath"
}

if ($Clean) {
    Remove-PathIfExists -Path $outputRootPath
}
Ensure-Directory -Path $outputRootPath

$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}

$selectorSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_opening_flow_consumer_selector_sample_smoke.ps1"
$planExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer_plan.py"
$planValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_consumer_plan.py"
$actionExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer_plan_action.py"
$actionValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_consumer_plan_action.py"
foreach ($requiredPath in @(
    $selectorSmokeScript,
    $planExportScript,
    $planValidateScript,
    $actionExportScript,
    $actionValidateScript
)) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing script: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $selectorChainRoot = Join-Path $outputRootPath "selector-chain"
    $planRoot = Join-Path $outputRootPath "plan"
    $actionRoot = Join-Path $outputRootPath "action"
    $selectorSummaryPath = Join-Path $selectorChainRoot "selector\front-page.entry-opening-flow.consumer.selector.summary.json"
    $planSummaryPath = Join-Path $planRoot "front-page.entry-opening-flow.consumer.plan.summary.json"
    $actionSummaryPath = Join-Path $actionRoot "front-page.entry-opening-flow.consumer.plan-action.summary.json"

    $selectorArgs = @(
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        $selectorSmokeScript,
        "-SampleSummary",
        $sampleSummaryPath,
        "-OutputRoot",
        $selectorChainRoot,
        "-PythonExe",
        $resolvedPythonExe
    )
    if ($Clean) {
        $selectorArgs += "-Clean"
    }

    Invoke-ExternalTool `
        -Executable "powershell.exe" `
        -ArgumentList $selectorArgs `
        -FailureMessage "runtime session opening flow consumer selector sample smoke failed"

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $planExportScript,
            "--selector",
            $selectorSummaryPath,
            "--output-root",
            $planRoot
        ) `
        -FailureMessage "runtime session opening flow consumer plan sample export failed"

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($planValidateScript, "--summary", $planSummaryPath) `
        -FailureMessage "runtime session opening flow consumer plan sample validation failed"

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $actionExportScript,
            "--plan",
            $planSummaryPath,
            "--output-root",
            $actionRoot
        ) `
        -FailureMessage "runtime session opening flow consumer plan action sample export failed"

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($actionValidateScript, "--summary", $actionSummaryPath) `
        -FailureMessage "runtime session opening flow consumer plan action sample validation failed"

    $planSummary = Load-JsonObject -Path $planSummaryPath
    Assert-Condition `
        -Condition ([int]$planSummary.planner_status.planned_action_count -eq 1) `
        -Message ("expected one planned action but got {0}" -f $planSummary.planner_status.planned_action_count)
    Assert-Condition `
        -Condition ([string]$planSummary.planner_status.default_action_name -eq "runtime-session-sample") `
        -Message ("expected plan default runtime-session-sample but got '{0}'" -f $planSummary.planner_status.default_action_name)
    Assert-Condition `
        -Condition ([string]$planSummary.execution_plan.default_action.action_id -eq "open-default") `
        -Message ("expected plan default action open-default but got '{0}'" -f $planSummary.execution_plan.default_action.action_id)
    Assert-Condition `
        -Condition ([string]$planSummary.execution_plan.default_action.selected_tab_id -eq "runtime_session") `
        -Message ("expected plan selected_tab_id runtime_session but got '{0}'" -f $planSummary.execution_plan.default_action.selected_tab_id)
    Assert-Condition `
        -Condition ([string]$planSummary.execution_plan.default_action.projection_kind -eq "kernel_runtime_session_overview") `
        -Message ("expected plan projection kernel_runtime_session_overview but got '{0}'" -f $planSummary.execution_plan.default_action.projection_kind)
    Assert-Condition `
        -Condition (@($planSummary.execution_plan.default_action.projection_summary_lines).Count -gt 0) `
        -Message "expected plan default action to carry runtime-session projection summary lines"
    Assert-Condition `
        -Condition (@($planSummary.execution_plan.default_action.projection_question_lines).Count -gt 0) `
        -Message "expected plan default action to carry runtime-session projection question lines"

    $actionSummary = Load-JsonObject -Path $actionSummaryPath
    Assert-Condition `
        -Condition ([string]$actionSummary.selection_request.effective_selector -eq "default_action") `
        -Message ("expected action effective_selector default_action but got '{0}'" -f $actionSummary.selection_request.effective_selector)
    Assert-Condition `
        -Condition ([string]$actionSummary.open_action.action_id -eq "open-default") `
        -Message ("expected action open-default but got '{0}'" -f $actionSummary.open_action.action_id)
    Assert-Condition `
        -Condition ([string]$actionSummary.open_action.entry_name -eq "runtime-session-sample") `
        -Message ("expected action entry runtime-session-sample but got '{0}'" -f $actionSummary.open_action.entry_name)
    Assert-Condition `
        -Condition ([string]$actionSummary.open_action.selected_tab_id -eq "runtime_session") `
        -Message ("expected action selected_tab_id runtime_session but got '{0}'" -f $actionSummary.open_action.selected_tab_id)
    Assert-Condition `
        -Condition ([string]$actionSummary.open_action.projection_kind -eq "kernel_runtime_session_overview") `
        -Message ("expected action projection kernel_runtime_session_overview but got '{0}'" -f $actionSummary.open_action.projection_kind)
    Assert-Condition `
        -Condition ([string]$actionSummary.open_action.expected_consumer_operation -eq "open-opener-summary") `
        -Message ("expected action operation open-opener-summary but got '{0}'" -f $actionSummary.open_action.expected_consumer_operation)
    Assert-Condition `
        -Condition (@($actionSummary.open_action.projection_summary_lines).Count -gt 0) `
        -Message "expected action to carry runtime-session projection summary lines"
    Assert-Condition `
        -Condition (@($actionSummary.open_action.projection_question_lines).Count -gt 0) `
        -Message "expected action to carry runtime-session projection question lines"
    Assert-Condition `
        -Condition (@($actionSummary.opening_preview.summary_lines).Count -eq @($actionSummary.open_action.projection_summary_lines).Count) `
        -Message "expected action opening preview summary lines to mirror open action projection summary lines"
    Assert-Condition `
        -Condition (@($actionSummary.opening_preview.question_lines).Count -eq @($actionSummary.open_action.projection_question_lines).Count) `
        -Message "expected action opening preview question lines to mirror open action projection question lines"

    Write-Host ("[FRONT-PAGE-ENTRY-RUNTIME-SESSION-PLAN-ACTION-SAMPLE-SMOKE] selector={0}" -f $selectorSummaryPath)
    Write-Host ("[FRONT-PAGE-ENTRY-RUNTIME-SESSION-PLAN-ACTION-SAMPLE-SMOKE] plan={0}" -f $planSummaryPath)
    Write-Host ("[FRONT-PAGE-ENTRY-RUNTIME-SESSION-PLAN-ACTION-SAMPLE-SMOKE] action={0}" -f $actionSummaryPath)
    Write-Host (
        "[FRONT-PAGE-ENTRY-RUNTIME-SESSION-PLAN-ACTION-SAMPLE-SMOKE] action_id={0} entry={1} projection={2} projection_summary={3} projection_questions={4}" -f
        [string]$actionSummary.open_action.action_id,
        [string]$actionSummary.open_action.entry_name,
        [string]$actionSummary.open_action.projection_kind,
        @($actionSummary.open_action.projection_summary_lines).Count,
        @($actionSummary.open_action.projection_question_lines).Count
    )
    Write-Host "ok=1"
} finally {
    Pop-Location
}
