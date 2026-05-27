param(
    [string]$Probe = "0001",
    [string]$Target = "stm32h747xihx",
    [string]$Frequency = "1000k",
    [string]$Elf = "",
    [switch]$ForceStopStalePyOcd
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
Set-Location $ProjectRoot

if ([string]::IsNullOrWhiteSpace($Elf)) {
    $Elf = Join-Path $ProjectRoot "cmake-build-h747-lab-debug\h747_lab_usb_msc_legacy_probe.elf"
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

if (-not (Test-Path -LiteralPath $Elf)) {
    throw "ELF not found: $Elf. Build preset first: cmake --build --preset build-h747-lab-usb-msc-legacy-probe-debug"
}

$PyOcd = Get-Command pyocd -ErrorAction Stop
$ResolvedElf = (Resolve-Path -LiteralPath $Elf).Path

Write-Host "Flashing H747 Lab USB MSC Legacy Probe"
Write-Host "  pyocd:     $($PyOcd.Source)"
Write-Host "  probe:     $Probe"
Write-Host "  target:    $Target"
Write-Host "  frequency: $Frequency"
Write-Host "  elf:       $ResolvedElf"
Write-Host ""
Write-Host "Note: pyOCD may print AP#2 discovery faults on this board."
Write-Host "Treat the flash as successful only when pyOCD exits with 0 and prints the final erase/program summary."
Write-Host ""

& $PyOcd.Source load `
    -u $Probe `
    -t $Target `
    -f $Frequency `
    --format elf `
    $ResolvedElf

if ($LASTEXITCODE -ne 0) {
    throw "pyocd load failed with exit code $LASTEXITCODE"
}

Write-Host ""
Write-Host "Flash command completed. Serial validation: COM16, 115200 8N1, command 'status'."
