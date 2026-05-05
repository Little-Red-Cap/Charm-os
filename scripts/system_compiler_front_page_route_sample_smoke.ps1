param(
    [string]$SampleSummary = "schemas/examples/system_compiler.witness_bundle.v0.sample.json",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-route-sample-smoke",
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

function Remove-PathIfExists {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path $Path)) {
        return
    }

    Remove-Item -LiteralPath $Path -Recurse -Force
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
$sampleSummaryPath = Resolve-FullPath -Path $SampleSummary
$outputRootPath = Resolve-FullPath -Path $OutputRoot

if (-not (Test-Path $sampleSummaryPath)) {
    throw "sample summary not found: $sampleSummaryPath"
}

if ($Clean) {
    Remove-PathIfExists -Path $outputRootPath
}

$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}

$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_route.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_route.py"
foreach ($requiredPath in @($exportScript, $validateScript)) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing script: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($exportScript, "--summary", $sampleSummaryPath, "--output-root", $outputRootPath) `
        -FailureMessage "front page route sample export failed"

    $routeSummaryPath = Join-Path $outputRootPath "front-page.route.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateScript, "--summary", $routeSummaryPath) `
        -FailureMessage "front page route sample validation failed"

    $routeSummary = Load-JsonObject -Path $routeSummaryPath
    $level1SurfaceIds = @(
        @($routeSummary.route_entries) |
            Where-Object { [int]$_.depth -eq 1 } |
            ForEach-Object { [string]$_.surface_id }
    )
    $level1Joined = $level1SurfaceIds -join ","
    Assert-Condition `
        -Condition ($level1Joined -eq "runtime_evidence,kernel_runtime_session") `
        -Message ("expected sample level-1 surfaces 'runtime_evidence,kernel_runtime_session' but got '{0}'" -f $level1Joined)

    $sessionEntry = @(
        @($routeSummary.route_entries) |
            Where-Object {
                [string]$_.surface_id -eq "kernel_runtime_session" -and
                [string]$_.summary_schema -eq "minimal_kernel.kernel_runtime_session/v0" -and
                [string]$_.summary_kind -eq "minimal_kernel.kernel_runtime_session"
            }
    )
    Assert-Condition `
        -Condition ($sessionEntry.Count -eq 1) `
        -Message ("expected one opened kernel_runtime_session route entry but got {0}" -f $sessionEntry.Count)

    Write-Host ("[FRONT-PAGE-ROUTE-SAMPLE-SMOKE] summary={0}" -f $routeSummaryPath)
    Write-Host ("[FRONT-PAGE-ROUTE-SAMPLE-SMOKE] level1={0}" -f $level1Joined)
    Write-Host "ok=1"
} finally {
    Pop-Location
}
