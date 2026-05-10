param(
    [string]$ActionWorkspaceRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-action-workspace-smoke",
    [string]$ActionCompareRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-action-compare-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-open-event-smoke",
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
$actionWorkspaceRootPath = Resolve-FullPath -Path $ActionWorkspaceRoot
$actionCompareRootPath = Resolve-FullPath -Path $ActionCompareRoot
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

$actionWorkspaceSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_flow_consumer_plan_action_workspace_smoke.ps1"
$actionCompareSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_flow_consumer_plan_action_compare_smoke.ps1"
$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_open_event.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_open_event.py"
foreach ($requiredPath in @($actionWorkspaceSmokeScript, $actionCompareSmokeScript, $exportScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $defaultActionPath = Join-Path $actionWorkspaceRootPath "cold-default\action\front-page.entry-opening-flow.consumer.plan-action.summary.json"
    if ((-not $Clean) -and (Test-Path -LiteralPath $defaultActionPath)) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-SMOKE] action_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $actionWorkspaceSmokeScript,
                "-OutputRoot",
                $actionWorkspaceRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "front page entry opening-flow consumer plan action workspace smoke bootstrap failed"
    }

    $compareSummaryPath = Join-Path $actionCompareRootPath "default-to-compare-neighbor\front-page.entry-opening-flow.consumer.plan-action.compare.summary.json"
    if ((-not $Clean) -and (Test-Path -LiteralPath $compareSummaryPath)) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-SMOKE] compare_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $actionCompareSmokeScript,
                "-ActionWorkspaceRoot",
                $actionWorkspaceRootPath,
                "-OutputRoot",
                $actionCompareRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "front page entry opening-flow consumer plan action compare smoke bootstrap failed"
    }

    $cases = @(
        [ordered]@{
            Name = "default-no-compare"
            Action = $defaultActionPath
            ActionCompare = ""
            ExpectedStatus = "accepted"
            ExpectedCompareAvailable = $false
            ExpectedCompareVerdict = "not_attached"
            ExpectedWitnessCount = 3
            ExpectedRejectedAtLeast = 1
        },
        [ordered]@{
            Name = "default-with-drift-compare"
            Action = $defaultActionPath
            ActionCompare = $compareSummaryPath
            ExpectedStatus = "accepted_with_drift"
            ExpectedCompareAvailable = $true
            ExpectedCompareVerdict = "drifted"
            ExpectedWitnessCount = 4
            ExpectedRejectedAtLeast = 1
        }
    )

    foreach ($case in $cases) {
        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        $arguments = @(
            $exportScript,
            "--action",
            $case.Action,
            "--output-root",
            $caseOutputRoot
        )
        if (-not [string]::IsNullOrWhiteSpace([string]$case.ActionCompare)) {
            $arguments += @("--action-compare", [string]$case.ActionCompare)
        }

        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList $arguments `
            -FailureMessage ("front page entry opening-flow open-event export failed for case '{0}'" -f $case.Name)

        $openEventSummaryPath = Join-Path $caseOutputRoot "front-page.entry-opening-flow.open-event.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($validateScript, "--summary", $openEventSummaryPath) `
            -FailureMessage ("front page entry opening-flow open-event validation failed for case '{0}'" -f $case.Name)

        $summary = Load-JsonObject -Path $openEventSummaryPath
        Assert-Condition `
            -Condition ([string]$summary.result -eq "ok") `
            -Message ("case '{0}' expected result ok but got '{1}'" -f $case.Name, $summary.result)
        Assert-Condition `
            -Condition ([string]$summary.open_event.status -eq [string]$case.ExpectedStatus) `
            -Message ("case '{0}' expected status '{1}' but got '{2}'" -f $case.Name, $case.ExpectedStatus, $summary.open_event.status)
        Assert-Condition `
            -Condition ([bool]$summary.compare_summary.available -eq [bool]$case.ExpectedCompareAvailable) `
            -Message ("case '{0}' compare availability mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([string]$summary.compare_summary.action_verdict -eq [string]$case.ExpectedCompareVerdict) `
            -Message ("case '{0}' expected compare verdict '{1}' but got '{2}'" -f $case.Name, $case.ExpectedCompareVerdict, $summary.compare_summary.action_verdict)
        Assert-Condition `
            -Condition (@($summary.witness_refs).Count -eq [int]$case.ExpectedWitnessCount) `
            -Message ("case '{0}' expected witness count '{1}' but got '{2}'" -f $case.Name, $case.ExpectedWitnessCount, @($summary.witness_refs).Count)
        Assert-Condition `
            -Condition ([int]$summary.consumer_decision.rejected_consumer_count -ge [int]$case.ExpectedRejectedAtLeast) `
            -Message ("case '{0}' expected at least '{1}' rejected consumer but got '{2}'" -f $case.Name, $case.ExpectedRejectedAtLeast, $summary.consumer_decision.rejected_consumer_count)
        Assert-Condition `
            -Condition ([string]$summary.workspace_facade.status -eq "projected") `
            -Message ("case '{0}' expected projected workspace facade" -f $case.Name)

        Write-Host (
            "[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-SMOKE] case={0} status={1} compare={2}/{3} selected={4} rejected={5} witness_refs={6}" -f
            $case.Name,
            [string]$summary.open_event.status,
            [bool]$summary.compare_summary.available,
            [string]$summary.compare_summary.action_verdict,
            [string]$summary.consumer_decision.selected_consumer.consumer_id,
            [int]$summary.consumer_decision.rejected_consumer_count,
            @($summary.witness_refs).Count
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-SMOKE] output_root={0}" -f $outputRootPath)
