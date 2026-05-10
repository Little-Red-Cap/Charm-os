param(
    [string]$OpeningTestimonyExplainEntryRoot = "cmake-build-system-compiler-front-page-entry-opening-testimony-explain-entry-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-testimony-explain-entry-handoff-smoke",
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
$openingTestimonyExplainEntryRootPath = Resolve-FullPath -Path $OpeningTestimonyExplainEntryRoot
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

$explainEntrySmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_testimony_explain_entry_smoke.ps1"
$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_testimony_explain_entry_handoff.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_testimony_explain_entry_handoff.py"
foreach ($requiredPath in @($explainEntrySmokeScript, $exportScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $cleanExplainEntryPath = Join-Path $openingTestimonyExplainEntryRootPath "clean-route-explain-entry\front-page.entry-opening-testimony.explain-entry.summary.json"
    $driftExplainEntryPath = Join-Path $openingTestimonyExplainEntryRootPath "drift-route-explain-entry\front-page.entry-opening-testimony.explain-entry.summary.json"
    $landingCompareExplainEntryPath = Join-Path $openingTestimonyExplainEntryRootPath "landing-compare-route-explain-entry\front-page.entry-opening-testimony.explain-entry.summary.json"
    $blockedExplainEntryPath = Join-Path $openingTestimonyExplainEntryRootPath "missing-selected-surface-explain-entry\front-page.entry-opening-testimony.explain-entry.summary.json"
    if (
        (-not $Clean) -and
        (Test-Path -LiteralPath $cleanExplainEntryPath) -and
        (Test-Path -LiteralPath $driftExplainEntryPath) -and
        (Test-Path -LiteralPath $landingCompareExplainEntryPath) -and
        (Test-Path -LiteralPath $blockedExplainEntryPath)
    ) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-SMOKE] explain_entry_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $explainEntrySmokeScript,
                "-OutputRoot",
                $openingTestimonyExplainEntryRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "opening testimony explain-entry smoke bootstrap failed"
    }

    $cases = @(
        [ordered]@{
            Name = "clean-route-handoff"
            SourceSummary = $cleanExplainEntryPath
            ExpectedStatus = "ready"
            ExpectedResult = "ok"
            ExpectedOpenTarget = "source_open_event_witness"
            ExpectedViolation = ""
        },
        [ordered]@{
            Name = "drift-route-handoff"
            SourceSummary = $driftExplainEntryPath
            ExpectedStatus = "ready"
            ExpectedResult = "ok"
            ExpectedOpenTarget = "source_open_event_witness"
            ExpectedViolation = ""
        },
        [ordered]@{
            Name = "landing-compare-route-handoff"
            SourceSummary = $landingCompareExplainEntryPath
            ExpectedStatus = "ready"
            ExpectedResult = "ok"
            ExpectedOpenTarget = "candidate_opening_testimony_landing"
            ExpectedViolation = ""
        },
        [ordered]@{
            Name = "missing-selected-surface-handoff"
            SourceSummary = $blockedExplainEntryPath
            ExpectedStatus = "blocked"
            ExpectedResult = "fail"
            ExpectedOpenTarget = ""
            ExpectedViolation = "source explain_entry_decision.status is not ready"
        }
    )

    foreach ($case in $cases) {
        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @(
                $exportScript,
                "--source-summary",
                [string]$case.SourceSummary,
                "--output-root",
                $caseOutputRoot
            ) `
            -FailureMessage ("opening testimony explain-entry handoff export failed for case '{0}'" -f $case.Name)

        $summaryPath = Join-Path $caseOutputRoot "front-page.entry-opening-testimony.explain-entry.handoff.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($validateScript, "--summary", $summaryPath) `
            -FailureMessage ("opening testimony explain-entry handoff validation failed for case '{0}'" -f $case.Name)

        $summary = Load-JsonObject -Path $summaryPath
        Assert-Condition `
            -Condition ([string]$summary.result -eq [string]$case.ExpectedResult) `
            -Message ("case '{0}' expected result '{1}' but got '{2}'" -f $case.Name, $case.ExpectedResult, $summary.result)
        Assert-Condition `
            -Condition ([string]$summary.handoff_decision.status -eq [string]$case.ExpectedStatus) `
            -Message ("case '{0}' expected status '{1}' but got '{2}'" -f $case.Name, $case.ExpectedStatus, $summary.handoff_decision.status)
        Assert-Condition `
            -Condition ([string]$summary.open_target.surface_id -eq [string]$case.ExpectedOpenTarget) `
            -Message ("case '{0}' expected open target '{1}' but got '{2}'" -f $case.Name, $case.ExpectedOpenTarget, $summary.open_target.surface_id)
        Assert-Condition `
            -Condition ([string]$summary.handoff_action.expected_consumer_operation -eq "open-selected-summary") `
            -Message ("case '{0}' expected operation open-selected-summary" -f $case.Name)
        if (-not [string]::IsNullOrWhiteSpace([string]$case.ExpectedViolation)) {
            Assert-Condition `
                -Condition (@($summary.violations) -contains [string]$case.ExpectedViolation) `
                -Message ("case '{0}' expected violation '{1}'" -f $case.Name, $case.ExpectedViolation)
        } else {
            Assert-Condition `
                -Condition (@($summary.violations).Count -eq 0) `
                -Message ("case '{0}' expected no violations" -f $case.Name)
        }

        Write-Host (
            "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-SMOKE] case={0} status={1} target={2}" -f
            $case.Name,
            [string]$summary.handoff_decision.status,
            [string]$summary.open_target.surface_id
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-SMOKE] output_root={0}" -f $outputRootPath)
