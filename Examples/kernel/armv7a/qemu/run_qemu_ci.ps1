param(
    [string]$QemuExe = "qemu-system-arm",
    [string]$ElfPath = "out\\build\\debug\\charm-armv7a-qemu",
    [int]$TimeoutSec = 3,
    [int]$TailLines = 40
)

$ErrorActionPreference = "Stop"

function Resolve-ToolPath {
    param([string]$Tool)

    $cmd = Get-Command $Tool -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    if (Test-Path $Tool) {
        return (Resolve-Path $Tool).Path
    }

    throw "tool not found: $Tool"
}

function Resolve-ExamplePath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return (Resolve-Path $Path).Path
    }

    return (Resolve-Path (Join-Path $PSScriptRoot $Path)).Path
}

function Read-LogSafe {
    param([string]$Path)

    if (-not (Test-Path $Path)) {
        return ""
    }

    try {
        return Get-Content $Path -Raw
    } catch {
        return ""
    }
}

$qemu = Resolve-ToolPath -Tool $QemuExe
$elf = Resolve-ExamplePath -Path $ElfPath
$outFile = Join-Path $PSScriptRoot "qemu-ci.log"
$errFile = Join-Path $PSScriptRoot "qemu-ci.err.log"

Remove-Item $outFile, $errFile -Force -ErrorAction SilentlyContinue

$args = @(
    "-machine", "virt",
    "-cpu", "cortex-a7",
    "-nographic",
    "-monitor", "none",
    "-device", "loader,file=$elf,cpu-num=0"
)

$proc = Start-Process -FilePath $qemu -ArgumentList $args `
    -RedirectStandardOutput $outFile -RedirectStandardError $errFile -PassThru

$expected = @(
    "Charm ARMv7-A QEMU skeleton",
    "Targeting Cortex-A7 first, RK3506 later.",
    "Charm out.format import active, PL011 @ 0x09000000",
    "ARMv7-A SVC vector active, imm=0x000043"
)

Start-Sleep -Seconds $TimeoutSec

if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    Wait-Process -Id $proc.Id -Timeout 2 -ErrorAction SilentlyContinue
}

$log = (Read-LogSafe -Path $outFile) + (Read-LogSafe -Path $errFile)
$missing = $expected | Where-Object { -not $log.Contains($_) }
if ($missing.Count -gt 0) {
    Write-Output "[armv7a-qemu] log tail:"
    if (Test-Path $outFile) {
        Get-Content $outFile -Tail $TailLines
    }
    if (Test-Path $errFile) {
        Get-Content $errFile -Tail $TailLines
    }
    throw "missing expected output: $($missing -join '; ')"
}

Write-Output "[ok] armv7a qemu hello detected"
