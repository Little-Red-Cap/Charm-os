param(
    [string]$DriveRoot = "",
    [string]$ArtifactManifest = "",
    [string[]]$Apps = @("hello_app", "player_min"),
    [string]$TargetDirectory = "CHARM\APPS",
    [switch]$DryRun,
    [switch]$SelfTest
)

$ErrorActionPreference = "Stop"
[Console]::InputEncoding = [System.Text.Encoding]::UTF8
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
. (Join-Path $PSScriptRoot "charm-resident-artifacts.ps1")

function Get-ResidentLauncherAppList {
    param([string[]]$RawApps)

    $Seen = New-Object System.Collections.Generic.HashSet[string]
    $Result = New-Object System.Collections.Generic.List[string]
    foreach ($Item in $RawApps) {
        foreach ($Part in ([string]$Item -split ",")) {
            $Name = $Part.Trim()
            if ([string]::IsNullOrWhiteSpace($Name)) {
                continue
            }
            if ($Seen.Add($Name)) {
                [void]$Result.Add($Name)
            }
        }
    }
    if ($Result.Count -eq 0) {
        throw "invalid_argument: at least one app name must be provided"
    }
    return ,$Result.ToArray()
}

function Resolve-ResidentLauncherDriveRoot {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "target_missing: DriveRoot is required"
    }
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw "target_missing: DriveRoot not found: $Path"
    }
    return (Resolve-Path -LiteralPath $Path).Path
}

function Get-ResidentLauncherProvisionPlan {
    param(
        [string]$DriveRoot,
        [string]$ArtifactManifest,
        [string[]]$Apps,
        [string]$TargetDirectory
    )

    $Manifest = Read-CharmResidentArtifactManifest -Path $ArtifactManifest
    $Root = Resolve-ResidentLauncherDriveRoot -Path $DriveRoot
    $AppNames = Get-ResidentLauncherAppList -RawApps $Apps
    $DestinationDir = Join-Path $Root $TargetDirectory
    $Items = New-Object System.Collections.Generic.List[object]

    foreach ($Name in $AppNames) {
        $Artifact = Get-CharmResidentArtifact -Manifest $Manifest -Name $Name
        if ($Artifact.Format -ne "elf") {
            throw "format_mismatch: resident launcher v1 only provisions ELF apps, got $($Artifact.Format) for $Name"
        }
        $Destination = Join-Path $DestinationDir ([System.IO.Path]::GetFileName($Artifact.Path))
        [void]$Items.Add([pscustomobject]@{
            Name = $Artifact.Name
            Source = $Artifact.Path
            Destination = $Destination
            Size = $Artifact.Size
            Crc32 = $Artifact.Crc32
        })
    }

    return [pscustomobject]@{
        Manifest = $Manifest
        DriveRoot = $Root
        DestinationDir = $DestinationDir
        Items = @($Items.ToArray())
    }
}

function Test-ResidentLauncherWritableTarget {
    param([string]$DestinationDir)

    try {
        New-Item -ItemType Directory -Force -Path $DestinationDir | Out-Null
        $Probe = Join-Path $DestinationDir (".charm_write_probe_" + [System.Guid]::NewGuid().ToString("N"))
        [System.IO.File]::WriteAllText($Probe, "probe", [System.Text.Encoding]::UTF8)
        Remove-Item -LiteralPath $Probe -Force
    } catch {
        throw "target_not_writable: $DestinationDir ($($_.Exception.Message))"
    }
}

function Invoke-ResidentLauncherProvision {
    param(
        [string]$DriveRoot,
        [string]$ArtifactManifest,
        [string[]]$Apps,
        [string]$TargetDirectory,
        [bool]$DryRun
    )

    $Plan = Get-ResidentLauncherProvisionPlan `
        -DriveRoot $DriveRoot `
        -ArtifactManifest $ArtifactManifest `
        -Apps $Apps `
        -TargetDirectory $TargetDirectory

    Write-Host "Resident Launcher eMMC app provisioning"
    Write-Host "  manifest: $($Plan.Manifest.Path)"
    Write-Host "  drive: $($Plan.DriveRoot)"
    Write-Host "  target: $($Plan.DestinationDir)"
    Write-Host "  dry_run: $DryRun"
    foreach ($Item in $Plan.Items) {
        Write-Host ("  app {0}: {1} bytes crc={2} -> {3}" -f `
            $Item.Name, `
            $Item.Size, `
            (Format-CharmResidentHex32 $Item.Crc32), `
            $Item.Destination)
    }

    if ($DryRun) {
        Write-Host "Dry run complete."
        return 0
    }

    Test-ResidentLauncherWritableTarget -DestinationDir $Plan.DestinationDir
    foreach ($Item in $Plan.Items) {
        try {
            Copy-Item -LiteralPath $Item.Source -Destination $Item.Destination -Force
        } catch {
            throw "copy_failed: $($Item.Name) $($Item.Source) -> $($Item.Destination) ($($_.Exception.Message))"
        }
    }

    Write-Host "Resident Launcher eMMC app provisioning passed."
    return 0
}

