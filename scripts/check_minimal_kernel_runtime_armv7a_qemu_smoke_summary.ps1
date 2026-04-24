param(
    [string]$Summary = "",
    [int]$MaxFailures = 0,
    [int]$MaxOtherResults = 0,
    [int]$RequireCaseCount = -1
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

function Add-Violation {
    param(
        [System.Collections.Generic.List[string]]$Violations,
        [string]$Message
    )

    if (-not [string]::IsNullOrWhiteSpace($Message)) {
        $Violations.Add($Message) | Out-Null
    }
}

$summaryPath = Resolve-FullPath -Path $Summary
if ([string]::IsNullOrWhiteSpace($summaryPath) -or -not (Test-Path $summaryPath)) {
    throw "summary not found: $Summary"
}

$summaryData = Get-Content -LiteralPath $summaryPath -Raw -Encoding utf8 | ConvertFrom-Json
if ([string]$summaryData.schema -ne "minimal_kernel.runtime_armv7a_qemu_smoke.summary/v1") {
    throw "unsupported qemu smoke summary schema: $([string]$summaryData.schema)"
}

$results = @($summaryData.results)
$okCount = @($results | Where-Object { [string]$_.Status -eq "ok" }).Count
$failCount = @($results | Where-Object { [string]$_.Status -eq "fail" }).Count
$otherCount = @($results | Where-Object { ([string]$_.Status -ne "ok") -and ([string]$_.Status -ne "fail") }).Count
$configureMs = if ($null -ne $summaryData.phase_elapsed_ms) { [int64]$summaryData.phase_elapsed_ms.configure } else { 0 }
$buildMs = if ($null -ne $summaryData.phase_elapsed_ms) { [int64]$summaryData.phase_elapsed_ms.build } else { 0 }

$violations = [System.Collections.Generic.List[string]]::new()
if ($failCount -gt $MaxFailures) {
    Add-Violation -Violations $violations -Message ("failures {0} exceed allowed {1}" -f $failCount, $MaxFailures)
}
if ($otherCount -gt $MaxOtherResults) {
    Add-Violation -Violations $violations -Message ("other results {0} exceed allowed {1}" -f $otherCount, $MaxOtherResults)
}
if ([int]$summaryData.completed_case_count -ne [int]$summaryData.case_count) {
    Add-Violation -Violations $violations -Message ("completed cases {0} do not match summary case_count {1}" -f [int]$summaryData.completed_case_count, [int]$summaryData.case_count)
}
if (@($results).Count -ne [int]$summaryData.completed_case_count) {
    Add-Violation -Violations $violations -Message ("result entries {0} do not match completed_case_count {1}" -f @($results).Count, [int]$summaryData.completed_case_count)
}
if ($RequireCaseCount -ge 0 -and @($results).Count -ne $RequireCaseCount) {
    Add-Violation -Violations $violations -Message ("completed cases {0} do not match required {1}" -f @($results).Count, $RequireCaseCount)
}
if ($null -ne $summaryData.fatal_failure) {
    Add-Violation -Violations $violations -Message ("fatal failure phase={0}: {1}" -f [string]$summaryData.fatal_failure.phase, [string]$summaryData.fatal_failure.message)
}

Write-Output ("summary: {0}" -f $summaryPath)
Write-Output "profile: armv7a-qemu-lower-half"
Write-Output ("status: ok={0} fail={1} other={2}" -f $okCount, $failCount, $otherCount)
Write-Output ("cases: completed={0} expected={1}" -f @($results).Count, [int]$summaryData.case_count)
Write-Output ("build_phase: configure={0}ms build={1}ms" -f $configureMs, $buildMs)

if ($null -ne $summaryData.fatal_failure) {
    Write-Output ("fatal_failure: phase={0} message={1}" -f [string]$summaryData.fatal_failure.phase, [string]$summaryData.fatal_failure.message)
}

if ($violations.Count -gt 0) {
    Write-Output "result: fail"
    Write-Output "violations:"
    foreach ($message in $violations) {
        Write-Output ("- {0}" -f $message)
    }
    exit 1
}

Write-Output "result: ok"
