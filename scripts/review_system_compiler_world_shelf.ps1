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
    [string]$ReviewSummaryPath = "",
    [string]$ReviewReportMarkdownPath = "",
    [string]$ReviewCheckTextPath = "",
    [string]$ReviewValidationLogPath = "",
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

function ConvertTo-PlainJsonValue {
    param(
        $Value
    )

    if ($null -eq $Value) {
        return $null
    }

    if (
        $Value -is [string] -or
        $Value -is [char] -or
        $Value -is [bool] -or
        $Value -is [byte] -or
        $Value -is [sbyte] -or
        $Value -is [int16] -or
        $Value -is [uint16] -or
        $Value -is [int32] -or
        $Value -is [uint32] -or
        $Value -is [int64] -or
        $Value -is [uint64] -or
        $Value -is [single] -or
        $Value -is [double] -or
        $Value -is [decimal]
    ) {
        return $Value
    }

    if ($Value -is [datetime]) {
        return $Value.ToString("o")
    }

    if ($Value -is [guid] -or $Value -is [uri]) {
        return [string]$Value
    }

    if ($Value -is [System.Collections.IDictionary]) {
        $plainMap = [ordered]@{}
        foreach ($key in $Value.Keys) {
            $plainMap[[string]$key] = ConvertTo-PlainJsonValue -Value $Value[$key]
        }
        return $plainMap
    }

    if ($Value -is [System.Array]) {
        $plainItems = [System.Collections.Generic.List[object]]::new()
        foreach ($item in $Value) {
            $plainItems.Add((ConvertTo-PlainJsonValue -Value $item)) | Out-Null
        }
        return ,([object[]]$plainItems.ToArray())
    }

    if ($Value -is [System.Collections.IEnumerable] -and -not ($Value -is [string])) {
        $plainItems = [System.Collections.Generic.List[object]]::new()
        foreach ($item in $Value) {
            $plainItems.Add((ConvertTo-PlainJsonValue -Value $item)) | Out-Null
        }
        return ,([object[]]$plainItems.ToArray())
    }

    if ($Value -is [psobject]) {
        $baseObject = $Value.PSObject.BaseObject
        if ($null -ne $baseObject -and $baseObject -ne $Value) {
            return ConvertTo-PlainJsonValue -Value $baseObject
        }

        $plainObject = [ordered]@{}
        foreach ($property in $Value.PSObject.Properties) {
            if (-not $property.IsGettable) {
                continue
            }

            if ($property.MemberType -ne [System.Management.Automation.PSMemberTypes]::NoteProperty -and
                $property.MemberType -ne [System.Management.Automation.PSMemberTypes]::AliasProperty -and
                $property.MemberType -ne [System.Management.Automation.PSMemberTypes]::ScriptProperty) {
                continue
            }

            $plainObject[$property.Name] = ConvertTo-PlainJsonValue -Value $property.Value
        }
        return $plainObject
    }

    return [string]$Value
}

