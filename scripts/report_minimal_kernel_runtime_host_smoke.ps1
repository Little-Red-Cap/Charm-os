param(
    [string]$Summary = "",
    [string]$BaselineSummary = "",
    [string]$InspectJson = "",
    [string]$OutputPath = "",
    [string]$Title = "Minimal Kernel Host Smoke Report",
    [int]$Top = 10
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
        throw "summary path is required when InspectJson is not provided"
    }

    return Resolve-FullPath -Path $Summary
}

function Get-OptionalPath {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ""
    }

    return Resolve-FullPath -Path $Path
}

function Ensure-ParentDirectory {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent) -and -not (Test-Path $parent)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
}

function Get-ObjectPropertyValue {
    param(
        $Object,
        [string]$Name,
        $Default = $null
    )

    if ($null -eq $Object) {
        return $Default
    }

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property -or $null -eq $property.Value) {
        return $Default
    }

    return $property.Value
}

function Get-ObjectStringValue {
    param(
        $Object,
        [string]$Name,
        [string]$Default = ""
    )

    $value = Get-ObjectPropertyValue -Object $Object -Name $Name -Default $null
    if ($null -eq $value) {
        return $Default
    }

    return [string]$value
}

function Get-ObjectIntValue {
    param(
        $Object,
        [string]$Name,
        [int64]$Default = 0
    )

    $value = Get-ObjectPropertyValue -Object $Object -Name $Name -Default $null
    if ($null -eq $value) {
        return $Default
    }

    return [int64]$value
}

function Get-ObjectBoolValue {
    param(
        $Object,
        [string]$Name,
        [bool]$Default = $false
    )

    $value = Get-ObjectPropertyValue -Object $Object -Name $Name -Default $null
    if ($null -eq $value) {
        return $Default
    }

    return [bool]$value
}

function Get-ObjectArray {
    param(
        $Object,
        [string]$Name
    )

    $value = Get-ObjectPropertyValue -Object $Object -Name $Name -Default @()
    return @($value)
}

function Escape-MarkdownCell {
    param(
        [string]$Text
    )

    if ($null -eq $Text) {
        return ""
    }

    $escaped = $Text.Replace("|", "\|").Replace("`r", "")
    return $escaped.Replace("`n", "<br/>")
}

function Get-OutputReportPath {
    param(
        [string]$ExplicitPath,
        [string]$SummaryPath,
        [string]$InspectJsonPath
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        return Resolve-FullPath -Path $ExplicitPath
    }

    if (-not [string]::IsNullOrWhiteSpace($SummaryPath)) {
        return Join-Path (Split-Path -Parent $SummaryPath) "report.md"
    }

    if (-not [string]::IsNullOrWhiteSpace($InspectJsonPath)) {
        return Join-Path (Split-Path -Parent $InspectJsonPath) "report.md"
    }

    throw "unable to determine report output path"
}

function Load-InspectData {
    param(
        [string]$InspectJsonPath,
        [string]$SummaryPath,
        [string]$BaselinePath
    )

    if (-not [string]::IsNullOrWhiteSpace($InspectJsonPath)) {
        if (-not (Test-Path $InspectJsonPath)) {
            throw "inspect json not found: $InspectJsonPath"
        }

        return (Get-Content -LiteralPath $InspectJsonPath -Raw -Encoding utf8 | ConvertFrom-Json)
    }

    $inspectScript = Join-Path $PSScriptRoot "inspect_minimal_kernel_runtime_host_smoke.ps1"
    if (-not (Test-Path $inspectScript)) {
        throw "missing inspect script: $inspectScript"
    }

    $powershellExe = (Get-Command "powershell.exe" -ErrorAction Stop).Source
    $args = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $inspectScript,
        "-Summary", $SummaryPath,
        "-AsJson",
        "-Top", [string]$Top
    )

    if (-not [string]::IsNullOrWhiteSpace($BaselinePath)) {
        $args += @("-BaselineSummary", $BaselinePath)
    }

    $output = & $powershellExe @args | Out-String
    if ($LASTEXITCODE -ne 0) {
        throw "inspect script failed with exit code $LASTEXITCODE"
    }

    return ($output | ConvertFrom-Json)
}

