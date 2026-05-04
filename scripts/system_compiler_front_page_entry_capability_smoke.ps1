param(
    [string]$InputRoot = "cmake-build-system-compiler-front-page-route-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-capability-smoke",
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

function Ensure-ParentDirectory {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        Ensure-Directory -Path $parent
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

function Ensure-TextFileIfMissing {
    param(
        [string]$Path,
        [string]$Content,
        [System.Collections.Generic.List[string]]$CreatedPaths
    )

    if ([string]::IsNullOrWhiteSpace($Path) -or (Test-Path $Path)) {
        return
    }

    Ensure-ParentDirectory -Path $Path
    Set-Content -LiteralPath $Path -Value $Content -Encoding utf8
    $CreatedPaths.Add([System.IO.Path]::GetFullPath($Path)) | Out-Null
}

function Ensure-PlaceholderSurfaceArtifacts {
    param(
        [object]$Entry,
        [System.Collections.Generic.List[string]]$CreatedPaths
    )

    if ($Entry -isnot [System.Management.Automation.PSCustomObject] -and $Entry -isnot [hashtable]) {
        return
    }

    if ([string]$Entry.summary_schema -ne "temporary.placeholder/v0") {
        return
    }

    $placeholderSummary = "{`n  `"schema`": `"temporary.placeholder/v0`",`n  `"note`": `"Temporary runtime evidence root placeholder for front-page entry capability smoke.`"`n}`n"
    $placeholderReport = "# Temporary Runtime Evidence Placeholder`n"
    $placeholderCheck = "temporary runtime evidence placeholder`n"

    Ensure-TextFileIfMissing -Path ([string]$Entry.summary_path) -Content $placeholderSummary -CreatedPaths $CreatedPaths
    Ensure-TextFileIfMissing -Path ([string]$Entry.report_markdown_path) -Content $placeholderReport -CreatedPaths $CreatedPaths
    Ensure-TextFileIfMissing -Path ([string]$Entry.check_text_path) -Content $placeholderCheck -CreatedPaths $CreatedPaths
}

function Ensure-RouteSummaryPlaceholders {
    param(
        [string]$RouteSummaryPath,
        [System.Collections.Generic.List[string]]$CreatedPaths
    )

    $routeSummary = Load-JsonObject -Path $RouteSummaryPath
    Ensure-PlaceholderSurfaceArtifacts -Entry $routeSummary.root_surface -CreatedPaths $CreatedPaths
    foreach ($entry in @($routeSummary.route_entries)) {
        Ensure-PlaceholderSurfaceArtifacts -Entry $entry -CreatedPaths $CreatedPaths
    }
}

function Remove-TemporaryFiles {
    param(
        [string[]]$Paths,
        [string]$RepoRootPath
    )

    foreach ($path in @($Paths | Sort-Object -Unique | Sort-Object Length -Descending)) {
        if ([string]::IsNullOrWhiteSpace($path) -or -not (Test-Path $path)) {
            continue
        }

        Remove-Item -LiteralPath $path -Force
        $parent = Split-Path -Parent $path
        while (-not [string]::IsNullOrWhiteSpace($parent) -and $parent.StartsWith($RepoRootPath, [System.StringComparison]::OrdinalIgnoreCase)) {
            if (-not (Test-Path $parent)) {
                break
            }

            $remaining = @(Get-ChildItem -LiteralPath $parent -Force)
            if ($remaining.Count -ne 0) {
                break
            }

            Remove-Item -LiteralPath $parent -Force
            $parent = Split-Path -Parent $parent
        }
    }
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

function Assert-OpeningReason {
    param(
        [object]$OpeningReason,
        [string[]]$ExpectedKinds,
        [AllowNull()][object]$ExpectedDriftChanged,
        [AllowNull()][object]$ExpectedDriftVerdict,
        [string]$CaseName
    )

    Assert-Condition `
        -Condition ($OpeningReason -is [System.Management.Automation.PSCustomObject] -or $OpeningReason -is [hashtable]) `
        -Message ("case '{0}' expected opening_reason object" -f $CaseName)
    Assert-Condition `
        -Condition ($ExpectedKinds -contains [string]$OpeningReason.kind) `
        -Message ("case '{0}' expected opening_reason kind in '{1}' but got '{2}'" -f $CaseName, ($ExpectedKinds -join ","), $OpeningReason.kind)
    if ($null -ne $ExpectedDriftChanged) {
        Assert-Condition `
            -Condition ([bool]$OpeningReason.drift_changed -eq [bool]$ExpectedDriftChanged) `
            -Message ("case '{0}' expected opening_reason.drift_changed '{1}' but got '{2}'" -f $CaseName, $ExpectedDriftChanged, $OpeningReason.drift_changed)
    }
    if ($null -ne $ExpectedDriftVerdict) {
        Assert-Condition `
            -Condition ([string]$OpeningReason.drift_verdict -eq [string]$ExpectedDriftVerdict) `
            -Message ("case '{0}' expected opening_reason.drift_verdict '{1}' but got '{2}'" -f $CaseName, $ExpectedDriftVerdict, $OpeningReason.drift_verdict)
    }
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

$routeSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_route_smoke.ps1"
$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_capability.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_capability.py"
foreach ($requiredPath in @($routeSmokeScript, $exportScript, $validateScript)) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    Invoke-ExternalTool `
        -Executable "powershell.exe" `
        -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $routeSmokeScript,
            "-OutputRoot",
            $inputRootPath
            ) `
        -FailureMessage "front page route smoke bootstrap failed"

    $temporaryPlaceholderPaths = [System.Collections.Generic.List[string]]::new()
    $cases = @(
        [ordered]@{
            Name = "root-witness"
            SummaryPath = Join-Path $inputRootPath "root-witness\front-page.route.summary.json"
            ExpectedMode = "biography"
            ExpectedTier = "biography_ready"
            RequiredCapabilities = @("delivery_biography", "supporting_evidence", "runtime_session", "supporting_testimony")
            ExpectedProvenanceCount = 0
            ExpectedOpeningReasonKinds = @("delivery_biography")
            ExpectedOpeningReasonDriftChanged = $false
            ExpectedOpeningReasonDriftVerdict = ""
        },
        [ordered]@{
            Name = "root-world-compare"
            SummaryPath = Join-Path $inputRootPath "root-world-compare\front-page.route.summary.json"
            ExpectedMode = "compare"
            ExpectedTier = "compare_ready"
            RequiredCapabilities = @("delivery_biography", "counterfactual_verdict", "supporting_evidence", "runtime_session")
            ExpectedProvenanceCount = 0
            ExpectedOpeningReasonKinds = @("counterfactual_verdict")
            ExpectedOpeningReasonDriftChanged = $false
            ExpectedOpeningReasonDriftVerdict = ""
        },
        [ordered]@{
            Name = "witness-ci-shelf"
            SummaryPath = Join-Path $inputRootPath "witness-ci-shelf\front-page.route.summary.json"
            ExpectedMode = "review"
            ExpectedTier = "review_ready"
            RequiredCapabilities = @("grouped_review", "delivery_biography", "supporting_evidence", "runtime_session", "shelf_compare")
            ExpectedProvenanceCount = 0
            ExpectedOpeningReasonKinds = @("world_shelf_review", "world_shelf_review_drift", "grouped_review")
            ExpectedOpeningReasonDriftChanged = $null
            ExpectedOpeningReasonDriftVerdict = $null
        },
        [ordered]@{
            Name = "review-provenance"
            SummaryPath = Join-Path $inputRootPath "review-provenance\front-page.route.summary.json"
            ExpectedMode = "review"
            ExpectedTier = "review_ready"
            RequiredCapabilities = @("grouped_review", "candidate_shelf", "baseline_shelf", "route_provenance")
            ExpectedProvenanceCount = 5
            ExpectedOpeningReasonKinds = @("world_shelf_review", "world_shelf_review_drift", "grouped_review")
            ExpectedOpeningReasonDriftChanged = $null
            ExpectedOpeningReasonDriftVerdict = $null
        }
    )

    try {
        foreach ($case in $cases) {
            if (-not (Test-Path $case.SummaryPath)) {
                throw "route summary not found for case '$($case.Name)': $($case.SummaryPath)"
            }

            Ensure-RouteSummaryPlaceholders -RouteSummaryPath $case.SummaryPath -CreatedPaths $temporaryPlaceholderPaths

            $caseOutputRoot = Join-Path $outputRootPath $case.Name
            Invoke-ExternalTool `
                -Executable $resolvedPythonExe `
                -ArgumentList @($exportScript, "--summary", $case.SummaryPath, "--output-root", $caseOutputRoot) `
                -FailureMessage ("front page entry capability export failed for case '{0}'" -f $case.Name)

            $capabilitySummaryPath = Join-Path $caseOutputRoot "front-page.entry-capability.summary.json"
            Invoke-ExternalTool `
                -Executable $resolvedPythonExe `
                -ArgumentList @($validateScript, "--summary", $capabilitySummaryPath) `
                -FailureMessage ("front page entry capability validation failed for case '{0}'" -f $case.Name)

            $capabilitySummary = Load-JsonObject -Path $capabilitySummaryPath
            Assert-Condition `
                -Condition ([string]$capabilitySummary.entry_status.recommended_entry_mode -eq $case.ExpectedMode) `
                -Message ("case '{0}' expected mode '{1}' but got '{2}'" -f $case.Name, $case.ExpectedMode, $capabilitySummary.entry_status.recommended_entry_mode)
            Assert-Condition `
                -Condition ([string]$capabilitySummary.entry_status.entry_tier -eq $case.ExpectedTier) `
                -Message ("case '{0}' expected tier '{1}' but got '{2}'" -f $case.Name, $case.ExpectedTier, $capabilitySummary.entry_status.entry_tier)
            Assert-Condition `
                -Condition ([int]$capabilitySummary.entry_status.route_provenance_entry_count -eq [int]$case.ExpectedProvenanceCount) `
                -Message ("case '{0}' expected route provenance count '{1}' but got '{2}'" -f $case.Name, $case.ExpectedProvenanceCount, $capabilitySummary.entry_status.route_provenance_entry_count)
            Assert-OpeningReason `
                -OpeningReason $capabilitySummary.entry_status.opening_reason `
                -ExpectedKinds ([string[]]$case.ExpectedOpeningReasonKinds) `
                -ExpectedDriftChanged $case.ExpectedOpeningReasonDriftChanged `
                -ExpectedDriftVerdict $case.ExpectedOpeningReasonDriftVerdict `
                -CaseName $case.Name

            $availableCapabilities = @([string[]]$capabilitySummary.capability_summary.available_capability_ids)
            foreach ($capabilityId in @($case.RequiredCapabilities)) {
                Assert-Condition `
                    -Condition ($availableCapabilities -contains [string]$capabilityId) `
                    -Message ("case '{0}' is missing capability '{1}'" -f $case.Name, $capabilityId)
            }

            Write-Host (
                "[FRONT-PAGE-ENTRY-CAPABILITY-SMOKE] case={0} mode={1} tier={2} capabilities={3}" -f
                $case.Name,
                [string]$capabilitySummary.entry_status.recommended_entry_mode,
                [string]$capabilitySummary.entry_status.entry_tier,
                ($availableCapabilities -join ",")
            )
        }
    } finally {
        Remove-TemporaryFiles -Paths @($temporaryPlaceholderPaths) -RepoRootPath $repoRoot
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-CAPABILITY-SMOKE] output_root={0}" -f $outputRootPath)
