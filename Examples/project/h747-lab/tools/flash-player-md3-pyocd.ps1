param(
    [string]$Elf = "",
    [string]$Probe = "0001",
    [string]$Target = "stm32h747xihx",
    [string]$Frequency = "1000k"
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")

if ([string]::IsNullOrWhiteSpace($Elf)) {
    $Elf = Join-Path $ProjectRoot "cmake-build-h747-lab-debug\h747_lab_player_md3.elf"
}

if (-not (Test-Path -LiteralPath $Elf)) {
    throw "ELF not found: $Elf"
}

$PyOcd = Get-Command pyocd -ErrorAction Stop
$ResolvedElf = (Resolve-Path -LiteralPath $Elf).Path

Write-Host "Flashing H747 Lab Player MD3"
Write-Host "  pyocd:     $($PyOcd.Source)"
Write-Host "  probe:     $Probe"
Write-Host "  target:    $Target"
Write-Host "  frequency: $Frequency"
Write-Host "  elf:       $ResolvedElf"
Write-Host ""
Write-Host "Note: pyOCD may print 'Exception reading AP#2 IDR: Memory transfer fault' on this board."
Write-Host "Treat the flash as successful when pyOCD exits with 0 and prints the final erase/program summary."
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
Write-Host "Flash command completed. Serial smoke: COM16, 115200 8N1."
