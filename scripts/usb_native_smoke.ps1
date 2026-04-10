param(
    [int]$Jobs = 4,
    [switch]$Clean,
    [switch]$ConfigureOnly
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot

$cases = @(
    @{
        Name = 'usb-cdc-mock-smoke'
        Source = Join-Path $repoRoot 'Examples/usb/usb_cdc_mock_smoke'
        Build = Join-Path $repoRoot 'cmake-build-usb-cdc-mock-smoke-clang'
        Exe = 'usb-cdc-mock-smoke.exe'
    },
    @{
        Name = 'usb-replay-suite-smoke'
        Source = Join-Path $repoRoot 'Examples/usb/usb_replay_suite_smoke'
        Build = Join-Path $repoRoot 'cmake-build-usb-replay-suite-smoke-clang'
        Exe = 'usb-replay-suite-smoke.exe'
    }
)

foreach ($case in $cases) {
    if ($Clean -and (Test-Path $case.Build)) {
        Remove-Item -Recurse -Force $case.Build
    }

    Write-Host "[CFG] $($case.Name)"
    cmake -S $case.Source -B $case.Build -G Ninja -D CMAKE_C_COMPILER=clang -D CMAKE_CXX_COMPILER=clang++
    if ($LASTEXITCODE -ne 0) {
        throw "configure failed: $($case.Name)"
    }

    if ($ConfigureOnly) {
        continue
    }

    Write-Host "[BUILD] $($case.Name)"
    cmake --build $case.Build --target $case.Name -j $Jobs
    if ($LASTEXITCODE -ne 0) {
        throw "build failed: $($case.Name)"
    }

    $exePath = Join-Path $case.Build $case.Exe
    Write-Host "[RUN] $exePath"
    & $exePath
    if ($LASTEXITCODE -ne 0) {
        throw "run failed: $($case.Name)"
    }
}

if ($ConfigureOnly) {
    Write-Host '[OK] configure finished'
} else {
    Write-Host '[OK] usb native smoke finished'
}
