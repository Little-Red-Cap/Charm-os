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

function Test-CaseOk {
    param(
        $QemuSummaryData,
        [string]$CaseName
    )

    if ($null -eq $QemuSummaryData) {
        return $false
    }

    foreach ($entry in @($QemuSummaryData.results)) {
        if ([string]$entry.Case -eq $CaseName -and [string]$entry.Status -eq "ok") {
            return $true
        }
    }

    return $false
}

function Write-KernelRuntimeSessionArtifacts {
    param(
        [string]$SessionOutputRoot,
        [string]$SessionSummaryPath,
        [string]$RuntimeLedgerPath,
        [string]$SessionCheckPath,
        $HostColdInspectData,
        $HostWarmInspectData,
        $QemuSummaryData
    )

    Ensure-Directory -Path $SessionOutputRoot

    $hostColdStanding = $false
    if ($null -ne $HostColdInspectData -and $null -ne $HostColdInspectData.status) {
        $hostColdStanding = ([int]$HostColdInspectData.status.fail -eq 0 -and [int]$HostColdInspectData.status.other -eq 0)
    }

    $hostWarmStanding = $false
    if ($null -ne $HostWarmInspectData -and $null -ne $HostWarmInspectData.status) {
        $hostWarmStanding = ([int]$HostWarmInspectData.status.fail -eq 0 -and [int]$HostWarmInspectData.status.other -eq 0)
    }

    $qemuStanding = $false
    $runtimeTrapOk = Test-CaseOk -QemuSummaryData $QemuSummaryData -CaseName "runtime-trap"
    $runtimeLiveOk = Test-CaseOk -QemuSummaryData $QemuSummaryData -CaseName "runtime-live"
    $taskSyscallOk = Test-CaseOk -QemuSummaryData $QemuSummaryData -CaseName "task-syscall"
    $handoffLiveOk = Test-CaseOk -QemuSummaryData $QemuSummaryData -CaseName "handoff-live"
    if ($null -ne $QemuSummaryData) {
        $results = @($QemuSummaryData.results)
        $qemuFailCount = @($results | Where-Object { [string]$_.Status -eq "fail" }).Count
        $qemuOtherCount = @($results | Where-Object { ([string]$_.Status -ne "ok") -and ([string]$_.Status -ne "fail") }).Count
        $qemuStanding = ($qemuFailCount -eq 0 -and $qemuOtherCount -eq 0 -and $results.Count -gt 0)
    }

    $events = [System.Collections.Generic.List[object]]::new()
    foreach ($entry in @(
        @{ Phase = "host.semantic.cold"; Fact = "host cold semantic witness"; Observed = $hostColdStanding },
        @{ Phase = "host.semantic.warm"; Fact = "host warm semantic witness"; Observed = $hostWarmStanding },
        @{ Phase = "machine.trap"; Fact = "runtime trap lower-half case"; Observed = $runtimeTrapOk },
        @{ Phase = "machine.runtime_live"; Fact = "runtime live lower-half case"; Observed = $runtimeLiveOk },
        @{ Phase = "machine.task_syscall"; Fact = "task syscall lower-half case"; Observed = $taskSyscallOk },
        @{ Phase = "machine.handoff_continuity"; Fact = "handoff live continuity case"; Observed = $handoffLiveOk }
    )) {
        $events.Add([ordered]@{
            phase = [string]$entry.Phase
            fact = [string]$entry.Fact
            observed = [bool]$entry.Observed
        }) | Out-Null
    }

    $runtimeLedger = [ordered]@{
        schema = "minimal_kernel.kernel_runtime_session.runtime_ledger/v0"
        kind = "minimal_kernel.kernel_runtime_session.runtime_ledger"
        session_id = "minimal_kernel_runtime.armv7a_qemu.debug"
        events = @($events)
    }
    $runtimeLedger | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $RuntimeLedgerPath -Encoding utf8

    $sessionStanding = $hostColdStanding -and $hostWarmStanding -and $qemuStanding -and $runtimeTrapOk -and $runtimeLiveOk -and $taskSyscallOk -and $handoffLiveOk
    $failure = $null
    if (-not $sessionStanding) {
        $failureCode = if (-not $hostColdStanding -or -not $hostWarmStanding) {
            "host_semantic_mismatch"
        } elseif (-not $qemuStanding) {
            "machine_witness_missing"
        } elseif (-not $runtimeTrapOk) {
            "trap_not_observed"
        } elseif (-not $runtimeLiveOk) {
            "thread_not_resumed"
        } elseif (-not $taskSyscallOk) {
            "decode_failed"
        } else {
            "handoff_continuity_broken"
        }
        $failureDomain = if ($failureCode -eq "host_semantic_mismatch") { "semantic" } else { "machine" }
        $failurePhase = if ($failureCode -eq "handoff_continuity_broken") { "handoff.live" } else { "runtime.session" }
        $failure = [ordered]@{
            code = $failureCode
            domain = $failureDomain
            layer = if ($failureDomain -eq "semantic") { "upper_half" } else { "lower_half" }
            focus = if ($failureCode -eq "handoff_continuity_broken") { @("handoff", "session") } else { @("session", "runtime") }
            required = $true
            phase = $failurePhase
            message = "kernel_runtime_session did not collect every required semantic and machine witness"
        }
    }

    $sessionSummary = [ordered]@{
        schema = "minimal_kernel.kernel_runtime_session/v0"
        kind = "minimal_kernel.kernel_runtime_session"
        session_id = "minimal_kernel_runtime.armv7a_qemu.debug"
        world = "minimal_kernel_runtime"
        subject = [ordered]@{
            board = "armv7a_qemu"
            profile = "debug"
            leaf = "Examples/kernel/armv7a/qemu"
        }
        semantic_witness = [ordered]@{
            host = ($hostColdStanding -and $hostWarmStanding)
            contracts = @(
                "trap_ingress",
                "task_message_session",
                "task_syscall_frame"
            )
        }
        machine_witness = [ordered]@{
            qemu = $qemuStanding
            exception_ingress = $qemuStanding
            interrupt_ingress = $qemuStanding
            timer_ingress = $qemuStanding
            trap_ingress = $runtimeTrapOk
            context_ingress = $runtimeLiveOk
            runtime_loop = $runtimeLiveOk
        }
        runtime = [ordered]@{
            tick = $runtimeLiveOk
            trap = $runtimeTrapOk
            thread = $runtimeLiveOk
            task_syscall = $taskSyscallOk
            handoff_continuity = $handoffLiveOk
        }
        ledger = [ordered]@{
            phase_ledger = $null
            runtime_ledger = $RuntimeLedgerPath
        }
        verdict = [ordered]@{
            session_status = if ($sessionStanding) { "standing" } else { "failed" }
            failure = $failure
        }
    }
    $sessionSummary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $SessionSummaryPath -Encoding utf8

    $checkBuilder = [System.Text.StringBuilder]::new()
    [void]$checkBuilder.AppendLine(("summary: {0}" -f $SessionSummaryPath))
    [void]$checkBuilder.AppendLine(("session_id: {0}" -f [string]$sessionSummary.session_id))
    [void]$checkBuilder.AppendLine(("result: {0}" -f $(if ($sessionStanding) { "ok" } else { "fail" })))
    [void]$checkBuilder.AppendLine(("session_status: {0}" -f [string]$sessionSummary.verdict.session_status))
    [void]$checkBuilder.AppendLine(("host_semantic: {0}" -f [bool]$sessionSummary.semantic_witness.host))
    [void]$checkBuilder.AppendLine(("machine_qemu: {0}" -f [bool]$sessionSummary.machine_witness.qemu))
    [void]$checkBuilder.AppendLine(("handoff_continuity: {0}" -f [bool]$sessionSummary.runtime.handoff_continuity))
    Set-Content -LiteralPath $SessionCheckPath -Encoding utf8 ($checkBuilder.ToString())

    return [ordered]@{
        summary_path = $SessionSummaryPath
        runtime_ledger_path = $RuntimeLedgerPath
        check_text_path = $SessionCheckPath
        result = if ($sessionStanding) { "ok" } else { "fail" }
        session_id = [string]$sessionSummary.session_id
        world = [string]$sessionSummary.world
        session_status = [string]$sessionSummary.verdict.session_status
    }
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$resolvedOutputRoot = if ([string]::IsNullOrWhiteSpace($OutputRoot)) { "out/minimal-kernel-runtime-evidence" } else { $OutputRoot }
$outputRootPath = Resolve-FullPath -Path $resolvedOutputRoot
$resolvedHostOutputRoot = if ([string]::IsNullOrWhiteSpace($HostOutputRoot)) { Join-Path $outputRootPath "host" } else { Resolve-FullPath -Path $HostOutputRoot }
$resolvedQemuOutputRoot = if ([string]::IsNullOrWhiteSpace($QemuOutputRoot)) { Join-Path $outputRootPath "qemu" } else { Resolve-FullPath -Path $QemuOutputRoot }
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
$sessionOutputRootResolved = Join-Path $outputRootPath "session"
$sessionSummaryPathResolved = Join-Path $sessionOutputRootResolved "kernel_runtime_session.summary.json"
$sessionRuntimeLedgerPathResolved = Join-Path $sessionOutputRootResolved "runtime_ledger.json"
$sessionCheckTextPathResolved = Join-Path $sessionOutputRootResolved "check.txt"
$requiredQemuLowerHalfCaseCount = 4
$witnessBundleSummaryPathResolved = Get-OutputPath -ExplicitPath $WitnessBundleSummaryPath -OutputRootPath $resolvedWitnessBundleOutputRoot -DefaultFileName "summary.json"
$witnessBundleReportMarkdownPathResolved = Get-OutputPath -ExplicitPath $WitnessBundleReportMarkdownPath -OutputRootPath $resolvedWitnessBundleOutputRoot -DefaultFileName "report.md"
$witnessBundleCheckTextPathResolved = Get-OutputPath -ExplicitPath $WitnessBundleCheckTextPath -OutputRootPath $resolvedWitnessBundleOutputRoot -DefaultFileName "check.txt"
$witnessBundleLogPathResolved = Get-OutputPath -ExplicitPath "" -OutputRootPath $outputRootPath -DefaultFileName "witness.bundle.log"

$powerShellExe = Resolve-ToolPath -Candidates @("powershell.exe", "pwsh.exe", "powershell", "pwsh")
$hostBundleScript = Join-Path $PSScriptRoot "minimal_kernel_runtime_host_smoke_dual_bundle.ps1"
$qemuBundleScript = Join-Path $PSScriptRoot "minimal_kernel_runtime_armv7a_qemu_smoke_bundle.ps1"
$witnessBundleScript = Join-Path $PSScriptRoot "export_system_compiler_witness_bundle.ps1"
$requiredScripts = [System.Collections.Generic.List[string]]::new()
$requiredScripts.Add($hostBundleScript) | Out-Null
$requiredScripts.Add($qemuBundleScript) | Out-Null
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
Add-ScriptArgument -Arguments $qemuArgs -Name "-RequireCaseCount" -Value (Format-Number -Value $requiredQemuLowerHalfCaseCount)
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

$sessionView = Write-KernelRuntimeSessionArtifacts `
    -SessionOutputRoot $sessionOutputRootResolved `
    -SessionSummaryPath $sessionSummaryPathResolved `
    -RuntimeLedgerPath $sessionRuntimeLedgerPathResolved `
    -SessionCheckPath $sessionCheckTextPathResolved `
    -HostColdInspectData $hostCiInspectData `
    -HostWarmInspectData $hostDailyInspectData `
    -QemuSummaryData $qemuSummaryData

if ([string]$sessionView.result -ne "ok") {
    $violations.Add(("kernel runtime session status {0}" -f [string]$sessionView.session_status)) | Out-Null
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
    session_summary = $sessionView
}
if (-not $SkipWitnessBundle) {
    $summaryObject.witness_bundle = $null
}
$summaryObject.violations = @($violations)

Ensure-ParentDirectory -Path $summaryPathResolved
$summaryObject | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $summaryPathResolved -Encoding utf8

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
[void]$reportBuilder.AppendLine("## Kernel Runtime Session")
[void]$reportBuilder.AppendLine(('- Session summary: `{0}`' -f [string]$summaryObject.session_summary.summary_path))
[void]$reportBuilder.AppendLine(('- Runtime ledger: `{0}`' -f [string]$summaryObject.session_summary.runtime_ledger_path))
[void]$reportBuilder.AppendLine(('- Session status: `{0}`' -f [string]$summaryObject.session_summary.session_status))
[void]$reportBuilder.AppendLine(('- Session result: `{0}`' -f [string]$summaryObject.session_summary.result))

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
[void]$checkBuilder.AppendLine(("session_summary: {0}" -f [string]$summaryObject.session_summary.summary_path))
[void]$checkBuilder.AppendLine(("session_status: {0}" -f [string]$summaryObject.session_summary.session_status))
[void]$checkBuilder.AppendLine(("host_bundle_exit_code: {0}" -f $hostBundleExitCode))
[void]$checkBuilder.AppendLine(("qemu_bundle_exit_code: {0}" -f $qemuBundleExitCode))
if ($null -ne $summaryObject.witness_bundle) {
    [void]$checkBuilder.AppendLine(("witness_bundle_exit_code: {0}" -f [int]$summaryObject.witness_bundle.bundle_exit_code))
    [void]$checkBuilder.AppendLine(("witness_bundle_result: {0}" -f [string]$summaryObject.witness_bundle.result))
    if ($null -ne $summaryObject.witness_bundle.witness_summary) {
        [void]$checkBuilder.AppendLine(("witness_bundle_entries: total={0} required_missing={1}" -f $summaryObject.witness_bundle.witness_summary.entry_count, $summaryObject.witness_bundle.witness_summary.required_missing_count))
    }
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
if (-not $SkipWitnessBundle) {
    Write-Host ("witness_output_root={0}" -f $resolvedWitnessBundleOutputRoot)
    Write-Host ("witness_summary={0}" -f $witnessBundleSummaryPathResolved)
}

if ($violations.Count -gt 0) {
    throw "minimal kernel runtime evidence bundle failed"
}
