param(
    [string[]]$BiographySummary = @(),
    [string[]]$BiographyRoot = @(),
    [string[]]$SearchRoot = @(),
    [string[]]$BaselineBiographySummary = @(),
    [string[]]$BaselineBiographyRoot = @(),
    [string[]]$BaselineSearchRoot = @(),
    [string]$BaselineBiographyIndexSummary = "",
    [string]$BaselineShelfRoot = "",
    [string]$OutputRoot = "",
    [string]$CandidateShelfOutputRoot = "",
    [string]$BaselineShelfOutputRoot = "",
    [string]$CompareOutputRoot = "",
    [string]$ReviewReportMarkdownPath = "",
    [string]$ReviewCheckTextPath = "",
    [string]$PythonExe = "",
    [switch]$Clean,
    [switch]$CompareAgainstSelf,
    [switch]$SkipCompare,
    [string]$CandidateProfile = "system-compiler-world-shelf",
    [switch]$SkipCandidateGate,
    [string]$CandidateRequireResult = "ok",
    [int]$CandidateRequireBiographyCount = -1,
    [int]$CandidateRequireUniqueWorldCount = -1,
    [int]$CandidateRequireOkCount = -1,
    [int]$CandidateMaxFailCount = -1,
    [int]$CandidateRequireCompareAttachedCount = -1,
    [int]$CandidateRequireNotAttachedCount = -1,
    [int]$CandidateRequireStandingCount = -1,
    [int]$CandidateRequireImprovedCount = -1,
    [int]$CandidateRequireDriftedCount = -1,
    [int]$CandidateRequireCollapsedCount = -1,
    [string]$BaselineProfile = "system-compiler-world-shelf-baseline",
    [switch]$SkipBaselineGate,
    [string]$BaselineRequireResult = "ok",
    [int]$BaselineRequireBiographyCount = -1,
    [int]$BaselineRequireUniqueWorldCount = -1,
    [int]$BaselineRequireOkCount = -1,
    [int]$BaselineMaxFailCount = -1,
    [int]$BaselineRequireCompareAttachedCount = -1,
    [int]$BaselineRequireNotAttachedCount = -1,
    [int]$BaselineRequireStandingCount = -1,
    [int]$BaselineRequireImprovedCount = -1,
    [int]$BaselineRequireDriftedCount = -1,
    [int]$BaselineRequireCollapsedCount = -1,
    [switch]$SkipCompareGate,
    [string]$CompareRequireResult = "ok",
    [string]$CompareRequireVerdict = "",
    [int]$CompareMaxRegressions = -1,
    [int]$CompareRequireAddedEntries = -1,
    [int]$CompareRequireRemovedEntries = -1,
    [int]$CompareRequireChangedEntries = -1,
    [int]$CompareRequireImprovementCount = -1,
    [int]$CompareRequireAddedWorlds = -1,
    [int]$CompareRequireRemovedWorlds = -1,
    [int]$CompareMaxAddedFailedEntries = -1
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

function Add-ToolArgument {
    param(
        [System.Collections.Generic.List[string]]$Arguments,
        [string]$Name,
        [string]$Value
    )

    $Arguments.Add($Name) | Out-Null
    $Arguments.Add($Value) | Out-Null
}

function Add-ToolArgumentsFromArray {
    param(
        [System.Collections.Generic.List[string]]$Arguments,
        [string]$Name,
        [string[]]$Values
    )

    foreach ($value in @($Values)) {
        if ([string]::IsNullOrWhiteSpace([string]$value)) {
            continue
        }

        $Arguments.Add($Name) | Out-Null
        $Arguments.Add([string]$value) | Out-Null
    }
}

function Invoke-PowerShellFile {
    param(
        [string]$PowerShellExe,
        [string]$ScriptPath,
        [string[]]$ArgumentList,
        [string]$FailureMessage
    )

    $commandArgs = [System.Collections.Generic.List[string]]::new()
    $commandArgs.Add("-NoProfile") | Out-Null
    $commandArgs.Add("-ExecutionPolicy") | Out-Null
    $commandArgs.Add("Bypass") | Out-Null
    $commandArgs.Add("-File") | Out-Null
    $commandArgs.Add($ScriptPath) | Out-Null
    foreach ($entry in @($ArgumentList)) {
        $commandArgs.Add([string]$entry) | Out-Null
    }

    Write-Host ("==> {0}" -f [System.IO.Path]::GetFileName($ScriptPath))

    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & $PowerShellExe @commandArgs
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    if ($exitCode -ne 0) {
        throw ("{0} (exit code {1})" -f $FailureMessage, $exitCode)
    }
}

function Invoke-PowerShellScript {
    param(
        [string]$ScriptPath,
        [hashtable]$Parameters,
        [string]$FailureMessage
    )

    Write-Host ("==> {0}" -f [System.IO.Path]::GetFileName($ScriptPath))

    try {
        & $ScriptPath @Parameters
    } catch {
        throw ("{0}: {1}" -f $FailureMessage, $_.Exception.Message)
    }
}

function Write-Utf8Text {
    param(
        [string]$Path,
        [string]$Text
    )

    Ensure-ParentDirectory -Path $Path
    Set-Content -LiteralPath $Path -Encoding utf8 $Text
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

function Add-ShelfAssemblyArguments {
    param(
        [hashtable]$Arguments,
        [string[]]$Summaries,
        [string[]]$Roots,
        [string[]]$SearchRoots,
        [string]$OutputRootPath,
        [string]$Profile,
        [bool]$SkipGate,
        [string]$RequireResult,
        [int]$RequireBiographyCount,
        [int]$RequireUniqueWorldCount,
        [int]$RequireOkCount,
        [int]$MaxFailCount,
        [int]$RequireCompareAttachedCount,
        [int]$RequireNotAttachedCount,
        [int]$RequireStandingCount,
        [int]$RequireImprovedCount,
        [int]$RequireDriftedCount,
        [int]$RequireCollapsedCount,
        [string]$ResolvedPythonExe
    )

    $filteredSummaries = Get-StringValues -Values $Summaries
    if ($filteredSummaries.Count -gt 0) {
        $Arguments.BiographySummary = $filteredSummaries
    }

    $filteredRoots = Get-StringValues -Values $Roots
    if ($filteredRoots.Count -gt 0) {
        $Arguments.BiographyRoot = $filteredRoots
    }

    $filteredSearchRoots = Get-StringValues -Values $SearchRoots
    if ($filteredSearchRoots.Count -gt 0) {
        $Arguments.SearchRoot = $filteredSearchRoots
    }

    $Arguments.OutputRoot = $OutputRootPath
    $Arguments.Profile = $Profile
    if (-not [string]::IsNullOrWhiteSpace($ResolvedPythonExe)) {
        $Arguments.PythonExe = $ResolvedPythonExe
    }
    if ($SkipGate) {
        $Arguments.SkipGate = $true
    }
    if (-not [string]::IsNullOrWhiteSpace($RequireResult)) {
        $Arguments.RequireResult = $RequireResult
    }
    if ($RequireBiographyCount -ge 0) {
        $Arguments.RequireBiographyCount = $RequireBiographyCount
    }
    if ($RequireUniqueWorldCount -ge 0) {
        $Arguments.RequireUniqueWorldCount = $RequireUniqueWorldCount
    }
    if ($RequireOkCount -ge 0) {
        $Arguments.RequireOkCount = $RequireOkCount
    }
    if ($MaxFailCount -ge 0) {
        $Arguments.MaxFailCount = $MaxFailCount
    }
    if ($RequireCompareAttachedCount -ge 0) {
        $Arguments.RequireCompareAttachedCount = $RequireCompareAttachedCount
    }
    if ($RequireNotAttachedCount -ge 0) {
        $Arguments.RequireNotAttachedCount = $RequireNotAttachedCount
    }
    if ($RequireStandingCount -ge 0) {
        $Arguments.RequireStandingCount = $RequireStandingCount
    }
    if ($RequireImprovedCount -ge 0) {
        $Arguments.RequireImprovedCount = $RequireImprovedCount
    }
    if ($RequireDriftedCount -ge 0) {
        $Arguments.RequireDriftedCount = $RequireDriftedCount
    }
    if ($RequireCollapsedCount -ge 0) {
        $Arguments.RequireCollapsedCount = $RequireCollapsedCount
    }
}

function Add-ShelfCompareArguments {
    param(
        [hashtable]$Arguments,
        [string]$BaselineSummaryPath,
        [string]$BaselineRootPath,
        [string]$CandidateRootPath,
        [string]$OutputRootPath,
        [bool]$SkipGate,
        [string]$RequireResult,
        [string]$RequireVerdict,
        [int]$MaxRegressions,
        [int]$RequireAddedEntries,
        [int]$RequireRemovedEntries,
        [int]$RequireChangedEntries,
        [int]$RequireImprovementCount,
        [int]$RequireAddedWorlds,
        [int]$RequireRemovedWorlds,
        [int]$MaxAddedFailedEntries,
        [string]$ResolvedPythonExe
    )

    if (-not [string]::IsNullOrWhiteSpace($BaselineSummaryPath)) {
        $Arguments.BaselineSummary = $BaselineSummaryPath
    } elseif (-not [string]::IsNullOrWhiteSpace($BaselineRootPath)) {
        $Arguments.BaselineShelfRoot = $BaselineRootPath
    }

    $Arguments.CandidateShelfRoot = $CandidateRootPath
    $Arguments.OutputRoot = $OutputRootPath
    if (-not [string]::IsNullOrWhiteSpace($ResolvedPythonExe)) {
        $Arguments.PythonExe = $ResolvedPythonExe
    }
    if ($SkipGate) {
        $Arguments.SkipGate = $true
    }
    if (-not [string]::IsNullOrWhiteSpace($RequireResult)) {
        $Arguments.RequireResult = $RequireResult
    }
    if (-not [string]::IsNullOrWhiteSpace($RequireVerdict)) {
        $Arguments.RequireVerdict = $RequireVerdict
    }
    if ($MaxRegressions -ge 0) {
        $Arguments.MaxRegressions = $MaxRegressions
    }
    if ($RequireAddedEntries -ge 0) {
        $Arguments.RequireAddedEntries = $RequireAddedEntries
    }
    if ($RequireRemovedEntries -ge 0) {
        $Arguments.RequireRemovedEntries = $RequireRemovedEntries
    }
    if ($RequireChangedEntries -ge 0) {
        $Arguments.RequireChangedEntries = $RequireChangedEntries
    }
    if ($RequireImprovementCount -ge 0) {
        $Arguments.RequireImprovementCount = $RequireImprovementCount
    }
    if ($RequireAddedWorlds -ge 0) {
        $Arguments.RequireAddedWorlds = $RequireAddedWorlds
    }
    if ($RequireRemovedWorlds -ge 0) {
        $Arguments.RequireRemovedWorlds = $RequireRemovedWorlds
    }
    if ($MaxAddedFailedEntries -ge 0) {
        $Arguments.MaxAddedFailedEntries = $MaxAddedFailedEntries
    }
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$resolvedOutputRoot = if ([string]::IsNullOrWhiteSpace($OutputRoot)) { "out/system-compiler-world-shelf-review" } else { $OutputRoot }
$outputRootPath = Resolve-FullPath -Path $resolvedOutputRoot

if ($Clean) {
    Remove-PathIfExists -Path $outputRootPath
}
Ensure-Directory -Path $outputRootPath

$candidateShelfOutputRootResolved = if ([string]::IsNullOrWhiteSpace($CandidateShelfOutputRoot)) {
    Join-Path $outputRootPath "world-shelf"
} else {
    Resolve-FullPath -Path $CandidateShelfOutputRoot
}
$baselineShelfOutputRootResolved = if ([string]::IsNullOrWhiteSpace($BaselineShelfOutputRoot)) {
    Join-Path $outputRootPath "world-shelf-baseline"
} else {
    Resolve-FullPath -Path $BaselineShelfOutputRoot
}
$compareOutputRootResolved = if ([string]::IsNullOrWhiteSpace($CompareOutputRoot)) {
    Join-Path $outputRootPath "world-shelf-compare"
} else {
    Resolve-FullPath -Path $CompareOutputRoot
}
$reviewReportMarkdownPathResolved = Get-OutputPath -ExplicitPath $ReviewReportMarkdownPath -OutputRootPath $outputRootPath -DefaultFileName "world-shelf.review.md"
$reviewCheckTextPathResolved = Get-OutputPath -ExplicitPath $ReviewCheckTextPath -OutputRootPath $outputRootPath -DefaultFileName "world-shelf.check.txt"

$assembleShelfScript = Join-Path $PSScriptRoot "assemble_system_compiler_world_shelf.ps1"
$compareShelfScript = Join-Path $PSScriptRoot "compare_system_compiler_world_shelf.ps1"
foreach ($requiredPath in @($assembleShelfScript, $compareShelfScript)) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}

Push-Location $repoRoot
try {
    $candidateAssemblyArgs = @{}
    Add-ShelfAssemblyArguments `
        -Arguments $candidateAssemblyArgs `
        -Summaries $BiographySummary `
        -Roots $BiographyRoot `
        -SearchRoots $SearchRoot `
        -OutputRootPath $candidateShelfOutputRootResolved `
        -Profile $CandidateProfile `
        -SkipGate ([bool]$SkipCandidateGate) `
        -RequireResult $CandidateRequireResult `
        -RequireBiographyCount $CandidateRequireBiographyCount `
        -RequireUniqueWorldCount $CandidateRequireUniqueWorldCount `
        -RequireOkCount $CandidateRequireOkCount `
        -MaxFailCount $CandidateMaxFailCount `
        -RequireCompareAttachedCount $CandidateRequireCompareAttachedCount `
        -RequireNotAttachedCount $CandidateRequireNotAttachedCount `
        -RequireStandingCount $CandidateRequireStandingCount `
        -RequireImprovedCount $CandidateRequireImprovedCount `
        -RequireDriftedCount $CandidateRequireDriftedCount `
        -RequireCollapsedCount $CandidateRequireCollapsedCount `
        -ResolvedPythonExe $resolvedPythonExe

    Invoke-PowerShellScript `
        -ScriptPath $assembleShelfScript `
        -Parameters $candidateAssemblyArgs `
        -FailureMessage "candidate world shelf review assembly failed"

    $candidateSummaryPath = Join-Path $candidateShelfOutputRootResolved "biography.index.summary.json"
    if (-not (Test-Path $candidateSummaryPath)) {
        throw "candidate world shelf summary not found: $candidateSummaryPath"
    }

    $baselineSummaryPath = ""
    $baselineReferenceRoot = ""
    if (-not $SkipCompare) {
        if ($CompareAgainstSelf) {
            $baselineSummaryPath = $candidateSummaryPath
            $baselineReferenceRoot = $candidateShelfOutputRootResolved
        } elseif (-not [string]::IsNullOrWhiteSpace($BaselineBiographyIndexSummary)) {
            $baselineSummaryPath = Resolve-FullPath -Path $BaselineBiographyIndexSummary
        } elseif (-not [string]::IsNullOrWhiteSpace($BaselineShelfRoot)) {
            $baselineReferenceRoot = Resolve-FullPath -Path $BaselineShelfRoot
        } else {
            $baselineAssemblyArgs = @{}
            Add-ShelfAssemblyArguments `
                -Arguments $baselineAssemblyArgs `
                -Summaries $BaselineBiographySummary `
                -Roots $BaselineBiographyRoot `
                -SearchRoots $BaselineSearchRoot `
                -OutputRootPath $baselineShelfOutputRootResolved `
                -Profile $BaselineProfile `
                -SkipGate ([bool]$SkipBaselineGate) `
                -RequireResult $BaselineRequireResult `
                -RequireBiographyCount $BaselineRequireBiographyCount `
                -RequireUniqueWorldCount $BaselineRequireUniqueWorldCount `
                -RequireOkCount $BaselineRequireOkCount `
                -MaxFailCount $BaselineMaxFailCount `
                -RequireCompareAttachedCount $BaselineRequireCompareAttachedCount `
                -RequireNotAttachedCount $BaselineRequireNotAttachedCount `
                -RequireStandingCount $BaselineRequireStandingCount `
                -RequireImprovedCount $BaselineRequireImprovedCount `
                -RequireDriftedCount $BaselineRequireDriftedCount `
                -RequireCollapsedCount $BaselineRequireCollapsedCount `
                -ResolvedPythonExe $resolvedPythonExe

            Invoke-PowerShellScript `
                -ScriptPath $assembleShelfScript `
                -Parameters $baselineAssemblyArgs `
                -FailureMessage "baseline world shelf review assembly failed"

            $baselineSummaryPath = Join-Path $baselineShelfOutputRootResolved "biography.index.summary.json"
            $baselineReferenceRoot = $baselineShelfOutputRootResolved
        }

        if ([string]::IsNullOrWhiteSpace($baselineSummaryPath) -and [string]::IsNullOrWhiteSpace($baselineReferenceRoot)) {
            throw "baseline shelf input is required unless -SkipCompare or -CompareAgainstSelf is used"
        }

        if (-not [string]::IsNullOrWhiteSpace($baselineSummaryPath) -and -not (Test-Path $baselineSummaryPath)) {
            throw "baseline world shelf summary not found: $baselineSummaryPath"
        }

        $compareArgs = @{}
        Add-ShelfCompareArguments `
            -Arguments $compareArgs `
            -BaselineSummaryPath $baselineSummaryPath `
            -BaselineRootPath $baselineReferenceRoot `
            -CandidateRootPath $candidateShelfOutputRootResolved `
            -OutputRootPath $compareOutputRootResolved `
            -SkipGate ([bool]$SkipCompareGate) `
            -RequireResult $CompareRequireResult `
            -RequireVerdict $CompareRequireVerdict `
            -MaxRegressions $CompareMaxRegressions `
            -RequireAddedEntries $CompareRequireAddedEntries `
            -RequireRemovedEntries $CompareRequireRemovedEntries `
            -RequireChangedEntries $CompareRequireChangedEntries `
            -RequireImprovementCount $CompareRequireImprovementCount `
            -RequireAddedWorlds $CompareRequireAddedWorlds `
            -RequireRemovedWorlds $CompareRequireRemovedWorlds `
            -MaxAddedFailedEntries $CompareMaxAddedFailedEntries `
            -ResolvedPythonExe $resolvedPythonExe

        Invoke-PowerShellScript `
            -ScriptPath $compareShelfScript `
            -Parameters $compareArgs `
            -FailureMessage "world shelf review compare failed"
    }

    $candidateSummary = Load-JsonObject -Path $candidateSummaryPath
    $baselineSummary = $null
    $compareSummary = $null
    $baselineSummaryPathForReport = ""
    if (-not $SkipCompare) {
        if (-not [string]::IsNullOrWhiteSpace($baselineSummaryPath)) {
            $baselineSummaryPathForReport = $baselineSummaryPath
        } elseif (-not [string]::IsNullOrWhiteSpace($baselineReferenceRoot)) {
            $baselineSummaryPathForReport = Join-Path $baselineReferenceRoot "biography.index.summary.json"
        }
        if (-not [string]::IsNullOrWhiteSpace($baselineSummaryPathForReport) -and (Test-Path $baselineSummaryPathForReport)) {
            $baselineSummary = Load-JsonObject -Path $baselineSummaryPathForReport
        }

        $compareSummaryPath = Join-Path $compareOutputRootResolved "summary.json"
        if (-not (Test-Path $compareSummaryPath)) {
            throw "world shelf compare summary not found: $compareSummaryPath"
        }
        $compareSummary = Load-JsonObject -Path $compareSummaryPath
    }

    $candidateQuestions = Get-StringValues -Values $candidateSummary.questions.next_questions
    $compareQuestions = if ($null -ne $compareSummary) {
        Get-StringValues -Values $compareSummary.questions.next_questions
    } else {
        @()
    }

    $reportLines = [System.Collections.Generic.List[string]]::new()
    $reportLines.Add("# System Compiler World Shelf Review") | Out-Null
    $reportLines.Add("") | Out-Null
    $reportLines.Add("- Output root: ``$outputRootPath``") | Out-Null
    $reportLines.Add("- Candidate shelf root: ``$candidateShelfOutputRootResolved``") | Out-Null
    $reportLines.Add("- Candidate shelf summary: ``$candidateSummaryPath``") | Out-Null
    $reportLines.Add("- Candidate shelf result: ``$([string]$candidateSummary.result)``") | Out-Null
    $reportLines.Add("- Candidate shelf counts: ``biographies=$([int]$candidateSummary.summary.biography_count) worlds=$([int]$candidateSummary.summary.unique_world_count) compare_attached=$([int]$candidateSummary.summary.compare_attached_count) not_attached=$([int]$candidateSummary.summary.not_attached_count)``") | Out-Null

    if ($null -ne $baselineSummary) {
        $reportLines.Add("- Baseline shelf summary: ``$baselineSummaryPathForReport``") | Out-Null
        $reportLines.Add("- Baseline shelf result: ``$([string]$baselineSummary.result)``") | Out-Null
        $reportLines.Add("- Baseline shelf counts: ``biographies=$([int]$baselineSummary.summary.biography_count) worlds=$([int]$baselineSummary.summary.unique_world_count) compare_attached=$([int]$baselineSummary.summary.compare_attached_count) not_attached=$([int]$baselineSummary.summary.not_attached_count)``") | Out-Null
    } elseif (-not $SkipCompare -and -not [string]::IsNullOrWhiteSpace($baselineSummaryPathForReport)) {
        $reportLines.Add("- Baseline shelf summary: ``$baselineSummaryPathForReport``") | Out-Null
    }

    if ($null -ne $compareSummary) {
        $reportLines.Add("- Shelf compare summary: ``$(Join-Path $compareOutputRootResolved 'summary.json')``") | Out-Null
        $reportLines.Add("- Shelf compare verdict: ``$([string]$compareSummary.shelf_verdict)``") | Out-Null
        $reportLines.Add("- Shelf compare changes: ``changed=$([int]$compareSummary.entry_summary.changed_entry_count) added=$([int]$compareSummary.entry_summary.added_entry_count) removed=$([int]$compareSummary.entry_summary.removed_entry_count) regressions=$([int]$compareSummary.entry_summary.regression_count) improvements=$([int]$compareSummary.entry_summary.improvement_count)``") | Out-Null
    }

    $reportLines.Add("") | Out-Null
    $reportLines.Add("## Next Questions") | Out-Null
    $reviewQuestions = if ($compareQuestions.Count -gt 0) { $compareQuestions } else { $candidateQuestions }
    if ($reviewQuestions.Count -eq 0) {
        $reportLines.Add("- none") | Out-Null
    } else {
        foreach ($question in $reviewQuestions) {
            $reportLines.Add("- $question") | Out-Null
        }
    }

    Write-Utf8Text -Path $reviewReportMarkdownPathResolved -Text (($reportLines -join [Environment]::NewLine) + [Environment]::NewLine)

    $checkLines = [System.Collections.Generic.List[string]]::new()
    $checkLines.Add(("output_root: {0}" -f $outputRootPath)) | Out-Null
    $checkLines.Add(("candidate_shelf_summary: {0}" -f $candidateSummaryPath)) | Out-Null
    $checkLines.Add(("candidate_shelf_result: {0}" -f [string]$candidateSummary.result)) | Out-Null
    $checkLines.Add(("candidate_shelf_biography_count: {0}" -f [int]$candidateSummary.summary.biography_count)) | Out-Null
    $checkLines.Add(("candidate_shelf_compare_attached_count: {0}" -f [int]$candidateSummary.summary.compare_attached_count)) | Out-Null
    if (-not [string]::IsNullOrWhiteSpace($baselineSummaryPathForReport)) {
        $checkLines.Add(("baseline_shelf_summary: {0}" -f $baselineSummaryPathForReport)) | Out-Null
    }
    if ($null -ne $compareSummary) {
        $checkLines.Add(("shelf_compare_summary: {0}" -f (Join-Path $compareOutputRootResolved "summary.json"))) | Out-Null
        $checkLines.Add(("shelf_compare_result: {0}" -f [string]$compareSummary.result)) | Out-Null
        $checkLines.Add(("shelf_compare_verdict: {0}" -f [string]$compareSummary.shelf_verdict)) | Out-Null
        $checkLines.Add(("shelf_compare_changed_entry_count: {0}" -f [int]$compareSummary.entry_summary.changed_entry_count)) | Out-Null
        $checkLines.Add(("shelf_compare_regression_count: {0}" -f [int]$compareSummary.entry_summary.regression_count)) | Out-Null
    } else {
        $checkLines.Add("shelf_compare: skipped") | Out-Null
    }
    $checkLines.Add(("review_question_count: {0}" -f $reviewQuestions.Count)) | Out-Null
    Write-Utf8Text -Path $reviewCheckTextPathResolved -Text (($checkLines -join [Environment]::NewLine) + [Environment]::NewLine)

    Write-Host "==> system compiler world shelf review"
    Write-Host ("output_root={0}" -f $outputRootPath)
    Write-Host ("candidate_shelf_root={0}" -f $candidateShelfOutputRootResolved)
    if (-not [string]::IsNullOrWhiteSpace($baselineSummaryPathForReport)) {
        Write-Host ("baseline_shelf_summary={0}" -f $baselineSummaryPathForReport)
    }
    if ($null -ne $compareSummary) {
        Write-Host ("compare_output_root={0}" -f $compareOutputRootResolved)
        Write-Host ("compare_verdict={0}" -f [string]$compareSummary.shelf_verdict)
    }
    Write-Host ("review_report={0}" -f $reviewReportMarkdownPathResolved)
    Write-Host ("review_check={0}" -f $reviewCheckTextPathResolved)
} finally {
    Pop-Location
}
