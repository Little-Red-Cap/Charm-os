param(
    [ValidateSet("mainline", "stdio")]
    [string]$Stage = "mainline",
    [string]$QemuExe = "D:\Toolchains\qemu\qemu-system-arm.exe",
    [string]$ElfPath = "",
    [int]$TimeoutSec = 8,
    [int]$TailLines = 80,
    [string]$ReportPath = "",
    [bool]$KeepLogs = $false
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($ElfPath)) {
    if ($Stage -eq "stdio") {
        $ElfPath = "cmake-build-arm3\posix-qemu-newlib-stdio.elf"
    } else {
        $ElfPath = "cmake-build-arm3\posix-qemu-demo.elf"
    }
}

$runner = Join-Path $PSScriptRoot "run_qemu_ci.ps1"
if (-not (Test-Path $runner)) {
    throw "runner not found: $runner"
}

$requireBusybox = $Stage -eq "mainline"
Write-Host ("[stage1] stage={0} elf={1}" -f $Stage, $ElfPath)
if ($ReportPath -ne "") {
    & $runner `
        -QemuExe $QemuExe `
        -ElfPath $ElfPath `
        -TimeoutSec $TimeoutSec `
        -RequirePosixSmoke $true `
        -RequireBusyboxPhase2 $requireBusybox `
        -TailLines $TailLines `
        -ReportPath $ReportPath `
        -KeepLogs $KeepLogs
} else {
    & $runner `
        -QemuExe $QemuExe `
        -ElfPath $ElfPath `
        -TimeoutSec $TimeoutSec `
        -RequirePosixSmoke $true `
        -RequireBusyboxPhase2 $requireBusybox `
        -TailLines $TailLines `
        -KeepLogs $KeepLogs
}
exit $LASTEXITCODE
