param(
    [string]$OpenEventWitnessRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-open-event-witness-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-witness-open-event-witness-export-smoke",
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
        [string]$RepoRoot
    )

    return [ordered]@{
        schema = "system_compiler.canonical_world/v0"
        kind = "system_compiler.canonical_world"
        name = "minimal_kernel_runtime"
        title = "Minimal Kernel Runtime Open Event Witness Export Smoke"
        summary = "A targeted witness world that proves runtime-session open-event witness can be lifted into witness_bundle as a first-class testimony object."
        subject = [ordered]@{
            profile = "debug"
            board = "armv7a_qemu"
            active_facets = @("session", "opening_flow", "front_page", "witness")
        }
        first_class_terms = @("subject", "session", "witness", "opening", "artifact_target")
        core_questions = @(
            "Can runtime-session opening testimony be exported as a first-class witness entry?",
            "Does witness_bundle preserve the route and facade judgment without reinterpreting session evidence?"
        )
        compare_questions = @(
            "If the session opening drifts, does the testimony witness remain explainable through the bundle?"
        )
        contract_refs = @(
            (Resolve-FullPath -Path (Join-Path $RepoRoot "docs\system\witness_bundle_v0.md")),
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
$openEventWitnessRootPath = Resolve-FullPath -Path $OpenEventWitnessRoot
$resolvedOutputRoot = Resolve-FullPath -Path $OutputRoot

if ($Clean) {
    Remove-PathIfExists -Path $resolvedOutputRoot
}

$sourceRoot = Join-Path $resolvedOutputRoot "source"
$bundleRoot = Join-Path $resolvedOutputRoot "bundle"
Ensure-Directory -Path $sourceRoot
Ensure-Directory -Path $bundleRoot

$powerShellExe = Resolve-ToolPath -Tool "powershell.exe"
$python = Resolve-ToolPath -Tool $PythonExe
$openEventWitnessSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_open_event_witness_smoke.ps1"
$witnessExporter = Join-Path $PSScriptRoot "export_system_compiler_witness_bundle.ps1"
$witnessValidator = Join-Path $PSScriptRoot "validate_system_compiler_witness_bundle.py"

$worldPath = Join-Path $sourceRoot "minimal_kernel_runtime.open_event_witness.world.json"
$bundleSummaryPath = Join-Path $bundleRoot "summary.json"
$bundleReportPath = Join-Path $bundleRoot "report.md"
$bundleCheckPath = Join-Path $bundleRoot "check.txt"

Push-Location $repoRoot
try {
    $openEventWitnessSummaryPath = Join-Path $openEventWitnessRootPath "witness\front-page.entry-opening-flow.open-event.witness.summary.json"
    if ($Clean -or -not (Test-Path -LiteralPath $openEventWitnessSummaryPath)) {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $openEventWitnessSmokeScript,
                "-OutputRoot",
                $openEventWitnessRootPath,
                "-PythonExe",
                $python,
                "-Clean"
            ) `
            -FailureMessage "runtime session open-event witness smoke bootstrap failed"
    } else {
        Write-Host "[WITNESS-OPEN-EVENT-WITNESS-SMOKE] bootstrap=reuse-existing"
    }

    if (-not (Test-Path -LiteralPath $openEventWitnessSummaryPath)) {
        throw "missing open event witness summary: $openEventWitnessSummaryPath"
    }

    $world = New-OpenEventWitnessWorld -OpenEventWitnessPath $openEventWitnessSummaryPath -RepoRoot $repoRoot
    Write-Utf8Json -Path $worldPath -Value $world

    Invoke-ExternalTool `
        -Executable $powerShellExe `
        -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $witnessExporter,
            "-CanonicalWorld",
            $worldPath,
            "-OutputRoot",
            $bundleRoot,
            "-OutputPath",
            $bundleSummaryPath,
            "-ReportMarkdownPath",
            $bundleReportPath,
            "-CheckTextPath",
            $bundleCheckPath
        ) `
        -FailureMessage "witness bundle export failed for open_event_witness smoke"

    Invoke-ExternalTool `
        -Executable $python `
        -ArgumentList @($witnessValidator, "--summary", $bundleSummaryPath) `
        -FailureMessage "witness bundle validation failed for open_event_witness smoke"
} finally {
    Pop-Location
}

$bundleSummary = Load-JsonObject -Path $bundleSummaryPath
$entry = @($bundleSummary.witness_entries | Where-Object { [string]$_.kind -eq "open_event_witness" }) | Select-Object -First 1
if ($null -eq $entry) {
    throw "witness bundle did not include an open_event_witness entry"
}

$sourceWitness = Load-JsonObject -Path $openEventWitnessSummaryPath
$supportingSurface = @($bundleSummary.front_page.supporting_surfaces | Where-Object { [string]$_.summary_schema -eq "system_compiler.front_page_entry_opening_flow_open_event_witness/v0" }) | Select-Object -First 1

Assert-Condition `
    -Condition ([string]$bundleSummary.result -eq "ok") `
    -Message ("expected bundle result ok but got '{0}'" -f [string]$bundleSummary.result)
Assert-Condition `
    -Condition ([string]$entry.status -eq "ok") `
    -Message ("expected open_event_witness entry status ok but got '{0}'" -f [string]$entry.status)
Assert-Condition `
    -Condition ([string]$entry.source_path -eq (Resolve-FullPath -Path $openEventWitnessSummaryPath)) `
    -Message "open_event_witness entry should point to the source witness summary"
Assert-Condition `
    -Condition (@($entry.observations | Where-Object { [string]$_ -eq "open_event_status=accepted_with_drift" }).Count -eq 1) `
    -Message "expected accepted_with_drift observation to survive witness export"
Assert-Condition `
    -Condition (@($entry.observations | Where-Object { [string]$_ -like "opening_input_refs=*" }).Count -eq 1) `
    -Message "expected opening_input_refs observation to survive witness export"
Assert-Condition `
    -Condition ($null -ne $supportingSurface) `
    -Message "expected front page supporting surface for open_event_witness"
Assert-Condition `
    -Condition ([string]$supportingSurface.summary_path -eq (Resolve-FullPath -Path $openEventWitnessSummaryPath)) `
    -Message "front page open_event_witness surface should point to the witness summary"
Assert-Condition `
    -Condition ([string]$supportingSurface.report_markdown_path -eq [string]$sourceWitness.artifact_context.report_markdown_path) `
    -Message "front page open_event_witness surface should point to witness report"
Assert-Condition `
    -Condition ([string]$supportingSurface.check_text_path -eq [string]$sourceWitness.artifact_context.check_text_path) `
    -Message "front page open_event_witness surface should point to witness check"
Assert-Condition `
    -Condition ([int]$bundleSummary.witness_summary.kind_counts.open_event_witness -eq 1) `
    -Message "expected witness summary kind count open_event_witness=1"
Assert-Condition `
    -Condition (@($entry.artifact_refs | Where-Object { [string]$_ -eq (Resolve-FullPath -Path $openEventWitnessSummaryPath) }).Count -eq 1) `
    -Message "expected open_event_witness summary path in artifact refs"

Write-Host "==> system compiler witness open-event witness export smoke"
Write-Host ("open_event_witness={0}" -f $openEventWitnessSummaryPath)
Write-Host ("bundle_summary={0}" -f $bundleSummaryPath)
Write-Host ("bundle_surface={0}" -f [string]$supportingSurface.id)
Write-Host ("bundle_entry={0}" -f [string]$entry.id)
Write-Host "ok=1"
