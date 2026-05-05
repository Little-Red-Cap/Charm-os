param(
    [string]$OpenerWorkspaceRoot = "cmake-build-system-compiler-front-page-entry-opener-workspace-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opener-workspace-compare-smoke",
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
$openerWorkspaceRootPath = Resolve-FullPath -Path $OpenerWorkspaceRoot
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

$openerWorkspaceSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opener_workspace_smoke.ps1"
$workspaceCompareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opener_workspace.ps1"
foreach ($requiredPath in @($openerWorkspaceSmokeScript, $workspaceCompareScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $coldOpenerWorkspaceRoot = Join-Path $openerWorkspaceRootPath "cold-runtime-evidence"
    $hotOpenerWorkspaceRoot = Join-Path $openerWorkspaceRootPath "hot-runtime-evidence-with-landing-compare"
    $coldOpenerSummaryPath = Join-Path $coldOpenerWorkspaceRoot "opener\front-page.entry-opener.summary.json"
    $hotOpenerSummaryPath = Join-Path $hotOpenerWorkspaceRoot "opener\front-page.entry-opener.summary.json"

    if ($Clean -or -not ((Test-Path -LiteralPath $coldOpenerSummaryPath) -and (Test-Path -LiteralPath $hotOpenerSummaryPath))) {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $openerWorkspaceSmokeScript,
                "-OutputRoot",
                $openerWorkspaceRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "front page entry opener workspace smoke bootstrap failed"
    } else {
        Write-Host "[FRONT-PAGE-ENTRY-OPENER-WORKSPACE-COMPARE-SMOKE] opener_workspace_bootstrap=reuse-existing"
    }

    foreach ($requiredSummaryPath in @($coldOpenerSummaryPath, $hotOpenerSummaryPath)) {
        if (-not (Test-Path -LiteralPath $requiredSummaryPath)) {
            throw "missing opener summary fixture: $requiredSummaryPath"
        }
    }

    $cases = @(
        [ordered]@{
            Name = "workspace-self-standing"
            BaselineOpenerWorkspaceRoot = $coldOpenerWorkspaceRoot
            CandidateOpenerWorkspaceRoot = $coldOpenerWorkspaceRoot
            ExpectedVerdict = "standing"
            ExpectedChangedFields = 0
            ExpectedCompareChanged = $false
            ExpectedProjectionChanged = $false
            ExpectedImprovement = $false
            ExpectedRegression = $false
        },
        [ordered]@{
            Name = "workspace-cold-to-hot-compare-context"
            BaselineOpenerWorkspaceRoot = $coldOpenerWorkspaceRoot
            CandidateOpenerWorkspaceRoot = $hotOpenerWorkspaceRoot
            ExpectedVerdict = "improved"
            ExpectedChangedFields = -1
            ExpectedCompareChanged = $true
            ExpectedProjectionChanged = $true
            ExpectedImprovement = $true
            ExpectedRegression = $false
        },
        [ordered]@{
            Name = "workspace-hot-to-cold-lost-compare-context"
            BaselineOpenerWorkspaceRoot = $hotOpenerWorkspaceRoot
            CandidateOpenerWorkspaceRoot = $coldOpenerWorkspaceRoot
            ExpectedVerdict = "drifted"
            ExpectedChangedFields = -1
            ExpectedCompareChanged = $true
            ExpectedProjectionChanged = $true
            ExpectedImprovement = $false
            ExpectedRegression = $true
        }
    )

    foreach ($case in $cases) {
        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $workspaceCompareScript,
                "-BaselineOpenerWorkspaceRoot",
                [string]$case.BaselineOpenerWorkspaceRoot,
                "-CandidateOpenerWorkspaceRoot",
                [string]$case.CandidateOpenerWorkspaceRoot,
                "-OutputRoot",
                $caseOutputRoot,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage ("front page entry opener workspace compare failed for case '{0}'" -f $case.Name)

        $summaryPath = Join-Path $caseOutputRoot "compare\front-page.entry-opener.compare.summary.json"
        $summary = Load-JsonObject -Path $summaryPath
        Assert-Condition `
            -Condition ([string]$summary.opener_verdict -eq [string]$case.ExpectedVerdict) `
            -Message ("case '{0}' expected verdict '{1}' but got '{2}'" -f $case.Name, $case.ExpectedVerdict, $summary.opener_verdict)
        if ([int]$case.ExpectedChangedFields -ge 0) {
            Assert-Condition `
                -Condition ([int]$summary.change_summary.changed_field_count -eq [int]$case.ExpectedChangedFields) `
                -Message ("case '{0}' expected changed fields '{1}' but got '{2}'" -f $case.Name, $case.ExpectedChangedFields, $summary.change_summary.changed_field_count)
        } else {
            Assert-Condition `
                -Condition ([int]$summary.change_summary.changed_field_count -gt 0) `
                -Message ("case '{0}' expected positive changed field count" -f $case.Name)
        }
        Assert-Condition `
            -Condition ([bool]$summary.opener_changes.compare_context_changed -eq [bool]$case.ExpectedCompareChanged) `
            -Message ("case '{0}' compare context changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$summary.opener_changes.projection_changed -eq [bool]$case.ExpectedProjectionChanged) `
            -Message ("case '{0}' projection changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$summary.opener_improvement_surface.changed -eq [bool]$case.ExpectedImprovement) `
            -Message ("case '{0}' improvement surface expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$summary.opener_regression_surface.changed -eq [bool]$case.ExpectedRegression) `
            -Message ("case '{0}' regression surface expectation mismatch" -f $case.Name)

        Write-Host (
            "[FRONT-PAGE-ENTRY-OPENER-WORKSPACE-COMPARE-SMOKE] case={0} verdict={1} changed={2} compare_changed={3} projection_changed={4} improved={5} regressed={6}" -f
            $case.Name,
            [string]$summary.opener_verdict,
            [int]$summary.change_summary.changed_field_count,
            [bool]$summary.opener_changes.compare_context_changed,
            [bool]$summary.opener_changes.projection_changed,
            [bool]$summary.opener_improvement_surface.changed,
            [bool]$summary.opener_regression_surface.changed
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENER-WORKSPACE-COMPARE-SMOKE] output_root={0}" -f $outputRootPath)