function Format-ConfigureCell {
    param(
        $Entry
    )

    if (Get-ObjectBoolValue -Object $Entry -Name "configure_skipped") {
        return "skip"
    }

    if ($null -eq (Get-ObjectPropertyValue -Object $Entry -Name "configure_ms" -Default $null)) {
        return "-"
    }

    return ("{0}ms" -f (Get-ObjectIntValue -Object $Entry -Name "configure_ms"))
}

function Format-PhaseSummaryText {
    param(
        [string]$Name,
        $Phase
    )

    if ($null -eq $Phase) {
        return ""
    }

    $parts = [System.Collections.Generic.List[string]]::new()
    $parts.Add(("total={0}ms" -f (Get-ObjectIntValue -Object $Phase -Name "total_ms"))) | Out-Null
    $parts.Add(("executed={0}" -f (Get-ObjectIntValue -Object $Phase -Name "executed_count"))) | Out-Null

    $skippedCount = Get-ObjectIntValue -Object $Phase -Name "skipped_count" -Default -1
    if ($skippedCount -ge 0) {
        $parts.Add(("reused={0}" -f $skippedCount)) | Out-Null
    }

    $slowest = Get-ObjectPropertyValue -Object $Phase -Name "slowest" -Default $null
    if ($null -ne $slowest) {
        $parts.Add(("slowest={0}|{1}ms" -f (Get-ObjectStringValue -Object $slowest -Name "example"), (Get-ObjectIntValue -Object $slowest -Name "elapsed_ms"))) | Out-Null
    }

    return ("- `{0}`: {1}" -f $Name, ($parts -join ", "))
}

function Add-TableHeader {
    param(
        [System.Text.StringBuilder]$Builder,
        [string[]]$Columns
    )

    [void]$Builder.AppendLine(($Columns -join " | "))

    $separator = @($Columns | ForEach-Object { "---" })
    [void]$Builder.AppendLine(($separator -join " | "))
}

function Add-ResultRows {
    param(
        [System.Text.StringBuilder]$Builder,
        [object[]]$Results
    )

    foreach ($entry in @($Results)) {
        $exampleCell = Escape-MarkdownCell -Text (Get-ObjectStringValue -Object $entry -Name "example")
        $statusCell = Escape-MarkdownCell -Text (Get-ObjectStringValue -Object $entry -Name "status")
        $buildCell = if ($null -eq (Get-ObjectPropertyValue -Object $entry -Name "build_ms" -Default $null)) { "-" } else { "{0}ms" -f (Get-ObjectIntValue -Object $entry -Name "build_ms") }
        $runCell = if ($null -eq (Get-ObjectPropertyValue -Object $entry -Name "run_ms" -Default $null)) { "-" } else { "{0}ms" -f (Get-ObjectIntValue -Object $entry -Name "run_ms") }
        $columns = @(
            $exampleCell,
            $statusCell,
            ("{0}ms" -f (Get-ObjectIntValue -Object $entry -Name "elapsed_ms")),
            (Format-ConfigureCell -Entry $entry),
            $buildCell,
            $runCell
        )

        [void]$Builder.AppendLine(($columns -join " | "))
    }
}

function Add-ComparisonRows {
    param(
        [System.Text.StringBuilder]$Builder,
        [object[]]$Entries
    )

    foreach ($entry in @($Entries)) {
        $deltaPct = Get-ObjectPropertyValue -Object $entry -Name "delta_pct" -Default $null
        $deltaText = "{0:+#;-#;0}ms" -f (Get-ObjectIntValue -Object $entry -Name "delta_ms")
        if ($null -ne $deltaPct) {
            $deltaText += " ({0:+0.0;-0.0;0.0}%)" -f [double]$deltaPct
        }

        $exampleCell = Escape-MarkdownCell -Text (Get-ObjectStringValue -Object $entry -Name "example")
        $deltaCell = Escape-MarkdownCell -Text $deltaText
        $columns = @(
            $exampleCell,
            $deltaCell,
            ("{0}ms" -f (Get-ObjectIntValue -Object $entry -Name "current_elapsed_ms")),
            ("{0}ms" -f (Get-ObjectIntValue -Object $entry -Name "baseline_elapsed_ms"))
        )

        [void]$Builder.AppendLine(($columns -join " | "))
    }
}

