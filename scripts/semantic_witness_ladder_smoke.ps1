param(
    [string]$CMakeExe = "cmake",
    [string]$CxxCompiler = "D:/Toolchains/LLVM/bin/clang++.exe",
    [string]$Generator = "Ninja",
    [string]$BuildRoot = "D:/Temp/charm-codex/cmake-build-semantic-witness-ladder",
    [switch]$Clean,
    [switch]$KeepBuildRoot
)

$ErrorActionPreference = "Stop"

function Resolve-FullPath {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ""
    }

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

function Resolve-ToolPath {
    param([string]$Tool)

    $command = Get-Command $Tool -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    if (Test-Path -LiteralPath $Tool) {
        return (Resolve-Path -LiteralPath $Tool).Path
    }

    throw "tool not found: $Tool"
}

function Test-PathUnderRoot {
    param(
        [string]$Root,
        [string]$Path
    )

    $rootPath = Resolve-FullPath -Path $Root
    $targetPath = Resolve-FullPath -Path $Path
    return $targetPath.StartsWith($rootPath, [System.StringComparison]::OrdinalIgnoreCase)
}

function Remove-SafeDirectory {
    param(
        [string]$RepoRoot,
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }

    $repoRootPath = Resolve-FullPath -Path $RepoRoot
    $tempRootPath = Resolve-FullPath -Path "D:/Temp/charm-codex"
    $targetPath = (Resolve-Path -LiteralPath $Path).Path
    $insideRepo = $targetPath.StartsWith($repoRootPath, [System.StringComparison]::OrdinalIgnoreCase)
    $insideTemp = $targetPath.StartsWith($tempRootPath, [System.StringComparison]::OrdinalIgnoreCase)
    if (-not ($insideRepo -or $insideTemp)) {
        throw "refusing to remove outside safe roots: $targetPath"
    }

    Remove-Item -LiteralPath $targetPath -Recurse -Force
}

function Assert-OutputContains {
    param(
        [string]$Output,
        [string]$Pattern,
        [string]$Label
    )

    if ($Output -notmatch $Pattern) {
        throw ("{0}: missing pattern [{1}]" -f $Label, $Pattern)
    }
}

