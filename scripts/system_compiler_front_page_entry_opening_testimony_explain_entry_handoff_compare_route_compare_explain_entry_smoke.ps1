param(
    [string]$OpeningTestimonyExplainEntryHandoffCompareRouteRoot = "cmake-build-system-compiler-front-page-entry-opening-testimony-explain-entry-handoff-compare-route-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-testimony-explain-entry-handoff-compare-route-compare-explain-entry-smoke",
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
    $json = $Value | ConvertTo-Json -Depth 100
    Set-Content -LiteralPath $Path -Encoding utf8 -Value ($json + [Environment]::NewLine)
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
$routeCompareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_route.py"
$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_testimony_explain_entry.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_testimony_explain_entry.py"
foreach ($requiredPath in @($handoffCompareRouteSmokeScript, $routeCompareScript, $exportScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $standingRoutePath = Join-Path $openingTestimonyExplainEntryHandoffCompareRouteRootPath "standing-handoff-compare-route\front-page.route.summary.json"
    $driftedRoutePath = Join-Path $openingTestimonyExplainEntryHandoffCompareRouteRootPath "drifted-handoff-compare-route\front-page.route.summary.json"
    $collapsedRoutePath = Join-Path $openingTestimonyExplainEntryHandoffCompareRouteRootPath "collapsed-handoff-compare-route\front-page.route.summary.json"
    $standingComparePath = Join-Path $openingTestimonyExplainEntryHandoffCompareRouteRootPath "standing-handoff-compare-route-self\front-page.route.compare.summary.json"
    if (
        (-not $Clean) -and
        (Test-Path -LiteralPath $standingRoutePath) -and
        (Test-Path -LiteralPath $driftedRoutePath) -and
        (Test-Path -LiteralPath $collapsedRoutePath) -and
        (Test-Path -LiteralPath $standingComparePath)
    ) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-COMPARE-ROUTE-COMPARE-EXPLAIN-ENTRY-SMOKE] handoff_compare_route_bootstrap=reuse-existing"
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
            -FailureMessage "opening testimony handoff-compare route smoke bootstrap failed"
    }

    $improvedCompareRoot = Join-Path $outputRootPath "_standing-to-drifted-route-compare"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $routeCompareScript,
            "--baseline",
            $standingRoutePath,
            "--candidate",
            $driftedRoutePath,
            "--output-root",
            $improvedCompareRoot
        ) `
        -FailureMessage "opening testimony handoff-compare route improved compare fixture export failed"
    $improvedComparePath = Join-Path $improvedCompareRoot "front-page.route.compare.summary.json"

    $collapsedCompareRoot = Join-Path $outputRootPath "_standing-to-collapsed-route-compare-fixture"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $routeCompareScript,
            "--baseline",
            $standingRoutePath,
            "--candidate",
            $collapsedRoutePath,
            "--output-root",
            $collapsedCompareRoot
        ) `
        -FailureMessage "opening testimony handoff-compare collapsed route compare fixture export failed"
    $collapsedComparePath = Join-Path $collapsedCompareRoot "front-page.route.compare.summary.json"
    $collapsedCompareSummary = Load-JsonObject -Path $collapsedComparePath
    $collapsedCompareSummary.result = "fail"
    $collapsedCompareSummary.route_verdict = "collapsed"
    Write-JsonFile -Path $collapsedComparePath -Value $collapsedCompareSummary

    $cases = @(
        [ordered]@{
            Name = "standing-handoff-compare-route-compare-explain-entry"
            SourceSummary = $standingComparePath
            ExpectedStatus = "ready"
            ExpectedResult = "ok"
            ExpectedSelectionKind = "route_compare_candidate_root"
            ExpectedSelectedSurface = "candidate_route"
            ExpectedViolation = ""
        },
        [ordered]@{
            Name = "improved-handoff-compare-route-compare-explain-entry"
            SourceSummary = $improvedComparePath
            ExpectedStatus = "ready"
            ExpectedResult = "ok"
            ExpectedSelectionKind = "route_compare_candidate_change"
            ExpectedSelectedSurface = "candidate_opening_testimony_explain_entry_handoff"
            ExpectedViolation = ""
        },
        [ordered]@{
            Name = "collapsed-handoff-compare-route-compare-explain-entry"
            SourceSummary = $collapsedComparePath
            ExpectedStatus = "blocked"
            ExpectedResult = "fail"
            ExpectedSelectionKind = "blocked"
            ExpectedSelectedSurface = ""
            ExpectedViolation = "source route compare verdict is collapsed"
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
            -FailureMessage ("opening testimony handoff-compare route-compare explain-entry export failed for case '{0}'" -f $case.Name)

        $summaryPath = Join-Path $caseOutputRoot "front-page.entry-opening-testimony.explain-entry.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($validateScript, "--summary", $summaryPath) `
            -FailureMessage ("opening testimony handoff-compare route-compare explain-entry validation failed for case '{0}'" -f $case.Name)

        $summary = Load-JsonObject -Path $summaryPath
        Assert-Condition `
            -Condition ([string]$summary.result -eq [string]$case.ExpectedResult) `
            -Message ("case '{0}' expected result '{1}' but got '{2}'" -f $case.Name, $case.ExpectedResult, $summary.result)
        Assert-Condition `
            -Condition ([string]$summary.explain_entry_decision.status -eq [string]$case.ExpectedStatus) `
            -Message ("case '{0}' expected status '{1}' but got '{2}'" -f $case.Name, $case.ExpectedStatus, $summary.explain_entry_decision.status)
        Assert-Condition `
            -Condition ([string]$summary.explain_entry_decision.selection_kind -eq [string]$case.ExpectedSelectionKind) `
            -Message ("case '{0}' expected selection kind '{1}' but got '{2}'" -f $case.Name, $case.ExpectedSelectionKind, $summary.explain_entry_decision.selection_kind)
        Assert-Condition `
            -Condition ([string]$summary.selected_surface.surface_id -eq [string]$case.ExpectedSelectedSurface) `
            -Message ("case '{0}' expected selected surface '{1}' but got '{2}'" -f $case.Name, $case.ExpectedSelectedSurface, $summary.selected_surface.surface_id)
        if (-not [string]::IsNullOrWhiteSpace([string]$case.ExpectedViolation)) {
            Assert-Condition `
                -Condition (@($summary.violations) -contains [string]$case.ExpectedViolation) `
                -Message ("case '{0}' expected violation '{1}'" -f $case.Name, $case.ExpectedViolation)
        } else {
            Assert-Condition `
                -Condition (@($summary.violations).Count -eq 0) `
                -Message ("case '{0}' expected no violations" -f $case.Name)
        }

        $serialized = $summary | ConvertTo-Json -Depth 100 -Compress
        foreach ($forbiddenText in @("runtime_session_summary", "world_compare_summary", "session_witness_inspect_compare_consumer")) {
            Assert-Condition `
                -Condition (-not $serialized.Contains($forbiddenText)) `
                -Message ("case '{0}' should not contain forbidden raw evidence field '{1}'" -f $case.Name, $forbiddenText)
        }

        Write-Host (
            "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-COMPARE-ROUTE-COMPARE-EXPLAIN-ENTRY-SMOKE] case={0} status={1} selection={2} selected={3}" -f
            $case.Name,
            [string]$summary.explain_entry_decision.status,
            [string]$summary.explain_entry_decision.selection_kind,
            [string]$summary.selected_surface.surface_id
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-COMPARE-ROUTE-COMPARE-EXPLAIN-ENTRY-SMOKE] output_root={0}" -f $outputRootPath)
