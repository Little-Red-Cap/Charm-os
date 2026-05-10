param(
    [string]$InputRoot = "cmake-build-system-compiler-front-page-entry-capability-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-landing-smoke",
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

function Ensure-PlaceholderEntryArtifacts {
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

    $placeholderSummary = "{`n  `"schema`": `"temporary.placeholder/v0`",`n  `"note`": `"Temporary runtime evidence root placeholder for front-page entry landing smoke.`"`n}`n"
    $placeholderReport = "# Temporary Runtime Evidence Placeholder`n"
    $placeholderCheck = "temporary runtime evidence placeholder`n"

    Ensure-TextFileIfMissing -Path ([string]$Entry.summary_path) -Content $placeholderSummary -CreatedPaths $CreatedPaths
    Ensure-TextFileIfMissing -Path ([string]$Entry.report_markdown_path) -Content $placeholderReport -CreatedPaths $CreatedPaths
    Ensure-TextFileIfMissing -Path ([string]$Entry.check_text_path) -Content $placeholderCheck -CreatedPaths $CreatedPaths
}

function Ensure-CapabilitySummaryPlaceholders {
    param(
        [string]$CapabilitySummaryPath,
        [System.Collections.Generic.List[string]]$CreatedPaths
    )

    $capabilitySummary = Load-JsonObject -Path $CapabilitySummaryPath
    $preferredEntries = $capabilitySummary.capability_summary.preferred_entries
    if ($preferredEntries -isnot [System.Management.Automation.PSCustomObject] -and $preferredEntries -isnot [hashtable]) {
        return
    }

    foreach ($property in $preferredEntries.PSObject.Properties) {
        Ensure-PlaceholderEntryArtifacts -Entry $property.Value -CreatedPaths $CreatedPaths
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

function Test-AllPathsExist {
    param(
        [string[]]$Paths
    )

    foreach ($path in @($Paths)) {
        if ([string]::IsNullOrWhiteSpace($path) -or -not (Test-Path $path)) {
            return $false
        }
    }

    return $true
}

function Test-CapabilityInputsReady {
    param(
        [string[]]$Paths
    )

    if (-not (Test-AllPathsExist -Paths $Paths)) {
        return $false
    }

    foreach ($path in @($Paths)) {
        try {
            $summary = Load-JsonObject -Path $path
        } catch {
            return $false
        }

        $openingReason = $summary.entry_status.opening_reason
        if ($openingReason -isnot [System.Management.Automation.PSCustomObject] -and $openingReason -isnot [hashtable]) {
            return $false
        }
    }

    return $true
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

$capabilitySmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_capability_smoke.ps1"
$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_landing.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_landing.py"
foreach ($requiredPath in @($capabilitySmokeScript, $exportScript, $validateScript)) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

$cases = @(
    [ordered]@{
        Name = "root-witness"
        SummaryPath = Join-Path $inputRootPath "root-witness\front-page.entry-capability.summary.json"
        ExpectedMode = "biography"
        ExpectedPrimary = "delivery_biography"
        ExpectedTabsPrefix = @("delivery_biography", "supporting_evidence", "runtime_session", "supporting_testimony")
        ExpectedProvenanceRoots = 0
        ExpectedPrimaryQueryKind = "default_overview"
        ExpectedPrimaryQueryScope = "report"
        ExpectedOpeningReasonKinds = @("delivery_biography")
        ExpectedOpeningReasonDriftChanged = $false
        ExpectedOpeningReasonDriftVerdict = ""
    },
    [ordered]@{
        Name = "root-world-compare"
        SummaryPath = Join-Path $inputRootPath "root-world-compare\front-page.entry-capability.summary.json"
        ExpectedMode = "compare"
        ExpectedPrimary = "counterfactual_verdict"
        ExpectedTabsPrefix = @("counterfactual_verdict", "delivery_biography", "supporting_evidence", "runtime_session")
        ExpectedProvenanceRoots = 0
        ExpectedPrimaryQueryKind = "default_overview"
        ExpectedPrimaryQueryScope = "artifact_root"
        ExpectedOpeningReasonKinds = @("counterfactual_verdict")
        ExpectedOpeningReasonDriftChanged = $false
        ExpectedOpeningReasonDriftVerdict = ""
    },
    [ordered]@{
        Name = "witness-ci-shelf"
        SummaryPath = Join-Path $inputRootPath "witness-ci-shelf\front-page.entry-capability.summary.json"
        ExpectedMode = "review"
        ExpectedPrimary = "grouped_review"
        ExpectedTabsPrefix = @("grouped_review", "shelf_compare", "candidate_shelf", "baseline_shelf")
        ExpectedProvenanceRoots = 0
        ExpectedPrimaryQueryKind = "default_overview"
        ExpectedPrimaryQueryScope = "artifact_root"
        ExpectedOpeningReasonKinds = @("world_shelf_review", "world_shelf_review_drift", "grouped_review")
        ExpectedOpeningReasonDriftChanged = $null
        ExpectedOpeningReasonDriftVerdict = $null
    },
    [ordered]@{
        Name = "review-provenance"
        SummaryPath = Join-Path $inputRootPath "review-provenance\front-page.entry-capability.summary.json"
        ExpectedMode = "review"
        ExpectedPrimary = "grouped_review"
        ExpectedTabsPrefix = @("grouped_review", "shelf_compare", "candidate_shelf", "baseline_shelf")
        ExpectedProvenanceRoots = 3
        ExpectedPrimaryQueryKind = "default_overview"
        ExpectedPrimaryQueryScope = "artifact_root"
        ExpectedOpeningReasonKinds = @("world_shelf_review", "world_shelf_review_drift", "grouped_review")
        ExpectedOpeningReasonDriftChanged = $null
        ExpectedOpeningReasonDriftVerdict = $null
    }
)

Push-Location $repoRoot
try {
    $bootstrapInputs = @($cases | ForEach-Object { [string]$_.SummaryPath })
    if (Test-CapabilityInputsReady -Paths $bootstrapInputs) {
        Write-Host "[FRONT-PAGE-ENTRY-LANDING-SMOKE] bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable "powershell.exe" `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $capabilitySmokeScript,
                "-OutputRoot",
                $inputRootPath
            ) `
            -FailureMessage "front page entry capability smoke bootstrap failed"
    }

    $temporaryPlaceholderPaths = [System.Collections.Generic.List[string]]::new()
    try {
        foreach ($case in $cases) {
            if (-not (Test-Path $case.SummaryPath)) {
                throw "entry capability summary not found for case '$($case.Name)': $($case.SummaryPath)"
            }

            Ensure-CapabilitySummaryPlaceholders -CapabilitySummaryPath $case.SummaryPath -CreatedPaths $temporaryPlaceholderPaths

            $capabilitySummary = Load-JsonObject -Path $case.SummaryPath
            $caseOutputRoot = Join-Path $outputRootPath $case.Name
            Invoke-ExternalTool `
                -Executable $resolvedPythonExe `
                -ArgumentList @($exportScript, "--summary", $case.SummaryPath, "--output-root", $caseOutputRoot) `
                -FailureMessage ("front page entry landing export failed for case '{0}'" -f $case.Name)

            $landingSummaryPath = Join-Path $caseOutputRoot "front-page.entry-landing.summary.json"
            Assert-Condition `
                -Condition (Test-Path $landingSummaryPath) `
                -Message ("front page entry landing summary missing for case '{0}'" -f $case.Name)

            Invoke-ExternalTool `
                -Executable $resolvedPythonExe `
                -ArgumentList @($validateScript, "--summary", $landingSummaryPath) `
                -FailureMessage ("front page entry landing validation failed for case '{0}'" -f $case.Name)

            $landingSummary = Load-JsonObject -Path $landingSummaryPath
            Assert-Condition `
                -Condition ([string]$landingSummary.landing_status.recommended_entry_mode -eq $case.ExpectedMode) `
                -Message ("case '{0}' expected mode '{1}' but got '{2}'" -f $case.Name, $case.ExpectedMode, $landingSummary.landing_status.recommended_entry_mode)
            Assert-Condition `
                -Condition ([string]$landingSummary.landing_status.primary_tab_id -eq $case.ExpectedPrimary) `
                -Message ("case '{0}' expected primary tab '{1}' but got '{2}'" -f $case.Name, $case.ExpectedPrimary, $landingSummary.landing_status.primary_tab_id)
            Assert-Condition `
                -Condition ([int]$landingSummary.landing_status.provenance_root_count -eq [int]$case.ExpectedProvenanceRoots) `
                -Message ("case '{0}' expected provenance roots '{1}' but got '{2}'" -f $case.Name, $case.ExpectedProvenanceRoots, $landingSummary.landing_status.provenance_root_count)
            Assert-Condition `
                -Condition ([string]$landingSummary.query_hints.primary_query.query_kind -eq $case.ExpectedPrimaryQueryKind) `
                -Message ("case '{0}' expected primary query kind '{1}' but got '{2}'" -f $case.Name, $case.ExpectedPrimaryQueryKind, $landingSummary.query_hints.primary_query.query_kind)
            Assert-Condition `
                -Condition ([string]$landingSummary.query_hints.primary_query.scope -eq $case.ExpectedPrimaryQueryScope) `
                -Message ("case '{0}' expected primary query scope '{1}' but got '{2}'" -f $case.Name, $case.ExpectedPrimaryQueryScope, $landingSummary.query_hints.primary_query.scope)
            Assert-OpeningReason `
                -OpeningReason $landingSummary.landing_status.opening_reason `
                -ExpectedKinds ([string[]]$case.ExpectedOpeningReasonKinds) `
                -ExpectedDriftChanged $case.ExpectedOpeningReasonDriftChanged `
                -ExpectedDriftVerdict $case.ExpectedOpeningReasonDriftVerdict `
                -CaseName $case.Name
            Assert-Condition `
                -Condition (($landingSummary.landing_status.opening_reason | ConvertTo-Json -Depth 8 -Compress) -eq ($capabilitySummary.entry_status.opening_reason | ConvertTo-Json -Depth 8 -Compress)) `
                -Message ("case '{0}' expected landing opening_reason to pass through capability opening_reason" -f $case.Name)
            Assert-Condition `
                -Condition (@($landingSummary.query_hints.tab_queries).Count -eq @($landingSummary.landing_tabs).Count) `
                -Message ("case '{0}' query_hints.tab_queries must match landing tab count" -f $case.Name)

            $availableTabIds = @([string[]]$landingSummary.landing_status.available_tab_ids)
            $hasRuntimeSessionTab = $availableTabIds -contains "runtime_session"
            Assert-Condition `
                -Condition ([bool]$landingSummary.landing_status.direct_runtime_session_available -eq [bool]$hasRuntimeSessionTab) `
                -Message ("case '{0}' expected direct_runtime_session_available to match runtime_session tab presence" -f $case.Name)
            for ($i = 0; $i -lt $case.ExpectedTabsPrefix.Count; $i++) {
                $expectedTabId = [string]$case.ExpectedTabsPrefix[$i]
                Assert-Condition `
                    -Condition ($availableTabIds.Count -gt $i -and [string]$availableTabIds[$i] -eq $expectedTabId) `
                    -Message ("case '{0}' expected tab index {1} to be '{2}' but got '{3}'" -f $case.Name, $i, $expectedTabId, ($(if ($availableTabIds.Count -gt $i) { [string]$availableTabIds[$i] } else { "" })))
            }

            Write-Host (
                "[FRONT-PAGE-ENTRY-LANDING-SMOKE] case={0} mode={1} primary={2} query={3}/{4} tabs={5} provenance_roots={6}" -f
                $case.Name,
                [string]$landingSummary.landing_status.recommended_entry_mode,
                [string]$landingSummary.landing_status.primary_tab_id,
                [string]$landingSummary.query_hints.primary_query.query_kind,
                [string]$landingSummary.query_hints.primary_query.scope,
                ($availableTabIds -join ","),
                [int]$landingSummary.landing_status.provenance_root_count
            )
        }
    } finally {
        Remove-TemporaryFiles -Paths @($temporaryPlaceholderPaths) -RepoRootPath $repoRoot
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-LANDING-SMOKE] output_root={0}" -f $outputRootPath)
