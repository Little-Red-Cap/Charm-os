param(
    [string]$Summary = "",
    [string]$OutputPath = "",
    [string]$Title = "Minimal Kernel ARMv7-A QEMU Lower-Half Smoke Report",
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

function Get-OutputReportPath {
    param(
        [string]$ExplicitPath,
        [string]$SummaryPath
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        return Resolve-FullPath -Path $ExplicitPath
    }

    return Join-Path (Split-Path -Parent $SummaryPath) "report.md"
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
            TotalMs = 0
            AverageMs = 0
            MedianMs = 0
            MinMs = 0
            MaxMs = 0
        }
    }

    $count = $elapsedValues.Count
    $totalMs = [int64]0
    foreach ($value in $elapsedValues) {
        $totalMs += [int64]$value
    }

    $medianMs = if (($count % 2) -eq 1) {
        $elapsedValues[[int](($count - 1) / 2)]
    } else {
        [int64][Math]::Round((($elapsedValues[($count / 2) - 1] + $elapsedValues[$count / 2]) / 2.0), 0)
    }

    return [pscustomobject]@{
        TotalMs = $totalMs
        AverageMs = [int64][Math]::Round(($totalMs / [double]$count), 0)
        MedianMs = $medianMs
        MinMs = $elapsedValues[0]
        MaxMs = $elapsedValues[$count - 1]
    }
}

if ($Top -lt 1) {
    throw "Top must be at least 1"
}

$summaryPath = Resolve-FullPath -Path $Summary
if ([string]::IsNullOrWhiteSpace($summaryPath) -or -not (Test-Path $summaryPath)) {
    throw "summary not found: $Summary"
}

$summaryData = Get-Content -LiteralPath $summaryPath -Raw -Encoding utf8 | ConvertFrom-Json
if ([string]$summaryData.schema -ne "minimal_kernel.runtime_armv7a_qemu_smoke.summary/v1") {
    throw "unsupported qemu smoke summary schema: $([string]$summaryData.schema)"
}

$results = @(
    @($summaryData.results) |
        Sort-Object @{ Expression = { [int64]$_.ElapsedMs }; Descending = $true }, @{ Expression = { [string]$_.Case } }
)
$failures = @($results | Where-Object { [string]$_.Status -ne "ok" })

$okCount = @($results | Where-Object { [string]$_.Status -eq "ok" }).Count
$failCount = @($results | Where-Object { [string]$_.Status -eq "fail" }).Count
$otherCount = @($results | Where-Object { ([string]$_.Status -ne "ok") -and ([string]$_.Status -ne "fail") }).Count

$elapsedStats = Get-ElapsedStats -Results $results
$configureMs = if ($null -ne $summaryData.phase_elapsed_ms) { [int64]$summaryData.phase_elapsed_ms.configure } else { 0 }
$buildMs = if ($null -ne $summaryData.phase_elapsed_ms) { [int64]$summaryData.phase_elapsed_ms.build } else { 0 }
$resolvedOutputPath = Get-OutputReportPath -ExplicitPath $OutputPath -SummaryPath $summaryPath

$builder = [System.Text.StringBuilder]::new()
[void]$builder.AppendLine(("# {0}" -f $Title))
[void]$builder.AppendLine("")
[void]$builder.AppendLine('- Profile: `armv7a-qemu-lower-half`')
[void]$builder.AppendLine(('- Generated at: `{0}`' -f [string]$summaryData.generated_at))
[void]$builder.AppendLine(('- Status: `ok={0} fail={1} other={2}`' -f $okCount, $failCount, $otherCount))
[void]$builder.AppendLine(('- Cases: `completed={0} expected={1}`' -f @($results).Count, [int]$summaryData.case_count))
[void]$builder.AppendLine(('- Elapsed: `total={0}ms avg={1}ms median={2}ms min={3}ms max={4}ms`' -f $elapsedStats.TotalMs, $elapsedStats.AverageMs, $elapsedStats.MedianMs, $elapsedStats.MinMs, $elapsedStats.MaxMs))
[void]$builder.AppendLine(('- Build phases: `configure={0}ms build={1}ms`' -f $configureMs, $buildMs))
[void]$builder.AppendLine(('- Summary JSON: `{0}`' -f $summaryPath))
if (-not [string]::IsNullOrWhiteSpace([string]$summaryData.case_artifacts_root)) {
    [void]$builder.AppendLine(('- Case artifacts: `{0}`' -f [string]$summaryData.case_artifacts_root))
}
[void]$builder.AppendLine("")
[void]$builder.AppendLine("## Cases")
[void]$builder.AppendLine("Case | Status | Elapsed | Script")
[void]$builder.AppendLine("--- | --- | --- | ---")
foreach ($entry in @($results | Select-Object -First $Top)) {
    [void]$builder.AppendLine(("{0} | {1} | {2}ms | {3}" -f [string]$entry.Label, [string]$entry.Status, [int64]$entry.ElapsedMs, [string]$entry.Script))
}

$highlightResults = @(
    $results | Where-Object {
        $null -ne $_.Highlights -and (@($_.Highlights)).Count -gt 0
    }
)

if ($highlightResults.Count -gt 0) {
    [void]$builder.AppendLine("")
    [void]$builder.AppendLine("## Evidence Highlights")
    foreach ($entry in @($highlightResults)) {
        [void]$builder.AppendLine(("### {0}" -f [string]$entry.Label))
        foreach ($highlight in @($entry.Highlights)) {
            [void]$builder.AppendLine(('- `{0}`' -f [string]$highlight))
        }
        [void]$builder.AppendLine("")
    }
}

if ($null -ne $summaryData.fatal_failure) {
    [void]$builder.AppendLine("")
    [void]$builder.AppendLine("## Fatal Failure")
    [void]$builder.AppendLine(('- Phase: `{0}`' -f [string]$summaryData.fatal_failure.phase))
    [void]$builder.AppendLine(('- Message: `{0}`' -f [string]$summaryData.fatal_failure.message))
}

if (@($failures).Count -gt 0) {
    [void]$builder.AppendLine("")
    [void]$builder.AppendLine("## Failure Details")
    foreach ($entry in @($failures)) {
        [void]$builder.AppendLine(('- `{0}` elapsed=`{1}ms`' -f [string]$entry.Label, [int64]$entry.ElapsedMs))
        [void]$builder.AppendLine(('  detail: `{0}`' -f [string]$entry.Detail))
        if (-not [string]::IsNullOrWhiteSpace([string]$entry.StdoutLogPath)) {
            [void]$builder.AppendLine(('  stdout: `{0}`' -f [string]$entry.StdoutLogPath))
        }
        if (-not [string]::IsNullOrWhiteSpace([string]$entry.StderrLogPath)) {
            [void]$builder.AppendLine(('  stderr: `{0}`' -f [string]$entry.StderrLogPath))
        }
    }
}

Ensure-ParentDirectory -Path $resolvedOutputPath
Set-Content -LiteralPath $resolvedOutputPath -Encoding utf8 ($builder.ToString())
Write-Host ("[MD] {0}" -f $resolvedOutputPath)
