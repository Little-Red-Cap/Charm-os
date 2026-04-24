param(
    [string]$BuildDir = "Examples/ui/vivid/soa_demo/cmake-build-debug",
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

if (-not $Exe) {
    $Exe = Join-Path $BuildDir "Debug/vivid-soa-demo.exe"
}

if (-not (Test-Path $Exe)) {
    throw "Executable not found: $Exe"
}

if (-not $NoBuild) {
    $buildCmd = "cmake --build `"$BuildDir`" --target vivid-soa-demo -j 22"
    Write-Host "[soa-ci] build: $buildCmd"
    cmd /c $buildCmd | Out-Host
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
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
        if (-not $DumpPath) {
            $DumpPath = Join-Path $OutDir "soa_ci.vcmd"
        }
        $args = @("--soa-ci", "--regress-ui", "--dump-cmd=$DumpPath")
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
