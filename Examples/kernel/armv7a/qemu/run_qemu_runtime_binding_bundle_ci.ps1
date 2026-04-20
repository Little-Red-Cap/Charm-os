param(
    [string]$CMakeExe = "cmake",
    [string]$QemuExe = "qemu-system-arm",
    [int]$BuildJobs = 1,
    [int]$TimeoutSec = 10,
    [int]$TailLines = 40,
    [switch]$SkipBuild
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
        return [string](Get-Content $Path -Raw)
    } catch {
        return ""
    }
}

function Show-LogTail {
    param(
        [string]$OutPath,
        [string]$ErrPath,
        [int]$Lines
    )

    Write-Output "[armv7a-qemu-runtime-binding-bundle] log tail:"
    if (Test-Path $OutPath) {
        Get-Content $OutPath -Tail $Lines
    }
    if (Test-Path $ErrPath) {
        Get-Content $ErrPath -Tail $Lines
    }
}

$cmake = Resolve-ToolPath -Tool $CMakeExe
$qemu = Resolve-ToolPath -Tool $QemuExe
$configurePreset = "debug"
$buildPreset = "debug"
$elfPath = "out\\build\\debug\\charm-armv7a-qemu"

if (-not $SkipBuild) {
    Push-Location $PSScriptRoot
    try {
        & $cmake --preset $configurePreset
        if ($LASTEXITCODE -ne 0) {
            throw "cmake configure failed for preset: $configurePreset"
        }

        & $cmake --build --preset $buildPreset --parallel $BuildJobs
        if ($LASTEXITCODE -ne 0) {
            throw "cmake build failed for preset: $buildPreset"
        }
    } finally {
        Pop-Location
    }
}

$elf = Resolve-ExamplePath -Path $elfPath
$outFile = Join-Path $PSScriptRoot "qemu-runtime-binding-bundle.log"
$errFile = Join-Path $PSScriptRoot "qemu-runtime-binding-bundle.err.log"

Remove-Item $outFile, $errFile -Force -ErrorAction SilentlyContinue

$args = @(
    "-machine", "virt",
    "-cpu", "cortex-a7",
    "-display", "none",
    "-serial", "file:$outFile",
    "-monitor", "none",
    "-device", "loader,file=$elf,cpu-num=0"
)

$proc = Start-Process -FilePath $qemu -ArgumentList $args `
    -RedirectStandardError $errFile -PassThru

Start-Sleep -Seconds $TimeoutSec

if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    Wait-Process -Id $proc.Id -Timeout 2 -ErrorAction SilentlyContinue
}

$log = [string](Read-LogSafe -Path $outFile) + [string](Read-LogSafe -Path $errFile)
$expected = @(
    "Charm ARMv7-A QEMU skeleton",
    "Targeting Cortex-A7 first, RK3506 later.",
    "ARMv7-A phase, stage=runtime-binding-bundle",
    "ARMv7-A phase complete, stage=runtime-binding-bundle",
    "ARMv7-A phase, stage=runtime-leaf-bundle"
)
$missing = $expected | Where-Object { -not $log.Contains($_) }

$phaseMarker = "ARMv7-A phase, stage=runtime-binding-bundle"
$nextPhaseMarker = "ARMv7-A phase, stage=runtime-leaf-bundle"
$phaseIndex = $log.IndexOf($phaseMarker)
$nextPhaseIndex = if ($phaseIndex -ge 0) {
    $log.IndexOf($nextPhaseMarker, $phaseIndex)
} else {
    -1
}
$bindingLog = if ($phaseIndex -ge 0) {
    if ($nextPhaseIndex -gt $phaseIndex) {
        $log.Substring($phaseIndex, $nextPhaseIndex - $phaseIndex)
    } else {
        $log.Substring($phaseIndex)
    }
} else {
    $log
}

if (($bindingLog -notmatch "ARMv7-A runtime binding bundle, current=yes, trap=yes, thread=yes, loop=yes, live=yes, binding=yes")) {
    $missing += "ARMv7-A runtime binding bundle, current=yes..."
}

if ($missing.Count -gt 0) {
    Show-LogTail -OutPath $outFile -ErrPath $errFile -Lines $TailLines
    throw "missing expected output: $($missing -join '; ')"
}

Write-Output "[ok] armv7a qemu runtime-binding-bundle smoke detected"
