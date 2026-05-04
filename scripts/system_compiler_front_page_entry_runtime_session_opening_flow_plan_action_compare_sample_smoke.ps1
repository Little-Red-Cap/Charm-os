param(
    [string]$SampleSummary = "schemas/examples/system_compiler.witness_bundle.v0.sample.json",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-flow-plan-action-compare-sample-smoke",
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

$planActionSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_opening_flow_plan_action_sample_smoke.ps1"
$actionExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer_plan_action.py"
$actionValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_consumer_plan_action.py"
$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_flow_consumer_plan_action.py"
$compareValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_consumer_plan_action_compare.py"
foreach ($requiredPath in @(
    $planActionSmokeScript,
    $actionExportScript,
    $actionValidateScript,
    $compareScript,
    $compareValidateScript
)) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing script: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $actionChainRoot = Join-Path $outputRootPath "action-chain"
    $entrySelectedActionRoot = Join-Path $outputRootPath "entry-selected-action"
    $selfCompareRoot = Join-Path $outputRootPath "action-self-standing"
    $selectorCompareRoot = Join-Path $outputRootPath "default-selector-to-entry-selector"
    $planSummaryPath = Join-Path $actionChainRoot "plan\front-page.entry-opening-flow.consumer.plan.summary.json"
    $defaultActionPath = Join-Path $actionChainRoot "action\front-page.entry-opening-flow.consumer.plan-action.summary.json"
    $entrySelectedActionPath = Join-Path $entrySelectedActionRoot "front-page.entry-opening-flow.consumer.plan-action.summary.json"

    $chainArgs = @(
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        $planActionSmokeScript,
        "-SampleSummary",
        $sampleSummaryPath,
        "-OutputRoot",
        $actionChainRoot,
        "-PythonExe",
        $resolvedPythonExe
    )
    if ($Clean) {
        $chainArgs += "-Clean"
    }

    Invoke-ExternalTool `
        -Executable "powershell.exe" `
        -ArgumentList $chainArgs `
        -FailureMessage "runtime session opening flow plan action sample smoke failed"

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $actionExportScript,
            "--plan",
            $planSummaryPath,
            "--entry-name",
            "runtime-session-sample",
            "--output-root",
            $entrySelectedActionRoot
        ) `
        -FailureMessage "runtime session entry-selected plan action sample export failed"

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($actionValidateScript, "--summary", $entrySelectedActionPath) `
        -FailureMessage "runtime session entry-selected plan action sample validation failed"

    $cases = @(
        [ordered]@{
            Name = "action-self-standing"
            Baseline = $defaultActionPath
            Candidate = $defaultActionPath
            OutputRoot = $selfCompareRoot
            ExpectedVerdict = "standing"
            ExpectedChangedFields = 0
            ExpectedSelectionChanged = $false
            ExpectedReceiptChanged = $false
            ExpectedOpenActionChanged = $false
        },
        [ordered]@{
            Name = "default-selector-to-entry-selector"
            Baseline = $defaultActionPath
            Candidate = $entrySelectedActionPath
            OutputRoot = $selectorCompareRoot
            ExpectedVerdict = "drifted"
            ExpectedChangedFields = 3
            ExpectedSelectionChanged = $true
            ExpectedReceiptChanged = $true
            ExpectedOpenActionChanged = $false
        }
    )

    foreach ($case in $cases) {
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @(
                $compareScript,
                "--baseline",
                $case.Baseline,
                "--candidate",
                $case.Candidate,
                "--output-root",
                $case.OutputRoot
            ) `
            -FailureMessage ("runtime session plan action compare failed for case '{0}'" -f $case.Name)

        $compareSummaryPath = Join-Path $case.OutputRoot "front-page.entry-opening-flow.consumer.plan-action.compare.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($compareValidateScript, "--summary", $compareSummaryPath) `
            -FailureMessage ("runtime session plan action compare validation failed for case '{0}'" -f $case.Name)

        $compareSummary = Load-JsonObject -Path $compareSummaryPath
        Assert-Condition `
            -Condition ([string]$compareSummary.action_verdict -eq [string]$case.ExpectedVerdict) `
            -Message ("case '{0}' expected verdict '{1}' but got '{2}'" -f $case.Name, $case.ExpectedVerdict, $compareSummary.action_verdict)
        Assert-Condition `
            -Condition ([int]$compareSummary.change_summary.changed_field_count -eq [int]$case.ExpectedChangedFields) `
            -Message ("case '{0}' expected changed fields '{1}' but got '{2}'" -f $case.Name, $case.ExpectedChangedFields, $compareSummary.change_summary.changed_field_count)
        Assert-Condition `
            -Condition ([bool]$compareSummary.selection_changes.changed -eq [bool]$case.ExpectedSelectionChanged) `
            -Message ("case '{0}' selection changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$compareSummary.open_action_changes.changed -eq [bool]$case.ExpectedOpenActionChanged) `
            -Message ("case '{0}' open action changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$compareSummary.execution_receipt_changes.changed -eq [bool]$case.ExpectedReceiptChanged) `
            -Message ("case '{0}' execution receipt changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition (-not [bool]$compareSummary.action_regression_surface.action_id_changed) `
            -Message ("case '{0}' should not change action id" -f $case.Name)
        Assert-Condition `
            -Condition (-not [bool]$compareSummary.action_regression_surface.entry_changed) `
            -Message ("case '{0}' should not change entry" -f $case.Name)
        Assert-Condition `
            -Condition (-not [bool]$compareSummary.action_regression_surface.opener_changed) `
            -Message ("case '{0}' should not change opener" -f $case.Name)
        Assert-Condition `
            -Condition (-not [bool]$compareSummary.action_regression_surface.target_changed) `
            -Message ("case '{0}' should not change target" -f $case.Name)
        Assert-Condition `
            -Condition (-not [bool]$compareSummary.action_regression_surface.opening_reason_changed) `
            -Message ("case '{0}' should not change opening reason" -f $case.Name)
        Assert-Condition `
            -Condition (-not [bool]$compareSummary.action_regression_surface.projection_headline_changed) `
            -Message ("case '{0}' should not change projection headline" -f $case.Name)

        Write-Host (
            "[FRONT-PAGE-ENTRY-RUNTIME-SESSION-PLAN-ACTION-COMPARE-SAMPLE-SMOKE] case={0} verdict={1} changed={2} selection_changed={3} open_action_changed={4} receipt_changed={5}" -f
            $case.Name,
            [string]$compareSummary.action_verdict,
            [int]$compareSummary.change_summary.changed_field_count,
            [bool]$compareSummary.selection_changes.changed,
            [bool]$compareSummary.open_action_changes.changed,
            [bool]$compareSummary.execution_receipt_changes.changed
        )
    }

    Write-Host ("[FRONT-PAGE-ENTRY-RUNTIME-SESSION-PLAN-ACTION-COMPARE-SAMPLE-SMOKE] default_action={0}" -f $defaultActionPath)
    Write-Host ("[FRONT-PAGE-ENTRY-RUNTIME-SESSION-PLAN-ACTION-COMPARE-SAMPLE-SMOKE] entry_selected_action={0}" -f $entrySelectedActionPath)
    Write-Host "ok=1"
} finally {
    Pop-Location
}
