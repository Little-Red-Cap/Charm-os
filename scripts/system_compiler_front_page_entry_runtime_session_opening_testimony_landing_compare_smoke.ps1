param(
    [string]$RuntimeSessionLandingRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-landing-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-landing-compare-smoke",
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
$runtimeSessionLandingRootPath = Resolve-FullPath -Path $RuntimeSessionLandingRoot
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

$landingSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_opening_testimony_landing_smoke.ps1"
$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_testimony_landing.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_testimony_landing_compare.py"
foreach ($requiredPath in @($landingSmokeScript, $compareScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $cleanLandingPath = Join-Path $runtimeSessionLandingRootPath "clean-witness-landing\front-page.entry-opening-testimony.landing.summary.json"
    $driftLandingPath = Join-Path $runtimeSessionLandingRootPath "drift-witness-landing\front-page.entry-opening-testimony.landing.summary.json"
    $blockedLandingPath = Join-Path $runtimeSessionLandingRootPath "blocked-empty-explanation-landing\front-page.entry-opening-testimony.landing.summary.json"
    if (
        (-not $Clean) -and
        (Test-Path -LiteralPath $cleanLandingPath) -and
        (Test-Path -LiteralPath $driftLandingPath) -and
        (Test-Path -LiteralPath $blockedLandingPath)
    ) {
        Write-Host "[RUNTIME-SESSION-OPENING-TESTIMONY-LANDING-COMPARE-SMOKE] landing_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $landingSmokeScript,
                "-OutputRoot",
                $runtimeSessionLandingRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "runtime session opening testimony landing smoke bootstrap failed"
    }

    $cases = @(
        [ordered]@{
            Name = "opening-testimony-landing-self-standing"
            Baseline = $cleanLandingPath
            Candidate = $cleanLandingPath
            ExpectedVerdict = "standing"
            ExpectedChangedFields = 0
            ExpectedSourceJudgmentChanged = $false
            ExpectedPreviewChanged = $false
            ExpectedEvidenceChanged = $false
        },
        [ordered]@{
            Name = "opening-testimony-landing-clean-to-drift"
            Baseline = $cleanLandingPath
            Candidate = $driftLandingPath
            ExpectedVerdict = "drifted"
            ExpectedChangedFields = $null
            ExpectedSourceJudgmentChanged = $true
            ExpectedPreviewChanged = $true
            ExpectedEvidenceChanged = $true
        },
        [ordered]@{
            Name = "opening-testimony-landing-clean-to-blocked"
            Baseline = $cleanLandingPath
            Candidate = $blockedLandingPath
            ExpectedVerdict = "collapsed"
            ExpectedChangedFields = $null
            ExpectedSourceJudgmentChanged = $false
            ExpectedPreviewChanged = $true
            ExpectedEvidenceChanged = $false
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
            -FailureMessage ("runtime session opening testimony landing compare export failed for case '{0}'" -f $case.Name)

        $compareSummaryPath = Join-Path $caseOutputRoot "front-page.entry-opening-testimony.landing.compare.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($validateScript, "--summary", $compareSummaryPath) `
            -FailureMessage ("runtime session opening testimony landing compare validation failed for case '{0}'" -f $case.Name)

        $summary = Load-JsonObject -Path $compareSummaryPath
        Assert-Condition `
            -Condition ([string]$summary.landing_verdict -eq [string]$case.ExpectedVerdict) `
            -Message ("case '{0}' expected verdict '{1}' but got '{2}'" -f $case.Name, $case.ExpectedVerdict, $summary.landing_verdict)
        if ($null -ne $case.ExpectedChangedFields) {
            Assert-Condition `
                -Condition ([int]$summary.change_summary.changed_field_count -eq [int]$case.ExpectedChangedFields) `
                -Message ("case '{0}' expected changed fields '{1}' but got '{2}'" -f $case.Name, $case.ExpectedChangedFields, $summary.change_summary.changed_field_count)
        } else {
            Assert-Condition `
                -Condition ([int]$summary.change_summary.changed_field_count -gt 0) `
                -Message ("case '{0}' expected positive changed field count" -f $case.Name)
        }
        Assert-Condition `
            -Condition ([bool]$summary.landing_regression_surface.source_judgment_changed -eq [bool]$case.ExpectedSourceJudgmentChanged) `
            -Message ("case '{0}' source judgment changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$summary.landing_regression_surface.preview_changed -eq [bool]$case.ExpectedPreviewChanged) `
            -Message ("case '{0}' preview changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$summary.landing_regression_surface.evidence_refs_changed -eq [bool]$case.ExpectedEvidenceChanged) `
            -Message ("case '{0}' evidence refs changed expectation mismatch" -f $case.Name)

        $serialized = $summary | ConvertTo-Json -Depth 100 -Compress
        foreach ($forbiddenText in @("runtime_session_summary", "world_compare_summary", "session_witness_inspect_compare_consumer")) {
            Assert-Condition `
                -Condition (-not $serialized.Contains($forbiddenText)) `
                -Message ("case '{0}' unexpectedly exposed forbidden raw field '{1}'" -f $case.Name, $forbiddenText)
        }

        Write-Host (
            "[RUNTIME-SESSION-OPENING-TESTIMONY-LANDING-COMPARE-SMOKE] case={0} verdict={1} changed={2} judgment_changed={3} preview_changed={4} evidence_changed={5}" -f
            $case.Name,
            [string]$summary.landing_verdict,
            [int]$summary.change_summary.changed_field_count,
            [bool]$summary.landing_regression_surface.source_judgment_changed,
            [bool]$summary.landing_regression_surface.preview_changed,
            [bool]$summary.landing_regression_surface.evidence_refs_changed
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[RUNTIME-SESSION-OPENING-TESTIMONY-LANDING-COMPARE-SMOKE] output_root={0}" -f $outputRootPath)
