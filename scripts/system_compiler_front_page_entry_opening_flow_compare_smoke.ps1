param(
    [string]$InputRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-compare-smoke",
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

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    if (Test-Path $Path) {
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

function Write-JsonFile {
    param(
        [string]$Path,
        $Value
    )

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        Ensure-Directory -Path $parent
    }

    $json = $Value | ConvertTo-Json -Depth 64
    [System.IO.File]::WriteAllText($Path, $json + [Environment]::NewLine, [System.Text.Encoding]::UTF8)
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

function New-SyntheticDriftFlow {
    param(
        [string]$SourcePath,
        [string]$OutputPath
    )

    $summary = Load-JsonObject -Path $SourcePath
    $removedCaseName = ""
    $preferredRemovedCase = @($summary.opener_cases) |
        Where-Object { [string]$_.name -eq "root-witness-to-root-world-compare" } |
        Select-Object -First 1
    if ($preferredRemovedCase -is [System.Management.Automation.PSCustomObject] -or $preferredRemovedCase -is [hashtable]) {
        $removedCaseName = [string]$preferredRemovedCase.name
    } else {
        $firstCase = @($summary.opener_cases) | Select-Object -First 1
        Assert-Condition `
            -Condition ($firstCase -is [System.Management.Automation.PSCustomObject] -or $firstCase -is [hashtable]) `
            -Message "no opener case found for synthetic drift"
        $removedCaseName = [string]$firstCase.name
    }

    $keptCases = @()
    foreach ($case in @($summary.opener_cases)) {
        if ([string]$case.name -ne $removedCaseName) {
            $keptCases += $case
        }
    }

    $summary.opener_cases = @($keptCases)
    $summary.flow_status.actual_opener_count = @($keptCases).Count
    $summary.flow_status.available_projection_count = @(
        $keptCases | Where-Object { [string]$_.projection_status -eq "available" }
    ).Count
    $summary.flow_status.compare_context_count = @(
        $keptCases | Where-Object { [bool]$_.compare_context_available }
    ).Count
    $summary.flow_status.inspector_ready_count = @(
        $keptCases | Where-Object { [bool]$_.inspector_ready }
    ).Count
    $summary.flow_status.blocked_inspector_count = [int]$summary.flow_status.actual_opener_count - [int]$summary.flow_status.inspector_ready_count
    $summary.artifact_context.output_root = (Split-Path -Parent $OutputPath)
    $summary.artifact_context.flow_summary_path = $OutputPath
    $summary.front_page.summary_path = $OutputPath

    Write-JsonFile -Path $OutputPath -Value $summary
}

function New-SyntheticProjectionDriftFlow {
    param(
        [string]$SourcePath,
        [string]$OutputPath
    )

    $summary = Load-JsonObject -Path $SourcePath
    $case = @($summary.opener_cases) |
        Where-Object { [string]$_.projection_status -eq "available" } |
        Select-Object -First 1
    Assert-Condition `
        -Condition ($case -is [System.Management.Automation.PSCustomObject] -or $case -is [hashtable]) `
        -Message "no available opener case found for synthetic projection drift"

    $case.projection_headline = "synthetic projection headline drift"
    $case.projection_summary_lines = @(
        "synthetic projection summary drift",
        "projection diagnostics changed"
    )
    $case.projection_question_lines = @(
        "Which projection summary line changed the diagnosis?",
        "Should projection question drift block this opening flow?"
    )
    $summary.artifact_context.output_root = (Split-Path -Parent $OutputPath)
    $summary.artifact_context.flow_summary_path = $OutputPath
    $summary.front_page.summary_path = $OutputPath

    Write-JsonFile -Path $OutputPath -Value $summary
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$inputRootPath = Resolve-FullPath -Path $InputRoot
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

$flowSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_flow_smoke.ps1"
$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_flow.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_compare.py"
foreach ($requiredPath in @($flowSmokeScript, $compareScript, $validateScript)) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $flowSummaryPath = Join-Path $inputRootPath "front-page.entry-opening-flow.summary.json"
    if (Test-Path $flowSummaryPath) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-COMPARE-SMOKE] bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable "powershell.exe" `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $flowSmokeScript,
                "-OutputRoot",
                $inputRootPath
            ) `
            -FailureMessage "front page entry opening flow smoke bootstrap failed"
    }

    $syntheticRoot = Join-Path $outputRootPath "_synthetic"
    Ensure-Directory -Path $syntheticRoot
    $driftedFlowSummaryPath = Join-Path $syntheticRoot "front-page.entry-opening-flow.drifted.summary.json"
    $removedCaseName = New-SyntheticDriftFlow -SourcePath $flowSummaryPath -OutputPath $driftedFlowSummaryPath
    $projectionDriftFlowSummaryPath = Join-Path $syntheticRoot "front-page.entry-opening-flow.projection-drift.summary.json"
    New-SyntheticProjectionDriftFlow -SourcePath $flowSummaryPath -OutputPath $projectionDriftFlowSummaryPath

    $cases = @(
        [ordered]@{
            Name = "self-standing"
            Baseline = $flowSummaryPath
            Candidate = $flowSummaryPath
            ExpectedVerdict = "standing"
            ExpectedRemoved = @()
            ExpectedChangedCases = 0
            ExpectedProjectionSummaryChanged = 0
            ExpectedProjectionQuestionChanged = 0
        },
        [ordered]@{
            Name = "removed-compare-opener"
            Baseline = $flowSummaryPath
            Candidate = $driftedFlowSummaryPath
            ExpectedVerdict = "drifted"
            ExpectedRemoved = @($removedCaseName)
            ExpectedChangedCases = 0
            ExpectedProjectionSummaryChanged = 0
            ExpectedProjectionQuestionChanged = 0
        },
        [ordered]@{
            Name = "projection-preview-drift"
            Baseline = $flowSummaryPath
            Candidate = $projectionDriftFlowSummaryPath
            ExpectedVerdict = "drifted"
            ExpectedRemoved = @()
            ExpectedChangedCases = 1
            ExpectedProjectionSummaryChanged = 1
            ExpectedProjectionQuestionChanged = 1
        }
    )

    foreach ($case in $cases) {
        foreach ($requiredSummary in @($case.Baseline, $case.Candidate)) {
            if (-not (Test-Path $requiredSummary)) {
                throw "opening flow summary not found for case '$($case.Name)': $requiredSummary"
            }
        }

        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($compareScript, "--baseline", $case.Baseline, "--candidate", $case.Candidate, "--output-root", $caseOutputRoot) `
            -FailureMessage ("front page entry opening flow compare export failed for case '{0}'" -f $case.Name)

        $compareSummaryPath = Join-Path $caseOutputRoot "front-page.entry-opening-flow.compare.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($validateScript, "--summary", $compareSummaryPath) `
            -FailureMessage ("front page entry opening flow compare validation failed for case '{0}'" -f $case.Name)

        $compareSummary = Load-JsonObject -Path $compareSummaryPath
        Assert-Condition `
            -Condition ([string]$compareSummary.flow_verdict -eq [string]$case.ExpectedVerdict) `
            -Message ("case '{0}' expected verdict '{1}' but got '{2}'" -f $case.Name, $case.ExpectedVerdict, $compareSummary.flow_verdict)
        Assert-Condition `
            -Condition ([int]$compareSummary.opener_case_summary.changed_case_count -eq [int]$case.ExpectedChangedCases) `
            -Message ("case '{0}' expected changed cases '{1}' but got '{2}'" -f $case.Name, $case.ExpectedChangedCases, $compareSummary.opener_case_summary.changed_case_count)
        Assert-Condition `
            -Condition ([int]$compareSummary.opener_case_summary.projection_summary_changed_count -eq [int]$case.ExpectedProjectionSummaryChanged) `
            -Message ("case '{0}' expected projection summary changed '{1}' but got '{2}'" -f $case.Name, $case.ExpectedProjectionSummaryChanged, $compareSummary.opener_case_summary.projection_summary_changed_count)
        Assert-Condition `
            -Condition ([int]$compareSummary.opener_case_summary.projection_question_changed_count -eq [int]$case.ExpectedProjectionQuestionChanged) `
            -Message ("case '{0}' expected projection question changed '{1}' but got '{2}'" -f $case.Name, $case.ExpectedProjectionQuestionChanged, $compareSummary.opener_case_summary.projection_question_changed_count)

        $removedCases = @([string[]]$compareSummary.flow_changes.opener_case_changes.removed)
        foreach ($caseName in @($case.ExpectedRemoved)) {
            Assert-Condition `
                -Condition ($removedCases -contains [string]$caseName) `
                -Message ("case '{0}' expected removed opener case '{1}'" -f $case.Name, $caseName)
        }

        Write-Host (
            "[FRONT-PAGE-ENTRY-OPENING-FLOW-COMPARE-SMOKE] case={0} verdict={1} changed={2} added={3} removed={4} projection_summary_changed={5}" -f
            $case.Name,
            [string]$compareSummary.flow_verdict,
            [int]$compareSummary.opener_case_summary.changed_case_count,
            [int]$compareSummary.opener_case_summary.added_case_count,
            [int]$compareSummary.opener_case_summary.removed_case_count,
            [int]$compareSummary.opener_case_summary.projection_summary_changed_count
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-COMPARE-SMOKE] output_root={0}" -f $outputRootPath)
