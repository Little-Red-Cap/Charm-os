param(
    [string]$OpeningTestimonyExplainEntryHandoffCompareRouteRoot = "cmake-build-system-compiler-front-page-entry-opening-testimony-explain-entry-handoff-compare-route-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-testimony-explain-entry-handoff-compare-route-explain-entry-smoke",
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
$openingTestimonyExplainEntryHandoffCompareRouteRootPath = Resolve-FullPath -Path $OpeningTestimonyExplainEntryHandoffCompareRouteRoot
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

$handoffCompareRouteSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_testimony_explain_entry_handoff_compare_route_smoke.ps1"
$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_testimony_explain_entry.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_testimony_explain_entry.py"
foreach ($requiredPath in @($handoffCompareRouteSmokeScript, $exportScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $driftedHandoffCompareRoutePath = Join-Path $openingTestimonyExplainEntryHandoffCompareRouteRootPath "drifted-handoff-compare-route\front-page.route.summary.json"
    $standingHandoffCompareRoutePath = Join-Path $openingTestimonyExplainEntryHandoffCompareRouteRootPath "standing-handoff-compare-route\front-page.route.summary.json"
    if (
        (-not $Clean) -and
        (Test-Path -LiteralPath $driftedHandoffCompareRoutePath) -and
        (Test-Path -LiteralPath $standingHandoffCompareRoutePath)
    ) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-COMPARE-ROUTE-EXPLAIN-ENTRY-SMOKE] handoff_compare_route_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $handoffCompareRouteSmokeScript,
                "-OutputRoot",
                $openingTestimonyExplainEntryHandoffCompareRouteRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "opening testimony handoff compare route smoke bootstrap failed"
    }

    $cases = @(
        [ordered]@{
            Name = "standing-handoff-compare-route-explain-entry"
            SourceSummary = $standingHandoffCompareRoutePath
            ExpectedStatus = "ready"
            ExpectedSelectionKind = "route_handoff_compare_default"
            ExpectedSelectedSurface = "candidate_opening_testimony_explain_entry_handoff"
        },
        [ordered]@{
            Name = "drifted-handoff-compare-route-explain-entry"
            SourceSummary = $driftedHandoffCompareRoutePath
            ExpectedStatus = "ready"
            ExpectedSelectionKind = "route_handoff_compare_default"
            ExpectedSelectedSurface = "candidate_opening_testimony_explain_entry_handoff"
        }
    )

    foreach ($case in $cases) {
        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @(
                $exportScript,
                "--source-summary",
                [string]$case.SourceSummary,
                "--output-root",
                $caseOutputRoot
            ) `
            -FailureMessage ("opening testimony handoff-compare route explain-entry export failed for case '{0}'" -f $case.Name)

        $summaryPath = Join-Path $caseOutputRoot "front-page.entry-opening-testimony.explain-entry.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($validateScript, "--summary", $summaryPath) `
            -FailureMessage ("opening testimony handoff-compare route explain-entry validation failed for case '{0}'" -f $case.Name)

        $summary = Load-JsonObject -Path $summaryPath
        Assert-Condition `
            -Condition ([string]$summary.explain_entry_decision.status -eq [string]$case.ExpectedStatus) `
            -Message ("case '{0}' expected status '{1}' but got '{2}'" -f $case.Name, $case.ExpectedStatus, $summary.explain_entry_decision.status)
        Assert-Condition `
            -Condition ([string]$summary.explain_entry_decision.selection_kind -eq [string]$case.ExpectedSelectionKind) `
            -Message ("case '{0}' expected selection kind '{1}' but got '{2}'" -f $case.Name, $case.ExpectedSelectionKind, $summary.explain_entry_decision.selection_kind)
        Assert-Condition `
            -Condition ([string]$summary.selected_surface.surface_id -eq [string]$case.ExpectedSelectedSurface) `
            -Message ("case '{0}' expected selected surface '{1}' but got '{2}'" -f $case.Name, $case.ExpectedSelectedSurface, $summary.selected_surface.surface_id)
        Assert-Condition `
            -Condition ([string]$summary.selected_surface.summary_schema -eq "system_compiler.front_page_entry_opening_testimony_explain_entry_handoff/v0") `
            -Message ("case '{0}' expected selected surface to be a handoff summary" -f $case.Name)

        Write-Host (
            "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-COMPARE-ROUTE-EXPLAIN-ENTRY-SMOKE] case={0} status={1} selection={2} selected={3}" -f
            $case.Name,
            [string]$summary.explain_entry_decision.status,
            [string]$summary.explain_entry_decision.selection_kind,
            [string]$summary.selected_surface.surface_id
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-COMPARE-ROUTE-EXPLAIN-ENTRY-SMOKE] output_root={0}" -f $outputRootPath)
