param(
    [string]$CMakeExe = "cmake",
    [string]$Generator = "Ninja",
    [string]$SummaryPath = "",
    [switch]$SkipConfigureIfPresent,
    [switch]$Fresh,
    [switch]$KeepBuildDirs,
    [int]$Jobs = 0,
    [switch]$StopOnFailure,
    [string[]]$Examples = @(
        "runtime_minimal_host",
        "runtime_binding_chain_host",
        "runtime_bridge_binding_host",
        "runtime_mailbox_host",
        "runtime_task_message_host",
        "runtime_task_message_table_host",
        "runtime_task_message_dispatch_host",
        "runtime_loop_port_host",
        "runtime_run_loop_host",
        "runtime_tick_host",
        "runtime_isr_defer_host",
        "runtime_thread_port_host",
        "runtime_service_host",
        "runtime_task_api_host",
        "runtime_task_syscall_catalog_host",
        "runtime_task_syscall_dispatch_host",
        "runtime_task_syscall_frame_caller_host",
        "runtime_task_syscall_frame_host",
        "runtime_task_syscall_host",
        "runtime_task_syscall_table_host",
        "runtime_trap_armv7a_host"
    )
)

$ErrorActionPreference = "Stop"

function Resolve-FullPath {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ""
    }

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

function Ensure-Directory {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    if (-not (Test-Path $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

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

function Resolve-RepoRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}

function Resolve-ExamplePath {
    param(
        [string]$RepoRoot,
        [string]$Example
    )

    $path = Join-Path $RepoRoot (Join-Path "Examples/kernel" $Example)
    if (-not (Test-Path (Join-Path $path "CMakeLists.txt"))) {
        throw "missing example CMakeLists.txt: $path"
    }

    return $path
}

function Resolve-ExecutablePath {
    param(
        [string]$BuildDir,
        [string]$TargetName
    )

    $candidates = @(
        (Join-Path $BuildDir ($TargetName + ".exe")),
        (Join-Path $BuildDir $TargetName)
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    $search = Get-ChildItem -Path $BuildDir -Recurse -File | Where-Object {
        $_.BaseName -eq $TargetName -or $_.Name -eq ($TargetName + ".exe")
    } | Select-Object -First 1

    if ($search) {
        return $search.FullName
    }

    throw "executable not found for target: $TargetName"
}

function Get-CMakeCacheValue {
    param(
        [string]$CachePath,
        [string]$Name
    )

    if (-not (Test-Path $CachePath)) {
        return ""
    }

    $pattern = '^{0}:[^=]*=(.*)$' -f [regex]::Escape($Name)
    $match = Select-String -LiteralPath $CachePath -Pattern $pattern | Select-Object -First 1
    if ($null -eq $match) {
        return ""
    }

    return [string]$match.Matches[0].Groups[1].Value
}

function Test-ConfiguredBuildDirReuse {
    param(
        [string]$BuildDir,
        [string]$Generator,
        [string]$SourceDir
    )

    $cachePath = Join-Path $BuildDir "CMakeCache.txt"
    if (-not (Test-Path $cachePath)) {
        return $false
    }

    $configuredGenerator = Get-CMakeCacheValue -CachePath $cachePath -Name "CMAKE_GENERATOR"
    if ([string]::IsNullOrWhiteSpace($configuredGenerator) -or $configuredGenerator -ne $Generator) {
        return $false
    }

    $configuredSourceDir = Get-CMakeCacheValue -CachePath $cachePath -Name "CMAKE_HOME_DIRECTORY"
    if ([string]::IsNullOrWhiteSpace($configuredSourceDir)) {
        return $false
    }

    return ([System.IO.Path]::GetFullPath($configuredSourceDir) -eq [System.IO.Path]::GetFullPath($SourceDir))
}

function Normalize-Examples {
    param([string[]]$InputExamples)

    $normalized = [System.Collections.Generic.List[string]]::new()
    foreach ($entry in $InputExamples) {
        if ([string]::IsNullOrWhiteSpace($entry)) {
            continue
        }

        foreach ($item in ($entry -split ",")) {
            $name = $item.Trim()
            if (-not [string]::IsNullOrWhiteSpace($name)) {
                $normalized.Add($name)
            }
        }
    }

    if ($normalized.Count -eq 0) {
        throw "no examples selected"
    }

    return $normalized.ToArray()
}

function Invoke-Checked {
    param(
        [string]$FilePath,
        [string[]]$ArgumentList,
        [string]$FailureMessage
    )

    & $FilePath @ArgumentList
    if ($LASTEXITCODE -ne 0) {
        throw $FailureMessage
    }
}

$cmake = Resolve-ToolPath -Tool $CMakeExe
$repoRoot = Resolve-RepoRoot
$resolvedSummaryPath = Resolve-FullPath -Path $SummaryPath
$selectedExamples = Normalize-Examples -InputExamples $Examples
$results = [System.Collections.Generic.List[object]]::new()
$stopRequested = $false

Push-Location $repoRoot
try {
    foreach ($example in $selectedExamples) {
        $buildDir = Join-Path $repoRoot ("cmake-build-verify-" + $example)
        $targetName = "kernel-" + ($example -replace "_", "-")
        $outputText = ""
        $status = "ok"
        $elapsedMs = 0
        $configureMs = 0
        $buildMs = 0
        $runMs = 0
        $configureSkipped = $false
        $failurePhase = ""
        $currentPhase = "prepare"
        $exampleStopwatch = [System.Diagnostics.Stopwatch]::StartNew()

        try {
            if ($Fresh -and (Test-Path $buildDir)) {
                Remove-Item $buildDir -Recurse -Force
            }

            $sourceDir = Resolve-ExamplePath -RepoRoot $repoRoot -Example $example

            $currentPhase = "configure"
            if ($SkipConfigureIfPresent -and (Test-ConfiguredBuildDirReuse -BuildDir $buildDir -Generator $Generator -SourceDir $sourceDir)) {
                $configureSkipped = $true
                Write-Host "==> [$example] configure (reused)"
            } else {
                Write-Host "==> [$example] configure"
                $configureStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
                try {
                    Invoke-Checked -FilePath $cmake `
                        -ArgumentList @("-S", $sourceDir, "-B", $buildDir, "-G", $Generator) `
                        -FailureMessage "cmake configure failed for $example"
                } finally {
                    $configureStopwatch.Stop()
                    $configureMs = $configureStopwatch.ElapsedMilliseconds
                }
            }

            $currentPhase = "build"
            Write-Host "==> [$example] build"
            $buildArgs = @("--build", $buildDir)
            if ($Jobs -gt 0) {
                $buildArgs += @("--parallel", $Jobs)
            }
            $buildStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
            try {
                Invoke-Checked -FilePath $cmake `
                    -ArgumentList $buildArgs `
                    -FailureMessage "cmake build failed for $example"
            } finally {
                $buildStopwatch.Stop()
                $buildMs = $buildStopwatch.ElapsedMilliseconds
            }

            $exePath = Resolve-ExecutablePath -BuildDir $buildDir -TargetName $targetName

            $currentPhase = "run"
            Write-Host "==> [$example] run"
            $runStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
            try {
                $runOutput = & $exePath 2>&1
                $exitCode = $LASTEXITCODE
                $outputText = (($runOutput | Out-String).Trim() -replace "`r?`n", " | ")

                if ([string]::IsNullOrWhiteSpace($outputText)) {
                    $outputText = "<no output>"
                }

                if ($exitCode -ne 0) {
                    throw "program exited with code $exitCode"
                }
            } finally {
                $runStopwatch.Stop()
                $runMs = $runStopwatch.ElapsedMilliseconds
            }

            Write-Host $outputText

            if (-not $KeepBuildDirs) {
                Remove-Item $buildDir -Recurse -Force -ErrorAction SilentlyContinue
            }
        } catch {
            $status = "fail"
            if ([string]::IsNullOrWhiteSpace($outputText)) {
                $outputText = $_.Exception.Message
            } else {
                $outputText = $outputText + " ; " + $_.Exception.Message
            }
            $failurePhase = $currentPhase

            Write-Host "FAIL [$example] $outputText"
            if ($StopOnFailure) {
                $stopRequested = $true
            }
        } finally {
            $exampleStopwatch.Stop()
            $elapsedMs = $exampleStopwatch.ElapsedMilliseconds
        }

        $results.Add([pscustomobject]@{
                Example = $example
                Status  = $status
                ElapsedMs = $elapsedMs
                ConfigureMs = $configureMs
                ConfigureSkipped = $configureSkipped
                BuildMs = $buildMs
                RunMs = $runMs
                FailurePhase = $failurePhase
                Detail  = $outputText
            })

        if ($stopRequested) {
            break
        }
    }
} finally {
    Pop-Location
}

Write-Host ""
Write-Host "==> summary"

$hasFailure = $false
foreach ($result in $results) {
    $configureText = if ($result.ConfigureSkipped) { "skip" } else { "{0}ms" -f $result.ConfigureMs }
    Write-Host ("{0}|{1}|{2}ms|cfg={3}|build={4}ms|run={5}ms|{6}" -f $result.Example, $result.Status, $result.ElapsedMs, $configureText, $result.BuildMs, $result.RunMs, $result.Detail)
    if ($result.Status -ne "ok") {
        $hasFailure = $true
    }
}

if (-not [string]::IsNullOrWhiteSpace($resolvedSummaryPath)) {
    Ensure-Directory -Path (Split-Path -Parent $resolvedSummaryPath)
    $summary = [pscustomobject]@{
        schema          = "minimal_kernel.runtime_host_smoke.summary/v1"
        generated_at    = (Get-Date).ToString("o")
        repo_root       = $repoRoot
        cmake           = $cmake
        generator       = $Generator
        selected_examples = @($selectedExamples)
        example_count   = $results.Count
        has_failure     = $hasFailure
        mode            = [pscustomobject]@{
            skip_configure_if_present = [bool]$SkipConfigureIfPresent
            fresh          = [bool]$Fresh
            keep_build_dirs = [bool]$KeepBuildDirs
            jobs           = $Jobs
            stop_on_failure = [bool]$StopOnFailure
        }
        results         = @($results)
    }
    $summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $resolvedSummaryPath -Encoding utf8
    Write-Host ("[SUMMARY] {0}" -f $resolvedSummaryPath)
}

if ($hasFailure) {
    exit 1
}

exit 0
