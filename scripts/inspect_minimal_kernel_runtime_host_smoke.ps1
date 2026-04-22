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

function Get-EntryInt64Property {
    param(
        $Entry,
        [string]$Name
    )

    $property = $Entry.PSObject.Properties[$Name]
    if ($null -eq $property -or $null -eq $property.Value) {
        return $null
    }

    return [int64]$property.Value
}

function Get-EntryStringProperty {
    param(
        $Entry,
        [string]$Name
    )

    $property = $Entry.PSObject.Properties[$Name]
    if ($null -eq $property -or [string]::IsNullOrWhiteSpace([string]$property.Value)) {
        return ""
    }

    return [string]$property.Value
}

function Get-EntryBoolProperty {
    param(
        $Entry,
        [string]$Name
    )

    $property = $Entry.PSObject.Properties[$Name]
    if ($null -eq $property -or $null -eq $property.Value) {
        return $false
    }

    return [bool]$property.Value
}

function Convert-ToSummaryResult {
    param(
        $Entry
    )

    $configureMs = Get-EntryInt64Property -Entry $Entry -Name "ConfigureMs"
    $configureSkipped = Get-EntryBoolProperty -Entry $Entry -Name "ConfigureSkipped"
    $buildMs = Get-EntryInt64Property -Entry $Entry -Name "BuildMs"
    $runMs = Get-EntryInt64Property -Entry $Entry -Name "RunMs"
    $failurePhase = Get-EntryStringProperty -Entry $Entry -Name "FailurePhase"

    return [pscustomobject]@{
        Example        = [string]$Entry.Example
        Status         = [string]$Entry.Status
        ElapsedMs      = [int64]$Entry.ElapsedMs
        ConfigureMs    = $configureMs
        ConfigureSkipped = $configureSkipped
        BuildMs        = $buildMs
        RunMs          = $runMs
        FailurePhase   = $failurePhase
        HasPhaseTiming = ($null -ne $configureMs) -or ($null -ne $buildMs) -or ($null -ne $runMs)
        Detail         = [string]$Entry.Detail
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
    $skipConfigureIfPresent = $false
    $keepBuildDirs = $false

    if ($null -ne $SummaryData.PSObject.Properties["mode"] -and $null -ne $SummaryData.mode) {
        if ($null -ne $SummaryData.mode.PSObject.Properties["fresh"]) {
            $fresh = [bool]$SummaryData.mode.fresh
        }
        if ($null -ne $SummaryData.mode.PSObject.Properties["skip_configure_if_present"]) {
            $skipConfigureIfPresent = [bool]$SummaryData.mode.skip_configure_if_present
        }
        if ($null -ne $SummaryData.mode.PSObject.Properties["keep_build_dirs"]) {
            $keepBuildDirs = [bool]$SummaryData.mode.keep_build_dirs
        }
    }

    if ($skipConfigureIfPresent -and -not $fresh) {
        return "daily"
    }

    if ($fresh -and -not $skipConfigureIfPresent) {
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

function Get-PhaseMetricStats {
    param(
        [object[]]$Results,
        [string]$PropertyName,
        [string]$SkipPropertyName = ""
    )

    $phaseResults = @(
        @($Results) |
            Where-Object { $null -ne $_.$PropertyName }
    )

    if ($phaseResults.Count -eq 0) {
        return $null
    }

    $values = @(
        @($phaseResults) |
            ForEach-Object { [int64]$_.$PropertyName } |
            Sort-Object
    )

    $totalMs = 0
    foreach ($value in $values) {
        $totalMs += $value
    }

    $count = $values.Count
    if (($count % 2) -eq 1) {
        $medianMs = $values[[int]($count / 2)]
    } else {
        $left = $values[($count / 2) - 1]
        $right = $values[$count / 2]
        $medianMs = [int64][Math]::Round((($left + $right) / 2.0), 0)
    }

    $skippedCount = 0
    $executedResults = $phaseResults
    if (-not [string]::IsNullOrWhiteSpace($SkipPropertyName)) {
        $skippedCount = @(
            @($phaseResults) |
                Where-Object { $_.$SkipPropertyName }
        ).Count
        $executedResults = @(
            @($phaseResults) |
                Where-Object { -not $_.$SkipPropertyName }
        )
    }

    $executedCount = @($executedResults).Count
    $slowest = $null
    if ($executedCount -gt 0) {
        $slowest = @(
            @($executedResults) |
                Sort-Object -Property @{ Expression = { [int64]$_.($PropertyName) }; Descending = $true }, @{ Expression = { [string]$_.Example }; Descending = $false } |
                Select-Object -First 1
        )[0]
    }

    return [pscustomobject]@{
        Count          = $count
        ExecutedCount  = $executedCount
        SkippedCount   = $skippedCount
        AllSkipped     = ($count -gt 0 -and $executedCount -eq 0 -and $skippedCount -gt 0)
        TotalMs        = $totalMs
        AverageMs      = [int64][Math]::Round(($totalMs / [double]$count), 0)
        MedianMs       = $medianMs
        MinMs          = $values[0]
        MaxMs          = $values[$count - 1]
        SlowestExample = if ($null -eq $slowest) { "" } else { [string]$slowest.Example }
        SlowestMs      = if ($null -eq $slowest) { $null } else { [int64]$slowest.($PropertyName) }
    }
}

function Get-PhaseStats {
    param(
        [object[]]$Results
    )

    return [pscustomobject]@{
        Configure = Get-PhaseMetricStats -Results $Results -PropertyName "ConfigureMs" -SkipPropertyName "ConfigureSkipped"
        Build     = Get-PhaseMetricStats -Results $Results -PropertyName "BuildMs"
        Run       = Get-PhaseMetricStats -Results $Results -PropertyName "RunMs"
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

    $view = [ordered]@{
        example    = [string]$Result.Example
        status     = [string]$Result.Status
        elapsed_ms = [int64]$Result.ElapsedMs
    }

    if ($Result.HasPhaseTiming) {
        $view.configure_ms = $Result.ConfigureMs
        if ($Result.ConfigureSkipped) {
            $view.configure_skipped = $true
        }
        $view.build_ms = $Result.BuildMs
        $view.run_ms = $Result.RunMs
    }

    if (-not [string]::IsNullOrWhiteSpace([string]$Result.FailurePhase)) {
        $view.failure_phase = [string]$Result.FailurePhase
    }

    return $view
}

function New-FailureView {
    param(
        $Result
    )

    $view = [ordered]@{
        example    = [string]$Result.Example
        status     = [string]$Result.Status
        elapsed_ms = [int64]$Result.ElapsedMs
        detail     = [string]$Result.Detail
    }

    if ($Result.HasPhaseTiming) {
        $view.configure_ms = $Result.ConfigureMs
        if ($Result.ConfigureSkipped) {
            $view.configure_skipped = $true
        }
        $view.build_ms = $Result.BuildMs
        $view.run_ms = $Result.RunMs
    }

    if (-not [string]::IsNullOrWhiteSpace([string]$Result.FailurePhase)) {
        $view.failure_phase = [string]$Result.FailurePhase
    }

    return $view
}

function New-PhaseStatsView {
    param(
        $PhaseStat
    )

    if ($null -eq $PhaseStat) {
        return $null
    }

    return [ordered]@{
        count = $PhaseStat.Count
        executed_count = $PhaseStat.ExecutedCount
        total_ms = $PhaseStat.TotalMs
        average_ms = $PhaseStat.AverageMs
        median_ms = $PhaseStat.MedianMs
        min_ms = $PhaseStat.MinMs
        max_ms = $PhaseStat.MaxMs
    }
}

function Add-OptionalPhaseStatsViewFields {
    param(
        [System.Collections.IDictionary]$View,
        $PhaseStat
    )

    if ($PhaseStat.SkippedCount -gt 0) {
        $View.skipped_count = $PhaseStat.SkippedCount
    }

    if ($null -ne $PhaseStat.SlowestMs) {
        $View.slowest = [ordered]@{
            example = [string]$PhaseStat.SlowestExample
            elapsed_ms = [int64]$PhaseStat.SlowestMs
        }
    }
}

function Format-PhaseTotalsText {
    param(
        [string]$Name,
        $PhaseStat
    )

    $text = "{0}={1}ms" -f $Name, [int64]$PhaseStat.TotalMs
    if ($PhaseStat.SkippedCount -gt 0) {
        $text += "(reused={0})" -f [int]$PhaseStat.SkippedCount
    }

    return $text
}

function Format-PhaseSlowestText {
    param(
        [string]$Name,
        $PhaseStat
    )

    if ($PhaseStat.AllSkipped) {
        return "{0}=reused({1})" -f $Name, [int]$PhaseStat.SkippedCount
    }

    $text = "{0}={1}|{2}ms" -f $Name, [string]$PhaseStat.SlowestExample, [int64]$PhaseStat.SlowestMs
    if ($PhaseStat.SkippedCount -gt 0) {
        $text += "(reused={0})" -f [int]$PhaseStat.SkippedCount
    }

    return $text
}

function Get-PhaseValueText {
    param(
        $Value
    )

    if ($null -eq $Value) {
        return "-"
    }

    return ("{0}" -f [int64]$Value)
}

function Format-ResultText {
    param(
        $Result,
        [switch]$IncludeDetail
    )

    $text = ("{0}|{1}|{2}ms" -f [string]$Result.Example, [string]$Result.Status, [int64]$Result.ElapsedMs)
    if ($Result.HasPhaseTiming) {
        $configureText = if ($Result.ConfigureSkipped) { "skip" } else { "{0}ms" -f (Get-PhaseValueText -Value $Result.ConfigureMs) }
        $text += ("|cfg={0}|build={1}ms|run={2}ms" -f $configureText, (Get-PhaseValueText -Value $Result.BuildMs), (Get-PhaseValueText -Value $Result.RunMs))
    }
    if (-not [string]::IsNullOrWhiteSpace([string]$Result.FailurePhase)) {
        $text += ("|phase={0}" -f [string]$Result.FailurePhase)
    }
    if ($IncludeDetail) {
        $text += ("|{0}" -f [string]$Result.Detail)
    }

    return $text
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
        $PhaseStats,
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

    $phaseElapsedView = [ordered]@{}
    if ($null -ne $PhaseStats.Configure) {
        $configureView = New-PhaseStatsView -PhaseStat $PhaseStats.Configure
        Add-OptionalPhaseStatsViewFields -View $configureView -PhaseStat $PhaseStats.Configure
        $phaseElapsedView.configure = $configureView
    }
    if ($null -ne $PhaseStats.Build) {
        $buildView = New-PhaseStatsView -PhaseStat $PhaseStats.Build
        Add-OptionalPhaseStatsViewFields -View $buildView -PhaseStat $PhaseStats.Build
        $phaseElapsedView.build = $buildView
    }
    if ($null -ne $PhaseStats.Run) {
        $runView = New-PhaseStatsView -PhaseStat $PhaseStats.Run
        Add-OptionalPhaseStatsViewFields -View $runView -PhaseStat $PhaseStats.Run
        $phaseElapsedView.run = $runView
    }
    if ($phaseElapsedView.Count -gt 0) {
        $view.phase_elapsed_ms = $phaseElapsedView
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
$phaseStats = Get-PhaseStats -Results $sortedResults

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
        -PhaseStats $phaseStats `
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

if ($null -ne $phaseStats.Configure -or $null -ne $phaseStats.Build -or $null -ne $phaseStats.Run) {
    $phaseTotals = [System.Collections.Generic.List[string]]::new()
    $phaseSlowest = [System.Collections.Generic.List[string]]::new()
    if ($null -ne $phaseStats.Configure) {
        $phaseTotals.Add((Format-PhaseTotalsText -Name "configure" -PhaseStat $phaseStats.Configure))
        $phaseSlowest.Add((Format-PhaseSlowestText -Name "configure" -PhaseStat $phaseStats.Configure))
    }
    if ($null -ne $phaseStats.Build) {
        $phaseTotals.Add((Format-PhaseTotalsText -Name "build" -PhaseStat $phaseStats.Build))
        $phaseSlowest.Add((Format-PhaseSlowestText -Name "build" -PhaseStat $phaseStats.Build))
    }
    if ($null -ne $phaseStats.Run) {
        $phaseTotals.Add((Format-PhaseTotalsText -Name "run" -PhaseStat $phaseStats.Run))
        $phaseSlowest.Add((Format-PhaseSlowestText -Name "run" -PhaseStat $phaseStats.Run))
    }

    Write-Output ("phase_elapsed_ms: {0}" -f ($phaseTotals -join " "))
    Write-Output ("phase_slowest: {0}" -f ($phaseSlowest -join " "))
}

Write-Output ""
Write-Output "slowest:"
foreach ($result in @($sortedResults | Select-Object -First $Top)) {
    Write-Output (Format-ResultText -Result $result)
}

if ($ShowAllResults) {
    Write-Output ""
    Write-Output "all_results:"
    foreach ($result in @($sortedResults)) {
        Write-Output (Format-ResultText -Result $result)
    }
}

if ($failures.Count -gt 0) {
    Write-Output ""
    Write-Output "failures:"
    foreach ($result in @($failures)) {
        Write-Output (Format-ResultText -Result $result -IncludeDetail)
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
