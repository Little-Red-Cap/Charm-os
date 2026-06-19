param(
    [string]$CMakeExe = "cmake",
    [string]$QemuExe = "D:\Toolchains\qemu\qemu-system-arm.exe",
    [string]$ToolchainPrefix = "arm-none-eabi-",
    [string]$HostCompiler = "D:/Toolchains/w64devkit/bin/g++.exe",
    [string]$BuildDir = "$PSScriptRoot\cmake-build-resident-elf-qemu-smoke",
    [string]$AppOutDir = "$PSScriptRoot\..\..\app_abi\elf_samples\out-qemu",
    [string]$FrameSignatureOut = "$PSScriptRoot\frame-signatures.json",
    [string]$GoldenFrameSignatures = "$PSScriptRoot\frame-signatures.golden.json",
    [string]$FrameDumpOut = "$PSScriptRoot\frame-dumps.json",
    [string]$FramePpmOut = "$PSScriptRoot\frame-ppm",
    [string]$InputTraceOut = "$PSScriptRoot\input-trace.json",
    [string]$GoldenInputTrace = "$PSScriptRoot\input-trace.golden.json",
    [string]$StorageTraceOut = "$PSScriptRoot\storage-trace.json",
    [string]$GoldenStorageTrace = "$PSScriptRoot\storage-trace.golden.json",
    [string]$DomainSummaryOut = "$PSScriptRoot\domain-summary.json",
    [string]$GoldenDomainSummary = "$PSScriptRoot\domain-summary.golden.json",
    [string]$ElfBase = "0x20080000",
    [int]$TimeoutSec = 8,
    [int]$TailLines = 80,
    [string]$ValidateLog = "",
    [string]$ValidateFrameSignatures = "",
    [string]$CompareFrameSignatures = "",
    [string]$ActualFrameSignatures = "",
    [string]$ValidateFrameDumps = "",
    [string]$ValidateFramePpm = "",
    [string]$ValidateInputTrace = "",
    [string]$CompareInputTrace = "",
    [string]$ActualInputTrace = "",
    [string]$ValidateStorageTrace = "",
    [string]$CompareStorageTrace = "",
    [string]$ActualStorageTrace = "",
    [string]$ValidateDomainSummary = "",
    [string]$CompareDomainSummary = "",
    [string]$ActualDomainSummary = "",
    [string]$CompareFrameDumps = "",
    [string]$ActualFrameDumps = "",
    [switch]$SkipGoldenFrameSignatures,
    [switch]$SkipGoldenInputTrace,
    [switch]$SkipGoldenStorageTrace,
    [switch]$SkipGoldenDomainSummary,
    [switch]$ValidateEvidenceBundle,
    [switch]$DryRun,
    [switch]$SelfTest
)

$ErrorActionPreference = "Stop"
$OutputEncoding = [System.Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)

function Resolve-ToolPath {
    param([string]$Tool)

    $cmd = Get-Command $Tool -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }
    if (Test-Path $Tool) {
        return (Resolve-Path $Tool).Path
    }
    throw "missing_tool: tool not found: $Tool"
}

function Invoke-Checked {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    if ($DryRun) {
        Write-Host ("[dry-run] {0} {1}" -f $FilePath, ($Arguments -join " "))
        return
    }
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "command failed with exit code ${LASTEXITCODE}: $FilePath $($Arguments -join ' ')"
    }
}

function Write-IncFile {
    param(
        [string]$InputPath,
        [string]$OutputPath,
        [string]$Symbol
    )

    $bytes = [System.IO.File]::ReadAllBytes($InputPath)
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.Append("static const unsigned char $Symbol[] = {`n")
    for ($i = 0; $i -lt $bytes.Length; $i++) {
        if (($i % 12) -eq 0) {
            [void]$sb.Append("    ")
        }
        [void]$sb.AppendFormat("0x{0:X2}, ", $bytes[$i])
        if (($i % 12) -eq 11) {
            [void]$sb.Append("`n")
        }
    }
    if (($bytes.Length % 12) -ne 0) {
        [void]$sb.Append("`n")
    }
    [void]$sb.Append("};`n")
    [void]$sb.Append("static const unsigned int ${Symbol}_len = $($bytes.Length);`n")
    [System.IO.File]::WriteAllText($OutputPath, $sb.ToString(), [System.Text.Encoding]::ASCII)
}

function Write-PaddedArtifact {
    param(
        [string]$InputPath,
        [string]$OutputPath,
        [int]$TargetSize
    )

    $bytes = [System.IO.File]::ReadAllBytes($InputPath)
    if ($bytes.Length -ge $TargetSize) {
        [System.IO.File]::WriteAllBytes($OutputPath, $bytes)
        return
    }
    $padded = New-Object byte[] $TargetSize
    [System.Array]::Copy($bytes, $padded, $bytes.Length)
    [System.IO.File]::WriteAllBytes($OutputPath, $padded)
}

function Read-LogSafe {
    param([string]$Path, [int]$MaxBytes = 131072)

    if (-not (Test-Path $Path)) {
        return ""
    }
    try {
        $fs = [System.IO.File]::Open($Path,
                                     [System.IO.FileMode]::Open,
                                     [System.IO.FileAccess]::Read,
                                     [System.IO.FileShare]::ReadWrite)
        try {
            $start = [Math]::Max(0, $fs.Length - $MaxBytes)
            $null = $fs.Seek($start, [System.IO.SeekOrigin]::Begin)
            $sr = New-Object System.IO.StreamReader($fs)
            try {
                return $sr.ReadToEnd()
            } finally {
                $sr.Close()
            }
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
    if ($RootId -gt 0) {
        $pids += $RootId
    }
    try {
        $children = Get-CimInstance Win32_Process |
            Where-Object { $_.Name -eq "qemu-system-arm.exe" -and $_.ParentProcessId -eq $RootId }
        foreach ($child in $children) {
            $pids += $child.ProcessId
        }
    } catch {
    }
    try {
        $matches = Get-CimInstance Win32_Process |
            Where-Object { $_.Name -eq "qemu-system-arm.exe" -and $_.CommandLine -like "*$Elf*" }
        foreach ($match in $matches) {
            $pids += $match.ProcessId
        }
    } catch {
    }
    $pids = $pids | Sort-Object -Unique
    foreach ($procId in $pids) {
        try {
            Stop-Process -Id $procId -Force -ErrorAction Stop
        } catch {
        }
    }
}

function Test-SelfTestThrowsLike {
    param(
        [scriptblock]$Script,
        [string]$Prefix
    )

    try {
        & $Script | Out-Null
    } catch {
        return $_.Exception.Message.StartsWith($Prefix, [System.StringComparison]::Ordinal)
    }
    return $false
}

function Get-QemuAppNames {
    return @(
        "hello_app",
        "player_min",
        "argv_app",
        "bss_app",
        "data_app",
        "display_sequence_app",
        "exit_app",
        "input_sequence_app",
        "large_fit_app",
        "unsupported_caps_app",
        "storage_app",
        "storage_catalog_app",
        "time_app",
        "too_large_app"
    )
}

function Resolve-ScriptPath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot $Path))
}

function Get-ExpectedTokens {
    return @(
        "resident-elf-qemu: begin",
        "resident-elf-qemu: backend=virtual_m7 machine=mps2-an500 cpu=cortex-m7",
        "resident-elf-qemu: backend-capabilities capabilities=console,time,display,input,storage,app_exit storage=readonly afe=unsupported",
        "resident-elf-qemu: run-region base=0x20080000 expected=0x20080000 size=65536",
        "resident-elf-qemu: stage-cache bytes=16384",
        "resident-elf-qemu: store entries=14 bytes=",
        "resident-elf-qemu: store-media kind=memory bytes=",
        "resident-elf-qemu: unsupported storage_open=1 storage_read=1 storage_write=1 storage_close=1 afe_configure=1 afe_read=1 storage_count=1/1/1/1 afe_count=1/1",
        "hello_app: charm_app_main entered",
        "hello_app: argv1=alpha",
        "resident-elf-qemu: app hello_app stage=exit code=ok exit=0",
        "resident-elf-qemu: capacity hello_app needed=270 free=65266 fits=1 region=65536 probe=ok",
        "resident-elf-qemu: caps hello_app console=78 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: received stage name=hello_app code=ok format=elf bytes=5132",
        "resident-elf-qemu: app received:hello_app stage=exit code=ok exit=0",
        "resident-elf-qemu: capacity received:hello_app needed=270 free=65266 fits=1 region=65536 probe=ok",
        "resident-elf-qemu: caps received:hello_app console=78 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: packetstream stage name=hello_app transport=ok packet=ok stage=launch_ready code=ok payload=5132",
        "resident-elf-qemu: packetstream read name=hello_app code=ok bytes=5132",
        "resident-elf-qemu: packetstream app-stage name=hello_app code=ok format=elf bytes=5132",
        "resident-elf-qemu: app packetstream:hello_app stage=exit code=ok exit=0",
        "resident-elf-qemu: capacity packetstream:hello_app needed=270 free=65266 fits=1 region=65536 probe=ok",
        "resident-elf-qemu: caps packetstream:hello_app console=78 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: store stage name=hello_app code=ok format=elf size=5132",
        "resident-elf-qemu: app store:hello_app stage=exit code=ok exit=0",
        "resident-elf-qemu: capacity store:hello_app needed=270 free=65266 fits=1 region=65536 probe=ok",
        "resident-elf-qemu: caps store:hello_app console=78 time=0 describe=0 present=0 input=0 exit=0",
        "argv_app: argc=4 checksum=2052",
        "resident-elf-qemu: app argv_app stage=exit code=ok exit=0",
        "resident-elf-qemu: capacity argv_app needed=396 free=65140 fits=1 region=65536 probe=ok",
        "resident-elf-qemu: caps argv_app console=31 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: store stage name=argv_app code=ok format=elf size=5392",
        "resident-elf-qemu: app store:argv_app stage=exit code=ok exit=0",
        "resident-elf-qemu: capacity store:argv_app needed=396 free=65140 fits=1 region=65536 probe=ok",
        "resident-elf-qemu: caps store:argv_app console=31 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: prepare prepare:argv_app stage=start code=ok ready=1 argc=4",
        "resident-elf-qemu: capacity prepare:argv_app needed=396 free=65140 fits=1 region=65536 probe=ok",
        "resident-elf-qemu: caps prepare:argv_app console=0 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=0/0/0/0 storage_bytes=0",
        "resident-elf-qemu: app argv_overflow_app stage=argv code=argv_overflow exit=0",
        "resident-elf-qemu: capacity argv_overflow_app needed=396 free=65140 fits=1 region=65536 probe=ok",
        "resident-elf-qemu: caps argv_overflow_app console=0 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=0/0/0/0 storage_bytes=0",
        "resident-elf-qemu: app abi_mismatch_app stage=abi code=abi_mismatch exit=0",
        "resident-elf-qemu: capacity abi_mismatch_app needed=270 free=65266 fits=1 region=65536 probe=ok",
        "resident-elf-qemu: caps abi_mismatch_app console=0 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=0/0/0/0 storage_bytes=0",
        "resident-elf-qemu: store stage name=missing_app code=image_not_found expected=image_not_found image_size=0",
        "resident-elf-qemu: caps missing_app console=0 time=0 describe=0 present=0 input=0 exit=0",
        "bss_app: zero-fill ok",
        "resident-elf-qemu: app bss_app stage=exit code=ok exit=0",
        "resident-elf-qemu: capacity bss_app needed=513 free=65023 fits=1 region=65536 probe=ok",
        "resident-elf-qemu: caps bss_app console=22 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: store stage name=bss_app code=ok format=elf size=",
        "resident-elf-qemu: app store:bss_app stage=exit code=ok exit=0",
        "resident-elf-qemu: capacity store:bss_app needed=513 free=65023 fits=1 region=65536 probe=ok",
        "resident-elf-qemu: caps store:bss_app console=22 time=0 describe=0 present=0 input=0 exit=0",
        "data_app: data-init ok checksum=50",
        "resident-elf-qemu: app data_app stage=exit code=ok exit=0",
        "resident-elf-qemu: store stage name=data_app code=ok format=elf size=",
        "resident-elf-qemu: app store:data_app stage=exit code=ok exit=0",
        "resident-elf-qemu: app.exit code=7",
        "resident-elf-qemu: app exit_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps exit_app console=0 time=0 describe=0 present=0 input=0 exit=1",
        "resident-elf-qemu: app store:exit_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps store:exit_app console=0 time=0 describe=0 present=0 input=0 exit=1",
        "resident-elf-qemu: app unsupported_caps_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps unsupported_caps_app console=0 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: app store:unsupported_caps_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps store:unsupported_caps_app console=0 time=0 describe=0 present=0 input=0 exit=0",
        "storage_app: bytes=27 checksum=2441",
        "resident-elf-qemu: app storage_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps storage_app console=36 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=1/5/0/1 storage_bytes=27",
        "resident-elf-qemu: app store:storage_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps store:storage_app console=36 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=1/5/0/1 storage_bytes=27",
        "storage_catalog_app: files=2 bytes=31 checksum=2845",
        "resident-elf-qemu: app storage_catalog_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps storage_catalog_app console=52 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=2/9/0/2 storage_bytes=31",
        "resident-elf-qemu: app store:storage_catalog_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps store:storage_catalog_app console=52 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=2/9/0/2 storage_bytes=31",
        "display_sequence_app: frames=2 checksum=3072",
        "resident-elf-qemu: display present bytes=1024 checksum=1024",
        "resident-elf-qemu: display present bytes=1024 checksum=1024 hash=0x373fb1c5 frame=1",
        "resident-elf-qemu: display present bytes=1024 checksum=2048",
        "resident-elf-qemu: display present bytes=1024 checksum=2048 hash=0xa9b09dc5 frame=2",
        "resident-elf-qemu: app display_sequence_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps display_sequence_app console=45 time=0 describe=1 present=2 input=0 exit=0 display_checksum=2048 display_checksum_total=3072",
        "display_hash=0xa9b09dc5 display_hash_total=0x9e8f2c00 display_frame=2",
        "resident-elf-qemu: app store:display_sequence_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps store:display_sequence_app console=45 time=0 describe=1 present=2 input=0 exit=0 display_checksum=2048 display_checksum_total=3072",
        "input_sequence_app: polls=4 checksum=114",
        "resident-elf-qemu: input poll encoder1=0 pointer=6,8 max=15,15 detected=1 down=0",
        "resident-elf-qemu: app input_sequence_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps input_sequence_app console=41 time=0 describe=0 present=0 input=4 exit=0",
        "input_checksum=114 input_last=6,8,0",
        "resident-elf-qemu: app store:input_sequence_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps store:input_sequence_app console=41 time=0 describe=0 present=0 input=4 exit=0",
        "resident-elf-qemu: app time_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps time_app console=0 time=2 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: app store:time_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps store:time_app console=0 time=2 describe=0 present=0 input=0 exit=0",
        "large_fit_app: near-limit ok",
        "resident-elf-qemu: app large_fit_app stage=exit code=ok exit=0",
        "resident-elf-qemu: capacity large_fit_app needed=61696 free=3840 fits=1 region=65536 probe=ok",
        "resident-elf-qemu: caps large_fit_app console=29 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: received stage name=large_fit_app code=ok format=elf bytes=",
        "resident-elf-qemu: app received:large_fit_app stage=exit code=ok exit=0",
        "resident-elf-qemu: capacity received:large_fit_app needed=61696 free=3840 fits=1 region=65536 probe=ok",
        "resident-elf-qemu: caps received:large_fit_app console=29 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: packetstream stage name=large_fit_app transport=ok packet=ok stage=launch_ready code=ok payload=5168",
        "resident-elf-qemu: packetstream read name=large_fit_app code=ok bytes=5168",
        "resident-elf-qemu: packetstream app-stage name=large_fit_app code=ok format=elf bytes=5168",
        "resident-elf-qemu: app packetstream:large_fit_app stage=exit code=ok exit=0",
        "resident-elf-qemu: capacity packetstream:large_fit_app needed=61696 free=3840 fits=1 region=65536 probe=ok",
        "resident-elf-qemu: caps packetstream:large_fit_app console=29 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: store stage name=large_fit_app code=ok format=elf size=",
        "resident-elf-qemu: app store:large_fit_app stage=exit code=ok exit=0",
        "resident-elf-qemu: capacity store:large_fit_app needed=61696 free=3840 fits=1 region=65536 probe=ok",
        "resident-elf-qemu: caps store:large_fit_app console=29 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: packetstream stage name=packetstream_crc_mismatch transport=packet_failed packet=receive_failed stage=failed code=crc_mismatch payload=5132",
        "resident-elf-qemu: caps packetstream_crc_mismatch console=0 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: received stage name=received_too_large_app code=buffer_too_small expected=buffer_too_small bytes=0 image_size=16385",
        "resident-elf-qemu: caps received_too_large_app console=0 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: store stage name=too_large_store_app code=image_too_large expected=image_too_large image_size=0",
        "resident-elf-qemu: caps too_large_store_app console=0 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: app bad_elf_magic_app stage=load code=load_failed exit=0",
        "resident-elf-qemu: load bad_elf_magic_app format=elf probe=bad_magic",
        "resident-elf-qemu: capacity bad_elf_magic_app needed=0 free=65536 fits=1 region=65536 probe=bad_magic",
        "resident-elf-qemu: caps bad_elf_magic_app console=0 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: packetstream stage name=packetstream_bad_elf_magic_app transport=ok packet=ok stage=launch_ready code=ok payload=64",
        "resident-elf-qemu: packetstream read name=packetstream_bad_elf_magic_app code=ok bytes=64",
        "resident-elf-qemu: packetstream app-stage name=packetstream_bad_elf_magic_app code=ok format=elf bytes=64",
        "resident-elf-qemu: app packetstream:packetstream_bad_elf_magic_app stage=load code=load_failed exit=0",
        "resident-elf-qemu: load packetstream:packetstream_bad_elf_magic_app format=elf probe=bad_magic",
        "resident-elf-qemu: capacity packetstream:packetstream_bad_elf_magic_app needed=0 free=65536 fits=1 region=65536 probe=bad_magic",
        "resident-elf-qemu: caps packetstream:packetstream_bad_elf_magic_app console=0 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: app bad_header_app stage=load code=load_failed exit=0",
        "resident-elf-qemu: load bad_header_app format=elf probe=bad_header",
        "resident-elf-qemu: capacity bad_header_app needed=0 free=65536 fits=1 region=65536 probe=bad_header",
        "resident-elf-qemu: caps bad_header_app console=0 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: app bad_class_app stage=load code=load_failed exit=0",
        "resident-elf-qemu: load bad_class_app format=elf probe=bad_class",
        "resident-elf-qemu: capacity bad_class_app needed=0 free=65536 fits=1 region=65536 probe=bad_class",
        "resident-elf-qemu: caps bad_class_app console=0 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: app bad_endian_app stage=load code=load_failed exit=0",
        "resident-elf-qemu: load bad_endian_app format=elf probe=bad_endian",
        "resident-elf-qemu: capacity bad_endian_app needed=0 free=65536 fits=1 region=65536 probe=bad_endian",
        "resident-elf-qemu: caps bad_endian_app console=0 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: app bad_program_header_app stage=load code=load_failed exit=0",
        "resident-elf-qemu: load bad_program_header_app format=elf probe=bad_program_header",
        "resident-elf-qemu: capacity bad_program_header_app needed=0 free=65536 fits=1 region=65536 probe=bad_program_header",
        "resident-elf-qemu: caps bad_program_header_app console=0 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: app truncated_payload_app stage=load code=load_failed exit=0",
        "resident-elf-qemu: load truncated_payload_app format=elf probe=truncated_payload",
        "resident-elf-qemu: capacity truncated_payload_app needed=0 free=65536 fits=1 region=65536 probe=truncated_payload",
        "resident-elf-qemu: caps truncated_payload_app console=0 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: app no_load_segment_app stage=load code=load_failed exit=0",
        "resident-elf-qemu: load no_load_segment_app format=elf probe=no_load_segment",
        "resident-elf-qemu: capacity no_load_segment_app needed=0 free=65536 fits=1 region=65536 probe=no_load_segment",
        "resident-elf-qemu: caps no_load_segment_app console=0 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: app entry_outside_segment_app stage=load code=load_failed exit=0",
        "resident-elf-qemu: load entry_outside_segment_app format=elf probe=entry_outside_segment",
        "resident-elf-qemu: capacity entry_outside_segment_app needed=0 free=65536 fits=1 region=65536 probe=entry_outside_segment",
        "resident-elf-qemu: caps entry_outside_segment_app console=0 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: app overlapping_segments_app stage=load code=load_failed exit=0",
        "resident-elf-qemu: load overlapping_segments_app format=elf probe=overlapping_segments",
        "resident-elf-qemu: capacity overlapping_segments_app needed=0 free=65536 fits=1 region=65536 probe=overlapping_segments",
        "resident-elf-qemu: caps overlapping_segments_app console=0 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: app rwx_segment_app stage=load code=load_failed exit=0",
        "resident-elf-qemu: load rwx_segment_app format=elf probe=rwx_segment",
        "resident-elf-qemu: capacity rwx_segment_app needed=0 free=65536 fits=1 region=65536 probe=rwx_segment",
        "resident-elf-qemu: caps rwx_segment_app console=0 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: app too_large_app stage=load code=load_failed exit=0",
        "resident-elf-qemu: load too_large_app format=elf probe=load_buffer_too_small",
        "resident-elf-qemu: capacity too_large_app needed=82176 free=0 fits=0 region=65536 probe=load_buffer_too_small",
        "resident-elf-qemu: caps too_large_app console=0 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: display describe width=16 height=16 stride=64 format=argb8888 frame_bytes=1024",
        "resident-elf-qemu: input poll",
        "resident-elf-qemu: input poll encoder1=1 pointer=3,5 max=15,15 detected=1 down=0",
        "resident-elf-qemu: display present bytes=1024",
        "resident-elf-qemu: display present bytes=1024 checksum=174720",
        "resident-elf-qemu: display present bytes=1024 checksum=174720 hash=0xfac53a05 frame=1",
        "player_min: presented one frame",
        "resident-elf-qemu: app player_min stage=exit code=ok exit=0",
        "resident-elf-qemu: capacity player_min needed=1280 free=64256 fits=1 region=65536 probe=ok",
        "resident-elf-qemu: caps player_min console=32 time=1 describe=1 present=1 input=1 exit=0 display_checksum=174720",
        "display_hash=0xfac53a05 display_hash_total=0xfac53a05 display_frame=1",
        "resident-elf-qemu: received stage name=player_min code=ok format=elf bytes=5168",
        "resident-elf-qemu: app received:player_min stage=exit code=ok exit=0",
        "resident-elf-qemu: capacity received:player_min needed=1280 free=64256 fits=1 region=65536 probe=ok",
        "resident-elf-qemu: caps received:player_min console=32 time=1 describe=1 present=1 input=1 exit=0 display_checksum=174720",
        "resident-elf-qemu: packetstream stage name=player_min transport=ok packet=ok stage=launch_ready code=ok payload=5168",
        "resident-elf-qemu: packetstream read name=player_min code=ok bytes=5168",
        "resident-elf-qemu: packetstream app-stage name=player_min code=ok format=elf bytes=5168",
        "resident-elf-qemu: app packetstream:player_min stage=exit code=ok exit=0",
        "resident-elf-qemu: capacity packetstream:player_min needed=1280 free=64256 fits=1 region=65536 probe=ok",
        "resident-elf-qemu: caps packetstream:player_min console=32 time=1 describe=1 present=1 input=1 exit=0 display_checksum=174720",
        "resident-elf-qemu: store stage name=player_min code=ok format=elf size=",
        "resident-elf-qemu: display present bytes=1024 checksum=174720",
        "resident-elf-qemu: app store:player_min stage=exit code=ok exit=0",
        "resident-elf-qemu: capacity store:player_min needed=1280 free=64256 fits=1 region=65536 probe=ok",
        "resident-elf-qemu: caps store:player_min console=32 time=1 describe=1 present=1 input=1 exit=0 display_checksum=174720",
        "resident-elf-qemu: ok"
    )
}