function New-ResidentLauncherSelfTestPacketStream {
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

function New-ResidentLauncherSelfTestManifest {
    param([string]$Root)

    [byte[]]$Payload = @(0x01, 0x02, 0x03, 0x04)
    foreach ($File in @("hello_app.elf", "player_min.elf", "modulex_hello_app.modulex", "appstore.bin")) {
        [System.IO.File]::WriteAllBytes((Join-Path $Root $File), $Payload)
        New-ResidentLauncherSelfTestPacketStream -Path (Join-Path $Root "$File.packetstream") -Bytes $Payload
    }
    $Crc = Format-CharmResidentHex32 (Get-CharmResidentCrc32 -Bytes $Payload)
    $ManifestPath = Join-Path $Root "artifact_manifest.json"
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
    return $ManifestPath
}

function Test-ResidentLauncherThrowsLike {
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

function Invoke-ResidentLauncherProvisionSelfTest {
    $TempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("charm_launcher_provision_" + [System.Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $TempRoot | Out-Null
    try {
        $ArtifactRoot = Join-Path $TempRoot "artifacts"
        $DriveRoot = Join-Path $TempRoot "drive"
        New-Item -ItemType Directory -Path $ArtifactRoot | Out-Null
        New-Item -ItemType Directory -Path $DriveRoot | Out-Null
        $ManifestPath = New-ResidentLauncherSelfTestManifest -Root $ArtifactRoot

        [void](Invoke-ResidentLauncherProvision `
            -DriveRoot $DriveRoot `
            -ArtifactManifest $ManifestPath `
            -Apps @("hello_app", "player_min") `
            -TargetDirectory "CHARM\APPS" `
            -DryRun $true)

        [void](Invoke-ResidentLauncherProvision `
            -DriveRoot $DriveRoot `
            -ArtifactManifest $ManifestPath `
            -Apps @("hello_app", "player_min") `
            -TargetDirectory "CHARM\APPS" `
            -DryRun $false)

        if (-not (Test-Path -LiteralPath (Join-Path $DriveRoot "CHARM\APPS\hello_app.elf"))) {
            return $false
        }
        if (-not (Test-ResidentLauncherThrowsLike -Prefix "target_missing:" -Script {
            Invoke-ResidentLauncherProvision -DriveRoot (Join-Path $TempRoot "missing") -ArtifactManifest $ManifestPath -Apps @("hello_app") -TargetDirectory "CHARM\APPS" -DryRun $true
        })) {
            return $false
        }
        if (-not (Test-ResidentLauncherThrowsLike -Prefix "artifact_missing:" -Script {
            Invoke-ResidentLauncherProvision -DriveRoot $DriveRoot -ArtifactManifest $ManifestPath -Apps @("missing_app") -TargetDirectory "CHARM\APPS" -DryRun $true
        })) {
            return $false
        }
        if (-not (Test-ResidentLauncherThrowsLike -Prefix "format_mismatch:" -Script {
            Invoke-ResidentLauncherProvision -DriveRoot $DriveRoot -ArtifactManifest $ManifestPath -Apps @("modulex_hello_app") -TargetDirectory "CHARM\APPS" -DryRun $true
        })) {
            return $false
        }
        return $true
    } finally {
        if (Test-Path -LiteralPath $TempRoot) {
            Remove-Item -LiteralPath $TempRoot -Recurse -Force
        }
    }
}

if ($SelfTest) {
    if (Invoke-ResidentLauncherProvisionSelfTest) {
        Write-Host "SelfTest passed."
        exit 0
    }
    Write-Host "SelfTest failed."
    exit 1
}

exit (Invoke-ResidentLauncherProvision `
    -DriveRoot $DriveRoot `
    -ArtifactManifest $ArtifactManifest `
    -Apps $Apps `
    -TargetDirectory $TargetDirectory `
    -DryRun ([bool]$DryRun))
