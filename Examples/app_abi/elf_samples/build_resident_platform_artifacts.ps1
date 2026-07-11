param(
    [string]$ToolchainPrefix = "arm-none-eabi-",
    [string]$OutDir = "$PSScriptRoot/out",
    [string]$ElfBase = "0x24070000",
    [string]$HostCompiler = "D:/Toolchains/w64devkit/bin/g++.exe",
    [uint32]$PacketChunkSize = 256,
    [switch]$Validate
)

$ErrorActionPreference = "Stop"

function Get-Crc32 {
    param([byte[]]$Bytes)

    $crc = [uint32]4294967295
    foreach ($byte in $Bytes) {
        $crc = $crc -bxor [uint32]$byte
        for ($i = 0; $i -lt 8; $i++) {
            if (($crc -band 1) -ne 0) {
                $crc = [uint32](($crc -shr 1) -bxor [uint32]3988292384)
            } else {
                $crc = $crc -shr 1
            }
        }
    }
    return [uint32]($crc -bxor [uint32]4294967295)
}

function Get-RelativePath {
    param(
        [string]$BasePath,
        [string]$Path
    )

    $baseUri = [System.Uri]((Resolve-Path $BasePath).Path.TrimEnd('\') + '\')
    $pathUri = [System.Uri]((Resolve-Path $Path).Path)
    return [System.Uri]::UnescapeDataString($baseUri.MakeRelativeUri($pathUri).ToString()).Replace('/', '\')
}

function New-ArtifactEntry {
    param(
        [string]$Name,
        [string]$Format,
        [string]$Path,
        [uint32]$StoreFlags,
        [string]$PacketStreamPath
    )

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    $packetBytes = [System.IO.File]::ReadAllBytes($PacketStreamPath)
    $entry = [ordered]@{
        name = $Name
        format = $Format
        path = Get-RelativePath -BasePath $OutDir -Path $Path
        size = $bytes.Length
        crc32 = ("0x{0:x8}" -f (Get-Crc32 -Bytes $bytes))
        store_flags = $StoreFlags
        packetstream_path = Get-RelativePath -BasePath $OutDir -Path $PacketStreamPath
        packetstream_size = $packetBytes.Length
    }
    if ($Format -eq "elf") {
        $probe = Get-ElfProbeMetadata -Path $Path
        $entry.elf_probe = [ordered]@{
            code = $probe.Code
            entry_offset = $probe.EntryOffset
            load_span = $probe.LoadSpan
            segment_count = $probe.SegmentCount
            runnable = $probe.Runnable
            run_region_size = $probe.RunRegionSize
            run_region_fits = $probe.RunRegionFits
        }
    }
    [pscustomobject]$entry
}

function Invoke-Checked {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $FilePath $($Arguments -join ' ')"
    }
}

function Get-ElfProbeMetadata {
    param([string]$Path)

    $RunRegionSize = [uint32](64 * 1024)
    $Fail = {
        param([string]$Code)
        return [pscustomobject]@{
            Code = $Code
            EntryOffset = [uint32]0
            LoadSpan = [uint32]0
            SegmentCount = [uint32]0
            Runnable = $false
            RunRegionSize = $RunRegionSize
            RunRegionFits = $false
        }
    }

    function ConvertFrom-HexU32 {
        param([string]$Text)

        $Trimmed = $Text.Trim()
        if ($Trimmed.StartsWith("0x", [System.StringComparison]::OrdinalIgnoreCase)) {
            $Trimmed = $Trimmed.Substring(2)
        }
        return [uint32]([Convert]::ToUInt64($Trimmed, 16))
    }

    $readelf = "${ToolchainPrefix}readelf"
    $output = & $readelf -l $Path
    if ($LASTEXITCODE -ne 0) {
        return (& $Fail "bad_header")
    }

    $Entry = [uint32]0
    foreach ($line in $output) {
        if ($line -match 'Entry point\s+0x([0-9A-Fa-f]+)') {
            $Entry = ConvertFrom-HexU32 -Text $Matches[1]
            break
        }
    }
    if ($Entry -eq 0) {
        return (& $Fail "bad_header")
    }

    $MinSet = $false
    $MinVaddr = [uint32]0
    $MaxVaddr = [uint32]0
    $Segments = [uint32]0
    $EntryOk = $false
    foreach ($line in $output) {
        $fields = @($line.Trim() -split '\s+' | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
        if ($fields.Count -lt 6 -or $fields[0] -ne "LOAD") {
            continue
        }

        $Vaddr = ConvertFrom-HexU32 -Text $fields[2]
        $Filesz = ConvertFrom-HexU32 -Text $fields[4]
        $Memsz = ConvertFrom-HexU32 -Text $fields[5]
        if ($Memsz -eq 0 -and $Filesz -eq 0) {
            continue
        }
        if ($Memsz -lt $Filesz) {
            return (& $Fail "bad_program_header")
        }
        if (-not $MinSet -or $Vaddr -lt $MinVaddr) {
            $MinVaddr = $Vaddr
            $MinSet = $true
        }
        $End = [uint64]$Vaddr + [uint64]$Memsz
        if ($End -gt [uint64]4294967295) {
            return (& $Fail "bad_program_header")
        }
        if ([uint32]$End -gt $MaxVaddr) {
            $MaxVaddr = [uint32]$End
        }
        if ($Entry -ge $Vaddr -and $Entry -lt [uint32]$End) {
            $EntryOk = $true
        }
        $Segments++
    }

    if ($Segments -eq 0 -or -not $MinSet -or $MaxVaddr -le $MinVaddr) {
        return (& $Fail "no_load_segment")
    }
    $Span = [uint32]($MaxVaddr - $MinVaddr)
    if (-not $EntryOk -or $Entry -lt $MinVaddr) {
        return [pscustomobject]@{
            Code = "entry_outside_segment"
            EntryOffset = [uint32]0
            LoadSpan = $Span
            SegmentCount = $Segments
            Runnable = $false
            RunRegionSize = $RunRegionSize
            RunRegionFits = ($Span -le $RunRegionSize)
        }
    }
    return [pscustomobject]@{
        Code = "ok"
        EntryOffset = [uint32]($Entry - $MinVaddr)
        LoadSpan = $Span
        SegmentCount = $Segments
        Runnable = $true
        RunRegionSize = $RunRegionSize
        RunRegionFits = ($Span -le $RunRegionSize)
    }
}

if ($PacketChunkSize -eq 0) {
    throw "PacketChunkSize must be non-zero"
}

if (-not (Test-Path $OutDir)) {
    New-Item -ItemType Directory -Path $OutDir | Out-Null
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "../../..")
$outResolved = (Resolve-Path $OutDir).Path

& (Join-Path $PSScriptRoot "build_app_elf_samples.ps1") `
    -ToolchainPrefix $ToolchainPrefix `
    -OutDir $outResolved `
    -IncDir $outResolved `
    -ElfBase $ElfBase `
    -HostCompiler $HostCompiler `
    -StorePath (Join-Path $outResolved "appstore.bin")
if ($LASTEXITCODE -ne 0) {
    throw "build_app_elf_samples.ps1 failed with exit code $LASTEXITCODE"
}

$packetToolSource = Join-Path $repoRoot "Examples/system/dev_loader_packet_stream_tool"
$packetToolBuild = Join-Path $outResolved "cmake-build-dev-loader-packet-stream-tool"
Invoke-Checked -FilePath "cmake" -Arguments @(
    "-S", $packetToolSource,
    "-B", $packetToolBuild,
    "-G", "Ninja",
    "-DCMAKE_CXX_COMPILER=$HostCompiler"
)
Invoke-Checked -FilePath "cmake" -Arguments @("--build", $packetToolBuild)

$packetTool = Join-Path $packetToolBuild "dev-loader-packet-stream.exe"
$artifacts = @(
    @{ Name = "hello_app"; Format = "elf"; Path = Join-Path $outResolved "hello_app.elf"; Flags = [uint32]0 },
    @{ Name = "player_min"; Format = "elf"; Path = Join-Path $outResolved "player_min.elf"; Flags = [uint32]0 },
    @{ Name = "modulex_hello_app"; Format = "modulex"; Path = Join-Path $outResolved "modulex_hello_app.modulex"; Flags = [uint32]1 }
)

foreach ($artifact in $artifacts) {
    $packetPath = "$($artifact.Path).packetstream"
    Invoke-Checked -FilePath $packetTool -Arguments @(
        $artifact.Path,
        $packetPath,
        "--chunk",
        ([string]$PacketChunkSize)
    )
    $artifact.PacketStream = $packetPath
}

$storePath = Join-Path $outResolved "appstore.bin"
$storePacketPath = Join-Path $outResolved "appstore.bin.packetstream"
Invoke-Checked -FilePath $packetTool -Arguments @(
    $storePath,
    $storePacketPath,
    "--chunk",
    ([string]$PacketChunkSize)
)

$storeBytes = [System.IO.File]::ReadAllBytes($storePath)
$storePacketBytes = [System.IO.File]::ReadAllBytes($storePacketPath)
$manifest = [ordered]@{
    schema = "charm.resident_platform.artifacts.v1"
    generated_utc = [System.DateTime]::UtcNow.ToString("o")
    elf_base = $ElfBase
    packet_chunk_size = $PacketChunkSize
    store = [ordered]@{
        path = Get-RelativePath -BasePath $outResolved -Path $storePath
        size = $storeBytes.Length
        crc32 = ("0x{0:x8}" -f (Get-Crc32 -Bytes $storeBytes))
        packetstream_path = Get-RelativePath -BasePath $outResolved -Path $storePacketPath
        packetstream_size = $storePacketBytes.Length
    }
    artifacts = @($artifacts | ForEach-Object {
        New-ArtifactEntry `
            -Name $_.Name `
            -Format $_.Format `
            -Path $_.Path `
            -StoreFlags $_.Flags `
            -PacketStreamPath $_.PacketStream
    })
}

$manifestPath = Join-Path $outResolved "artifact_manifest.json"
$manifest | ConvertTo-Json -Depth 8 | Set-Content -Path $manifestPath -Encoding UTF8

if ($Validate) {
    $manifestText = Get-Content -Raw -Encoding UTF8 $manifestPath
    if ($manifestText -notmatch '"schema":\s*"charm\.resident_platform\.artifacts\.v1"') {
        throw "artifact_manifest.json schema token missing"
    }
    foreach ($artifact in $manifest.artifacts) {
        $artifactPath = Join-Path $outResolved $artifact.path
        $packetPath = Join-Path $outResolved $artifact.packetstream_path
        if (-not (Test-Path $artifactPath)) {
            throw "missing artifact: $artifactPath"
        }
        if (-not (Test-Path $packetPath)) {
            throw "missing packetstream: $packetPath"
        }
        $actualBytes = [System.IO.File]::ReadAllBytes($artifactPath)
        $actualCrc = "0x{0:x8}" -f (Get-Crc32 -Bytes $actualBytes)
        if ($actualBytes.Length -ne $artifact.size -or $actualCrc -ne $artifact.crc32) {
            throw "artifact manifest mismatch: $($artifact.name)"
        }
    }
}

Write-Host "[ok] resident platform artifacts built at $outResolved"
Write-Host "[ok] manifest $manifestPath"
