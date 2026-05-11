param(
    [string]$BuildDir = "cmake-build-soa-ci",
    [string]$Exe = "",
    [string]$OutDir = "artifacts/soa_ci",
    [ValidateSet("ci","dump","replay")] [string]$Mode = "ci",
    [string]$DumpPath = "",
    [string]$ReplayPath = "",
    [switch]$Tile,
    [switch]$RequireFontProvider,
    [switch]$RequireFallbackFont,
    [switch]$RequireUtf8ReplaceDisabled,
    [switch]$NoBuild
)

$ErrorActionPreference = "Stop"

if (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $PSScriptRoot $BuildDir
}

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "../../../..")).Path
if (-not [System.IO.Path]::IsPathRooted($OutDir)) {
    $OutDir = Join-Path $RepoRoot $OutDir
}

if (-not $Exe) {
    $Exe = Join-Path $BuildDir "vivid-soa-demo.exe"
}

$configureArgs = @(
    "-S", $PSScriptRoot,
    "-B", $BuildDir,
    "-G", "Ninja",
    "-DCHARM_VIVID_SOA_TRACE_INPUT=ON"
)
Write-Host "[soa-ci] configure: cmake $($configureArgs -join ' ')"
& cmake @configureArgs 2>&1 | Out-Host
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (-not $NoBuild) {
    $buildArgs = @("--build", $BuildDir, "--target", "vivid-soa-demo", "-j", "22")
    Write-Host "[soa-ci] build: cmake $($buildArgs -join ' ')"
    & cmake @buildArgs 2>&1 | Out-Host
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

if ($Mode -ne "ci" -and -not (Test-Path $Exe)) {
    throw "Executable not found: $Exe"
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$logPath = Join-Path $OutDir "soa_ci.log"
$gitHash = ""
try {
    $gitHash = (git rev-parse HEAD 2>$null)
} catch { }

if ($gitHash) {
    "[soa-ci] git=$gitHash" | Out-File -FilePath $logPath -Encoding ASCII
}

switch ($Mode) {
    "ci" {
        $ctestArgs = @("--test-dir", $BuildDir, "--output-on-failure")
        Write-Host "[soa-ci] ctest: ctest $($ctestArgs -join ' ')"
        & ctest @ctestArgs 2>&1 | Tee-Object -FilePath $logPath
        exit $LASTEXITCODE
    }
    "dump" {
        if (-not $DumpPath) {
            $DumpPath = Join-Path $OutDir "soa_dump.vcmd"
        }
        $args = @("--dump-cmd=$DumpPath", "--soa-compare", "--regress-ui")
    }
    "replay" {
        if (-not $ReplayPath) {
            throw "ReplayPath is required for Mode=replay"
        }
        $args = @("--replay-cmd=$ReplayPath")
        if ($Tile) {
            $args += "--backend=tile"
        } else {
            $args += "--backend=full"
        }
    }
}

if ($RequireFontProvider) { $args += "--require-font-provider" }
if ($RequireFallbackFont) { $args += "--require-fallback-font" }
if ($RequireUtf8ReplaceDisabled) { $args += "--require-utf8-replace-disabled" }

Write-Host "[soa-ci] run: $Exe $($args -join ' ')"
& $Exe @args 2>&1 | Tee-Object -FilePath $logPath
exit $LASTEXITCODE
