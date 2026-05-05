param(
    [string]$RuntimeSessionOpenEventWitnessRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-open-event-witness-smoke",
    [string]$BlockedBridgeRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-flow-plan-action-bridge-blocked-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-witness-open-event-witness-world-compare-smoke",
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

function New-OpenEventWitnessWorld {
    param(
        [string]$OpenEventWitnessPath,
        [string]$RepoRoot,
        [string]$Title,
        [string]$Summary
    )

    return [ordered]@{
        schema = "system_compiler.canonical_world/v0"
        kind = "system_compiler.canonical_world"
        name = "minimal_kernel_runtime"
        title = $Title
        summary = $Summary
        subject = [ordered]@{
            profile = "debug"
            board = "armv7a_qemu"
            active_facets = @("session", "opening_flow", "front_page", "witness")
        }
        first_class_terms = @("subject", "session", "witness", "opening", "artifact_target")
        core_questions = @(
            "Can runtime-session opening testimony be compared as a first-class witness-bearing world object?",
            "Does world compare preserve front-page opening route drift without reopening runtime/session evidence?"
        )
        compare_questions = @(
            "If runtime-session opening testimony drifts or collapses, which opening witness breaks first?"
        )
        contract_refs = @(
            (Resolve-FullPath -Path (Join-Path $RepoRoot "docs\system\witness_bundle_v0.md")),
            (Resolve-FullPath -Path (Join-Path $RepoRoot "docs\system\world_compare_v0.md")),
            (Resolve-FullPath -Path (Join-Path $RepoRoot "docs\system\system_compiler_front_page_entry_opening_flow_open_event_witness_v0.md"))
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
$runtimeSessionOpenEventWitnessRootPath = Resolve-FullPath -Path $RuntimeSessionOpenEventWitnessRoot
$blockedBridgeRootPath = Resolve-FullPath -Path $BlockedBridgeRoot
$resolvedOutputRoot = Resolve-FullPath -Path $OutputRoot

if ($Clean) {
    Remove-PathIfExists -Path $resolvedOutputRoot
}

$baselineSourceRoot = Join-Path $resolvedOutputRoot "baseline-source"
$candidateSourceRoot = Join-Path $resolvedOutputRoot "candidate-source"
$baselineBundleRoot = Join-Path $resolvedOutputRoot "baseline"
$candidateBundleRoot = Join-Path $resolvedOutputRoot "candidate"
$worldCompareRoot = Join-Path $resolvedOutputRoot "world_compare"
Ensure-Directory -Path $baselineSourceRoot
Ensure-Directory -Path $candidateSourceRoot
Ensure-Directory -Path $baselineBundleRoot
Ensure-Directory -Path $candidateBundleRoot
Ensure-Directory -Path $worldCompareRoot

$powerShellExe = Resolve-ToolPath -Tool "powershell.exe"
$python = Resolve-ToolPath -Tool $PythonExe

$runtimeSessionOpenEventWitnessSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_open_event_witness_smoke.ps1"
$blockedBridgeSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_opening_flow_plan_action_bridge_blocked_smoke.ps1"
$openEventExporter = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_runtime_session_opening_flow_open_event.py"
$openEventValidator = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_runtime_session_opening_flow_open_event.py"
$openEventWitnessExporter = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_open_event_witness.py"
$openEventWitnessValidator = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_open_event_witness.py"
$witnessExporter = Join-Path $PSScriptRoot "export_system_compiler_witness_bundle.ps1"
$witnessValidator = Join-Path $PSScriptRoot "validate_system_compiler_witness_bundle.py"
$worldCompare = Join-Path $PSScriptRoot "compare_system_compiler_world.py"
$worldCompareValidator = Join-Path $PSScriptRoot "validate_system_compiler_world_compare.py"

$baselineWorldPath = Join-Path $baselineSourceRoot "minimal_kernel_runtime.open_event_witness.world.json"
$candidateWorldPath = Join-Path $candidateSourceRoot "minimal_kernel_runtime.open_event_witness.blocked.world.json"
$baselineBundleSummaryPath = Join-Path $baselineBundleRoot "summary.json"
$candidateBundleSummaryPath = Join-Path $candidateBundleRoot "summary.json"
$worldCompareSummaryPath = Join-Path $worldCompareRoot "summary.json"

Push-Location $repoRoot
try {
    $baselineOpenEventWitnessSummaryPath = Join-Path $runtimeSessionOpenEventWitnessRootPath "witness\front-page.entry-opening-flow.open-event.witness.summary.json"
    if ($Clean -or -not (Test-Path -LiteralPath $baselineOpenEventWitnessSummaryPath)) {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $runtimeSessionOpenEventWitnessSmokeScript,
                "-OutputRoot",
                $runtimeSessionOpenEventWitnessRootPath,
                "-PythonExe",
                $python,
                "-Clean"
            ) `
            -FailureMessage "runtime session open-event witness smoke bootstrap failed"
    } else {
        Write-Host "[WITNESS-OPEN-EVENT-WORLD-COMPARE-SMOKE] baseline_bootstrap=reuse-existing"
    }

    $blockedBridgeSummaryPath = Join-Path $blockedBridgeRootPath "front-page.entry-runtime-session-opening-flow.plan-action.summary.json"
    if ($Clean -or -not (Test-Path -LiteralPath $blockedBridgeSummaryPath)) {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $blockedBridgeSmokeScript,
                "-OutputRoot",
                $blockedBridgeRootPath,
                "-PythonExe",
                $python,
                "-Clean"
            ) `
            -FailureMessage "runtime session blocked bridge smoke bootstrap failed"
    } else {
        Write-Host "[WITNESS-OPEN-EVENT-WORLD-COMPARE-SMOKE] blocked_bridge_bootstrap=reuse-existing"
    }

    if (-not (Test-Path -LiteralPath $baselineOpenEventWitnessSummaryPath)) {
        throw "missing baseline open-event witness summary: $baselineOpenEventWitnessSummaryPath"
    }
    if (-not (Test-Path -LiteralPath $blockedBridgeSummaryPath)) {
        throw "missing blocked bridge summary: $blockedBridgeSummaryPath"
    }

    $candidateOpenEventRoot = Join-Path $candidateSourceRoot "open-event"
    $candidateWitnessRoot = Join-Path $candidateSourceRoot "witness"
    Ensure-Directory -Path $candidateOpenEventRoot
    Ensure-Directory -Path $candidateWitnessRoot

    Invoke-ExternalTool `
        -Executable $python `
        -ArgumentList @(
            $openEventExporter,
            "--bridge",
            $blockedBridgeSummaryPath,
            "--output-root",
            $candidateOpenEventRoot
        ) `
        -FailureMessage "blocked runtime-session open-event export failed"

    $candidateOpenEventSummaryPath = Join-Path $candidateOpenEventRoot "front-page.entry-opening-flow.open-event.summary.json"
    Invoke-ExternalTool `
        -Executable $python `
        -ArgumentList @($openEventValidator, "--summary", $candidateOpenEventSummaryPath) `
        -FailureMessage "blocked runtime-session open-event validation failed"

    Invoke-ExternalTool `
        -Executable $python `
        -ArgumentList @(
            $openEventWitnessExporter,
            "--open-event",
            $candidateOpenEventSummaryPath,
            "--output-root",
            $candidateWitnessRoot
        ) `
        -FailureMessage "blocked runtime-session open-event witness export failed"

    $candidateOpenEventWitnessSummaryPath = Join-Path $candidateWitnessRoot "front-page.entry-opening-flow.open-event.witness.summary.json"
    Invoke-ExternalTool `
        -Executable $python `
        -ArgumentList @($openEventWitnessValidator, "--summary", $candidateOpenEventWitnessSummaryPath) `
        -FailureMessage "blocked runtime-session open-event witness validation failed"

    $baselineWorld = New-OpenEventWitnessWorld `
        -OpenEventWitnessPath $baselineOpenEventWitnessSummaryPath `
        -RepoRoot $repoRoot `
        -Title "Minimal Kernel Runtime Open Event Witness World Compare Smoke" `
        -Summary "A targeted witness world that compares runtime-session opening testimony as a first-class witness-bearing object."
    $candidateWorld = New-OpenEventWitnessWorld `
        -OpenEventWitnessPath $candidateOpenEventWitnessSummaryPath `
        -RepoRoot $repoRoot `
        -Title "Minimal Kernel Runtime Open Event Witness Blocked World Compare Smoke" `
        -Summary "A targeted witness world that proves blocked runtime-session opening testimony collapses through witness_bundle into world_compare."
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
        -FailureMessage "baseline witness bundle export failed for open_event_witness world compare smoke"

    Invoke-ExternalTool `
        -Executable $python `
        -ArgumentList @($witnessValidator, "--summary", $baselineBundleSummaryPath) `
        -FailureMessage "baseline witness bundle validation failed for open_event_witness world compare smoke"

    $candidateExportFailedAsExpected = $false
    try {
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
            -FailureMessage "candidate witness bundle export failed for open_event_witness world compare smoke"
        throw "candidate witness export was expected to fail because the open-event witness is blocked"
    } catch {
        if (-not (Test-Path -LiteralPath $candidateBundleSummaryPath)) {
            throw
        }
        $candidateExportFailedAsExpected = $true
        Write-Host ("[WITNESS-OPEN-EVENT-WORLD-COMPARE-SMOKE] candidate_export_failed_as_expected={0}" -f $_.Exception.Message)
    }

    if (-not $candidateExportFailedAsExpected) {
        throw "candidate witness export did not report the expected blocked open-event witness failure"
    }

    Invoke-ExternalTool `
        -Executable $python `
        -ArgumentList @($witnessValidator, "--summary", $candidateBundleSummaryPath) `
        -FailureMessage "candidate witness bundle validation failed for open_event_witness world compare smoke"

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
        -FailureMessage "world compare failed for open_event_witness world compare smoke"

    Invoke-ExternalTool `
        -Executable $python `
        -ArgumentList @($worldCompareValidator, "--summary", $worldCompareSummaryPath) `
        -FailureMessage "world compare validation failed for open_event_witness world compare smoke"
} finally {
    Pop-Location
}

$baselineBundleSummary = Load-JsonObject -Path $baselineBundleSummaryPath
$candidateBundleSummary = Load-JsonObject -Path $candidateBundleSummaryPath
$candidateOpenEventWitness = Load-JsonObject -Path $candidateOpenEventWitnessSummaryPath
$worldCompareSummary = Load-JsonObject -Path $worldCompareSummaryPath

$baselineEntry = @($baselineBundleSummary.witness_entries | Where-Object { [string]$_.kind -eq "open_event_witness" }) | Select-Object -First 1
$candidateEntry = @($candidateBundleSummary.witness_entries | Where-Object { [string]$_.kind -eq "open_event_witness" }) | Select-Object -First 1
$witnessChange = @($worldCompareSummary.witness_changes | Where-Object { [string]$_.id -eq "runtime_session_open_event_witness" }) | Select-Object -First 1

Assert-Condition `
    -Condition ([string]$baselineBundleSummary.result -eq "ok") `
    -Message ("expected baseline bundle result ok but got '{0}'" -f [string]$baselineBundleSummary.result)
Assert-Condition `
    -Condition ([string]$candidateBundleSummary.result -eq "fail") `
    -Message ("expected candidate bundle result fail but got '{0}'" -f [string]$candidateBundleSummary.result)
Assert-Condition `
    -Condition ([string]$candidateOpenEventWitness.result -eq "fail") `
    -Message ("expected blocked candidate open-event witness result fail but got '{0}'" -f [string]$candidateOpenEventWitness.result)
Assert-Condition `
    -Condition ([string]$candidateOpenEventWitness.judgment.witness_status -eq "fail") `
    -Message ("expected blocked candidate open-event witness status fail but got '{0}'" -f [string]$candidateOpenEventWitness.judgment.witness_status)
Assert-Condition `
    -Condition ([string]$worldCompareSummary.world_verdict -eq "collapsed") `
    -Message ("expected world verdict collapsed but got '{0}'" -f [string]$worldCompareSummary.world_verdict)
Assert-Condition `
    -Condition ($null -ne $baselineEntry -and [string]$baselineEntry.status -eq "ok") `
    -Message "expected baseline bundle open_event_witness entry status ok"
Assert-Condition `
    -Condition ($null -ne $candidateEntry -and [string]$candidateEntry.status -eq "fail") `
    -Message "expected candidate bundle open_event_witness entry status fail"
Assert-Condition `
    -Condition ($null -ne $witnessChange) `
    -Message "expected world compare witness change for runtime_session_open_event_witness"
Assert-Condition `
    -Condition ([string]$witnessChange.impact -eq "regression") `
    -Message ("expected open_event_witness change impact regression but got '{0}'" -f [string]$witnessChange.impact)
Assert-Condition `
    -Condition ([string]$witnessChange.left_status -eq "ok" -and [string]$witnessChange.right_status -eq "fail") `
    -Message "expected open_event_witness status transition ok -> fail"
Assert-Condition `
    -Condition (@($candidateEntry.observations | Where-Object { [string]$_ -eq "open_event_status=blocked" }).Count -eq 1) `
    -Message "expected blocked open_event_status observation in candidate witness entry"
Assert-Condition `
    -Condition (@($candidateBundleSummary.violations | Where-Object { [string]$_ -eq "failed witness: runtime_session_open_event_witness" }).Count -eq 1) `
    -Message "expected candidate witness bundle failure to name runtime_session_open_event_witness"
Assert-Condition `
    -Condition (@($worldCompareSummary.collapse_surface.regressed_witnesses | Where-Object { [string]$_ -eq "runtime_session_open_event_witness" }).Count -eq 1) `
    -Message "expected collapse surface to include runtime_session_open_event_witness"
Assert-Condition `
    -Condition (@($worldCompareSummary.collapse_surface.affected_focus | Where-Object { [string]$_ -eq "front_page" }).Count -ge 1) `
    -Message "expected collapse surface to retain front_page focus"
Assert-Condition `
    -Condition ([int]$worldCompareSummary.witness_summary.regression_count -ge 1) `
    -Message "expected at least one witness regression in world compare summary"

Write-Host "==> system compiler witness open-event witness world compare smoke"
Write-Host ("baseline_open_event_witness={0}" -f $baselineOpenEventWitnessSummaryPath)
Write-Host ("candidate_open_event_witness={0}" -f $candidateOpenEventWitnessSummaryPath)
Write-Host ("baseline_bundle={0}" -f $baselineBundleSummaryPath)
Write-Host ("candidate_bundle={0}" -f $candidateBundleSummaryPath)
Write-Host ("world_compare={0}" -f $worldCompareSummaryPath)
Write-Host ("world_verdict={0}" -f [string]$worldCompareSummary.world_verdict)
Write-Host ("regressed_witnesses={0}" -f (@($worldCompareSummary.collapse_surface.regressed_witnesses) -join ","))
Write-Host "ok=1"
