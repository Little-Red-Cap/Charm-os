param(
    [string]$CMakeExe = "cmake",
    [string]$QemuExe = "D:\Toolchains\qemu\qemu-system-arm.exe",
    [string]$ToolchainPrefix = "arm-none-eabi-",
    [string]$HostCompiler = "D:/Toolchains/w64devkit/bin/g++.exe",
    [string]$BuildDir = "$PSScriptRoot\cmake-build-resident-elf-qemu",
    [string]$AppOutDir = "$PSScriptRoot\cmake-build-resident-elf-qemu\apps",
    [string]$EvidenceDir = "$PSScriptRoot\cmake-build-resident-elf-qemu\evidence",
    [string]$FrameSignatureOut = "$PSScriptRoot\frame-signatures.json",
    [string]$GoldenFrameSignatures = "$PSScriptRoot\frame-signatures.golden.json",
    [string]$FrameDumpOut = "$PSScriptRoot\frame-dumps.json",
    [string]$GoldenFrameDumps = "$PSScriptRoot\frame-dumps.golden.json",
    [string]$FramePpmOut = "$PSScriptRoot\frame-ppm",
    [string]$InputTraceOut = "$PSScriptRoot\input-trace.json",
    [string]$GoldenInputTrace = "$PSScriptRoot\input-trace.golden.json",
    [string]$StorageTraceOut = "$PSScriptRoot\storage-trace.json",
    [string]$GoldenStorageTrace = "$PSScriptRoot\storage-trace.golden.json",
    [string]$DomainSummaryOut = "$PSScriptRoot\domain-summary.json",
    [string]$BackendContractOut = "$PSScriptRoot\backend-contract.json",
    [string]$GoldenDomainSummary = "$PSScriptRoot\domain-summary.golden.json",
    [string]$ElfBase = "0x20080000",
    [int]$TimeoutSec = 15,
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
    [string]$ValidateBackendContract = "",
    [string]$CompareDomainSummary = "",
    [string]$ActualDomainSummary = "",
    [string]$CompareFrameDumps = "",
    [string]$ActualFrameDumps = "",
    [switch]$SkipGoldenFrameSignatures,
    [switch]$SkipGoldenFrameDumps,
    [switch]$SkipGoldenInputTrace,
    [switch]$SkipGoldenStorageTrace,
    [switch]$SkipGoldenDomainSummary,
    [switch]$ValidateEvidenceBundle,
    [switch]$Doctor,
    [switch]$DryRun,
    [switch]$SelfTest
)

$ErrorActionPreference = "Stop"
$OutputEncoding = [System.Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)

$ExplicitFrameSignatureOut = $PSBoundParameters.ContainsKey("FrameSignatureOut")
$ExplicitFrameDumpOut = $PSBoundParameters.ContainsKey("FrameDumpOut")
$ExplicitFramePpmOut = $PSBoundParameters.ContainsKey("FramePpmOut")
$ExplicitInputTraceOut = $PSBoundParameters.ContainsKey("InputTraceOut")
$ExplicitStorageTraceOut = $PSBoundParameters.ContainsKey("StorageTraceOut")
$ExplicitDomainSummaryOut = $PSBoundParameters.ContainsKey("DomainSummaryOut")
$ExplicitBackendContractOut = $PSBoundParameters.ContainsKey("BackendContractOut")

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

function Acquire-QemuEvidenceLock {
    param([int]$TimeoutSec = 120)

    $LockRoot = Get-QemuEvidenceRoot
    if (-not (Test-Path -LiteralPath $LockRoot)) {
        New-Item -ItemType Directory -Path $LockRoot | Out-Null
    }
    $LockPath = Join-Path $LockRoot "qemu-evidence.lock"
    $Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSec)
    while ($true) {
        try {
            $Stream = [System.IO.File]::Open(
                $LockPath,
                [System.IO.FileMode]::OpenOrCreate,
                [System.IO.FileAccess]::ReadWrite,
                [System.IO.FileShare]::None)
            $Text = "pid=$PID acquired=$(Get-Date -Format o)`n"
            $Bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
            $Stream.SetLength(0)
            $Stream.Write($Bytes, 0, $Bytes.Length)
            $Stream.Flush()
            Write-Host "resident-elf-qemu evidence lock acquired path=$LockPath"
            return $Stream
        } catch [System.IO.IOException] {
            if ([DateTime]::UtcNow -ge $Deadline) {
                throw "qemu_evidence_lock_timeout: $LockPath"
            }
            Start-Sleep -Milliseconds 200
        }
    }
}

function Release-QemuEvidenceLock {
    param([object]$Lock)

    if ($null -ne $Lock) {
        $LockPath = $Lock.Name
        $Lock.Dispose()
        if (-not [string]::IsNullOrWhiteSpace($LockPath)) {
            Remove-Item -LiteralPath $LockPath -Force -ErrorAction SilentlyContinue
        }
        Write-Host "resident-elf-qemu evidence lock released"
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

function Get-QemuArtifactCrc32 {
    param([byte[]]$Bytes)

    $crc = [uint32]::MaxValue
    $poly = [uint32]3988292384
    foreach ($b in $Bytes) {
        $crc = $crc -bxor [uint32]$b
        for ($i = 0; $i -lt 8; $i++) {
            if (($crc -band 1) -ne 0) {
                $crc = ($crc -shr 1) -bxor $poly
            } else {
                $crc = $crc -shr 1
            }
        }
    }
    return ($crc -bxor [uint32]::MaxValue)
}

function Format-QemuArtifactHex32 {
    param([uint32]$Value)

    return ("0x{0:x8}" -f $Value)
}

function Read-QemuStoreUInt16Le {
    param(
        [byte[]]$Bytes,
        [int]$Offset
    )

    if ($Offset -lt 0 -or ($Offset + 2) -gt $Bytes.Length) {
        throw "domain_summary_failed: store uint16 read out of range offset=$Offset"
    }
    return [BitConverter]::ToUInt16($Bytes, $Offset)
}

function Read-QemuStoreUInt32Le {
    param(
        [byte[]]$Bytes,
        [int]$Offset
    )

    if ($Offset -lt 0 -or ($Offset + 4) -gt $Bytes.Length) {
        throw "domain_summary_failed: store uint32 read out of range offset=$Offset"
    }
    return [BitConverter]::ToUInt32($Bytes, $Offset)
}

function Read-QemuStoreEntryName {
    param(
        [byte[]]$Bytes,
        [int]$Offset
    )

    if ($Offset -lt 0 -or ($Offset + 32) -gt $Bytes.Length) {
        throw "domain_summary_failed: store entry name out of range offset=$Offset"
    }
    $Length = 0
    while ($Length -lt 32 -and $Bytes[$Offset + $Length] -ne 0) {
        ++$Length
    }
    return [System.Text.Encoding]::ASCII.GetString($Bytes, $Offset, $Length)
}

function Get-QemuStoreEntryFormatName {
    param([uint32]$Flags)

    $FormatBits = $Flags -band 0x0000000f
    if ($FormatBits -eq 0) {
        return "elf"
    }
    if ($FormatBits -eq 1) {
        return "modulex"
    }
    return "unknown"
}

function Get-QemuStorePayloadCrc32 {
    param(
        [byte[]]$Bytes,
        [uint32]$Offset,
        [uint32]$Size
    )

    $End = [uint64]$Offset + [uint64]$Size
    if ($Size -eq 0 -or $End -gt [uint64]$Bytes.Length) {
        throw "domain_summary_failed: store payload out of range offset=$Offset size=$Size"
    }
    $Payload = New-Object byte[] ([int]$Size)
    [Array]::Copy($Bytes, [int]$Offset, $Payload, 0, [int]$Size)
    return (Format-QemuArtifactHex32 -Value (Get-QemuArtifactCrc32 -Bytes $Payload))
}

function Get-QemuStoreEntryManifest {
    param([string]$StorePath)

    if ([string]::IsNullOrWhiteSpace($StorePath) -or -not (Test-Path -LiteralPath $StorePath)) {
        throw "domain_summary_failed: store artifact missing for entry manifest: $StorePath"
    }
    $ResolvedPath = (Resolve-Path -LiteralPath $StorePath).Path
    $Bytes = [System.IO.File]::ReadAllBytes($ResolvedPath)
    if ($Bytes.Length -lt 16) {
        throw "domain_summary_failed: store image too small for header"
    }

    $Magic = Read-QemuStoreUInt32Le -Bytes $Bytes -Offset 0
    $Version = Read-QemuStoreUInt16Le -Bytes $Bytes -Offset 4
    $HeaderSize = Read-QemuStoreUInt16Le -Bytes $Bytes -Offset 6
    $EntryCount = Read-QemuStoreUInt32Le -Bytes $Bytes -Offset 8
    $EntrySize = Read-QemuStoreUInt32Le -Bytes $Bytes -Offset 12
    if ($Magic -ne 0x50415043 -or $Version -ne 1 -or $HeaderSize -lt 16 -or $EntrySize -lt 44 -or $EntryCount -gt 128) {
        throw "domain_summary_failed: invalid Store v1 header in $ResolvedPath"
    }
    $TableEnd = [uint64]$HeaderSize + ([uint64]$EntryCount * [uint64]$EntrySize)
    if ($TableEnd -gt [uint64]$Bytes.Length) {
        throw "domain_summary_failed: Store entry table exceeds artifact size"
    }

    $Entries = @()
    for ($Index = 0; $Index -lt [int]$EntryCount; ++$Index) {
        $EntryOffset = [int]($HeaderSize + ([uint32]$Index * $EntrySize))
        $Name = Read-QemuStoreEntryName -Bytes $Bytes -Offset $EntryOffset
        $PayloadOffset = Read-QemuStoreUInt32Le -Bytes $Bytes -Offset ($EntryOffset + 32)
        $PayloadSize = Read-QemuStoreUInt32Le -Bytes $Bytes -Offset ($EntryOffset + 36)
        $Flags = Read-QemuStoreUInt32Le -Bytes $Bytes -Offset ($EntryOffset + 40)
        $PayloadEnd = [uint64]$PayloadOffset + [uint64]$PayloadSize
        if ([string]::IsNullOrWhiteSpace($Name) -or $PayloadSize -eq 0 -or $PayloadEnd -gt [uint64]$Bytes.Length) {
            throw "domain_summary_failed: invalid Store entry index=$Index name=$Name offset=$PayloadOffset size=$PayloadSize"
        }
        $Entries += [pscustomobject]@{
            index = $Index
            name = $Name
            format = Get-QemuStoreEntryFormatName -Flags $Flags
            format_bits = [int]($Flags -band 0x0000000f)
            flags = (Format-QemuArtifactHex32 -Value $Flags)
            offset = [int]$PayloadOffset
            size = [int]$PayloadSize
            payload_crc32 = Get-QemuStorePayloadCrc32 -Bytes $Bytes -Offset $PayloadOffset -Size $PayloadSize
        }
    }

    return [pscustomobject]@{
        header = [pscustomobject]@{
            magic = (Format-QemuArtifactHex32 -Value $Magic)
            version = [int]$Version
            header_size = [int]$HeaderSize
            entry_count = [int]$EntryCount
            entry_size = [int]$EntrySize
        }
        entries = @($Entries)
    }
}

function New-QemuArtifactRecord {
    param(
        [string]$Name,
        [string]$Kind,
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path -LiteralPath $Path)) {
        throw "domain_summary_failed: artifact missing: $Name path=$Path"
    }
    $ResolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $Bytes = [System.IO.File]::ReadAllBytes($ResolvedPath)
    return [pscustomobject]@{
        name = $Name
        kind = $Kind
        path = $ResolvedPath
        size = $Bytes.Length
        crc32 = (Format-QemuArtifactHex32 (Get-QemuArtifactCrc32 -Bytes $Bytes))
    }
}

function Get-QemuArtifactSummary {
    param([string]$AppOutDir)

    $ResolvedOut = [System.IO.Path]::GetFullPath($AppOutDir)
    if (-not (Test-Path -LiteralPath $ResolvedOut)) {
        throw "domain_summary_failed: app artifact directory missing: $ResolvedOut"
    }

    $Apps = @()
    foreach ($Spec in (Get-QemuAppSpecs)) {
        $Apps += New-QemuArtifactRecord `
            -Name $Spec.Name `
            -Kind "elf" `
            -Path (Join-Path $ResolvedOut "$($Spec.Name).elf")
    }
    $TooLargeStore = New-QemuArtifactRecord `
        -Name "too_large_store_app" `
        -Kind "elf" `
        -Path (Join-Path $ResolvedOut "too_large_store_app.elf")
    $Store = New-QemuArtifactRecord `
        -Name "appstore" `
        -Kind "store_v1" `
        -Path (Join-Path $ResolvedOut "appstore.bin")
    $StoreManifest = Get-QemuStoreEntryManifest -StorePath $Store.path
    $Includes = @()
    foreach ($Inc in (Get-QemuRequiredIncFiles)) {
        $Includes += New-QemuArtifactRecord `
            -Name ([System.IO.Path]::GetFileNameWithoutExtension($Inc)) `
            -Kind "generated_inc" `
            -Path (Join-Path $ResolvedOut $Inc)
    }

    return [pscustomobject]@{
        directory = $ResolvedOut
        app_count = @($Apps).Count
        store_app_count = @($Apps | Where-Object { $_.name -ne "too_large_app" }).Count
        include_count = @($Includes).Count
        apps = @($Apps)
        extra = @($TooLargeStore)
        store = $Store
        store_manifest = $StoreManifest
        includes = @($Includes)
    }
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

function Get-QemuAppSpecs {
    return @(
        @{ Name = "afe_error_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "hello_app"; SourceRoot = "samples"; Store = $true },
        @{ Name = "player_min"; SourceRoot = "samples"; Store = $true },
        @{ Name = "argv_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "bss_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "console_error_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "data_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "display_describe_error_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "display_error_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "display_null_present_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "display_sequence_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "exit_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "exit_error_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "exit_negative_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "input_error_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "input_sequence_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "input_wrap_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "large_fit_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "return_negative_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "unsupported_caps_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "storage_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "storage_catalog_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "storage_close_error_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "storage_error_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "storage_fd_exhaustion_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "storage_open_error_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "storage_write_error_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "storage_zero_io_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "time_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "time_sequence_app"; SourceRoot = "qemu"; Store = $true },
        @{ Name = "too_large_app"; SourceRoot = "qemu"; Store = $false }
    )
}

function Get-QemuAppNames {
    return Get-QemuAppSpecs | ForEach-Object { $_.Name }
}

function Resolve-QemuAppSource {
    param(
        [hashtable]$Spec,
        [string]$AppSampleDir
    )

    if ($Spec.SourceRoot -eq "qemu") {
        return Join-Path $PSScriptRoot "$($Spec.Name).c"
    }
    if ($Spec.SourceRoot -eq "samples") {
        return Join-Path $AppSampleDir "$($Spec.Name).c"
    }
    throw "unknown_qemu_app_source_root: $($Spec.SourceRoot)"
}

function Get-QemuRequiredIncFiles {
    return @("appstore.bin.inc") + (Get-QemuAppNames | ForEach-Object { "$_.elf.inc" })
}

function Get-QemuStorePackArguments {
    param(
        [string]$StorePath,
        [string]$AppOutDir,
        [string]$TooLargeStoreElf
    )

    $args = @($StorePath)
    foreach ($Spec in (Get-QemuAppSpecs | Where-Object { [bool]$_.Store })) {
        $args += ("{0}={1}" -f $Spec.Name, (Join-Path $AppOutDir "$($Spec.Name).elf"))
    }
    $args += ("too_large_store_app={0}" -f $TooLargeStoreElf)
    return $args
}

function Get-QemuRequiredSourcePaths {
    param([string]$AppSampleDir)

    return @(
        (Join-Path $PSScriptRoot "afe_error_app.c"),
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
        (Join-Path $PSScriptRoot "console_error_app.c"),
        (Join-Path $PSScriptRoot "data_app.c"),
        (Join-Path $PSScriptRoot "display_describe_error_app.c"),
        (Join-Path $PSScriptRoot "display_error_app.c"),
        (Join-Path $PSScriptRoot "display_null_present_app.c"),
        (Join-Path $PSScriptRoot "display_sequence_app.c"),
        (Join-Path $PSScriptRoot "exit_app.c"),
        (Join-Path $PSScriptRoot "exit_error_app.c"),
        (Join-Path $PSScriptRoot "exit_negative_app.c"),
        (Join-Path $PSScriptRoot "input_error_app.c"),
        (Join-Path $PSScriptRoot "input_sequence_app.c"),
        (Join-Path $PSScriptRoot "input_wrap_app.c"),
        (Join-Path $PSScriptRoot "large_fit_app.c"),
        (Join-Path $PSScriptRoot "return_negative_app.c"),
        (Join-Path $PSScriptRoot "time_app.c"),
        (Join-Path $PSScriptRoot "time_sequence_app.c"),
        (Join-Path $PSScriptRoot "too_large_app.c"),
        (Join-Path $PSScriptRoot "storage_app.c"),
        (Join-Path $PSScriptRoot "storage_catalog_app.c"),
        (Join-Path $PSScriptRoot "storage_close_error_app.c"),
        (Join-Path $PSScriptRoot "storage_error_app.c"),
        (Join-Path $PSScriptRoot "storage_fd_exhaustion_app.c"),
        (Join-Path $PSScriptRoot "storage_open_error_app.c"),
        (Join-Path $PSScriptRoot "storage_write_error_app.c"),
        (Join-Path $PSScriptRoot "storage_zero_io_app.c"),
        (Join-Path $PSScriptRoot "unsupported_caps_app.c"),
        (Join-Path $AppSampleDir "app_elf.ld"),
        (Join-Path $AppSampleDir "hello_app.c"),
        (Join-Path $AppSampleDir "player_min.c"),
        (Join-Path $PSScriptRoot "..\app_abi_store_pack_tool\CMakeLists.txt"),
        (Join-Path $PSScriptRoot "..\..\kernel\posix\qemu\arm-none-eabi-m7.cmake")
    )
}

function Resolve-ScriptPath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot $Path))
}

function Get-QemuEvidenceRoot {
    if ([string]::IsNullOrWhiteSpace($EvidenceDir)) {
        return [System.IO.Path]::GetFullPath($PSScriptRoot)
    }
    if ([System.IO.Path]::IsPathRooted($EvidenceDir)) {
        return [System.IO.Path]::GetFullPath($EvidenceDir)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot $EvidenceDir))
}

function Get-QemuEvidencePath {
    param([string]$FileName)

    return (Join-Path (Get-QemuEvidenceRoot) $FileName)
}

function Convert-QemuHex32 {
    param(
        [string]$Value,
        [string]$ErrorPrefix,
        [string]$Field
    )

    if ($Value -notmatch '^0x[0-9A-Fa-f]{1,8}$') {
        throw "${ErrorPrefix}: bad $Field hex value: $Value"
    }
    return ("0x{0:x8}" -f ([Convert]::ToUInt32($Value.Substring(2), 16)))
}

function Get-QemuFirmwareElfLoadBase {
    param(
        [string]$Path,
        [string]$ErrorPrefix
    )

    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path -LiteralPath $Path)) {
        throw "${ErrorPrefix}: QEMU firmware linker script not found: $Path"
    }
    $Text = Get-Content -LiteralPath $Path -Raw -Encoding UTF8
    $Match = [regex]::Match($Text, '(?m)^\s*\.elf_load\s+(0x[0-9A-Fa-f]+)\s+\(NOLOAD\)')
    if (-not $Match.Success) {
        throw "${ErrorPrefix}: QEMU firmware linker script missing .elf_load address: $Path"
    }
    return (Convert-QemuHex32 -Value $Match.Groups[1].Value -ErrorPrefix $ErrorPrefix -Field ".elf_load")
}

function Assert-QemuElfDomainLayout {
    param(
        [string]$LdscriptPath,
        [string]$ErrorPrefix
    )

    $ExpectedElfBase = Convert-QemuHex32 -Value $ElfBase -ErrorPrefix $ErrorPrefix -Field "ElfBase"
    $FirmwareElfLoadBase = Get-QemuFirmwareElfLoadBase -Path $LdscriptPath -ErrorPrefix $ErrorPrefix
    if ($ExpectedElfBase -ne "0x20080000") {
        throw "${ErrorPrefix}: QEMU ELF base must remain 0x20080000, got $ExpectedElfBase"
    }
    if ($FirmwareElfLoadBase -ne $ExpectedElfBase) {
        throw "${ErrorPrefix}: QEMU firmware .elf_load base $FirmwareElfLoadBase does not match ElfBase $ExpectedElfBase"
    }
    return [pscustomobject]@{
        elf_base = $ExpectedElfBase
        firmware_elf_load_base = $FirmwareElfLoadBase
    }
}

function Initialize-QemuEvidencePaths {
    $Root = Get-QemuEvidenceRoot
    if (-not $ExplicitFrameSignatureOut) {
        $script:FrameSignatureOut = Join-Path $Root "frame-signatures.json"
    }
    if (-not $ExplicitFrameDumpOut) {
        $script:FrameDumpOut = Join-Path $Root "frame-dumps.json"
    }
    if (-not $ExplicitFramePpmOut) {
        $script:FramePpmOut = Join-Path $Root "frame-ppm"
    }
    if (-not $ExplicitInputTraceOut) {
        $script:InputTraceOut = Join-Path $Root "input-trace.json"
    }
    if (-not $ExplicitStorageTraceOut) {
        $script:StorageTraceOut = Join-Path $Root "storage-trace.json"
    }
    if (-not $ExplicitDomainSummaryOut) {
        $script:DomainSummaryOut = Join-Path $Root "domain-summary.json"
    }
    if (-not $ExplicitBackendContractOut) {
        $script:BackendContractOut = Join-Path $Root "backend-contract.json"
    }
}

function Resolve-QemuDoctorToolPath {
    param(
        [string]$Name,
        [string]$Tool
    )

    try {
        return Resolve-ToolPath -Tool $Tool
    } catch {
        throw "doctor_failed: missing tool $Name ($Tool): $($_.Exception.Message)"
    }
}

function Assert-QemuDoctorMachineSupport {
    param(
        [string]$MachineHelp,
        [string]$Machine = "mps2-an500"
    )

    if ([string]::IsNullOrWhiteSpace($MachineHelp) -or
        $MachineHelp.IndexOf($Machine, [System.StringComparison]::OrdinalIgnoreCase) -lt 0) {
        throw "doctor_failed: qemu machine not supported: $Machine"
    }
}

function Get-QemuDoctorVersionLineFromText {
    param(
        [string]$Text,
        [string]$Label
    )

    foreach ($Line in ($Text -split "\r?\n")) {
        $Trimmed = $Line.Trim()
        if (-not [string]::IsNullOrWhiteSpace($Trimmed)) {
            return $Trimmed
        }
    }
    throw "doctor_failed: empty version output: $Label"
}

function Get-QemuDoctorToolVersionLine {
    param(
        [string]$Name,
        [string]$FilePath,
        [string[]]$Arguments
    )

    $Output = & $FilePath @Arguments 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) {
        throw "doctor_failed: version probe failed for $Name exit=$LASTEXITCODE"
    }
    return (Get-QemuDoctorVersionLineFromText -Text $Output -Label $Name)
}

function Assert-QemuDoctorRequiredPath {
    param(
        [string]$Label,
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path -LiteralPath $Path)) {
        throw "doctor_failed: missing required path: $Label path=$Path"
    }
    return (Resolve-Path -LiteralPath $Path).Path
}

function Write-QemuDoctorOptionalPath {
    param(
        [string]$Label,
        [string]$Path,
        [bool]$Required = $false
    )

    $Resolved = Resolve-ScriptPath -Path $Path
    if (Test-Path -LiteralPath $Resolved) {
        Write-Host ("  {0}=present:{1}" -f $Label, $Resolved)
        return
    }
    if ($Required) {
        throw "doctor_failed: missing required path: $Label path=$Resolved"
    }
    Write-Host ("  {0}=missing:{1}" -f $Label, $Resolved)
}

