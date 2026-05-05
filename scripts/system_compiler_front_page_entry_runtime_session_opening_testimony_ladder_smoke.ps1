param(
    [string]$WitnessRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-witness-root-smoke",
    [string]$LandingRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-landing-smoke",
    [string]$LandingCompareRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-landing-compare-smoke",
    [string]$RouteRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-landing-route-smoke",
    [string]$RouteCompareRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-landing-route-compare-smoke",
    [string]$ExplainEntryRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-explain-entry-smoke",
    [string]$ExplainEntryRouteCompareRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-explain-entry-route-compare-smoke",
    [string]$ExplainEntryCompareRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-explain-entry-compare-smoke",
    [string]$ExplainEntryCompareRouteRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-explain-entry-compare-route-smoke",
    [string]$PythonExe = "",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

function Resolve-FullPath {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { return "" }
    if ([System.IO.Path]::IsPathRooted($Path)) { return [System.IO.Path]::GetFullPath($Path) }
    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

function Resolve-ToolPath {
    param([string[]]$Candidates)
    foreach ($candidate in $Candidates) {
        $command = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($null -ne $command) { return $command.Source }
    }
    throw "tool not found: $($Candidates -join ', ')"
}

function Assert-Condition {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

function Load-JsonObject {
    param([string]$Path)
    return (Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json)
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$witnessRootPath = Resolve-FullPath -Path $WitnessRoot
$landingRootPath = Resolve-FullPath -Path $LandingRoot
$landingCompareRootPath = Resolve-FullPath -Path $LandingCompareRoot
$routeRootPath = Resolve-FullPath -Path $RouteRoot
$routeCompareRootPath = Resolve-FullPath -Path $RouteCompareRoot
$explainEntryRootPath = Resolve-FullPath -Path $ExplainEntryRoot
$explainEntryRouteCompareRootPath = Resolve-FullPath -Path $ExplainEntryRouteCompareRoot
$explainEntryCompareRootPath = Resolve-FullPath -Path $ExplainEntryCompareRoot
$explainEntryCompareRouteRootPath = Resolve-FullPath -Path $ExplainEntryCompareRouteRoot
$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}
$powerShellExe = Resolve-ToolPath -Candidates @("powershell.exe", "pwsh.exe", "powershell", "pwsh")

$scripts = [ordered]@{
    witness = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_opening_testimony_witness_root_smoke.ps1"
    landing = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_opening_testimony_landing_smoke.ps1"
    landing_compare = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_opening_testimony_landing_compare_smoke.ps1"
    route = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_opening_testimony_landing_route_smoke.ps1"
    route_compare = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_opening_testimony_landing_route_compare_smoke.ps1"
    explain_entry = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_opening_testimony_explain_entry_smoke.ps1"
    explain_entry_route_compare = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_opening_testimony_explain_entry_route_compare_smoke.ps1"
    explain_entry_compare = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_opening_testimony_explain_entry_compare_smoke.ps1"
    explain_entry_compare_route = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_opening_testimony_explain_entry_compare_route_smoke.ps1"
}
foreach ($requiredPath in $scripts.Values) {
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "missing path: $requiredPath" }
}

Push-Location $repoRoot
try {
    & $powerShellExe -NoProfile -ExecutionPolicy Bypass -File $scripts.witness -OutputRoot $witnessRootPath -PythonExe $resolvedPythonExe @($(if ($Clean) { "-Clean" }))
    if ($LASTEXITCODE -ne 0) { throw "runtime session testimony witness-root smoke failed" }

    & $powerShellExe -NoProfile -ExecutionPolicy Bypass -File $scripts.landing -RuntimeSessionWitnessRoot $witnessRootPath -OutputRoot $landingRootPath -PythonExe $resolvedPythonExe @($(if ($Clean) { "-Clean" }))
    if ($LASTEXITCODE -ne 0) { throw "runtime session testimony landing smoke failed" }

    & $powerShellExe -NoProfile -ExecutionPolicy Bypass -File $scripts.landing_compare -RuntimeSessionLandingRoot $landingRootPath -OutputRoot $landingCompareRootPath -PythonExe $resolvedPythonExe @($(if ($Clean) { "-Clean" }))
    if ($LASTEXITCODE -ne 0) { throw "runtime session testimony landing compare smoke failed" }

    & $powerShellExe -NoProfile -ExecutionPolicy Bypass -File $scripts.route -RuntimeSessionLandingRoot $landingRootPath -RuntimeSessionLandingCompareRoot $landingCompareRootPath -OutputRoot $routeRootPath -PythonExe $resolvedPythonExe @($(if ($Clean) { "-Clean" }))
    if ($LASTEXITCODE -ne 0) { throw "runtime session testimony route smoke failed" }

    & $powerShellExe -NoProfile -ExecutionPolicy Bypass -File $scripts.route_compare -RuntimeSessionRouteRoot $routeRootPath -OutputRoot $routeCompareRootPath -PythonExe $resolvedPythonExe @($(if ($Clean) { "-Clean" }))
    if ($LASTEXITCODE -ne 0) { throw "runtime session testimony route compare smoke failed" }

    & $powerShellExe -NoProfile -ExecutionPolicy Bypass -File $scripts.explain_entry -RuntimeSessionRouteRoot $routeRootPath -OutputRoot $explainEntryRootPath -PythonExe $resolvedPythonExe @($(if ($Clean) { "-Clean" }))
    if ($LASTEXITCODE -ne 0) { throw "runtime session testimony explain-entry smoke failed" }

    & $powerShellExe -NoProfile -ExecutionPolicy Bypass -File $scripts.explain_entry_route_compare -RuntimeSessionRouteCompareRoot $routeCompareRootPath -RuntimeSessionLandingCompareRoot $landingCompareRootPath -RuntimeSessionRouteRoot $routeRootPath -OutputRoot $explainEntryRouteCompareRootPath -PythonExe $resolvedPythonExe @($(if ($Clean) { "-Clean" }))
    if ($LASTEXITCODE -ne 0) { throw "runtime session testimony explain-entry route-compare smoke failed" }

    & $powerShellExe -NoProfile -ExecutionPolicy Bypass -File $scripts.explain_entry_compare -RuntimeSessionExplainEntryRoot $explainEntryRootPath -RuntimeSessionExplainEntryRouteCompareRoot $explainEntryRouteCompareRootPath -OutputRoot $explainEntryCompareRootPath -PythonExe $resolvedPythonExe @($(if ($Clean) { "-Clean" }))
    if ($LASTEXITCODE -ne 0) { throw "runtime session testimony explain-entry compare smoke failed" }

    & $powerShellExe -NoProfile -ExecutionPolicy Bypass -File $scripts.explain_entry_compare_route -RuntimeSessionExplainEntryCompareRoot $explainEntryCompareRootPath -OutputRoot $explainEntryCompareRouteRootPath -PythonExe $resolvedPythonExe @($(if ($Clean) { "-Clean" }))
    if ($LASTEXITCODE -ne 0) { throw "runtime session testimony explain-entry compare-route smoke failed" }

    $cleanLanding = Load-JsonObject -Path (Join-Path $landingRootPath "clean-witness-landing\front-page.entry-opening-testimony.landing.summary.json")
    $driftLanding = Load-JsonObject -Path (Join-Path $landingRootPath "drift-witness-landing\front-page.entry-opening-testimony.landing.summary.json")
    $cleanRoute = Load-JsonObject -Path (Join-Path $routeRootPath "clean-landing-route\front-page.route.summary.json")
    $driftExplain = Load-JsonObject -Path (Join-Path $explainEntryRootPath "drift-route-explain-entry\front-page.entry-opening-testimony.explain-entry.summary.json")
    $collapsedRouteCompareExplain = Load-JsonObject -Path (Join-Path $explainEntryRouteCompareRootPath "collapsed-route-compare-explain-entry\front-page.entry-opening-testimony.explain-entry.summary.json")
    $collapsedExplainCompare = Load-JsonObject -Path (Join-Path $explainEntryCompareRootPath "ready-to-blocked\front-page.entry-opening-testimony.explain-entry.compare.summary.json")
    $readyCompareRouteExplain = Load-JsonObject -Path (Join-Path $explainEntryCompareRouteRootPath "drifted-compare-route-explain-entry\front-page.entry-opening-testimony.explain-entry.summary.json")

    Assert-Condition `
        -Condition ([string]$cleanLanding.opening_identity.source_judgment_status -eq "accepted") `
        -Message "runtime-session clean landing should keep accepted testimony status"
    Assert-Condition `
        -Condition ([string]$driftLanding.opening_identity.source_judgment_status -eq "accepted_with_drift") `
        -Message "runtime-session drift landing should keep accepted_with_drift testimony status"
    Assert-Condition `
        -Condition ([string]$cleanRoute.root_surface.summary_schema -eq "system_compiler.front_page_entry_opening_testimony_landing/v0") `
        -Message "runtime-session clean route should root at opening_testimony_landing"
    Assert-Condition `
        -Condition ([string]$driftExplain.explain_entry_decision.status -eq "ready") `
        -Message "runtime-session drift explain-entry should remain ready"
    Assert-Condition `
        -Condition ([string]$collapsedRouteCompareExplain.explain_entry_decision.status -eq "blocked") `
        -Message "runtime-session collapsed route-compare explain-entry should be blocked"
    Assert-Condition `
        -Condition ([string]$collapsedExplainCompare.explain_entry_verdict -eq "collapsed") `
        -Message "runtime-session explain-entry compare should expose collapsed verdict"
    Assert-Condition `
        -Condition ([string]$readyCompareRouteExplain.explain_entry_decision.status -eq "ready") `
        -Message "runtime-session explain-entry compare-route should keep ready explain-entry projection"
    Assert-Condition `
        -Condition ([string]$readyCompareRouteExplain.selected_surface.surface_id -eq "candidate_opening_testimony_explain_entry") `
        -Message "runtime-session explain-entry compare-route should point at candidate explain-entry surface"

    Write-Host (
        "[RUNTIME-SESSION-OPENING-TESTIMONY-LADDER-SMOKE] clean={0} drift={1} route_root={2} explain={3} compare={4} compare_route={5}" -f
        [string]$cleanLanding.opening_identity.source_judgment_status,
        [string]$driftLanding.opening_identity.source_judgment_status,
        [string]$cleanRoute.root_surface.summary_schema,
        [string]$driftExplain.selected_surface.surface_id,
        [string]$collapsedExplainCompare.explain_entry_verdict,
        [string]$readyCompareRouteExplain.selected_surface.surface_id
    )
} finally {
    Pop-Location
}
