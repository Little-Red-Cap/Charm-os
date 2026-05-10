param(
    [string]$SelectorRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-consumer-selector-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-consumer-selector-compare-smoke",
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

function New-SyntheticDriftSelector {
    param(
        [string]$SourcePath,
        [string]$OutputPath
    )

    $summary = Load-JsonObject -Path $SourcePath
    $removedName = [string]$summary.selector_status.compare_entry_name
    $keptOrderedEntries = @()
    foreach ($entry in @($summary.open_plan.ordered_entries)) {
        if ([string]$entry.name -ne $removedName) {
            $keptOrderedEntries += $entry
        }
    }

    $keptFallbackEntries = @()
    foreach ($entry in @($summary.open_plan.fallback_entries)) {
        if ([string]$entry.name -ne $removedName) {
            $keptFallbackEntries += $entry
        }
    }

    $rank = 0
    foreach ($entry in @($keptOrderedEntries)) {
        $entry.rank = $rank
        $rank += 1
    }

    $summary.open_plan.compare_entry = [ordered]@{}
    $summary.open_plan.ordered_entries = @($keptOrderedEntries)
    $summary.open_plan.fallback_entries = @($keptFallbackEntries)
    $summary.selector_status.selected_entry_count = @($keptOrderedEntries).Count
    $summary.selector_status.compare_entry_name = ""
    $summary.selector_status.fallback_entry_count = @($keptFallbackEntries).Count
    $summary.artifact_context.output_root = (Split-Path -Parent $OutputPath)
    $summary.artifact_context.selector_summary_path = $OutputPath
    $summary.front_page.summary_path = $OutputPath

    Write-JsonFile -Path $OutputPath -Value $summary
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$selectorRootPath = Resolve-FullPath -Path $SelectorRoot
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

$selectorSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_flow_consumer_selector_smoke.ps1"
$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_flow_consumer_selector.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_consumer_selector_compare.py"
foreach ($requiredPath in @($selectorSmokeScript, $compareScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $selectorSummaryPath = Join-Path $selectorRootPath "front-page.entry-opening-flow.consumer.selector.summary.json"
    if (Test-Path -LiteralPath $selectorSummaryPath) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-COMPARE-SMOKE] bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable "powershell.exe" `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $selectorSmokeScript,
                "-OutputRoot",
                $selectorRootPath
            ) `
            -FailureMessage "front page entry opening-flow consumer selector smoke bootstrap failed"
    }

    $syntheticRoot = Join-Path $outputRootPath "_synthetic"
    Ensure-Directory -Path $syntheticRoot
    $driftedSelectorSummaryPath = Join-Path $syntheticRoot "front-page.entry-opening-flow.consumer.selector.drifted.summary.json"
    New-SyntheticDriftSelector -SourcePath $selectorSummaryPath -OutputPath $driftedSelectorSummaryPath

    $cases = @(
        [ordered]@{
            Name = "self-standing"
            Baseline = $selectorSummaryPath
            Candidate = $selectorSummaryPath
            ExpectedVerdict = "standing"
            ExpectedChangedEntries = 0
            ExpectedRemoved = @()
            ExpectedDefaultChanged = $false
            ExpectedCompareChanged = $false
        },
        [ordered]@{
            Name = "removed-compare-entry"
            Baseline = $selectorSummaryPath
            Candidate = $driftedSelectorSummaryPath
            ExpectedVerdict = "drifted"
            ExpectedChangedEntries = 8
            ExpectedRemoved = @("root-witness-to-root-world-compare")
            ExpectedDefaultChanged = $false
            ExpectedCompareChanged = $true
        }
    )

    foreach ($case in $cases) {
        foreach ($requiredSummary in @($case.Baseline, $case.Candidate)) {
            if (-not (Test-Path -LiteralPath $requiredSummary)) {
                throw "selector summary not found for case '$($case.Name)': $requiredSummary"
            }
        }

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
            -FailureMessage ("front page entry opening-flow consumer selector compare export failed for case '{0}'" -f $case.Name)

        $compareSummaryPath = Join-Path $caseOutputRoot "front-page.entry-opening-flow.consumer.selector.compare.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($validateScript, "--summary", $compareSummaryPath) `
            -FailureMessage ("front page entry opening-flow consumer selector compare validation failed for case '{0}'" -f $case.Name)

        $compareSummary = Load-JsonObject -Path $compareSummaryPath
        Assert-Condition `
            -Condition ([string]$compareSummary.selector_verdict -eq [string]$case.ExpectedVerdict) `
            -Message ("case '{0}' expected verdict '{1}' but got '{2}'" -f $case.Name, $case.ExpectedVerdict, $compareSummary.selector_verdict)
        Assert-Condition `
            -Condition ([int]$compareSummary.selector_entry_summary.changed_entry_count -eq [int]$case.ExpectedChangedEntries) `
            -Message ("case '{0}' expected changed entries '{1}' but got '{2}'" -f $case.Name, $case.ExpectedChangedEntries, $compareSummary.selector_entry_summary.changed_entry_count)
        Assert-Condition `
            -Condition ([bool]$compareSummary.selector_regression_surface.default_entry_changed -eq [bool]$case.ExpectedDefaultChanged) `
            -Message ("case '{0}' default changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$compareSummary.selector_regression_surface.compare_entry_changed -eq [bool]$case.ExpectedCompareChanged) `
            -Message ("case '{0}' compare changed expectation mismatch" -f $case.Name)

        $removedEntries = @([string[]]$compareSummary.selector_changes.ordered_entry_name_changes.removed)
        foreach ($entryName in @($case.ExpectedRemoved)) {
            Assert-Condition `
                -Condition ($removedEntries -contains [string]$entryName) `
                -Message ("case '{0}' expected removed selector entry '{1}'" -f $case.Name, $entryName)
        }

        Write-Host (
            "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-COMPARE-SMOKE] case={0} verdict={1} changed={2} added={3} removed={4} default_changed={5} compare_changed={6}" -f
            $case.Name,
            [string]$compareSummary.selector_verdict,
            [int]$compareSummary.selector_entry_summary.changed_entry_count,
            [int]$compareSummary.selector_entry_summary.added_entry_count,
            [int]$compareSummary.selector_entry_summary.removed_entry_count,
            [bool]$compareSummary.selector_regression_surface.default_entry_changed,
            [bool]$compareSummary.selector_regression_surface.compare_entry_changed
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-COMPARE-SMOKE] output_root={0}" -f $outputRootPath)
