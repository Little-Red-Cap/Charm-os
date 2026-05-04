param(
    [string]$BaselineOpenEventWitnessWorkspaceRoot = "",
    [string]$CandidateOpenEventWitnessWorkspaceRoot = "",
    [string]$BaselineOpenEventWitnessSummaryPath = "",
    [string]$CandidateOpenEventWitnessSummaryPath = "",
    [string]$BaselineOpenEventWorkspaceRoot = "",
    [string]$CandidateOpenEventWorkspaceRoot = "",
    [string]$BaselineOpenEventSummaryPath = "",
    [string]$CandidateOpenEventSummaryPath = "",
    [string]$OutputRoot = "out/system-compiler-open-event-witness-ws-compare",
    [string]$BaselineWitnessRoot = "",
    [string]$CandidateWitnessRoot = "",
    [string]$CompareRoot = "",
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

function Resolve-OpenEventWitnessSummaryPath {
    param(
        [string]$WorkspaceRoot
    )

    $workspaceSummaryPath = Join-Path $WorkspaceRoot "open-event-witness\front-page.entry-opening-flow.open-event.witness.summary.json"
    if (Test-Path -LiteralPath $workspaceSummaryPath) {
        return $workspaceSummaryPath
    }

    return (Join-Path $WorkspaceRoot "front-page.entry-opening-flow.open-event.witness.summary.json")
}

function Resolve-OpenEventSummaryPath {
    param(
        [string]$WorkspaceRoot
    )

    $workspaceSummaryPath = Join-Path $WorkspaceRoot "open-event\front-page.entry-opening-flow.open-event.summary.json"
    if (Test-Path -LiteralPath $workspaceSummaryPath) {
        return $workspaceSummaryPath
    }

    return (Join-Path $WorkspaceRoot "front-page.entry-opening-flow.open-event.summary.json")
}

function Invoke-OpenEventWitnessExport {
    param(
        [string]$Role,
        [string]$OpenEventSummaryPath,
        [string]$WitnessRoot
    )

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $exportWitnessScript,
            "--open-event",
            $OpenEventSummaryPath,
            "--output-root",
            $WitnessRoot
        ) `
        -FailureMessage ("{0} opening-flow open-event witness export failed" -f $Role)
}

function Resolve-WitnessInput {
    param(
        [string]$Role,
        [string]$WitnessSummaryPath,
        [string]$WitnessWorkspaceRoot,
        [string]$OpenEventSummaryPath,
        [string]$OpenEventWorkspaceRoot,
        [string]$WitnessRoot
    )

    if (-not [string]::IsNullOrWhiteSpace($WitnessSummaryPath)) {
        $resolved = Resolve-FullPath -Path $WitnessSummaryPath
        if (-not (Test-Path -LiteralPath $resolved)) {
            throw ("{0} open-event witness summary not found: {1}" -f $Role, $resolved)
        }
        return $resolved
    }

    if (-not [string]::IsNullOrWhiteSpace($WitnessWorkspaceRoot)) {
        $resolved = Resolve-FullPath -Path (Resolve-OpenEventWitnessSummaryPath -WorkspaceRoot $WitnessWorkspaceRoot)
        if (-not (Test-Path -LiteralPath $resolved)) {
            throw ("{0} open-event witness workspace summary not found: {1}" -f $Role, $resolved)
        }
        return $resolved
    }

    $resolvedOpenEventSummaryPath = Resolve-FullPath -Path $OpenEventSummaryPath
    if ([string]::IsNullOrWhiteSpace($resolvedOpenEventSummaryPath) -and -not [string]::IsNullOrWhiteSpace($OpenEventWorkspaceRoot)) {
        $resolvedOpenEventSummaryPath = Resolve-FullPath -Path (Resolve-OpenEventSummaryPath -WorkspaceRoot $OpenEventWorkspaceRoot)
    }
    if ([string]::IsNullOrWhiteSpace($resolvedOpenEventSummaryPath)) {
        throw ("{0}: provide an open-event witness summary/workspace or an open-event summary/workspace" -f $Role)
    }
    if (-not (Test-Path -LiteralPath $resolvedOpenEventSummaryPath)) {
        throw ("{0} open-event summary not found: {1}" -f $Role, $resolvedOpenEventSummaryPath)
    }

    $null = Invoke-OpenEventWitnessExport -Role $Role -OpenEventSummaryPath $resolvedOpenEventSummaryPath -WitnessRoot $WitnessRoot
    $exportedWitnessSummaryPath = Join-Path $WitnessRoot "front-page.entry-opening-flow.open-event.witness.summary.json"
    if (-not (Test-Path -LiteralPath $exportedWitnessSummaryPath)) {
        throw ("{0} exported open-event witness summary not found: {1}" -f $Role, $exportedWitnessSummaryPath)
    }
    return $exportedWitnessSummaryPath
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$outputRootPath = Resolve-FullPath -Path $OutputRoot
$baselineOpenEventWitnessWorkspaceRootPath = Resolve-FullPath -Path $BaselineOpenEventWitnessWorkspaceRoot
$candidateOpenEventWitnessWorkspaceRootPath = Resolve-FullPath -Path $CandidateOpenEventWitnessWorkspaceRoot
$baselineOpenEventWitnessSummaryPath = Resolve-FullPath -Path $BaselineOpenEventWitnessSummaryPath
$candidateOpenEventWitnessSummaryPath = Resolve-FullPath -Path $CandidateOpenEventWitnessSummaryPath
$baselineOpenEventWorkspaceRootPath = Resolve-FullPath -Path $BaselineOpenEventWorkspaceRoot
$candidateOpenEventWorkspaceRootPath = Resolve-FullPath -Path $CandidateOpenEventWorkspaceRoot
$baselineOpenEventSummaryPath = Resolve-FullPath -Path $BaselineOpenEventSummaryPath
$candidateOpenEventSummaryPath = Resolve-FullPath -Path $CandidateOpenEventSummaryPath
$baselineWitnessRootPath = if ([string]::IsNullOrWhiteSpace($BaselineWitnessRoot)) {
    Join-Path $outputRootPath "bw"
} else {
    Resolve-FullPath -Path $BaselineWitnessRoot
}
$candidateWitnessRootPath = if ([string]::IsNullOrWhiteSpace($CandidateWitnessRoot)) {
    Join-Path $outputRootPath "cw"
} else {
    Resolve-FullPath -Path $CandidateWitnessRoot
}
$compareRootPath = if ([string]::IsNullOrWhiteSpace($CompareRoot)) {
    Join-Path $outputRootPath "compare"
} else {
    Resolve-FullPath -Path $CompareRoot
}

if ($Clean) {
    $cleanPaths = @($compareRootPath, $outputRootPath)
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

$exportWitnessScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_open_event_witness.py"
$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_flow_open_event_witness.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_open_event_witness_compare.py"
foreach ($requiredPath in @($exportWitnessScript, $compareScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $resolvedBaselineWitnessSummaryPath = Resolve-WitnessInput `
        -Role "baseline" `
        -WitnessSummaryPath $baselineOpenEventWitnessSummaryPath `
        -WitnessWorkspaceRoot $baselineOpenEventWitnessWorkspaceRootPath `
        -OpenEventSummaryPath $baselineOpenEventSummaryPath `
        -OpenEventWorkspaceRoot $baselineOpenEventWorkspaceRootPath `
        -WitnessRoot $baselineWitnessRootPath
    $resolvedCandidateWitnessSummaryPath = Resolve-WitnessInput `
        -Role "candidate" `
        -WitnessSummaryPath $candidateOpenEventWitnessSummaryPath `
        -WitnessWorkspaceRoot $candidateOpenEventWitnessWorkspaceRootPath `
        -OpenEventSummaryPath $candidateOpenEventSummaryPath `
        -OpenEventWorkspaceRoot $candidateOpenEventWorkspaceRootPath `
        -WitnessRoot $candidateWitnessRootPath

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $compareScript,
            "--baseline",
            $resolvedBaselineWitnessSummaryPath,
            "--candidate",
            $resolvedCandidateWitnessSummaryPath,
            "--output-root",
            $compareRootPath
        ) `
        -FailureMessage "opening-flow open-event witness workspace compare export failed"

    $compareSummaryPath = Join-Path $compareRootPath "front-page.entry-opening-flow.open-event.witness.compare.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateScript, "--summary", $compareSummaryPath) `
        -FailureMessage "opening-flow open-event witness workspace compare validation failed"
} finally {
    Pop-Location
}

