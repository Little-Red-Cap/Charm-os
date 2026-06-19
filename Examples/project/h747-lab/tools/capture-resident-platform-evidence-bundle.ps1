param(
    [string]$ArtifactManifest = "",
    [string]$Log = "",
    [switch]$BoardMatrix,
    [switch]$InstalledStoreMatrix,
    [string]$ControlPort = "COM16",
    [string]$UsbPort = "",
    [string[]]$Media = @("qspi", "emmc"),
    [int]$RepeatPerMedia = 1,
    [int]$WriteChunkSize = 256,
    [int]$InterChunkDelayMs = 1,
    [switch]$QemuElf,
    [switch]$QemuElfValidateOnly,
    [switch]$SkipH747Build,
    [switch]$DryRun,
    [switch]$SelfTest
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "charm-resident-artifacts.ps1")

function Get-RepoRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot "..\..\..\..")).Path
}

function Get-DefaultBundleLog {
    $H747Root = Resolve-Path (Join-Path $PSScriptRoot "..")
    return (Join-Path $H747Root "cmake-build-h747-lab-debug\resident_platform_evidence_bundle.log")
}

function Get-DefaultManifest {
    return (Get-CharmResidentDefaultManifestPath)
}

function Get-CmakeBuildDir {
    param(
        [string]$SourceDir,
        [string]$BuildName
    )
    return (Join-Path $SourceDir $BuildName)
}

function Get-MediaList {
    param([string[]]$RawMedia)

    $Seen = New-Object System.Collections.Generic.HashSet[string]
    $Result = New-Object System.Collections.Generic.List[string]
    foreach ($Item in $RawMedia) {
        foreach ($Part in ($Item -split ",")) {
            $Name = $Part.Trim().ToLowerInvariant()
            if ([string]::IsNullOrWhiteSpace($Name)) {
                continue
            }
            if ($Name -ne "qspi" -and $Name -ne "emmc") {
                throw "Unsupported media '$Name'. Expected qspi or emmc."
            }
            if ($Seen.Add($Name)) {
                [void]$Result.Add($Name)
            }
        }
    }
    if ($Result.Count -eq 0) {
        throw "At least one media must be selected."
    }
    return ,$Result.ToArray()
}

function Write-BundleLine {
    param(
        [System.Collections.Generic.List[string]]$Lines,
        [string]$Text
    )
    Write-Host $Text
    [void]$Lines.Add($Text)
}

function Invoke-Logged {
    param(
        [System.Collections.Generic.List[string]]$Lines,
        [string]$Label,
        [string]$FilePath,
        [string[]]$Arguments,
        [string]$WorkingDirectory = ""
    )

    $CommandLine = "$FilePath $($Arguments -join ' ')"
    Write-BundleLine -Lines $Lines -Text "== $Label =="
    Write-BundleLine -Lines $Lines -Text $CommandLine

    if ([string]::IsNullOrWhiteSpace($WorkingDirectory)) {
        & $FilePath @Arguments 2>&1 | Tee-Object -Variable Output | Write-Host
    } else {
        Push-Location $WorkingDirectory
        try {
            & $FilePath @Arguments 2>&1 | Tee-Object -Variable Output | Write-Host
        } finally {
            Pop-Location
        }
    }

    foreach ($Line in @($Output)) {
        [void]$Lines.Add([string]$Line)
    }
    if ($LASTEXITCODE -ne 0) {
        throw "$Label failed with exit code $LASTEXITCODE"
    }
}

function Invoke-CmakeProject {
    param(
        [System.Collections.Generic.List[string]]$Lines,
        [string]$Name,
        [string]$SourceDir,
        [string]$BuildDir
    )

    Invoke-Logged -Lines $Lines -Label "$Name configure" -FilePath "cmake" -Arguments @(
        "-S", $SourceDir,
        "-B", $BuildDir
    )
    Invoke-Logged -Lines $Lines -Label "$Name build" -FilePath "cmake" -Arguments @(
        "--build", $BuildDir, "--config", "Debug"
    )
}

function Invoke-Ctest {
    param(
        [System.Collections.Generic.List[string]]$Lines,
        [string]$Name,
        [string]$BuildDir
    )

    Invoke-Logged -Lines $Lines -Label "$Name ctest" -FilePath "ctest" -Arguments @(
        "--test-dir", $BuildDir,
        "-C", "Debug",
        "--output-on-failure"
    )
}

function Get-ExecutablePath {
    param(
        [string]$BuildDir,
        [string]$ExeName
    )

    $DebugPath = Join-Path $BuildDir "Debug\$ExeName"
    if (Test-Path -LiteralPath $DebugPath) {
        return (Resolve-Path -LiteralPath $DebugPath).Path
    }
    $FlatPath = Join-Path $BuildDir $ExeName
    if (Test-Path -LiteralPath $FlatPath) {
        return (Resolve-Path -LiteralPath $FlatPath).Path
    }
    throw "missing_tool: $ExeName was not built under $BuildDir"
}

function Get-BinSize {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "missing_firmware: $Path"
    }
    return (Get-Item -LiteralPath $Path).Length
}