$inspectJsonPath = Get-OptionalPath -Path $InspectJson
$summaryPath = ""
if ([string]::IsNullOrWhiteSpace($inspectJsonPath)) {
    $summaryPath = Get-RequiredSummaryPath
} elseif (-not [string]::IsNullOrWhiteSpace($Summary)) {
    $summaryPath = Resolve-FullPath -Path $Summary
}
$baselinePath = Get-OptionalPath -Path $BaselineSummary

if ($Top -lt 1) {
    throw "Top must be at least 1"
}

$data = Load-InspectData -InspectJsonPath $inspectJsonPath -SummaryPath $summaryPath -BaselinePath $baselinePath
$resolvedOutputPath = Get-OutputReportPath -ExplicitPath $OutputPath -SummaryPath $summaryPath -InspectJsonPath $inspectJsonPath

$summaryPathFromData = Get-ObjectStringValue -Object $data -Name "summary_path" -Default $summaryPath
$comparison = Get-ObjectPropertyValue -Object $data -Name "comparison" -Default $null
$comparisonBaselinePath = if ($null -eq $comparison) { "" } else { Get-ObjectStringValue -Object $comparison -Name "baseline_summary_path" -Default $baselinePath }
$status = Get-ObjectPropertyValue -Object $data -Name "status" -Default $null
$elapsed = Get-ObjectPropertyValue -Object $data -Name "elapsed_ms" -Default $null
$phaseElapsed = Get-ObjectPropertyValue -Object $data -Name "phase_elapsed_ms" -Default $null
$slowest = @(Get-ObjectArray -Object $data -Name "slowest")
$failures = @(Get-ObjectArray -Object $data -Name "failures")
$profileValue = Get-ObjectStringValue -Object $data -Name "profile" -Default "custom"
$generatedAtValue = Get-ObjectStringValue -Object $data -Name "generated_at"
$statusOkCount = Get-ObjectIntValue -Object $status -Name "ok"
$statusFailCount = Get-ObjectIntValue -Object $status -Name "fail"
$statusOtherCount = Get-ObjectIntValue -Object $status -Name "other"
$elapsedTotalMs = Get-ObjectIntValue -Object $elapsed -Name "total"
$elapsedAverageMs = Get-ObjectIntValue -Object $elapsed -Name "average"
$elapsedMedianMs = Get-ObjectIntValue -Object $elapsed -Name "median"
$elapsedMinMs = Get-ObjectIntValue -Object $elapsed -Name "min"
$elapsedMaxMs = Get-ObjectIntValue -Object $elapsed -Name "max"

$builder = [System.Text.StringBuilder]::new()
[void]$builder.AppendLine(("# {0}" -f $Title))
[void]$builder.AppendLine("")
[void]$builder.AppendLine(('- Profile: `{0}`' -f $profileValue))
[void]$builder.AppendLine(('- Generated at: `{0}`' -f $generatedAtValue))
[void]$builder.AppendLine(('- Status: `ok={0} fail={1} other={2}`' -f $statusOkCount, $statusFailCount, $statusOtherCount))
[void]$builder.AppendLine(('- Elapsed: `total={0}ms avg={1}ms median={2}ms min={3}ms max={4}ms`' -f $elapsedTotalMs, $elapsedAverageMs, $elapsedMedianMs, $elapsedMinMs, $elapsedMaxMs))
if (-not [string]::IsNullOrWhiteSpace($summaryPathFromData)) {
    [void]$builder.AppendLine(('- Summary JSON: `{0}`' -f $summaryPathFromData))
}
if (-not [string]::IsNullOrWhiteSpace($comparisonBaselinePath)) {
    [void]$builder.AppendLine(('- Baseline JSON: `{0}`' -f $comparisonBaselinePath))
}
[void]$builder.AppendLine("")
[void]$builder.AppendLine("## Phase Breakdown")

$phaseLines = @(
    Format-PhaseSummaryText -Name "configure" -Phase (Get-ObjectPropertyValue -Object $phaseElapsed -Name "configure" -Default $null)
    Format-PhaseSummaryText -Name "build" -Phase (Get-ObjectPropertyValue -Object $phaseElapsed -Name "build" -Default $null)
    Format-PhaseSummaryText -Name "run" -Phase (Get-ObjectPropertyValue -Object $phaseElapsed -Name "run" -Default $null)
) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }

