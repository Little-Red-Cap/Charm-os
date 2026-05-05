param(
    [string]$RuntimeSessionRouteRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-landing-route-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-explain-entry-smoke",
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

function New-MissingSelectedSurfaceRouteFixture {
    param(
        [string]$SourceRoutePath,
        [string]$OutputPath
    )

    $summary = Load-JsonObject -Path $SourceRoutePath
    $summary.route_entries = @(
        @($summary.route_entries) |
            Where-Object { [string]$_.surface_id -ne "source_open_event_witness" }
    )
    $summary.artifact_context.route_summary_path = (Resolve-FullPath -Path $OutputPath)
    Write-JsonFile -Path $OutputPath -Value $summary
    return (Resolve-FullPath -Path $OutputPath)
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$runtimeSessionRouteRootPath = Resolve-FullPath -Path $RuntimeSessionRouteRoot
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

$routeSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_opening_testimony_landing_route_smoke.ps1"
$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_testimony_explain_entry.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_testimony_explain_entry.py"
foreach ($requiredPath in @($routeSmokeScript, $exportScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $cleanRoutePath = Join-Path $runtimeSessionRouteRootPath "clean-landing-route\front-page.route.summary.json"
    $driftRoutePath = Join-Path $runtimeSessionRouteRootPath "drift-landing-route\front-page.route.summary.json"
    $compareRoutePath = Join-Path $runtimeSessionRouteRootPath "landing-compare-route\front-page.route.summary.json"
    if (
        (-not $Clean) -and
        (Test-Path -LiteralPath $cleanRoutePath) -and
        (Test-Path -LiteralPath $driftRoutePath) -and
        (Test-Path -LiteralPath $compareRoutePath)
    ) {
        Write-Host "[RUNTIME-SESSION-OPENING-TESTIMONY-EXPLAIN-ENTRY-SMOKE] route_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $routeSmokeScript,
                "-OutputRoot",
                $runtimeSessionRouteRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "runtime session opening testimony landing route smoke bootstrap failed"
    }

    $blockedRoutePath = New-MissingSelectedSurfaceRouteFixture `
        -SourceRoutePath $cleanRoutePath `
        -OutputPath (Join-Path $outputRootPath "_blocked-fixtures\missing-selected-surface\front-page.route.summary.json")

    $cases = @(
        [ordered]@{
            Name = "clean-route-explain-entry"
            SourceSummary = $cleanRoutePath
            ExpectedStatus = "ready"
            ExpectedResult = "ok"
            ExpectedSelectionKind = "route_landing_default"
            ExpectedSelectedSurface = "source_open_event_witness"
            ExpectedViolation = ""
        },
        [ordered]@{
            Name = "drift-route-explain-entry"
            SourceSummary = $driftRoutePath
            ExpectedStatus = "ready"
            ExpectedResult = "ok"
            ExpectedSelectionKind = "route_landing_default"
            ExpectedSelectedSurface = "source_open_event_witness"
            ExpectedViolation = ""
        },
        [ordered]@{
            Name = "landing-compare-route-explain-entry"
            SourceSummary = $compareRoutePath
            ExpectedStatus = "ready"
            ExpectedResult = "ok"
            ExpectedSelectionKind = "route_landing_compare_default"
            ExpectedSelectedSurface = "candidate_opening_testimony_landing"
            ExpectedViolation = ""
        },
        [ordered]@{
            Name = "missing-selected-surface-explain-entry"
            SourceSummary = $blockedRoutePath
            ExpectedStatus = "blocked"
            ExpectedResult = "fail"
            ExpectedSelectionKind = "route_landing_default"
            ExpectedSelectedSurface = ""
            ExpectedViolation = "selected explain surface is missing"
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
            -FailureMessage ("runtime session opening testimony explain-entry export failed for case '{0}'" -f $case.Name)

        $summaryPath = Join-Path $caseOutputRoot "front-page.entry-opening-testimony.explain-entry.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($validateScript, "--summary", $summaryPath) `
            -FailureMessage ("runtime session opening testimony explain-entry validation failed for case '{0}'" -f $case.Name)

        $summary = Load-JsonObject -Path $summaryPath
        Assert-Condition `
            -Condition ([string]$summary.result -eq [string]$case.ExpectedResult) `
            -Message ("case '{0}' expected result '{1}' but got '{2}'" -f $case.Name, $case.ExpectedResult, $summary.result)
        Assert-Condition `
            -Condition ([string]$summary.explain_entry_decision.status -eq [string]$case.ExpectedStatus) `
            -Message ("case '{0}' expected status '{1}' but got '{2}'" -f $case.Name, $case.ExpectedStatus, $summary.explain_entry_decision.status)
        Assert-Condition `
            -Condition ([string]$summary.explain_entry_decision.selection_kind -eq [string]$case.ExpectedSelectionKind) `
            -Message ("case '{0}' expected selection kind '{1}' but got '{2}'" -f $case.Name, $case.ExpectedSelectionKind, $summary.explain_entry_decision.selection_kind)
        Assert-Condition `
            -Condition ([string]$summary.selected_surface.surface_id -eq [string]$case.ExpectedSelectedSurface) `
            -Message ("case '{0}' expected selected surface '{1}' but got '{2}'" -f $case.Name, $case.ExpectedSelectedSurface, $summary.selected_surface.surface_id)
        Assert-Condition `
            -Condition ([string]$summary.explain_entry_decision.selected_tab_id -eq "opening_testimony_explain") `
            -Message ("case '{0}' expected selected tab opening_testimony_explain" -f $case.Name)
        Assert-Condition `
            -Condition ([string]$summary.explain_entry_decision.selected_role -eq "opening_testimony_explain_entry") `
            -Message ("case '{0}' expected selected role opening_testimony_explain_entry" -f $case.Name)
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
                -Message ("case '{0}' should not contain forbidden raw evidence field '{1}'" -f $case.Name, $forbiddenText)
        }

        Write-Host (
            "[RUNTIME-SESSION-OPENING-TESTIMONY-EXPLAIN-ENTRY-SMOKE] case={0} status={1} selection={2} selected={3}" -f
            $case.Name,
            [string]$summary.explain_entry_decision.status,
            [string]$summary.explain_entry_decision.selection_kind,
            [string]$summary.selected_surface.surface_id
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[RUNTIME-SESSION-OPENING-TESTIMONY-EXPLAIN-ENTRY-SMOKE] output_root={0}" -f $outputRootPath)