function Get-MissingTokens {
    param([string]$Text)

    return @(Get-ExpectedTokens | Where-Object { -not $Text.Contains($_) })
}

function Get-ForbiddenTokens {
    return @(
        "resident-elf-qemu: fail",
        "resident-elf-qemu: packetstream read name=packetstream_crc_mismatch",
        "resident-elf-qemu: packetstream app-stage name=packetstream_crc_mismatch",
        "resident-elf-qemu: app packetstream:packetstream_crc_mismatch"
    )
}

function Get-PresentForbiddenTokens {
    param([string]$Text)

    return @(Get-ForbiddenTokens | Where-Object { $Text.Contains($_) })
}

function Test-LogText {
    param([string]$Text)

    return (Get-MissingTokens -Text $Text).Count -eq 0 -and
           (Get-PresentForbiddenTokens -Text $Text).Count -eq 0
}

function Validate-LogFile {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "validate_log_failed: path is empty"
    }
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "validate_log_failed: log not found: $Path"
    }
    $ResolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $Text = Get-Content -LiteralPath $ResolvedPath -Raw -Encoding UTF8
    $Missing = Get-MissingTokens -Text $Text
    $Forbidden = Get-PresentForbiddenTokens -Text $Text
    if ($Missing.Count -ne 0 -or $Forbidden.Count -ne 0) {
        Write-Host "resident-elf-qemu log validation failed"
        Write-Host "  log=$ResolvedPath"
        foreach ($Token in $Missing) {
            Write-Host "  missing: $Token"
        }
        foreach ($Token in $Forbidden) {
            Write-Host "  forbidden: $Token"
        }
        return 1
    }
    Write-Host "resident-elf-qemu log validation ok"
    Write-Host "  log=$ResolvedPath"
    return 0
}

function Get-FrameSignaturesFromText {
    param([string]$Text)

    $PresentRegex = [System.Text.RegularExpressions.Regex]::new(
        'resident-elf-qemu: display present bytes=(\d+) checksum=(\d+) hash=(0x[0-9a-f]+) frame=(\d+)')
    $AppRegex = [System.Text.RegularExpressions.Regex]::new(
        'resident-elf-qemu: app (\S+) stage=(\S+) code=(\S+) exit=(\d+)')
    $Frames = @()
    $Runs = @()
    $PendingFrames = @()
    $Lines = $Text -split "`r?`n"
    foreach ($Line in $Lines) {
        $Present = $PresentRegex.Match($Line)
        if ($Present.Success) {
            $Frame = [pscustomobject]@{
                index = $Frames.Count + 1
                bytes = [int]$Present.Groups[1].Value
                checksum = [uint32]$Present.Groups[2].Value
                hash = $Present.Groups[3].Value
                frame = [int]$Present.Groups[4].Value
            }
            $Frames += $Frame
            $PendingFrames += $Frame
            continue
        }

        $App = $AppRegex.Match($Line)
        if ($App.Success -and $PendingFrames.Count -gt 0) {
            $RunFrames = @()
            foreach ($Pending in $PendingFrames) {
                $RunFrames += [pscustomobject]@{
                    index = $Pending.index
                    bytes = $Pending.bytes
                    checksum = $Pending.checksum
                    hash = $Pending.hash
                    frame = $Pending.frame
                }
            }
            $Runs += [pscustomobject]@{
                name = $App.Groups[1].Value
                stage = $App.Groups[2].Value
                code = $App.Groups[3].Value
                exit = [int]$App.Groups[4].Value
                frame_count = $RunFrames.Count
                frames = $RunFrames
            }
            $PendingFrames = @()
        }
    }

    return [pscustomobject]@{
        frames = @($Frames)
        runs = @($Runs)
    }
}

function Get-FrameDumpsFromText {
    param([string]$Text)

    $DumpRegex = [System.Text.RegularExpressions.Regex]::new(
        'resident-elf-qemu: display dump bytes=(\d+) checksum=(\d+) hash=(0x[0-9a-f]+) frame=(\d+) hex=([0-9a-f]+)')
    $AppRegex = [System.Text.RegularExpressions.Regex]::new(
        'resident-elf-qemu: app ([^ ]+) stage=([^ ]+) code=([^ ]+) exit=(\d+)')
    $Frames = @()
    $Runs = @()
    $PendingFrames = @()
    $Lines = $Text -split "`r?`n"
    foreach ($Line in $Lines) {
        $Dump = $DumpRegex.Match($Line)
        if ($Dump.Success) {
            $Frame = [pscustomobject]@{
                index = $Frames.Count + 1
                bytes = [int]$Dump.Groups[1].Value
                checksum = [uint32]$Dump.Groups[2].Value
                hash = $Dump.Groups[3].Value
                frame = [int]$Dump.Groups[4].Value
                hex = $Dump.Groups[5].Value
            }
            $Frames += $Frame
            $PendingFrames += $Frame
            continue
        }

        $App = $AppRegex.Match($Line)
        if ($App.Success -and $PendingFrames.Count -gt 0) {
            $RunFrames = @()
            foreach ($Pending in $PendingFrames) {
                $RunFrames += [pscustomobject]@{
                    index = $Pending.index
                    bytes = $Pending.bytes
                    checksum = $Pending.checksum
                    hash = $Pending.hash
                    frame = $Pending.frame
                    hex = $Pending.hex
                }
            }
            $Runs += [pscustomobject]@{
                name = $App.Groups[1].Value
                stage = $App.Groups[2].Value
                code = $App.Groups[3].Value
                exit = [int]$App.Groups[4].Value
                frame_count = $RunFrames.Count
                frames = $RunFrames
            }
            $PendingFrames = @()
        }
    }

    return [pscustomobject]@{
        frames = @($Frames)
        runs = @($Runs)
    }
}

function Get-InputTraceFromText {
    param([string]$Text)

    $InputRegex = [System.Text.RegularExpressions.Regex]::new(
        'resident-elf-qemu: input poll encoder1=([0-9]+) pointer=(\d+),(\d+) max=(\d+),(\d+) detected=(\d+) down=(\d+)')
    $AppRegex = [System.Text.RegularExpressions.Regex]::new(
        'resident-elf-qemu: app ([^ ]+) stage=([^ ]+) code=([^ ]+) exit=(\d+)')
    $Events = @()
    $Runs = @()
    $PendingEvents = @()
    $Lines = $Text -split "`r?`n"
    foreach ($Line in $Lines) {
        $Input = $InputRegex.Match($Line)
        if ($Input.Success) {
            $Encoder1Raw = [uint32]$Input.Groups[1].Value
            $Encoder1 = if ($Encoder1Raw -eq [uint32]::MaxValue) { -1 } else { [int]$Encoder1Raw }
            $Event = [pscustomobject]@{
                index = $Events.Count + 1
                encoder1 = $Encoder1
                pointer_x = [int]$Input.Groups[2].Value
                pointer_y = [int]$Input.Groups[3].Value
                pointer_max_x = [int]$Input.Groups[4].Value
                pointer_max_y = [int]$Input.Groups[5].Value
                detected = [int]$Input.Groups[6].Value
                down = [int]$Input.Groups[7].Value
            }
            $Events += $Event
            $PendingEvents += $Event
            continue
        }

        $App = $AppRegex.Match($Line)
        if ($App.Success -and $PendingEvents.Count -gt 0) {
            $AppName = $App.Groups[1].Value
            $RunEvents = @()
            foreach ($Pending in $PendingEvents) {
                $RunEvents += [pscustomobject]@{
                    index = $Pending.index
                    encoder1 = $Pending.encoder1
                    pointer_x = $Pending.pointer_x
                    pointer_y = $Pending.pointer_y
                    pointer_max_x = $Pending.pointer_max_x
                    pointer_max_y = $Pending.pointer_max_y
                    detected = $Pending.detected
                    down = $Pending.down
                }
            }
            $Runs += [pscustomobject]@{
                name = $AppName
                stage = $App.Groups[2].Value
                code = $App.Groups[3].Value
                exit = [int]$App.Groups[4].Value
                event_count = $RunEvents.Count
                events = $RunEvents
            }
            $PendingEvents = @()
        }
    }

    return [pscustomobject]@{
        events = @($Events)
        runs = @($Runs)
    }
}

function Get-StorageTraceFromText {
    param([string]$Text)

    $OpenRegex = [System.Text.RegularExpressions.Regex]::new(
        'resident-elf-qemu: storage open path=([^ ]+) code=([^ ]+) fd=(-?\d+)(?: size=(\d+))?')
    $ReadRegex = [System.Text.RegularExpressions.Regex]::new(
        'resident-elf-qemu: storage read fd=(-?\d+) code=([^ ]+) requested=(\d+) count=(\d+) offset=(\d+) remaining=(\d+)')
    $CloseRegex = [System.Text.RegularExpressions.Regex]::new(
        'resident-elf-qemu: storage close fd=(-?\d+) code=([^ ]+)')
    $AppRegex = [System.Text.RegularExpressions.Regex]::new(
        'resident-elf-qemu: app ([^ ]+) stage=([^ ]+) code=([^ ]+) exit=(\d+)')
    $Events = @()
    $Runs = @()
    $PendingEvents = @()
    $Lines = $Text -split "`r?`n"
    foreach ($Line in $Lines) {
        $Event = $null
        $Open = $OpenRegex.Match($Line)
        if ($Open.Success) {
            $SizeValue = if ($Open.Groups[4].Success) { [int]$Open.Groups[4].Value } else { 0 }
            $Event = [pscustomobject]@{
                index = $Events.Count + 1
                op = "open"
                path = $Open.Groups[1].Value
                fd = [int]$Open.Groups[3].Value
                code = $Open.Groups[2].Value
                size = $SizeValue
                requested = 0
                count = 0
                offset = 0
                remaining = 0
            }
        } else {
            $Read = $ReadRegex.Match($Line)
            if ($Read.Success) {
                $Event = [pscustomobject]@{
                    index = $Events.Count + 1
                    op = "read"
                    path = ""
                    fd = [int]$Read.Groups[1].Value
                    code = $Read.Groups[2].Value
                    size = 0
                    requested = [int]$Read.Groups[3].Value
                    count = [int]$Read.Groups[4].Value
                    offset = [int]$Read.Groups[5].Value
                    remaining = [int]$Read.Groups[6].Value
                }
            } else {
                $Close = $CloseRegex.Match($Line)
                if ($Close.Success) {
                    $Event = [pscustomobject]@{
                        index = $Events.Count + 1
                        op = "close"
                        path = ""
                        fd = [int]$Close.Groups[1].Value
                        code = $Close.Groups[2].Value
                        size = 0
                        requested = 0
                        count = 0
                        offset = 0
                        remaining = 0
                    }
                }
            }
        }
        if ($null -ne $Event) {
            $Events += $Event
            $PendingEvents += $Event
            continue
        }

        $App = $AppRegex.Match($Line)
        if ($App.Success -and $PendingEvents.Count -gt 0) {
            $AppName = $App.Groups[1].Value
            $StorageRunNames = @(
                "unsupported_caps_app",
                "store:unsupported_caps_app",
                "storage_app",
                "store:storage_app",
                "storage_catalog_app",
                "store:storage_catalog_app"
            )
            if (-not ($StorageRunNames -contains $AppName)) {
                $PendingEvents = @()
                continue
            }
            $RunEvents = @()
            foreach ($Pending in $PendingEvents) {
                $RunEvents += [pscustomobject]@{
                    index = $Pending.index
                    op = $Pending.op
                    path = $Pending.path
                    fd = $Pending.fd
                    code = $Pending.code
                    size = $Pending.size
                    requested = $Pending.requested
                    count = $Pending.count
                    offset = $Pending.offset
                    remaining = $Pending.remaining
                }
            }
            $Runs += [pscustomobject]@{
                name = $AppName
                stage = $App.Groups[2].Value
                code = $App.Groups[3].Value
                exit = [int]$App.Groups[4].Value
                event_count = $RunEvents.Count
                events = $RunEvents
            }
            $PendingEvents = @()
        }
    }

    return [pscustomobject]@{
        events = @($Events)
        runs = @($Runs)
    }
}

function Write-FrameSignatureCapture {
    param(
        [string]$LogPath,
        [string]$OutputPath
    )

    if ([string]::IsNullOrWhiteSpace($OutputPath)) {
        return
    }
    if (-not (Test-Path -LiteralPath $LogPath)) {
        throw "frame_signature_failed: log not found: $LogPath"
    }
    $ResolvedLog = (Resolve-Path -LiteralPath $LogPath).Path
    $ResolvedOut = Resolve-ScriptPath -Path $OutputPath
    $OutDir = [System.IO.Path]::GetDirectoryName($ResolvedOut)
    if (-not [string]::IsNullOrWhiteSpace($OutDir) -and -not (Test-Path -LiteralPath $OutDir)) {
        New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
    }

    $Text = Get-Content -LiteralPath $ResolvedLog -Raw -Encoding UTF8
    $Signatures = Get-FrameSignaturesFromText -Text $Text
    $Frames = @($Signatures.frames)
    $Runs = @($Signatures.runs)
    if ($Frames.Count -eq 0) {
        throw "frame_signature_failed: no display present signatures found in $ResolvedLog"
    }

    $Capture = [pscustomobject]@{
        schema = "charm.resident_elf_qemu.frame_signatures.v1"
        log = $ResolvedLog
        frame_count = $Frames.Count
        frames = $Frames
        run_count = $Runs.Count
        runs = $Runs
    }
    $Json = $Capture | ConvertTo-Json -Depth 8
    [System.IO.File]::WriteAllText($ResolvedOut, ($Json + "`n"), [System.Text.UTF8Encoding]::new($false))
    Write-Host "resident-elf-qemu frame signatures:"
    Write-Host "  path=$ResolvedOut"
    Write-Host "  frames=$($Frames.Count)"
    Write-Host "  runs=$($Runs.Count)"
}

function Write-FrameDumpCapture {
    param(
        [string]$LogPath,
        [string]$OutputPath
    )

    if ([string]::IsNullOrWhiteSpace($OutputPath)) {
        return
    }
    if (-not (Test-Path -LiteralPath $LogPath)) {
        throw "frame_dump_failed: log not found: $LogPath"
    }
    $ResolvedLog = (Resolve-Path -LiteralPath $LogPath).Path
    $ResolvedOut = Resolve-ScriptPath -Path $OutputPath
    $OutDir = [System.IO.Path]::GetDirectoryName($ResolvedOut)
    if (-not [string]::IsNullOrWhiteSpace($OutDir) -and -not (Test-Path -LiteralPath $OutDir)) {
        New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
    }

    $Text = Get-Content -LiteralPath $ResolvedLog -Raw -Encoding UTF8
    $Dumps = Get-FrameDumpsFromText -Text $Text
    $Frames = @($Dumps.frames)
    $Runs = @($Dumps.runs)
    if ($Frames.Count -eq 0) {
        throw "frame_dump_failed: no display frame dumps found in $ResolvedLog"
    }

    $Capture = [pscustomobject]@{
        schema = "charm.resident_elf_qemu.frame_dumps.v1"
        log = $ResolvedLog
        format = "argb8888"
        frame_count = $Frames.Count
        frames = $Frames
        run_count = $Runs.Count
        runs = $Runs
    }
    $Json = $Capture | ConvertTo-Json -Depth 8
    [System.IO.File]::WriteAllText($ResolvedOut, ($Json + "`n"), [System.Text.UTF8Encoding]::new($false))
    Write-Host "resident-elf-qemu frame dumps:"
    Write-Host "  path=$ResolvedOut"
    Write-Host "  frames=$($Frames.Count)"
    Write-Host "  runs=$($Runs.Count)"
}

function Write-InputTraceCapture {
    param(
        [string]$LogPath,
        [string]$OutputPath
    )

    if ([string]::IsNullOrWhiteSpace($OutputPath)) {
        return
    }
    if (-not (Test-Path -LiteralPath $LogPath)) {
        throw "input_trace_failed: log not found: $LogPath"
    }
    $ResolvedLog = (Resolve-Path -LiteralPath $LogPath).Path
    $ResolvedOut = Resolve-ScriptPath -Path $OutputPath
    $OutDir = [System.IO.Path]::GetDirectoryName($ResolvedOut)
    if (-not [string]::IsNullOrWhiteSpace($OutDir) -and -not (Test-Path -LiteralPath $OutDir)) {
        New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
    }

    $Text = Get-Content -LiteralPath $ResolvedLog -Raw -Encoding UTF8
    $Trace = Get-InputTraceFromText -Text $Text
    $Events = @($Trace.events)
    $Runs = @($Trace.runs)
    if ($Events.Count -eq 0) {
        throw "input_trace_failed: no input poll events found in $ResolvedLog"
    }

    $Capture = [pscustomobject]@{
        schema = "charm.resident_elf_qemu.input_trace.v1"
        log = $ResolvedLog
        event_count = $Events.Count
        events = $Events
        run_count = $Runs.Count
        runs = $Runs
    }
    $Json = $Capture | ConvertTo-Json -Depth 8
    [System.IO.File]::WriteAllText($ResolvedOut, ($Json + "`n"), [System.Text.UTF8Encoding]::new($false))
    Write-Host "resident-elf-qemu input trace:"
    Write-Host "  path=$ResolvedOut"
    Write-Host "  events=$($Events.Count)"
    Write-Host "  runs=$($Runs.Count)"
}

function Write-StorageTraceCapture {
    param(
        [string]$LogPath,
        [string]$OutputPath
    )

    if ([string]::IsNullOrWhiteSpace($OutputPath)) {
        return
    }
    if (-not (Test-Path -LiteralPath $LogPath)) {
        throw "storage_trace_failed: log not found: $LogPath"
    }
    $ResolvedLog = (Resolve-Path -LiteralPath $LogPath).Path
    $ResolvedOut = Resolve-ScriptPath -Path $OutputPath
    $OutDir = [System.IO.Path]::GetDirectoryName($ResolvedOut)
    if (-not [string]::IsNullOrWhiteSpace($OutDir) -and -not (Test-Path -LiteralPath $OutDir)) {
        New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
    }

    $Text = Get-Content -LiteralPath $ResolvedLog -Raw -Encoding UTF8
    $Trace = Get-StorageTraceFromText -Text $Text
    $Events = @($Trace.events)
    $Runs = @($Trace.runs)
    if ($Events.Count -eq 0) {
        throw "storage_trace_failed: no storage events found in $ResolvedLog"
    }

    $Capture = [pscustomobject]@{
        schema = "charm.resident_elf_qemu.storage_trace.v1"
        log = $ResolvedLog
        event_count = $Events.Count
        events = $Events
        run_count = $Runs.Count
        runs = $Runs
    }
    $Json = $Capture | ConvertTo-Json -Depth 8
    [System.IO.File]::WriteAllText($ResolvedOut, ($Json + "`n"), [System.Text.UTF8Encoding]::new($false))
    Write-Host "resident-elf-qemu storage trace:"
    Write-Host "  path=$ResolvedOut"
    Write-Host "  events=$($Events.Count)"
    Write-Host "  runs=$($Runs.Count)"
}

