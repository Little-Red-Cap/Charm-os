param(
    [string]$Image = "",
    [string]$Probe = "0001",
    [string]$Target = "stm32h747xihx",
    [string]$Frequency = "1000k",
    [string]$Address = "0x08000000"
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")

if ([string]::IsNullOrWhiteSpace($Image)) {
    $Image = Join-Path $ProjectRoot "cmake-build-h747-lab-debug\h747_lab_player_md3.bin"
}

if (-not (Test-Path -LiteralPath $Image)) {
    throw "Image not found: $Image"
}

$PyOcd = Get-Command pyocd -ErrorAction Stop
$ResolvedImage = (Resolve-Path -LiteralPath $Image).Path

Write-Host "Flashing H747 Lab Player MD3"
Write-Host "  pyocd:     $($PyOcd.Source)"
Write-Host "  probe:     $Probe"
Write-Host "  target:    $Target"
Write-Host "  frequency: $Frequency"
Write-Host "  address:   $Address"
Write-Host "  image:     $ResolvedImage"
Write-Host ""
Write-Host "Note: pyOCD may print 'Exception reading AP#2 IDR: Memory transfer fault' on this board."
Write-Host "Treat the flash as successful when pyOCD exits with 0 and prints the final erase/program summary."
Write-Host ""

& $PyOcd.Source load `
    -u $Probe `
    -t $Target `
    -f $Frequency `
    --connect halt `
    --erase sector `
    --format bin `
    -a $Address `
    $ResolvedImage

if ($LASTEXITCODE -ne 0) {
    throw "pyocd load failed with exit code $LASTEXITCODE"
}

Write-Host ""
Write-Host "Flash command completed. Serial smoke: COM16, 115200 8N1."
