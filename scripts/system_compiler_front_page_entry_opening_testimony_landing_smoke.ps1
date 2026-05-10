param(
    [string]$OpenEventWitnessRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-open-event-witness-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-testimony-landing-smoke",
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

function Write-JsonFile {
    param(
        [string]$Path,
        $Value
    )

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        Ensure-Directory -Path $parent
    }
    $json = $Value | ConvertTo-Json -Depth 100
    Set-Content -LiteralPath $Path -Encoding utf8 -Value ($json + [Environment]::NewLine)
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

function New-BlockedWitnessFixture {
    param(
        [string]$SourceWitnessPath,
        [string]$OutputPath
    )

    $summary = Load-JsonObject -Path $SourceWitnessPath
    $summary.explanation.text_lines = @()
    $summary.artifact_context.open_event_witness_summary_path = (Resolve-FullPath -Path $OutputPath)
    $summary.front_page.summary_path = (Resolve-FullPath -Path $OutputPath)
    Write-JsonFile -Path $OutputPath -Value $summary
    return (Resolve-FullPath -Path $OutputPath)
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
$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_testimony_landing.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_testimony_landing.py"
foreach ($requiredPath in @($openEventWitnessSmokeScript, $exportScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $cleanWitnessPath = Join-Path $openEventWitnessRootPath "default-no-compare-witness\front-page.entry-opening-flow.open-event.witness.summary.json"
    $driftWitnessPath = Join-Path $openEventWitnessRootPath "default-with-drift-compare-witness\front-page.entry-opening-flow.open-event.witness.summary.json"
    if ((-not $Clean) -and (Test-Path -LiteralPath $cleanWitnessPath) -and (Test-Path -LiteralPath $driftWitnessPath)) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-LANDING-SMOKE] witness_bootstrap=reuse-existing"
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

    $blockedFixturePath = New-BlockedWitnessFixture `
        -SourceWitnessPath $cleanWitnessPath `
        -OutputPath (Join-Path $outputRootPath "_blocked-fixtures\empty-explanation\front-page.entry-opening-flow.open-event.witness.summary.json")

    $cases = @(
        [ordered]@{
            Name = "clean-witness-landing"
            Witness = $cleanWitnessPath
            ExpectedLandingStatus = "ready"
            ExpectedResult = "ok"
            ExpectedSourceJudgmentStatus = "accepted"
            ExpectedViolation = ""
        },
        [ordered]@{
            Name = "drift-witness-landing"
            Witness = $driftWitnessPath
            ExpectedLandingStatus = "ready"
            ExpectedResult = "ok"
            ExpectedSourceJudgmentStatus = "accepted_with_drift"
            ExpectedViolation = ""
        },
        [ordered]@{
            Name = "blocked-empty-explanation-landing"
            Witness = $blockedFixturePath
            ExpectedLandingStatus = "blocked"
            ExpectedResult = "fail"
            ExpectedSourceJudgmentStatus = "accepted"
            ExpectedViolation = "explanation.text_lines is empty"
        }
    )

    foreach ($case in $cases) {
        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @(
                $exportScript,
                "--witness",
                [string]$case.Witness,
                "--output-root",
                $caseOutputRoot
            ) `
            -FailureMessage ("front page entry opening testimony landing export failed for case '{0}'" -f $case.Name)

        $landingSummaryPath = Join-Path $caseOutputRoot "front-page.entry-opening-testimony.landing.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($validateScript, "--summary", $landingSummaryPath) `
            -FailureMessage ("front page entry opening testimony landing validation failed for case '{0}'" -f $case.Name)

        $summary = Load-JsonObject -Path $landingSummaryPath
        Assert-Condition `
            -Condition ([string]$summary.result -eq [string]$case.ExpectedResult) `
            -Message ("case '{0}' expected result '{1}' but got '{2}'" -f $case.Name, $case.ExpectedResult, $summary.result)
        Assert-Condition `
            -Condition ([string]$summary.landing_decision.status -eq [string]$case.ExpectedLandingStatus) `
            -Message ("case '{0}' expected landing status '{1}' but got '{2}'" -f $case.Name, $case.ExpectedLandingStatus, $summary.landing_decision.status)
        Assert-Condition `
            -Condition ([string]$summary.opening_identity.source_judgment_status -eq [string]$case.ExpectedSourceJudgmentStatus) `
            -Message ("case '{0}' expected source judgment status '{1}' but got '{2}'" -f $case.Name, $case.ExpectedSourceJudgmentStatus, $summary.opening_identity.source_judgment_status)
        Assert-Condition `
            -Condition ([string]$summary.landing_decision.selected_entry_id -eq "open-event-witness") `
            -Message ("case '{0}' expected selected entry open-event-witness" -f $case.Name)
        Assert-Condition `
            -Condition ([string]$summary.landing_decision.selected_tab_id -eq "opening_testimony") `
            -Message ("case '{0}' expected selected tab opening_testimony" -f $case.Name)
        Assert-Condition `
            -Condition ([string]$summary.landing_decision.selected_role -eq "opening_testimony") `
            -Message ("case '{0}' expected selected role opening_testimony" -f $case.Name)
        Assert-Condition `
            -Condition (@($summary.next_questions).Count -eq 3) `
            -Message ("case '{0}' expected three typed next questions" -f $case.Name)
        Assert-Condition `
            -Condition ([string]$summary.next_questions[0].kind -eq "inspect_open_event") `
            -Message ("case '{0}' expected first next question inspect_open_event" -f $case.Name)
        Assert-Condition `
            -Condition ([string]$summary.next_questions[1].kind -eq "inspect_evidence_refs") `
            -Message ("case '{0}' expected second next question inspect_evidence_refs" -f $case.Name)
        Assert-Condition `
            -Condition ([string]$summary.next_questions[2].kind -eq "compare_open_event_witness") `
            -Message ("case '{0}' expected third next question compare_open_event_witness" -f $case.Name)
        if (-not [string]::IsNullOrWhiteSpace([string]$case.ExpectedViolation)) {
            Assert-Condition `
                -Condition (@($summary.violations) -contains [string]$case.ExpectedViolation) `
                -Message ("case '{0}' expected violation '{1}'" -f $case.Name, $case.ExpectedViolation)
        } else {
            Assert-Condition `
                -Condition (@($summary.violations).Count -eq 0) `
                -Message ("case '{0}' expected no violations" -f $case.Name)
        }

        $serialized = $summary | ConvertTo-Json -Depth 100 -Compress
        foreach ($forbiddenText in @("runtime_session_summary", "world_compare_summary", "session_witness_inspect_compare_consumer")) {
            Assert-Condition `
                -Condition (-not $serialized.Contains($forbiddenText)) `
                -Message ("case '{0}' unexpectedly exposed forbidden raw field '{1}'" -f $case.Name, $forbiddenText)
        }

        Write-Host (
            "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-LANDING-SMOKE] case={0} status={1} source_judgment={2} witness_status={3} evidence_refs={4} artifact_refs={5}" -f
            $case.Name,
            [string]$summary.landing_decision.status,
            [string]$summary.opening_identity.source_judgment_status,
            [string]$summary.opening_identity.source_witness_status,
            [int]$summary.artifact_targets.evidence_ref_count,
            [int]$summary.artifact_targets.witness_artifact_ref_count
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-LANDING-SMOKE] output_root={0}" -f $outputRootPath)