$compareSummaryPath = Join-Path $compareRootPath "front-page.entry-opening-flow.open-event.witness.compare.summary.json"
$compareReportPath = Join-Path $compareRootPath "front-page.entry-opening-flow.open-event.witness.compare.report.md"
$compareCheckPath = Join-Path $compareRootPath "front-page.entry-opening-flow.open-event.witness.compare.check.txt"

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-WORKSPACE-COMPARE] baseline_open_event_witness_workspace_root={0}" -f $baselineOpenEventWitnessWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-WORKSPACE-COMPARE] candidate_open_event_witness_workspace_root={0}" -f $candidateOpenEventWitnessWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-WORKSPACE-COMPARE] baseline_open_event_workspace_root={0}" -f $baselineOpenEventWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-WORKSPACE-COMPARE] candidate_open_event_workspace_root={0}" -f $candidateOpenEventWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-WORKSPACE-COMPARE] baseline_open_event_witness_summary={0}" -f $resolvedBaselineWitnessSummaryPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-WORKSPACE-COMPARE] candidate_open_event_witness_summary={0}" -f $resolvedCandidateWitnessSummaryPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-WORKSPACE-COMPARE] compare_root={0}" -f $compareRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-WORKSPACE-COMPARE] summary={0}" -f $compareSummaryPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-WORKSPACE-COMPARE] report={0}" -f $compareReportPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-WORKSPACE-COMPARE] check={0}" -f $compareCheckPath)
