param(
    [string]$Bin = "",
    [string]$Probe = "0001",
    [string]$Target = "stm32h747xihx",
    [string]$Frequency = "1000k",
    [string]$Connect = "under-reset",
    [string]$Erase = "sector",
    [switch]$ForceStopStalePyOcd
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")

if ([string]::IsNullOrWhiteSpace($Bin)) {
    $Bin = Join-Path $ProjectRoot "cmake-build-h747-lab-debug\h747_lab_dev_loader.bin"
}

if (-not (Test-Path -LiteralPath $Bin)) {
    throw "BIN not found: $Bin. Build preset first: cmake --build --preset build-h747-lab-dev-loader-debug"
}

$Stale = Get-CimInstance Win32_Process |
    Where-Object {
        ($_.Name -match '^(pyocd|python|python3\.10)\.exe$') -and
        ($_.CommandLine -like '*pyocd*') -and
        ($_.CommandLine -like '*load*') -and
        ($_.ProcessId -ne $PID)
    }

if ($Stale) {
    Write-Host "Existing pyOCD load process(es) detected:"
    foreach ($Process in $Stale) {
        Write-Host "  pid=$($Process.ProcessId) $($Process.CommandLine)"
    }
    if (-not $ForceStopStalePyOcd) {
        throw "Refusing to flash while another pyOCD load process may own the CMSIS-DAP probe. Re-run with -ForceStopStalePyOcd if it is known stale."
    }
    foreach ($Process in $Stale) {
        Stop-Process -Id $Process.ProcessId -Force -ErrorAction SilentlyContinue
    }
    Start-Sleep -Seconds 2
}

$PyOcd = Get-Command pyocd -ErrorAction Stop
$ResolvedBin = (Resolve-Path -LiteralPath $Bin).Path

Write-Host "Flashing H747 Lab Dev Loader"
Write-Host "  pyocd:     $($PyOcd.Source)"
Write-Host "  probe:     $Probe"
Write-Host "  target:    $Target"
Write-Host "  frequency: $Frequency"
Write-Host "  connect:   $Connect"
Write-Host "  erase:     $Erase"
Write-Host "  bin:       $ResolvedBin"
Write-Host ""
Write-Host "Known CMSIS-DAP v1 caveats on this board:"
Write-Host "  - pyOCD may print AP#2 discovery faults that are not fatal by themselves."
Write-Host "  - Stale pyOCD load processes can corrupt the next DAP_INFO handshake."
Write-Host "  - If Flash analysis fails with DAP_TRANSFER_BLOCK, this script disables smart/keep-unwritten reads."
Write-Host ""

$Elapsed = Measure-Command {
    & $PyOcd.Source load `
        -u $Probe `
        -t $Target `
        -f $Frequency `
        -M $Connect `
        -O keep_unwritten=false `
        -O smart_flash=false `
        -O fast_program=false `
        --format bin `
        -e $Erase `
        -a 0x08000000 `
        $ResolvedBin
    $script:PyOcdExit = $LASTEXITCODE
}

Write-Host ""
Write-Host ("pyocd exit: {0}" -f $script:PyOcdExit)
Write-Host ("elapsed:    {0:n3}s" -f $Elapsed.TotalSeconds)

if ($script:PyOcdExit -ne 0) {
    throw "pyocd load failed with exit code $script:PyOcdExit"
}

Write-Host ""
Write-Host "Flash command completed. Serial validation: COM16, 115200 8N1, command 'dev status'."