function Assert-FrameRun {
    param(
        [object[]]$Runs,
        [string]$Name,
        [int]$FrameCount,
        [string[]]$Hashes
    )

    $Matches = @($Runs | Where-Object { $_.name -eq $Name })
    if ($Matches.Count -ne 1) {
        throw "frame_signature_validate_failed: expected one run named $Name, got $($Matches.Count)"
    }
    $Run = $Matches[0]
    if ([int]$Run.frame_count -ne $FrameCount) {
        throw "frame_signature_validate_failed: run $Name frame_count=$($Run.frame_count), expected $FrameCount"
    }
    $Frames = @($Run.frames)
    if ($Frames.Count -ne $FrameCount) {
        throw "frame_signature_validate_failed: run $Name frames array count=$($Frames.Count), expected $FrameCount"
    }
    for ($i = 0; $i -lt $Hashes.Count; ++$i) {
        if ($Frames[$i].hash -ne $Hashes[$i]) {
            throw "frame_signature_validate_failed: run $Name frame $($i + 1) hash=$($Frames[$i].hash), expected $($Hashes[$i])"
        }
    }
}

function Validate-FrameSignatureFile {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "frame_signature_validate_failed: path is empty"
    }
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "frame_signature_validate_failed: file not found: $Path"
    }
    $ResolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $Capture = Get-Content -LiteralPath $ResolvedPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($Capture.schema -ne "charm.resident_elf_qemu.frame_signatures.v1") {
        throw "frame_signature_validate_failed: bad schema: $($Capture.schema)"
    }
    if ([int]$Capture.frame_count -ne 8) {
        throw "frame_signature_validate_failed: frame_count=$($Capture.frame_count), expected 8"
    }
    if ([int]$Capture.run_count -ne 6) {
        throw "frame_signature_validate_failed: run_count=$($Capture.run_count), expected 6"
    }
    $Frames = @($Capture.frames)
    if ($Frames.Count -ne 8) {
        throw "frame_signature_validate_failed: frames array count=$($Frames.Count), expected 8"
    }
    $Runs = @($Capture.runs)
    Assert-FrameRun -Runs $Runs -Name "display_sequence_app" -FrameCount 2 -Hashes @("0x373fb1c5", "0xa9b09dc5")
    Assert-FrameRun -Runs $Runs -Name "store:display_sequence_app" -FrameCount 2 -Hashes @("0x373fb1c5", "0xa9b09dc5")
    Assert-FrameRun -Runs $Runs -Name "player_min" -FrameCount 1 -Hashes @("0xfac53a05")
    Assert-FrameRun -Runs $Runs -Name "received:player_min" -FrameCount 1 -Hashes @("0xfac53a05")
    Assert-FrameRun -Runs $Runs -Name "packetstream:player_min" -FrameCount 1 -Hashes @("0xfac53a05")
    Assert-FrameRun -Runs $Runs -Name "store:player_min" -FrameCount 1 -Hashes @("0xfac53a05")
    Write-Host "resident-elf-qemu frame signature validation ok"
    Write-Host "  path=$ResolvedPath"
    return 0
}

function ConvertTo-CanonicalFrameSignature {
    param([object]$Capture)

    $Runs = @()
    foreach ($Run in @($Capture.runs)) {
        $Frames = @()
        foreach ($Frame in @($Run.frames)) {
            $Frames += [pscustomobject]@{
                bytes = [int]$Frame.bytes
                checksum = [uint32]$Frame.checksum
                hash = [string]$Frame.hash
                frame = [int]$Frame.frame
            }
        }
        $Runs += [pscustomobject]@{
            name = [string]$Run.name
            stage = [string]$Run.stage
            code = [string]$Run.code
            exit = [int]$Run.exit
            frame_count = [int]$Run.frame_count
            frames = $Frames
        }
    }
    return [pscustomobject]@{
        schema = "charm.resident_elf_qemu.frame_signatures.canonical.v1"
        frame_count = [int]$Capture.frame_count
        run_count = [int]$Capture.run_count
        runs = $Runs
    }
}

function Read-CanonicalFrameSignatureJson {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "frame_signature_compare_failed: path is empty"
    }
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "frame_signature_compare_failed: file not found: $Path"
    }
    $ResolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $Capture = Get-Content -LiteralPath $ResolvedPath -Raw -Encoding UTF8 | ConvertFrom-Json
    return (ConvertTo-CanonicalFrameSignature -Capture $Capture) | ConvertTo-Json -Depth 16 -Compress
}

function Compare-FrameSignatureFiles {
    param(
        [string]$ExpectedPath,
        [string]$ActualPath
    )

    if ([string]::IsNullOrWhiteSpace($ExpectedPath) -or [string]::IsNullOrWhiteSpace($ActualPath)) {
        throw "frame_signature_compare_failed: expected and actual paths are required"
    }
    [void](Validate-FrameSignatureFile -Path $ExpectedPath)
    [void](Validate-FrameSignatureFile -Path $ActualPath)
    $ExpectedJson = Read-CanonicalFrameSignatureJson -Path $ExpectedPath
    $ActualJson = Read-CanonicalFrameSignatureJson -Path $ActualPath
    if ($ExpectedJson -ne $ActualJson) {
        Write-Host "resident-elf-qemu frame signature comparison failed"
        Write-Host "  expected=$ExpectedPath"
        Write-Host "  actual=$ActualPath"
        return 1
    }
    Write-Host "resident-elf-qemu frame signature comparison ok"
    Write-Host "  expected=$ExpectedPath"
    Write-Host "  actual=$ActualPath"
    return 0
}

function Assert-InputTraceRun {
    param(
        [object[]]$Runs,
        [string]$Name,
        [int[]]$Encoder1,
        [int[]]$PointerX,
        [int[]]$PointerY,
        [int[]]$Down
    )

    $Matches = @($Runs | Where-Object { $_.name -eq $Name })
    if ($Matches.Count -ne 1) {
        throw "input_trace_validate_failed: expected one run named $Name, got $($Matches.Count)"
    }
    $Run = $Matches[0]
    $Events = @($Run.events)
    if ([int]$Run.event_count -ne $Encoder1.Count -or $Events.Count -ne $Encoder1.Count) {
        throw "input_trace_validate_failed: run $Name event_count=$($Run.event_count), expected $($Encoder1.Count)"
    }
    for ($i = 0; $i -lt $Encoder1.Count; ++$i) {
        $Event = $Events[$i]
        if ([int]$Event.encoder1 -ne $Encoder1[$i] -or
            [int]$Event.pointer_x -ne $PointerX[$i] -or
            [int]$Event.pointer_y -ne $PointerY[$i] -or
            [int]$Event.pointer_max_x -ne 15 -or
            [int]$Event.pointer_max_y -ne 15 -or
            [int]$Event.detected -ne 1 -or
            [int]$Event.down -ne $Down[$i]) {
            throw "input_trace_validate_failed: run $Name event $($i + 1) mismatch"
        }
    }
}

function Validate-InputTraceFile {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "input_trace_validate_failed: path is empty"
    }
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "input_trace_validate_failed: file not found: $Path"
    }
    $ResolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $Capture = Get-Content -LiteralPath $ResolvedPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($Capture.schema -ne "charm.resident_elf_qemu.input_trace.v1") {
        throw "input_trace_validate_failed: bad schema: $($Capture.schema)"
    }
    if ([int]$Capture.event_count -ne 12 -or [int]$Capture.run_count -ne 6) {
        throw "input_trace_validate_failed: event/run count mismatch"
    }
    $Events = @($Capture.events)
    if ($Events.Count -ne 12) {
        throw "input_trace_validate_failed: events array count=$($Events.Count), expected 12"
    }
    $Runs = @($Capture.runs)
    $SeqEncoder1 = @(1, 0, -1, 0)
    $SeqX = @(3, 4, 5, 6)
    $SeqY = @(5, 6, 7, 8)
    $SeqDown = @(0, 1, 1, 0)
    Assert-InputTraceRun -Runs $Runs -Name "input_sequence_app" -Encoder1 $SeqEncoder1 -PointerX $SeqX -PointerY $SeqY -Down $SeqDown
    Assert-InputTraceRun -Runs $Runs -Name "store:input_sequence_app" -Encoder1 $SeqEncoder1 -PointerX $SeqX -PointerY $SeqY -Down $SeqDown
    foreach ($Name in @("player_min", "received:player_min", "packetstream:player_min", "store:player_min")) {
        Assert-InputTraceRun -Runs $Runs -Name $Name -Encoder1 @(1) -PointerX @(3) -PointerY @(5) -Down @(0)
    }
    Write-Host "resident-elf-qemu input trace validation ok"
    Write-Host "  path=$ResolvedPath"
    return 0
}

function ConvertTo-CanonicalInputTrace {
    param([object]$Capture)

    $Runs = @()
    foreach ($Run in @($Capture.runs)) {
        $Events = @()
        foreach ($Event in @($Run.events)) {
            $Events += [pscustomobject]@{
                encoder1 = [int]$Event.encoder1
                pointer_x = [int]$Event.pointer_x
                pointer_y = [int]$Event.pointer_y
                pointer_max_x = [int]$Event.pointer_max_x
                pointer_max_y = [int]$Event.pointer_max_y
                detected = [int]$Event.detected
                down = [int]$Event.down
            }
        }
        $Runs += [pscustomobject]@{
            name = [string]$Run.name
            stage = [string]$Run.stage
            code = [string]$Run.code
            exit = [int]$Run.exit
            event_count = [int]$Run.event_count
            events = $Events
        }
    }
    return [pscustomobject]@{
        schema = "charm.resident_elf_qemu.input_trace.canonical.v1"
        event_count = [int]$Capture.event_count
        run_count = [int]$Capture.run_count
        runs = $Runs
    }
}

function Read-CanonicalInputTraceJson {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "input_trace_compare_failed: path is empty"
    }
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "input_trace_compare_failed: file not found: $Path"
    }
    $ResolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $Capture = Get-Content -LiteralPath $ResolvedPath -Raw -Encoding UTF8 | ConvertFrom-Json
    return (ConvertTo-CanonicalInputTrace -Capture $Capture) | ConvertTo-Json -Depth 16 -Compress
}

function Compare-InputTraceFiles {
    param(
        [string]$ExpectedPath,
        [string]$ActualPath
    )

    if ([string]::IsNullOrWhiteSpace($ExpectedPath) -or [string]::IsNullOrWhiteSpace($ActualPath)) {
        throw "input_trace_compare_failed: expected and actual paths are required"
    }
    [void](Validate-InputTraceFile -Path $ExpectedPath)
    [void](Validate-InputTraceFile -Path $ActualPath)
    $ExpectedJson = Read-CanonicalInputTraceJson -Path $ExpectedPath
    $ActualJson = Read-CanonicalInputTraceJson -Path $ActualPath
    if ($ExpectedJson -ne $ActualJson) {
        Write-Host "resident-elf-qemu input trace comparison failed"
        Write-Host "  expected=$ExpectedPath"
        Write-Host "  actual=$ActualPath"
        return 1
    }
    Write-Host "resident-elf-qemu input trace comparison ok"
    Write-Host "  expected=$ExpectedPath"
    Write-Host "  actual=$ActualPath"
    return 0
}

function Assert-StorageTraceRun {
    param(
        [object[]]$Runs,
        [string]$Name,
        [string[]]$Ops,
        [string[]]$Paths,
        [int[]]$Fds,
        [int[]]$Counts
    )

    $Matches = @($Runs | Where-Object { $_.name -eq $Name })
    if ($Matches.Count -ne 1) {
        throw "storage_trace_validate_failed: expected one run named $Name, got $($Matches.Count)"
    }
    $Run = $Matches[0]
    $Events = @($Run.events)
    if ([int]$Run.event_count -ne $Ops.Count -or $Events.Count -ne $Ops.Count) {
        throw "storage_trace_validate_failed: run $Name event_count=$($Run.event_count), expected $($Ops.Count)"
    }
    for ($i = 0; $i -lt $Ops.Count; ++$i) {
        $Event = $Events[$i]
        if ([string]$Event.op -ne $Ops[$i] -or
            [int]$Event.fd -ne $Fds[$i] -or
            [int]$Event.count -ne $Counts[$i] -or
            [string]$Event.code -ne "ok") {
            throw "storage_trace_validate_failed: run $Name event $($i + 1) mismatch"
        }
        if (-not [string]::IsNullOrWhiteSpace($Paths[$i]) -and [string]$Event.path -ne $Paths[$i]) {
            throw "storage_trace_validate_failed: run $Name event $($i + 1) path=$($Event.path), expected $($Paths[$i])"
        }
    }
}

function Validate-StorageTraceFile {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "storage_trace_validate_failed: path is empty"
    }
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "storage_trace_validate_failed: file not found: $Path"
    }
    $ResolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $Capture = Get-Content -LiteralPath $ResolvedPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($Capture.schema -ne "charm.resident_elf_qemu.storage_trace.v1") {
        throw "storage_trace_validate_failed: bad schema: $($Capture.schema)"
    }
    if ([int]$Capture.event_count -ne 49 -or [int]$Capture.run_count -ne 6) {
        throw "storage_trace_validate_failed: event/run count mismatch"
    }
    $Events = @($Capture.events)
    if ($Events.Count -ne 49) {
        throw "storage_trace_validate_failed: events array count=$($Events.Count), expected 49"
    }
    $Runs = @($Capture.runs)
    $ReadmeOps = @("open", "read", "read", "read", "read", "read", "close")
    $ReadmePaths = @("/virtual/readme.txt", "", "", "", "", "", "")
    $ReadmeFds = @(3, 3, 3, 3, 3, 3, 3)
    $ReadmeCounts = @(0, 8, 8, 8, 3, 0, 0)
    Assert-StorageTraceRun -Runs $Runs -Name "storage_app" -Ops $ReadmeOps -Paths $ReadmePaths -Fds $ReadmeFds -Counts $ReadmeCounts
    Assert-StorageTraceRun -Runs $Runs -Name "store:storage_app" -Ops $ReadmeOps -Paths $ReadmePaths -Fds $ReadmeFds -Counts $ReadmeCounts

    $CatalogOps = @("open", "open", "read", "read", "read", "read", "read", "read", "read", "read", "read", "close", "close")
    $CatalogPaths = @("/virtual/alpha.txt", "/virtual/beta.bin", "", "", "", "", "", "", "", "", "", "", "")
    $CatalogFds = @(3, 4, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 3)
    $CatalogCounts = @(0, 0, 5, 5, 5, 0, 5, 5, 5, 1, 0, 0, 0)
    Assert-StorageTraceRun -Runs $Runs -Name "storage_catalog_app" -Ops $CatalogOps -Paths $CatalogPaths -Fds $CatalogFds -Counts $CatalogCounts
    Assert-StorageTraceRun -Runs $Runs -Name "store:storage_catalog_app" -Ops $CatalogOps -Paths $CatalogPaths -Fds $CatalogFds -Counts $CatalogCounts

    foreach ($Name in @("unsupported_caps_app", "store:unsupported_caps_app")) {
        $Matches = @($Runs | Where-Object { $_.name -eq $Name })
        if ($Matches.Count -ne 1 -or [int]$Matches[0].event_count -ne 3) {
            throw "storage_trace_validate_failed: unsupported storage run missing $Name"
        }
    }
    Write-Host "resident-elf-qemu storage trace validation ok"
    Write-Host "  path=$ResolvedPath"
    return 0
}

function ConvertTo-CanonicalStorageTrace {
    param([object]$Capture)

    $Runs = @()
    foreach ($Run in @($Capture.runs)) {
        $Events = @()
        foreach ($Event in @($Run.events)) {
            $Events += [pscustomobject]@{
                op = [string]$Event.op
                path = [string]$Event.path
                fd = [int]$Event.fd
                code = [string]$Event.code
                size = [int]$Event.size
                requested = [int]$Event.requested
                count = [int]$Event.count
                offset = [int]$Event.offset
                remaining = [int]$Event.remaining
            }
        }
        $Runs += [pscustomobject]@{
            name = [string]$Run.name
            stage = [string]$Run.stage
            code = [string]$Run.code
            exit = [int]$Run.exit
            event_count = [int]$Run.event_count
            events = $Events
        }
    }
    return [pscustomobject]@{
        schema = "charm.resident_elf_qemu.storage_trace.canonical.v1"
        event_count = [int]$Capture.event_count
        run_count = [int]$Capture.run_count
        runs = $Runs
    }
}

function Read-CanonicalStorageTraceJson {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "storage_trace_compare_failed: path is empty"
    }
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "storage_trace_compare_failed: file not found: $Path"
    }
    $ResolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $Capture = Get-Content -LiteralPath $ResolvedPath -Raw -Encoding UTF8 | ConvertFrom-Json
    return (ConvertTo-CanonicalStorageTrace -Capture $Capture) | ConvertTo-Json -Depth 16 -Compress
}

function Compare-StorageTraceFiles {
    param(
        [string]$ExpectedPath,
        [string]$ActualPath
    )

    if ([string]::IsNullOrWhiteSpace($ExpectedPath) -or [string]::IsNullOrWhiteSpace($ActualPath)) {
        throw "storage_trace_compare_failed: expected and actual paths are required"
    }
    [void](Validate-StorageTraceFile -Path $ExpectedPath)
    [void](Validate-StorageTraceFile -Path $ActualPath)
    $ExpectedJson = Read-CanonicalStorageTraceJson -Path $ExpectedPath
    $ActualJson = Read-CanonicalStorageTraceJson -Path $ActualPath
    if ($ExpectedJson -ne $ActualJson) {
        Write-Host "resident-elf-qemu storage trace comparison failed"
        Write-Host "  expected=$ExpectedPath"
        Write-Host "  actual=$ActualPath"
        return 1
    }
    Write-Host "resident-elf-qemu storage trace comparison ok"
    Write-Host "  expected=$ExpectedPath"
    Write-Host "  actual=$ActualPath"
    return 0
}

function Assert-FrameDumpRun {
    param(
        [object[]]$Runs,
        [string]$Name,
        [int]$FrameCount,
        [string[]]$Hashes
    )

    $Matches = @($Runs | Where-Object { $_.name -eq $Name })
    if ($Matches.Count -ne 1) {
        throw "frame_dump_validate_failed: expected one run named $Name, got $($Matches.Count)"
    }
    $Run = $Matches[0]
    if ([int]$Run.frame_count -ne $FrameCount) {
        throw "frame_dump_validate_failed: run $Name frame_count=$($Run.frame_count), expected $FrameCount"
    }
    $Frames = @($Run.frames)
    if ($Frames.Count -ne $FrameCount) {
        throw "frame_dump_validate_failed: run $Name frames array count=$($Frames.Count), expected $FrameCount"
    }
    for ($i = 0; $i -lt $Frames.Count; ++$i) {
        $Frame = $Frames[$i]
        if ($Frame.hash -ne $Hashes[$i]) {
            throw "frame_dump_validate_failed: run $Name frame $($i + 1) hash=$($Frame.hash), expected $($Hashes[$i])"
        }
        if ([int]$Frame.bytes -ne 1024) {
            throw "frame_dump_validate_failed: run $Name frame $($i + 1) bytes=$($Frame.bytes), expected 1024"
        }
        if ($Frame.hex.Length -ne 2048) {
            throw "frame_dump_validate_failed: run $Name frame $($i + 1) hex length=$($Frame.hex.Length), expected 2048"
        }
    }
}

function Validate-FrameDumpFile {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "frame_dump_validate_failed: path is empty"
    }
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "frame_dump_validate_failed: file not found: $Path"
    }
    $ResolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $Capture = Get-Content -LiteralPath $ResolvedPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($Capture.schema -ne "charm.resident_elf_qemu.frame_dumps.v1") {
        throw "frame_dump_validate_failed: bad schema: $($Capture.schema)"
    }
    if ($Capture.format -ne "argb8888") {
        throw "frame_dump_validate_failed: bad format: $($Capture.format)"
    }
    if ([int]$Capture.frame_count -ne 8) {
        throw "frame_dump_validate_failed: frame_count=$($Capture.frame_count), expected 8"
    }
    if ([int]$Capture.run_count -ne 6) {
        throw "frame_dump_validate_failed: run_count=$($Capture.run_count), expected 6"
    }
    $Frames = @($Capture.frames)
    if ($Frames.Count -ne 8) {
        throw "frame_dump_validate_failed: frames array count=$($Frames.Count), expected 8"
    }
    foreach ($Frame in $Frames) {
        if ([int]$Frame.bytes -ne 1024) {
            throw "frame_dump_validate_failed: global frame $($Frame.index) bytes=$($Frame.bytes), expected 1024"
        }
        if ($Frame.hex.Length -ne 2048) {
            throw "frame_dump_validate_failed: global frame $($Frame.index) hex length=$($Frame.hex.Length), expected 2048"
        }
    }
    $Runs = @($Capture.runs)
    Assert-FrameDumpRun -Runs $Runs -Name "display_sequence_app" -FrameCount 2 -Hashes @("0x373fb1c5", "0xa9b09dc5")
    Assert-FrameDumpRun -Runs $Runs -Name "store:display_sequence_app" -FrameCount 2 -Hashes @("0x373fb1c5", "0xa9b09dc5")
    Assert-FrameDumpRun -Runs $Runs -Name "player_min" -FrameCount 1 -Hashes @("0xfac53a05")
    Assert-FrameDumpRun -Runs $Runs -Name "received:player_min" -FrameCount 1 -Hashes @("0xfac53a05")
    Assert-FrameDumpRun -Runs $Runs -Name "packetstream:player_min" -FrameCount 1 -Hashes @("0xfac53a05")
    Assert-FrameDumpRun -Runs $Runs -Name "store:player_min" -FrameCount 1 -Hashes @("0xfac53a05")
    Write-Host "resident-elf-qemu frame dump validation ok"
    Write-Host "  path=$ResolvedPath"
    return 0
}

