param(
    [string]$OutputRoot = "",
    [string]$SummaryPath = "",
    [string]$ReportMarkdownPath = "",
    [string]$CheckTextPath = "",
    [string]$HostOutputRoot = "",
    [string]$QemuOutputRoot = "",
    [string]$CMakeExe = "cmake",
    [string]$Generator = "Ninja",
    [string]$QemuExe = "qemu-system-arm",
    [int]$HostJobs = 0,
    [int]$QemuBuildJobs = 1,
    [int]$QemuTimeoutSec = 30,
    [int]$QemuTailLines = 40,
    [switch]$Clean,
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

    $Arguments.Add($Name) | Out-Null
    foreach ($value in @($Values)) {
        if (-not [string]::IsNullOrWhiteSpace([string]$value)) {
            $Arguments.Add([string]$value) | Out-Null
        }
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
        [string]$FailureMessage,
        [switch]$AllowFailure
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

    if ($exitCode -ne 0 -and -not $AllowFailure) {
        throw ("{0} (exit code {1})" -f $FailureMessage, $exitCode)
    }

    return $exitCode
}

function Load-JsonFile {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path $Path)) {
        return $null
    }

    return (Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json)
}

function Get-ElapsedStatsFromResults {
    param(
        [object[]]$Results
    )

    $elapsedValues = @(
        @($Results) |
            ForEach-Object { [int64]$_.ElapsedMs } |
            Sort-Object
    )

    if ($elapsedValues.Count -eq 0) {
        return [ordered]@{
            total = 0
            average = 0
            median = 0
            min = 0
            max = 0
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

    return [ordered]@{
        total = $totalMs
        average = [int64][Math]::Round(($totalMs / [double]$count), 0)
        median = $medianMs
        min = $elapsedValues[0]
        max = $elapsedValues[$count - 1]
    }
}

function Get-HostInspectView {
    param(
        $InspectData,
        [string]$SummaryPath,
        [string]$InspectPath,
        [string]$ReportPath,
        [string]$CheckPath
    )

    if ($null -eq $InspectData) {
        return $null
    }

    $view = [ordered]@{
        summary_path = $SummaryPath
        inspect_json_path = $InspectPath
        report_markdown_path = $ReportPath
        check_text_path = $CheckPath
        profile = [string]$InspectData.profile
        example_count = [int]$InspectData.example_count
        status = [ordered]@{
            ok = [int]$InspectData.status.ok
            fail = [int]$InspectData.status.fail
            other = [int]$InspectData.status.other
        }
        elapsed_ms = [ordered]@{
            total = [int64]$InspectData.elapsed_ms.total
            average = [int64]$InspectData.elapsed_ms.average
            median = [int64]$InspectData.elapsed_ms.median
            min = [int64]$InspectData.elapsed_ms.min
            max = [int64]$InspectData.elapsed_ms.max
        }
    }

    if ($null -ne $InspectData.phase_elapsed_ms -and $null -ne $InspectData.phase_elapsed_ms.configure) {
        $configurePhase = $InspectData.phase_elapsed_ms.configure
        $configureView = [ordered]@{
            total = [int64]$configurePhase.total_ms
            executed = [int]$configurePhase.executed_count
        }

        if ($null -ne $configurePhase.PSObject.Properties["skipped_count"]) {
            $configureView.reused = [int]$configurePhase.skipped_count
        }

        $view.configure = $configureView
    }

    if ($null -ne $InspectData.comparison) {
        $view.comparison = [ordered]@{
            baseline_summary_path = [string]$InspectData.comparison.baseline_summary_path
            matched_examples = [int]$InspectData.comparison.matched_examples
            regressions = @($InspectData.comparison.regressions).Count
            improvements = @($InspectData.comparison.improvements).Count
            added_examples = @($InspectData.comparison.added_examples).Count
            removed_examples = @($InspectData.comparison.removed_examples).Count
        }
    }

    return $view
}

function Get-QemuSummaryView {
    param(
        $SummaryData,
        [string]$SummaryPath,
        [string]$ReportPath,
        [string]$CheckPath
    )

    if ($null -eq $SummaryData) {
        return $null
    }

    $results = @($SummaryData.results)
    $okCount = @($results | Where-Object { [string]$_.Status -eq "ok" }).Count
    $failCount = @($results | Where-Object { [string]$_.Status -eq "fail" }).Count
    $otherCount = @($results | Where-Object { ([string]$_.Status -ne "ok") -and ([string]$_.Status -ne "fail") }).Count

    $view = [ordered]@{
        summary_path = $SummaryPath
        report_markdown_path = $ReportPath
        check_text_path = $CheckPath
        case_count = [int]$SummaryData.case_count
        completed_case_count = [int]$SummaryData.completed_case_count
        status = [ordered]@{
            ok = $okCount
            fail = $failCount
            other = $otherCount
        }
        elapsed_ms = Get-ElapsedStatsFromResults -Results $results
        phase_elapsed_ms = [ordered]@{
            configure = [int64]$SummaryData.phase_elapsed_ms.configure
            build = [int64]$SummaryData.phase_elapsed_ms.build
        }
        results = @(
            @($results) |
                ForEach-Object {
                    [ordered]@{
                        case = [string]$_.Case
                        label = [string]$_.Label
                        status = [string]$_.Status
                        elapsed_ms = [int64]$_.ElapsedMs
                        stdout_log_path = [string]$_.StdoutLogPath
                        stderr_log_path = [string]$_.StderrLogPath
                        detail = [string]$_.Detail
                    }
                }
        )
    }

    if ($null -ne $SummaryData.fatal_failure) {
        $view.fatal_failure = [ordered]@{
            phase = [string]$SummaryData.fatal_failure.phase
            message = [string]$SummaryData.fatal_failure.message
        }
    }

    return $view
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$resolvedOutputRoot = if ([string]::IsNullOrWhiteSpace($OutputRoot)) { "out/minimal-kernel-runtime-evidence" } else { $OutputRoot }
$outputRootPath = Resolve-FullPath -Path $resolvedOutputRoot
$resolvedHostOutputRoot = if ([string]::IsNullOrWhiteSpace($HostOutputRoot)) { Join-Path $outputRootPath "host" } else { Resolve-FullPath -Path $HostOutputRoot }
$resolvedQemuOutputRoot = if ([string]::IsNullOrWhiteSpace($QemuOutputRoot)) { Join-Path $outputRootPath "qemu" } else { Resolve-FullPath -Path $QemuOutputRoot }

if ($Clean) {
    Remove-PathIfExists -Path $outputRootPath
}
Ensure-Directory -Path $outputRootPath

$summaryPathResolved = Get-OutputPath -ExplicitPath $SummaryPath -OutputRootPath $outputRootPath -DefaultFileName "summary.json"
$reportMarkdownPathResolved = Get-OutputPath -ExplicitPath $ReportMarkdownPath -OutputRootPath $outputRootPath -DefaultFileName "report.md"
$checkTextPathResolved = Get-OutputPath -ExplicitPath $CheckTextPath -OutputRootPath $outputRootPath -DefaultFileName "check.txt"
$hostBundleLogPathResolved = Get-OutputPath -ExplicitPath "" -OutputRootPath $outputRootPath -DefaultFileName "host.bundle.log"
$qemuBundleLogPathResolved = Get-OutputPath -ExplicitPath "" -OutputRootPath $outputRootPath -DefaultFileName "qemu.bundle.log"

$powerShellExe = Resolve-ToolPath -Candidates @("powershell.exe", "pwsh.exe", "powershell", "pwsh")
$hostBundleScript = Join-Path $PSScriptRoot "minimal_kernel_runtime_host_smoke_dual_bundle.ps1"
$qemuBundleScript = Join-Path $PSScriptRoot "minimal_kernel_runtime_armv7a_qemu_smoke_bundle.ps1"
foreach ($scriptPath in @($hostBundleScript, $qemuBundleScript)) {
    if (-not (Test-Path $scriptPath)) {
        throw "missing script: $scriptPath"
    }
}

$hostArgs = [System.Collections.Generic.List[string]]::new()
Add-ScriptArgument -Arguments $hostArgs -Name "-OutputRoot" -Value $resolvedHostOutputRoot
Add-ScriptArgument -Arguments $hostArgs -Name "-CMakeExe" -Value $CMakeExe
Add-ScriptArgument -Arguments $hostArgs -Name "-Generator" -Value $Generator
Add-ScriptArgument -Arguments $hostArgs -Name "-Jobs" -Value (Format-Number -Value $HostJobs)
if ($Clean) {
    $hostArgs.Add("-Clean") | Out-Null
}
Add-StringArrayScriptArgument -Arguments $hostArgs -Name "-Examples" -Values $HostExamples

$qemuArgs = [System.Collections.Generic.List[string]]::new()
Add-ScriptArgument -Arguments $qemuArgs -Name "-OutputRoot" -Value $resolvedQemuOutputRoot
Add-ScriptArgument -Arguments $qemuArgs -Name "-CMakeExe" -Value $CMakeExe
Add-ScriptArgument -Arguments $qemuArgs -Name "-QemuExe" -Value $QemuExe
Add-ScriptArgument -Arguments $qemuArgs -Name "-BuildJobs" -Value (Format-Number -Value $QemuBuildJobs)
Add-ScriptArgument -Arguments $qemuArgs -Name "-TimeoutSec" -Value (Format-Number -Value $QemuTimeoutSec)
Add-ScriptArgument -Arguments $qemuArgs -Name "-TailLines" -Value (Format-Number -Value $QemuTailLines)
if ($Clean) {
    $qemuArgs.Add("-Clean") | Out-Null
}

$hostCiSummaryPath = Join-Path $resolvedHostOutputRoot "ci\summary.json"
$hostCiInspectPath = Join-Path $resolvedHostOutputRoot "ci\inspect.json"
$hostCiReportPath = Join-Path $resolvedHostOutputRoot "ci\report.md"
$hostCiCheckPath = Join-Path $resolvedHostOutputRoot "ci\check.txt"
$hostDailySummaryPath = Join-Path $resolvedHostOutputRoot "daily\summary.json"
$hostDailyInspectPath = Join-Path $resolvedHostOutputRoot "daily\inspect.json"
$hostDailyReportPath = Join-Path $resolvedHostOutputRoot "daily\report.md"
$hostDailyCheckPath = Join-Path $resolvedHostOutputRoot "daily\check.txt"
$qemuSummaryPath = Join-Path $resolvedQemuOutputRoot "summary.json"
$qemuReportPath = Join-Path $resolvedQemuOutputRoot "report.md"
$qemuCheckPath = Join-Path $resolvedQemuOutputRoot "check.txt"

$hostBundleExitCode = 0
$qemuBundleExitCode = 0
$violations = [System.Collections.Generic.List[string]]::new()

Push-Location $repoRoot
try {
    $hostBundleExitCode = Invoke-PowerShellFile `
        -PowerShellExe $powerShellExe `
        -ScriptPath $hostBundleScript `
        -ArgumentList $hostArgs.ToArray() `
        -LogPath $hostBundleLogPathResolved `
        -FailureMessage "minimal kernel host dual bundle failed" `
        -AllowFailure

    $qemuBundleExitCode = Invoke-PowerShellFile `
        -PowerShellExe $powerShellExe `
        -ScriptPath $qemuBundleScript `
        -ArgumentList $qemuArgs.ToArray() `
        -LogPath $qemuBundleLogPathResolved `
        -FailureMessage "minimal kernel armv7a qemu bundle failed" `
        -AllowFailure
} finally {
    Pop-Location
}

$hostCiInspectData = Load-JsonFile -Path $hostCiInspectPath
$hostDailyInspectData = Load-JsonFile -Path $hostDailyInspectPath
$qemuSummaryData = Load-JsonFile -Path $qemuSummaryPath

if ($null -eq $hostCiInspectData) {
    $violations.Add("missing host cold inspect json") | Out-Null
}
if ($null -eq $hostDailyInspectData) {
    $violations.Add("missing host warm inspect json") | Out-Null
}
if ($null -eq $qemuSummaryData) {
    $violations.Add("missing qemu summary json") | Out-Null
}
if ($hostBundleExitCode -ne 0) {
    $violations.Add(("host dual bundle exit code {0}" -f $hostBundleExitCode)) | Out-Null
}
if ($qemuBundleExitCode -ne 0) {
    $violations.Add(("qemu bundle exit code {0}" -f $qemuBundleExitCode)) | Out-Null
}

$hostView = [ordered]@{
    output_root = $resolvedHostOutputRoot
    bundle_log_path = $hostBundleLogPathResolved
    bundle_exit_code = $hostBundleExitCode
    cold = Get-HostInspectView `
        -InspectData $hostCiInspectData `
        -SummaryPath $hostCiSummaryPath `
        -InspectPath $hostCiInspectPath `
        -ReportPath $hostCiReportPath `
        -CheckPath $hostCiCheckPath
    warm = Get-HostInspectView `
        -InspectData $hostDailyInspectData `
        -SummaryPath $hostDailySummaryPath `
        -InspectPath $hostDailyInspectPath `
        -ReportPath $hostDailyReportPath `
        -CheckPath $hostDailyCheckPath
}

$qemuView = [ordered]@{
    output_root = $resolvedQemuOutputRoot
    bundle_log_path = $qemuBundleLogPathResolved
    bundle_exit_code = $qemuBundleExitCode
    lower_half = Get-QemuSummaryView `
        -SummaryData $qemuSummaryData `
        -SummaryPath $qemuSummaryPath `
        -ReportPath $qemuReportPath `
        -CheckPath $qemuCheckPath
}

$summaryObject = [ordered]@{
    schema = "minimal_kernel.runtime_evidence_bundle.summary/v1"
    generated_at = (Get-Date).ToString("o")
    result = if ($violations.Count -eq 0) { "ok" } else { "fail" }
    output_root = $outputRootPath
    report_markdown_path = $reportMarkdownPathResolved
    check_text_path = $checkTextPathResolved
    host = $hostView
    qemu = $qemuView
    violations = @($violations)
}

Ensure-ParentDirectory -Path $summaryPathResolved
$summaryObject | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $summaryPathResolved -Encoding utf8

$reportBuilder = [System.Text.StringBuilder]::new()
[void]$reportBuilder.AppendLine("# Minimal Kernel Runtime Evidence Bundle Report")
[void]$reportBuilder.AppendLine("")
[void]$reportBuilder.AppendLine(('- Generated at: `{0}`' -f [string]$summaryObject.generated_at))
[void]$reportBuilder.AppendLine(('- Result: `{0}`' -f [string]$summaryObject.result))
[void]$reportBuilder.AppendLine(('- Summary JSON: `{0}`' -f $summaryPathResolved))
[void]$reportBuilder.AppendLine(('- Output root: `{0}`' -f $outputRootPath))

[void]$reportBuilder.AppendLine("")
[void]$reportBuilder.AppendLine("## Upper-Half Host Evidence")
[void]$reportBuilder.AppendLine(('- Bundle log: `{0}`' -f $hostBundleLogPathResolved))
if ($null -ne $hostView.cold) {
    [void]$reportBuilder.AppendLine(('- Cold start: `ok={0} fail={1} other={2} total={3}ms configure={4}ms`' -f $hostView.cold.status.ok, $hostView.cold.status.fail, $hostView.cold.status.other, $hostView.cold.elapsed_ms.total, $hostView.cold.configure.total))
    [void]$reportBuilder.AppendLine(('- Cold report: `{0}`' -f $hostView.cold.report_markdown_path))
}
if ($null -ne $hostView.warm) {
    $warmReuseCount = if ($null -ne $hostView.warm.configure) { [int]$hostView.warm.configure.reused } else { 0 }
    [void]$reportBuilder.AppendLine(('- Warm reuse: `ok={0} fail={1} other={2} total={3}ms configure_reused={4}`' -f $hostView.warm.status.ok, $hostView.warm.status.fail, $hostView.warm.status.other, $hostView.warm.elapsed_ms.total, $warmReuseCount))
    if ($null -ne $hostView.warm.comparison) {
        [void]$reportBuilder.AppendLine(('- Warm compare: `matched={0} regressions={1} improvements={2}`' -f $hostView.warm.comparison.matched_examples, $hostView.warm.comparison.regressions, $hostView.warm.comparison.improvements))
    }
    [void]$reportBuilder.AppendLine(('- Warm report: `{0}`' -f $hostView.warm.report_markdown_path))
}

[void]$reportBuilder.AppendLine("")
[void]$reportBuilder.AppendLine("## Lower-Half QEMU Evidence")
[void]$reportBuilder.AppendLine(('- Bundle log: `{0}`' -f $qemuBundleLogPathResolved))
if ($null -ne $qemuView.lower_half) {
    [void]$reportBuilder.AppendLine(('- Lower-half status: `ok={0} fail={1} other={2}`' -f $qemuView.lower_half.status.ok, $qemuView.lower_half.status.fail, $qemuView.lower_half.status.other))
    [void]$reportBuilder.AppendLine(('- Cases: `completed={0} expected={1}`' -f $qemuView.lower_half.completed_case_count, $qemuView.lower_half.case_count))
    [void]$reportBuilder.AppendLine(('- Elapsed: `total={0}ms avg={1}ms max={2}ms`' -f $qemuView.lower_half.elapsed_ms.total, $qemuView.lower_half.elapsed_ms.average, $qemuView.lower_half.elapsed_ms.max))
    [void]$reportBuilder.AppendLine(('- Build phases: `configure={0}ms build={1}ms`' -f $qemuView.lower_half.phase_elapsed_ms.configure, $qemuView.lower_half.phase_elapsed_ms.build))
    [void]$reportBuilder.AppendLine(('- QEMU report: `{0}`' -f $qemuView.lower_half.report_markdown_path))
    [void]$reportBuilder.AppendLine("")
    [void]$reportBuilder.AppendLine("### QEMU Cases")
    [void]$reportBuilder.AppendLine("Case | Status | Elapsed")
    [void]$reportBuilder.AppendLine("--- | --- | ---")
    foreach ($entry in @($qemuView.lower_half.results)) {
        [void]$reportBuilder.AppendLine(("{0} | {1} | {2}ms" -f [string]$entry.label, [string]$entry.status, [int64]$entry.elapsed_ms))
    }
}

if ($violations.Count -gt 0) {
    [void]$reportBuilder.AppendLine("")
    [void]$reportBuilder.AppendLine("## Violations")
    foreach ($message in $violations) {
        [void]$reportBuilder.AppendLine(("- {0}" -f $message))
    }
}

Ensure-ParentDirectory -Path $reportMarkdownPathResolved
Set-Content -LiteralPath $reportMarkdownPathResolved -Encoding utf8 ($reportBuilder.ToString())

$checkBuilder = [System.Text.StringBuilder]::new()
[void]$checkBuilder.AppendLine(("summary: {0}" -f $summaryPathResolved))
[void]$checkBuilder.AppendLine(("result: {0}" -f [string]$summaryObject.result))
[void]$checkBuilder.AppendLine(("host_bundle_exit_code: {0}" -f $hostBundleExitCode))
[void]$checkBuilder.AppendLine(("qemu_bundle_exit_code: {0}" -f $qemuBundleExitCode))
if ($null -ne $hostView.warm -and $null -ne $hostView.warm.comparison) {
    [void]$checkBuilder.AppendLine(("host_warm_compare: regressions={0} improvements={1}" -f $hostView.warm.comparison.regressions, $hostView.warm.comparison.improvements))
}
if ($null -ne $qemuView.lower_half) {
    [void]$checkBuilder.AppendLine(("qemu_lower_half: ok={0} fail={1} other={2}" -f $qemuView.lower_half.status.ok, $qemuView.lower_half.status.fail, $qemuView.lower_half.status.other))
}
if ($violations.Count -gt 0) {
    [void]$checkBuilder.AppendLine("violations:")
    foreach ($message in $violations) {
        [void]$checkBuilder.AppendLine(("- {0}" -f $message))
    }
}
Set-Content -LiteralPath $checkTextPathResolved -Encoding utf8 ($checkBuilder.ToString())

Write-Host "==> bundle"
Write-Host "profile=minimal-kernel-runtime-evidence"
Write-Host ("output_root={0}" -f $outputRootPath)
Write-Host ("summary={0}" -f $summaryPathResolved)
Write-Host ("report_markdown={0}" -f $reportMarkdownPathResolved)
Write-Host ("check_text={0}" -f $checkTextPathResolved)
Write-Host ("host_output_root={0}" -f $resolvedHostOutputRoot)
Write-Host ("qemu_output_root={0}" -f $resolvedQemuOutputRoot)

if ($violations.Count -gt 0) {
    throw "minimal kernel runtime evidence bundle failed"
}
