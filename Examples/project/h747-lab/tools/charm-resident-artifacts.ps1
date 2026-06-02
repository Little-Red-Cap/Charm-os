$ErrorActionPreference = "Stop"

function Get-CharmResidentDefaultManifestPath {
    return (Join-Path $PSScriptRoot "..\..\..\app_abi\elf_samples\out\artifact_manifest.json")
}

function ConvertFrom-CharmResidentHex32 {
    param([string]$Text)

    if ([string]::IsNullOrWhiteSpace($Text)) {
        throw "invalid_manifest: empty hex32 value"
    }
    $Trimmed = $Text.Trim()
    if ($Trimmed.StartsWith("0x", [System.StringComparison]::OrdinalIgnoreCase)) {
        $Trimmed = $Trimmed.Substring(2)
    }
    if ($Trimmed.Length -ne 8) {
        throw "invalid_manifest: hex32 value must contain 8 digits: $Text"
    }
    return [Convert]::ToUInt32($Trimmed, 16)
}

function Format-CharmResidentHex32 {
    param([uint32]$Value)
    return ("0x{0:x8}" -f $Value)
}

function Get-CharmResidentCrc32 {
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

function Get-CharmResidentPacketStreamBeginInfo {
    param([byte[]]$Bytes)

    $HeaderSize = 28
    if ($Bytes.Length -lt $HeaderSize) {
        throw "packetstream_invalid: too small to contain a packet header"
    }

    $Magic = [BitConverter]::ToUInt32($Bytes, 0)
    $Version = [BitConverter]::ToUInt16($Bytes, 4)
    $HeaderFieldSize = [BitConverter]::ToUInt16($Bytes, 6)
    $Kind = [BitConverter]::ToUInt16($Bytes, 8)
    $Flags = [BitConverter]::ToUInt16($Bytes, 10)
    $Sequence = [BitConverter]::ToUInt32($Bytes, 12)
    $Size = [BitConverter]::ToUInt32($Bytes, 20)
    $Crc32 = [BitConverter]::ToUInt32($Bytes, 24)

    if ($Magic -ne 0x504C5643) {
        throw ("packetstream_invalid: bad magic 0x{0:x8}" -f $Magic)
    }
    if ($Version -ne 1) {
        throw "packetstream_invalid: unsupported version $Version"
    }
    if ($HeaderFieldSize -ne $HeaderSize) {
        throw "packetstream_invalid: unsupported header size $HeaderFieldSize"
    }
    if ($Kind -ne 1) {
        throw "packetstream_invalid: first packet is not begin: kind=$Kind"
    }
    if ($Sequence -ne 0) {
        throw "packetstream_invalid: begin packet sequence must be 0, got $Sequence"
    }
    if ($Size -eq 0) {
        throw "packetstream_invalid: begin packet payload size must be non-zero"
    }

    return [pscustomobject]@{
        PayloadSize = [uint32]$Size
        Crc32 = [uint32]$Crc32
        CheckCrc = (($Flags -band 1) -ne 0)
    }
}

function Resolve-CharmResidentArtifactPath {
    param(
        [string]$ManifestPath,
        [string]$RelativePath
    )

    if ([string]::IsNullOrWhiteSpace($RelativePath)) {
        throw "invalid_manifest: artifact path is empty"
    }
    $ManifestDir = Split-Path -Parent (Resolve-Path -LiteralPath $ManifestPath).Path
    return [System.IO.Path]::GetFullPath((Join-Path $ManifestDir $RelativePath))
}

function Test-CharmResidentArtifactFile {
    param(
        [string]$Path,
        [uint64]$ExpectedSize,
        [uint32]$ExpectedCrc32,
        [string]$Label
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "artifact_missing: $Label path not found: $Path"
    }
    $ResolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $Bytes = [System.IO.File]::ReadAllBytes($ResolvedPath)
    if ([uint64]$Bytes.Length -ne $ExpectedSize) {
        throw "size_mismatch: $Label expected=$ExpectedSize actual=$($Bytes.Length) path=$ResolvedPath"
    }
    $ActualCrc = Get-CharmResidentCrc32 -Bytes $Bytes
    if ($ActualCrc -ne $ExpectedCrc32) {
        throw "crc_mismatch: $Label expected=$(Format-CharmResidentHex32 $ExpectedCrc32) actual=$(Format-CharmResidentHex32 $ActualCrc) path=$ResolvedPath"
    }
    return [pscustomobject]@{
        Path = $ResolvedPath
        Bytes = $Bytes
        Size = [uint32]$Bytes.Length
        Crc32 = $ActualCrc
    }
}

function Get-CharmResidentExpectedStoreFlags {
    param([string]$Format)

    switch ($Format) {
        "elf" { return [uint32]0 }
        "modulex" { return [uint32]1 }
        default { throw "format_mismatch: unsupported artifact format '$Format'" }
    }
}

function Read-CharmResidentArtifactManifest {
    param([string]$Path = "")

    if ([string]::IsNullOrWhiteSpace($Path)) {
        $Path = Get-CharmResidentDefaultManifestPath
    }
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "manifest_missing: $Path"
    }
    $ResolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $JsonText = Get-Content -LiteralPath $ResolvedPath -Raw -Encoding UTF8
    try {
        $Json = $JsonText | ConvertFrom-Json
    } catch {
        throw "invalid_schema: failed to parse manifest JSON: $($_.Exception.Message)"
    }

    if ($Json.schema -ne "charm.resident_platform.artifacts.v1") {
        throw "invalid_schema: expected charm.resident_platform.artifacts.v1, got '$($Json.schema)'"
    }
    if ($null -eq $Json.store -or $null -eq $Json.artifacts) {
        throw "invalid_schema: manifest must contain store and artifacts"
    }

    $Expected = @{
        hello_app = "elf"
        player_min = "elf"
        modulex_hello_app = "modulex"
    }
    $Artifacts = @()
    foreach ($Item in @($Json.artifacts)) {
        if ([string]::IsNullOrWhiteSpace($Item.name)) {
            throw "invalid_schema: artifact name is empty"
        }
        if (-not $Expected.ContainsKey([string]$Item.name)) {
            throw "invalid_schema: unexpected artifact '$($Item.name)'"
        }
        if ($Item.format -ne $Expected[[string]$Item.name]) {
            throw "format_mismatch: $($Item.name) expected=$($Expected[[string]$Item.name]) actual=$($Item.format)"
        }
        $ExpectedFlags = Get-CharmResidentExpectedStoreFlags -Format $Item.format
        if ([uint32]$Item.store_flags -ne $ExpectedFlags) {
            throw "store_flags_mismatch: $($Item.name) expected=$ExpectedFlags actual=$($Item.store_flags)"
        }

        $ArtifactPath = Resolve-CharmResidentArtifactPath -ManifestPath $ResolvedPath -RelativePath $Item.path
        $PacketPath = Resolve-CharmResidentArtifactPath -ManifestPath $ResolvedPath -RelativePath $Item.packetstream_path
        $PayloadCrc = ConvertFrom-CharmResidentHex32 -Text $Item.crc32
        $PayloadFile = Test-CharmResidentArtifactFile `
            -Path $ArtifactPath `
            -ExpectedSize ([uint64]$Item.size) `
            -ExpectedCrc32 $PayloadCrc `
            -Label $Item.name

        if (-not (Test-Path -LiteralPath $PacketPath)) {
            throw "artifact_missing: packetstream for $($Item.name) not found: $PacketPath"
        }
        $PacketBytes = [System.IO.File]::ReadAllBytes($PacketPath)
        if ([uint64]$PacketBytes.Length -ne [uint64]$Item.packetstream_size) {
            throw "size_mismatch: packetstream $($Item.name) expected=$($Item.packetstream_size) actual=$($PacketBytes.Length)"
        }
        $PacketInfo = Get-CharmResidentPacketStreamBeginInfo -Bytes $PacketBytes
        if ($PacketInfo.PayloadSize -ne [uint32]$Item.size) {
            throw "size_mismatch: packetstream $($Item.name) payload expected=$($Item.size) actual=$($PacketInfo.PayloadSize)"
        }
        if ($PacketInfo.Crc32 -ne $PayloadCrc) {
            throw "crc_mismatch: packetstream $($Item.name) expected=$(Format-CharmResidentHex32 $PayloadCrc) actual=$(Format-CharmResidentHex32 $PacketInfo.Crc32)"
        }

        $Artifacts += [pscustomobject]@{
            Name = [string]$Item.name
            Format = [string]$Item.format
            Path = $PayloadFile.Path
            Size = [uint32]$Item.size
            Crc32 = $PayloadCrc
            StoreFlags = [uint32]$Item.store_flags
            PacketStreamPath = (Resolve-Path -LiteralPath $PacketPath).Path
            PacketStreamSize = [uint32]$PacketBytes.Length
        }
    }

    foreach ($Name in $Expected.Keys) {
        if (-not (@($Artifacts | Where-Object { $_.Name -eq $Name }).Count -eq 1)) {
            throw "invalid_schema: manifest must contain exactly one artifact named $Name"
        }
    }

    $StorePath = Resolve-CharmResidentArtifactPath -ManifestPath $ResolvedPath -RelativePath $Json.store.path
    $StorePacketPath = Resolve-CharmResidentArtifactPath -ManifestPath $ResolvedPath -RelativePath $Json.store.packetstream_path
    $StoreCrc = ConvertFrom-CharmResidentHex32 -Text $Json.store.crc32
    $StoreFile = Test-CharmResidentArtifactFile `
        -Path $StorePath `
        -ExpectedSize ([uint64]$Json.store.size) `
        -ExpectedCrc32 $StoreCrc `
        -Label "appstore.bin"
    if (-not (Test-Path -LiteralPath $StorePacketPath)) {
        throw "artifact_missing: store packetstream not found: $StorePacketPath"
    }
    $StorePacketBytes = [System.IO.File]::ReadAllBytes($StorePacketPath)
    if ([uint64]$StorePacketBytes.Length -ne [uint64]$Json.store.packetstream_size) {
        throw "size_mismatch: store packetstream expected=$($Json.store.packetstream_size) actual=$($StorePacketBytes.Length)"
    }
    $StorePacketInfo = Get-CharmResidentPacketStreamBeginInfo -Bytes $StorePacketBytes
    if ($StorePacketInfo.PayloadSize -ne [uint32]$Json.store.size) {
        throw "size_mismatch: store packetstream payload expected=$($Json.store.size) actual=$($StorePacketInfo.PayloadSize)"
    }
    if ($StorePacketInfo.Crc32 -ne $StoreCrc) {
        throw "crc_mismatch: store packetstream expected=$(Format-CharmResidentHex32 $StoreCrc) actual=$(Format-CharmResidentHex32 $StorePacketInfo.Crc32)"
    }

    return [pscustomobject]@{
        Path = $ResolvedPath
        StorePath = $StoreFile.Path
        StoreSize = [uint32]$Json.store.size
        StoreCrc32 = $StoreCrc
        StorePacketStreamPath = (Resolve-Path -LiteralPath $StorePacketPath).Path
        StorePacketStreamSize = [uint32]$StorePacketBytes.Length
        Artifacts = $Artifacts
    }
}

