param(
    [string]$QemuExe = "D:\Toolchains\qemu\qemu-system-arm.exe",
    [string]$ElfPath = "cmake-build-arm3\posix-qemu-demo.elf",
    [int]$TimeoutSec = 2
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $QemuExe)) {
    throw "qemu not found: $QemuExe"
}
if (-not (Test-Path $ElfPath)) {
    throw "elf not found: $ElfPath"
}

$outFile = Join-Path $PSScriptRoot "qemu-ci.log"
if (Test-Path $outFile) { Remove-Item $outFile -Force }

$args = @(
    "-M", "mps2-an500",
    "-cpu", "cortex-m7",
    "-nographic",
    "-kernel", $ElfPath
)

$errFile = Join-Path $PSScriptRoot "qemu-ci.err.log"
if (Test-Path $errFile) { Remove-Item $errFile -Force }

$proc = Start-Process -FilePath $QemuExe -ArgumentList $args `
    -RedirectStandardOutput $outFile -RedirectStandardError $errFile -PassThru

Start-Sleep -Seconds $TimeoutSec

if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force
}

$log = (Get-Content $outFile -Raw) + (Get-Content $errFile -Raw)
if ($log -notmatch "bb2 all ok") {
    throw "busybox phase2 smoke failed"
}

Write-Host "[ok] busybox phase2 smoke"
