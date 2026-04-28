param(
    [string]$CanonicalWorld = "Examples/kernel/canonical_worlds/minimal_kernel_runtime.world.json",
    [string]$RuntimeEvidenceSummary = "",
    [string]$RuntimeEvidenceOutputRoot = "",
    [string]$RuntimeEvidenceValidationLogPath = "",
    [string]$BaselineRuntimeEvidenceSummary = "",
    [string]$BaselineRuntimeEvidenceOutputRoot = "",
    [string]$ArtifactRoot = "",
    [string[]]$ArtifactReport = @(),
    [string[]]$Case = @(),
    [string]$BaselineArtifactRoot = "",
    [string[]]$BaselineArtifactReport = @(),
    [string[]]$BaselineCase = @(),
    [string]$BaselineWitnessSummary = "",
    [string]$BaselineWitnessOutputRoot = "",
    [string]$OutputRoot = "",
    [string]$SummaryPath = "",
    [string]$ReportMarkdownPath = "",
    [string]$CheckTextPath = "",
    [string]$ValidationLogPath = "",
    [string]$BiographySummaryPath = "",
    [string]$BiographyReportMarkdownPath = "",
    [string]$BiographyCheckTextPath = "",
    [string]$BiographyValidationLogPath = "",
    [string]$FrontPageRouteOutputRoot = "",
    [string]$FrontPageRouteSummaryPath = "",
    [string]$FrontPageRouteReportMarkdownPath = "",
    [string]$FrontPageRouteCheckTextPath = "",
    [string]$FrontPageRouteValidationLogPath = "",
    [string]$WorldCompareOutputRoot = "",
    [string]$WorldCompareSummaryPath = "",
    [string]$WorldCompareReportMarkdownPath = "",
    [string]$WorldCompareCheckTextPath = "",
    [string]$WorldCompareValidationLogPath = "",
    [string]$CMakeExe = "cmake",
    [string]$Generator = "Ninja",
    [string]$QemuExe = "qemu-system-arm",
    [string]$PythonExe = "",
    [int]$HostJobs = 0,
    [int]$QemuBuildJobs = 1,
    [int]$QemuTimeoutSec = 30,
    [int]$QemuTailLines = 40,
    [switch]$Clean,
    [switch]$SkipWorldCompare,
    [string[]]$HostExamples
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
        Remove-Item -Recurse -Force $Path
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

function Get-OutputPath {
    param(
        [string]$ExplicitPath,
        [string]$OutputRootPath,
        [string]$DefaultFileName
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        return Resolve-FullPath -Path $ExplicitPath
    }

    return Join-Path $OutputRootPath $DefaultFileName
}

function Add-ScriptArgument {
    param(
        [System.Collections.Generic.List[string]]$Arguments,
        [string]$Name,
        [string]$Value
    )

    $Arguments.Add($Name) | Out-Null
    $Arguments.Add($Value) | Out-Null
}

function Add-StringArrayScriptArgument {
    param(
        [System.Collections.Generic.List[string]]$Arguments,
        [string]$Name,
        [string[]]$Values
    )

    if (@($Values).Count -eq 0) {
        return
    }

    $filteredValues = @(
        @($Values) |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [string]$_ }
    )

    if ($filteredValues.Count -eq 0) {
        return
    }

    $Arguments.Add($Name) | Out-Null
    foreach ($value in $filteredValues) {
        $Arguments.Add($value) | Out-Null
    }
}

function Format-Number {
    param(
        $Value
    )

    return [Convert]::ToString($Value, [System.Globalization.CultureInfo]::InvariantCulture)
}

function Invoke-PowerShellFile {
    param(
        [string]$PowerShellExe,
        [string]$ScriptPath,
        [string[]]$ArgumentList,
        [string]$LogPath,
        [string]$FailureMessage
    )

    Ensure-ParentDirectory -Path $LogPath
    Write-Host ("==> {0}" -f [System.IO.Path]::GetFileName($ScriptPath))

    $commandArgs = [System.Collections.Generic.List[string]]::new()
    $commandArgs.Add("-NoProfile") | Out-Null
    $commandArgs.Add("-ExecutionPolicy") | Out-Null
    $commandArgs.Add("Bypass") | Out-Null
    $commandArgs.Add("-File") | Out-Null
    $commandArgs.Add($ScriptPath) | Out-Null
    foreach ($entry in @($ArgumentList)) {
        $commandArgs.Add([string]$entry) | Out-Null
    }

    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & $PowerShellExe @commandArgs 2>&1 | Tee-Object -FilePath $LogPath | Out-Host
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    if ($exitCode -ne 0) {
        throw ("{0} (exit code {1})" -f $FailureMessage, $exitCode)
    }

    return $exitCode
}