function Convert-HexFrameToBytes {
    param([string]$Hex)

    if ([string]::IsNullOrWhiteSpace($Hex) -or ($Hex.Length % 2) -ne 0) {
        throw "frame_ppm_failed: invalid frame hex length"
    }
    $Bytes = New-Object byte[] ($Hex.Length / 2)
    for ($i = 0; $i -lt $Bytes.Length; ++$i) {
        $Bytes[$i] = [Convert]::ToByte($Hex.Substring($i * 2, 2), 16)
    }
    return $Bytes
}

function Convert-RunNameToFileStem {
    param([string]$Name)

    $Stem = $Name -replace '[^A-Za-z0-9._-]', '_'
    if ([string]::IsNullOrWhiteSpace($Stem)) {
        return "run"
    }
    return $Stem
}

function Write-FramePpmFile {
    param(
        [object]$Frame,
        [string]$OutputPath
    )

    $Argb = Convert-HexFrameToBytes -Hex ([string]$Frame.hex)
    if ($Argb.Length -ne 1024) {
        throw "frame_ppm_failed: frame $($Frame.index) bytes=$($Argb.Length), expected 1024"
    }
    $Rgb = New-Object byte[] (16 * 16 * 3)
    $Out = 0
    for ($i = 0; $i -lt $Argb.Length; $i += 4) {
        $Rgb[$Out++] = $Argb[$i + 2]
        $Rgb[$Out++] = $Argb[$i + 1]
        $Rgb[$Out++] = $Argb[$i]
    }

    $Header = [System.Text.Encoding]::ASCII.GetBytes("P6`n16 16`n255`n")
    $Bytes = New-Object byte[] ($Header.Length + $Rgb.Length)
    [Array]::Copy($Header, 0, $Bytes, 0, $Header.Length)
    [Array]::Copy($Rgb, 0, $Bytes, $Header.Length, $Rgb.Length)
    [System.IO.File]::WriteAllBytes($OutputPath, $Bytes)
}

function Write-FramePpmCapture {
    param(
        [string]$FrameDumpPath,
        [string]$OutputDir
    )

    if ([string]::IsNullOrWhiteSpace($OutputDir)) {
        return
    }
    if ([string]::IsNullOrWhiteSpace($FrameDumpPath) -or -not (Test-Path -LiteralPath $FrameDumpPath)) {
        throw "frame_ppm_failed: frame dump not found: $FrameDumpPath"
    }
    $ResolvedDump = (Resolve-Path -LiteralPath $FrameDumpPath).Path
    $ResolvedOut = Resolve-ScriptPath -Path $OutputDir
    if (Test-Path -LiteralPath $ResolvedOut) {
        Remove-Item -LiteralPath $ResolvedOut -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $ResolvedOut | Out-Null

    $Capture = Get-Content -LiteralPath $ResolvedDump -Raw -Encoding UTF8 | ConvertFrom-Json
    $ManifestFrames = @()
    foreach ($Run in @($Capture.runs)) {
        $RunStem = Convert-RunNameToFileStem -Name ([string]$Run.name)
        $Index = 0
        foreach ($Frame in @($Run.frames)) {
            ++$Index
            $FileName = "{0}_{1:00}_frame{2:00}_{3}.ppm" -f $RunStem, $Index, ([int]$Frame.frame), ([string]$Frame.hash).Substring(2)
            $OutputPath = Join-Path $ResolvedOut $FileName
            Write-FramePpmFile -Frame $Frame -OutputPath $OutputPath
            $ManifestFrames += [pscustomobject]@{
                run = [string]$Run.name
                frame = [int]$Frame.frame
                index = [int]$Frame.index
                bytes = [int]$Frame.bytes
                checksum = [uint32]$Frame.checksum
                hash = [string]$Frame.hash
                width = 16
                height = 16
                format = "ppm_rgb888"
                path = $FileName
            }
        }
    }

    $Manifest = [pscustomobject]@{
        schema = "charm.resident_elf_qemu.frame_ppm.v1"
        source = $ResolvedDump
        width = 16
        height = 16
        format = "ppm_rgb888"
        frame_count = $ManifestFrames.Count
        run_count = @($Capture.runs).Count
        frames = $ManifestFrames
    }
    $ManifestPath = Join-Path $ResolvedOut "manifest.json"
    [System.IO.File]::WriteAllText($ManifestPath, (($Manifest | ConvertTo-Json -Depth 8) + "`n"), [System.Text.UTF8Encoding]::new($false))
    Write-Host "resident-elf-qemu frame ppm:"
    Write-Host "  path=$ResolvedOut"
    Write-Host "  frames=$($ManifestFrames.Count)"
    Write-Host "  runs=$(@($Capture.runs).Count)"
}

function Assert-FramePpmRun {
    param(
        [object[]]$Frames,
        [string]$Name,
        [int]$FrameCount,
        [string[]]$Hashes
    )

    $Matches = @($Frames | Where-Object { $_.run -eq $Name })
    if ($Matches.Count -ne $FrameCount) {
        throw "frame_ppm_validate_failed: run $Name frame_count=$($Matches.Count), expected $FrameCount"
    }
    for ($i = 0; $i -lt $Hashes.Count; ++$i) {
        if ($Matches[$i].hash -ne $Hashes[$i]) {
            throw "frame_ppm_validate_failed: run $Name frame $($i + 1) hash=$($Matches[$i].hash), expected $($Hashes[$i])"
        }
    }
}

function Validate-FramePpmDirectory {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "frame_ppm_validate_failed: path is empty"
    }
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "frame_ppm_validate_failed: directory not found: $Path"
    }
    $ResolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $ManifestPath = Join-Path $ResolvedPath "manifest.json"
    if (-not (Test-Path -LiteralPath $ManifestPath)) {
        throw "frame_ppm_validate_failed: manifest not found: $ManifestPath"
    }
    $Manifest = Get-Content -LiteralPath $ManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($Manifest.schema -ne "charm.resident_elf_qemu.frame_ppm.v1") {
        throw "frame_ppm_validate_failed: bad schema: $($Manifest.schema)"
    }
    if ([int]$Manifest.width -ne 16 -or [int]$Manifest.height -ne 16 -or $Manifest.format -ne "ppm_rgb888") {
        throw "frame_ppm_validate_failed: bad image metadata"
    }
    if ([int]$Manifest.frame_count -ne 8 -or [int]$Manifest.run_count -ne 6) {
        throw "frame_ppm_validate_failed: frame/run count mismatch"
    }
    $Frames = @($Manifest.frames)
    if ($Frames.Count -ne 8) {
        throw "frame_ppm_validate_failed: manifest frames count=$($Frames.Count), expected 8"
    }
    foreach ($Frame in $Frames) {
        $FramePath = Join-Path $ResolvedPath ([string]$Frame.path)
        if (-not (Test-Path -LiteralPath $FramePath)) {
            throw "frame_ppm_validate_failed: frame file missing: $FramePath"
        }
        $Bytes = [System.IO.File]::ReadAllBytes($FramePath)
        $ExpectedHeader = "P6`n16 16`n255`n"
        $ExpectedHeaderBytes = [System.Text.Encoding]::ASCII.GetBytes($ExpectedHeader)
        if ($Bytes.Length -ne ($ExpectedHeaderBytes.Length + 16 * 16 * 3)) {
            throw "frame_ppm_validate_failed: frame file $FramePath length=$($Bytes.Length), expected $($ExpectedHeaderBytes.Length + 16 * 16 * 3)"
        }
        $Header = [System.Text.Encoding]::ASCII.GetString($Bytes, 0, $ExpectedHeaderBytes.Length)
        if ($Header -ne $ExpectedHeader) {
            throw "frame_ppm_validate_failed: frame file $FramePath bad ppm header"
        }
    }
    Assert-FramePpmRun -Frames $Frames -Name "display_sequence_app" -FrameCount 2 -Hashes @("0x373fb1c5", "0xa9b09dc5")
    Assert-FramePpmRun -Frames $Frames -Name "store:display_sequence_app" -FrameCount 2 -Hashes @("0x373fb1c5", "0xa9b09dc5")
    Assert-FramePpmRun -Frames $Frames -Name "player_min" -FrameCount 1 -Hashes @("0xfac53a05")
    Assert-FramePpmRun -Frames $Frames -Name "received:player_min" -FrameCount 1 -Hashes @("0xfac53a05")
    Assert-FramePpmRun -Frames $Frames -Name "packetstream:player_min" -FrameCount 1 -Hashes @("0xfac53a05")
    Assert-FramePpmRun -Frames $Frames -Name "store:player_min" -FrameCount 1 -Hashes @("0xfac53a05")
    Write-Host "resident-elf-qemu frame ppm validation ok"
    Write-Host "  path=$ResolvedPath"
    return 0
}

function ConvertTo-CanonicalFrameDump {
    param([object]$Capture)

    $Runs = @()
    foreach ($Run in @($Capture.runs)) {
        $Frames = @()
        foreach ($Frame in @($Run.frames)) {
            $Frames += [pscustomobject]@{
                bytes = [int]$Frame.bytes
                checksum = [uint32]$Frame.checksum
                hash = [string]$Frame.hash
                frame = [int]$Frame.frame
                hex = [string]$Frame.hex
            }
        }
        $Runs += [pscustomobject]@{
            name = [string]$Run.name
            frame_count = [int]$Run.frame_count
            frames = $Frames
        }
    }
    return [pscustomobject]@{
        schema = "charm.resident_elf_qemu.frame_dumps.canonical.v1"
        format = [string]$Capture.format
        frame_count = [int]$Capture.frame_count
        run_count = [int]$Capture.run_count
        runs = $Runs
    }
}

function Read-CanonicalFrameDumpJson {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "frame_dump_compare_failed: path is empty"
    }
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "frame_dump_compare_failed: file not found: $Path"
    }
    $ResolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $Capture = Get-Content -LiteralPath $ResolvedPath -Raw -Encoding UTF8 | ConvertFrom-Json
    return (ConvertTo-CanonicalFrameDump -Capture $Capture) | ConvertTo-Json -Depth 16 -Compress
}

function Compare-FrameDumpFiles {
    param(
        [string]$ExpectedPath,
        [string]$ActualPath
    )

    if ([string]::IsNullOrWhiteSpace($ExpectedPath) -or [string]::IsNullOrWhiteSpace($ActualPath)) {
        throw "frame_dump_compare_failed: expected and actual paths are required"
    }
    [void](Validate-FrameDumpFile -Path $ExpectedPath)
    [void](Validate-FrameDumpFile -Path $ActualPath)
    $ExpectedJson = Read-CanonicalFrameDumpJson -Path $ExpectedPath
    $ActualJson = Read-CanonicalFrameDumpJson -Path $ActualPath
    if ($ExpectedJson -ne $ActualJson) {
        Write-Host "resident-elf-qemu frame dump comparison failed"
        Write-Host "  expected=$ExpectedPath"
        Write-Host "  actual=$ActualPath"
        return 1
    }
    Write-Host "resident-elf-qemu frame dump comparison ok"
    Write-Host "  expected=$ExpectedPath"
    Write-Host "  actual=$ActualPath"
    return 0
}

function Get-RegexGroupValue {
    param(
        [string]$Text,
        [string]$Pattern,
        [int]$Group = 1,
        [string]$ErrorPrefix
    )

    $Match = [System.Text.RegularExpressions.Regex]::Match($Text, $Pattern)
    if (-not $Match.Success) {
        throw "$ErrorPrefix`: missing pattern: $Pattern"
    }
    return $Match.Groups[$Group].Value
}

function Convert-HexToInt64 {
    param([string]$Value)

    $Hex = $Value
    if ($Hex.StartsWith("0x", [System.StringComparison]::OrdinalIgnoreCase)) {
        $Hex = $Hex.Substring(2)
    }
    return [System.Convert]::ToInt64($Hex, 16)
}

function Get-AppRunSummaryFromText {
    param([string]$Text)

    $RunRegex = [System.Text.RegularExpressions.Regex]::new(
        'resident-elf-qemu: app ([^ ]+) stage=([^ ]+) code=([^ ]+) exit=(\d+)')
    $Runs = @()
    foreach ($Match in $RunRegex.Matches($Text)) {
        $Runs += [pscustomobject]@{
            name = $Match.Groups[1].Value
            stage = $Match.Groups[2].Value
            code = $Match.Groups[3].Value
            exit = [int]$Match.Groups[4].Value
        }
    }
    return @($Runs)
}

function Get-StageSummaryFromText {
    param([string]$Text)

    $StageRegex = [System.Text.RegularExpressions.Regex]::new(
        'resident-elf-qemu: (received|store) stage name=(\S+) code=(\S+)(?: expected=(\S+))?')
    $Stages = @()
    foreach ($Match in $StageRegex.Matches($Text)) {
        $Stages += [pscustomobject]@{
            source = $Match.Groups[1].Value
            name = $Match.Groups[2].Value
            code = $Match.Groups[3].Value
            expected = $Match.Groups[4].Value
        }
    }
    return @($Stages)
}

function Get-ElfLoadSummaryFromText {
    param([string]$Text)

    $LoadRegex = [System.Text.RegularExpressions.Regex]::new(
        'resident-elf-qemu: load (\S+) format=(\S+) probe=(\S+) entry=(0x[0-9a-f]+) span=(\d+) segments=(\d+)')
    $CapacityRegex = [System.Text.RegularExpressions.Regex]::new(
        'resident-elf-qemu: capacity (\S+) needed=(\d+) free=(\d+) fits=(\d+) region=(\d+) probe=(\S+)')
    $CapacityByName = @{}
    foreach ($Match in $CapacityRegex.Matches($Text)) {
        $CapacityByName[$Match.Groups[1].Value] = [pscustomobject]@{
            needed = [int]$Match.Groups[2].Value
            free = [int]$Match.Groups[3].Value
            fits = ([int]$Match.Groups[4].Value) -ne 0
            region = [int]$Match.Groups[5].Value
            probe = $Match.Groups[6].Value
        }
    }

    $Loads = @()
    foreach ($Match in $LoadRegex.Matches($Text)) {
        $Name = $Match.Groups[1].Value
        $Capacity = $CapacityByName[$Name]
        $Loads += [pscustomobject]@{
            name = $Name
            format = $Match.Groups[2].Value
            probe = $Match.Groups[3].Value
            entry = $Match.Groups[4].Value
            entry_numeric = (Convert-HexToInt64 -Value $Match.Groups[4].Value)
            span = [int]$Match.Groups[5].Value
            segments = [int]$Match.Groups[6].Value
            needed = if ($null -ne $Capacity) { [int]$Capacity.needed } else { $null }
            free = if ($null -ne $Capacity) { [int]$Capacity.free } else { $null }
            fits = if ($null -ne $Capacity) { [bool]$Capacity.fits } else { $null }
            region = if ($null -ne $Capacity) { [int]$Capacity.region } else { $null }
            capacity_probe = if ($null -ne $Capacity) { [string]$Capacity.probe } else { "" }
        }
    }
    return @($Loads)
}

function Get-PacketstreamSummaryFromText {
    param([string]$Text)

    $PacketRegex = [System.Text.RegularExpressions.Regex]::new(
        'resident-elf-qemu: packetstream stage name=(\S+) transport=(\S+) packet=(\S+) stage=(\S+) code=(\S+) payload=(\d+) stream=(\d+) packets=(\d+) dispatch=(\d+) crc=(0x[0-9a-f]+)/(0x[0-9a-f]+)')
    $ReadRegex = [System.Text.RegularExpressions.Regex]::new(
        'resident-elf-qemu: packetstream read name=(\S+) code=(\S+) bytes=(\d+)')
    $StageRegex = [System.Text.RegularExpressions.Regex]::new(
        'resident-elf-qemu: packetstream app-stage name=(\S+) code=(\S+) format=(\S+) bytes=(\d+)')
    $ReadByName = @{}
    foreach ($Match in $ReadRegex.Matches($Text)) {
        $ReadByName[$Match.Groups[1].Value] = [pscustomobject]@{
            code = $Match.Groups[2].Value
            bytes = [int]$Match.Groups[3].Value
        }
    }
    $StageByName = @{}
    foreach ($Match in $StageRegex.Matches($Text)) {
        $StageByName[$Match.Groups[1].Value] = [pscustomobject]@{
            code = $Match.Groups[2].Value
            format = $Match.Groups[3].Value
            bytes = [int]$Match.Groups[4].Value
        }
    }

    $Packetstreams = @()
    foreach ($Match in $PacketRegex.Matches($Text)) {
        $Name = $Match.Groups[1].Value
        $Read = $ReadByName[$Name]
        $Stage = $StageByName[$Name]
        $Packetstreams += [pscustomobject]@{
            name = $Name
            transport = $Match.Groups[2].Value
            packet = $Match.Groups[3].Value
            receive_stage = $Match.Groups[4].Value
            receive_code = $Match.Groups[5].Value
            payload = [int]$Match.Groups[6].Value
            stream = [int]$Match.Groups[7].Value
            packets = [int]$Match.Groups[8].Value
            dispatch = [int]$Match.Groups[9].Value
            actual_crc = $Match.Groups[10].Value
            expected_crc = $Match.Groups[11].Value
            read_code = if ($null -ne $Read) { [string]$Read.code } else { "" }
            read_bytes = if ($null -ne $Read) { [int]$Read.bytes } else { 0 }
            app_stage_code = if ($null -ne $Stage) { [string]$Stage.code } else { "" }
            app_stage_format = if ($null -ne $Stage) { [string]$Stage.format } else { "" }
            app_stage_bytes = if ($null -ne $Stage) { [int]$Stage.bytes } else { 0 }
        }
    }
    return @($Packetstreams)
}

function Get-SourceMatrixFromText {
    param([string]$Text)

    $Runs = Get-AppRunSummaryFromText -Text $Text
    $Matrix = [ordered]@{}
    foreach ($Run in @($Runs)) {
        $Name = [string]$Run.name
        $Source = "direct"
        $App = $Name
        if ($Name.Contains(":")) {
            $Parts = $Name.Split(":", 2)
            $Source = $Parts[0]
            $App = $Parts[1]
        }
        if ($Source -eq "prepare") {
            continue
        }
        if (-not $Matrix.Contains($App)) {
            $Matrix[$App] = [ordered]@{
                name = $App
                direct = $null
                received = $null
                packetstream = $null
                store = $null
                prepare = $null
            }
        }
        if ($Matrix[$App].Contains($Source)) {
            $Matrix[$App][$Source] = [pscustomobject]@{
                stage = [string]$Run.stage
                code = [string]$Run.code
                exit = [int]$Run.exit
            }
        }
    }

    $PrepareRegex = [System.Text.RegularExpressions.Regex]::new(
        'resident-elf-qemu: prepare prepare:(\S+) stage=(\S+) code=(\S+) ready=(\d+) argc=(\d+)')
    foreach ($Match in $PrepareRegex.Matches($Text)) {
        $App = $Match.Groups[1].Value
        if (-not $Matrix.Contains($App)) {
            $Matrix[$App] = [ordered]@{
                name = $App
                direct = $null
                received = $null
                packetstream = $null
                store = $null
                prepare = $null
            }
        }
        $Matrix[$App]["prepare"] = [pscustomobject]@{
            stage = $Match.Groups[2].Value
            code = $Match.Groups[3].Value
            ready = ([int]$Match.Groups[4].Value) -ne 0
            argc = [int]$Match.Groups[5].Value
        }
    }

    return @($Matrix.Values | ForEach-Object { [pscustomobject]$_ })
}

function Get-CapabilitySummaryFromText {
    param([string]$Text)

    $Caps = [ordered]@{
        console = $false
        time = $false
        display = $false
        input = $false
        storage = $false
        app_exit = $false
        unsupported = $false
    }
    if ($Text.Contains("hello_app: charm_app_main entered")) {
        $Caps.console = $true
    }
    if ($Text.Contains("resident-elf-qemu: caps time_app console=0 time=2")) {
        $Caps.time = $true
    }
    if ($Text.Contains("display_sequence_app: frames=2 checksum=3072")) {
        $Caps.display = $true
    }
    if ($Text.Contains("input_sequence_app: polls=4 checksum=114")) {
        $Caps.input = $true
    }
    if ($Text.Contains("storage_catalog_app: files=2 bytes=31 checksum=2845")) {
        $Caps.storage = $true
    }
    if ($Text.Contains("resident-elf-qemu: app.exit code=7")) {
        $Caps.app_exit = $true
    }
    if ($Text.Contains("resident-elf-qemu: unsupported storage_open=1 storage_read=1 storage_write=1 storage_close=1 afe_configure=1 afe_read=1")) {
        $Caps.unsupported = $true
    }
    return [pscustomobject]$Caps
}

