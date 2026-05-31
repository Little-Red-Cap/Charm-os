param(
    [string]$PacketStream = "",
    [string]$ControlPort = "COM16",
    [string]$UsbPort = "",
    [int]$ControlBaudRate = 115200,
    [int]$UsbBaudRate = 115200,
    [int]$TimeoutSeconds = 45,
    [int]$UsbEnumerateTimeoutSeconds = 20,
    [int]$UsbSettleMilliseconds = 1000,
    [int[]]$ChunkSizes = @(256, 512, 1024),
    [int]$Repeat = 3,
    [string]$Log = "",
    [switch]$DryRun,
    [switch]$SelfTest
)

$ErrorActionPreference = "Stop"

if ($TimeoutSeconds -le 0) {
    throw "TimeoutSeconds must be greater than zero."
}
if ($UsbEnumerateTimeoutSeconds -le 0) {
    throw "UsbEnumerateTimeoutSeconds must be greater than zero."
}
if ($UsbSettleMilliseconds -lt 0) {
    throw "UsbSettleMilliseconds must not be negative."
}
if ($ControlBaudRate -le 0 -or $UsbBaudRate -le 0) {
    throw "Baud rates must be greater than zero."
}
if ($Repeat -le 0) {
    throw "Repeat must be greater than zero."
}
if ($ChunkSizes.Count -eq 0) {
    throw "At least one chunk size is required."
}
foreach ($ChunkSize in $ChunkSizes) {
    if ($ChunkSize -le 0) {
        throw "Chunk sizes must be greater than zero."
    }
}

function Test-ContainsLiteral {
    param(
        [string]$Text,
        [string]$Needle
    )

    if ($null -eq $Text -or $null -eq $Needle) {
        return $false
    }
    return $Text.IndexOf($Needle, [System.StringComparison]::Ordinal) -ge 0
}

function Get-MissingTokens {
    param(
        [string]$Text,
        [string[]]$Tokens
    )

    $Missing = New-Object System.Collections.Generic.List[string]
    foreach ($Token in $Tokens) {
        if (-not (Test-ContainsLiteral -Text $Text -Needle $Token)) {
            [void]$Missing.Add($Token)
        }
    }
    return ,$Missing.ToArray()
}

function Invoke-SelfTest {
    $PassingLog = @"
=== chunk 256 bytes ===
USB CDC App Store transfer smoke passed.
chunk 256 passed
=== chunk 512 bytes ===
USB CDC App Store transfer smoke passed.
chunk 512 passed
"@
    $Missing = Get-MissingTokens -Text $PassingLog -Tokens @("chunk 256 passed", "chunk 512 passed")
    if ($Missing.Count -ne 0) {
        throw "Self-test failed: missing expected tokens: $($Missing -join ', ')"
    }
    Write-Host "Dev Loader USB CDC throughput sweep self-test passed."
    return 0
}

if ($SelfTest) {
    exit (Invoke-SelfTest)
}

$ProjectRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($PacketStream)) {
    $PacketStream = Join-Path $ProjectRoot "..\..\app_abi\elf_samples\out\appstore.bin.packetstream"
}
if (-not (Test-Path -LiteralPath $PacketStream)) {
    throw "PacketStream not found: $PacketStream"
}

$TransferSmoke = Join-Path $PSScriptRoot "capture-dev-loader-usb-cdc-appstore-transfer-smoke.ps1"
if (-not (Test-Path -LiteralPath $TransferSmoke)) {
    throw "USB CDC App Store transfer smoke not found: $TransferSmoke"
}

if ([string]::IsNullOrWhiteSpace($Log)) {
    $Log = Join-Path $ProjectRoot "cmake-build-h747-lab-debug\h747_lab_dev_loader_usb_cdc_throughput_sweep.log"
}

$ResolvedPacketStream = (Resolve-Path -LiteralPath $PacketStream).Path
$ResolvedLog = [System.IO.Path]::GetFullPath($Log)
$LogDir = Split-Path -Parent $ResolvedLog
if (-not [string]::IsNullOrWhiteSpace($LogDir)) {
    New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
}

Write-Host "H747 Dev Loader USB CDC throughput sweep"
Write-Host "  control port: $ControlPort"
Write-Host "  usb port:     $(if ([string]::IsNullOrWhiteSpace($UsbPort)) { 'auto' } else { $UsbPort })"
Write-Host "  packetstream: $ResolvedPacketStream"
Write-Host "  chunks:       $($ChunkSizes -join ', ')"
Write-Host "  repeat:       $Repeat"
Write-Host "  log:          $ResolvedLog"
if ($DryRun) {
    Write-Host ""
    Write-Host "Dry run: no serial ports were opened."
    exit 0
}

$Utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$LogWriter = [System.IO.StreamWriter]::new($ResolvedLog, $false, $Utf8NoBom)
try {
    $LogWriter.WriteLine("H747 Dev Loader USB CDC throughput sweep")
    $LogWriter.WriteLine("  control port: $ControlPort")
    $LogWriter.WriteLine("  usb port:     $(if ([string]::IsNullOrWhiteSpace($UsbPort)) { 'auto' } else { $UsbPort })")
    $LogWriter.WriteLine("  packetstream: $ResolvedPacketStream")
    $LogWriter.WriteLine("  chunks:       $($ChunkSizes -join ', ')")
    $LogWriter.WriteLine("  repeat:       $Repeat")
    $LogWriter.WriteLine("")
    $LogWriter.Flush()

    foreach ($ChunkSize in $ChunkSizes) {
        $ChunkLog = [System.IO.Path]::ChangeExtension($ResolvedLog, ".chunk$ChunkSize.log")
        $Header = "=== chunk $ChunkSize bytes ==="
        Write-Host ""
        Write-Host $Header
        $LogWriter.WriteLine($Header)
        $LogWriter.Flush()

        & powershell -NoProfile -ExecutionPolicy Bypass -File $TransferSmoke `
            -PacketStream $ResolvedPacketStream `
            -ControlPort $ControlPort `
            -UsbPort $UsbPort `
            -ControlBaudRate $ControlBaudRate `
            -UsbBaudRate $UsbBaudRate `
            -TimeoutSeconds $TimeoutSeconds `
            -UsbEnumerateTimeoutSeconds $UsbEnumerateTimeoutSeconds `
            -UsbSettleMilliseconds $UsbSettleMilliseconds `
            -WriteChunkSize $ChunkSize `
            -InterChunkDelayMs 0 `
            -Repeat $Repeat `
            -Log $ChunkLog
        if ($LASTEXITCODE -ne 0) {
            $LogWriter.WriteLine("chunk $ChunkSize failed")
            $LogWriter.WriteLine("log: $ChunkLog")
            $LogWriter.Flush()
            throw "USB CDC throughput sweep failed at chunk size $ChunkSize."
        }

        $ChunkText = Get-Content -LiteralPath $ChunkLog -Raw -Encoding UTF8
        $LogWriter.WriteLine("--- chunk $ChunkSize log ---")
        $LogWriter.Write($ChunkText)
        $LogWriter.WriteLine("")
        $LogWriter.WriteLine("chunk $ChunkSize passed")
        $LogWriter.Flush()
        Write-Host "chunk $ChunkSize passed"
    }

    Write-Host ""
    Write-Host "USB CDC throughput sweep passed."
    $LogWriter.WriteLine("USB CDC throughput sweep passed.")
    exit 0
} finally {
    $LogWriter.Dispose()
}
