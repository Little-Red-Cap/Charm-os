param(
    [string]$OpenEventWitnessRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-open-event-witness-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-open-event-witness-compare-smoke",
    [string]$PythonExe = "",
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

function Assert-Condition {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$openEventWitnessRootPath = Resolve-FullPath -Path $OpenEventWitnessRoot
$outputRootPath = Resolve-FullPath -Path $OutputRoot

if ($Clean) {
    Remove-PathIfExists -Path $outputRootPath
}
Ensure-Directory -Path $outputRootPath

$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}
$powerShellExe = Resolve-ToolPath -Candidates @("powershell.exe", "pwsh.exe", "powershell", "pwsh")

$openEventWitnessSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_flow_open_event_witness_smoke.ps1"
$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_flow_open_event_witness.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_open_event_witness_compare.py"
foreach ($requiredPath in @($openEventWitnessSmokeScript, $compareScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $baselineWitnessPath = Join-Path $openEventWitnessRootPath "default-no-compare-witness\front-page.entry-opening-flow.open-event.witness.summary.json"
    $candidateWitnessPath = Join-Path $openEventWitnessRootPath "default-with-drift-compare-witness\front-page.entry-opening-flow.open-event.witness.summary.json"
    if ((-not $Clean) -and (Test-Path -LiteralPath $baselineWitnessPath) -and (Test-Path -LiteralPath $candidateWitnessPath)) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-COMPARE-SMOKE] witness_bootstrap=reuse-existing"
    } else {
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
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "front page entry opening-flow open-event witness smoke bootstrap failed"
    }

    $cases = @(
        [ordered]@{
            Name = "open-event-witness-self-standing"
            Baseline = $baselineWitnessPath
            Candidate = $baselineWitnessPath
            ExpectedVerdict = "standing"
            ExpectedChangedFields = 0
            ExpectedEventIdentityChanged = $false
            ExpectedCompareContextChanged = $false
            ExpectedEvidenceChanged = $false
            ExpectedExplanationChanged = $false
        },
        [ordered]@{
            Name = "open-event-witness-default-to-drift-context"
            Baseline = $baselineWitnessPath
            Candidate = $candidateWitnessPath
            ExpectedVerdict = "drifted"
            ExpectedEventIdentityChanged = $true
            ExpectedCompareContextChanged = $true
            ExpectedEvidenceChanged = $true
            ExpectedExplanationChanged = $true
        }
    )

    foreach ($case in $cases) {
        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @(
                $compareScript,
                "--baseline",
                $case.Baseline,
                "--candidate",
                $case.Candidate,
                "--output-root",
                $caseOutputRoot
            ) `
            -FailureMessage ("front page entry opening-flow open-event witness compare failed for case '{0}'" -f $case.Name)

        $compareSummaryPath = Join-Path $caseOutputRoot "front-page.entry-opening-flow.open-event.witness.compare.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($validateScript, "--summary", $compareSummaryPath) `
            -FailureMessage ("front page entry opening-flow open-event witness compare validation failed for case '{0}'" -f $case.Name)

        $summary = Load-JsonObject -Path $compareSummaryPath
        Assert-Condition `
            -Condition ([string]$summary.witness_verdict -eq [string]$case.ExpectedVerdict) `
            -Message ("case '{0}' expected verdict '{1}' but got '{2}'" -f $case.Name, $case.ExpectedVerdict, $summary.witness_verdict)
        if ($case.Contains("ExpectedChangedFields")) {
            Assert-Condition `
                -Condition ([int]$summary.change_summary.changed_field_count -eq [int]$case.ExpectedChangedFields) `
                -Message ("case '{0}' expected changed fields '{1}' but got '{2}'" -f $case.Name, $case.ExpectedChangedFields, $summary.change_summary.changed_field_count)
        } else {
            Assert-Condition `
                -Condition ([int]$summary.change_summary.changed_field_count -gt 0) `
                -Message ("case '{0}' expected positive changed field count" -f $case.Name)
        }
        Assert-Condition `
            -Condition ([bool]$summary.witness_regression_surface.event_identity_changed -eq [bool]$case.ExpectedEventIdentityChanged) `
            -Message ("case '{0}' event identity changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$summary.witness_regression_surface.compare_context_changed -eq [bool]$case.ExpectedCompareContextChanged) `
            -Message ("case '{0}' compare context changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$summary.witness_regression_surface.evidence_refs_changed -eq [bool]$case.ExpectedEvidenceChanged) `
            -Message ("case '{0}' evidence refs changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$summary.witness_regression_surface.explanation_changed -eq [bool]$case.ExpectedExplanationChanged) `
            -Message ("case '{0}' explanation changed expectation mismatch" -f $case.Name)

        Write-Host (
            "[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-COMPARE-SMOKE] case={0} verdict={1} changed={2} identity_changed={3} compare_changed={4} evidence_changed={5} explanation_changed={6}" -f
            $case.Name,
            [string]$summary.witness_verdict,
            [int]$summary.change_summary.changed_field_count,
            [bool]$summary.witness_regression_surface.event_identity_changed,
            [bool]$summary.witness_regression_surface.compare_context_changed,
            [bool]$summary.witness_regression_surface.evidence_refs_changed,
            [bool]$summary.witness_regression_surface.explanation_changed
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-COMPARE-SMOKE] output_root={0}" -f $outputRootPath)
