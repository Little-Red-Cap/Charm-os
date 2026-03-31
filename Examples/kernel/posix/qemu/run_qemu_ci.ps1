param(
    [string]$QemuExe = "D:\Toolchains\qemu\qemu-system-arm.exe",
    [string]$ElfPath = "cmake-build-arm3\posix-qemu-demo.elf",
    [int]$TimeoutSec = 8,
    [bool]$RequirePosixSmoke = $true,
    [bool]$RequireBusyboxPhase2 = $true,
    [int]$TailLines = 80,
    [string]$ReportPath = "",
    [bool]$KeepLogs = $false
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $QemuExe)) {
    throw "qemu not found: $QemuExe"
}
if (-not (Test-Path $ElfPath)) {
    throw "elf not found: $ElfPath"
}

$outFile = Join-Path $PSScriptRoot "qemu-ci.log"
if ((-not $KeepLogs) -and (Test-Path $outFile)) { Remove-Item $outFile -Force }

$args = @(
    "-M", "mps2-an500",
    "-cpu", "cortex-m7",
    "-nographic",
    "-kernel", $ElfPath
)

$errFile = Join-Path $PSScriptRoot "qemu-ci.err.log"
if ((-not $KeepLogs) -and (Test-Path $errFile)) { Remove-Item $errFile -Force }

$proc = Start-Process -FilePath $QemuExe -ArgumentList $args `
    -RedirectStandardOutput $outFile -RedirectStandardError $errFile -PassThru

Start-Sleep -Seconds $TimeoutSec

if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force
}

$log = (Get-Content $outFile -Raw) + (Get-Content $errFile -Raw)
$posixOk = $log.Contains("[posix-smoke] end ok")
$bb2Ok = $log.Contains("bb2 all ok")

if ($RequirePosixSmoke -and -not $posixOk) {
    if ($TailLines -gt 0) {
        Write-Host "[posix-smoke] log tail:"
        Get-Content $outFile -Tail $TailLines | ForEach-Object { Write-Host $_ }
        Get-Content $errFile -Tail $TailLines | ForEach-Object { Write-Host $_ }
    }
    throw "posix smoke failed"
}

if ($RequireBusyboxPhase2 -and -not $bb2Ok) {
    if ($TailLines -gt 0) {
        Write-Host "[bb2] log tail:"
        Get-Content $outFile -Tail $TailLines | ForEach-Object { Write-Host $_ }
        Get-Content $errFile -Tail $TailLines | ForEach-Object { Write-Host $_ }
    }
    throw "busybox phase2 smoke failed"
}

if ($ReportPath -ne "") {
    $report = @(
        "posix_smoke=" + ($(if ($posixOk) { "ok" } else { "fail" })),
        "busybox_phase2=" + ($(if ($bb2Ok) { "ok" } else { "fail" })),
        "elf=" + $ElfPath,
        "timestamp=" + (Get-Date).ToString("s")
    ) -join "`n"
    Set-Content -Encoding utf8 $ReportPath $report
}

Write-Host "[ok] posix smoke + busybox phase2 smoke"