function Invoke-SelfTest {
    $RepoRoot = Get-RepoRoot
    $DefaultManifest = [System.IO.Path]::GetFullPath((Get-DefaultManifest))
    $DefaultLog = [System.IO.Path]::GetFullPath((Get-DefaultBundleLog))

    if (-not $DefaultManifest.EndsWith("Examples\app_abi\elf_samples\out\artifact_manifest.json", [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "selftest_failed: default manifest path is unexpected: $DefaultManifest"
    }
    if (-not $DefaultLog.EndsWith("Examples\project\h747-lab\cmake-build-h747-lab-debug\resident_platform_evidence_bundle.log", [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "selftest_failed: default log path is unexpected: $DefaultLog"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $RepoRoot "Examples\app_abi\elf_samples\build_resident_platform_artifacts.ps1"))) {
        throw "selftest_failed: artifact build script is missing"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $RepoRoot "Examples\system\resident_platform_inspect_tool\CMakeLists.txt"))) {
        throw "missing_tool: resident_platform_inspect_tool source is missing"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot "capture-dev-loader-usb-cdc-appstore-platform-matrix-smoke.ps1"))) {
        throw "selftest_failed: board matrix script is missing"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $RepoRoot "Examples\system\run-resident-elf-qemu-smoke.ps1"))) {
        throw "selftest_failed: resident ELF QEMU smoke entry is missing"
    }
    if ($QemuElfValidateOnly -and -not $QemuElf) {
        throw "selftest_failed: -QemuElfValidateOnly requires -QemuElf"
    }
    $ParsedMedia = Get-MediaList -RawMedia @("qspi,emmc")
    if ($ParsedMedia.Count -ne 2 -or $ParsedMedia[0] -ne "qspi" -or $ParsedMedia[1] -ne "emmc") {
        throw "selftest_failed: comma media parsing failed"
    }
    try {
        Get-MediaList -RawMedia @("badmedia") | Out-Null
        throw "selftest_failed: invalid media did not fail"
    } catch {
        if ($_.Exception.Message.IndexOf("Unsupported media", [System.StringComparison]::Ordinal) -lt 0) {
            throw "selftest_failed: invalid media did not report Unsupported media: $($_.Exception.Message)"
        }
    }
    try {
        Get-ExecutablePath -BuildDir (Join-Path ([System.IO.Path]::GetTempPath()) "charm_missing_tool_dir") -ExeName "missing.exe" | Out-Null
        throw "selftest_failed: missing tool did not fail"
    } catch {
        if ($_.Exception.Message.IndexOf("missing_tool", [System.StringComparison]::Ordinal) -lt 0) {
            throw "selftest_failed: missing tool did not report missing_tool: $($_.Exception.Message)"
        }
    }

    try {
        Read-CharmResidentArtifactManifest -Path (Join-Path ([System.IO.Path]::GetTempPath()) "charm_missing_manifest.json") | Out-Null
        throw "selftest_failed: missing manifest did not fail"
    } catch {
        if ($_.Exception.Message.IndexOf("manifest_missing", [System.StringComparison]::Ordinal) -lt 0) {
            throw "selftest_failed: missing manifest did not report manifest_missing: $($_.Exception.Message)"
        }
    }

    function New-SelfTestQemuRun {
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

    function New-SelfTestQemuPrepare {
        return [pscustomobject]@{
            stage = "start"
            code = "ok"
            ready = $true
            argc = 4
        }
    }

    function New-SelfTestQemuSourceCase {
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

    function New-SelfTestQemuPacketstream {
        param(
            [string]$Name,
            [string]$Transport,
            [string]$Packet,
            [string]$Stage,
            [string]$Code,
            [int]$Payload,
            [int]$Stream,
            [int]$Packets,
            [int]$Dispatch,
            [string]$ActualCrc,
            [string]$ExpectedCrc,
            [string]$ReadCode = "",
            [int]$ReadBytes = 0,
            [string]$AppStageCode = "",
            [string]$AppStageFormat = "",
            [int]$AppStageBytes = 0
        )
        return [pscustomobject]@{
            name = $Name
            transport = $Transport
            packet = $Packet
            receive_stage = $Stage
            receive_code = $Code
            payload = $Payload
            stream = $Stream
            packets = $Packets
            dispatch = $Dispatch
            actual_crc = $ActualCrc
            expected_crc = $ExpectedCrc
            read_code = $ReadCode
            read_bytes = $ReadBytes
            app_stage_code = $AppStageCode
            app_stage_format = $AppStageFormat
            app_stage_bytes = $AppStageBytes
        }
    }

    $TempSummary = Join-Path ([System.IO.Path]::GetTempPath()) "charm_qemu_domain_summary_selftest.json"
    $TempQemuLog = Join-Path ([System.IO.Path]::GetTempPath()) "charm_qemu_domain_summary_selftest.log"
    $GoodSummary = [pscustomobject]@{
        schema = "charm.resident_elf_qemu.domain_summary.v1"
        domain = "virtual_m7"
        machine = "mps2-an500"
        cpu = "cortex-m7"
        image_format = "elf"
        app_model = "CharmAppApi"
        backend_contract = [pscustomobject]@{
            kind = "virtual"
            runtime_domain = "virtual_m7"
            capabilities = @("console", "time", "display", "input", "storage", "app_exit")
            storage = "readonly"
            afe = "unsupported"
        }
        run_region = [pscustomobject]@{
            base = "0x20080000"
            expected = "0x20080000"
            size = 65536
        }
        stage_cache = [pscustomobject]@{
            bytes = 16384
        }
        display = [pscustomobject]@{
            width = 16
            height = 16
            format = "argb8888"
            stride_bytes = 64
            frame_bytes = 1024
        }
        store = [pscustomobject]@{
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
        coverage = [pscustomobject]@{
            runs = 1..39
            stages = 1..19
            source_matrix = @(
                (New-SelfTestQemuSourceCase -Name "hello_app" -Direct (New-SelfTestQemuRun -Stage "exit" -Code "ok") -Received (New-SelfTestQemuRun -Stage "exit" -Code "ok") -Packetstream (New-SelfTestQemuRun -Stage "exit" -Code "ok") -Store (New-SelfTestQemuRun -Stage "exit" -Code "ok")),
                (New-SelfTestQemuSourceCase -Name "argv_app" -Direct (New-SelfTestQemuRun -Stage "exit" -Code "ok") -Store (New-SelfTestQemuRun -Stage "exit" -Code "ok") -Prepare (New-SelfTestQemuPrepare)),
                (New-SelfTestQemuSourceCase -Name "argv_overflow_app" -Direct (New-SelfTestQemuRun -Stage "argv" -Code "argv_overflow")),
                (New-SelfTestQemuSourceCase -Name "abi_mismatch_app" -Direct (New-SelfTestQemuRun -Stage "abi" -Code "abi_mismatch")),
                (New-SelfTestQemuSourceCase -Name "bss_app" -Direct (New-SelfTestQemuRun -Stage "exit" -Code "ok") -Store (New-SelfTestQemuRun -Stage "exit" -Code "ok")),
                (New-SelfTestQemuSourceCase -Name "data_app" -Direct (New-SelfTestQemuRun -Stage "exit" -Code "ok") -Store (New-SelfTestQemuRun -Stage "exit" -Code "ok")),
                (New-SelfTestQemuSourceCase -Name "exit_app" -Direct (New-SelfTestQemuRun -Stage "exit" -Code "ok") -Store (New-SelfTestQemuRun -Stage "exit" -Code "ok")),
                (New-SelfTestQemuSourceCase -Name "unsupported_caps_app" -Direct (New-SelfTestQemuRun -Stage "exit" -Code "ok") -Store (New-SelfTestQemuRun -Stage "exit" -Code "ok")),
                (New-SelfTestQemuSourceCase -Name "storage_app" -Direct (New-SelfTestQemuRun -Stage "exit" -Code "ok") -Store (New-SelfTestQemuRun -Stage "exit" -Code "ok")),
                (New-SelfTestQemuSourceCase -Name "storage_catalog_app" -Direct (New-SelfTestQemuRun -Stage "exit" -Code "ok") -Store (New-SelfTestQemuRun -Stage "exit" -Code "ok")),
                (New-SelfTestQemuSourceCase -Name "display_sequence_app" -Direct (New-SelfTestQemuRun -Stage "exit" -Code "ok") -Store (New-SelfTestQemuRun -Stage "exit" -Code "ok")),
                (New-SelfTestQemuSourceCase -Name "input_sequence_app" -Direct (New-SelfTestQemuRun -Stage "exit" -Code "ok") -Store (New-SelfTestQemuRun -Stage "exit" -Code "ok")),
                (New-SelfTestQemuSourceCase -Name "time_app" -Direct (New-SelfTestQemuRun -Stage "exit" -Code "ok") -Store (New-SelfTestQemuRun -Stage "exit" -Code "ok")),
                (New-SelfTestQemuSourceCase -Name "large_fit_app" -Direct (New-SelfTestQemuRun -Stage "exit" -Code "ok") -Received (New-SelfTestQemuRun -Stage "exit" -Code "ok") -Packetstream (New-SelfTestQemuRun -Stage "exit" -Code "ok") -Store (New-SelfTestQemuRun -Stage "exit" -Code "ok")),
                (New-SelfTestQemuSourceCase -Name "bad_elf_magic_app" -Direct (New-SelfTestQemuRun -Stage "load" -Code "load_failed")),
                (New-SelfTestQemuSourceCase -Name "packetstream_bad_elf_magic_app" -Packetstream (New-SelfTestQemuRun -Stage "load" -Code "load_failed")),
                (New-SelfTestQemuSourceCase -Name "entry_outside_segment_app" -Direct (New-SelfTestQemuRun -Stage "load" -Code "load_failed")),
                (New-SelfTestQemuSourceCase -Name "rwx_segment_app" -Direct (New-SelfTestQemuRun -Stage "load" -Code "load_failed")),
                (New-SelfTestQemuSourceCase -Name "too_large_app" -Direct (New-SelfTestQemuRun -Stage "load" -Code "load_failed")),
                (New-SelfTestQemuSourceCase -Name "player_min" -Direct (New-SelfTestQemuRun -Stage "exit" -Code "ok") -Received (New-SelfTestQemuRun -Stage "exit" -Code "ok") -Packetstream (New-SelfTestQemuRun -Stage "exit" -Code "ok") -Store (New-SelfTestQemuRun -Stage "exit" -Code "ok"))
            )
            packetstreams = @(
                (New-SelfTestQemuPacketstream -Name "hello_app" -Transport "ok" -Packet "ok" -Stage "launch_ready" -Code "ok" -Payload 5132 -Stream 5776 -Packets 23 -Dispatch 23 -ActualCrc "0xb3b7bcc5" -ExpectedCrc "0xb3b7bcc5" -ReadCode "ok" -ReadBytes 5132 -AppStageCode "ok" -AppStageFormat "elf" -AppStageBytes 5132),
                (New-SelfTestQemuPacketstream -Name "large_fit_app" -Transport "ok" -Packet "ok" -Stage "launch_ready" -Code "ok" -Payload 5168 -Stream 5840 -Packets 24 -Dispatch 24 -ActualCrc "0xdffdfba1" -ExpectedCrc "0xdffdfba1" -ReadCode "ok" -ReadBytes 5168 -AppStageCode "ok" -AppStageFormat "elf" -AppStageBytes 5168),
                (New-SelfTestQemuPacketstream -Name "packetstream_crc_mismatch" -Transport "packet_failed" -Packet "receive_failed" -Stage "failed" -Code "crc_mismatch" -Payload 5132 -Stream 5776 -Packets 23 -Dispatch 22 -ActualCrc "0x7d9c7647" -ExpectedCrc "0xb3b7bcc5"),
                (New-SelfTestQemuPacketstream -Name "packetstream_bad_elf_magic_app" -Transport "ok" -Packet "ok" -Stage "launch_ready" -Code "ok" -Payload 64 -Stream 176 -Packets 4 -Dispatch 4 -ActualCrc "0xbd40f3c7" -ExpectedCrc "0xbd40f3c7" -ReadCode "ok" -ReadBytes 64 -AppStageCode "ok" -AppStageFormat "elf" -AppStageBytes 64),
                (New-SelfTestQemuPacketstream -Name "player_min" -Transport "ok" -Packet "ok" -Stage "launch_ready" -Code "ok" -Payload 5168 -Stream 5840 -Packets 24 -Dispatch 24 -ActualCrc "0xba5eb94a" -ExpectedCrc "0xba5eb94a" -ReadCode "ok" -ReadBytes 5168 -AppStageCode "ok" -AppStageFormat "elf" -AppStageBytes 5168)
            )
            loads = @(
                [pscustomobject]@{
                    name = "hello_app"
                    format = "elf"
                    probe = "ok"
                    entry = "0x20080021"
                    span = 270
                    segments = 2
                    needed = 270
                    free = 65266
                    fits = $true
                    region = 65536
                    capacity_probe = "ok"
                },
                [pscustomobject]@{
                    name = "large_fit_app"
                    format = "elf"
                    probe = "ok"
                    entry = "0x20080019"
                    span = 61696
                    segments = 3
                    needed = 61696
                    free = 3840
                    fits = $true
                    region = 65536
                    capacity_probe = "ok"
                },
                [pscustomobject]@{
                    name = "packetstream:player_min"
                    format = "elf"
                    probe = "ok"
                    entry = "0x20080001"
                    span = 1280
                    segments = 3
                    needed = 1280
                    free = 64256
                    fits = $true
                    region = 65536
                    capacity_probe = "ok"
                },
                [pscustomobject]@{
                    name = "bad_elf_magic_app"
                    format = "elf"
                    probe = "bad_magic"
                    entry = "0x00000000"
                    span = 0
                    segments = 0
                    needed = 0
                    free = 65536
                    fits = $true
                    region = 65536
                    capacity_probe = "bad_magic"
                },
                [pscustomobject]@{
                    name = "entry_outside_segment_app"
                    format = "elf"
                    probe = "entry_outside_segment"
                    entry = "0x00000000"
                    span = 0
                    segments = 0
                    needed = 0
                    free = 65536
                    fits = $true
                    region = 65536
                    capacity_probe = "entry_outside_segment"
                },
                [pscustomobject]@{
                    name = "rwx_segment_app"
                    format = "elf"
                    probe = "rwx_segment"
                    entry = "0x00000000"
                    span = 0
                    segments = 0
                    needed = 0
                    free = 65536
                    fits = $true
                    region = 65536
                    capacity_probe = "rwx_segment"
                },
                [pscustomobject]@{
                    name = "too_large_app"
                    format = "elf"
                    probe = "load_buffer_too_small"
                    entry = "0x00000000"
                    span = 82176
                    segments = 2
                    needed = 82176
                    free = 0
                    fits = $false
                    region = 65536
                    capacity_probe = "load_buffer_too_small"
                }
            )
            prepare = [pscustomobject]@{
                name = "prepare:argv_app"
                stage = "start"
                code = "ok"
                ready = $true
                argc = 4
            }
            capabilities = [pscustomobject]@{
                console = $true
                time = $true
                display = $true
                input = $true
                storage = $true
                app_exit = $true
                unsupported = $true
            }
            negative_cases = 1..10
            gui_timeline = @(
                [pscustomobject]@{ name = "player_min"; frames = 1; inputs = 1; last_frame_hash = "0xfac53a05"; last_input = "3,5,0" },
                [pscustomobject]@{ name = "received:player_min"; frames = 1; inputs = 1; last_frame_hash = "0xfac53a05"; last_input = "3,5,0" },
                [pscustomobject]@{ name = "packetstream:player_min"; frames = 1; inputs = 1; last_frame_hash = "0xfac53a05"; last_input = "3,5,0" },
                [pscustomobject]@{ name = "store:player_min"; frames = 1; inputs = 1; last_frame_hash = "0xfac53a05"; last_input = "3,5,0" }
            )
        }
        evidence = [pscustomobject]@{
            frame_signature_count = 8
            frame_signature_run_count = 6
            frame_dump_count = 8
            frame_dump_run_count = 6
            frame_ppm_count = 8
            frame_ppm_run_count = 6
            input_trace_event_count = 12
            input_trace_run_count = 6
            storage_trace_event_count = 49
            storage_trace_run_count = 6
        }
    }
    [System.IO.File]::WriteAllText($TempSummary, (($GoodSummary | ConvertTo-Json -Depth 8) + "`n"), [System.Text.UTF8Encoding]::new($false))
    [System.IO.File]::WriteAllText($TempQemuLog, "resident-elf-qemu domain summary comparison ok`n  expected=domain-summary.golden.json`n", [System.Text.UTF8Encoding]::new($false))
    try {
        $ParsedQemu = Read-QemuElfDomainSummary -Path $TempSummary
        $DomainGolden = Read-QemuElfDomainGoldenStatus -LogPath $TempQemuLog
        if ($ParsedQemu.Domain -ne "virtual_m7" -or
            $ParsedQemu.AppModel -ne "format=elf:model=CharmAppApi" -or
            $ParsedQemu.Backend -ne "virtual:virtual_m7:console,time,display,input,storage,app_exit:storage=readonly:afe=unsupported" -or
            $ParsedQemu.Memory -ne "run_base=0x20080000:run_size=65536:stage_cache=16384" -or
            $ParsedQemu.Store -ne "store_v1:entries=14:bytes=87584:media=memory:reads=147:read_bytes=72360:failures=0" -or
            $ParsedQemu.Display -ne "16x16:argb8888:stride=64:frame=1024" -or
            $ParsedQemu.Evidence -ne "frames=8/6:dumps=8/6:ppm=8/6:input=12/6:storage=49/6" -or
            $ParsedQemu.Capacity -ne "large_fit=61696/65536:free=3840:fits=1:probe=ok;too_large=82176/65536:free=0:fits=0:probe=load_buffer_too_small" -or
            $ParsedQemu.Loads -ne "hello_app=entry:0x20080021:span:270:segments:2:fits:1:probe:ok;packetstream:player_min=entry:0x20080001:span:1280:segments:3:fits:1:probe:ok;large_fit_app=entry:0x20080019:span:61696:segments:3:fits:1:probe:ok;bad_elf_magic_app=entry:0x00000000:span:0:segments:0:fits:1:probe:bad_magic;entry_outside_segment_app=entry:0x00000000:span:0:segments:0:fits:1:probe:entry_outside_segment;rwx_segment_app=entry:0x00000000:span:0:segments:0:fits:1:probe:rwx_segment;too_large_app=entry:0x00000000:span:82176:segments:2:fits:0:probe:load_buffer_too_small" -or
            $ParsedQemu.Packetstreams -ne "hello_app=payload:5132:stream:5776:packets:23:crc:0xb3b7bcc5;large_fit_app=payload:5168:stream:5840:packets:24:crc:0xdffdfba1;packetstream_bad_elf_magic_app=payload:64:stream:176:packets:4:crc:0xbd40f3c7;player_min=payload:5168:stream:5840:packets:24:crc:0xba5eb94a;packetstream_crc_mismatch=stage:failed/crc_mismatch:dispatch:22/23:crc:0x7d9c7647->0xb3b7bcc5:read:0:stage_bytes:0" -or
            $ParsedQemu.Sources -ne "hello_app=direct,received,packetstream,store:exit/ok;large_fit_app=direct,received,packetstream,store:exit/ok;player_min=direct,received,packetstream,store:exit/ok;argv_app=direct,store:exit/ok:prepare=start/ok:argc=4;data_app=direct,store:exit/ok;negatives=argv_overflow_app:direct=argv/argv_overflow,abi_mismatch_app:direct=abi/abi_mismatch,bad_elf_magic_app:direct=load/load_failed,packetstream_bad_elf_magic_app:packetstream=load/load_failed,entry_outside_segment_app:direct=load/load_failed,rwx_segment_app:direct=load/load_failed,too_large_app:direct=load/load_failed" -or
            $DomainGolden -ne "ok" -or
            $ParsedQemu.Coverage -ne "runs=39:stages=19:loads=7:packetstreams=5:source_matrix=20:gui_timeline=4:prepare=1:capabilities=7:negative_cases=10" -or
            $ParsedQemu.GuiTimeline -ne 4 -or
            $ParsedQemu.PlayerMin.IndexOf("packetstream:player_min:frames=1,inputs=1", [System.StringComparison]::Ordinal) -lt 0) {
            throw "selftest_failed: qemu summary parse result is unexpected"
        }
        function Test-BadQemuSummary {
            param(
                [scriptblock]$Mutate,
                [string]$Label
            )

            $BadSummary = $GoodSummary | ConvertTo-Json -Depth 8 | ConvertFrom-Json
            & $Mutate $BadSummary
            [System.IO.File]::WriteAllText($TempSummary, (($BadSummary | ConvertTo-Json -Depth 8) + "`n"), [System.Text.UTF8Encoding]::new($false))
            try {
                Read-QemuElfDomainSummary -Path $TempSummary | Out-Null
                throw "selftest_failed: bad qemu summary '$Label' did not fail"
            } catch {
                if ($_.Exception.Message.IndexOf("qemu_elf_summary_invalid", [System.StringComparison]::Ordinal) -lt 0) {
                    throw "selftest_failed: bad qemu summary '$Label' did not report qemu_elf_summary_invalid: $($_.Exception.Message)"
                }
            }
        }

        Test-BadQemuSummary -Label "schema" -Mutate { param($Summary) $Summary.schema = "bad" }
        Test-BadQemuSummary -Label "app_model" -Mutate { param($Summary) $Summary.app_model = "RawJump" }
        Test-BadQemuSummary -Label "backend_contract" -Mutate { param($Summary) $Summary.backend_contract.storage = "writeable" }
        Test-BadQemuSummary -Label "run_region" -Mutate { param($Summary) $Summary.run_region.size = 32768 }
        Test-BadQemuSummary -Label "stage_cache" -Mutate { param($Summary) $Summary.stage_cache.bytes = 8192 }
        Test-BadQemuSummary -Label "store_media" -Mutate { param($Summary) $Summary.store.media.read_failures = 1 }
        Test-BadQemuSummary -Label "evidence_counts" -Mutate { param($Summary) $Summary.evidence.frame_signature_count = 7 }
        Test-BadQemuSummary -Label "capacity" -Mutate { param($Summary) ($Summary.coverage.loads | Where-Object { $_.name -eq "too_large_app" }).fits = $true }
        Test-BadQemuSummary -Label "packetstream_crc_mismatch" -Mutate { param($Summary) ($Summary.coverage.packetstreams | Where-Object { $_.name -eq "packetstream_crc_mismatch" }).read_code = "ok" }
        Test-BadQemuSummary -Label "source_matrix" -Mutate { param($Summary) ($Summary.coverage.source_matrix | Where-Object { $_.name -eq "player_min" }).packetstream.code = "load_failed" }
        Test-BadQemuSummary -Label "gui_timeline" -Mutate { param($Summary) $Summary.coverage.gui_timeline = @($Summary.coverage.gui_timeline | Where-Object { $_.name -ne "store:player_min" }) }
        [System.IO.File]::WriteAllText($TempQemuLog, "resident-elf-qemu domain summary validation ok`n", [System.Text.UTF8Encoding]::new($false))
        try {
            Read-QemuElfDomainGoldenStatus -LogPath $TempQemuLog | Out-Null
            throw "selftest_failed: bad qemu domain golden log validated unexpectedly"
        } catch {
            if ($_.Exception.Message.IndexOf("qemu_elf_summary_invalid", [System.StringComparison]::Ordinal) -lt 0) {
                throw "selftest_failed: bad qemu domain golden log did not report qemu_elf_summary_invalid: $($_.Exception.Message)"
            }
        }
    } finally {
        Remove-Item -LiteralPath $TempSummary -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $TempQemuLog -Force -ErrorAction SilentlyContinue
    }

    Write-Host "[resident-platform-evidence-bundle] selftest ok"
}

function Get-QemuElfSourceMatrixCase {
    param(
        [object[]]$Matrix,
        [string]$Name
    )

    $Matches = @($Matrix | Where-Object { $_.name -eq $Name })
    if ($Matches.Count -ne 1) {
        throw "qemu_elf_summary_invalid: missing source matrix $Name"
    }
    return $Matches[0]
}

function Assert-QemuElfSourceRun {
    param(
        [object]$Case,
        [string]$Path,
        [string]$Stage,
        [string]$Code,
        [object]$Exit = 0
    )

    $Property = $Case.PSObject.Properties[$Path]
    if ($null -eq $Property -or $null -eq $Property.Value) {
        throw "qemu_elf_summary_invalid: missing source matrix $($Case.name).$Path"
    }
    $Record = $Property.Value
    if ($Record.stage -ne $Stage -or $Record.code -ne $Code) {
        throw "qemu_elf_summary_invalid: bad source matrix $($Case.name).$Path"
    }
    if ($null -ne $Exit -and [int]$Record.exit -ne [int]$Exit) {
        throw "qemu_elf_summary_invalid: bad source matrix exit $($Case.name).$Path"
    }
    return ("{0}={1}/{2}" -f $Path, ([string]$Record.stage), ([string]$Record.code))
}

function Assert-QemuElfSourcePrepare {
    param(
        [object]$Case,
        [int]$Argc
    )

    $Property = $Case.PSObject.Properties["prepare"]
    if ($null -eq $Property -or $null -eq $Property.Value) {
        throw "qemu_elf_summary_invalid: missing source matrix $($Case.name).prepare"
    }
    $Record = $Property.Value
    if ($Record.stage -ne "start" -or $Record.code -ne "ok" -or -not [bool]$Record.ready -or [int]$Record.argc -ne $Argc) {
        throw "qemu_elf_summary_invalid: bad source matrix $($Case.name).prepare"
    }
    return ("prepare={0}/{1}:argc={2}" -f ([string]$Record.stage), ([string]$Record.code), ([int]$Record.argc))
}

function Get-QemuElfPacketstreamCase {
    param(
        [object[]]$Packetstreams,
        [string]$Name
    )

    $Matches = @($Packetstreams | Where-Object { $_.name -eq $Name })
    if ($Matches.Count -ne 1) {
        throw "qemu_elf_summary_invalid: missing packetstream $Name"
    }
    return $Matches[0]
}

function Assert-QemuElfPacketstreamSuccess {
    param(
        [object]$Packetstream,
        [int]$Payload
    )

    if ($Packetstream.transport -ne "ok" -or
        $Packetstream.packet -ne "ok" -or
        $Packetstream.receive_stage -ne "launch_ready" -or
        $Packetstream.receive_code -ne "ok" -or
        [int]$Packetstream.payload -ne $Payload -or
        [int]$Packetstream.stream -le [int]$Packetstream.payload -or
        [int]$Packetstream.packets -le 0 -or
        [int]$Packetstream.dispatch -ne [int]$Packetstream.packets -or
        [string]$Packetstream.actual_crc -ne [string]$Packetstream.expected_crc -or
        $Packetstream.read_code -ne "ok" -or
        [int]$Packetstream.read_bytes -ne $Payload -or
        $Packetstream.app_stage_code -ne "ok" -or
        $Packetstream.app_stage_format -ne "elf" -or
        [int]$Packetstream.app_stage_bytes -ne $Payload) {
        throw "qemu_elf_summary_invalid: bad packetstream success $($Packetstream.name)"
    }
    return ("{0}=payload:{1}:stream:{2}:packets:{3}:crc:{4}" -f `
        ([string]$Packetstream.name), `
        ([int]$Packetstream.payload), `
        ([int]$Packetstream.stream), `
        ([int]$Packetstream.packets), `
        ([string]$Packetstream.actual_crc))
}

function Get-QemuElfLoadCase {
    param(
        [object[]]$Loads,
        [string]$Name
    )

    $Matches = @($Loads | Where-Object { $_.name -eq $Name })
    if ($Matches.Count -ne 1) {
        throw "qemu_elf_summary_invalid: missing load $Name"
    }
    return $Matches[0]
}

function Assert-QemuElfLoadSummary {
    param(
        [object]$Load,
        [string]$Probe,
        [string]$Entry,
        [int]$Span,
        [int]$Segments,
        [bool]$Fits
    )

    if ($Load.format -ne "elf" -or
        $Load.probe -ne $Probe -or
        [string]$Load.entry -ne $Entry -or
        [int]$Load.span -ne $Span -or
        [int]$Load.segments -ne $Segments -or
        [int]$Load.needed -ne [int]$Load.span -or
        [int]$Load.region -ne 65536 -or
        [bool]$Load.fits -ne $Fits -or
        $Load.capacity_probe -ne $Probe) {
        throw "qemu_elf_summary_invalid: bad load $($Load.name)"
    }
    $FitToken = if ([bool]$Load.fits) { "1" } else { "0" }
    return ("{0}=entry:{1}:span:{2}:segments:{3}:fits:{4}:probe:{5}" -f `
        ([string]$Load.name), `
        ([string]$Load.entry), `
        ([int]$Load.span), `
        ([int]$Load.segments), `
        $FitToken, `
        ([string]$Load.probe))
}

function Read-QemuElfDomainGoldenStatusFromText {
    param([string]$Text)

    if ([string]::IsNullOrWhiteSpace($Text)) {
        throw "qemu_elf_summary_invalid: missing domain summary golden comparison"
    }
    if ($Text.IndexOf("resident-elf-qemu domain summary comparison ok", [System.StringComparison]::Ordinal) -lt 0) {
        throw "qemu_elf_summary_invalid: missing domain summary golden comparison"
    }
    if ($Text.IndexOf("domain-summary.golden.json", [System.StringComparison]::OrdinalIgnoreCase) -lt 0) {
        throw "qemu_elf_summary_invalid: missing domain summary golden path"
    }
    return "ok"
}

function Read-QemuElfDomainGoldenStatus {
    param([string]$LogPath)

    if ([string]::IsNullOrWhiteSpace($LogPath) -or -not (Test-Path -LiteralPath $LogPath)) {
        return "skipped"
    }
    return (Read-QemuElfDomainGoldenStatusFromText -Text (Get-Content -LiteralPath $LogPath -Raw -Encoding UTF8))
}

function Read-QemuElfDomainGoldenStatusFromLines {
    param([System.Collections.Generic.List[string]]$Lines)

    return (Read-QemuElfDomainGoldenStatusFromText -Text ([string]::Join("`n", $Lines)))
}

function Read-QemuElfDomainSummary {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path -LiteralPath $Path)) {
        return $null
    }

    $Summary = Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($Summary.schema -ne "charm.resident_elf_qemu.domain_summary.v1") {
        throw "qemu_elf_summary_invalid: bad schema: $($Summary.schema)"
    }
    if ($Summary.image_format -ne "elf" -or $Summary.app_model -ne "CharmAppApi") {
        throw "qemu_elf_summary_invalid: bad app model"
    }
    if ($Summary.backend_contract.kind -ne "virtual" -or
        $Summary.backend_contract.runtime_domain -ne "virtual_m7" -or
        $Summary.backend_contract.storage -ne "readonly" -or
        $Summary.backend_contract.afe -ne "unsupported") {
        throw "qemu_elf_summary_invalid: bad backend_contract"
    }
    $BackendCapabilities = @($Summary.backend_contract.capabilities)
    $ExpectedBackendCapabilities = @("console", "time", "display", "input", "storage", "app_exit")
    foreach ($Capability in $ExpectedBackendCapabilities) {
        if (-not ($BackendCapabilities -contains $Capability)) {
            throw "qemu_elf_summary_invalid: missing backend capability $Capability"
        }
    }
    if ($BackendCapabilities.Count -ne $ExpectedBackendCapabilities.Count) {
        throw "qemu_elf_summary_invalid: unexpected backend capability count"
    }
    if ($Summary.run_region.base -ne "0x20080000" -or
        $Summary.run_region.expected -ne "0x20080000" -or
        [int]$Summary.run_region.size -ne 65536) {
        throw "qemu_elf_summary_invalid: bad run_region"
    }
    if ([int]$Summary.stage_cache.bytes -ne 16384) {
        throw "qemu_elf_summary_invalid: bad stage_cache"
    }
    if ($Summary.store.format -ne "store_v1" -or
        [int]$Summary.store.entries -ne 14 -or
        [int]$Summary.store.bytes -le 0 -or
        $Summary.store.media.kind -ne "memory" -or
        [int]$Summary.store.media.bytes -ne [int]$Summary.store.bytes -or
        [int]$Summary.store.media.read_calls -le 0 -or
        [int]$Summary.store.media.read_bytes -le 0 -or
        [int]$Summary.store.media.read_failures -ne 0) {
        throw "qemu_elf_summary_invalid: bad store media"
    }
    if ([int]$Summary.evidence.frame_signature_count -ne 8 -or
        [int]$Summary.evidence.frame_signature_run_count -ne 6 -or
        [int]$Summary.evidence.frame_dump_count -ne 8 -or
        [int]$Summary.evidence.frame_dump_run_count -ne 6 -or
        [int]$Summary.evidence.frame_ppm_count -ne 8 -or
        [int]$Summary.evidence.frame_ppm_run_count -ne 6 -or
        [int]$Summary.evidence.input_trace_event_count -ne 12 -or
        [int]$Summary.evidence.input_trace_run_count -ne 6 -or
        [int]$Summary.evidence.storage_trace_event_count -ne 49 -or
        [int]$Summary.evidence.storage_trace_run_count -ne 6) {
        throw "qemu_elf_summary_invalid: bad evidence counts"
    }
    $LargeFit = @($Summary.coverage.loads | Where-Object { $_.name -eq "large_fit_app" })
    $TooLarge = @($Summary.coverage.loads | Where-Object { $_.name -eq "too_large_app" })
    if ($LargeFit.Count -ne 1 -or $TooLarge.Count -ne 1) {
        throw "qemu_elf_summary_invalid: missing capacity loads"
    }
    $LargeFitLoad = $LargeFit[0]
    $TooLargeLoad = $TooLarge[0]
    if ($LargeFitLoad.probe -ne "ok" -or
        [int]$LargeFitLoad.span -lt 60000 -or
        [int]$LargeFitLoad.needed -ne [int]$LargeFitLoad.span -or
        [int]$LargeFitLoad.region -ne 65536 -or
        [int]$LargeFitLoad.free -ne 3840 -or
        -not [bool]$LargeFitLoad.fits -or
        $LargeFitLoad.capacity_probe -ne "ok") {
        throw "qemu_elf_summary_invalid: bad large-fit capacity"
    }
    if ($TooLargeLoad.probe -ne "load_buffer_too_small" -or
        [int]$TooLargeLoad.span -le 65536 -or
        [int]$TooLargeLoad.needed -ne [int]$TooLargeLoad.span -or
        [int]$TooLargeLoad.region -ne 65536 -or
        [int]$TooLargeLoad.free -ne 0 -or
        [bool]$TooLargeLoad.fits -or
        $TooLargeLoad.capacity_probe -ne "load_buffer_too_small") {
        throw "qemu_elf_summary_invalid: bad too-large capacity"
    }
    $LargeFitFits = if ([bool]$LargeFitLoad.fits) { "1" } else { "0" }
    $TooLargeFits = if ([bool]$TooLargeLoad.fits) { "1" } else { "0" }
    $LoadSummaryParts = @()
    foreach ($Expected in @(
            [pscustomobject]@{ name = "hello_app"; probe = "ok"; entry = "0x20080021"; span = 270; segments = 2; fits = $true },
            [pscustomobject]@{ name = "packetstream:player_min"; probe = "ok"; entry = "0x20080001"; span = 1280; segments = 3; fits = $true },
            [pscustomobject]@{ name = "large_fit_app"; probe = "ok"; entry = "0x20080019"; span = 61696; segments = 3; fits = $true },
            [pscustomobject]@{ name = "bad_elf_magic_app"; probe = "bad_magic"; entry = "0x00000000"; span = 0; segments = 0; fits = $true },
            [pscustomobject]@{ name = "entry_outside_segment_app"; probe = "entry_outside_segment"; entry = "0x00000000"; span = 0; segments = 0; fits = $true },
            [pscustomobject]@{ name = "rwx_segment_app"; probe = "rwx_segment"; entry = "0x00000000"; span = 0; segments = 0; fits = $true },
            [pscustomobject]@{ name = "too_large_app"; probe = "load_buffer_too_small"; entry = "0x00000000"; span = 82176; segments = 2; fits = $false }
        )) {
        $Load = Get-QemuElfLoadCase -Loads @($Summary.coverage.loads) -Name $Expected.name
        $LoadSummaryParts += (Assert-QemuElfLoadSummary `
                -Load $Load `
                -Probe $Expected.probe `
                -Entry $Expected.entry `
                -Span $Expected.span `
                -Segments $Expected.segments `
                -Fits $Expected.fits)
    }
    $Packetstreams = @($Summary.coverage.packetstreams)
    if ($Packetstreams.Count -ne 5) {
        throw "qemu_elf_summary_invalid: bad packetstream count"
    }
    $PacketstreamSummaryParts = @()
    foreach ($Expected in @(
            [pscustomobject]@{ name = "hello_app"; payload = 5132 },
            [pscustomobject]@{ name = "large_fit_app"; payload = 5168 },
            [pscustomobject]@{ name = "packetstream_bad_elf_magic_app"; payload = 64 },
            [pscustomobject]@{ name = "player_min"; payload = 5168 }
        )) {
        $Packetstream = Get-QemuElfPacketstreamCase -Packetstreams $Packetstreams -Name $Expected.name
        $PacketstreamSummaryParts += (Assert-QemuElfPacketstreamSuccess -Packetstream $Packetstream -Payload $Expected.payload)
    }
    $CrcMismatch = Get-QemuElfPacketstreamCase -Packetstreams $Packetstreams -Name "packetstream_crc_mismatch"
    if ($CrcMismatch.transport -ne "packet_failed" -or
        $CrcMismatch.packet -ne "receive_failed" -or
        $CrcMismatch.receive_stage -ne "failed" -or
        $CrcMismatch.receive_code -ne "crc_mismatch" -or
        [int]$CrcMismatch.payload -ne 5132 -or
        [int]$CrcMismatch.stream -le [int]$CrcMismatch.payload -or
        [int]$CrcMismatch.packets -ne 23 -or
        [int]$CrcMismatch.dispatch -ne 22 -or
        [string]$CrcMismatch.actual_crc -eq [string]$CrcMismatch.expected_crc -or
        -not [string]::IsNullOrWhiteSpace([string]$CrcMismatch.read_code) -or
        [int]$CrcMismatch.read_bytes -ne 0 -or
        -not [string]::IsNullOrWhiteSpace([string]$CrcMismatch.app_stage_code) -or
        [int]$CrcMismatch.app_stage_bytes -ne 0) {
        throw "qemu_elf_summary_invalid: bad packetstream crc mismatch"
    }
    $PacketstreamSummaryParts += ("packetstream_crc_mismatch=stage:{0}/{1}:dispatch:{2}/{3}:crc:{4}->{5}:read:{6}:stage_bytes:{7}" -f `
        ([string]$CrcMismatch.receive_stage), `
        ([string]$CrcMismatch.receive_code), `
        ([int]$CrcMismatch.dispatch), `
        ([int]$CrcMismatch.packets), `
        ([string]$CrcMismatch.actual_crc), `
        ([string]$CrcMismatch.expected_crc), `
        ([int]$CrcMismatch.read_bytes), `
        ([int]$CrcMismatch.app_stage_bytes))
    $SourceMatrix = @($Summary.coverage.source_matrix)
    if ($SourceMatrix.Count -ne 20) {
        throw "qemu_elf_summary_invalid: bad source matrix count"
    }
    $CoverageStages = @($Summary.coverage.stages).Count
    $CoveragePrepare = if ($null -ne $Summary.coverage.prepare) { 1 } else { 0 }
    $CoverageCapabilities = @($Summary.coverage.capabilities.PSObject.Properties | Where-Object { [bool]$_.Value }).Count
    $CoverageNegativeCases = @($Summary.coverage.negative_cases).Count
    if ($CoverageStages -ne 19 -or
        $CoveragePrepare -ne 1 -or
        $CoverageCapabilities -ne 7 -or
        $CoverageNegativeCases -ne 10) {
        throw "qemu_elf_summary_invalid: bad coverage counts"
    }
    $SourceSummaryParts = @()
    foreach ($Name in @("hello_app", "large_fit_app", "player_min")) {
        $Case = Get-QemuElfSourceMatrixCase -Matrix $SourceMatrix -Name $Name
        Assert-QemuElfSourceRun -Case $Case -Path "direct" -Stage "exit" -Code "ok" | Out-Null
        Assert-QemuElfSourceRun -Case $Case -Path "received" -Stage "exit" -Code "ok" | Out-Null
        Assert-QemuElfSourceRun -Case $Case -Path "packetstream" -Stage "exit" -Code "ok" | Out-Null
        Assert-QemuElfSourceRun -Case $Case -Path "store" -Stage "exit" -Code "ok" | Out-Null
        $SourceSummaryParts += ("{0}=direct,received,packetstream,store:exit/ok" -f $Name)
    }
    $ArgvCase = Get-QemuElfSourceMatrixCase -Matrix $SourceMatrix -Name "argv_app"
    Assert-QemuElfSourceRun -Case $ArgvCase -Path "direct" -Stage "exit" -Code "ok" | Out-Null
    Assert-QemuElfSourceRun -Case $ArgvCase -Path "store" -Stage "exit" -Code "ok" | Out-Null
    Assert-QemuElfSourcePrepare -Case $ArgvCase -Argc 4 | Out-Null
    $SourceSummaryParts += "argv_app=direct,store:exit/ok:prepare=start/ok:argc=4"
    $DataCase = Get-QemuElfSourceMatrixCase -Matrix $SourceMatrix -Name "data_app"
    Assert-QemuElfSourceRun -Case $DataCase -Path "direct" -Stage "exit" -Code "ok" | Out-Null
    Assert-QemuElfSourceRun -Case $DataCase -Path "store" -Stage "exit" -Code "ok" | Out-Null
    $SourceSummaryParts += "data_app=direct,store:exit/ok"
    $NegativeSummaryParts = @()
    foreach ($Negative in @(
            [pscustomobject]@{ name = "argv_overflow_app"; path = "direct"; stage = "argv"; code = "argv_overflow" },
            [pscustomobject]@{ name = "abi_mismatch_app"; path = "direct"; stage = "abi"; code = "abi_mismatch" },
            [pscustomobject]@{ name = "bad_elf_magic_app"; path = "direct"; stage = "load"; code = "load_failed" },
            [pscustomobject]@{ name = "packetstream_bad_elf_magic_app"; path = "packetstream"; stage = "load"; code = "load_failed" },
            [pscustomobject]@{ name = "entry_outside_segment_app"; path = "direct"; stage = "load"; code = "load_failed" },
            [pscustomobject]@{ name = "rwx_segment_app"; path = "direct"; stage = "load"; code = "load_failed" },
            [pscustomobject]@{ name = "too_large_app"; path = "direct"; stage = "load"; code = "load_failed" }
        )) {
        $Case = Get-QemuElfSourceMatrixCase -Matrix $SourceMatrix -Name $Negative.name
        Assert-QemuElfSourceRun -Case $Case -Path $Negative.path -Stage $Negative.stage -Code $Negative.code | Out-Null
        $NegativeSummaryParts += ("{0}:{1}={2}/{3}" -f $Negative.name, $Negative.path, $Negative.stage, $Negative.code)
    }
    $SourceSummaryParts += ("negatives={0}" -f ($NegativeSummaryParts -join ","))
    $Timeline = @($Summary.coverage.gui_timeline)
    $PlayerMinPaths = @("player_min", "received:player_min", "packetstream:player_min", "store:player_min")
    $PlayerMinSummary = @()
    foreach ($Name in $PlayerMinPaths) {
        $Matches = @($Timeline | Where-Object { $_.name -eq $Name })
        if ($Matches.Count -ne 1) {
            throw "qemu_elf_summary_invalid: missing gui timeline $Name"
        }
        $Entry = $Matches[0]
        $PlayerMinSummary += ("{0}:frames={1},inputs={2},hash={3},input={4}" -f `
            $Name, `
            ([int]$Entry.frames), `
            ([int]$Entry.inputs), `
            ([string]$Entry.last_frame_hash), `
            ([string]$Entry.last_input))
    }

    return [pscustomobject]@{
        Domain = [string]$Summary.domain
        Machine = [string]$Summary.machine
        Cpu = [string]$Summary.cpu
        AppModel = ("format={0}:model={1}" -f `
            ([string]$Summary.image_format), `
            ([string]$Summary.app_model))
        Backend = ("{0}:{1}:{2}:storage={3}:afe={4}" -f `
            ([string]$Summary.backend_contract.kind), `
            ([string]$Summary.backend_contract.runtime_domain), `
            ($BackendCapabilities -join ","), `
            ([string]$Summary.backend_contract.storage), `
            ([string]$Summary.backend_contract.afe))
        Memory = ("run_base={0}:run_size={1}:stage_cache={2}" -f `
            ([string]$Summary.run_region.base), `
            ([int]$Summary.run_region.size), `
            ([int]$Summary.stage_cache.bytes))
        Store = ("{0}:entries={1}:bytes={2}:media={3}:reads={4}:read_bytes={5}:failures={6}" -f `
            ([string]$Summary.store.format), `
            ([int]$Summary.store.entries), `
            ([int]$Summary.store.bytes), `
            ([string]$Summary.store.media.kind), `
            ([int]$Summary.store.media.read_calls), `
            ([int]$Summary.store.media.read_bytes), `
            ([int]$Summary.store.media.read_failures))
        Evidence = ("frames={0}/{1}:dumps={2}/{3}:ppm={4}/{5}:input={6}/{7}:storage={8}/{9}" -f `
            ([int]$Summary.evidence.frame_signature_count), `
            ([int]$Summary.evidence.frame_signature_run_count), `
            ([int]$Summary.evidence.frame_dump_count), `
            ([int]$Summary.evidence.frame_dump_run_count), `
            ([int]$Summary.evidence.frame_ppm_count), `
            ([int]$Summary.evidence.frame_ppm_run_count), `
            ([int]$Summary.evidence.input_trace_event_count), `
            ([int]$Summary.evidence.input_trace_run_count), `
            ([int]$Summary.evidence.storage_trace_event_count), `
            ([int]$Summary.evidence.storage_trace_run_count))
        Capacity = ("large_fit={0}/{1}:free={2}:fits={3}:probe={4};too_large={5}/{6}:free={7}:fits={8}:probe={9}" -f `
            ([int]$LargeFitLoad.needed), `
            ([int]$LargeFitLoad.region), `
            ([int]$LargeFitLoad.free), `
            $LargeFitFits, `
            ([string]$LargeFitLoad.capacity_probe), `
            ([int]$TooLargeLoad.needed), `
            ([int]$TooLargeLoad.region), `
            ([int]$TooLargeLoad.free), `
            $TooLargeFits, `
            ([string]$TooLargeLoad.capacity_probe))
        Loads = ($LoadSummaryParts -join ";")
        Packetstreams = ($PacketstreamSummaryParts -join ";")
        Runs = @($Summary.coverage.runs).Count
        Stages = $CoverageStages
        LoadCount = @($Summary.coverage.loads).Count
        PacketstreamCount = $Packetstreams.Count
        SourceMatrix = @($Summary.coverage.source_matrix).Count
        Sources = ($SourceSummaryParts -join ";")
        GuiTimeline = $Timeline.Count
        Prepare = $CoveragePrepare
        Capabilities = $CoverageCapabilities
        NegativeCases = $CoverageNegativeCases
        Coverage = ("runs={0}:stages={1}:loads={2}:packetstreams={3}:source_matrix={4}:gui_timeline={5}:prepare={6}:capabilities={7}:negative_cases={8}" -f `
            @($Summary.coverage.runs).Count, `
            $CoverageStages, `
            @($Summary.coverage.loads).Count, `
            $Packetstreams.Count, `
            @($Summary.coverage.source_matrix).Count, `
            $Timeline.Count, `
            $CoveragePrepare, `
            $CoverageCapabilities, `
            $CoverageNegativeCases)
        Display = ("{0}x{1}:{2}:stride={3}:frame={4}" -f `
            ([int]$Summary.display.width), `
            ([int]$Summary.display.height), `
            ([string]$Summary.display.format), `
            ([int]$Summary.display.stride_bytes), `
            ([int]$Summary.display.frame_bytes))
        PlayerMin = ($PlayerMinSummary -join ";")
    }
}

function Write-QemuElfSummaryLines {
    param(
        [System.Collections.Generic.List[string]]$Lines,
        [object]$QemuElfSummary,
        [string]$QemuElfDomainGolden = "skipped"
    )

    if ($null -eq $QemuElfSummary) {
        Write-BundleLine -Lines $Lines -Text "qemu_elf_domain=skipped"
        Write-BundleLine -Lines $Lines -Text "qemu_elf_app_model=skipped"
        Write-BundleLine -Lines $Lines -Text "qemu_elf_backend=skipped"
        Write-BundleLine -Lines $Lines -Text "qemu_elf_memory=skipped"
        Write-BundleLine -Lines $Lines -Text "qemu_elf_store=skipped"
        Write-BundleLine -Lines $Lines -Text "qemu_elf_display=skipped"
        Write-BundleLine -Lines $Lines -Text "qemu_elf_evidence=skipped"
        Write-BundleLine -Lines $Lines -Text "qemu_elf_capacity=skipped"
        Write-BundleLine -Lines $Lines -Text "qemu_elf_loads=skipped"
        Write-BundleLine -Lines $Lines -Text "qemu_elf_packetstreams=skipped"
        Write-BundleLine -Lines $Lines -Text "qemu_elf_domain_golden=skipped"
        Write-BundleLine -Lines $Lines -Text "qemu_elf_sources=skipped"
        Write-BundleLine -Lines $Lines -Text "qemu_elf_coverage=skipped"
        Write-BundleLine -Lines $Lines -Text "qemu_elf_player_min_gui=skipped"
        return
    }

    Write-BundleLine -Lines $Lines -Text ("qemu_elf_domain={0}/{1}/{2}" -f `
        $QemuElfSummary.Domain, `
        $QemuElfSummary.Machine, `
        $QemuElfSummary.Cpu)
    Write-BundleLine -Lines $Lines -Text ("qemu_elf_app_model={0}" -f $QemuElfSummary.AppModel)
    Write-BundleLine -Lines $Lines -Text ("qemu_elf_backend={0}" -f $QemuElfSummary.Backend)
    Write-BundleLine -Lines $Lines -Text ("qemu_elf_memory={0}" -f $QemuElfSummary.Memory)
    Write-BundleLine -Lines $Lines -Text ("qemu_elf_store={0}" -f $QemuElfSummary.Store)
    Write-BundleLine -Lines $Lines -Text ("qemu_elf_display={0}" -f $QemuElfSummary.Display)
    Write-BundleLine -Lines $Lines -Text ("qemu_elf_evidence={0}" -f $QemuElfSummary.Evidence)
    Write-BundleLine -Lines $Lines -Text ("qemu_elf_capacity={0}" -f $QemuElfSummary.Capacity)
    Write-BundleLine -Lines $Lines -Text ("qemu_elf_loads={0}" -f $QemuElfSummary.Loads)
    Write-BundleLine -Lines $Lines -Text ("qemu_elf_packetstreams={0}" -f $QemuElfSummary.Packetstreams)
    Write-BundleLine -Lines $Lines -Text ("qemu_elf_domain_golden={0}" -f $QemuElfDomainGolden)
    Write-BundleLine -Lines $Lines -Text ("qemu_elf_sources={0}" -f $QemuElfSummary.Sources)
    Write-BundleLine -Lines $Lines -Text ("qemu_elf_coverage runs={0} stages={1} loads={2} packetstreams={3} source_matrix={4} gui_timeline={5} prepare={6} capabilities={7} negative_cases={8}" -f `
        $QemuElfSummary.Runs, `
        $QemuElfSummary.Stages, `
        $QemuElfSummary.LoadCount, `
        $QemuElfSummary.PacketstreamCount, `
        $QemuElfSummary.SourceMatrix, `
        $QemuElfSummary.GuiTimeline, `
        $QemuElfSummary.Prepare, `
        $QemuElfSummary.Capabilities, `
        $QemuElfSummary.NegativeCases)
    Write-BundleLine -Lines $Lines -Text ("qemu_elf_player_min_gui={0}" -f $QemuElfSummary.PlayerMin)
}

function Write-Summary {
    param(
        [System.Collections.Generic.List[string]]$Lines,
        [object]$Manifest,
        [string]$InspectStatus,
        [object[]]$SmokeResults,
        [string]$QemuElfStatus,
        [string]$QemuElfMode,
        [string]$QemuElfLog,
        [string]$QemuElfDomainSummary,
        [string]$QemuElfFramePpm,
        [object]$QemuElfSummary,
        [string]$QemuElfDomainGolden,
        [string]$FirmwareSize,
        [string]$BoardMatrixLog,
        [string]$InstalledStoreMatrixLog
    )

    Write-BundleLine -Lines $Lines -Text "== summary =="
    Write-BundleLine -Lines $Lines -Text "manifest=$($Manifest.Path)"
    Write-BundleLine -Lines $Lines -Text ("store size={0} crc={1} packetstream_size={2}" -f `
        $Manifest.StoreSize, `
        (Format-CharmResidentHex32 $Manifest.StoreCrc32), `
        $Manifest.StorePacketStreamSize)
    Write-BundleLine -Lines $Lines -Text "inspect=$InspectStatus"
    foreach ($Result in $SmokeResults) {
        Write-BundleLine -Lines $Lines -Text "smoke $($Result.Name)=$($Result.Status)"
    }
    Write-BundleLine -Lines $Lines -Text "qemu_elf=$QemuElfStatus"
    Write-BundleLine -Lines $Lines -Text "qemu_elf_mode=$QemuElfMode"
    if (-not [string]::IsNullOrWhiteSpace($QemuElfLog)) {
        Write-BundleLine -Lines $Lines -Text "qemu_elf_log=$QemuElfLog"
    } else {
        Write-BundleLine -Lines $Lines -Text "qemu_elf_log=skipped"
    }
    if (-not [string]::IsNullOrWhiteSpace($QemuElfDomainSummary)) {
        Write-BundleLine -Lines $Lines -Text "qemu_elf_domain_summary=$QemuElfDomainSummary"
    } else {
        Write-BundleLine -Lines $Lines -Text "qemu_elf_domain_summary=skipped"
    }
    if (-not [string]::IsNullOrWhiteSpace($QemuElfFramePpm)) {
        Write-BundleLine -Lines $Lines -Text "qemu_elf_frame_ppm=$QemuElfFramePpm"
    } else {
        Write-BundleLine -Lines $Lines -Text "qemu_elf_frame_ppm=skipped"
    }
    Write-QemuElfSummaryLines -Lines $Lines -QemuElfSummary $QemuElfSummary -QemuElfDomainGolden $QemuElfDomainGolden
    Write-BundleLine -Lines $Lines -Text "h747_lab_dev_loader.bin=$FirmwareSize"
    if (-not [string]::IsNullOrWhiteSpace($BoardMatrixLog)) {
        Write-BundleLine -Lines $Lines -Text "board_matrix_log=$BoardMatrixLog"
    } else {
        Write-BundleLine -Lines $Lines -Text "board_matrix_log=skipped"
    }
    if (-not [string]::IsNullOrWhiteSpace($InstalledStoreMatrixLog)) {
        Write-BundleLine -Lines $Lines -Text "installed_store_matrix_log=$InstalledStoreMatrixLog"
    } else {
        Write-BundleLine -Lines $Lines -Text "installed_store_matrix_log=skipped"
    }
    Write-BundleLine -Lines $Lines -Text "resident-platform-evidence-bundle: ok"
}

if ($SelfTest) {
    Invoke-SelfTest
    exit 0
}

$RepoRoot = Get-RepoRoot
$H747Root = Resolve-Path (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($ArtifactManifest)) {
    $ArtifactManifest = Get-DefaultManifest
}
if ([string]::IsNullOrWhiteSpace($Log)) {
    $Log = Get-DefaultBundleLog
}
$MediaList = Get-MediaList -RawMedia $Media
if ($QemuElfValidateOnly -and -not $QemuElf) {
    throw "invalid_argument: -QemuElfValidateOnly requires -QemuElf"
}
$QemuElfMode = "skipped"
if ($QemuElf) {
    if ($QemuElfValidateOnly) {
        $QemuElfMode = "validate_existing_evidence"
    } else {
        $QemuElfMode = "build_and_run"
    }
}

$ArtifactManifest = [System.IO.Path]::GetFullPath($ArtifactManifest)
$Log = [System.IO.Path]::GetFullPath($Log)
$LogDir = Split-Path -Parent $Log

$InspectSource = Join-Path $RepoRoot "Examples\system\resident_platform_inspect_tool"
$InspectBuild = Get-CmakeBuildDir -SourceDir $InspectSource -BuildName "cmake-build-resident-platform-inspect-tool"
$InspectSmokeSource = Join-Path $RepoRoot "Examples\system\resident_platform_inspect_smoke"
$InspectSmokeBuild = Get-CmakeBuildDir -SourceDir $InspectSmokeSource -BuildName "cmake-build-resident-platform-inspect-smoke"
$ArtifactSmokeSource = Join-Path $RepoRoot "Examples\system\resident_platform_artifact_smoke"
$ArtifactSmokeBuild = Get-CmakeBuildDir -SourceDir $ArtifactSmokeSource -BuildName "cmake-build-resident-platform-artifact-smoke"
$PacketSmokeSource = Join-Path $RepoRoot "Examples\system\dev_loader_packet_stream_smoke"
$PacketSmokeBuild = Get-CmakeBuildDir -SourceDir $PacketSmokeSource -BuildName "cmake-build-dev-loader-packet-stream-smoke"
$StoreHandoffSource = Join-Path $RepoRoot "Examples\system\dev_loader_store_install_handoff_smoke"
$StoreHandoffBuild = Get-CmakeBuildDir -SourceDir $StoreHandoffSource -BuildName "cmake-build-dev-loader-store-install-handoff-smoke"
$ModuleXSmokeSource = Join-Path $RepoRoot "Examples\system\app_abi_modulex_smoke"
$ModuleXSmokeBuild = Get-CmakeBuildDir -SourceDir $ModuleXSmokeSource -BuildName "cmake-build-app-abi-modulex-smoke"
$QemuElfScript = Join-Path $RepoRoot "Examples\system\run-resident-elf-qemu-smoke.ps1"
$QemuElfLog = Join-Path $RepoRoot "Examples\system\resident_elf_qemu_smoke\qemu-ci.log"
$QemuElfDomainSummary = Join-Path $RepoRoot "Examples\system\resident_elf_qemu_smoke\domain-summary.json"
$QemuElfFramePpm = Join-Path $RepoRoot "Examples\system\resident_elf_qemu_smoke\frame-ppm"

$BoardMatrixLog = Join-Path $H747Root "cmake-build-h747-lab-debug\resident_platform_board_matrix_from_bundle.log"
$InstalledStoreMatrixLog = Join-Path $H747Root "cmake-build-h747-lab-debug\resident_platform_installed_store_matrix_from_bundle.log"

if ($DryRun) {
    Write-Host "resident-platform-evidence-bundle dry-run"
    Write-Host "repo=$RepoRoot"
    Write-Host "artifact_manifest=$ArtifactManifest"
    Write-Host "log=$Log"
    Write-Host "board_matrix=$($BoardMatrix.IsPresent)"
    Write-Host "installed_store_matrix=$($InstalledStoreMatrix.IsPresent)"
    Write-Host "qemu_elf=$($QemuElf.IsPresent)"
    Write-Host "qemu_elf_validate_only=$($QemuElfValidateOnly.IsPresent)"
    Write-Host "qemu_elf_mode=$QemuElfMode"
    Write-Host "skip_h747_build=$($SkipH747Build.IsPresent)"
    Write-Host "inspect_source=$InspectSource"
    Write-Host "host_smokes=resident_platform_inspect_smoke,resident_platform_artifact_smoke,dev_loader_packet_stream_smoke,dev_loader_store_install_handoff_smoke,app_abi_modulex_smoke"
    if ($SkipH747Build) {
        Write-Host "h747_build=skipped"
    } else {
        Write-Host "h747_build=build-h747-lab-dev-loader-debug"
    }
    if ($QemuElf) {
        Write-Host "qemu_elf_script=$QemuElfScript"
        Write-Host "qemu_elf_log=$QemuElfLog"
        Write-Host "qemu_elf_domain_summary=$QemuElfDomainSummary"
        Write-Host "qemu_elf_frame_ppm=$QemuElfFramePpm"
    }
    if ($BoardMatrix) {
        Write-Host "board_matrix_log=$BoardMatrixLog"
        Write-Host "board_matrix_media=$($MediaList -join ',') repeat=$RepeatPerMedia write_chunk=$WriteChunkSize delay_ms=$InterChunkDelayMs"
    }
    if ($InstalledStoreMatrix) {
        Write-Host "installed_store_matrix_log=$InstalledStoreMatrixLog"
        Write-Host "installed_store_matrix_media=$($MediaList -join ',') repeat=$RepeatPerMedia"
    }
    exit 0
}

$Lines = New-Object System.Collections.Generic.List[string]

try {
    if (-not (Test-Path -LiteralPath $LogDir)) {
        New-Item -ItemType Directory -Path $LogDir | Out-Null
    }

    Write-BundleLine -Lines $Lines -Text "resident-platform-evidence-bundle started $(Get-Date -Format o)"
    Write-BundleLine -Lines $Lines -Text "repo=$RepoRoot"
    Write-BundleLine -Lines $Lines -Text "artifact_manifest=$ArtifactManifest"

    Invoke-Logged -Lines $Lines -Label "build resident platform artifacts" -FilePath "powershell" -Arguments @(
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        (Join-Path $RepoRoot "Examples\app_abi\elf_samples\build_resident_platform_artifacts.ps1"),
        "-Validate"
    )

    $Manifest = Read-CharmResidentArtifactManifest -Path $ArtifactManifest

    Invoke-CmakeProject -Lines $Lines -Name "resident-platform-inspect" -SourceDir $InspectSource -BuildDir $InspectBuild
    $InspectExe = Get-ExecutablePath -BuildDir $InspectBuild -ExeName "resident-platform-inspect.exe"
    Invoke-Logged -Lines $Lines -Label "resident-platform-inspect" -FilePath $InspectExe -Arguments @(
        $ArtifactManifest,
        "--strict"
    )
    $InspectStatus = "ok"

    $SmokeResults = @()
    $Smokes = @(
        @{ Name = "resident_platform_inspect_smoke"; Source = $InspectSmokeSource; Build = $InspectSmokeBuild },
        @{ Name = "resident_platform_artifact_smoke"; Source = $ArtifactSmokeSource; Build = $ArtifactSmokeBuild },
        @{ Name = "dev_loader_packet_stream_smoke"; Source = $PacketSmokeSource; Build = $PacketSmokeBuild },
        @{ Name = "dev_loader_store_install_handoff_smoke"; Source = $StoreHandoffSource; Build = $StoreHandoffBuild },
        @{ Name = "app_abi_modulex_smoke"; Source = $ModuleXSmokeSource; Build = $ModuleXSmokeBuild }
    )
    foreach ($Smoke in $Smokes) {
        Invoke-CmakeProject -Lines $Lines -Name $Smoke.Name -SourceDir $Smoke.Source -BuildDir $Smoke.Build
        Invoke-Ctest -Lines $Lines -Name $Smoke.Name -BuildDir $Smoke.Build
        $SmokeResults += [pscustomobject]@{
            Name = $Smoke.Name
            Status = "pass"
        }
    }

    $QemuElfStatus = "skipped"
    $QemuElfLogResolved = ""
    $QemuElfDomainSummaryResolved = ""
    $QemuElfFramePpmResolved = ""
    $QemuElfSummary = $null
    $QemuElfDomainGolden = "skipped"
    if ($QemuElf) {
        if ($QemuElfValidateOnly) {
            Invoke-Logged -Lines $Lines -Label "resident ELF QEMU evidence validate" -FilePath "powershell" -Arguments @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $QemuElfScript,
                "-ValidateEvidenceBundle"
            )
        } else {
            Invoke-Logged -Lines $Lines -Label "resident ELF QEMU smoke selftest" -FilePath "powershell" -Arguments @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $QemuElfScript,
                "-SelfTest"
            )
            Invoke-Logged -Lines $Lines -Label "resident ELF QEMU smoke" -FilePath "powershell" -Arguments @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $QemuElfScript
            )
        }
        $QemuElfStatus = "pass"
        if (Test-Path -LiteralPath $QemuElfLog) {
            $QemuElfLogResolved = (Resolve-Path -LiteralPath $QemuElfLog).Path
        }
        if (Test-Path -LiteralPath $QemuElfDomainSummary) {
            $QemuElfDomainSummaryResolved = (Resolve-Path -LiteralPath $QemuElfDomainSummary).Path
        } else {
            throw "qemu_elf_summary_missing: $QemuElfDomainSummary"
        }
        if (Test-Path -LiteralPath $QemuElfFramePpm) {
            $QemuElfFramePpmResolved = (Resolve-Path -LiteralPath $QemuElfFramePpm).Path
        }
        $QemuElfSummary = Read-QemuElfDomainSummary -Path $QemuElfDomainSummaryResolved
        $QemuElfDomainGolden = Read-QemuElfDomainGoldenStatusFromLines -Lines $Lines
        Write-BundleLine -Lines $Lines -Text "== resident ELF QEMU summary =="
        Write-QemuElfSummaryLines -Lines $Lines -QemuElfSummary $QemuElfSummary -QemuElfDomainGolden $QemuElfDomainGolden
    }

    $FirmwareSize = "skipped"
    if ($SkipH747Build) {
        Write-BundleLine -Lines $Lines -Text "== h747 dev_loader build-only =="
        Write-BundleLine -Lines $Lines -Text "skipped by -SkipH747Build"
    } else {
        Invoke-Logged -Lines $Lines -Label "h747 dev_loader build-only" -FilePath "cmake" -Arguments @(
            "--build",
            "--preset",
            "build-h747-lab-dev-loader-debug",
            "--",
            "-j1"
        ) -WorkingDirectory $H747Root

        $FirmwarePath = Join-Path $H747Root "cmake-build-h747-lab-debug\h747_lab_dev_loader.bin"
        $FirmwareSize = [string](Get-BinSize -Path $FirmwarePath)
    }

    $BoardLogResolved = ""
    if ($BoardMatrix) {
        $BoardArgs = New-Object System.Collections.Generic.List[string]
        foreach ($Argument in @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            (Join-Path $PSScriptRoot "capture-dev-loader-usb-cdc-appstore-platform-matrix-smoke.ps1"),
            "-ArtifactManifest",
            $ArtifactManifest,
            "-ControlPort",
            $ControlPort,
            "-UsbPort",
            $UsbPort,
            "-RepeatPerMedia",
            ([string]$RepeatPerMedia),
            "-WriteChunkSize",
            ([string]$WriteChunkSize),
            "-InterChunkDelayMs",
            ([string]$InterChunkDelayMs),
            "-Media"
        )) {
            [void]$BoardArgs.Add($Argument)
        }
        foreach ($Name in $MediaList) {
            [void]$BoardArgs.Add($Name)
        }
        [void]$BoardArgs.Add("-Log")
        [void]$BoardArgs.Add($BoardMatrixLog)

        Invoke-Logged -Lines $Lines -Label "board matrix" -FilePath "powershell" -Arguments @(
            $BoardArgs.ToArray()
        )
        $BoardLogResolved = [System.IO.Path]::GetFullPath($BoardMatrixLog)
    }

    $InstalledStoreLogResolved = ""
    if ($InstalledStoreMatrix) {
        $InstalledArgs = New-Object System.Collections.Generic.List[string]
        foreach ($Argument in @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            (Join-Path $PSScriptRoot "capture-dev-loader-installed-store-matrix-smoke.ps1"),
            "-ArtifactManifest",
            $ArtifactManifest,
            "-ControlPort",
            $ControlPort,
            "-RepeatPerMedia",
            ([string]$RepeatPerMedia),
            "-Media"
        )) {
            [void]$InstalledArgs.Add($Argument)
        }
        foreach ($Name in $MediaList) {
            [void]$InstalledArgs.Add($Name)
        }
        [void]$InstalledArgs.Add("-Log")
        [void]$InstalledArgs.Add($InstalledStoreMatrixLog)

        Invoke-Logged -Lines $Lines -Label "installed store matrix" -FilePath "powershell" -Arguments @(
            $InstalledArgs.ToArray()
        )
        $InstalledStoreLogResolved = [System.IO.Path]::GetFullPath($InstalledStoreMatrixLog)
    }

    Write-Summary -Lines $Lines `
        -Manifest $Manifest `
        -InspectStatus $InspectStatus `
        -SmokeResults $SmokeResults `
        -QemuElfStatus $QemuElfStatus `
        -QemuElfMode $QemuElfMode `
        -QemuElfLog $QemuElfLogResolved `
        -QemuElfDomainSummary $QemuElfDomainSummaryResolved `
        -QemuElfFramePpm $QemuElfFramePpmResolved `
        -QemuElfSummary $QemuElfSummary `
        -QemuElfDomainGolden $QemuElfDomainGolden `
        -FirmwareSize $FirmwareSize `
        -BoardMatrixLog $BoardLogResolved `
        -InstalledStoreMatrixLog $InstalledStoreLogResolved

    Set-Content -LiteralPath $Log -Encoding UTF8 -Value $Lines
    Write-Host "log=$Log"
} catch {
    [void]$Lines.Add("resident-platform-evidence-bundle: fail")
    [void]$Lines.Add($_.Exception.Message)
    if (-not (Test-Path -LiteralPath $LogDir)) {
        New-Item -ItemType Directory -Path $LogDir | Out-Null
    }
    Set-Content -LiteralPath $Log -Encoding UTF8 -Value $Lines
    Write-Error $_.Exception.Message
    exit 1
}
