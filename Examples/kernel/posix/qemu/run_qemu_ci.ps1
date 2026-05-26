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
$debugPath = Join-Path $PSScriptRoot "qemu-ci.debug.log"
function Write-DebugLog {
    param([string]$Message)
    if (-not $env:QEMU_CI_DEBUG) { return }
    $line = "[{0}] {1}" -f (Get-Date).ToString("s"), $Message
    Add-Content -Path $debugPath -Value $line
}
if ($env:QEMU_CI_DEBUG) {
    try { Remove-Item $debugPath -Force -ErrorAction SilentlyContinue } catch { }
}

if (-not (Test-Path $QemuExe)) {
    throw "qemu not found: $QemuExe"
}
if (-not (Test-Path $ElfPath)) {
    throw "elf not found: $ElfPath"
}
Write-DebugLog "start qemu=$QemuExe elf=$ElfPath timeout=$TimeoutSec"

$outFile = Join-Path $PSScriptRoot "qemu-ci.log"
function Reset-LogFile {
    param([string]$Path)
    if (-not (Test-Path $Path)) { return }
    try {
        Remove-Item $Path -Force -ErrorAction Stop
    } catch {
        try {
            Clear-Content $Path -ErrorAction SilentlyContinue
        } catch {
        }
    }
}

if (-not $KeepLogs) { Reset-LogFile -Path $outFile }

$args = @(
    "-M", "mps2-an500",
    "-cpu", "cortex-m7",
    "-nographic",
    "-kernel", $ElfPath
)

$errFile = Join-Path $PSScriptRoot "qemu-ci.err.log"
if (-not $KeepLogs) { Reset-LogFile -Path $errFile }

$proc = Start-Process -FilePath $QemuExe -ArgumentList $args `
    -RedirectStandardOutput $outFile -RedirectStandardError $errFile -PassThru
Write-DebugLog "started pid=$($proc.Id)"

function Read-LogSafe {
    param([string]$Path, [int]$MaxBytes = 16384)
    if (-not (Test-Path $Path)) { return "" }
    try {
        $fs = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open,
                                     [System.IO.FileAccess]::Read,
                                     [System.IO.FileShare]::ReadWrite)
        try {
            $len = $fs.Length
            $start = [Math]::Max(0, $len - $MaxBytes)
            $null = $fs.Seek($start, [System.IO.SeekOrigin]::Begin)
            $sr = New-Object System.IO.StreamReader($fs)
            $text = $sr.ReadToEnd()
            $sr.Close()
            return $text
        } finally {
            $fs.Close()
        }
    } catch {
        return ""
    }
}

function Stop-QemuProcessTree {
    param([int]$RootId, [string]$Elf)
    $pids = @()
    if ($RootId -gt 0) { $pids += $RootId }
    try {
        $children = Get-CimInstance Win32_Process `
            | Where-Object { $_.Name -eq "qemu-system-arm.exe" -and $_.ParentProcessId -eq $RootId }
        foreach ($c in $children) { $pids += $c.ProcessId }
    } catch {
    }
    try {
        $matches = Get-CimInstance Win32_Process `
            | Where-Object { $_.Name -eq "qemu-system-arm.exe" -and $_.CommandLine -like "*$Elf*" }
        foreach ($m in $matches) { $pids += $m.ProcessId }
    } catch {
    }
    $pids = $pids | Sort-Object -Unique
    foreach ($procId in $pids) {
        try { Stop-Process -Id $procId -Force -ErrorAction Stop } catch { }
    }
}

$posixOk = $false
$bb2Ok = $false
$sw = [System.Diagnostics.Stopwatch]::StartNew()

while ($true) {
    if ($proc.HasExited) { break }
    if ($sw.Elapsed.TotalSeconds -ge $TimeoutSec) { break }

    $log = (Read-LogSafe -Path $outFile) + (Read-LogSafe -Path $errFile)
    $posixOk = $log.Contains("[posix-smoke] end ok")
    $bb2Ok = $log.Contains("bb2 all ok")
    Write-DebugLog ("poll posix={0} bb2={1} elapsed={2:N1}" -f $posixOk,$bb2Ok,$sw.Elapsed.TotalSeconds)

    if ($posixOk -and (-not $RequireBusyboxPhase2 -or $bb2Ok)) {
        try {
            Stop-QemuProcessTree -RootId $proc.Id -Elf $ElfPath
        } catch {
            Write-DebugLog ("stop error: {0}" -f $_.Exception.Message)
        }
        try { Wait-Process -Id $proc.Id -Timeout 2 -ErrorAction SilentlyContinue } catch { }
        break
    }

    Start-Sleep -Milliseconds 200
}

if (-not $proc.HasExited) {
    try {
        Stop-QemuProcessTree -RootId $proc.Id -Elf $ElfPath
    } catch {
        Write-DebugLog ("stop error: {0}" -f $_.Exception.Message)
    }
    try { Wait-Process -Id $proc.Id -Timeout 2 -ErrorAction SilentlyContinue } catch { }
}
Write-DebugLog "stopped pid=$($proc.Id) exited=$($proc.HasExited)"

$log = (Read-LogSafe -Path $outFile) + (Read-LogSafe -Path $errFile)
$posixOk = $log.Contains("[posix-smoke] end ok")
$bb2Ok = $log.Contains("bb2 all ok")
Write-DebugLog ("final posix={0} bb2={1}" -f $posixOk,$bb2Ok)

if ($RequirePosixSmoke -and -not $posixOk) {
    if ($TailLines -gt 0) {
        Write-Host "[posix-smoke] log tail:"
        Get-Content $outFile -Tail $TailLines | ForEach-Object { Write-Host $_ }
        Get-Content $errFile -Tail $TailLines | ForEach-Object { Write-Host $_ }
    }
    Write-Error "posix smoke failed"
    exit 1
}

if ($RequireBusyboxPhase2 -and -not $bb2Ok) {
    if ($TailLines -gt 0) {
        Write-Host "[bb2] log tail:"
        Get-Content $outFile -Tail $TailLines | ForEach-Object { Write-Host $_ }
        Get-Content $errFile -Tail $TailLines | ForEach-Object { Write-Host $_ }
    }
    Write-Error "busybox phase2 smoke failed"
    exit 1
}

if ($ReportPath -ne "") {
    $report = @(
        "mode=" + ($(if ($RequireBusyboxPhase2) { "stage1-mainline" } else { "stage1-stdio" })),
        "posix_smoke=" + ($(if ($posixOk) { "ok" } else { "fail" })),
        "busybox_phase2=" + ($(if ($bb2Ok) { "ok" } else { "fail" })),
        "elf=" + $ElfPath,
        "timestamp=" + (Get-Date).ToString("s")
    ) -join "`n"
    Set-Content -Encoding utf8 $ReportPath $report
}

$successParts = @()
if ($RequirePosixSmoke) {
    $successParts += "posix smoke"
}
if ($RequireBusyboxPhase2) {
    $successParts += "busybox phase2 smoke"
}
if ($successParts.Count -eq 0) {
    $successParts += "qemu run"
}

Write-Host ("[ok] " + ($successParts -join " + "))
exit 0