function Get-CharmResidentArtifact {
    param(
        [object]$Manifest,
        [string]$Name
    )

    $Matches = @($Manifest.Artifacts | Where-Object { $_.Name -eq $Name })
    if ($Matches.Count -ne 1) {
        throw "artifact_missing: manifest does not contain app '$Name'"
    }
    return $Matches[0]
}

function Test-CharmResidentThrowsLike {
    param(
        [scriptblock]$Script,
        [string]$Prefix
    )

    try {
        & $Script
    } catch {
        return $_.Exception.Message.StartsWith($Prefix, [System.StringComparison]::Ordinal)
    }
    return $false
}

function Test-CharmResidentArtifactManifestSelfTest {
    $TempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("charm_resident_manifest_selftest_" + [System.Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $TempRoot | Out-Null
    try {
        [byte[]]$Payload = @(0x01, 0x02, 0x03, 0x04)
        [System.IO.File]::WriteAllBytes((Join-Path $TempRoot "hello_app.elf"), $Payload)
        [System.IO.File]::WriteAllBytes((Join-Path $TempRoot "player_min.elf"), $Payload)
        [System.IO.File]::WriteAllBytes((Join-Path $TempRoot "modulex_hello_app.modulex"), $Payload)
        [System.IO.File]::WriteAllBytes((Join-Path $TempRoot "appstore.bin"), $Payload)

        function New-SelfTestPacketStream {
            param(
                [string]$Path,
                [byte[]]$Bytes
            )
            $Header = New-Object byte[] 28
            [BitConverter]::GetBytes([uint32]0x504C5643).CopyTo($Header, 0)
            [BitConverter]::GetBytes([uint16]1).CopyTo($Header, 4)
            [BitConverter]::GetBytes([uint16]28).CopyTo($Header, 6)
            [BitConverter]::GetBytes([uint16]1).CopyTo($Header, 8)
            [BitConverter]::GetBytes([uint16]1).CopyTo($Header, 10)
            [BitConverter]::GetBytes([uint32]0).CopyTo($Header, 12)
            [BitConverter]::GetBytes([uint32]0).CopyTo($Header, 16)
            [BitConverter]::GetBytes([uint32]$Bytes.Length).CopyTo($Header, 20)
            [BitConverter]::GetBytes([uint32](Get-CharmResidentCrc32 -Bytes $Bytes)).CopyTo($Header, 24)
            [System.IO.File]::WriteAllBytes($Path, $Header)
        }

        foreach ($Name in @("hello_app.elf", "player_min.elf", "modulex_hello_app.modulex", "appstore.bin")) {
            New-SelfTestPacketStream -Path (Join-Path $TempRoot "$Name.packetstream") -Bytes $Payload
        }

        $Crc = Format-CharmResidentHex32 (Get-CharmResidentCrc32 -Bytes $Payload)
        $ManifestPath = Join-Path $TempRoot "artifact_manifest.json"
        $Manifest = [ordered]@{
            schema = "charm.resident_platform.artifacts.v1"
            store = [ordered]@{
                path = "appstore.bin"
                size = 4
                crc32 = $Crc
                packetstream_path = "appstore.bin.packetstream"
                packetstream_size = 28
            }
            artifacts = @(
                [ordered]@{ name = "hello_app"; format = "elf"; path = "hello_app.elf"; size = 4; crc32 = $Crc; store_flags = 0; packetstream_path = "hello_app.elf.packetstream"; packetstream_size = 28 },
                [ordered]@{ name = "player_min"; format = "elf"; path = "player_min.elf"; size = 4; crc32 = $Crc; store_flags = 0; packetstream_path = "player_min.elf.packetstream"; packetstream_size = 28 },
                [ordered]@{ name = "modulex_hello_app"; format = "modulex"; path = "modulex_hello_app.modulex"; size = 4; crc32 = $Crc; store_flags = 1; packetstream_path = "modulex_hello_app.modulex.packetstream"; packetstream_size = 28 }
            )
        }
        $Manifest | ConvertTo-Json -Depth 8 | Set-Content -Path $ManifestPath -Encoding UTF8
        [void](Read-CharmResidentArtifactManifest -Path $ManifestPath)

        if (-not (Test-CharmResidentThrowsLike -Prefix "manifest_missing:" -Script { Read-CharmResidentArtifactManifest -Path (Join-Path $TempRoot "missing.json") })) {
            return $false
        }

        $Bad = $Manifest | ConvertTo-Json -Depth 8 | ConvertFrom-Json
        $Bad.schema = "bad"
        $BadPath = Join-Path $TempRoot "bad_schema.json"
        $Bad | ConvertTo-Json -Depth 8 | Set-Content -Path $BadPath -Encoding UTF8
        if (-not (Test-CharmResidentThrowsLike -Prefix "invalid_schema:" -Script { Read-CharmResidentArtifactManifest -Path $BadPath })) {
            return $false
        }

        $Bad = $Manifest | ConvertTo-Json -Depth 8 | ConvertFrom-Json
        $Bad.artifacts[0].size = 5
        $BadPath = Join-Path $TempRoot "bad_size.json"
        $Bad | ConvertTo-Json -Depth 8 | Set-Content -Path $BadPath -Encoding UTF8
        if (-not (Test-CharmResidentThrowsLike -Prefix "size_mismatch:" -Script { Read-CharmResidentArtifactManifest -Path $BadPath })) {
            return $false
        }

        $Bad = $Manifest | ConvertTo-Json -Depth 8 | ConvertFrom-Json
        $Bad.artifacts[0].crc32 = "0x00000000"
        $BadPath = Join-Path $TempRoot "bad_crc.json"
        $Bad | ConvertTo-Json -Depth 8 | Set-Content -Path $BadPath -Encoding UTF8
        if (-not (Test-CharmResidentThrowsLike -Prefix "crc_mismatch:" -Script { Read-CharmResidentArtifactManifest -Path $BadPath })) {
            return $false
        }

        $Bad = $Manifest | ConvertTo-Json -Depth 8 | ConvertFrom-Json
        $Bad.artifacts[2].store_flags = 0
        $BadPath = Join-Path $TempRoot "bad_flags.json"
        $Bad | ConvertTo-Json -Depth 8 | Set-Content -Path $BadPath -Encoding UTF8
        if (-not (Test-CharmResidentThrowsLike -Prefix "store_flags_mismatch:" -Script { Read-CharmResidentArtifactManifest -Path $BadPath })) {
            return $false
        }

        return $true
    } finally {
        if (Test-Path -LiteralPath $TempRoot) {
            Remove-Item -LiteralPath $TempRoot -Recurse -Force
        }
    }
}