function Get-GuiTimelineSummary {
    param(
        [object]$FrameSignatureCapture,
        [object]$InputTraceCapture
    )

    $Entries = [ordered]@{}
    foreach ($Run in @($FrameSignatureCapture.runs)) {
        $Frames = @($Run.frames)
        $Entries[[string]$Run.name] = [ordered]@{
            name = [string]$Run.name
            stage = [string]$Run.stage
            code = [string]$Run.code
            exit = [int]$Run.exit
            frames = [int]$Run.frame_count
            inputs = 0
            last_frame_hash = if ($Frames.Count -gt 0) { [string]$Frames[$Frames.Count - 1].hash } else { "" }
            last_input = ""
        }
    }
    foreach ($Run in @($InputTraceCapture.runs)) {
        $Name = [string]$Run.name
        if (-not $Entries.Contains($Name)) {
            $Entries[$Name] = [ordered]@{
                name = $Name
                stage = [string]$Run.stage
                code = [string]$Run.code
                exit = [int]$Run.exit
                frames = 0
                inputs = 0
                last_frame_hash = ""
                last_input = ""
            }
        }
        $Events = @($Run.events)
        $Entries[$Name].inputs = [int]$Run.event_count
        if ($Events.Count -gt 0) {
            $Last = $Events[$Events.Count - 1]
            $Entries[$Name].last_input = "{0},{1},{2}" -f ([int]$Last.pointer_x), ([int]$Last.pointer_y), ([int]$Last.down)
        }
    }

    return @($Entries.Values | ForEach-Object { [pscustomobject]$_ })
}

function Write-DomainSummaryCapture {
    param(
        [string]$LogPath,
        [string]$FrameSignaturePath,
        [string]$FrameDumpPath,
        [string]$FramePpmPath,
        [string]$InputTracePath,
        [string]$StorageTracePath,
        [string]$OutputPath
    )

    if ([string]::IsNullOrWhiteSpace($OutputPath)) {
        return
    }
    if (-not (Test-Path -LiteralPath $LogPath)) {
        throw "domain_summary_failed: log not found: $LogPath"
    }
    $ResolvedLog = (Resolve-Path -LiteralPath $LogPath).Path
    $ResolvedOut = Resolve-ScriptPath -Path $OutputPath
    $OutDir = [System.IO.Path]::GetDirectoryName($ResolvedOut)
    if (-not [string]::IsNullOrWhiteSpace($OutDir) -and -not (Test-Path -LiteralPath $OutDir)) {
        New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
    }

    $Text = Get-Content -LiteralPath $ResolvedLog -Raw -Encoding UTF8
    $BackendCapabilities = Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-capabilities capabilities=([a-z0-9_,]+)' -ErrorPrefix "domain_summary_failed"
    $BackendStorageMode = Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-capabilities capabilities=[a-z0-9_,]+ storage=([a-z0-9_]+)' -ErrorPrefix "domain_summary_failed"
    $BackendAfeMode = Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-capabilities capabilities=[a-z0-9_,]+ storage=[a-z0-9_]+ afe=([a-z0-9_]+)' -ErrorPrefix "domain_summary_failed"
    $RunRegionBase = Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: run-region base=(0x[0-9a-f]+)' -ErrorPrefix "domain_summary_failed"
    $RunRegionExpected = Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: run-region base=0x[0-9a-f]+ expected=(0x[0-9a-f]+)' -ErrorPrefix "domain_summary_failed"
    $RunRegionSize = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: run-region base=0x[0-9a-f]+ expected=0x[0-9a-f]+ size=(\d+)' -ErrorPrefix "domain_summary_failed")
    $StageCacheBytes = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: stage-cache bytes=(\d+)' -ErrorPrefix "domain_summary_failed")
    $StoreEntries = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: store entries=(\d+)' -ErrorPrefix "domain_summary_failed")
    $StoreBytes = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: store entries=\d+ bytes=(\d+)' -ErrorPrefix "domain_summary_failed")
    $StoreMediaKind = Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: store-media kind=([a-z0-9_]+)' -ErrorPrefix "domain_summary_failed"
    $StoreMediaBytes = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: store-media kind=[a-z0-9_]+ bytes=(\d+)' -ErrorPrefix "domain_summary_failed")
    $StoreMediaReadCalls = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: store-media kind=[a-z0-9_]+ bytes=\d+ read_calls=(\d+)' -ErrorPrefix "domain_summary_failed")
    $StoreMediaReadBytes = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: store-media kind=[a-z0-9_]+ bytes=\d+ read_calls=\d+ read_bytes=(\d+)' -ErrorPrefix "domain_summary_failed")
    $StoreMediaReadFailures = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: store-media kind=[a-z0-9_]+ bytes=\d+ read_calls=\d+ read_bytes=\d+ read_failures=(\d+)' -ErrorPrefix "domain_summary_failed")
    $DisplayWidth = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: display describe width=(\d+)' -ErrorPrefix "domain_summary_failed")
    $DisplayHeight = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: display describe width=\d+ height=(\d+)' -ErrorPrefix "domain_summary_failed")
    $DisplayStride = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: display describe width=\d+ height=\d+ stride=(\d+)' -ErrorPrefix "domain_summary_failed")
    $DisplayFormat = Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: display describe width=\d+ height=\d+ stride=\d+ format=([a-z0-9_]+)' -ErrorPrefix "domain_summary_failed"
    $DisplayFrameBytes = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: display describe width=\d+ height=\d+ stride=\d+ format=[a-z0-9_]+ frame_bytes=(\d+)' -ErrorPrefix "domain_summary_failed")
    $PrepareArgc = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: prepare prepare:argv_app stage=start code=ok ready=1 argc=(\d+)' -ErrorPrefix "domain_summary_failed")

    $FrameSignatureResolved = ""
    $FrameSignatureCount = 0
    $FrameSignatureRunCount = 0
    $FrameSignatureCapture = [pscustomobject]@{ runs = @() }
    if (-not [string]::IsNullOrWhiteSpace($FrameSignaturePath) -and (Test-Path -LiteralPath $FrameSignaturePath)) {
        $FrameSignatureResolved = (Resolve-Path -LiteralPath $FrameSignaturePath).Path
        $FrameSignatureCapture = Get-Content -LiteralPath $FrameSignatureResolved -Raw -Encoding UTF8 | ConvertFrom-Json
        $FrameSignatureCount = [int]$FrameSignatureCapture.frame_count
        $FrameSignatureRunCount = [int]$FrameSignatureCapture.run_count
    }

    $FrameDumpResolved = ""
    $FrameDumpCount = 0
    $FrameDumpRunCount = 0
    if (-not [string]::IsNullOrWhiteSpace($FrameDumpPath) -and (Test-Path -LiteralPath $FrameDumpPath)) {
        $FrameDumpResolved = (Resolve-Path -LiteralPath $FrameDumpPath).Path
        $FrameDumpCapture = Get-Content -LiteralPath $FrameDumpResolved -Raw -Encoding UTF8 | ConvertFrom-Json
        $FrameDumpCount = [int]$FrameDumpCapture.frame_count
        $FrameDumpRunCount = [int]$FrameDumpCapture.run_count
    }

    $FramePpmResolved = ""
    $FramePpmCount = 0
    $FramePpmRunCount = 0
    if (-not [string]::IsNullOrWhiteSpace($FramePpmPath) -and (Test-Path -LiteralPath $FramePpmPath)) {
        $FramePpmResolved = (Resolve-Path -LiteralPath $FramePpmPath).Path
        $FramePpmManifest = Join-Path $FramePpmResolved "manifest.json"
        if (Test-Path -LiteralPath $FramePpmManifest) {
            $FramePpmCapture = Get-Content -LiteralPath $FramePpmManifest -Raw -Encoding UTF8 | ConvertFrom-Json
            $FramePpmCount = [int]$FramePpmCapture.frame_count
            $FramePpmRunCount = [int]$FramePpmCapture.run_count
        }
    }

    $InputTraceResolved = ""
    $InputTraceCount = 0
    $InputTraceRunCount = 0
    $InputTraceCapture = [pscustomobject]@{ runs = @() }
    if (-not [string]::IsNullOrWhiteSpace($InputTracePath) -and (Test-Path -LiteralPath $InputTracePath)) {
        $InputTraceResolved = (Resolve-Path -LiteralPath $InputTracePath).Path
        $InputTraceCapture = Get-Content -LiteralPath $InputTraceResolved -Raw -Encoding UTF8 | ConvertFrom-Json
        $InputTraceCount = [int]$InputTraceCapture.event_count
        $InputTraceRunCount = [int]$InputTraceCapture.run_count
    }

    $StorageTraceResolved = ""
    $StorageTraceCount = 0
    $StorageTraceRunCount = 0
    if (-not [string]::IsNullOrWhiteSpace($StorageTracePath) -and (Test-Path -LiteralPath $StorageTracePath)) {
        $StorageTraceResolved = (Resolve-Path -LiteralPath $StorageTracePath).Path
        $StorageTraceCapture = Get-Content -LiteralPath $StorageTraceResolved -Raw -Encoding UTF8 | ConvertFrom-Json
        $StorageTraceCount = [int]$StorageTraceCapture.event_count
        $StorageTraceRunCount = [int]$StorageTraceCapture.run_count
    }

    $Summary = [pscustomobject]@{
        schema = "charm.resident_elf_qemu.domain_summary.v1"
        domain = "virtual_m7"
        machine = "mps2-an500"
        cpu = "cortex-m7"
        image_format = "elf"
        app_model = "CharmAppApi"
        backend_contract = [pscustomobject]@{
            kind = "virtual"
            runtime_domain = "virtual_m7"
            capabilities = @($BackendCapabilities -split ",")
            storage = $BackendStorageMode
            afe = $BackendAfeMode
        }
        run_region = [pscustomobject]@{
            base = $RunRegionBase
            expected = $RunRegionExpected
            size = $RunRegionSize
            base_numeric = (Convert-HexToInt64 -Value $RunRegionBase)
        }
        stage_cache = [pscustomobject]@{
            bytes = $StageCacheBytes
        }
        display = [pscustomobject]@{
            width = $DisplayWidth
            height = $DisplayHeight
            stride_bytes = $DisplayStride
            format = $DisplayFormat
            frame_bytes = $DisplayFrameBytes
            pixel_bytes = 4
        }
        store = [pscustomobject]@{
            format = "store_v1"
            entries = $StoreEntries
            bytes = $StoreBytes
            media = [pscustomobject]@{
                kind = $StoreMediaKind
                bytes = $StoreMediaBytes
                read_calls = $StoreMediaReadCalls
                read_bytes = $StoreMediaReadBytes
                read_failures = $StoreMediaReadFailures
            }
        }
        coverage = [pscustomobject]@{
            runs = Get-AppRunSummaryFromText -Text $Text
            stages = Get-StageSummaryFromText -Text $Text
            loads = Get-ElfLoadSummaryFromText -Text $Text
            packetstreams = Get-PacketstreamSummaryFromText -Text $Text
            source_matrix = Get-SourceMatrixFromText -Text $Text
            gui_timeline = Get-GuiTimelineSummary -FrameSignatureCapture $FrameSignatureCapture -InputTraceCapture $InputTraceCapture
            prepare = [pscustomobject]@{
                name = "prepare:argv_app"
                stage = "start"
                code = "ok"
                ready = $true
                argc = $PrepareArgc
                capability_calls = 0
            }
            capabilities = Get-CapabilitySummaryFromText -Text $Text
            negative_cases = @(
                [pscustomobject]@{ name = "packetstream_crc_mismatch"; stage = "packetstream_verify"; code = "crc_mismatch" },
                [pscustomobject]@{ name = "received_too_large_app"; stage = "received_stage"; code = "buffer_too_small" },
                [pscustomobject]@{ name = "too_large_store_app"; stage = "store_stage"; code = "image_too_large" },
                [pscustomobject]@{ name = "bad_elf_magic_app"; stage = "load"; code = "load_failed" },
                [pscustomobject]@{ name = "packetstream_bad_elf_magic_app"; stage = "load"; code = "load_failed" },
                [pscustomobject]@{ name = "bad_header_app"; stage = "load"; code = "load_failed" },
                [pscustomobject]@{ name = "bad_class_app"; stage = "load"; code = "load_failed" },
                [pscustomobject]@{ name = "bad_endian_app"; stage = "load"; code = "load_failed" },
                [pscustomobject]@{ name = "bad_program_header_app"; stage = "load"; code = "load_failed" },
                [pscustomobject]@{ name = "truncated_payload_app"; stage = "load"; code = "load_failed" },
                [pscustomobject]@{ name = "no_load_segment_app"; stage = "load"; code = "load_failed" },
                [pscustomobject]@{ name = "entry_outside_segment_app"; stage = "load"; code = "load_failed" },
                [pscustomobject]@{ name = "overlapping_segments_app"; stage = "load"; code = "load_failed" },
                [pscustomobject]@{ name = "rwx_segment_app"; stage = "load"; code = "load_failed" },
                [pscustomobject]@{ name = "too_large_app"; stage = "load"; code = "load_failed" },
                [pscustomobject]@{ name = "argv_overflow_app"; stage = "argv"; code = "argv_overflow" },
                [pscustomobject]@{ name = "abi_mismatch_app"; stage = "abi"; code = "abi_mismatch" }
            )
        }
        evidence = [pscustomobject]@{
            log = $ResolvedLog
            frame_signatures = $FrameSignatureResolved
            frame_signature_count = $FrameSignatureCount
            frame_signature_run_count = $FrameSignatureRunCount
            frame_dumps = $FrameDumpResolved
            frame_dump_count = $FrameDumpCount
            frame_dump_run_count = $FrameDumpRunCount
            frame_ppm = $FramePpmResolved
            frame_ppm_count = $FramePpmCount
            frame_ppm_run_count = $FramePpmRunCount
            input_trace = $InputTraceResolved
            input_trace_event_count = $InputTraceCount
            input_trace_run_count = $InputTraceRunCount
            storage_trace = $StorageTraceResolved
            storage_trace_event_count = $StorageTraceCount
            storage_trace_run_count = $StorageTraceRunCount
        }
    }

    $Json = $Summary | ConvertTo-Json -Depth 16
    [System.IO.File]::WriteAllText($ResolvedOut, ($Json + "`n"), [System.Text.UTF8Encoding]::new($false))
    Write-Host "resident-elf-qemu domain summary:"
    Write-Host "  path=$ResolvedOut"
    Write-Host "  domain=virtual_m7"
    Write-Host "  runs=$(@($Summary.coverage.runs).Count)"
    Write-Host "  store_entries=$StoreEntries"
    Write-Host "  store_media_reads=$StoreMediaReadCalls"
}

function ConvertTo-CanonicalDomainSummary {
    param([object]$Summary)

    $Canonical = $Summary | ConvertTo-Json -Depth 32 | ConvertFrom-Json
    if ($null -ne $Canonical.evidence) {
        foreach ($PropertyName in @("log", "frame_signatures", "frame_dumps", "frame_ppm", "input_trace", "storage_trace")) {
            $Property = $Canonical.evidence.PSObject.Properties[$PropertyName]
            if ($null -ne $Property -and -not [string]::IsNullOrWhiteSpace([string]$Property.Value)) {
                $Property.Value = [System.IO.Path]::GetFileName([string]$Property.Value)
            }
        }
    }
    return $Canonical
}

function Read-CanonicalDomainSummaryJson {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "domain_summary_compare_failed: path is empty"
    }
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "domain_summary_compare_failed: file not found: $Path"
    }
    $ResolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $Summary = Get-Content -LiteralPath $ResolvedPath -Raw -Encoding UTF8 | ConvertFrom-Json
    return (ConvertTo-CanonicalDomainSummary -Summary $Summary) | ConvertTo-Json -Depth 32 -Compress
}

function Compare-DomainSummaryFiles {
    param(
        [string]$ExpectedPath,
        [string]$ActualPath
    )

    if ([string]::IsNullOrWhiteSpace($ExpectedPath) -or [string]::IsNullOrWhiteSpace($ActualPath)) {
        throw "domain_summary_compare_failed: expected and actual paths are required"
    }
    [void](Validate-DomainSummaryFile -Path $ExpectedPath)
    [void](Validate-DomainSummaryFile -Path $ActualPath)
    $ExpectedJson = Read-CanonicalDomainSummaryJson -Path $ExpectedPath
    $ActualJson = Read-CanonicalDomainSummaryJson -Path $ActualPath
    if ($ExpectedJson -ne $ActualJson) {
        Write-Host "resident-elf-qemu domain summary comparison failed"
        Write-Host "  expected=$ExpectedPath"
        Write-Host "  actual=$ActualPath"
        throw "domain_summary_compare_failed: captures differ"
    }
    Write-Host "resident-elf-qemu domain summary comparison ok"
    Write-Host "  expected=$ExpectedPath"
    Write-Host "  actual=$ActualPath"
    return 0
}

function Assert-DomainRun {
    param(
        [object[]]$Runs,
        [string]$Name,
        [string]$Stage,
        [string]$Code
    )

    $Matches = @($Runs | Where-Object { $_.name -eq $Name -and $_.stage -eq $Stage -and $_.code -eq $Code })
    if ($Matches.Count -lt 1) {
        throw "domain_summary_validate_failed: missing run $Name stage=$Stage code=$Code"
    }
}

function Assert-DomainStage {
    param(
        [object[]]$Stages,
        [string]$Source,
        [string]$Name,
        [string]$Code
    )

    $Matches = @($Stages | Where-Object { $_.source -eq $Source -and $_.name -eq $Name -and $_.code -eq $Code })
    if ($Matches.Count -lt 1) {
        throw "domain_summary_validate_failed: missing stage source=$Source name=$Name code=$Code"
    }
}

function Assert-DomainLoad {
    param(
        [object[]]$Loads,
        [string]$Name,
        [string]$Probe,
        [bool]$Fits,
        [int]$Region,
        [int]$MinSpan,
        [int]$Segments
    )

    $Matches = @($Loads | Where-Object { $_.name -eq $Name })
    if ($Matches.Count -lt 1) {
        throw "domain_summary_validate_failed: missing load $Name"
    }
    $Load = $Matches[0]
    if ($Load.format -ne "elf" -or $Load.probe -ne $Probe -or $Load.capacity_probe -ne $Probe) {
        throw "domain_summary_validate_failed: load $Name bad probe/format"
    }
    if ([bool]$Load.fits -ne $Fits) {
        throw "domain_summary_validate_failed: load $Name fits=$($Load.fits), expected $Fits"
    }
    if ([int]$Load.region -ne $Region) {
        throw "domain_summary_validate_failed: load $Name region=$($Load.region), expected $Region"
    }
    if ([int]$Load.span -lt $MinSpan -or [int]$Load.needed -ne [int]$Load.span) {
        throw "domain_summary_validate_failed: load $Name bad span/needed"
    }
    if ([int]$Load.segments -ne $Segments) {
        throw "domain_summary_validate_failed: load $Name segments=$($Load.segments), expected $Segments"
    }
    if ($Fits -and ([int]$Load.free -ne ($Region - [int]$Load.span))) {
        throw "domain_summary_validate_failed: load $Name free=$($Load.free), expected $($Region - [int]$Load.span)"
    }
    if (-not $Fits -and [int]$Load.free -ne 0) {
        throw "domain_summary_validate_failed: load $Name free=$($Load.free), expected 0"
    }
}

function Assert-DomainPacketstream {
    param(
        [object[]]$Packetstreams,
        [string]$Name,
        [int]$Payload
    )

    $Matches = @($Packetstreams | Where-Object { $_.name -eq $Name })
    if ($Matches.Count -lt 1) {
        throw "domain_summary_validate_failed: missing packetstream $Name"
    }
    $Packetstream = $Matches[0]
    if ($Packetstream.transport -ne "ok" -or
        $Packetstream.packet -ne "ok" -or
        $Packetstream.receive_stage -ne "launch_ready" -or
        $Packetstream.receive_code -ne "ok") {
        throw "domain_summary_validate_failed: packetstream $Name did not reach launch_ready"
    }
    if ([int]$Packetstream.payload -ne $Payload -or
        [int]$Packetstream.read_bytes -ne $Payload -or
        [int]$Packetstream.app_stage_bytes -ne $Payload) {
        throw "domain_summary_validate_failed: packetstream $Name bad byte counts"
    }
    if ($Packetstream.read_code -ne "ok" -or
        $Packetstream.app_stage_code -ne "ok" -or
        $Packetstream.app_stage_format -ne "elf") {
        throw "domain_summary_validate_failed: packetstream $Name bad read/app-stage"
    }
    if ($Packetstream.actual_crc -ne $Packetstream.expected_crc) {
        throw "domain_summary_validate_failed: packetstream $Name crc mismatch"
    }
    if ([int]$Packetstream.stream -le $Payload -or [int]$Packetstream.packets -lt 4 -or [int]$Packetstream.dispatch -ne [int]$Packetstream.packets) {
        throw "domain_summary_validate_failed: packetstream $Name bad stream/packet counts"
    }
}

