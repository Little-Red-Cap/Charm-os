param(
    [string]$RuntimeSessionExplainEntryRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-explain-entry-smoke",
    [string]$RuntimeSessionExplainEntryRouteCompareRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-explain-entry-route-compare-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-explain-entry-compare-smoke",
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

function New-BlockedExplainEntryFixture {
    param(
        [string]$SourceExplainEntryPath,
        [string]$OutputPath
    )

    $summary = Load-JsonObject -Path $SourceExplainEntryPath
    $summary.result = "fail"
    $summary.explain_entry_decision.status = "blocked"
    $summary.explain_entry_decision.selection_kind = "blocked"
    $summary.explain_entry_decision.selected_entry_id = ""
    $summary.explain_entry_decision.selected_source = ""
    $summary.selected_surface.surface_id = ""
    $summary.selected_surface.label = ""
    $summary.selected_surface.role = ""
    $summary.selected_surface.summary_schema = ""
    $summary.selected_surface.summary_kind = ""
    $summary.selected_surface.summary_path = ""
    $summary.selected_surface.report_markdown_path = ""
    $summary.selected_surface.check_text_path = ""
    $summary.selected_surface.route_id = ""
    $summary.selected_surface.depth = $null
    $summary.selected_surface.source = ""
    $summary.front_page.supporting_surfaces = @(
        @($summary.front_page.supporting_surfaces) |
            Where-Object { [string]$_.id -ne "selected_explain_surface" }
    )
    $summary.artifact_context.explain_entry_summary_path = (Resolve-FullPath -Path $OutputPath)
    $summary.front_page.summary_path = (Resolve-FullPath -Path $OutputPath)
    $summary.violations = @("selected explain surface is missing")
    Write-JsonFile -Path $OutputPath -Value $summary
    return (Resolve-FullPath -Path $OutputPath)
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$runtimeSessionExplainEntryRootPath = Resolve-FullPath -Path $RuntimeSessionExplainEntryRoot
$runtimeSessionExplainEntryRouteCompareRootPath = Resolve-FullPath -Path $RuntimeSessionExplainEntryRouteCompareRoot
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

$explainEntrySmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_opening_testimony_explain_entry_smoke.ps1"
$explainEntryRouteCompareSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_opening_testimony_explain_entry_route_compare_smoke.ps1"
$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_testimony_explain_entry.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_testimony_explain_entry_compare.py"
foreach ($requiredPath in @($explainEntrySmokeScript, $explainEntryRouteCompareSmokeScript, $compareScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $cleanExplainEntryPath = Join-Path $runtimeSessionExplainEntryRootPath "clean-route-explain-entry\front-page.entry-opening-testimony.explain-entry.summary.json"
    $driftExplainEntryPath = Join-Path $runtimeSessionExplainEntryRootPath "drift-route-explain-entry\front-page.entry-opening-testimony.explain-entry.summary.json"
    $landingCompareExplainEntryPath = Join-Path $runtimeSessionExplainEntryRootPath "landing-compare-route-explain-entry\front-page.entry-opening-testimony.explain-entry.summary.json"
    if (
        (-not $Clean) -and
        (Test-Path -LiteralPath $cleanExplainEntryPath) -and
        (Test-Path -LiteralPath $driftExplainEntryPath) -and
        (Test-Path -LiteralPath $landingCompareExplainEntryPath)
    ) {
        Write-Host "[RUNTIME-SESSION-OPENING-TESTIMONY-EXPLAIN-ENTRY-COMPARE-SMOKE] explain_entry_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $explainEntrySmokeScript,
                "-OutputRoot",
                $runtimeSessionExplainEntryRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "runtime session opening testimony explain-entry smoke bootstrap failed"
    }

    $standingRouteCompareExplainEntryPath = Join-Path $runtimeSessionExplainEntryRouteCompareRootPath "standing-route-compare-explain-entry\front-page.entry-opening-testimony.explain-entry.summary.json"
    if ((-not $Clean) -and (Test-Path -LiteralPath $standingRouteCompareExplainEntryPath)) {
        Write-Host "[RUNTIME-SESSION-OPENING-TESTIMONY-EXPLAIN-ENTRY-COMPARE-SMOKE] route_compare_explain_entry_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $explainEntryRouteCompareSmokeScript,
                "-OutputRoot",
                $runtimeSessionExplainEntryRouteCompareRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "runtime session opening testimony route-compare explain-entry smoke bootstrap failed"
    }

    $blockedExplainEntryPath = New-BlockedExplainEntryFixture `
        -SourceExplainEntryPath $cleanExplainEntryPath `
        -OutputPath (Join-Path $outputRootPath "_blocked-fixtures\blocked-explain-entry\front-page.entry-opening-testimony.explain-entry.summary.json")

    $cleanSummary = Load-JsonObject -Path $cleanExplainEntryPath
    $driftSummary = Load-JsonObject -Path $driftExplainEntryPath
    $cleanToDriftExpected = if (
        ([string]$cleanSummary.selected_surface.surface_id -eq [string]$driftSummary.selected_surface.surface_id) -and
        ([string]$cleanSummary.selected_surface.summary_schema -eq [string]$driftSummary.selected_surface.summary_schema) -and
        ([string]$cleanSummary.selected_surface.summary_path -eq [string]$driftSummary.selected_surface.summary_path) -and
        ([string]$cleanSummary.source_route_ref.summary_path -eq [string]$driftSummary.source_route_ref.summary_path)
    ) {
        "standing"
    } else {
        "drifted"
    }

    $cases = @(
        [ordered]@{
            Name = "self-standing"
            Baseline = $cleanExplainEntryPath
            Candidate = $cleanExplainEntryPath
            ExpectedVerdict = "standing"
            ExpectedCandidateStatus = "ready"
        },
        [ordered]@{
            Name = "clean-to-drift"
            Baseline = $cleanExplainEntryPath
            Candidate = $driftExplainEntryPath
            ExpectedVerdict = $cleanToDriftExpected
            ExpectedCandidateStatus = "ready"
        },
        [ordered]@{
            Name = "clean-to-landing-compare"
            Baseline = $cleanExplainEntryPath
            Candidate = $landingCompareExplainEntryPath
            ExpectedVerdict = "drifted"
            ExpectedCandidateStatus = "ready"
        },
        [ordered]@{
            Name = "ready-to-blocked"
            Baseline = $cleanExplainEntryPath
            Candidate = $blockedExplainEntryPath
            ExpectedVerdict = "collapsed"
            ExpectedCandidateStatus = "blocked"
        }
    )

    foreach ($case in $cases) {
        foreach ($requiredSummary in @($case.Baseline, $case.Candidate)) {
            if (-not (Test-Path -LiteralPath $requiredSummary)) {
                throw "explain-entry summary not found for case '$($case.Name)': $requiredSummary"
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
            -FailureMessage ("runtime session opening testimony explain-entry compare export failed for case '{0}'" -f $case.Name)

        $compareSummaryPath = Join-Path $caseOutputRoot "front-page.entry-opening-testimony.explain-entry.compare.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($validateScript, "--summary", $compareSummaryPath) `
            -FailureMessage ("runtime session opening testimony explain-entry compare validation failed for case '{0}'" -f $case.Name)

        $summary = Load-JsonObject -Path $compareSummaryPath
        Assert-Condition `
            -Condition ([string]$summary.explain_entry_verdict -eq [string]$case.ExpectedVerdict) `
            -Message ("case '{0}' expected verdict '{1}' but got '{2}'" -f $case.Name, $case.ExpectedVerdict, $summary.explain_entry_verdict)
        Assert-Condition `
            -Condition ([string]$summary.explain_entry_status.candidate_decision_status -eq [string]$case.ExpectedCandidateStatus) `
            -Message ("case '{0}' expected candidate status '{1}' but got '{2}'" -f $case.Name, $case.ExpectedCandidateStatus, $summary.explain_entry_status.candidate_decision_status)

        $selectedChanged = [bool]$summary.explain_entry_regression_surface.selected_surface_changed
        $selectedPathChanged = [bool]$summary.selected_surface_changes.selected_summary_path.changed
        $selectedIdChanged = [bool]$summary.selected_surface_changes.selected_surface_id.changed
        Assert-Condition `
            -Condition ($selectedChanged -eq ($selectedPathChanged -or $selectedIdChanged -or [bool]$summary.selected_surface_changes.selected_summary_schema.changed)) `
            -Message ("case '{0}' selected surface changed summary is inconsistent" -f $case.Name)

        $frontPageSurfaceIds = @([string[]]$summary.front_page.supporting_surfaces.id)
        Assert-Condition `
            -Condition ($frontPageSurfaceIds -contains "baseline_opening_testimony_explain_entry") `
            -Message ("case '{0}' missing baseline explain-entry front_page surface" -f $case.Name)
        Assert-Condition `
            -Condition ($frontPageSurfaceIds -contains "candidate_opening_testimony_explain_entry") `
            -Message ("case '{0}' missing candidate explain-entry front_page surface" -f $case.Name)

        $serialized = $summary | ConvertTo-Json -Depth 100 -Compress
        foreach ($forbiddenText in @("runtime_session_summary", "world_compare_summary")) {
            Assert-Condition `
                -Condition (-not $serialized.Contains($forbiddenText)) `
                -Message ("case '{0}' should not contain forbidden raw evidence field '{1}'" -f $case.Name, $forbiddenText)
        }

        Write-Host (
            "[RUNTIME-SESSION-OPENING-TESTIMONY-EXPLAIN-ENTRY-COMPARE-SMOKE] case={0} verdict={1} candidate_status={2} selected_changed={3}" -f
            $case.Name,
            [string]$summary.explain_entry_verdict,
            [string]$summary.explain_entry_status.candidate_decision_status,
            [bool]$summary.explain_entry_regression_surface.selected_surface_changed
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[RUNTIME-SESSION-OPENING-TESTIMONY-EXPLAIN-ENTRY-COMPARE-SMOKE] output_root={0}" -f $outputRootPath)
