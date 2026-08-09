param(
    [string]$BuildDir = "Examples/ui/vivid/soa_demo/cmake-build-soa-ci",
    [string]$CMakeExe = "cmake"
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
if (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $RepoRoot $BuildDir
}
$BuildDir = [System.IO.Path]::GetFullPath($BuildDir)
$repoPrefix = $RepoRoot.TrimEnd('\') + '\'
if (-not $BuildDir.StartsWith($repoPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "BuildDir must stay inside the repository: $BuildDir"
}

$FixtureDir = Join-Path $BuildDir "vivid-stack-usage-gate-smoke"
$fixturePrefix = $BuildDir.TrimEnd('\') + '\'
if (-not $FixtureDir.StartsWith($fixturePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Stack fixture escaped the build directory: $FixtureDir"
}

function Write-Utf8NoBom {
    param(
        [string]$Path,
        [string]$Content
    )

    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Content, $encoding)
}

try {
    if (Test-Path -LiteralPath $FixtureDir) {
        Remove-Item -LiteralPath $FixtureDir -Recurse -Force
    }
    $StackRoot = Join-Path $FixtureDir "stack"
    New-Item -ItemType Directory -Path $StackRoot -Force | Out-Null

    $SourceManifest = Join-Path $FixtureDir "stack_usage_sources.txt"
    $OutputManifest = Join-Path $FixtureDir "stack_usage_manifest.txt"
    $StackFile = Join-Path $StackRoot "probe.cppm.su"
    $source = "Modules/ui/vivid/gfx/stack_gate_probe.cppm"
    $repoPath = $RepoRoot.Replace('\', '/')
    Write-Utf8NoBom -Path $SourceManifest -Content "$source`n"
    Write-Utf8NoBom -Path $StackFile -Content (
        "$repoPath/$source`:11`:7`:void template_probe() [with int A = 1; int B = 2]`t5000`tstatic`n" +
        "$repoPath/$source`:12`:7`:void bounded_probe()`t64`tstatic`n"
    )

    $gate = Join-Path $RepoRoot "Modules/ui/vivid/cmake/stack_usage_gate.cmake"
    $args = @(
        "-DVIVID_STACK_USAGE_ROOT=$StackRoot",
        "-DVIVID_STACK_USAGE_SOURCE_MANIFEST=$SourceManifest",
        "-DVIVID_STACK_USAGE_OUT=$OutputManifest",
        "-DVIVID_STACK_USAGE_MAX_BYTES=4096",
        "-DVIVID_STACK_USAGE_ENFORCE=ON",
        "-P", $gate
    )
    $previousErrorAction = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $output = & $CMakeExe @args 2>&1
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorAction
    }
    if ($exitCode -eq 0) {
        throw "Stack gate accepted the over-limit template frame"
    }
    if (($output -join "`n") -notmatch 'violations=1') {
        $output | Out-Host
        throw "Stack gate did not report the expected violation"
    }
    if (-not (Test-Path -LiteralPath $OutputManifest)) {
        throw "Stack gate did not write its failure manifest"
    }

    $manifest = Get-Content -LiteralPath $OutputManifest -Raw -Encoding UTF8
    foreach ($expected in @(
        "entry_count=2",
        "max_observed_bytes=5000",
        "violation_count=1",
        "5000|static|$repoPath/$source`:11`:7`:void template_probe() [with int A = 1; int B = 2]"
    )) {
        if (-not $manifest.Contains($expected)) {
            throw "Stack manifest is missing expected evidence: $expected"
        }
    }
} finally {
    if (Test-Path -LiteralPath $FixtureDir) {
        Remove-Item -LiteralPath $FixtureDir -Recurse -Force
    }
}

Write-Host "[OK] Vivid stack usage gate smoke passed"
