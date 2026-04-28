param(
    [string]$InputRoot = "cmake-build-system-compiler-front-page-route-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-route-compare-smoke",
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

    if (-not (Test-Path $Path)) {
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

    if (Test-Path $Path) {
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
$inputRootPath = Resolve-FullPath -Path $InputRoot
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

$routeSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_route_smoke.ps1"
$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_route.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_route_compare.py"
foreach ($requiredPath in @($routeSmokeScript, $compareScript, $validateScript)) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    Invoke-ExternalTool `
        -Executable "powershell.exe" `
        -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $routeSmokeScript,
            "-OutputRoot",
            $inputRootPath
        ) `
        -FailureMessage "front page route smoke bootstrap failed"

    $cases = @(
        [ordered]@{
            Name = "root-witness-to-root-world-compare"
            Baseline = Join-Path $inputRootPath "root-witness\front-page.route.summary.json"
            Candidate = Join-Path $inputRootPath "root-world-compare\front-page.route.summary.json"
            ExpectedVerdict = "improved"
            AddedLevel1 = @("world_compare")
            RemovedLevel1 = @()
        },
        [ordered]@{
            Name = "root-witness-to-witness-ci-shelf"
            Baseline = Join-Path $inputRootPath "root-witness\front-page.route.summary.json"
            Candidate = Join-Path $inputRootPath "witness-ci-shelf\front-page.route.summary.json"
            ExpectedVerdict = "drifted"
            AddedLevel1 = @("world_shelf_review")
            RemovedLevel1 = @()
        },
        [ordered]@{
            Name = "root-world-compare-to-root-witness"
            Baseline = Join-Path $inputRootPath "root-world-compare\front-page.route.summary.json"
            Candidate = Join-Path $inputRootPath "root-witness\front-page.route.summary.json"
            ExpectedVerdict = "drifted"
            AddedLevel1 = @()
            RemovedLevel1 = @("world_compare")
        }
    )

    foreach ($case in $cases) {
        foreach ($requiredSummary in @($case.Baseline, $case.Candidate)) {
            if (-not (Test-Path $requiredSummary)) {
                throw "route summary not found for case '$($case.Name)': $requiredSummary"
            }
        }

        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($compareScript, "--baseline", $case.Baseline, "--candidate", $case.Candidate, "--output-root", $caseOutputRoot) `
            -FailureMessage ("front page route compare export failed for case '{0}'" -f $case.Name)

        $compareSummaryPath = Join-Path $caseOutputRoot "front-page.route.compare.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($validateScript, "--summary", $compareSummaryPath) `
            -FailureMessage ("front page route compare validation failed for case '{0}'" -f $case.Name)

        $compareSummary = Load-JsonObject -Path $compareSummaryPath
        Assert-Condition `
            -Condition ([string]$compareSummary.route_verdict -eq $case.ExpectedVerdict) `
            -Message ("case '{0}' expected verdict '{1}' but got '{2}'" -f $case.Name, $case.ExpectedVerdict, $compareSummary.route_verdict)

        $level1Added = @([string[]]$compareSummary.route_changes.level1_surface_changes.added)
        $level1Removed = @([string[]]$compareSummary.route_changes.level1_surface_changes.removed)
        foreach ($surfaceId in @($case.AddedLevel1)) {
            Assert-Condition `
                -Condition ($level1Added -contains [string]$surfaceId) `
                -Message ("case '{0}' expected added level-1 surface '{1}'" -f $case.Name, $surfaceId)
        }
        foreach ($surfaceId in @($case.RemovedLevel1)) {
            Assert-Condition `
                -Condition ($level1Removed -contains [string]$surfaceId) `
                -Message ("case '{0}' expected removed level-1 surface '{1}'" -f $case.Name, $surfaceId)
        }

        Assert-Condition `
            -Condition ([int]$compareSummary.entry_summary.changed_entry_count -ge 1) `
            -Message ("case '{0}' expected at least one changed route entry" -f $case.Name)

        Write-Host (
            "[FRONT-PAGE-ROUTE-COMPARE-SMOKE] case={0} verdict={1} level1=+[{2}] -[{3}]" -f
            $case.Name,
            [string]$compareSummary.route_verdict,
            ($level1Added -join ","),
            ($level1Removed -join ",")
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ROUTE-COMPARE-SMOKE] output_root={0}" -f $outputRootPath)