function Invoke-ExternalTool {
    param(
        [string]$Executable,
        [string[]]$ArgumentList,
        [string]$LogPath,
        [string]$FailureMessage
    )

    Ensure-ParentDirectory -Path $LogPath
    Write-Host ("==> {0}" -f [System.IO.Path]::GetFileName($Executable))

    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & $Executable @ArgumentList 2>&1 | Tee-Object -FilePath $LogPath | Out-Host
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    if ($exitCode -ne 0) {
        throw ("{0} (exit code {1})" -f $FailureMessage, $exitCode)
    }

    return $exitCode
}

function Load-JsonObject {
    param(
        [string]$Path
    )

    return (Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json)
}

function Get-StringValues {
    param(
        $Values
    )

    return @(
        @($Values) |
            Where-Object { $null -ne $_ -and -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [string]$_ }
    )
}

function Append-Utf8Text {
    param(
        [string]$Path,
        [string]$Text
    )

    Ensure-ParentDirectory -Path $Path
    $existing = if (Test-Path $Path) {
        Get-Content -LiteralPath $Path -Raw -Encoding utf8
    } else {
        ""
    }

    Set-Content -LiteralPath $Path -Encoding utf8 -Value ($existing + $Text)
}

function Export-FrontPageRouteArtifacts {
    param(
        [string]$PythonExe,
        [string]$ExportScript,
        [string]$ValidateScript,
        [string]$InputSummaryPath,
        [string]$OutputRootPath,
        [string]$SummaryPath,
        [string]$ReportMarkdownPath,
        [string]$CheckTextPath,
        [string]$ValidationLogPath,
        [string]$FailurePrefix
    )

    $exportLogPath = Join-Path $OutputRootPath "front-page.route.export.log"
    $null = Invoke-ExternalTool `
        -Executable $PythonExe `
        -ArgumentList @(
            $ExportScript,
            "--summary",
            $InputSummaryPath,
            "--output-root",
            $OutputRootPath,
            "--route-summary",
            $SummaryPath,
            "--report-markdown",
            $ReportMarkdownPath,
            "--check-text",
            $CheckTextPath
        ) `
        -LogPath $exportLogPath `
        -FailureMessage ("{0} front page route export failed" -f $FailurePrefix)

    $null = Invoke-ExternalTool `
        -Executable $PythonExe `
        -ArgumentList @(
            $ValidateScript,
            "--summary",
            $SummaryPath
        ) `
        -LogPath $ValidationLogPath `
        -FailureMessage ("{0} front page route validation failed" -f $FailurePrefix)
}

