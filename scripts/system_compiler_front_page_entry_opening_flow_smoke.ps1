param(
    [string]$RouteRoot = "cmake-build-system-compiler-front-page-route-smoke",
    [string]$CapabilityRoot = "cmake-build-system-compiler-front-page-entry-capability-smoke",
    [string]$LandingRoot = "cmake-build-system-compiler-front-page-entry-landing-smoke",
    [string]$LandingCompareRoot = "cmake-build-system-compiler-front-page-entry-landing-compare-smoke",
    [string]$OpenerRoot = "cmake-build-system-compiler-front-page-entry-opener-smoke",
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

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    if (Test-Path $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
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
$routeRootPath = Resolve-FullPath -Path $RouteRoot
$capabilityRootPath = Resolve-FullPath -Path $CapabilityRoot
$landingRootPath = Resolve-FullPath -Path $LandingRoot
$landingCompareRootPath = Resolve-FullPath -Path $LandingCompareRoot
$openerRootPath = Resolve-FullPath -Path $OpenerRoot

if ($Clean) {
    foreach ($path in @($capabilityRootPath, $landingRootPath, $landingCompareRootPath, $openerRootPath)) {
        Remove-PathIfExists -Path $path
    }
}

$capabilitySmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_capability_smoke.ps1"
$landingSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_landing_smoke.ps1"
$landingCompareSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_landing_compare_smoke.ps1"
$openerSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opener_smoke.ps1"
foreach ($requiredPath in @($capabilitySmokeScript, $landingSmokeScript, $landingCompareSmokeScript, $openerSmokeScript)) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

$pythonArgs = @()
if (-not [string]::IsNullOrWhiteSpace($PythonExe)) {
    $pythonArgs = @("-PythonExe", (Resolve-FullPath -Path $PythonExe))
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
            $capabilitySmokeScript,
            "-InputRoot",
            $routeRootPath,
            "-OutputRoot",
            $capabilityRootPath
        ) + $pythonArgs `
        -FailureMessage "front page entry capability smoke failed"

    Invoke-ExternalTool `
        -Executable "powershell.exe" `
        -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $landingSmokeScript,
            "-InputRoot",
            $capabilityRootPath,
            "-OutputRoot",
            $landingRootPath
        ) + $pythonArgs `
        -FailureMessage "front page entry landing smoke failed"

    Invoke-ExternalTool `
        -Executable "powershell.exe" `
        -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $landingCompareSmokeScript,
            "-InputRoot",
            $landingRootPath,
            "-OutputRoot",
            $landingCompareRootPath
        ) + $pythonArgs `
        -FailureMessage "front page entry landing compare smoke failed"

    Invoke-ExternalTool `
        -Executable "powershell.exe" `
        -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $openerSmokeScript,
            "-LandingRoot",
            $landingRootPath,
            "-LandingCompareRoot",
            $landingCompareRootPath,
            "-OutputRoot",
            $openerRootPath
        ) + $pythonArgs `
        -FailureMessage "front page entry opener smoke failed"

    $expectedOpeners = @(
        "root-witness",
        "root-world-compare",
        "root-witness-to-root-world-compare",
        "root-world-compare-to-root-witness",
        "witness-ci-shelf",
        "review-provenance",
        "runtime-evidence-sample"
    )
    $availableProjectionCount = 0
    $compareContextCount = 0
    foreach ($caseName in $expectedOpeners) {
        $summaryPath = Join-Path $openerRootPath "$caseName\front-page.entry-opener.summary.json"
        Assert-Condition `
            -Condition (Test-Path $summaryPath) `
            -Message ("missing opener summary for case '{0}': {1}" -f $caseName, $summaryPath)
        $summary = Load-JsonObject -Path $summaryPath
        if ([string]$summary.opened_projection.status -eq "available") {
            $availableProjectionCount += 1
        }
        if ([bool]$summary.compare_context.available) {
            $compareContextCount += 1
        }
    }

    Assert-Condition `
        -Condition ($availableProjectionCount -eq @($expectedOpeners).Count) `
        -Message ("expected all opener cases to expose available projections, got {0}/{1}" -f $availableProjectionCount, @($expectedOpeners).Count)
    Assert-Condition `
        -Condition ($compareContextCount -ge 2) `
        -Message ("expected at least two opener cases with compare context, got {0}" -f $compareContextCount)

    Write-Host (
        "[FRONT-PAGE-ENTRY-OPENING-FLOW-SMOKE] openers={0} projections={1} compare_context={2}" -f
        @($expectedOpeners).Count,
        $availableProjectionCount,
        $compareContextCount
    )
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-SMOKE] route_root={0}" -f $routeRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-SMOKE] capability_root={0}" -f $capabilityRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-SMOKE] landing_root={0}" -f $landingRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-SMOKE] landing_compare_root={0}" -f $landingCompareRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-SMOKE] opener_root={0}" -f $openerRootPath)
