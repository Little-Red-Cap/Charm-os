param(
    [string]$Summary = "",
    [string]$BaselineSummary = "",
    [int]$Top = 5,
    [switch]$ShowAllResults,
    [switch]$AsJson
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

function Get-RequiredSummaryPath {
    if ([string]::IsNullOrWhiteSpace($Summary)) {
        throw "summary path is required"
    }

    return Resolve-FullPath -Path $Summary
}

function Get-OptionalSummaryPath {
    if ([string]::IsNullOrWhiteSpace($BaselineSummary)) {
        return ""
    }

    return Resolve-FullPath -Path $BaselineSummary
}

function Convert-ToSummaryResult {
    param(
        $Entry
    )

    return [pscustomobject]@{
        Example   = [string]$Entry.Example
        Status    = [string]$Entry.Status
        ElapsedMs = [int64]$Entry.ElapsedMs
        Detail    = [string]$Entry.Detail
    }
}

function Load-SmokeSummary {
    param(
        [string]$Path
    )

    if (-not (Test-Path $Path)) {
        throw "summary not found: $Path"
    }

    $data = Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json
    if ([string]$data.schema -ne "minimal_kernel.runtime_host_smoke.summary/v1") {
        throw "unsupported host smoke summary schema: $([string]$data.schema)"
    }

    $results = @(
        @($data.results) |
            ForEach-Object { Convert-ToSummaryResult -Entry $_ }
    )

    return [pscustomobject]@{
        Path    = $Path
        Data    = $data
        Results = $results
    }
}

function Get-RunProfile {
    param(
        $SummaryData
    )

    $fresh = $false
    $keepBuildDirs = $false

    if ($null -ne $SummaryData.PSObject.Properties["mode"] -and $null -ne $SummaryData.mode) {
        if ($null -ne $SummaryData.mode.PSObject.Properties["fresh"]) {
            $fresh = [bool]$SummaryData.mode.fresh
        }
        if ($null -ne $SummaryData.mode.PSObject.Properties["keep_build_dirs"]) {
            $keepBuildDirs = [bool]$SummaryData.mode.keep_build_dirs
        }
    }

    if ($fresh -and -not $keepBuildDirs) {
        return "ci"
    }

    if (-not $fresh -and $keepBuildDirs) {
        return "daily"
    }

    return "custom"
}

function Get-ElapsedStats {
    param(
        [object[]]$Results
    )

    $elapsedValues = @(
        @($Results) |
            ForEach-Object { [int64]$_.ElapsedMs } |
            Sort-Object
    )

    if ($elapsedValues.Count -eq 0) {
        return [pscustomobject]@{
            TotalMs   = 0
            AverageMs = 0
            MedianMs  = 0
            MinMs     = 0
            MaxMs     = 0
        }
    }

    $totalMs = 0
    foreach ($value in $elapsedValues) {
        $totalMs += $value
    }

    $count = $elapsedValues.Count
    if (($count % 2) -eq 1) {
        $medianMs = $elapsedValues[[int]($count / 2)]
    } else {
        $left = $elapsedValues[($count / 2) - 1]
        $right = $elapsedValues[$count / 2]
        $medianMs = [int64][Math]::Round((($left + $right) / 2.0), 0)
    }

    return [pscustomobject]@{
        TotalMs   = $totalMs
        AverageMs = [int64][Math]::Round(($totalMs / [double]$count), 0)
        MedianMs  = $medianMs
        MinMs     = $elapsedValues[0]
        MaxMs     = $elapsedValues[$count - 1]
    }
}

function Get-StatusSummary {
    param(
        [object[]]$Results
    )

    $okCount = 0
    $failCount = 0
    $otherCount = 0

    foreach ($result in @($Results)) {
        switch ([string]$result.Status) {
            "ok" { $okCount += 1 }
            "fail" { $failCount += 1 }
            default { $otherCount += 1 }
        }
    }

    return [pscustomobject]@{
        OkCount    = $okCount
        FailCount  = $failCount
        OtherCount = $otherCount
    }
}

function Get-SortedResults {
    param(
        [object[]]$Results
    )

    return @(
        @($Results) |
            Sort-Object -Property @{ Expression = { [int64]$_.ElapsedMs }; Descending = $true }, @{ Expression = { [string]$_.Example }; Descending = $false }
    )
}

function New-ResultView {
    param(
        $Result
    )

    return [ordered]@{
        example    = [string]$Result.Example
        status     = [string]$Result.Status
        elapsed_ms = [int64]$Result.ElapsedMs
    }
}

function New-FailureView {
    param(
        $Result
    )

    return [ordered]@{
        example    = [string]$Result.Example
        status     = [string]$Result.Status
        elapsed_ms = [int64]$Result.ElapsedMs
        detail     = [string]$Result.Detail
    }
}

function New-ComparisonEntry {
    param(
        $Current,
        $Baseline
    )

    $deltaMs = [int64]$Current.ElapsedMs - [int64]$Baseline.ElapsedMs
    $deltaPct = $null
    if ([int64]$Baseline.ElapsedMs -ne 0) {
        $deltaPct = [Math]::Round((($deltaMs * 100.0) / [double]$Baseline.ElapsedMs), 1)
    }

    return [pscustomobject]@{
        Example            = [string]$Current.Example
        CurrentStatus      = [string]$Current.Status
        BaselineStatus     = [string]$Baseline.Status
        CurrentElapsedMs   = [int64]$Current.ElapsedMs
        BaselineElapsedMs  = [int64]$Baseline.ElapsedMs
        DeltaMs            = $deltaMs
        DeltaPct           = $deltaPct
    }
}

function Compare-Summaries {
    param(
        [object[]]$CurrentResults,
        [object[]]$BaselineResults
    )

    $baselineMap = @{}
    foreach ($result in @($BaselineResults)) {
        $baselineMap[[string]$result.Example] = $result
    }

    $currentMap = @{}
    foreach ($result in @($CurrentResults)) {
        $currentMap[[string]$result.Example] = $result
    }

    $matched = [System.Collections.Generic.List[object]]::new()
    $added = [System.Collections.Generic.List[string]]::new()
    foreach ($result in @($CurrentResults)) {
        $name = [string]$result.Example
        if ($baselineMap.ContainsKey($name)) {
            $matched.Add((New-ComparisonEntry -Current $result -Baseline $baselineMap[$name]))
        } else {
            $added.Add($name)
        }
    }

    $removed = [System.Collections.Generic.List[string]]::new()
    foreach ($result in @($BaselineResults)) {
        $name = [string]$result.Example
        if (-not $currentMap.ContainsKey($name)) {
            $removed.Add($name)
        }
    }

    $regressions = @(
        @($matched) |
            Where-Object { [int64]$_.DeltaMs -gt 0 } |
            Sort-Object -Property @{ Expression = { [int64]$_.DeltaMs }; Descending = $true }, @{ Expression = { [string]$_.Example }; Descending = $false }
    )

    $improvements = @(
        @($matched) |
            Where-Object { [int64]$_.DeltaMs -lt 0 } |
            Sort-Object -Property @{ Expression = { [int64]$_.DeltaMs }; Descending = $false }, @{ Expression = { [string]$_.Example }; Descending = $false }
    )

    return [pscustomobject]@{
        Matched      = @($matched)
        Added        = @($added | Sort-Object)
        Removed      = @($removed | Sort-Object)
        Regressions  = $regressions
        Improvements = $improvements
    }
}

function Format-DeltaText {
    param(
        $Comparison
    )

    $deltaMs = [int64]$Comparison.DeltaMs
    $pctText = ""
    if ($null -ne $Comparison.DeltaPct) {
        $pctText = " ({0:+0.0;-0.0;0.0}%)" -f [double]$Comparison.DeltaPct
    }

    return ("{0:+#;-#;0}ms{1}" -f $deltaMs, $pctText)
}

function New-JsonView {
    param(
        $SummaryData,
        [string]$SummaryPath,
        $StatusSummary,
        $ElapsedStats,
        [object[]]$SortedResults,
        [object[]]$Failures,
        $ComparisonData,
        [string]$BaselinePath
    )

    $topResults = @($SortedResults | Select-Object -First $Top)
    $view = [ordered]@{
        summary_path  = $SummaryPath
        schema        = [string]$SummaryData.schema
        profile       = Get-RunProfile -SummaryData $SummaryData
        generated_at  = [string]$SummaryData.generated_at
        example_count = @($SortedResults).Count
        selected_examples = @(
            @($SummaryData.selected_examples) |
                ForEach-Object { [string]$_ }
        )
        status = [ordered]@{
            ok    = $StatusSummary.OkCount
            fail  = $StatusSummary.FailCount
            other = $StatusSummary.OtherCount
        }
        elapsed_ms = [ordered]@{
            total   = $ElapsedStats.TotalMs
            average = $ElapsedStats.AverageMs
            median  = $ElapsedStats.MedianMs
            min     = $ElapsedStats.MinMs
            max     = $ElapsedStats.MaxMs
        }
        slowest = @(
            @($topResults) |
                ForEach-Object { New-ResultView -Result $_ }
        )
        failures = @(
            @($Failures) |
                ForEach-Object { New-FailureView -Result $_ }
        )
    }

    if ($ShowAllResults) {
        $view.results = @(
            @($SortedResults) |
                ForEach-Object { New-ResultView -Result $_ }
        )
    }

    if ($null -ne $ComparisonData) {
        $view.comparison = [ordered]@{
            baseline_summary_path = $BaselinePath
            matched_examples      = @($ComparisonData.Matched).Count
            added_examples        = @($ComparisonData.Added)
            removed_examples      = @($ComparisonData.Removed)
            regressions           = @(
                @($ComparisonData.Regressions | Select-Object -First $Top) |
                    ForEach-Object {
                        [ordered]@{
                            example              = [string]$_.Example
                            delta_ms             = [int64]$_.DeltaMs
                            delta_pct            = $_.DeltaPct
                            current_elapsed_ms   = [int64]$_.CurrentElapsedMs
                            baseline_elapsed_ms  = [int64]$_.BaselineElapsedMs
                            current_status       = [string]$_.CurrentStatus
                            baseline_status      = [string]$_.BaselineStatus
                        }
                    }
            )
            improvements = @(
                @($ComparisonData.Improvements | Select-Object -First $Top) |
                    ForEach-Object {
                        [ordered]@{
                            example              = [string]$_.Example
                            delta_ms             = [int64]$_.DeltaMs
                            delta_pct            = $_.DeltaPct
                            current_elapsed_ms   = [int64]$_.CurrentElapsedMs
                            baseline_elapsed_ms  = [int64]$_.BaselineElapsedMs
                            current_status       = [string]$_.CurrentStatus
                            baseline_status      = [string]$_.BaselineStatus
                        }
                    }
            )
        }
    }

    return $view
}

if ($Top -lt 1) {
    throw "Top must be at least 1"
}

$summaryPath = Get-RequiredSummaryPath
$baselinePath = Get-OptionalSummaryPath
$loaded = Load-SmokeSummary -Path $summaryPath
$sortedResults = Get-SortedResults -Results $loaded.Results
$failures = @($sortedResults | Where-Object { [string]$_.Status -ne "ok" })
$statusSummary = Get-StatusSummary -Results $sortedResults
$elapsedStats = Get-ElapsedStats -Results $sortedResults

$comparison = $null
if (-not [string]::IsNullOrWhiteSpace($baselinePath)) {
    $baselineLoaded = Load-SmokeSummary -Path $baselinePath
    $comparison = Compare-Summaries -CurrentResults $sortedResults -BaselineResults $baselineLoaded.Results
}

if ($AsJson) {
    New-JsonView `
        -SummaryData $loaded.Data `
        -SummaryPath $summaryPath `
        -StatusSummary $statusSummary `
        -ElapsedStats $elapsedStats `
        -SortedResults $sortedResults `
        -Failures $failures `
        -ComparisonData $comparison `
        -BaselinePath $baselinePath | ConvertTo-Json -Depth 8
    exit 0
}

$selectedExamples = @(
    @($loaded.Data.selected_examples) |
        ForEach-Object { [string]$_ }
)

Write-Output ("summary: {0}" -f $summaryPath)
Write-Output ("profile: {0}" -f (Get-RunProfile -SummaryData $loaded.Data))
Write-Output ("examples: selected={0} results={1} ok={2} fail={3} other={4}" -f $selectedExamples.Count, @($sortedResults).Count, $statusSummary.OkCount, $statusSummary.FailCount, $statusSummary.OtherCount)
Write-Output ("elapsed_ms: total={0} avg={1} median={2} min={3} max={4}" -f $elapsedStats.TotalMs, $elapsedStats.AverageMs, $elapsedStats.MedianMs, $elapsedStats.MinMs, $elapsedStats.MaxMs)

Write-Output ""
Write-Output "slowest:"
foreach ($result in @($sortedResults | Select-Object -First $Top)) {
    Write-Output ("{0}|{1}|{2}ms" -f [string]$result.Example, [string]$result.Status, [int64]$result.ElapsedMs)
}

if ($ShowAllResults) {
    Write-Output ""
    Write-Output "all_results:"
    foreach ($result in @($sortedResults)) {
        Write-Output ("{0}|{1}|{2}ms" -f [string]$result.Example, [string]$result.Status, [int64]$result.ElapsedMs)
    }
}

if ($failures.Count -gt 0) {
    Write-Output ""
    Write-Output "failures:"
    foreach ($result in @($failures)) {
        Write-Output ("{0}|{1}|{2}ms|{3}" -f [string]$result.Example, [string]$result.Status, [int64]$result.ElapsedMs, [string]$result.Detail)
    }
}

if ($null -ne $comparison) {
    Write-Output ""
    Write-Output ("baseline: {0}" -f $baselinePath)
    Write-Output ("compare: matched={0} added={1} removed={2}" -f @($comparison.Matched).Count, @($comparison.Added).Count, @($comparison.Removed).Count)

    if (@($comparison.Regressions).Count -gt 0) {
        Write-Output ""
        Write-Output "regressions:"
        foreach ($entry in @($comparison.Regressions | Select-Object -First $Top)) {
            Write-Output ("{0}|{1}|current={2}ms|baseline={3}ms" -f [string]$entry.Example, (Format-DeltaText -Comparison $entry), [int64]$entry.CurrentElapsedMs, [int64]$entry.BaselineElapsedMs)
        }
    }

    if (@($comparison.Improvements).Count -gt 0) {
        Write-Output ""
        Write-Output "improvements:"
        foreach ($entry in @($comparison.Improvements | Select-Object -First $Top)) {
            Write-Output ("{0}|{1}|current={2}ms|baseline={3}ms" -f [string]$entry.Example, (Format-DeltaText -Comparison $entry), [int64]$entry.CurrentElapsedMs, [int64]$entry.BaselineElapsedMs)
        }
    }

    if (@($comparison.Added).Count -gt 0) {
        Write-Output ""
        Write-Output ("added_examples: {0}" -f (@($comparison.Added) -join ", "))
    }

    if (@($comparison.Removed).Count -gt 0) {
        Write-Output ""
        Write-Output ("removed_examples: {0}" -f (@($comparison.Removed) -join ", "))
    }
}
