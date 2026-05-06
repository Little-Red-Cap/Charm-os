param(
    [string]$OpeningTestimonyExplainEntryHandoffCompareRouteExplainEntryHandoffRoot = "cmake-build-system-compiler-front-page-entry-opening-testimony-explain-entry-handoff-compare-route-explain-entry-handoff-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-testimony-explain-entry-handoff-compare-route-explain-entry-handoff-compare-smoke",
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
$openingTestimonyExplainEntryHandoffCompareRouteExplainEntryHandoffRootPath = Resolve-FullPath -Path $OpeningTestimonyExplainEntryHandoffCompareRouteExplainEntryHandoffRoot
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

$handoffCompareRouteExplainEntryHandoffSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_testimony_explain_entry_handoff_compare_route_explain_entry_handoff_smoke.ps1"
$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_testimony_explain_entry_handoff.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_testimony_explain_entry_handoff_compare.py"
foreach ($requiredPath in @($handoffCompareRouteExplainEntryHandoffSmokeScript, $compareScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $standingHandoffPath = Join-Path $openingTestimonyExplainEntryHandoffCompareRouteExplainEntryHandoffRootPath "standing-handoff-compare-route-explain-entry-handoff\front-page.entry-opening-testimony.explain-entry.handoff.summary.json"
    $driftedHandoffPath = Join-Path $openingTestimonyExplainEntryHandoffCompareRouteExplainEntryHandoffRootPath "drifted-handoff-compare-route-explain-entry-handoff\front-page.entry-opening-testimony.explain-entry.handoff.summary.json"
    if (
        (-not $Clean) -and
        (Test-Path -LiteralPath $standingHandoffPath) -and
        (Test-Path -LiteralPath $driftedHandoffPath)
    ) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-COMPARE-ROUTE-EXPLAIN-ENTRY-HANDOFF-COMPARE-SMOKE] handoff_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $handoffCompareRouteExplainEntryHandoffSmokeScript,
                "-OutputRoot",
                $openingTestimonyExplainEntryHandoffCompareRouteExplainEntryHandoffRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "opening testimony handoff-compare route explain-entry handoff smoke bootstrap failed"
    }

    $cases = @(
        [ordered]@{
            Name = "self-standing"
            Baseline = $standingHandoffPath
            Candidate = $standingHandoffPath
            ExpectedVerdict = "standing"
            ExpectedCandidateStatus = "ready"
            ExpectedOpenTargetChanged = $false
            ExpectedSameOpenTarget = $true
            ExpectedActionChanged = $false
        },
        [ordered]@{
            Name = "standing-to-drifted"
            Baseline = $standingHandoffPath
            Candidate = $driftedHandoffPath
            ExpectedVerdict = "drifted"
            ExpectedCandidateStatus = "ready"
            ExpectedOpenTargetChanged = $true
            ExpectedSameOpenTarget = $false
            ExpectedActionChanged = $true
        }
    )

    foreach ($case in $cases) {
        foreach ($requiredSummary in @($case.Baseline, $case.Candidate)) {
            if (-not (Test-Path -LiteralPath $requiredSummary)) {
                throw "handoff summary not found for case '$($case.Name)': $requiredSummary"
            }
        }

        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @(
                $compareScript,
                "--baseline",
                [string]$case.Baseline,
                "--candidate",
                [string]$case.Candidate,
                "--output-root",
                $caseOutputRoot
            ) `
            -FailureMessage ("opening testimony handoff-compare route explain-entry handoff compare export failed for case '{0}'" -f $case.Name)

        $compareSummaryPath = Join-Path $caseOutputRoot "front-page.entry-opening-testimony.explain-entry.handoff.compare.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($validateScript, "--summary", $compareSummaryPath) `
            -FailureMessage ("opening testimony handoff-compare route explain-entry handoff compare validation failed for case '{0}'" -f $case.Name)

        $summary = Load-JsonObject -Path $compareSummaryPath
        Assert-Condition `
            -Condition ([string]$summary.handoff_verdict -eq [string]$case.ExpectedVerdict) `
            -Message ("case '{0}' expected verdict '{1}' but got '{2}'" -f $case.Name, $case.ExpectedVerdict, $summary.handoff_verdict)
        Assert-Condition `
            -Condition ([string]$summary.handoff_status.candidate_handoff_status -eq [string]$case.ExpectedCandidateStatus) `
            -Message ("case '{0}' expected candidate handoff status '{1}' but got '{2}'" -f $case.Name, $case.ExpectedCandidateStatus, $summary.handoff_status.candidate_handoff_status)
        Assert-Condition `
            -Condition ([bool]$summary.handoff_regression_surface.open_target_changed -eq [bool]$case.ExpectedOpenTargetChanged) `
            -Message ("case '{0}' open target changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$summary.handoff_regression_surface.same_open_target -eq [bool]$case.ExpectedSameOpenTarget) `
            -Message ("case '{0}' same open target expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$summary.handoff_regression_surface.handoff_action_changed -eq [bool]$case.ExpectedActionChanged) `
            -Message ("case '{0}' handoff action changed expectation mismatch" -f $case.Name)

        $frontPageSurfaceIds = @([string[]]$summary.front_page.supporting_surfaces.id)
        Assert-Condition `
            -Condition ($frontPageSurfaceIds -contains "baseline_opening_testimony_explain_entry_handoff") `
            -Message ("case '{0}' missing baseline handoff front_page surface" -f $case.Name)
        Assert-Condition `
            -Condition ($frontPageSurfaceIds -contains "candidate_opening_testimony_explain_entry_handoff") `
            -Message ("case '{0}' missing candidate handoff front_page surface" -f $case.Name)

        $serialized = $summary | ConvertTo-Json -Depth 100 -Compress
        foreach ($forbiddenText in @("runtime_session_summary", "world_compare_summary", "session_witness_inspect_compare_consumer")) {
            Assert-Condition `
                -Condition (-not $serialized.Contains($forbiddenText)) `
                -Message ("case '{0}' should not contain forbidden raw evidence field '{1}'" -f $case.Name, $forbiddenText)
        }

        Write-Host (
            "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-COMPARE-ROUTE-EXPLAIN-ENTRY-HANDOFF-COMPARE-SMOKE] case={0} verdict={1} candidate_status={2} target_changed={3} action_changed={4}" -f
            $case.Name,
            [string]$summary.handoff_verdict,
            [string]$summary.handoff_status.candidate_handoff_status,
            [bool]$summary.handoff_regression_surface.open_target_changed,
            [bool]$summary.handoff_regression_surface.handoff_action_changed
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-COMPARE-ROUTE-EXPLAIN-ENTRY-HANDOFF-COMPARE-SMOKE] output_root={0}" -f $outputRootPath)