function Assert-DomainPacketstreamFailure {
    param(
        [object[]]$Packetstreams,
        [string]$Name,
        [int]$Payload,
        [string]$Transport,
        [string]$Packet,
        [string]$ReceiveStage,
        [string]$ReceiveCode
    )

    $Matches = @($Packetstreams | Where-Object { $_.name -eq $Name })
    if ($Matches.Count -lt 1) {
        throw "domain_summary_validate_failed: missing packetstream failure $Name"
    }
    $Packetstream = $Matches[0]
    if ($Packetstream.transport -ne $Transport -or
        $Packetstream.packet -ne $Packet -or
        $Packetstream.receive_stage -ne $ReceiveStage -or
        $Packetstream.receive_code -ne $ReceiveCode) {
        throw "domain_summary_validate_failed: packetstream failure $Name wrong state"
    }
    if ([int]$Packetstream.payload -ne $Payload -or
        [int]$Packetstream.read_bytes -ne 0 -or
        [int]$Packetstream.app_stage_bytes -ne 0) {
        throw "domain_summary_validate_failed: packetstream failure $Name bad byte counts"
    }
    if ($Packetstream.read_code -ne "" -or
        $Packetstream.app_stage_code -ne "" -or
        $Packetstream.app_stage_format -ne "") {
        throw "domain_summary_validate_failed: packetstream failure $Name unexpectedly read or staged image"
    }
    if ($Packetstream.actual_crc -eq $Packetstream.expected_crc) {
        throw "domain_summary_validate_failed: packetstream failure $Name did not report crc mismatch"
    }
    if ([int]$Packetstream.stream -le $Payload -or [int]$Packetstream.packets -lt 4 -or [int]$Packetstream.dispatch -ge [int]$Packetstream.packets) {
        throw "domain_summary_validate_failed: packetstream failure $Name bad stream/packet counts"
    }
}

function Assert-DomainNegativeCase {
    param(
        [object[]]$NegativeCases,
        [string]$Name,
        [string]$Stage,
        [string]$Code
    )

    $Matches = @($NegativeCases | Where-Object { $_.name -eq $Name -and $_.stage -eq $Stage -and $_.code -eq $Code })
    if ($Matches.Count -lt 1) {
        throw "domain_summary_validate_failed: missing negative case $Name stage=$Stage code=$Code"
    }
}

function Assert-DomainStoreMedia {
    param([object]$Store)

    if ($Store.format -ne "store_v1" -or [int]$Store.entries -ne 14 -or [int]$Store.bytes -le 0) {
        throw "domain_summary_validate_failed: bad store summary"
    }
    if ($Store.media.kind -ne "memory" -or
        [int]$Store.media.bytes -ne [int]$Store.bytes -or
        [int]$Store.media.read_calls -le 0 -or
        [int]$Store.media.read_bytes -le 0 -or
        [int]$Store.media.read_failures -ne 0) {
        throw "domain_summary_validate_failed: bad store media summary"
    }
}

function Assert-DomainCount {
    param(
        [string]$Name,
        [int]$Actual,
        [int]$Expected
    )

    if ($Actual -ne $Expected) {
        throw "domain_summary_validate_failed: $Name count=$Actual, expected $Expected"
    }
}

function Assert-SourceMatrixEntry {
    param(
        [object[]]$Matrix,
        [string]$Name,
        [string[]]$Sources
    )

    $Matches = @($Matrix | Where-Object { $_.name -eq $Name })
    if ($Matches.Count -ne 1) {
        throw "domain_summary_validate_failed: source matrix missing or duplicate app $Name"
    }
    $Entry = $Matches[0]
    foreach ($Source in $Sources) {
        $Value = $Entry.$Source
        if ($null -eq $Value) {
            throw "domain_summary_validate_failed: source matrix $Name missing source $Source"
        }
        if ($Source -eq "prepare") {
            if ($Value.stage -ne "start" -or $Value.code -ne "ok" -or -not [bool]$Value.ready) {
                throw "domain_summary_validate_failed: source matrix $Name prepare is not ready"
            }
        } elseif ($Value.stage -ne "exit" -or $Value.code -ne "ok" -or [int]$Value.exit -ne 0) {
            throw "domain_summary_validate_failed: source matrix $Name source $Source did not exit ok"
        }
    }
}

function Assert-SourceMatrixFailure {
    param(
        [object[]]$Matrix,
        [string]$Name,
        [string]$Source,
        [string]$Stage,
        [string]$Code
    )

    $Matches = @($Matrix | Where-Object { $_.name -eq $Name })
    if ($Matches.Count -ne 1) {
        throw "domain_summary_validate_failed: source matrix missing or duplicate app $Name"
    }
    $Value = $Matches[0].$Source
    if ($null -eq $Value -or $Value.stage -ne $Stage -or $Value.code -ne $Code) {
        throw "domain_summary_validate_failed: source matrix $Name source $Source expected $Stage/$Code"
    }
}

function New-SelfTestSourceRun {
    param(
        [string]$Stage,
        [string]$Code,
        [int]$Exit = 0
    )
    return [pscustomobject]@{
        stage = $Stage
        code = $Code
        exit = $Exit
    }
}

function New-SelfTestSourcePrepare {
    return [pscustomobject]@{
        stage = "start"
        code = "ok"
        ready = $true
        argc = 4
    }
}

function New-SelfTestSourceMatrixEntry {
    param(
        [string]$Name,
        [object]$Direct = $null,
        [object]$Received = $null,
        [object]$Packetstream = $null,
        [object]$Store = $null,
        [object]$Prepare = $null
    )
    return [pscustomobject]@{
        name = $Name
        direct = $Direct
        received = $Received
        packetstream = $Packetstream
        store = $Store
        prepare = $Prepare
    }
}

function Assert-BadDomainSummaryRejected {
    param(
        [string]$SourcePath,
        [string]$Label,
        [scriptblock]$Mutate
    )

    $TempPath = Join-Path ([System.IO.Path]::GetTempPath()) ("charm_resident_elf_qemu_bad_domain_{0}.json" -f $Label)
    $Summary = Get-Content -LiteralPath $SourcePath -Raw -Encoding UTF8 | ConvertFrom-Json
    & $Mutate $Summary
    try {
        [System.IO.File]::WriteAllText($TempPath, (($Summary | ConvertTo-Json -Depth 16) + "`n"), [System.Text.UTF8Encoding]::new($false))
        if (-not (Test-SelfTestThrowsLike -Prefix "domain_summary_validate_failed:" -Script { Validate-DomainSummaryFile -Path $TempPath })) {
            throw "selftest_failed: bad domain summary '$Label' validated unexpectedly"
        }
    } finally {
        Remove-Item -LiteralPath $TempPath -Force -ErrorAction SilentlyContinue
    }
}

function Assert-GuiTimelineEntry {
    param(
        [object[]]$Timeline,
        [string]$Name,
        [int]$Frames,
        [int]$Inputs,
        [string]$LastFrameHash,
        [string]$LastInput
    )

    $Matches = @($Timeline | Where-Object { $_.name -eq $Name })
    if ($Matches.Count -ne 1) {
        throw "domain_summary_validate_failed: gui timeline missing or duplicate run $Name"
    }
    $Entry = $Matches[0]
    if ([int]$Entry.frames -ne $Frames -or [int]$Entry.inputs -ne $Inputs) {
        throw "domain_summary_validate_failed: gui timeline $Name expected frames=$Frames inputs=$Inputs"
    }
    if ($Entry.stage -ne "exit" -or $Entry.code -ne "ok" -or [int]$Entry.exit -ne 0) {
        throw "domain_summary_validate_failed: gui timeline $Name did not exit ok"
    }
    if ([string]$Entry.last_frame_hash -ne $LastFrameHash) {
        throw "domain_summary_validate_failed: gui timeline $Name bad last frame hash"
    }
    if ([string]$Entry.last_input -ne $LastInput) {
        throw "domain_summary_validate_failed: gui timeline $Name bad last input"
    }
}

