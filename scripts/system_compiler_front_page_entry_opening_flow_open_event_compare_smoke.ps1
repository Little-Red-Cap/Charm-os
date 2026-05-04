param(
    [string]$OpenEventRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-open-event-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-open-event-compare-smoke",
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
$openEventRootPath = Resolve-FullPath -Path $OpenEventRoot
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

$openEventSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_flow_open_event_smoke.ps1"
$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_flow_open_event.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_open_event_compare.py"
foreach ($requiredPath in @($openEventSmokeScript, $compareScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $baselineEventPath = Join-Path $openEventRootPath "default-no-compare\front-page.entry-opening-flow.open-event.summary.json"
    $candidateEventPath = Join-Path $openEventRootPath "default-with-drift-compare\front-page.entry-opening-flow.open-event.summary.json"
    if ((-not $Clean) -and (Test-Path -LiteralPath $baselineEventPath) -and (Test-Path -LiteralPath $candidateEventPath)) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-COMPARE-SMOKE] open_event_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $openEventSmokeScript,
                "-OutputRoot",
                $openEventRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "front page entry opening-flow open-event smoke bootstrap failed"
    }

    $cases = @(
        [ordered]@{
            Name = "open-event-self-standing"
            Baseline = $baselineEventPath
            Candidate = $baselineEventPath
            ExpectedVerdict = "standing"
            ExpectedChangedFields = 0
            ExpectedStatusChanged = $false
            ExpectedCompareContextChanged = $false
            ExpectedWitnessChanged = $false
        },
        [ordered]@{
            Name = "open-event-default-to-drift-context"
            Baseline = $baselineEventPath
            Candidate = $candidateEventPath
            ExpectedVerdict = "drifted"
            ExpectedStatusChanged = $true
            ExpectedCompareContextChanged = $true
            ExpectedWitnessChanged = $true
        }
    )

    foreach ($case in $cases) {
        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @(
                $compareScript,
                "--baseline",
                $case.Baseline,
                "--candidate",
                $case.Candidate,
                "--output-root",
                $caseOutputRoot
            ) `
            -FailureMessage ("front page entry opening-flow open-event compare failed for case '{0}'" -f $case.Name)

        $compareSummaryPath = Join-Path $caseOutputRoot "front-page.entry-opening-flow.open-event.compare.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($validateScript, "--summary", $compareSummaryPath) `
            -FailureMessage ("front page entry opening-flow open-event compare validation failed for case '{0}'" -f $case.Name)

        $summary = Load-JsonObject -Path $compareSummaryPath
        Assert-Condition `
            -Condition ([string]$summary.event_verdict -eq [string]$case.ExpectedVerdict) `
            -Message ("case '{0}' expected verdict '{1}' but got '{2}'" -f $case.Name, $case.ExpectedVerdict, $summary.event_verdict)
        if ($case.Contains("ExpectedChangedFields")) {
            Assert-Condition `
                -Condition ([int]$summary.change_summary.changed_field_count -eq [int]$case.ExpectedChangedFields) `
                -Message ("case '{0}' expected changed fields '{1}' but got '{2}'" -f $case.Name, $case.ExpectedChangedFields, $summary.change_summary.changed_field_count)
        } else {
            Assert-Condition `
                -Condition ([int]$summary.change_summary.changed_field_count -gt 0) `
                -Message ("case '{0}' expected positive changed field count" -f $case.Name)
        }
        Assert-Condition `
            -Condition ([bool]$summary.event_regression_surface.event_status_changed -eq [bool]$case.ExpectedStatusChanged) `
            -Message ("case '{0}' event status changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$summary.event_regression_surface.compare_context_changed -eq [bool]$case.ExpectedCompareContextChanged) `
            -Message ("case '{0}' compare context changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$summary.event_regression_surface.witness_set_changed -eq [bool]$case.ExpectedWitnessChanged) `
            -Message ("case '{0}' witness set changed expectation mismatch" -f $case.Name)

        Write-Host (
            "[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-COMPARE-SMOKE] case={0} verdict={1} changed={2} status_changed={3} compare_changed={4} witness_changed={5}" -f
            $case.Name,
            [string]$summary.event_verdict,
            [int]$summary.change_summary.changed_field_count,
            [bool]$summary.event_regression_surface.event_status_changed,
            [bool]$summary.event_regression_surface.compare_context_changed,
            [bool]$summary.event_regression_surface.witness_set_changed
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-COMPARE-SMOKE] output_root={0}" -f $outputRootPath)
