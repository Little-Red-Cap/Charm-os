param(
    [string]$ConsumerRoot = "cmake-build-minimal-kernel-runtime-session-witness-inspect-compare-consumer-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-witness-open-event-witness-world-compare-neutral-drift-smoke",
    [string]$TargetFocusId = "runtime-regression",
    [string]$PythonExe = "python",
    [switch]$Clean
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

    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Remove-PathIfExists {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
}

function Resolve-ToolPath {
    param(
        [string]$Tool
    )

    $command = Get-Command $Tool -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    if (Test-Path $Tool) {
        return (Resolve-Path $Tool).Path
    }

    throw "tool not found: $Tool"
}

function Invoke-ExternalTool {
    param(
        [string]$Executable,
        [string[]]$ArgumentList,
        [string]$FailureMessage
    )

    Write-Host ("==> {0}" -f [System.IO.Path]::GetFileName($Executable))
    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & $Executable @ArgumentList
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

function Write-Utf8Json {
    param(
        [string]$Path,
        $Value
    )

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        Ensure-Directory -Path $parent
    }

    $json = ($Value | ConvertTo-Json -Depth 100) + "`n"
    $encoding = [System.Text.UTF8Encoding]::new($false)
    [System.IO.File]::WriteAllText($Path, $json, $encoding)
}

function Assert-Condition {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Set-DefaultConsumerFocus {
    param(
        $Consumer,
        [string]$FocusId
    )

    $focusEntries = @($Consumer.focus_entries)
    if (@($focusEntries).Count -eq 0) {
        throw "consumer focus entries are empty"
    }

    $selectedEntry = @($focusEntries | Where-Object { [string]$_.focus_id -eq $FocusId }) | Select-Object -First 1
    if ($null -eq $selectedEntry) {
        throw "focus entry not found: $FocusId"
    }

    $reorderedEntries = [System.Collections.Generic.List[object]]::new()
    $reorderedEntries.Add($selectedEntry) | Out-Null
    foreach ($entry in $focusEntries) {
        if ([string]$entry.focus_id -ne $FocusId) {
            $reorderedEntries.Add($entry) | Out-Null
        }
    }

    $orderedEntries = @($reorderedEntries)
    $Consumer.focus_entries = $orderedEntries
    $Consumer.default_focus = $orderedEntries[0]
    $Consumer.default_explain_hop = $orderedEntries[0].preferred_explain_hop
    $Consumer.fallback_explain_hops = @($orderedEntries | Select-Object -Skip 1 | ForEach-Object { $_.preferred_explain_hop })

    if ($null -ne $Consumer.consumer_status) {
        $Consumer.consumer_status.default_focus_id = [string]$orderedEntries[0].focus_id
    }

    if ($null -ne $Consumer.readiness_surface) {
        $Consumer.readiness_surface.changed_focus_ids = @(
            $orderedEntries |
                Where-Object { [bool]$_.changed } |
                ForEach-Object { [string]$_.focus_id }
        )
        $Consumer.readiness_surface.actionable_focus_ids = @(
            $orderedEntries |
                Where-Object { [string]$_.focus_kind -ne "steady_state" } |
                ForEach-Object { [string]$_.focus_id }
        )
    }

    return $Consumer
}

function Invoke-RuntimeSessionOpenEventWitnessChain {
    param(
        [string]$Python,
        [string]$BridgeExportScript,
        [string]$BridgeValidateScript,
        [string]$OpenEventExportScript,
        [string]$OpenEventValidateScript,
        [string]$OpenEventWitnessExportScript,
        [string]$OpenEventWitnessValidateScript,
        [string]$ConsumerSummaryPath,
        [string]$CaseRoot,
        [string]$CaseLabel
    )

    $bridgeRoot = Join-Path $CaseRoot "bridge"
    $openEventRoot = Join-Path $CaseRoot "open-event"
    $witnessRoot = Join-Path $CaseRoot "witness"

    Invoke-ExternalTool `
        -Executable $Python `
        -ArgumentList @(
            $BridgeExportScript,
            "--consumer",
            $ConsumerSummaryPath,
            "--output-root",
            $bridgeRoot
        ) `
        -FailureMessage ("{0}: bridge export failed" -f $CaseLabel)

    $bridgeSummaryPath = Join-Path $bridgeRoot "front-page.entry-runtime-session-opening-flow.plan-action.summary.json"
    Invoke-ExternalTool `
        -Executable $Python `
        -ArgumentList @($BridgeValidateScript, "--summary", $bridgeSummaryPath) `
        -FailureMessage ("{0}: bridge validation failed" -f $CaseLabel)

    Invoke-ExternalTool `
        -Executable $Python `
        -ArgumentList @(
            $OpenEventExportScript,
            "--bridge",
            $bridgeSummaryPath,
            "--output-root",
            $openEventRoot
        ) `
        -FailureMessage ("{0}: open-event export failed" -f $CaseLabel)

    $openEventSummaryPath = Join-Path $openEventRoot "front-page.entry-opening-flow.open-event.summary.json"
    Invoke-ExternalTool `
        -Executable $Python `
        -ArgumentList @($OpenEventValidateScript, "--summary", $openEventSummaryPath) `
        -FailureMessage ("{0}: open-event validation failed" -f $CaseLabel)

    Invoke-ExternalTool `
        -Executable $Python `
        -ArgumentList @(
            $OpenEventWitnessExportScript,
            "--open-event",
            $openEventSummaryPath,
            "--output-root",
            $witnessRoot
        ) `
        -FailureMessage ("{0}: open-event witness export failed" -f $CaseLabel)

    $witnessSummaryPath = Join-Path $witnessRoot "front-page.entry-opening-flow.open-event.witness.summary.json"
    Invoke-ExternalTool `
        -Executable $Python `
        -ArgumentList @($OpenEventWitnessValidateScript, "--summary", $witnessSummaryPath) `
        -FailureMessage ("{0}: open-event witness validation failed" -f $CaseLabel)

    return [ordered]@{
        BridgeSummaryPath = $bridgeSummaryPath
        OpenEventSummaryPath = $openEventSummaryPath
        WitnessSummaryPath = $witnessSummaryPath
    }
}

function New-OpenEventWitnessWorld {
    param(
        [string]$OpenEventWitnessPath,
        [string]$RepoRoot
    )

    return [ordered]@{
        schema = "system_compiler.canonical_world/v0"
        kind = "system_compiler.canonical_world"
        name = "minimal_kernel_runtime"
        title = "Minimal Kernel Runtime Open Event Witness Neutral Drift World Compare Smoke"
        summary = "A targeted witness world that proves runtime-session opening testimony can drift at the testimony layer while the world remains standing."
        subject = [ordered]@{
            profile = "debug"
            board = "armv7a_qemu"
            active_facets = @("session", "opening_flow", "front_page", "witness")
        }
        first_class_terms = @("subject", "session", "witness", "opening", "artifact_target")
        core_questions = @(
            "Can runtime-session opening testimony drift without forcing world compare to reinterpret runtime evidence?",
            "Does witness bundle preserve consumer-chosen route drift as a first-class testimony change?"
        )
        compare_questions = @(
            "If runtime-session opening testimony drifts but still stands, does world compare keep that change at the witness layer?"
        )
        contract_refs = @(
            (Resolve-FullPath -Path (Join-Path $RepoRoot "docs\architecture\system_compiler_roadmap.md")),
            (Resolve-FullPath -Path (Join-Path $RepoRoot "docs\architecture\system_compiler_vocabulary_v0.md")),
            (Resolve-FullPath -Path (Join-Path $RepoRoot "docs\archive\system-compiler-front-page-v0\README.md"))
        )
        witness_plan = @(
            [ordered]@{
                id = "runtime_session_open_event_witness"
                kind = "open_event_witness"
                label = "runtime-session-open-event-witness"
                role = "runtime session explainable opening testimony"
                layer = "opening_flow"
                required = $true
                witness_focus = @("front_page", "opening_flow", "runtime_session", "session_witness", "artifact_target")
                case = "open-event-witness::runtime-session-inspect-consumer::open-default"
                path = $OpenEventWitnessPath
            }
        )
    }
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$consumerRootPath = Resolve-FullPath -Path $ConsumerRoot
$resolvedOutputRoot = Resolve-FullPath -Path $OutputRoot

if ($Clean) {
    Remove-PathIfExists -Path $resolvedOutputRoot
}

$baselineSourceRoot = Join-Path $resolvedOutputRoot "baseline-source"
$candidateSourceRoot = Join-Path $resolvedOutputRoot "candidate-source"
$witnessCompareRoot = Join-Path $resolvedOutputRoot "open-event-witness-compare"
$baselineBundleRoot = Join-Path $resolvedOutputRoot "baseline"
$candidateBundleRoot = Join-Path $resolvedOutputRoot "candidate"
$worldCompareRoot = Join-Path $resolvedOutputRoot "world_compare"
Ensure-Directory -Path $baselineSourceRoot
Ensure-Directory -Path $candidateSourceRoot
Ensure-Directory -Path $witnessCompareRoot
Ensure-Directory -Path $baselineBundleRoot
Ensure-Directory -Path $candidateBundleRoot
Ensure-Directory -Path $worldCompareRoot

$powerShellExe = Resolve-ToolPath -Tool "powershell.exe"
$python = Resolve-ToolPath -Tool $PythonExe

$consumerSmokeScript = Join-Path $PSScriptRoot "system_compiler_minimal_kernel_runtime_session_witness_inspect_compare_consumer_smoke.ps1"
$bridgeExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_runtime_session_opening_flow_plan_action.py"
$bridgeValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_runtime_session_opening_flow_plan_action.py"
$openEventExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_runtime_session_opening_flow_open_event.py"
$openEventValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_runtime_session_opening_flow_open_event.py"
$openEventWitnessExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_open_event_witness.py"
$openEventWitnessValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_open_event_witness.py"
$openEventWitnessCompareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_flow_open_event_witness.py"
$openEventWitnessCompareValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_open_event_witness_compare.py"
$witnessExporter = Join-Path $PSScriptRoot "export_system_compiler_witness_bundle.ps1"
$witnessValidator = Join-Path $PSScriptRoot "validate_system_compiler_witness_bundle.py"
$worldCompare = Join-Path $PSScriptRoot "compare_system_compiler_world.py"
$worldCompareValidator = Join-Path $PSScriptRoot "validate_system_compiler_world_compare.py"

$baselineWorldPath = Join-Path $baselineSourceRoot "minimal_kernel_runtime.open_event_witness.neutral_drift.world.json"
$candidateWorldPath = Join-Path $candidateSourceRoot "minimal_kernel_runtime.open_event_witness.neutral_drift.world.json"
$baselineBundleSummaryPath = Join-Path $baselineBundleRoot "summary.json"
$candidateBundleSummaryPath = Join-Path $candidateBundleRoot "summary.json"
$worldCompareSummaryPath = Join-Path $worldCompareRoot "summary.json"
$openEventWitnessCompareSummaryPath = Join-Path $witnessCompareRoot "front-page.entry-opening-flow.open-event.witness.compare.summary.json"

Push-Location $repoRoot
try {
    $consumerSummaryPath = Join-Path $consumerRootPath "session-witness.inspect.compare.consumer.summary.json"
    if ($Clean -or -not (Test-Path -LiteralPath $consumerSummaryPath)) {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $consumerSmokeScript,
                "-OutputRoot",
                $consumerRootPath,
                "-PythonExe",
                $python,
                "-Clean"
            ) `
            -FailureMessage "runtime session inspect compare consumer smoke bootstrap failed"
    } else {
        Write-Host "[WITNESS-OPEN-EVENT-WORLD-COMPARE-NEUTRAL-DRIFT-SMOKE] consumer_bootstrap=reuse-existing"
    }

    if (-not (Test-Path -LiteralPath $consumerSummaryPath)) {
        throw "missing consumer summary: $consumerSummaryPath"
    }

    $baselineChain = Invoke-RuntimeSessionOpenEventWitnessChain `
        -Python $python `
        -BridgeExportScript $bridgeExportScript `
        -BridgeValidateScript $bridgeValidateScript `
        -OpenEventExportScript $openEventExportScript `
        -OpenEventValidateScript $openEventValidateScript `
        -OpenEventWitnessExportScript $openEventWitnessExportScript `
        -OpenEventWitnessValidateScript $openEventWitnessValidateScript `
        -ConsumerSummaryPath $consumerSummaryPath `
        -CaseRoot $baselineSourceRoot `
        -CaseLabel "baseline runtime-session opening witness chain"

    $mutatedConsumer = Load-JsonObject -Path $consumerSummaryPath
    $mutatedConsumer = Set-DefaultConsumerFocus -Consumer $mutatedConsumer -FocusId $TargetFocusId
    $mutatedConsumerPath = Join-Path $candidateSourceRoot "session-witness.inspect.compare.consumer.neutral-drift.summary.json"
    Write-Utf8Json -Path $mutatedConsumerPath -Value $mutatedConsumer

    $candidateChain = Invoke-RuntimeSessionOpenEventWitnessChain `
        -Python $python `
        -BridgeExportScript $bridgeExportScript `
        -BridgeValidateScript $bridgeValidateScript `
        -OpenEventExportScript $openEventExportScript `
        -OpenEventValidateScript $openEventValidateScript `
        -OpenEventWitnessExportScript $openEventWitnessExportScript `
        -OpenEventWitnessValidateScript $openEventWitnessValidateScript `
        -ConsumerSummaryPath $mutatedConsumerPath `
        -CaseRoot $candidateSourceRoot `
        -CaseLabel "candidate runtime-session opening witness chain"

    Invoke-ExternalTool `
        -Executable $python `
        -ArgumentList @(
            $openEventWitnessCompareScript,
            "--baseline",
            $baselineChain.WitnessSummaryPath,
            "--candidate",
            $candidateChain.WitnessSummaryPath,
            "--output-root",
            $witnessCompareRoot,
            "--summary",
            $openEventWitnessCompareSummaryPath,
            "--report-markdown",
            (Join-Path $witnessCompareRoot "report.md"),
            "--check-text",
            (Join-Path $witnessCompareRoot "check.txt")
        ) `
        -FailureMessage "runtime-session open-event witness compare failed for neutral drift smoke"

    Invoke-ExternalTool `
        -Executable $python `
        -ArgumentList @($openEventWitnessCompareValidateScript, "--summary", $openEventWitnessCompareSummaryPath) `
        -FailureMessage "runtime-session open-event witness compare validation failed for neutral drift smoke"

    $baselineWorld = New-OpenEventWitnessWorld -OpenEventWitnessPath $baselineChain.WitnessSummaryPath -RepoRoot $repoRoot
    $candidateWorld = New-OpenEventWitnessWorld -OpenEventWitnessPath $candidateChain.WitnessSummaryPath -RepoRoot $repoRoot
    Write-Utf8Json -Path $baselineWorldPath -Value $baselineWorld
    Write-Utf8Json -Path $candidateWorldPath -Value $candidateWorld

    Invoke-ExternalTool `
        -Executable $powerShellExe `
        -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $witnessExporter,
            "-CanonicalWorld",
            $baselineWorldPath,
            "-OutputRoot",
            $baselineBundleRoot,
            "-OutputPath",
            $baselineBundleSummaryPath,
            "-ReportMarkdownPath",
            (Join-Path $baselineBundleRoot "report.md"),
            "-CheckTextPath",
            (Join-Path $baselineBundleRoot "check.txt")
        ) `
        -FailureMessage "baseline witness bundle export failed for neutral drift smoke"

    Invoke-ExternalTool `
        -Executable $python `
        -ArgumentList @($witnessValidator, "--summary", $baselineBundleSummaryPath) `
        -FailureMessage "baseline witness bundle validation failed for neutral drift smoke"

    Invoke-ExternalTool `
        -Executable $powerShellExe `
        -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $witnessExporter,
            "-CanonicalWorld",
            $candidateWorldPath,
            "-OutputRoot",
            $candidateBundleRoot,
            "-OutputPath",
            $candidateBundleSummaryPath,
            "-ReportMarkdownPath",
            (Join-Path $candidateBundleRoot "report.md"),
            "-CheckTextPath",
            (Join-Path $candidateBundleRoot "check.txt")
        ) `
        -FailureMessage "candidate witness bundle export failed for neutral drift smoke"

    Invoke-ExternalTool `
        -Executable $python `
        -ArgumentList @($witnessValidator, "--summary", $candidateBundleSummaryPath) `
        -FailureMessage "candidate witness bundle validation failed for neutral drift smoke"

    Invoke-ExternalTool `
        -Executable $python `
        -ArgumentList @(
            $worldCompare,
            "--baseline",
            $baselineBundleSummaryPath,
            "--candidate",
            $candidateBundleSummaryPath,
            "--output-root",
            $worldCompareRoot,
            "--summary",
            $worldCompareSummaryPath,
            "--report-markdown",
            (Join-Path $worldCompareRoot "report.md"),
            "--check-text",
            (Join-Path $worldCompareRoot "check.txt")
        ) `
        -FailureMessage "world compare failed for neutral drift smoke"

    Invoke-ExternalTool `
        -Executable $python `
        -ArgumentList @($worldCompareValidator, "--summary", $worldCompareSummaryPath) `
        -FailureMessage "world compare validation failed for neutral drift smoke"
} finally {
    Pop-Location
}

$baselineOpenEvent = Load-JsonObject -Path $baselineChain.OpenEventSummaryPath
$candidateOpenEvent = Load-JsonObject -Path $candidateChain.OpenEventSummaryPath
$baselineOpenEventWitness = Load-JsonObject -Path $baselineChain.WitnessSummaryPath
$candidateOpenEventWitness = Load-JsonObject -Path $candidateChain.WitnessSummaryPath
$baselineBundleSummary = Load-JsonObject -Path $baselineBundleSummaryPath
$candidateBundleSummary = Load-JsonObject -Path $candidateBundleSummaryPath
$worldCompareSummary = Load-JsonObject -Path $worldCompareSummaryPath
$openEventWitnessCompareSummary = Load-JsonObject -Path $openEventWitnessCompareSummaryPath
$baselineBundleEntry = @($baselineBundleSummary.witness_entries | Where-Object { [string]$_.id -eq "runtime_session_open_event_witness" }) | Select-Object -First 1
$candidateBundleEntry = @($candidateBundleSummary.witness_entries | Where-Object { [string]$_.id -eq "runtime_session_open_event_witness" }) | Select-Object -First 1
$witnessChange = @($worldCompareSummary.witness_changes | Where-Object { [string]$_.id -eq "runtime_session_open_event_witness" }) | Select-Object -First 1
$targetFocusObservationPattern = "*focus:$TargetFocusId*"

Assert-Condition `
    -Condition ([string]$baselineOpenEvent.open_event.status -eq "accepted_with_drift") `
    -Message ("expected baseline open event status accepted_with_drift but got '{0}'" -f [string]$baselineOpenEvent.open_event.status)
Assert-Condition `
    -Condition ([string]$candidateOpenEvent.open_event.status -eq "accepted_with_drift") `
    -Message ("expected candidate open event status accepted_with_drift but got '{0}'" -f [string]$candidateOpenEvent.open_event.status)
Assert-Condition `
    -Condition ([string]$baselineOpenEvent.open_event.opening_input_refs.selected_focus_ref.id -ne $TargetFocusId) `
    -Message ("baseline selected focus should differ from target focus '{0}'" -f $TargetFocusId)
Assert-Condition `
    -Condition ([string]$candidateOpenEvent.open_event.opening_input_refs.selected_focus_ref.id -eq $TargetFocusId) `
    -Message ("candidate selected focus should equal target focus '{0}'" -f $TargetFocusId)
Assert-Condition `
    -Condition ([string]$baselineOpenEvent.open_event.opening_input_refs.selected_artifact_ref.id -ne [string]$candidateOpenEvent.open_event.opening_input_refs.selected_artifact_ref.id) `
    -Message "expected candidate selected artifact to differ from baseline selected artifact"
Assert-Condition `
    -Condition ([string]$baselineOpenEvent.open_event.opening_input_refs.selected_explain_hop_ref.id -ne [string]$candidateOpenEvent.open_event.opening_input_refs.selected_explain_hop_ref.id) `
    -Message "expected candidate selected explain hop to differ from baseline selected explain hop"
Assert-Condition `
    -Condition ([string]$baselineOpenEventWitness.result -eq "ok" -and [string]$candidateOpenEventWitness.result -eq "ok") `
    -Message "expected both baseline and candidate open-event witness results to remain ok"
Assert-Condition `
    -Condition ([string]$baselineOpenEventWitness.judgment.witness_status -eq "ok" -and [string]$candidateOpenEventWitness.judgment.witness_status -eq "ok") `
    -Message "expected both baseline and candidate open-event witness statuses to remain ok"
Assert-Condition `
    -Condition ([string]$openEventWitnessCompareSummary.witness_verdict -eq "drifted") `
    -Message ("expected open-event witness compare verdict drifted but got '{0}'" -f [string]$openEventWitnessCompareSummary.witness_verdict)
Assert-Condition `
    -Condition ([string]$openEventWitnessCompareSummary.witness_status.baseline_witness_status -eq "ok" -and [string]$openEventWitnessCompareSummary.witness_status.candidate_witness_status -eq "ok") `
    -Message "expected open-event witness compare to keep witness status ok -> ok"
Assert-Condition `
    -Condition ([string]$baselineBundleSummary.result -eq "ok" -and [string]$candidateBundleSummary.result -eq "ok") `
    -Message "expected both baseline and candidate witness bundles to remain ok"
Assert-Condition `
    -Condition ($null -ne $baselineBundleEntry -and [string]$baselineBundleEntry.status -eq "ok") `
    -Message "expected baseline bundle open_event_witness entry status ok"
Assert-Condition `
    -Condition ($null -ne $candidateBundleEntry -and [string]$candidateBundleEntry.status -eq "ok") `
    -Message "expected candidate bundle open_event_witness entry status ok"
Assert-Condition `
    -Condition ([string]$worldCompareSummary.world_verdict -eq "standing") `
    -Message ("expected world verdict standing but got '{0}'" -f [string]$worldCompareSummary.world_verdict)
Assert-Condition `
    -Condition ($null -ne $witnessChange) `
    -Message "expected world compare witness change for runtime_session_open_event_witness"
Assert-Condition `
    -Condition ([string]$witnessChange.impact -eq "neutral") `
    -Message ("expected open_event_witness change impact neutral but got '{0}'" -f [string]$witnessChange.impact)
Assert-Condition `
    -Condition ([string]$witnessChange.left_status -eq "ok" -and [string]$witnessChange.right_status -eq "ok") `
    -Message "expected open_event_witness witness status transition ok -> ok"
Assert-Condition `
    -Condition ([int]$worldCompareSummary.witness_summary.regression_count -eq 0) `
    -Message "expected zero witness regressions in world compare summary"
Assert-Condition `
    -Condition ([int]$worldCompareSummary.witness_summary.neutral_change_count -ge 1) `
    -Message "expected at least one neutral witness change in world compare summary"
Assert-Condition `
    -Condition (@($worldCompareSummary.collapse_surface.regressed_witnesses | Where-Object { [string]$_ -eq "runtime_session_open_event_witness" }).Count -eq 0) `
    -Message "did not expect runtime_session_open_event_witness to enter collapse surface"
Assert-Condition `
    -Condition (@($candidateBundleEntry.observations | Where-Object { [string]$_ -like $targetFocusObservationPattern }).Count -ge 1) `
    -Message ("expected candidate bundle observations to retain target focus '{0}'" -f $TargetFocusId)
Assert-Condition `
    -Condition (@($witnessChange.observations_added | Where-Object { [string]$_ -like $targetFocusObservationPattern }).Count -ge 1) `
    -Message ("expected witness change observations_added to name target focus '{0}'" -f $TargetFocusId)
Assert-Condition `
    -Condition (@($witnessChange.artifact_refs_added | Where-Object { [string]$_ -eq [string]$candidateOpenEvent.open_event.opening_input_refs.selected_artifact_ref.path }).Count -ge 1) `
    -Message "expected witness change artifact_refs_added to include candidate selected artifact"
Assert-Condition `
    -Condition (@($witnessChange.observations_removed | Where-Object { [string]$_ -like "*focus:$([string]$baselineOpenEvent.open_event.opening_input_refs.selected_focus_ref.id)*" }).Count -ge 1) `
    -Message "expected witness change observations_removed to retain baseline selected focus route"

Write-Host "==> system compiler witness open-event witness world compare neutral drift smoke"
Write-Host ("baseline_open_event_witness={0}" -f $baselineChain.WitnessSummaryPath)
Write-Host ("candidate_open_event_witness={0}" -f $candidateChain.WitnessSummaryPath)
Write-Host ("baseline_bundle={0}" -f $baselineBundleSummaryPath)
Write-Host ("candidate_bundle={0}" -f $candidateBundleSummaryPath)
Write-Host ("open_event_witness_compare={0}" -f $openEventWitnessCompareSummaryPath)
Write-Host ("world_compare={0}" -f $worldCompareSummaryPath)
Write-Host ("target_focus={0}" -f $TargetFocusId)
Write-Host ("world_verdict={0}" -f [string]$worldCompareSummary.world_verdict)
Write-Host "ok=1"