function Validate-DomainSummaryFile {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "domain_summary_validate_failed: path is empty"
    }
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "domain_summary_validate_failed: file not found: $Path"
    }
    $ResolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $Summary = Get-Content -LiteralPath $ResolvedPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($Summary.schema -ne "charm.resident_elf_qemu.domain_summary.v1") {
        throw "domain_summary_validate_failed: bad schema: $($Summary.schema)"
    }
    if ($Summary.domain -ne "virtual_m7" -or $Summary.machine -ne "mps2-an500" -or $Summary.cpu -ne "cortex-m7") {
        throw "domain_summary_validate_failed: bad domain identity"
    }
    if ($Summary.image_format -ne "elf" -or $Summary.app_model -ne "CharmAppApi") {
        throw "domain_summary_validate_failed: bad app model"
    }
    if ($Summary.backend_contract.kind -ne "virtual" -or
        $Summary.backend_contract.runtime_domain -ne "virtual_m7" -or
        $Summary.backend_contract.storage -ne "readonly" -or
        $Summary.backend_contract.afe -ne "unsupported") {
        throw "domain_summary_validate_failed: bad backend contract"
    }
    $BackendCapabilities = @($Summary.backend_contract.capabilities)
    foreach ($Capability in @("console", "time", "display", "input", "storage", "app_exit")) {
        if (-not ($BackendCapabilities -contains $Capability)) {
            throw "domain_summary_validate_failed: backend capability missing $Capability"
        }
    }
    if ($BackendCapabilities.Count -ne 6) {
        throw "domain_summary_validate_failed: unexpected backend capability count"
    }
    if ($Summary.run_region.base -ne "0x20080000" -or $Summary.run_region.expected -ne "0x20080000" -or [int]$Summary.run_region.size -ne 65536) {
        throw "domain_summary_validate_failed: bad run region"
    }
    if ([int]$Summary.stage_cache.bytes -ne 16384) {
        throw "domain_summary_validate_failed: bad stage cache size"
    }
    if ([int]$Summary.display.width -ne 16 -or
        [int]$Summary.display.height -ne 16 -or
        [int]$Summary.display.stride_bytes -ne 64 -or
        $Summary.display.format -ne "argb8888" -or
        [int]$Summary.display.frame_bytes -ne 1024 -or
        [int]$Summary.display.pixel_bytes -ne 4) {
        throw "domain_summary_validate_failed: bad display summary"
    }
    Assert-DomainStoreMedia -Store $Summary.store
    $Runs = @($Summary.coverage.runs)
    Assert-DomainCount -Name "runs" -Actual $Runs.Count -Expected 46
    Assert-DomainRun -Runs $Runs -Name "hello_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "received:hello_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "store:hello_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "player_min" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "received:player_min" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "packetstream:player_min" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "store:player_min" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "large_fit_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "received:large_fit_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "packetstream:large_fit_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "store:large_fit_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "data_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "store:data_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "bad_elf_magic_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "packetstream:packetstream_bad_elf_magic_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "bad_header_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "bad_class_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "bad_endian_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "bad_program_header_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "truncated_payload_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "no_load_segment_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "entry_outside_segment_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "overlapping_segments_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "rwx_segment_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "too_large_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "argv_overflow_app" -Stage "argv" -Code "argv_overflow"
    Assert-DomainRun -Runs $Runs -Name "abi_mismatch_app" -Stage "abi" -Code "abi_mismatch"
    $Stages = @($Summary.coverage.stages)
    Assert-DomainCount -Name "stages" -Actual $Stages.Count -Expected 19
    Assert-DomainStage -Stages $Stages -Source "received" -Name "hello_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "received" -Name "large_fit_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "store" -Name "hello_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "store" -Name "data_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "store" -Name "large_fit_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "store" -Name "too_large_store_app" -Code "image_too_large"
    $Loads = @($Summary.coverage.loads)
    Assert-DomainCount -Name "loads" -Actual $Loads.Count -Expected 47
    Assert-DomainLoad -Loads $Loads -Name "hello_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "received:hello_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "packetstream:hello_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "store:hello_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "prepare:argv_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "player_min" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 3
    Assert-DomainLoad -Loads $Loads -Name "received:player_min" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 3
    Assert-DomainLoad -Loads $Loads -Name "packetstream:player_min" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 3
    Assert-DomainLoad -Loads $Loads -Name "store:player_min" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 3
    Assert-DomainLoad -Loads $Loads -Name "bss_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 3
    Assert-DomainLoad -Loads $Loads -Name "data_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 3
    Assert-DomainLoad -Loads $Loads -Name "store:data_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 3
    Assert-DomainLoad -Loads $Loads -Name "large_fit_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 60000 -Segments 3
    Assert-DomainLoad -Loads $Loads -Name "received:large_fit_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 60000 -Segments 3
    Assert-DomainLoad -Loads $Loads -Name "packetstream:large_fit_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 60000 -Segments 3
    Assert-DomainLoad -Loads $Loads -Name "store:large_fit_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 60000 -Segments 3
    Assert-DomainLoad -Loads $Loads -Name "bad_elf_magic_app" -Probe "bad_magic" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "packetstream:packetstream_bad_elf_magic_app" -Probe "bad_magic" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "bad_header_app" -Probe "bad_header" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "bad_class_app" -Probe "bad_class" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "bad_endian_app" -Probe "bad_endian" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "bad_program_header_app" -Probe "bad_program_header" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "truncated_payload_app" -Probe "truncated_payload" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "no_load_segment_app" -Probe "no_load_segment" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "entry_outside_segment_app" -Probe "entry_outside_segment" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "overlapping_segments_app" -Probe "overlapping_segments" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "rwx_segment_app" -Probe "rwx_segment" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "too_large_app" -Probe "load_buffer_too_small" -Fits $false -Region 65536 -MinSpan 65537 -Segments 2
    $Packetstreams = @($Summary.coverage.packetstreams)
    Assert-DomainCount -Name "packetstreams" -Actual $Packetstreams.Count -Expected 5
    Assert-DomainPacketstream -Packetstreams $Packetstreams -Name "hello_app" -Payload 5132
    Assert-DomainPacketstream -Packetstreams $Packetstreams -Name "large_fit_app" -Payload 5168
    Assert-DomainPacketstream -Packetstreams $Packetstreams -Name "player_min" -Payload 5168
    Assert-DomainPacketstream -Packetstreams $Packetstreams -Name "packetstream_bad_elf_magic_app" -Payload 64
    Assert-DomainPacketstreamFailure -Packetstreams $Packetstreams `
        -Name "packetstream_crc_mismatch" `
        -Payload 5132 `
        -Transport "packet_failed" `
        -Packet "receive_failed" `
        -ReceiveStage "failed" `
        -ReceiveCode "crc_mismatch"
    if ($Summary.coverage.prepare.name -ne "prepare:argv_app" -or
        $Summary.coverage.prepare.stage -ne "start" -or
        $Summary.coverage.prepare.code -ne "ok" -or
        -not [bool]$Summary.coverage.prepare.ready -or
        [int]$Summary.coverage.prepare.argc -ne 4 -or
        [int]$Summary.coverage.prepare.capability_calls -ne 0) {
        throw "domain_summary_validate_failed: bad prepare summary"
    }
    foreach ($Capability in @("console", "time", "display", "input", "storage", "app_exit", "unsupported")) {
        if (-not [bool]$Summary.coverage.capabilities.$Capability) {
            throw "domain_summary_validate_failed: capability coverage missing $Capability"
        }
    }
    $NegativeCases = @($Summary.coverage.negative_cases)
    Assert-DomainCount -Name "negative_cases" -Actual $NegativeCases.Count -Expected 17
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "packetstream_crc_mismatch" -Stage "packetstream_verify" -Code "crc_mismatch"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "received_too_large_app" -Stage "received_stage" -Code "buffer_too_small"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "too_large_store_app" -Stage "store_stage" -Code "image_too_large"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "bad_elf_magic_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "packetstream_bad_elf_magic_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "bad_header_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "bad_class_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "bad_endian_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "bad_program_header_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "truncated_payload_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "no_load_segment_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "entry_outside_segment_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "overlapping_segments_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "rwx_segment_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "too_large_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "argv_overflow_app" -Stage "argv" -Code "argv_overflow"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "abi_mismatch_app" -Stage "abi" -Code "abi_mismatch"
    $SourceMatrix = @($Summary.coverage.source_matrix)
    Assert-DomainCount -Name "source_matrix" -Actual $SourceMatrix.Count -Expected 27
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "hello_app" -Sources @("direct", "received", "packetstream", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "large_fit_app" -Sources @("direct", "received", "packetstream", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "player_min" -Sources @("direct", "received", "packetstream", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "argv_app" -Sources @("direct", "store", "prepare")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "bss_app" -Sources @("direct", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "data_app" -Sources @("direct", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "display_sequence_app" -Sources @("direct", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "input_sequence_app" -Sources @("direct", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "storage_catalog_app" -Sources @("direct", "store")
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "bad_header_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "bad_class_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "bad_endian_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "bad_program_header_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "truncated_payload_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "no_load_segment_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "entry_outside_segment_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "overlapping_segments_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "rwx_segment_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "too_large_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "packetstream_bad_elf_magic_app" -Source "packetstream" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "argv_overflow_app" -Source "direct" -Stage "argv" -Code "argv_overflow"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "abi_mismatch_app" -Source "direct" -Stage "abi" -Code "abi_mismatch"
    $GuiTimeline = @($Summary.coverage.gui_timeline)
    Assert-DomainCount -Name "gui_timeline" -Actual $GuiTimeline.Count -Expected 8
    Assert-GuiTimelineEntry -Timeline $GuiTimeline -Name "display_sequence_app" -Frames 2 -Inputs 0 -LastFrameHash "0xa9b09dc5" -LastInput ""
    Assert-GuiTimelineEntry -Timeline $GuiTimeline -Name "store:display_sequence_app" -Frames 2 -Inputs 0 -LastFrameHash "0xa9b09dc5" -LastInput ""
    Assert-GuiTimelineEntry -Timeline $GuiTimeline -Name "input_sequence_app" -Frames 0 -Inputs 4 -LastFrameHash "" -LastInput "6,8,0"
    Assert-GuiTimelineEntry -Timeline $GuiTimeline -Name "store:input_sequence_app" -Frames 0 -Inputs 4 -LastFrameHash "" -LastInput "6,8,0"
    foreach ($Name in @("player_min", "received:player_min", "packetstream:player_min", "store:player_min")) {
        Assert-GuiTimelineEntry -Timeline $GuiTimeline -Name $Name -Frames 1 -Inputs 1 -LastFrameHash "0xfac53a05" -LastInput "3,5,0"
    }
    if ([int]$Summary.evidence.frame_signature_count -ne 8 -or [int]$Summary.evidence.frame_signature_run_count -ne 6) {
        throw "domain_summary_validate_failed: bad frame signature evidence"
    }
    if ([int]$Summary.evidence.frame_dump_count -ne 8 -or [int]$Summary.evidence.frame_dump_run_count -ne 6) {
        throw "domain_summary_validate_failed: bad frame dump evidence"
    }
    if ([int]$Summary.evidence.frame_ppm_count -ne 8 -or [int]$Summary.evidence.frame_ppm_run_count -ne 6) {
        throw "domain_summary_validate_failed: bad frame ppm evidence"
    }
    if ([int]$Summary.evidence.input_trace_event_count -ne 12 -or [int]$Summary.evidence.input_trace_run_count -ne 6) {
        throw "domain_summary_validate_failed: bad input trace evidence"
    }
    if ([int]$Summary.evidence.storage_trace_event_count -ne 49 -or [int]$Summary.evidence.storage_trace_run_count -ne 6) {
        throw "domain_summary_validate_failed: bad storage trace evidence"
    }
    Write-Host "resident-elf-qemu domain summary validation ok"
    Write-Host "  path=$ResolvedPath"
    return 0
}

function Get-SyntheticPassingLog {
    return (Get-ExpectedTokens -join "`n")
}

function Assert-ValidationOk {
    param(
        [string]$Name,
        [int]$Code
    )

    if ($Code -ne 0) {
        throw "evidence_bundle_validate_failed: $Name returned $Code"
    }
}

function Invoke-EvidenceBundleValidation {
    $LogPath = $ValidateLog
    if ([string]::IsNullOrWhiteSpace($LogPath)) {
        $LogPath = Join-Path $PSScriptRoot "qemu-ci.log"
    }

    Assert-ValidationOk -Name "log" -Code (Validate-LogFile -Path (Resolve-ScriptPath -Path $LogPath))

    if (-not [string]::IsNullOrWhiteSpace($FrameSignatureOut)) {
        $ResolvedFrameSignatureOut = Resolve-ScriptPath -Path $FrameSignatureOut
        Assert-ValidationOk -Name "frame_signatures" -Code (Validate-FrameSignatureFile -Path $ResolvedFrameSignatureOut)
        if (-not $SkipGoldenFrameSignatures -and -not [string]::IsNullOrWhiteSpace($GoldenFrameSignatures)) {
            Assert-ValidationOk -Name "frame_signatures_golden" -Code (Compare-FrameSignatureFiles -ExpectedPath (Resolve-ScriptPath -Path $GoldenFrameSignatures) -ActualPath $ResolvedFrameSignatureOut)
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($FrameDumpOut)) {
        Assert-ValidationOk -Name "frame_dumps" -Code (Validate-FrameDumpFile -Path (Resolve-ScriptPath -Path $FrameDumpOut))
    }

    if (-not [string]::IsNullOrWhiteSpace($FramePpmOut)) {
        Assert-ValidationOk -Name "frame_ppm" -Code (Validate-FramePpmDirectory -Path (Resolve-ScriptPath -Path $FramePpmOut))
    }

    if (-not [string]::IsNullOrWhiteSpace($InputTraceOut)) {
        $ResolvedInputTraceOut = Resolve-ScriptPath -Path $InputTraceOut
        Assert-ValidationOk -Name "input_trace" -Code (Validate-InputTraceFile -Path $ResolvedInputTraceOut)
        if (-not $SkipGoldenInputTrace -and -not [string]::IsNullOrWhiteSpace($GoldenInputTrace)) {
            Assert-ValidationOk -Name "input_trace_golden" -Code (Compare-InputTraceFiles -ExpectedPath (Resolve-ScriptPath -Path $GoldenInputTrace) -ActualPath $ResolvedInputTraceOut)
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($StorageTraceOut)) {
        $ResolvedStorageTraceOut = Resolve-ScriptPath -Path $StorageTraceOut
        Assert-ValidationOk -Name "storage_trace" -Code (Validate-StorageTraceFile -Path $ResolvedStorageTraceOut)
        if (-not $SkipGoldenStorageTrace -and -not [string]::IsNullOrWhiteSpace($GoldenStorageTrace)) {
            Assert-ValidationOk -Name "storage_trace_golden" -Code (Compare-StorageTraceFiles -ExpectedPath (Resolve-ScriptPath -Path $GoldenStorageTrace) -ActualPath $ResolvedStorageTraceOut)
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($DomainSummaryOut)) {
        $ResolvedDomainSummaryOut = Resolve-ScriptPath -Path $DomainSummaryOut
        Assert-ValidationOk -Name "domain_summary" -Code (Validate-DomainSummaryFile -Path $ResolvedDomainSummaryOut)
        if (-not $SkipGoldenDomainSummary -and -not [string]::IsNullOrWhiteSpace($GoldenDomainSummary)) {
            Assert-ValidationOk -Name "domain_summary_golden" -Code (Compare-DomainSummaryFiles -ExpectedPath (Resolve-ScriptPath -Path $GoldenDomainSummary) -ActualPath $ResolvedDomainSummaryOut)
        }
    }

    Write-Host "resident-elf-qemu evidence bundle validation ok"
    return 0
}

function Invoke-SelfTest {
    if ($ElfBase -ne "0x20080000") {
        throw "selftest_failed: QEMU ELF base must remain 0x20080000, got $ElfBase"
    }
    if ($TimeoutSec -le 0) {
        throw "selftest_failed: TimeoutSec must be positive"
    }
    if ($TailLines -le 0) {
        throw "selftest_failed: TailLines must be positive"
    }
    if (-not (Test-SelfTestThrowsLike -Prefix "missing_tool:" -Script { Resolve-ToolPath "__charm_missing_qemu_tool__" })) {
        throw "selftest_failed: missing tool did not report missing_tool"
    }
    if (-not (Test-SelfTestThrowsLike -Prefix "frame_signature_compare_failed:" -Script { Compare-FrameSignatureFiles -ExpectedPath "" -ActualPath "" })) {
        throw "selftest_failed: frame signature compare did not report frame_signature_compare_failed"
    }
    if (-not (Test-SelfTestThrowsLike -Prefix "frame_dump_compare_failed:" -Script { Compare-FrameDumpFiles -ExpectedPath "" -ActualPath "" })) {
        throw "selftest_failed: frame dump compare did not report frame_dump_compare_failed"
    }
    if (-not (Test-SelfTestThrowsLike -Prefix "frame_ppm_validate_failed:" -Script { Validate-FramePpmDirectory -Path "" })) {
        throw "selftest_failed: frame ppm validation did not report frame_ppm_validate_failed"
    }
    if (-not (Test-SelfTestThrowsLike -Prefix "input_trace_validate_failed:" -Script { Validate-InputTraceFile -Path "" })) {
        throw "selftest_failed: input trace validation did not report input_trace_validate_failed"
    }
    if (-not (Test-SelfTestThrowsLike -Prefix "input_trace_compare_failed:" -Script { Compare-InputTraceFiles -ExpectedPath "" -ActualPath "" })) {
        throw "selftest_failed: input trace compare did not report input_trace_compare_failed"
    }
    if (-not (Test-SelfTestThrowsLike -Prefix "storage_trace_validate_failed:" -Script { Validate-StorageTraceFile -Path "" })) {
        throw "selftest_failed: storage trace validation did not report storage_trace_validate_failed"
    }
    if (-not (Test-SelfTestThrowsLike -Prefix "storage_trace_compare_failed:" -Script { Compare-StorageTraceFiles -ExpectedPath "" -ActualPath "" })) {
        throw "selftest_failed: storage trace compare did not report storage_trace_compare_failed"
    }
    if (-not (Test-SelfTestThrowsLike -Prefix "domain_summary_validate_failed:" -Script { Validate-DomainSummaryFile -Path "" })) {
        throw "selftest_failed: domain summary validation did not report domain_summary_validate_failed"
    }
    if (-not (Test-SelfTestThrowsLike -Prefix "domain_summary_compare_failed:" -Script { Compare-DomainSummaryFiles -ExpectedPath "" -ActualPath "" })) {
        throw "selftest_failed: domain summary compare did not report domain_summary_compare_failed"
    }
    if (-not (Test-SelfTestThrowsLike -Prefix "evidence_bundle_validate_failed:" -Script { Assert-ValidationOk -Name "selftest" -Code 1 })) {
        throw "selftest_failed: evidence bundle validation did not report evidence_bundle_validate_failed"
    }
    $GoodStoreSummary = [pscustomobject]@{
        format = "store_v1"
        entries = 14
        bytes = 87584
        media = [pscustomobject]@{
            kind = "memory"
            bytes = 87584
            read_calls = 147
            read_bytes = 72360
            read_failures = 0
        }
    }
    Assert-DomainStoreMedia -Store $GoodStoreSummary
    $BadStoreSummary = [pscustomobject]@{
        format = "store_v1"
        entries = 14
        bytes = 87584
        media = [pscustomobject]@{
            kind = "memory"
            bytes = 87584
            read_calls = 147
            read_bytes = 72360
            read_failures = 1
        }
    }
    if (-not (Test-SelfTestThrowsLike -Prefix "domain_summary_validate_failed:" -Script { Assert-DomainStoreMedia -Store $BadStoreSummary })) {
        throw "selftest_failed: bad store media summary validated unexpectedly"
    }
    $GoodSourceMatrix = @(
        (New-SelfTestSourceMatrixEntry -Name "hello_app" -Direct (New-SelfTestSourceRun -Stage "exit" -Code "ok") -Received (New-SelfTestSourceRun -Stage "exit" -Code "ok") -Packetstream (New-SelfTestSourceRun -Stage "exit" -Code "ok") -Store (New-SelfTestSourceRun -Stage "exit" -Code "ok")),
        (New-SelfTestSourceMatrixEntry -Name "argv_app" -Direct (New-SelfTestSourceRun -Stage "exit" -Code "ok") -Store (New-SelfTestSourceRun -Stage "exit" -Code "ok") -Prepare (New-SelfTestSourcePrepare)),
        (New-SelfTestSourceMatrixEntry -Name "data_app" -Direct (New-SelfTestSourceRun -Stage "exit" -Code "ok") -Store (New-SelfTestSourceRun -Stage "exit" -Code "ok")),
        (New-SelfTestSourceMatrixEntry -Name "too_large_app" -Direct (New-SelfTestSourceRun -Stage "load" -Code "load_failed"))
    )
    Assert-SourceMatrixEntry -Matrix $GoodSourceMatrix -Name "hello_app" -Sources @("direct", "received", "packetstream", "store")
    Assert-SourceMatrixEntry -Matrix $GoodSourceMatrix -Name "argv_app" -Sources @("direct", "store", "prepare")
    Assert-SourceMatrixEntry -Matrix $GoodSourceMatrix -Name "data_app" -Sources @("direct", "store")
    Assert-SourceMatrixFailure -Matrix $GoodSourceMatrix -Name "too_large_app" -Source "direct" -Stage "load" -Code "load_failed"
    if (-not (Test-SelfTestThrowsLike -Prefix "domain_summary_validate_failed:" -Script { Assert-SourceMatrixEntry -Matrix $GoodSourceMatrix -Name "hello_app" -Sources @("received", "store", "prepare") })) {
        throw "selftest_failed: bad source matrix entry validated unexpectedly"
    }
    if (-not (Test-SelfTestThrowsLike -Prefix "domain_summary_validate_failed:" -Script { Assert-SourceMatrixFailure -Matrix $GoodSourceMatrix -Name "too_large_app" -Source "direct" -Stage "exit" -Code "ok" })) {
        throw "selftest_failed: bad source matrix failure validated unexpectedly"
    }
    if (-not (Test-LogText -Text (Get-SyntheticPassingLog))) {
        throw "selftest_failed: synthetic passing log did not validate"
    }
    $FailingLog = (Get-SyntheticPassingLog).Replace(
        "resident-elf-qemu: app player_min stage=exit code=ok exit=0",
        "resident-elf-qemu: app player_min stage=start code=ok exit=0")
    if (Test-LogText -Text $FailingLog) {
        throw "selftest_failed: synthetic failing log validated unexpectedly"
    }
    $ForbiddenLog = (Get-SyntheticPassingLog) + "`nresident-elf-qemu: fail"
    if (Test-LogText -Text $ForbiddenLog) {
        throw "selftest_failed: synthetic forbidden log validated unexpectedly"
    }
    $SyntheticFrameLog = @(
        "resident-elf-qemu: display present bytes=1024 checksum=1024 hash=0x373fb1c5 frame=1",
        "resident-elf-qemu: display present bytes=1024 checksum=2048 hash=0xa9b09dc5 frame=2",
        "resident-elf-qemu: app display_sequence_app stage=exit code=ok exit=0",
        "resident-elf-qemu: display present bytes=1024 checksum=1024 hash=0x373fb1c5 frame=1",
        "resident-elf-qemu: display present bytes=1024 checksum=2048 hash=0xa9b09dc5 frame=2",
        "resident-elf-qemu: app store:display_sequence_app stage=exit code=ok exit=0",
        "resident-elf-qemu: display present bytes=1024 checksum=174720 hash=0xfac53a05 frame=1",
        "resident-elf-qemu: app player_min stage=exit code=ok exit=0",
        "resident-elf-qemu: display present bytes=1024 checksum=174720 hash=0xfac53a05 frame=1",
        "resident-elf-qemu: app received:player_min stage=exit code=ok exit=0",
        "resident-elf-qemu: display present bytes=1024 checksum=174720 hash=0xfac53a05 frame=1",
        "resident-elf-qemu: app packetstream:player_min stage=exit code=ok exit=0",
        "resident-elf-qemu: display present bytes=1024 checksum=174720 hash=0xfac53a05 frame=1",
        "resident-elf-qemu: app store:player_min stage=exit code=ok exit=0"
    ) -join "`n"
    $SyntheticSignatures = Get-FrameSignaturesFromText -Text $SyntheticFrameLog
    $SyntheticFrames = @($SyntheticSignatures.frames)
    $SyntheticRuns = @($SyntheticSignatures.runs)
    if ($SyntheticFrames.Count -ne 8) {
        throw "selftest_failed: synthetic frame signature parse returned $($SyntheticFrames.Count) frames"
    }
    if ($SyntheticFrames[0].hash -ne "0x373fb1c5" -or
        $SyntheticFrames[1].hash -ne "0xa9b09dc5" -or
        $SyntheticFrames[4].hash -ne "0xfac53a05") {
        throw "selftest_failed: synthetic frame signature parse returned unexpected hashes"
    }
    if ($SyntheticRuns.Count -lt 3) {
        throw "selftest_failed: synthetic frame signature grouping returned $($SyntheticRuns.Count) runs"
    }
    $RunNames = @($SyntheticRuns | ForEach-Object { $_.name })
    foreach ($RequiredRun in @("display_sequence_app", "store:display_sequence_app", "player_min", "received:player_min", "packetstream:player_min", "store:player_min")) {
        if (-not ($RunNames -contains $RequiredRun)) {
            throw "selftest_failed: synthetic frame signature grouping missed run $RequiredRun"
        }
    }
    $SyntheticInputLog = @(
        "resident-elf-qemu: input poll encoder1=1 pointer=3,5 max=15,15 detected=1 down=0",
        "resident-elf-qemu: input poll encoder1=0 pointer=4,6 max=15,15 detected=1 down=1",
        "resident-elf-qemu: input poll encoder1=4294967295 pointer=5,7 max=15,15 detected=1 down=1",
        "resident-elf-qemu: input poll encoder1=0 pointer=6,8 max=15,15 detected=1 down=0",
        "resident-elf-qemu: app input_sequence_app stage=exit code=ok exit=0"
    ) -join "`n"
    $SyntheticInputTrace = Get-InputTraceFromText -Text $SyntheticInputLog
    $SyntheticInputRuns = @($SyntheticInputTrace.runs)
    if (@($SyntheticInputTrace.events).Count -ne 4 -or $SyntheticInputRuns.Count -ne 1) {
        throw "selftest_failed: synthetic input trace grouping failed"
    }
    Assert-InputTraceRun -Runs $SyntheticInputRuns -Name "input_sequence_app" -Encoder1 @(1, 0, -1, 0) -PointerX @(3, 4, 5, 6) -PointerY @(5, 6, 7, 8) -Down @(0, 1, 1, 0)
    $SyntheticStorageLog = @(
        "resident-elf-qemu: storage open path=/virtual/readme.txt code=ok fd=3 size=27",
        "resident-elf-qemu: storage read fd=3 code=ok requested=8 count=8 offset=0 remaining=19",
        "resident-elf-qemu: storage read fd=3 code=ok requested=8 count=8 offset=8 remaining=11",
        "resident-elf-qemu: storage read fd=3 code=ok requested=8 count=8 offset=16 remaining=3",
        "resident-elf-qemu: storage read fd=3 code=ok requested=8 count=3 offset=24 remaining=0",
        "resident-elf-qemu: storage read fd=3 code=ok requested=8 count=0 offset=27 remaining=0",
        "resident-elf-qemu: storage close fd=3 code=ok",
        "resident-elf-qemu: app storage_app stage=exit code=ok exit=0"
    ) -join "`n"
    $SyntheticStorageTrace = Get-StorageTraceFromText -Text $SyntheticStorageLog
    $SyntheticStorageRuns = @($SyntheticStorageTrace.runs)
    if (@($SyntheticStorageTrace.events).Count -ne 7 -or $SyntheticStorageRuns.Count -ne 1) {
        throw "selftest_failed: synthetic storage trace grouping failed"
    }
    Assert-StorageTraceRun -Runs $SyntheticStorageRuns `
        -Name "storage_app" `
        -Ops @("open", "read", "read", "read", "read", "read", "close") `
        -Paths @("/virtual/readme.txt", "", "", "", "", "", "") `
        -Fds @(3, 3, 3, 3, 3, 3, 3) `
        -Counts @(0, 8, 8, 8, 3, 0, 0)
    $SyntheticDumpLog = @(
        "resident-elf-qemu: display dump bytes=1024 checksum=1024 hash=0x373fb1c5 frame=1 hex=00",
        "resident-elf-qemu: display dump bytes=1024 checksum=2048 hash=0xa9b09dc5 frame=2 hex=11",
        "resident-elf-qemu: app display_sequence_app stage=exit code=ok exit=0",
        "resident-elf-qemu: display dump bytes=1024 checksum=174720 hash=0xfac53a05 frame=1 hex=22",
        "resident-elf-qemu: app player_min stage=exit code=ok exit=0",
        "resident-elf-qemu: display dump bytes=1024 checksum=174720 hash=0xfac53a05 frame=1 hex=33",
        "resident-elf-qemu: app received:player_min stage=exit code=ok exit=0",
        "resident-elf-qemu: display dump bytes=1024 checksum=174720 hash=0xfac53a05 frame=1 hex=44",
        "resident-elf-qemu: app packetstream:player_min stage=exit code=ok exit=0"
    ) -join "`n"
    $SyntheticDumps = Get-FrameDumpsFromText -Text $SyntheticDumpLog
    if (@($SyntheticDumps.frames).Count -ne 5 -or @($SyntheticDumps.runs).Count -ne 4) {
        throw "selftest_failed: synthetic frame dump grouping failed"
    }

    $AppSampleDir = Resolve-Path (Join-Path $PSScriptRoot "..\..\app_abi\elf_samples")
    $CMakeListsPath = Join-Path $PSScriptRoot "CMakeLists.txt"
    $CMakeListsText = Get-Content -LiteralPath $CMakeListsPath -Raw -Encoding UTF8
    foreach ($RequiredInc in @("appstore.bin.inc") + (Get-QemuAppNames | ForEach-Object { "$_.elf.inc" })) {
        if (-not $CMakeListsText.Contains($RequiredInc)) {
            throw "selftest_failed: CMakeLists.txt does not require generated artifact $RequiredInc"
        }
    }

    $ReadmePath = Join-Path $PSScriptRoot "README.md"
    $ReadmeText = Get-Content -LiteralPath $ReadmePath -Raw -Encoding UTF8
    foreach ($RequiredReadmeToken in @(
        "..\run-resident-elf-qemu-smoke.ps1 -SelfTest",
        "-SkipGoldenFrameSignatures",
        "-SkipGoldenInputTrace",
        "-SkipGoldenStorageTrace",
        "-SkipGoldenDomainSummary",
        "capture-resident-platform-evidence-bundle.ps1 -QemuElf",
        "display mode is fixed at 16x16 ARGB8888",
        "coverage.gui_timeline",
        "virtual_m7"
    )) {
        if (-not $ReadmeText.Contains($RequiredReadmeToken)) {
            throw "selftest_failed: README.md does not document $RequiredReadmeToken"
        }
    }

    foreach ($Required in @(
        (Join-Path $PSScriptRoot "CMakeLists.txt"),
        (Join-Path $PSScriptRoot "README.md"),
        (Join-Path $PSScriptRoot "main.cpp"),
        (Join-Path $PSScriptRoot "qemu_virtual_backend.hpp"),
        (Join-Path $PSScriptRoot "qemu_virtual_backend.cpp"),
        (Join-Path $PSScriptRoot "startup.cpp"),
        (Join-Path $PSScriptRoot "syscalls.cpp"),
        (Join-Path $PSScriptRoot "ldscript.ld"),
        (Join-Path $PSScriptRoot "argv_app.c"),
        (Join-Path $PSScriptRoot "bss_app.c"),
        (Join-Path $PSScriptRoot "data_app.c"),
        (Join-Path $PSScriptRoot "display_sequence_app.c"),
        (Join-Path $PSScriptRoot "exit_app.c"),
        (Join-Path $PSScriptRoot "input_sequence_app.c"),
        (Join-Path $PSScriptRoot "large_fit_app.c"),
        (Join-Path $PSScriptRoot "time_app.c"),
        (Join-Path $PSScriptRoot "too_large_app.c"),
        (Join-Path $PSScriptRoot "storage_app.c"),
        (Join-Path $PSScriptRoot "storage_catalog_app.c"),
        (Join-Path $PSScriptRoot "unsupported_caps_app.c"),
        (Join-Path $PSScriptRoot "frame-signatures.golden.json"),
        (Join-Path $PSScriptRoot "input-trace.golden.json"),
        (Join-Path $PSScriptRoot "storage-trace.golden.json"),
        (Join-Path $PSScriptRoot "domain-summary.golden.json"),
        (Join-Path $AppSampleDir "app_elf.ld"),
        (Join-Path $AppSampleDir "hello_app.c"),
        (Join-Path $AppSampleDir "player_min.c"),
        (Join-Path $PSScriptRoot "..\app_abi_store_pack_tool\CMakeLists.txt"),
        (Join-Path $PSScriptRoot "..\..\kernel\posix\qemu\arm-none-eabi-m7.cmake")
    )) {
        if (-not (Test-Path -LiteralPath $Required)) {
            throw "selftest_failed: required path is missing: $Required"
        }
    }

    $GoldenFrameSignatureFile = Join-Path $PSScriptRoot "frame-signatures.golden.json"
    $GoldenInputTraceFile = Join-Path $PSScriptRoot "input-trace.golden.json"
    $GoldenStorageTraceFile = Join-Path $PSScriptRoot "storage-trace.golden.json"
    $GoldenDomainSummaryFile = Join-Path $PSScriptRoot "domain-summary.golden.json"
    [void](Validate-FrameSignatureFile -Path $GoldenFrameSignatureFile)
    [void](Validate-InputTraceFile -Path $GoldenInputTraceFile)
    [void](Validate-StorageTraceFile -Path $GoldenStorageTraceFile)
    [void](Validate-DomainSummaryFile -Path $GoldenDomainSummaryFile)
    Assert-BadDomainSummaryRejected -SourcePath $GoldenDomainSummaryFile -Label "app_model" -Mutate {
        param($Summary)
        $Summary.app_model = "RawJump"
    }
    Assert-BadDomainSummaryRejected -SourcePath $GoldenDomainSummaryFile -Label "packetstream_crc" -Mutate {
        param($Summary)
        ($Summary.coverage.packetstreams | Where-Object { $_.name -eq "packetstream_crc_mismatch" }).read_code = "ok"
    }

    $CMakePath = Resolve-ToolPath -Tool $CMakeExe
    $QemuPath = Resolve-ToolPath -Tool $QemuExe
    $CcPath = Resolve-ToolPath -Tool "${ToolchainPrefix}gcc"
    $HostCompilerPath = Resolve-ToolPath -Tool $HostCompiler
    $BuildPath = Resolve-ScriptPath -Path $BuildDir
    $AppOutPath = Resolve-ScriptPath -Path $AppOutDir
    $FrameSignaturePath = Resolve-ScriptPath -Path $FrameSignatureOut
    $GoldenFrameSignaturePath = if ($SkipGoldenFrameSignatures) { "skipped" } else { Resolve-ScriptPath -Path $GoldenFrameSignatures }
    $FrameDumpPath = Resolve-ScriptPath -Path $FrameDumpOut
    $FramePpmPath = Resolve-ScriptPath -Path $FramePpmOut
    $InputTracePath = Resolve-ScriptPath -Path $InputTraceOut
    $GoldenInputTracePath = if ($SkipGoldenInputTrace) { "skipped" } else { Resolve-ScriptPath -Path $GoldenInputTrace }
    $StorageTracePath = Resolve-ScriptPath -Path $StorageTraceOut
    $GoldenStorageTracePath = if ($SkipGoldenStorageTrace) { "skipped" } else { Resolve-ScriptPath -Path $GoldenStorageTrace }
    $DomainSummaryPath = Resolve-ScriptPath -Path $DomainSummaryOut
    $GoldenDomainSummaryPath = if ($SkipGoldenDomainSummary) { "skipped" } else { Resolve-ScriptPath -Path $GoldenDomainSummary }

    Write-Host "resident-elf-qemu selftest:"
    Write-Host "  cmake=$CMakePath"
    Write-Host "  qemu=$QemuPath"
    Write-Host "  cc=$CcPath"
    Write-Host "  host_compiler=$HostCompilerPath"
    Write-Host "  build=$BuildPath"
    Write-Host "  app_out=$AppOutPath"
    Write-Host "  frame_signatures=$FrameSignaturePath"
    Write-Host "  golden_frame_signatures=$GoldenFrameSignaturePath"
    Write-Host "  frame_dumps=$FrameDumpPath"
    Write-Host "  frame_ppm=$FramePpmPath"
    Write-Host "  input_trace=$InputTracePath"
    Write-Host "  golden_input_trace=$GoldenInputTracePath"
    Write-Host "  storage_trace=$StorageTracePath"
    Write-Host "  golden_storage_trace=$GoldenStorageTracePath"
    Write-Host "  domain_summary=$DomainSummaryPath"
    Write-Host "  golden_domain_summary=$GoldenDomainSummaryPath"
    Write-Host "  elf_base=$ElfBase"
    Write-Host "[resident-elf-qemu] selftest ok"
}

if ($SelfTest) {
    Invoke-SelfTest
    exit 0
}

if ($ValidateEvidenceBundle) {
    exit (Invoke-EvidenceBundleValidation)
}

if (-not [string]::IsNullOrWhiteSpace($ValidateLog)) {
    exit (Validate-LogFile -Path $ValidateLog)
}

if (-not [string]::IsNullOrWhiteSpace($ValidateFrameSignatures)) {
    exit (Validate-FrameSignatureFile -Path $ValidateFrameSignatures)
}

if (-not [string]::IsNullOrWhiteSpace($CompareFrameSignatures) -or
    -not [string]::IsNullOrWhiteSpace($ActualFrameSignatures)) {
    exit (Compare-FrameSignatureFiles -ExpectedPath $CompareFrameSignatures -ActualPath $ActualFrameSignatures)
}

if (-not [string]::IsNullOrWhiteSpace($ValidateFrameDumps)) {
    exit (Validate-FrameDumpFile -Path $ValidateFrameDumps)
}

if (-not [string]::IsNullOrWhiteSpace($ValidateFramePpm)) {
    exit (Validate-FramePpmDirectory -Path $ValidateFramePpm)
}

if (-not [string]::IsNullOrWhiteSpace($ValidateInputTrace)) {
    exit (Validate-InputTraceFile -Path $ValidateInputTrace)
}

if (-not [string]::IsNullOrWhiteSpace($CompareInputTrace) -or
    -not [string]::IsNullOrWhiteSpace($ActualInputTrace)) {
    exit (Compare-InputTraceFiles -ExpectedPath $CompareInputTrace -ActualPath $ActualInputTrace)
}

if (-not [string]::IsNullOrWhiteSpace($ValidateStorageTrace)) {
    exit (Validate-StorageTraceFile -Path $ValidateStorageTrace)
}

if (-not [string]::IsNullOrWhiteSpace($CompareStorageTrace) -or
    -not [string]::IsNullOrWhiteSpace($ActualStorageTrace)) {
    exit (Compare-StorageTraceFiles -ExpectedPath $CompareStorageTrace -ActualPath $ActualStorageTrace)
}

if (-not [string]::IsNullOrWhiteSpace($ValidateDomainSummary)) {
    exit (Validate-DomainSummaryFile -Path $ValidateDomainSummary)
}

if (-not [string]::IsNullOrWhiteSpace($CompareDomainSummary) -or
    -not [string]::IsNullOrWhiteSpace($ActualDomainSummary)) {
    exit (Compare-DomainSummaryFiles -ExpectedPath $CompareDomainSummary -ActualPath $ActualDomainSummary)
}

if (-not [string]::IsNullOrWhiteSpace($CompareFrameDumps) -or
    -not [string]::IsNullOrWhiteSpace($ActualFrameDumps)) {
    exit (Compare-FrameDumpFiles -ExpectedPath $CompareFrameDumps -ActualPath $ActualFrameDumps)
}

$cmake = Resolve-ToolPath -Tool $CMakeExe
$qemu = Resolve-ToolPath -Tool $QemuExe
$cc = Resolve-ToolPath -Tool "${ToolchainPrefix}gcc"
$hostCompilerResolved = Resolve-ToolPath -Tool $HostCompiler

$appSampleDir = Resolve-Path (Join-Path $PSScriptRoot "..\..\app_abi\elf_samples")
$appOut = $AppOutDir
if (-not [System.IO.Path]::IsPathRooted($appOut)) {
    $appOut = Join-Path $PSScriptRoot $appOut
}
$appOut = [System.IO.Path]::GetFullPath($appOut)
$build = [System.IO.Path]::GetFullPath($BuildDir)
$toolchainFile = Resolve-Path (Join-Path $PSScriptRoot "..\..\kernel\posix\qemu\arm-none-eabi-m7.cmake")

Write-Host "resident-elf-qemu: app_out=$appOut"
Write-Host "resident-elf-qemu: build=$build"
Write-Host "resident-elf-qemu: elf_base=$ElfBase"

if ($DryRun) {
    Write-Host "[dry-run] qemu=$qemu"
    Write-Host "[dry-run] cc=$cc"
    Write-Host "[dry-run] host_compiler=$hostCompilerResolved"
    Write-Host "[dry-run] toolchain=$($toolchainFile.Path)"
    Write-Host "[dry-run] frame_signatures=$(Resolve-ScriptPath -Path $FrameSignatureOut)"
    if ($SkipGoldenFrameSignatures) {
        Write-Host "[dry-run] golden_frame_signatures=skipped"
    } else {
        Write-Host "[dry-run] golden_frame_signatures=$(Resolve-ScriptPath -Path $GoldenFrameSignatures)"
    }
    Write-Host "[dry-run] frame_dumps=$(Resolve-ScriptPath -Path $FrameDumpOut)"
    Write-Host "[dry-run] frame_ppm=$(Resolve-ScriptPath -Path $FramePpmOut)"
    Write-Host "[dry-run] input_trace=$(Resolve-ScriptPath -Path $InputTraceOut)"
    if ($SkipGoldenInputTrace) {
        Write-Host "[dry-run] golden_input_trace=skipped"
    } else {
        Write-Host "[dry-run] golden_input_trace=$(Resolve-ScriptPath -Path $GoldenInputTrace)"
    }
    Write-Host "[dry-run] storage_trace=$(Resolve-ScriptPath -Path $StorageTraceOut)"
    if ($SkipGoldenStorageTrace) {
        Write-Host "[dry-run] golden_storage_trace=skipped"
    } else {
        Write-Host "[dry-run] golden_storage_trace=$(Resolve-ScriptPath -Path $GoldenStorageTrace)"
    }
    Write-Host "[dry-run] domain_summary=$(Resolve-ScriptPath -Path $DomainSummaryOut)"
    if ($SkipGoldenDomainSummary) {
        Write-Host "[dry-run] golden_domain_summary=skipped"
    } else {
        Write-Host "[dry-run] golden_domain_summary=$(Resolve-ScriptPath -Path $GoldenDomainSummary)"
    }
    Write-Host "[dry-run] validate_evidence_bundle=$($ValidateEvidenceBundle.IsPresent)"
    exit 0
}

if (-not (Test-Path $appOut)) {
    New-Item -ItemType Directory -Path $appOut | Out-Null
}

$cachePath = Join-Path $build "CMakeCache.txt"
if (Test-Path $cachePath) {
    $cacheText = Get-Content -Raw -Encoding UTF8 $cachePath
    $expectedToolchainLine = "CMAKE_TOOLCHAIN_FILE:FILEPATH=$($toolchainFile.Path)"
    if (-not $cacheText.Contains($expectedToolchainLine)) {
        Remove-Item -Recurse -Force $build
    }
}

$ldscriptTemplate = Join-Path $appSampleDir "app_elf.ld"
$ldscript = Join-Path $appOut "app_elf.qemu.generated.ld"
$ldtext = Get-Content -Path $ldscriptTemplate -Raw -Encoding UTF8
$ldtext = $ldtext -replace 'ELF_BASE = 0x[0-9A-Fa-f]+;', "ELF_BASE = $ElfBase;"
[System.IO.File]::WriteAllText($ldscript, $ldtext, [System.Text.Encoding]::ASCII)

$includeRoot = Resolve-Path (Join-Path $appSampleDir "..")
$cflags = @(
    "-mcpu=cortex-m7",
    "-mthumb",
    "-ffreestanding",
    "-fno-builtin",
    "-fno-pic",
    "-fno-pie",
    "-fdata-sections",
    "-ffunction-sections",
    "-Os",
    "-nostdlib",
    "-nostartfiles",
    "-I$includeRoot"
)
$ldflags = @(
    "-Wl,--gc-sections",
    "-Wl,-T$ldscript"
)

foreach ($name in (Get-QemuAppNames)) {
    if ($name -eq "too_large_app" -or $name -eq "argv_app" -or $name -eq "bss_app" -or $name -eq "data_app" -or $name -eq "display_sequence_app" -or $name -eq "exit_app" -or $name -eq "input_sequence_app" -or $name -eq "large_fit_app" -or $name -eq "unsupported_caps_app" -or $name -eq "storage_app" -or $name -eq "storage_catalog_app" -or $name -eq "time_app") {
        $src = Join-Path $PSScriptRoot "too_large_app.c"
        if ($name -eq "argv_app") {
            $src = Join-Path $PSScriptRoot "argv_app.c"
        }
        if ($name -eq "bss_app") {
            $src = Join-Path $PSScriptRoot "bss_app.c"
        }
        if ($name -eq "data_app") {
            $src = Join-Path $PSScriptRoot "data_app.c"
        }
        if ($name -eq "exit_app") {
            $src = Join-Path $PSScriptRoot "exit_app.c"
        }
        if ($name -eq "input_sequence_app") {
            $src = Join-Path $PSScriptRoot "input_sequence_app.c"
        }
        if ($name -eq "large_fit_app") {
            $src = Join-Path $PSScriptRoot "large_fit_app.c"
        }
        if ($name -eq "display_sequence_app") {
            $src = Join-Path $PSScriptRoot "display_sequence_app.c"
        }
        if ($name -eq "unsupported_caps_app") {
            $src = Join-Path $PSScriptRoot "unsupported_caps_app.c"
        }
        if ($name -eq "storage_app") {
            $src = Join-Path $PSScriptRoot "storage_app.c"
        }
        if ($name -eq "storage_catalog_app") {
            $src = Join-Path $PSScriptRoot "storage_catalog_app.c"
        }
        if ($name -eq "time_app") {
            $src = Join-Path $PSScriptRoot "time_app.c"
        }
    } else {
        $src = Join-Path $appSampleDir "$name.c"
    }
    $elf = Join-Path $appOut "$name.elf"
    $inc = Join-Path $appOut "$name.elf.inc"
    Invoke-Checked -FilePath $cc -Arguments (@($cflags) + @($src, "-o", $elf) + @($ldflags))
    Write-IncFile -InputPath $elf -OutputPath $inc -Symbol "${name}_elf"
}

$tooLargeStoreElf = Join-Path $appOut "too_large_store_app.elf"
Write-PaddedArtifact -InputPath (Join-Path $appOut "hello_app.elf") `
    -OutputPath $tooLargeStoreElf `
    -TargetSize 20000

$store = Join-Path $appOut "appstore.bin"
$storeInc = Join-Path $appOut "appstore.bin.inc"
$storePackSource = Resolve-Path (Join-Path $PSScriptRoot "..\app_abi_store_pack_tool")
$storePackBuild = Join-Path $appOut "cmake-build-app-abi-store-pack-tool"
Invoke-Checked -FilePath $cmake -Arguments @(
    "-S", $storePackSource.Path,
    "-B", $storePackBuild,
    "-G", "Ninja",
    "-DCMAKE_CXX_COMPILER=$hostCompilerResolved"
)
Invoke-Checked -FilePath $cmake -Arguments @("--build", $storePackBuild, "--parallel", "1")
$storePackExe = Join-Path $storePackBuild "app-abi-store-pack.exe"
if (-not (Test-Path -LiteralPath $storePackExe)) {
    throw "missing_tool: app-abi-store-pack was not built at $storePackExe"
}
Invoke-Checked -FilePath $storePackExe -Arguments @(
    $store,
    ("hello_app={0}" -f (Join-Path $appOut "hello_app.elf")),
    ("player_min={0}" -f (Join-Path $appOut "player_min.elf")),
    ("argv_app={0}" -f (Join-Path $appOut "argv_app.elf")),
    ("bss_app={0}" -f (Join-Path $appOut "bss_app.elf")),
    ("data_app={0}" -f (Join-Path $appOut "data_app.elf")),
    ("display_sequence_app={0}" -f (Join-Path $appOut "display_sequence_app.elf")),
    ("exit_app={0}" -f (Join-Path $appOut "exit_app.elf")),
    ("input_sequence_app={0}" -f (Join-Path $appOut "input_sequence_app.elf")),
    ("unsupported_caps_app={0}" -f (Join-Path $appOut "unsupported_caps_app.elf")),
    ("storage_app={0}" -f (Join-Path $appOut "storage_app.elf")),
    ("storage_catalog_app={0}" -f (Join-Path $appOut "storage_catalog_app.elf")),
    ("time_app={0}" -f (Join-Path $appOut "time_app.elf")),
    ("large_fit_app={0}" -f (Join-Path $appOut "large_fit_app.elf")),
    ("too_large_store_app={0}" -f $tooLargeStoreElf)
)
Write-IncFile -InputPath $store -OutputPath $storeInc -Symbol "qemu_appstore_bin"

Invoke-Checked -FilePath $cmake -Arguments @(
    "-S", $PSScriptRoot,
    "-B", $build,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Debug",
    "-DCMAKE_TOOLCHAIN_FILE=$($toolchainFile.Path)",
    "-DCHARM_QEMU_APP_ELF_INC_DIR=$appOut"
)
Invoke-Checked -FilePath $cmake -Arguments @("--build", $build, "--parallel", "1")

$firmware = Join-Path $build "resident-elf-qemu-smoke.elf"
if (-not (Test-Path $firmware)) {
    throw "firmware not found: $firmware"
}

$outFile = Join-Path $PSScriptRoot "qemu-ci.log"
$errFile = Join-Path $PSScriptRoot "qemu-ci.err.log"
Remove-Item $outFile, $errFile -Force -ErrorAction SilentlyContinue

$args = @(
    "-M", "mps2-an500",
    "-cpu", "cortex-m7",
    "-nographic",
    "-kernel", $firmware
)

$proc = Start-Process -FilePath $qemu -ArgumentList $args `
    -RedirectStandardOutput $outFile -RedirectStandardError $errFile -WindowStyle Hidden -PassThru

$expected = Get-ExpectedTokens

$sw = [System.Diagnostics.Stopwatch]::StartNew()
while ($true) {
    if ($proc.HasExited) {
        break
    }
    $log = (Read-LogSafe -Path $outFile) + (Read-LogSafe -Path $errFile)
    $missingNow = $expected | Where-Object { -not $log.Contains($_) }
    if ($missingNow.Count -eq 0) {
        Stop-QemuProcessTree -RootId $proc.Id -Elf $firmware
        try {
            Wait-Process -Id $proc.Id -Timeout 2 -ErrorAction SilentlyContinue
        } catch {
        }
        break
    }
    if ($sw.Elapsed.TotalSeconds -ge $TimeoutSec) {
        break
    }
    Start-Sleep -Milliseconds 200
}

if (-not $proc.HasExited) {
    Stop-QemuProcessTree -RootId $proc.Id -Elf $firmware
    try {
        Wait-Process -Id $proc.Id -Timeout 2 -ErrorAction SilentlyContinue
    } catch {
    }
}

$log = (Read-LogSafe -Path $outFile) + (Read-LogSafe -Path $errFile)
$missing = $expected | Where-Object { -not $log.Contains($_) }
if ($missing.Count -gt 0) {
    Write-Host "[resident-elf-qemu] log tail:"
    if (Test-Path $outFile) {
        Get-Content $outFile -Tail $TailLines | ForEach-Object { Write-Host $_ }
    }
    if (Test-Path $errFile) {
        Get-Content $errFile -Tail $TailLines | ForEach-Object { Write-Host $_ }
    }
    throw "missing expected output: $($missing -join '; ')"
}

Write-FrameSignatureCapture -LogPath $outFile -OutputPath $FrameSignatureOut
if (-not [string]::IsNullOrWhiteSpace($FrameSignatureOut)) {
    $ResolvedFrameSignatureOut = Resolve-ScriptPath -Path $FrameSignatureOut
    [void](Validate-FrameSignatureFile -Path $ResolvedFrameSignatureOut)
    if (-not $SkipGoldenFrameSignatures -and -not [string]::IsNullOrWhiteSpace($GoldenFrameSignatures)) {
        $ResolvedGoldenFrameSignatures = Resolve-ScriptPath -Path $GoldenFrameSignatures
        Assert-ValidationOk -Name "frame_signatures_golden" -Code (Compare-FrameSignatureFiles -ExpectedPath $ResolvedGoldenFrameSignatures -ActualPath $ResolvedFrameSignatureOut)
    }
}
Write-FrameDumpCapture -LogPath $outFile -OutputPath $FrameDumpOut
if (-not [string]::IsNullOrWhiteSpace($FrameDumpOut)) {
    $ResolvedFrameDumpOut = Resolve-ScriptPath -Path $FrameDumpOut
    [void](Validate-FrameDumpFile -Path $ResolvedFrameDumpOut)
}
if (-not [string]::IsNullOrWhiteSpace($FramePpmOut)) {
    if ([string]::IsNullOrWhiteSpace($FrameDumpOut)) {
        throw "frame_ppm_failed: FrameDumpOut is required when FramePpmOut is set"
    }
    $ResolvedFrameDumpOut = Resolve-ScriptPath -Path $FrameDumpOut
    Write-FramePpmCapture -FrameDumpPath $ResolvedFrameDumpOut -OutputDir $FramePpmOut
    $ResolvedFramePpmOut = Resolve-ScriptPath -Path $FramePpmOut
    [void](Validate-FramePpmDirectory -Path $ResolvedFramePpmOut)
}
Write-InputTraceCapture -LogPath $outFile -OutputPath $InputTraceOut
if (-not [string]::IsNullOrWhiteSpace($InputTraceOut)) {
    $ResolvedInputTraceOut = Resolve-ScriptPath -Path $InputTraceOut
    [void](Validate-InputTraceFile -Path $ResolvedInputTraceOut)
    if (-not $SkipGoldenInputTrace -and -not [string]::IsNullOrWhiteSpace($GoldenInputTrace)) {
        $ResolvedGoldenInputTrace = Resolve-ScriptPath -Path $GoldenInputTrace
        Assert-ValidationOk -Name "input_trace_golden" -Code (Compare-InputTraceFiles -ExpectedPath $ResolvedGoldenInputTrace -ActualPath $ResolvedInputTraceOut)
    }
}
Write-StorageTraceCapture -LogPath $outFile -OutputPath $StorageTraceOut
if (-not [string]::IsNullOrWhiteSpace($StorageTraceOut)) {
    $ResolvedStorageTraceOut = Resolve-ScriptPath -Path $StorageTraceOut
    [void](Validate-StorageTraceFile -Path $ResolvedStorageTraceOut)
    if (-not $SkipGoldenStorageTrace -and -not [string]::IsNullOrWhiteSpace($GoldenStorageTrace)) {
        $ResolvedGoldenStorageTrace = Resolve-ScriptPath -Path $GoldenStorageTrace
        Assert-ValidationOk -Name "storage_trace_golden" -Code (Compare-StorageTraceFiles -ExpectedPath $ResolvedGoldenStorageTrace -ActualPath $ResolvedStorageTraceOut)
    }
}
Write-DomainSummaryCapture -LogPath $outFile `
    -FrameSignaturePath $FrameSignatureOut `
    -FrameDumpPath $FrameDumpOut `
    -FramePpmPath $FramePpmOut `
    -InputTracePath $InputTraceOut `
    -StorageTracePath $StorageTraceOut `
    -OutputPath $DomainSummaryOut
if (-not [string]::IsNullOrWhiteSpace($DomainSummaryOut)) {
    $ResolvedDomainSummaryOut = Resolve-ScriptPath -Path $DomainSummaryOut
    [void](Validate-DomainSummaryFile -Path $ResolvedDomainSummaryOut)
    if (-not $SkipGoldenDomainSummary -and -not [string]::IsNullOrWhiteSpace($GoldenDomainSummary)) {
        $ResolvedGoldenDomainSummary = Resolve-ScriptPath -Path $GoldenDomainSummary
        Assert-ValidationOk -Name "domain_summary_golden" -Code (Compare-DomainSummaryFiles -ExpectedPath $ResolvedGoldenDomainSummary -ActualPath $ResolvedDomainSummaryOut)
    }
}
Write-Host "[ok] resident ELF QEMU smoke detected"
