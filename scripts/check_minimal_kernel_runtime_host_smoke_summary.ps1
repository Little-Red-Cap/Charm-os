param(
    [string]$Summary = "",
    [string]$BaselineSummary = "",
    [int]$MaxFailures = 0,
    [int]$MaxOtherResults = 0,
    [int64]$MaxTotalElapsedMs = -1,
    [int64]$MaxAverageElapsedMs = -1,
    [int64]$MaxMaxElapsedMs = -1,
    [int]$MaxRegressionCount = -1,
    [int64]$MaxRegressionMs = -1,
    [double]$MaxRegressionPct = -1,
    [switch]$RequireConfigureReused,
    [switch]$RequireConfigureExecuted,
    [switch]$AllowAddedExamples,
    [switch]$AllowRemovedExamples
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

function Get-RequiredPath {
    param(
        [string]$Path,
        [string]$Label
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "$Label path is required"
    }

    return Resolve-FullPath -Path $Path
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
    if ($null -eq $property) {
        return $Default
    }

    if ($null -eq $property.Value) {
        return $Default
    }

    return $property.Value
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

function Get-ObjectDoubleValue {
    param(
        $Object,
        [string]$Name,
        [double]$Default = 0
    )

    $value = Get-ObjectPropertyValue -Object $Object -Name $Name -Default $null
    if ($null -eq $value) {
        return $Default
    }

    return [double]$value
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

function Get-ArrayCount {
    param(
        $Value
    )

    if ($null -eq $Value) {
        return 0
    }

    if ($Value -is [string]) {
        return 1
    }

    if ($Value -is [System.Collections.ICollection]) {
        return $Value.Count
    }

    return @($Value | ForEach-Object { $_ }).Count
}

function Add-Violation {
    param(
        [System.Collections.Generic.List[string]]$Violations,
        [string]$Message
    )

    $Violations.Add($Message)
}

function Format-RegressionEntry {
    param(
        $Entry
    )

    $example = Get-ObjectStringValue -Object $Entry -Name "example"
    $deltaMs = Get-ObjectIntValue -Object $Entry -Name "delta_ms"
    $currentMs = Get-ObjectIntValue -Object $Entry -Name "current_elapsed_ms"
    $baselineMs = Get-ObjectIntValue -Object $Entry -Name "baseline_elapsed_ms"
    $text = "{0}|delta={1:+#;-#;0}ms|current={2}ms|baseline={3}ms" -f $example, $deltaMs, $currentMs, $baselineMs

    $deltaPct = Get-ObjectPropertyValue -Object $Entry -Name "delta_pct" -Default $null
    if ($null -ne $deltaPct) {
        $text += "|pct={0:+0.0;-0.0;0.0}%" -f [double]$deltaPct
    }

    return $text
}

function Invoke-InspectJson {
    param(
        [string]$InspectScript,
        [string]$SummaryPath,
        [string]$BaselinePath
    )

    $powershellExe = (Get-Command "powershell.exe" -ErrorAction Stop).Source
    $args = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $InspectScript,
        "-Summary", $SummaryPath,
        "-AsJson",
        "-Top", "1000000"
    )

    if (-not [string]::IsNullOrWhiteSpace($BaselinePath)) {
        $args += @("-BaselineSummary", $BaselinePath)
    }

    $output = & $powershellExe @args | Out-String
    if ($LASTEXITCODE -ne 0) {
        throw "inspect script failed with exit code $LASTEXITCODE"
    }

    return $output | ConvertFrom-Json
}

if ($RequireConfigureReused -and $RequireConfigureExecuted) {
    throw "RequireConfigureReused and RequireConfigureExecuted cannot be used together"
}

$summaryPath = Get-RequiredPath -Path $Summary -Label "summary"
$baselinePath = Get-OptionalPath -Path $BaselineSummary
$comparisonBudgetRequested = ($MaxRegressionCount -ge 0) -or ($MaxRegressionMs -ge 0) -or ($MaxRegressionPct -ge 0)
if ([string]::IsNullOrWhiteSpace($baselinePath) -and $comparisonBudgetRequested) {
    throw "BaselineSummary is required when regression budgets are configured"
}

$inspectScript = Join-Path $PSScriptRoot "inspect_minimal_kernel_runtime_host_smoke.ps1"
if (-not (Test-Path $inspectScript)) {
    throw "missing inspect script: $inspectScript"
}

$data = Invoke-InspectJson -InspectScript $inspectScript -SummaryPath $summaryPath -BaselinePath $baselinePath
$violations = [System.Collections.Generic.List[string]]::new()

$status = Get-ObjectPropertyValue -Object $data -Name "status" -Default $null
$failCount = Get-ObjectIntValue -Object $status -Name "fail"
$otherCount = Get-ObjectIntValue -Object $status -Name "other"
if ($failCount -gt $MaxFailures) {
    Add-Violation -Violations $violations -Message ("fail results {0} exceed allowed {1}" -f $failCount, $MaxFailures)
}
if ($otherCount -gt $MaxOtherResults) {
    Add-Violation -Violations $violations -Message ("other-status results {0} exceed allowed {1}" -f $otherCount, $MaxOtherResults)
}

$elapsed = Get-ObjectPropertyValue -Object $data -Name "elapsed_ms" -Default $null
$totalElapsedMs = Get-ObjectIntValue -Object $elapsed -Name "total"
$averageElapsedMs = Get-ObjectIntValue -Object $elapsed -Name "average"
$maxElapsedMs = Get-ObjectIntValue -Object $elapsed -Name "max"

if ($MaxTotalElapsedMs -ge 0 -and $totalElapsedMs -gt $MaxTotalElapsedMs) {
    Add-Violation -Violations $violations -Message ("total elapsed {0}ms exceed allowed {1}ms" -f $totalElapsedMs, $MaxTotalElapsedMs)
}
if ($MaxAverageElapsedMs -ge 0 -and $averageElapsedMs -gt $MaxAverageElapsedMs) {
    Add-Violation -Violations $violations -Message ("average elapsed {0}ms exceed allowed {1}ms" -f $averageElapsedMs, $MaxAverageElapsedMs)
}
if ($MaxMaxElapsedMs -ge 0 -and $maxElapsedMs -gt $MaxMaxElapsedMs) {
    Add-Violation -Violations $violations -Message ("max single example {0}ms exceed allowed {1}ms" -f $maxElapsedMs, $MaxMaxElapsedMs)
}

$phaseElapsed = Get-ObjectPropertyValue -Object $data -Name "phase_elapsed_ms" -Default $null
$configurePhase = Get-ObjectPropertyValue -Object $phaseElapsed -Name "configure" -Default $null
$exampleCount = Get-ObjectIntValue -Object $data -Name "example_count"
if ($RequireConfigureReused) {
    if ($null -eq $configurePhase) {
        Add-Violation -Violations $violations -Message "configure phase data is missing, cannot verify reuse"
    } else {
        $skippedCount = Get-ObjectIntValue -Object $configurePhase -Name "skipped_count"
        $executedCount = Get-ObjectIntValue -Object $configurePhase -Name "executed_count"
        if ($skippedCount -lt $exampleCount) {
            Add-Violation -Violations $violations -Message ("configure reused only {0}/{1} examples" -f $skippedCount, $exampleCount)
        }
        if ($executedCount -gt 0) {
            Add-Violation -Violations $violations -Message ("configure still executed for {0} examples" -f $executedCount)
        }
    }
}
if ($RequireConfigureExecuted) {
    if ($null -eq $configurePhase) {
        Add-Violation -Violations $violations -Message "configure phase data is missing, cannot verify cold configure"
    } else {
        $skippedCount = Get-ObjectIntValue -Object $configurePhase -Name "skipped_count"
        $executedCount = Get-ObjectIntValue -Object $configurePhase -Name "executed_count"
        if ($skippedCount -gt 0) {
            Add-Violation -Violations $violations -Message ("configure was reused for {0} examples" -f $skippedCount)
        }
        if ($executedCount -lt $exampleCount) {
            Add-Violation -Violations $violations -Message ("configure executed only {0}/{1} examples" -f $executedCount, $exampleCount)
        }
    }
}

$comparison = Get-ObjectPropertyValue -Object $data -Name "comparison" -Default $null
$regressions = @()
if ($null -ne $comparison) {
    $addedExamples = @(
        Get-ObjectPropertyValue -Object $comparison -Name "added_examples" -Default @()
    )
    $removedExamples = @(
        Get-ObjectPropertyValue -Object $comparison -Name "removed_examples" -Default @()
    )
    $regressions = @(
        Get-ObjectPropertyValue -Object $comparison -Name "regressions" -Default @()
    )

    if (-not $AllowAddedExamples -and $addedExamples.Count -gt 0) {
        Add-Violation -Violations $violations -Message ("added examples are not allowed: {0}" -f ($addedExamples -join ", "))
    }
    if (-not $AllowRemovedExamples -and $removedExamples.Count -gt 0) {
        Add-Violation -Violations $violations -Message ("removed examples are not allowed: {0}" -f ($removedExamples -join ", "))
    }
    if ($MaxRegressionCount -ge 0 -and $regressions.Count -gt $MaxRegressionCount) {
        Add-Violation -Violations $violations -Message ("regressions {0} exceed allowed {1}" -f $regressions.Count, $MaxRegressionCount)
    }
    if ($MaxRegressionMs -ge 0) {
        $overBudget = @(
            @($regressions) |
                Where-Object { (Get-ObjectIntValue -Object $_ -Name "delta_ms") -gt $MaxRegressionMs }
        )
        if ($overBudget.Count -gt 0) {
            $samples = @($overBudget | Select-Object -First 3 | ForEach-Object { Format-RegressionEntry -Entry $_ })
            Add-Violation -Violations $violations -Message ("regression delta exceeds {0}ms: {1}" -f $MaxRegressionMs, ($samples -join "; "))
        }
    }
    if ($MaxRegressionPct -ge 0) {
        $overPctBudget = @(
            @($regressions) |
                Where-Object {
                    $deltaPct = Get-ObjectPropertyValue -Object $_ -Name "delta_pct" -Default $null
                    ($null -ne $deltaPct) -and ([double]$deltaPct -gt $MaxRegressionPct)
                }
        )
        if ($overPctBudget.Count -gt 0) {
            $samples = @($overPctBudget | Select-Object -First 3 | ForEach-Object { Format-RegressionEntry -Entry $_ })
            Add-Violation -Violations $violations -Message ("regression percent exceeds {0}%: {1}" -f $MaxRegressionPct, ($samples -join "; "))
        }
    }
}

$profile = Get-ObjectStringValue -Object $data -Name "profile" -Default "custom"
Write-Output ("summary: {0}" -f $summaryPath)
Write-Output ("profile: {0}" -f $profile)
Write-Output ("status: ok={0} fail={1} other={2}" -f (Get-ObjectIntValue -Object $status -Name "ok"), $failCount, $otherCount)
Write-Output ("elapsed_ms: total={0} avg={1} max={2}" -f $totalElapsedMs, $averageElapsedMs, $maxElapsedMs)

if ($null -ne $configurePhase) {
    Write-Output ("configure_phase: executed={0} reused={1}" -f (Get-ObjectIntValue -Object $configurePhase -Name "executed_count"), (Get-ObjectIntValue -Object $configurePhase -Name "skipped_count"))
}

if ($null -ne $comparison) {
    Write-Output ("comparison: regressions={0} added={1} removed={2}" -f $regressions.Count, (Get-ArrayCount -Value (Get-ObjectPropertyValue -Object $comparison -Name "added_examples" -Default @())), (Get-ArrayCount -Value (Get-ObjectPropertyValue -Object $comparison -Name "removed_examples" -Default @())))
}

if ($violations.Count -gt 0) {
    Write-Output "result: fail"
    Write-Output "violations:"
    foreach ($message in $violations) {
        Write-Output ("- {0}" -f $message)
    }
    exit 1
}

if ($null -ne $comparison -and $regressions.Count -gt 0) {
    $topRegression = @($regressions | Select-Object -First 1)
    if ($topRegression.Count -gt 0) {
        Write-Output ("regression_headroom: {0}" -f (Format-RegressionEntry -Entry $topRegression[0]))
    }
}

Write-Output "result: ok"
exit 0
