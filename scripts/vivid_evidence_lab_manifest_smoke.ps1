param(
    [string]$BuildRoot = "cmake-build-vivid-evidence-lab-manifest-smoke",
    [string]$CMakeExe = "cmake",
    [string]$Generator = "Ninja",
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

    return [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $Path))
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

function Invoke-Step {
    param(
        [string]$Name,
        [scriptblock]$Step
    )

    Write-Host ("==> {0}" -f $Name)
    & $Step
    if ($LASTEXITCODE -ne 0) {
        throw ("{0} failed with exit code {1}" -f $Name, $LASTEXITCODE)
    }
}

$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$SourceDir = Join-Path $RepoRoot "Examples/ui/vivid/evidence_lab_manifest_demo"
$ResolvedBuildRoot = Resolve-FullPath -Path $BuildRoot

if (-not (Test-Path -LiteralPath $SourceDir)) {
    throw "missing Vivid evidence manifest demo: $SourceDir"
}

if ($Clean) {
    Remove-PathIfExists -Path $ResolvedBuildRoot
}

Invoke-Step -Name "configure vivid evidence manifest" -Step {
    & $CMakeExe -S $SourceDir -B $ResolvedBuildRoot -G $Generator
}

Invoke-Step -Name "build vivid evidence manifest" -Step {
    & $CMakeExe --build $ResolvedBuildRoot
}

Invoke-Step -Name "test vivid evidence manifest" -Step {
    & $CMakeExe --build $ResolvedBuildRoot --target test
}

Write-Host ("[OK] vivid evidence lab manifest smoke passed build_root={0}" -f $ResolvedBuildRoot)