function Get-ExpectedTokens {
    return @(
        "resident-elf-qemu: begin",
        "resident-elf-qemu: backend=virtual_m7 machine=mps2-an500 cpu=cortex-m7",
        "resident-elf-qemu: backend-capabilities capabilities=console,time,display,input,storage,app_exit storage=readonly afe=unsupported",
        "resident-elf-qemu: backend-scope proves=elf_loader,app_runtime,charm_app_api,capability_backend,received_image,packetstream,store_v1_semantics does_not_prove=h747_usb_cdc,h747_qspi,h747_emmc,h747_fmc_sdram,h747_hal_init,h747_mpu_cache,h747_pinmux",
        "resident-elf-qemu: backend-contract time=deterministic_tick start_ms=1000 step_ms=17 reset_per_run=1",
        "resident-elf-qemu: backend-contract display=framebuffer width=16 height=16 stride=64 format=argb8888 frame_bytes=1024 evidence=frame_signatures,frame_dumps,frame_ppm,gui_timeline",
        "resident-elf-qemu: backend-contract input=deterministic_sequence sample_count=4 pointer_max=15,15 wraps=1 evidence=input_trace,gui_timeline",
        "resident-elf-qemu: backend-contract storage=virtual_readonly_files file_count=3 fd_base=3 fd_slots=4 write_policy=unsupported evidence=storage_trace",
        "resident-elf-qemu: backend-contract app_exit=notification_counter overrides_return=0",
        "resident-elf-qemu: backend-self-check api=1 display=1 input=1 storage=1 afe=1 app_exit=1 result=ok",
        "resident-elf-qemu: backend-reset-self-check counters=1 display=1 time=1 input=1 storage=1 result=ok",
        "resident-elf-qemu: run-region base=0x20080000 expected=0x20080000 size=65536",
        "resident-elf-qemu: stage-cache bytes=16384",
        "resident-elf-qemu: packetstream-buffers storage=16384 transport=2048 stream=32768 received=16384",
        "resident-elf-qemu: packetstream fragmentation=ok packet_chunk=257 ingress=1,27,113,256,512 payload=5132",
        "resident-elf-qemu: store entries=31 bytes=",
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
        "afe_error_app: unsupported afe preserved buffer",
        "resident-elf-qemu: app afe_error_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps afe_error_app console=48 time=0 describe=0 present=0 input=0 exit=0",
        "storage=0/0/0/0 storage_bytes=0 afe=1/1",
        "resident-elf-qemu: store stage name=afe_error_app code=ok format=elf size=",
        "resident-elf-qemu: app store:afe_error_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps store:afe_error_app console=48 time=0 describe=0 present=0 input=0 exit=0",
        "bss_app: zero-fill ok",
        "resident-elf-qemu: app bss_app stage=exit code=ok exit=0",
        "resident-elf-qemu: capacity bss_app needed=513 free=65023 fits=1 region=65536 probe=ok",
        "resident-elf-qemu: caps bss_app console=22 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: store stage name=bss_app code=ok format=elf size=",
        "resident-elf-qemu: app store:bss_app stage=exit code=ok exit=0",
        "resident-elf-qemu: capacity store:bss_app needed=513 free=65023 fits=1 region=65536 probe=ok",
        "resident-elf-qemu: caps store:bss_app console=22 time=0 describe=0 present=0 input=0 exit=0",
        "console_error_app: null write rejected",
        "resident-elf-qemu: app console_error_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps console_error_app console=39 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: app store:console_error_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps store:console_error_app console=39 time=0 describe=0 present=0 input=0 exit=0",
        "data_app: data-init ok checksum=50",
        "resident-elf-qemu: app data_app stage=exit code=ok exit=0",
        "resident-elf-qemu: store stage name=data_app code=ok format=elf size=",
        "resident-elf-qemu: app store:data_app stage=exit code=ok exit=0",
        "resident-elf-qemu: app.exit code=7",
        "resident-elf-qemu: app exit_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps exit_app console=0 time=0 describe=0 present=0 input=0 exit=1",
        "resident-elf-qemu: app store:exit_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps store:exit_app console=0 time=0 describe=0 present=0 input=0 exit=1",
        "resident-elf-qemu: app exit_error_app stage=exit code=ok exit=42",
        "resident-elf-qemu: caps exit_error_app console=0 time=0 describe=0 present=0 input=0 exit=1",
        "resident-elf-qemu: app store:exit_error_app stage=exit code=ok exit=42",
        "resident-elf-qemu: caps store:exit_error_app console=0 time=0 describe=0 present=0 input=0 exit=1",
        "resident-elf-qemu: app.exit code=-3",
        "resident-elf-qemu: app exit_negative_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps exit_negative_app console=0 time=0 describe=0 present=0 input=0 exit=1",
        "resident-elf-qemu: app store:exit_negative_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps store:exit_negative_app console=0 time=0 describe=0 present=0 input=0 exit=1",
        "resident-elf-qemu: app return_negative_app stage=exit code=ok exit=-5",
        "resident-elf-qemu: caps return_negative_app console=0 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: app store:return_negative_app stage=exit code=ok exit=-5",
        "resident-elf-qemu: caps store:return_negative_app console=0 time=0 describe=0 present=0 input=0 exit=0",
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
        "storage_close_error_app: close reuse ok",
        "resident-elf-qemu: storage close fd=3 code=unsupported",
        "resident-elf-qemu: app storage_close_error_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps storage_close_error_app console=40 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=2/1/0/3 storage_bytes=1",
        "resident-elf-qemu: app store:storage_close_error_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps store:storage_close_error_app console=40 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=2/1/0/3 storage_bytes=1",
        "storage_error_app: invalid read preserved cursor",
        "resident-elf-qemu: storage read fd=3 code=invalid_argument requested=4 count=0 offset=0 remaining=27",
        "resident-elf-qemu: app storage_error_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps storage_error_app console=49 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=1/2/0/1 storage_bytes=4",
        "resident-elf-qemu: app store:storage_error_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps store:storage_error_app console=49 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=1/2/0/1 storage_bytes=4",
        "storage_fd_exhaustion_app: fd slots exhausted and reused",
        "resident-elf-qemu: storage open path=/virtual/readme.txt code=io_error fd=-1",
        "resident-elf-qemu: app storage_fd_exhaustion_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps storage_fd_exhaustion_app console=57 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=6/1/0/5 storage_bytes=1",
        "resident-elf-qemu: app store:storage_fd_exhaustion_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps store:storage_fd_exhaustion_app console=57 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=6/1/0/5 storage_bytes=1",
        "storage_open_error_app: open errors preserve fd",
        "resident-elf-qemu: storage open path=<null> code=invalid_argument fd=-1",
        "resident-elf-qemu: storage open path=/virtual/readme.txt code=unsupported fd=-1",
        "resident-elf-qemu: app storage_open_error_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps storage_open_error_app console=48 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=4/1/0/1 storage_bytes=1",
        "resident-elf-qemu: app store:storage_open_error_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps store:storage_open_error_app console=48 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=4/1/0/1 storage_bytes=1",
        "storage_write_error_app: readonly write preserved cursor",
        "resident-elf-qemu: storage write fd=3 code=unsupported requested=1 count=0 offset=1 remaining=26",
        "resident-elf-qemu: app storage_write_error_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps storage_write_error_app console=57 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=1/2/1/1 storage_bytes=2",
        "resident-elf-qemu: app store:storage_write_error_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps store:storage_write_error_app console=57 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=1/2/1/1 storage_bytes=2",
        "storage_zero_io_app: zero io preserved cursor",
        "resident-elf-qemu: storage read fd=3 code=ok requested=0 count=0 offset=0 remaining=27",
        "resident-elf-qemu: storage write fd=3 code=unsupported requested=0 count=0 offset=1 remaining=26",
        "resident-elf-qemu: app storage_zero_io_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps storage_zero_io_app console=46 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=1/3/1/1 storage_bytes=2",
        "resident-elf-qemu: app store:storage_zero_io_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps store:storage_zero_io_app console=46 time=0 describe=0 present=0 input=0 exit=0 display_checksum=0 display_checksum_total=0 storage=1/3/1/1 storage_bytes=2",
        "display_describe_error_app: null describe rejected",
        "resident-elf-qemu: app display_describe_error_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps display_describe_error_app console=51 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: app store:display_describe_error_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps store:display_describe_error_app console=51 time=0 describe=0 present=0 input=0 exit=0",
        "display_error_app: invalid present rejected",
        "resident-elf-qemu: display present bytes=1020 code=invalid_argument expected=1024",
        "resident-elf-qemu: app display_error_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps display_error_app console=44 time=0 describe=1 present=1 input=0 exit=0 display_checksum=0 display_checksum_total=0",
        "display_hash=0x00000000 display_hash_total=0x00000000 display_frame=0",
        "resident-elf-qemu: app store:display_error_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps store:display_error_app console=44 time=0 describe=1 present=1 input=0 exit=0 display_checksum=0 display_checksum_total=0",
        "display_null_present_app: null present rejected",
        "resident-elf-qemu: display present bytes=1024 code=invalid_argument expected=1024",
        "resident-elf-qemu: app display_null_present_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps display_null_present_app console=48 time=0 describe=1 present=1 input=0 exit=0 display_checksum=0 display_checksum_total=0",
        "resident-elf-qemu: app store:display_null_present_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps store:display_null_present_app console=48 time=0 describe=1 present=1 input=0 exit=0 display_checksum=0 display_checksum_total=0",
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
        "input_error_app: null poll rejected",
        "resident-elf-qemu: input poll code=invalid_argument",
        "resident-elf-qemu: app input_error_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps input_error_app console=36 time=0 describe=0 present=0 input=1 exit=0",
        "input_checksum=0 input_last=0,0,0",
        "resident-elf-qemu: app store:input_error_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps store:input_error_app console=36 time=0 describe=0 present=0 input=1 exit=0",
        "input_sequence_app: polls=4 checksum=114",
        "resident-elf-qemu: input poll encoder1=0 pointer=6,8 max=15,15 detected=1 down=0",
        "resident-elf-qemu: app input_sequence_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps input_sequence_app console=41 time=0 describe=0 present=0 input=4 exit=0",
        "input_checksum=114 input_last=6,8,0",
        "resident-elf-qemu: app store:input_sequence_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps store:input_sequence_app console=41 time=0 describe=0 present=0 input=4 exit=0",
        "input_wrap_app: polls=6 checksum=169",
        "resident-elf-qemu: input poll encoder1=0 pointer=4,6 max=15,15 detected=1 down=1",
        "resident-elf-qemu: app input_wrap_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps input_wrap_app console=37 time=0 describe=0 present=0 input=6 exit=0",
        "input_checksum=169 input_last=4,6,1",
        "resident-elf-qemu: app store:input_wrap_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps store:input_wrap_app console=37 time=0 describe=0 present=0 input=6 exit=0",
        "resident-elf-qemu: app time_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps time_app console=0 time=2 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: app store:time_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps store:time_app console=0 time=2 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: app time_sequence_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps time_sequence_app console=0 time=4 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: app store:time_sequence_app stage=exit code=ok exit=0",
        "resident-elf-qemu: caps store:time_sequence_app console=0 time=4 describe=0 present=0 input=0 exit=0",
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
        "resident-elf-qemu: app bad_ident_version_app stage=load code=load_failed exit=0",
        "resident-elf-qemu: load bad_ident_version_app format=elf probe=bad_header",
        "resident-elf-qemu: capacity bad_ident_version_app needed=0 free=65536 fits=1 region=65536 probe=bad_header",
        "resident-elf-qemu: caps bad_ident_version_app console=0 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: app bad_type_app stage=load code=load_failed exit=0",
        "resident-elf-qemu: load bad_type_app format=elf probe=bad_header",
        "resident-elf-qemu: capacity bad_type_app needed=0 free=65536 fits=1 region=65536 probe=bad_header",
        "resident-elf-qemu: caps bad_type_app console=0 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: app bad_machine_app stage=load code=load_failed exit=0",
        "resident-elf-qemu: load bad_machine_app format=elf probe=bad_header",
        "resident-elf-qemu: capacity bad_machine_app needed=0 free=65536 fits=1 region=65536 probe=bad_header",
        "resident-elf-qemu: caps bad_machine_app console=0 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: app bad_version_app stage=load code=load_failed exit=0",
        "resident-elf-qemu: load bad_version_app format=elf probe=bad_header",
        "resident-elf-qemu: capacity bad_version_app needed=0 free=65536 fits=1 region=65536 probe=bad_header",
        "resident-elf-qemu: caps bad_version_app console=0 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: app bad_ehsize_app stage=load code=load_failed exit=0",
        "resident-elf-qemu: load bad_ehsize_app format=elf probe=bad_header",
        "resident-elf-qemu: capacity bad_ehsize_app needed=0 free=65536 fits=1 region=65536 probe=bad_header",
        "resident-elf-qemu: caps bad_ehsize_app console=0 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: app bad_phentsize_app stage=load code=load_failed exit=0",
        "resident-elf-qemu: load bad_phentsize_app format=elf probe=bad_program_header",
        "resident-elf-qemu: capacity bad_phentsize_app needed=0 free=65536 fits=1 region=65536 probe=bad_program_header",
        "resident-elf-qemu: caps bad_phentsize_app console=0 time=0 describe=0 present=0 input=0 exit=0",
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
        "resident-elf-qemu: app wrong_link_base_app stage=load code=load_failed exit=0",
        "resident-elf-qemu: load wrong_link_base_app format=elf probe=ok link_base=0x20081000 expected_base=0x20080000",
        "base_match=0",
        "resident-elf-qemu: capacity wrong_link_base_app needed=270 free=65266 fits=1 region=65536 probe=ok",
        "resident-elf-qemu: caps wrong_link_base_app console=0 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: app too_large_app stage=load code=load_failed exit=0",
        "resident-elf-qemu: load too_large_app format=elf probe=load_buffer_too_small",
        "resident-elf-qemu: capacity too_large_app needed=82176 free=0 fits=0 region=65536 probe=load_buffer_too_small",
        "resident-elf-qemu: caps too_large_app console=0 time=0 describe=0 present=0 input=0 exit=0",
        "resident-elf-qemu: app unaligned_load_buffer_app stage=load code=load_failed exit=0",
        "resident-elf-qemu: load unaligned_load_buffer_app format=elf probe=load_buffer_unaligned",
        "resident-elf-qemu: capacity unaligned_load_buffer_app needed=0 free=65536 fits=1 region=65536 probe=load_buffer_unaligned",
        "resident-elf-qemu: caps unaligned_load_buffer_app console=0 time=0 describe=0 present=0 input=0 exit=0",
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
        'resident-elf-qemu: app (\S+) stage=(\S+) code=(\S+) exit=(-?\d+)')
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
        'resident-elf-qemu: app ([^ ]+) stage=([^ ]+) code=([^ ]+) exit=(-?\d+)')
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
        'resident-elf-qemu: app ([^ ]+) stage=([^ ]+) code=([^ ]+) exit=(-?\d+)')
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
    $WriteRegex = [System.Text.RegularExpressions.Regex]::new(
        'resident-elf-qemu: storage write fd=(-?\d+) code=([^ ]+) requested=(\d+) count=(\d+) offset=(\d+) remaining=(\d+)')
    $CloseRegex = [System.Text.RegularExpressions.Regex]::new(
        'resident-elf-qemu: storage close fd=(-?\d+) code=([^ ]+)')
    $AppRegex = [System.Text.RegularExpressions.Regex]::new(
        'resident-elf-qemu: app ([^ ]+) stage=([^ ]+) code=([^ ]+) exit=(-?\d+)')
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
                $Write = $WriteRegex.Match($Line)
                if ($Write.Success) {
                    $Event = [pscustomobject]@{
                        index = $Events.Count + 1
                        op = "write"
                        path = ""
                        fd = [int]$Write.Groups[1].Value
                        code = $Write.Groups[2].Value
                        size = 0
                        requested = [int]$Write.Groups[3].Value
                        count = [int]$Write.Groups[4].Value
                        offset = [int]$Write.Groups[5].Value
                        remaining = [int]$Write.Groups[6].Value
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
                "store:storage_catalog_app",
                "storage_close_error_app",
                "store:storage_close_error_app",
                "storage_error_app",
                "store:storage_error_app",
                "storage_fd_exhaustion_app",
                "store:storage_fd_exhaustion_app",
                "storage_open_error_app",
                "store:storage_open_error_app",
                "storage_write_error_app",
                "store:storage_write_error_app",
                "storage_zero_io_app",
                "store:storage_zero_io_app"
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
    if ([int]$Capture.event_count -ne 24 -or [int]$Capture.run_count -ne 8) {
        throw "input_trace_validate_failed: event/run count mismatch"
    }
    $Events = @($Capture.events)
    if ($Events.Count -ne 24) {
        throw "input_trace_validate_failed: events array count=$($Events.Count), expected 24"
    }
    $Runs = @($Capture.runs)
    $SeqEncoder1 = @(1, 0, -1, 0)
    $SeqX = @(3, 4, 5, 6)
    $SeqY = @(5, 6, 7, 8)
    $SeqDown = @(0, 1, 1, 0)
    $WrapEncoder1 = @(1, 0, -1, 0, 1, 0)
    $WrapX = @(3, 4, 5, 6, 3, 4)
    $WrapY = @(5, 6, 7, 8, 5, 6)
    $WrapDown = @(0, 1, 1, 0, 0, 1)
    Assert-InputTraceRun -Runs $Runs -Name "input_sequence_app" -Encoder1 $SeqEncoder1 -PointerX $SeqX -PointerY $SeqY -Down $SeqDown
    Assert-InputTraceRun -Runs $Runs -Name "store:input_sequence_app" -Encoder1 $SeqEncoder1 -PointerX $SeqX -PointerY $SeqY -Down $SeqDown
    Assert-InputTraceRun -Runs $Runs -Name "input_wrap_app" -Encoder1 $WrapEncoder1 -PointerX $WrapX -PointerY $WrapY -Down $WrapDown
    Assert-InputTraceRun -Runs $Runs -Name "store:input_wrap_app" -Encoder1 $WrapEncoder1 -PointerX $WrapX -PointerY $WrapY -Down $WrapDown
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
        [int[]]$Counts,
        [string[]]$Codes = @(),
        [int[]]$Offsets = @(),
        [int[]]$Remainings = @()
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
        $ExpectedCode = if ($Codes.Count -gt $i) { $Codes[$i] } else { "ok" }
        if ([string]$Event.op -ne $Ops[$i] -or
            [int]$Event.fd -ne $Fds[$i] -or
            [int]$Event.count -ne $Counts[$i] -or
            [string]$Event.code -ne $ExpectedCode) {
            throw "storage_trace_validate_failed: run $Name event $($i + 1) mismatch"
        }
        if (-not [string]::IsNullOrWhiteSpace($Paths[$i]) -and [string]$Event.path -ne $Paths[$i]) {
            throw "storage_trace_validate_failed: run $Name event $($i + 1) path=$($Event.path), expected $($Paths[$i])"
        }
        if ($Offsets.Count -gt $i -and [int]$Event.offset -ne $Offsets[$i]) {
            throw "storage_trace_validate_failed: run $Name event $($i + 1) offset=$($Event.offset), expected $($Offsets[$i])"
        }
        if ($Remainings.Count -gt $i -and [int]$Event.remaining -ne $Remainings[$i]) {
            throw "storage_trace_validate_failed: run $Name event $($i + 1) remaining=$($Event.remaining), expected $($Remainings[$i])"
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
    if ([int]$Capture.event_count -ne 130 -or [int]$Capture.run_count -ne 18) {
        throw "storage_trace_validate_failed: event/run count mismatch"
    }
    $Events = @($Capture.events)
    if ($Events.Count -ne 130) {
        throw "storage_trace_validate_failed: events array count=$($Events.Count), expected 130"
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

    $StorageCloseErrorOps = @("open", "close", "close", "open", "read", "close")
    $StorageCloseErrorPaths = @("/virtual/readme.txt", "", "", "/virtual/readme.txt", "", "")
    $StorageCloseErrorFds = @(3, 3, 3, 3, 3, 3)
    $StorageCloseErrorCounts = @(0, 0, 0, 0, 1, 0)
    $StorageCloseErrorCodes = @("ok", "ok", "unsupported", "ok", "ok", "ok")
    $StorageCloseErrorOffsets = @(0, 0, 0, 0, 0, 0)
    $StorageCloseErrorRemainings = @(0, 0, 0, 0, 26, 0)
    Assert-StorageTraceRun -Runs $Runs -Name "storage_close_error_app" -Ops $StorageCloseErrorOps -Paths $StorageCloseErrorPaths -Fds $StorageCloseErrorFds -Counts $StorageCloseErrorCounts -Codes $StorageCloseErrorCodes -Offsets $StorageCloseErrorOffsets -Remainings $StorageCloseErrorRemainings
    Assert-StorageTraceRun -Runs $Runs -Name "store:storage_close_error_app" -Ops $StorageCloseErrorOps -Paths $StorageCloseErrorPaths -Fds $StorageCloseErrorFds -Counts $StorageCloseErrorCounts -Codes $StorageCloseErrorCodes -Offsets $StorageCloseErrorOffsets -Remainings $StorageCloseErrorRemainings

    $StorageErrorOps = @("open", "read", "read", "close")
    $StorageErrorPaths = @("/virtual/readme.txt", "", "", "")
    $StorageErrorFds = @(3, 3, 3, 3)
    $StorageErrorCounts = @(0, 0, 4, 0)
    $StorageErrorCodes = @("ok", "invalid_argument", "ok", "ok")
    $StorageErrorOffsets = @(0, 0, 0, 0)
    $StorageErrorRemainings = @(0, 27, 23, 0)
    Assert-StorageTraceRun -Runs $Runs -Name "storage_error_app" -Ops $StorageErrorOps -Paths $StorageErrorPaths -Fds $StorageErrorFds -Counts $StorageErrorCounts -Codes $StorageErrorCodes -Offsets $StorageErrorOffsets -Remainings $StorageErrorRemainings
    Assert-StorageTraceRun -Runs $Runs -Name "store:storage_error_app" -Ops $StorageErrorOps -Paths $StorageErrorPaths -Fds $StorageErrorFds -Counts $StorageErrorCounts -Codes $StorageErrorCodes -Offsets $StorageErrorOffsets -Remainings $StorageErrorRemainings

    $StorageFdExhaustionOps = @("open", "open", "open", "open", "open", "close", "open", "read", "close", "close", "close", "close")
    $StorageFdExhaustionPaths = @("/virtual/readme.txt", "/virtual/readme.txt", "/virtual/alpha.txt", "/virtual/beta.bin", "/virtual/readme.txt", "", "/virtual/readme.txt", "", "", "", "", "")
    $StorageFdExhaustionFds = @(3, 4, 5, 6, -1, 4, 4, 4, 4, 6, 5, 3)
    $StorageFdExhaustionCounts = @(0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0)
    $StorageFdExhaustionCodes = @("ok", "ok", "ok", "ok", "io_error", "ok", "ok", "ok", "ok", "ok", "ok", "ok")
    $StorageFdExhaustionOffsets = @(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
    $StorageFdExhaustionRemainings = @(0, 0, 0, 0, 0, 0, 0, 26, 0, 0, 0, 0)
    Assert-StorageTraceRun -Runs $Runs -Name "storage_fd_exhaustion_app" -Ops $StorageFdExhaustionOps -Paths $StorageFdExhaustionPaths -Fds $StorageFdExhaustionFds -Counts $StorageFdExhaustionCounts -Codes $StorageFdExhaustionCodes -Offsets $StorageFdExhaustionOffsets -Remainings $StorageFdExhaustionRemainings
    Assert-StorageTraceRun -Runs $Runs -Name "store:storage_fd_exhaustion_app" -Ops $StorageFdExhaustionOps -Paths $StorageFdExhaustionPaths -Fds $StorageFdExhaustionFds -Counts $StorageFdExhaustionCounts -Codes $StorageFdExhaustionCodes -Offsets $StorageFdExhaustionOffsets -Remainings $StorageFdExhaustionRemainings

    $StorageOpenErrorOps = @("open", "open", "open", "open", "read", "close")
    $StorageOpenErrorPaths = @("<null>", "/virtual/readme.txt", "/virtual/readme.txt", "/virtual/readme.txt", "", "")
    $StorageOpenErrorFds = @(-1, -1, -1, 3, 3, 3)
    $StorageOpenErrorCounts = @(0, 0, 0, 0, 1, 0)
    $StorageOpenErrorCodes = @("invalid_argument", "unsupported", "unsupported", "ok", "ok", "ok")
    $StorageOpenErrorOffsets = @(0, 0, 0, 0, 0, 0)
    $StorageOpenErrorRemainings = @(0, 0, 0, 0, 26, 0)
    Assert-StorageTraceRun -Runs $Runs -Name "storage_open_error_app" -Ops $StorageOpenErrorOps -Paths $StorageOpenErrorPaths -Fds $StorageOpenErrorFds -Counts $StorageOpenErrorCounts -Codes $StorageOpenErrorCodes -Offsets $StorageOpenErrorOffsets -Remainings $StorageOpenErrorRemainings
    Assert-StorageTraceRun -Runs $Runs -Name "store:storage_open_error_app" -Ops $StorageOpenErrorOps -Paths $StorageOpenErrorPaths -Fds $StorageOpenErrorFds -Counts $StorageOpenErrorCounts -Codes $StorageOpenErrorCodes -Offsets $StorageOpenErrorOffsets -Remainings $StorageOpenErrorRemainings

    $StorageWriteErrorOps = @("open", "read", "write", "read", "close")
    $StorageWriteErrorPaths = @("/virtual/readme.txt", "", "", "", "")
    $StorageWriteErrorFds = @(3, 3, 3, 3, 3)
    $StorageWriteErrorCounts = @(0, 1, 0, 1, 0)
    $StorageWriteErrorCodes = @("ok", "ok", "unsupported", "ok", "ok")
    $StorageWriteErrorOffsets = @(0, 0, 1, 1, 0)
    $StorageWriteErrorRemainings = @(0, 26, 26, 25, 0)
    Assert-StorageTraceRun -Runs $Runs -Name "storage_write_error_app" -Ops $StorageWriteErrorOps -Paths $StorageWriteErrorPaths -Fds $StorageWriteErrorFds -Counts $StorageWriteErrorCounts -Codes $StorageWriteErrorCodes -Offsets $StorageWriteErrorOffsets -Remainings $StorageWriteErrorRemainings
    Assert-StorageTraceRun -Runs $Runs -Name "store:storage_write_error_app" -Ops $StorageWriteErrorOps -Paths $StorageWriteErrorPaths -Fds $StorageWriteErrorFds -Counts $StorageWriteErrorCounts -Codes $StorageWriteErrorCodes -Offsets $StorageWriteErrorOffsets -Remainings $StorageWriteErrorRemainings

    $StorageZeroIoOps = @("open", "read", "read", "write", "read", "close")
    $StorageZeroIoPaths = @("/virtual/readme.txt", "", "", "", "", "")
    $StorageZeroIoFds = @(3, 3, 3, 3, 3, 3)
    $StorageZeroIoCounts = @(0, 0, 1, 0, 1, 0)
    $StorageZeroIoCodes = @("ok", "ok", "ok", "unsupported", "ok", "ok")
    $StorageZeroIoOffsets = @(0, 0, 0, 1, 1, 0)
    $StorageZeroIoRemainings = @(0, 27, 26, 26, 25, 0)
    Assert-StorageTraceRun -Runs $Runs -Name "storage_zero_io_app" -Ops $StorageZeroIoOps -Paths $StorageZeroIoPaths -Fds $StorageZeroIoFds -Counts $StorageZeroIoCounts -Codes $StorageZeroIoCodes -Offsets $StorageZeroIoOffsets -Remainings $StorageZeroIoRemainings
    Assert-StorageTraceRun -Runs $Runs -Name "store:storage_zero_io_app" -Ops $StorageZeroIoOps -Paths $StorageZeroIoPaths -Fds $StorageZeroIoFds -Counts $StorageZeroIoCounts -Codes $StorageZeroIoCodes -Offsets $StorageZeroIoOffsets -Remainings $StorageZeroIoRemainings

    foreach ($Name in @("unsupported_caps_app", "store:unsupported_caps_app")) {
        $UnsupportedOps = @("open", "read", "write", "close")
        $UnsupportedPaths = @("/missing", "", "", "")
        $UnsupportedFds = @(-1, -1, -1, -1)
        $UnsupportedCounts = @(0, 0, 0, 0)
        $UnsupportedCodes = @("unsupported", "unsupported", "unsupported", "unsupported")
        $UnsupportedOffsets = @(0, 0, 0, 0)
        $UnsupportedRemainings = @(0, 0, 0, 0)
        Assert-StorageTraceRun -Runs $Runs -Name $Name -Ops $UnsupportedOps -Paths $UnsupportedPaths -Fds $UnsupportedFds -Counts $UnsupportedCounts -Codes $UnsupportedCodes -Offsets $UnsupportedOffsets -Remainings $UnsupportedRemainings
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

function Convert-BackendContractFlag {
    param(
        [string]$Value,
        [string]$Field
    )

    if ($Value -eq "1") {
        return $true
    }
    if ($Value -eq "0") {
        return $false
    }
    throw "domain_summary_failed: bad backend contract flag $Field=$Value"
}

function Split-BackendContractEvidence {
    param([string]$Value)

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return @()
    }
    return @($Value -split "," | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
}

function Get-BackendScopeFromText {
    param([string]$Text)

    $Proves = Split-BackendContractEvidence -Value (Get-RegexGroupValue `
            -Text $Text `
            -Pattern 'resident-elf-qemu: backend-scope proves=([a-z0-9_,]+)' `
            -ErrorPrefix "domain_summary_failed")
    $DoesNotProve = Split-BackendContractEvidence -Value (Get-RegexGroupValue `
            -Text $Text `
            -Pattern 'resident-elf-qemu: backend-scope proves=[a-z0-9_,]+ does_not_prove=([a-z0-9_,]+)' `
            -ErrorPrefix "domain_summary_failed")

    return [pscustomobject]@{
        schema = "charm.resident_elf_qemu.backend_scope.v1"
        proves = @($Proves)
        does_not_prove = @($DoesNotProve)
    }
}

function Get-BackendIdentityFromText {
    param([string]$Text)

    $RuntimeDomain = Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend=([a-z0-9_]+)' -ErrorPrefix "domain_summary_failed"
    $Machine = Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend=[a-z0-9_]+ machine=([a-z0-9_-]+)' -ErrorPrefix "domain_summary_failed"
    $Cpu = Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend=[a-z0-9_]+ machine=[a-z0-9_-]+ cpu=([a-z0-9_-]+)' -ErrorPrefix "domain_summary_failed"
    $Capabilities = Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-capabilities capabilities=([a-z0-9_,]+)' -ErrorPrefix "domain_summary_failed"
    $StorageMode = Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-capabilities capabilities=[a-z0-9_,]+ storage=([a-z0-9_]+)' -ErrorPrefix "domain_summary_failed"
    $AfeMode = Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-capabilities capabilities=[a-z0-9_,]+ storage=[a-z0-9_]+ afe=([a-z0-9_]+)' -ErrorPrefix "domain_summary_failed"

    return [pscustomobject]@{
        runtime_domain = $RuntimeDomain
        machine = $Machine
        cpu = $Cpu
        capabilities = $Capabilities
        storage = $StorageMode
        afe = $AfeMode
    }
}

function Get-BackendContractFromText {
    param(
        [string]$Text,
        [string]$RuntimeDomain,
        [string]$Capabilities,
        [string]$StorageMode,
        [string]$AfeMode
    )

    $TimeKind = Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-contract time=([a-z0-9_]+)' -ErrorPrefix "domain_summary_failed"
    $TimeStartMs = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-contract time=[a-z0-9_]+ start_ms=(\d+)' -ErrorPrefix "domain_summary_failed")
    $TimeStepMs = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-contract time=[a-z0-9_]+ start_ms=\d+ step_ms=(\d+)' -ErrorPrefix "domain_summary_failed")
    $TimeReset = Convert-BackendContractFlag `
        -Value (Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-contract time=[a-z0-9_]+ start_ms=\d+ step_ms=\d+ reset_per_run=([01])' -ErrorPrefix "domain_summary_failed") `
        -Field "time.reset_per_run"

    $DisplayKind = Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-contract display=([a-z0-9_]+)' -ErrorPrefix "domain_summary_failed"
    $DisplayWidth = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-contract display=[a-z0-9_]+ width=(\d+)' -ErrorPrefix "domain_summary_failed")
    $DisplayHeight = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-contract display=[a-z0-9_]+ width=\d+ height=(\d+)' -ErrorPrefix "domain_summary_failed")
    $DisplayStride = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-contract display=[a-z0-9_]+ width=\d+ height=\d+ stride=(\d+)' -ErrorPrefix "domain_summary_failed")
    $DisplayFormat = Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-contract display=[a-z0-9_]+ width=\d+ height=\d+ stride=\d+ format=([a-z0-9_]+)' -ErrorPrefix "domain_summary_failed"
    $DisplayFrameBytes = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-contract display=[a-z0-9_]+ width=\d+ height=\d+ stride=\d+ format=[a-z0-9_]+ frame_bytes=(\d+)' -ErrorPrefix "domain_summary_failed")
    $DisplayEvidence = Split-BackendContractEvidence -Value (Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-contract display=[a-z0-9_]+ width=\d+ height=\d+ stride=\d+ format=[a-z0-9_]+ frame_bytes=\d+ evidence=([a-z0-9_,]+)' -ErrorPrefix "domain_summary_failed")

    $InputKind = Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-contract input=([a-z0-9_]+)' -ErrorPrefix "domain_summary_failed"
    $InputSampleCount = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-contract input=[a-z0-9_]+ sample_count=(\d+)' -ErrorPrefix "domain_summary_failed")
    $InputPointerMaxX = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-contract input=[a-z0-9_]+ sample_count=\d+ pointer_max=(\d+),' -ErrorPrefix "domain_summary_failed")
    $InputPointerMaxY = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-contract input=[a-z0-9_]+ sample_count=\d+ pointer_max=\d+,(\d+)' -ErrorPrefix "domain_summary_failed")
    $InputWraps = Convert-BackendContractFlag `
        -Value (Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-contract input=[a-z0-9_]+ sample_count=\d+ pointer_max=\d+,\d+ wraps=([01])' -ErrorPrefix "domain_summary_failed") `
        -Field "input.wraps"
    $InputEvidence = Split-BackendContractEvidence -Value (Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-contract input=[a-z0-9_]+ sample_count=\d+ pointer_max=\d+,\d+ wraps=[01] evidence=([a-z0-9_,]+)' -ErrorPrefix "domain_summary_failed")

    $StorageKind = Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-contract storage=([a-z0-9_]+)' -ErrorPrefix "domain_summary_failed"
    $StorageFileCount = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-contract storage=[a-z0-9_]+ file_count=(\d+)' -ErrorPrefix "domain_summary_failed")
    $StorageFdBase = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-contract storage=[a-z0-9_]+ file_count=\d+ fd_base=(\d+)' -ErrorPrefix "domain_summary_failed")
    $StorageFdSlots = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-contract storage=[a-z0-9_]+ file_count=\d+ fd_base=\d+ fd_slots=(\d+)' -ErrorPrefix "domain_summary_failed")
    $StorageWritePolicy = Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-contract storage=[a-z0-9_]+ file_count=\d+ fd_base=\d+ fd_slots=\d+ write_policy=([a-z0-9_]+)' -ErrorPrefix "domain_summary_failed"
    $StorageEvidence = Split-BackendContractEvidence -Value (Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-contract storage=[a-z0-9_]+ file_count=\d+ fd_base=\d+ fd_slots=\d+ write_policy=[a-z0-9_]+ evidence=([a-z0-9_,]+)' -ErrorPrefix "domain_summary_failed")

    $AppExitKind = Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-contract app_exit=([a-z0-9_]+)' -ErrorPrefix "domain_summary_failed"
    $AppExitOverridesReturn = Convert-BackendContractFlag `
        -Value (Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-contract app_exit=[a-z0-9_]+ overrides_return=([01])' -ErrorPrefix "domain_summary_failed") `
        -Field "app_exit.overrides_return"

    return [pscustomobject]@{
        kind = "virtual"
        runtime_domain = $RuntimeDomain
        capabilities = @($Capabilities -split ",")
        storage = $StorageMode
        afe = $AfeMode
        time = [pscustomobject]@{
            kind = $TimeKind
            start_ms = $TimeStartMs
            step_ms = $TimeStepMs
            reset_per_run = $TimeReset
        }
        display = [pscustomobject]@{
            kind = $DisplayKind
            width = $DisplayWidth
            height = $DisplayHeight
            stride_bytes = $DisplayStride
            format = $DisplayFormat
            frame_bytes = $DisplayFrameBytes
            evidence = @($DisplayEvidence)
        }
        input = [pscustomobject]@{
            kind = $InputKind
            sample_count = $InputSampleCount
            pointer_max_x = $InputPointerMaxX
            pointer_max_y = $InputPointerMaxY
            wraps = $InputWraps
            evidence = @($InputEvidence)
        }
        storage_media = [pscustomobject]@{
            kind = $StorageKind
            file_count = $StorageFileCount
            fd_base = $StorageFdBase
            fd_slots = $StorageFdSlots
            write_policy = $StorageWritePolicy
            evidence = @($StorageEvidence)
        }
        app_exit = [pscustomobject]@{
            kind = $AppExitKind
            overrides_return = $AppExitOverridesReturn
        }
    }
}

function Get-BackendSelfCheckFromText {
    param([string]$Text)

    $Api = Convert-BackendContractFlag `
        -Value (Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-self-check api=([01])' -ErrorPrefix "domain_summary_failed") `
        -Field "backend_self_check.api"
    $Display = Convert-BackendContractFlag `
        -Value (Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-self-check api=[01] display=([01])' -ErrorPrefix "domain_summary_failed") `
        -Field "backend_self_check.display"
    $Input = Convert-BackendContractFlag `
        -Value (Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-self-check api=[01] display=[01] input=([01])' -ErrorPrefix "domain_summary_failed") `
        -Field "backend_self_check.input"
    $Storage = Convert-BackendContractFlag `
        -Value (Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-self-check api=[01] display=[01] input=[01] storage=([01])' -ErrorPrefix "domain_summary_failed") `
        -Field "backend_self_check.storage"
    $Afe = Convert-BackendContractFlag `
        -Value (Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-self-check api=[01] display=[01] input=[01] storage=[01] afe=([01])' -ErrorPrefix "domain_summary_failed") `
        -Field "backend_self_check.afe"
    $AppExit = Convert-BackendContractFlag `
        -Value (Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-self-check api=[01] display=[01] input=[01] storage=[01] afe=[01] app_exit=([01])' -ErrorPrefix "domain_summary_failed") `
        -Field "backend_self_check.app_exit"
    $Result = Get-RegexGroupValue `
        -Text $Text `
        -Pattern 'resident-elf-qemu: backend-self-check api=[01] display=[01] input=[01] storage=[01] afe=[01] app_exit=[01] result=([a-z_]+)' `
        -ErrorPrefix "domain_summary_failed"

    return [pscustomobject]@{
        schema = "charm.resident_elf_qemu.backend_self_check.v1"
        api = $Api
        display = $Display
        input = $Input
        storage = $Storage
        afe = $Afe
        app_exit = $AppExit
        result = $Result
    }
}

function Get-BackendResetSelfCheckFromText {
    param([string]$Text)

    $Counters = Convert-BackendContractFlag `
        -Value (Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-reset-self-check counters=([01])' -ErrorPrefix "domain_summary_failed") `
        -Field "backend_reset_self_check.counters"
    $Display = Convert-BackendContractFlag `
        -Value (Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-reset-self-check counters=[01] display=([01])' -ErrorPrefix "domain_summary_failed") `
        -Field "backend_reset_self_check.display"
    $Time = Convert-BackendContractFlag `
        -Value (Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-reset-self-check counters=[01] display=[01] time=([01])' -ErrorPrefix "domain_summary_failed") `
        -Field "backend_reset_self_check.time"
    $Input = Convert-BackendContractFlag `
        -Value (Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-reset-self-check counters=[01] display=[01] time=[01] input=([01])' -ErrorPrefix "domain_summary_failed") `
        -Field "backend_reset_self_check.input"
    $Storage = Convert-BackendContractFlag `
        -Value (Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: backend-reset-self-check counters=[01] display=[01] time=[01] input=[01] storage=([01])' -ErrorPrefix "domain_summary_failed") `
        -Field "backend_reset_self_check.storage"
    $Result = Get-RegexGroupValue `
        -Text $Text `
        -Pattern 'resident-elf-qemu: backend-reset-self-check counters=[01] display=[01] time=[01] input=[01] storage=[01] result=([a-z_]+)' `
        -ErrorPrefix "domain_summary_failed"

    return [pscustomobject]@{
        schema = "charm.resident_elf_qemu.backend_reset_self_check.v1"
        counters = $Counters
        display = $Display
        time = $Time
        input = $Input
        storage = $Storage
        result = $Result
    }
}

function Get-QemuRuntimeDomainProfile {
    param(
        [string]$RuntimeDomain,
        [string]$Machine,
        [string]$Cpu,
        [object]$BackendContract,
        [object]$BackendScope
    )

    return [pscustomobject]@{
        schema = "charm.resident_elf_qemu.runtime_domain_profile.v1"
        domain = $RuntimeDomain
        machine = $Machine
        cpu = $Cpu
        kind = "virtual_board"
        proves = @($BackendScope.proves)
        does_not_prove = @($BackendScope.does_not_prove)
        app_model = "CharmAppApi"
        image_format = "elf"
        run_region = [pscustomobject]@{
            base = "0x20080000"
            size = 65536
            execute = $true
        }
        stage_cache = [pscustomobject]@{
            bytes = 16384
        }
        store_media = "memory"
        capability_provider = [pscustomobject]@{
            kind = "smoke_local_virtual_backend"
            capabilities = @($BackendContract.capabilities)
            storage = $BackendContract.storage
            afe = $BackendContract.afe
        }
    }
}

function Get-AppRunSummaryFromText {
    param([string]$Text)

    $RunRegex = [System.Text.RegularExpressions.Regex]::new(
        'resident-elf-qemu: app ([^ ]+) stage=([^ ]+) code=([^ ]+) exit=(-?\d+)')
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
        'resident-elf-qemu: load (\S+) format=(\S+) probe=(\S+) link_base=(0x[0-9a-f]+) expected_base=(0x[0-9a-f]+) entry=(0x[0-9a-f]+) entry_vaddr=(0x[0-9a-f]+) span=(\d+) segments=(\d+) base_match=(\d+)')
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
            link_base = $Match.Groups[4].Value
            link_base_numeric = (Convert-HexToInt64 -Value $Match.Groups[4].Value)
            expected_base = $Match.Groups[5].Value
            expected_base_numeric = (Convert-HexToInt64 -Value $Match.Groups[5].Value)
            entry = $Match.Groups[6].Value
            entry_numeric = (Convert-HexToInt64 -Value $Match.Groups[6].Value)
            entry_vaddr = $Match.Groups[7].Value
            entry_vaddr_numeric = (Convert-HexToInt64 -Value $Match.Groups[7].Value)
            span = [int]$Match.Groups[8].Value
            segments = [int]$Match.Groups[9].Value
            base_match = ([int]$Match.Groups[10].Value) -ne 0
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

function Split-QemuRunName {
    param([string]$Name)

    $Source = "direct"
    $App = $Name
    if ($Name.Contains(":")) {
        $Parts = $Name.Split(":", 2)
        $Source = $Parts[0]
        $App = $Parts[1]
    }
    return [pscustomobject]@{
        source = $Source
        app = $App
    }
}

function Get-QemuRunByName {
    param(
        [object[]]$Runs,
        [string]$Name
    )

    $Matches = @($Runs | Where-Object { $_.name -eq $Name })
    if ($Matches.Count -eq 0) {
        return $null
    }
    return $Matches[0]
}

function Get-QemuStageBySourceName {
    param(
        [object[]]$Stages,
        [string]$Source,
        [string]$Name
    )

    $Matches = @($Stages | Where-Object { $_.source -eq $Source -and $_.name -eq $Name })
    if ($Matches.Count -eq 0) {
        return $null
    }
    return $Matches[0]
}

function Get-QemuPacketstreamByName {
    param(
        [object[]]$Packetstreams,
        [string]$Name
    )

    $Matches = @($Packetstreams | Where-Object { $_.name -eq $Name })
    if ($Matches.Count -eq 0) {
        return $null
    }
    return $Matches[0]
}

function Get-QemuPrepareSummaryByName {
    param(
        [object]$Prepare,
        [string]$Name
    )

    if ($null -ne $Prepare -and [string]$Prepare.name -eq $Name) {
        return $Prepare
    }
    return $null
}

function Get-ElfRunEvidenceMatrix {
    param(
        [object[]]$Runs,
        [object[]]$Stages,
        [object[]]$Loads,
        [object[]]$Packetstreams,
        [object]$Prepare
    )

    $Evidence = @()
    foreach ($Load in @($Loads)) {
        $Name = [string]$Load.name
        $Parts = Split-QemuRunName -Name $Name
        $Source = [string]$Parts.source
        $App = [string]$Parts.app
        $Run = Get-QemuRunByName -Runs $Runs -Name $Name
        $Stage = if ($Source -eq "received" -or $Source -eq "store") {
            Get-QemuStageBySourceName -Stages $Stages -Source $Source -Name $App
        } else {
            $null
        }
        $Packetstream = if ($Source -eq "packetstream") {
            Get-QemuPacketstreamByName -Packetstreams $Packetstreams -Name $App
        } else {
            $null
        }
        $PrepareRun = if ($Source -eq "prepare") {
            Get-QemuPrepareSummaryByName -Prepare $Prepare -Name $Name
        } else {
            $null
        }

        $SourceStage = if ($Source -eq "direct") {
            "image"
        } elseif ($Source -eq "prepare") {
            if ($null -ne $PrepareRun) { [string]$PrepareRun.stage } else { "" }
        } elseif ($Source -eq "packetstream") {
            if ($null -ne $Packetstream) { [string]$Packetstream.receive_stage } else { "" }
        } elseif ($null -ne $Stage) {
            "stage"
        } else {
            ""
        }
        $SourceCode = if ($Source -eq "direct") {
            "ok"
        } elseif ($Source -eq "prepare") {
            if ($null -ne $PrepareRun) { [string]$PrepareRun.code } else { "" }
        } elseif ($Source -eq "packetstream") {
            if ($null -ne $Packetstream) { [string]$Packetstream.receive_code } else { "" }
        } elseif ($null -ne $Stage) {
            [string]$Stage.code
        } else {
            ""
        }
        $RunStage = if ($null -ne $Run) {
            [string]$Run.stage
        } elseif ($null -ne $PrepareRun) {
            [string]$PrepareRun.stage
        } else {
            ""
        }
        $RunCode = if ($null -ne $Run) {
            [string]$Run.code
        } elseif ($null -ne $PrepareRun) {
            [string]$PrepareRun.code
        } else {
            ""
        }
        $Exit = if ($null -ne $Run) { [int]$Run.exit } else { 0 }
        $Ready = if ($null -ne $Run) {
            ($Run.stage -eq "exit" -and $Run.code -eq "ok")
        } elseif ($null -ne $PrepareRun) {
            [bool]$PrepareRun.ready
        } else {
            $false
        }

        $Evidence += [pscustomobject]@{
            name = $Name
            app = $App
            source = $Source
            source_stage = $SourceStage
            source_code = $SourceCode
            format = [string]$Load.format
            load_probe = [string]$Load.probe
            load_stage = "load"
            link_base = [string]$Load.link_base
            expected_base = [string]$Load.expected_base
            entry = [string]$Load.entry
            entry_vaddr = [string]$Load.entry_vaddr
            span = [int]$Load.span
            segments = [int]$Load.segments
            base_match = [bool]$Load.base_match
            needed = [int]$Load.needed
            free = [int]$Load.free
            fits = [bool]$Load.fits
            region = [int]$Load.region
            capacity_probe = [string]$Load.capacity_probe
            run_stage = $RunStage
            run_code = $RunCode
            exit = $Exit
            ready = $Ready
        }
    }
    return @($Evidence)
}

function Get-QemuNegativeCases {
    return @(
        [pscustomobject]@{ name = "packetstream_crc_mismatch"; stage = "packetstream_verify"; code = "crc_mismatch" },
        [pscustomobject]@{ name = "received_too_large_app"; stage = "received_stage"; code = "buffer_too_small" },
        [pscustomobject]@{ name = "too_large_store_app"; stage = "store_stage"; code = "image_too_large" },
        [pscustomobject]@{ name = "bad_elf_magic_app"; stage = "load"; code = "load_failed" },
        [pscustomobject]@{ name = "packetstream_bad_elf_magic_app"; stage = "load"; code = "load_failed" },
        [pscustomobject]@{ name = "bad_header_app"; stage = "load"; code = "load_failed" },
        [pscustomobject]@{ name = "bad_class_app"; stage = "load"; code = "load_failed" },
        [pscustomobject]@{ name = "bad_endian_app"; stage = "load"; code = "load_failed" },
        [pscustomobject]@{ name = "bad_ident_version_app"; stage = "load"; code = "load_failed" },
        [pscustomobject]@{ name = "bad_type_app"; stage = "load"; code = "load_failed" },
        [pscustomobject]@{ name = "bad_machine_app"; stage = "load"; code = "load_failed" },
        [pscustomobject]@{ name = "bad_version_app"; stage = "load"; code = "load_failed" },
        [pscustomobject]@{ name = "bad_ehsize_app"; stage = "load"; code = "load_failed" },
        [pscustomobject]@{ name = "bad_phentsize_app"; stage = "load"; code = "load_failed" },
        [pscustomobject]@{ name = "bad_program_header_app"; stage = "load"; code = "load_failed" },
        [pscustomobject]@{ name = "truncated_payload_app"; stage = "load"; code = "load_failed" },
        [pscustomobject]@{ name = "no_load_segment_app"; stage = "load"; code = "load_failed" },
        [pscustomobject]@{ name = "entry_outside_segment_app"; stage = "load"; code = "load_failed" },
        [pscustomobject]@{ name = "overlapping_segments_app"; stage = "load"; code = "load_failed" },
        [pscustomobject]@{ name = "rwx_segment_app"; stage = "load"; code = "load_failed" },
        [pscustomobject]@{ name = "wrong_link_base_app"; stage = "load"; code = "load_failed" },
        [pscustomobject]@{ name = "too_large_app"; stage = "load"; code = "load_failed" },
        [pscustomobject]@{ name = "unaligned_load_buffer_app"; stage = "load"; code = "load_failed" },
        [pscustomobject]@{ name = "argv_overflow_app"; stage = "argv"; code = "argv_overflow" },
        [pscustomobject]@{ name = "abi_mismatch_app"; stage = "abi"; code = "abi_mismatch" }
    )
}

function Get-QemuFailureCategory {
    param([string]$Stage)

    switch ($Stage) {
        "packetstream_verify" { return "transport" }
        "received_stage" { return "stage" }
        "store_stage" { return "stage" }
        "load" { return "load" }
        "argv" { return "runtime" }
        "abi" { return "runtime" }
        default { return "unknown" }
    }
}

function Get-QemuFailureTaxonomy {
    param([object[]]$NegativeCases)

    $CategoryOrder = @("transport", "stage", "load", "runtime")
    $StageOrder = @("packetstream_verify", "received_stage", "store_stage", "load", "argv", "abi")

    $Categories = @()
    foreach ($Category in $CategoryOrder) {
        $Cases = @($NegativeCases | Where-Object { (Get-QemuFailureCategory -Stage ([string]$_.stage)) -eq $Category })
        $Categories += [pscustomobject]@{
            category = $Category
            count = $Cases.Count
            stages = @($Cases | ForEach-Object { [string]$_.stage } | Select-Object -Unique)
        }
    }

    $Stages = @()
    foreach ($Stage in $StageOrder) {
        $Cases = @($NegativeCases | Where-Object { $_.stage -eq $Stage })
        if ($Cases.Count -eq 0) {
            continue
        }
        $Stages += [pscustomobject]@{
            stage = $Stage
            category = Get-QemuFailureCategory -Stage $Stage
            count = $Cases.Count
            codes = @($Cases | ForEach-Object { [string]$_.code } | Select-Object -Unique)
            cases = @($Cases | ForEach-Object { [string]$_.name })
        }
    }

    return [pscustomobject]@{
        schema = "charm.resident_elf_qemu.failure_taxonomy.v1"
        total = @($NegativeCases).Count
        categories = $Categories
        stages = $Stages
    }
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
    if ($Text.Contains("resident-elf-qemu: caps time_sequence_app console=0 time=4")) {
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

function Get-CapabilityCounterSummaryFromText {
    param([string]$Text)

    $CapsRegex = [System.Text.RegularExpressions.Regex]::new(
        'resident-elf-qemu: caps ([^ ]+) console=(\d+) time=(\d+) describe=(\d+) present=(\d+) input=(\d+) exit=(\d+)(?: display_checksum=(\d+) display_checksum_total=(\d+) storage=(\d+)/(\d+)/(\d+)/(\d+) storage_bytes=(\d+)(?: afe=(\d+)/(\d+) input_checksum=(\d+) input_last=(-?\d+),(-?\d+),(\d+) display_hash=(0x[0-9a-fA-F]+) display_hash_total=(0x[0-9a-fA-F]+) display_frame=(\d+))?)?')
    $Counters = @()
    foreach ($Match in $CapsRegex.Matches($Text)) {
        $HasExtendedCounters = $Match.Groups[8].Success -and -not [string]::IsNullOrWhiteSpace($Match.Groups[8].Value)
        $HasResetCounters = $Match.Groups[15].Success -and -not [string]::IsNullOrWhiteSpace($Match.Groups[15].Value)
        $Counters += [pscustomobject]@{
            name = $Match.Groups[1].Value
            console = [int]$Match.Groups[2].Value
            time = [int]$Match.Groups[3].Value
            describe = [int]$Match.Groups[4].Value
            present = [int]$Match.Groups[5].Value
            input = [int]$Match.Groups[6].Value
            exit = [int]$Match.Groups[7].Value
            display_checksum = if ($HasExtendedCounters) { [int]$Match.Groups[8].Value } else { 0 }
            display_checksum_total = if ($HasExtendedCounters) { [int]$Match.Groups[9].Value } else { 0 }
            storage_open = if ($HasExtendedCounters) { [int]$Match.Groups[10].Value } else { 0 }
            storage_read = if ($HasExtendedCounters) { [int]$Match.Groups[11].Value } else { 0 }
            storage_write = if ($HasExtendedCounters) { [int]$Match.Groups[12].Value } else { 0 }
            storage_close = if ($HasExtendedCounters) { [int]$Match.Groups[13].Value } else { 0 }
            storage_bytes = if ($HasExtendedCounters) { [int]$Match.Groups[14].Value } else { 0 }
            afe_configure = if ($HasResetCounters) { [int]$Match.Groups[15].Value } else { 0 }
            afe_read = if ($HasResetCounters) { [int]$Match.Groups[16].Value } else { 0 }
            input_checksum = if ($HasResetCounters) { [int]$Match.Groups[17].Value } else { 0 }
            input_last_x = if ($HasResetCounters) { [int]$Match.Groups[18].Value } else { 0 }
            input_last_y = if ($HasResetCounters) { [int]$Match.Groups[19].Value } else { 0 }
            input_last_down = if ($HasResetCounters) { [int]$Match.Groups[20].Value } else { 0 }
            input_last = if ($HasResetCounters) { "{0},{1},{2}" -f ([int]$Match.Groups[18].Value), ([int]$Match.Groups[19].Value), ([int]$Match.Groups[20].Value) } else { "0,0,0" }
            display_hash = if ($HasResetCounters) { ([string]$Match.Groups[21].Value).ToLowerInvariant() } else { "0x00000000" }
            display_hash_total = if ($HasResetCounters) { ([string]$Match.Groups[22].Value).ToLowerInvariant() } else { "0x00000000" }
            display_frame = if ($HasResetCounters) { [int]$Match.Groups[23].Value } else { 0 }
        }
    }
    return @($Counters)
}

function Get-QemuCapsByName {
    param(
        [object[]]$CapabilityCounters,
        [string]$Name
    )

    $Matches = @($CapabilityCounters | Where-Object { $_.name -eq $Name })
    if ($Matches.Count -eq 0) {
        return $null
    }
    return $Matches[0]
}

function Test-QemuCapsEqual {
    param(
        [object]$Left,
        [object]$Right,
        [string[]]$Properties
    )

    if ($null -eq $Left -or $null -eq $Right) {
        return $false
    }
    foreach ($Property in $Properties) {
        if ([string]$Left.$Property -ne [string]$Right.$Property) {
            return $false
        }
    }
    return $true
}

function Test-QemuCapsExpected {
    param(
        [object]$Caps,
        [hashtable]$Expected
    )

    if ($null -eq $Caps) {
        return $false
    }
    foreach ($Key in $Expected.Keys) {
        if ([string]$Caps.$Key -ne [string]$Expected[$Key]) {
            return $false
        }
    }
    return $true
}

function Test-QemuRunExitOk {
    param(
        [object[]]$Runs,
        [string]$Name
    )

    $Run = Get-QemuRunByName -Runs $Runs -Name $Name
    return ($null -ne $Run -and $Run.stage -eq "exit" -and $Run.code -eq "ok" -and [int]$Run.exit -eq 0)
}

function Get-QemuTraceRunByName {
    param(
        [object]$Trace,
        [string]$Name
    )

    if ($null -eq $Trace) {
        return $null
    }
    $Matches = @(@($Trace.runs) | Where-Object { $_.name -eq $Name })
    if ($Matches.Count -eq 0) {
        return $null
    }
    return $Matches[0]
}

function Test-QemuStorageFdResetTraceRun {
    param([object]$Run)

    if ($null -eq $Run -or $Run.stage -ne "exit" -or $Run.code -ne "ok" -or [int]$Run.exit -ne 0) {
        return $false
    }
    $Events = @($Run.events)
    if ([int]$Run.event_count -ne 12 -or $Events.Count -ne 12) {
        return $false
    }
    $Expected = @(
        [pscustomobject]@{ index = 0; op = "open"; fd = 3; code = "ok"; path = "/virtual/readme.txt"; count = 0 },
        [pscustomobject]@{ index = 1; op = "open"; fd = 4; code = "ok"; path = "/virtual/readme.txt"; count = 0 },
        [pscustomobject]@{ index = 2; op = "open"; fd = 5; code = "ok"; path = "/virtual/alpha.txt"; count = 0 },
        [pscustomobject]@{ index = 3; op = "open"; fd = 6; code = "ok"; path = "/virtual/beta.bin"; count = 0 },
        [pscustomobject]@{ index = 4; op = "open"; fd = -1; code = "io_error"; path = "/virtual/readme.txt"; count = 0 },
        [pscustomobject]@{ index = 5; op = "close"; fd = 4; code = "ok"; path = ""; count = 0 },
        [pscustomobject]@{ index = 6; op = "open"; fd = 4; code = "ok"; path = "/virtual/readme.txt"; count = 0 },
        [pscustomobject]@{ index = 7; op = "read"; fd = 4; code = "ok"; path = ""; count = 1 },
        [pscustomobject]@{ index = 8; op = "close"; fd = 4; code = "ok"; path = ""; count = 0 },
        [pscustomobject]@{ index = 9; op = "close"; fd = 6; code = "ok"; path = ""; count = 0 },
        [pscustomobject]@{ index = 10; op = "close"; fd = 5; code = "ok"; path = ""; count = 0 },
        [pscustomobject]@{ index = 11; op = "close"; fd = 3; code = "ok"; path = ""; count = 0 }
    )
    foreach ($Item in $Expected) {
        $Event = $Events[[int]$Item.index]
        if ($Event.op -ne $Item.op -or
            [int]$Event.fd -ne [int]$Item.fd -or
            $Event.code -ne $Item.code -or
            $Event.path -ne $Item.path -or
            [int]$Event.count -ne [int]$Item.count) {
            return $false
        }
    }
    return $true
}

function Get-QemuRuntimeResetDeterminism {
    param(
        [object[]]$Runs,
        [object[]]$Loads,
        [object[]]$SourceMatrix,
        [object[]]$GuiTimeline,
        [object[]]$CapabilityCounters,
        [object]$InputTraceCapture,
        [object]$StorageTraceCapture
    )

    $CounterProperties = @(
        "console",
        "time",
        "describe",
        "present",
        "input",
        "exit",
        "display_checksum",
        "display_checksum_total",
        "storage_open",
        "storage_read",
        "storage_write",
        "storage_close",
        "storage_bytes",
        "afe_configure",
        "afe_read",
        "input_checksum",
        "input_last_x",
        "input_last_y",
        "input_last_down",
        "display_hash",
        "display_hash_total",
        "display_frame"
    )

    $PairNames = @(
        @("bss_app", "store:bss_app"),
        @("data_app", "store:data_app"),
        @("time_sequence_app", "store:time_sequence_app"),
        @("input_sequence_app", "store:input_sequence_app"),
        @("display_sequence_app", "store:display_sequence_app"),
        @("storage_fd_exhaustion_app", "store:storage_fd_exhaustion_app")
    )
    $CapabilityCountersReset = $true
    foreach ($Pair in $PairNames) {
        $Left = Get-QemuCapsByName -CapabilityCounters $CapabilityCounters -Name $Pair[0]
        $Right = Get-QemuCapsByName -CapabilityCounters $CapabilityCounters -Name $Pair[1]
        if (-not (Test-QemuRunExitOk -Runs $Runs -Name $Pair[0]) -or
            -not (Test-QemuRunExitOk -Runs $Runs -Name $Pair[1]) -or
            -not (Test-QemuCapsEqual -Left $Left -Right $Right -Properties $CounterProperties)) {
            $CapabilityCountersReset = $false
        }
    }

    $PlayerCaps = Get-QemuCapsByName -CapabilityCounters $CapabilityCounters -Name "player_min"
    foreach ($Name in @("received:player_min", "packetstream:player_min", "store:player_min")) {
        if (-not (Test-QemuRunExitOk -Runs $Runs -Name $Name) -or
            -not (Test-QemuCapsEqual -Left $PlayerCaps -Right (Get-QemuCapsByName -CapabilityCounters $CapabilityCounters -Name $Name) -Properties $CounterProperties)) {
            $CapabilityCountersReset = $false
        }
    }

    $BssCaps = Get-QemuCapsByName -CapabilityCounters $CapabilityCounters -Name "bss_app"
    $StoreBssCaps = Get-QemuCapsByName -CapabilityCounters $CapabilityCounters -Name "store:bss_app"
    $BssZeroFill = (Test-QemuRunExitOk -Runs $Runs -Name "bss_app") -and
        (Test-QemuRunExitOk -Runs $Runs -Name "store:bss_app") -and
        (Test-QemuCapsExpected -Caps $BssCaps -Expected @{ console = 22; time = 0; input = 0; present = 0; storage_open = 0; display_frame = 0 }) -and
        (Test-QemuCapsEqual -Left $BssCaps -Right $StoreBssCaps -Properties $CounterProperties)

    $DataCaps = Get-QemuCapsByName -CapabilityCounters $CapabilityCounters -Name "data_app"
    $StoreDataCaps = Get-QemuCapsByName -CapabilityCounters $CapabilityCounters -Name "store:data_app"
    $DataReinitialized = (Test-QemuRunExitOk -Runs $Runs -Name "data_app") -and
        (Test-QemuRunExitOk -Runs $Runs -Name "store:data_app") -and
        (Test-QemuCapsExpected -Caps $DataCaps -Expected @{ console = 35; time = 0; input = 0; present = 0; storage_open = 0; display_frame = 0 }) -and
        (Test-QemuCapsEqual -Left $DataCaps -Right $StoreDataCaps -Properties $CounterProperties)

    $TimeCaps = Get-QemuCapsByName -CapabilityCounters $CapabilityCounters -Name "time_sequence_app"
    $StoreTimeCaps = Get-QemuCapsByName -CapabilityCounters $CapabilityCounters -Name "store:time_sequence_app"
    $TimeReset = (Test-QemuRunExitOk -Runs $Runs -Name "time_sequence_app") -and
        (Test-QemuRunExitOk -Runs $Runs -Name "store:time_sequence_app") -and
        (Test-QemuCapsExpected -Caps $TimeCaps -Expected @{ console = 0; time = 4; input = 0; present = 0; storage_open = 0; display_frame = 0 }) -and
        (Test-QemuCapsEqual -Left $TimeCaps -Right $StoreTimeCaps -Properties $CounterProperties)

    $InputCaps = Get-QemuCapsByName -CapabilityCounters $CapabilityCounters -Name "input_sequence_app"
    $StoreInputCaps = Get-QemuCapsByName -CapabilityCounters $CapabilityCounters -Name "store:input_sequence_app"
    $InputReset = (Test-QemuRunExitOk -Runs $Runs -Name "input_sequence_app") -and
        (Test-QemuRunExitOk -Runs $Runs -Name "store:input_sequence_app") -and
        (Test-QemuCapsExpected -Caps $InputCaps -Expected @{ console = 41; input = 4; input_checksum = 114; input_last = "6,8,0"; present = 0 }) -and
        (Test-QemuCapsEqual -Left $InputCaps -Right $StoreInputCaps -Properties $CounterProperties)
    foreach ($Name in @("player_min", "received:player_min", "packetstream:player_min", "store:player_min")) {
        $Caps = Get-QemuCapsByName -CapabilityCounters $CapabilityCounters -Name $Name
        if (-not (Test-QemuRunExitOk -Runs $Runs -Name $Name) -or
            -not (Test-QemuCapsExpected -Caps $Caps -Expected @{ input = 1; input_checksum = 26; input_last = "3,5,0" })) {
            $InputReset = $false
        }
    }
    foreach ($Name in @("input_sequence_app", "store:input_sequence_app")) {
        $TraceRun = Get-QemuTraceRunByName -Trace $InputTraceCapture -Name $Name
        if ($null -eq $TraceRun -or [int]$TraceRun.event_count -ne 4) {
            $InputReset = $false
        }
    }

    $DisplayCaps = Get-QemuCapsByName -CapabilityCounters $CapabilityCounters -Name "display_sequence_app"
    $StoreDisplayCaps = Get-QemuCapsByName -CapabilityCounters $CapabilityCounters -Name "store:display_sequence_app"
    $DisplayReset = (Test-QemuRunExitOk -Runs $Runs -Name "display_sequence_app") -and
        (Test-QemuRunExitOk -Runs $Runs -Name "store:display_sequence_app") -and
        (Test-QemuCapsExpected -Caps $DisplayCaps -Expected @{ console = 45; describe = 1; present = 2; display_checksum_total = 3072; display_hash = "0xa9b09dc5"; display_frame = 2 }) -and
        (Test-QemuCapsEqual -Left $DisplayCaps -Right $StoreDisplayCaps -Properties $CounterProperties)
    foreach ($Name in @("player_min", "received:player_min", "packetstream:player_min", "store:player_min")) {
        $Caps = Get-QemuCapsByName -CapabilityCounters $CapabilityCounters -Name $Name
        if (-not (Test-QemuRunExitOk -Runs $Runs -Name $Name) -or
            -not (Test-QemuCapsExpected -Caps $Caps -Expected @{ describe = 1; present = 1; display_checksum_total = 174720; display_hash = "0xfac53a05"; display_frame = 1 })) {
            $DisplayReset = $false
        }
    }
    foreach ($Name in @("display_sequence_app", "store:display_sequence_app")) {
        $Timeline = @($GuiTimeline | Where-Object { $_.name -eq $Name })
        if ($Timeline.Count -ne 1 -or [int]$Timeline[0].frames -ne 2 -or [string]$Timeline[0].last_frame_hash -ne "0xa9b09dc5") {
            $DisplayReset = $false
        }
    }

    $StorageCaps = Get-QemuCapsByName -CapabilityCounters $CapabilityCounters -Name "storage_fd_exhaustion_app"
    $StoreStorageCaps = Get-QemuCapsByName -CapabilityCounters $CapabilityCounters -Name "store:storage_fd_exhaustion_app"
    $StorageFdReset = (Test-QemuRunExitOk -Runs $Runs -Name "storage_fd_exhaustion_app") -and
        (Test-QemuRunExitOk -Runs $Runs -Name "store:storage_fd_exhaustion_app") -and
        (Test-QemuCapsExpected -Caps $StorageCaps -Expected @{ console = 57; storage_open = 6; storage_read = 1; storage_write = 0; storage_close = 5; storage_bytes = 1 }) -and
        (Test-QemuCapsEqual -Left $StorageCaps -Right $StoreStorageCaps -Properties $CounterProperties) -and
        (Test-QemuStorageFdResetTraceRun -Run (Get-QemuTraceRunByName -Trace $StorageTraceCapture -Name "storage_fd_exhaustion_app")) -and
        (Test-QemuStorageFdResetTraceRun -Run (Get-QemuTraceRunByName -Trace $StorageTraceCapture -Name "store:storage_fd_exhaustion_app"))

    $SourceEquivalence = $true
    foreach ($Name in @("hello_app", "large_fit_app", "player_min")) {
        if (-not (Test-QemuSummarySourceMatrixEntry -Matrix $SourceMatrix -Name $Name -Sources @("direct", "received", "packetstream", "store"))) {
            $SourceEquivalence = $false
        }
    }

    $RunRegionCleared = $BssZeroFill -and $DataReinitialized
    $Ready = $RunRegionCleared -and
        $CapabilityCountersReset -and
        $TimeReset -and
        $InputReset -and
        $DisplayReset -and
        $StorageFdReset -and
        $BssZeroFill -and
        $DataReinitialized -and
        $SourceEquivalence

    return [pscustomobject]@{
        schema = "charm.resident_elf_qemu.runtime_reset_determinism.v1"
        status = if ($Ready) { "ok" } else { "failed" }
        run_region_cleared = $RunRegionCleared
        capability_counters_reset = $CapabilityCountersReset
        time_reset = $TimeReset
        input_reset = $InputReset
        display_reset = $DisplayReset
        storage_fd_reset = $StorageFdReset
        bss_zero_fill = $BssZeroFill
        data_reinitialized = $DataReinitialized
        source_equivalence = $SourceEquivalence
        evidence_runs = @(
            "bss_app",
            "store:bss_app",
            "data_app",
            "store:data_app",
            "time_sequence_app",
            "store:time_sequence_app",
            "input_sequence_app",
            "store:input_sequence_app",
            "display_sequence_app",
            "store:display_sequence_app",
            "storage_fd_exhaustion_app",
            "store:storage_fd_exhaustion_app",
            "player_min",
            "received:player_min",
            "packetstream:player_min",
            "store:player_min",
            "hello_app",
            "received:hello_app",
            "packetstream:hello_app",
            "store:hello_app",
            "large_fit_app",
            "received:large_fit_app",
            "packetstream:large_fit_app",
            "store:large_fit_app"
        )
    }
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

function Test-QemuSummaryRun {
    param(
        [object[]]$Runs,
        [string]$Name,
        [string]$Stage,
        [string]$Code
    )

    return (@($Runs | Where-Object {
                $_.name -eq $Name -and $_.stage -eq $Stage -and $_.code -eq $Code
            }).Count -gt 0)
}

function Test-QemuSummaryStage {
    param(
        [object[]]$Stages,
        [string]$Source,
        [string]$Name,
        [string]$Code
    )

    return (@($Stages | Where-Object {
                $_.source -eq $Source -and $_.name -eq $Name -and $_.code -eq $Code
            }).Count -gt 0)
}

function Test-QemuSummaryLoad {
    param(
        [object[]]$Loads,
        [string]$Name,
        [string]$Probe,
        [bool]$Fits
    )

    return (@($Loads | Where-Object {
                $_.name -eq $Name -and $_.probe -eq $Probe -and [bool]$_.fits -eq $Fits
            }).Count -gt 0)
}

function Test-QemuSummaryPacketstream {
    param(
        [object[]]$Packetstreams,
        [string]$Name,
        [string]$ReceiveStage,
        [string]$ReceiveCode
    )

    return (@($Packetstreams | Where-Object {
                $_.name -eq $Name -and $_.receive_stage -eq $ReceiveStage -and $_.receive_code -eq $ReceiveCode
            }).Count -gt 0)
}

function Test-QemuSummarySourceMatrixEntry {
    param(
        [object[]]$Matrix,
        [string]$Name,
        [string[]]$Sources
    )

    $Matches = @($Matrix | Where-Object { $_.name -eq $Name })
    if ($Matches.Count -ne 1) {
        return $false
    }
    $Entry = $Matches[0]
    foreach ($Source in $Sources) {
        $Value = $Entry.$Source
        if ($null -eq $Value -or $Value.stage -ne "exit" -or $Value.code -ne "ok") {
            return $false
        }
    }
    return $true
}

function Get-QemuBackendReadiness {
    param(
        [object]$BackendContract,
        [object]$Store,
        [object[]]$Runs,
        [object[]]$Stages,
        [object[]]$Loads,
        [object[]]$Packetstreams,
        [object[]]$SourceMatrix,
        [object[]]$GuiTimeline,
        [object]$Capabilities,
        [int]$FrameSignatureCount,
        [int]$FrameDumpCount,
        [int]$FramePpmCount,
        [int]$InputTraceCount,
        [int]$StorageTraceCount,
        [object]$RuntimeResetDeterminism
    )

    $ElfLoaderReady =
        (Test-QemuSummaryLoad -Loads $Loads -Name "hello_app" -Probe "ok" -Fits $true) -and
        (Test-QemuSummaryLoad -Loads $Loads -Name "large_fit_app" -Probe "ok" -Fits $true) -and
        (Test-QemuSummaryLoad -Loads $Loads -Name "too_large_app" -Probe "load_buffer_too_small" -Fits $false) -and
        (Test-QemuSummaryLoad -Loads $Loads -Name "unaligned_load_buffer_app" -Probe "load_buffer_unaligned" -Fits $true) -and
        (Test-QemuSummaryLoad -Loads $Loads -Name "bad_header_app" -Probe "bad_header" -Fits $true)
    $AppRuntimeReady =
        (Test-QemuSummaryRun -Runs $Runs -Name "hello_app" -Stage "exit" -Code "ok") -and
        (Test-QemuSummaryRun -Runs $Runs -Name "player_min" -Stage "exit" -Code "ok") -and
        (Test-QemuSummaryRun -Runs $Runs -Name "argv_overflow_app" -Stage "argv" -Code "argv_overflow") -and
        (Test-QemuSummaryRun -Runs $Runs -Name "abi_mismatch_app" -Stage "abi" -Code "abi_mismatch")
    $ReceivedReady =
        (Test-QemuSummaryStage -Stages $Stages -Source "received" -Name "hello_app" -Code "ok") -and
        (Test-QemuSummaryRun -Runs $Runs -Name "received:hello_app" -Stage "exit" -Code "ok")
    $PacketstreamReady =
        (Test-QemuSummaryPacketstream -Packetstreams $Packetstreams -Name "hello_app" -ReceiveStage "launch_ready" -ReceiveCode "ok") -and
        (Test-QemuSummaryPacketstream -Packetstreams $Packetstreams -Name "packetstream_crc_mismatch" -ReceiveStage "failed" -ReceiveCode "crc_mismatch")
    $StoreReady =
        $Store.format -eq "store_v1" -and
        $Store.media.kind -eq "memory" -and
        [int]$Store.entries -eq 31 -and
        [int]$Store.media.read_failures -eq 0 -and
        (Test-QemuSummaryStage -Stages $Stages -Source "store" -Name "hello_app" -Code "ok") -and
        (Test-QemuSummaryRun -Runs $Runs -Name "store:hello_app" -Stage "exit" -Code "ok")
    $EquivalentSourcesReady = $true
    foreach ($Name in @("hello_app", "large_fit_app", "player_min")) {
        if (-not (Test-QemuSummarySourceMatrixEntry -Matrix $SourceMatrix -Name $Name -Sources @("direct", "received", "packetstream", "store"))) {
            $EquivalentSourcesReady = $false
        }
    }
    $GuiReady =
        [int]$FrameSignatureCount -eq 8 -and
        [int]$FrameDumpCount -eq 8 -and
        [int]$FramePpmCount -eq 8 -and
        (@($GuiTimeline | Where-Object { $_.name -eq "player_min" -and [int]$_.frames -eq 1 -and [int]$_.inputs -eq 1 }).Count -eq 1)
    $StorageReady =
        $BackendContract.storage_media.kind -eq "virtual_readonly_files" -and
        [int]$StorageTraceCount -eq 130 -and
        [bool]$Capabilities.storage
    $InputReady =
        $BackendContract.input.kind -eq "deterministic_sequence" -and
        [int]$InputTraceCount -eq 24 -and
        [bool]$Capabilities.input
    $UnsupportedReady =
        [bool]$Capabilities.unsupported -and
        $BackendContract.afe -eq "unsupported" -and
        $BackendContract.storage -eq "readonly"
    $RuntimeResetReady =
        $null -ne $RuntimeResetDeterminism -and
        $RuntimeResetDeterminism.schema -eq "charm.resident_elf_qemu.runtime_reset_determinism.v1" -and
        $RuntimeResetDeterminism.status -eq "ok" -and
        [bool]$RuntimeResetDeterminism.run_region_cleared -and
        [bool]$RuntimeResetDeterminism.capability_counters_reset -and
        [bool]$RuntimeResetDeterminism.time_reset -and
        [bool]$RuntimeResetDeterminism.input_reset -and
        [bool]$RuntimeResetDeterminism.display_reset -and
        [bool]$RuntimeResetDeterminism.storage_fd_reset -and
        [bool]$RuntimeResetDeterminism.bss_zero_fill -and
        [bool]$RuntimeResetDeterminism.data_reinitialized -and
        [bool]$RuntimeResetDeterminism.source_equivalence

    $Ready = $ElfLoaderReady -and
        $AppRuntimeReady -and
        $ReceivedReady -and
        $PacketstreamReady -and
        $StoreReady -and
        $EquivalentSourcesReady -and
        $GuiReady -and
        $StorageReady -and
        $InputReady -and
        $UnsupportedReady -and
        $RuntimeResetReady

    return [pscustomobject]@{
        status = if ($Ready) { "ready" } else { "incomplete" }
        elf_loader = $ElfLoaderReady
        app_runtime = $AppRuntimeReady
        received = $ReceivedReady
        packetstream = $PacketstreamReady
        store = $StoreReady
        equivalent_sources = $EquivalentSourcesReady
        gui = $GuiReady
        storage = $StorageReady
        input = $InputReady
        unsupported_boundary = $UnsupportedReady
        runtime_reset = $RuntimeResetReady
    }
}

function New-QemuBackendCapabilityMatrixEntry {
    param(
        [string]$Capability,
        [string]$Provider,
        [string]$Policy,
        [string[]]$Evidence,
        [string[]]$Runs
    )

    return [pscustomobject]@{
        capability = $Capability
        provider = $Provider
        policy = $Policy
        evidence = @($Evidence)
        runs = @($Runs)
    }
}

function Get-QemuBackendCapabilityMatrix {
    param([object]$BackendContract)

    return @(
        (New-QemuBackendCapabilityMatrixEntry `
            -Capability "console" `
            -Provider "cmsdk_uart" `
            -Policy "write_only_log_sink" `
            -Evidence @("run_log", "capability_counters") `
            -Runs @("hello_app", "console_error_app", "store:console_error_app")),
        (New-QemuBackendCapabilityMatrixEntry `
            -Capability "time" `
            -Provider ([string]$BackendContract.time.kind) `
            -Policy ("start={0}:step={1}:reset_per_run={2}" -f `
                ([int]$BackendContract.time.start_ms), `
                ([int]$BackendContract.time.step_ms), `
                ($(if ([bool]$BackendContract.time.reset_per_run) { "1" } else { "0" }))) `
            -Evidence @("run_log", "capability_counters") `
            -Runs @("time_app", "time_sequence_app", "store:time_sequence_app")),
        (New-QemuBackendCapabilityMatrixEntry `
            -Capability "display" `
            -Provider ([string]$BackendContract.display.kind) `
            -Policy ("{0}x{1}:{2}:stride={3}:frame={4}" -f `
                ([int]$BackendContract.display.width), `
                ([int]$BackendContract.display.height), `
                ([string]$BackendContract.display.format), `
                ([int]$BackendContract.display.stride_bytes), `
                ([int]$BackendContract.display.frame_bytes)) `
            -Evidence @("frame_signatures", "frame_dumps", "frame_ppm", "gui_timeline", "capability_counters") `
            -Runs @("player_min", "display_sequence_app", "display_error_app", "display_null_present_app")),
        (New-QemuBackendCapabilityMatrixEntry `
            -Capability "input" `
            -Provider ([string]$BackendContract.input.kind) `
            -Policy ("samples={0}:max={1},{2}:wraps={3}" -f `
                ([int]$BackendContract.input.sample_count), `
                ([int]$BackendContract.input.pointer_max_x), `
                ([int]$BackendContract.input.pointer_max_y), `
                ($(if ([bool]$BackendContract.input.wraps) { "1" } else { "0" }))) `
            -Evidence @("input_trace", "gui_timeline", "capability_counters") `
            -Runs @("player_min", "input_sequence_app", "input_wrap_app", "input_error_app")),
        (New-QemuBackendCapabilityMatrixEntry `
            -Capability "storage" `
            -Provider ([string]$BackendContract.storage_media.kind) `
            -Policy ("files={0}:fd={1}+{2}:write={3}" -f `
                ([int]$BackendContract.storage_media.file_count), `
                ([int]$BackendContract.storage_media.fd_base), `
                ([int]$BackendContract.storage_media.fd_slots), `
                ([string]$BackendContract.storage_media.write_policy)) `
            -Evidence @("storage_trace", "capability_counters") `
            -Runs @("storage_app", "storage_catalog_app", "storage_fd_exhaustion_app", "storage_write_error_app")),
        (New-QemuBackendCapabilityMatrixEntry `
            -Capability "app_exit" `
            -Provider ([string]$BackendContract.app_exit.kind) `
            -Policy ("overrides_return={0}" -f ($(if ([bool]$BackendContract.app_exit.overrides_return) { "1" } else { "0" }))) `
            -Evidence @("run_log", "capability_counters") `
            -Runs @("exit_app", "exit_error_app", "exit_negative_app", "return_negative_app")),
        (New-QemuBackendCapabilityMatrixEntry `
            -Capability "afe" `
            -Provider "unsupported_stub" `
            -Policy "configure/read=unsupported:preserve_buffer=1" `
            -Evidence @("run_log", "capability_counters") `
            -Runs @("unsupported_caps_app", "afe_error_app", "store:afe_error_app"))
    )
}

function Write-DomainSummaryCapture {
    param(
        [string]$LogPath,
        [string]$FrameSignaturePath,
        [string]$FrameDumpPath,
        [string]$FramePpmPath,
        [string]$InputTracePath,
        [string]$StorageTracePath,
        [string]$OutputPath,
        [string]$BackendContractOutputPath,
        [string]$ArtifactDir,
        [int]$TimeoutSec,
        [int]$TailLines
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
    $BackendIdentity = Get-BackendIdentityFromText -Text $Text
    $RunRegionBase = Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: run-region base=(0x[0-9a-f]+)' -ErrorPrefix "domain_summary_failed"
    $RunRegionExpected = Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: run-region base=0x[0-9a-f]+ expected=(0x[0-9a-f]+)' -ErrorPrefix "domain_summary_failed"
    $RunRegionSize = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: run-region base=0x[0-9a-f]+ expected=0x[0-9a-f]+ size=(\d+)' -ErrorPrefix "domain_summary_failed")
    $StageCacheBytes = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: stage-cache bytes=(\d+)' -ErrorPrefix "domain_summary_failed")
    $PacketstreamStorageBytes = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: packetstream-buffers storage=(\d+)' -ErrorPrefix "domain_summary_failed")
    $PacketstreamTransportBytes = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: packetstream-buffers storage=\d+ transport=(\d+)' -ErrorPrefix "domain_summary_failed")
    $PacketstreamStreamBytes = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: packetstream-buffers storage=\d+ transport=\d+ stream=(\d+)' -ErrorPrefix "domain_summary_failed")
    $PacketstreamReceivedBytes = [int](Get-RegexGroupValue -Text $Text -Pattern 'resident-elf-qemu: packetstream-buffers storage=\d+ transport=\d+ stream=\d+ received=(\d+)' -ErrorPrefix "domain_summary_failed")
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
    $BackendContract = Get-BackendContractFromText -Text $Text `
        -RuntimeDomain $BackendIdentity.runtime_domain `
        -Capabilities $BackendIdentity.capabilities `
        -StorageMode $BackendIdentity.storage `
        -AfeMode $BackendIdentity.afe
    $BackendScope = Get-BackendScopeFromText -Text $Text
    $BackendSelfCheck = Get-BackendSelfCheckFromText -Text $Text
    $BackendResetSelfCheck = Get-BackendResetSelfCheckFromText -Text $Text
    $ArtifactSummary = Get-QemuArtifactSummary -AppOutDir $ArtifactDir
    if ([int]$ArtifactSummary.store.size -ne $StoreBytes) {
        throw "domain_summary_failed: artifact store size does not match runtime store bytes"
    }
    if ([int]$BackendContract.display.width -ne $DisplayWidth -or
        [int]$BackendContract.display.height -ne $DisplayHeight -or
        [int]$BackendContract.display.stride_bytes -ne $DisplayStride -or
        [string]$BackendContract.display.format -ne $DisplayFormat -or
        [int]$BackendContract.display.frame_bytes -ne $DisplayFrameBytes) {
        throw "domain_summary_failed: display describe does not match backend contract"
    }

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

    $StoreSummary = [pscustomobject]@{
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
    $Runs = Get-AppRunSummaryFromText -Text $Text
    $Stages = Get-StageSummaryFromText -Text $Text
    $Loads = Get-ElfLoadSummaryFromText -Text $Text
    $Packetstreams = Get-PacketstreamSummaryFromText -Text $Text
    $SourceMatrix = Get-SourceMatrixFromText -Text $Text
    $PrepareSummary = [pscustomobject]@{
        name = "prepare:argv_app"
        stage = "start"
        code = "ok"
        ready = $true
        argc = $PrepareArgc
        capability_calls = 0
    }
    $ElfRunEvidenceMatrix = Get-ElfRunEvidenceMatrix `
        -Runs $Runs `
        -Stages $Stages `
        -Loads $Loads `
        -Packetstreams $Packetstreams `
        -Prepare $PrepareSummary
    $GuiTimeline = Get-GuiTimelineSummary -FrameSignatureCapture $FrameSignatureCapture -InputTraceCapture $InputTraceCapture
    $Capabilities = Get-CapabilitySummaryFromText -Text $Text
    $CapabilityCounters = Get-CapabilityCounterSummaryFromText -Text $Text
    $RuntimeResetDeterminism = Get-QemuRuntimeResetDeterminism `
        -Runs $Runs `
        -Loads $Loads `
        -SourceMatrix $SourceMatrix `
        -GuiTimeline $GuiTimeline `
        -CapabilityCounters $CapabilityCounters `
        -InputTraceCapture $InputTraceCapture `
        -StorageTraceCapture $StorageTraceCapture
    $BackendCapabilityMatrix = Get-QemuBackendCapabilityMatrix -BackendContract $BackendContract
    $BackendReadiness = Get-QemuBackendReadiness `
        -BackendContract $BackendContract `
        -Store $StoreSummary `
        -Runs $Runs `
        -Stages $Stages `
        -Loads $Loads `
        -Packetstreams $Packetstreams `
        -SourceMatrix $SourceMatrix `
        -GuiTimeline $GuiTimeline `
        -Capabilities $Capabilities `
        -FrameSignatureCount $FrameSignatureCount `
        -FrameDumpCount $FrameDumpCount `
        -FramePpmCount $FramePpmCount `
        -InputTraceCount $InputTraceCount `
        -StorageTraceCount $StorageTraceCount `
        -RuntimeResetDeterminism $RuntimeResetDeterminism
    $NegativeCases = Get-QemuNegativeCases
    $FailureTaxonomy = Get-QemuFailureTaxonomy -NegativeCases $NegativeCases
    $RuntimeDomainProfile = Get-QemuRuntimeDomainProfile `
        -RuntimeDomain $BackendIdentity.runtime_domain `
        -Machine $BackendIdentity.machine `
        -Cpu $BackendIdentity.cpu `
        -BackendContract $BackendContract `
        -BackendScope $BackendScope

    $Summary = [pscustomobject]@{
        schema = "charm.resident_elf_qemu.domain_summary.v1"
        domain = $BackendIdentity.runtime_domain
        machine = $BackendIdentity.machine
        cpu = $BackendIdentity.cpu
        image_format = "elf"
        app_model = "CharmAppApi"
        backend_scope = $BackendScope
        backend_contract = $BackendContract
        backend_self_check = $BackendSelfCheck
        backend_reset_self_check = $BackendResetSelfCheck
        backend_readiness = $BackendReadiness
        backend_capability_matrix = $BackendCapabilityMatrix
        runtime_domain_profile = $RuntimeDomainProfile
        runtime_reset_determinism = $RuntimeResetDeterminism
        elf_run_evidence_matrix = $ElfRunEvidenceMatrix
        failure_taxonomy = $FailureTaxonomy
        artifacts = $ArtifactSummary
        run_budget = [pscustomobject]@{
            timeout_sec = $TimeoutSec
            tail_lines = $TailLines
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
        packetstream_buffers = [pscustomobject]@{
            storage_bytes = $PacketstreamStorageBytes
            transport_bytes = $PacketstreamTransportBytes
            stream_bytes = $PacketstreamStreamBytes
            received_bytes = $PacketstreamReceivedBytes
        }
        display = [pscustomobject]@{
            width = $DisplayWidth
            height = $DisplayHeight
            stride_bytes = $DisplayStride
            format = $DisplayFormat
            frame_bytes = $DisplayFrameBytes
            pixel_bytes = 4
        }
        store = $StoreSummary
        coverage = [pscustomobject]@{
            runs = $Runs
            stages = $Stages
            loads = $Loads
            packetstreams = $Packetstreams
            source_matrix = $SourceMatrix
            gui_timeline = $GuiTimeline
            prepare = $PrepareSummary
            capabilities = $Capabilities
            capability_counters = $CapabilityCounters
            negative_cases = $NegativeCases
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

    if (-not [string]::IsNullOrWhiteSpace($BackendContractOutputPath)) {
        $ResolvedBackendContractOut = Resolve-ScriptPath -Path $BackendContractOutputPath
        $BackendContractOutDir = [System.IO.Path]::GetDirectoryName($ResolvedBackendContractOut)
        if (-not [string]::IsNullOrWhiteSpace($BackendContractOutDir) -and -not (Test-Path -LiteralPath $BackendContractOutDir)) {
            New-Item -ItemType Directory -Force -Path $BackendContractOutDir | Out-Null
        }
        $BackendContractCapture = [pscustomobject]@{
            schema = "charm.resident_elf_qemu.backend_contract.v1"
            domain = $BackendIdentity.runtime_domain
            machine = $BackendIdentity.machine
            cpu = $BackendIdentity.cpu
            image_format = "elf"
            app_model = "CharmAppApi"
            backend_scope = $BackendScope
            backend_contract = $BackendContract
            backend_self_check = $BackendSelfCheck
            backend_reset_self_check = $BackendResetSelfCheck
            backend_readiness = $BackendReadiness
            backend_capability_matrix = $BackendCapabilityMatrix
            runtime_domain_profile = $RuntimeDomainProfile
        }
        $BackendContractJson = $BackendContractCapture | ConvertTo-Json -Depth 16
        [System.IO.File]::WriteAllText($ResolvedBackendContractOut, ($BackendContractJson + "`n"), [System.Text.UTF8Encoding]::new($false))
        Write-Host "resident-elf-qemu backend contract:"
        Write-Host "  path=$ResolvedBackendContractOut"
        Write-Host "  domain=virtual_m7"
        Write-Host "  capabilities=$($BackendIdentity.capabilities)"
    }
}

function ConvertTo-CanonicalDomainSummary {
    param([object]$Summary)

    $Canonical = $Summary | ConvertTo-Json -Depth 32 | ConvertFrom-Json
    if ($null -ne $Canonical.artifacts) {
        if (-not [string]::IsNullOrWhiteSpace([string]$Canonical.artifacts.directory)) {
            $Canonical.artifacts.directory = "<artifact-dir>"
        }
        foreach ($GroupName in @("apps", "extra", "includes")) {
            $Group = $Canonical.artifacts.PSObject.Properties[$GroupName]
            if ($null -eq $Group) {
                continue
            }
            foreach ($Artifact in @($Group.Value)) {
                if ($null -ne $Artifact.PSObject.Properties["path"] -and
                    -not [string]::IsNullOrWhiteSpace([string]$Artifact.path)) {
                    $Artifact.path = [System.IO.Path]::GetFileName([string]$Artifact.path)
                }
            }
        }
        if ($null -ne $Canonical.artifacts.store -and
            $null -ne $Canonical.artifacts.store.PSObject.Properties["path"] -and
            -not [string]::IsNullOrWhiteSpace([string]$Canonical.artifacts.store.path)) {
            $Canonical.artifacts.store.path = [System.IO.Path]::GetFileName([string]$Canonical.artifacts.store.path)
        }
    }
    if ($null -ne $Canonical.evidence) {
        foreach ($PropertyName in @("log", "frame_signatures", "frame_dumps", "frame_ppm", "input_trace", "storage_trace")) {
            $Property = $Canonical.evidence.PSObject.Properties[$PropertyName]
            if ($null -ne $Property -and -not [string]::IsNullOrWhiteSpace([string]$Property.Value)) {
                $Property.Value = [System.IO.Path]::GetFileName([string]$Property.Value)
            }
        }
    }
    if ($null -ne $Canonical.run_budget) {
        $Canonical.run_budget.timeout_sec = 0
        $Canonical.run_budget.tail_lines = 0
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
        [int]$Segments,
        [string]$ExpectedBase = "0x20080000",
        [string]$ExpectedLinkBase = "0x20080000",
        [bool]$ExpectBaseMatch = $true
    )

    $Matches = @($Loads | Where-Object { $_.name -eq $Name })
    if ($Matches.Count -lt 1) {
        throw "domain_summary_validate_failed: missing load $Name"
    }
    $Load = $Matches[0]
    if ($Load.format -ne "elf" -or $Load.probe -ne $Probe -or $Load.capacity_probe -ne $Probe) {
        throw "domain_summary_validate_failed: load $Name bad probe/format"
    }
    if ($Probe -eq "ok") {
        if ($Load.link_base -ne $ExpectedLinkBase -or
            $Load.expected_base -ne $ExpectedBase -or
            [bool]$Load.base_match -ne $ExpectBaseMatch) {
            throw "domain_summary_validate_failed: load $Name bad link base"
        }
        if ($ExpectBaseMatch -and [int64]$Load.entry_vaddr_numeric -lt [int64]$Load.link_base_numeric) {
            throw "domain_summary_validate_failed: load $Name entry before link base"
        }
        if ($ExpectBaseMatch -and ([int64]$Load.entry_numeric - [int64]$Load.entry_vaddr_numeric) -ne
            ([int64]$Load.expected_base_numeric - [int64]$Load.link_base_numeric)) {
            throw "domain_summary_validate_failed: load $Name entry/link relocation mismatch"
        }
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

function Assert-EquivalentElfEntrySources {
    param(
        [object[]]$Runs,
        [object[]]$Loads,
        [string]$Name,
        [string[]]$Sources
    )

    $DirectRuns = @($Runs | Where-Object { $_.name -eq $Name })
    if ($DirectRuns.Count -lt 1) {
        throw "domain_summary_validate_failed: equivalent ELF source $Name missing direct run"
    }
    $ExpectedRun = $DirectRuns[0]

    $DirectLoads = @($Loads | Where-Object { $_.name -eq $Name })
    if ($DirectLoads.Count -lt 1) {
        throw "domain_summary_validate_failed: equivalent ELF source $Name missing direct load"
    }
    $ExpectedLoad = $DirectLoads[0]

    foreach ($Source in $Sources) {
        $RunName = if ($Source -eq "direct") { $Name } else { "$($Source):$Name" }
        $RunMatches = @($Runs | Where-Object { $_.name -eq $RunName })
        if ($RunMatches.Count -lt 1) {
            throw "domain_summary_validate_failed: equivalent ELF source $RunName missing run"
        }
        $Run = $RunMatches[0]
        if ($Run.stage -ne $ExpectedRun.stage -or
            $Run.code -ne $ExpectedRun.code -or
            [int]$Run.exit -ne [int]$ExpectedRun.exit) {
            throw "domain_summary_validate_failed: equivalent ELF source $RunName run differs from direct"
        }

        $LoadMatches = @($Loads | Where-Object { $_.name -eq $RunName })
        if ($LoadMatches.Count -lt 1) {
            throw "domain_summary_validate_failed: equivalent ELF source $RunName missing load"
        }
        $Load = $LoadMatches[0]
        foreach ($Property in @(
            "format",
            "probe",
            "link_base",
            "expected_base",
            "entry",
            "entry_vaddr",
            "span",
            "segments",
            "base_match",
            "needed",
            "free",
            "fits",
            "region",
            "capacity_probe"
        )) {
            if ($Load.$Property -ne $ExpectedLoad.$Property) {
                throw "domain_summary_validate_failed: equivalent ELF source $RunName load $Property differs from direct"
            }
        }
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

function Get-DomainFailureTaxonomyCategory {
    param(
        [object[]]$Categories,
        [string]$Category
    )

    $Matches = @($Categories | Where-Object { $_.category -eq $Category })
    if ($Matches.Count -ne 1) {
        throw "domain_summary_validate_failed: failure taxonomy missing or duplicate category $Category"
    }
    return $Matches[0]
}

function Get-DomainFailureTaxonomyStage {
    param(
        [object[]]$Stages,
        [string]$Stage
    )

    $Matches = @($Stages | Where-Object { $_.stage -eq $Stage })
    if ($Matches.Count -ne 1) {
        throw "domain_summary_validate_failed: failure taxonomy missing or duplicate stage $Stage"
    }
    return $Matches[0]
}

function Assert-DomainFailureTaxonomyCategory {
    param(
        [object[]]$Categories,
        [string]$Category,
        [int]$Count,
        [string[]]$Stages
    )

    $Entry = Get-DomainFailureTaxonomyCategory -Categories $Categories -Category $Category
    if ([int]$Entry.count -ne $Count) {
        throw "domain_summary_validate_failed: failure taxonomy category $Category count=$($Entry.count), expected $Count"
    }
    $ActualStages = @($Entry.stages)
    if ($ActualStages.Count -ne $Stages.Count) {
        throw "domain_summary_validate_failed: failure taxonomy category $Category bad stage count"
    }
    foreach ($Stage in $Stages) {
        if (-not ($ActualStages -contains $Stage)) {
            throw "domain_summary_validate_failed: failure taxonomy category $Category missing stage $Stage"
        }
    }
}

function Assert-DomainFailureTaxonomyStage {
    param(
        [object[]]$Stages,
        [string]$Stage,
        [string]$Category,
        [int]$Count,
        [string[]]$Codes,
        [string[]]$Cases
    )

    $Entry = Get-DomainFailureTaxonomyStage -Stages $Stages -Stage $Stage
    if ($Entry.category -ne $Category -or [int]$Entry.count -ne $Count) {
        throw "domain_summary_validate_failed: failure taxonomy stage $Stage bad category/count"
    }
    $ActualCodes = @($Entry.codes)
    if ($ActualCodes.Count -ne $Codes.Count) {
        throw "domain_summary_validate_failed: failure taxonomy stage $Stage bad code count"
    }
    foreach ($Code in $Codes) {
        if (-not ($ActualCodes -contains $Code)) {
            throw "domain_summary_validate_failed: failure taxonomy stage $Stage missing code $Code"
        }
    }
    $ActualCases = @($Entry.cases)
    if ($ActualCases.Count -ne $Cases.Count) {
        throw "domain_summary_validate_failed: failure taxonomy stage $Stage bad case count"
    }
    foreach ($Case in $Cases) {
        if (-not ($ActualCases -contains $Case)) {
            throw "domain_summary_validate_failed: failure taxonomy stage $Stage missing case $Case"
        }
    }
}

function Assert-DomainFailureTaxonomy {
    param(
        [object]$Taxonomy,
        [object[]]$NegativeCases
    )

    if ($null -eq $Taxonomy -or $Taxonomy.schema -ne "charm.resident_elf_qemu.failure_taxonomy.v1") {
        throw "domain_summary_validate_failed: bad failure taxonomy schema"
    }
    if ([int]$Taxonomy.total -ne @($NegativeCases).Count -or [int]$Taxonomy.total -ne 25) {
        throw "domain_summary_validate_failed: bad failure taxonomy total"
    }

    $Categories = @($Taxonomy.categories)
    $Stages = @($Taxonomy.stages)
    Assert-DomainCount -Name "failure_taxonomy.categories" -Actual $Categories.Count -Expected 4
    Assert-DomainCount -Name "failure_taxonomy.stages" -Actual $Stages.Count -Expected 6

    Assert-DomainFailureTaxonomyCategory -Categories $Categories -Category "transport" -Count 1 -Stages @("packetstream_verify")
    Assert-DomainFailureTaxonomyCategory -Categories $Categories -Category "stage" -Count 2 -Stages @("received_stage", "store_stage")
    Assert-DomainFailureTaxonomyCategory -Categories $Categories -Category "load" -Count 20 -Stages @("load")
    Assert-DomainFailureTaxonomyCategory -Categories $Categories -Category "runtime" -Count 2 -Stages @("argv", "abi")

    Assert-DomainFailureTaxonomyStage `
        -Stages $Stages `
        -Stage "packetstream_verify" `
        -Category "transport" `
        -Count 1 `
        -Codes @("crc_mismatch") `
        -Cases @("packetstream_crc_mismatch")
    Assert-DomainFailureTaxonomyStage `
        -Stages $Stages `
        -Stage "received_stage" `
        -Category "stage" `
        -Count 1 `
        -Codes @("buffer_too_small") `
        -Cases @("received_too_large_app")
    Assert-DomainFailureTaxonomyStage `
        -Stages $Stages `
        -Stage "store_stage" `
        -Category "stage" `
        -Count 1 `
        -Codes @("image_too_large") `
        -Cases @("too_large_store_app")
    Assert-DomainFailureTaxonomyStage `
        -Stages $Stages `
        -Stage "load" `
        -Category "load" `
        -Count 20 `
        -Codes @("load_failed") `
        -Cases @(
            "bad_elf_magic_app",
            "packetstream_bad_elf_magic_app",
            "bad_header_app",
            "bad_class_app",
            "bad_endian_app",
            "bad_ident_version_app",
            "bad_type_app",
            "bad_machine_app",
            "bad_version_app",
            "bad_ehsize_app",
            "bad_phentsize_app",
            "bad_program_header_app",
            "truncated_payload_app",
            "no_load_segment_app",
            "entry_outside_segment_app",
            "overlapping_segments_app",
            "rwx_segment_app",
            "wrong_link_base_app",
            "too_large_app",
            "unaligned_load_buffer_app"
        )
    Assert-DomainFailureTaxonomyStage `
        -Stages $Stages `
        -Stage "argv" `
        -Category "runtime" `
        -Count 1 `
        -Codes @("argv_overflow") `
        -Cases @("argv_overflow_app")
    Assert-DomainFailureTaxonomyStage `
        -Stages $Stages `
        -Stage "abi" `
        -Category "runtime" `
        -Count 1 `
        -Codes @("abi_mismatch") `
        -Cases @("abi_mismatch_app")

    foreach ($Negative in @($NegativeCases)) {
        $Stage = Get-DomainFailureTaxonomyStage -Stages $Stages -Stage ([string]$Negative.stage)
        if (-not (@($Stage.cases) -contains ([string]$Negative.name)) -or
            -not (@($Stage.codes) -contains ([string]$Negative.code))) {
            throw "domain_summary_validate_failed: failure taxonomy does not include negative case $($Negative.name)"
        }
    }
}

function Assert-DomainStoreMedia {
    param([object]$Store)

    if ($Store.format -ne "store_v1" -or [int]$Store.entries -ne 31 -or [int]$Store.bytes -le 0) {
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

function Assert-DomainArtifactRecord {
    param(
        [object[]]$Artifacts,
        [string]$Name,
        [string]$Kind,
        [int]$Size = 0
    )

    $Matches = @($Artifacts | Where-Object { $_.name -eq $Name })
    if ($Matches.Count -ne 1) {
        throw "domain_summary_validate_failed: missing or duplicate artifact $Name"
    }
    $Artifact = $Matches[0]
    if ($Artifact.kind -ne $Kind) {
        throw "domain_summary_validate_failed: artifact $Name kind=$($Artifact.kind), expected $Kind"
    }
    if ($Size -gt 0 -and [int]$Artifact.size -ne $Size) {
        throw "domain_summary_validate_failed: artifact $Name size=$($Artifact.size), expected $Size"
    }
    if ([int]$Artifact.size -le 0) {
        throw "domain_summary_validate_failed: artifact $Name has invalid size"
    }
    if (([string]$Artifact.crc32) -notmatch '^0x[0-9a-f]{8}$') {
        throw "domain_summary_validate_failed: artifact $Name has invalid crc32"
    }
    if ([string]::IsNullOrWhiteSpace([string]$Artifact.path)) {
        throw "domain_summary_validate_failed: artifact $Name path is empty"
    }
    return $Artifact
}

function Find-DomainArtifactRecord {
    param(
        [object[]]$Artifacts,
        [string]$Name
    )

    $Matches = @($Artifacts | Where-Object { $_.name -eq $Name })
    if ($Matches.Count -ne 1) {
        throw "domain_summary_validate_failed: missing or duplicate artifact for Store entry $Name"
    }
    return $Matches[0]
}

function Assert-DomainStoreEntryManifest {
    param(
        [object]$StoreManifest,
        [object]$Artifacts,
        [object]$Store
    )

    if ($null -eq $StoreManifest -or $null -eq $StoreManifest.header) {
        throw "domain_summary_validate_failed: missing Store entry manifest"
    }
    if ($StoreManifest.header.magic -ne "0x50415043" -or
        [int]$StoreManifest.header.version -ne 1 -or
        [int]$StoreManifest.header.header_size -ne 16 -or
        [int]$StoreManifest.header.entry_count -ne [int]$Store.entries -or
        [int]$StoreManifest.header.entry_size -ne 44) {
        throw "domain_summary_validate_failed: bad Store manifest header"
    }

    $Entries = @($StoreManifest.entries)
    if ($Entries.Count -ne [int]$Store.entries) {
        throw "domain_summary_validate_failed: Store manifest entry count mismatch"
    }
    $ArtifactPayloads = @(@($Artifacts.apps | Where-Object { $_.name -ne "too_large_app" }) + @($Artifacts.extra))
    if ($ArtifactPayloads.Count -ne $Entries.Count) {
        throw "domain_summary_validate_failed: Store manifest payload count mismatch"
    }

    $PreviousOffset = 0
    foreach ($Entry in $Entries) {
        $Name = [string]$Entry.name
        $Artifact = Find-DomainArtifactRecord -Artifacts $ArtifactPayloads -Name $Name
        if ([string]$Entry.format -ne "elf" -or
            [int]$Entry.format_bits -ne 0 -or
            [string]$Entry.flags -ne "0x00000000") {
            throw "domain_summary_validate_failed: Store entry $Name has unexpected format/flags"
        }
        if ([int]$Entry.size -ne [int]$Artifact.size) {
            throw "domain_summary_validate_failed: Store entry $Name size=$($Entry.size), artifact size=$($Artifact.size)"
        }
        if ([string]$Entry.payload_crc32 -ne [string]$Artifact.crc32) {
            throw "domain_summary_validate_failed: Store entry $Name crc=$($Entry.payload_crc32), artifact crc=$($Artifact.crc32)"
        }
        if ([int]$Entry.offset -le $PreviousOffset -or ([int]$Entry.offset % 16) -ne 0) {
            throw "domain_summary_validate_failed: Store entry $Name offset is not monotonic/aligned"
        }
        $PreviousOffset = [int]$Entry.offset
    }

    foreach ($Artifact in $ArtifactPayloads) {
        $Matches = @($Entries | Where-Object { $_.name -eq $Artifact.name })
        if ($Matches.Count -ne 1) {
            throw "domain_summary_validate_failed: Store manifest missing artifact payload $($Artifact.name)"
        }
    }
}

function Assert-DomainArtifacts {
    param(
        [object]$Artifacts,
        [object]$Store
    )

    if ($null -eq $Artifacts) {
        throw "domain_summary_validate_failed: missing artifacts summary"
    }
    if ([string]::IsNullOrWhiteSpace([string]$Artifacts.directory)) {
        throw "domain_summary_validate_failed: artifact directory is empty"
    }
    if ([int]$Artifacts.app_count -ne 31 -or
        [int]$Artifacts.store_app_count -ne 30 -or
        [int]$Artifacts.include_count -ne 32) {
        throw "domain_summary_validate_failed: bad artifact counts"
    }
    $Apps = @($Artifacts.apps)
    $Extra = @($Artifacts.extra)
    $Includes = @($Artifacts.includes)
    if ($Apps.Count -ne 31 -or $Extra.Count -ne 1 -or $Includes.Count -ne 32) {
        throw "domain_summary_validate_failed: bad artifact array counts"
    }
    [void](Assert-DomainArtifactRecord -Artifacts $Apps -Name "hello_app" -Kind "elf" -Size 5132)
    [void](Assert-DomainArtifactRecord -Artifacts $Apps -Name "player_min" -Kind "elf" -Size 5168)
    [void](Assert-DomainArtifactRecord -Artifacts $Apps -Name "large_fit_app" -Kind "elf" -Size 5168)
    [void](Assert-DomainArtifactRecord -Artifacts $Apps -Name "too_large_app" -Kind "elf" -Size 4868)
    [void](Assert-DomainArtifactRecord -Artifacts $Extra -Name "too_large_store_app" -Kind "elf" -Size 20000)
    [void](Assert-DomainArtifactRecord -Artifacts $Includes -Name "hello_app.elf" -Kind "generated_inc")
    [void](Assert-DomainArtifactRecord -Artifacts $Includes -Name "appstore.bin" -Kind "generated_inc")

    $StoreArtifact = Assert-DomainArtifactRecord -Artifacts @($Artifacts.store) -Name "appstore" -Kind "store_v1" -Size ([int]$Store.bytes)
    if ([int]$StoreArtifact.size -ne [int]$Store.media.bytes) {
        throw "domain_summary_validate_failed: store artifact size does not match media bytes"
    }
    Assert-DomainStoreEntryManifest -StoreManifest $Artifacts.store_manifest -Artifacts $Artifacts -Store $Store
}

function Assert-BackendSelfCheck {
    param(
        [object]$SelfCheck,
        [string]$ErrorPrefix
    )

    if ($null -eq $SelfCheck) {
        throw "${ErrorPrefix}: missing backend self-check"
    }
    if ($SelfCheck.schema -ne "charm.resident_elf_qemu.backend_self_check.v1") {
        throw "${ErrorPrefix}: bad backend self-check schema"
    }
    foreach ($Field in @("api", "display", "input", "storage", "afe", "app_exit")) {
        if (-not [bool]$SelfCheck.$Field) {
            throw "${ErrorPrefix}: backend self-check $Field failed"
        }
    }
    if ($SelfCheck.result -ne "ok") {
        throw "${ErrorPrefix}: backend self-check result=$($SelfCheck.result)"
    }
}

function Assert-BackendResetSelfCheck {
    param(
        [object]$SelfCheck,
        [string]$ErrorPrefix
    )

    if ($null -eq $SelfCheck) {
        throw "${ErrorPrefix}: missing backend reset self-check"
    }
    if ($SelfCheck.schema -ne "charm.resident_elf_qemu.backend_reset_self_check.v1") {
        throw "${ErrorPrefix}: bad backend reset self-check schema"
    }
    foreach ($Field in @("counters", "display", "time", "input", "storage")) {
        if (-not [bool]$SelfCheck.$Field) {
            throw "${ErrorPrefix}: backend reset self-check $Field failed"
        }
    }
    if ($SelfCheck.result -ne "ok") {
        throw "${ErrorPrefix}: backend reset self-check result=$($SelfCheck.result)"
    }
}

function Assert-DomainBackendReadiness {
    param([object]$Readiness)

    if ($null -eq $Readiness) {
        throw "domain_summary_validate_failed: missing backend readiness"
    }
    $ExpectedProperties = @(
        "status",
        "elf_loader",
        "app_runtime",
        "received",
        "packetstream",
        "store",
        "equivalent_sources",
        "gui",
        "storage",
        "input",
        "unsupported_boundary",
        "runtime_reset"
    )
    $ActualProperties = @($Readiness.PSObject.Properties | ForEach-Object { $_.Name })
    foreach ($Property in $ExpectedProperties) {
        if (-not ($ActualProperties -contains $Property)) {
            throw "domain_summary_validate_failed: backend readiness missing $Property"
        }
    }
    if ($ActualProperties.Count -ne $ExpectedProperties.Count) {
        throw "domain_summary_validate_failed: unexpected backend readiness property count"
    }
    if ($Readiness.status -ne "ready") {
        throw "domain_summary_validate_failed: backend readiness status=$($Readiness.status)"
    }
    foreach ($Property in @(
            "elf_loader",
            "app_runtime",
            "received",
            "packetstream",
            "store",
            "equivalent_sources",
            "gui",
            "storage",
            "input",
            "unsupported_boundary",
            "runtime_reset"
        )) {
        if (-not [bool]$Readiness.$Property) {
            throw "domain_summary_validate_failed: backend readiness $Property is false"
        }
    }
}

function Assert-DomainRuntimeResetDeterminism {
    param([object]$Reset)

    if ($null -eq $Reset -or $Reset.schema -ne "charm.resident_elf_qemu.runtime_reset_determinism.v1") {
        throw "domain_summary_validate_failed: bad runtime reset determinism schema"
    }
    if ($Reset.status -ne "ok") {
        throw "domain_summary_validate_failed: runtime reset determinism status=$($Reset.status)"
    }
    foreach ($Property in @(
            "run_region_cleared",
            "capability_counters_reset",
            "time_reset",
            "input_reset",
            "display_reset",
            "storage_fd_reset",
            "bss_zero_fill",
            "data_reinitialized",
            "source_equivalence"
        )) {
        if (-not [bool]$Reset.$Property) {
            throw "domain_summary_validate_failed: runtime reset determinism $Property is false"
        }
    }

    $EvidenceRuns = @($Reset.evidence_runs)
    foreach ($Name in @(
            "bss_app",
            "store:bss_app",
            "data_app",
            "store:data_app",
            "time_sequence_app",
            "store:time_sequence_app",
            "input_sequence_app",
            "store:input_sequence_app",
            "display_sequence_app",
            "store:display_sequence_app",
            "storage_fd_exhaustion_app",
            "store:storage_fd_exhaustion_app",
            "player_min",
            "received:player_min",
            "packetstream:player_min",
            "store:player_min",
            "hello_app",
            "received:hello_app",
            "packetstream:hello_app",
            "store:hello_app",
            "large_fit_app",
            "received:large_fit_app",
            "packetstream:large_fit_app",
            "store:large_fit_app"
        )) {
        if (-not ($EvidenceRuns -contains $Name)) {
            throw "domain_summary_validate_failed: runtime reset determinism missing evidence run $Name"
        }
    }
    if ($EvidenceRuns.Count -ne 24) {
        throw "domain_summary_validate_failed: runtime reset determinism evidence count=$($EvidenceRuns.Count)"
    }
}

function Assert-DomainBackendCapabilityMatrixEntry {
    param(
        [object[]]$Matrix,
        [string]$Capability,
        [string]$Provider,
        [string]$Policy,
        [string[]]$Evidence,
        [string[]]$Runs
    )

    $Matches = @($Matrix | Where-Object { $_.capability -eq $Capability })
    if ($Matches.Count -ne 1) {
        throw "domain_summary_validate_failed: backend capability matrix missing or duplicate $Capability"
    }
    $Entry = $Matches[0]
    if ($Entry.provider -ne $Provider -or $Entry.policy -ne $Policy) {
        throw "domain_summary_validate_failed: backend capability matrix $Capability bad provider/policy"
    }
    $ActualEvidence = @($Entry.evidence)
    if ($ActualEvidence.Count -ne $Evidence.Count) {
        throw "domain_summary_validate_failed: backend capability matrix $Capability bad evidence count"
    }
    foreach ($Item in $Evidence) {
        if (-not ($ActualEvidence -contains $Item)) {
            throw "domain_summary_validate_failed: backend capability matrix $Capability missing evidence $Item"
        }
    }
    $ActualRuns = @($Entry.runs)
    if ($ActualRuns.Count -ne $Runs.Count) {
        throw "domain_summary_validate_failed: backend capability matrix $Capability bad run count"
    }
    foreach ($Item in $Runs) {
        if (-not ($ActualRuns -contains $Item)) {
            throw "domain_summary_validate_failed: backend capability matrix $Capability missing run $Item"
        }
    }
}

function Assert-DomainBackendCapabilityMatrix {
    param([object[]]$Matrix)

    if ($Matrix.Count -ne 7) {
        throw "domain_summary_validate_failed: backend capability matrix count=$($Matrix.Count), expected 7"
    }
    Assert-DomainBackendCapabilityMatrixEntry -Matrix $Matrix `
        -Capability "console" `
        -Provider "cmsdk_uart" `
        -Policy "write_only_log_sink" `
        -Evidence @("run_log", "capability_counters") `
        -Runs @("hello_app", "console_error_app", "store:console_error_app")
    Assert-DomainBackendCapabilityMatrixEntry -Matrix $Matrix `
        -Capability "time" `
        -Provider "deterministic_tick" `
        -Policy "start=1000:step=17:reset_per_run=1" `
        -Evidence @("run_log", "capability_counters") `
        -Runs @("time_app", "time_sequence_app", "store:time_sequence_app")
    Assert-DomainBackendCapabilityMatrixEntry -Matrix $Matrix `
        -Capability "display" `
        -Provider "framebuffer" `
        -Policy "16x16:argb8888:stride=64:frame=1024" `
        -Evidence @("frame_signatures", "frame_dumps", "frame_ppm", "gui_timeline", "capability_counters") `
        -Runs @("player_min", "display_sequence_app", "display_error_app", "display_null_present_app")
    Assert-DomainBackendCapabilityMatrixEntry -Matrix $Matrix `
        -Capability "input" `
        -Provider "deterministic_sequence" `
        -Policy "samples=4:max=15,15:wraps=1" `
        -Evidence @("input_trace", "gui_timeline", "capability_counters") `
        -Runs @("player_min", "input_sequence_app", "input_wrap_app", "input_error_app")
    Assert-DomainBackendCapabilityMatrixEntry -Matrix $Matrix `
        -Capability "storage" `
        -Provider "virtual_readonly_files" `
        -Policy "files=3:fd=3+4:write=unsupported" `
        -Evidence @("storage_trace", "capability_counters") `
        -Runs @("storage_app", "storage_catalog_app", "storage_fd_exhaustion_app", "storage_write_error_app")
    Assert-DomainBackendCapabilityMatrixEntry -Matrix $Matrix `
        -Capability "app_exit" `
        -Provider "notification_counter" `
        -Policy "overrides_return=0" `
        -Evidence @("run_log", "capability_counters") `
        -Runs @("exit_app", "exit_error_app", "exit_negative_app", "return_negative_app")
    Assert-DomainBackendCapabilityMatrixEntry -Matrix $Matrix `
        -Capability "afe" `
        -Provider "unsupported_stub" `
        -Policy "configure/read=unsupported:preserve_buffer=1" `
        -Evidence @("run_log", "capability_counters") `
        -Runs @("unsupported_caps_app", "afe_error_app", "store:afe_error_app")
}

function Assert-DomainBackendScope {
    param(
        [object]$Scope,
        [string]$ErrorPrefix = "domain_summary_validate_failed"
    )

    if ($null -eq $Scope -or $Scope.schema -ne "charm.resident_elf_qemu.backend_scope.v1") {
        throw "${ErrorPrefix}: bad backend scope schema"
    }

    $Proves = @($Scope.proves)
    foreach ($Item in @(
            "elf_loader",
            "app_runtime",
            "charm_app_api",
            "capability_backend",
            "received_image",
            "packetstream",
            "store_v1_semantics"
        )) {
        if (-not ($Proves -contains $Item)) {
            throw "${ErrorPrefix}: backend scope proves missing $Item"
        }
    }
    if ($Proves.Count -ne 7) {
        throw "${ErrorPrefix}: backend scope proves count=$($Proves.Count)"
    }

    $DoesNotProve = @($Scope.does_not_prove)
    foreach ($Item in @(
            "h747_usb_cdc",
            "h747_qspi",
            "h747_emmc",
            "h747_fmc_sdram",
            "h747_hal_init",
            "h747_mpu_cache",
            "h747_pinmux"
        )) {
        if (-not ($DoesNotProve -contains $Item)) {
            throw "${ErrorPrefix}: backend scope does_not_prove missing $Item"
        }
    }
    if ($DoesNotProve.Count -ne 7) {
        throw "${ErrorPrefix}: backend scope does_not_prove count=$($DoesNotProve.Count)"
    }
}

function Assert-DomainRuntimeDomainProfile {
    param(
        [object]$Profile,
        [object]$BackendContract,
        [object]$BackendScope
    )

    if ($null -eq $Profile -or $Profile.schema -ne "charm.resident_elf_qemu.runtime_domain_profile.v1") {
        throw "domain_summary_validate_failed: bad runtime domain profile schema"
    }
    if ($Profile.domain -ne "virtual_m7" -or
        $Profile.machine -ne "mps2-an500" -or
        $Profile.cpu -ne "cortex-m7" -or
        $Profile.kind -ne "virtual_board" -or
        $Profile.app_model -ne "CharmAppApi" -or
        $Profile.image_format -ne "elf") {
        throw "domain_summary_validate_failed: bad runtime domain profile identity"
    }

    $Proves = @($Profile.proves)
    foreach ($Item in @(
            "elf_loader",
            "app_runtime",
            "charm_app_api",
            "capability_backend",
            "received_image",
            "packetstream",
            "store_v1_semantics"
        )) {
        if (-not ($Proves -contains $Item)) {
            throw "domain_summary_validate_failed: runtime domain profile proves missing $Item"
        }
    }
    if ($Proves.Count -ne 7) {
        throw "domain_summary_validate_failed: runtime domain profile proves count=$($Proves.Count)"
    }

    $DoesNotProve = @($Profile.does_not_prove)
    foreach ($Item in @(
            "h747_usb_cdc",
            "h747_qspi",
            "h747_emmc",
            "h747_fmc_sdram",
            "h747_hal_init",
            "h747_mpu_cache",
            "h747_pinmux"
        )) {
        if (-not ($DoesNotProve -contains $Item)) {
            throw "domain_summary_validate_failed: runtime domain profile does_not_prove missing $Item"
        }
    }
    if ($DoesNotProve.Count -ne 7) {
        throw "domain_summary_validate_failed: runtime domain profile does_not_prove count=$($DoesNotProve.Count)"
    }
    if ($null -eq $BackendScope) {
        throw "domain_summary_validate_failed: runtime domain profile missing backend scope"
    }
    $ScopeProves = @($BackendScope.proves)
    $ScopeDoesNotProve = @($BackendScope.does_not_prove)
    if (($Proves -join ",") -ne ($ScopeProves -join ",") -or
        ($DoesNotProve -join ",") -ne ($ScopeDoesNotProve -join ",")) {
        throw "domain_summary_validate_failed: runtime domain profile must mirror backend_scope"
    }
    foreach ($Item in $Proves) {
        if (-not ($ScopeProves -contains $Item)) {
            throw "domain_summary_validate_failed: runtime domain profile proves not declared by backend scope: $Item"
        }
    }
    foreach ($Item in $DoesNotProve) {
        if (-not ($ScopeDoesNotProve -contains $Item)) {
            throw "domain_summary_validate_failed: runtime domain profile does_not_prove not declared by backend scope: $Item"
        }
    }

    if ($Profile.run_region.base -ne "0x20080000" -or
        [int]$Profile.run_region.size -ne 65536 -or
        -not [bool]$Profile.run_region.execute) {
        throw "domain_summary_validate_failed: bad runtime domain profile run region"
    }
    if ([int]$Profile.stage_cache.bytes -ne 16384 -or $Profile.store_media -ne "memory") {
        throw "domain_summary_validate_failed: bad runtime domain profile storage/stage"
    }
    if ($Profile.capability_provider.kind -ne "smoke_local_virtual_backend" -or
        $Profile.capability_provider.storage -ne "readonly" -or
        $Profile.capability_provider.afe -ne "unsupported") {
        throw "domain_summary_validate_failed: bad runtime domain profile capability provider"
    }
    $ProfileCapabilities = @($Profile.capability_provider.capabilities)
    $BackendCapabilities = @($BackendContract.capabilities)
    if ($ProfileCapabilities.Count -ne $BackendCapabilities.Count) {
        throw "domain_summary_validate_failed: runtime domain profile capability count mismatch"
    }
    foreach ($Capability in $BackendCapabilities) {
        if (-not ($ProfileCapabilities -contains $Capability)) {
            throw "domain_summary_validate_failed: runtime domain profile capability missing $Capability"
        }
    }
}

function Get-DomainElfRunEvidence {
    param(
        [object[]]$Matrix,
        [string]$Name
    )

    $Matches = @($Matrix | Where-Object { $_.name -eq $Name })
    if ($Matches.Count -ne 1) {
        throw "domain_summary_validate_failed: ELF run evidence missing or duplicate $Name"
    }
    return $Matches[0]
}

function Assert-DomainElfRunEvidence {
    param(
        [object]$Entry,
        [string]$Source,
        [string]$App,
        [string]$SourceStage,
        [string]$SourceCode,
        [string]$LoadProbe,
        [string]$RunStage,
        [string]$RunCode,
        [bool]$Ready,
        [bool]$Fits,
        [int]$Region = 65536
    )

    if ($Entry.source -ne $Source -or
        $Entry.app -ne $App -or
        $Entry.source_stage -ne $SourceStage -or
        $Entry.source_code -ne $SourceCode -or
        $Entry.format -ne "elf" -or
        $Entry.load_probe -ne $LoadProbe -or
        $Entry.capacity_probe -ne $LoadProbe -or
        $Entry.run_stage -ne $RunStage -or
        $Entry.run_code -ne $RunCode -or
        [bool]$Entry.ready -ne $Ready -or
        [bool]$Entry.fits -ne $Fits -or
        [int]$Entry.region -ne $Region) {
        throw "domain_summary_validate_failed: bad ELF run evidence $($Entry.name)"
    }
    if ([int]$Entry.needed -ne [int]$Entry.span) {
        throw "domain_summary_validate_failed: bad ELF run evidence needed/span $($Entry.name)"
    }
    if ($Fits -and [int]$Entry.free -ne ($Region - [int]$Entry.span)) {
        throw "domain_summary_validate_failed: bad ELF run evidence free $($Entry.name)"
    }
    if (-not $Fits -and [int]$Entry.free -ne 0) {
        throw "domain_summary_validate_failed: bad ELF run evidence overflow free $($Entry.name)"
    }
}

function Assert-DomainElfRunEvidenceMatchesLoad {
    param(
        [object[]]$Matrix,
        [object[]]$Loads
    )

    foreach ($Load in @($Loads)) {
        $Entry = Get-DomainElfRunEvidence -Matrix $Matrix -Name ([string]$Load.name)
        foreach ($Property in @(
                "format",
                "link_base",
                "expected_base",
                "entry",
                "entry_vaddr",
                "span",
                "segments",
                "base_match",
                "needed",
                "free",
                "fits",
                "region",
                "capacity_probe"
            )) {
            if ($Entry.$Property -ne $Load.$Property) {
                throw "domain_summary_validate_failed: ELF run evidence $($Load.name) $Property differs from load"
            }
        }
        if ($Entry.load_probe -ne $Load.probe) {
            throw "domain_summary_validate_failed: ELF run evidence $($Load.name) probe differs from load"
        }
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

function Assert-SourceMatrixExit {
    param(
        [object[]]$Matrix,
        [string]$Name,
        [string[]]$Sources,
        [int]$Exit
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
        if ($Value.stage -ne "exit" -or $Value.code -ne "ok" -or [int]$Value.exit -ne $Exit) {
            throw "domain_summary_validate_failed: source matrix $Name source $Source did not exit with $Exit"
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

function New-SelfTestBackendContractCapture {
    $SyntheticLog = Get-SyntheticPassingLog
    $BackendIdentity = Get-BackendIdentityFromText -Text $SyntheticLog
    $BackendContract = Get-BackendContractFromText -Text $SyntheticLog `
        -RuntimeDomain $BackendIdentity.runtime_domain `
        -Capabilities $BackendIdentity.capabilities `
        -StorageMode $BackendIdentity.storage `
        -AfeMode $BackendIdentity.afe
    $BackendScope = Get-BackendScopeFromText -Text $SyntheticLog
    $BackendSelfCheck = Get-BackendSelfCheckFromText -Text $SyntheticLog
    $BackendResetSelfCheck = Get-BackendResetSelfCheckFromText -Text $SyntheticLog
    $BackendReadiness = [pscustomobject]@{
        status = "ready"
        elf_loader = $true
        app_runtime = $true
        received = $true
        packetstream = $true
        store = $true
        equivalent_sources = $true
        gui = $true
        storage = $true
        input = $true
        unsupported_boundary = $true
        runtime_reset = $true
    }
    $BackendCapabilityMatrix = Get-QemuBackendCapabilityMatrix -BackendContract $BackendContract
    $RuntimeDomainProfile = Get-QemuRuntimeDomainProfile `
        -RuntimeDomain $BackendIdentity.runtime_domain `
        -Machine $BackendIdentity.machine `
        -Cpu $BackendIdentity.cpu `
        -BackendContract $BackendContract `
        -BackendScope $BackendScope

    return [pscustomobject]@{
        schema = "charm.resident_elf_qemu.backend_contract.v1"
        domain = $BackendIdentity.runtime_domain
        machine = $BackendIdentity.machine
        cpu = $BackendIdentity.cpu
        image_format = "elf"
        app_model = "CharmAppApi"
        backend_scope = $BackendScope
        backend_contract = $BackendContract
        backend_self_check = $BackendSelfCheck
        backend_reset_self_check = $BackendResetSelfCheck
        backend_readiness = $BackendReadiness
        backend_capability_matrix = $BackendCapabilityMatrix
        runtime_domain_profile = $RuntimeDomainProfile
    }
}

function Write-SelfTestBackendContractCapture {
    param(
        [object]$Capture,
        [string]$Path
    )

    [System.IO.File]::WriteAllText($Path, (($Capture | ConvertTo-Json -Depth 16) + "`n"), [System.Text.UTF8Encoding]::new($false))
}

function Assert-BackendContractCaptureAccepted {
    param(
        [object]$Capture,
        [string]$Label
    )

    $TempPath = Join-Path ([System.IO.Path]::GetTempPath()) ("charm_resident_elf_qemu_good_backend_{0}.json" -f $Label)
    try {
        Write-SelfTestBackendContractCapture -Capture $Capture -Path $TempPath
        if ((Validate-BackendContractFile -Path $TempPath) -ne 0) {
            throw "selftest_failed: backend contract '$Label' returned nonzero validation"
        }
    } finally {
        Remove-Item -LiteralPath $TempPath -Force -ErrorAction SilentlyContinue
    }
}

function Assert-BadBackendContractRejected {
    param(
        [object]$Capture,
        [string]$Label,
        [scriptblock]$Mutate
    )

    $TempPath = Join-Path ([System.IO.Path]::GetTempPath()) ("charm_resident_elf_qemu_bad_backend_{0}.json" -f $Label)
    $BadCapture = $Capture | ConvertTo-Json -Depth 16 | ConvertFrom-Json
    & $Mutate $BadCapture
    try {
        Write-SelfTestBackendContractCapture -Capture $BadCapture -Path $TempPath
        if (-not (Test-SelfTestThrowsLike -Prefix "backend_contract_validate_failed:" -Script { Validate-BackendContractFile -Path $TempPath })) {
            throw "selftest_failed: bad backend contract '$Label' validated unexpectedly"
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
    if ($null -eq $Summary.run_budget -or
        [int]$Summary.run_budget.timeout_sec -le 0 -or
        [int]$Summary.run_budget.tail_lines -le 0) {
        throw "domain_summary_validate_failed: bad run budget"
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
    if ($Summary.backend_contract.time.kind -ne "deterministic_tick" -or
        [int]$Summary.backend_contract.time.start_ms -ne 1000 -or
        [int]$Summary.backend_contract.time.step_ms -ne 17 -or
        -not [bool]$Summary.backend_contract.time.reset_per_run) {
        throw "domain_summary_validate_failed: bad time backend contract"
    }
    $BackendDisplayEvidence = @($Summary.backend_contract.display.evidence)
    foreach ($Evidence in @("frame_signatures", "frame_dumps", "frame_ppm", "gui_timeline")) {
        if (-not ($BackendDisplayEvidence -contains $Evidence)) {
            throw "domain_summary_validate_failed: display backend evidence missing $Evidence"
        }
    }
    if ($Summary.backend_contract.display.kind -ne "framebuffer" -or
        [int]$Summary.backend_contract.display.width -ne 16 -or
        [int]$Summary.backend_contract.display.height -ne 16 -or
        [int]$Summary.backend_contract.display.stride_bytes -ne 64 -or
        $Summary.backend_contract.display.format -ne "argb8888" -or
        [int]$Summary.backend_contract.display.frame_bytes -ne 1024 -or
        $BackendDisplayEvidence.Count -ne 4) {
        throw "domain_summary_validate_failed: bad display backend contract"
    }
    $BackendInputEvidence = @($Summary.backend_contract.input.evidence)
    foreach ($Evidence in @("input_trace", "gui_timeline")) {
        if (-not ($BackendInputEvidence -contains $Evidence)) {
            throw "domain_summary_validate_failed: input backend evidence missing $Evidence"
        }
    }
    if ($Summary.backend_contract.input.kind -ne "deterministic_sequence" -or
        [int]$Summary.backend_contract.input.sample_count -ne 4 -or
        [int]$Summary.backend_contract.input.pointer_max_x -ne 15 -or
        [int]$Summary.backend_contract.input.pointer_max_y -ne 15 -or
        -not [bool]$Summary.backend_contract.input.wraps -or
        $BackendInputEvidence.Count -ne 2) {
        throw "domain_summary_validate_failed: bad input backend contract"
    }
    $BackendStorageEvidence = @($Summary.backend_contract.storage_media.evidence)
    if ($Summary.backend_contract.storage_media.kind -ne "virtual_readonly_files" -or
        [int]$Summary.backend_contract.storage_media.file_count -ne 3 -or
        [int]$Summary.backend_contract.storage_media.fd_base -ne 3 -or
        [int]$Summary.backend_contract.storage_media.fd_slots -ne 4 -or
        $Summary.backend_contract.storage_media.write_policy -ne "unsupported" -or
        $BackendStorageEvidence.Count -ne 1 -or
        -not ($BackendStorageEvidence -contains "storage_trace")) {
        throw "domain_summary_validate_failed: bad storage backend contract"
    }
    if ($Summary.backend_contract.app_exit.kind -ne "notification_counter" -or
        [bool]$Summary.backend_contract.app_exit.overrides_return) {
        throw "domain_summary_validate_failed: bad app_exit backend contract"
    }
    Assert-BackendSelfCheck `
        -SelfCheck $Summary.backend_self_check `
        -ErrorPrefix "domain_summary_validate_failed"
    Assert-BackendResetSelfCheck `
        -SelfCheck $Summary.backend_reset_self_check `
        -ErrorPrefix "domain_summary_validate_failed"
    Assert-DomainBackendScope -Scope $Summary.backend_scope
    Assert-DomainBackendReadiness -Readiness $Summary.backend_readiness
    Assert-DomainBackendCapabilityMatrix -Matrix @($Summary.backend_capability_matrix)
    Assert-DomainRuntimeDomainProfile -Profile $Summary.runtime_domain_profile -BackendContract $Summary.backend_contract -BackendScope $Summary.backend_scope
    Assert-DomainRuntimeResetDeterminism -Reset $Summary.runtime_reset_determinism
    $ElfRunEvidenceMatrix = @($Summary.elf_run_evidence_matrix)
    Assert-DomainCount -Name "elf_run_evidence_matrix" -Actual $ElfRunEvidenceMatrix.Count -Expected 89
    if ($Summary.run_region.base -ne "0x20080000" -or $Summary.run_region.expected -ne "0x20080000" -or [int]$Summary.run_region.size -ne 65536) {
        throw "domain_summary_validate_failed: bad run region"
    }
    if ([int]$Summary.stage_cache.bytes -ne 16384) {
        throw "domain_summary_validate_failed: bad stage cache size"
    }
    if ([int]$Summary.packetstream_buffers.storage_bytes -ne 16384 -or
        [int]$Summary.packetstream_buffers.transport_bytes -ne 2048 -or
        [int]$Summary.packetstream_buffers.stream_bytes -ne 32768 -or
        [int]$Summary.packetstream_buffers.received_bytes -ne 16384) {
        throw "domain_summary_validate_failed: bad packetstream buffer sizes"
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
    Assert-DomainArtifacts -Artifacts $Summary.artifacts -Store $Summary.store
    $Runs = @($Summary.coverage.runs)
    Assert-DomainCount -Name "runs" -Actual $Runs.Count -Expected 88
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
    Assert-DomainRun -Runs $Runs -Name "afe_error_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "store:afe_error_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "data_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "console_error_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "store:console_error_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "store:data_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "display_error_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "store:display_error_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "display_null_present_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "store:display_null_present_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "input_error_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "store:input_error_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "input_wrap_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "store:input_wrap_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "storage_error_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "store:storage_error_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "storage_fd_exhaustion_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "store:storage_fd_exhaustion_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "storage_open_error_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "store:storage_open_error_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "storage_write_error_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "store:storage_write_error_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "storage_zero_io_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "store:storage_zero_io_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "storage_close_error_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "store:storage_close_error_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "exit_error_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "store:exit_error_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "exit_negative_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "store:exit_negative_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "return_negative_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "store:return_negative_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "display_describe_error_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "store:display_describe_error_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "time_sequence_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "store:time_sequence_app" -Stage "exit" -Code "ok"
    Assert-DomainRun -Runs $Runs -Name "bad_elf_magic_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "packetstream:packetstream_bad_elf_magic_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "bad_header_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "bad_class_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "bad_endian_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "bad_ident_version_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "bad_type_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "bad_machine_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "bad_version_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "bad_ehsize_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "bad_phentsize_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "bad_program_header_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "truncated_payload_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "no_load_segment_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "entry_outside_segment_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "overlapping_segments_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "rwx_segment_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "wrong_link_base_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "too_large_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "unaligned_load_buffer_app" -Stage "load" -Code "load_failed"
    Assert-DomainRun -Runs $Runs -Name "argv_overflow_app" -Stage "argv" -Code "argv_overflow"
    Assert-DomainRun -Runs $Runs -Name "abi_mismatch_app" -Stage "abi" -Code "abi_mismatch"
    $Stages = @($Summary.coverage.stages)
    Assert-DomainCount -Name "stages" -Actual $Stages.Count -Expected 36
    Assert-DomainStage -Stages $Stages -Source "received" -Name "hello_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "received" -Name "large_fit_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "store" -Name "hello_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "store" -Name "afe_error_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "store" -Name "console_error_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "store" -Name "data_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "store" -Name "display_error_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "store" -Name "display_null_present_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "store" -Name "input_error_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "store" -Name "input_wrap_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "store" -Name "storage_close_error_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "store" -Name "storage_error_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "store" -Name "storage_fd_exhaustion_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "store" -Name "storage_open_error_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "store" -Name "storage_write_error_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "store" -Name "storage_zero_io_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "store" -Name "exit_error_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "store" -Name "exit_negative_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "store" -Name "return_negative_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "store" -Name "display_describe_error_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "store" -Name "time_sequence_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "store" -Name "large_fit_app" -Code "ok"
    Assert-DomainStage -Stages $Stages -Source "store" -Name "too_large_store_app" -Code "image_too_large"
    $Loads = @($Summary.coverage.loads)
    Assert-DomainCount -Name "loads" -Actual $Loads.Count -Expected 89
    Assert-DomainLoad -Loads $Loads -Name "hello_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "received:hello_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "packetstream:hello_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "store:hello_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "prepare:argv_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "afe_error_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "store:afe_error_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "player_min" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 3
    Assert-DomainLoad -Loads $Loads -Name "received:player_min" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 3
    Assert-DomainLoad -Loads $Loads -Name "packetstream:player_min" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 3
    Assert-DomainLoad -Loads $Loads -Name "store:player_min" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 3
    Assert-DomainLoad -Loads $Loads -Name "bss_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 3
    Assert-DomainLoad -Loads $Loads -Name "console_error_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "store:console_error_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "data_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 3
    Assert-DomainLoad -Loads $Loads -Name "store:data_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 3
    Assert-DomainLoad -Loads $Loads -Name "display_error_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 3
    Assert-DomainLoad -Loads $Loads -Name "store:display_error_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 3
    Assert-DomainLoad -Loads $Loads -Name "display_null_present_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "store:display_null_present_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "input_error_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "store:input_error_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "storage_error_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "store:storage_error_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "storage_fd_exhaustion_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "store:storage_fd_exhaustion_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "storage_open_error_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "store:storage_open_error_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "storage_write_error_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "store:storage_write_error_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "storage_zero_io_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "store:storage_zero_io_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "storage_close_error_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "store:storage_close_error_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "exit_error_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 1
    Assert-DomainLoad -Loads $Loads -Name "store:exit_error_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 1
    Assert-DomainLoad -Loads $Loads -Name "exit_negative_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 1
    Assert-DomainLoad -Loads $Loads -Name "store:exit_negative_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 1
    Assert-DomainLoad -Loads $Loads -Name "return_negative_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 1
    Assert-DomainLoad -Loads $Loads -Name "store:return_negative_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 1
    Assert-DomainLoad -Loads $Loads -Name "display_describe_error_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "store:display_describe_error_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "time_sequence_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 1
    Assert-DomainLoad -Loads $Loads -Name "store:time_sequence_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 1
    Assert-DomainLoad -Loads $Loads -Name "large_fit_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 60000 -Segments 3
    Assert-DomainLoad -Loads $Loads -Name "received:large_fit_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 60000 -Segments 3
    Assert-DomainLoad -Loads $Loads -Name "packetstream:large_fit_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 60000 -Segments 3
    Assert-DomainLoad -Loads $Loads -Name "store:large_fit_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 60000 -Segments 3
    foreach ($EquivalentName in @("hello_app", "player_min", "large_fit_app")) {
        Assert-EquivalentElfEntrySources -Runs $Runs -Loads $Loads -Name $EquivalentName -Sources @("direct", "received", "packetstream", "store")
    }
    Assert-DomainLoad -Loads $Loads -Name "bad_elf_magic_app" -Probe "bad_magic" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "packetstream:packetstream_bad_elf_magic_app" -Probe "bad_magic" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "bad_header_app" -Probe "bad_header" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "bad_class_app" -Probe "bad_class" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "bad_endian_app" -Probe "bad_endian" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "bad_ident_version_app" -Probe "bad_header" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "bad_type_app" -Probe "bad_header" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "bad_machine_app" -Probe "bad_header" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "bad_version_app" -Probe "bad_header" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "bad_ehsize_app" -Probe "bad_header" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "bad_phentsize_app" -Probe "bad_program_header" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "bad_program_header_app" -Probe "bad_program_header" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "truncated_payload_app" -Probe "truncated_payload" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "no_load_segment_app" -Probe "no_load_segment" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "entry_outside_segment_app" -Probe "entry_outside_segment" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "overlapping_segments_app" -Probe "overlapping_segments" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "rwx_segment_app" -Probe "rwx_segment" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainLoad -Loads $Loads -Name "wrong_link_base_app" -Probe "ok" -Fits $true -Region 65536 -MinSpan 1 -Segments 2 -ExpectedBase "0x20080000" -ExpectedLinkBase "0x20081000" -ExpectBaseMatch $false
    Assert-DomainLoad -Loads $Loads -Name "too_large_app" -Probe "load_buffer_too_small" -Fits $false -Region 65536 -MinSpan 65537 -Segments 2
    Assert-DomainLoad -Loads $Loads -Name "unaligned_load_buffer_app" -Probe "load_buffer_unaligned" -Fits $true -Region 65536 -MinSpan 0 -Segments 0
    Assert-DomainElfRunEvidenceMatchesLoad -Matrix $ElfRunEvidenceMatrix -Loads $Loads
    Assert-DomainElfRunEvidence `
        -Entry (Get-DomainElfRunEvidence -Matrix $ElfRunEvidenceMatrix -Name "hello_app") `
        -Source "direct" `
        -App "hello_app" `
        -SourceStage "image" `
        -SourceCode "ok" `
        -LoadProbe "ok" `
        -RunStage "exit" `
        -RunCode "ok" `
        -Ready $true `
        -Fits $true
    Assert-DomainElfRunEvidence `
        -Entry (Get-DomainElfRunEvidence -Matrix $ElfRunEvidenceMatrix -Name "received:hello_app") `
        -Source "received" `
        -App "hello_app" `
        -SourceStage "stage" `
        -SourceCode "ok" `
        -LoadProbe "ok" `
        -RunStage "exit" `
        -RunCode "ok" `
        -Ready $true `
        -Fits $true
    Assert-DomainElfRunEvidence `
        -Entry (Get-DomainElfRunEvidence -Matrix $ElfRunEvidenceMatrix -Name "packetstream:hello_app") `
        -Source "packetstream" `
        -App "hello_app" `
        -SourceStage "launch_ready" `
        -SourceCode "ok" `
        -LoadProbe "ok" `
        -RunStage "exit" `
        -RunCode "ok" `
        -Ready $true `
        -Fits $true
    Assert-DomainElfRunEvidence `
        -Entry (Get-DomainElfRunEvidence -Matrix $ElfRunEvidenceMatrix -Name "store:hello_app") `
        -Source "store" `
        -App "hello_app" `
        -SourceStage "stage" `
        -SourceCode "ok" `
        -LoadProbe "ok" `
        -RunStage "exit" `
        -RunCode "ok" `
        -Ready $true `
        -Fits $true
    Assert-DomainElfRunEvidence `
        -Entry (Get-DomainElfRunEvidence -Matrix $ElfRunEvidenceMatrix -Name "prepare:argv_app") `
        -Source "prepare" `
        -App "argv_app" `
        -SourceStage "start" `
        -SourceCode "ok" `
        -LoadProbe "ok" `
        -RunStage "start" `
        -RunCode "ok" `
        -Ready $true `
        -Fits $true
    Assert-DomainElfRunEvidence `
        -Entry (Get-DomainElfRunEvidence -Matrix $ElfRunEvidenceMatrix -Name "bad_elf_magic_app") `
        -Source "direct" `
        -App "bad_elf_magic_app" `
        -SourceStage "image" `
        -SourceCode "ok" `
        -LoadProbe "bad_magic" `
        -RunStage "load" `
        -RunCode "load_failed" `
        -Ready $false `
        -Fits $true
    Assert-DomainElfRunEvidence `
        -Entry (Get-DomainElfRunEvidence -Matrix $ElfRunEvidenceMatrix -Name "packetstream:packetstream_bad_elf_magic_app") `
        -Source "packetstream" `
        -App "packetstream_bad_elf_magic_app" `
        -SourceStage "launch_ready" `
        -SourceCode "ok" `
        -LoadProbe "bad_magic" `
        -RunStage "load" `
        -RunCode "load_failed" `
        -Ready $false `
        -Fits $true
    Assert-DomainElfRunEvidence `
        -Entry (Get-DomainElfRunEvidence -Matrix $ElfRunEvidenceMatrix -Name "wrong_link_base_app") `
        -Source "direct" `
        -App "wrong_link_base_app" `
        -SourceStage "image" `
        -SourceCode "ok" `
        -LoadProbe "ok" `
        -RunStage "load" `
        -RunCode "load_failed" `
        -Ready $false `
        -Fits $true
    Assert-DomainElfRunEvidence `
        -Entry (Get-DomainElfRunEvidence -Matrix $ElfRunEvidenceMatrix -Name "too_large_app") `
        -Source "direct" `
        -App "too_large_app" `
        -SourceStage "image" `
        -SourceCode "ok" `
        -LoadProbe "load_buffer_too_small" `
        -RunStage "load" `
        -RunCode "load_failed" `
        -Ready $false `
        -Fits $false
    Assert-DomainElfRunEvidence `
        -Entry (Get-DomainElfRunEvidence -Matrix $ElfRunEvidenceMatrix -Name "unaligned_load_buffer_app") `
        -Source "direct" `
        -App "unaligned_load_buffer_app" `
        -SourceStage "image" `
        -SourceCode "ok" `
        -LoadProbe "load_buffer_unaligned" `
        -RunStage "load" `
        -RunCode "load_failed" `
        -Ready $false `
        -Fits $true
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
    $CapabilityCounters = @($Summary.coverage.capability_counters)
    Assert-DomainCount -Name "capability_counters" -Actual $CapabilityCounters.Count -Expected 93
    foreach ($ExpectedCounter in @(
            [pscustomobject]@{ name = "bss_app"; console = 22; time = 0; input = 0; present = 0; storage_open = 0; display_frame = 0 },
            [pscustomobject]@{ name = "store:bss_app"; console = 22; time = 0; input = 0; present = 0; storage_open = 0; display_frame = 0 },
            [pscustomobject]@{ name = "data_app"; console = 35; time = 0; input = 0; present = 0; storage_open = 0; display_frame = 0 },
            [pscustomobject]@{ name = "store:data_app"; console = 35; time = 0; input = 0; present = 0; storage_open = 0; display_frame = 0 },
            [pscustomobject]@{ name = "time_sequence_app"; console = 0; time = 4; input = 0; present = 0; storage_open = 0; display_frame = 0 },
            [pscustomobject]@{ name = "store:time_sequence_app"; console = 0; time = 4; input = 0; present = 0; storage_open = 0; display_frame = 0 },
            [pscustomobject]@{ name = "input_sequence_app"; console = 41; time = 0; input = 4; present = 0; storage_open = 0; input_checksum = 114; display_frame = 0 },
            [pscustomobject]@{ name = "store:input_sequence_app"; console = 41; time = 0; input = 4; present = 0; storage_open = 0; input_checksum = 114; display_frame = 0 },
            [pscustomobject]@{ name = "display_sequence_app"; console = 45; time = 0; input = 0; present = 2; storage_open = 0; display_checksum_total = 3072; display_frame = 2 },
            [pscustomobject]@{ name = "store:display_sequence_app"; console = 45; time = 0; input = 0; present = 2; storage_open = 0; display_checksum_total = 3072; display_frame = 2 },
            [pscustomobject]@{ name = "storage_fd_exhaustion_app"; console = 57; time = 0; input = 0; present = 0; storage_open = 6; storage_read = 1; storage_close = 5; storage_bytes = 1; display_frame = 0 },
            [pscustomobject]@{ name = "store:storage_fd_exhaustion_app"; console = 57; time = 0; input = 0; present = 0; storage_open = 6; storage_read = 1; storage_close = 5; storage_bytes = 1; display_frame = 0 },
            [pscustomobject]@{ name = "player_min"; console = 32; time = 1; input = 1; present = 1; input_checksum = 26; display_checksum_total = 174720; display_frame = 1 },
            [pscustomobject]@{ name = "received:player_min"; console = 32; time = 1; input = 1; present = 1; input_checksum = 26; display_checksum_total = 174720; display_frame = 1 },
            [pscustomobject]@{ name = "packetstream:player_min"; console = 32; time = 1; input = 1; present = 1; input_checksum = 26; display_checksum_total = 174720; display_frame = 1 },
            [pscustomobject]@{ name = "store:player_min"; console = 32; time = 1; input = 1; present = 1; input_checksum = 26; display_checksum_total = 174720; display_frame = 1 }
        )) {
        $Counter = Get-QemuCapsByName -CapabilityCounters $CapabilityCounters -Name $ExpectedCounter.name
        if ($null -eq $Counter) {
            throw "domain_summary_validate_failed: missing capability counter $($ExpectedCounter.name)"
        }
        foreach ($Property in @($ExpectedCounter.PSObject.Properties | ForEach-Object { $_.Name } | Where-Object { $_ -ne "name" })) {
            if ([string]$Counter.$Property -ne [string]$ExpectedCounter.$Property) {
                throw "domain_summary_validate_failed: bad capability counter $($ExpectedCounter.name).$Property"
            }
        }
    }
    $NegativeCases = @($Summary.coverage.negative_cases)
    Assert-DomainCount -Name "negative_cases" -Actual $NegativeCases.Count -Expected 25
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "packetstream_crc_mismatch" -Stage "packetstream_verify" -Code "crc_mismatch"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "received_too_large_app" -Stage "received_stage" -Code "buffer_too_small"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "too_large_store_app" -Stage "store_stage" -Code "image_too_large"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "bad_elf_magic_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "packetstream_bad_elf_magic_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "bad_header_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "bad_class_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "bad_endian_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "bad_ident_version_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "bad_type_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "bad_machine_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "bad_version_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "bad_ehsize_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "bad_phentsize_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "bad_program_header_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "truncated_payload_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "no_load_segment_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "entry_outside_segment_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "overlapping_segments_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "rwx_segment_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "wrong_link_base_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "too_large_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "unaligned_load_buffer_app" -Stage "load" -Code "load_failed"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "argv_overflow_app" -Stage "argv" -Code "argv_overflow"
    Assert-DomainNegativeCase -NegativeCases $NegativeCases -Name "abi_mismatch_app" -Stage "abi" -Code "abi_mismatch"
    Assert-DomainFailureTaxonomy -Taxonomy $Summary.failure_taxonomy -NegativeCases $NegativeCases
    $SourceMatrix = @($Summary.coverage.source_matrix)
    Assert-DomainCount -Name "source_matrix" -Actual $SourceMatrix.Count -Expected 52
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "hello_app" -Sources @("direct", "received", "packetstream", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "large_fit_app" -Sources @("direct", "received", "packetstream", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "player_min" -Sources @("direct", "received", "packetstream", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "afe_error_app" -Sources @("direct", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "argv_app" -Sources @("direct", "store", "prepare")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "bss_app" -Sources @("direct", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "console_error_app" -Sources @("direct", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "data_app" -Sources @("direct", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "display_error_app" -Sources @("direct", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "display_describe_error_app" -Sources @("direct", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "display_null_present_app" -Sources @("direct", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "display_sequence_app" -Sources @("direct", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "input_error_app" -Sources @("direct", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "input_sequence_app" -Sources @("direct", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "input_wrap_app" -Sources @("direct", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "storage_catalog_app" -Sources @("direct", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "storage_close_error_app" -Sources @("direct", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "storage_error_app" -Sources @("direct", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "storage_fd_exhaustion_app" -Sources @("direct", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "storage_open_error_app" -Sources @("direct", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "storage_write_error_app" -Sources @("direct", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "storage_zero_io_app" -Sources @("direct", "store")
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "time_sequence_app" -Sources @("direct", "store")
    Assert-SourceMatrixExit -Matrix $SourceMatrix -Name "exit_error_app" -Sources @("direct", "store") -Exit 42
    Assert-SourceMatrixEntry -Matrix $SourceMatrix -Name "exit_negative_app" -Sources @("direct", "store")
    Assert-SourceMatrixExit -Matrix $SourceMatrix -Name "return_negative_app" -Sources @("direct", "store") -Exit -5
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "bad_header_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "bad_class_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "bad_endian_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "bad_ident_version_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "bad_type_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "bad_machine_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "bad_version_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "bad_ehsize_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "bad_phentsize_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "bad_program_header_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "truncated_payload_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "no_load_segment_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "entry_outside_segment_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "overlapping_segments_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "rwx_segment_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "wrong_link_base_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "too_large_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "unaligned_load_buffer_app" -Source "direct" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "packetstream_bad_elf_magic_app" -Source "packetstream" -Stage "load" -Code "load_failed"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "argv_overflow_app" -Source "direct" -Stage "argv" -Code "argv_overflow"
    Assert-SourceMatrixFailure -Matrix $SourceMatrix -Name "abi_mismatch_app" -Source "direct" -Stage "abi" -Code "abi_mismatch"
    $GuiTimeline = @($Summary.coverage.gui_timeline)
    Assert-DomainCount -Name "gui_timeline" -Actual $GuiTimeline.Count -Expected 10
    Assert-GuiTimelineEntry -Timeline $GuiTimeline -Name "display_sequence_app" -Frames 2 -Inputs 0 -LastFrameHash "0xa9b09dc5" -LastInput ""
    Assert-GuiTimelineEntry -Timeline $GuiTimeline -Name "store:display_sequence_app" -Frames 2 -Inputs 0 -LastFrameHash "0xa9b09dc5" -LastInput ""
    Assert-GuiTimelineEntry -Timeline $GuiTimeline -Name "input_sequence_app" -Frames 0 -Inputs 4 -LastFrameHash "" -LastInput "6,8,0"
    Assert-GuiTimelineEntry -Timeline $GuiTimeline -Name "store:input_sequence_app" -Frames 0 -Inputs 4 -LastFrameHash "" -LastInput "6,8,0"
    Assert-GuiTimelineEntry -Timeline $GuiTimeline -Name "input_wrap_app" -Frames 0 -Inputs 6 -LastFrameHash "" -LastInput "4,6,1"
    Assert-GuiTimelineEntry -Timeline $GuiTimeline -Name "store:input_wrap_app" -Frames 0 -Inputs 6 -LastFrameHash "" -LastInput "4,6,1"
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
    if ([int]$Summary.evidence.input_trace_event_count -ne 24 -or [int]$Summary.evidence.input_trace_run_count -ne 8) {
        throw "domain_summary_validate_failed: bad input trace evidence"
    }
    if ([int]$Summary.evidence.storage_trace_event_count -ne 130 -or [int]$Summary.evidence.storage_trace_run_count -ne 18) {
        throw "domain_summary_validate_failed: bad storage trace evidence"
    }
    Write-Host "resident-elf-qemu domain summary validation ok"
    Write-Host "  path=$ResolvedPath"
    return 0
}

function Validate-BackendContractFile {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "backend_contract_validate_failed: path is empty"
    }
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "backend_contract_validate_failed: file not found: $Path"
    }
    $ResolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $Contract = Get-Content -LiteralPath $ResolvedPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($Contract.schema -ne "charm.resident_elf_qemu.backend_contract.v1") {
        throw "backend_contract_validate_failed: bad schema: $($Contract.schema)"
    }
    if ($Contract.domain -ne "virtual_m7" -or
        $Contract.machine -ne "mps2-an500" -or
        $Contract.cpu -ne "cortex-m7") {
        throw "backend_contract_validate_failed: bad domain identity"
    }
    if ($Contract.image_format -ne "elf" -or $Contract.app_model -ne "CharmAppApi") {
        throw "backend_contract_validate_failed: bad app model"
    }
    if ($Contract.backend_contract.kind -ne "virtual" -or
        $Contract.backend_contract.runtime_domain -ne "virtual_m7" -or
        $Contract.backend_contract.storage -ne "readonly" -or
        $Contract.backend_contract.afe -ne "unsupported") {
        throw "backend_contract_validate_failed: bad backend contract"
    }
    $BackendCapabilities = @($Contract.backend_contract.capabilities)
    foreach ($Capability in @("console", "time", "display", "input", "storage", "app_exit")) {
        if (-not ($BackendCapabilities -contains $Capability)) {
            throw "backend_contract_validate_failed: backend capability missing $Capability"
        }
    }
    if ($BackendCapabilities.Count -ne 6) {
        throw "backend_contract_validate_failed: unexpected backend capability count"
    }
    if ($Contract.backend_contract.time.kind -ne "deterministic_tick" -or
        [int]$Contract.backend_contract.time.start_ms -ne 1000 -or
        [int]$Contract.backend_contract.time.step_ms -ne 17 -or
        -not [bool]$Contract.backend_contract.time.reset_per_run) {
        throw "backend_contract_validate_failed: bad time backend contract"
    }
    $BackendDisplayEvidence = @($Contract.backend_contract.display.evidence)
    foreach ($Evidence in @("frame_signatures", "frame_dumps", "frame_ppm", "gui_timeline")) {
        if (-not ($BackendDisplayEvidence -contains $Evidence)) {
            throw "backend_contract_validate_failed: display backend evidence missing $Evidence"
        }
    }
    if ($Contract.backend_contract.display.kind -ne "framebuffer" -or
        [int]$Contract.backend_contract.display.width -ne 16 -or
        [int]$Contract.backend_contract.display.height -ne 16 -or
        [int]$Contract.backend_contract.display.stride_bytes -ne 64 -or
        $Contract.backend_contract.display.format -ne "argb8888" -or
        [int]$Contract.backend_contract.display.frame_bytes -ne 1024 -or
        $BackendDisplayEvidence.Count -ne 4) {
        throw "backend_contract_validate_failed: bad display backend contract"
    }
    $BackendInputEvidence = @($Contract.backend_contract.input.evidence)
    foreach ($Evidence in @("input_trace", "gui_timeline")) {
        if (-not ($BackendInputEvidence -contains $Evidence)) {
            throw "backend_contract_validate_failed: input backend evidence missing $Evidence"
        }
    }
    if ($Contract.backend_contract.input.kind -ne "deterministic_sequence" -or
        [int]$Contract.backend_contract.input.sample_count -ne 4 -or
        [int]$Contract.backend_contract.input.pointer_max_x -ne 15 -or
        [int]$Contract.backend_contract.input.pointer_max_y -ne 15 -or
        -not [bool]$Contract.backend_contract.input.wraps -or
        $BackendInputEvidence.Count -ne 2) {
        throw "backend_contract_validate_failed: bad input backend contract"
    }
    $BackendStorageEvidence = @($Contract.backend_contract.storage_media.evidence)
    if ($Contract.backend_contract.storage_media.kind -ne "virtual_readonly_files" -or
        [int]$Contract.backend_contract.storage_media.file_count -ne 3 -or
        [int]$Contract.backend_contract.storage_media.fd_base -ne 3 -or
        [int]$Contract.backend_contract.storage_media.fd_slots -ne 4 -or
        $Contract.backend_contract.storage_media.write_policy -ne "unsupported" -or
        $BackendStorageEvidence.Count -ne 1 -or
        -not ($BackendStorageEvidence -contains "storage_trace")) {
        throw "backend_contract_validate_failed: bad storage backend contract"
    }
    if ($Contract.backend_contract.app_exit.kind -ne "notification_counter" -or
        [bool]$Contract.backend_contract.app_exit.overrides_return) {
        throw "backend_contract_validate_failed: bad app_exit backend contract"
    }
    Assert-BackendSelfCheck `
        -SelfCheck $Contract.backend_self_check `
        -ErrorPrefix "backend_contract_validate_failed"
    Assert-BackendResetSelfCheck `
        -SelfCheck $Contract.backend_reset_self_check `
        -ErrorPrefix "backend_contract_validate_failed"
    try {
        Assert-DomainBackendScope -Scope $Contract.backend_scope -ErrorPrefix "backend_contract_validate_failed"
        Assert-DomainBackendReadiness -Readiness $Contract.backend_readiness
        Assert-DomainBackendCapabilityMatrix -Matrix @($Contract.backend_capability_matrix)
        Assert-DomainRuntimeDomainProfile -Profile $Contract.runtime_domain_profile -BackendContract $Contract.backend_contract -BackendScope $Contract.backend_scope
    } catch {
        throw "backend_contract_validate_failed: $($_.Exception.Message)"
    }
    Write-Host "resident-elf-qemu backend contract validation ok"
    Write-Host "  path=$ResolvedPath"
    return 0
}

function Export-BackendContractFromDomainSummary {
    param(
        [string]$DomainSummaryPath,
        [string]$BackendContractPath
    )

    if ([string]::IsNullOrWhiteSpace($DomainSummaryPath)) {
        throw "backend_contract_export_failed: domain summary path is empty"
    }
    if ([string]::IsNullOrWhiteSpace($BackendContractPath)) {
        throw "backend_contract_export_failed: backend contract path is empty"
    }
    if (-not (Test-Path -LiteralPath $DomainSummaryPath)) {
        throw "backend_contract_export_failed: domain summary not found: $DomainSummaryPath"
    }
    $ResolvedDomainSummary = (Resolve-Path -LiteralPath $DomainSummaryPath).Path
    [void](Validate-DomainSummaryFile -Path $ResolvedDomainSummary)
    $Summary = Get-Content -LiteralPath $ResolvedDomainSummary -Raw -Encoding UTF8 | ConvertFrom-Json
    $ResolvedBackendContract = [System.IO.Path]::GetFullPath($BackendContractPath)
    $BackendContractDir = [System.IO.Path]::GetDirectoryName($ResolvedBackendContract)
    if (-not [string]::IsNullOrWhiteSpace($BackendContractDir) -and -not (Test-Path -LiteralPath $BackendContractDir)) {
        New-Item -ItemType Directory -Force -Path $BackendContractDir | Out-Null
    }
    $Capture = [pscustomobject]@{
        schema = "charm.resident_elf_qemu.backend_contract.v1"
        domain = $Summary.domain
        machine = $Summary.machine
        cpu = $Summary.cpu
        image_format = $Summary.image_format
        app_model = $Summary.app_model
        backend_scope = $Summary.backend_scope
        backend_contract = $Summary.backend_contract
        backend_self_check = $Summary.backend_self_check
        backend_reset_self_check = $Summary.backend_reset_self_check
        backend_readiness = $Summary.backend_readiness
        backend_capability_matrix = $Summary.backend_capability_matrix
        runtime_domain_profile = $Summary.runtime_domain_profile
    }
    [System.IO.File]::WriteAllText($ResolvedBackendContract, (($Capture | ConvertTo-Json -Depth 16) + "`n"), [System.Text.UTF8Encoding]::new($false))
    [void](Validate-BackendContractFile -Path $ResolvedBackendContract)
    Write-Host "resident-elf-qemu backend contract exported"
    Write-Host "  source=$ResolvedDomainSummary"
    Write-Host "  path=$ResolvedBackendContract"
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
        $LogPath = Get-QemuEvidencePath -FileName "qemu-ci.log"
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
        $ResolvedFrameDumpOut = Resolve-ScriptPath -Path $FrameDumpOut
        Assert-ValidationOk -Name "frame_dumps" -Code (Validate-FrameDumpFile -Path $ResolvedFrameDumpOut)
        if (-not $SkipGoldenFrameDumps -and -not [string]::IsNullOrWhiteSpace($GoldenFrameDumps)) {
            Assert-ValidationOk -Name "frame_dumps_golden" -Code (Compare-FrameDumpFiles -ExpectedPath (Resolve-ScriptPath -Path $GoldenFrameDumps) -ActualPath $ResolvedFrameDumpOut)
        }
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

    if (-not [string]::IsNullOrWhiteSpace($BackendContractOut)) {
        $ResolvedBackendContractOut = Resolve-ScriptPath -Path $BackendContractOut
        $BackendContractValidationPath = $ResolvedBackendContractOut
        $RemoveDerivedBackendContract = $false
        if (-not (Test-Path -LiteralPath $ResolvedBackendContractOut)) {
            $ResolvedDomainSummaryForExport = Resolve-ScriptPath -Path $DomainSummaryOut
            $BackendContractValidationPath = Join-Path ([System.IO.Path]::GetTempPath()) ("charm_resident_elf_qemu_derived_backend_contract_{0}.json" -f ([System.Guid]::NewGuid().ToString("N")))
            $RemoveDerivedBackendContract = $true
            Export-BackendContractFromDomainSummary `
                -DomainSummaryPath $ResolvedDomainSummaryForExport `
                -BackendContractPath $BackendContractValidationPath
        }
        try {
            Assert-ValidationOk -Name "backend_contract" -Code (Validate-BackendContractFile -Path $BackendContractValidationPath)
        } finally {
            if ($RemoveDerivedBackendContract) {
                Remove-Item -LiteralPath $BackendContractValidationPath -Force -ErrorAction SilentlyContinue
            }
        }
    }

    Write-Host "resident-elf-qemu evidence bundle validation ok"
    return 0
}

function Invoke-Doctor {
    $FirmwareLdscript = Join-Path $PSScriptRoot "ldscript.ld"
    $DomainLayout = Assert-QemuElfDomainLayout -LdscriptPath $FirmwareLdscript -ErrorPrefix "doctor_failed"
    if ($TimeoutSec -le 0) {
        throw "doctor_failed: TimeoutSec must be positive"
    }
    if ($TailLines -le 0) {
        throw "doctor_failed: TailLines must be positive"
    }

    $CMakePath = Resolve-QemuDoctorToolPath -Name "cmake" -Tool $CMakeExe
    $QemuPath = Resolve-QemuDoctorToolPath -Name "qemu-system-arm" -Tool $QemuExe
    $CcPath = Resolve-QemuDoctorToolPath -Name "arm-none-eabi-gcc" -Tool "${ToolchainPrefix}gcc"
    $HostCompilerPath = Resolve-QemuDoctorToolPath -Name "host compiler" -Tool $HostCompiler
    $CMakeVersion = Get-QemuDoctorToolVersionLine -Name "cmake" -FilePath $CMakePath -Arguments @("--version")
    $QemuVersion = Get-QemuDoctorToolVersionLine -Name "qemu-system-arm" -FilePath $QemuPath -Arguments @("--version")
    $CcVersion = Get-QemuDoctorToolVersionLine -Name "arm-none-eabi-gcc" -FilePath $CcPath -Arguments @("--version")
    $HostCompilerVersion = Get-QemuDoctorToolVersionLine -Name "host compiler" -FilePath $HostCompilerPath -Arguments @("--version")

    $MachineHelp = & $QemuPath -machine help 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) {
        throw "doctor_failed: qemu machine help failed exit=$LASTEXITCODE"
    }
    Assert-QemuDoctorMachineSupport -MachineHelp $MachineHelp -Machine "mps2-an500"

    $AppSampleDir = Assert-QemuDoctorRequiredPath `
        -Label "app sample dir" `
        -Path (Join-Path $PSScriptRoot "..\..\app_abi\elf_samples")
    $ToolchainPath = Assert-QemuDoctorRequiredPath `
        -Label "qemu toolchain file" `
        -Path (Join-Path $PSScriptRoot "..\..\kernel\posix\qemu\arm-none-eabi-m7.cmake")

    $RequiredPaths = @(Get-QemuRequiredSourcePaths -AppSampleDir $AppSampleDir)
    foreach ($Required in $RequiredPaths) {
        [void](Assert-QemuDoctorRequiredPath -Label ([System.IO.Path]::GetFileName($Required)) -Path $Required)
    }

    $QemuAppSpecs = @(Get-QemuAppSpecs)
    $QemuAppNames = @($QemuAppSpecs | ForEach-Object { $_.Name })
    if (@($QemuAppNames | Sort-Object -Unique).Count -ne $QemuAppNames.Count) {
        throw "doctor_failed: duplicate QEMU App specs"
    }
    foreach ($Spec in $QemuAppSpecs) {
        $ExpectedSource = Resolve-QemuAppSource -Spec $Spec -AppSampleDir $AppSampleDir
        [void](Assert-QemuDoctorRequiredPath -Label "app source $($Spec.Name)" -Path $ExpectedSource)
    }
    $StoreAppCount = @($QemuAppSpecs | Where-Object { [bool]$_.Store }).Count
    $RequiredIncCount = @(Get-QemuRequiredIncFiles).Count
    if ($RequiredIncCount -ne ($QemuAppSpecs.Count + 1)) {
        throw "doctor_failed: unexpected generated include count: $RequiredIncCount"
    }

    Write-Host "resident-elf-qemu doctor:"
    Write-Host "  status=ok"
    Write-Host "  domain=virtual_m7 machine=mps2-an500 cpu=cortex-m7"
    Write-Host "  qemu=$QemuPath"
    Write-Host "  qemu_version=$QemuVersion"
    Write-Host "  cmake=$CMakePath"
    Write-Host "  cmake_version=$CMakeVersion"
    Write-Host "  cc=$CcPath"
    Write-Host "  cc_version=$CcVersion"
    Write-Host "  host_compiler=$HostCompilerPath"
    Write-Host "  host_compiler_version=$HostCompilerVersion"
    Write-Host "  toolchain=$ToolchainPath"
    Write-Host "  app_sample_dir=$AppSampleDir"
    Write-Host "  build=$(Resolve-ScriptPath -Path $BuildDir)"
    Write-Host "  app_out=$(Resolve-ScriptPath -Path $AppOutDir)"
    Write-Host "  evidence_dir=$(Get-QemuEvidenceRoot)"
    Write-Host "  qemu_log=$(Get-QemuEvidencePath -FileName 'qemu-ci.log')"
    Write-Host "  qemu_err_log=$(Get-QemuEvidencePath -FileName 'qemu-ci.err.log')"
    Write-Host "  evidence_lock=$(Get-QemuEvidencePath -FileName 'qemu-evidence.lock')"
    Write-Host "  elf_base=$($DomainLayout.elf_base)"
    Write-Host "  firmware_elf_load_base=$($DomainLayout.firmware_elf_load_base)"
    Write-Host "  timeout_sec=$TimeoutSec"
    Write-Host "  tail_lines=$TailLines"
    Write-Host "  backend_scope_proves=elf_loader,app_runtime,charm_app_api,capability_backend,received_image,packetstream,store_v1_semantics"
    Write-Host "  backend_scope_does_not_prove=h747_usb_cdc,h747_qspi,h747_emmc,h747_fmc_sdram,h747_hal_init,h747_mpu_cache,h747_pinmux"
    Write-Host "  qemu_machine=mps2-an500 supported=1"
    Write-Host "  app_specs=$($QemuAppSpecs.Count) store_apps=$StoreAppCount generated_includes=$RequiredIncCount"
    Write-Host "  source_paths=$($RequiredPaths.Count)"
    Write-QemuDoctorOptionalPath -Label "frame_signatures" -Path $FrameSignatureOut
    Write-QemuDoctorOptionalPath -Label "golden_frame_signatures" -Path $GoldenFrameSignatures -Required (-not $SkipGoldenFrameSignatures.IsPresent)
    Write-QemuDoctorOptionalPath -Label "frame_dumps" -Path $FrameDumpOut
    Write-QemuDoctorOptionalPath -Label "golden_frame_dumps" -Path $GoldenFrameDumps -Required (-not $SkipGoldenFrameDumps.IsPresent)
    Write-QemuDoctorOptionalPath -Label "frame_ppm" -Path $FramePpmOut
    Write-QemuDoctorOptionalPath -Label "input_trace" -Path $InputTraceOut
    Write-QemuDoctorOptionalPath -Label "golden_input_trace" -Path $GoldenInputTrace -Required (-not $SkipGoldenInputTrace.IsPresent)
    Write-QemuDoctorOptionalPath -Label "storage_trace" -Path $StorageTraceOut
    Write-QemuDoctorOptionalPath -Label "golden_storage_trace" -Path $GoldenStorageTrace -Required (-not $SkipGoldenStorageTrace.IsPresent)
    Write-QemuDoctorOptionalPath -Label "domain_summary" -Path $DomainSummaryOut
    Write-QemuDoctorOptionalPath -Label "backend_contract" -Path $BackendContractOut
    Write-QemuDoctorOptionalPath -Label "golden_domain_summary" -Path $GoldenDomainSummary -Required (-not $SkipGoldenDomainSummary.IsPresent)
    Write-Host "[resident-elf-qemu] doctor ok"
}

function Invoke-SelfTest {
    $FirmwareLdscript = Join-Path $PSScriptRoot "ldscript.ld"
    $DomainLayout = Assert-QemuElfDomainLayout -LdscriptPath $FirmwareLdscript -ErrorPrefix "selftest_failed"
    if ($TimeoutSec -le 0) {
        throw "selftest_failed: TimeoutSec must be positive"
    }
    if ($TailLines -le 0) {
        throw "selftest_failed: TailLines must be positive"
    }
    if (-not (Test-SelfTestThrowsLike -Prefix "missing_tool:" -Script { Resolve-ToolPath "__charm_missing_qemu_tool__" })) {
        throw "selftest_failed: missing tool did not report missing_tool"
    }
    if ((Get-QemuDoctorVersionLineFromText -Text "`nqemu-system-arm version test`nsecond line" -Label "qemu") -ne "qemu-system-arm version test") {
        throw "selftest_failed: doctor version line parser returned unexpected value"
    }
    $TempLdscript = Join-Path ([System.IO.Path]::GetTempPath()) ("charm_qemu_ldscript_selftest_{0}.ld" -f ([System.Guid]::NewGuid().ToString("N")))
    try {
        [System.IO.File]::WriteAllText($TempLdscript, "SECTIONS {`n  .elf_load 0x20081000 (NOLOAD) : { *(.elf_load*) } > RAM`n}`n", [System.Text.UTF8Encoding]::new($false))
        if ((Get-QemuFirmwareElfLoadBase -Path $TempLdscript -ErrorPrefix "selftest_failed") -ne "0x20081000") {
            throw "selftest_failed: QEMU firmware .elf_load parser returned unexpected base"
        }
        [System.IO.File]::WriteAllText($TempLdscript, "SECTIONS { .text : { *(.text*) } > FLASH }`n", [System.Text.UTF8Encoding]::new($false))
        if (-not (Test-SelfTestThrowsLike -Prefix "selftest_failed:" -Script { Get-QemuFirmwareElfLoadBase -Path $TempLdscript -ErrorPrefix "selftest_failed" })) {
            throw "selftest_failed: missing .elf_load did not fail"
        }
    } finally {
        Remove-Item -LiteralPath $TempLdscript -Force -ErrorAction SilentlyContinue
    }
    if (-not (Test-SelfTestThrowsLike -Prefix "doctor_failed:" -Script { Get-QemuDoctorVersionLineFromText -Text "" -Label "empty" })) {
        throw "selftest_failed: empty doctor version output did not report doctor_failed"
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
        entries = 31
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
        entries = 31
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
        (New-SelfTestSourceMatrixEntry -Name "display_describe_error_app" -Direct (New-SelfTestSourceRun -Stage "exit" -Code "ok") -Store (New-SelfTestSourceRun -Stage "exit" -Code "ok")),
        (New-SelfTestSourceMatrixEntry -Name "exit_error_app" -Direct (New-SelfTestSourceRun -Stage "exit" -Code "ok" -Exit 42) -Store (New-SelfTestSourceRun -Stage "exit" -Code "ok" -Exit 42)),
        (New-SelfTestSourceMatrixEntry -Name "exit_negative_app" -Direct (New-SelfTestSourceRun -Stage "exit" -Code "ok") -Store (New-SelfTestSourceRun -Stage "exit" -Code "ok")),
        (New-SelfTestSourceMatrixEntry -Name "return_negative_app" -Direct (New-SelfTestSourceRun -Stage "exit" -Code "ok" -Exit -5) -Store (New-SelfTestSourceRun -Stage "exit" -Code "ok" -Exit -5)),
        (New-SelfTestSourceMatrixEntry -Name "too_large_app" -Direct (New-SelfTestSourceRun -Stage "load" -Code "load_failed"))
    )
    Assert-SourceMatrixEntry -Matrix $GoodSourceMatrix -Name "hello_app" -Sources @("direct", "received", "packetstream", "store")
    Assert-SourceMatrixEntry -Matrix $GoodSourceMatrix -Name "argv_app" -Sources @("direct", "store", "prepare")
    Assert-SourceMatrixEntry -Matrix $GoodSourceMatrix -Name "data_app" -Sources @("direct", "store")
    Assert-SourceMatrixEntry -Matrix $GoodSourceMatrix -Name "display_describe_error_app" -Sources @("direct", "store")
    Assert-SourceMatrixExit -Matrix $GoodSourceMatrix -Name "exit_error_app" -Sources @("direct", "store") -Exit 42
    Assert-SourceMatrixEntry -Matrix $GoodSourceMatrix -Name "exit_negative_app" -Sources @("direct", "store")
    Assert-SourceMatrixExit -Matrix $GoodSourceMatrix -Name "return_negative_app" -Sources @("direct", "store") -Exit -5
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
    $SyntheticBackendIdentity = Get-BackendIdentityFromText -Text (Get-SyntheticPassingLog)
    if ($SyntheticBackendIdentity.runtime_domain -ne "virtual_m7" -or
        $SyntheticBackendIdentity.machine -ne "mps2-an500" -or
        $SyntheticBackendIdentity.cpu -ne "cortex-m7" -or
        $SyntheticBackendIdentity.capabilities -ne "console,time,display,input,storage,app_exit" -or
        $SyntheticBackendIdentity.storage -ne "readonly" -or
        $SyntheticBackendIdentity.afe -ne "unsupported") {
        throw "selftest_failed: synthetic backend identity parse returned unexpected values"
    }
    $MissingBackendIdentityLog = ((Get-SyntheticPassingLog) -split "`r?`n" |
        Where-Object { $_ -ne "resident-elf-qemu: backend=virtual_m7 machine=mps2-an500 cpu=cortex-m7" }) -join "`n"
    if (-not (Test-SelfTestThrowsLike -Prefix "domain_summary_failed:" -Script { Get-BackendIdentityFromText -Text $MissingBackendIdentityLog })) {
        throw "selftest_failed: missing backend identity parsed unexpectedly"
    }
    $SyntheticBackendScope = Get-BackendScopeFromText -Text (Get-SyntheticPassingLog)
    if (-not (@($SyntheticBackendScope.proves) -contains "elf_loader") -or
        -not (@($SyntheticBackendScope.proves) -contains "store_v1_semantics") -or
        -not (@($SyntheticBackendScope.does_not_prove) -contains "h747_qspi") -or
        -not (@($SyntheticBackendScope.does_not_prove) -contains "h747_pinmux")) {
        throw "selftest_failed: synthetic backend scope parse returned unexpected values"
    }
    $MissingBackendScopeLog = ((Get-SyntheticPassingLog) -split "`r?`n" |
        Where-Object { -not $_.StartsWith("resident-elf-qemu: backend-scope ", [System.StringComparison]::Ordinal) }) -join "`n"
    if (-not (Test-SelfTestThrowsLike -Prefix "domain_summary_failed:" -Script { Get-BackendScopeFromText -Text $MissingBackendScopeLog })) {
        throw "selftest_failed: missing backend scope parsed unexpectedly"
    }
    $SyntheticBackendContract = Get-BackendContractFromText -Text (Get-SyntheticPassingLog) `
        -RuntimeDomain "virtual_m7" `
        -Capabilities "console,time,display,input,storage,app_exit" `
        -StorageMode "readonly" `
        -AfeMode "unsupported"
    if ($SyntheticBackendContract.time.kind -ne "deterministic_tick" -or
        [int]$SyntheticBackendContract.time.step_ms -ne 17 -or
        [int]$SyntheticBackendContract.display.frame_bytes -ne 1024 -or
        [int]$SyntheticBackendContract.input.pointer_max_x -ne 15 -or
        [int]$SyntheticBackendContract.storage_media.fd_slots -ne 4 -or
        [bool]$SyntheticBackendContract.app_exit.overrides_return) {
        throw "selftest_failed: synthetic backend contract parse returned unexpected values"
    }
    $SyntheticBackendSelfCheck = Get-BackendSelfCheckFromText -Text (Get-SyntheticPassingLog)
    if (-not [bool]$SyntheticBackendSelfCheck.api -or
        -not [bool]$SyntheticBackendSelfCheck.display -or
        -not [bool]$SyntheticBackendSelfCheck.input -or
        -not [bool]$SyntheticBackendSelfCheck.storage -or
        -not [bool]$SyntheticBackendSelfCheck.afe -or
        -not [bool]$SyntheticBackendSelfCheck.app_exit -or
        $SyntheticBackendSelfCheck.result -ne "ok") {
        throw "selftest_failed: synthetic backend self-check parse returned unexpected values"
    }
    $BadBackendSelfCheckLog = (Get-SyntheticPassingLog).Replace(
        "resident-elf-qemu: backend-self-check api=1 display=1 input=1 storage=1 afe=1 app_exit=1 result=ok",
        "resident-elf-qemu: backend-self-check api=0 display=1 input=1 storage=1 afe=1 app_exit=1 result=failed")
    if (-not (Test-SelfTestThrowsLike -Prefix "domain_summary_validate_failed:" -Script { Assert-BackendSelfCheck -SelfCheck (Get-BackendSelfCheckFromText -Text $BadBackendSelfCheckLog) -ErrorPrefix "domain_summary_validate_failed" })) {
        throw "selftest_failed: bad backend self-check validated unexpectedly"
    }
    $SyntheticBackendResetSelfCheck = Get-BackendResetSelfCheckFromText -Text (Get-SyntheticPassingLog)
    if (-not [bool]$SyntheticBackendResetSelfCheck.counters -or
        -not [bool]$SyntheticBackendResetSelfCheck.display -or
        -not [bool]$SyntheticBackendResetSelfCheck.time -or
        -not [bool]$SyntheticBackendResetSelfCheck.input -or
        -not [bool]$SyntheticBackendResetSelfCheck.storage -or
        $SyntheticBackendResetSelfCheck.result -ne "ok") {
        throw "selftest_failed: synthetic backend reset self-check parse returned unexpected values"
    }
    $BadBackendResetSelfCheckLog = (Get-SyntheticPassingLog).Replace(
        "resident-elf-qemu: backend-reset-self-check counters=1 display=1 time=1 input=1 storage=1 result=ok",
        "resident-elf-qemu: backend-reset-self-check counters=1 display=1 time=0 input=1 storage=1 result=failed")
    if (-not (Test-SelfTestThrowsLike -Prefix "domain_summary_validate_failed:" -Script { Assert-BackendResetSelfCheck -SelfCheck (Get-BackendResetSelfCheckFromText -Text $BadBackendResetSelfCheckLog) -ErrorPrefix "domain_summary_validate_failed" })) {
        throw "selftest_failed: bad backend reset self-check validated unexpectedly"
    }
    $BadBackendFlagLog = (Get-SyntheticPassingLog).Replace(
        "resident-elf-qemu: backend-contract app_exit=notification_counter overrides_return=0",
        "resident-elf-qemu: backend-contract app_exit=notification_counter overrides_return=2")
    if (-not (Test-SelfTestThrowsLike -Prefix "domain_summary_failed:" -Script { Get-BackendContractFromText -Text $BadBackendFlagLog -RuntimeDomain "virtual_m7" -Capabilities "console,time,display,input,storage,app_exit" -StorageMode "readonly" -AfeMode "unsupported" })) {
        throw "selftest_failed: bad backend contract flag parsed unexpectedly"
    }
    $SyntheticBackendContractCapture = New-SelfTestBackendContractCapture
    Assert-BackendContractCaptureAccepted -Capture $SyntheticBackendContractCapture -Label "base"
    Assert-BadBackendContractRejected -Capture $SyntheticBackendContractCapture -Label "schema" -Mutate {
        param($Capture)
        $Capture.schema = "bad.schema"
    }
    Assert-BadBackendContractRejected -Capture $SyntheticBackendContractCapture -Label "capabilities" -Mutate {
        param($Capture)
        $Capture.backend_contract.capabilities = @("console", "time", "display", "input", "storage")
    }
    Assert-BadBackendContractRejected -Capture $SyntheticBackendContractCapture -Label "backend_scope" -Mutate {
        param($Capture)
        $Capture.backend_scope.does_not_prove = @($Capture.backend_scope.does_not_prove | Where-Object { $_ -ne "h747_qspi" })
    }
    Assert-BadBackendContractRejected -Capture $SyntheticBackendContractCapture -Label "readiness" -Mutate {
        param($Capture)
        $Capture.backend_readiness.storage = $false
    }
    Assert-BadBackendContractRejected -Capture $SyntheticBackendContractCapture -Label "backend_self_check" -Mutate {
        param($Capture)
        $Capture.backend_self_check.api = $false
    }
    Assert-BadBackendContractRejected -Capture $SyntheticBackendContractCapture -Label "backend_reset_self_check" -Mutate {
        param($Capture)
        $Capture.backend_reset_self_check.time = $false
    }
    Assert-BadBackendContractRejected -Capture $SyntheticBackendContractCapture -Label "capability_matrix" -Mutate {
        param($Capture)
        $Capture.backend_capability_matrix[0].provider = "bad_provider"
    }
    Assert-BadBackendContractRejected -Capture $SyntheticBackendContractCapture -Label "runtime_domain_profile" -Mutate {
        param($Capture)
        $Capture.runtime_domain_profile.does_not_prove = @("h747_usb_cdc")
    }
    Assert-BadBackendContractRejected -Capture $SyntheticBackendContractCapture -Label "runtime_domain_profile_proves" -Mutate {
        param($Capture)
        $Capture.runtime_domain_profile.proves = @($Capture.runtime_domain_profile.proves | Where-Object { $_ -ne "packetstream" })
    }
    Assert-BadBackendContractRejected -Capture $SyntheticBackendContractCapture -Label "runtime_domain_profile_extra_exclusion" -Mutate {
        param($Capture)
        $Capture.runtime_domain_profile.does_not_prove = @($Capture.runtime_domain_profile.does_not_prove + "h747_display")
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
    $SyntheticStorageErrorLog = @(
        "resident-elf-qemu: storage open path=/virtual/readme.txt code=ok fd=3 size=27",
        "resident-elf-qemu: storage read fd=3 code=invalid_argument requested=4 count=0 offset=0 remaining=27",
        "resident-elf-qemu: storage read fd=3 code=ok requested=4 count=4 offset=0 remaining=23",
        "resident-elf-qemu: storage close fd=3 code=ok",
        "resident-elf-qemu: app storage_error_app stage=exit code=ok exit=0"
    ) -join "`n"
    $SyntheticStorageErrorTrace = Get-StorageTraceFromText -Text $SyntheticStorageErrorLog
    $SyntheticStorageErrorRuns = @($SyntheticStorageErrorTrace.runs)
    if (@($SyntheticStorageErrorTrace.events).Count -ne 4 -or $SyntheticStorageErrorRuns.Count -ne 1) {
        throw "selftest_failed: synthetic storage error trace grouping failed"
    }
    Assert-StorageTraceRun -Runs $SyntheticStorageErrorRuns `
        -Name "storage_error_app" `
        -Ops @("open", "read", "read", "close") `
        -Paths @("/virtual/readme.txt", "", "", "") `
        -Fds @(3, 3, 3, 3) `
        -Counts @(0, 0, 4, 0) `
        -Codes @("ok", "invalid_argument", "ok", "ok") `
        -Offsets @(0, 0, 0, 0) `
        -Remainings @(0, 27, 23, 0)
    $SyntheticStorageCloseErrorLog = @(
        "resident-elf-qemu: storage open path=/virtual/readme.txt code=ok fd=3 size=27",
        "resident-elf-qemu: storage close fd=3 code=ok",
        "resident-elf-qemu: storage close fd=3 code=unsupported",
        "resident-elf-qemu: storage open path=/virtual/readme.txt code=ok fd=3 size=27",
        "resident-elf-qemu: storage read fd=3 code=ok requested=1 count=1 offset=0 remaining=26",
        "resident-elf-qemu: storage close fd=3 code=ok",
        "resident-elf-qemu: app storage_close_error_app stage=exit code=ok exit=0"
    ) -join "`n"
    $SyntheticStorageCloseErrorTrace = Get-StorageTraceFromText -Text $SyntheticStorageCloseErrorLog
    $SyntheticStorageCloseErrorRuns = @($SyntheticStorageCloseErrorTrace.runs)
    if (@($SyntheticStorageCloseErrorTrace.events).Count -ne 6 -or $SyntheticStorageCloseErrorRuns.Count -ne 1) {
        throw "selftest_failed: synthetic storage close error trace grouping failed"
    }
    Assert-StorageTraceRun -Runs $SyntheticStorageCloseErrorRuns `
        -Name "storage_close_error_app" `
        -Ops @("open", "close", "close", "open", "read", "close") `
        -Paths @("/virtual/readme.txt", "", "", "/virtual/readme.txt", "", "") `
        -Fds @(3, 3, 3, 3, 3, 3) `
        -Counts @(0, 0, 0, 0, 1, 0) `
        -Codes @("ok", "ok", "unsupported", "ok", "ok", "ok") `
        -Offsets @(0, 0, 0, 0, 0, 0) `
        -Remainings @(0, 0, 0, 0, 26, 0)
    $SyntheticStorageFdExhaustionLog = @(
        "resident-elf-qemu: storage open path=/virtual/readme.txt code=ok fd=3 size=27",
        "resident-elf-qemu: storage open path=/virtual/readme.txt code=ok fd=4 size=27",
        "resident-elf-qemu: storage open path=/virtual/alpha.txt code=ok fd=5 size=15",
        "resident-elf-qemu: storage open path=/virtual/beta.bin code=ok fd=6 size=16",
        "resident-elf-qemu: storage open path=/virtual/readme.txt code=io_error fd=-1",
        "resident-elf-qemu: storage close fd=4 code=ok",
        "resident-elf-qemu: storage open path=/virtual/readme.txt code=ok fd=4 size=27",
        "resident-elf-qemu: storage read fd=4 code=ok requested=1 count=1 offset=0 remaining=26",
        "resident-elf-qemu: storage close fd=4 code=ok",
        "resident-elf-qemu: storage close fd=6 code=ok",
        "resident-elf-qemu: storage close fd=5 code=ok",
        "resident-elf-qemu: storage close fd=3 code=ok",
        "resident-elf-qemu: app storage_fd_exhaustion_app stage=exit code=ok exit=0"
    ) -join "`n"
    $SyntheticStorageFdExhaustionTrace = Get-StorageTraceFromText -Text $SyntheticStorageFdExhaustionLog
    $SyntheticStorageFdExhaustionRuns = @($SyntheticStorageFdExhaustionTrace.runs)
    if (@($SyntheticStorageFdExhaustionTrace.events).Count -ne 12 -or $SyntheticStorageFdExhaustionRuns.Count -ne 1) {
        throw "selftest_failed: synthetic storage fd exhaustion trace grouping failed"
    }
    Assert-StorageTraceRun -Runs $SyntheticStorageFdExhaustionRuns `
        -Name "storage_fd_exhaustion_app" `
        -Ops @("open", "open", "open", "open", "open", "close", "open", "read", "close", "close", "close", "close") `
        -Paths @("/virtual/readme.txt", "/virtual/readme.txt", "/virtual/alpha.txt", "/virtual/beta.bin", "/virtual/readme.txt", "", "/virtual/readme.txt", "", "", "", "", "") `
        -Fds @(3, 4, 5, 6, -1, 4, 4, 4, 4, 6, 5, 3) `
        -Counts @(0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0) `
        -Codes @("ok", "ok", "ok", "ok", "io_error", "ok", "ok", "ok", "ok", "ok", "ok", "ok") `
        -Offsets @(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) `
        -Remainings @(0, 0, 0, 0, 0, 0, 0, 26, 0, 0, 0, 0)
    $SyntheticStorageWriteErrorLog = @(
        "resident-elf-qemu: storage open path=/virtual/readme.txt code=ok fd=3 size=27",
        "resident-elf-qemu: storage read fd=3 code=ok requested=1 count=1 offset=0 remaining=26",
        "resident-elf-qemu: storage write fd=3 code=unsupported requested=1 count=0 offset=1 remaining=26",
        "resident-elf-qemu: storage read fd=3 code=ok requested=1 count=1 offset=1 remaining=25",
        "resident-elf-qemu: storage close fd=3 code=ok",
        "resident-elf-qemu: app storage_write_error_app stage=exit code=ok exit=0"
    ) -join "`n"
    $SyntheticStorageWriteErrorTrace = Get-StorageTraceFromText -Text $SyntheticStorageWriteErrorLog
    $SyntheticStorageWriteErrorRuns = @($SyntheticStorageWriteErrorTrace.runs)
    if (@($SyntheticStorageWriteErrorTrace.events).Count -ne 5 -or $SyntheticStorageWriteErrorRuns.Count -ne 1) {
        throw "selftest_failed: synthetic storage write error trace grouping failed"
    }
    Assert-StorageTraceRun -Runs $SyntheticStorageWriteErrorRuns `
        -Name "storage_write_error_app" `
        -Ops @("open", "read", "write", "read", "close") `
        -Paths @("/virtual/readme.txt", "", "", "", "") `
        -Fds @(3, 3, 3, 3, 3) `
        -Counts @(0, 1, 0, 1, 0) `
        -Codes @("ok", "ok", "unsupported", "ok", "ok") `
        -Offsets @(0, 0, 1, 1, 0) `
        -Remainings @(0, 26, 26, 25, 0)
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
    foreach ($RequiredCMakeToken in @("CHARM_QEMU_REQUIRED_INC_FILES", "missing generated QEMU resident ELF artifacts")) {
        if (-not $CMakeListsText.Contains($RequiredCMakeToken)) {
            throw "selftest_failed: CMakeLists.txt does not document $RequiredCMakeToken"
        }
    }
    $RequiredIncFiles = Get-QemuRequiredIncFiles
    if ($RequiredIncFiles.Count -ne ((Get-QemuAppNames).Count + 1)) {
        throw "selftest_failed: QEMU required inc list count is unexpected"
    }
    $QemuAppSpecs = @(Get-QemuAppSpecs)
    $QemuAppNames = @($QemuAppSpecs | ForEach-Object { $_.Name })
    if (@($QemuAppNames | Sort-Object -Unique).Count -ne $QemuAppNames.Count) {
        throw "selftest_failed: QEMU App specs contain duplicate names"
    }
    $NonStoreAppNames = @($QemuAppSpecs | Where-Object { -not [bool]$_.Store } | ForEach-Object { $_.Name })
    if ($NonStoreAppNames.Count -ne 1 -or $NonStoreAppNames[0] -ne "too_large_app") {
        throw "selftest_failed: QEMU non-Store App specs are unexpected"
    }
    foreach ($Spec in (Get-QemuAppSpecs)) {
        if ($Spec.SourceRoot -ne "qemu" -and $Spec.SourceRoot -ne "samples") {
            throw "selftest_failed: QEMU App source root is unexpected: $($Spec.SourceRoot)"
        }
        $ExpectedSource = Resolve-QemuAppSource -Spec $Spec -AppSampleDir $AppSampleDir
        if (-not (Test-Path -LiteralPath $ExpectedSource)) {
            throw "selftest_failed: QEMU App source is missing: $ExpectedSource"
        }
    }
    $StorePackArgs = Get-QemuStorePackArguments `
        -StorePath "appstore.bin" `
        -AppOutDir "out" `
        -TooLargeStoreElf "too_large_store_app.elf"
    if ($StorePackArgs.Count -ne ((Get-QemuAppSpecs | Where-Object { [bool]$_.Store }).Count + 2) -or
        -not ($StorePackArgs -contains "too_large_store_app=too_large_store_app.elf")) {
        throw "selftest_failed: QEMU Store pack argument list is unexpected"
    }

    $ReadmePath = Join-Path $PSScriptRoot "README.md"
    $ReadmeText = Get-Content -LiteralPath $ReadmePath -Raw -Encoding UTF8
    foreach ($RequiredReadmeToken in @(
        "..\run-resident-elf-qemu-smoke.ps1 -DryRun",
        "..\run-resident-elf-qemu-smoke.ps1 -SelfTest",
        "-SkipGoldenFrameSignatures",
        "-SkipGoldenInputTrace",
        "-SkipGoldenStorageTrace",
        "-SkipGoldenDomainSummary",
        "..\run-resident-elf-qemu-smoke.ps1 -Doctor",
        "-ValidateBackendContract",
        "backend-contract.json",
        "qemu_version",
        "capture-resident-platform-evidence-bundle.ps1 -QemuElf",
        "-QemuElfTimeoutSec <seconds>",
        "-QemuElfTailLines <lines>",
        "-EvidenceDir <dir>",
        "-QemuElfEvidenceDir <dir>",
        "timeout_sec",
        "tail_lines",
        "evidence_dir",
        "qemu_log",
        "qemu_err_log",
        "evidence_lock",
        "firmware_elf_load_base",
        "backend_scope_proves",
        "backend_scope_does_not_prove",
        "qemu_elf_evidence_dir",
        "wrapper owns the supported command-line surface",
        'direct script default at `-TimeoutSec 15`',
        "display mode is fixed at 16x16 ARGB8888",
        "coverage.gui_timeline",
        "runtime_reset_determinism",
        "qemu_elf_runtime_reset",
        "backend_scope",
        "qemu_elf_backend_scope",
        "qemu_elf_doctor_scope",
        "qemu_elf_scope_match",
        "doctor/runtime scope matching",
        "qemu_elf_backend_contract_file",
        'derived `backend-contract.json`',
        "qemu-evidence.lock",
        "same evidence files",
        '`qemu-ci.log`',
        "independent evidence directories",
        "frame-dumps.golden.json",
        "elf_run_evidence_matrix",
        "storage_fd_exhaustion_app",
        "storage_write_error_app",
        "storage_zero_io_app",
        "exit_negative_app",
        "return_negative_app",
        "store_manifest",
        "Get-QemuAppSpecs",
        "CHARM_QEMU_REQUIRED_INC_FILES",
        "virtual_m7"
    )) {
        if (-not $ReadmeText.Contains($RequiredReadmeToken)) {
            throw "selftest_failed: README.md does not document $RequiredReadmeToken"
        }
    }

    foreach ($Required in (Get-QemuRequiredSourcePaths -AppSampleDir $AppSampleDir)) {
        if (-not (Test-Path -LiteralPath $Required)) {
            throw "selftest_failed: required path is missing: $Required"
        }
    }

    $GoldenFrameSignatureFile = Join-Path $PSScriptRoot "frame-signatures.golden.json"
    $GoldenFrameDumpFile = Join-Path $PSScriptRoot "frame-dumps.golden.json"
    $GoldenInputTraceFile = Join-Path $PSScriptRoot "input-trace.golden.json"
    $GoldenStorageTraceFile = Join-Path $PSScriptRoot "storage-trace.golden.json"
    $GoldenDomainSummaryFile = Join-Path $PSScriptRoot "domain-summary.golden.json"
    [void](Validate-FrameSignatureFile -Path $GoldenFrameSignatureFile)
    [void](Validate-FrameDumpFile -Path $GoldenFrameDumpFile)
    [void](Validate-InputTraceFile -Path $GoldenInputTraceFile)
    [void](Validate-StorageTraceFile -Path $GoldenStorageTraceFile)
    [void](Validate-DomainSummaryFile -Path $GoldenDomainSummaryFile)
    $DerivedBackendContractPath = Join-Path ([System.IO.Path]::GetTempPath()) "charm_resident_elf_qemu_derived_backend_contract.json"
    try {
        Export-BackendContractFromDomainSummary `
            -DomainSummaryPath $GoldenDomainSummaryFile `
            -BackendContractPath $DerivedBackendContractPath
    } finally {
        Remove-Item -LiteralPath $DerivedBackendContractPath -Force -ErrorAction SilentlyContinue
    }
    Assert-BadDomainSummaryRejected -SourcePath $GoldenDomainSummaryFile -Label "app_model" -Mutate {
        param($Summary)
        $Summary.app_model = "RawJump"
    }
    Assert-BadDomainSummaryRejected -SourcePath $GoldenDomainSummaryFile -Label "backend_contract_storage_slots" -Mutate {
        param($Summary)
        $Summary.backend_contract.storage_media.fd_slots = 3
    }
    Assert-BadDomainSummaryRejected -SourcePath $GoldenDomainSummaryFile -Label "backend_scope_h747_boundary" -Mutate {
        param($Summary)
        $Summary.backend_scope.does_not_prove = @($Summary.backend_scope.does_not_prove | Where-Object { $_ -ne "h747_qspi" })
    }
    Assert-BadDomainSummaryRejected -SourcePath $GoldenDomainSummaryFile -Label "backend_self_check_api" -Mutate {
        param($Summary)
        $Summary.backend_self_check.api = $false
    }
    Assert-BadDomainSummaryRejected -SourcePath $GoldenDomainSummaryFile -Label "backend_reset_self_check_time" -Mutate {
        param($Summary)
        $Summary.backend_reset_self_check.time = $false
    }
    Assert-BadDomainSummaryRejected -SourcePath $GoldenDomainSummaryFile -Label "run_budget" -Mutate {
        param($Summary)
        $Summary.run_budget.timeout_sec = 0
    }
    Assert-BadDomainSummaryRejected -SourcePath $GoldenDomainSummaryFile -Label "packetstream_crc" -Mutate {
        param($Summary)
        ($Summary.coverage.packetstreams | Where-Object { $_.name -eq "packetstream_crc_mismatch" }).read_code = "ok"
    }
    Assert-BadDomainSummaryRejected -SourcePath $GoldenDomainSummaryFile -Label "equivalent_entry_load_span" -Mutate {
        param($Summary)
        ($Summary.coverage.loads | Where-Object { $_.name -eq "store:hello_app" }).span = 1
    }
    Assert-BadDomainSummaryRejected -SourcePath $GoldenDomainSummaryFile -Label "backend_readiness_gui" -Mutate {
        param($Summary)
        $Summary.backend_readiness.gui = $false
    }
    Assert-BadDomainSummaryRejected -SourcePath $GoldenDomainSummaryFile -Label "backend_readiness_runtime_reset" -Mutate {
        param($Summary)
        $Summary.backend_readiness.runtime_reset = $false
    }
    Assert-BadDomainSummaryRejected -SourcePath $GoldenDomainSummaryFile -Label "runtime_reset_determinism_time" -Mutate {
        param($Summary)
        $Summary.runtime_reset_determinism.time_reset = $false
    }
    Assert-BadDomainSummaryRejected -SourcePath $GoldenDomainSummaryFile -Label "backend_capability_matrix_storage" -Mutate {
        param($Summary)
        ($Summary.backend_capability_matrix | Where-Object { $_.capability -eq "storage" }).policy = "write=allowed"
    }
    Assert-BadDomainSummaryRejected -SourcePath $GoldenDomainSummaryFile -Label "runtime_domain_profile_h747_boundary" -Mutate {
        param($Summary)
        $Summary.runtime_domain_profile.does_not_prove = @($Summary.runtime_domain_profile.does_not_prove | Where-Object { $_ -ne "h747_qspi" })
    }
    Assert-BadDomainSummaryRejected -SourcePath $GoldenDomainSummaryFile -Label "runtime_domain_profile_scope_mismatch" -Mutate {
        param($Summary)
        $Values = @($Summary.runtime_domain_profile.proves)
        [array]::Reverse($Values)
        $Summary.runtime_domain_profile.proves = $Values
    }
    Assert-BadDomainSummaryRejected -SourcePath $GoldenDomainSummaryFile -Label "elf_run_evidence_matrix_store" -Mutate {
        param($Summary)
        ($Summary.elf_run_evidence_matrix | Where-Object { $_.name -eq "store:hello_app" }).run_code = "load_failed"
    }
    Assert-BadDomainSummaryRejected -SourcePath $GoldenDomainSummaryFile -Label "store_manifest_flags" -Mutate {
        param($Summary)
        ($Summary.artifacts.store_manifest.entries | Where-Object { $_.name -eq "hello_app" }).flags = "0x00000001"
    }
    Assert-BadDomainSummaryRejected -SourcePath $GoldenDomainSummaryFile -Label "store_manifest_crc" -Mutate {
        param($Summary)
        ($Summary.artifacts.store_manifest.entries | Where-Object { $_.name -eq "hello_app" }).payload_crc32 = "0x00000000"
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
    $GoldenFrameDumpPath = if ($SkipGoldenFrameDumps) { "skipped" } else { Resolve-ScriptPath -Path $GoldenFrameDumps }
    $FramePpmPath = Resolve-ScriptPath -Path $FramePpmOut
    $InputTracePath = Resolve-ScriptPath -Path $InputTraceOut
    $GoldenInputTracePath = if ($SkipGoldenInputTrace) { "skipped" } else { Resolve-ScriptPath -Path $GoldenInputTrace }
    $StorageTracePath = Resolve-ScriptPath -Path $StorageTraceOut
    $GoldenStorageTracePath = if ($SkipGoldenStorageTrace) { "skipped" } else { Resolve-ScriptPath -Path $GoldenStorageTrace }
    $DomainSummaryPath = Resolve-ScriptPath -Path $DomainSummaryOut
    $BackendContractPath = Resolve-ScriptPath -Path $BackendContractOut
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
    Write-Host "  golden_frame_dumps=$GoldenFrameDumpPath"
    Write-Host "  frame_ppm=$FramePpmPath"
    Write-Host "  input_trace=$InputTracePath"
    Write-Host "  golden_input_trace=$GoldenInputTracePath"
    Write-Host "  storage_trace=$StorageTracePath"
    Write-Host "  golden_storage_trace=$GoldenStorageTracePath"
    Write-Host "  domain_summary=$DomainSummaryPath"
    Write-Host "  backend_contract=$BackendContractPath"
    Write-Host "  golden_domain_summary=$GoldenDomainSummaryPath"
    Write-Host "  elf_base=$($DomainLayout.elf_base)"
    Write-Host "  firmware_elf_load_base=$($DomainLayout.firmware_elf_load_base)"
    Write-Host "  timeout_sec=$TimeoutSec"
    Write-Host "  tail_lines=$TailLines"
    Write-Host "[resident-elf-qemu] selftest ok"
}

if ($SelfTest) {
    Initialize-QemuEvidencePaths
    Invoke-SelfTest
    exit 0
}

if ($Doctor) {
    Initialize-QemuEvidencePaths
    Invoke-Doctor
    exit 0
}

if ($ValidateEvidenceBundle) {
    Initialize-QemuEvidencePaths
    $EvidenceLock = Acquire-QemuEvidenceLock
    try {
        exit (Invoke-EvidenceBundleValidation)
    } finally {
        Release-QemuEvidenceLock -Lock $EvidenceLock
    }
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

if (-not [string]::IsNullOrWhiteSpace($ValidateBackendContract)) {
    exit (Validate-BackendContractFile -Path $ValidateBackendContract)
}

if (-not [string]::IsNullOrWhiteSpace($CompareDomainSummary) -or
    -not [string]::IsNullOrWhiteSpace($ActualDomainSummary)) {
    exit (Compare-DomainSummaryFiles -ExpectedPath $CompareDomainSummary -ActualPath $ActualDomainSummary)
}

if (-not [string]::IsNullOrWhiteSpace($CompareFrameDumps) -or
    -not [string]::IsNullOrWhiteSpace($ActualFrameDumps)) {
    exit (Compare-FrameDumpFiles -ExpectedPath $CompareFrameDumps -ActualPath $ActualFrameDumps)
}

$EvidenceLock = $null

try {
Initialize-QemuEvidencePaths
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
Write-Host "resident-elf-qemu: evidence=$(Get-QemuEvidenceRoot)"
Write-Host "resident-elf-qemu: elf_base=$ElfBase"

if ($DryRun) {
    Write-Host "[dry-run] qemu=$qemu"
    Write-Host "[dry-run] cc=$cc"
    Write-Host "[dry-run] host_compiler=$hostCompilerResolved"
    Write-Host "[dry-run] toolchain=$($toolchainFile.Path)"
    Write-Host "[dry-run] evidence_dir=$(Get-QemuEvidenceRoot)"
    Write-Host "[dry-run] qemu_log=$(Get-QemuEvidencePath -FileName 'qemu-ci.log')"
    Write-Host "[dry-run] qemu_err_log=$(Get-QemuEvidencePath -FileName 'qemu-ci.err.log')"
    Write-Host "[dry-run] frame_signatures=$(Resolve-ScriptPath -Path $FrameSignatureOut)"
    if ($SkipGoldenFrameSignatures) {
        Write-Host "[dry-run] golden_frame_signatures=skipped"
    } else {
        Write-Host "[dry-run] golden_frame_signatures=$(Resolve-ScriptPath -Path $GoldenFrameSignatures)"
    }
    Write-Host "[dry-run] frame_dumps=$(Resolve-ScriptPath -Path $FrameDumpOut)"
    if ($SkipGoldenFrameDumps) {
        Write-Host "[dry-run] golden_frame_dumps=skipped"
    } else {
        Write-Host "[dry-run] golden_frame_dumps=$(Resolve-ScriptPath -Path $GoldenFrameDumps)"
    }
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
    Write-Host "[dry-run] backend_contract=$(Resolve-ScriptPath -Path $BackendContractOut)"
    if ($SkipGoldenDomainSummary) {
        Write-Host "[dry-run] golden_domain_summary=skipped"
    } else {
        Write-Host "[dry-run] golden_domain_summary=$(Resolve-ScriptPath -Path $GoldenDomainSummary)"
    }
    Write-Host "[dry-run] timeout_sec=$TimeoutSec"
    Write-Host "[dry-run] tail_lines=$TailLines"
    Write-Host "[dry-run] validate_evidence_bundle=$($ValidateEvidenceBundle.IsPresent)"
    exit 0
}

$cachePath = Join-Path $build "CMakeCache.txt"
if (Test-Path $cachePath) {
    $cacheText = Get-Content -Raw -Encoding UTF8 $cachePath
    $cachedToolchainLine = @($cacheText -split "`r?`n" |
        Where-Object { $_.StartsWith("CMAKE_TOOLCHAIN_FILE:FILEPATH=", [System.StringComparison]::Ordinal) } |
        Select-Object -First 1)
    $cachedToolchainPath = if ($cachedToolchainLine.Count -eq 1) {
        $cachedToolchainLine[0].Substring("CMAKE_TOOLCHAIN_FILE:FILEPATH=".Length)
    } else {
        ""
    }
    $toolchainMatches = -not [string]::IsNullOrWhiteSpace($cachedToolchainPath) -and
        [string]::Equals(
            [System.IO.Path]::GetFullPath($cachedToolchainPath),
            [System.IO.Path]::GetFullPath($toolchainFile.Path),
            [System.StringComparison]::OrdinalIgnoreCase)
    if (-not $toolchainMatches) {
        Write-Host "resident-elf-qemu: build cache toolchain changed; resetting build=$build"
        Remove-Item -Recurse -Force $build
    }
}

if (-not (Test-Path $appOut)) {
    New-Item -ItemType Directory -Path $appOut | Out-Null
}

$EvidenceLock = Acquire-QemuEvidenceLock

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

foreach ($Spec in (Get-QemuAppSpecs)) {
    $name = $Spec.Name
    $src = Resolve-QemuAppSource -Spec $Spec -AppSampleDir $appSampleDir
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
Invoke-Checked -FilePath $storePackExe -Arguments (Get-QemuStorePackArguments `
    -StorePath $store `
    -AppOutDir $appOut `
    -TooLargeStoreElf $tooLargeStoreElf)
Write-IncFile -InputPath $store -OutputPath $storeInc -Symbol "qemu_appstore_bin"

Invoke-Checked -FilePath $cmake -Arguments @(
    "-S", $PSScriptRoot,
    "-B", $build,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Debug",
    "-DCMAKE_TOOLCHAIN_FILE=$($toolchainFile.Path)",
    "-DCHARM_QEMU_APP_ELF_INC_DIR=$appOut",
    "-DCHARM_QEMU_REQUIRED_INC_FILES=$((Get-QemuRequiredIncFiles) -join ';')"
)
Invoke-Checked -FilePath $cmake -Arguments @("--build", $build, "--parallel", "1")

$firmware = Join-Path $build "resident-elf-qemu-smoke.elf"
if (-not (Test-Path $firmware)) {
    throw "firmware not found: $firmware"
}

$EvidenceRoot = Get-QemuEvidenceRoot
if (-not (Test-Path -LiteralPath $EvidenceRoot)) {
    New-Item -ItemType Directory -Path $EvidenceRoot | Out-Null
}
$outFile = Join-Path $EvidenceRoot "qemu-ci.log"
$errFile = Join-Path $EvidenceRoot "qemu-ci.err.log"
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
    if (-not $SkipGoldenFrameDumps -and -not [string]::IsNullOrWhiteSpace($GoldenFrameDumps)) {
        $ResolvedGoldenFrameDumps = Resolve-ScriptPath -Path $GoldenFrameDumps
        Assert-ValidationOk -Name "frame_dumps_golden" -Code (Compare-FrameDumpFiles -ExpectedPath $ResolvedGoldenFrameDumps -ActualPath $ResolvedFrameDumpOut)
    }
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
    -OutputPath $DomainSummaryOut `
    -BackendContractOutputPath $BackendContractOut `
    -ArtifactDir $appOut `
    -TimeoutSec $TimeoutSec `
    -TailLines $TailLines
if (-not [string]::IsNullOrWhiteSpace($DomainSummaryOut)) {
    $ResolvedDomainSummaryOut = Resolve-ScriptPath -Path $DomainSummaryOut
    [void](Validate-DomainSummaryFile -Path $ResolvedDomainSummaryOut)
    if (-not $SkipGoldenDomainSummary -and -not [string]::IsNullOrWhiteSpace($GoldenDomainSummary)) {
        $ResolvedGoldenDomainSummary = Resolve-ScriptPath -Path $GoldenDomainSummary
        Assert-ValidationOk -Name "domain_summary_golden" -Code (Compare-DomainSummaryFiles -ExpectedPath $ResolvedGoldenDomainSummary -ActualPath $ResolvedDomainSummaryOut)
    }
}
if (-not [string]::IsNullOrWhiteSpace($BackendContractOut)) {
    $ResolvedBackendContractOut = Resolve-ScriptPath -Path $BackendContractOut
    [void](Validate-BackendContractFile -Path $ResolvedBackendContractOut)
}
Write-Host "[ok] resident ELF QEMU smoke detected"
} finally {
    Release-QemuEvidenceLock -Lock $EvidenceLock
}
