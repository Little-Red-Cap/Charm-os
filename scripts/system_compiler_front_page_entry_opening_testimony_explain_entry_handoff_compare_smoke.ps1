param(
    [string]$OpeningTestimonyExplainEntryHandoffRoot = "cmake-build-system-compiler-front-page-entry-opening-testimony-explain-entry-handoff-smoke",
    [string]$OpeningTestimonyExplainEntryCompareRouteHandoffRoot = "cmake-build-system-compiler-front-page-entry-opening-testimony-explain-entry-compare-route-handoff-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-testimony-explain-entry-handoff-compare-smoke",
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
$openingTestimonyExplainEntryHandoffRootPath = Resolve-FullPath -Path $OpeningTestimonyExplainEntryHandoffRoot
$openingTestimonyExplainEntryCompareRouteHandoffRootPath = Resolve-FullPath -Path $OpeningTestimonyExplainEntryCompareRouteHandoffRoot
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

$handoffSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_testimony_explain_entry_handoff_smoke.ps1"
$compareRouteHandoffSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_testimony_explain_entry_compare_route_handoff_smoke.ps1"
$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_testimony_explain_entry_handoff.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_testimony_explain_entry_handoff_compare.py"
foreach ($requiredPath in @($handoffSmokeScript, $compareRouteHandoffSmokeScript, $compareScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $cleanHandoffPath = Join-Path $openingTestimonyExplainEntryHandoffRootPath "clean-route-handoff\front-page.entry-opening-testimony.explain-entry.handoff.summary.json"
    $driftHandoffPath = Join-Path $openingTestimonyExplainEntryHandoffRootPath "drift-route-handoff\front-page.entry-opening-testimony.explain-entry.handoff.summary.json"
    $landingCompareHandoffPath = Join-Path $openingTestimonyExplainEntryHandoffRootPath "landing-compare-route-handoff\front-page.entry-opening-testimony.explain-entry.handoff.summary.json"
    $blockedHandoffPath = Join-Path $openingTestimonyExplainEntryHandoffRootPath "missing-selected-surface-handoff\front-page.entry-opening-testimony.explain-entry.handoff.summary.json"
    if (
        (-not $Clean) -and
        (Test-Path -LiteralPath $cleanHandoffPath) -and
        (Test-Path -LiteralPath $driftHandoffPath) -and
        (Test-Path -LiteralPath $landingCompareHandoffPath) -and
        (Test-Path -LiteralPath $blockedHandoffPath)
    ) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-COMPARE-SMOKE] handoff_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $handoffSmokeScript,
                "-OutputRoot",
                $openingTestimonyExplainEntryHandoffRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "opening testimony explain-entry handoff smoke bootstrap failed"
    }

    $compareRouteHandoffPath = Join-Path $openingTestimonyExplainEntryCompareRouteHandoffRootPath "drifted-compare-route-explain-entry-handoff\front-page.entry-opening-testimony.explain-entry.handoff.summary.json"
    if ((-not $Clean) -and (Test-Path -LiteralPath $compareRouteHandoffPath)) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-COMPARE-SMOKE] compare_route_handoff_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $compareRouteHandoffSmokeScript,
                "-OutputRoot",
                $openingTestimonyExplainEntryCompareRouteHandoffRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "opening testimony explain-entry compare-route handoff smoke bootstrap failed"
    }

    $cleanSummary = Load-JsonObject -Path $cleanHandoffPath
    $driftSummary = Load-JsonObject -Path $driftHandoffPath
    $cleanToDriftExpected = if (
        ([string]$cleanSummary.open_target.surface_id -eq [string]$driftSummary.open_target.surface_id) -and
        ([string]$cleanSummary.open_target.summary_schema -eq [string]$driftSummary.open_target.summary_schema) -and
        ([string]$cleanSummary.open_target.summary_kind -eq [string]$driftSummary.open_target.summary_kind) -and
        ([string]$cleanSummary.open_target.summary_path -eq [string]$driftSummary.open_target.summary_path) -and
        ([string]$cleanSummary.handoff_action.action_kind -eq [string]$driftSummary.handoff_action.action_kind) -and
        ([string]$cleanSummary.handoff_action.expected_consumer_operation -eq [string]$driftSummary.handoff_action.expected_consumer_operation) -and
        ([string]$cleanSummary.handoff_decision.status -eq [string]$driftSummary.handoff_decision.status)
    ) {
        "standing"
    } else {
        "drifted"
    }

    $cases = @(
        [ordered]@{
            Name = "self-standing"
            Baseline = $cleanHandoffPath
            Candidate = $cleanHandoffPath
            ExpectedVerdict = "standing"
            ExpectedCandidateStatus = "ready"
            ExpectedTargetChanged = $false
        },
        [ordered]@{
            Name = "clean-to-drift"
            Baseline = $cleanHandoffPath
            Candidate = $driftHandoffPath
            ExpectedVerdict = $cleanToDriftExpected
            ExpectedCandidateStatus = "ready"
            ExpectedTargetChanged = ($cleanToDriftExpected -eq "drifted")
        },
        [ordered]@{
            Name = "clean-to-landing-compare"
            Baseline = $cleanHandoffPath
            Candidate = $landingCompareHandoffPath
            ExpectedVerdict = "drifted"
            ExpectedCandidateStatus = "ready"
            ExpectedTargetChanged = $true
        },
        [ordered]@{
            Name = "clean-to-compare-route"
            Baseline = $cleanHandoffPath
            Candidate = $compareRouteHandoffPath
            ExpectedVerdict = "drifted"
            ExpectedCandidateStatus = "ready"
            ExpectedTargetChanged = $true
        },
        [ordered]@{
            Name = "ready-to-blocked"
            Baseline = $cleanHandoffPath
            Candidate = $blockedHandoffPath
            ExpectedVerdict = "collapsed"
            ExpectedCandidateStatus = "blocked"
            ExpectedTargetChanged = $true
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
            -FailureMessage ("opening testimony explain-entry handoff compare export failed for case '{0}'" -f $case.Name)

        $compareSummaryPath = Join-Path $caseOutputRoot "front-page.entry-opening-testimony.explain-entry.handoff.compare.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($validateScript, "--summary", $compareSummaryPath) `
            -FailureMessage ("opening testimony explain-entry handoff compare validation failed for case '{0}'" -f $case.Name)

        $summary = Load-JsonObject -Path $compareSummaryPath
        Assert-Condition `
            -Condition ([string]$summary.handoff_verdict -eq [string]$case.ExpectedVerdict) `
            -Message ("case '{0}' expected verdict '{1}' but got '{2}'" -f $case.Name, $case.ExpectedVerdict, $summary.handoff_verdict)
        Assert-Condition `
            -Condition ([string]$summary.handoff_status.candidate_handoff_status -eq [string]$case.ExpectedCandidateStatus) `
            -Message ("case '{0}' expected candidate handoff status '{1}' but got '{2}'" -f $case.Name, $case.ExpectedCandidateStatus, $summary.handoff_status.candidate_handoff_status)
        Assert-Condition `
            -Condition ([bool]$summary.handoff_regression_surface.open_target_changed -eq [bool]$case.ExpectedTargetChanged) `
            -Message ("case '{0}' target changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$summary.handoff_regression_surface.same_open_target -eq (-not [bool]$summary.handoff_regression_surface.open_target_changed)) `
            -Message ("case '{0}' same_open_target is inconsistent" -f $case.Name)

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
            "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-COMPARE-SMOKE] case={0} verdict={1} candidate_status={2} target_changed={3}" -f
            $case.Name,
            [string]$summary.handoff_verdict,
            [string]$summary.handoff_status.candidate_handoff_status,
            [bool]$summary.handoff_regression_surface.open_target_changed
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-COMPARE-SMOKE] output_root={0}" -f $outputRootPath)