function Append-WorldCompareOverlay {
    param(
        [string]$ReportPath,
        [string]$CheckTextPath,
        [string]$WorldCompareSummaryPath
    )

    if (-not (Test-Path $WorldCompareSummaryPath)) {
        throw "world compare summary not found: $WorldCompareSummaryPath"
    }

    $summary = Load-JsonObject -Path $WorldCompareSummaryPath
    $bundleStatus = $summary.bundle_status
    $witnessSummary = $summary.witness_summary
    $contractDrift = $summary.contract_drift
    $collapseSurface = $summary.collapse_surface
    $artifactContext = $summary.artifact_context
    $questions = $summary.questions

    $reportBuilder = [System.Text.StringBuilder]::new()
    [void]$reportBuilder.AppendLine("")
    [void]$reportBuilder.AppendLine("## World Compare")
    [void]$reportBuilder.AppendLine(("- World verdict: {0}" -f [string]$summary.world_verdict))
    [void]$reportBuilder.AppendLine(("- Baseline state: {0} ({1})" -f [string]$bundleStatus.baseline_state, [string]$bundleStatus.baseline_result))
    [void]$reportBuilder.AppendLine(("- Candidate state: {0} ({1})" -f [string]$bundleStatus.candidate_state, [string]$bundleStatus.candidate_result))
    [void]$reportBuilder.AppendLine(("- Witness changes: changed={0} regressions={1} improvements={2} required_regressions={3}" -f [int]$witnessSummary.changed_entry_count, [int]$witnessSummary.regression_count, [int]$witnessSummary.improvement_count, [int]$witnessSummary.required_regression_count))
    [void]$reportBuilder.AppendLine(("- Contract drift: changed={0} missing_added={1} missing_removed={2}" -f [bool]$contractDrift.changed, @($contractDrift.missing_ref_changes.added).Count, @($contractDrift.missing_ref_changes.removed).Count))
    [void]$reportBuilder.AppendLine(("- Compare summary: {0}" -f $WorldCompareSummaryPath))
    [void]$reportBuilder.AppendLine(("- Compare report: {0}" -f [string]$artifactContext.report_markdown_path))
    [void]$reportBuilder.AppendLine(("- Compare check: {0}" -f [string]$artifactContext.check_text_path))

    [void]$reportBuilder.AppendLine("")
    [void]$reportBuilder.AppendLine("## Collapse Surface")
    if ([bool]$collapseSurface.changed) {
        $regressedWitnesses = Get-StringValues -Values $collapseSurface.regressed_witnesses
        $requiredRegressedWitnesses = Get-StringValues -Values $collapseSurface.required_regressed_witnesses
        $affectedLayers = Get-StringValues -Values $collapseSurface.affected_layers
        $affectedFocus = Get-StringValues -Values $collapseSurface.affected_focus
        $missingContractRefs = Get-StringValues -Values $collapseSurface.added_missing_contract_refs
        $narratives = Get-StringValues -Values $collapseSurface.narratives

        if ($regressedWitnesses.Count -gt 0) {
            [void]$reportBuilder.AppendLine(("- Regressed witnesses: {0}" -f ($regressedWitnesses -join ", ")))
        }
        if ($requiredRegressedWitnesses.Count -gt 0) {
            [void]$reportBuilder.AppendLine(("- Required regressions: {0}" -f ($requiredRegressedWitnesses -join ", ")))
        }
        if ($affectedLayers.Count -gt 0) {
            [void]$reportBuilder.AppendLine(("- Affected layers: {0}" -f ($affectedLayers -join ", ")))
        }
        if ($affectedFocus.Count -gt 0) {
            [void]$reportBuilder.AppendLine(("- Affected focus: {0}" -f ($affectedFocus -join ", ")))
        }
        if ($missingContractRefs.Count -gt 0) {
            [void]$reportBuilder.AppendLine(("- Missing contract refs added: {0}" -f ($missingContractRefs -join ", ")))
        }
        foreach ($narrative in $narratives) {
            [void]$reportBuilder.AppendLine(("- {0}" -f $narrative))
        }
    } else {
        [void]$reportBuilder.AppendLine("- No collapse-surface drift detected")
    }

    [void]$reportBuilder.AppendLine("")
    [void]$reportBuilder.AppendLine("## Next Questions")
    $nextQuestions = Get-StringValues -Values $questions.next_questions
    if ($nextQuestions.Count -eq 0) {
        [void]$reportBuilder.AppendLine("- No next questions were emitted")
    } else {
        foreach ($question in $nextQuestions) {
            [void]$reportBuilder.AppendLine(("- {0}" -f $question))
        }
    }

    Append-Utf8Text -Path $ReportPath -Text $reportBuilder.ToString()

    $checkBuilder = [System.Text.StringBuilder]::new()
    [void]$checkBuilder.AppendLine(("world_compare_summary: {0}" -f $WorldCompareSummaryPath))
    [void]$checkBuilder.AppendLine(("world_compare_verdict: {0}" -f [string]$summary.world_verdict))
    [void]$checkBuilder.AppendLine(("world_compare_baseline_state: {0}" -f [string]$bundleStatus.baseline_state))
    [void]$checkBuilder.AppendLine(("world_compare_candidate_state: {0}" -f [string]$bundleStatus.candidate_state))
    [void]$checkBuilder.AppendLine(("world_compare_witness_changes: changed={0} regressions={1} improvements={2} required_regressions={3}" -f [int]$witnessSummary.changed_entry_count, [int]$witnessSummary.regression_count, [int]$witnessSummary.improvement_count, [int]$witnessSummary.required_regression_count))
    [void]$checkBuilder.AppendLine(("world_compare_contract_drift: changed={0} missing_added={1} missing_removed={2}" -f [bool]$contractDrift.changed, @($contractDrift.missing_ref_changes.added).Count, @($contractDrift.missing_ref_changes.removed).Count))
    $affectedLayersSummary = (Get-StringValues -Values $collapseSurface.affected_layers) -join ","
    [void]$checkBuilder.AppendLine(("world_compare_collapse_surface: changed={0} affected_layers={1}" -f [bool]$collapseSurface.changed, $affectedLayersSummary))
    [void]$checkBuilder.AppendLine(("world_compare_next_questions: {0}" -f (Get-StringValues -Values $questions.next_questions).Count))

    Append-Utf8Text -Path $CheckTextPath -Text $checkBuilder.ToString()
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$resolvedCanonicalWorld = Resolve-FullPath -Path $CanonicalWorld
$resolvedOutputRoot = if ([string]::IsNullOrWhiteSpace($OutputRoot)) { "out/minimal-kernel-runtime-system-compiler-witness" } else { $OutputRoot }
$outputRootPath = Resolve-FullPath -Path $resolvedOutputRoot

if ($Clean) {
    Remove-PathIfExists -Path $outputRootPath
}
Ensure-Directory -Path $outputRootPath

$summaryPathResolved = Get-OutputPath -ExplicitPath $SummaryPath -OutputRootPath $outputRootPath -DefaultFileName "summary.json"
$reportMarkdownPathResolved = Get-OutputPath -ExplicitPath $ReportMarkdownPath -OutputRootPath $outputRootPath -DefaultFileName "report.md"
$checkTextPathResolved = Get-OutputPath -ExplicitPath $CheckTextPath -OutputRootPath $outputRootPath -DefaultFileName "check.txt"
$validationLogPathResolved = Get-OutputPath -ExplicitPath $ValidationLogPath -OutputRootPath $outputRootPath -DefaultFileName "validate.log"
$biographySummaryPathResolved = Get-OutputPath -ExplicitPath $BiographySummaryPath -OutputRootPath $outputRootPath -DefaultFileName "biography.summary.json"
$biographyReportMarkdownPathResolved = Get-OutputPath -ExplicitPath $BiographyReportMarkdownPath -OutputRootPath $outputRootPath -DefaultFileName "biography.report.md"
$biographyCheckTextPathResolved = Get-OutputPath -ExplicitPath $BiographyCheckTextPath -OutputRootPath $outputRootPath -DefaultFileName "biography.check.txt"
$biographyValidationLogPathResolved = Get-OutputPath -ExplicitPath $BiographyValidationLogPath -OutputRootPath $outputRootPath -DefaultFileName "biography.validate.log"
$resolvedFrontPageRouteOutputRoot = if ([string]::IsNullOrWhiteSpace($FrontPageRouteOutputRoot)) {
    Join-Path $outputRootPath "front_page_route"
} else {
    Resolve-FullPath -Path $FrontPageRouteOutputRoot
}
$frontPageRouteSummaryPathResolved = Get-OutputPath -ExplicitPath $FrontPageRouteSummaryPath -OutputRootPath $resolvedFrontPageRouteOutputRoot -DefaultFileName "front-page.route.summary.json"
$frontPageRouteReportMarkdownPathResolved = Get-OutputPath -ExplicitPath $FrontPageRouteReportMarkdownPath -OutputRootPath $resolvedFrontPageRouteOutputRoot -DefaultFileName "front-page.route.report.md"
$frontPageRouteCheckTextPathResolved = Get-OutputPath -ExplicitPath $FrontPageRouteCheckTextPath -OutputRootPath $resolvedFrontPageRouteOutputRoot -DefaultFileName "front-page.route.check.txt"
$frontPageRouteValidationLogPathResolved = Get-OutputPath -ExplicitPath $FrontPageRouteValidationLogPath -OutputRootPath $resolvedFrontPageRouteOutputRoot -DefaultFileName "front-page.route.validate.log"
$runtimeEvidenceValidationLogPathResolved = Get-OutputPath -ExplicitPath $RuntimeEvidenceValidationLogPath -OutputRootPath $outputRootPath -DefaultFileName "runtime_evidence_validate.log"
$runtimeBundleLogPathResolved = Join-Path $outputRootPath "runtime_evidence_bundle.log"
$exportLogPathResolved = Join-Path $outputRootPath "export.log"
$biographyExportLogPathResolved = Join-Path $outputRootPath "biography.export.log"
$baselineExportLogPathResolved = Join-Path $outputRootPath "baseline.export.log"
$baselineRuntimeEvidenceValidationLogPathResolved = Join-Path $outputRootPath "baseline.runtime_evidence_validate.log"
$baselineValidationLogPathResolved = Join-Path $outputRootPath "baseline.validate.log"
$worldCompareLogPathResolved = Join-Path $outputRootPath "world_compare.log"

$resolvedRuntimeEvidenceOutputRoot = if ([string]::IsNullOrWhiteSpace($RuntimeEvidenceOutputRoot)) {
    Join-Path $outputRootPath "runtime_evidence"
} else {
    Resolve-FullPath -Path $RuntimeEvidenceOutputRoot
}

$resolvedRuntimeEvidenceSummary = if ([string]::IsNullOrWhiteSpace($RuntimeEvidenceSummary)) {
    Join-Path $resolvedRuntimeEvidenceOutputRoot "summary.json"
} else {
    Resolve-FullPath -Path $RuntimeEvidenceSummary
}

$shouldRunWorldCompare = (-not $SkipWorldCompare) -and (
    -not [string]::IsNullOrWhiteSpace($BaselineWitnessSummary) -or
    -not [string]::IsNullOrWhiteSpace($BaselineRuntimeEvidenceSummary) -or
    -not [string]::IsNullOrWhiteSpace($BaselineRuntimeEvidenceOutputRoot)
)

$resolvedBaselineRuntimeEvidenceOutputRoot = if ([string]::IsNullOrWhiteSpace($BaselineRuntimeEvidenceOutputRoot)) {
    Join-Path $outputRootPath "baseline_runtime_evidence"
} else {
    Resolve-FullPath -Path $BaselineRuntimeEvidenceOutputRoot
}

$resolvedBaselineRuntimeEvidenceSummary = if ([string]::IsNullOrWhiteSpace($BaselineRuntimeEvidenceSummary)) {
    if ($shouldRunWorldCompare -and -not [string]::IsNullOrWhiteSpace($BaselineRuntimeEvidenceOutputRoot)) {
        Join-Path $resolvedBaselineRuntimeEvidenceOutputRoot "summary.json"
    } else {
        ""
    }
} else {
    Resolve-FullPath -Path $BaselineRuntimeEvidenceSummary
}

$resolvedBaselineWitnessOutputRoot = if ([string]::IsNullOrWhiteSpace($BaselineWitnessOutputRoot)) {
    Join-Path $outputRootPath "baseline_witness"
} else {
    Resolve-FullPath -Path $BaselineWitnessOutputRoot
}

$resolvedBaselineWitnessSummary = if ([string]::IsNullOrWhiteSpace($BaselineWitnessSummary)) {
    if ($shouldRunWorldCompare) {
        Join-Path $resolvedBaselineWitnessOutputRoot "summary.json"
    } else {
        ""
    }
} else {
    Resolve-FullPath -Path $BaselineWitnessSummary
}

$baselineWitnessReportMarkdownPathResolved = if ($shouldRunWorldCompare) {
    Join-Path $resolvedBaselineWitnessOutputRoot "report.md"
} else {
    ""
}

$baselineWitnessCheckTextPathResolved = if ($shouldRunWorldCompare) {
    Join-Path $resolvedBaselineWitnessOutputRoot "check.txt"
} else {
    ""
}

$resolvedWorldCompareOutputRoot = if ([string]::IsNullOrWhiteSpace($WorldCompareOutputRoot)) {
    Join-Path $outputRootPath "world_compare"
} else {
    Resolve-FullPath -Path $WorldCompareOutputRoot
}

$worldCompareSummaryPathResolved = Get-OutputPath -ExplicitPath $WorldCompareSummaryPath -OutputRootPath $resolvedWorldCompareOutputRoot -DefaultFileName "summary.json"
$worldCompareReportMarkdownPathResolved = Get-OutputPath -ExplicitPath $WorldCompareReportMarkdownPath -OutputRootPath $resolvedWorldCompareOutputRoot -DefaultFileName "report.md"
$worldCompareCheckTextPathResolved = Get-OutputPath -ExplicitPath $WorldCompareCheckTextPath -OutputRootPath $resolvedWorldCompareOutputRoot -DefaultFileName "check.txt"
$worldCompareValidationLogPathResolved = Get-OutputPath -ExplicitPath $WorldCompareValidationLogPath -OutputRootPath $resolvedWorldCompareOutputRoot -DefaultFileName "validate.log"

$runtimeEvidenceScript = Join-Path $PSScriptRoot "minimal_kernel_runtime_evidence_bundle.ps1"
$validateRuntimeEvidenceScript = Join-Path $PSScriptRoot "validate_minimal_kernel_runtime_evidence.py"
$exportWitnessScript = Join-Path $PSScriptRoot "export_system_compiler_witness_bundle.ps1"
$validateWitnessScript = Join-Path $PSScriptRoot "validate_system_compiler_witness_bundle.py"
$updateWitnessFrontPageScript = Join-Path $PSScriptRoot "update_system_compiler_witness_bundle_front_page.ps1"
$exportBiographyScript = Join-Path $PSScriptRoot "export_system_compiler_biography.py"
$validateBiographyScript = Join-Path $PSScriptRoot "validate_system_compiler_biography.py"
$exportFrontPageRouteScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_route.py"
$validateFrontPageRouteScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_route.py"
$worldCompareScript = Join-Path $PSScriptRoot "compare_system_compiler_world.py"
$validateWorldCompareScript = Join-Path $PSScriptRoot "validate_system_compiler_world_compare.py"
$runtimeEvidenceSupportsSkipWitnessBundle = $false

$requiredPaths = [System.Collections.Generic.List[string]]::new()
$requiredPaths.Add($runtimeEvidenceScript) | Out-Null
$requiredPaths.Add($validateRuntimeEvidenceScript) | Out-Null
$requiredPaths.Add($exportWitnessScript) | Out-Null
$requiredPaths.Add($validateWitnessScript) | Out-Null
$requiredPaths.Add($updateWitnessFrontPageScript) | Out-Null
$requiredPaths.Add($exportBiographyScript) | Out-Null
$requiredPaths.Add($validateBiographyScript) | Out-Null
$requiredPaths.Add($exportFrontPageRouteScript) | Out-Null
$requiredPaths.Add($validateFrontPageRouteScript) | Out-Null
$requiredPaths.Add($resolvedCanonicalWorld) | Out-Null
if ($shouldRunWorldCompare) {
    $requiredPaths.Add($worldCompareScript) | Out-Null
    $requiredPaths.Add($validateWorldCompareScript) | Out-Null
}

foreach ($requiredPath in $requiredPaths) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

$runtimeEvidenceSupportsSkipWitnessBundle = [bool](
    Select-String `
        -Path $runtimeEvidenceScript `
        -SimpleMatch '$SkipWitnessBundle' `
        -Quiet
)

$powerShellExe = Resolve-ToolPath -Candidates @("powershell.exe", "pwsh.exe", "powershell", "pwsh")
$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}

Push-Location $repoRoot
try {
    if ([string]::IsNullOrWhiteSpace($RuntimeEvidenceSummary)) {
        $runtimeBundleArgs = [System.Collections.Generic.List[string]]::new()
        Add-ScriptArgument -Arguments $runtimeBundleArgs -Name "-OutputRoot" -Value $resolvedRuntimeEvidenceOutputRoot
        Add-ScriptArgument -Arguments $runtimeBundleArgs -Name "-SummaryPath" -Value $resolvedRuntimeEvidenceSummary
        Add-ScriptArgument -Arguments $runtimeBundleArgs -Name "-CMakeExe" -Value $CMakeExe
        Add-ScriptArgument -Arguments $runtimeBundleArgs -Name "-Generator" -Value $Generator
        Add-ScriptArgument -Arguments $runtimeBundleArgs -Name "-QemuExe" -Value $QemuExe
        Add-ScriptArgument -Arguments $runtimeBundleArgs -Name "-HostJobs" -Value (Format-Number -Value $HostJobs)
        Add-ScriptArgument -Arguments $runtimeBundleArgs -Name "-QemuBuildJobs" -Value (Format-Number -Value $QemuBuildJobs)
        Add-ScriptArgument -Arguments $runtimeBundleArgs -Name "-QemuTimeoutSec" -Value (Format-Number -Value $QemuTimeoutSec)
        Add-ScriptArgument -Arguments $runtimeBundleArgs -Name "-QemuTailLines" -Value (Format-Number -Value $QemuTailLines)
        Add-StringArrayScriptArgument -Arguments $runtimeBundleArgs -Name "-HostExamples" -Values $HostExamples
        if ($runtimeEvidenceSupportsSkipWitnessBundle) {
            $runtimeBundleArgs.Add("-SkipWitnessBundle") | Out-Null
        }
        if ($Clean) {
            $runtimeBundleArgs.Add("-Clean") | Out-Null
        }

        $null = Invoke-PowerShellFile `
            -PowerShellExe $powerShellExe `
            -ScriptPath $runtimeEvidenceScript `
            -ArgumentList $runtimeBundleArgs.ToArray() `
            -LogPath $runtimeBundleLogPathResolved `
            -FailureMessage "minimal kernel runtime evidence bundle failed"
    }

    if (-not (Test-Path $resolvedRuntimeEvidenceSummary)) {
        throw "runtime evidence summary not found: $resolvedRuntimeEvidenceSummary"
    }

    $validateRuntimeEvidenceArgs = @(
        $validateRuntimeEvidenceScript,
        "--summary",
        $resolvedRuntimeEvidenceSummary
    )
    $null = Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList $validateRuntimeEvidenceArgs `
        -LogPath $runtimeEvidenceValidationLogPathResolved `
        -FailureMessage "runtime evidence validation failed"

    $exportArgs = [System.Collections.Generic.List[string]]::new()
    Add-ScriptArgument -Arguments $exportArgs -Name "-CanonicalWorld" -Value $resolvedCanonicalWorld
    Add-ScriptArgument -Arguments $exportArgs -Name "-RuntimeEvidenceSummary" -Value $resolvedRuntimeEvidenceSummary
    Add-ScriptArgument -Arguments $exportArgs -Name "-OutputRoot" -Value $outputRootPath
    Add-ScriptArgument -Arguments $exportArgs -Name "-OutputPath" -Value $summaryPathResolved
    Add-ScriptArgument -Arguments $exportArgs -Name "-ReportMarkdownPath" -Value $reportMarkdownPathResolved
    Add-ScriptArgument -Arguments $exportArgs -Name "-CheckTextPath" -Value $checkTextPathResolved
    if (-not [string]::IsNullOrWhiteSpace($ArtifactRoot)) {
        Add-ScriptArgument -Arguments $exportArgs -Name "-ArtifactRoot" -Value $ArtifactRoot
    }
    Add-StringArrayScriptArgument -Arguments $exportArgs -Name "-ArtifactReport" -Values $ArtifactReport
    Add-StringArrayScriptArgument -Arguments $exportArgs -Name "-Case" -Values $Case

    $null = Invoke-PowerShellFile `
        -PowerShellExe $powerShellExe `
        -ScriptPath $exportWitnessScript `
        -ArgumentList $exportArgs.ToArray() `
        -LogPath $exportLogPathResolved `
        -FailureMessage "system compiler witness export failed"

    if (-not (Test-Path $summaryPathResolved)) {
        throw "system compiler witness summary not found: $summaryPathResolved"
    }

    $validateArgs = @(
        $validateWitnessScript,
        "--bundle-root",
        $outputRootPath
    )
    $null = Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList $validateArgs `
        -LogPath $validationLogPathResolved `
        -FailureMessage "system compiler witness validation failed"

    if ($shouldRunWorldCompare) {
        if ([string]::IsNullOrWhiteSpace($BaselineWitnessSummary)) {
            if ([string]::IsNullOrWhiteSpace($resolvedBaselineRuntimeEvidenceSummary)) {
                throw "baseline runtime evidence summary is required when BaselineWitnessSummary is not provided"
            }
            if (-not (Test-Path $resolvedBaselineRuntimeEvidenceSummary)) {
                throw "baseline runtime evidence summary not found: $resolvedBaselineRuntimeEvidenceSummary"
            }

            $baselineValidateRuntimeEvidenceArgs = @(
                $validateRuntimeEvidenceScript,
                "--summary",
                $resolvedBaselineRuntimeEvidenceSummary
            )
            $null = Invoke-ExternalTool `
                -Executable $resolvedPythonExe `
                -ArgumentList $baselineValidateRuntimeEvidenceArgs `
                -LogPath $baselineRuntimeEvidenceValidationLogPathResolved `
                -FailureMessage "baseline runtime evidence validation failed"

            $baselineExportArgs = [System.Collections.Generic.List[string]]::new()
            Add-ScriptArgument -Arguments $baselineExportArgs -Name "-CanonicalWorld" -Value $resolvedCanonicalWorld
            Add-ScriptArgument -Arguments $baselineExportArgs -Name "-RuntimeEvidenceSummary" -Value $resolvedBaselineRuntimeEvidenceSummary
            Add-ScriptArgument -Arguments $baselineExportArgs -Name "-OutputRoot" -Value $resolvedBaselineWitnessOutputRoot
            Add-ScriptArgument -Arguments $baselineExportArgs -Name "-OutputPath" -Value $resolvedBaselineWitnessSummary
            Add-ScriptArgument -Arguments $baselineExportArgs -Name "-ReportMarkdownPath" -Value $baselineWitnessReportMarkdownPathResolved
            Add-ScriptArgument -Arguments $baselineExportArgs -Name "-CheckTextPath" -Value $baselineWitnessCheckTextPathResolved
            if (-not [string]::IsNullOrWhiteSpace($BaselineArtifactRoot)) {
                Add-ScriptArgument -Arguments $baselineExportArgs -Name "-ArtifactRoot" -Value $BaselineArtifactRoot
            }
            Add-StringArrayScriptArgument -Arguments $baselineExportArgs -Name "-ArtifactReport" -Values $BaselineArtifactReport
            Add-StringArrayScriptArgument -Arguments $baselineExportArgs -Name "-Case" -Values $BaselineCase

            $null = Invoke-PowerShellFile `
                -PowerShellExe $powerShellExe `
                -ScriptPath $exportWitnessScript `
                -ArgumentList $baselineExportArgs.ToArray() `
                -LogPath $baselineExportLogPathResolved `
                -FailureMessage "baseline system compiler witness export failed"
        }

        if (-not (Test-Path $resolvedBaselineWitnessSummary)) {
            throw "baseline system compiler witness summary not found: $resolvedBaselineWitnessSummary"
        }

        $baselineValidateArgs = @(
            $validateWitnessScript,
            "--summary",
            $resolvedBaselineWitnessSummary
        )
        $null = Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList $baselineValidateArgs `
            -LogPath $baselineValidationLogPathResolved `
            -FailureMessage "baseline system compiler witness validation failed"

        $worldCompareArgs = @(
            $worldCompareScript,
            "--baseline",
            $resolvedBaselineWitnessSummary,
            "--candidate",
            $summaryPathResolved,
            "--output-root",
            $resolvedWorldCompareOutputRoot,
            "--summary",
            $worldCompareSummaryPathResolved,
            "--report-markdown",
            $worldCompareReportMarkdownPathResolved,
            "--check-text",
            $worldCompareCheckTextPathResolved
        )
        $null = Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList $worldCompareArgs `
            -LogPath $worldCompareLogPathResolved `
            -FailureMessage "system compiler world compare failed"

        $worldCompareValidateArgs = @(
            $validateWorldCompareScript,
            "--summary",
            $worldCompareSummaryPathResolved
        )
        $null = Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList $worldCompareValidateArgs `
            -LogPath $worldCompareValidationLogPathResolved `
            -FailureMessage "system compiler world compare validation failed"

        Append-WorldCompareOverlay `
            -ReportPath $reportMarkdownPathResolved `
            -CheckTextPath $checkTextPathResolved `
            -WorldCompareSummaryPath $worldCompareSummaryPathResolved
    }

    $biographyArgs = @(
        $exportBiographyScript,
        "--runtime-evidence",
        $resolvedRuntimeEvidenceSummary,
        "--witness-bundle",
        $summaryPathResolved,
        "--output-root",
        $outputRootPath,
        "--summary",
        $biographySummaryPathResolved,
        "--report-markdown",
        $biographyReportMarkdownPathResolved,
        "--check-text",
        $biographyCheckTextPathResolved,
        "--profile",
        "minimal-kernel-runtime-system-compiler-witness"
    )
    if ($shouldRunWorldCompare) {
        $biographyArgs += @(
            "--world-compare",
            $worldCompareSummaryPathResolved
        )
    }
    $null = Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList $biographyArgs `
        -LogPath $biographyExportLogPathResolved `
        -FailureMessage "system compiler biography export failed"

    $biographyValidateArgs = @(
        $validateBiographyScript,
        "--summary",
        $biographySummaryPathResolved
    )
    $null = Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList $biographyValidateArgs `
        -LogPath $biographyValidationLogPathResolved `
        -FailureMessage "system compiler biography validation failed"

    $updateFrontPageArgs = @{
        SummaryPath = $summaryPathResolved
        RuntimeEvidenceSummary = $resolvedRuntimeEvidenceSummary
        BiographySummary = $biographySummaryPathResolved
    }
    if ($shouldRunWorldCompare) {
        $updateFrontPageArgs.WorldCompareSummary = $worldCompareSummaryPathResolved
    }

    & $updateWitnessFrontPageScript @updateFrontPageArgs

    $null = Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList $validateArgs `
        -LogPath $validationLogPathResolved `
        -FailureMessage "system compiler witness validation failed after front_page update"

    Export-FrontPageRouteArtifacts `
        -PythonExe $resolvedPythonExe `
        -ExportScript $exportFrontPageRouteScript `
        -ValidateScript $validateFrontPageRouteScript `
        -InputSummaryPath $summaryPathResolved `
        -OutputRootPath $resolvedFrontPageRouteOutputRoot `
        -SummaryPath $frontPageRouteSummaryPathResolved `
        -ReportMarkdownPath $frontPageRouteReportMarkdownPathResolved `
        -CheckTextPath $frontPageRouteCheckTextPathResolved `
        -ValidationLogPath $frontPageRouteValidationLogPathResolved `
        -FailurePrefix "system compiler witness"

    Write-Host "==> system compiler witness bundle"
    Write-Host "profile=minimal-kernel-runtime-system-compiler-witness"
    Write-Host ("canonical_world={0}" -f $resolvedCanonicalWorld)
    Write-Host ("output_root={0}" -f $outputRootPath)
    Write-Host ("runtime_evidence_summary={0}" -f $resolvedRuntimeEvidenceSummary)
    Write-Host ("runtime_evidence_validation_log={0}" -f $runtimeEvidenceValidationLogPathResolved)
    Write-Host ("summary={0}" -f $summaryPathResolved)
    Write-Host ("report_markdown={0}" -f $reportMarkdownPathResolved)
    Write-Host ("check_text={0}" -f $checkTextPathResolved)
    Write-Host ("validation_log={0}" -f $validationLogPathResolved)
    Write-Host ("biography_summary={0}" -f $biographySummaryPathResolved)
    Write-Host ("biography_report_markdown={0}" -f $biographyReportMarkdownPathResolved)
    Write-Host ("biography_check_text={0}" -f $biographyCheckTextPathResolved)
    Write-Host ("biography_validation_log={0}" -f $biographyValidationLogPathResolved)
    Write-Host ("front_page_route_output_root={0}" -f $resolvedFrontPageRouteOutputRoot)
    Write-Host ("front_page_route_summary={0}" -f $frontPageRouteSummaryPathResolved)
    Write-Host ("front_page_route_report_markdown={0}" -f $frontPageRouteReportMarkdownPathResolved)
    Write-Host ("front_page_route_check_text={0}" -f $frontPageRouteCheckTextPathResolved)
    Write-Host ("front_page_route_validation_log={0}" -f $frontPageRouteValidationLogPathResolved)
    if ($shouldRunWorldCompare) {
        Write-Host ("baseline_witness_summary={0}" -f $resolvedBaselineWitnessSummary)
        if ([string]::IsNullOrWhiteSpace($BaselineWitnessSummary)) {
            Write-Host ("baseline_runtime_evidence_validation_log={0}" -f $baselineRuntimeEvidenceValidationLogPathResolved)
        }
        Write-Host ("world_compare_output_root={0}" -f $resolvedWorldCompareOutputRoot)
        Write-Host ("world_compare_summary={0}" -f $worldCompareSummaryPathResolved)
        Write-Host ("world_compare_report_markdown={0}" -f $worldCompareReportMarkdownPathResolved)
        Write-Host ("world_compare_check_text={0}" -f $worldCompareCheckTextPathResolved)
        Write-Host ("world_compare_validation_log={0}" -f $worldCompareValidationLogPathResolved)
    }
} finally {
    Pop-Location
}
