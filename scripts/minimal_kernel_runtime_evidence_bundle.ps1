param(
    [string]$OutputRoot = "",
    [string]$SummaryPath = "",
    [string]$ReportMarkdownPath = "",
    [string]$CheckTextPath = "",
    [string]$CanonicalWorld = "",
    [string]$WitnessBundleOutputRoot = "",
    [string]$WitnessBundleSummaryPath = "",
    [string]$WitnessBundleReportMarkdownPath = "",
    [string]$WitnessBundleCheckTextPath = "",
    [string]$SessionOutputRoot = "",
    [string]$SessionSummaryPath = "",
    [string]$SessionRuntimeLedgerPath = "",
    [string]$SessionReportMarkdownPath = "",
    [string]$SessionCheckTextPath = "",
    [string]$HostOutputRoot = "",
    [string]$QemuOutputRoot = "",
    [string]$CMakeExe = "cmake",
    [string]$Generator = "Ninja",
    [string]$QemuExe = "qemu-system-arm",
    [string]$PythonExe = "python",
    [int]$HostJobs = 0,
    [int]$QemuBuildJobs = 1,
    [int]$QemuTimeoutSec = 30,
    [int]$QemuTailLines = 40,
    [switch]$Clean,
    [switch]$SkipSession,
    [switch]$SkipWitnessBundle,
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
    $Arguments.Add(($filteredValues -join ",")) | Out-Null
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

function Invoke-ExternalFile {
    param(
        [string]$FilePath,
        [string[]]$ArgumentList,
        [string]$LogPath,
        [string]$FailureMessage,
        [switch]$AllowFailure
    )

    Ensure-ParentDirectory -Path $LogPath
    Write-Host ("==> {0}" -f [System.IO.Path]::GetFileName($FilePath))

    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & $FilePath @ArgumentList 2>&1 | Tee-Object -FilePath $LogPath | Out-Host
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

function Get-TrapIngressWitnessView {
    param(
        $Witness
    )

    if ($null -eq $Witness) {
        return $null
    }

    return [ordered]@{
        ready = [bool]$Witness.ready
        ok = [bool]$Witness.ok
        sequence = [int64]$Witness.sequence
        stamp = [int64]$Witness.stamp
        stages = [ordered]@{
            decode = [bool]$Witness.stages.decode
            dispatch = [bool]$Witness.stages.dispatch
            writeback = [bool]$Witness.stages.writeback
        }
        terminal = [ordered]@{
            stage = [string]$Witness.terminal.stage
            service = [string]$Witness.terminal.service
            origin = [string]$Witness.terminal.origin
            disposition = [string]$Witness.terminal.disposition
            error = [string]$Witness.terminal.error
        }
        last_failure = [ordered]@{
            present = [bool]$Witness.last_failure.present
            prior_attempt = [bool]$Witness.last_failure.prior_attempt
            sequence = [int64]$Witness.last_failure.sequence
            stage = [string]$Witness.last_failure.stage
            service = [string]$Witness.last_failure.service
            origin = [string]$Witness.last_failure.origin
            disposition = [string]$Witness.last_failure.disposition
            error = [string]$Witness.last_failure.error
        }
    }
}

function Get-TrapIngressWitnessEntryView {
    param(
        $Entry
    )

    if ($null -eq $Entry) {
        return $null
    }

    return [ordered]@{
        example = [string]$Entry.example
        status = [string]$Entry.status
        trap_ingress_witness = Get-TrapIngressWitnessView -Witness $Entry.trap_ingress_witness
    }
}

function Get-TrapIngressWitnessSetView {
    param(
        $WitnessSet
    )

    if ($null -eq $WitnessSet) {
        return $null
    }

    return [ordered]@{
        expected_examples = @(
            @($WitnessSet.expected_examples) |
                ForEach-Object { [string]$_ }
        )
        present_count = [int]$WitnessSet.present_count
        ok_count = [int]$WitnessSet.ok_count
        missing_examples = @(
            @($WitnessSet.missing_examples) |
                ForEach-Object { [string]$_ }
        )
        entries = @(
            @($WitnessSet.entries) |
                ForEach-Object { Get-TrapIngressWitnessEntryView -Entry $_ }
        )
    }
}

function Format-TrapIngressWitnessStateText {
    param(
        $Witness
    )

    if ($null -eq $Witness) {
        return "missing"
    }

    $ready = if ($Witness.ready) { 1 } else { 0 }
    $ok = if ($Witness.ok) { 1 } else { 0 }
    return ("ready={0} ok={1}" -f $ready, $ok)
}

function Format-TrapIngressWitnessTerminalText {
    param(
        $Witness
    )

    if ($null -eq $Witness) {
        return "-"
    }

    return ("{0}/{1}/{2}/{3}/{4}" -f
        [string]$Witness.terminal.stage,
        [string]$Witness.terminal.service,
        [string]$Witness.terminal.origin,
        [string]$Witness.terminal.disposition,
        [string]$Witness.terminal.error)
}

function Format-TrapIngressWitnessLastFailureText {
    param(
        $Witness
    )

    if ($null -eq $Witness) {
        return "-"
    }

    if (-not [bool]$Witness.last_failure.present) {
        return "none"
    }

    $priorAttempt = if ([bool]$Witness.last_failure.prior_attempt) { 1 } else { 0 }
    return ("seq={0} prior={1} {2}/{3}/{4}/{5}/{6}" -f
        [int64]$Witness.last_failure.sequence,
        $priorAttempt,
        [string]$Witness.last_failure.stage,
        [string]$Witness.last_failure.service,
        [string]$Witness.last_failure.origin,
        [string]$Witness.last_failure.disposition,
        [string]$Witness.last_failure.error)
}

function Format-TrapIngressWitnessSetSummaryText {
    param(
        [string]$Label,
        $WitnessSet
    )

    if ($null -eq $WitnessSet) {
        return ""
    }

    $missingCount = @($WitnessSet.missing_examples).Count
    return ("- `{0}`: expected={1} present={2} ok={3} missing={4}" -f
        $Label,
        @($WitnessSet.expected_examples).Count,
        [int]$WitnessSet.present_count,
        [int]$WitnessSet.ok_count,
        $missingCount)
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

function Add-TrapIngressWitnessRows {
    param(
        [System.Text.StringBuilder]$Builder,
        [object[]]$Rows
    )

    foreach ($row in @($Rows)) {
        $columns = @(
            ([string]$row.host),
            ([string]$row.example),
            (Format-TrapIngressWitnessStateText -Witness $row.trap_ingress_witness),
            ([string]$row.trap_ingress_witness.sequence),
            (Format-TrapIngressWitnessTerminalText -Witness $row.trap_ingress_witness),
            (Format-TrapIngressWitnessLastFailureText -Witness $row.trap_ingress_witness)
        )

        [void]$Builder.AppendLine(($columns -join " | "))
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

    $trapIngressWitnesses = Get-TrapIngressWitnessSetView -WitnessSet $InspectData.trap_ingress_witnesses
    if ($null -ne $trapIngressWitnesses) {
        $view.trap_ingress_witnesses = $trapIngressWitnesses
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
        canonical_world = if ($null -ne $SummaryData.canonical_world) {
            [ordered]@{
                id = [string]$SummaryData.canonical_world.id
                label = [string]$SummaryData.canonical_world.label
                subject = [string]$SummaryData.canonical_world.subject
                environment = [string]$SummaryData.canonical_world.environment
                profile = [string]$SummaryData.canonical_world.profile
                focus = @($SummaryData.canonical_world.focus)
            }
        } else {
            $null
        }
        witness_bundle = if ($null -ne $SummaryData.witness_bundle) {
            [ordered]@{
                subject = [string]$SummaryData.witness_bundle.subject
                question = [string]$SummaryData.witness_bundle.question
                conclusion = [string]$SummaryData.witness_bundle.conclusion
                standing_cases = @($SummaryData.witness_bundle.standing_cases)
                regressed_cases = @($SummaryData.witness_bundle.regressed_cases)
            }
        } else {
            $null
        }
        biography = if ($null -ne $SummaryData.biography) {
            [ordered]@{
                identity = [string]$SummaryData.biography.identity
                thesis = [string]$SummaryData.biography.thesis
                evidence_path = @($SummaryData.biography.evidence_path)
                next_questions = @($SummaryData.biography.next_questions)
            }
        } else {
            $null
        }
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
                        highlights = @($_.Highlights)
                        canonical_case = if ($null -ne $_.CanonicalCase) {
                            [ordered]@{
                                world = [string]$_.CanonicalCase.world
                                seam = [string]$_.CanonicalCase.seam
                                world_state = [string]$_.CanonicalCase.world_state
                            }
                        } else {
                            $null
                        }
                        witness = if ($null -ne $_.Witness) {
                            [ordered]@{
                                claim = [string]$_.Witness.claim
                                question = [string]$_.Witness.question
                            }
                        } else {
                            $null
                        }
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

function Get-WitnessBundleView {
    param(
        $SummaryData,
        [string]$CanonicalWorldPath,
        [string]$OutputRootPath,
        [string]$SummaryPath,
        [string]$ReportPath,
        [string]$CheckPath,
        [string]$LogPath,
        [int]$ExitCode
    )

    if ($null -eq $SummaryData) {
        return $null
    }

    $resolvedCanonicalWorldPath = $CanonicalWorldPath
    if ([string]::IsNullOrWhiteSpace($resolvedCanonicalWorldPath) -and
        $null -ne $SummaryData.artifact_context -and
        $null -ne $SummaryData.artifact_context.canonical_world) {
        $resolvedCanonicalWorldPath = [string]$SummaryData.artifact_context.canonical_world
    }

    return [ordered]@{
        canonical_world_path = if ([string]::IsNullOrWhiteSpace($resolvedCanonicalWorldPath)) {
            $null
        } else {
            $resolvedCanonicalWorldPath
        }
        output_root = $OutputRootPath
        bundle_log_path = $LogPath
        bundle_exit_code = $ExitCode
        summary_path = $SummaryPath
        report_markdown_path = $ReportPath
        check_text_path = $CheckPath
        result = [string]$SummaryData.result
        world = if ($null -ne $SummaryData.world) {
            [ordered]@{
                name = [string]$SummaryData.world.name
                title = [string]$SummaryData.world.title
            }
        } else {
            $null
        }
        witness_summary = if ($null -ne $SummaryData.witness_summary) {
            [ordered]@{
                entry_count = [int]$SummaryData.witness_summary.entry_count
                ok_count = [int]$SummaryData.witness_summary.ok_count
                missing_count = [int]$SummaryData.witness_summary.missing_count
                fail_count = [int]$SummaryData.witness_summary.fail_count
                required_missing_count = [int]$SummaryData.witness_summary.required_missing_count
            }
        } else {
            $null
        }
    }
}

function Get-SessionView {
    param(
        $SummaryData,
        [string]$OutputRootPath,
        [string]$SummaryPath,
        [string]$RuntimeLedgerPath,
        [string]$ReportPath,
        [string]$CheckPath,
        [string]$LogPath,
        [int]$ExitCode
    )

    if ($null -eq $SummaryData) {
        return $null
    }

    return [ordered]@{
        output_root = $OutputRootPath
        bundle_log_path = $LogPath
        bundle_exit_code = $ExitCode
        summary_path = $SummaryPath
        runtime_ledger_path = $RuntimeLedgerPath
        report_markdown_path = $ReportPath
        check_text_path = $CheckPath
        result = if ([string]$SummaryData.verdict.session_status -eq "standing") { "ok" } else { "fail" }
        session_id = [string]$SummaryData.session_id
        world = [string]$SummaryData.world
        session_status = [string]$SummaryData.verdict.session_status
        failure_domain = if ($null -eq $SummaryData.verdict.failure_domain) { $null } else { [string]$SummaryData.verdict.failure_domain }
        failure_count = @($SummaryData.failures).Count
    }
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$resolvedOutputRoot = if ([string]::IsNullOrWhiteSpace($OutputRoot)) { "out/minimal-kernel-runtime-evidence" } else { $OutputRoot }
$outputRootPath = Resolve-FullPath -Path $resolvedOutputRoot
$resolvedHostOutputRoot = if ([string]::IsNullOrWhiteSpace($HostOutputRoot)) { Join-Path $outputRootPath "host" } else { Resolve-FullPath -Path $HostOutputRoot }
$resolvedQemuOutputRoot = if ([string]::IsNullOrWhiteSpace($QemuOutputRoot)) { Join-Path $outputRootPath "qemu" } else { Resolve-FullPath -Path $QemuOutputRoot }
$resolvedSessionOutputRoot = if ([string]::IsNullOrWhiteSpace($SessionOutputRoot)) { Join-Path $outputRootPath "session" } else { Resolve-FullPath -Path $SessionOutputRoot }
$resolvedWitnessBundleOutputRoot = if ([string]::IsNullOrWhiteSpace($WitnessBundleOutputRoot)) { Join-Path $outputRootPath "witness" } else { Resolve-FullPath -Path $WitnessBundleOutputRoot }

if ($Clean) {
    Remove-PathIfExists -Path $outputRootPath
}
Ensure-Directory -Path $outputRootPath

$defaultCanonicalWorldPath = Join-Path $repoRoot "Examples\kernel\canonical_worlds\minimal_kernel_runtime.world.json"
$resolvedCanonicalWorld = if (-not [string]::IsNullOrWhiteSpace($CanonicalWorld)) {
    Resolve-FullPath -Path $CanonicalWorld
} elseif (Test-Path $defaultCanonicalWorldPath) {
    Resolve-FullPath -Path $defaultCanonicalWorldPath
} else {
    ""
}

$summaryPathResolved = Get-OutputPath -ExplicitPath $SummaryPath -OutputRootPath $outputRootPath -DefaultFileName "summary.json"
$reportMarkdownPathResolved = Get-OutputPath -ExplicitPath $ReportMarkdownPath -OutputRootPath $outputRootPath -DefaultFileName "report.md"
$checkTextPathResolved = Get-OutputPath -ExplicitPath $CheckTextPath -OutputRootPath $outputRootPath -DefaultFileName "check.txt"
$hostBundleLogPathResolved = Get-OutputPath -ExplicitPath "" -OutputRootPath $outputRootPath -DefaultFileName "host.bundle.log"
$qemuBundleLogPathResolved = Get-OutputPath -ExplicitPath "" -OutputRootPath $outputRootPath -DefaultFileName "qemu.bundle.log"
$sessionSummaryPathResolved = Get-OutputPath -ExplicitPath $SessionSummaryPath -OutputRootPath $resolvedSessionOutputRoot -DefaultFileName "kernel_runtime_session.summary.json"
$sessionRuntimeLedgerPathResolved = Get-OutputPath -ExplicitPath $SessionRuntimeLedgerPath -OutputRootPath $resolvedSessionOutputRoot -DefaultFileName "runtime_ledger.json"
$sessionReportMarkdownPathResolved = Get-OutputPath -ExplicitPath $SessionReportMarkdownPath -OutputRootPath $resolvedSessionOutputRoot -DefaultFileName "report.md"
$sessionCheckTextPathResolved = Get-OutputPath -ExplicitPath $SessionCheckTextPath -OutputRootPath $resolvedSessionOutputRoot -DefaultFileName "check.txt"
$sessionBundleLogPathResolved = Get-OutputPath -ExplicitPath "" -OutputRootPath $outputRootPath -DefaultFileName "session.bundle.log"
$witnessBundleSummaryPathResolved = Get-OutputPath -ExplicitPath $WitnessBundleSummaryPath -OutputRootPath $resolvedWitnessBundleOutputRoot -DefaultFileName "summary.json"
$witnessBundleReportMarkdownPathResolved = Get-OutputPath -ExplicitPath $WitnessBundleReportMarkdownPath -OutputRootPath $resolvedWitnessBundleOutputRoot -DefaultFileName "report.md"
$witnessBundleCheckTextPathResolved = Get-OutputPath -ExplicitPath $WitnessBundleCheckTextPath -OutputRootPath $resolvedWitnessBundleOutputRoot -DefaultFileName "check.txt"
$witnessBundleLogPathResolved = Get-OutputPath -ExplicitPath "" -OutputRootPath $outputRootPath -DefaultFileName "witness.bundle.log"

$powerShellExe = Resolve-ToolPath -Candidates @("powershell.exe", "pwsh.exe", "powershell", "pwsh")
$pythonExeResolved = if ($SkipSession) { "" } else { Resolve-ToolPath -Candidates @($PythonExe, "python.exe", "python", "py.exe", "py") }
$hostBundleScript = Join-Path $PSScriptRoot "minimal_kernel_runtime_host_smoke_dual_bundle.ps1"
$qemuBundleScript = Join-Path $PSScriptRoot "minimal_kernel_runtime_armv7a_qemu_smoke_bundle.ps1"
$sessionExportScript = Join-Path $PSScriptRoot "export_minimal_kernel_runtime_session.py"
$witnessBundleScript = Join-Path $PSScriptRoot "export_system_compiler_witness_bundle.ps1"
$requiredScripts = [System.Collections.Generic.List[string]]::new()
$requiredScripts.Add($hostBundleScript) | Out-Null
$requiredScripts.Add($qemuBundleScript) | Out-Null
if (-not $SkipSession) {
    $requiredScripts.Add($sessionExportScript) | Out-Null
}
if (-not $SkipWitnessBundle) {
    $requiredScripts.Add($witnessBundleScript) | Out-Null
}
foreach ($scriptPath in $requiredScripts) {
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
$sessionBundleExitCode = 0
$witnessBundleExitCode = 0
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
}
if (-not $SkipWitnessBundle) {
    $summaryObject.witness_bundle = $null
}
if (-not $SkipSession) {
    $summaryObject.session = $null
}
$summaryObject.violations = @($violations)

Ensure-ParentDirectory -Path $summaryPathResolved
$summaryObject | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $summaryPathResolved -Encoding utf8

$sessionView = $null
if (-not $SkipSession) {
    $sessionArgs = [System.Collections.Generic.List[string]]::new()
    $sessionArgs.Add($sessionExportScript) | Out-Null
    $sessionArgs.Add("--runtime-evidence-summary") | Out-Null
    $sessionArgs.Add($summaryPathResolved) | Out-Null
    if (-not [string]::IsNullOrWhiteSpace($resolvedCanonicalWorld)) {
        $sessionArgs.Add("--canonical-world") | Out-Null
        $sessionArgs.Add($resolvedCanonicalWorld) | Out-Null
    }
    $sessionArgs.Add("--output-root") | Out-Null
    $sessionArgs.Add($resolvedSessionOutputRoot) | Out-Null
    $sessionArgs.Add("--summary-path") | Out-Null
    $sessionArgs.Add($sessionSummaryPathResolved) | Out-Null
    $sessionArgs.Add("--runtime-ledger-path") | Out-Null
    $sessionArgs.Add($sessionRuntimeLedgerPathResolved) | Out-Null
    $sessionArgs.Add("--report-path") | Out-Null
    $sessionArgs.Add($sessionReportMarkdownPathResolved) | Out-Null
    $sessionArgs.Add("--check-path") | Out-Null
    $sessionArgs.Add($sessionCheckTextPathResolved) | Out-Null

    Push-Location $repoRoot
    try {
        $sessionBundleExitCode = Invoke-ExternalFile `
            -FilePath $pythonExeResolved `
            -ArgumentList $sessionArgs.ToArray() `
            -LogPath $sessionBundleLogPathResolved `
            -FailureMessage "minimal kernel runtime session export failed" `
            -AllowFailure
    } finally {
        Pop-Location
    }

    $sessionSummaryData = Load-JsonFile -Path $sessionSummaryPathResolved
    if ($null -eq $sessionSummaryData) {
        $violations.Add("missing kernel runtime session summary json") | Out-Null
    }
    if ($sessionBundleExitCode -ne 0) {
        $violations.Add(("kernel runtime session exit code {0}" -f $sessionBundleExitCode)) | Out-Null
    }

    $sessionView = Get-SessionView `
        -SummaryData $sessionSummaryData `
        -OutputRootPath $resolvedSessionOutputRoot `
        -SummaryPath $sessionSummaryPathResolved `
        -RuntimeLedgerPath $sessionRuntimeLedgerPathResolved `
        -ReportPath $sessionReportMarkdownPathResolved `
        -CheckPath $sessionCheckTextPathResolved `
        -LogPath $sessionBundleLogPathResolved `
        -ExitCode $sessionBundleExitCode

    $summaryObject.session = $sessionView
    $summaryObject.result = if ($violations.Count -eq 0) { "ok" } else { "fail" }
    $summaryObject.violations = @($violations)
    $summaryObject | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $summaryPathResolved -Encoding utf8
}

$witnessBundleView = $null
if (-not $SkipWitnessBundle) {
    $witnessArgs = [System.Collections.Generic.List[string]]::new()
    if (-not [string]::IsNullOrWhiteSpace($resolvedCanonicalWorld)) {
        Add-ScriptArgument -Arguments $witnessArgs -Name "-CanonicalWorld" -Value $resolvedCanonicalWorld
    }
    Add-ScriptArgument -Arguments $witnessArgs -Name "-RuntimeEvidenceSummary" -Value $summaryPathResolved
    Add-ScriptArgument -Arguments $witnessArgs -Name "-OutputRoot" -Value $resolvedWitnessBundleOutputRoot
    Add-ScriptArgument -Arguments $witnessArgs -Name "-OutputPath" -Value $witnessBundleSummaryPathResolved
    Add-ScriptArgument -Arguments $witnessArgs -Name "-ReportMarkdownPath" -Value $witnessBundleReportMarkdownPathResolved
    Add-ScriptArgument -Arguments $witnessArgs -Name "-CheckTextPath" -Value $witnessBundleCheckTextPathResolved

    Push-Location $repoRoot
    try {
        $witnessBundleExitCode = Invoke-PowerShellFile `
            -PowerShellExe $powerShellExe `
            -ScriptPath $witnessBundleScript `
            -ArgumentList $witnessArgs.ToArray() `
            -LogPath $witnessBundleLogPathResolved `
            -FailureMessage "system compiler witness bundle export failed" `
            -AllowFailure
    } finally {
        Pop-Location
    }

    $witnessBundleSummaryData = Load-JsonFile -Path $witnessBundleSummaryPathResolved
    if ($null -eq $witnessBundleSummaryData) {
        $violations.Add("missing witness bundle summary json") | Out-Null
    }
    if ($witnessBundleExitCode -ne 0) {
        $violations.Add(("witness bundle exit code {0}" -f $witnessBundleExitCode)) | Out-Null
    }

    $witnessBundleView = Get-WitnessBundleView `
        -SummaryData $witnessBundleSummaryData `
        -CanonicalWorldPath $resolvedCanonicalWorld `
        -OutputRootPath $resolvedWitnessBundleOutputRoot `
        -SummaryPath $witnessBundleSummaryPathResolved `
        -ReportPath $witnessBundleReportMarkdownPathResolved `
        -CheckPath $witnessBundleCheckTextPathResolved `
        -LogPath $witnessBundleLogPathResolved `
        -ExitCode $witnessBundleExitCode

    $summaryObject.witness_bundle = $witnessBundleView
    $summaryObject.result = if ($violations.Count -eq 0) { "ok" } else { "fail" }
    $summaryObject.violations = @($violations)
    $summaryObject | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $summaryPathResolved -Encoding utf8
}

$reportBuilder = [System.Text.StringBuilder]::new()
[void]$reportBuilder.AppendLine("# Minimal Kernel Runtime Evidence Bundle Report")
[void]$reportBuilder.AppendLine("")
[void]$reportBuilder.AppendLine(('- Generated at: `{0}`' -f [string]$summaryObject.generated_at))
[void]$reportBuilder.AppendLine(('- Result: `{0}`' -f [string]$summaryObject.result))
[void]$reportBuilder.AppendLine(('- Summary JSON: `{0}`' -f $summaryPathResolved))
[void]$reportBuilder.AppendLine(('- Output root: `{0}`' -f $outputRootPath))
if ($null -ne $summaryObject.witness_bundle) {
    [void]$reportBuilder.AppendLine(('- Witness bundle: `{0}`' -f [string]$summaryObject.witness_bundle.summary_path))
}

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
if (($null -ne $hostView.cold -and $null -ne $hostView.cold.trap_ingress_witnesses) -or
    ($null -ne $hostView.warm -and $null -ne $hostView.warm.trap_ingress_witnesses)) {
    [void]$reportBuilder.AppendLine("")
    [void]$reportBuilder.AppendLine("### Trap Ingress Witnesses")

    $witnessSummaryLines = [System.Collections.Generic.List[string]]::new()
    $witnessRows = [System.Collections.Generic.List[object]]::new()

    foreach ($hostEntry in @(
        [ordered]@{ host = "cold"; view = $hostView.cold },
        [ordered]@{ host = "warm"; view = $hostView.warm }
    )) {
        if ($null -eq $hostEntry.view -or $null -eq $hostEntry.view.trap_ingress_witnesses) {
            continue
        }

        $witnessSet = $hostEntry.view.trap_ingress_witnesses
        $summaryLine = Format-TrapIngressWitnessSetSummaryText -Label $hostEntry.host -WitnessSet $witnessSet
        if (-not [string]::IsNullOrWhiteSpace($summaryLine)) {
            $witnessSummaryLines.Add($summaryLine) | Out-Null
        }

        foreach ($entry in @($witnessSet.entries)) {
            $witnessRows.Add([ordered]@{
                host = $hostEntry.host
                example = [string]$entry.example
                trap_ingress_witness = $entry.trap_ingress_witness
            }) | Out-Null
        }
    }

    if ($witnessSummaryLines.Count -eq 0) {
        [void]$reportBuilder.AppendLine("- No trap ingress witness data.")
    } else {
        foreach ($line in $witnessSummaryLines) {
            [void]$reportBuilder.AppendLine($line)
        }
        if ($witnessRows.Count -gt 0) {
            [void]$reportBuilder.AppendLine("")
            Add-TableHeader -Builder $reportBuilder -Columns @("Host", "Example", "Witness", "Sequence", "Terminal", "Last Failure")
            Add-TrapIngressWitnessRows -Builder $reportBuilder -Rows $witnessRows
        }
    }
}

[void]$reportBuilder.AppendLine("")
[void]$reportBuilder.AppendLine("## Lower-Half QEMU Evidence")
[void]$reportBuilder.AppendLine(('- Bundle log: `{0}`' -f $qemuBundleLogPathResolved))
if ($null -ne $qemuView.lower_half) {
    if ($null -ne $qemuView.lower_half.canonical_world) {
        [void]$reportBuilder.AppendLine(('- Canonical world: `{0}` (`{1}`)' -f $qemuView.lower_half.canonical_world.id, $qemuView.lower_half.canonical_world.label))
        [void]$reportBuilder.AppendLine(('- Canonical subject: `{0}` in `{1}`' -f $qemuView.lower_half.canonical_world.subject, $qemuView.lower_half.canonical_world.environment))
    }
    if ($null -ne $qemuView.lower_half.witness_bundle) {
        [void]$reportBuilder.AppendLine(('- Witness question: `{0}`' -f $qemuView.lower_half.witness_bundle.question))
        [void]$reportBuilder.AppendLine(('- Witness conclusion: `{0}`' -f $qemuView.lower_half.witness_bundle.conclusion))
    }
    if ($null -ne $qemuView.lower_half.biography) {
        [void]$reportBuilder.AppendLine(('- Biography identity: `{0}`' -f $qemuView.lower_half.biography.identity))
        [void]$reportBuilder.AppendLine(('- Biography thesis: `{0}`' -f $qemuView.lower_half.biography.thesis))
    }
    [void]$reportBuilder.AppendLine(('- Lower-half status: `ok={0} fail={1} other={2}`' -f $qemuView.lower_half.status.ok, $qemuView.lower_half.status.fail, $qemuView.lower_half.status.other))
    [void]$reportBuilder.AppendLine(('- Cases: `completed={0} expected={1}`' -f $qemuView.lower_half.completed_case_count, $qemuView.lower_half.case_count))
    [void]$reportBuilder.AppendLine(('- Elapsed: `total={0}ms avg={1}ms max={2}ms`' -f $qemuView.lower_half.elapsed_ms.total, $qemuView.lower_half.elapsed_ms.average, $qemuView.lower_half.elapsed_ms.max))
    [void]$reportBuilder.AppendLine(('- Build phases: `configure={0}ms build={1}ms`' -f $qemuView.lower_half.phase_elapsed_ms.configure, $qemuView.lower_half.phase_elapsed_ms.build))
    [void]$reportBuilder.AppendLine(('- QEMU report: `{0}`' -f $qemuView.lower_half.report_markdown_path))
    [void]$reportBuilder.AppendLine("")
    if ($null -ne $qemuView.lower_half.biography -and $null -ne $qemuView.lower_half.biography.evidence_path) {
        $biographyPath = ((@($qemuView.lower_half.biography.evidence_path) | ForEach-Object { [string]$_ }) -join " -> ")
        [void]$reportBuilder.AppendLine("### QEMU Biography Path")
        [void]$reportBuilder.AppendLine($biographyPath)
        [void]$reportBuilder.AppendLine("")
    }
    if ($null -ne $qemuView.lower_half.biography -and $null -ne $qemuView.lower_half.biography.next_questions) {
        [void]$reportBuilder.AppendLine("### QEMU Next Questions")
        foreach ($question in @($qemuView.lower_half.biography.next_questions)) {
            $questionLine = ('- `{0}`' -f [string]$question)
            [void]$reportBuilder.AppendLine($questionLine)
        }
        [void]$reportBuilder.AppendLine("")
    }
    [void]$reportBuilder.AppendLine("### QEMU Cases")
    [void]$reportBuilder.AppendLine("Case | Status | Elapsed | Seam")
    [void]$reportBuilder.AppendLine("--- | --- | --- | ---")
    foreach ($entry in @($qemuView.lower_half.results)) {
        $seam = ""
        if ($null -ne $entry.canonical_case) {
            $seam = [string]$entry.canonical_case.seam
        }
        [void]$reportBuilder.AppendLine(("{0} | {1} | {2}ms | {3}" -f [string]$entry.label, [string]$entry.status, [int64]$entry.elapsed_ms, $seam))
    }

    $qemuWitnessResults = @(
        $qemuView.lower_half.results | Where-Object {
            $null -ne $_.witness
        }
    )
    if ($qemuWitnessResults.Count -gt 0) {
        [void]$reportBuilder.AppendLine("")
        [void]$reportBuilder.AppendLine("### QEMU Witness Questions")
        foreach ($entry in @($qemuWitnessResults)) {
            [void]$reportBuilder.AppendLine(('- `{0}`' -f [string]$entry.label))
            [void]$reportBuilder.AppendLine(('  claim: `{0}`' -f [string]$entry.witness.claim))
            [void]$reportBuilder.AppendLine(('  question: `{0}`' -f [string]$entry.witness.question))
            if ($null -ne $entry.canonical_case) {
                [void]$reportBuilder.AppendLine(('  world-state: `{0}`' -f [string]$entry.canonical_case.world_state))
            }
        }
    }

    $qemuHighlightResults = @(
        $qemuView.lower_half.results | Where-Object {
            $null -ne $_.highlights -and (@($_.highlights)).Count -gt 0
        }
    )
    if ($qemuHighlightResults.Count -gt 0) {
        [void]$reportBuilder.AppendLine("")
        [void]$reportBuilder.AppendLine("### QEMU Highlights")
        foreach ($entry in @($qemuHighlightResults)) {
            [void]$reportBuilder.AppendLine(('- `{0}`' -f [string]$entry.label))
            foreach ($highlight in @($entry.highlights)) {
                [void]$reportBuilder.AppendLine(('  `{0}`' -f [string]$highlight))
            }
        }
    }
}

if ($null -ne $summaryObject.witness_bundle) {
    [void]$reportBuilder.AppendLine("")
    [void]$reportBuilder.AppendLine("## System Compiler Witness Bundle")
    [void]$reportBuilder.AppendLine(('- Bundle log: `{0}`' -f [string]$summaryObject.witness_bundle.bundle_log_path))
    if (-not [string]::IsNullOrWhiteSpace([string]$summaryObject.witness_bundle.canonical_world_path)) {
        [void]$reportBuilder.AppendLine(('- Canonical world file: `{0}`' -f [string]$summaryObject.witness_bundle.canonical_world_path))
    }
    if ($null -ne $summaryObject.witness_bundle.world) {
        [void]$reportBuilder.AppendLine(('- World: `{0}` (`{1}`)' -f [string]$summaryObject.witness_bundle.world.name, [string]$summaryObject.witness_bundle.world.title))
    }
    [void]$reportBuilder.AppendLine(('- Witness result: `{0}` (exit={1})' -f [string]$summaryObject.witness_bundle.result, [int]$summaryObject.witness_bundle.bundle_exit_code))
    if ($null -ne $summaryObject.witness_bundle.witness_summary) {
        [void]$reportBuilder.AppendLine(('- Witness entries: `total={0} ok={1} missing={2} fail={3} required_missing={4}`' -f $summaryObject.witness_bundle.witness_summary.entry_count, $summaryObject.witness_bundle.witness_summary.ok_count, $summaryObject.witness_bundle.witness_summary.missing_count, $summaryObject.witness_bundle.witness_summary.fail_count, $summaryObject.witness_bundle.witness_summary.required_missing_count))
    }
    [void]$reportBuilder.AppendLine(('- Witness report: `{0}`' -f [string]$summaryObject.witness_bundle.report_markdown_path))
}

if ($null -ne $summaryObject.session) {
    [void]$reportBuilder.AppendLine("")
    [void]$reportBuilder.AppendLine("## Kernel Runtime Session")
    [void]$reportBuilder.AppendLine(('- Bundle log: `{0}`' -f [string]$summaryObject.session.bundle_log_path))
    [void]$reportBuilder.AppendLine(('- Session: `{0}` in world `{1}`' -f [string]$summaryObject.session.session_id, [string]$summaryObject.session.world))
    [void]$reportBuilder.AppendLine(('- Session result: `{0}` status=`{1}` failures={2}' -f [string]$summaryObject.session.result, [string]$summaryObject.session.session_status, [int]$summaryObject.session.failure_count))
    if ($null -ne $summaryObject.session.failure_domain) {
        [void]$reportBuilder.AppendLine(('- Failure domain: `{0}`' -f [string]$summaryObject.session.failure_domain))
    }
    [void]$reportBuilder.AppendLine(('- Session summary: `{0}`' -f [string]$summaryObject.session.summary_path))
    [void]$reportBuilder.AppendLine(('- Runtime ledger: `{0}`' -f [string]$summaryObject.session.runtime_ledger_path))
    [void]$reportBuilder.AppendLine(('- Session report: `{0}`' -f [string]$summaryObject.session.report_markdown_path))
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
if ($null -ne $summaryObject.witness_bundle) {
    [void]$checkBuilder.AppendLine(("witness_bundle_exit_code: {0}" -f [int]$summaryObject.witness_bundle.bundle_exit_code))
    [void]$checkBuilder.AppendLine(("witness_bundle_result: {0}" -f [string]$summaryObject.witness_bundle.result))
    if ($null -ne $summaryObject.witness_bundle.witness_summary) {
        [void]$checkBuilder.AppendLine(("witness_bundle_entries: total={0} required_missing={1}" -f $summaryObject.witness_bundle.witness_summary.entry_count, $summaryObject.witness_bundle.witness_summary.required_missing_count))
    }
}
if ($null -ne $hostView.cold -and $null -ne $hostView.cold.trap_ingress_witnesses) {
    [void]$checkBuilder.AppendLine(("host_cold_trap_ingress_witnesses: expected={0} present={1} ok={2} missing={3}" -f
        @($hostView.cold.trap_ingress_witnesses.expected_examples).Count,
        [int]$hostView.cold.trap_ingress_witnesses.present_count,
        [int]$hostView.cold.trap_ingress_witnesses.ok_count,
        @($hostView.cold.trap_ingress_witnesses.missing_examples).Count))
}
if ($null -ne $hostView.warm -and $null -ne $hostView.warm.trap_ingress_witnesses) {
    [void]$checkBuilder.AppendLine(("host_warm_trap_ingress_witnesses: expected={0} present={1} ok={2} missing={3}" -f
        @($hostView.warm.trap_ingress_witnesses.expected_examples).Count,
        [int]$hostView.warm.trap_ingress_witnesses.present_count,
        [int]$hostView.warm.trap_ingress_witnesses.ok_count,
        @($hostView.warm.trap_ingress_witnesses.missing_examples).Count))
}
if ($null -ne $summaryObject.session) {
    [void]$checkBuilder.AppendLine(("session_exit_code: {0}" -f [int]$summaryObject.session.bundle_exit_code))
    [void]$checkBuilder.AppendLine(("session_result: {0}" -f [string]$summaryObject.session.result))
    [void]$checkBuilder.AppendLine(("session_status: {0}" -f [string]$summaryObject.session.session_status))
    [void]$checkBuilder.AppendLine(("session_failures: {0}" -f [int]$summaryObject.session.failure_count))
}
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
if (-not $SkipSession) {
    Write-Host ("session_output_root={0}" -f $resolvedSessionOutputRoot)
    Write-Host ("session_path={0}" -f $sessionSummaryPathResolved)
}
if (-not $SkipWitnessBundle) {
    Write-Host ("witness_output_root={0}" -f $resolvedWitnessBundleOutputRoot)
    Write-Host ("witness_summary={0}" -f $witnessBundleSummaryPathResolved)
}

if ($violations.Count -gt 0) {
    throw "minimal kernel runtime evidence bundle failed"
}