function Write-JsonFile {
    param(
        [string]$Path,
        $Value
    )

    Ensure-ParentDirectory -Path $Path
    $plainValue = ConvertTo-PlainJsonValue -Value $Value
    $json = ConvertTo-Json -InputObject $plainValue -Depth 64 -Compress
    Set-Content -LiteralPath $Path -Encoding utf8 $json
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

function To-StringArray {
    param(
        $Values
    )

    return ,([string[]]@(Get-StringValues -Values $Values))
}

function Get-TextValue {
    param(
        $Value
    )

    if ($null -eq $Value) {
        return ""
    }

    $text = [string]$Value
    if ([string]::IsNullOrWhiteSpace($text)) {
        return ""
    }

    return $text
}

function Resolve-SurfacePathValue {
    param(
        $PrimaryValue,
        $SecondaryValue,
        $FallbackValue
    )

    $primaryText = Get-TextValue -Value $PrimaryValue
    if (-not [string]::IsNullOrWhiteSpace($primaryText)) {
        return $primaryText
    }

    $secondaryText = Get-TextValue -Value $SecondaryValue
    if (-not [string]::IsNullOrWhiteSpace($secondaryText)) {
        return $secondaryText
    }

    return Get-TextValue -Value $FallbackValue
}

function New-FrontPageSurface {
    param(
        [string]$Id,
        [string]$Label,
        [string]$Role,
        [string]$SummarySchema,
        [string]$SummaryPath,
        [string]$ReportMarkdownPath,
        [string]$CheckTextPath
    )

    return [ordered]@{
        id = $Id
        label = $Label
        role = $Role
        summary_schema = $SummarySchema
        summary_path = $SummaryPath
        report_markdown_path = $ReportMarkdownPath
        check_text_path = $CheckTextPath
    }
}

function New-FrontPage {
    param(
        [string]$SummaryPath,
        [string]$ReportMarkdownPath,
        [string]$CheckTextPath,
        [object[]]$SupportingSurfaces = @()
    )

    return [ordered]@{
        summary_path = $SummaryPath
        report_markdown_path = $ReportMarkdownPath
        check_text_path = $CheckTextPath
        supporting_surfaces = [object[]]@($SupportingSurfaces)
    }
}

function New-FrontPageSurfaceFromSummary {
    param(
        $Summary,
        [string]$FallbackSummaryPath,
        [string]$FallbackReportMarkdownPath,
        [string]$FallbackCheckTextPath,
        [string]$SurfaceId,
        [string]$Role,
        [string]$SummarySchema,
        [string]$LabelPrefix
    )

    $surfaceTitle = Get-TextValue -Value $Summary.shelf.title
    if ([string]::IsNullOrWhiteSpace($surfaceTitle)) {
        $surfaceTitle = "System Compiler World Shelf"
    }

    $summaryPath = Resolve-SurfacePathValue `
        -PrimaryValue $Summary.front_page.summary_path `
        -SecondaryValue $Summary.delivery.summary_path `
        -FallbackValue $FallbackSummaryPath
    $reportMarkdownPath = Resolve-SurfacePathValue `
        -PrimaryValue $Summary.front_page.report_markdown_path `
        -SecondaryValue $Summary.delivery.report_markdown_path `
        -FallbackValue $FallbackReportMarkdownPath
    $checkTextPath = Resolve-SurfacePathValue `
        -PrimaryValue $Summary.front_page.check_text_path `
        -SecondaryValue $Summary.delivery.check_text_path `
        -FallbackValue $FallbackCheckTextPath

    return New-FrontPageSurface `
        -Id $SurfaceId `
        -Label ("{0}: {1}" -f $LabelPrefix, $surfaceTitle) `
        -Role $Role `
        -SummarySchema $SummarySchema `
        -SummaryPath $summaryPath `
        -ReportMarkdownPath $reportMarkdownPath `
        -CheckTextPath $checkTextPath
}

function New-RouteProvenanceEntryFromSummary {
    param(
        $Summary,
        [string]$FallbackSummaryPath,
        [string]$FallbackReportMarkdownPath,
        [string]$FallbackCheckTextPath,
        [string]$RouteId,
        [string]$SummarySchema
    )

    $summaryPath = Resolve-SurfacePathValue `
        -PrimaryValue $Summary.front_page.summary_path `
        -SecondaryValue $Summary.delivery.summary_path `
        -FallbackValue $FallbackSummaryPath
    $reportMarkdownPath = Resolve-SurfacePathValue `
        -PrimaryValue $Summary.front_page.report_markdown_path `
        -SecondaryValue $Summary.delivery.report_markdown_path `
        -FallbackValue $FallbackReportMarkdownPath
    $checkTextPath = Resolve-SurfacePathValue `
        -PrimaryValue $Summary.front_page.check_text_path `
        -SecondaryValue $Summary.delivery.check_text_path `
        -FallbackValue $FallbackCheckTextPath
    $availableSupportingSurfaceIds = To-StringArray -Values @(
        @($Summary.front_page.supporting_surfaces) |
            Where-Object { $null -ne $_ } |
            ForEach-Object { Get-TextValue -Value $_.id } |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    )

    return [ordered]@{
        id = $RouteId
        route_kind = "front_page_root"
        source_summary_schema = $SummarySchema
        source_summary_path = $FallbackSummaryPath
        source_front_page_summary_path = $summaryPath
        source_front_page_report_markdown_path = $reportMarkdownPath
        source_front_page_check_text_path = $checkTextPath
        available_supporting_surface_ids = $availableSupportingSurfaceIds
    }
}

function New-DriftDigest {
    param(
        $CompareSummary
    )

    if ($null -eq $CompareSummary) {
        return [ordered]@{
            changed = $false
            verdict = "candidate-only"
            entry_changed_count = 0
            entry_regression_count = 0
            entry_improvement_count = 0
            front_page_entry_detail_changed_count = 0
            front_page_entry_detail_changed_anchors = [string[]]@()
            removed_worlds = [string[]]@()
            added_failed_entries = [string[]]@()
            affected_worlds = [string[]]@()
            affected_profiles = [string[]]@()
            narratives = [string[]]@()
        }
    }

    $collapseSurface = $CompareSummary.collapse_surface
    $shelfChanges = $CompareSummary.shelf_changes
    $entrySummary = $CompareSummary.entry_summary
    $frontPageEntryDetailChangedAnchors = if ($null -ne $collapseSurface) {
        To-StringArray -Values $collapseSurface.front_page_entry_detail_changed_anchors
    } else {
        [string[]]@()
    }
    $frontPageEntryDetailChangeCount = if ($null -ne $shelfChanges -and $null -ne $shelfChanges.front_page_entry_detail_changes) {
        @($shelfChanges.front_page_entry_detail_changes).Count
    } else {
        0
    }

    return [ordered]@{
        changed = if ($null -ne $collapseSurface) { [bool]$collapseSurface.changed } else { $false }
        verdict = [string]$CompareSummary.shelf_verdict
        entry_changed_count = if ($null -ne $entrySummary) { [int]$entrySummary.changed_entry_count } else { 0 }
        entry_regression_count = if ($null -ne $entrySummary) { [int]$entrySummary.regression_count } else { 0 }
        entry_improvement_count = if ($null -ne $entrySummary) { [int]$entrySummary.improvement_count } else { 0 }
        front_page_entry_detail_changed_count = $frontPageEntryDetailChangeCount
        front_page_entry_detail_changed_anchors = $frontPageEntryDetailChangedAnchors
        removed_worlds = if ($null -ne $collapseSurface) { To-StringArray -Values $collapseSurface.removed_worlds } else { [string[]]@() }
        added_failed_entries = if ($null -ne $collapseSurface) { To-StringArray -Values $collapseSurface.added_failed_entries } else { [string[]]@() }
        affected_worlds = if ($null -ne $collapseSurface) { To-StringArray -Values $collapseSurface.affected_worlds } else { [string[]]@() }
        affected_profiles = if ($null -ne $collapseSurface) { To-StringArray -Values $collapseSurface.affected_profiles } else { [string[]]@() }
        narratives = if ($null -ne $collapseSurface) { To-StringArray -Values $collapseSurface.narratives } else { [string[]]@() }
    }
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
$reviewSummaryPathResolved = Get-OutputPath -ExplicitPath $ReviewSummaryPath -OutputRootPath $outputRootPath -DefaultFileName "world-shelf.review.summary.json"
$reviewReportMarkdownPathResolved = Get-OutputPath -ExplicitPath $ReviewReportMarkdownPath -OutputRootPath $outputRootPath -DefaultFileName "world-shelf.review.md"
$reviewCheckTextPathResolved = Get-OutputPath -ExplicitPath $ReviewCheckTextPath -OutputRootPath $outputRootPath -DefaultFileName "world-shelf.check.txt"
$reviewValidationLogPathResolved = Get-OutputPath -ExplicitPath $ReviewValidationLogPath -OutputRootPath $outputRootPath -DefaultFileName "world-shelf.review.validate.log"

$assembleShelfScript = Join-Path $PSScriptRoot "assemble_system_compiler_world_shelf.ps1"
$compareShelfScript = Join-Path $PSScriptRoot "compare_system_compiler_world_shelf.ps1"
$validateReviewScript = Join-Path $PSScriptRoot "validate_system_compiler_world_shelf_review.py"
foreach ($requiredPath in @($assembleShelfScript, $compareShelfScript, $validateReviewScript)) {
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
    $compareSurfaceQuestions = if ($null -ne $compareSummary) {
        Get-StringValues -Values $compareSummary.questions.compare_questions
    } else {
        @()
    }
    $nextQuestions = if ($null -ne $compareSummary) {
        Get-StringValues -Values $compareSummary.questions.next_questions
    } else {
        $candidateQuestions
    }
    $reviewMode = if ($SkipCompare) {
        "candidate-only"
    } elseif ($CompareAgainstSelf) {
        "self-compare"
    } else {
        "baseline-compare"
    }
    $reviewVerdict = if ($null -ne $compareSummary) {
        [string]$compareSummary.shelf_verdict
    } else {
        "candidate-only"
    }
    $driftDigest = New-DriftDigest -CompareSummary $compareSummary
    $compareSummaryPathForReport = if ($null -ne $compareSummary) {
        Join-Path $compareOutputRootResolved "summary.json"
    } else {
        ""
    }
    $baselineShelfRootForReview = if (-not [string]::IsNullOrWhiteSpace($baselineReferenceRoot)) {
        $baselineReferenceRoot
    } elseif (-not [string]::IsNullOrWhiteSpace($baselineSummaryPathForReport)) {
        Split-Path -Parent $baselineSummaryPathForReport
    } else {
        ""
    }
    $compareCollapseSurface = if ($null -ne $compareSummary -and $null -ne $compareSummary.collapse_surface) {
        [ordered]@{
            changed = [bool]$compareSummary.collapse_surface.changed
            regressed_entries = To-StringArray -Values $compareSummary.collapse_surface.regressed_entries
            removed_worlds = To-StringArray -Values $compareSummary.collapse_surface.removed_worlds
            added_failed_entries = To-StringArray -Values $compareSummary.collapse_surface.added_failed_entries
            front_page_entry_detail_changed_anchors = To-StringArray -Values $compareSummary.collapse_surface.front_page_entry_detail_changed_anchors
            affected_worlds = To-StringArray -Values $compareSummary.collapse_surface.affected_worlds
            affected_profiles = To-StringArray -Values $compareSummary.collapse_surface.affected_profiles
            narratives = To-StringArray -Values $compareSummary.collapse_surface.narratives
        }
    } else {
        [ordered]@{
            changed = $false
            regressed_entries = [string[]]@()
            removed_worlds = [string[]]@()
            added_failed_entries = [string[]]@()
            front_page_entry_detail_changed_anchors = [string[]]@()
            affected_worlds = [string[]]@()
            affected_profiles = [string[]]@()
            narratives = [string[]]@()
        }
    }

    $frontPageSupportingSurfaces = [System.Collections.Generic.List[object]]::new()
    $frontPageSupportingSurfaces.Add((
        New-FrontPageSurfaceFromSummary `
            -Summary $candidateSummary `
            -FallbackSummaryPath $candidateSummaryPath `
            -FallbackReportMarkdownPath (Join-Path $candidateShelfOutputRootResolved "biography.index.report.md") `
            -FallbackCheckTextPath (Join-Path $candidateShelfOutputRootResolved "biography.index.check.txt") `
            -SurfaceId "candidate_shelf" `
            -Role "candidate_shelf" `
            -SummarySchema "system_compiler.biography_index/v0" `
            -LabelPrefix "candidate shelf"
    )) | Out-Null
    if ($null -ne $compareSummary) {
        $frontPageSupportingSurfaces.Add((
            New-FrontPageSurfaceFromSummary `
                -Summary $compareSummary `
                -FallbackSummaryPath $compareSummaryPathForReport `
                -FallbackReportMarkdownPath (Join-Path $compareOutputRootResolved "report.md") `
                -FallbackCheckTextPath (Join-Path $compareOutputRootResolved "check.txt") `
                -SurfaceId "shelf_compare" `
                -Role "shelf_compare" `
                -SummarySchema "system_compiler.biography_index_compare/v0" `
                -LabelPrefix "shelf compare"
        )) | Out-Null
    }
    if ($null -ne $baselineSummary) {
        $frontPageSupportingSurfaces.Add((
            New-FrontPageSurfaceFromSummary `
                -Summary $baselineSummary `
                -FallbackSummaryPath $baselineSummaryPathForReport `
                -FallbackReportMarkdownPath (Join-Path $baselineShelfRootForReview "biography.index.report.md") `
                -FallbackCheckTextPath (Join-Path $baselineShelfRootForReview "biography.index.check.txt") `
                -SurfaceId "baseline_shelf" `
                -Role "baseline_shelf" `
                -SummarySchema "system_compiler.biography_index/v0" `
                -LabelPrefix "baseline shelf"
        )) | Out-Null
    }

    $routeProvenance = [System.Collections.Generic.List[object]]::new()
    $routeProvenance.Add((
        New-RouteProvenanceEntryFromSummary `
            -Summary $candidateSummary `
            -FallbackSummaryPath $candidateSummaryPath `
            -FallbackReportMarkdownPath (Join-Path $candidateShelfOutputRootResolved "biography.index.report.md") `
            -FallbackCheckTextPath (Join-Path $candidateShelfOutputRootResolved "biography.index.check.txt") `
            -RouteId "candidate_shelf" `
            -SummarySchema "system_compiler.biography_index/v0"
    )) | Out-Null
    if ($null -ne $compareSummary) {
        $routeProvenance.Add((
            New-RouteProvenanceEntryFromSummary `
                -Summary $compareSummary `
                -FallbackSummaryPath $compareSummaryPathForReport `
                -FallbackReportMarkdownPath (Join-Path $compareOutputRootResolved "report.md") `
                -FallbackCheckTextPath (Join-Path $compareOutputRootResolved "check.txt") `
                -RouteId "shelf_compare" `
                -SummarySchema "system_compiler.biography_index_compare/v0"
        )) | Out-Null
    }
    if ($null -ne $baselineSummary) {
        $routeProvenance.Add((
            New-RouteProvenanceEntryFromSummary `
                -Summary $baselineSummary `
                -FallbackSummaryPath $baselineSummaryPathForReport `
                -FallbackReportMarkdownPath (Join-Path $baselineShelfRootForReview "biography.index.report.md") `
                -FallbackCheckTextPath (Join-Path $baselineShelfRootForReview "biography.index.check.txt") `
                -RouteId "baseline_shelf" `
                -SummarySchema "system_compiler.biography_index/v0"
        )) | Out-Null
    }

    $reportLines = [System.Collections.Generic.List[string]]::new()
    $reportLines.Add("# System Compiler World Shelf Review") | Out-Null
    $reportLines.Add("") | Out-Null
    $reportLines.Add("- Review summary: ``$reviewSummaryPathResolved``") | Out-Null
    $reportLines.Add("- Output root: ``$outputRootPath``") | Out-Null
    $reportLines.Add("- Review mode: ``$reviewMode``") | Out-Null
    $reportLines.Add("- Review verdict: ``$reviewVerdict``") | Out-Null
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
        $reportLines.Add("- Shelf compare summary: ``$compareSummaryPathForReport``") | Out-Null
        $reportLines.Add("- Shelf compare verdict: ``$([string]$compareSummary.shelf_verdict)``") | Out-Null
        $reportLines.Add("- Shelf compare changes: ``changed=$([int]$compareSummary.entry_summary.changed_entry_count) added=$([int]$compareSummary.entry_summary.added_entry_count) removed=$([int]$compareSummary.entry_summary.removed_entry_count) regressions=$([int]$compareSummary.entry_summary.regression_count) improvements=$([int]$compareSummary.entry_summary.improvement_count)``") | Out-Null
    }

    $reportLines.Add("") | Out-Null
    $reportLines.Add("## Drift Digest") | Out-Null
    $reportLines.Add("- Changed: ``$([bool]$driftDigest.changed)``") | Out-Null
    $reportLines.Add("- Verdict: ``$([string]$driftDigest.verdict)``") | Out-Null
    $reportLines.Add("- Entry impact: ``changed=$([int]$driftDigest.entry_changed_count) regressions=$([int]$driftDigest.entry_regression_count) improvements=$([int]$driftDigest.entry_improvement_count)``") | Out-Null
    $reportLines.Add("- Front-page entry detail drift: ``count=$([int]$driftDigest.front_page_entry_detail_changed_count) anchors=$((To-StringArray -Values $driftDigest.front_page_entry_detail_changed_anchors) -join ', ')``") | Out-Null
    $affectedWorlds = To-StringArray -Values $driftDigest.affected_worlds
    if ($affectedWorlds.Count -gt 0) {
        $reportLines.Add(("- Affected worlds: ``{0}``" -f ($affectedWorlds -join ", "))) | Out-Null
    }
    $driftNarratives = To-StringArray -Values $driftDigest.narratives
    foreach ($narrative in $driftNarratives) {
        $reportLines.Add(("- {0}" -f $narrative)) | Out-Null
    }

    $reportLines.Add("") | Out-Null
    $reportLines.Add("## Route Provenance") | Out-Null
    foreach ($routeEntry in @($routeProvenance)) {
        $reportLines.Add(("- ``{0}`` via ``{1}`` -> ``{2}``" -f [string]$routeEntry.id, [string]$routeEntry.route_kind, [string]$routeEntry.source_front_page_summary_path)) | Out-Null
        $availableSurfaceIds = To-StringArray -Values $routeEntry.available_supporting_surface_ids
        if ($availableSurfaceIds.Count -gt 0) {
            $reportLines.Add(("- supporting surfaces: ``{0}``" -f ($availableSurfaceIds -join ", "))) | Out-Null
        } else {
            $reportLines.Add("- supporting surfaces: none") | Out-Null
        }
    }

    $reportLines.Add("") | Out-Null
    $reportLines.Add("## Next Questions") | Out-Null
    if ($nextQuestions.Count -eq 0) {
        $reportLines.Add("- none") | Out-Null
    } else {
        foreach ($question in $nextQuestions) {
            $reportLines.Add("- $question") | Out-Null
        }
    }

    Write-Utf8Text -Path $reviewReportMarkdownPathResolved -Text (($reportLines -join [Environment]::NewLine) + [Environment]::NewLine)

    $checkLines = [System.Collections.Generic.List[string]]::new()
    $checkLines.Add(("review_summary: {0}" -f $reviewSummaryPathResolved)) | Out-Null
    $checkLines.Add(("output_root: {0}" -f $outputRootPath)) | Out-Null
    $checkLines.Add(("review_mode: {0}" -f $reviewMode)) | Out-Null
    $checkLines.Add(("review_verdict: {0}" -f $reviewVerdict)) | Out-Null
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
    $checkLines.Add(("drift_digest_changed: {0}" -f [bool]$driftDigest.changed)) | Out-Null
    $checkLines.Add(("drift_digest_front_page_entry_detail_changed_count: {0}" -f [int]$driftDigest.front_page_entry_detail_changed_count)) | Out-Null
    $checkLines.Add(("review_question_count: {0}" -f $nextQuestions.Count)) | Out-Null
    Write-Utf8Text -Path $reviewCheckTextPathResolved -Text (($checkLines -join [Environment]::NewLine) + [Environment]::NewLine)

    $reviewSummary = [ordered]@{
        schema = "system_compiler.world_shelf_review/v0"
        kind = "system_compiler.world_shelf_review"
        generated_at_utc = [DateTime]::UtcNow.ToString("o")
        generator = "scripts/review_system_compiler_world_shelf.ps1"
        result = "ok"
        review_verdict = $reviewVerdict
        review = [ordered]@{
            title = "System Compiler World Shelf Review"
            summary = if ($null -ne $compareSummary) {
                "Candidate shelf review completed with compare verdict '$reviewVerdict'."
            } else {
                "Candidate shelf review completed without baseline compare."
            }
        }
        front_page = New-FrontPage `
            -SummaryPath $reviewSummaryPathResolved `
            -ReportMarkdownPath $reviewReportMarkdownPathResolved `
            -CheckTextPath $reviewCheckTextPathResolved `
            -SupportingSurfaces $frontPageSupportingSurfaces.ToArray()
        route_provenance = $routeProvenance.ToArray()
        artifact_context = [ordered]@{
            output_root = $outputRootPath
            review_summary_path = $reviewSummaryPathResolved
            review_report_markdown_path = $reviewReportMarkdownPathResolved
            review_check_text_path = $reviewCheckTextPathResolved
            candidate_shelf_root = $candidateShelfOutputRootResolved
            candidate_shelf_summary = $candidateSummaryPath
            baseline_shelf_root = if (-not [string]::IsNullOrWhiteSpace($baselineShelfRootForReview)) { $baselineShelfRootForReview } else { $null }
            baseline_shelf_summary = if (-not [string]::IsNullOrWhiteSpace($baselineSummaryPathForReport)) { $baselineSummaryPathForReport } else { $null }
            compare_output_root = if ($null -ne $compareSummary) { $compareOutputRootResolved } else { $null }
            compare_summary_path = if (-not [string]::IsNullOrWhiteSpace($compareSummaryPathForReport)) { $compareSummaryPathForReport } else { $null }
        }
        review_status = [ordered]@{
            compare_mode = $reviewMode
            compare_enabled = ($null -ne $compareSummary)
            candidate_result = [string]$candidateSummary.result
            baseline_result = if ($null -ne $baselineSummary) { [string]$baselineSummary.result } else { $null }
            compare_result = if ($null -ne $compareSummary) { [string]$compareSummary.result } else { $null }
            candidate_profile = [string]$candidateSummary.profile
            baseline_profile = if ($null -ne $baselineSummary) { [string]$baselineSummary.profile } else { $null }
            candidate_biography_count = [int]$candidateSummary.summary.biography_count
            baseline_biography_count = if ($null -ne $baselineSummary) { [int]$baselineSummary.summary.biography_count } else { $null }
            candidate_unique_world_count = [int]$candidateSummary.summary.unique_world_count
            baseline_unique_world_count = if ($null -ne $baselineSummary) { [int]$baselineSummary.summary.unique_world_count } else { $null }
            candidate_compare_attached_count = [int]$candidateSummary.summary.compare_attached_count
            baseline_compare_attached_count = if ($null -ne $baselineSummary) { [int]$baselineSummary.summary.compare_attached_count } else { $null }
            candidate_not_attached_count = [int]$candidateSummary.summary.not_attached_count
            baseline_not_attached_count = if ($null -ne $baselineSummary) { [int]$baselineSummary.summary.not_attached_count } else { $null }
        }
        questions = [ordered]@{
            candidate_questions = To-StringArray -Values $candidateQuestions
            compare_questions = To-StringArray -Values $compareSurfaceQuestions
            next_questions = To-StringArray -Values $nextQuestions
        }
        drift_digest = $driftDigest
        collapse_surface = $compareCollapseSurface
        violations = To-StringArray -Values @()
    }

    Write-JsonFile -Path $reviewSummaryPathResolved -Value $reviewSummary
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateReviewScript, "--summary", $reviewSummaryPathResolved) `
        -LogPath $reviewValidationLogPathResolved `
        -FailureMessage "world shelf review validation failed"

    Write-Host "==> system compiler world shelf review"
    Write-Host ("output_root={0}" -f $outputRootPath)
    Write-Host ("review_summary={0}" -f $reviewSummaryPathResolved)
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
    Write-Host ("review_validation_log={0}" -f $reviewValidationLogPathResolved)
} finally {
    Pop-Location
}