if (@($phaseLines).Count -eq 0) {
    [void]$builder.AppendLine("- No phase timing data.")
} else {
    foreach ($line in $phaseLines) {
        [void]$builder.AppendLine($line)
    }
}

[void]$builder.AppendLine("")
[void]$builder.AppendLine("## Slowest Examples")
if (@($slowest).Count -eq 0) {
    [void]$builder.AppendLine("No results.")
} else {
    Add-TableHeader -Builder $builder -Columns @("Example", "Status", "Elapsed", "Configure", "Build", "Run")
    Add-ResultRows -Builder $builder -Results $slowest
}

if (@($failures).Count -gt 0) {
    [void]$builder.AppendLine("")
    [void]$builder.AppendLine("## Failures")
    foreach ($entry in @($failures)) {
        $example = Get-ObjectStringValue -Object $entry -Name "example"
        $phase = Get-ObjectStringValue -Object $entry -Name "failure_phase" -Default "-"
        $detail = Get-ObjectStringValue -Object $entry -Name "detail"
        $failureElapsedMs = Get-ObjectIntValue -Object $entry -Name "elapsed_ms"
        [void]$builder.AppendLine(('- `{0}` elapsed=`{1}ms` phase=`{2}`' -f $example, $failureElapsedMs, $phase))
        if (-not [string]::IsNullOrWhiteSpace($detail)) {
            [void]$builder.AppendLine("")
            [void]$builder.AppendLine('```text')
            [void]$builder.AppendLine($detail)
            [void]$builder.AppendLine('```')
        }
    }
}

if ($null -ne $comparison) {
    $regressions = @(Get-ObjectArray -Object $comparison -Name "regressions")
    $improvements = @(Get-ObjectArray -Object $comparison -Name "improvements")
    $addedExamples = @(Get-ObjectArray -Object $comparison -Name "added_examples")
    $removedExamples = @(Get-ObjectArray -Object $comparison -Name "removed_examples")
    $matchedExampleCount = Get-ObjectIntValue -Object $comparison -Name "matched_examples"

    [void]$builder.AppendLine("")
    [void]$builder.AppendLine("## Comparison")
    [void]$builder.AppendLine(('- Matched examples: `{0}`' -f $matchedExampleCount))
    [void]$builder.AppendLine(('- Regressions: `{0}`' -f @($regressions).Count))
    [void]$builder.AppendLine(('- Improvements: `{0}`' -f @($improvements).Count))
    [void]$builder.AppendLine(('- Added examples: `{0}`' -f @($addedExamples).Count))
    [void]$builder.AppendLine(('- Removed examples: `{0}`' -f @($removedExamples).Count))

    if (@($regressions).Count -gt 0) {
        [void]$builder.AppendLine("")
        [void]$builder.AppendLine("### Regressions")
        Add-TableHeader -Builder $builder -Columns @("Example", "Delta", "Current", "Baseline")
        Add-ComparisonRows -Builder $builder -Entries $regressions
    }

    if (@($improvements).Count -gt 0) {
        [void]$builder.AppendLine("")
        [void]$builder.AppendLine("### Improvements")
        Add-TableHeader -Builder $builder -Columns @("Example", "Delta", "Current", "Baseline")
        Add-ComparisonRows -Builder $builder -Entries $improvements
    }

    if (@($addedExamples).Count -gt 0) {
        $addedList = @($addedExamples | ForEach-Object { [string]$_ }) -join '`, `'
        [void]$builder.AppendLine("")
        [void]$builder.AppendLine(('Added: `{0}`' -f $addedList))
    }

    if (@($removedExamples).Count -gt 0) {
        $removedList = @($removedExamples | ForEach-Object { [string]$_ }) -join '`, `'
        [void]$builder.AppendLine("")
        [void]$builder.AppendLine(('Removed: `{0}`' -f $removedList))
    }
}

Ensure-ParentDirectory -Path $resolvedOutputPath
Set-Content -LiteralPath $resolvedOutputPath -Encoding utf8 ($builder.ToString())
Write-Host ("[MD] {0}" -f $resolvedOutputPath)
