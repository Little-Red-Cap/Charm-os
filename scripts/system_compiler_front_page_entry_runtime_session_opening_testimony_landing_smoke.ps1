param(
    [string]$RuntimeSessionWitnessRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-witness-root-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-landing-smoke",
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

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$runtimeSessionWitnessRootPath = Resolve-FullPath -Path $RuntimeSessionWitnessRoot
$outputRootPath = Resolve-FullPath -Path $OutputRoot
$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}
$powerShellExe = Resolve-ToolPath -Candidates @("powershell.exe", "pwsh.exe", "powershell", "pwsh")
$witnessRootSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_opening_testimony_witness_root_smoke.ps1"
$genericSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_testimony_landing_smoke.ps1"
foreach ($requiredPath in @($witnessRootSmokeScript, $genericSmokeScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "missing path: $requiredPath" }
}

Push-Location $repoRoot
try {
    & $powerShellExe -NoProfile -ExecutionPolicy Bypass -File $witnessRootSmokeScript -RuntimeSessionOpenEventWitnessRoot $runtimeSessionWitnessRootPath -OutputRoot $runtimeSessionWitnessRootPath -PythonExe $resolvedPythonExe @($(if ($Clean) { "-Clean" }))
    if ($LASTEXITCODE -ne 0) { throw "runtime session testimony witness-root smoke bootstrap failed" }

    & $powerShellExe -NoProfile -ExecutionPolicy Bypass -File $genericSmokeScript -OpenEventWitnessRoot $runtimeSessionWitnessRootPath -OutputRoot $outputRootPath -PythonExe $resolvedPythonExe @($(if ($Clean) { "-Clean" }))
    if ($LASTEXITCODE -ne 0) { throw "runtime session testimony landing smoke failed" }
} finally {
    Pop-Location
}
