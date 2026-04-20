param(
    [string]$CMakeExe = "cmake",
    [string]$Generator = "Ninja",
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
        $exampleStopwatch = [System.Diagnostics.Stopwatch]::StartNew()

        try {
            if ($Fresh -and (Test-Path $buildDir)) {
                Remove-Item $buildDir -Recurse -Force
            }

            $sourceDir = Resolve-ExamplePath -RepoRoot $repoRoot -Example $example

            Write-Host "==> [$example] configure"
            Invoke-Checked -FilePath $cmake `
                -ArgumentList @("-S", $sourceDir, "-B", $buildDir, "-G", $Generator) `
                -FailureMessage "cmake configure failed for $example"

            Write-Host "==> [$example] build"
            $buildArgs = @("--build", $buildDir)
            if ($Jobs -gt 0) {
                $buildArgs += @("--parallel", $Jobs)
            }
            Invoke-Checked -FilePath $cmake `
                -ArgumentList $buildArgs `
                -FailureMessage "cmake build failed for $example"

            $exePath = Resolve-ExecutablePath -BuildDir $buildDir -TargetName $targetName

            Write-Host "==> [$example] run"
            $runOutput = & $exePath 2>&1
            $exitCode = $LASTEXITCODE
            $outputText = (($runOutput | Out-String).Trim() -replace "`r?`n", " | ")

            if ([string]::IsNullOrWhiteSpace($outputText)) {
                $outputText = "<no output>"
            }

            if ($exitCode -ne 0) {
                throw "program exited with code $exitCode"
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
    Write-Host ("{0}|{1}|{2}ms|{3}" -f $result.Example, $result.Status, $result.ElapsedMs, $result.Detail)
    if ($result.Status -ne "ok") {
        $hasFailure = $true
    }
}

if ($hasFailure) {
    exit 1
}

exit 0
