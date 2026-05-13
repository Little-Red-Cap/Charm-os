param(
    [string]$CMakeExe = "cmake",
    [string]$Generator = "Ninja",
    [string]$SummaryPath = "out/audio-sdl3-smoke/summary.json",
    [int]$Jobs = 0,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$rootScript = Join-Path $PSScriptRoot "audio_sdl3_wav_demo_smoke.ps1"
if (-not (Test-Path $rootScript)) {
    throw "missing audio smoke script: $rootScript"
}

$invokeArgs = @{
    CMakeExe    = $CMakeExe
    Generator   = $Generator
    SummaryPath = $SummaryPath
    Jobs        = $Jobs
}

if ($Clean) {
    $invokeArgs.Clean = $true
}

& $rootScript @invokeArgs
exit $LASTEXITCODE