function Invoke-Checked {
    param(
        [string]$FilePath,
        [string[]]$ArgumentList,
        [string]$FailureMessage
    )

    & $FilePath @ArgumentList
    if ($LASTEXITCODE -ne 0) {
        throw ("{0}: exit code {1}" -f $FailureMessage, $LASTEXITCODE)
    }
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$buildRootPath = Resolve-FullPath -Path $BuildRoot
$cmakePath = Resolve-ToolPath -Tool $CMakeExe

$ladder = @(
    [ordered]@{
        Name = "task-message-syscall"
        Example = "runtime_task_message_syscall_host"
        Target = "kernel-runtime-task-message-syscall-host"
        Required = @(
            "\[runtime-task-message-syscall-demo\] ok=1",
            "\[runtime-task-message-syscall-witness\] ok=1 supported=standing unsupported=standing route=none"
        )
    },
    [ordered]@{
        Name = "task-message-syscall-frame"
        Example = "runtime_task_message_syscall_frame_host"
        Target = "kernel-runtime-task-message-syscall-frame-host"
        Required = @(
            "\[runtime-task-message-syscall-frame-demo\] ok=1",
            "\[runtime-task-message-syscall-frame-witness\] ok=1 published=standing missing=standing route=none"
        )
    },
    [ordered]@{
        Name = "task-message-syscall-client"
        Example = "runtime_task_message_syscall_client_host"
        Target = "kernel-runtime-task-message-syscall-client-host"
        Required = @(
            "\[runtime-task-message-syscall-client-demo\] ok=1",
            "\[runtime-task-message-syscall-client-witness\] ok=1 reply=standing timeout=standing route=none"
        )
    },
    [ordered]@{
        Name = "task-message-syscall-pump"
        Example = "runtime_task_message_syscall_pump_host"
        Target = "kernel-runtime-task-message-syscall-pump-host"
        Required = @(
            "\[runtime-task-message-syscall-pump-demo\] ok=1",
            "\[runtime-task-message-syscall-pump-witness\] ok=1 reply=standing timeout=standing route=none"
        )
    },
    [ordered]@{
        Name = "task-message-runtime-service"
        Example = "runtime_task_message_runtime_service_host"
        Target = "kernel-runtime-task-message-runtime-service-host"
        Required = @(
            "\[runtime-task-message-runtime-service-demo\] ok=1"
        )
    },
    [ordered]@{
        Name = "task-message-runtime-api"
        Example = "runtime_task_message_runtime_api_host"
        Target = "kernel-runtime-task-message-runtime-api-host"
        Required = @(
            "\[runtime-task-message-runtime-api-demo\] ok=1"
        )
    },
    [ordered]@{
        Name = "task-message-syscall-api"
        Example = "runtime_task_message_syscall_api_host"
        Target = "kernel-runtime-task-message-syscall-api-host"
        Required = @(
            "\[runtime-task-message-syscall-api-demo\] ok=1"
        )
    },
    [ordered]@{
        Name = "task-message-session-api"
        Example = "runtime_task_message_session_api_host"
        Target = "kernel-runtime-task-message-session-api-host"
        Required = @(
            "\[runtime-task-message-session-api-demo\] ok=1",
            "\[runtime-task-message-session-api-witness\] ok=1 collapsed=collapsed"
        )
    },
    [ordered]@{
        Name = "task-message-session-protocol"
        Example = "runtime_task_message_session_protocol_host"
        Target = "kernel-runtime-task-message-session-protocol-host"
        Required = @(
            "\[runtime-task-message-session-protocol-demo\] ok=1",
            "\[runtime-task-message-session-protocol-witness\] ok=1 collapsed=collapsed route=input"
        )
    },
    [ordered]@{
        Name = "task-message-session-dispatch"
        Example = "runtime_task_message_session_dispatch_host"
        Target = "kernel-runtime-task-message-session-dispatch-host"
        Required = @(
            "\[runtime-task-message-session-dispatch-demo\] ok=1",
            "\[runtime-task-message-session-dispatch-witness\] ok=1 collapsed=collapsed"
        )
    },
    [ordered]@{
        Name = "task-message-session-acceptor"
        Example = "runtime_task_message_session_acceptor_host"
        Target = "kernel-runtime-task-message-session-acceptor-host"
        Required = @(
            "\[runtime-task-message-session-acceptor-demo\] ok=1",
            "\[runtime-task-message-session-acceptor-witness\] ok=1 collapsed=collapsed"
        )
    },
    [ordered]@{
        Name = "task-message-session-service"
        Example = "runtime_task_message_session_service_host"
        Target = "kernel-runtime-task-message-session-service-host"
        Required = @(
            "\[runtime-task-message-session-service-demo\] ok=1",
            "\[runtime-task-message-session-service-witness\] ok=1 collapsed=collapsed"
        )
    },
    [ordered]@{
        Name = "task-message-session-roundtrip"
        Example = "runtime_task_message_session_roundtrip_host"
        Target = "kernel-runtime-task-message-session-roundtrip-host"
        Required = @(
            "\[runtime-task-message-session-roundtrip-witness\] ok=1 dispatch=standing acceptor=standing protocol=standing service=standing pump=standing",
            "\[runtime-task-message-session-roundtrip-demo\] ok=1",
            "\[runtime-task-message-session-roundtrip-trace\] ok=1"
        )
    }
)

if (-not (Test-PathUnderRoot -Root $repoRoot -Path $buildRootPath) -and
    -not (Test-PathUnderRoot -Root "D:/Temp/charm-codex" -Path $buildRootPath)) {
    throw "build root must be inside repo or D:/Temp/charm-codex: $buildRootPath"
}

if ($Clean) {
    Remove-SafeDirectory -RepoRoot $repoRoot -Path $buildRootPath
}

Push-Location $repoRoot
try {
    $passed = 0
    foreach ($entry in $ladder) {
        $examplePath = Join-Path $repoRoot (Join-Path "Examples/kernel" $entry.Example)
        if (-not (Test-Path -LiteralPath (Join-Path $examplePath "CMakeLists.txt"))) {
            throw "missing example CMakeLists.txt: $examplePath"
        }

        Remove-SafeDirectory -RepoRoot $repoRoot -Path $buildRootPath
        New-Item -ItemType Directory -Path $buildRootPath -Force | Out-Null

        $configureArgs = @(
            "-S", $examplePath,
            "-B", $buildRootPath,
            "-G", $Generator,
            "-DCMAKE_CXX_COMPILER=$CxxCompiler",
            "-DCMAKE_BUILD_TYPE=Release"
        )
        Invoke-Checked -FilePath $cmakePath -ArgumentList $configureArgs -FailureMessage ("configure failed: {0}" -f $entry.Name)

        $buildArgs = @("--build", $buildRootPath, "--parallel", "1")
        Invoke-Checked -FilePath $cmakePath -ArgumentList $buildArgs -FailureMessage ("build failed: {0}" -f $entry.Name)

        $exePath = Join-Path $buildRootPath ($entry.Target + ".exe")
        if (-not (Test-Path -LiteralPath $exePath)) {
            $exePath = Join-Path $buildRootPath $entry.Target
        }
        if (-not (Test-Path -LiteralPath $exePath)) {
            throw "missing executable: $($entry.Target)"
        }

        $output = & $exePath 2>&1
        if ($LASTEXITCODE -ne 0) {
            $outputText = ($output | Out-String)
            throw ("run failed: {0}`n{1}" -f $entry.Name, $outputText)
        }

        $outputText = ($output | Out-String)
        foreach ($pattern in $entry.Required) {
            Assert-OutputContains -Output $outputText -Pattern $pattern -Label $entry.Name
        }

        ++$passed
        Write-Host ("[SEMANTIC-WITNESS-LADDER] {0}/{1} {2}=ok" -f $passed, $ladder.Count, $entry.Name)
    }

    Write-Host ("[SEMANTIC-WITNESS-LADDER-SMOKE] result=ok cases={0}" -f $passed)
} finally {
    Pop-Location
    if (-not $KeepBuildRoot) {
        Remove-SafeDirectory -RepoRoot $repoRoot -Path $buildRootPath
    }
}
